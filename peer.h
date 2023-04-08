// SPDX-FileCopyrightText: 2015-2023 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LIBTREMOTESF_PEER_H
#define LIBTREMOTESF_PEER_H

#include <QString>
#include "literals.h"

class QJsonObject;

namespace libtremotesf {
    struct Peer {
#ifndef SWIG
        static constexpr auto addressKey = "address"_l1;
#endif

        explicit Peer(QString&& address, const QJsonObject& peerJson);
        bool update(const QJsonObject& peerJson);

        bool operator==(const Peer& other) const = default;

        QString address{};
        QString client{};
        qint64 downloadSpeed{};
        qint64 uploadSpeed{};
        double progress{};
        QString flags{};
    };
}

#endif // LIBTREMOTESF_PEER_H
