// SPDX-FileCopyrightText: 2015-2022 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "rpc.h"

#include <QCoreApplication>
#include <QDir>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonObject>
#include <QHostAddress>
#include <QHostInfo>
#include <QNetworkProxy>
#include <QTimer>
#include <QSslCertificate>
#include <QSslKey>
#include <QStandardPaths>
#include <QtConcurrentRun>

#include "addressutils.h"
#include "itemlistupdater.h"
#include "log.h"
#include "requestrouter.h"
#include "serversettings.h"
#include "serverstats.h"
#include "target_os.h"
#include "torrent.h"

SPECIALIZE_FORMATTER_FOR_QDEBUG(QHostAddress)
SPECIALIZE_FORMATTER_FOR_QDEBUG(QUrl)

namespace libtremotesf {
    namespace {
        // Transmission 2.40+
        constexpr int minimumRpcVersion = 14;

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

    using namespace impl;

    Rpc::Rpc(QObject* parent)
        : QObject(parent),
          mRequestRouter(new RequestRouter(this)),
          mUpdateTimer(new QTimer(this)),
          mAutoReconnectTimer(new QTimer(this)),
          mServerSettings(new ServerSettings(this, this)),
          mServerStats(new ServerStats(this)) {
        mAutoReconnectTimer->setSingleShot(true);
        QObject::connect(mAutoReconnectTimer, &QTimer::timeout, this, [=] {
            logInfo("Auto reconnection");
            connect();
        });

        mUpdateTimer->setSingleShot(true);
        QObject::connect(mUpdateTimer, &QTimer::timeout, this, [=] { updateData(); });

        QObject::connect(
            mRequestRouter,
            &RequestRouter::requestFailed,
            this,
            [=](RpcError error, const QString& errorMessage, const QString& detailedErrorMessage) {
                setStatus({RpcConnectionState::Disconnected, error, errorMessage, detailedErrorMessage});
                if (mAutoReconnectEnabled && !mUpdateDisabled) {
                    logInfo("Auto reconnecting in {} seconds", mAutoReconnectTimer->interval() / 1000);
                    mAutoReconnectTimer->start();
                }
            }
        );
    }

    Rpc::~Rpc() = default;

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
        disconnect();

        RequestRouter::RequestsConfiguration configuration{};
        if (server.https) {
            configuration.serverUrl.setScheme("https"_l1);
        } else {
            configuration.serverUrl.setScheme("http"_l1);
        }
        configuration.serverUrl.setHost(server.address);
        if (auto error = configuration.serverUrl.errorString(); !error.isEmpty()) {
            logWarning("Error setting URL hostname: {}", error);
        }
        configuration.serverUrl.setPort(server.port);
        if (auto error = configuration.serverUrl.errorString(); !error.isEmpty()) {
            logWarning("Error setting URL port: {}", error);
        }
        if (auto i = server.apiPath.indexOf('?'); i != -1) {
            configuration.serverUrl.setPath(server.apiPath.mid(0, i));
            if (auto error = configuration.serverUrl.errorString(); !error.isEmpty()) {
                logWarning("Error setting URL path: {}", error);
            }
            if ((i + 1) < server.apiPath.size()) {
                configuration.serverUrl.setQuery(server.apiPath.mid(i + 1));
                if (auto error = configuration.serverUrl.errorString(); !error.isEmpty()) {
                    logWarning("Error setting URL query: {}", error);
                }
            }
        } else {
            //std::to_string(/*server.port*/);
            configuration.serverUrl.setPath(server.apiPath);
            if (auto error = configuration.serverUrl.errorString(); !error.isEmpty()) {
                logWarning("Error setting URL path: {}", error);
            }
        }
        if (!configuration.serverUrl.isValid()) {
            logWarning("URL {} is invalid", configuration.serverUrl);
        }

        switch (server.proxyType) {
        case Server::ProxyType::Default:
            break;
        case Server::ProxyType::Http:
            configuration.proxy = QNetworkProxy(
                QNetworkProxy::HttpProxy,
                server.proxyHostname,
                static_cast<quint16>(server.proxyPort),
                server.proxyUser,
                server.proxyPassword
            );
            break;
        case Server::ProxyType::Socks5:
            configuration.proxy = QNetworkProxy(
                QNetworkProxy::Socks5Proxy,
                server.proxyHostname,
                static_cast<quint16>(server.proxyPort),
                server.proxyUser,
                server.proxyPassword
            );
            break;
        }

        if (server.https && server.selfSignedCertificateEnabled) {
            configuration.serverCertificateChain = QSslCertificate::fromData(server.selfSignedCertificate, QSsl::Pem);
        }

        if (server.clientCertificateEnabled) {
            configuration.clientCertificate = QSslCertificate(server.clientCertificate, QSsl::Pem);
            configuration.clientPrivateKey = QSslKey(server.clientCertificate, QSsl::Rsa);
        }

        configuration.authentication = server.authentication;
        configuration.username = server.username;
        configuration.password = server.password;
        configuration.timeout = std::chrono::seconds(server.timeout); // server.timeout is in seconds

        mRequestRouter->setConfiguration(std::move(configuration));

        mUpdateTimer->setInterval(server.updateInterval * 1000); // msecs

        mAutoReconnectEnabled = server.autoReconnectEnabled;
        mAutoReconnectTimer->setInterval(server.autoReconnectInterval * 1000); // msecs
        mAutoReconnectTimer->stop();
    }

    void Rpc::resetServer() {
        disconnect();
        mRequestRouter->setConfiguration({});
        mAutoReconnectEnabled = false;
        mAutoReconnectTimer->stop();
    }

    void Rpc::connect() {
        if (connectionState() == ConnectionState::Disconnected &&
            !mRequestRouter->configuration().serverUrl.isEmpty()) {
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
            return RequestRouter::makeRequestData(
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
                mRequestRouter
                    ->postRequest("torrent-add"_l1, watcher->result(), [=](const RequestRouter::Response& response) {
                        if (response.success) {
                            if (response.arguments.contains(torrentDuplicateKey)) {
                                emit torrentAddDuplicate();
                            } else {
                                if (!renamedFiles.isEmpty()) {
                                    const QJsonObject torrentJson(
                                        response.arguments.value("torrent-added"_l1).toObject()
                                    );
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
            mRequestRouter->postRequest(
                "torrent-add"_l1,
                {{"filename"_l1, link},
                 {"download-dir"_l1, downloadDirectory},
                 {"bandwidthPriority"_l1, bandwidthPriority},
                 {"paused"_l1, !start}},
                [=](const RequestRouter::Response& response) {
                    if (response.success) {
                        if (response.arguments.contains(torrentDuplicateKey)) {
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
            mRequestRouter
                ->postRequest("torrent-start"_l1, {{"ids"_l1, ids}}, [=](const RequestRouter::Response& response) {
                    if (response.success) {
                        updateData();
                    }
                });
        }
    }

    void Rpc::startTorrentsNow(const QVariantList& ids) {
        if (isConnected()) {
            mRequestRouter
                ->postRequest("torrent-start-now"_l1, {{"ids"_l1, ids}}, [=](const RequestRouter::Response& response) {
                    if (response.success) {
                        updateData();
                    }
                });
        }
    }

    void Rpc::pauseTorrents(const QVariantList& ids) {
        if (isConnected()) {
            mRequestRouter
                ->postRequest("torrent-stop"_l1, {{"ids"_l1, ids}}, [=](const RequestRouter::Response& response) {
                    if (response.success) {
                        updateData();
                    }
                });
        }
    }

    void Rpc::removeTorrents(const QVariantList& ids, bool deleteFiles) {
        if (isConnected()) {
            mRequestRouter->postRequest(
                "torrent-remove"_l1,
                {{"ids"_l1, ids}, {"delete-local-data"_l1, deleteFiles}},
                [=](const RequestRouter::Response& response) {
                    if (response.success) {
                        updateData();
                    }
                }
            );
        }
    }

    void Rpc::checkTorrents(const QVariantList& ids) {
        if (isConnected()) {
            mRequestRouter
                ->postRequest("torrent-verify"_l1, {{"ids"_l1, ids}}, [=](const RequestRouter::Response& response) {
                    if (response.success) {
                        updateData();
                    }
                });
        }
    }

    void Rpc::moveTorrentsToTop(const QVariantList& ids) {
        if (isConnected()) {
            mRequestRouter
                ->postRequest("queue-move-top"_l1, {{"ids"_l1, ids}}, [=](const RequestRouter::Response& response) {
                    if (response.success) {
                        updateData();
                    }
                });
        }
    }

    void Rpc::moveTorrentsUp(const QVariantList& ids) {
        if (isConnected()) {
            mRequestRouter
                ->postRequest("queue-move-up"_l1, {{"ids"_l1, ids}}, [=](const RequestRouter::Response& response) {
                    if (response.success) {
                        updateData();
                    }
                });
        }
    }

    void Rpc::moveTorrentsDown(const QVariantList& ids) {
        if (isConnected()) {
            mRequestRouter
                ->postRequest("queue-move-down"_l1, {{"ids"_l1, ids}}, [=](const RequestRouter::Response& response) {
                    if (response.success) {
                        updateData();
                    }
                });
        }
    }

    void Rpc::moveTorrentsToBottom(const QVariantList& ids) {
        if (isConnected()) {
            mRequestRouter
                ->postRequest("queue-move-bottom"_l1, {{"ids"_l1, ids}}, [=](const RequestRouter::Response& response) {
                    if (response.success) {
                        updateData();
                    }
                });
        }
    }

    void Rpc::reannounceTorrents(const QVariantList& ids) {
        if (isConnected()) {
            mRequestRouter->postRequest("torrent-reannounce"_l1, {{"ids"_l1, ids}});
        }
    }

    void Rpc::setSessionProperty(const QString& property, const QVariant& value) {
        setSessionProperties({{property, value}});
    }

    void Rpc::setSessionProperties(const QVariantMap& properties) {
        if (isConnected()) {
            mRequestRouter->postRequest("session-set"_l1, properties);
        }
    }

    void Rpc::setTorrentProperty(int id, const QString& property, const QVariant& value, bool updateIfSuccessful) {
        if (isConnected()) {
            mRequestRouter->postRequest(
                "torrent-set"_l1,
                {{"ids"_l1, QVariantList{id}}, {property, value}},
                [=](const RequestRouter::Response& response) {
                    if (response.success && updateIfSuccessful) {
                        updateData();
                    }
                }
            );
        }
    }

    void Rpc::setTorrentsLocation(const QVariantList& ids, const QString& location, bool moveFiles) {
        if (isConnected()) {
            mRequestRouter->postRequest(
                "torrent-set-location"_l1,
                {{"ids"_l1, ids}, {"location"_l1, location}, {"move"_l1, moveFiles}},
                [=](const RequestRouter::Response& response) {
                    if (response.success) {
                        updateData();
                    }
                }
            );
        }
    }

    void Rpc::getTorrentsFiles(const QVariantList& ids, bool scheduled) {
        mRequestRouter->postRequest(
            "torrent-get"_l1,
            {{"fields"_l1, QStringList{"id"_l1, "files"_l1, "fileStats"_l1}}, {"ids"_l1, ids}},
            [=](const RequestRouter::Response& response) {
                if (response.success) {
                    const QJsonArray torrents(response.arguments.value(torrentsKey).toArray());
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
        mRequestRouter->postRequest(
            "torrent-get"_l1,
            {{"fields"_l1, QStringList{"id"_l1, "peers"_l1}}, {"ids"_l1, ids}},
            [=](const RequestRouter::Response& response) {
                if (response.success) {
                    const QJsonArray torrents(response.arguments.value(torrentsKey).toArray());
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
            mRequestRouter->postRequest(
                "torrent-rename-path"_l1,
                {{"ids"_l1, QVariantList{torrentId}}, {"path"_l1, filePath}, {"name"_l1, newName}},
                [=](const RequestRouter::Response& response) {
                    if (response.success) {
                        Torrent* torrent = torrentById(torrentId);
                        if (torrent) {
                            const QString path(response.arguments.value("path"_l1).toString());
                            const QString newName(response.arguments.value("name"_l1).toString());
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
            mRequestRouter->postRequest(
                "download-dir-free-space"_l1,
                QByteArrayLiteral("{"
                                  "\"arguments\":{"
                                  "\"fields\":["
                                  "\"download-dir-free-space\""
                                  "]"
                                  "},"
                                  "\"method\":\"session-get\""
                                  "}"),
                [=](const RequestRouter::Response& response) {
                    if (response.success) {
                        emit gotDownloadDirFreeSpace(
                            static_cast<long long>(response.arguments.value("download-dir-free-space"_l1).toDouble())
                        );
                    }
                }
            );
        }
    }

    void Rpc::getFreeSpaceForPath(const QString& path) {
        if (isConnected()) {
            mRequestRouter
                ->postRequest("free-space"_l1, {{"path"_l1, path}}, [=](const RequestRouter::Response& response) {
                    emit gotFreeSpaceForPath(
                        path,
                        response.success,
                        response.success ? static_cast<long long>(response.arguments.value("size-bytes"_l1).toDouble())
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
            mRequestRouter
                ->postRequest("session-close"_l1, QVariantMap{}, [=](const RequestRouter::Response& response) {
                    if (response.success) {
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

            mRequestRouter->cancelPendingRequestsAndClearSessionId();

            mUpdating = false;
            mServerIsLocal = std::nullopt;
            if (mPendingHostInfoLookupId.has_value()) {
                QHostInfo::abortHostLookup(*mPendingHostInfoLookupId);
                mPendingHostInfoLookupId = std::nullopt;
            }
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
        mRequestRouter->postRequest(
            "session-get"_l1,
            QByteArrayLiteral("{\"method\":\"session-get\"}"),
            [=](const RequestRouter::Response& response) {
                if (response.success) {
                    mServerSettings->update(response.arguments);
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

        std::vector<std::pair<int, int>> removedIndexRanges{};
        std::vector<std::pair<int, int>> changedIndexRanges{};
        int addedCount{};
        QVariantList metadataCompletedIds{};

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
                // Don't emit torrentFinished() if torrent's size became smaller
                // since there is high chance that it happened because user unselected some files
                // and torrent immediately became finished. We don't want notification in that case
                if (!wasFinished && torrent->isFinished() && !wasPaused && torrent->sizeWhenDone() >= oldSizeWhenDone) {
                    emit mRpc.torrentFinished(torrent.get());
                }
                if (!metadataWasComplete && torrent->isMetadataComplete()) {
                    metadataCompletedIds.push_back(id);
                }
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
                metadataCompletedIds.push_back(id);
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
        mRequestRouter->postRequest(
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
            [=](const RequestRouter::Response& response) {
                if (!response.success) {
                    return;
                }

                std::vector<NewTorrent> newTorrents;
                {
                    const QJsonArray torrentsJsons = response.arguments.value("torrents"_l1).toArray();
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

                if (!updater.metadataCompletedIds.isEmpty()) {
                    checkTorrentsSingleFile(updater.metadataCompletedIds);
                }
                QVariantList getFilesIds{};
                QVariantList getPeersIds{};
                for (const auto& torrent : mTorrents) {
                    if (torrent->isFilesEnabled()) {
                        getFilesIds.push_back(torrent->id());
                    }
                    if (torrent->isPeersEnabled()) {
                        getPeersIds.push_back(torrent->id());
                    }
                }
                if (getFilesIds.isEmpty()) {
                    getTorrentsFiles(getFilesIds, true);
                }
                if (getPeersIds.isEmpty()) {
                    getTorrentsPeers(getPeersIds, true);
                }
            }
        );
    }

    void Rpc::checkTorrentsSingleFile(const QVariantList& torrentIds) {
        mRequestRouter->postRequest(
            "torrent-get"_l1,
            {{"fields"_l1, QVariantList{"id"_l1, "priorities"_l1}}, {"ids"_l1, torrentIds}},
            [=](const RequestRouter::Response& response) {
                if (response.success) {
                    const QJsonArray torrents(response.arguments.value(torrentsKey).toArray());
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
        mRequestRouter->postRequest(
            "session-stats"_l1,
            QByteArrayLiteral("{\"method\":\"session-stats\"}"),
            [=](const RequestRouter::Response& response) {
                if (response.success) {
                    mServerStats->update(response.arguments);
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

    void Rpc::checkIfServerIsLocal() {
        logInfo("checkIfServerIsLocal() called");
        if (isSessionIdFileExists()) {
            mServerIsLocal = true;
            logInfo("checkIfServerIsLocal: server is running locally: true");
            return;
        }
        const auto host = mRequestRouter->configuration().serverUrl.host();
        if (auto localIp = isLocalIpAddress(host); localIp.has_value()) {
            mServerIsLocal = *localIp;
            logInfo("checkIfServerIsLocal: server is running locally: {}", *mServerIsLocal);
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
            maybeFinishUpdateOrConnection();
        });
    }

    bool Rpc::isSessionIdFileExists() const {
        if constexpr (targetOs != TargetOs::UnixAndroid) {
            if (mServerSettings->hasSessionIdFile() && !mRequestRouter->sessionId().isEmpty()) {
                const auto file =
                    QStandardPaths::locate(sessionIdFileLocation, sessionIdFilePrefix + mRequestRouter->sessionId());
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
}
