#pragma once

#include <algorithm>
#include <map>
#include <optional>
#include <span>
#include <vector>

#include "legacy_sort.h"
#include "star_queries.h"
#include "ship_ids.h"

namespace game {
    struct s_combat_ship_order {
        s_star_destination destination;
        std::vector<id_ship> keys;
        std::map<id_ship, int16_t> ranks;
    };

    // A guest uses the host's ordering for this battle, without changing its
    // own strategic row projection. Cleared when leaving the combat phase.
    inline std::optional<s_combat_ship_order> received_combat_order;

    inline const s_combat_ship_order* Received_Combat_Order(const s_game_state& state, int star_row) {
        return &state == &current_state && received_combat_order &&
            received_combat_order->destination == Destination_At_Row(state, star_row)
            ? &*received_combat_order : nullptr;
    }

    inline std::vector<id_ship> Ships_In_Battle_Orbit(const s_game_state& state, int star_row,
        const s_combat_ship_order* order) {
        std::vector<id_ship> keys;
        const auto in_orbit = [&](const s_ship_data& ship) {
            return ship.id != NO_SHIP && ship.status != 5 && Ship_Orbit_Row(state, ship.position) == star_row;
        };
        if (order) {
            for (const auto key : order->keys) {
                if (const auto* ship = Find_Ship(state, key); ship && in_orbit(*ship)) keys.push_back(key);
            }
        } else {
            for (int row = 0; row < state.ship_count; ++row) {
                const auto& ship = state.ship_records[row];
                if (in_orbit(ship)) keys.push_back(ship.id);
            }
        }
        return keys;
    }

    inline int Initiative_Roster_Rank(const s_game_state& state, id_ship key, const s_combat_ship_order* order) {
        if (order) {
            const auto rank = order->ranks.find(key);
            if (rank != order->ranks.end()) return rank->second;
        }
        return Ship_Row(state, key);
    }

    struct s_combat_fleets {
        std::vector<id_ship> attacker;
        std::vector<id_ship> defender;
    };

    inline s_combat_fleets Select_Combat_Fleets(const s_game_state& state, int star_row, int attacker, int defender,
        const s_combat_ship_order* order = nullptr) {
        s_combat_fleets fleets;
        for (const auto key : Ships_In_Battle_Orbit(state, star_row, order)) {
            const auto* ship = Find_Ship(state, key);
            if (ship->status != 0) continue;
            if (ship->owner == factions::From_Legacy(attacker)) fleets.attacker.push_back(key);
            else if (ship->owner == factions::From_Legacy(defender)) fleets.defender.push_back(key);
        }
        std::ranges::reverse(fleets.defender);
        return fleets;
    }

    inline std::vector<id_ship> Active_Combat_Keys(s_game_state& world, std::span<const id_ship> keys, int star_row, int owner) {
        std::vector<id_ship> active;
        for (const auto key : keys) {
            const auto* ship = Find_Ship(world, key);
            if (ship && ship->owner == factions::From_Legacy(owner) && ship->status == 0 && Ship_Orbit_Row(world, ship->position) == star_row) active.push_back(key);
        }
        return active;
    }

    inline void Sort_Combat_Ship_Keys(s_game_state& world, std::span<id_ship> keys) {
        Legacy_Sort(keys, [&](id_ship a, id_ship b) {
            const auto* first = Find_Ship(world, a);
            const auto* second = Find_Ship(world, b);
            if (!first || !second) return int(second != nullptr) - int(first != nullptr);
            return second->d.size - first->d.size;
        });
    }
}
