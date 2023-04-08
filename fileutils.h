// SPDX-FileCopyrightText: 2015-2023 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LIBTREMOTESF_FILEUTILS_H
#define LIBTREMOTESF_FILEUTILS_H

#include <QString>

class QFile;

namespace libtremotesf::impl {
    [[nodiscard]] QString readFileAsBase64String(QFile& file);

    [[nodiscard]] bool isTransmissionSessionIdFileExists(const QByteArray& sessionId);

    [[nodiscard]] QString fileNameOrHandle(const QFile& file);
}

#endif // LIBTREMOTESF_FILEUTILS_H
