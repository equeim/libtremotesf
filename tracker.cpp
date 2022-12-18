// SPDX-FileCopyrightText: 2015-2022 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "tracker.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonObject>
#include <QHostAddress>
#include <QUrl>

#include "literals.h"
#include "stdutils.h"

namespace libtremotesf {
    namespace {
        Tracker::Status statusFromInt(int intStatus) {
            switch (auto status = static_cast<Tracker::Status>(intStatus)) {
            case Tracker::Status::Inactive:
            case Tracker::Status::Active:
            case Tracker::Status::Queued:
            case Tracker::Status::Updating:
            case Tracker::Status::Error:
                return static_cast<Tracker::Status>(status);
            }
            return Tracker::Status::Inactive;
        }
    }

    Tracker::Tracker(int id, const QJsonObject& trackerMap) : mId(id) { update(trackerMap); }

    int Tracker::id() const { return mId; }

    const QString& Tracker::announce() const { return mAnnounce; }

#if QT_VERSION_MAJOR < 6
    const QString& Tracker::site() const { return mSite; }
#endif

    Tracker::AnnounceHostInfo Tracker::announceHostInfo() const {
        auto host = QUrl(mAnnounce).host();
        bool isIpAddress = !QHostAddress(host).isNull();
        return {std::move(host), isIpAddress};
    }

    Tracker::Status Tracker::status() const { return mStatus; }

    QString Tracker::errorMessage() const { return mErrorMessage; }

    int Tracker::peers() const { return mPeers; }

    long long Tracker::nextUpdateTime() const { return mNextUpdateTime; }

    int Tracker::nextUpdateEta() const { return mNextUpdateEta; }

    Tracker::UpdateResult Tracker::update(const QJsonObject& trackerMap) {
        bool changed = false;
        bool announceUrlChanged = false;

        QString announce(trackerMap.value("announce"_l1).toString());
        if (announce != mAnnounce) {
            changed = true;
            announceUrlChanged = true;
            mAnnounce = std::move(announce);
#if QT_VERSION_MAJOR < 6
            const QUrl url(mAnnounce);
            mSite = url.host();
            const int topLevelDomainSize = url.topLevelDomain().size();
            if (topLevelDomainSize > 0) {
                mSite.remove(0, mSite.lastIndexOf(QLatin1Char('.'), -1 - topLevelDomainSize) + 1);
            }
#endif
        }

        const bool scrapeError =
            (!trackerMap.value("lastScrapeSucceeded"_l1).toBool() && trackerMap.value("lastScrapeTime"_l1).toInt() != 0
            );

        const bool announceError =
            (!trackerMap.value("lastAnnounceSucceeded"_l1).toBool() &&
             trackerMap.value("lastAnnounceTime"_l1).toInt() != 0);

        if (scrapeError || announceError) {
            setChanged(mStatus, Status::Error, changed);
            if (scrapeError) {
                setChanged(mErrorMessage, trackerMap.value("lastScrapeResult"_l1).toString(), changed);
            } else {
                setChanged(mErrorMessage, trackerMap.value("lastAnnounceResult"_l1).toString(), changed);
            }
        } else {
            setChanged(mStatus, statusFromInt(trackerMap.value("announceState"_l1).toInt()), changed);
            setChanged(mErrorMessage, {}, changed);
        }

        setChanged(mPeers, trackerMap.value("lastAnnouncePeerCount"_l1).toInt(), changed);

        const long long nextUpdateTime = static_cast<long long>(trackerMap.value("nextAnnounceTime"_l1).toDouble());
        if (nextUpdateTime != mNextUpdateTime) {
            mNextUpdateTime = nextUpdateTime;
            changed = true;
        }
        const long long nextUpdateEta = nextUpdateTime - QDateTime::currentMSecsSinceEpoch() / 1000;
        if (nextUpdateEta < 0 || nextUpdateEta > std::numeric_limits<int>::max()) {
            mNextUpdateEta = -1;
        } else {
            mNextUpdateEta = static_cast<int>(nextUpdateEta);
        }

        return {changed, announceUrlChanged};
    }
}
