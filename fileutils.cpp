// SPDX-FileCopyrightText: 2015-2023 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fileutils.h"

#include <span>
#include <stdexcept>

#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QStringBuilder>

#include "literals.h"
#include "log.h"
#include "target_os.h"

namespace libtremotesf::impl {
    QString readFileAsBase64String(QFile& file) {
        QString string{};
        string.reserve(static_cast<QString::size_type>(((4 * file.size() / 3) + 3) & ~3));

        static constexpr qint64 bufferSize = 1024 * 1024 - 1; // 1 MiB minus 1 byte (dividable by 3)
        QByteArray buffer{};
        buffer.resize(static_cast<QByteArray::size_type>(std::min(bufferSize, file.size())));

        std::span<char> emptyBufferRemainder = buffer;
        while (true) {
            const qint64 bytes =
                file.read(emptyBufferRemainder.data(), static_cast<qint64>(emptyBufferRemainder.size()));
            if (bytes == -1) {
                throw std::runtime_error(fmt::format(
                    "{} (QFileDevice::FileError {})",
                    file.errorString(),
                    static_cast<std::underlying_type_t<QFile::FileError>>(file.error())
                ));
            }
            if (bytes == 0) {
                // End of file
                const auto filledBufferSize =
                    buffer.size() - static_cast<QByteArray::size_type>(emptyBufferRemainder.size());
                if (filledBufferSize > 0) {
                    buffer.resize(filledBufferSize);
                    string.append(QLatin1String(buffer.toBase64()));
                }
                return string;
            }
            if (bytes == static_cast<qint64>(emptyBufferRemainder.size())) {
                // Read whole buffer
                string.append(QLatin1String(buffer.toBase64()));
                emptyBufferRemainder = buffer;
            } else {
                // Read part of buffer
                emptyBufferRemainder = emptyBufferRemainder.subspan(static_cast<size_t>(bytes));
            }
        }
    }

    namespace {
        constexpr auto sessionIdFileLocation = [] {
            if constexpr (isTargetOsWindows) {
                return QStandardPaths::GenericDataLocation;
            } else {
                return QStandardPaths::TempLocation;
            }
        }();

        constexpr QLatin1String sessionIdFilePrefix = [] {
            if constexpr (isTargetOsWindows) {
                return "Transmission/tr_session_id_"_l1;
            } else {
                return "tr_session_id_"_l1;
            }
        }();
    }

    bool isTransmissionSessionIdFileExists(const QByteArray& sessionId) {
        if constexpr (targetOs != TargetOs::UnixAndroid) {
            const auto file = QStandardPaths::locate(sessionIdFileLocation, sessionIdFilePrefix % sessionId);
            if (!file.isEmpty()) {
                logInfo(
                    "isSessionIdFileExists: found transmission-daemon session id file {}",
                    QDir::toNativeSeparators(file)
                );
                return true;
            }
            logInfo("isSessionIdFileExists: did not find transmission-daemon session id file");
        }
        return false;
    }

    QString fileNameOrHandle(const QFile& file) {
        if (auto fileName = file.fileName(); !fileName.isEmpty()) {
            return fileName;
        }
        return QString::fromLatin1("handle=%1").arg(file.handle());
    }
}
