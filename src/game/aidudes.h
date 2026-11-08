#pragma once

#include "aidata_state.h"

struct s_colony;

namespace aidudes {

    struct s_ai_colony_context {
        int16_t i_ai;
        s_player* p_ai;
        AIDATA::s_colony_workspace* work;
        const game_world::s_view* world;
    };

    struct s_population_sort_context {
        s_colony* colony;
        const AIDATA::s_colony_assignment* info;
    };

    /**
     * Sorts colony-info entries so blockaded colonies are processed first.
     * @param a First colony-info entry.
     * @param b Second colony-info entry.
     */
    int __cdecl Sort_Colonies_Blockaded(void* context, const void* a, const void* b);

    /**
     * Collects the current AI player's owned, non-outpost colonies into the working lists.
     */
    void __cdecl Collect_AI_Colonies(s_ai_colony_context* ctx);

    /**
     * Clears build queues after a tech change when the colony is on a rebuild-eligible project.
     * @param force_clear Nonzero to force reevaluation immediately.
     */
    void __cdecl Clear_Build_Queues_If_New_Tech(s_ai_colony_context* ctx, bool force_clear);

    /**
     * Refreshes ships under construction so they use the current design template.
     */
    void __cdecl Keep_Ship_Designs_Up_To_Date(s_ai_colony_context* ctx);

    /**
     * Grants emergency freighters to an AI empire that is short on food logistics.
     * @param player_idx Player receiving the freighter check.
     */
    void __cdecl Freighter_Cheat(const game_world::s_view* world, int32_t player_idx);

    /**
     * Opportunistically upgrades eligible AI ships using the current refit tables.
     */
    void __cdecl Do_Ship_Upgrade_Cheats(AIDATA::s_state* ai, s_ai_colony_context* ctx);

    /**
     * Runs the colony-allocation passes after tax policy has already been chosen.
     */
    void __cdecl Colony_AI_Given_Tax(AIDATA::s_state* ai, s_ai_colony_context* ctx);

    int __cdecl Sort_Pops_FI(void* context, const void* a, const void* b);

    int __cdecl Sort_Pops_F_IR(void* context, const void* a, const void* b);

    int16_t __cdecl FI_Worthiness(const s_colony* colony, const uint32_t* pop);

    int __cdecl F_IR_Worthiness(const game_world::s_view* world, AIDATA::s_colony_assignment* info, uint32_t* flags);

    /**
     * Processes a blockaded colony's farmer and worker assignments.
     * @param colony_info Working colony-info record for the colony.
     */
    void __cdecl Do_Blockaded_Colony(const game_world::s_view* world, AIDATA::s_colony_assignment* colony_info);

    /**
     * Processes an unblockaded colony's worker and researcher assignments.
     * @param info Working colony-info record for the colony.
     */
    void __cdecl Do_Unblockaded_Colony(const game_world::s_view* world, AIDATA::s_colony_assignment* info);

    /**
     * Assigns required fixed colonists before the colony-specific AI pass runs.
     * @param colony_info Working colony-info record to populate.
     */
    void __cdecl Assign_Required_Colonists(const game_world::s_view* world, AIDATA::s_colony_assignment* colony_info);

    void __cdecl Make_First_Unassigned_Into_Farmer(const game_world::s_view* world, AIDATA::s_colony_assignment* info);

    int __cdecl Sort_Pops_Fixed(const void* a, const void* b);

    void __cdecl Compute_Max_Min_R_I_Worthiness(const game_world::s_view* world, AIDATA::s_colony_assignment* info);

    int __cdecl _Sort_Pops_colony(s_colony* colony, const void* a, const void* b);

    int __cdecl Sort_Pops_FR_I(void* context, const void* a, const void* b);

    /**
     * Balances empire-wide worker and scientist assignments across unblockaded colonies.
     */
    void __cdecl Assign_Additional_Research_Industry(s_ai_colony_context* ctx);

    void __cdecl Grab_Scientist(const game_world::s_view* world, AIDATA::s_colony_assignment* colony);

    void __cdecl Grab_Worker(const game_world::s_view* world, AIDATA::s_colony_assignment* info);

    /**
     * Selects a shared emergency build project for colonies with open queues.
     * @param player_idx Player whose colonies are being updated.
     */
    void __cdecl Assign_Empire_Building(AIDATA::s_state* ai, const game_world::s_view* world, int16_t player_idx);

    /**
     * Revalidates researching colonies after assignment changes.
     */
    void __cdecl Industry_Sanity_Check(s_ai_colony_context* ctx);

    /**
     * Adds extra farmers until the empire's food and freighter shortages are resolved.
     */
    void __cdecl Assign_Additional_Unblockaded_Farmers(s_ai_colony_context* ctx);

    /**
     * Runs the full colony-AI pass for one player.
     * @param player_idx AI player index to process.
     */
    void __cdecl Colony_AI(AIDATA::s_state* ai, const game_world::s_view* world, int16_t player_idx);

    /**
     * Runs colony AI for every active player.
     */
    void __cdecl All_Colony_AI(AIDATA::s_state* ai, const game_world::s_view* world);

    /**
     * Lets the building AI finalize per-colony build choices after empire-wide decisions.
     */
    void __cdecl Assign_Buildings(AIDATA::s_state* ai, const game_world::s_view* world, const AIDATA::s_colony_workspace* work);
    int __cdecl colony_less_industry_cmp(void* context, const void* p1, const void* p2);

    int __cdecl Sort_Pops_R_I(void* context, const void* p1, const void* p2);
} // namespace aidudes