// SPDX-FileCopyrightText: 2015-2022 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_RPC_PATHUTILS_H
#define TREMOTESF_RPC_PATHUTILS_H

#include <QString>

namespace libtremotesf {
    QString normalizePath(const QString& path);
    QString toNativeSeparators(const QString& path);
}

namespace tremotesf {
    using libtremotesf::normalizePath;
    using libtremotesf::toNativeSeparators;
}

#endif // TREMOTESF_RPC_PATHUTILS_H
