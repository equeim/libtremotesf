// SPDX-FileCopyrightText: 2015-2022 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "serverstats.h"

#include <QJsonObject>

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
        mDownloaded = static_cast<long long>(stats.value(QLatin1String("downloadedBytes")).toDouble());
        mUploaded = static_cast<long long>(stats.value(QLatin1String("uploadedBytes")).toDouble());
        mDuration = stats.value(QLatin1String("secondsActive")).toInt();
        mSessionCount = stats.value(QLatin1String("sessionCount")).toInt();
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
        mDownloadSpeed = static_cast<long long>(serverStats.value(QLatin1String("downloadSpeed")).toDouble());
        mUploadSpeed = static_cast<long long>(serverStats.value(QLatin1String("uploadSpeed")).toDouble());
        mCurrentSession.update(serverStats.value(QLatin1String("current-stats")).toObject());
        mTotal.update(serverStats.value(QLatin1String("cumulative-stats")).toObject());
        emit updated();
    }
}
