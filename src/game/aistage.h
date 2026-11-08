#pragma once

#include <cstdint>

namespace AISTAGE {
    /**
     * Computes and marks the player's strategic stage-point stars.
     * @param player_idx Player index whose stage points are recalculated.
     */
    void __cdecl Compute_Defensive_Stage_Points(AIDATA::s_state* ai, int16_t player_idx);
} // namespace AISTAGE