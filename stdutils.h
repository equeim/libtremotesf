// SPDX-FileCopyrightText: 2015-2022 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LIBTREMOTESF_STDUTILS_H
#define LIBTREMOTESF_STDUTILS_H

#include <iterator>
#include <type_traits>

#include <QtGlobal>

namespace libtremotesf
{
    template<typename C, typename V>
    inline int index_of_i(const C& container, const V& value) {
        return static_cast<int>(std::find(std::begin(container), std::end(container), value) - std::begin(container));
    }

    template<typename T, typename std::enable_if_t<std::is_scalar_v<T> && !std::is_floating_point_v<T>, int> = 0>
    inline void setChanged(T& value, T newValue, bool& changed)
    {
        if (newValue != value) {
            value = newValue;
            changed = true;
        }
    }

    template<typename T, typename std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
    inline void setChanged(T& value, T newValue, bool& changed)
    {
        if (!qFuzzyCompare(newValue, value)) {
            value = newValue;
            changed = true;
        }
    }

    template<typename T, typename std::enable_if_t<!std::is_scalar_v<T>, int> = 0>
    inline void setChanged(T& value, T&& newValue, bool& changed)
    {
        if (newValue != value) {
            value = std::forward<T>(newValue);
            changed = true;
        }
    }

    // QLatin1String(const char*) is not guaranteed to be constexpr in Qt < 6.4 since it uses strlen()
    // Add user-defined literal to circumvent this (and it's nicer to use!)
    inline constexpr QLatin1String operator ""_l1(const char* str, size_t length) {
        return QLatin1String(str, static_cast<QLatin1String::size_type>(length));
    }
}

namespace tremotesf
{
    using libtremotesf::index_of_i;
    using libtremotesf::setChanged;
    using libtremotesf::operator ""_l1;
}

#endif // LIBTREMOTESF_STDUTILS_H
