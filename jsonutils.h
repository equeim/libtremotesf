// SPDX-FileCopyrightText: 2015-2022 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LIBTREMOTESF_JSONUTILS_H
#define LIBTREMOTESF_JSONUTILS_H

#include <array>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include <QJsonArray>
#include <QJsonValue>

#include "log.h"

SPECIALIZE_FORMATTER_FOR_QDEBUG(QJsonValue)

namespace libtremotesf::impl {
    template<typename EnumType, typename JsonType>
    struct EnumMapping {
        constexpr explicit EnumMapping(EnumType enumValue, JsonType jsonValue)
            : enumValue(enumValue), jsonValue(jsonValue) {}
        EnumType enumValue{};
        JsonType jsonValue{};
    };

    template<typename EnumType, size_t EnumCount, typename JsonType>
    struct EnumMapper {
        constexpr explicit EnumMapper(std::array<EnumMapping<EnumType, JsonType>, EnumCount>&& mappings)
            : mappings(std::move(mappings)) {}

        EnumType fromJsonValue(const QJsonValue& value, QLatin1String key) const {
            const auto jsonValue = [&] {
                if constexpr (std::is_same_v<JsonType, int>) {
                    if (!value.isDouble()) {
                        return std::optional<int>{};
                    }
                    return std::optional(value.toInt());
                } else if constexpr (std::is_same_v<JsonType, QLatin1String>) {
                    if (!value.isString()) {
                        return std::optional<QString>{};
                    }
                    return std::optional(value.toString());
                }
            }();
            if (!jsonValue.has_value()) {
                logWarning("Unknown {} value {}", key, value);
                return {};
            }
            const auto found = std::find_if(mappings.begin(), mappings.end(), [&](const auto& mapping) {
                return mapping.jsonValue == jsonValue;
            });
            if (found == mappings.end()) {
                logWarning("Unknown {} value {}", key, *jsonValue);
                return {};
            }
            return found->enumValue;
        }

        JsonType toJsonValue(EnumType value) const {
            const auto found = std::find_if(mappings.begin(), mappings.end(), [value](const auto& mapping) {
                return mapping.enumValue == value;
            });
            if (found == mappings.end()) {
                throw std::logic_error(fmt::format("Unknown enum value {}", value));
            }
            return found->jsonValue;
        }

    private:
        std::array<EnumMapping<EnumType, JsonType>, EnumCount> mappings{};
    };

    inline QJsonArray toJsonArray(const std::vector<int>& ids) {
        QJsonArray array{};
        std::copy(ids.begin(), ids.end(), std::back_inserter(array));
        return array;
    }
}

#endif // LIBTREMOTESF_JSONUTILS_H
