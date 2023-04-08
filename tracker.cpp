// SPDX-FileCopyrightText: 2015-2023 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "tracker.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonObject>
#include <QHostAddress>
#include <QUrl>
#include <libpsl.h>

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

    bool Tracker::update(const QJsonObject& trackerMap) {
        bool changed = false;

        QString announce(trackerMap.value("announce"_l1).toString());
        if (announce != mAnnounce) {
            changed = true;
            mAnnounce = std::move(announce);

            const auto host = QUrl(mAnnounce).host().toLower().normalized(QString::NormalizationForm_KC);
            const bool isIpAddress = !QHostAddress(host).isNull();
            if (!isIpAddress) {
                const auto domain = host.toUtf8();
                const char* registrableDomain = psl_registrable_domain(psl_builtin(), domain);
                if (registrableDomain) {
                    mSite = QString::fromUtf8(registrableDomain);
                } else {
                    mSite = host;
                }
            } else {
                mSite = host;
            }
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
        setChanged(
            mSeeders,
            [&] {
                if (auto seeders = trackerMap.value("seederCount"_l1).toInt(); seeders >= 0) {
                    return seeders;
                }
                return 0;
            }(),
            changed
        );
        setChanged(
            mLeechers,
            [&] {
                if (auto leechers = trackerMap.value("leecherCount"_l1).toInt(); leechers >= 0) {
                    return leechers;
                }
                return 0;
            }(),
            changed
        );
        updateDateTime(mNextUpdateTime, trackerMap.value("nextAnnounceTime"_l1), changed);

        return changed;
    }
}
