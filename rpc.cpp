// SPDX-FileCopyrightText: 2015-2022 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "rpc.h"

#include <QAuthenticator>
#include <QCoreApplication>
#include <QDir>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHostAddress>
#include <QHostInfo>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QTimer>
#include <QSslCertificate>
#include <QSslKey>
#include <QStringBuilder>
#include <QStandardPaths>
#include <QtConcurrentRun>

#include "addressutils.h"
#include "itemlistupdater.h"
#include "log.h"
#include "serversettings.h"
#include "serverstats.h"
#include "target_os.h"
#include "torrent.h"

SPECIALIZE_FORMATTER_FOR_Q_ENUM(QNetworkReply::NetworkError)
SPECIALIZE_FORMATTER_FOR_Q_ENUM(QSslError::SslError)

SPECIALIZE_FORMATTER_FOR_QDEBUG(QJsonObject)
SPECIALIZE_FORMATTER_FOR_QDEBUG(QHostAddress)
SPECIALIZE_FORMATTER_FOR_QDEBUG(QSslError)

namespace libtremotesf {
    namespace {
        // Transmission 2.40+
        constexpr int minimumRpcVersion = 14;

        constexpr int maxRetryAttempts = 3;

        const auto sessionIdHeader = QByteArrayLiteral("X-Transmission-Session-Id");
        constexpr auto torrentsKey = "torrents"_l1;
        constexpr auto torrentDuplicateKey = "torrent-duplicate"_l1;

        constexpr auto sessionIdFileLocation = [] {
            if constexpr (isTargetOsWindows) {
                return QStandardPaths::GenericDataLocation;
            } else {
                return QStandardPaths::TempLocation;
            }
        }();

        constexpr QLatin1String sessionIdFilePrefix = [] {
            if constexpr (isTargetOsWindows) {
                return "Transmission/tr_session_id_"_l1;
            } else {
                return "tr_session_id_"_l1;
            }
        }();

        inline QByteArray makeRequestData(const QString& method, const QVariantMap& arguments) {
            return QJsonDocument::fromVariant(
                       QVariantMap{{QStringLiteral("method"), method}, {QStringLiteral("arguments"), arguments}}
            ).toJson(QJsonDocument::Compact);
        }

        inline QJsonObject getReplyArguments(const QJsonObject& parseResult) {
            return parseResult.value("arguments"_l1).toObject();
        }

        inline bool isResultSuccessful(const QJsonObject& parseResult) {
            return (parseResult.value("result"_l1).toString() == "success"_l1);
        }

        QString readFileAsBase64String(QFile& file) {
            if (!file.isOpen() && !file.open(QIODevice::ReadOnly)) {
                logWarning("Failed to open file {}", file.fileName());
                return {};
            }

            static constexpr qint64 bufferSize = 1024 * 1024 - 1; // 1 MiB minus 1 byte (dividable by 3)
            QString string{};
            string.reserve(static_cast<QString::size_type>(((4 * file.size() / 3) + 3) & ~3));

            QByteArray buffer{};
            buffer.resize(static_cast<QByteArray::size_type>(std::min(bufferSize, file.size())));

            qint64 offset{};

            while (true) {
                const qint64 n = file.read(buffer.data() + offset, buffer.size() - offset);
                if (n <= 0) {
                    if (offset > 0) {
                        buffer.resize(static_cast<QByteArray::size_type>(offset));
                        string.append(QLatin1String(buffer.toBase64()));
                    }
                    break;
                }
                offset += n;
                if (offset == buffer.size()) {
                    string.append(QLatin1String(buffer.toBase64()));
                    offset = 0;
                }
            }

            return string;
        }
    }

    Rpc::Rpc(QObject* parent)
        : QObject(parent),
          mNetwork(new QNetworkAccessManager(this)),
          mUpdateTimer(new QTimer(this)),
          mAutoReconnectTimer(new QTimer(this)),
          mServerSettings(new ServerSettings(this, this)),
          mServerStats(new ServerStats(this)) {
        mNetwork->setAutoDeleteReplies(true);
        QObject::connect(
            mNetwork,
            &QNetworkAccessManager::authenticationRequired,
            this,
            &Rpc::onAuthenticationRequired
        );

        mAutoReconnectTimer->setSingleShot(true);
        QObject::connect(mAutoReconnectTimer, &QTimer::timeout, this, [=] {
            logInfo("Auto reconnection");
            connect();
        });

        mUpdateTimer->setSingleShot(true);
        QObject::connect(mUpdateTimer, &QTimer::timeout, this, [=] { updateData(); });
    }

    ServerSettings* Rpc::serverSettings() const { return mServerSettings; }

    ServerStats* Rpc::serverStats() const { return mServerStats; }

    const std::vector<std::unique_ptr<Torrent>>& Rpc::torrents() const { return mTorrents; }

    Torrent* Rpc::torrentByHash(const QString& hash) const {
        for (const std::unique_ptr<Torrent>& torrent : mTorrents) {
            if (torrent->hashString() == hash) {
                return torrent.get();
            }
        }
        return nullptr;
    }

    Torrent* Rpc::torrentById(int id) const {
        const auto end(mTorrents.end());
        const auto found(std::find_if(mTorrents.begin(), mTorrents.end(), [id](const auto& torrent) {
            return torrent->id() == id;
        }));
        return (found == end) ? nullptr : found->get();
    }

    bool Rpc::isConnected() const { return (mStatus.connectionState == ConnectionState::Connected); }

    const Rpc::Status& Rpc::status() const { return mStatus; }

    Rpc::ConnectionState Rpc::connectionState() const { return mStatus.connectionState; }

    Rpc::Error Rpc::error() const { return mStatus.error; }

    const QString& Rpc::errorMessage() const { return mStatus.errorMessage; }

    const QString& Rpc::detailedErrorMessage() const { return mStatus.detailedErrorMessage; }

    bool Rpc::isLocal() const { return mServerIsLocal.value_or(false); }

    bool Rpc::isServerRunningOnWindows() const { return mServerIsRunningOnWindows; }

    int Rpc::torrentsCount() const { return static_cast<int>(mTorrents.size()); }

    bool Rpc::isUpdateDisabled() const { return mUpdateDisabled; }

    void Rpc::setUpdateDisabled(bool disabled) {
        if (disabled != mUpdateDisabled) {
            mUpdateDisabled = disabled;
            if (isConnected()) {
                if (disabled) {
                    mUpdateTimer->stop();
                } else {
                    updateData();
                }
            }
            if (disabled) {
                mAutoReconnectTimer->stop();
            }
            emit updateDisabledChanged();
        }
    }

    void Rpc::setServer(const Server& server) {
        mNetwork->clearAccessCache();
        disconnect();

        QLatin1String scheme;
        if (server.https) {
            scheme = "https://"_l1;
        } else {
            scheme = "http://"_l1;
        }
        const QString urlString = scheme % server.address % QLatin1Char(':') % QString::number(server.port) %
                                  QLatin1Char('/') % server.apiPath;
        mServerUrl =
            QUrl(urlString, QUrl::TolerantMode).adjusted(QUrl::StripTrailingSlash | QUrl::NormalizePathSegments);
        if (!mServerUrl.isValid()) {
            logWarning("Failed to parse URL from {}", urlString);
        }

        switch (server.proxyType) {
        case Server::ProxyType::Default:
            mNetwork->setProxy(QNetworkProxy::applicationProxy());
            break;
        case Server::ProxyType::Http:
            mNetwork->setProxy(QNetworkProxy(
                QNetworkProxy::HttpProxy,
                server.proxyHostname,
                static_cast<quint16>(server.proxyPort),
                server.proxyUser,
                server.proxyPassword
            ));
            break;
        case Server::ProxyType::Socks5:
            mNetwork->setProxy(QNetworkProxy(
                QNetworkProxy::Socks5Proxy,
                server.proxyHostname,
                static_cast<quint16>(server.proxyPort),
                server.proxyUser,
                server.proxyPassword
            ));
            break;
        }

        mSslConfiguration = QSslConfiguration::defaultConfiguration();
        mExpectedSslErrors.clear();

        if (server.selfSignedCertificateEnabled) {
            const auto certificates(QSslCertificate::fromData(server.selfSignedCertificate, QSsl::Pem));
            mExpectedSslErrors.reserve(certificates.size() * 3);
            for (const auto& certificate : certificates) {
                mExpectedSslErrors.push_back(QSslError(QSslError::HostNameMismatch, certificate));
                mExpectedSslErrors.push_back(QSslError(QSslError::SelfSignedCertificate, certificate));
                mExpectedSslErrors.push_back(QSslError(QSslError::SelfSignedCertificateInChain, certificate));
            }
        }

        if (server.clientCertificateEnabled) {
            mSslConfiguration.setLocalCertificate(QSslCertificate(server.clientCertificate, QSsl::Pem));
            mSslConfiguration.setPrivateKey(QSslKey(server.clientCertificate, QSsl::Rsa));
        }

        mAuthentication = server.authentication;
        mUsername = server.username;
        mPassword = server.password;
        mTimeoutMillis = server.timeout * 1000; // msecs
        mUpdateTimer->setInterval(server.updateInterval * 1000); // msecs

        mAutoReconnectEnabled = server.autoReconnectEnabled;
        mAutoReconnectTimer->setInterval(server.autoReconnectInterval * 1000); // msecs
        mAutoReconnectTimer->stop();
    }

    void Rpc::resetServer() {
        disconnect();
        mServerUrl.clear();
        mSslConfiguration = QSslConfiguration::defaultConfiguration();
        mExpectedSslErrors.clear();
        mAuthentication = false;
        mUsername.clear();
        mPassword.clear();
        mTimeoutMillis = 0;
        mAutoReconnectEnabled = false;
        mAutoReconnectTimer->stop();
    }

    void Rpc::connect() {
        if (connectionState() == ConnectionState::Disconnected && !mServerUrl.isEmpty()) {
            setStatus(Status{ConnectionState::Connecting});
            getServerSettings();
        }
    }

    void Rpc::disconnect() {
        setStatus(Status{ConnectionState::Disconnected});
        mAutoReconnectTimer->stop();
    }

    void Rpc::addTorrentFile(
        const QString& filePath,
        const QString& downloadDirectory,
        const QVariantList& unwantedFiles,
        const QVariantList& highPriorityFiles,
        const QVariantList& lowPriorityFiles,
        const QVariantMap& renamedFiles,
        int bandwidthPriority,
        bool start
    ) {
        if (!isConnected()) {
            return;
        }
        if (auto file = std::make_shared<QFile>(filePath); file->open(QIODevice::ReadOnly)) {
            addTorrentFile(
                std::move(file),
                downloadDirectory,
                unwantedFiles,
                highPriorityFiles,
                lowPriorityFiles,
                renamedFiles,
                bandwidthPriority,
                start
            );
        } else {
            logWarning(
                "addTorrentFile: failed to open file, error = {}, error string = {}",
                static_cast<std::underlying_type_t<QFile::FileError>>(file->error()),
                file->errorString()
            );
            emit torrentAddError();
        }
    }

    void Rpc::addTorrentFile(
        std::shared_ptr<QFile> file,
        const QString& downloadDirectory,
        const QVariantList& unwantedFiles,
        const QVariantList& highPriorityFiles,
        const QVariantList& lowPriorityFiles,
        const QVariantMap& renamedFiles,
        int bandwidthPriority,
        bool start
    ) {
        if (!isConnected()) {
            return;
        }
        const auto future = QtConcurrent::run([=, file = std::move(file)]() {
            return makeRequestData(
                "torrent-add"_l1,
                {{"metainfo"_l1, readFileAsBase64String(*file)},
                 {"download-dir"_l1, downloadDirectory},
                 {"files-unwanted"_l1, unwantedFiles},
                 {"priority-high"_l1, highPriorityFiles},
                 {"priority-low"_l1, lowPriorityFiles},
                 {"bandwidthPriority"_l1, bandwidthPriority},
                 {"paused"_l1, !start}}
            );
        });
        auto watcher = new QFutureWatcher<QByteArray>(this);
        QObject::connect(watcher, &QFutureWatcher<QByteArray>::finished, this, [=] {
            if (isConnected()) {
                postRequest("torrent-add"_l1, watcher->result(), [=](const auto& parseResult, bool success) {
                    if (success) {
                        const auto arguments(getReplyArguments(parseResult));
                        if (arguments.contains(torrentDuplicateKey)) {
                            emit torrentAddDuplicate();
                        } else {
                            if (!renamedFiles.isEmpty()) {
                                const QJsonObject torrentJson(arguments.value("torrent-added"_l1).toObject());
                                if (!torrentJson.isEmpty()) {
                                    const int id = torrentJson.value(Torrent::idKey).toInt();
                                    for (auto i = renamedFiles.begin(), end = renamedFiles.end(); i != end; ++i) {
                                        renameTorrentFile(id, i.key(), i.value().toString());
                                    }
                                }
                            }
                            updateData();
                        }
                    } else {
                        emit torrentAddError();
                    }
                });
                watcher->deleteLater();
            }
        });
        watcher->setFuture(future);
    }

    void Rpc::addTorrentLink(const QString& link, const QString& downloadDirectory, int bandwidthPriority, bool start) {
        if (isConnected()) {
            postRequest(
                "torrent-add"_l1,
                {{"filename"_l1, link},
                 {"download-dir"_l1, downloadDirectory},
                 {"bandwidthPriority"_l1, bandwidthPriority},
                 {"paused"_l1, !start}},
                [=](const auto& parseResult, bool success) {
                    if (success) {
                        if (getReplyArguments(parseResult).contains(torrentDuplicateKey)) {
                            emit torrentAddDuplicate();
                        } else {
                            updateData();
                        }
                    } else {
                        emit torrentAddError();
                    }
                }
            );
        }
    }

    void Rpc::startTorrents(const QVariantList& ids) {
        if (isConnected()) {
            postRequest("torrent-start"_l1, {{"ids"_l1, ids}}, [=](const auto&, bool success) {
                if (success) {
                    updateData();
                }
            });
        }
    }

    void Rpc::startTorrentsNow(const QVariantList& ids) {
        if (isConnected()) {
            postRequest("torrent-start-now"_l1, {{"ids"_l1, ids}}, [=](const auto&, bool success) {
                if (success) {
                    updateData();
                }
            });
        }
    }

    void Rpc::pauseTorrents(const QVariantList& ids) {
        if (isConnected()) {
            postRequest("torrent-stop"_l1, {{"ids"_l1, ids}}, [=](const auto&, bool success) {
                if (success) {
                    updateData();
                }
            });
        }
    }

    void Rpc::removeTorrents(const QVariantList& ids, bool deleteFiles) {
        if (isConnected()) {
            postRequest(
                "torrent-remove"_l1,
                {{"ids"_l1, ids}, {"delete-local-data"_l1, deleteFiles}},
                [=](const auto&, bool success) {
                    if (success) {
                        updateData();
                    }
                }
            );
        }
    }

    void Rpc::checkTorrents(const QVariantList& ids) {
        if (isConnected()) {
            postRequest("torrent-verify"_l1, {{"ids"_l1, ids}}, [=](const auto&, bool success) {
                if (success) {
                    updateData();
                }
            });
        }
    }

    void Rpc::moveTorrentsToTop(const QVariantList& ids) {
        if (isConnected()) {
            postRequest("queue-move-top"_l1, {{"ids"_l1, ids}}, [=](const auto&, bool success) {
                if (success) {
                    updateData();
                }
            });
        }
    }

    void Rpc::moveTorrentsUp(const QVariantList& ids) {
        if (isConnected()) {
            postRequest("queue-move-up"_l1, {{"ids"_l1, ids}}, [=](const auto&, bool success) {
                if (success) {
                    updateData();
                }
            });
        }
    }

    void Rpc::moveTorrentsDown(const QVariantList& ids) {
        if (isConnected()) {
            postRequest("queue-move-down"_l1, {{"ids"_l1, ids}}, [=](const auto&, bool success) {
                if (success) {
                    updateData();
                }
            });
        }
    }

    void Rpc::moveTorrentsToBottom(const QVariantList& ids) {
        if (isConnected()) {
            postRequest("queue-move-bottom"_l1, {{"ids"_l1, ids}}, [=](const auto&, bool success) {
                if (success) {
                    updateData();
                }
            });
        }
    }

    void Rpc::reannounceTorrents(const QVariantList& ids) {
        if (isConnected()) {
            postRequest("torrent-reannounce"_l1, {{"ids"_l1, ids}});
        }
    }

    void Rpc::setSessionProperty(const QString& property, const QVariant& value) {
        setSessionProperties({{property, value}});
    }

    void Rpc::setSessionProperties(const QVariantMap& properties) {
        if (isConnected()) {
            postRequest("session-set"_l1, properties);
        }
    }

    void Rpc::setTorrentProperty(int id, const QString& property, const QVariant& value, bool updateIfSuccessful) {
        if (isConnected()) {
            postRequest(
                "torrent-set"_l1,
                {{"ids"_l1, QVariantList{id}}, {property, value}},
                [=](const auto&, bool success) {
                    if (success && updateIfSuccessful) {
                        updateData();
                    }
                }
            );
        }
    }

    void Rpc::setTorrentsLocation(const QVariantList& ids, const QString& location, bool moveFiles) {
        if (isConnected()) {
            postRequest(
                "torrent-set-location"_l1,
                {{"ids"_l1, ids}, {"location"_l1, location}, {"move"_l1, moveFiles}},
                [=](const auto&, bool success) {
                    if (success) {
                        updateData();
                    }
                }
            );
        }
    }

    void Rpc::getTorrentsFiles(const QVariantList& ids, bool scheduled) {
        postRequest(
            "torrent-get"_l1,
            {{"fields"_l1, QStringList{"id"_l1, "files"_l1, "fileStats"_l1}}, {"ids"_l1, ids}},
            [=](const auto& parseResult, bool success) {
                if (success) {
                    const QJsonArray torrents(getReplyArguments(parseResult).value(torrentsKey).toArray());
                    for (const auto& torrentJson : torrents) {
                        const QJsonObject torrentMap(torrentJson.toObject());
                        const int torrentId = torrentMap.value(Torrent::idKey).toInt();
                        Torrent* torrent = torrentById(torrentId);
                        if (torrent && torrent->isFilesEnabled()) {
                            torrent->updateFiles(torrentMap);
                        }
                    }
                    if (scheduled) {
                        for (const auto& torrent : mTorrents) {
                            torrent->checkThatFilesUpdated();
                        }
                        maybeFinishUpdatingTorrents();
                        maybeFinishUpdateOrConnection();
                    }
                }
            }
        );
    }

    void Rpc::getTorrentsPeers(const QVariantList& ids, bool scheduled) {
        postRequest(
            "torrent-get"_l1,
            {{"fields"_l1, QStringList{"id"_l1, "peers"_l1}}, {"ids"_l1, ids}},
            [=](const auto& parseResult, bool success) {
                if (success) {
                    const QJsonArray torrents(getReplyArguments(parseResult).value(torrentsKey).toArray());
                    for (const auto& torrentJson : torrents) {
                        const QJsonObject torrentMap(torrentJson.toObject());
                        const int torrentId = torrentMap.value(Torrent::idKey).toInt();
                        Torrent* torrent = torrentById(torrentId);
                        if (torrent && torrent->isPeersEnabled()) {
                            torrent->updatePeers(torrentMap);
                        }
                    }
                    if (scheduled) {
                        for (const auto& torrent : mTorrents) {
                            torrent->checkThatPeersUpdated();
                        }
                        maybeFinishUpdatingTorrents();
                        maybeFinishUpdateOrConnection();
                    }
                }
            }
        );
    }

    void Rpc::renameTorrentFile(int torrentId, const QString& filePath, const QString& newName) {
        if (isConnected()) {
            postRequest(
                "torrent-rename-path"_l1,
                {{"ids"_l1, QVariantList{torrentId}}, {"path"_l1, filePath}, {"name"_l1, newName}},
                [=](const auto& parseResult, bool success) {
                    if (success) {
                        Torrent* torrent = torrentById(torrentId);
                        if (torrent) {
                            const QJsonObject arguments(getReplyArguments(parseResult));
                            const QString path(arguments.value("path"_l1).toString());
                            const QString newName(arguments.value("name"_l1).toString());
                            emit torrent->fileRenamed(path, newName);
                            emit torrentFileRenamed(torrentId, path, newName);
                            updateData();
                        }
                    }
                }
            );
        }
    }

    void Rpc::getDownloadDirFreeSpace() {
        if (isConnected()) {
            postRequest(
                "download-dir-free-space"_l1,
                QByteArrayLiteral("{"
                                  "\"arguments\":{"
                                  "\"fields\":["
                                  "\"download-dir-free-space\""
                                  "]"
                                  "},"
                                  "\"method\":\"session-get\""
                                  "}"),
                [=](const auto& parseResult, bool success) {
                    if (success) {
                        emit gotDownloadDirFreeSpace(static_cast<long long>(
                            getReplyArguments(parseResult).value("download-dir-free-space"_l1).toDouble()
                        ));
                    }
                }
            );
        }
    }

    void Rpc::getFreeSpaceForPath(const QString& path) {
        if (isConnected()) {
            postRequest("free-space"_l1, {{"path"_l1, path}}, [=](const auto& parseResult, bool success) {
                emit gotFreeSpaceForPath(
                    path,
                    success,
                    success ? static_cast<long long>(getReplyArguments(parseResult).value("size-bytes"_l1).toDouble())
                            : 0
                );
            });
        }
    }

    void Rpc::updateData(bool updateServerSettings) {
        if (connectionState() != ConnectionState::Disconnected && !mUpdating) {
            if (updateServerSettings) {
                mServerSettingsUpdated = false;
            }
            mTorrentsUpdated = false;
            mServerStatsUpdated = false;

            mUpdateTimer->stop();

            mUpdating = true;

            if (updateServerSettings) {
                getServerSettings();
            }
            getTorrents();
            getServerStats();
        } else {
            logWarning(
                "updateData: called in incorrect state, connectionState = {}, updating = {}",
                connectionState(),
                mUpdating
            );
        }
    }

    void Rpc::shutdownServer() {
        if (isConnected()) {
            postRequest("session-close"_l1, QVariantMap{}, [=](const QJsonObject&, bool success) {
                if (success) {
                    logInfo("Successfully sent shutdown request, disconnecting");
                    disconnect();
                }
            });
        }
    }

    void Rpc::setStatus(Status&& status) {
        if (status == mStatus) {
            return;
        }

        const Status oldStatus = mStatus;

        const bool connectionStateChanged = status.connectionState != oldStatus.connectionState;
        if (connectionStateChanged && oldStatus.connectionState == ConnectionState::Connected) {
            emit aboutToDisconnect();
        }

        mStatus = std::move(status);

        size_t removedTorrentsCount = 0;
        if (connectionStateChanged) {
            resetStateOnConnectionStateChanged(oldStatus.connectionState, removedTorrentsCount);
        }

        emit statusChanged();

        if (connectionStateChanged) {
            emitSignalsOnConnectionStateChanged(oldStatus.connectionState, removedTorrentsCount);
        }

        if (mStatus.error != oldStatus.error || mStatus.errorMessage != oldStatus.errorMessage) {
            emit errorChanged();
        }
    }

    void Rpc::resetStateOnConnectionStateChanged(ConnectionState oldConnectionState, size_t& removedTorrentsCount) {
        switch (mStatus.connectionState) {
        case ConnectionState::Disconnected: {
            logInfo("Disconnected");

            mNetwork->clearAccessCache();

            const auto activeRequests(mActiveNetworkRequests);
            mActiveNetworkRequests.clear();
            mRetryingNetworkRequests.clear();
            for (QNetworkReply* reply : activeRequests) {
                reply->abort();
            }

            mAuthenticationRequested = false;
            mSessionId.clear();
            mUpdating = false;
            mServerIsLocal = std::nullopt;
            if (mPendingHostInfoLookupId.has_value()) {
                QHostInfo::abortHostLookup(*mPendingHostInfoLookupId);
                mPendingHostInfoLookupId = std::nullopt;
            }
            mServerIsRunningOnWindows = false;
            mServerSettingsUpdated = false;
            mTorrentsUpdated = false;
            mServerStatsUpdated = false;
            mUpdateTimer->stop();

            if (!mTorrents.empty() && oldConnectionState == ConnectionState::Connected) {
                removedTorrentsCount = mTorrents.size();
                emit onAboutToRemoveTorrents(0, removedTorrentsCount);
                mTorrents.clear();
                emit onRemovedTorrents(0, removedTorrentsCount);
            }

            break;
        }
        case ConnectionState::Connecting:
            logInfo("Connecting");
            break;
        case ConnectionState::Connected: {
            logInfo("Connected");
            break;
        }
        }
    }

    void
    Rpc::emitSignalsOnConnectionStateChanged(Rpc::ConnectionState oldConnectionState, size_t removedTorrentsCount) {
        emit connectionStateChanged();

        switch (mStatus.connectionState) {
        case ConnectionState::Disconnected: {
            if (oldConnectionState == ConnectionState::Connected) {
                emit connectedChanged();
                emit torrentsUpdated({{0, static_cast<int>(removedTorrentsCount)}}, {}, 0);
            }
            break;
        }
        case ConnectionState::Connecting:
            break;
        case ConnectionState::Connected: {
            emit connectedChanged();
            emit torrentsUpdated({}, {}, torrentsCount());
            break;
        }
        }
    }

    void Rpc::getServerSettings() {
        postRequest(
            "session-get"_l1,
            QByteArrayLiteral("{\"method\":\"session-get\"}"),
            [=](const auto& parseResult, bool success) {
                if (success) {
                    mServerSettings->update(getReplyArguments(parseResult));
                    mServerSettingsUpdated = true;
                    if (connectionState() == ConnectionState::Connecting) {
                        if (mServerSettings->minimumRpcVersion() > minimumRpcVersion) {
                            setStatus(Status{ConnectionState::Disconnected, Error::ServerIsTooNew});
                        } else if (mServerSettings->rpcVersion() < minimumRpcVersion) {
                            setStatus(Status{ConnectionState::Disconnected, Error::ServerIsTooOld});
                        } else {
                            updateData(false);
                            checkIfServerIsLocal();
                        }
                    } else {
                        maybeFinishUpdateOrConnection();
                    }
                }
            }
        );
    }

    using NewTorrent = std::pair<QJsonObject, int>;

    class TorrentsListUpdater : public ItemListUpdater<std::unique_ptr<Torrent>, NewTorrent, std::vector<NewTorrent>> {
    public:
        inline explicit TorrentsListUpdater(Rpc& rpc) : mRpc(rpc) {}

        void update(std::vector<std::unique_ptr<Torrent>>& torrents, std::vector<NewTorrent>&& newTorrents) override {
            if (newTorrents.size() > torrents.size()) {
                checkSingleFileIds.reserve(
                    static_cast<decltype(checkSingleFileIds)::size_type>(newTorrents.size() - torrents.size())
                );
            }
            ItemListUpdater::update(torrents, std::move(newTorrents));
        }

        std::vector<std::pair<int, int>> removedIndexRanges;
        std::vector<std::pair<int, int>> changedIndexRanges;
        int addedCount = 0;
        QVariantList checkSingleFileIds;
        QVariantList getFilesIds;
        QVariantList getPeersIds;

    protected:
        std::vector<NewTorrent>::iterator
        findNewItemForItem(std::vector<NewTorrent>& newTorrents, const std::unique_ptr<Torrent>& torrent) override {
            const int id = torrent->id();
            return std::find_if(newTorrents.begin(), newTorrents.end(), [id](const auto& t) {
                const auto& [json, newTorrentId] = t;
                return newTorrentId == id;
            });
        }

        void onAboutToRemoveItems(size_t first, size_t last) override {
            emit mRpc.onAboutToRemoveTorrents(first, last);
        };

        void onRemovedItems(size_t first, size_t last) override {
            removedIndexRanges.emplace_back(static_cast<int>(first), static_cast<int>(last));
            emit mRpc.onRemovedTorrents(first, last);
        }

        bool updateItem(std::unique_ptr<Torrent>& torrent, NewTorrent&& newTorrent) override {
            const auto& [json, id] = newTorrent;

            const bool wasFinished = torrent->isFinished();
            const bool wasPaused = (torrent->status() == Torrent::Status::Paused);
            const auto oldSizeWhenDone = torrent->sizeWhenDone();
            const bool metadataWasComplete = torrent->isMetadataComplete();

            const bool changed = torrent->update(json);
            if (changed) {
                if (!wasFinished && torrent->isFinished() &&
                    !wasPaused
                    // Don't emit torrentFinished() if torrent's size became smaller
                    // since there is high chance that it happened because user unselected some files
                    // and torrent immediately became finished. We don't want notification in that case
                    && torrent->sizeWhenDone() >= oldSizeWhenDone) {
                    emit mRpc.torrentFinished(torrent.get());
                }
                if (!metadataWasComplete && torrent->isMetadataComplete()) {
                    checkSingleFileIds.push_back(id);
                }
            }
            if (torrent->isFilesEnabled()) {
                getFilesIds.push_back(id);
            }
            if (torrent->isPeersEnabled()) {
                getPeersIds.push_back(id);
            }

            return changed;
        }

        void onChangedItems(size_t first, size_t last) override {
            changedIndexRanges.emplace_back(static_cast<int>(first), static_cast<int>(last));
            emit mRpc.onChangedTorrents(first, last);
        }

        std::unique_ptr<Torrent> createItemFromNewItem(NewTorrent&& newTorrent) override {
            const auto& [torrentJson, id] = newTorrent;
            auto torrent = std::make_unique<Torrent>(id, torrentJson, &mRpc);
            if (mRpc.isConnected()) {
                emit mRpc.torrentAdded(torrent.get());
            }
            if (torrent->isMetadataComplete()) {
                checkSingleFileIds.push_back(id);
            }
            return torrent;
        }

        void onAboutToAddItems(size_t count) override { emit mRpc.onAboutToAddTorrents(count); }

        void onAddedItems(size_t count) override {
            addedCount = static_cast<int>(count);
            emit mRpc.onAddedTorrents(count);
        };

    private:
        Rpc& mRpc;
    };

    void Rpc::getTorrents() {
        postRequest(
            "torrent-get"_l1,
            QByteArrayLiteral("{"
                              "\"arguments\":{"
                              "\"fields\":["
                              "\"activityDate\","
                              "\"addedDate\","
                              "\"bandwidthPriority\","
                              "\"comment\","
                              "\"creator\","
                              "\"dateCreated\","
                              "\"doneDate\","
                              "\"downloadDir\","
                              "\"downloadedEver\","
                              "\"downloadLimit\","
                              "\"downloadLimited\","
                              "\"error\","
                              "\"errorString\","
                              "\"eta\","
                              "\"hashString\","
                              "\"haveValid\","
                              "\"honorsSessionLimits\","
                              "\"id\","
                              "\"leftUntilDone\","
                              "\"magnetLink\","
                              "\"metadataPercentComplete\","
                              "\"name\","
                              "\"peer-limit\","
                              "\"peersConnected\","
                              "\"peersGettingFromUs\","
                              "\"peersSendingToUs\","
                              "\"percentDone\","
                              "\"queuePosition\","
                              "\"rateDownload\","
                              "\"rateUpload\","
                              "\"recheckProgress\","
                              "\"seedIdleLimit\","
                              "\"seedIdleMode\","
                              "\"seedRatioLimit\","
                              "\"seedRatioMode\","
                              "\"sizeWhenDone\","
                              "\"status\","
                              "\"totalSize\","
                              "\"trackerStats\","
                              "\"uploadedEver\","
                              "\"uploadLimit\","
                              "\"uploadLimited\","
                              "\"uploadRatio\","
                              "\"webseeds\","
                              "\"webseedsSendingToUs\""
                              "]"
                              "},"
                              "\"method\":\"torrent-get\""
                              "}"),
            [=](const auto& parseResult, bool success) {
                if (!success) {
                    return;
                }

                std::vector<NewTorrent> newTorrents;
                {
                    const QJsonArray torrentsJsons(getReplyArguments(parseResult).value("torrents"_l1).toArray());
                    newTorrents.reserve(static_cast<size_t>(torrentsJsons.size()));
                    for (const auto& i : torrentsJsons) {
                        QJsonObject torrentJson(i.toObject());
                        const int id = torrentJson.value(Torrent::idKey).toInt();
                        newTorrents.emplace_back(std::move(torrentJson), id);
                    }
                }

                TorrentsListUpdater updater(*this);
                updater.update(mTorrents, std::move(newTorrents));

                maybeFinishUpdatingTorrents();
                const bool wasConnecting = connectionState() == ConnectionState::Connecting;
                maybeFinishUpdateOrConnection();
                if (!wasConnecting) {
                    emit torrentsUpdated(updater.removedIndexRanges, updater.changedIndexRanges, updater.addedCount);
                }

                if (!updater.checkSingleFileIds.isEmpty()) {
                    checkTorrentsSingleFile(updater.checkSingleFileIds);
                }
                if (!updater.getFilesIds.isEmpty()) {
                    getTorrentsFiles(updater.getFilesIds, true);
                }
                if (!updater.getPeersIds.isEmpty()) {
                    getTorrentsPeers(updater.getPeersIds, true);
                }
            }
        );
    }

    void Rpc::checkTorrentsSingleFile(const QVariantList& torrentIds) {
        postRequest(
            "torrent-get"_l1,
            {{"fields"_l1, QVariantList{"id"_l1, "priorities"_l1}}, {"ids"_l1, torrentIds}},
            [=](const auto& parseResult, bool success) {
                if (success) {
                    const QJsonArray torrents(getReplyArguments(parseResult).value(torrentsKey).toArray());
                    for (const auto& i : torrents) {
                        const QJsonObject torrentMap(i.toObject());
                        const int torrentId = torrentMap.value(Torrent::idKey).toInt();
                        Torrent* torrent = torrentById(torrentId);
                        if (torrent) {
                            torrent->checkSingleFile(torrentMap);
                        }
                    }
                }
            }
        );
    }

    void Rpc::getServerStats() {
        postRequest(
            "session-stats"_l1,
            QByteArrayLiteral("{\"method\":\"session-stats\"}"),
            [=](const auto& parseResult, bool success) {
                if (success) {
                    mServerStats->update(getReplyArguments(parseResult));
                    mServerStatsUpdated = true;
                    maybeFinishUpdateOrConnection();
                }
            }
        );
    }

    void Rpc::maybeFinishUpdatingTorrents() {
        if (mUpdating && !mTorrentsUpdated) {
            for (const std::unique_ptr<Torrent>& torrent : mTorrents) {
                if (!torrent->isUpdated()) {
                    return;
                }
            }
            mTorrentsUpdated = true;
        }
    }

    bool Rpc::checkIfUpdateCompleted() { return mServerSettingsUpdated && mTorrentsUpdated && mServerStatsUpdated; }

    bool Rpc::checkIfConnectionCompleted() { return checkIfUpdateCompleted() && mServerIsLocal.has_value(); }

    void Rpc::maybeFinishUpdateOrConnection() {
        const bool connecting = connectionState() == ConnectionState::Connecting;
        if (!mUpdating && !connecting) return;
        if (mUpdating) {
            if (checkIfUpdateCompleted()) {
                mUpdating = false;
            } else {
                return;
            }
        }
        if (connecting) {
            if (checkIfConnectionCompleted()) {
                setStatus(Status{ConnectionState::Connected});
            } else {
                return;
            }
        }
        if (!mUpdateDisabled) {
            mUpdateTimer->start();
        }
    }

    void Rpc::onAuthenticationRequired(QNetworkReply*, QAuthenticator* authenticator) {
        if (mAuthentication && !mAuthenticationRequested) {
            authenticator->setUser(mUsername);
            authenticator->setPassword(mPassword);
            mAuthenticationRequested = true;
        }
    }

    QNetworkReply* Rpc::postRequest(Request&& request) {
        QNetworkReply* reply = mNetwork->post(request.request, request.data);
        mActiveNetworkRequests.insert(reply);

        reply->ignoreSslErrors(mExpectedSslErrors);
        auto sslErrors = std::make_shared<QList<QSslError>>();

        QObject::connect(reply, &QNetworkReply::sslErrors, this, [sslErrors](const QList<QSslError>& errors) {
            for (const QSslError& error : errors) {
                logWarning("{} on {}", error, error.certificate().toText());
            }
            *sslErrors = errors;
        });

        QObject::connect(reply, &QNetworkReply::finished, this, [=, request = std::move(request)]() mutable {
            onRequestFinished(reply, *sslErrors, std::move(request));
        });

        return reply;
    }

    bool Rpc::retryRequest(Request&& request, QNetworkReply* previousAttempt) {
        int retryAttempts{};
        if (const auto found = mRetryingNetworkRequests.find(previousAttempt);
            found != mRetryingNetworkRequests.end()) {
            retryAttempts = found->second;
            mRetryingNetworkRequests.erase(found);
        } else {
            retryAttempts = 0;
        }
        ++retryAttempts;
        if (retryAttempts > maxRetryAttempts) {
            return false;
        }

        request.setSessionId(mSessionId);

        logWarning("Retrying '{}' request, retry attempts = {}", request.method, retryAttempts);
        QNetworkReply* reply = postRequest(std::move(request));
        mRetryingNetworkRequests.emplace(reply, retryAttempts);

        return true;
    }

    void Rpc::postRequest(
        QLatin1String method,
        const QByteArray& data,
        const std::function<void(const QJsonObject&, bool)>& callOnSuccessParse
    ) {
        Request request{method, QNetworkRequest(mServerUrl), data, callOnSuccessParse};
        request.setSessionId(mSessionId);
        request.request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json"_l1);
        request.request.setSslConfiguration(mSslConfiguration);
        request.request.setTransferTimeout(mTimeoutMillis);
        postRequest(std::move(request));
    }

    void Rpc::postRequest(
        QLatin1String method,
        const QVariantMap& arguments,
        const std::function<void(const QJsonObject&, bool)>& callOnSuccessParse
    ) {
        postRequest(method, makeRequestData(method, arguments), callOnSuccessParse);
    }

    void Rpc::onRequestFinished(QNetworkReply* reply, const QList<QSslError>& sslErrors, Request&& request) {
        if (mActiveNetworkRequests.erase(reply) == 0) {
            return;
        }

        if (connectionState() == ConnectionState::Disconnected) {
            return;
        }

        if (reply->error() == QNetworkReply::NoError) {
            mRetryingNetworkRequests.erase(reply);
            const QByteArray replyData(reply->readAll());

            const auto future = QtConcurrent::run([replyData] {
                QJsonParseError error{};
                QJsonObject result(QJsonDocument::fromJson(replyData, &error).object());
                const bool parsedOk = (error.error == QJsonParseError::NoError);
                return std::pair<QJsonObject, bool>(std::move(result), parsedOk);
            });
            auto watcher = new QFutureWatcher<std::pair<QJsonObject, bool>>(this);
            QObject::connect(watcher, &QFutureWatcher<std::pair<QJsonObject, bool>>::finished, this, [=] {
                const auto result = watcher->result();
                if (connectionState() != ConnectionState::Disconnected) {
                    const QJsonObject& parseResult = result.first;
                    const bool parsedOk = result.second;
                    if (parsedOk) {
                        const bool success = isResultSuccessful(parseResult);
                        if (!success) {
                            logWarning("method '{}' failed, response: {}", request.method, parseResult);
                        }
                        if (request.callOnSuccessParse) {
                            request.callOnSuccessParse(parseResult, success);
                        }
                    } else {
                        logWarning("Parsing error");
                        setStatus(Status{ConnectionState::Disconnected, Error::ParseError});
                    }
                }
                watcher->deleteLater();
            });
            watcher->setFuture(future);

            return;
        }

        if (reply->error() == QNetworkReply::ContentConflictError && reply->hasRawHeader(sessionIdHeader)) {
            const auto newSessionId(reply->rawHeader(sessionIdHeader));
            if (newSessionId != request.request.rawHeader(sessionIdHeader)) {
                logInfo("Session id changed, retrying '{}' request", request.method);
                mSessionId = reply->rawHeader(sessionIdHeader);
                // Retry without incrementing retryAttempts
                request.setSessionId(mSessionId);
                postRequest(std::move(request));
                return;
            }
        }

        logWarning("'{}' request error {} {}", request.method, reply->error(), reply->errorString());
        if (auto httpStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
            httpStatusCode.isValid()) {
            logWarning(
                "HTTP status code {} {}",
                httpStatusCode.toInt(),
                reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString()
            );
        }

        const auto createErrorStatus = [&](Error error) {
            auto detailedErrorMessage =
                QString::fromStdString(fmt::format("{}: {}", reply->error(), reply->errorString()));
            if (reply->url() == request.request.url()) {
                detailedErrorMessage += QString::fromStdString(fmt::format("\nURL: {}", reply->url().toString()));
            } else {
                detailedErrorMessage += QString::fromStdString(fmt::format(
                    "\nOriginal URL: {}\nRedirected URL: {}",
                    request.request.url().toString(),
                    reply->url().toString()
                ));
            }
            if (auto httpStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
                httpStatusCode.isValid()) {
                detailedErrorMessage += QString::fromStdString(fmt::format(
                    "\nHTTP status code: {} {}\nEncrypted: {}\nHTTP/2 was used: {}",
                    httpStatusCode.toInt(),
                    reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString(),
                    reply->attribute(QNetworkRequest::ConnectionEncryptedAttribute).toBool(),
                    reply->attribute(QNetworkRequest::Http2WasUsedAttribute).toBool()
                ));
                if (!reply->rawHeaderPairs().isEmpty()) {
                    detailedErrorMessage += "\nReply headers:"_l1;
                    for (const QNetworkReply::RawHeaderPair& pair : reply->rawHeaderPairs()) {
                        detailedErrorMessage +=
                            QString::fromStdString(fmt::format("\n  {}: {}", pair.first, pair.second));
                    }
                }
            } else {
                detailedErrorMessage += "\nDid not establish HTTP connection"_l1;
            }
            if (!sslErrors.isEmpty()) {
                detailedErrorMessage += QString::fromStdString(fmt::format("\n\n{} TLS errors:", sslErrors.size()));
                int i = 1;
                for (const QSslError& sslError : sslErrors) {
                    detailedErrorMessage += QString::fromStdString(fmt::format(
                        "\n\n {}. {}: {} on certificate:\n - {}",
                        i,
                        sslError.error(),
                        sslError.errorString(),
                        sslError.certificate().toText()
                    ));
                    ++i;
                }
            }
            return Status{ConnectionState::Disconnected, error, reply->errorString(), std::move(detailedErrorMessage)};
        };

        switch (reply->error()) {
        case QNetworkReply::AuthenticationRequiredError:
            logWarning("Authentication error");
            setStatus(createErrorStatus(Error::AuthenticationError));
            break;
        case QNetworkReply::OperationCanceledError:
        case QNetworkReply::TimeoutError:
            logWarning("Timed out");
            if (!retryRequest(std::move(request), reply)) {
                setStatus(createErrorStatus(Error::TimedOut));
                if (mAutoReconnectEnabled && !mUpdateDisabled) {
                    logInfo("Auto reconnecting in {} seconds", mAutoReconnectTimer->interval() / 1000);
                    mAutoReconnectTimer->start();
                }
            }
            break;
        default: {
            if (!retryRequest(std::move(request), reply)) {
                setStatus(createErrorStatus(Error::ConnectionError));
                if (mAutoReconnectEnabled && !mUpdateDisabled) {
                    logInfo("Auto reconnecting in {} seconds", mAutoReconnectTimer->interval() / 1000);
                    mAutoReconnectTimer->start();
                }
            }
        }
        }
    }

    void Rpc::checkIfServerIsLocal() {
        logInfo("checkIfServerIsLocal() called");
        if (isSessionIdFileExists()) {
            mServerIsLocal = true;
            logInfo("checkIfServerIsLocal: server is running locally: true");
            checkIfServerIsRunningOnWindows();
            return;
        }
        const auto host = mServerUrl.host();
        if (auto localIp = isLocalIpAddress(host); localIp.has_value()) {
            mServerIsLocal = *localIp;
            logInfo("checkIfServerIsLocal: server is running locally: {}", *mServerIsLocal);
            checkIfServerIsRunningOnWindows();
            return;
        }
        logInfo("checkIfServerIsLocal: resolving IP address for host name {}", host);
        mPendingHostInfoLookupId = QHostInfo::lookupHost(host, this, [=](const QHostInfo& info) {
            logInfo("checkIfServerIsLocal: resolved IP address for host name {}", host);
            const auto addresses = info.addresses();
            if (!addresses.isEmpty()) {
                logInfo("checkIfServerIsLocal: IP addresses:");
                for (const auto& address : addresses) {
                    logInfo("checkIfServerIsLocal: - {}", address);
                }
                logInfo("checkIfServerIsLocal: checking first address");
                mServerIsLocal = isLocalIpAddress(addresses.first());
            } else {
                mServerIsLocal = false;
            }
            logInfo("checkIfServerIsLocal: server is running locally: {}", *mServerIsLocal);
            mPendingHostInfoLookupId = std::nullopt;
            checkIfServerIsRunningOnWindows();
            maybeFinishUpdateOrConnection();
        });
    }

    void Rpc::checkIfServerIsRunningOnWindows() {
        if (!mServerIsLocal.has_value()) return;
        if (*mServerIsLocal) {
            mServerIsRunningOnWindows = isTargetOsWindows;
        } else {
            mServerIsRunningOnWindows = mServerSettings->isRunningOnWindows();
        }
        if (mServerIsRunningOnWindows) {
            logInfo("Server is running on Windows");
        } else {
            logInfo("Server is not running on Windows");
        }
    }

    bool Rpc::isSessionIdFileExists() const {
        if constexpr (targetOs != TargetOs::UnixAndroid) {
            if (mServerSettings->hasSessionIdFile() && !mSessionId.isEmpty()) {
                const auto file = QStandardPaths::locate(sessionIdFileLocation, sessionIdFilePrefix + mSessionId);
                if (!file.isEmpty()) {
                    logInfo(
                        "isSessionIdFileExists: found transmission-daemon session id file {}",
                        QDir::toNativeSeparators(file)
                    );
                    return true;
                }
                logInfo("isSessionIdFileExists: did not find transmission-daemon session id file");
            }
        }
        return false;
    }

    void Rpc::Request::setSessionId(const QByteArray& sessionId) { request.setRawHeader(sessionIdHeader, sessionId); }
}
