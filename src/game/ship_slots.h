#pragma once

#include <iterator>

#include "game_state.h"
#include "faction_ids.h"
#include "ship_ids.h"

namespace ship_slots {
    inline game::id_ship Allocate_Key(game::s_game_state& state, std::optional<game::id_player> peer) {
        // Leave the implicit keys of an imported, not-yet-keyed roster alone.
        state.next_ship_id = std::max(state.next_ship_id, static_cast<uint32_t>(std::max<int>(state.ship_count, 0)));
        return game::Allocate_Ship_Id(state, peer ? std::optional<unsigned>{peer->id} : std::nullopt);
    }

    inline s_ship_data Vacant(game::id_faction owner = {}) {
        s_ship_data ship{};
        ship.owner = owner;
        ship.status = 5;
        ship.position.Set(game::NO_SHIP_LOCATION);
        ship.officer_index = -1;
        return ship;
    }

    inline game::id_faction Owner(const game::s_game_state& state, int row) {
        return row < state.ship_count ? state.ship_records[row].owner : state.spare_ship_owners.at(row);
    }

    inline void Reserve(game::s_game_state& state, int row, game::id_faction owner) {
        if (row < state.ship_count) state.ship_records[row].owner = owner;
        else state.spare_ship_owners.at(row) = owner;
    }

    inline void Grow(game::s_game_state& state, int16_t extent) {
        if (extent <= state.ship_count) return;
        if (extent > std::ssize(state.ship_records)) state.ship_records.resize(extent);
        for (int row = state.ship_count; row < extent; ++row) {
            state.ship_records.Replace_Row(row, Vacant(state.spare_ship_owners[row]));
            state.spare_ship_owners[row] = {};
        }
        state.ship_count = extent;
    }
}
