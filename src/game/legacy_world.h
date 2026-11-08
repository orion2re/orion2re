#pragma once

#include "world_view.h"
#include "legacy_location.h"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace legacy_world {
    game_world::s_view World();
    std::optional<uint32_t> Colony_Id(const game_world::s_view* world, const s_colony* colony);

    inline std::optional<uint32_t> Object_Id(int32_t index) {
        if (index < 0) return std::nullopt;
        return (uint32_t)index;
    }

    inline std::optional<int16_t> Index(uint32_t id) {
        if (id > INT16_MAX) return std::nullopt;
        return (int16_t)id;
    }

    inline std::optional<std::vector<int16_t>> Ship_List(std::span<const uint32_t> ids) {
        if (ids.size() > INT16_MAX) return std::nullopt;
        std::vector<int16_t> encoded;
        encoded.reserve(ids.size() + 1);
        for (uint32_t id : ids) {
            auto index = Index(id);
            if (!index) return std::nullopt;
            encoded.push_back(*index);
        }
        encoded.push_back(-1);
        return encoded;
    }

    inline std::optional<uint16_t> Travel_Turns(int32_t turns) {
        if (turns < 0) return std::nullopt;
        // Retain the legacy byte-cache conversion, including its wrap behavior.
        const uint8_t encoded = (uint8_t)turns;
        if (encoded == UINT8_MAX) return std::nullopt;
        return encoded;
    }
} // namespace legacy_world
