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
        bool isAbsoluteWindowsFilePath(const QString& path) {
            static const QRegularExpression regex(R"(^[A-Za-z]:[\\/].*$)"_l1);
            return regex.match(path).hasMatch();
        }

        QString fromNativeWindowsSeparators(const QString& path) {
            static const QRegularExpression regex(R"(\\+)"_l1);
            QString internal = path;
            internal.replace(regex, "/"_l1);
            return internal;
        }

        QString toNativeWindowsSeparators(const QString& path) {
            static const QRegularExpression regex(R"(/+)"_l1);
            QString native = path;
            native.replace(regex, R"(\)"_l1);
            return native;
        }
    }

    QString normalizePath(const QString& path) {
        QString normalized = [&] {
            if (isAbsoluteWindowsFilePath(path)) {
                return fromNativeWindowsSeparators(path);
            }
            return path;
        }();
        if (normalized.endsWith('/')) {
            normalized.chop(1);
        }
        return normalized;
    }

    QString toNativeSeparators(const QString& path) {
        if (isAbsoluteWindowsFilePath(path)) {
            return toNativeWindowsSeparators(path);
        }
        return path;
    }
}
