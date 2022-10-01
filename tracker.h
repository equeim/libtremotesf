// SPDX-FileCopyrightText: 2015-2022 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LIBTREMOTESF_TRACKER_H
#define LIBTREMOTESF_TRACKER_H

#include <QString>

class QJsonObject;

namespace libtremotesf
{
    class Tracker
    {
    public:
        enum Status
        {
            Inactive,
            Active,
            Queued,
            Updating,
            Error
        };

        explicit Tracker(int id, const QJsonObject& trackerMap);

        int id() const;
        const QString& announce() const;
#if QT_VERSION_MAJOR < 6
        const QString& site() const;
#endif
        struct AnnounceHostInfo {
            QString host;
            bool isIpAddress;
        };
        AnnounceHostInfo announceHostInfo() const;

        Status status() const;
        QString errorMessage() const;

        int peers() const;
        long long nextUpdateTime() const;
        int nextUpdateEta() const;

        struct UpdateResult {
            bool changed;
            bool announceUrlChanged;
        };
        UpdateResult update(const QJsonObject& trackerMap);

        inline bool operator==(const Tracker& other) const
        {
            return mId == other.mId &&
                    mAnnounce == other.mAnnounce &&
#if QT_VERSION_MAJOR < 6
                    mSite == other.mSite &&
#endif
                    mErrorMessage == other.mErrorMessage &&
                    mStatus == other.mStatus &&
                    mNextUpdateEta == other.mNextUpdateEta &&
                    mNextUpdateTime == other.mNextUpdateTime &&
                    mPeers == other.mPeers;
        }

        inline bool operator!=(const Tracker& other) const { return !(*this == other); }

    private:
        QString mAnnounce;
#if QT_VERSION_MAJOR < 6
        QString mSite;
#endif

        QString mErrorMessage;
        Status mStatus = Inactive;

        int mNextUpdateEta = -1;
        long long mNextUpdateTime = 0;

        int mPeers = 0;

        int mId;
    };
}

#endif // LIBTREMOTESF_TRACKER_H
