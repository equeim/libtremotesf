/*
 * Tremotesf
 * Copyright (C) 2015-2022 Alexey Rochev <equeim@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBTREMOTESF_PRINTLN_H
#define LIBTREMOTESF_PRINTLN_H

#include <type_traits>

#include <QMessageLogger>
#include <QString>

#include "formatters.h"

namespace libtremotesf
{
    namespace impl
    {
        inline constexpr auto printlnFormatString =
#if FMT_VERSION >= 80000
            FMT_COMPILE("{}\n");
#else
            "{}\n";
#endif

        template<class T>
        [[maybe_unused]]
        inline constexpr bool is_bounded_array_v = false;

        template<class T, std::size_t N>
        [[maybe_unused]]
        inline constexpr bool is_bounded_array_v<T[N]> = true;

        template<class T>
        inline constexpr bool is_exception_v =
                std::is_base_of_v<std::exception, T>
#ifdef Q_OS_WIN
                || std::is_base_of_v<winrt::hresult_error, T>
#endif
        ;

        struct QMessageLoggerDelegate {
            constexpr explicit QMessageLoggerDelegate(QtMsgType type, const char* fileName, int lineNumber, const char* functionName)
                : type(type),
                  context(fileName, lineNumber, functionName, "default") {}

            template<typename S, typename FirstArg, typename... Args>
            void log(S&& fmt, FirstArg&& firstArg, Args&&... args) const {
                logString(fmt::format(std::forward<S>(fmt), std::forward<FirstArg>(firstArg), std::forward<Args>(args)...));
            }

            template<typename T>
            void log(T&& value) const {
                using Type = std::decay_t<T>;
                if constexpr (std::is_same_v<Type, QString> || std::is_same_v<Type, QLatin1String>) {
                    logString(value);
                } else if constexpr (std::is_same_v<Type, const char*> || std::is_same_v<Type, char*>) {
                    using MaybeArray = std::remove_reference_t<T>;
                    if constexpr (is_bounded_array_v<MaybeArray>) {
                        logString(QString::fromUtf8(value, std::extent_v<MaybeArray> - 1));
                    } else {
                        logString(QString(value));
                    }
                } else if constexpr (
                    std::is_same_v<Type, QStringView>
#if QT_VERSION_MAJOR >= 6
                    || std::is_same_v<Type, QUtf8StringView>
                    || std::is_same_v<Type, QAnyStringView>
#endif
                ) {
                    logString(value.toString());
                } else if constexpr (std::is_same_v<Type, std::string> || std::is_same_v<Type, std::string_view>) {
                    logString(std::string_view(value));
                } else if constexpr (is_exception_v<Type>) {
                    logException<T, false>(std::forward<T>(value));
                } else {
                    logString(fmt::to_string(value));
                }
            }

            template<typename E, typename S, typename... Args>
            void logWithException(E&& e, S&& fmt, Args&&... args) const {
                static_assert(is_exception_v<std::decay_t<E>>, "First argument must be of exception type");
                log(fmt::format(std::forward<S>(fmt), std::forward<Args>(args)...));
                logException(std::forward<E>(e));
            }

        private:
            void logString(const QString& string) const {
                // We use internal qt_message_output() function here because there are only two methods
                // to output string to QMessageLogger and they have overheads that are unneccessary
                // when we are doing formatting on our own:
                // 1. QDebug marshalls everything through QTextStream
                // 2. QMessageLogger::<>(const char*, ...) overloads perform QString::vasprintf() formatting
                qt_message_output(type, context, string);
            }

            void logString(const std::string_view& string) const {
                logString(QString::fromUtf8(string.data(), static_cast<QString::size_type>(string.size())));
            }

            template<typename E, bool PrintCausedBy = true>
            void logException(E&& e) const {
                if constexpr (PrintCausedBy) {
                    log(" |- Caused by: {}", e);
                } else {
                    log(singleArgumentFormatString, e);
                }
                try {
                    std::rethrow_if_nested(e);
                } catch (const std::exception& nested) {
                    logException(nested);
                }
    #ifdef Q_OS_WIN
                catch (const winrt::hresult_error& nested) {
                    logException(nested);
                }
    #endif
                catch (...) {
                    log(" |- Caused by: unknown exception");
                }
            }

            QtMsgType type;
            QMessageLogContext context;
        };
    }

    template<typename FirstArg, typename... OtherArgs>
    void printlnStdout(FirstArg&& firstArg, OtherArgs&&... otherArgs) {
        if constexpr (sizeof...(otherArgs) == 0) {
            // Print our single argument as-is
            fmt::print(stdout, impl::printlnFormatString, std::forward<FirstArg>(firstArg));
        } else {
            // First argument must be format string, format it first
            fmt::print(stdout, impl::printlnFormatString, fmt::format(std::forward<FirstArg>(firstArg), std::forward<OtherArgs>(otherArgs)...));
        }
    }
}

namespace tremotesf
{
    using libtremotesf::printlnStdout;
}

#define QMLD(type) libtremotesf::impl::QMessageLoggerDelegate(type, QT_MESSAGELOG_FILE, QT_MESSAGELOG_LINE, QT_MESSAGELOG_FUNC)
#define logDebug(...)                QMLD(QtDebugMsg).log(__VA_ARGS__)
#define logDebugWithException(...)   QMLD(QtDebugMsg).logWithException(__VA_ARGS__)
#define logInfo(...)                 QMLD(QtInfoMsg).log(__VA_ARGS__)
#define logInfoWithException(...)    QMLD(QtInfoMsg).logWithException(__VA_ARGS__)
#define logWarning(...)              QMLD(QtWarningMsg).log(__VA_ARGS__)
#define logWarningWithException(...) QMLD(QtWarningMsg).logWithException(__VA_ARGS__)

#endif // LIBTREMOTESF_PRINTLN_H