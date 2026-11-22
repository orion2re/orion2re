#pragma once

#include "world_view.h"
#include "legacy_location.h"
#include "legacy_ids.h"
#include "ship_ids.h"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace legacy_world {
    game::s_view World();

    inline game::id_ship Allocate_Live_Ship_Id(game::s_game_state& state, int game_type, int local_player) {
        // Leave the implicit keys of an imported, not-yet-keyed roster alone.
        state.next_ship_id = std::max(state.next_ship_id, static_cast<uint32_t>(std::max<int>(state.ship_count, 0)));
        if (game_type == 2 || game_type == 3) {
            return game::Allocate_Ship_Id(state, static_cast<unsigned>(local_player));
        }
        return game::Allocate_Ship_Id(state);
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
