// SPDX-FileCopyrightText: 2015-2022 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "torrent.h"

#include <type_traits>

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QLocale>

#include "itemlistupdater.h"
#include "log.h"
#include "pathutils.h"
#include "rpc.h"
#include "stdutils.h"

namespace libtremotesf {
    namespace {
        constexpr auto hashStringKey = "hashString"_l1;
        constexpr auto addedDateKey = "addedDate"_l1;

        constexpr auto nameKey = "name"_l1;

        constexpr auto magnetLinkKey = "magnetLink"_l1;

        constexpr auto errorStringKey = "errorString"_l1;
        constexpr auto queuePositionKey = "queuePosition"_l1;

        constexpr auto totalSizeKey = "totalSize"_l1;
        constexpr auto completedSizeKey = "haveValid"_l1;
        constexpr auto leftUntilDoneKey = "leftUntilDone"_l1;
        constexpr auto sizeWhenDoneKey = "sizeWhenDone"_l1;
        constexpr auto percentDoneKey = "percentDone"_l1;
        constexpr auto recheckProgressKey = "recheckProgress"_l1;
        constexpr auto etaKey = "eta"_l1;

        constexpr auto metadataCompleteKey = "metadataPercentComplete"_l1;

        constexpr auto downloadSpeedKey = "rateDownload"_l1;
        constexpr auto uploadSpeedKey = "rateUpload"_l1;

        constexpr auto downloadSpeedLimitedKey = "downloadLimited"_l1;
        constexpr auto downloadSpeedLimitKey = "downloadLimit"_l1;
        constexpr auto uploadSpeedLimitedKey = "uploadLimited"_l1;
        constexpr auto uploadSpeedLimitKey = "uploadLimit"_l1;

        constexpr auto totalDownloadedKey = "downloadedEver"_l1;
        constexpr auto totalUploadedKey = "uploadedEver"_l1;
        constexpr auto ratioKey = "uploadRatio"_l1;
        constexpr auto ratioLimitModeKey = "seedRatioMode"_l1;
        constexpr auto ratioLimitKey = "seedRatioLimit"_l1;

        constexpr auto seedersKey = "peersSendingToUs"_l1;
        constexpr auto leechersKey = "peersGettingFromUs"_l1;

        constexpr auto errorKey = "error"_l1;
        constexpr auto statusKey = "status"_l1;

        constexpr auto activityDateKey = "activityDate"_l1;
        constexpr auto doneDateKey = "doneDate"_l1;

        constexpr auto peersLimitKey = "peer-limit"_l1;
        constexpr auto honorSessionLimitsKey = "honorsSessionLimits"_l1;
        constexpr auto bandwidthPriorityKey = "bandwidthPriority"_l1;
        constexpr auto idleSeedingLimitModeKey = "seedIdleMode"_l1;
        constexpr auto idleSeedingLimitKey = "seedIdleLimit"_l1;
        constexpr auto downloadDirectoryKey = "downloadDir"_l1;
        constexpr auto prioritiesKey = "priorities"_l1;
        constexpr auto creatorKey = "creator"_l1;
        constexpr auto creationDateKey = "dateCreated"_l1;
        constexpr auto commentKey = "comment"_l1;

        constexpr auto webSeedersKey = "webseeds"_l1;
        constexpr auto activeWebSeedersKey = "webseedsSendingToUs"_l1;

        constexpr auto wantedFilesKey = "files-wanted"_l1;
        constexpr auto unwantedFilesKey = "files-unwanted"_l1;

        constexpr auto lowPriorityKey = "priority-low"_l1;
        constexpr auto normalPriorityKey = "priority-normal"_l1;
        constexpr auto highPriorityKey = "priority-high"_l1;

        constexpr auto addTrackerKey = "trackerAdd"_l1;
        constexpr auto replaceTrackerKey = "trackerReplace"_l1;
        constexpr auto removeTrackerKey = "trackerRemove"_l1;
    }

    int TorrentData::priorityToInt(Priority value) { return static_cast<int>(value); }

    bool TorrentData::update(const QJsonObject& torrentMap) {
        bool changed = false;

        setChanged(name, torrentMap.value(nameKey).toString(), changed);
        setChanged(magnetLink, torrentMap.value(magnetLinkKey).toString(), changed);

        setChanged(
            error,
            [&] {
                switch (auto error = static_cast<Error>(torrentMap.value(errorKey).toInt())) {
                case Error::None:
                case Error::TrackerWarning:
                case Error::TrackerError:
                case Error::LocalError:
                    return error;
                default:
                    return Error::None;
                }
            }(),
            changed
        );
        setChanged(errorString, torrentMap.value(errorStringKey).toString(), changed);
        setChanged(queuePosition, torrentMap.value(queuePositionKey).toInt(), changed);
        setChanged(totalSize, static_cast<long long>(torrentMap.value(totalSizeKey).toDouble()), changed);
        setChanged(completedSize, static_cast<long long>(torrentMap.value(completedSizeKey).toDouble()), changed);
        setChanged(leftUntilDone, static_cast<long long>(torrentMap.value(leftUntilDoneKey).toDouble()), changed);
        setChanged(sizeWhenDone, static_cast<long long>(torrentMap.value(sizeWhenDoneKey).toDouble()), changed);
        setChanged(percentDone, torrentMap.value(percentDoneKey).toDouble(), changed);
        setChanged(recheckProgress, torrentMap.value(recheckProgressKey).toDouble(), changed);
        setChanged(eta, torrentMap.value(etaKey).toInt(), changed);

        setChanged(metadataComplete, torrentMap.value(metadataCompleteKey).toInt() == 1, changed);

        setChanged(downloadSpeed, static_cast<long long>(torrentMap.value(downloadSpeedKey).toDouble()), changed);
        setChanged(uploadSpeed, static_cast<long long>(torrentMap.value(uploadSpeedKey).toDouble()), changed);

        setChanged(downloadSpeedLimited, torrentMap.value(downloadSpeedLimitedKey).toBool(), changed);
        setChanged(downloadSpeedLimit, torrentMap.value(downloadSpeedLimitKey).toInt(), changed);
        setChanged(uploadSpeedLimited, torrentMap.value(uploadSpeedLimitedKey).toBool(), changed);
        setChanged(uploadSpeedLimit, torrentMap.value(uploadSpeedLimitKey).toInt(), changed);

        setChanged(totalDownloaded, static_cast<long long>(torrentMap.value(totalDownloadedKey).toDouble()), changed);
        setChanged(totalUploaded, static_cast<long long>(torrentMap.value(totalUploadedKey).toDouble()), changed);
        setChanged(ratio, torrentMap.value(ratioKey).toDouble(), changed);

        setChanged(
            ratioLimitMode,
            [&] {
                switch (auto mode = static_cast<RatioLimitMode>(torrentMap.value(ratioLimitModeKey).toInt())) {
                case RatioLimitMode::Global:
                case RatioLimitMode::Single:
                case RatioLimitMode::Unlimited:
                    return mode;
                default:
                    return RatioLimitMode::Global;
                }
            }(),
            changed
        );
        setChanged(ratioLimit, torrentMap.value(ratioLimitKey).toDouble(), changed);

        setChanged(seeders, torrentMap.value(seedersKey).toInt(), changed);
        setChanged(leechers, torrentMap.value(leechersKey).toInt(), changed);

        const bool hasActivePeers = (seeders != 0 || activeWebSeeders != 0 || leechers != 0);

        switch (torrentMap.value(statusKey).toInt()) {
        case 0:
            setChanged(status, Status::Paused, changed);
            break;
        case 1:
            setChanged(status, Status::QueuedForChecking, changed);
            break;
        case 2:
            setChanged(status, Status::Checking, changed);
            break;
        case 3:
            setChanged(status, Status::QueuedForDownloading, changed);
            break;
        case 4:
            if (hasActivePeers) {
                setChanged(status, Status::Downloading, changed);
            } else {
                setChanged(status, Status::StalledDownloading, changed);
            }
            break;
        case 5:
            setChanged(status, Status::QueuedForSeeding, changed);
            break;
        case 6:
            if (hasActivePeers) {
                setChanged(status, Status::Seeding, changed);
            } else {
                setChanged(status, Status::StalledSeeding, changed);
            }
        }

        setChanged(peersLimit, torrentMap.value(peersLimitKey).toInt(), changed);

        const auto newActivityDateTime = static_cast<long long>(torrentMap.value(activityDateKey).toDouble()) * 1000;
        if (newActivityDateTime > 0) {
            if (newActivityDateTime != activityDateTime) {
                activityDateTime = newActivityDateTime;
                activityDate.setMSecsSinceEpoch(newActivityDateTime);
                changed = true;
            }
        } else {
            if (!activityDate.isNull()) {
                activityDateTime = -1;
                activityDate = QDateTime();
                changed = true;
            }
        }
        const auto newDoneDateTime = static_cast<long long>(torrentMap.value(doneDateKey).toDouble()) * 1000;
        if (newDoneDateTime > 0) {
            if (newDoneDateTime != doneDateTime) {
                doneDateTime = newDoneDateTime;
                doneDate.setMSecsSinceEpoch(newDoneDateTime);
                changed = true;
            }
        } else {
            if (!doneDate.isNull()) {
                doneDateTime = -1;
                doneDate = QDateTime();
                changed = true;
            }
        }

        setChanged(honorSessionLimits, torrentMap.value(honorSessionLimitsKey).toBool(), changed);
        setChanged(
            bandwidthPriority,
            [&] {
                switch (auto priority = static_cast<Priority>(torrentMap.value(bandwidthPriorityKey).toInt())) {
                case Priority::Low:
                case Priority::Normal:
                case Priority::High:
                    return static_cast<Priority>(priority);
                default:
                    return Priority::Normal;
                }
            }(),
            changed
        );
        setChanged(
            idleSeedingLimitMode,
            [&] {
                switch (auto mode =
                            static_cast<IdleSeedingLimitMode>(torrentMap.value(idleSeedingLimitModeKey).toInt())) {
                case IdleSeedingLimitMode::Global:
                case IdleSeedingLimitMode::Single:
                case IdleSeedingLimitMode::Unlimited:
                    return mode;
                default:
                    return IdleSeedingLimitMode::Global;
                }
            }(),
            changed
        );
        setChanged(idleSeedingLimit, torrentMap.value(idleSeedingLimitKey).toInt(), changed);
        setChanged(downloadDirectory, normalizePath(torrentMap.value(downloadDirectoryKey).toString()), changed);
        setChanged(creator, torrentMap.value(creatorKey).toString(), changed);

        const auto newCreationDateTime = static_cast<long long>(torrentMap.value(creationDateKey).toDouble()) * 1000;
        if (newCreationDateTime > 0) {
            if (newCreationDateTime != creationDateTime) {
                creationDateTime = newCreationDateTime;
                creationDate.setMSecsSinceEpoch(newCreationDateTime);
                changed = true;
            }
        } else {
            if (!creationDate.isNull()) {
                creationDateTime = -1;
                creationDate = QDateTime();
                changed = true;
            }
        }

        setChanged(comment, torrentMap.value(commentKey).toString(), changed);

        trackersAnnounceUrlsChanged = false;
        std::vector<Tracker> newTrackers;
        const QJsonArray trackerJsons(torrentMap.value("trackerStats"_l1).toArray());
        newTrackers.reserve(static_cast<size_t>(trackerJsons.size()));
        for (const auto& i : trackerJsons) {
            const QJsonObject trackerMap(i.toObject());
            const int trackerId = trackerMap.value("id"_l1).toInt();

            const auto found(std::find_if(trackers.begin(), trackers.end(), [&](const auto& tracker) {
                return tracker.id() == trackerId;
            }));

            if (found == trackers.end()) {
                newTrackers.emplace_back(trackerId, trackerMap);
                trackersAnnounceUrlsChanged = true;
            } else {
                const auto result = found->update(trackerMap);
                if (result.changed) {
                    changed = true;
                }
                if (result.announceUrlChanged) {
                    trackersAnnounceUrlsChanged = true;
                }
                newTrackers.push_back(std::move(*found));
            }
        }
        if (newTrackers.size() != trackers.size()) {
            trackersAnnounceUrlsChanged = true;
        }
        if (trackersAnnounceUrlsChanged) {
            changed = true;
        }
        trackers = std::move(newTrackers);

        setChanged(activeWebSeeders, torrentMap.value(activeWebSeedersKey).toInt(), changed);
        {
            std::vector<QString> newWebSeeders;
            const auto webSeedersStrings = torrentMap.value(webSeedersKey).toArray();
            newWebSeeders.reserve(static_cast<size_t>(webSeedersStrings.size()));
            std::transform(
                webSeedersStrings.begin(),
                webSeedersStrings.end(),
                std::back_insert_iterator(newWebSeeders),
                [](const QJsonValue& i) { return i.toString(); }
            );
            setChanged(webSeeders, std::move(newWebSeeders), changed);
        }

        return changed;
    }

    Torrent::Torrent(int id, const QJsonObject& torrentMap, Rpc* rpc, QObject* parent) : QObject(parent), mRpc(rpc) {
        mData.id = id;
        mData.hashString = torrentMap.value(hashStringKey).toString();
        const auto date = static_cast<long long>(torrentMap.value(addedDateKey).toDouble()) * 1000;
        mData.addedDate = QDateTime::fromMSecsSinceEpoch(date);
        mData.addedDateTime = date;
        update(torrentMap);
    }

    int Torrent::id() const { return mData.id; }

    const QString& Torrent::hashString() const { return mData.hashString; }

    const QString& Torrent::name() const { return mData.name; }

    TorrentData::Status Torrent::status() const { return mData.status; }

    TorrentData::Error Torrent::error() const { return mData.error; }

    bool Torrent::hasError() const { return mData.error != TorrentData::Error::None; }

    const QString& Torrent::errorString() const { return mData.errorString; }

    int Torrent::queuePosition() const { return mData.queuePosition; }

    long long Torrent::totalSize() const { return mData.totalSize; }

    long long Torrent::completedSize() const { return mData.completedSize; }

    long long Torrent::leftUntilDone() const { return mData.leftUntilDone; }

    long long Torrent::sizeWhenDone() const { return mData.sizeWhenDone; }

    double Torrent::percentDone() const { return mData.percentDone; }

    bool Torrent::isFinished() const { return mData.leftUntilDone == 0; }

    double Torrent::recheckProgress() const { return mData.recheckProgress; }

    int Torrent::eta() const { return mData.eta; }

    bool Torrent::isMetadataComplete() const { return mData.metadataComplete; }

    long long Torrent::downloadSpeed() const { return mData.downloadSpeed; }

    long long Torrent::uploadSpeed() const { return mData.uploadSpeed; }

    bool Torrent::isDownloadSpeedLimited() const { return mData.downloadSpeedLimited; }

    void Torrent::setDownloadSpeedLimited(bool limited) {
        mData.downloadSpeedLimited = limited;
        mRpc->setTorrentProperty(id(), downloadSpeedLimitedKey, limited);
    }

    int Torrent::downloadSpeedLimit() const { return mData.downloadSpeedLimit; }

    void Torrent::setDownloadSpeedLimit(int limit) {
        mData.downloadSpeedLimit = limit;
        mRpc->setTorrentProperty(id(), downloadSpeedLimitKey, limit);
    }

    bool Torrent::isUploadSpeedLimited() const { return mData.uploadSpeedLimited; }

    void Torrent::setUploadSpeedLimited(bool limited) {
        mData.uploadSpeedLimited = limited;
        mRpc->setTorrentProperty(id(), uploadSpeedLimitedKey, limited);
    }

    int Torrent::uploadSpeedLimit() const { return mData.uploadSpeedLimit; }

    void Torrent::setUploadSpeedLimit(int limit) {
        mData.uploadSpeedLimit = limit;
        mRpc->setTorrentProperty(id(), uploadSpeedLimitKey, limit);
    }

    long long Torrent::totalDownloaded() const { return mData.totalDownloaded; }

    long long Torrent::totalUploaded() const { return mData.totalUploaded; }

    double Torrent::ratio() const { return mData.ratio; }

    TorrentData::RatioLimitMode Torrent::ratioLimitMode() const { return mData.ratioLimitMode; }

    void Torrent::setRatioLimitMode(TorrentData::RatioLimitMode mode) {
        mData.ratioLimitMode = mode;
        mRpc->setTorrentProperty(id(), ratioLimitModeKey, static_cast<int>(mode));
    }

    double Torrent::ratioLimit() const { return mData.ratioLimit; }

    void Torrent::setRatioLimit(double limit) {
        mData.ratioLimit = limit;
        mRpc->setTorrentProperty(id(), ratioLimitKey, limit);
    }

    int Torrent::seeders() const { return mData.seeders; }

    int Torrent::leechers() const { return mData.leechers; }

    int Torrent::peersLimit() const { return mData.peersLimit; }

    void Torrent::setPeersLimit(int limit) {
        mData.peersLimit = limit;
        mRpc->setTorrentProperty(id(), peersLimitKey, limit);
    }

    const QDateTime& Torrent::addedDate() const { return mData.addedDate; }

    const QDateTime& Torrent::activityDate() const { return mData.activityDate; }

    const QDateTime& Torrent::doneDate() const { return mData.doneDate; }

    bool Torrent::honorSessionLimits() const { return mData.honorSessionLimits; }

    void Torrent::setHonorSessionLimits(bool honor) {
        mData.honorSessionLimits = honor;
        mRpc->setTorrentProperty(id(), honorSessionLimitsKey, honor);
    }

    TorrentData::Priority Torrent::bandwidthPriority() const { return mData.bandwidthPriority; }

    void Torrent::setBandwidthPriority(TorrentData::Priority priority) {
        mData.bandwidthPriority = priority;
        mRpc->setTorrentProperty(id(), bandwidthPriorityKey, static_cast<int>(priority));
    }

    TorrentData::IdleSeedingLimitMode Torrent::idleSeedingLimitMode() const { return mData.idleSeedingLimitMode; }

    void Torrent::setIdleSeedingLimitMode(TorrentData::IdleSeedingLimitMode mode) {
        mData.idleSeedingLimitMode = mode;
        mRpc->setTorrentProperty(id(), idleSeedingLimitModeKey, static_cast<int>(mode));
    }

    int Torrent::idleSeedingLimit() const { return mData.idleSeedingLimit; }

    void Torrent::setIdleSeedingLimit(int limit) {
        mData.idleSeedingLimit = limit;
        mRpc->setTorrentProperty(id(), idleSeedingLimitKey, limit);
    }

    const QString& Torrent::downloadDirectory() const { return mData.downloadDirectory; }

    bool Torrent::isSingleFile() const { return mData.singleFile; }

    const QString& Torrent::creator() const { return mData.creator; }

    const QDateTime& Torrent::creationDate() const { return mData.creationDate; }

    const QString& Torrent::comment() const { return mData.comment; }

    const std::vector<Tracker>& Torrent::trackers() const { return mData.trackers; }

    bool Torrent::isTrackersAnnounceUrlsChanged() const { return mData.trackersAnnounceUrlsChanged; }

    void Torrent::addTrackers(const QStringList& announceUrls) {
        mRpc->setTorrentProperty(id(), addTrackerKey, announceUrls, true);
    }

    void Torrent::setTracker(int trackerId, const QString& announce) {
        mRpc->setTorrentProperty(id(), replaceTrackerKey, QVariantList{trackerId, announce}, true);
    }

    void Torrent::removeTrackers(const QVariantList& ids) {
        mRpc->setTorrentProperty(id(), removeTrackerKey, ids, true);
    }

    const std::vector<QString>& Torrent::webSeeders() const { return mData.webSeeders; }

    int Torrent::activeWebSeeders() const { return mData.activeWebSeeders; }

    const TorrentData& Torrent::data() const { return mData; }

    bool Torrent::isFilesEnabled() const { return mFilesEnabled; }

    void Torrent::setFilesEnabled(bool enabled) {
        if (enabled != mFilesEnabled) {
            mFilesEnabled = enabled;
            if (mFilesEnabled) {
                mRpc->getTorrentsFiles({id()}, false);
            } else {
                mFiles.clear();
            }
        }
    }

    const std::vector<TorrentFile>& Torrent::files() const { return mFiles; }

    void Torrent::setFilesWanted(const QVariantList& files, bool wanted) {
        mRpc->setTorrentProperty(id(), wanted ? wantedFilesKey : unwantedFilesKey, files);
    }

    void Torrent::setFilesPriority(const QVariantList& files, TorrentFile::Priority priority) {
        QLatin1String propertyName;
        switch (priority) {
        case TorrentFile::Priority::Low:
            propertyName = lowPriorityKey;
            break;
        case TorrentFile::Priority::Normal:
            propertyName = normalPriorityKey;
            break;
        case TorrentFile::Priority::High:
            propertyName = highPriorityKey;
            break;
        }
        mRpc->setTorrentProperty(id(), propertyName, files);
    }

    void Torrent::renameFile(const QString& path, const QString& newName) {
        mRpc->renameTorrentFile(id(), path, newName);
    }

    bool Torrent::isPeersEnabled() const { return mPeersEnabled; }

    void Torrent::setPeersEnabled(bool enabled) {
        if (enabled != mPeersEnabled) {
            mPeersEnabled = enabled;
            if (mPeersEnabled) {
                mRpc->getTorrentsPeers({id()}, false);
            } else {
                mPeers.clear();
            }
        }
    }

    const std::vector<Peer>& Torrent::peers() const { return mPeers; }

    bool Torrent::isUpdated() const {
        bool updated = true;
        if (mFilesEnabled && !mFilesUpdated) {
            updated = false;
        }
        if (mPeersEnabled && !mPeersUpdated) {
            updated = false;
        }
        return updated;
    }

    void Torrent::checkThatFilesUpdated() {
        if (mFilesEnabled && !mFilesUpdated) {
            logWarning("Files were not updated for {}", *this);
            mFilesUpdated = true;
        }
    }

    void Torrent::checkThatPeersUpdated() {
        if (mPeersEnabled && !mPeersUpdated) {
            logWarning("Peers were not updated for {}", *this);
            mPeersUpdated = true;
        }
    }

    bool Torrent::update(const QJsonObject& torrentMap) {
        mFilesUpdated = false;
        mPeersUpdated = false;
        const bool c = mData.update(torrentMap);
        emit updated();
        if (c) {
            emit changed();
        }
        return c;
    }

    void Torrent::updateFiles(const QJsonObject& torrentMap) {
        std::vector<int> changed;

        const QJsonArray fileStats(torrentMap.value("fileStats"_l1).toArray());
        if (!fileStats.isEmpty()) {
            if (mFiles.empty()) {
                const QJsonArray fileJsons(torrentMap.value("files"_l1).toArray());
                mFiles.reserve(static_cast<size_t>(fileStats.size()));
                changed.reserve(static_cast<size_t>(fileStats.size()));
                for (QJsonArray::size_type i = 0, max = fileStats.size(); i < max; ++i) {
                    mFiles.emplace_back(i, fileJsons[i].toObject(), fileStats[i].toObject());
                    changed.push_back(static_cast<int>(i));
                }
            } else {
                for (QJsonArray::size_type i = 0, max = fileStats.size(); i < max; ++i) {
                    TorrentFile& file = mFiles[static_cast<size_t>(i)];
                    if (file.update(fileStats[i].toObject())) {
                        changed.push_back(static_cast<int>(i));
                    }
                }
            }
        }

        mFilesUpdated = true;

        emit filesUpdated(changed);
        emit mRpc->torrentFilesUpdated(this, changed);
    }

    namespace {
        using NewPeer = std::pair<QJsonObject, QString>;

        class PeersListUpdater : public ItemListUpdater<Peer, NewPeer, std::vector<NewPeer>> {
        public:
            std::vector<std::pair<int, int>> removedIndexRanges;
            std::vector<std::pair<int, int>> changedIndexRanges;
            int addedCount = 0;

        protected:
            std::vector<NewPeer>::iterator
            findNewItemForItem(std::vector<NewPeer>& newPeers, const Peer& peer) override {
                const auto& address = peer.address;
                return std::find_if(newPeers.begin(), newPeers.end(), [address](const auto& newPeer) {
                    const auto& [json, newPeerAddress] = newPeer;
                    return newPeerAddress == address;
                });
            }

            void onAboutToRemoveItems(size_t, size_t) override{};

            void onRemovedItems(size_t first, size_t last) override {
                removedIndexRanges.emplace_back(static_cast<int>(first), static_cast<int>(last));
            }

            bool updateItem(Peer& peer, NewPeer&& newPeer) override {
                const auto& [json, address] = newPeer;
                return peer.update(json);
            }

            void onChangedItems(size_t first, size_t last) override {
                changedIndexRanges.emplace_back(static_cast<int>(first), static_cast<int>(last));
            }

            Peer createItemFromNewItem(NewPeer&& newPeer) override {
                auto& [json, address] = newPeer;
                return Peer(std::move(address), json);
            }

            void onAboutToAddItems(size_t) override {}

            void onAddedItems(size_t count) override { addedCount = static_cast<int>(count); };
        };
    }

    void Torrent::updatePeers(const QJsonObject& torrentMap) {
        std::vector<NewPeer> newPeers;
        {
            const QJsonArray peers(torrentMap.value("peers"_l1).toArray());
            newPeers.reserve(static_cast<size_t>(peers.size()));
            for (const auto& i : peers) {
                QJsonObject json = i.toObject();
                QString address(json.value(Peer::addressKey).toString());
                newPeers.emplace_back(std::move(json), std::move(address));
            }
        }

        PeersListUpdater updater;
        updater.update(mPeers, std::move(newPeers));

        mPeersUpdated = true;

        emit peersUpdated(updater.removedIndexRanges, updater.changedIndexRanges, updater.addedCount);
        emit mRpc
            ->torrentPeersUpdated(this, updater.removedIndexRanges, updater.changedIndexRanges, updater.addedCount);
    }

    void Torrent::checkSingleFile(const QJsonObject& torrentMap) {
        mData.singleFile = (torrentMap.value(prioritiesKey).toArray().size() == 1);
    }
}

fmt::format_context::iterator
fmt::formatter<libtremotesf::Torrent>::format(const libtremotesf::Torrent& torrent, format_context& ctx) FORMAT_CONST {
    return format_to(ctx.out(), "Torrent(id={}, name={})", torrent.id(), torrent.name());
}
