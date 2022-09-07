#ifndef LIBTREMOTESF_FORMATTERS_H
#define LIBTREMOTESF_FORMATTERS_H

#include <stdexcept>
#include <type_traits>

#include <QDebug>
#include <QMetaEnum>
#include <QString>

#include <fmt/core.h>
#include <fmt/compile.h>

#include "demangle.h"

namespace libtremotesf {
    // Can't use FMT_COMPILE with fmt::print() and fmt 7

    inline constexpr auto singleArgumentFormatString =
    #if FMT_VERSION >= 80000
        FMT_COMPILE("{}");
    #else
        "{}";
    #endif

    [[maybe_unused]]
    inline std::string_view toStdStringView(const QByteArray& str) {
        return std::string_view(str.data(), static_cast<size_t>(str.size()));
    }
}

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

template<typename T>
struct fmt::formatter<T, char, std::enable_if_t<std::is_base_of_v<std::exception, T>>> {
    constexpr auto parse(fmt::format_parse_context& ctx) -> decltype(ctx.begin()) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const T& e, FormatContext& ctx) -> decltype(ctx.out()) {
        const auto type = libtremotesf::typeName(e);
        const auto what = e.what();
        if constexpr (std::is_base_of_v<std::system_error, T>) {
            return formatSystemError(type, e, ctx);
        } else {
            if (auto s = dynamic_cast<const std::system_error*>(&e); s) {
                return formatSystemError(type, *s, ctx);
            }
            return fmt::format_to(ctx.out(), "{}: {}", type, what);
        }
    }

private:
    template <typename FormatContext>
    auto formatSystemError(const std::string& type, const std::system_error& e, FormatContext& ctx) -> decltype(ctx.out()) {
        const int code = e.code().value();
        return fmt::format_to(ctx.out(), "{}: {} (error code {} ({:#x}))", type, e.what(), code, static_cast<unsigned int>(code));
    }
};

#endif // LIBTREMOTESF_FORMATTERS_H
