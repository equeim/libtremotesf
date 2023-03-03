// SPDX-FileCopyrightText: 2015-2022 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LIBTREMOTESF_STDUTILS_H
#define LIBTREMOTESF_STDUTILS_H

#include <algorithm>
#include <concepts>
#include <optional>
#include <ranges>
#include <type_traits>

#include <QtGlobal>

namespace libtremotesf {
    namespace impl {
        template<typename T>
        concept CheapToCopy = std::is_trivially_copyable_v<std::decay_t<T>> &&
                              sizeof(std::decay_t<T>) <= (sizeof(void*) * 2);

        template<typename T>
        using ValueOrConstRef = std::conditional_t<
            CheapToCopy<T>,
            std::decay_t<T>,
            std::add_lvalue_reference_t<std::add_const_t<std::decay_t<T>>>>;

        template<typename T>
        concept HasReserveFunction =
            requires(T container, std::ranges::range_size_t<T> size) { container.reserve(size); };

        template<typename T>
        concept HasPushBackFunction =
            requires(T container, std::ranges::range_reference_t<T> item) { container.push_back(item); };

        template<typename T>
        concept HasInsertFunction =
            requires(T container, std::ranges::range_reference_t<T> item) { container.insert(item); };
    }

    template<std::ranges::random_access_range Range>
    inline constexpr std::optional<std::ranges::range_size_t<Range>>
    indexOf(const Range& range, std::ranges::range_value_t<Range> value) {
        namespace r = std::ranges;
        const auto found = std::find(r::begin(range), r::end(range), value);
        if (found == r::end(range)) {
            return std::nullopt;
        };
        return static_cast<r::range_size_t<Range>>(std::distance(r::begin(range), found));
    }

    template<std::integral Index, std::ranges::random_access_range Range>
    inline constexpr std::optional<Index> indexOfCasted(const Range& range, std::ranges::range_value_t<Range> value) {
        const auto index = indexOf(range, value);
        if (!index.has_value()) return std::nullopt;
        return static_cast<Index>(*index);
    }

    template<
        std::default_initializable NewContainer,
        std::ranges::forward_range FromRange,
        std::invocable<std::ranges::range_reference_t<FromRange>> Transform>
        requires(impl::HasPushBackFunction<NewContainer> || impl::HasInsertFunction<NewContainer>)
    inline NewContainer createTransforming(FromRange&& from, Transform&& transform) {
        NewContainer container{};
        if constexpr (std::ranges::sized_range<FromRange> && impl::HasReserveFunction<NewContainer>) {
            container.reserve(static_cast<std::ranges::range_size_t<NewContainer>>(std::ranges::size(from)));
        }
        auto outputIterator = [&] {
            if constexpr (impl::HasPushBackFunction<NewContainer>) {
                return std::back_insert_iterator(container);
            } else if constexpr (impl::HasInsertFunction<NewContainer>) {
                return std::insert_iterator(container);
            }
        }();
        if constexpr (std::is_rvalue_reference_v<decltype(from)> && !std::is_const_v<std::remove_reference_t<decltype(from)>>) {
            std::transform(
                std::move_iterator(std::ranges::begin(from)),
                std::move_iterator(std::ranges::end(from)),
                outputIterator,
                std::forward<Transform>(transform)
            );
        } else {
            std::transform(
                std::ranges::begin(from),
                std::ranges::end(from),
                outputIterator,
                std::forward<Transform>(transform)
            );
        }
        return container;
    }

    template<std::regular T>
    inline void setChanged(T& value, impl::ValueOrConstRef<T> newValue, bool& changed) {
        if (newValue != value) {
            value = std::forward<decltype(newValue)>(newValue);
            changed = true;
        }
    }

    template<std::floating_point T>
    inline void setChanged(T& value, T newValue, bool& changed) {
        if (!qFuzzyCompare(newValue, value)) {
            value = newValue;
            changed = true;
        }
    }
}

namespace tremotesf {
    using libtremotesf::createTransforming;
    using libtremotesf::indexOf;
    using libtremotesf::indexOfCasted;
    using libtremotesf::setChanged;
}

#endif // LIBTREMOTESF_STDUTILS_H
