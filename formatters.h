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
#include <QUtf8StringView>
#endif

#include <fmt/core.h>
#if FMT_VERSION < 80000
#include <fmt/format.h>
#endif

namespace libtremotesf {
    struct SimpleFormatter {
        constexpr auto parse(fmt::format_parse_context& ctx) -> decltype(ctx.begin()) {
            return ctx.begin();
        }
    };
}

template<>
struct fmt::formatter<QString> : formatter<string_view> {
    auto format(const QString& string, format_context& ctx) -> decltype(ctx.out());
};

class QStringView;
template<>
struct fmt::formatter<QStringView> : formatter<string_view> {
    auto format(const QStringView& string, format_context& ctx) -> decltype(ctx.out());
};

class QLatin1String;
template<>
struct fmt::formatter<QLatin1String> : formatter<string_view> {
    auto format(const QLatin1String& string, format_context& ctx) -> decltype(ctx.out());
};

class QByteArray;
template<>
struct fmt::formatter<QByteArray> : formatter<string_view> {
    auto format(const QByteArray& array, format_context& ctx) -> decltype(ctx.out());
};

#if QT_VERSION_MAJOR >= 6
template<>
struct fmt::formatter<QUtf8StringView> : formatter<string_view> {
    auto format(const QUtf8StringView& string, format_context& ctx) -> decltype(ctx.out());
};

class QAnyStringView;
template<>
struct fmt::formatter<QAnyStringView> : formatter<QString> {
    auto format(const QAnyStringView& string, format_context& ctx) -> decltype(ctx.out());
};
#endif

namespace libtremotesf::impl {
    inline constexpr auto singleArgumentFormatString = "{}";

    template<typename T>
    struct QDebugFormatter : SimpleFormatter {
        auto format(const T& t, fmt::format_context& ctx) -> decltype(ctx.out()) {
            QString buffer{};
            QDebug stream(&buffer);
            stream.nospace() << t;
            return fmt::format_to(ctx.out(), singleArgumentFormatString, buffer);
        }
    };

    auto formatQEnum(const QMetaEnum& meta, int64_t value, fmt::format_context& ctx) -> decltype(ctx.out());
    auto formatQEnum(const QMetaEnum& meta, uint64_t value, fmt::format_context& ctx) -> decltype(ctx.out());

    template<typename T>
    struct QEnumFormatter : SimpleFormatter {
        static_assert(std::is_enum_v<T>);

        auto format(T t, fmt::format_context& ctx) -> decltype(ctx.out()) {
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
    auto format(const T& object, fmt::format_context& ctx) -> decltype(ctx.out()) {
        QString buffer{};
        QDebug stream(&buffer);
        stream.nospace() << &object;
        return fmt::format_to(ctx.out(), libtremotesf::impl::singleArgumentFormatString, buffer);
    }
};

#define SPECIALIZE_FORMATTER_FOR_QDEBUG(Class) template<> struct fmt::formatter<Class> : libtremotesf::impl::QDebugFormatter<Class> {};

#define SPECIALIZE_FORMATTER_FOR_Q_ENUM(Enum) template<> struct fmt::formatter<Enum> : libtremotesf::impl::QEnumFormatter<Enum> {};

template<>
struct fmt::formatter<std::exception> : libtremotesf::SimpleFormatter {
    auto format(const std::exception& e, fmt::format_context& ctx) -> decltype(ctx.out());
};

template<typename T>
struct fmt::formatter<T, char, std::enable_if_t<std::is_base_of_v<std::exception, T>>> : formatter<std::exception> {};

template<>
struct fmt::formatter<std::system_error> : libtremotesf::SimpleFormatter {
    auto format(const std::system_error& e, fmt::format_context& ctx) -> decltype(ctx.out());
};

template<typename T>
struct fmt::formatter<T, char, std::enable_if_t<std::is_base_of_v<std::system_error, T>>> : formatter<std::system_error> {};

#ifdef Q_OS_WIN
namespace winrt {
    struct hstring;
    struct hresult_error;
}

template<>
struct fmt::formatter<winrt::hstring> : fmt::formatter<QString> {
    auto format(const winrt::hstring& str, fmt::format_context& ctx) -> decltype(ctx.out());
};

template<>
struct fmt::formatter<winrt::hresult_error> : libtremotesf::SimpleFormatter {
    auto format(const winrt::hresult_error& e, fmt::format_context& ctx) -> decltype(ctx.out());
};
#endif

#endif // LIBTREMOTESF_FORMATTERS_H
