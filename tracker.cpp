// SPDX-FileCopyrightText: 2015-2022 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "tracker.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonObject>
#include <QHostAddress>
#include <QUrl>

#include "jsonutils.h"
#include "literals.h"
#include "stdutils.h"

namespace libtremotesf {
    using namespace impl;
    namespace {
        constexpr auto statusMapper = EnumMapper(std::array{
            EnumMapping(Tracker::Status::Inactive, 0),
            EnumMapping(Tracker::Status::WaitingForUpdate, 1),
            EnumMapping(Tracker::Status::QueuedForUpdate, 2),
            EnumMapping(Tracker::Status::Updating, 3)});
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

    const QDateTime& Tracker::nextUpdateTime() const { return mNextUpdateTime; }

    bool Tracker::update(const QJsonObject& trackerMap) {
        bool changed = false;

        QString announce(trackerMap.value("announce"_l1).toString());
        if (announce != mAnnounce) {
            changed = true;
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

        const bool announceError =
            (!trackerMap.value("lastAnnounceSucceeded"_l1).toBool() &&
             trackerMap.value("lastAnnounceTime"_l1).toInt() != 0);
        if (announceError) {
            setChanged(mErrorMessage, trackerMap.value("lastAnnounceResult"_l1).toString(), changed);
        } else {
            setChanged(mErrorMessage, {}, changed);
        }

        constexpr auto announceStateKey = "announceState"_l1;
        setChanged(mStatus, statusMapper.fromJsonValue(trackerMap.value(announceStateKey), announceStateKey), changed);

        setChanged(mPeers, trackerMap.value("lastAnnouncePeerCount"_l1).toInt(), changed);
        updateDateTime(mNextUpdateTime, trackerMap.value("nextAnnounceTime"_l1), changed);

        return changed;
    }
}
