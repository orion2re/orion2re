#pragma once

#include <vector>

#include "object_ids.h"

namespace MAPGEN {
    // The candidate order preserves the legacy RNG selection sequence.
    std::vector<game::id_planet> Choose_Planets_To_Prune(
        std::vector<game::id_planet> candidates, int live_count, int limit);
}
