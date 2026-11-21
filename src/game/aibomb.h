#pragma once

namespace aibomb {
    /** Estimates whether an invasion is likely to succeed by simulating ground combat.
     * @param attacker_id Attacking player index.
     * @param ship_list List of ships contributing to the invasion.
     * @param n_ships Number of ship entries in ship_list.
     * @param colony Target colony being invaded.
     */
    bool __cdecl AI_Invasion_Will_Succeed(int8_t attacker_id, int16_t* ship_list, int32_t n_ships, game::id_colony colony_id, s_colony* colony);
} // namespace aibomb