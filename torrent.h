// SPDX-FileCopyrightText: 2015-2022 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LIBTREMOTESF_TORRENT_H
#define LIBTREMOTESF_TORRENT_H

#include <vector>

#include <QDateTime>
#include <QObject>

#include "formatters.h"
#include "literals.h"
#include "peer.h"
#include "torrentfile.h"
#include "tracker.h"

class QJsonObject;

namespace libtremotesf {
    class Rpc;

    struct TorrentData {
        Q_GADGET
    public:
        enum Status {
            Paused,
            Downloading,
            Seeding,
            StalledDownloading,
            StalledSeeding,
            QueuedForDownloading,
            QueuedForSeeding,
            Checking,
            QueuedForChecking,
            Errored
        };
        Q_ENUM(Status)

        enum Priority { LowPriority = -1, NormalPriority, HighPriority };
        Q_ENUM(Priority)

        enum RatioLimitMode { GlobalRatioLimit, SingleRatioLimit, UnlimitedRatio };
        Q_ENUM(RatioLimitMode)

        enum IdleSeedingLimitMode { GlobalIdleSeedingLimit, SingleIdleSeedingLimit, UnlimitedIdleSeeding };
        Q_ENUM(IdleSeedingLimitMode)

        bool update(const QJsonObject& torrentMap);

        int id = 0;
        QString hashString;
        QString name;
        QString magnetLink;

        QString errorString;
        Status status = Paused;
        int queuePosition = 0;

        long long totalSize = 0;
        long long completedSize = 0;
        long long leftUntilDone = 0;
        long long sizeWhenDone = 0;

        double percentDone = 0.0;
        double recheckProgress = 0.0;
        int eta = 0;

        bool metadataComplete = false;

        long long downloadSpeed = 0;
        long long uploadSpeed = 0;

        bool downloadSpeedLimited = false;
        int downloadSpeedLimit = 0; // kB/s
        bool uploadSpeedLimited = false;
        int uploadSpeedLimit = 0; // kB/s

        long long totalDownloaded = 0;
        long long totalUploaded = 0;
        double ratio = 0.0;
        double ratioLimit = 0.0;
        RatioLimitMode ratioLimitMode = GlobalRatioLimit;

        int seeders = 0;
        int leechers = 0;
        int peersLimit = 0;

        QDateTime addedDate;
        long long addedDateTime = -1;
        QDateTime activityDate;
        long long activityDateTime = -1;
        QDateTime doneDate;
        long long doneDateTime = -1;

        IdleSeedingLimitMode idleSeedingLimitMode = GlobalIdleSeedingLimit;
        int idleSeedingLimit = 0;
        QString downloadDirectory;
        QString comment;
        QString creator;
        QDateTime creationDate;
        long long creationDateTime = -1;
        Priority bandwidthPriority = NormalPriority;
        bool honorSessionLimits = false;

        bool singleFile = true;

        bool trackersAnnounceUrlsChanged = false;

        std::vector<Tracker> trackers;

        std::vector<QString> webSeeders;
        int activeWebSeeders = 0;
    };

    class Torrent : public QObject {
        Q_OBJECT
    public:
        static constexpr auto idKey = "id"_l1;

        explicit Torrent(int id, const QJsonObject& torrentMap, Rpc* rpc, QObject* parent = nullptr);
        // For testing only
        explicit Torrent() = default;

        int id() const;
        const QString& hashString() const;
        const QString& name() const;

        TorrentData::Status status() const;
        QString errorString() const;
        int queuePosition() const;

        long long totalSize() const;
        long long completedSize() const;
        long long leftUntilDone() const;
        long long sizeWhenDone() const;
        double percentDone() const;
        bool isFinished() const;
        double recheckProgress() const;
        int eta() const;

        bool isMetadataComplete() const;

        long long downloadSpeed() const;
        long long uploadSpeed() const;

        bool isDownloadSpeedLimited() const;
        void setDownloadSpeedLimited(bool limited);
        int downloadSpeedLimit() const;
        void setDownloadSpeedLimit(int limit);

        bool isUploadSpeedLimited() const;
        void setUploadSpeedLimited(bool limited);
        int uploadSpeedLimit() const;
        void setUploadSpeedLimit(int limit);

        long long totalDownloaded() const;
        long long totalUploaded() const;
        double ratio() const;
        TorrentData::RatioLimitMode ratioLimitMode() const;
        void setRatioLimitMode(TorrentData::RatioLimitMode mode);
        double ratioLimit() const;
        void setRatioLimit(double limit);

        int seeders() const;
        int leechers() const;
        int peersLimit() const;
        void setPeersLimit(int limit);

        const QDateTime& addedDate() const;
        const QDateTime& activityDate() const;
        const QDateTime& doneDate() const;

        bool honorSessionLimits() const;
        void setHonorSessionLimits(bool honor);
        TorrentData::Priority bandwidthPriority() const;
        void setBandwidthPriority(TorrentData::Priority priority);
        TorrentData::IdleSeedingLimitMode idleSeedingLimitMode() const;
        void setIdleSeedingLimitMode(TorrentData::IdleSeedingLimitMode mode);
        int idleSeedingLimit() const;
        void setIdleSeedingLimit(int limit);
        const QString& downloadDirectory() const;
        bool isSingleFile() const;
        const QString& creator() const;
        const QDateTime& creationDate() const;
        const QString& comment() const;

        const std::vector<Tracker>& trackers() const;
        bool isTrackersAnnounceUrlsChanged() const;
        void addTrackers(const QStringList& announceUrls);
        void setTracker(int trackerId, const QString& announce);
        void removeTrackers(const QVariantList& ids);

        const std::vector<QString>& webSeeders() const;
        int activeWebSeeders() const;

        const TorrentData& data() const;

        bool isFilesEnabled() const;
        void setFilesEnabled(bool enabled);
        const std::vector<TorrentFile>& files() const;

        void setFilesWanted(const QVariantList& files, bool wanted);
        void setFilesPriority(const QVariantList& files, TorrentFile::Priority priority);
        void renameFile(const QString& path, const QString& newName);

        bool isPeersEnabled() const;
        void setPeersEnabled(bool enabled);
        const std::vector<Peer>& peers() const;

        bool isUpdated() const;
        void checkThatFilesUpdated();
        void checkThatPeersUpdated();

        bool update(const QJsonObject& torrentMap);
        void updateFiles(const QJsonObject& torrentMap);
        void updatePeers(const QJsonObject& torrentMap);

        void checkSingleFile(const QJsonObject& torrentMap);

    private:
        Rpc* mRpc;

        TorrentData mData;

        std::vector<TorrentFile> mFiles;
        bool mFilesEnabled = false;
        bool mFilesUpdated = false;

        std::vector<Peer> mPeers;
        bool mPeersEnabled = false;
        bool mPeersUpdated = false;

    signals:
        void updated();
        void changed();

        void filesUpdated(const std::vector<int>& changedIndexes);
        void peersUpdated(
            const std::vector<std::pair<int, int>>& removedIndexRanges,
            const std::vector<std::pair<int, int>>& changedIndexRanges,
            int addedCount
        );
        void fileRenamed(const QString& filePath, const QString& newName);
    };
}

// SWIG can't parse it :(
#ifndef SWIG
template<>
struct fmt::formatter<libtremotesf::Torrent> : libtremotesf::SimpleFormatter {
    format_context::iterator format(const libtremotesf::Torrent& torrent, format_context& ctx) FORMAT_CONST;
};
#endif // SWIG

#endif // LIBTREMOTESF_TORRENT_H
