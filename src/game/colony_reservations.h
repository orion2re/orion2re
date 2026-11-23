#pragma once

#include <span>

#include "game_state.h"

namespace game {
    // The star priorities determine who can claim the remaining uncolonized worlds.
    inline void Reserve_Colony_Ids(s_game_state& state, int players, uint32_t stardate) {
        for (auto& queue : state.colony_reservations) queue.clear();
        if (players < 1 || players > MAX_PLAYERS || state.colonies.size() >= MAX_COLONIES) return;

        std::array<size_t, MAX_PLAYERS> demand{};
        for (const auto& [id, star] : state.stars) {
            const auto colonizer = factions::Player(star.colonize_player);
            if (colonizer == game::NO_PLAYER || colonizer.id >= static_cast<uint32_t>(players)) continue;
            for (const auto& link : star.orbit_planet_ids) {
                const auto planet = state.planets.find(link);
                if (planet != state.planets.end() && planet->second.colony_id == NO_COLONY) ++demand[colonizer.id];
            }
        }

        auto free = MAX_COLONIES - state.colonies.size();
        auto next = state.next_colony_id;
        const auto first = stardate % players;
        while (free && next != NO_COLONY.id) {
            bool allocated = false;
            for (int i = 0; i < players && free && next != NO_COLONY.id; ++i) {
                const auto player = (first + i) % players;
                if (demand[player] == 0) continue;
                state.colony_reservations[player].push_back(id_colony{next++});
                --free;
                --demand[player];
                allocated = true;
            }
            if (!allocated) break;
        }
    }
}
