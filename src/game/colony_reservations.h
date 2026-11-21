#pragma once

#include <span>

#include "game_state.h"

namespace game {
    // The star priorities determine who can claim the remaining uncolonized worlds.
    template<class Star>
    void Reserve_Colony_Ids(s_game_state& state, std::span<const Star> stars, int players, uint32_t stardate) {
        for (auto& queue : state.colony_reservations) queue.clear();
        if (players < 1 || players > MAX_PLAYERS || state.colonies.size() >= MAX_COLONIES) return;

        std::array<size_t, MAX_PLAYERS> demand{};
        for (const auto& star : stars) {
            if (star.colonize_player < 0 || star.colonize_player >= players) continue;
            for (const auto& link : star.orbit_planet_ids) {
                const auto planet = state.planets.find(link.get());
                if (planet != state.planets.end() && planet->second.colony_id == NO_COLONY) ++demand[star.colonize_player];
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
