// SPDX-FileCopyrightText: 2015-2022 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "peer.h"

#include <QJsonObject>

#include "stdutils.h"

namespace libtremotesf
{
    const QLatin1String Peer::addressKey("address");

    Peer::Peer(QString&& address, const QJsonObject& peerJson)
        : address(std::move(address)),
          client(peerJson.value(QLatin1String("clientName")).toString())
    {
        update(peerJson);
    }

    bool Peer::update(const QJsonObject& peerJson)
    {
        bool changed = false;
        setChanged(downloadSpeed, static_cast<long long>(peerJson.value(QLatin1String("rateToClient")).toDouble()), changed);
        setChanged(uploadSpeed, static_cast<long long>(peerJson.value(QLatin1String("rateToPeer")).toDouble()), changed);
        setChanged(progress, peerJson.value(QLatin1String("progress")).toDouble(), changed);
        setChanged(flags, peerJson.value(QLatin1String("flagStr")).toString(), changed);
        return changed;
    }
}
