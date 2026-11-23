#pragma once

#include <iterator>

#include "game_state.h"

namespace ship_slots {
    inline s_ship_data Vacant(int8_t owner = 0) {
        s_ship_data ship{};
        ship.owner = owner;
        ship.status = 5;
        ship.position.Set(game::NO_SHIP_LOCATION);
        ship.officer_index = -1;
        return ship;
    }

    inline int8_t Owner(const game::s_game_state& state, int row) {
        return row < state.ship_count ? state.ship_records[row].owner : state.spare_ship_owners.at(row);
    }

    inline void Reserve(game::s_game_state& state, int row, int8_t owner) {
        if (row < state.ship_count) state.ship_records[row].owner = owner;
        else state.spare_ship_owners.at(row) = owner;
    }

    inline void Grow(game::s_game_state& state, int16_t extent) {
        if (extent <= state.ship_count) return;
        if (extent > std::ssize(state.ship_records)) state.ship_records.resize(extent);
        for (int row = state.ship_count; row < extent; ++row) {
            state.ship_records.Replace_Row(row, Vacant(state.spare_ship_owners[row]));
            state.spare_ship_owners[row] = 0;
        }
        state.ship_count = extent;
    }
}
