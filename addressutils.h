// SPDX-FileCopyrightText: 2015-2023 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LIBTREMOTESF_ADDRESSUTILS_H
#define LIBTREMOTESF_ADDRESSUTILS_H

#include <optional>

class QHostAddress;
class QString;

namespace libtremotesf {
    [[nodiscard]] bool isLocalIpAddress(const QHostAddress& ipAddress);
    [[nodiscard]] std::optional<bool> isLocalIpAddress(const QString& address);
}

#endif // LIBTREMOTESF_ADDRESSUTILS_H
