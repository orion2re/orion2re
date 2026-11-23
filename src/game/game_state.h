#pragma once

#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <vector>

#include "colony_data.h"
#include "consts.h"
#include "leader_data.h"
#include "object_ids.h"
#include "planet_data.h"
#include "player_data.h"
#include "ship_roster.h"
#include "star_data.h"

namespace game {
    struct s_game_state {
        // Slots stay assigned even when a participant is eliminated.
        std::array<s_player, MAX_PLAYERS> players = player_roster::Make_Roster();
        int16_t player_count = 0;

        // A leader's ID is its fixed catalog slot, including inactive leaders.
        std::array<s_leader_data, MAX_LEADERS> leaders = [] {
            std::array<s_leader_data, MAX_LEADERS> catalog{};
            for (uint32_t i = 0; i < catalog.size(); ++i) catalog[i].id = id_leader{i};
            return catalog;
        }();

        std::map<id_star, s_star_data> stars;
        // Ordered projection for positional tables and transitional interfaces.
        // Rebuilding it does not change the identity of any live record.
        std::vector<id_star> star_order;
        bool star_order_sorted = true;
        uint32_t next_star_id = 0;
        // The legacy ship destination encoding gives Antares a numeric slot,
        // but Antares is not a galaxy star and does not get a canonical ID.
        s_star_data antares_destination{};

        std::map<id_planet, s_planet_data> planets;
        // Traversal order only; relationships and lookups use the map key.
        std::vector<id_planet> planet_order;
        uint32_t next_planet_id = 0;

        std::map<id_colony, s_colony> colonies;
        uint32_t next_colony_id = 0;
        // Traversal order only; relationships and lookups use the map key.
        std::array<std::vector<id_colony>, MAX_PLAYERS> colony_reservations;

        s_ship_roster ship_records;
        // Extent of the legacy row projection, including retired ships.
        int16_t ship_count = 0;
        // Reservations for slots at or beyond ship_count, not ship records.
        std::array<id_faction, MAX_SHIPS> spare_ship_owners{};
        uint32_t next_ship_id = 0;
        std::array<uint64_t, MAX_SHIPS> ship_generations{};

        bool operator==(const s_game_state&) const = default;
    };

    inline s_game_state current_state;
} // namespace game
