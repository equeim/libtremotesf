// SPDX-FileCopyrightText: 2015-2023 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_RPC_PATHUTILS_H
#define TREMOTESF_RPC_PATHUTILS_H

#include <QString>

#include "target_os.h"

namespace libtremotesf {
    bool isAbsoluteWindowsDOSFilePath(QStringView path);

    /**
     * We need to pass PathOs explicitly because we can't determing whether given path is Unix or Windows path from its string alone:
     * There is no way to distinguish whether '//foo' is Unix path with duplicate directory separator, or Windows UNC path
     * (we need to handle Windows paths with both '\' and '/' separators)
     */

    enum class PathOs { Unix, Windows };

    constexpr inline PathOs localPathOs = isTargetOsWindows ? PathOs::Windows : PathOs::Unix;

    QString normalizePath(const QString& path, PathOs pathOs);
    QString toNativeSeparators(const QString& path, PathOs pathOs);
}

namespace tremotesf {
    using libtremotesf::localPathOs;
    using libtremotesf::normalizePath;
    using libtremotesf::PathOs;
    using libtremotesf::toNativeSeparators;
}

#endif // TREMOTESF_RPC_PATHUTILS_H
