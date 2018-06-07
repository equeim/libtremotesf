#ifndef LIBTREMOTESF_JNIRPC_H
#define LIBTREMOTESF_JNIRPC_H

#include "rpc.h"
#include "serversettings.h"
#include "torrent.h"

class QCoreApplication;
class QThread;

namespace libtremotesf
{
    class JniServerSettings : public ServerSettings
    {
    public:
        explicit JniServerSettings(QObject* parent = nullptr);

        void setDownloadDirectory(const QString& directory);
        void setStartAddedTorrents(bool start);
        void setTrashTorrentFiles(bool trash);
        void setRenameIncompleteFiles(bool rename);
        void setIncompleteDirectoryEnabled(bool enabled);
        void setIncompleteDirectory(const QString& directory);
        void setRatioLimited(bool limited);
        void setRatioLimit(float limit);
        void setIdleSeedingLimited(bool limited);
        void setIdleSeedingLimit(int limit);
        void setDownloadQueueEnabled(bool enabled);
        void setDownloadQueueSize(int size);
        void setSeedQueueEnabled(bool enabled);
        void setSeedQueueSize(int size);
        void setIdleQueueLimited(bool limited);
        void setIdleQueueLimit(int limit);
        void setDownloadSpeedLimited(bool limited);
        void setDownloadSpeedLimit(int limit);
        void setUploadSpeedLimited(bool limited);
        void setUploadSpeedLimit(int limit);
        void setAlternativeSpeedLimitsEnabled(bool enabled);
        void setAlternativeDownloadSpeedLimit(int limit);
        void setAlternativeUploadSpeedLimit(int limit);
        void setAlternativeSpeedLimitsScheduled(bool scheduled);
        void setAlternativeSpeedLimitsBeginTime(const QTime& time);
        void setAlternativeSpeedLimitsEndTime(const QTime& time);
        void setAlternativeSpeedLimitsDays(AlternativeSpeedLimitsDays days);
        void setPeerPort(int port);
        void setRandomPortEnabled(bool enabled);
        void setPortForwardingEnabled(bool enabled);
        void setEncryptionMode(EncryptionMode mode);
        void setUtpEnabled(bool enabled);
        void setPexEnabled(bool enabled);
        void setDhtEnabled(bool enabled);
        void setLpdEnabled(bool enabled);
        void setMaximumPeersPerTorrent(int peers);
        void setMaximumPeersGlobally(int peers);
    };

    class JniRpc : public Rpc
    {
    public:
        JniRpc();

        JniServerSettings* serverSettings() const;

        void setServer(const QString& name,
                       const QString& address,
                       int port,
                       const QString& apiPath,
                       bool https,
                       bool selfSignedCertificateEnabled,
                       const QByteArray& selfSignedCertificate,
                       bool clientCertificateEnabled,
                       const QByteArray& clientCertificate,
                       bool authentication,
                       const QString& username,
                       const QString& password,
                       int updateInterval,
                       int backgroundUpdateInterval,
                       int timeout);
        void resetServer();

        void connect();
        void disconnect();

        bool backgroundUpdate() const;
        void setBackgroundUpdate(bool background);

        bool isUpdateDisabled() const;
        void setUpdateDisabled(bool disabled);

        void addTorrentFile(const QByteArray& fileData,
                            const QString& downloadDirectory,
                            const QVariantList& wantedFiles,
                            const QVariantList& unwantedFiles,
                            const QVariantList& highPriorityFiles,
                            const QVariantList& normalPriorityFiles,
                            const QVariantList& lowPriorityFiles,
                            int bandwidthPriority,
                            bool start);

        void addTorrentLink(const QString& link,
                            const QString& downloadDirectory,
                            int bandwidthPriority,
                            bool start);

        void startTorrents(const QVariantList& ids);
        void startTorrentsNow(const QVariantList& ids);
        void pauseTorrents(const QVariantList& ids);
        void removeTorrents(const QVariantList& ids, bool deleteFiles);
        void checkTorrents(const QVariantList& ids);
        void moveTorrentsToTop(const QVariantList& ids);
        void moveTorrentsUp(const QVariantList& ids);
        void moveTorrentsDown(const QVariantList& ids);
        void moveTorrentsToBottom(const QVariantList& ids);

        void setTorrentLocation(int id, const QString& location, bool moveFiles);

        void getTorrentFiles(int id, bool scheduled);
        void getTorrentPeers(int id, bool scheduled);

        void renameTorrentFile(int torrentId,
                               const QString& filePath,
                               const QString& newName);

        void getDownloadDirFreeSpace();
        void getFreeSpaceForPath(const QString& path);

        void setTorrentDownloadSpeedLimited(Torrent* torrent, bool limited);
        void setTorrentDownloadSpeedLimit(Torrent* torrent, int limit);
        void setTorrentUploadSpeedLimited(Torrent* torrent, bool limited);
        void setTorrentUploadSpeedLimit(Torrent* torrent, int limit);
        void setTorrentRatioLimitMode(Torrent* torrent, Torrent::RatioLimitMode mode);
        void setTorrentRatioLimit(Torrent* torrent, float limit);
        void setTorrentPeersLimit(Torrent* torrent, int limit);
        void setTorrentHonorSessionLimits(Torrent* torrent, bool honor);
        void setTorrentBandwidthPriority(Torrent* torrent, Torrent::Priority priority);
        void setTorrentIdleSeedingLimitMode(Torrent* torrent, Torrent::IdleSeedingLimitMode mode);
        void setTorrentIdleSeedingLimit(Torrent* torrent, int limit);
        void setTorrentFilesEnabled(Torrent* torrent, bool enabled);
        void setTorrentFilesWanted(Torrent* torrent, const QVariantList& files, bool wanted);
        void setTorrentFilesPriority(Torrent* torrent, const QVariantList& files, TorrentFile::Priority priority);
        void torrentRenameFile(Torrent* torrent, const QString& path, const QString& newName);
        void torrentAddTracker(Torrent* torrent, const QString& announce);
        void torrentSetTracker(Torrent* torrent, int trackerId, const QString& announce);
        void torrentRemoveTrackers(Torrent* torrent, const QVariantList& ids);
        void setTorrentPeersEnabled(Torrent* torrent, bool enabled);

    protected:
        virtual void onAboutToDisconnect();
        virtual void onConnectedChanged();
        virtual void onStatusChanged();
        virtual void onErrorChanged();

        virtual void onTorrentsUpdated();
        virtual void onServerStatsUpdated();

        virtual void onTorrentFinished(int id, const QString& hashString, const QString& name);

        virtual void onTorrentAddDuplicate();
        virtual void onTorrentAddError();

        virtual void onGotTorrentFiles(int torrentId);
        virtual void onTorrentFileRenamed(int torrentId, const QString& filePath, const QString& newName);

        virtual void onGotTorrentPeers(int torrentId);

        virtual void onGotDownloadDirFreeSpace(long long bytes);
        virtual void onGotFreeSpaceForPath(const QString& path, bool success, long long bytes);
    };

    class JniWrapper
    {
    public:
        JniWrapper(JniRpc* rpc);
        virtual ~JniWrapper();

    private:
        QCoreApplication* mApp;
        QThread* mThread;
    };
}

#endif // LIBTREMOTESF_JNIRPC_H
