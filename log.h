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

#include <fmt/core.h>
#include <fmt/compile.h>
#include <fmt/format.h>

#include <QDebug>
#include <QMessageLogger>
#include <QMetaEnum>
#include <QString>

namespace libtremotesf
{
    namespace
    {
        // Can't use FMT_COMPILE with fmt::print() and fmt 7

        inline constexpr auto singleArgumentFormatString =
#if FMT_VERSION >= 80000
            FMT_COMPILE("{}");
#else
            "{}";
#endif

        inline constexpr auto printlnFormatString =
#if FMT_VERSION >= 80000
            FMT_COMPILE("{}\n");
#else
            "{}\n";
#endif

        template<class T>
        inline constexpr bool is_bounded_array_v = false;

        template<class T, std::size_t N>
        inline constexpr bool is_bounded_array_v<T[N]> = true;

        [[maybe_unused]]
        inline std::string_view toStdStringView(const QByteArray& str) {
            return std::string_view(str.data(), static_cast<size_t>(str.size()));
        }
    }

    template<typename FirstArg, typename... OtherArgs>
    void printlnStdout(FirstArg&& firstArg, OtherArgs&&... otherArgs) {
        if constexpr (sizeof...(otherArgs) == 0) {
            // Print our single argument as-is
            fmt::print(stdout, printlnFormatString, std::forward<FirstArg>(firstArg));
        } else {
            // First argument must be format string, format it first
            fmt::print(stdout, printlnFormatString, fmt::format(std::forward<FirstArg>(firstArg), std::forward<OtherArgs>(otherArgs)...));
        }
    }

    struct QMessageLoggerCallable {
        constexpr explicit QMessageLoggerCallable(QtMsgType type, const char* fileName, int lineNumber, const char* functionName)
            : type(type),
              context(fileName, lineNumber, functionName, "default") {}

        template<typename S, typename FirstArg, typename... Args>
        void operator()(S&& fmt, FirstArg&& firstArg, Args&&... args) const {
            log(fmt::format(std::forward<S>(fmt), std::forward<FirstArg>(firstArg), std::forward<Args>(args)...));
        }

        template<typename T>
        void operator()(T&& value) const {
            using Type = std::decay_t<T>;
            if constexpr (std::is_same_v<Type, QString> || std::is_same_v<Type, QLatin1String>) {
                log(value);
            } else if constexpr (std::is_same_v<Type, const char*> || std::is_same_v<Type, char*>) {
                using MaybeArray = std::remove_reference_t<T>;
                if constexpr (is_bounded_array_v<MaybeArray>) {
                    log(QString::fromUtf8(value, std::extent_v<MaybeArray> - 1));
                } else {
                    log(QString(value));
                }
            } else if constexpr (
                std::is_same_v<Type, QStringView>
#if QT_VERSION_MAJOR >= 6
                || std::is_same_v<Type, QUtf8StringView>
                || std::is_same_v<Type, QAnyStringView>
#endif
            ) {
                log(value.toString());
            } else if constexpr (std::is_same_v<Type, std::string> || std::is_same_v<Type, std::string_view>) {
                log(std::string_view(value));
            } else {
                log(fmt::to_string(value));
            }
        }

    private:
        void log(const QString& string) const {
            // We use internal qt_message_output() function here because there are only two methods
            // to output string to QMessageLogger and they have overheads that are unneccessary
            // when we are doing formatting on our own:
            // 1. QDebug marshalls everything through QTextStream
            // 2. QMessageLogger::<>(const char*, ...) overloads perform QString::vasprintf() formatting
            qt_message_output(type, context, string);
        }

        void log(const std::string_view& string) const {
            log(QString::fromUtf8(string.data(), static_cast<QString::size_type>(string.size())));
        }

        QtMsgType type;
        QMessageLogContext context;
    };
}

namespace tremotesf
{
    using libtremotesf::printlnStdout;
}

#define logDebug   libtremotesf::QMessageLoggerCallable(QtDebugMsg,   QT_MESSAGELOG_FILE, QT_MESSAGELOG_LINE, QT_MESSAGELOG_FUNC)
#define logInfo    libtremotesf::QMessageLoggerCallable(QtInfoMsg,    QT_MESSAGELOG_FILE, QT_MESSAGELOG_LINE, QT_MESSAGELOG_FUNC)
#define logWarning libtremotesf::QMessageLoggerCallable(QtWarningMsg, QT_MESSAGELOG_FILE, QT_MESSAGELOG_LINE, QT_MESSAGELOG_FUNC)

template<>
struct fmt::formatter<QString> : fmt::formatter<std::string_view> {
    template<typename FormatContext>
    auto format(const QString& string, FormatContext& ctx) -> decltype(ctx.out()) {
        return fmt::formatter<std::string_view>::format(libtremotesf::toStdStringView(string.toUtf8()), ctx);
    }
};

template<>
struct fmt::formatter<QStringView> : fmt::formatter<std::string_view> {
    template<typename FormatContext>
    auto format(const QStringView& string, FormatContext& ctx) -> decltype(ctx.out()) {
        return fmt::formatter<std::string_view>::format(libtremotesf::toStdStringView(string.toUtf8()), ctx);
    }
};

template<>
struct fmt::formatter<QLatin1String> : fmt::formatter<std::string_view> {
    template<typename FormatContext>
    auto format(const QLatin1String& string, FormatContext& ctx) -> decltype(ctx.out()) {
        return fmt::formatter<std::string_view>::format(std::string_view(string.data(), static_cast<size_t>(string.size())), ctx);
    }
};

template<>
struct fmt::formatter<QByteArray> : fmt::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(const QByteArray& array, FormatContext& ctx) -> decltype(ctx.out()) {
        return fmt::formatter<std::string_view>::format(libtremotesf::toStdStringView(array), ctx);
    }
};

#if QT_VERSION_MAJOR >= 6
template<>
struct fmt::formatter<QUtf8StringView> : fmt::formatter<std::string_view> {
    template<typename FormatContext>
    auto format(const QUtf8StringView& string, FormatContext& ctx) -> decltype(ctx.out()) {
        return fmt::formatter<std::string_view>::format(std::string_view(string.data(), static_cast<size_t>(string.size())), ctx);
    }
};

template<>
struct fmt::formatter<QAnyStringView> : fmt::formatter<QString> {
    template<typename FormatContext>
    auto format(const QAnyStringView& string, FormatContext& ctx) -> decltype(ctx.out()) {
        return fmt::formatter<QString>::format(string.toString(), ctx);
    }
};
#endif

namespace libtremotesf {
    template<typename T>
    struct QDebugFormatter {
        constexpr auto parse(fmt::format_parse_context& ctx) -> decltype(ctx.begin()) {
            return ctx.begin();
        }

        template<typename FormatContext>
        auto format(const T& t, FormatContext& ctx) -> decltype(ctx.out()) {
            QString buffer{};
            QDebug stream(&buffer);
            stream.nospace() << t;
            return fmt::format_to(ctx.out(), singleArgumentFormatString, buffer);
        }
    };

    template<typename T>
    struct QEnumFormatter {
        static_assert(std::is_enum_v<T>);

        constexpr auto parse(fmt::format_parse_context& ctx) -> decltype(ctx.begin()) {
            return ctx.begin();
        }

        template<typename FormatContext>
        auto format(const T& t, FormatContext& ctx) -> decltype(ctx.out()) {
            const auto meta = QMetaEnum::fromType<T>();
            std::string unnamed{};
            const char* key = [&]() -> const char* {
                if (auto named = meta.valueToKey(static_cast<int>(t)); named) {
                    return named;
                }
                unnamed = fmt::format("<unnamed value {}>", static_cast<std::underlying_type_t<T>>(t));
                return unnamed.c_str();
            }();
            return fmt::format_to(ctx.out(), "{}::{}::{}", meta.scope(), meta.enumName(), key);
        }
    };
}

template<typename T>
struct fmt::formatter<T, char, std::enable_if_t<std::is_base_of_v<QObject, T>>> {
    constexpr auto parse(fmt::format_parse_context& ctx) -> decltype(ctx.begin()) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const QObject& object, FormatContext& ctx) -> decltype(ctx.out()) {
        QString buffer{};
        QDebug stream(&buffer);
        stream.nospace() << &object;
        return fmt::format_to(ctx.out(), libtremotesf::singleArgumentFormatString, buffer);
    }
};

#define SPECIALIZE_FORMATTER_FOR_QDEBUG(Class) template<> struct fmt::formatter<Class> : libtremotesf::QDebugFormatter<Class> {};

#define SPECIALIZE_FORMATTER_FOR_Q_ENUM(Enum) template<> struct fmt::formatter<Enum> : libtremotesf::QEnumFormatter<Enum> {};

#endif // LIBTREMOTESF_PRINTLN_H
