#pragma once

namespace BOMB {
    extern bool capitol_can_be_bombed;

    /**
     * Handles any officer tied to a colony that is being removed.
     * @param colony Colony that is being lost.
     * @return Killed leader index, or -1 if no leader was killed.
     */
    int32_t __cdecl Handle_Leader_At_Lost_Colony(game::s_world_state* world_state, s_colony* colony);

    /**
     * Removes a colony and updates related star, message, and homeworld state.
     * @param colony Colony to destroy.
     * @param capturing_player Player that captured the colony, or -1 if it was only destroyed.
     */
    void __cdecl Destroy_Colony(s_colony* colony, int32_t capturing_player);

    /**
     * Applies one bomb-hit outcome against a colony.
     * @param colony Colony receiving the hit.
     * @param bomb_result Accumulator for combat damage results.
     * @param available_bomb_strength Remaining bomb strength available for this hit.
     * @return Hit strength consumed, or 0 if no damage can be applied.
     */
    int32_t __cdecl Resolve_Bomb_Hit(s_colony* colony, s_combat_bomb_result* bomb_result, int32_t available_bomb_strength);

    /**
     * Applies one biological attack hit against a colony.
     * @param colony Colony receiving the hit.
     * @param bomb_result Accumulator for combat damage results.
     */
    void __cdecl Resolve_Biological_Hit(s_colony* colony, s_combat_bomb_result* bomb_result);

    /**
     * Resolves all pending bomb and bio hits stored in a combat result.
     * @param bomb_result Combat result that tracks remaining hits and inflicted damage.
     */
    void __cdecl Apply_Bomb_Hits(s_combat_bomb_result* bomb_result);
} // namespace BOMB