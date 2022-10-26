// SPDX-FileCopyrightText: 2015-2022 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "pathutils.h"
#include "rpc.h"

namespace libtremotesf {
    namespace {
        QString fromNativeRemoteSeparators(const QString& path, const Rpc* rpc) {
            if (!rpc->isServerRunningOnWindows()) return path;
            QString internal = path;
            internal.replace('\\', '/');
            return internal;
        }
    }

    QString normalizeRemotePath(const QString& path, const Rpc* rpc) {
        // In case of Windows, Transmission may return paths with both \ and / separators over RPC
        // Normalize them to /
        auto normalized = fromNativeRemoteSeparators(path, rpc);
        if (normalized.endsWith('/')) {
            normalized.chop(1);
        }
        return normalized;
    }

    QString toNativeRemoteSeparators(const QString& path, const Rpc* rpc) {
        if (!rpc->isServerRunningOnWindows()) return path;
        QString native = path;
        native.replace('/', '\\');
        return native;
    }
}
