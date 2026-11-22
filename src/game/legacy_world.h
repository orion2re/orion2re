#pragma once

#include "world_view.h"
#include "legacy_location.h"
#include "legacy_ids.h"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace legacy_world {
    game::s_view World();

    // Phase-one bridge for movement and combat, which still encode Antares
    // as the legacy star count. Ordinary star lookups use the roster directly.
    template<class State>
    inline auto& Star_Or_Antares(State& state, int16_t location, int16_t antares_slot) {
        return location == antares_slot ? state.antares_destination : state.stars.at(state.Star_Id_At_Position(location));
    }

    inline std::optional<uint32_t> Nonnegative_Slot(int32_t index) {
        if (index < 0) return std::nullopt;
        return (uint32_t)index;
    }

    inline std::optional<std::vector<int16_t>> Ship_List(std::span<const game::id_ship> ids) {
        if (ids.size() > INT16_MAX) return std::nullopt;
        std::vector<int16_t> encoded;
        encoded.reserve(ids.size() + 1);
        for (game::id_ship id : ids) {
            if (id.id > INT16_MAX) return std::nullopt;
            encoded.push_back(id.id);
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
