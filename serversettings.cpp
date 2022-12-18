// SPDX-FileCopyrightText: 2015-2022 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "serversettings.h"

#include <QJsonObject>

#include "literals.h"
#include "pathutils.h"
#include "rpc.h"
#include "stdutils.h"

namespace libtremotesf {
    namespace {
        constexpr auto downloadDirectoryKey = "download-dir"_l1;
        constexpr auto trashTorrentFilesKey = "trash-original-torrent-files"_l1;
        constexpr auto startAddedTorrentsKey = "start-added-torrents"_l1;
        constexpr auto renameIncompleteFilesKey = "rename-partial-files"_l1;
        constexpr auto incompleteDirectoryEnabledKey = "incomplete-dir-enabled"_l1;
        constexpr auto incompleteDirectoryKey = "incomplete-dir"_l1;

        constexpr auto ratioLimitedKey = "seedRatioLimited"_l1;
        constexpr auto ratioLimitKey = "seedRatioLimit"_l1;
        constexpr auto idleSeedingLimitedKey = "idle-seeding-limit-enabled"_l1;
        constexpr auto idleSeedingLimitKey = "idle-seeding-limit"_l1;

        constexpr auto downloadQueueEnabledKey = "download-queue-enabled"_l1;
        constexpr auto downloadQueueSizeKey = "download-queue-size"_l1;
        constexpr auto seedQueueEnabledKey = "seed-queue-enabled"_l1;
        constexpr auto seedQueueSizeKey = "seed-queue-size"_l1;
        constexpr auto idleQueueLimitedKey = "queue-stalled-enabled"_l1;
        constexpr auto idleQueueLimitKey = "queue-stalled-minutes"_l1;

        constexpr auto downloadSpeedLimitedKey = "speed-limit-down-enabled"_l1;
        constexpr auto downloadSpeedLimitKey = "speed-limit-down"_l1;
        constexpr auto uploadSpeedLimitedKey = "speed-limit-up-enabled"_l1;
        constexpr auto uploadSpeedLimitKey = "speed-limit-up"_l1;
        constexpr auto alternativeSpeedLimitsEnabledKey = "alt-speed-enabled"_l1;
        constexpr auto alternativeDownloadSpeedLimitKey = "alt-speed-down"_l1;
        constexpr auto alternativeUploadSpeedLimitKey = "alt-speed-up"_l1;
        constexpr auto alternativeSpeedLimitsScheduledKey = "alt-speed-time-enabled"_l1;
        constexpr auto alternativeSpeedLimitsBeginTimeKey = "alt-speed-time-begin"_l1;
        constexpr auto alternativeSpeedLimitsEndTimeKey = "alt-speed-time-end"_l1;
        constexpr auto alternativeSpeedLimitsDaysKey = "alt-speed-time-day"_l1;

        inline ServerSettingsData::AlternativeSpeedLimitsDays daysFromInt(int days) {
            switch (auto d = static_cast<ServerSettingsData::AlternativeSpeedLimitsDays>(days)) {
            case ServerSettingsData::AlternativeSpeedLimitsDays::Sunday:
            case ServerSettingsData::AlternativeSpeedLimitsDays::Monday:
            case ServerSettingsData::AlternativeSpeedLimitsDays::Tuesday:
            case ServerSettingsData::AlternativeSpeedLimitsDays::Wednesday:
            case ServerSettingsData::AlternativeSpeedLimitsDays::Thursday:
            case ServerSettingsData::AlternativeSpeedLimitsDays::Friday:
            case ServerSettingsData::AlternativeSpeedLimitsDays::Saturday:
            case ServerSettingsData::AlternativeSpeedLimitsDays::Weekdays:
            case ServerSettingsData::AlternativeSpeedLimitsDays::Weekends:
            case ServerSettingsData::AlternativeSpeedLimitsDays::All:
                return d;
            default:
                return ServerSettingsData::AlternativeSpeedLimitsDays::All;
            }
        }

        constexpr auto peerPortKey = "peer-port"_l1;
        constexpr auto randomPortEnabledKey = "peer-port-random-on-start"_l1;
        constexpr auto portForwardingEnabledKey = "port-forwarding-enabled"_l1;

        constexpr auto encryptionModeKey = "encryption"_l1;
        constexpr auto encryptionModeAllowed = "tolerated"_l1;
        constexpr auto encryptionModePreferred = "preferred"_l1;
        constexpr auto encryptionModeRequired = "required"_l1;

        inline QString encryptionModeString(ServerSettingsData::EncryptionMode mode) {
            switch (mode) {
            case ServerSettingsData::EncryptionMode::Allowed:
                return encryptionModeAllowed;
            case ServerSettingsData::EncryptionMode::Preferred:
                return encryptionModePreferred;
            case ServerSettingsData::EncryptionMode::Required:
                return encryptionModeRequired;
            default:
                return {};
            }
        }

        constexpr auto utpEnabledKey = "utp-enabled"_l1;
        constexpr auto pexEnabledKey = "pex-enabled"_l1;
        constexpr auto dhtEnabledKey = "dht-enabled"_l1;
        constexpr auto lpdEnabledKey = "lpd-enabled"_l1;
        constexpr auto maximumPeersPerTorrentKey = "peer-limit-per-torrent"_l1;
        constexpr auto maximumPeersGloballyKey = "peer-limit-global"_l1;
    }

    bool ServerSettingsData::canRenameFiles() const { return (rpcVersion >= 15); }

    bool ServerSettingsData::canShowFreeSpaceForPath() const { return (rpcVersion >= 15); }

    bool ServerSettingsData::hasSessionIdFile() const { return (rpcVersion >= 16); }

    ServerSettings::ServerSettings(Rpc* rpc, QObject* parent) : QObject(parent), mRpc(rpc), mSaveOnSet(true) {}

    int ServerSettings::rpcVersion() const { return mData.rpcVersion; }

    int ServerSettings::minimumRpcVersion() const { return mData.minimumRpcVersion; }

    bool ServerSettings::canRenameFiles() const { return mData.canRenameFiles(); }

    bool ServerSettings::canShowFreeSpaceForPath() const { return mData.canShowFreeSpaceForPath(); }

    bool ServerSettings::hasSessionIdFile() const { return mData.hasSessionIdFile(); }

    const QString& ServerSettings::downloadDirectory() const { return mData.downloadDirectory; }

    void ServerSettings::setDownloadDirectory(const QString& directory) {
        if (directory != mData.downloadDirectory) {
            mData.downloadDirectory = directory;
            if (mSaveOnSet) {
                mRpc->setSessionProperty(downloadDirectoryKey, mData.downloadDirectory);
            }
        }
    }

    bool ServerSettings::startAddedTorrents() const { return mData.startAddedTorrents; }

    void ServerSettings::setStartAddedTorrents(bool start) {
        mData.startAddedTorrents = start;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(startAddedTorrentsKey, mData.startAddedTorrents);
        }
    }

    bool ServerSettings::trashTorrentFiles() const { return mData.trashTorrentFiles; }

    void ServerSettings::setTrashTorrentFiles(bool trash) {
        mData.trashTorrentFiles = trash;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(trashTorrentFilesKey, mData.trashTorrentFiles);
        }
    }

    bool ServerSettings::renameIncompleteFiles() const { return mData.renameIncompleteFiles; }

    void ServerSettings::setRenameIncompleteFiles(bool rename) {
        mData.renameIncompleteFiles = rename;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(renameIncompleteFilesKey, mData.renameIncompleteFiles);
        }
    }

    bool ServerSettings::isIncompleteDirectoryEnabled() const { return mData.incompleteDirectoryEnabled; }

    void ServerSettings::setIncompleteDirectoryEnabled(bool enabled) {
        mData.incompleteDirectoryEnabled = enabled;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(incompleteDirectoryEnabledKey, mData.incompleteDirectoryEnabled);
        }
    }

    const QString& ServerSettings::incompleteDirectory() const { return mData.incompleteDirectory; }

    void ServerSettings::setIncompleteDirectory(const QString& directory) {
        if (directory != mData.incompleteDirectory) {
            mData.incompleteDirectory = directory;
            if (mSaveOnSet) {
                mRpc->setSessionProperty(incompleteDirectoryKey, mData.incompleteDirectory);
            }
        }
    }

    bool ServerSettings::isRatioLimited() const { return mData.ratioLimited; }

    void ServerSettings::setRatioLimited(bool limited) {
        mData.ratioLimited = limited;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(ratioLimitedKey, mData.ratioLimited);
        }
    }

    double ServerSettings::ratioLimit() const { return mData.ratioLimit; }

    void ServerSettings::setRatioLimit(double limit) {
        mData.ratioLimit = limit;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(ratioLimitKey, mData.ratioLimit);
        }
    }

    bool ServerSettings::isIdleSeedingLimited() const { return mData.idleSeedingLimited; }

    void ServerSettings::setIdleSeedingLimited(bool limited) {
        mData.idleSeedingLimited = limited;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(idleSeedingLimitedKey, mData.idleSeedingLimited);
        }
    }

    int ServerSettings::idleSeedingLimit() const { return mData.idleSeedingLimit; }

    void ServerSettings::setIdleSeedingLimit(int limit) {
        mData.idleSeedingLimit = limit;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(idleSeedingLimitKey, mData.idleSeedingLimit);
        }
    }

    bool ServerSettings::isDownloadQueueEnabled() const { return mData.downloadQueueEnabled; }

    void ServerSettings::setDownloadQueueEnabled(bool enabled) {
        mData.downloadQueueEnabled = enabled;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(downloadQueueEnabledKey, mData.downloadQueueEnabled);
        }
    }

    int ServerSettings::downloadQueueSize() const { return mData.downloadQueueSize; }

    void ServerSettings::setDownloadQueueSize(int size) {
        mData.downloadQueueSize = size;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(downloadQueueSizeKey, mData.downloadQueueSize);
        }
    }

    bool ServerSettings::isSeedQueueEnabled() const { return mData.seedQueueEnabled; }

    void ServerSettings::setSeedQueueEnabled(bool enabled) {
        mData.seedQueueEnabled = enabled;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(seedQueueEnabledKey, mData.seedQueueEnabled);
        }
    }

    int ServerSettings::seedQueueSize() const { return mData.seedQueueSize; }

    void ServerSettings::setSeedQueueSize(int size) {
        mData.seedQueueSize = size;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(seedQueueSizeKey, mData.seedQueueSize);
        }
    }

    bool ServerSettings::isIdleQueueLimited() const { return mData.idleQueueLimited; }

    void ServerSettings::setIdleQueueLimited(bool limited) {
        mData.idleQueueLimited = limited;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(idleQueueLimitedKey, mData.idleQueueLimited);
        }
    }

    int ServerSettings::idleQueueLimit() const { return mData.idleQueueLimit; }

    void ServerSettings::setIdleQueueLimit(int limit) {
        mData.idleQueueLimit = limit;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(idleQueueLimitKey, mData.idleQueueLimit);
        }
    }

    bool ServerSettings::isDownloadSpeedLimited() const { return mData.downloadSpeedLimited; }

    void ServerSettings::setDownloadSpeedLimited(bool limited) {
        mData.downloadSpeedLimited = limited;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(downloadSpeedLimitedKey, mData.downloadSpeedLimited);
        }
    }

    int ServerSettings::downloadSpeedLimit() const { return mData.downloadSpeedLimit; }

    void ServerSettings::setDownloadSpeedLimit(int limit) {
        mData.downloadSpeedLimit = limit;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(downloadSpeedLimitKey, mData.downloadSpeedLimit);
        }
    }

    bool ServerSettings::isUploadSpeedLimited() const { return mData.uploadSpeedLimited; }

    void ServerSettings::setUploadSpeedLimited(bool limited) {
        mData.uploadSpeedLimited = limited;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(uploadSpeedLimitedKey, mData.uploadSpeedLimited);
        }
    }

    int ServerSettings::uploadSpeedLimit() const { return mData.uploadSpeedLimit; }

    void ServerSettings::setUploadSpeedLimit(int limit) {
        mData.uploadSpeedLimit = limit;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(uploadSpeedLimitKey, mData.uploadSpeedLimit);
        }
    }

    bool ServerSettings::isAlternativeSpeedLimitsEnabled() const { return mData.alternativeSpeedLimitsEnabled; }

    void ServerSettings::setAlternativeSpeedLimitsEnabled(bool enabled) {
        mData.alternativeSpeedLimitsEnabled = enabled;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(alternativeSpeedLimitsEnabledKey, mData.alternativeSpeedLimitsEnabled);
        }
    }

    int ServerSettings::alternativeDownloadSpeedLimit() const { return mData.alternativeDownloadSpeedLimit; }

    void ServerSettings::setAlternativeDownloadSpeedLimit(int limit) {
        mData.alternativeDownloadSpeedLimit = limit;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(alternativeDownloadSpeedLimitKey, mData.alternativeDownloadSpeedLimit);
        }
    }

    int ServerSettings::alternativeUploadSpeedLimit() const { return mData.alternativeUploadSpeedLimit; }

    void ServerSettings::setAlternativeUploadSpeedLimit(int limit) {
        mData.alternativeUploadSpeedLimit = limit;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(alternativeUploadSpeedLimitKey, mData.alternativeUploadSpeedLimit);
        }
    }

    bool ServerSettings::isAlternativeSpeedLimitsScheduled() const { return mData.alternativeSpeedLimitsScheduled; }

    void ServerSettings::setAlternativeSpeedLimitsScheduled(bool scheduled) {
        mData.alternativeSpeedLimitsScheduled = scheduled;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(alternativeSpeedLimitsScheduledKey, mData.alternativeSpeedLimitsScheduled);
        }
    }

    QTime ServerSettings::alternativeSpeedLimitsBeginTime() const { return mData.alternativeSpeedLimitsBeginTime; }

    void ServerSettings::setAlternativeSpeedLimitsBeginTime(QTime time) {
        mData.alternativeSpeedLimitsBeginTime = time;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(
                alternativeSpeedLimitsBeginTimeKey,
                mData.alternativeSpeedLimitsBeginTime.msecsSinceStartOfDay() / 60000
            );
        }
    }

    QTime ServerSettings::alternativeSpeedLimitsEndTime() const { return mData.alternativeSpeedLimitsEndTime; }

    void ServerSettings::setAlternativeSpeedLimitsEndTime(QTime time) {
        mData.alternativeSpeedLimitsEndTime = time;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(
                alternativeSpeedLimitsEndTimeKey,
                mData.alternativeSpeedLimitsEndTime.msecsSinceStartOfDay() / 60000
            );
        }
    }

    ServerSettingsData::AlternativeSpeedLimitsDays ServerSettings::alternativeSpeedLimitsDays() const {
        return mData.alternativeSpeedLimitsDays;
    }

    void ServerSettings::setAlternativeSpeedLimitsDays(ServerSettingsData::AlternativeSpeedLimitsDays days) {
        if (days != mData.alternativeSpeedLimitsDays) {
            mData.alternativeSpeedLimitsDays = days;
            if (mSaveOnSet) {
                mRpc->setSessionProperty(
                    alternativeSpeedLimitsDaysKey,
                    static_cast<int>(mData.alternativeSpeedLimitsDays)
                );
            }
        }
    }

    int ServerSettings::peerPort() const { return mData.peerPort; }

    void ServerSettings::setPeerPort(int port) {
        mData.peerPort = port;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(peerPortKey, mData.peerPort);
        }
    }

    bool ServerSettings::isRandomPortEnabled() const { return mData.randomPortEnabled; }

    void ServerSettings::setRandomPortEnabled(bool enabled) {
        mData.randomPortEnabled = enabled;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(randomPortEnabledKey, mData.randomPortEnabled);
        }
    }

    bool ServerSettings::isPortForwardingEnabled() const { return mData.portForwardingEnabled; }

    void ServerSettings::setPortForwardingEnabled(bool enabled) {
        mData.portForwardingEnabled = enabled;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(portForwardingEnabledKey, mData.portForwardingEnabled);
        }
    }

    ServerSettingsData::EncryptionMode ServerSettings::encryptionMode() const { return mData.encryptionMode; }

    void ServerSettings::setEncryptionMode(ServerSettingsData::EncryptionMode mode) {
        mData.encryptionMode = mode;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(encryptionModeKey, encryptionModeString(mode));
        }
    }

    bool ServerSettings::isUtpEnabled() const { return mData.utpEnabled; }

    void ServerSettings::setUtpEnabled(bool enabled) {
        mData.utpEnabled = enabled;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(utpEnabledKey, mData.utpEnabled);
        }
    }

    bool ServerSettings::isPexEnabled() const { return mData.pexEnabled; }

    void ServerSettings::setPexEnabled(bool enabled) {
        mData.pexEnabled = enabled;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(pexEnabledKey, mData.pexEnabled);
        }
    }

    bool ServerSettings::isDhtEnabled() const { return mData.dhtEnabled; }

    void ServerSettings::setDhtEnabled(bool enabled) {
        mData.dhtEnabled = enabled;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(dhtEnabledKey, mData.dhtEnabled);
        }
    }

    bool ServerSettings::isLpdEnabled() const { return mData.lpdEnabled; }

    void ServerSettings::setLpdEnabled(bool enabled) {
        mData.lpdEnabled = enabled;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(lpdEnabledKey, mData.lpdEnabled);
        }
    }

    int ServerSettings::maximumPeersPerTorrent() const { return mData.maximumPeersPerTorrent; }

    void ServerSettings::setMaximumPeersPerTorrent(int peers) {
        mData.maximumPeersPerTorrent = peers;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(maximumPeersPerTorrentKey, mData.maximumPeersPerTorrent);
        }
    }

    int ServerSettings::maximumPeersGlobally() const { return mData.maximumPeersGlobally; }

    void ServerSettings::setMaximumPeersGlobally(int peers) {
        mData.maximumPeersGlobally = peers;
        if (mSaveOnSet) {
            mRpc->setSessionProperty(maximumPeersGloballyKey, mData.maximumPeersGlobally);
        }
    }

    bool ServerSettings::saveOnSet() const { return mSaveOnSet; }

    void ServerSettings::setSaveOnSet(bool save) { mSaveOnSet = save; }

    void ServerSettings::update(const QJsonObject& serverSettings) {
        bool changed = false;

        mData.rpcVersion = serverSettings.value("rpc-version"_l1).toInt();
        mData.minimumRpcVersion = serverSettings.value("rpc-version-minimum"_l1).toInt();

        setChanged(
            mData.downloadDirectory,
            normalizePath(serverSettings.value(downloadDirectoryKey).toString()),
            changed
        );
        setChanged(mData.trashTorrentFiles, serverSettings.value(trashTorrentFilesKey).toBool(), changed);
        setChanged(mData.startAddedTorrents, serverSettings.value(startAddedTorrentsKey).toBool(), changed);
        setChanged(mData.renameIncompleteFiles, serverSettings.value(renameIncompleteFilesKey).toBool(), changed);
        setChanged(
            mData.incompleteDirectoryEnabled,
            serverSettings.value(incompleteDirectoryEnabledKey).toBool(),
            changed
        );
        setChanged(
            mData.incompleteDirectory,
            normalizePath(serverSettings.value(incompleteDirectoryKey).toString()),
            changed
        );

        setChanged(mData.ratioLimited, serverSettings.value(ratioLimitedKey).toBool(), changed);
        setChanged(mData.ratioLimit, serverSettings.value(ratioLimitKey).toDouble(), changed);
        setChanged(mData.idleSeedingLimited, serverSettings.value(idleSeedingLimitedKey).toBool(), changed);
        setChanged(mData.idleSeedingLimit, serverSettings.value(idleSeedingLimitKey).toInt(), changed);

        setChanged(mData.downloadQueueEnabled, serverSettings.value(downloadQueueEnabledKey).toBool(), changed);
        setChanged(mData.downloadQueueSize, serverSettings.value(downloadQueueSizeKey).toInt(), changed);
        setChanged(mData.seedQueueEnabled, serverSettings.value(seedQueueEnabledKey).toBool(), changed);
        setChanged(mData.seedQueueSize, serverSettings.value(seedQueueSizeKey).toInt(), changed);
        setChanged(mData.idleQueueLimited, serverSettings.value(idleQueueLimitedKey).toBool(), changed);
        setChanged(mData.idleQueueLimit, serverSettings.value(idleQueueLimitKey).toInt(), changed);

        setChanged(mData.downloadSpeedLimited, serverSettings.value(downloadSpeedLimitedKey).toBool(), changed);
        setChanged(mData.downloadSpeedLimit, serverSettings.value(downloadSpeedLimitKey).toInt(), changed);
        setChanged(mData.uploadSpeedLimited, serverSettings.value(uploadSpeedLimitedKey).toBool(), changed);
        setChanged(mData.uploadSpeedLimit, serverSettings.value(uploadSpeedLimitKey).toInt(), changed);
        setChanged(
            mData.alternativeSpeedLimitsEnabled,
            serverSettings.value(alternativeSpeedLimitsEnabledKey).toBool(),
            changed
        );
        setChanged(
            mData.alternativeDownloadSpeedLimit,
            serverSettings.value(alternativeDownloadSpeedLimitKey).toInt(),
            changed
        );
        setChanged(
            mData.alternativeUploadSpeedLimit,
            serverSettings.value(alternativeUploadSpeedLimitKey).toInt(),
            changed
        );
        setChanged(
            mData.alternativeSpeedLimitsScheduled,
            serverSettings.value(alternativeSpeedLimitsScheduledKey).toBool(),
            changed
        );
        setChanged(
            mData.alternativeSpeedLimitsBeginTime,
            QTime::fromMSecsSinceStartOfDay(serverSettings.value(alternativeSpeedLimitsBeginTimeKey).toInt() * 60000),
            changed
        );
        setChanged(
            mData.alternativeSpeedLimitsEndTime,
            QTime::fromMSecsSinceStartOfDay(serverSettings.value(alternativeSpeedLimitsEndTimeKey).toInt() * 60000),
            changed
        );
        setChanged(
            mData.alternativeSpeedLimitsDays,
            daysFromInt(serverSettings.value(alternativeSpeedLimitsDaysKey).toInt()),
            changed
        );

        setChanged(mData.peerPort, serverSettings.value(peerPortKey).toInt(), changed);
        setChanged(mData.randomPortEnabled, serverSettings.value(randomPortEnabledKey).toBool(), changed);
        setChanged(mData.portForwardingEnabled, serverSettings.value(portForwardingEnabledKey).toBool(), changed);

        const QString encryption(serverSettings.value(encryptionModeKey).toString());
        if (encryption == encryptionModeAllowed) {
            setChanged(mData.encryptionMode, ServerSettingsData::EncryptionMode::Allowed, changed);
        } else if (encryption == encryptionModePreferred) {
            setChanged(mData.encryptionMode, ServerSettingsData::EncryptionMode::Preferred, changed);
        } else {
            setChanged(mData.encryptionMode, ServerSettingsData::EncryptionMode::Required, changed);
        }

        setChanged(mData.utpEnabled, serverSettings.value(utpEnabledKey).toBool(), changed);
        setChanged(mData.pexEnabled, serverSettings.value(pexEnabledKey).toBool(), changed);
        setChanged(mData.dhtEnabled, serverSettings.value(dhtEnabledKey).toBool(), changed);
        setChanged(mData.lpdEnabled, serverSettings.value(lpdEnabledKey).toBool(), changed);
        setChanged(mData.maximumPeersPerTorrent, serverSettings.value(maximumPeersPerTorrentKey).toInt(), changed);
        setChanged(mData.maximumPeersGlobally, serverSettings.value(maximumPeersGloballyKey).toInt(), changed);

        if (changed) {
            emit this->changed();
        }
    }

    void ServerSettings::save() const {
        mRpc->setSessionProperties(
            {{downloadDirectoryKey, mData.downloadDirectory},
             {trashTorrentFilesKey, mData.trashTorrentFiles},
             {startAddedTorrentsKey, mData.startAddedTorrents},
             {renameIncompleteFilesKey, mData.renameIncompleteFiles},
             {incompleteDirectoryEnabledKey, mData.incompleteDirectoryEnabled},
             {incompleteDirectoryKey, mData.incompleteDirectory},

             {ratioLimitedKey, mData.ratioLimited},
             {ratioLimitKey, mData.ratioLimit},
             {idleSeedingLimitedKey, mData.idleSeedingLimit},
             {idleSeedingLimitKey, mData.idleSeedingLimit},

             {downloadQueueEnabledKey, mData.downloadQueueEnabled},
             {downloadQueueSizeKey, mData.downloadQueueSize},
             {seedQueueEnabledKey, mData.seedQueueEnabled},
             {seedQueueSizeKey, mData.seedQueueSize},
             {idleQueueLimitedKey, mData.idleQueueLimited},
             {idleQueueLimitKey, mData.idleQueueLimit},

             {downloadSpeedLimitedKey, mData.downloadSpeedLimited},
             {downloadSpeedLimitKey, mData.downloadSpeedLimit},
             {uploadSpeedLimitedKey, mData.uploadSpeedLimited},
             {uploadSpeedLimitKey, mData.uploadSpeedLimit},
             {alternativeSpeedLimitsEnabledKey, mData.alternativeSpeedLimitsEnabled},
             {alternativeDownloadSpeedLimitKey, mData.alternativeDownloadSpeedLimit},
             {alternativeUploadSpeedLimitKey, mData.alternativeUploadSpeedLimit},
             {alternativeSpeedLimitsScheduledKey, mData.alternativeSpeedLimitsScheduled},
             {alternativeSpeedLimitsBeginTimeKey, mData.alternativeSpeedLimitsBeginTime.msecsSinceStartOfDay() / 60000},
             {alternativeSpeedLimitsEndTimeKey, mData.alternativeSpeedLimitsEndTime.msecsSinceStartOfDay() / 60000},
             {alternativeSpeedLimitsDaysKey, static_cast<int>(mData.alternativeSpeedLimitsDays)},

             {peerPortKey, mData.peerPort},
             {randomPortEnabledKey, mData.randomPortEnabled},
             {portForwardingEnabledKey, mData.portForwardingEnabled},
             {encryptionModeKey, encryptionModeString(mData.encryptionMode)},
             {utpEnabledKey, mData.utpEnabled},
             {pexEnabledKey, mData.pexEnabled},
             {dhtEnabledKey, mData.dhtEnabled},
             {lpdEnabledKey, mData.lpdEnabled},
             {maximumPeersPerTorrentKey, mData.maximumPeersPerTorrent},
             {maximumPeersGloballyKey, mData.maximumPeersGlobally}}
        );
    }

    const ServerSettingsData& ServerSettings::data() const { return mData; }
}
