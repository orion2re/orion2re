#pragma once

#include <cstdint>

struct s_leader_data;

namespace aileader {
    /** Updates AI leader hiring, marooned-hero checks, and automatic leader assignment for non-human players. */
    void __cdecl Do_AI_Leaders(AIDATA::s_state* ai, const game_world::s_view* world);

    /**
     * Assigns an unassigned fleet officer to the best eligible ship for a player.
     * @param player_index Owning player index.
     * @param leader_index Leader index in the global leader table.
     */
    void __cdecl Assign_Fleet_Officer(const game_world::s_view* world, int32_t player_index, int32_t leader_index);

    /**
     * Assigns an unassigned star officer to the best eligible colony system for a player.
     * @param player_index Owning player index.
     * @param leader_index Leader index in the global leader table.
     */
    void __cdecl Assign_Star_Officer(AIDATA::s_state* ai, const game_world::s_view* world, int32_t player_index, int32_t leader_index);

    /**
     * Reassigns a fleet officer to a better ship at the same location, or any eligible ship when the current assignment is being scrapped.
     * @param player_index Owning player index.
     * @param leader_index Leader index in the global leader table.
     */
    void __cdecl Reassign_Fleet_Officer(const game_world::s_view* world, int32_t player_index, int32_t leader_index);

    /**
     * Assigns or reassigns any currently owned leaders that are still available for AI use.
     * @param player_idx Owning player index.
     */
    void __cdecl Assign_Unassigned_Leaders(AIDATA::s_state* ai, const game_world::s_view* world, int32_t player_idx);
} // namespace aileader