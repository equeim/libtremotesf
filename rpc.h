// SPDX-FileCopyrightText: 2015-2022 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LIBTREMOTESF_RPC_H
#define LIBTREMOTESF_RPC_H

#include <functional>
#include <memory>
#include <optional>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include <QByteArray>
#include <QNetworkRequest>
#include <QObject>
#include <QSslConfiguration>
#include <QUrl>
#include <QVariantList>

#include "formatters.h"
#include "serversettings.h"
#include "serverstats.h"

class QAuthenticator;
class QFile;
class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

namespace libtremotesf {
    Q_NAMESPACE

    namespace impl {
        class RequestRouter;
    }

    class Torrent;

    struct Server {
        Q_GADGET
    public:
        enum class ProxyType { Default, Http, Socks5 };
        Q_ENUM(ProxyType)

        QString name;

        QString address;
        int port;
        QString apiPath;

        ProxyType proxyType;
        QString proxyHostname;
        int proxyPort;
        QString proxyUser;
        QString proxyPassword;

        bool https;
        bool selfSignedCertificateEnabled;
        QByteArray selfSignedCertificate;
        bool clientCertificateEnabled;
        QByteArray clientCertificate;

        bool authentication;
        QString username;
        QString password;

        int updateInterval;
        int timeout;

        bool autoReconnectEnabled;
        int autoReconnectInterval;
    };

    enum class RpcConnectionState { Disconnected, Connecting, Connected };
    Q_ENUM_NS(RpcConnectionState)

    enum class RpcError {
        NoError,
        TimedOut,
        ConnectionError,
        AuthenticationError,
        ParseError,
        ServerIsTooNew,
        ServerIsTooOld
    };
    Q_ENUM_NS(RpcError)

    class Rpc : public QObject {
        Q_OBJECT
    public:
        using ConnectionState = RpcConnectionState;
        using Error = RpcError;

        explicit Rpc(QObject* parent = nullptr);
        ~Rpc() override;

        ServerSettings* serverSettings() const;
        ServerStats* serverStats() const;

        const std::vector<std::unique_ptr<Torrent>>& torrents() const;
        libtremotesf::Torrent* torrentByHash(const QString& hash) const;
        Torrent* torrentById(int id) const;

        struct Status {
            ConnectionState connectionState{ConnectionState::Disconnected};
            Error error{Error::NoError};
            QString errorMessage{};
            QString detailedErrorMessage{};

            inline bool operator==(const Status& other) const noexcept {
                return connectionState == other.connectionState && error == other.error &&
                       errorMessage == other.errorMessage && detailedErrorMessage == other.detailedErrorMessage;
            }
        };

        bool isConnected() const;
        const Status& status() const;
        ConnectionState connectionState() const;
        Error error() const;
        const QString& errorMessage() const;
        const QString& detailedErrorMessage() const;
        bool isLocal() const;

        int torrentsCount() const;

        bool isUpdateDisabled() const;
        void setUpdateDisabled(bool disabled);

        void setServer(const libtremotesf::Server& server);
        void resetServer();

        void connect();
        void disconnect();

        void addTorrentFile(
            const QString& filePath,
            const QString& downloadDirectory,
            const QVariantList& unwantedFiles,
            const QVariantList& highPriorityFiles,
            const QVariantList& lowPriorityFiles,
            const QVariantMap& renamedFiles,
            int bandwidthPriority,
            bool start
        );

        void addTorrentFile(
            std::shared_ptr<QFile> file,
            const QString& downloadDirectory,
            const QVariantList& unwantedFiles,
            const QVariantList& highPriorityFiles,
            const QVariantList& lowPriorityFiles,
            const QVariantMap& renamedFiles,
            int bandwidthPriority,
            bool start
        );

        void addTorrentLink(const QString& link, const QString& downloadDirectory, int bandwidthPriority, bool start);

        void startTorrents(const QVariantList& ids);
        void startTorrentsNow(const QVariantList& ids);
        void pauseTorrents(const QVariantList& ids);
        void removeTorrents(const QVariantList& ids, bool deleteFiles);
        void checkTorrents(const QVariantList& ids);
        void moveTorrentsToTop(const QVariantList& ids);
        void moveTorrentsUp(const QVariantList& ids);
        void moveTorrentsDown(const QVariantList& ids);
        void moveTorrentsToBottom(const QVariantList& ids);

        void reannounceTorrents(const QVariantList& ids);

        void setSessionProperty(const QString& property, const QVariant& value);
        void setSessionProperties(const QVariantMap& properties);
        void
        setTorrentProperty(int id, const QString& property, const QVariant& value, bool updateIfSuccessful = false);
        void setTorrentsLocation(const QVariantList& ids, const QString& location, bool moveFiles);
        void getTorrentsFiles(const QVariantList& ids, bool scheduled);
        void getTorrentsPeers(const QVariantList& ids, bool scheduled);

        void renameTorrentFile(int torrentId, const QString& filePath, const QString& newName);

        void getDownloadDirFreeSpace();
        void getFreeSpaceForPath(const QString& path);

        void updateData(bool updateServerSettings = true);

        void shutdownServer();

    private:
        void setStatus(Status&& status);
        void resetStateOnConnectionStateChanged(ConnectionState oldConnectionState, size_t& removedTorrentsCount);
        void emitSignalsOnConnectionStateChanged(ConnectionState oldConnectionState, size_t removedTorrentsCount);

        void getServerSettings();
        void getTorrents();
        void checkTorrentsSingleFile(const QVariantList& torrentIds);
        void getServerStats();

        void maybeFinishUpdatingTorrents();
        bool checkIfUpdateCompleted();
        bool checkIfConnectionCompleted();
        void maybeFinishUpdateOrConnection();

        void checkIfServerIsLocal();
        [[nodiscard]] bool isSessionIdFileExists() const;

        impl::RequestRouter* mRequestRouter{};

        bool mUpdateDisabled{};
        bool mUpdating{};

        bool mAutoReconnectEnabled{};

        std::optional<bool> mServerIsLocal{};
        std::optional<int> mPendingHostInfoLookupId{};

        bool mServerSettingsUpdated{};
        bool mTorrentsUpdated{};
        bool mServerStatsUpdated{};
        QTimer* mUpdateTimer{};
        QTimer* mAutoReconnectTimer{};

        ServerSettings* mServerSettings{};
        // Don't use member initializer to workaround Android NDK bug (https://github.com/android/ndk/issues/1798)
        std::vector<std::unique_ptr<Torrent>> mTorrents;
        ServerStats* mServerStats{};

        Status mStatus{};

    signals:
        void aboutToDisconnect();
        void statusChanged();
        void connectedChanged();
        void connectionStateChanged();
        void errorChanged();

        void onAboutToRemoveTorrents(size_t first, size_t last);
        void onRemovedTorrents(size_t first, size_t last);
        void onChangedTorrents(size_t first, size_t last);
        void onAboutToAddTorrents(size_t count);
        void onAddedTorrents(size_t count);

        void torrentsUpdated(
            const std::vector<std::pair<int, int>>& removedIndexRanges,
            const std::vector<std::pair<int, int>>& changedIndexRanges,
            int addedCount
        );

        void torrentFilesUpdated(const libtremotesf::Torrent* torrent, const std::vector<int>& changedIndexes);
        void torrentPeersUpdated(
            const libtremotesf::Torrent* torrent,
            const std::vector<std::pair<int, int>>& removedIndexRanges,
            const std::vector<std::pair<int, int>>& changedIndexRanges,
            int addedCount
        );

        void torrentFileRenamed(int torrentId, const QString& filePath, const QString& newName);

        void torrentAdded(libtremotesf::Torrent* torrent);
        void torrentFinished(libtremotesf::Torrent* torrent);

        void torrentAddDuplicate();
        void torrentAddError();

        void gotDownloadDirFreeSpace(long long bytes);
        void gotFreeSpaceForPath(const QString& path, bool success, long long bytes);

        void updateDisabledChanged();
    };
}

#ifndef SWIG
SPECIALIZE_FORMATTER_FOR_Q_ENUM(libtremotesf::RpcConnectionState)
SPECIALIZE_FORMATTER_FOR_Q_ENUM(libtremotesf::RpcError)
#endif

#endif // LIBTREMOTESF_RPC_H
