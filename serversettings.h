// SPDX-FileCopyrightText: 2015-2022 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LIBTREMOTESF_SERVERSETTINGS_H
#define LIBTREMOTESF_SERVERSETTINGS_H

#include <QObject>
#include <QTime>

#include "formatters.h"

class QJsonObject;

namespace libtremotesf {
    class Rpc;

    struct ServerSettingsData {
        Q_GADGET
    public:
        enum class AlternativeSpeedLimitsDays {
            Sunday,
            Monday,
            Tuesday,
            Wednesday,
            Thursday,
            Friday,
            Saturday,
            Weekdays,
            Weekends,
            All
        };
        Q_ENUM(AlternativeSpeedLimitsDays)

        enum class EncryptionMode { Allowed, Preferred, Required };
        Q_ENUM(EncryptionMode)

        bool canRenameFiles() const;
        bool canShowFreeSpaceForPath() const;
        bool hasSessionIdFile() const;
        bool hasTableMode() const;

        int rpcVersion = 0;
        int minimumRpcVersion = 0;

        QString downloadDirectory;
        bool startAddedTorrents = false;
        bool trashTorrentFiles = false;
        bool renameIncompleteFiles = false;
        bool incompleteDirectoryEnabled = false;
        QString incompleteDirectory;

        bool ratioLimited = false;
        double ratioLimit = 0.0;
        bool idleSeedingLimited = false;
        int idleSeedingLimit = 0;

        bool downloadQueueEnabled = false;
        int downloadQueueSize = 0;
        bool seedQueueEnabled = false;
        int seedQueueSize = 0;
        bool idleQueueLimited = false;
        int idleQueueLimit = 0;

        bool downloadSpeedLimited = false;
        int downloadSpeedLimit = 0;
        bool uploadSpeedLimited = false;
        int uploadSpeedLimit = 0;
        bool alternativeSpeedLimitsEnabled = false;
        int alternativeDownloadSpeedLimit = 0;
        int alternativeUploadSpeedLimit = 0;
        bool alternativeSpeedLimitsScheduled = false;
        QTime alternativeSpeedLimitsBeginTime;
        QTime alternativeSpeedLimitsEndTime;
        AlternativeSpeedLimitsDays alternativeSpeedLimitsDays{};

        int peerPort = 0;
        bool randomPortEnabled = false;
        bool portForwardingEnabled = false;
        EncryptionMode encryptionMode{};
        bool utpEnabled = false;
        bool pexEnabled = false;
        bool dhtEnabled = false;
        bool lpdEnabled = false;
        int maximumPeersPerTorrent = 0;
        int maximumPeersGlobally = 0;
    };

    class ServerSettings : public QObject {
        Q_OBJECT
    public:
        explicit ServerSettings(Rpc* rpc = nullptr, QObject* parent = nullptr);

        int rpcVersion() const;
        int minimumRpcVersion() const;

        bool canRenameFiles() const;
        bool canShowFreeSpaceForPath() const;
        bool hasSessionIdFile() const;
        bool hasTableMode() const;

        const QString& downloadDirectory() const;
        void setDownloadDirectory(const QString& directory);
        bool startAddedTorrents() const;
        void setStartAddedTorrents(bool start);
        bool trashTorrentFiles() const;
        void setTrashTorrentFiles(bool trash);
        bool renameIncompleteFiles() const;
        void setRenameIncompleteFiles(bool rename);
        bool isIncompleteDirectoryEnabled() const;
        void setIncompleteDirectoryEnabled(bool enabled);
        const QString& incompleteDirectory() const;
        void setIncompleteDirectory(const QString& directory);

        bool isRatioLimited() const;
        void setRatioLimited(bool limited);
        double ratioLimit() const;
        void setRatioLimit(double limit);
        bool isIdleSeedingLimited() const;
        void setIdleSeedingLimited(bool limited);
        int idleSeedingLimit() const;
        void setIdleSeedingLimit(int limit);

        bool isDownloadQueueEnabled() const;
        void setDownloadQueueEnabled(bool enabled);
        int downloadQueueSize() const;
        void setDownloadQueueSize(int size);
        bool isSeedQueueEnabled() const;
        void setSeedQueueEnabled(bool enabled);
        int seedQueueSize() const;
        void setSeedQueueSize(int size);
        bool isIdleQueueLimited() const;
        void setIdleQueueLimited(bool limited);
        int idleQueueLimit() const;
        void setIdleQueueLimit(int limit);

        bool isDownloadSpeedLimited() const;
        void setDownloadSpeedLimited(bool limited);
        int downloadSpeedLimit() const; // kB/s
        void setDownloadSpeedLimit(int limit);
        bool isUploadSpeedLimited() const;
        void setUploadSpeedLimited(bool limited);
        int uploadSpeedLimit() const; // kB/s
        void setUploadSpeedLimit(int limit);
        bool isAlternativeSpeedLimitsEnabled() const;
        void setAlternativeSpeedLimitsEnabled(bool enabled);
        int alternativeDownloadSpeedLimit() const; // kB/s
        void setAlternativeDownloadSpeedLimit(int limit); // kB/s
        int alternativeUploadSpeedLimit() const; // kB/s
        void setAlternativeUploadSpeedLimit(int limit);
        bool isAlternativeSpeedLimitsScheduled() const;
        void setAlternativeSpeedLimitsScheduled(bool scheduled);
        QTime alternativeSpeedLimitsBeginTime() const;
        void setAlternativeSpeedLimitsBeginTime(QTime time);
        QTime alternativeSpeedLimitsEndTime() const;
        void setAlternativeSpeedLimitsEndTime(QTime time);
        ServerSettingsData::AlternativeSpeedLimitsDays alternativeSpeedLimitsDays() const;
        void setAlternativeSpeedLimitsDays(libtremotesf::ServerSettingsData::AlternativeSpeedLimitsDays days);

        int peerPort() const;
        void setPeerPort(int port);
        bool isRandomPortEnabled() const;
        void setRandomPortEnabled(bool enabled);
        bool isPortForwardingEnabled() const;
        void setPortForwardingEnabled(bool enabled);
        ServerSettingsData::EncryptionMode encryptionMode() const;
        void setEncryptionMode(libtremotesf::ServerSettingsData::EncryptionMode mode);
        bool isUtpEnabled() const;
        void setUtpEnabled(bool enabled);
        bool isPexEnabled() const;
        void setPexEnabled(bool enabled);
        bool isDhtEnabled() const;
        void setDhtEnabled(bool enabled);
        bool isLpdEnabled() const;
        void setLpdEnabled(bool enabled);
        int maximumPeersPerTorrent() const;
        void setMaximumPeersPerTorrent(int peers);
        int maximumPeersGlobally() const;
        void setMaximumPeersGlobally(int peers);

        bool saveOnSet() const;
        void setSaveOnSet(bool save);

        void update(const QJsonObject& serverSettings);
        void save() const;

        const ServerSettingsData& data() const;

    private:
        Rpc* mRpc;
        ServerSettingsData mData;
        bool mSaveOnSet;

    signals:
        void changed();
    };
}

SPECIALIZE_FORMATTER_FOR_Q_ENUM(libtremotesf::ServerSettingsData::AlternativeSpeedLimitsDays)
SPECIALIZE_FORMATTER_FOR_Q_ENUM(libtremotesf::ServerSettingsData::EncryptionMode)

#endif // LIBTREMOTESF_SERVERSETTINGS_H
