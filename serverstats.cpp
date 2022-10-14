// SPDX-FileCopyrightText: 2015-2022 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "serverstats.h"

#include <QJsonObject>

#include "literals.h"

namespace libtremotesf
{
    long long SessionStats::downloaded() const
    {
        return mDownloaded;
    }

    long long SessionStats::uploaded() const
    {
        return mUploaded;
    }

    int SessionStats::duration() const
    {
        return mDuration;
    }

    int SessionStats::sessionCount() const
    {
        return mSessionCount;
    }

    void SessionStats::update(const QJsonObject& stats)
    {
        mDownloaded = static_cast<long long>(stats.value("downloadedBytes"_l1).toDouble());
        mUploaded = static_cast<long long>(stats.value("uploadedBytes"_l1).toDouble());
        mDuration = stats.value("secondsActive"_l1).toInt();
        mSessionCount = stats.value("sessionCount"_l1).toInt();
    }

    ServerStats::ServerStats(QObject* parent)
        : QObject(parent),
          mDownloadSpeed(0),
          mUploadSpeed(0),
          mCurrentSession(),
          mTotal()
    {

    }

    long long ServerStats::downloadSpeed() const
    {
        return mDownloadSpeed;
    }

    long long ServerStats::uploadSpeed() const
    {
        return mUploadSpeed;
    }

    SessionStats ServerStats::currentSession() const
    {
        return mCurrentSession;
    }

    SessionStats ServerStats::total() const
    {
        return mTotal;
    }

    void ServerStats::update(const QJsonObject& serverStats)
    {
        mDownloadSpeed = static_cast<long long>(serverStats.value("downloadSpeed"_l1).toDouble());
        mUploadSpeed = static_cast<long long>(serverStats.value("uploadSpeed"_l1).toDouble());
        mCurrentSession.update(serverStats.value("current-stats"_l1).toObject());
        mTotal.update(serverStats.value("cumulative-stats"_l1).toObject());
        emit updated();
    }
}
