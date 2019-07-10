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

#ifndef LIBTREMOTESF_SERVERSETTINGS_H
#define LIBTREMOTESF_SERVERSETTINGS_H

#include <QObject>
#include <QTime>

class QJsonObject;

namespace libtremotesf
{
    class Rpc;

    class ServerSettings : public QObject
    {
        Q_OBJECT

        Q_PROPERTY(bool canRenameFiles READ canRenameFiles)
        Q_PROPERTY(bool canShowFreeSpaceForPath READ canShowFreeSpaceForPath)

        Q_PROPERTY(QString downloadDirectory READ downloadDirectory WRITE setDownloadDirectory)
        Q_PROPERTY(bool startAddedTorrents READ startAddedTorrents WRITE setStartAddedTorrents)
        Q_PROPERTY(bool trashTorrentFiles READ trashTorrentFiles WRITE setTrashTorrentFiles)
        Q_PROPERTY(bool renameIncompleteFiles READ renameIncompleteFiles WRITE setRenameIncompleteFiles)
        Q_PROPERTY(bool incompleteDirectoryEnabled READ isIncompleteDirectoryEnabled WRITE setIncompleteDirectoryEnabled)
        Q_PROPERTY(QString incompleteDirectory READ incompleteDirectory WRITE setIncompleteDirectory)

        Q_PROPERTY(bool ratioLimited READ isRatioLimited WRITE setRatioLimited)
        Q_PROPERTY(double ratioLimit READ ratioLimit WRITE setRatioLimit)
        Q_PROPERTY(bool idleSeedingLimited READ isIdleSeedingLimited WRITE setIdleSeedingLimited)
        Q_PROPERTY(int idleSeedingLimit READ idleSeedingLimit WRITE setIdleSeedingLimit)

        Q_PROPERTY(bool downloadQueueEnabled READ isDownloadQueueEnabled WRITE setDownloadQueueEnabled)
        Q_PROPERTY(int downloadQueueSize READ downloadQueueSize WRITE setDownloadQueueSize)
        Q_PROPERTY(bool seedQueueEnabled READ isSeedQueueEnabled WRITE setSeedQueueEnabled)
        Q_PROPERTY(int seedQueueSize READ seedQueueSize WRITE setSeedQueueSize)
        Q_PROPERTY(bool idleQueueLimited READ isIdleQueueLimited WRITE setIdleQueueLimited)
        Q_PROPERTY(int idleQueueLimit READ idleQueueLimit WRITE setIdleQueueLimit)

        Q_PROPERTY(bool downloadSpeedLimited READ isDownloadSpeedLimited WRITE setDownloadSpeedLimited)
        Q_PROPERTY(int downloadSpeedLimit READ downloadSpeedLimit WRITE setDownloadSpeedLimit)
        Q_PROPERTY(bool uploadSpeedLimited READ isUploadSpeedLimited WRITE setUploadSpeedLimited)
        Q_PROPERTY(int uploadSpeedLimit READ uploadSpeedLimit WRITE setUploadSpeedLimit)
        Q_PROPERTY(bool alternativeSpeedLimitsEnabled READ isAlternativeSpeedLimitsEnabled WRITE setAlternativeSpeedLimitsEnabled)
        Q_PROPERTY(int alternativeDownloadSpeedLimit READ alternativeDownloadSpeedLimit WRITE setAlternativeDownloadSpeedLimit)
        Q_PROPERTY(int alternativeUploadSpeedLimit READ alternativeUploadSpeedLimit WRITE setAlternativeUploadSpeedLimit)
        Q_PROPERTY(bool alternativeSpeedLimitsScheduled READ isAlternativeSpeedLimitsScheduled WRITE setAlternativeSpeedLimitsScheduled)
        Q_PROPERTY(QTime alternativeSpeedLimitsBeginTime READ alternativeSpeedLimitsBeginTime WRITE setAlternativeSpeedLimitsBeginTime)
        Q_PROPERTY(QTime alternativeSpeedLimitsEndTime READ alternativeSpeedLimitsEndTime WRITE setAlternativeSpeedLimitsEndTime)
        Q_PROPERTY(AlternativeSpeedLimitsDays alternativeSpeedLimitsDays READ alternativeSpeedLimitsDays WRITE setAlternativeSpeedLimitsDays)

        Q_PROPERTY(int peerPort READ peerPort WRITE setPeerPort)
        Q_PROPERTY(bool randomPortEnabled READ isRandomPortEnabled WRITE setRandomPortEnabled)
        Q_PROPERTY(bool portForwardingEnabled READ isPortForwardingEnabled WRITE setPortForwardingEnabled)
        Q_PROPERTY(EncryptionMode encryptionMode READ encryptionMode WRITE setEncryptionMode)
        Q_PROPERTY(bool utpEnabled READ isUtpEnabled WRITE setUtpEnabled)
        Q_PROPERTY(bool pexEnabled READ isPexEnabled WRITE setPexEnabled)
        Q_PROPERTY(bool dhtEnabled READ isDhtEnabled WRITE setDhtEnabled)
        Q_PROPERTY(bool lpdEnabled READ isLpdEnabled WRITE setLpdEnabled)
        Q_PROPERTY(int maximumPeerPerTorrent READ maximumPeersPerTorrent WRITE setMaximumPeersPerTorrent)
        Q_PROPERTY(int maximumPeersGlobally READ maximumPeersGlobally WRITE setMaximumPeersGlobally)

    public:
        enum AlternativeSpeedLimitsDays
        {
            Sunday = (1 << 0),
            Monday = (1 << 1),
            Tuesday = (1 << 2),
            Wednesday = (1 << 3),
            Thursday = (1 << 4),
            Friday = (1 << 5),
            Saturday = (1 << 6),
            Weekdays = (Monday | Tuesday | Wednesday | Thursday | Friday),
            Weekends = (Sunday | Saturday),
            All = (Weekdays | Weekends)
        };
        Q_ENUM(AlternativeSpeedLimitsDays)

        enum EncryptionMode
        {
            AllowedEncryption,
            PreferredEncryption,
            RequiredEncryption
        };
        Q_ENUM(EncryptionMode)

        explicit ServerSettings(Rpc* rpc = nullptr, QObject* parent = nullptr);

        int rpcVersion() const;
        int minimumRpcVersion() const;

        bool canRenameFiles() const;
        bool canShowFreeSpaceForPath() const;

        const QString& downloadDirectory() const;
        Q_INVOKABLE void setDownloadDirectory(const QString& directory);
        bool startAddedTorrents() const;
        Q_INVOKABLE void setStartAddedTorrents(bool start);
        bool trashTorrentFiles() const;
        Q_INVOKABLE void setTrashTorrentFiles(bool trash);
        bool renameIncompleteFiles() const;
        Q_INVOKABLE void setRenameIncompleteFiles(bool rename);
        bool isIncompleteDirectoryEnabled() const;
        Q_INVOKABLE void setIncompleteDirectoryEnabled(bool enabled);
        const QString& incompleteDirectory() const;
        Q_INVOKABLE void setIncompleteDirectory(const QString& directory);

        bool isRatioLimited() const;
        Q_INVOKABLE void setRatioLimited(bool limited);
        double ratioLimit() const;
        Q_INVOKABLE void setRatioLimit(double limit);
        bool isIdleSeedingLimited() const;
        Q_INVOKABLE void setIdleSeedingLimited(bool limited);
        int idleSeedingLimit() const;
        Q_INVOKABLE void setIdleSeedingLimit(int limit);

        bool isDownloadQueueEnabled() const;
        Q_INVOKABLE void setDownloadQueueEnabled(bool enabled);
        int downloadQueueSize() const;
        Q_INVOKABLE void setDownloadQueueSize(int size);
        bool isSeedQueueEnabled() const;
        Q_INVOKABLE void setSeedQueueEnabled(bool enabled);
        int seedQueueSize() const;
        Q_INVOKABLE void setSeedQueueSize(int size);
        bool isIdleQueueLimited() const;
        Q_INVOKABLE void setIdleQueueLimited(bool limited);
        int idleQueueLimit() const;
        Q_INVOKABLE void setIdleQueueLimit(int limit);

        bool isDownloadSpeedLimited() const;
        Q_INVOKABLE void setDownloadSpeedLimited(bool limited);
        int downloadSpeedLimit() const;
        Q_INVOKABLE void setDownloadSpeedLimit(int limit);
        bool isUploadSpeedLimited() const;
        Q_INVOKABLE void setUploadSpeedLimited(bool limited);
        int uploadSpeedLimit() const;
        Q_INVOKABLE void setUploadSpeedLimit(int limit);
        bool isAlternativeSpeedLimitsEnabled() const;
        Q_INVOKABLE void setAlternativeSpeedLimitsEnabled(bool enabled);
        int alternativeDownloadSpeedLimit() const;
        Q_INVOKABLE void setAlternativeDownloadSpeedLimit(int limit);
        int alternativeUploadSpeedLimit() const;
        Q_INVOKABLE void setAlternativeUploadSpeedLimit(int limit);
        bool isAlternativeSpeedLimitsScheduled() const;
        Q_INVOKABLE void setAlternativeSpeedLimitsScheduled(bool scheduled);
        const QTime& alternativeSpeedLimitsBeginTime() const;
        Q_INVOKABLE void setAlternativeSpeedLimitsBeginTime(const QTime& time);
        const QTime& alternativeSpeedLimitsEndTime() const;
        Q_INVOKABLE void setAlternativeSpeedLimitsEndTime(const QTime& time);
        AlternativeSpeedLimitsDays alternativeSpeedLimitsDays() const;
        Q_INVOKABLE void setAlternativeSpeedLimitsDays(libtremotesf::ServerSettings::AlternativeSpeedLimitsDays days);

        int peerPort() const;
        Q_INVOKABLE void setPeerPort(int port);
        bool isRandomPortEnabled() const;
        Q_INVOKABLE void setRandomPortEnabled(bool enabled);
        bool isPortForwardingEnabled() const;
        Q_INVOKABLE void setPortForwardingEnabled(bool enabled);
        EncryptionMode encryptionMode() const;
        Q_INVOKABLE void setEncryptionMode(libtremotesf::ServerSettings::EncryptionMode mode);
        bool isUtpEnabled() const;
        Q_INVOKABLE void setUtpEnabled(bool enabled);
        bool isPexEnabled() const;
        Q_INVOKABLE void setPexEnabled(bool enabled);
        bool isDhtEnabled() const;
        Q_INVOKABLE void setDhtEnabled(bool enabled);
        bool isLpdEnabled() const;
        Q_INVOKABLE void setLpdEnabled(bool enabled);
        int maximumPeersPerTorrent() const;
        Q_INVOKABLE void setMaximumPeersPerTorrent(int peers);
        int maximumPeersGlobally() const;
        Q_INVOKABLE void setMaximumPeersGlobally(int peers);

        bool saveOnSet() const;
        void setSaveOnSet(bool save);

        int toKibiBytes(int kiloBytesOrKibiBytes) const;
        int fromKibiBytes(int kibiBytes) const;

        void update(const QJsonObject& serverSettings);
        void save() const;

    private:
        Rpc* mRpc;

        int mRpcVersion = 0;
        int mMinimumRpcVersion = 0;

        bool mUsingDecimalUnits = false;

        QString mDownloadDirectory;
        bool mStartAddedTorrents = false;
        bool mTrashTorrentFiles = false;
        bool mRenameIncompleteFiles = false;
        bool mIncompleteDirectoryEnabled = false;
        QString mIncompleteDirectory;

        bool mRatioLimited = false;
        double mRatioLimit = 0.0;
        bool mIdleSeedingLimited = false;
        int mIdleSeedingLimit = 0;

        bool mDownloadQueueEnabled = false;
        int mDownloadQueueSize = 0;
        bool mSeedQueueEnabled = false;
        int mSeedQueueSize = 0;
        bool mIdleQueueLimited = false;
        int mIdleQueueLimit = 0;

        bool mDownloadSpeedLimited = false;
        int mDownloadSpeedLimit = 0;
        bool mUploadSpeedLimited = false;
        int mUploadSpeedLimit = 0;
        bool mAlternativeSpeedLimitsEnabled = false;
        int mAlternativeDownloadSpeedLimit = 0;
        int mAlternativeUploadSpeedLimit = 0;
        bool mAlternativeSpeedLimitsScheduled = false;
        QTime mAlternativeSpeedLimitsBeginTime;
        QTime mAlternativeSpeedLimitsEndTime;
        AlternativeSpeedLimitsDays mAlternativeSpeedLimitsDays = All;

        int mPeerPort = 0;
        bool mRandomPortEnabled = false;
        bool mPortForwardingEnabled = false;
        EncryptionMode mEncryptionMode = PreferredEncryption;
        bool mUtpEnabled = false;
        bool mPexEnabled = false;
        bool mDhtEnabled = false;
        bool mLpdEnabled = false;
        int mMaximumPeersPerTorrent = 0;
        int mMaximumPeersGlobally = 0;

        bool mSaveOnSet;
    };
}

#endif // LIBTREMOTESF_SERVERSETTINGS_H
