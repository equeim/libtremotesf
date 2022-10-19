// SPDX-FileCopyrightText: 2015-2022 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LIBTREMOTESF_FORMATTERS_H
#define LIBTREMOTESF_FORMATTERS_H

#include <stdexcept>
#include <system_error>
#include <type_traits>

#include <QDebug>
#include <QMetaEnum>
#include <QObject>
#include <QString>
#if QT_VERSION_MAJOR >= 6
#    include <QUtf8StringView>
#endif

#include <fmt/core.h>
#if FMT_VERSION < 80000
#    include <fmt/format.h>
#    define FORMAT_CONST
#else
#    define FORMAT_CONST const
#endif

// Include it here just because it's useful
#include "literals.h"

namespace libtremotesf {
    struct SimpleFormatter {
        constexpr fmt::format_parse_context::iterator parse(fmt::format_parse_context& ctx) { return ctx.begin(); }
    };
}

template<>
struct fmt::formatter<QString> : formatter<string_view> {
    format_context::iterator format(const QString& string, format_context& ctx) FORMAT_CONST;
};

class QStringView;
template<>
struct fmt::formatter<QStringView> : formatter<string_view> {
    format_context::iterator format(const QStringView& string, format_context& ctx) FORMAT_CONST;
};

class QLatin1String;
template<>
struct fmt::formatter<QLatin1String> : formatter<string_view> {
    format_context::iterator format(const QLatin1String& string, format_context& ctx) FORMAT_CONST;
};

class QByteArray;
template<>
struct fmt::formatter<QByteArray> : formatter<string_view> {
    format_context::iterator format(const QByteArray& array, format_context& ctx) FORMAT_CONST;
};

#if QT_VERSION_MAJOR >= 6
template<>
struct fmt::formatter<QUtf8StringView> : formatter<string_view> {
    format_context::iterator format(const QUtf8StringView& string, format_context& ctx) FORMAT_CONST;
};

class QAnyStringView;
template<>
struct fmt::formatter<QAnyStringView> : formatter<QString> {
    format_context::iterator format(const QAnyStringView& string, format_context& ctx) FORMAT_CONST;
};
#endif

namespace libtremotesf::impl {
    inline constexpr auto singleArgumentFormatString = "{}";

    template<typename T>
    struct QDebugFormatter : SimpleFormatter {
        fmt::format_context::iterator format(const T& t, fmt::format_context& ctx) FORMAT_CONST {
            QString buffer{};
            QDebug stream(&buffer);
            stream.nospace() << t;
            return fmt::format_to(ctx.out(), singleArgumentFormatString, buffer);
        }
    };

    fmt::format_context::iterator formatQEnum(const QMetaEnum& meta, int64_t value, fmt::format_context& ctx);
    fmt::format_context::iterator formatQEnum(const QMetaEnum& meta, uint64_t value, fmt::format_context& ctx);

    template<typename T>
    struct QEnumFormatter : SimpleFormatter {
        static_assert(std::is_enum_v<T>);

        fmt::format_context::iterator format(T t, fmt::format_context& ctx) FORMAT_CONST {
            const auto meta = QMetaEnum::fromType<T>();
            const auto underlying = static_cast<std::underlying_type_t<T>>(t);
            if constexpr (std::is_signed_v<decltype(underlying)>) {
                return formatQEnum(meta, static_cast<int64_t>(underlying), ctx);
            } else {
                return formatQEnum(meta, static_cast<uint64_t>(underlying), ctx);
            }
        }
    };
}

template<typename T>
struct fmt::formatter<T, char, std::enable_if_t<std::is_base_of_v<QObject, T>>> : libtremotesf::SimpleFormatter {
    format_context::iterator format(const T& object, format_context& ctx) FORMAT_CONST {
        QString buffer{};
        QDebug stream(&buffer);
        stream.nospace() << &object;
        return fmt::format_to(ctx.out(), libtremotesf::impl::singleArgumentFormatString, buffer);
    }
};

#define SPECIALIZE_FORMATTER_FOR_QDEBUG(Class) \
    template<>                                 \
    struct fmt::formatter<Class> : libtremotesf::impl::QDebugFormatter<Class> {};

#define SPECIALIZE_FORMATTER_FOR_Q_ENUM(Enum) \
    template<>                                \
    struct fmt::formatter<Enum> : libtremotesf::impl::QEnumFormatter<Enum> {};

template<>
struct fmt::formatter<std::exception> : libtremotesf::SimpleFormatter {
    format_context::iterator format(const std::exception& e, fmt::format_context& ctx) FORMAT_CONST;
};

template<typename T>
struct fmt::formatter<T, char, std::enable_if_t<std::is_base_of_v<std::exception, T>>> : formatter<std::exception> {};

template<>
struct fmt::formatter<std::system_error> : libtremotesf::SimpleFormatter {
    format_context::iterator format(const std::system_error& e, fmt::format_context& ctx) FORMAT_CONST;
};

template<typename T>
struct fmt::formatter<T, char, std::enable_if_t<std::is_base_of_v<std::system_error, T>>>
    : formatter<std::system_error> {};

#ifdef Q_OS_WIN
namespace winrt {
    struct hstring;
    struct hresult_error;
}

template<>
struct fmt::formatter<winrt::hstring> : fmt::formatter<QString> {
    format_context::iterator format(const winrt::hstring& str, fmt::format_context& ctx) FORMAT_CONST;
};

template<>
struct fmt::formatter<winrt::hresult_error> : libtremotesf::SimpleFormatter {
    format_context::iterator format(const winrt::hresult_error& e, fmt::format_context& ctx) FORMAT_CONST;
};
#endif

#endif // LIBTREMOTESF_FORMATTERS_H
