// SPDX-FileCopyrightText: 2015-2022 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LIBTREMOTESF_PEER_H
#define LIBTREMOTESF_PEER_H

#include <QString>

class QJsonObject;

namespace libtremotesf
{
    struct Peer
    {
        static const QLatin1String addressKey;

        explicit Peer(QString&& address, const QJsonObject& peerJson);
        bool update(const QJsonObject& peerJson);

        bool operator==(const Peer& other) const {
            return address == other.address;
        }

        QString address;
        QString client;
        long long downloadSpeed;
        long long uploadSpeed;
        double progress;
        QString flags;
    };
}

#endif // LIBTREMOTESF_PEER_H
