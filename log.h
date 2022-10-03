// SPDX-FileCopyrightText: 2015-2022 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LIBTREMOTESF_LOG_H
#define LIBTREMOTESF_LOG_H

#include <type_traits>

#include <QMessageLogger>
#include <QString>

#include "formatters.h"

#include <fmt/core.h>
#if FMT_VERSION < 80000
#define FORMAT_STRING fmt::string_view
#else
#define FORMAT_STRING fmt::format_string<Args...>
#endif

#if defined(__GNUC__)
#define ALWAYS_INLINE [[gnu::always_inline]]
#elif defined(_MSC_VER)
#define ALWAYS_INLINE __forceinline
#else
#define ALWAYS_INLINE
#endif

namespace libtremotesf
{
    namespace impl
    {
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

            void log(const QString& string) const;

            ALWAYS_INLINE void log(std::string_view string) const {
                log(QString::fromUtf8(string.data(), static_cast<QString::size_type>(string.size())));
            }
            // Needed to resolve overload resolution ambiguity since QString and std::string_view
            // are both implicitly convertible from const char*
            ALWAYS_INLINE void log(const char* string) const { log(std::string_view(string)); };

            template<typename... Args, typename = std::enable_if_t<sizeof...(Args) != 0>>
            void log(FORMAT_STRING fmt, Args&&... args) const {
                log(fmt::format(fmt, std::forward<Args>(args)...));
            }

            template<typename T, typename = std::enable_if_t<!std::is_convertible_v<T, QString> && !std::is_convertible_v<T, std::string_view>>>
            void log(const T& value) const {
                using Type = std::remove_cv_t<std::remove_reference_t<T>>;
                if constexpr (
                    std::is_same_v<Type, QStringView>
#if QT_VERSION_MAJOR >= 6
                    || std::is_same_v<Type, QUtf8StringView>
                    || std::is_same_v<Type, QAnyStringView>
#endif
                ) {
                    log(value.toString());
                } else if constexpr (is_exception_v<Type>) {
                    logExceptionRecursively<false>(value);
                } else {
                    log(singleArgumentFormatString, value);
                }
            }

            template<typename E, typename... Args, typename = std::enable_if_t<sizeof...(Args) != 0>>
            void logWithException(E&& e, FORMAT_STRING fmt, Args&&... args) const {
                static_assert(is_exception_v<std::decay_t<E>>, "First argument must be of exception type");
                log(fmt, std::forward<Args>(args)...);
                logExceptionRecursively<true>(e);
            }

            template<typename E, typename T>
            void logWithException(E&& e, const T& value) const {
                static_assert(is_exception_v<std::decay_t<E>>, "First argument must be of exception type");
                log(value);
                logExceptionRecursively<true>(e);
            }

        private:
            template<bool PrintCausedBy>
            void logExceptionRecursively(const std::exception& e) const { return logExceptionRecursivelyImpl<std::exception, PrintCausedBy>(e); }

            template<bool PrintCausedBy>
            void logExceptionRecursively(const std::system_error& e) const { return logExceptionRecursivelyImpl<std::system_error, PrintCausedBy>(e); }

#ifdef Q_OS_WIN
            template<bool PrintCausedBy>
            void logExceptionRecursively(const winrt::hresult_error& e) const { return logExceptionRecursivelyImpl<winrt::hresult_error, PrintCausedBy>(e); }
#endif

            template<typename E, bool PrintCausedBy>
            void logExceptionRecursivelyImpl(const E& e) const;

            QtMsgType type;
            QMessageLogContext context;
        };

        extern template void QMessageLoggerDelegate::logExceptionRecursivelyImpl<std::exception, true>(const std::exception&) const;
        extern template void QMessageLoggerDelegate::logExceptionRecursivelyImpl<std::exception, false>(const std::exception&) const;
        extern template void QMessageLoggerDelegate::logExceptionRecursivelyImpl<std::system_error, true>(const std::system_error&) const;
        extern template void QMessageLoggerDelegate::logExceptionRecursivelyImpl<std::system_error, false>(const std::system_error&) const;
#ifdef Q_OS_WIN
        extern template void QMessageLoggerDelegate::logExceptionRecursivelyImpl<winrt::hresult_error, true>(const winrt::hresult_error&) const;
        extern template void QMessageLoggerDelegate::logExceptionRecursivelyImpl<winrt::hresult_error, false>(const winrt::hresult_error&) const;
#endif

        inline constexpr auto printlnFormatString = "{}\n";
    }

    template<typename T>
    void printlnStdout(const T& value) {
        fmt::print(stdout, impl::printlnFormatString, value);
    }

    template<typename... Args, typename = std::enable_if_t<sizeof...(Args) != 0>>
    void printlnStdout(FORMAT_STRING fmt, Args&&... args) {
        fmt::print(stdout, impl::printlnFormatString, fmt::format(fmt, std::forward<Args>(args)...));
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

#endif // LIBTREMOTESF_LOG_H
