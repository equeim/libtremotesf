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

#include "serversettings.h"

#include <QJsonObject>

#include "rpc.h"
#include "stdutils.h"

namespace libtremotesf
{
    namespace
    {
        const auto downloadDirectoryKey(QJsonKeyStringInit("download-dir"));
        const auto trashTorrentFilesKey(QJsonKeyStringInit("trash-original-torrent-files"));
        const auto startAddedTorrentsKey(QJsonKeyStringInit("start-added-torrents"));
        const auto renameIncompleteFilesKey(QJsonKeyStringInit("rename-partial-files"));
        const auto incompleteDirectoryEnabledKey(QJsonKeyStringInit("incomplete-dir-enabled"));
        const auto incompleteDirectoryKey(QJsonKeyStringInit("incomplete-dir"));

        const auto ratioLimitedKey(QJsonKeyStringInit("seedRatioLimited"));
        const auto ratioLimitKey(QJsonKeyStringInit("seedRatioLimit"));
        const auto idleSeedingLimitedKey(QJsonKeyStringInit("idle-seeding-limit-enabled"));
        const auto idleSeedingLimitKey(QJsonKeyStringInit("idle-seeding-limit"));

        const auto downloadQueueEnabledKey(QJsonKeyStringInit("download-queue-enabled"));
        const auto downloadQueueSizeKey(QJsonKeyStringInit("download-queue-size"));
        const auto seedQueueEnabledKey(QJsonKeyStringInit("seed-queue-enabled"));
        const auto seedQueueSizeKey(QJsonKeyStringInit("seed-queue-size"));
        const auto idleQueueLimitedKey(QJsonKeyStringInit("queue-stalled-enabled"));
        const auto idleQueueLimitKey(QJsonKeyStringInit("queue-stalled-minutes"));

        const auto downloadSpeedLimitedKey(QJsonKeyStringInit("speed-limit-down-enabled"));
        const auto downloadSpeedLimitKey(QJsonKeyStringInit("speed-limit-down"));
        const auto uploadSpeedLimitedKey(QJsonKeyStringInit("speed-limit-up-enabled"));
        const auto uploadSpeedLimitKey(QJsonKeyStringInit("speed-limit-up"));
        const auto alternativeSpeedLimitsEnabledKey(QJsonKeyStringInit("alt-speed-enabled"));
        const auto alternativeDownloadSpeedLimitKey(QJsonKeyStringInit("alt-speed-down"));
        const auto alternativeUploadSpeedLimitKey(QJsonKeyStringInit("alt-speed-up"));
        const auto alternativeSpeedLimitsScheduledKey(QJsonKeyStringInit("alt-speed-time-enabled"));
        const auto alternativeSpeedLimitsBeginTimeKey(QJsonKeyStringInit("alt-speed-time-begin"));
        const auto alternativeSpeedLimitsEndTimeKey(QJsonKeyStringInit("alt-speed-time-end"));
        const auto alternativeSpeedLimitsDaysKey(QJsonKeyStringInit("alt-speed-time-day"));

        const auto peerPortKey(QJsonKeyStringInit("peer-port"));
        const auto randomPortEnabledKey(QJsonKeyStringInit("peer-port-random-on-start"));
        const auto portForwardingEnabledKey(QJsonKeyStringInit("port-forwarding-enabled"));

        const auto encryptionModeKey(QJsonKeyStringInit("encryption"));
        const QLatin1String encryptionModeAllowed("tolerated");
        const QLatin1String encryptionModePreferred("preferred");
        const QLatin1String encryptionModeRequired("required");

        inline QString encryptionModeString(ServerSettings::EncryptionMode mode)
        {
            switch (mode) {
            case ServerSettings::AllowedEncryption:
                return encryptionModeAllowed;
            case ServerSettings::PreferredEncryption:
                return encryptionModePreferred;
            case ServerSettings::RequiredEncryption:
                return encryptionModeRequired;
            }
            return QString();
        }

        const auto utpEnabledKey(QJsonKeyStringInit("utp-enabled"));
        const auto pexEnabledKey(QJsonKeyStringInit("pex-enabled"));
        const auto dhtEnabledKey(QJsonKeyStringInit("dht-enabled"));
        const auto lpdEnabledKey(QJsonKeyStringInit("lpd-enabled"));
        const auto maximumPeersPerTorrentKey(QJsonKeyStringInit("peer-limit-per-torrent"));
        const auto maximumPeersGloballyKey(QJsonKeyStringInit("peer-limit-global"));
    }

    ServerSettings::ServerSettings(Rpc* rpc, QObject* parent)
        : QObject(parent),
          mRpc(rpc),
          mSaveOnSet(true)
    {

    }

    int ServerSettings::rpcVersion() const
    {
        return mRpcVersion;
    }

    int ServerSettings::minimumRpcVersion() const
    {
        return mMinimumRpcVersion;
    }

    bool ServerSettings::canRenameFiles() const
    {
        return (mRpcVersion >= 15);
    }

    bool ServerSettings::canShowFreeSpaceForPath() const
    {
        return (mRpcVersion >= 15);
    }

    const QString& ServerSettings::downloadDirectory() const
    {
        return mDownloadDirectory;
    }

    void ServerSettings::setDownloadDirectory(const QString& directory)
    {
        if (directory != mDownloadDirectory) {
            mDownloadDirectory = directory;
            if (mSaveOnSet) {
                mRpc->setSessionProperty(downloadDirectoryKey, mDownloadDirectory);
            }
        }
    }

    bool ServerSettings::startAddedTorrents() const
    {
        return mStartAddedTorrents;
    }

    void ServerSettings::setStartAddedTorrents(bool start)
    {
        mStartAddedTorrents = start;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(startAddedTorrentsKey, mStartAddedTorrents);
        }
    }

    bool ServerSettings::trashTorrentFiles() const
    {
        return mTrashTorrentFiles;
    }

    void ServerSettings::setTrashTorrentFiles(bool trash)
    {
        mTrashTorrentFiles = trash;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(trashTorrentFilesKey, mTrashTorrentFiles);
        }
    }

    bool ServerSettings::renameIncompleteFiles() const
    {
        return mRenameIncompleteFiles;
    }

    void ServerSettings::setRenameIncompleteFiles(bool rename)
    {
        mRenameIncompleteFiles = rename;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(renameIncompleteFilesKey, mRenameIncompleteFiles);
        }
    }

    bool ServerSettings::isIncompleteDirectoryEnabled() const
    {
        return mIncompleteDirectoryEnabled;
    }

    void ServerSettings::setIncompleteDirectoryEnabled(bool enabled)
    {
        mIncompleteDirectoryEnabled = enabled;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(incompleteDirectoryEnabledKey, mIncompleteDirectoryEnabled);
        }
    }

    const QString& ServerSettings::incompleteDirectory() const
    {
        return mIncompleteDirectory;
    }

    void ServerSettings::setIncompleteDirectory(const QString& directory)
    {
        if (directory != mIncompleteDirectory) {
            mIncompleteDirectory = directory;
            if (mSaveOnSet) {
                mRpc->setSessionProperty(incompleteDirectoryKey, mIncompleteDirectory);
            }
        }
    }

    bool ServerSettings::isRatioLimited() const
    {
        return mRatioLimited;
    }

    void ServerSettings::setRatioLimited(bool limited)
    {
        mRatioLimited = limited;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(ratioLimitedKey, mRatioLimited);
        }
    }

    double ServerSettings::ratioLimit() const
    {
        return mRatioLimit;
    }

    void ServerSettings::setRatioLimit(double limit)
    {
        mRatioLimit = limit;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(ratioLimitKey, mRatioLimit);
        }
    }

    bool ServerSettings::isIdleSeedingLimited() const
    {
        return mIdleSeedingLimited;
    }

    void ServerSettings::setIdleSeedingLimited(bool limited)
    {
        mIdleSeedingLimited = limited;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(idleSeedingLimitedKey, mIdleSeedingLimited);
        }
    }

    int ServerSettings::idleSeedingLimit() const
    {
        return mIdleSeedingLimit;
    }

    void ServerSettings::setIdleSeedingLimit(int limit)
    {
        mIdleSeedingLimit = limit;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(idleSeedingLimitKey, mIdleSeedingLimit);
        }
    }

    bool ServerSettings::isDownloadQueueEnabled() const
    {
        return mDownloadQueueEnabled;
    }

    void ServerSettings::setDownloadQueueEnabled(bool enabled)
    {
        mDownloadQueueEnabled = enabled;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(downloadQueueEnabledKey, mDownloadQueueEnabled);
        }
    }

    int ServerSettings::downloadQueueSize() const
    {
        return mDownloadQueueSize;
    }

    void ServerSettings::setDownloadQueueSize(int size)
    {
        mDownloadQueueSize = size;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(downloadQueueSizeKey, mDownloadQueueSize);
        }
    }

    bool ServerSettings::isSeedQueueEnabled() const
    {
        return mSeedQueueEnabled;
    }

    void ServerSettings::setSeedQueueEnabled(bool enabled)
    {
        mSeedQueueEnabled = enabled;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(seedQueueEnabledKey, mSeedQueueEnabled);
        }
    }

    int ServerSettings::seedQueueSize() const
    {
        return mSeedQueueSize;
    }

    void ServerSettings::setSeedQueueSize(int size)
    {
        mSeedQueueSize = size;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(seedQueueSizeKey, mSeedQueueSize);
        }
    }

    bool ServerSettings::isIdleQueueLimited() const
    {
        return mIdleQueueLimited;
    }

    void ServerSettings::setIdleQueueLimited(bool limited)
    {
        mIdleQueueLimited = limited;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(idleQueueLimitedKey, mIdleQueueLimited);
        }
    }

    int ServerSettings::idleQueueLimit() const
    {
        return mIdleQueueLimit;
    }

    void ServerSettings::setIdleQueueLimit(int limit)
    {
        mIdleQueueLimit = limit;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(idleQueueLimitKey, mIdleQueueLimit);
        }
    }

    bool ServerSettings::isDownloadSpeedLimited() const
    {
        return mDownloadSpeedLimited;
    }

    void ServerSettings::setDownloadSpeedLimited(bool limited)
    {
        mDownloadSpeedLimited = limited;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(downloadSpeedLimitedKey, mDownloadSpeedLimited);
        }
    }

    int ServerSettings::downloadSpeedLimit() const
    {
        return mDownloadSpeedLimit;
    }

    void ServerSettings::setDownloadSpeedLimit(int limit)
    {
        mDownloadSpeedLimit = limit;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(downloadSpeedLimitKey, mDownloadSpeedLimit);
        }
    }

    bool ServerSettings::isUploadSpeedLimited() const
    {
        return mUploadSpeedLimited;
    }

    void ServerSettings::setUploadSpeedLimited(bool limited)
    {
        mUploadSpeedLimited = limited;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(uploadSpeedLimitedKey, mUploadSpeedLimited);
        }
    }

    int ServerSettings::uploadSpeedLimit() const
    {
        return mUploadSpeedLimit;
    }

    void ServerSettings::setUploadSpeedLimit(int limit)
    {
        mUploadSpeedLimit = limit;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(uploadSpeedLimitKey, mUploadSpeedLimit);
        }
    }

    bool ServerSettings::isAlternativeSpeedLimitsEnabled() const
    {
        return mAlternativeSpeedLimitsEnabled;
    }

    void ServerSettings::setAlternativeSpeedLimitsEnabled(bool enabled)
    {
        mAlternativeSpeedLimitsEnabled = enabled;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(alternativeSpeedLimitsEnabledKey, mAlternativeSpeedLimitsEnabled);
        }
    }

    int ServerSettings::alternativeDownloadSpeedLimit() const
    {
        return mAlternativeDownloadSpeedLimit;
    }

    void ServerSettings::setAlternativeDownloadSpeedLimit(int limit)
    {
        mAlternativeDownloadSpeedLimit = limit;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(alternativeDownloadSpeedLimitKey, mAlternativeDownloadSpeedLimit);
        }
    }

    int ServerSettings::alternativeUploadSpeedLimit() const
    {
        return mAlternativeUploadSpeedLimit;
    }

    void ServerSettings::setAlternativeUploadSpeedLimit(int limit)
    {
        mAlternativeUploadSpeedLimit = limit;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(alternativeUploadSpeedLimitKey, mAlternativeUploadSpeedLimit);
        }
    }

    bool ServerSettings::isAlternativeSpeedLimitsScheduled() const
    {
        return mAlternativeSpeedLimitsScheduled;
    }

    void ServerSettings::setAlternativeSpeedLimitsScheduled(bool scheduled)
    {
        mAlternativeSpeedLimitsScheduled = scheduled;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(alternativeSpeedLimitsScheduledKey, mAlternativeSpeedLimitsScheduled);
        }
    }

    QTime ServerSettings::alternativeSpeedLimitsBeginTime() const
    {
        return mAlternativeSpeedLimitsBeginTime;
    }

    void ServerSettings::setAlternativeSpeedLimitsBeginTime(QTime time)
    {
        mAlternativeSpeedLimitsBeginTime = time;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(alternativeSpeedLimitsBeginTimeKey, mAlternativeSpeedLimitsBeginTime.msecsSinceStartOfDay() / 60000);
        }
    }

    QTime ServerSettings::alternativeSpeedLimitsEndTime() const
    {
        return mAlternativeSpeedLimitsEndTime;
    }

    void ServerSettings::setAlternativeSpeedLimitsEndTime(QTime time)
    {
        mAlternativeSpeedLimitsEndTime = time;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(alternativeSpeedLimitsEndTimeKey, mAlternativeSpeedLimitsEndTime.msecsSinceStartOfDay() / 60000);
        }
    }

    ServerSettings::AlternativeSpeedLimitsDays ServerSettings::alternativeSpeedLimitsDays() const
    {
        return mAlternativeSpeedLimitsDays;
    }

    void ServerSettings::setAlternativeSpeedLimitsDays(ServerSettings::AlternativeSpeedLimitsDays days)
    {
        if (days != mAlternativeSpeedLimitsDays) {
            mAlternativeSpeedLimitsDays = days;
            if (mSaveOnSet) {
                mRpc->setSessionProperty(alternativeSpeedLimitsDaysKey, mAlternativeSpeedLimitsDays);
            }
        }
    }

    int ServerSettings::peerPort() const
    {
        return mPeerPort;
    }

    void ServerSettings::setPeerPort(int port)
    {
        mPeerPort = port;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(peerPortKey, mPeerPort);
        }
    }

    bool ServerSettings::isRandomPortEnabled() const
    {
        return mRandomPortEnabled;
    }

    void ServerSettings::setRandomPortEnabled(bool enabled)
    {
        mRandomPortEnabled = enabled;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(randomPortEnabledKey, mRandomPortEnabled);
        }
    }

    bool ServerSettings::isPortForwardingEnabled() const
    {
        return mPortForwardingEnabled;
    }

    void ServerSettings::setPortForwardingEnabled(bool enabled)
    {
        mPortForwardingEnabled = enabled;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(portForwardingEnabledKey, mPortForwardingEnabled);
        }
    }

    ServerSettings::EncryptionMode ServerSettings::encryptionMode() const
    {
        return mEncryptionMode;
    }

    void ServerSettings::setEncryptionMode(ServerSettings::EncryptionMode mode)
    {
        mEncryptionMode = mode;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(encryptionModeKey, encryptionModeString(mode));
        }
    }

    bool ServerSettings::isUtpEnabled() const
    {
        return mUtpEnabled;
    }

    void ServerSettings::setUtpEnabled(bool enabled)
    {
        mUtpEnabled = enabled;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(utpEnabledKey, mUtpEnabled);
        }
    }

    bool ServerSettings::isPexEnabled() const
    {
        return mPexEnabled;
    }

    void ServerSettings::setPexEnabled(bool enabled)
    {
        mPexEnabled = enabled;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(pexEnabledKey, mPexEnabled);
        }
    }

    bool ServerSettings::isDhtEnabled() const
    {
        return mDhtEnabled;
    }

    void ServerSettings::setDhtEnabled(bool enabled)
    {
        mDhtEnabled = enabled;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(dhtEnabledKey, mDhtEnabled);
        }
    }

    bool ServerSettings::isLpdEnabled() const
    {
        return mLpdEnabled;
    }

    void ServerSettings::setLpdEnabled(bool enabled)
    {
        mLpdEnabled = enabled;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(lpdEnabledKey, mLpdEnabled);
        }
    }

    int ServerSettings::maximumPeersPerTorrent() const
    {
        return mMaximumPeersPerTorrent;
    }

    void ServerSettings::setMaximumPeersPerTorrent(int peers)
    {
        mMaximumPeersPerTorrent = peers;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(maximumPeersPerTorrentKey, mMaximumPeersPerTorrent);
        }
    }

    int ServerSettings::maximumPeersGlobally() const
    {
        return mMaximumPeersGlobally;
    }

    bool ServerSettings::saveOnSet() const
    {
        return mSaveOnSet;
    }

    void ServerSettings::setSaveOnSet(bool save)
    {
        mSaveOnSet = save;
    }

    void ServerSettings::setMaximumPeersGlobally(int peers)
    {
        mMaximumPeersGlobally = peers;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(maximumPeersGloballyKey, mMaximumPeersGlobally);
        }
    }

    int ServerSettings::toKibiBytes(int kiloBytesOrKibiBytes) const
    {
        if (mUsingDecimalUnits) {
            return kiloBytesOrKibiBytes * 1000 / 1024;
        }
        return kiloBytesOrKibiBytes;
    }

    int ServerSettings::fromKibiBytes(int kibiBytes) const
    {
        if (mUsingDecimalUnits) {
            return kibiBytes * 1024 / 1000;
        }
        return kibiBytes;
    }

    void ServerSettings::update(const QJsonObject& serverSettings)
    {
        mRpcVersion = serverSettings.value(QJsonKeyStringInit("rpc-version")).toInt();
        mMinimumRpcVersion = serverSettings.value(QJsonKeyStringInit("rpc-version-minimum")).toInt();

        mUsingDecimalUnits = (serverSettings
                                  .value(QJsonKeyStringInit("units"))
                                  .toObject()
                                  .value(QJsonKeyStringInit("speed-bytes"))
                                  .toInt() == 1000);

        mDownloadDirectory = serverSettings.value(downloadDirectoryKey).toString();
        mTrashTorrentFiles = serverSettings.value(trashTorrentFilesKey).toBool();
        mStartAddedTorrents = serverSettings.value(startAddedTorrentsKey).toBool();
        mRenameIncompleteFiles = serverSettings.value(renameIncompleteFilesKey).toBool();
        mIncompleteDirectoryEnabled = serverSettings.value(incompleteDirectoryEnabledKey).toBool();
        mIncompleteDirectory = serverSettings.value(incompleteDirectoryKey).toString();

        mRatioLimited = serverSettings.value(ratioLimitedKey).toBool();
        mRatioLimit = serverSettings.value(ratioLimitKey).toDouble();
        mIdleSeedingLimited = serverSettings.value(idleSeedingLimitedKey).toBool();
        mIdleSeedingLimit = serverSettings.value(idleSeedingLimitKey).toInt();

        mDownloadQueueEnabled = serverSettings.value(downloadQueueEnabledKey).toBool();
        mDownloadQueueSize = serverSettings.value(downloadQueueSizeKey).toInt();
        mSeedQueueEnabled = serverSettings.value(seedQueueEnabledKey).toBool();
        mSeedQueueSize = serverSettings.value(seedQueueSizeKey).toInt();
        mIdleQueueLimited = serverSettings.value(idleQueueLimitedKey).toBool();
        mIdleQueueLimit = serverSettings.value(idleQueueLimitKey).toInt();

        mDownloadSpeedLimited = serverSettings.value(downloadSpeedLimitedKey).toBool();
        mDownloadSpeedLimit = toKibiBytes(serverSettings.value(downloadSpeedLimitKey).toInt());
        mUploadSpeedLimited = serverSettings.value(uploadSpeedLimitedKey).toBool();
        mUploadSpeedLimit = toKibiBytes(serverSettings.value(uploadSpeedLimitKey).toInt());
        mAlternativeSpeedLimitsEnabled = serverSettings.value(alternativeSpeedLimitsEnabledKey).toBool();
        mAlternativeDownloadSpeedLimit = toKibiBytes(serverSettings.value(alternativeDownloadSpeedLimitKey).toInt());
        mAlternativeUploadSpeedLimit = toKibiBytes(serverSettings.value(alternativeUploadSpeedLimitKey).toInt());
        mAlternativeSpeedLimitsScheduled = serverSettings.value(alternativeSpeedLimitsScheduledKey).toBool();
        mAlternativeSpeedLimitsBeginTime = QTime::fromMSecsSinceStartOfDay(serverSettings.value(alternativeSpeedLimitsBeginTimeKey).toInt() * 60000);
        mAlternativeSpeedLimitsEndTime = QTime::fromMSecsSinceStartOfDay(serverSettings.value(alternativeSpeedLimitsEndTimeKey).toInt() * 60000);
        switch (int days = serverSettings.value(alternativeSpeedLimitsDaysKey).toInt()) {
        case Sunday:
        case Monday:
        case Tuesday:
        case Wednesday:
        case Thursday:
        case Friday:
        case Saturday:
        case Weekdays:
        case Weekends:
        case All:
            mAlternativeSpeedLimitsDays = static_cast<AlternativeSpeedLimitsDays>(days);
            break;
        default:
            mAlternativeSpeedLimitsDays = All;
        }

        mPeerPort = serverSettings.value(peerPortKey).toInt();
        mRandomPortEnabled = serverSettings.value(randomPortEnabledKey).toBool();
        mPortForwardingEnabled = serverSettings.value(portForwardingEnabledKey).toBool();

        const QString encryption(serverSettings.value(encryptionModeKey).toString());
        if (encryption == encryptionModeAllowed) {
            mEncryptionMode = AllowedEncryption;
        } else if (encryption == encryptionModePreferred) {
            mEncryptionMode = PreferredEncryption;
        } else {
            mEncryptionMode = RequiredEncryption;
        }

        mUtpEnabled = serverSettings.value(utpEnabledKey).toBool();
        mPexEnabled = serverSettings.value(pexEnabledKey).toBool();
        mDhtEnabled = serverSettings.value(dhtEnabledKey).toBool();
        mLpdEnabled = serverSettings.value(lpdEnabledKey).toBool();
        mMaximumPeersPerTorrent = serverSettings.value(maximumPeersPerTorrentKey).toInt();
        mMaximumPeersGlobally = serverSettings.value(maximumPeersGloballyKey).toInt();
    }

    void ServerSettings::save() const
    {
        mRpc->setSessionProperties({{downloadDirectoryKey, mDownloadDirectory},
                                    {trashTorrentFilesKey, mTrashTorrentFiles},
                                    {startAddedTorrentsKey, mStartAddedTorrents},
                                    {renameIncompleteFilesKey, mRenameIncompleteFiles},
                                    {incompleteDirectoryEnabledKey, mIncompleteDirectoryEnabled},
                                    {incompleteDirectoryKey, mIncompleteDirectory},

                                    {ratioLimitedKey, mRatioLimited},
                                    {ratioLimitKey, mRatioLimit},
                                    {idleSeedingLimitedKey, mIdleSeedingLimit},
                                    {idleSeedingLimitKey, mIdleSeedingLimit},

                                    {downloadQueueEnabledKey, mDownloadQueueEnabled},
                                    {downloadQueueSizeKey, mDownloadQueueSize},
                                    {seedQueueEnabledKey, mSeedQueueEnabled},
                                    {seedQueueSizeKey, mSeedQueueSize},
                                    {idleQueueLimitedKey, mIdleQueueLimited},
                                    {idleQueueLimitKey, mIdleQueueLimit},

                                    {downloadSpeedLimitedKey, mDownloadSpeedLimited},
                                    {downloadSpeedLimitKey, mDownloadSpeedLimit},
                                    {uploadSpeedLimitedKey, mUploadSpeedLimited},
                                    {uploadSpeedLimitKey, mUploadSpeedLimit},
                                    {alternativeSpeedLimitsEnabledKey, mAlternativeSpeedLimitsEnabled},
                                    {alternativeDownloadSpeedLimitKey, mAlternativeDownloadSpeedLimit},
                                    {alternativeUploadSpeedLimitKey, mAlternativeUploadSpeedLimit},
                                    {alternativeSpeedLimitsScheduledKey, mAlternativeSpeedLimitsScheduled},
                                    {alternativeSpeedLimitsBeginTimeKey, mAlternativeSpeedLimitsBeginTime.msecsSinceStartOfDay() / 60000},
                                    {alternativeSpeedLimitsEndTimeKey, mAlternativeSpeedLimitsEndTime.msecsSinceStartOfDay() / 60000},
                                    {alternativeSpeedLimitsDaysKey, mAlternativeSpeedLimitsDays},

                                    {peerPortKey, mPeerPort},
                                    {randomPortEnabledKey, mRandomPortEnabled},
                                    {portForwardingEnabledKey, mPortForwardingEnabled},
                                    {encryptionModeKey, encryptionModeString(mEncryptionMode)},
                                    {utpEnabledKey, mUtpEnabled},
                                    {pexEnabledKey, mPexEnabled},
                                    {dhtEnabledKey, mDhtEnabled},
                                    {lpdEnabledKey, mLpdEnabled},
                                    {maximumPeersPerTorrentKey, mMaximumPeersPerTorrent},
                                    {maximumPeersGloballyKey, mMaximumPeersGlobally}});
    }
}
