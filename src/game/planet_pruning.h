#pragma once

#include <vector>
#include <span>

#include "object_ids.h"

namespace game { struct s_game_state; }

namespace MAPGEN {
    // The candidate order preserves the legacy RNG selection sequence.
    std::vector<game::id_planet> Choose_Planets_To_Prune(
        std::vector<game::id_planet> candidates, int live_count, int limit);

    void Prune_Planet_Roster(game::s_game_state& state,
        std::span<const uint8_t> protected_stars, int limit);
}
