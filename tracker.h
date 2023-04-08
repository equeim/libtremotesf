// SPDX-FileCopyrightText: 2015-2023 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LIBTREMOTESF_TRACKER_H
#define LIBTREMOTESF_TRACKER_H

#include <QDateTime>
#include <QObject>
#include <QString>

#include "formatters.h"

class QJsonObject;
class QUrl;

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

        bool operator==(const Tracker& other) const = default;

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

    namespace impl {
        QString registrableDomainFromUrl(const QUrl& url);
    }
}

SPECIALIZE_FORMATTER_FOR_Q_ENUM(libtremotesf::Tracker::Status)

#endif // LIBTREMOTESF_TRACKER_H
