// SPDX-FileCopyrightText: 2015-2023 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LIBTREMOTESF_FILEUTILS_H
#define LIBTREMOTESF_FILEUTILS_H

#include <span>
#include <variant>
#include <QFile>
#include <QString>

#include "formatters.h"

namespace fmt {
    template<>
    struct formatter<QFile::FileError> : libtremotesf::SimpleFormatter {
        format_context::iterator format(QFile::FileError e, format_context& ctx) FORMAT_CONST;
    };
}

namespace libtremotesf {
    class QFileError : public std::runtime_error {
    public:
        explicit QFileError(const std::string& what) : std::runtime_error(what) {}
        explicit QFileError(const char* what) : std::runtime_error(what) {}
    };

    void openFile(QFile& file, QIODevice::OpenMode mode);
    void openFileFromFd(QFile& file, int fd, QIODevice::OpenMode mode);

    void readBytes(QFile& file, std::span<char> buffer);
    void skipBytes(QFile& file, qint64 bytes);
    [[nodiscard]] std::span<char> peekBytes(QFile& file, std::span<char> buffer);
    void writeBytes(QFile& file, std::span<const char> data);

    [[nodiscard]] QByteArray readFile(const QString& path);

    void deleteFile(const QString& path);

    namespace impl {
        [[nodiscard]] QString readFileAsBase64String(QFile& file);
        [[nodiscard]] bool isTransmissionSessionIdFileExists(const QByteArray& sessionId);
    }
}

namespace tremotesf {
    using libtremotesf::QFileError;
    using libtremotesf::openFile;
    using libtremotesf::openFileFromFd;
    using libtremotesf::readBytes;
    using libtremotesf::skipBytes;
    using libtremotesf::peekBytes;
    using libtremotesf::writeBytes;
    using libtremotesf::readFile;
    using libtremotesf::deleteFile;
}

#endif // LIBTREMOTESF_FILEUTILS_H
