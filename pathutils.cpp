// SPDX-FileCopyrightText: 2015-2022 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QRegularExpression>

#include "literals.h"
#include "pathutils.h"

namespace libtremotesf {
    // We can't use QDir::to/fromNativeSeparators because it checks for current OS,
    // and we need it to work regardless of OS we are running on

    namespace {
        constexpr auto windowsSeparatorChar = '\\';
        constexpr auto unixSeparatorChar = '/';
        constexpr auto unixSeparatorString = "/"_l1;
        constexpr QString::size_type minimumWindowsPathLength = 3; // e.g. C:/

        bool isAbsoluteWindowsFilePath(const QString& path) {
            static const QRegularExpression regex(R"(^[A-Za-z]:[\\/].*$)"_l1);
            return regex.match(path).hasMatch();
        }

        void convertFromNativeWindowsSeparators(QString& path) {
            path.replace(windowsSeparatorChar, unixSeparatorChar);
        }

        void capitalizeWindowsDriveLetter(QString& path) {
            const auto drive = path[0];
            if (drive.isLower()) {
                path[0] = drive.toUpper();
            }
        }

        void collapseRepeatingSeparators(QString& path) {
            static const QRegularExpression regex(R"(/+)"_l1);
            path.replace(regex, unixSeparatorString);
        }

        void dropTrailingSeparator(QString& path, bool isAbsoluteWindowsFilePath) {
            if (path.size() <= 1) return;
            if (isAbsoluteWindowsFilePath && path.size() <= minimumWindowsPathLength) return;
            if (path.back() == unixSeparatorChar) {
                path.chop(1);
            }
        }

        QString toNativeWindowsSeparators(const QString& path) {
            QString native = path;
            native.replace(unixSeparatorChar, windowsSeparatorChar);
            return native;
        }
    }

    QString normalizePath(const QString& path) {
        if (path.isEmpty()) {
            return path;
        }
        QString normalized = path.trimmed();
        if (normalized.isEmpty()) {
            return normalized;
        }
        const bool windows = isAbsoluteWindowsFilePath(normalized);
        if (windows) {
            convertFromNativeWindowsSeparators(normalized);
            capitalizeWindowsDriveLetter(normalized);
        }
        collapseRepeatingSeparators(normalized);
        dropTrailingSeparator(normalized, windows);
        return normalized;
    }

    QString toNativeSeparators(const QString& path) {
        if (!path.isEmpty() && isAbsoluteWindowsFilePath(path)) {
            return toNativeWindowsSeparators(path);
        }
        return path;
    }
}
