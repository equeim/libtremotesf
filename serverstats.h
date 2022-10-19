// SPDX-FileCopyrightText: 2015-2022 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LIBTREMOTESF_SERVERSTATS_H
#define LIBTREMOTESF_SERVERSTATS_H

#include <QObject>

class QJsonObject;

namespace libtremotesf {
    class Rpc;

    class SessionStats {
    public:
        long long downloaded() const;
        long long uploaded() const;
        int duration() const;
        int sessionCount() const;

        void update(const QJsonObject& stats);

    private:
        long long mDownloaded;
        long long mUploaded;
        int mDuration;
        int mSessionCount;
    };

    class ServerStats : public QObject {
        Q_OBJECT
    public:
        explicit ServerStats(QObject* parent = nullptr);

        long long downloadSpeed() const;
        long long uploadSpeed() const;

        SessionStats currentSession() const;
        SessionStats total() const;

        void update(const QJsonObject& serverStats);

    private:
        long long mDownloadSpeed;
        long long mUploadSpeed;
        SessionStats mCurrentSession;
        SessionStats mTotal;
    signals:
        void updated();
    };
}

#endif // LIBTREMOTESF_SERVERSTATS_H
