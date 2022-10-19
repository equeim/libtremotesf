// SPDX-FileCopyrightText: 2015-2022 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LIBTREMTOESF_TARGET_OS_H
#define LIBTREMTOESF_TARGET_OS_H

#include <QtGlobal>

namespace libtremotesf {
    enum class TargetOs { UnixFreedesktop, UnixAndroid, UnixOther, Windows, Other };

    inline constexpr TargetOs targetOs =
#ifdef Q_OS_UNIX
#    ifdef TREMOTESF_UNIX_FREEDESKTOP
        TargetOs::UnixFreedesktop;
#    else
#        ifdef Q_OS_ANDROID
        TargetOs::UnixAndroid;
#        else
        TargetOs::UnixOther;
#        endif
#    endif
#else
#    ifdef Q_OS_WIN
        TargetOs::Windows;
#    else
        TargetOs::Other;
#    endif
#endif

    inline constexpr bool isTargetOsWindows{targetOs == TargetOs::Windows};
}

namespace tremotesf {
    using libtremotesf::isTargetOsWindows;
    using libtremotesf::TargetOs;
    using libtremotesf::targetOs;
}

#endif // LIBTREMTOESF_TARGET_OS_H
