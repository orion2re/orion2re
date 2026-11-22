#pragma once

#include <iterator>
#include "game_state.h"
#include "ship_location.h"
#include "star_destination.h"

namespace game {
    // Rows belong to ordered UI and bitmap tables, not to the star records.
    inline id_star Star_Key_At_Row(const s_game_state& state, int row) {
        return row >= 0 && row < std::ssize(state.star_order) ? state.star_order[row] : NO_STAR;
    }

    inline int Star_Row(const s_game_state& state, id_star key) {
        const auto& order = state.star_order;
        const auto it = state.star_order_sorted ? std::lower_bound(order.begin(), order.end(), key)
                                                : std::find(order.begin(), order.end(), key);
        return it != order.end() && *it == key ? static_cast<int>(it - order.begin()) : -1;
    }

    inline std::optional<id_star> Find_Star_At_Coordinates(const s_game_state& state, int16_t x, int16_t y) {
        for (const auto& [key, star] : state.stars) {
            if (star.x == x && star.y == y) return key;
        }
        return std::nullopt;
    }

    inline int Leader_Assignment_Row(const s_game_state& state, const s_leader_data& leader) {
        return leader.star_location == NO_STAR ? leader.location : Star_Row(state, leader.star_location);
    }

    template<class State>
    inline auto& Destination_Record_At_Row(State& state, int row) {
        return row == ANTARES_POSITION ? state.antares_destination : state.stars.at(Star_Key_At_Row(state, row));
    }

    inline s_star_destination Destination_At_Row(const s_game_state& state, int row) {
        if (row == ANTARES_POSITION) return {e_star_destination::antares, NO_STAR};
        const auto key = Star_Key_At_Row(state, row);
        return key == NO_STAR ? s_star_destination{} : s_star_destination{e_star_destination::star, key};
    }

    inline int Destination_Row(const s_game_state& state, const s_star_destination& destination) {
        if (destination.kind == e_star_destination::antares) return ANTARES_POSITION;
        return destination.kind == e_star_destination::star ? Star_Row(state, destination.star_id.get()) : -1;
    }

    inline int Ship_Target_Row(const s_game_state& state, const s_stored_ship_location& location, int antares_row = ANTARES_POSITION) {
        if (location.target == e_ship_target::antares) return antares_row;
        return location.target == e_ship_target::star ? Star_Row(state, location.star_id.get()) : -1;
    }

    inline int Ship_Orbit_Row(const s_game_state& state, const s_stored_ship_location& location, int antares_row = ANTARES_POSITION) {
        return location.kind == e_ship_location::at_star ? Ship_Target_Row(state, location, antares_row) : -1;
    }

    inline void Set_Ship_Orbit_At_Row(const s_game_state& state, s_stored_ship_location& location, int row, int antares_row = ANTARES_POSITION) {
        if (row >= 0 && row == antares_row) {
            location.Set({e_ship_location::at_star, NO_STAR, e_ship_target::antares});
        } else {
            const auto key = Star_Key_At_Row(state, row);
            location.Set(key == NO_STAR ? NO_SHIP_LOCATION : s_ship_location{e_ship_location::at_star, key});
        }
    }

    inline bool Has_Other_Colonies(const s_star_data& star, unsigned player_slot) {
        return (star.has_colony & ~(1u << player_slot)) != 0;
    }

    inline bool Has_Player_Colony(const s_star_data& star, unsigned player_slot) {
        return (star.has_colony & (1u << player_slot)) != 0;
    }

    inline bool Ignores_Combat_Ships(const s_star_data& star, unsigned player_slot) {
        return (star.ignore_combat_ships & (1u << player_slot)) != 0;
    }
}
