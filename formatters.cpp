#include "formatters.h"

#include <string_view>

// If we don't include it here we will get undefined reference link error for fmt::formatter<fmt::string_view>
#if FMT_VERSION >= 80000
#include <fmt/format.h>
#endif

#include <QtGlobal>
#include <QByteArray>
#include <QLatin1String>
#include <QStringView>

#if QT_VERSION_MAJOR >= 6
#include <QAnyStringView>
#endif

#ifdef Q_OS_WIN
#include <guiddef.h>
#include <winrt/base.h>
#endif

#include "demangle.h"

namespace {
    [[maybe_unused]]
    fmt::string_view toFmtStringView(const QByteArray& str) {
        return fmt::string_view(str.data(), static_cast<size_t>(str.size()));
    }
}

auto fmt::formatter<QString>::format(const QString& string, fmt::format_context& ctx) FORMAT_CONST -> decltype(ctx.out()) {
    return fmt::formatter<string_view>::format(toFmtStringView(string.toUtf8()), ctx);
}

auto fmt::formatter<QStringView>::format(const QStringView& string, fmt::format_context& ctx) FORMAT_CONST -> decltype(ctx.out()) {
    return fmt::formatter<string_view>::format(toFmtStringView(string.toUtf8()), ctx);
}

auto fmt::formatter<QLatin1String>::format(const QLatin1String& string, fmt::format_context& ctx) FORMAT_CONST -> decltype(ctx.out()) {
    return fmt::formatter<string_view>::format(std::string_view(string.data(), static_cast<size_t>(string.size())), ctx);
}

auto fmt::formatter<QByteArray>::format(const QByteArray& array, fmt::format_context & ctx) FORMAT_CONST -> decltype(ctx.out()) {
    return fmt::formatter<string_view>::format(toFmtStringView(array), ctx);
}

#if QT_VERSION_MAJOR >= 6

auto fmt::formatter<QUtf8StringView>::format(const QUtf8StringView& string, format_context& ctx) FORMAT_CONST -> decltype(ctx.out()) {
    return fmt::formatter<string_view>::format(string_view(string.data(), static_cast<size_t>(string.size())), ctx);
}

auto fmt::formatter<QAnyStringView>::format(const QAnyStringView& string, format_context& ctx) FORMAT_CONST -> decltype(ctx.out()) {
    return fmt::formatter<QString>::format(string.toString(), ctx);
}

#endif

namespace libtremotesf::impl {
    template<typename Integer>
    auto formatQEnumImpl(const QMetaEnum& meta, Integer value, fmt::format_context& ctx) -> decltype(ctx.out()) {
        std::string unnamed{};
        const char* key = [&]() -> const char* {
            if (auto named = meta.valueToKey(static_cast<int>(value)); named) {
                return named;
            }
            unnamed = fmt::format("<unnamed value {}>", value);
            return unnamed.c_str();
        }();
        return fmt::format_to(ctx.out(), "{}::{}::{}", meta.scope(), meta.enumName(), key);
    }

    auto formatQEnum(const QMetaEnum& meta, int64_t value, fmt::format_context& ctx) -> decltype(ctx.out()) {
        return formatQEnumImpl(meta, value, ctx);
    }

    auto formatQEnum(const QMetaEnum& meta, uint64_t value, fmt::format_context& ctx) -> decltype(ctx.out()) {
        return formatQEnumImpl(meta, value, ctx);
    }
}

namespace {
    auto formatSystemError(std::string_view type, const std::system_error& e, fmt::format_context& ctx) -> decltype(ctx.out()) {
        const int code = e.code().value();
        return fmt::format_to(ctx.out(), "{}: {} (error code {} ({:#x}))", type, e.what(), code, static_cast<unsigned int>(code));
    }
}

auto fmt::formatter<std::exception>::format(const std::exception& e, fmt::format_context& ctx) FORMAT_CONST -> decltype(ctx.out()) {
    const auto type = libtremotesf::typeName(e);
    const auto what = e.what();
    if (auto s = dynamic_cast<const std::system_error*>(&e); s) {
        return formatSystemError(type, *s, ctx);
    }
    return fmt::format_to(ctx.out(), "{}: {}", type, what);
}

auto fmt::formatter<std::system_error>::format(const std::system_error& e, fmt::format_context& ctx) FORMAT_CONST -> decltype(ctx.out()) {
    return formatSystemError(libtremotesf::typeName(e), e, ctx);
}

#ifdef Q_OS_WIN

auto fmt::formatter<winrt::hstring>::format(const winrt::hstring& str, fmt::format_context& ctx) FORMAT_CONST -> decltype(ctx.out()) {
    return fmt::formatter<QString>::format(
        QString::fromWCharArray(str.data(), static_cast<QString::size_type>(str.size())),
        ctx
    );
}

auto fmt::formatter<winrt::hresult_error>::format(const winrt::hresult_error& e, fmt::format_context& ctx) FORMAT_CONST -> decltype(ctx.out()) {
    const auto code = e.code().value;
    return fmt::format_to(
        ctx.out(),
        "{}: {} (error code {} ({:#x}))",
        libtremotesf::typeName(e),
        e.message(),
        code,
        static_cast<uint32_t>(code)
    );
}

#endif // Q_OS_WIN
