/*
 * Tremotesf
 * Copyright (C) 2015-2018 Alexey Rochev <equeim@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "rpc.h"

#include <QAuthenticator>
#include <QCoreApplication>
#include <QDebug>
#include <QFutureWatcher>
#include <QHostAddress>
#include <QHostInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkInterface>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QTimer>
#include <QSslCertificate>
#include <QSslKey>
#include <QtConcurrentRun>

#ifdef TREMOTESF_SAILFISHOS
#include <QQmlEngine>
#endif

#include "serversettings.h"
#include "serverstats.h"
#include "stdutils.h"
#include "torrent.h"

namespace libtremotesf
{
    namespace
    {
        // Transmission 2.40+
        const int minimumRpcVersion = 14;

        const QByteArray sessionIdHeader(QByteArrayLiteral("X-Transmission-Session-Id"));
        const auto torrentsKey(QJsonKeyStringInit("torrents"));
        const QLatin1String torrentDuplicateKey("torrent-duplicate");

        inline QByteArray makeRequestData(const QString& method, const QVariantMap& arguments)
        {
            return QJsonDocument::fromVariant(QVariantMap{{QStringLiteral("method"), method},
                                                          {QStringLiteral("arguments"), arguments}})
                .toJson(QJsonDocument::Compact);
        }

        inline QJsonObject getReplyArguments(const QJsonObject& parseResult)
        {
            return parseResult.value(QJsonKeyStringInit("arguments")).toObject();
        }

        inline bool isResultSuccessful(const QJsonObject& parseResult)
        {
            return (parseResult.value(QJsonKeyStringInit("result")).toString() == QLatin1String("success"));
        }

        bool isAddressLocal(const QString& address)
        {
            if (address == QHostInfo::localHostName()) {
                return true;
            }

            const QHostAddress ipAddress(address);

            if (ipAddress.isNull()) {
                return address == QHostInfo::fromName(QHostAddress(QHostAddress::LocalHost).toString()).hostName() ||
                       address == QHostInfo::fromName(QHostAddress(QHostAddress::LocalHostIPv6).toString()).hostName();
            }

            return ipAddress.isLoopback() || QNetworkInterface::allAddresses().contains(ipAddress);
        }
    }

    Rpc::Rpc(QObject* parent)
        : QObject(parent),
          mNetwork(new QNetworkAccessManager(this)),
          mAuthenticationRequested(false),
          mBackgroundUpdate(false),
          mUpdateDisabled(false),
          mUpdating(false),
          mAuthentication(false),
          mUpdateInterval(0),
          mBackgroundUpdateInterval(0),
          mTimeout(0),
          mLocal(false),
          mRpcVersionChecked(false),
          mServerSettingsUpdated(false),
          mTorrentsUpdated(false),
          mServerStatsUpdated(false),
          mUpdateTimer(new QTimer(this)),
          mServerSettings(new ServerSettings(this, this)),
          mServerStats(new ServerStats(this)),
          mStatus(Disconnected),
          mError(NoError)
    {
        QObject::connect(mNetwork, &QNetworkAccessManager::authenticationRequired, this, &Rpc::onAuthenticationRequired);

        mUpdateTimer->setSingleShot(true);
        QObject::connect(mUpdateTimer, &QTimer::timeout, this, &Rpc::updateData);

        QObject::connect(mNetwork, &QNetworkAccessManager::sslErrors, this, [=](QNetworkReply*, const QList<QSslError>& errors) {
            for (const auto& error : errors) {
                if (!mExpectedSslErrors.contains(error)) {
                    qWarning() << error;
                }
            }
        });
    }

    ServerSettings* Rpc::serverSettings() const
    {
        return mServerSettings;
    }

    ServerStats* Rpc::serverStats() const
    {
        return mServerStats;
    }

    const std::vector<std::shared_ptr<Torrent>>& Rpc::torrents() const
    {
        return mTorrents;
    }

    Torrent* Rpc::torrentByHash(const QString& hash) const
    {
        for (const std::shared_ptr<Torrent>& torrent : mTorrents) {
            if (torrent->hashString() == hash) {
                return torrent.get();
            }
        }
        return nullptr;
    }

    Torrent* Rpc::torrentById(int id) const
    {
        const auto end(mTorrents.end());
        const auto found(std::find_if(mTorrents.begin(), mTorrents.end(), [id](const std::shared_ptr<Torrent>& torrent) {
            return (torrent->id() == id);
        }));
        return (found == end) ? nullptr : found->get();
    }

    bool Rpc::isConnected() const
    {
        return (mStatus == Connected);
    }

    Rpc::Status Rpc::status() const
    {
        return mStatus;
    }

    Rpc::Error Rpc::error() const
    {
        return mError;
    }

    const QString& Rpc::errorMessage() const
    {
        return mErrorMessage;
    }

    bool Rpc::isLocal() const
    {
        return mLocal;
    }

    int Rpc::torrentsCount() const
    {
        return static_cast<int>(mTorrents.size());
    }

    bool Rpc::backgroundUpdate() const
    {
        return mBackgroundUpdate;
    }

    void Rpc::setBackgroundUpdate(bool background)
    {
        if (background != mBackgroundUpdate) {
            mBackgroundUpdate = background;
            const int interval = background ? mBackgroundUpdateInterval : mUpdateInterval;
            if (mUpdateTimer->isActive()) {
                mUpdateTimer->stop();
                mUpdateTimer->setInterval(interval);
                startUpdateTimer();
            } else if (!mServerUrl.isEmpty()) {
                mUpdateTimer->setInterval(interval);
            }
            emit backgroundUpdateChanged();
        }
    }

    bool Rpc::isUpdateDisabled() const
    {
        return mUpdateDisabled;
    }

    void Rpc::setUpdateDisabled(bool disabled)
    {
        if (disabled != mUpdateDisabled) {
            mUpdateDisabled = disabled;
            if (isConnected()) {
                if (disabled) {
                    mUpdateTimer->stop();
                } else {
                    updateData();
                }
            }
            emit updateDisabledChanged();
        }
    }

    void Rpc::setServer(const Server& server)
    {
        mNetwork->clearAccessCache();
        disconnect();

        mServerUrl.setHost(server.address);
        mServerUrl.setPort(server.port);
        mServerUrl.setPath(server.apiPath);
        if (server.https) {
            mServerUrl.setScheme(QLatin1String("https"));
        } else {
            mServerUrl.setScheme(QLatin1String("http"));
        }

        switch (server.proxyType) {
        case Server::ProxyType::Default:
            mNetwork->setProxy(QNetworkProxy::applicationProxy());
            break;
        case Server::ProxyType::Http:
            mNetwork->setProxy(QNetworkProxy(QNetworkProxy::HttpProxy,
                                             server.proxyHostname,
                                             static_cast<quint16>(server.proxyPort),
                                             server.proxyUser,
                                             server.proxyPassword));
            break;
        case Server::ProxyType::Socks5:
            mNetwork->setProxy(QNetworkProxy(QNetworkProxy::Socks5Proxy,
                                             server.proxyHostname,
                                             static_cast<quint16>(server.proxyPort),
                                             server.proxyUser,
                                             server.proxyPassword));
            break;
        }

        mSslConfiguration = QSslConfiguration::defaultConfiguration();
        mExpectedSslErrors.clear();

        if (server.selfSignedCertificateEnabled) {
            const QSslCertificate certificate(server.selfSignedCertificate);
            mExpectedSslErrors.reserve(2);
            mExpectedSslErrors.push_back(QSslError(QSslError::HostNameMismatch, certificate));
            mExpectedSslErrors.push_back(QSslError(QSslError::SelfSignedCertificate, certificate));
        }

        if (server.clientCertificateEnabled) {
            mSslConfiguration.setLocalCertificate(QSslCertificate(server.clientCertificate));
            mSslConfiguration.setPrivateKey(QSslKey(server.clientCertificate, QSsl::Rsa));
        }

        mAuthentication = server.authentication;
        mUsername = server.username;
        mPassword = server.password;
        mTimeout = server.timeout * 1000; // msecs
        mUpdateInterval = server.updateInterval * 1000; // msecs
        mBackgroundUpdateInterval = server.backgroundUpdateInterval * 1000; // msecs
        mUpdateTimer->setInterval(mUpdateInterval);

        mLocal = isAddressLocal(server.address);
    }

    void Rpc::resetServer()
    {
        disconnect();
        mServerUrl.clear();
        mSslConfiguration = QSslConfiguration::defaultConfiguration();
        mExpectedSslErrors.clear();
        mAuthentication = false;
        mUsername.clear();
        mPassword.clear();
        mUpdateInterval = 0;
        mBackgroundUpdateInterval = 0;
        mTimeout = 0;
        mLocal = false;
    }

    void Rpc::connect()
    {
        if (mStatus == Disconnected && !mServerUrl.isEmpty()) {
            setError(NoError);
            setStatus(Connecting);
            getServerSettings();
        }
    }

    void Rpc::disconnect()
    {
        setError(NoError);
        setStatus(Disconnected);
    }

    void Rpc::addTorrentFile(const QByteArray& fileData,
                             const QString& downloadDirectory,
                             const QVariantList& unwantedFiles,
                             const QVariantList& highPriorityFiles,
                             const QVariantList& lowPriorityFiles,
                             const QVariantMap& renamedFiles,
                             int bandwidthPriority,
                             bool start)
    {
        if (isConnected()) {
            const auto future = QtConcurrent::run([=] {
                return makeRequestData(QLatin1String("torrent-add"),
                                       {{QLatin1String("metainfo"), QString::fromLatin1(fileData.toBase64())},
                                        {QLatin1String("download-dir"), downloadDirectory},
                                        {QLatin1String("files-unwanted"), unwantedFiles},
                                        {QLatin1String("priority-high"), highPriorityFiles},
                                        {QLatin1String("priority-low"), lowPriorityFiles},
                                        {QLatin1String("bandwidthPriority"), bandwidthPriority},
                                        {QLatin1String("paused"), !start}});
            });
            auto watcher = new QFutureWatcher<QByteArray>(this);
            QObject::connect(watcher, &QFutureWatcher<QByteArray>::finished, this, [=] {
                if (isConnected()) {
                    postRequest(QLatin1String("torrent-add"), watcher->result(), [=](const QJsonObject& parseResult, bool success) {
                        if (success) {
                            const auto arguments(getReplyArguments(parseResult));
                            if (arguments.contains(torrentDuplicateKey)) {
                                emit torrentAddDuplicate();
                            } else {
                                if (!renamedFiles.isEmpty()) {
                                    const QJsonObject torrentJson(arguments.value(QLatin1String("torrent-added")).toObject());
                                    if (!torrentJson.isEmpty()) {
                                        const int id = torrentJson.value(Torrent::idKey).toInt();
                                        for (auto i = renamedFiles.begin(), end = renamedFiles.end();
                                             i != end;
                                             ++i) {
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
    }

    void Rpc::addTorrentLink(const QString& link,
                             const QString& downloadDirectory,
                             int bandwidthPriority,
                             bool start)
    {
        if (isConnected()) {
            postRequest(QLatin1String("torrent-add"),
                        {{QLatin1String("filename"), link},
                         {QLatin1String("download-dir"), downloadDirectory},
                         {QLatin1String("bandwidthPriority"), bandwidthPriority},
                         {QLatin1String("paused"), !start}},
                    [=](const QJsonObject& parseResult, bool success) {
                        if (success) {
                            if (getReplyArguments(parseResult).contains(torrentDuplicateKey)) {
                                emit torrentAddDuplicate();
                            } else {
                                updateData();
                            }
                        } else {
                            emit torrentAddError();
                        }
                    });
        }
    }

    void Rpc::startTorrents(const QVariantList& ids)
    {
        if (isConnected()) {
            postRequest(QLatin1String("torrent-start"),
                        {{QLatin1String("ids"), ids}},
                        [=](const QJsonObject&, bool success) {
                            if (success) {
                                updateData();
                            }
                        });
        }
    }

    void Rpc::startTorrentsNow(const QVariantList& ids)
    {
        if (isConnected()) {
            postRequest(QLatin1String("torrent-start-now"),
                        {{QLatin1String("ids"), ids}},
                        [=](const QJsonObject&, bool success) {
                            if (success) {
                                updateData();
                            }
                        });
        }
    }

    void Rpc::pauseTorrents(const QVariantList& ids)
    {
        if (isConnected()) {
            postRequest(QLatin1String("torrent-stop"),
                        {{QLatin1String("ids"), ids}},
                        [=](const QJsonObject&, bool success) {
                            if (success) {
                                updateData();
                            }
                        });
        }
    }

    void Rpc::removeTorrents(const QVariantList& ids, bool deleteFiles)
    {
        if (isConnected()) {
            postRequest(QLatin1String("torrent-remove"),
                        {{QLatin1String("ids"), ids},
                         {QLatin1String("delete-local-data"), deleteFiles}},
                        [=](const QJsonObject&, bool success) {
                            if (success) {
                                updateData();
                            }
                        });
        }
    }

    void Rpc::checkTorrents(const QVariantList& ids)
    {
        if (isConnected()) {
            postRequest(QLatin1String("torrent-verify"),
                        {{QLatin1String("ids"), ids}},
                        [=](const QJsonObject&, bool success) {
                            if (success) {
                                updateData();
                            }
                        });
        }
    }

    void Rpc::moveTorrentsToTop(const QVariantList& ids)
    {
        if (isConnected()) {
            postRequest(QLatin1String("queue-move-top"),
                        {{QLatin1String("ids"), ids}},
                        [=](const QJsonObject&, bool success) {
                            if (success) {
                                updateData();
                            }
                        });
        }
    }

    void Rpc::moveTorrentsUp(const QVariantList& ids)
    {
        if (isConnected()) {
            postRequest(QLatin1String("queue-move-up"),
                        {{QLatin1String("ids"), ids}},
                        [=](const QJsonObject&, bool success) {
                            if (success) {
                                updateData();
                            }
                        });
        }
    }

    void Rpc::moveTorrentsDown(const QVariantList& ids)
    {
        if (isConnected()) {
            postRequest(QLatin1String("queue-move-down"),
                        {{QLatin1String("ids"), ids}},
                        [=](const QJsonObject&, bool success) {
                            if (success) {
                                updateData();
                            }
                        });
        }
    }

    void Rpc::moveTorrentsToBottom(const QVariantList& ids)
    {
        if (isConnected()) {
            postRequest(QLatin1String("queue-move-bottom"),
                        {{QLatin1String("ids"), ids}},
                        [=](const QJsonObject&, bool success) {
                            if (success) {
                                updateData();
                            }
                        });
        }
    }

    void Rpc::reannounceTorrents(const QVariantList& ids)
    {
        if (isConnected()) {
            postRequest(QLatin1String("torrent-reannounce"),
                        {{QLatin1String("ids"), ids}});
        }
    }

    void Rpc::setSessionProperty(const QString& property, const QVariant& value)
    {
        setSessionProperties({{property, value}});
    }

    void Rpc::setSessionProperties(const QVariantMap& properties)
    {
        if (isConnected()) {
            postRequest(QLatin1String("session-set"), properties);
        }
    }

    void Rpc::setTorrentProperty(int id, const QString& property, const QVariant& value, bool updateIfSuccessful)
    {
        if (isConnected()) {
            postRequest(QLatin1String("torrent-set"),
                        {{QLatin1String("ids"), QVariantList{id}},
                         {property, value}},
                        [=](const QJsonObject&, bool success) {
                            if (success && updateIfSuccessful) {
                                updateData();
                            }
            });
        }
    }

    void Rpc::setTorrentsLocation(const QVariantList& ids, const QString& location, bool moveFiles)
    {
        if (isConnected()) {
            postRequest(QLatin1String("torrent-set-location"),
                        {{QLatin1String("ids"), ids},
                         {QLatin1String("location"), location},
                         {QLatin1String("move"), moveFiles}},
                        [=](const QJsonObject&, bool success) {
                if (success) {
                    updateData();
                }
            });
        }
    }

    void Rpc::getTorrentFiles(int id, bool scheduled)
    {
        postRequest(QLatin1String("torrent-get"),
                    QStringLiteral("{"
                                        "\"arguments\":{"
                                            "\"fields\":["
                                                "\"files\","
                                                "\"fileStats\""
                                            "],"
                                            "\"ids\":[%1]"
                                        "},"
                                        "\"method\":\"torrent-get\""
                                   "}")
                        .arg(id)
                        .toLatin1(),
                    [=](const QJsonObject& parseResult, bool success) {
                        if (success) {
                            const QJsonArray torrentsVariants(getReplyArguments(parseResult)
                                                                    .value(torrentsKey)
                                                                    .toArray());
                            Torrent* torrent = torrentById(id);
                            if (!torrentsVariants.isEmpty() && torrent) {
                                if (torrent->isFilesEnabled()) {
                                    torrent->updateFiles(torrentsVariants.first().toObject());
                                }
                                if (scheduled) {
                                    checkIfTorrentsUpdated();
                                    startUpdateTimer();
                                }
                            }
                        }
                    });
    }

    void Rpc::getTorrentPeers(int id, bool scheduled)
    {
        postRequest(QLatin1String("torrent-get"),
                    QStringLiteral("{"
                                       "\"arguments\":{"
                                           "\"fields\":[\"peers\"],"
                                           "\"ids\":[%1]"
                                       "},"
                                       "\"method\":\"torrent-get\""
                                   "}")
                        .arg(id)
                        .toLatin1(),
                    [=](const QJsonObject& parseResult, bool success) {
                        if (success) {
                            const QJsonArray torrentsVariants(getReplyArguments(parseResult)
                                                                    .value(torrentsKey)
                                                                    .toArray());
                            Torrent* torrent = torrentById(id);
                            if (!torrentsVariants.isEmpty() && torrent) {
                                if (torrent->isPeersEnabled()) {
                                    torrent->updatePeers(torrentsVariants.first().toObject());
                                }
                                if (scheduled) {
                                    checkIfTorrentsUpdated();
                                    startUpdateTimer();
                                }
                            }
                        }
                    });
    }

    void Rpc::renameTorrentFile(int torrentId, const QString& filePath, const QString& newName)
    {
        if (isConnected()) {
            postRequest(QLatin1String("torrent-rename-path"),
                        {{QLatin1String("ids"), QVariantList{torrentId}},
                         {QLatin1String("path"), filePath},
                         {QLatin1String("name"), newName}},
                        [=](const QJsonObject& parseResult, bool success) {
                            if (success) {
                                Torrent* torrent = torrentById(torrentId);
                                if (torrent) {
                                    const QJsonObject arguments(getReplyArguments(parseResult));
                                    const QString path(arguments.value(QLatin1String("path")).toString());
                                    const QString newName(arguments.value(QLatin1String("name")).toString());
                                    emit torrent->fileRenamed(path, newName);
                                    emit torrentFileRenamed(torrentId, path, newName);
                                    updateData();
                                }
                            }
                        });
        }
    }

    void Rpc::getDownloadDirFreeSpace()
    {
        if (isConnected()) {
            postRequest(QLatin1String("download-dir-free-space"),
                        QByteArrayLiteral(
                        "{"
                            "\"arguments\":{"
                                "\"fields\":["
                                    "\"download-dir-free-space\""
                                "]"
                            "},"
                            "\"method\":\"session-get\""
                        "}"),
                        [=](const QJsonObject& parseResult, bool success) {
                            if (success) {
                                emit gotDownloadDirFreeSpace(static_cast<long long>(getReplyArguments(parseResult).value(QJsonKeyStringInit("download-dir-free-space")).toDouble()));
                            }
                        });
        }
    }

    void Rpc::getFreeSpaceForPath(const QString& path)
    {
        if (isConnected()) {
            postRequest(QLatin1String("free-space"),
                        {{QLatin1String("path"), path}},
                        [=](const QJsonObject& parseResult, bool success) {
                            emit gotFreeSpaceForPath(path,
                                                     success,
                                                     success ? static_cast<long long>(getReplyArguments(parseResult).value(QJsonKeyStringInit("size-bytes")).toDouble()) : 0);
                        });
        }
    }

    void Rpc::updateData()
    {
        if (isConnected() && !mUpdating) {
            mServerSettingsUpdated = false;
            mTorrentsUpdated = false;
            mServerStatsUpdated = false;

            mUpdateTimer->stop();

            mUpdating = true;

            getServerSettings();
            getTorrents();
            getServerStats();
        }
    }

    void Rpc::setStatus(Status status)
    {
        if (status == mStatus) {
            return;
        }

        const bool wasConnected = isConnected();

        if (wasConnected && (status == Disconnected)) {
            emit aboutToDisconnect();
        }

        mStatus = status;

        switch (mStatus) {
        case Disconnected:
        {
            qDebug("Disconnected");

            mNetwork->clearAccessCache();

            for (QNetworkReply* reply : mNetworkRequests) {
                reply->abort();
            }
            mNetworkRequests.clear();

            mUpdating = false;

            mAuthenticationRequested = false;
            mRpcVersionChecked = false;
            mServerSettingsUpdated = false;
            mTorrentsUpdated = false;
            mServerStatsUpdated = false;
            mUpdateTimer->stop();

            emit statusChanged();

            if (wasConnected) {
                emit connectedChanged();
            }

            if (!mTorrents.empty()) {
                std::vector<int> removed;
                removed.reserve(mTorrents.size());
                for (int i = static_cast<int>(mTorrents.size()) - 1; i >= 0; --i) {
                    removed.push_back(i);
                }
                mTorrents.clear();
                emit torrentsUpdated(removed, {}, 0);
            }

            break;
        }
        case Connecting:
            qDebug("Connecting");
            mUpdating = true;
            emit statusChanged();
            break;
        case Connected:
        {
            qDebug("Connected");
            emit statusChanged();
            emit connectedChanged();
            break;
        }
        }
    }

    void Rpc::setError(Error error, const QString& errorMessage)
    {
        if (error != mError) {
            mError = error;
            mErrorMessage = errorMessage;
            emit errorChanged();
        }
    }

    void Rpc::getServerSettings()
    {
        postRequest(QLatin1String("session-get"), QByteArrayLiteral("{\"method\":\"session-get\"}"),
                    [=](const QJsonObject& parseResult, bool success) {
                        if (success) {
                            mServerSettings->update(getReplyArguments(parseResult));
                            mServerSettingsUpdated = true;
                            if (mRpcVersionChecked) {
                                startUpdateTimer();
                            } else {
                                mRpcVersionChecked = true;
                                if (mServerSettings->minimumRpcVersion() > minimumRpcVersion) {
                                    setError(ServerIsTooNew);
                                    setStatus(Disconnected);
                                } else if (mServerSettings->rpcVersion() < minimumRpcVersion) {
                                    setError(ServerIsTooOld);
                                    setStatus(Disconnected);
                                } else {
                                    getTorrents();
                                    getServerStats();
                                }
                            }
                        }
                    });
    }

    void Rpc::getTorrents()
    {
        postRequest(QLatin1String("torrent-get"),
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
                                                  "\"uploadRatio\""
                                              "]"
                                          "},"
                                          "\"method\":\"torrent-get\""
                                      "}"),
                    [=](const QJsonObject& parseResult, bool success) {
                        if (!success) {
                            return;
                        }

                        std::vector<std::tuple<QJsonObject, int, bool>> newTorrents;
                        {
                            const QJsonArray torrentsJsons(getReplyArguments(parseResult)
                                                            .value(QJsonKeyStringInit("torrents"))
                                                            .toArray());
                            newTorrents.reserve(static_cast<size_t>(torrentsJsons.size()));
                            for (const QJsonValue& torrentValue : torrentsJsons) {
                                QJsonObject torrentJson(torrentValue.toObject());
                                const int id = torrentJson.value(Torrent::idKey).toInt();
                                newTorrents.emplace_back(std::move(torrentJson), id, false);
                            }
                        }

                        std::vector<int> removed;
                        if (newTorrents.size() < mTorrents.size()) {
                            removed.reserve(mTorrents.size() - newTorrents.size());
                        }
                        std::vector<int> changed;
                        QVariantList checkSingleFile;
                        if (newTorrents.size() > mTorrents.size()) {
                            checkSingleFile.reserve(static_cast<int>(newTorrents.size() - mTorrents.size()));
                        }

                        {
                            const auto newTorrentsEnd(newTorrents.end());
                            VectorBatchRemover<std::shared_ptr<Torrent>> remover(mTorrents, &removed, &changed);
                            for (int i = static_cast<int>(mTorrents.size()) - 1; i >= 0; --i) {
                                const auto& torrent = mTorrents[static_cast<size_t>(i)];
                                const int id = torrent->id();
                                const auto found(std::find_if(newTorrents.begin(), newTorrents.end(), [id](const auto& t) {
                                    return std::get<1>(t) == id;
                                }));
                                if (found == newTorrentsEnd) {
                                    remover.remove(i);
                                } else {
                                    std::get<2>(*found) = true;

                                    const bool wasFinished = torrent->isFinished();
                                    const bool metadataWasComplete = torrent->isMetadataComplete();
                                    if (torrent->update(std::get<0>(*found))) {
                                        changed.push_back(i);
                                        if (!wasFinished && torrent->isFinished()) {
                                            emit torrentFinished(torrent.get());
                                        }
                                        if (!metadataWasComplete && torrent->isMetadataComplete()) {
                                            checkSingleFile.push_back(id);
                                        }
                                    }
                                    if (torrent->isFilesEnabled()) {
                                        getTorrentFiles(id, true);
                                    }
                                    if (torrent->isPeersEnabled()) {
                                        getTorrentPeers(id, true);
                                    }
                                }
                            }
                            remover.doRemove();
                        }
                        std::reverse(changed.begin(), changed.end());

                        int added = 0;
                        if (newTorrents.size() > mTorrents.size()) {
                            mTorrents.reserve(newTorrents.size());
                            for (const auto& t : newTorrents) {
                                const QJsonObject& torrentJson = std::get<0>(t);
                                const int id = std::get<1>(t);
                                const bool existing = std::get<2>(t);
                                if (!existing) {
                                    mTorrents.emplace_back(std::make_shared<Torrent>(id, torrentJson, this));
                                    ++added;
                                    Torrent* torrent = mTorrents.back().get();
#ifdef TREMOTESF_SAILFISHOS
                                    // prevent automatic destroying on QML side
                                    QQmlEngine::setObjectOwnership(torrent, QQmlEngine::CppOwnership);
#endif
                                    if (isConnected()) {
                                        emit torrentAdded(torrent);
                                    }

                                    if (torrent->isMetadataComplete()) {
                                        checkSingleFile.push_back(id);
                                    }
                                }
                            }
                        }

                        emit torrentsUpdated(removed, changed, added);

                        checkIfTorrentsUpdated();
                        startUpdateTimer();

                        checkTorrentsSingleFile(checkSingleFile);
        });
    }

    void Rpc::checkTorrentsSingleFile(const QVariantList& torrentIds)
    {
        postRequest(QLatin1String("torrent-get"),
                    {{QLatin1String("fields"), QVariantList{QLatin1String("id"), QLatin1String("priorities")}},
                     {QLatin1String("ids"), torrentIds}},
                    [=](const QJsonObject& parseResult, bool success) {
                        if (success) {
                            const QJsonArray torrents(getReplyArguments(parseResult).value(torrentsKey).toArray());
                            for (const QJsonValue& torrentValue : torrents) {
                                const QJsonObject torrentMap(torrentValue.toObject());
                                const int torrentId = torrentMap.value(Torrent::idKey).toInt();
                                Torrent* torrent = torrentById(torrentId);
                                if (torrent) {
                                    torrent->checkSingleFile(torrentMap);
                                }
                            }
                        }
                    });
    }

    void Rpc::getServerStats()
    {
        postRequest(QLatin1String("session-stats"), QByteArrayLiteral("{\"method\":\"session-stats\"}"),
                    [=](const QJsonObject& parseResult, bool success) {
                        if (success) {
                            mServerStats->update(getReplyArguments(parseResult));
                            mServerStatsUpdated = true;
                            startUpdateTimer();
                        }
                    });
    }

    void Rpc::checkIfTorrentsUpdated()
    {
        if (mUpdating && !mTorrentsUpdated) {
            for (const std::shared_ptr<Torrent>& torrent : mTorrents) {
                if (!torrent->isUpdated()) {
                    return;
                }
            }
            mTorrentsUpdated = true;
        }
    }

    void Rpc::startUpdateTimer()
    {
        if (mUpdating && mServerSettingsUpdated && mTorrentsUpdated && mServerStatsUpdated) {
            if (mStatus == Connecting) {
                setStatus(Connected);
            }
            if (!mUpdateDisabled) {
                mUpdateTimer->start();
            }
            mUpdating = false;
        }
    }

    void Rpc::onAuthenticationRequired(QNetworkReply*, QAuthenticator* authenticator)
    {
        if (mAuthentication && !mAuthenticationRequested) {
            authenticator->setUser(mUsername);
            authenticator->setPassword(mPassword);
            mAuthenticationRequested = true;
        }
    }

    void Rpc::postRequest(const QLatin1String& method, const QByteArray& data, const std::function<void(const QJsonObject&, bool)>& callOnSuccessParse)
    {
        QNetworkRequest request(mServerUrl);
        static const QVariant contentType(QLatin1String("application/json"));
        request.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
        request.setRawHeader(sessionIdHeader, mSessionId);
        request.setSslConfiguration(mSslConfiguration);

        QNetworkReply* reply = mNetwork->post(request, data);
        mNetworkRequests.insert(reply);

        reply->ignoreSslErrors(mExpectedSslErrors);

        QObject::connect(reply, &QNetworkReply::finished, this, [=] {
            if (mStatus != Disconnected) {
                mNetworkRequests.erase(reply);

                switch (reply->error()) {
                case QNetworkReply::NoError:
                {
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
                        if (mStatus != Disconnected) {
                            const QJsonObject& parseResult = result.first;
                            const bool parsedOk = result.second;
                            if (parsedOk) {
                                const bool success = isResultSuccessful(parseResult);
                                if (!success) {
                                    qWarning() << "method" << method << "failed, response:" << parseResult;
                                }
                                if (callOnSuccessParse) {
                                    callOnSuccessParse(parseResult, success);
                                }
                            } else {
                                qWarning("Parsing error");
                                setError(ParseError);
                                setStatus(Disconnected);
                            }
                        }
                        watcher->deleteLater();
                    });
                    watcher->setFuture(future);
                    break;
                }
                case QNetworkReply::AuthenticationRequiredError:
                    qWarning("Authentication error");
                    setError(AuthenticationError);
                    setStatus(Disconnected);
                    break;
                case QNetworkReply::OperationCanceledError:
                case QNetworkReply::TimeoutError:
                    qWarning("Timed out");
                    setError(TimedOut);
                    setStatus(Disconnected);
                    break;
                default:
                    if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 409 &&
                        reply->hasRawHeader(sessionIdHeader)) {
                        mSessionId = reply->rawHeader(sessionIdHeader);
                        postRequest(method, data, callOnSuccessParse);
                    } else {
                        qWarning() << reply->error() << reply->errorString();
                        setError(ConnectionError, reply->errorString());
                        setStatus(Disconnected);
                    }
                }
            }
            reply->deleteLater();
        });

        QObject::connect(this, &Rpc::connectedChanged, reply, [=] {
            if (!isConnected()) {
                reply->abort();
            }
        });

        auto timer = new QTimer(this);
        timer->setInterval(mTimeout);
        timer->setSingleShot(true);
        QObject::connect(timer, &QTimer::timeout, reply, &QNetworkReply::abort);
        QObject::connect(timer, &QTimer::timeout, timer, &QObject::deleteLater);
        timer->start();
    }

    void Rpc::postRequest(const QLatin1String& method, const QVariantMap& arguments, const std::function<void (const QJsonObject&, bool)>& callOnSuccessParse)
    {
        postRequest(method, makeRequestData(method, arguments), callOnSuccessParse);
    }
}
