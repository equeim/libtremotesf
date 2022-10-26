// SPDX-FileCopyrightText: 2015-2022 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_RPC_PATHUTILS_H
#define TREMOTESF_RPC_PATHUTILS_H

#include <QString>

namespace libtremotesf {
    class Rpc;

    QString normalizeRemotePath(const QString& path, const Rpc* rpc);
    QString toNativeRemoteSeparators(const QString& path, const Rpc* rpc);
}

namespace tremotesf {
    using libtremotesf::normalizeRemotePath;
    using libtremotesf::toNativeRemoteSeparators;
}

#endif // TREMOTESF_RPC_PATHUTILS_H
