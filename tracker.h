// SPDX-FileCopyrightText: 2015-2022 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LIBTREMOTESF_TRACKER_H
#define LIBTREMOTESF_TRACKER_H

#include <QDateTime>
#include <QObject>
#include <QString>

#include "formatters.h"

class QJsonObject;

namespace libtremotesf {
    class Tracker {
        Q_GADGET
    public:
        enum class Status {
            // Tracker is inactive, possibly due to error
            Inactive,
            // Waiting for announce/scrape
            WaitingForUpdate,
            // Queued for immediate announce/scrape
            QueuedForUpdate,
            // We are announcing/scraping
            Updating,
        };
        Q_ENUM(Status)

        explicit Tracker(int id, const QJsonObject& trackerMap);

        int id() const { return mId; };
        const QString& announce() const { return mAnnounce; };
        const QString& site() const { return mSite; };

        Status status() const { return mStatus; };
        QString errorMessage() const { return mErrorMessage; };

        int peers() const { return mPeers; };
        int seeders() const { return mSeeders; }
        int leechers() const { return mLeechers; }
        const QDateTime& nextUpdateTime() const { return mNextUpdateTime; };

        bool update(const QJsonObject& trackerMap);

        inline bool operator==(const Tracker& other) const {
            return mId == other.mId && mAnnounce == other.mAnnounce &&
                   mSite == other.mSite &&
                   mErrorMessage == other.mErrorMessage && mStatus == other.mStatus &&
                   mNextUpdateTime == other.mNextUpdateTime && mPeers == other.mPeers;
        }

        inline bool operator!=(const Tracker& other) const { return !(*this == other); }

    private:
        QString mAnnounce{};
        QString mSite{};

        Status mStatus{};
        QString mErrorMessage{};

        QDateTime mNextUpdateTime{{}, {}, Qt::UTC};

        int mPeers{};
        int mSeeders{};
        int mLeechers{};

        int mId{};
    };
}

SPECIALIZE_FORMATTER_FOR_Q_ENUM(libtremotesf::Tracker::Status)

#endif // LIBTREMOTESF_TRACKER_H
