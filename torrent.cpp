/*
 * Libtremotesf
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

#include "torrent.h"

#include <type_traits>

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QLocale>

#include "rpc.h"
#include "serversettings.h"
#include "stdutils.h"
#include "tracker.h"

namespace libtremotesf
{
    namespace
    {
        const QString hashStringKey(QLatin1String("hashString"));
        const QString addedDateKey(QLatin1String("addedDate"));

        const QString nameKey(QLatin1String("name"));

        const QString errorStringKey(QLatin1String("errorString"));
        const QString queuePositionKey(QLatin1String("queuePosition"));

        const QString totalSizeKey(QLatin1String("totalSize"));
        const QString completedSizeKey(QLatin1String("haveValid"));
        const QString leftUntilDoneKey(QLatin1String("leftUntilDone"));
        const QString sizeWhenDoneKey(QLatin1String("sizeWhenDone"));
        const QString percentDoneKey(QLatin1String("percentDone"));
        const QString recheckProgressKey(QLatin1String("recheckProgress"));
        const QString etaKey(QLatin1String("eta"));

        const QString downloadSpeedKey(QLatin1String("rateDownload"));
        const QString uploadSpeedKey(QLatin1String("rateUpload"));

        const QString downloadSpeedLimitedKey(QLatin1String("downloadLimited"));
        const QString downloadSpeedLimitKey(QLatin1String("downloadLimit"));
        const QString uploadSpeedLimitedKey(QLatin1String("uploadLimited"));
        const QString uploadSpeedLimitKey(QLatin1String("uploadLimit"));

        const QString totalDownloadedKey(QLatin1String("downloadedEver"));
        const QString totalUploadedKey(QLatin1String("uploadedEver"));
        const QString ratioKey(QLatin1String("uploadRatio"));
        const QString ratioLimitModeKey(QLatin1String("seedRatioMode"));
        const QString ratioLimitKey(QLatin1String("seedRatioLimit"));

        const QString seedersKey(QLatin1String("peersSendingToUs"));
        const QString leechersKey(QLatin1String("peersGettingFromUs"));

        const QString errorKey(QLatin1String("error"));
        const QString statusKey(QLatin1String("status"));

        const QString activityDateKey(QLatin1String("activityDate"));
        const QString doneDateKey(QLatin1String("doneDate"));

        const QString peersLimitKey(QLatin1String("peer-limit"));
        const QString honorSessionLimitsKey(QLatin1String("honorsSessionLimits"));
        const QString bandwidthPriorityKey(QLatin1String("bandwidthPriority"));
        const QString idleSeedingLimitModeKey(QLatin1String("seedIdleMode"));
        const QString idleSeedingLimitKey(QLatin1String("seedRatioLimit"));
        const QString downloadDirectoryKey(QLatin1String("downloadDir"));
        const QString prioritiesKey(QLatin1String("priorities"));
        const QString creatorKey(QLatin1String("creator"));
        const QString creationDateKey(QLatin1String("dateCreated"));
        const QString commentKey(QLatin1String("comment"));

        const QString trackerStatsKey(QLatin1String("trackerStats"));
        const QString trackerIdKey(QLatin1String("id"));

        const QString filesKey(QLatin1String("files"));
        const QString fileStatsKey(QLatin1String("fileStats"));

        const QString peersKey(QLatin1String("peers"));

        const QString wantedFilesKey(QLatin1String("files-wanted"));
        const QString unwantedFilesKey(QLatin1String("files-unwanted"));

        const QString lowPriorityKey(QLatin1String("priority-low"));
        const QString normalPriorityKey(QLatin1String("priority-normal"));
        const QString highPriorityKey(QLatin1String("priority-high"));

        const QString addTrackerKey(QLatin1String("trackerAdd"));
        const QString replaceTrackerKey(QLatin1String("trackerReplace"));
        const QString removeTrackerKey(QLatin1String("trackerRemove"));

        template<typename T, typename std::enable_if<std::is_scalar<T>::value && !std::is_floating_point<T>::value, int>::type = 0>
        void setChanged(T& value, T newValue, bool& changed)
        {
            if (newValue != value) {
                value = newValue;
                changed = true;
            }
        }

        template<typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
        void setChanged(T& value, T newValue, bool& changed)
        {
            if (!qFuzzyCompare(newValue, value)) {
                value = newValue;
                changed = true;
            }
        }

        template<typename T, typename std::enable_if<!std::is_scalar<T>::value, int>::type = 0>
        void setChanged(T& value, T&& newValue, bool& changed)
        {
            if (newValue != value) {
                value = std::forward<T>(newValue);
                changed = true;
            }
        }
    }

    const QLatin1String Torrent::idKey("id");

    TorrentFile::TorrentFile(const QJsonObject& fileMap, const QJsonObject& fileStatsMap)
        : size(fileMap.value(QLatin1String("length")).toDouble())
    {
        QStringList p(fileMap.value(QLatin1String("name")).toString().split(QLatin1Char('/'), QString::SkipEmptyParts));
        path.reserve(p.size());
        for (QString& part : p) {
            path.push_back(std::move(part));
        }
        update(fileStatsMap);
    }

    void TorrentFile::update(const QJsonObject& fileStatsMap)
    {
        changed = false;
        setChanged(completedSize, static_cast<long long>(fileStatsMap.value(QLatin1String("bytesCompleted")).toDouble()), changed);
        setChanged(wanted, fileStatsMap.value(QLatin1String("wanted")).toBool(), changed);
        setChanged(priority, [&]() {
            switch (int priority = fileStatsMap.value(QLatin1String("priority")).toInt()) {
            case TorrentFile::LowPriority:
            case TorrentFile::NormalPriority:
            case TorrentFile::HighPriority:
                return static_cast<TorrentFile::Priority>(priority);
            default:
                return TorrentFile::NormalPriority;
            }
        }(), changed);
     }

    Peer::Peer(QString&& address, const QJsonObject& peerMap)
        : address(std::move(address))
    {
        update(peerMap);
    }

    void Peer::update(const QJsonObject& peerMap)
    {
        downloadSpeed = peerMap.value(QLatin1String("rateToClient")).toDouble();
        uploadSpeed = peerMap.value(QLatin1String("rateToPeer")).toDouble();
        progress = peerMap.value(QLatin1String("progress")).toDouble();
        flags = peerMap.value(QLatin1String("flagStr")).toString();
        client = peerMap.value(QLatin1String("clientName")).toString();
    }

    Torrent::Torrent(int id, const QJsonObject& torrentMap, Rpc* rpc)
        : mId(id),
          mHashString(torrentMap.value(hashStringKey).toString()),
          mAddedDate(QDateTime::fromMSecsSinceEpoch(torrentMap.value(addedDateKey).toDouble() * 1000)),
          mRpc(rpc)
    {
        update(torrentMap);
    }

    int Torrent::id() const
    {
        return mId;
    }

    const QString& Torrent::hashString() const
    {
        return mHashString;
    }

    const QString& Torrent::name() const
    {
        return mName;
    }

    Torrent::Status Torrent::status() const
    {
        return mStatus;
    }

    QString Torrent::errorString() const
    {
        return mErrorString;
    }

    int Torrent::queuePosition() const
    {
        return mQueuePosition;
    }

    long long Torrent::totalSize() const
    {
        return mTotalSize;
    }

    long long Torrent::completedSize() const
    {
        return mCompletedSize;
    }

    long long Torrent::leftUntilDone() const
    {
        return mLeftUntilDone;
    }

    long long Torrent::sizeWhenDone() const
    {
        return mSizeWhenDone;
    }

    double Torrent::percentDone() const
    {
        return mPercentDone;
    }

    bool Torrent::isFinished() const
    {
        return mLeftUntilDone == 0;
    }

    double Torrent::recheckProgress() const
    {
        return mRecheckProgress;
    }

    int Torrent::eta() const
    {
        return mEta;
    }

    long long Torrent::downloadSpeed() const
    {
        return mDownloadSpeed;
    }

    long long Torrent::uploadSpeed() const
    {
        return mUploadSpeed;
    }

    bool Torrent::isDownloadSpeedLimited() const
    {
        return mDownloadSpeedLimited;
    }

    void Torrent::setDownloadSpeedLimited(bool limited)
    {
        mDownloadSpeedLimited = limited;
        emit limitsEdited();
        mRpc->setTorrentProperty(mId, downloadSpeedLimitedKey, mDownloadSpeedLimited);
    }

    int Torrent::downloadSpeedLimit() const
    {
        return mDownloadSpeedLimit;
    }

    void Torrent::setDownloadSpeedLimit(int limit)
    {
        mDownloadSpeedLimit = limit;
        emit limitsEdited();
        mRpc->setTorrentProperty(mId, downloadSpeedLimitKey, mRpc->serverSettings()->fromKibiBytes(mDownloadSpeedLimit));
    }

    bool Torrent::isUploadSpeedLimited() const
    {
        return mUploadSpeedLimited;
    }

    void Torrent::setUploadSpeedLimited(bool limited)
    {
        mUploadSpeedLimited = limited;
        emit limitsEdited();
        mRpc->setTorrentProperty(mId, uploadSpeedLimitedKey, mUploadSpeedLimited);
    }

    int Torrent::uploadSpeedLimit() const
    {
        return mUploadSpeedLimit;
    }

    void Torrent::setUploadSpeedLimit(int limit)
    {
        mUploadSpeedLimit = limit;
        emit limitsEdited();
        mRpc->setTorrentProperty(mId, uploadSpeedLimitKey, mRpc->serverSettings()->fromKibiBytes(mUploadSpeedLimit));
    }

    long long Torrent::totalDownloaded() const
    {
        return mTotalDownloaded;
    }

    long long Torrent::totalUploaded() const
    {
        return mTotalUploaded;
    }

    double Torrent::ratio() const
    {
        return mRatio;
    }

    Torrent::RatioLimitMode Torrent::ratioLimitMode() const
    {
        return mRatioLimitMode;
    }

    void Torrent::setRatioLimitMode(Torrent::RatioLimitMode mode)
    {
        mRatioLimitMode = mode;
        emit limitsEdited();
        mRpc->setTorrentProperty(mId, ratioLimitModeKey, mRatioLimitMode);
    }

    double Torrent::ratioLimit() const
    {
        return mRatioLimit;
    }

    void Torrent::setRatioLimit(double limit)
    {
        mRatioLimit = limit;
        emit limitsEdited();
        mRpc->setTorrentProperty(mId, ratioLimitKey, mRatioLimit);
    }

    int Torrent::seeders() const
    {
        return mSeeders;
    }

    int Torrent::leechers() const
    {
        return mLeechers;
    }

    int Torrent::peersLimit() const
    {
        return mPeersLimit;
    }

    void Torrent::setPeersLimit(int limit)
    {
        mPeersLimit = limit;
        emit limitsEdited();
        mRpc->setTorrentProperty(mId, peersLimitKey, mPeersLimit);
    }

    const QDateTime& Torrent::addedDate() const
    {
        return mAddedDate;
    }

    const QDateTime& Torrent::activityDate() const
    {
        return mActivityDate;
    }

    const QDateTime& Torrent::doneDate() const
    {
        return mDoneDate;
    }

    bool Torrent::honorSessionLimits() const
    {
        return mHonorSessionLimits;
    }

    void Torrent::setHonorSessionLimits(bool honor)
    {
        mHonorSessionLimits = honor;
        emit limitsEdited();
        mRpc->setTorrentProperty(mId, honorSessionLimitsKey, mHonorSessionLimits);
    }

    Torrent::Priority Torrent::bandwidthPriority() const
    {
        return mBandwidthPriority;
    }

    void Torrent::setBandwidthPriority(Torrent::Priority priority)
    {
        mBandwidthPriority = priority;
        emit limitsEdited();
        mRpc->setTorrentProperty(mId, bandwidthPriorityKey, mBandwidthPriority);
    }

    Torrent::IdleSeedingLimitMode Torrent::idleSeedingLimitMode() const
    {
        return mIdleSeedingLimitMode;
    }

    void Torrent::setIdleSeedingLimitMode(Torrent::IdleSeedingLimitMode mode)
    {
        mIdleSeedingLimitMode = mode;
        emit limitsEdited();
        mRpc->setTorrentProperty(mId, idleSeedingLimitModeKey, mIdleSeedingLimitMode);
    }

    int Torrent::idleSeedingLimit() const
    {
        return mIdleSeedingLimit;
    }

    void Torrent::setIdleSeedingLimit(int limit)
    {
        mIdleSeedingLimit = limit;
        emit limitsEdited();
        mRpc->setTorrentProperty(mId, idleSeedingLimitKey, mIdleSeedingLimit);
    }

    const QString& Torrent::downloadDirectory() const
    {
        return mDownloadDirectory;
    }

    bool Torrent::isSingleFile() const
    {
        return mSingleFile;
    }

    const QString& Torrent::creator() const
    {
        return mCreator;
    }

    const QDateTime& Torrent::creationDate() const
    {
        return mCreationDate;
    }

    const QString& Torrent::comment() const
    {
        return mComment;
    }

    bool Torrent::isFilesEnabled() const
    {
        return mFilesEnabled;
    }

    bool Torrent::isFilesLoaded() const
    {
        return mFilesLoaded;
    }

    void Torrent::setFilesEnabled(bool enabled)
    {
        if (enabled != mFilesEnabled) {
            mFilesEnabled = enabled;
            if (mFilesEnabled) {
                mRpc->getTorrentFiles(mId, false);
            } else {
                mFiles.clear();
                mFilesLoaded = false;
            }
        }
    }

    bool Torrent::isFilesUpdated() const
    {
        return mFilesUpdated;
    }

    const std::vector<std::shared_ptr<TorrentFile>>& Torrent::files() const
    {
        return mFiles;
    }

    void Torrent::setFilesWanted(const QVariantList& files, bool wanted)
    {
        mRpc->setTorrentProperty(mId,
                                 wanted ? wantedFilesKey
                                        : unwantedFilesKey,
                                 files);
    }

    void Torrent::setFilesPriority(const QVariantList& files, TorrentFile::Priority priority)
    {
        QString propertyName;
        switch (priority) {
        case TorrentFile::LowPriority:
            propertyName = lowPriorityKey;
            break;
        case TorrentFile::NormalPriority:
            propertyName = normalPriorityKey;
            break;
        case TorrentFile::HighPriority:
            propertyName = highPriorityKey;
            break;
        }
        mRpc->setTorrentProperty(mId, propertyName, files);
    }

    void Torrent::renameFile(const QString& path, const QString& newName)
    {
        mRpc->renameTorrentFile(mId, path, newName);
    }

    const std::vector<std::shared_ptr<Tracker>>& Torrent::trackers() const
    {
        return mTrackers;
    }

    void Torrent::addTracker(const QString& announce)
    {
        mRpc->setTorrentProperty(mId, addTrackerKey, QVariantList{announce}, true);
    }

    void Torrent::setTracker(int trackerId, const QString& announce)
    {
        mRpc->setTorrentProperty(mId, replaceTrackerKey, QVariantList{trackerId, announce}, true);
    }

    void Torrent::removeTrackers(const QVariantList& ids)
    {
        mRpc->setTorrentProperty(mId, removeTrackerKey, ids, true);
    }

    bool Torrent::isPeersEnabled() const
    {
        return mPeersEnabled;
    }

    void Torrent::setPeersEnabled(bool enabled)
    {
        if (enabled != mPeersEnabled) {
            mPeersEnabled = enabled;
            if (mPeersEnabled) {
                mRpc->getTorrentPeers(mId, false);
            } else {
                mPeers.clear();
                mPeersLoaded = false;
            }
        }
    }

    bool Torrent::isPeersLoaded() const
    {
        return mPeersLoaded;
    }

    bool Torrent::isPeersUpdated() const
    {
        return mPeersUpdated;
    }

    const std::vector<std::shared_ptr<Peer>>& Torrent::peers() const
    {
        return mPeers;
    }

    bool Torrent::isUpdated() const
    {
        bool updated = true;
        if (mFilesEnabled && !mFilesUpdated) {
            updated = false;
        }
        if (mPeersEnabled && !mPeersUpdated) {
            updated = false;
        }
        return updated;
    }

    void Torrent::update(const QJsonObject& torrentMap)
    {
        mChanged = false;

        setChanged(mName, torrentMap.value(nameKey).toString(), mChanged);

        setChanged(mErrorString, torrentMap.value(errorStringKey).toString(), mChanged);
        setChanged(mQueuePosition, torrentMap.value(queuePositionKey).toInt(), mChanged);
        setChanged(mTotalSize, static_cast<long long>(torrentMap.value(totalSizeKey).toDouble()), mChanged);
        setChanged(mCompletedSize, static_cast<long long>(torrentMap.value(completedSizeKey).toDouble()), mChanged);
        setChanged(mLeftUntilDone, static_cast<long long>(torrentMap.value(leftUntilDoneKey).toDouble()), mChanged);
        setChanged(mSizeWhenDone, static_cast<long long>(torrentMap.value(sizeWhenDoneKey).toDouble()), mChanged);
        setChanged(mPercentDone, torrentMap.value(percentDoneKey).toDouble(), mChanged);
        setChanged(mRecheckProgress, torrentMap.value(recheckProgressKey).toDouble(), mChanged);
        setChanged(mEta, torrentMap.value(etaKey).toInt(), mChanged);

        setChanged(mDownloadSpeed, static_cast<long long>(torrentMap.value(downloadSpeedKey).toDouble()), mChanged);
        setChanged(mUploadSpeed, static_cast<long long>(torrentMap.value(uploadSpeedKey).toDouble()), mChanged);

        setChanged(mDownloadSpeedLimited, torrentMap.value(downloadSpeedLimitedKey).toBool(), mChanged);
        setChanged(mDownloadSpeedLimit, mRpc->serverSettings()->toKibiBytes(torrentMap.value(downloadSpeedLimitKey).toInt()), mChanged);
        setChanged(mUploadSpeedLimited, torrentMap.value(uploadSpeedLimitedKey).toBool(), mChanged);
        setChanged(mUploadSpeedLimit, mRpc->serverSettings()->toKibiBytes(torrentMap.value(uploadSpeedLimitKey).toInt()), mChanged);

        setChanged(mTotalDownloaded, static_cast<long long>(torrentMap.value(totalDownloadedKey).toDouble()), mChanged);
        setChanged(mTotalUploaded, static_cast<long long>(torrentMap.value(totalUploadedKey).toDouble()), mChanged);
        setChanged(mRatio, torrentMap.value(ratioKey).toDouble(), mChanged);

        setChanged(mRatioLimitMode, [&]() {
            switch (int mode = torrentMap.value(ratioLimitModeKey).toInt()) {
            case GlobalRatioLimit:
            case SingleRatioLimit:
            case UnlimitedRatio:
                return static_cast<RatioLimitMode>(mode);
            default:
                return GlobalRatioLimit;
            }
        }(), mChanged);
        setChanged(mRatioLimit, torrentMap.value(ratioLimitKey).toDouble(), mChanged);

        setChanged(mSeeders, torrentMap.value(seedersKey).toInt(), mChanged);
        setChanged(mLeechers, torrentMap.value(leechersKey).toInt(), mChanged);

        const bool stalled = (mSeeders == 0 && mLeechers == 0);
        if (torrentMap.value(errorKey).toInt() == 0) {
            switch (torrentMap.value(statusKey).toInt()) {
            case 0:
                setChanged(mStatus, Paused, mChanged);
                break;
            case 1:
                setChanged(mStatus, QueuedForChecking, mChanged);
                break;
            case 2:
                setChanged(mStatus, Checking, mChanged);
                break;
            case 3:
                setChanged(mStatus, QueuedForDownloading, mChanged);
                break;
            case 4:
                if (stalled) {
                    setChanged(mStatus, StalledDownloading, mChanged);
                } else {
                    setChanged(mStatus, Downloading, mChanged);
                }
                break;
            case 5:
                setChanged(mStatus, QueuedForSeeding, mChanged);
                break;
            case 6:
                if (stalled) {
                    setChanged(mStatus, StalledSeeding, mChanged);
                } else {
                    setChanged(mStatus, Seeding, mChanged);
                }
            }
        } else {
            setChanged(mStatus, Errored, mChanged);
        }

        setChanged(mPeersLimit, torrentMap.value(peersLimitKey).toInt(), mChanged);

        const long long activityDate = torrentMap.value(activityDateKey).toDouble() * 1000;
        if (activityDate > 0) {
            if (activityDate != mActivityDateTime) {
                mActivityDateTime = activityDate;
                mActivityDate.setMSecsSinceEpoch(activityDate);
                mChanged = true;
            }
        } else {
            if (!mActivityDate.isNull()) {
                mActivityDateTime = -1;
                mActivityDate = QDateTime();
                mChanged = true;
            }
        }
        const long long doneDate = torrentMap.value(doneDateKey).toDouble() * 1000;
        if (doneDate > 0) {
            if (doneDate != mDoneDateTime) {
                mDoneDateTime = doneDate;
                mDoneDate.setMSecsSinceEpoch(doneDate);
                mChanged = true;
            }
        } else {
            if (!mDoneDate.isNull()) {
                mDoneDateTime = -1;
                mDoneDate = QDateTime();
                mChanged = true;
            }
        }

        setChanged(mHonorSessionLimits, torrentMap.value(honorSessionLimitsKey).toBool(), mChanged);
        setChanged(mBandwidthPriority, [&]() {
            switch (int priority = torrentMap.value(bandwidthPriorityKey).toInt()) {
            case LowPriority:
            case NormalPriority:
            case HighPriority:
                return static_cast<Priority>(priority);
            default:
                return NormalPriority;
            }
        }(), mChanged);
        setChanged(mIdleSeedingLimitMode, [&]() {
            switch (int mode = torrentMap.value(idleSeedingLimitModeKey).toInt()) {
            case GlobalIdleSeedingLimit:
            case SingleIdleSeedingLimit:
            case UnlimitedIdleSeeding:
                return static_cast<IdleSeedingLimitMode>(mode);
            default:
                return GlobalIdleSeedingLimit;
            }
        }(), mChanged);
        setChanged(mIdleSeedingLimit, torrentMap.value(idleSeedingLimitKey).toInt(), mChanged);
        setChanged(mDownloadDirectory, torrentMap.value(downloadDirectoryKey).toString(), mChanged);
        setChanged(mSingleFile, torrentMap.value(prioritiesKey).toArray().size() == 1, mChanged);
        setChanged(mCreator, torrentMap.value(creatorKey).toString(), mChanged);

        const long long creationDate = torrentMap.value(creationDateKey).toDouble() * 1000;
        if (creationDate > 0) {
            if (creationDate != mCreationDateTime) {
                mCreationDateTime = creationDate;
                mCreationDate.setMSecsSinceEpoch(creationDate);
                mChanged = true;
            }
        } else {
            if (!mCreationDate.isNull()) {
                mCreationDateTime = -1;
                mCreationDate = QDateTime();
                mChanged = true;
            }
        }

        setChanged(mComment, torrentMap.value(commentKey).toString(), mChanged);

        std::vector<std::shared_ptr<Tracker>> trackers;
        const QJsonArray trackersJson(torrentMap.value(trackerStatsKey).toArray());
        trackers.reserve(trackersJson.size());
        for (const QJsonValue& trackerVariant : trackersJson) {
            const QJsonObject trackerMap(trackerVariant.toObject());
            const int id = trackerMap.value(trackerIdKey).toInt();

            std::shared_ptr<Tracker> tracker;
            for (const std::shared_ptr<Tracker>& existingTracker : mTrackers) {
                if (existingTracker->id() == id) {
                    tracker = existingTracker;
                    break;
                }
            }
            if (tracker) {
                tracker->update(trackerMap);
            } else {
                tracker = std::make_shared<Tracker>(id, trackerMap);
            }

            trackers.push_back(std::move(tracker));
        }
        mTrackers = std::move(trackers);

        mFilesUpdated = false;
        mPeersUpdated = false;

        emit updated();
    }

    void Torrent::updateFiles(const QJsonObject& torrentMap)
    {
        const QJsonArray fileStats(torrentMap.value(fileStatsKey).toArray());

        if (!fileStats.isEmpty()) {
            if (mFiles.empty()) {
                const QJsonArray files(torrentMap.value(filesKey).toArray());
                mFiles.reserve(fileStats.size());
                for (int i = 0, max = fileStats.size(); i < max; ++i) {
                    mFiles.push_back(std::make_shared<TorrentFile>(files[i].toObject(), fileStats[i].toObject()));
                }
            } else {
                for (int i = 0, max = fileStats.size(); i < max; ++i) {
                    mFiles[i]->update(fileStats[i].toObject());
                }
            }
        }

        mFilesUpdated = true;
        mFilesLoaded = true;
        emit filesUpdated(mFiles);
    }

    void Torrent::updatePeers(const QJsonObject& torrentMap)
    {
        std::vector<QString> addresses;
        const std::vector<QJsonObject> peers([&]() {
            std::vector<QJsonObject> p;
            const QJsonArray peerValues(torrentMap.value(peersKey).toArray());
            p.reserve(peerValues.size());
            addresses.reserve(peerValues.size());

            for (const QJsonValue& peerValue : peerValues) {
                QJsonObject peerMap(peerValue.toObject());
                addresses.push_back(peerMap.value(QLatin1String("address")).toString());
                p.push_back(std::move(peerMap));
            }

            return p;
        }());

        for (int i = mPeers.size() - 1; i >= 0; --i) {
            if (!tremotesf::contains(addresses, mPeers[i]->address)) {
                mPeers.erase(mPeers.begin() + i);
            }
        }

        mPeers.reserve(peers.size());

        for (int i = 0, max = peers.size(); i < max; ++i) {
            const QJsonObject& peerMap = peers[i];
            QString& address = addresses[i];

            int row = -1;
            for (int i = 0, max = mPeers.size(); i < max; ++i) {
                if (mPeers[i]->address == address) {
                    row = i;
                    break;
                }
            }
            if (row == -1) {
                mPeers.push_back(std::make_shared<Peer>(std::move(address), peerMap));
            } else {
                mPeers[row]->update(peerMap);
            }
        }

        mPeersUpdated = true;
        mPeersLoaded = true;
        emit peersUpdated(mPeers);
    }

    bool Torrent::isChanged() const
    {
        return mChanged;
    }
}
