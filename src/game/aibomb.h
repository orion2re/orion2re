#pragma once
#include <span>
#include "object_ids.h"

namespace aibomb {
    /** Estimates whether an invasion is likely to succeed by simulating ground combat.
     * @param attacker_id Attacking player index.
     * @param ship_list List of ships contributing to the invasion.
     * @param colony Target colony being invaded.
     */
    bool AI_Invasion_Will_Succeed(int8_t attacker_id, std::span<const game::id_ship> ship_list, game::id_colony colony_id, s_colony* colony);
} // namespace aibomb