// SPDX-FileCopyrightText: 2015-2022 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LIBTREMOTESF_STDUTILS_H
#define LIBTREMOTESF_STDUTILS_H

#include <iterator>
#include <optional>
#include <type_traits>

#include <QtGlobal>

namespace libtremotesf {
    namespace impl {
        template<typename T>
        constexpr bool isCheapToCopy =
            std::is_trivially_copyable_v<std::decay_t<T>> && sizeof(std::decay_t<T>) <= (sizeof(void*) * 2);

        template<typename T>
        using valueOrConstRef = std::conditional_t<
            isCheapToCopy<T>,
            std::decay_t<T>,
            std::add_lvalue_reference_t<std::add_const_t<std::decay_t<T>>>>;

        template<typename Container, bool IsArray = std::is_array_v<std::remove_reference_t<Container>>>
        struct ContainerTypes {
            // Array
            using size_type = size_t;
            using value_type = std::remove_all_extents_t<Container>;
        };

        template<typename Container>
        struct ContainerTypes<Container, false> {
            // Not array
            using size_type = typename Container::size_type;
            using value_type = typename Container::value_type;
        };
    }

    template<
        typename Container,
        typename Size = typename impl::ContainerTypes<Container>::size_type,
        typename Value = impl::valueOrConstRef<typename impl::ContainerTypes<Container>::value_type>>
    inline constexpr std::optional<Size> indexOf(const Container& container, Value value) {
        const auto begin = std::begin(container);
        const auto end = std::end(container);
        const auto found = std::find(begin, end, value);
        if (found == end) {
            return std::nullopt;
        };
        return static_cast<Size>(std::distance(begin, found));
    }

    template<
        typename Index,
        typename Container,
        typename Value = impl::valueOrConstRef<typename impl::ContainerTypes<Container>::value_type>>
    inline constexpr std::optional<Index> indexOfCasted(const Container& container, Value value) {
        const auto index = indexOf<Container>(container, value);
        if (!index.has_value()) return std::nullopt;
        return static_cast<Index>(*index);
    }

    template<typename T, typename std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
    inline void setChanged(T& value, T newValue, bool& changed) {
        if (!qFuzzyCompare(newValue, value)) {
            value = newValue;
            changed = true;
        }
    }

    template<typename T, typename std::enable_if_t<!std::is_floating_point_v<T> && impl::isCheapToCopy<T>, int> = 0>
    inline void setChanged(T& value, T newValue, bool& changed) {
        if (newValue != value) {
            value = newValue;
            changed = true;
        }
    }

    template<typename T, typename std::enable_if_t<!std::is_floating_point_v<T> && !impl::isCheapToCopy<T>, int> = 0>
    inline void setChanged(T& value, T&& newValue, bool& changed) {
        if (newValue != value) {
            value = std::forward<T>(newValue);
            changed = true;
        }
    }
}

namespace tremotesf {
    using libtremotesf::indexOf;
    using libtremotesf::indexOfCasted;
    using libtremotesf::setChanged;
}

#endif // LIBTREMOTESF_STDUTILS_H
