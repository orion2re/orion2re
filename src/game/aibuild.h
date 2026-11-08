#pragma once

#include "production_rules.h"
#include "aidata_state.h"

namespace AIBUILD {
    constexpr int32_t AUTOBUILD_CANDIDATE_COUNT = BUILDING_COUNT + 2;
    enum e_allocation_candidate : uint8_t {
        ALLOCATION_COMBAT_SHIPS,
        ALLOCATION_FREIGHTERS,
        ALLOCATION_REFITS,
        ALLOCATION_SPIES,
        ALLOCATION_TRANSPORTS,
        ALLOCATION_DEFER,
        STRATEGIC_ALLOCATION_CANDIDATE_COUNT,
    };
    constexpr int32_t EMERGENCY_DEFENSE_PRODUCT_COUNT = 8;

    struct s_autobuild_rules {
        production_rules::s_cutoff cutoff;
        production_rules::s_candidate candidates[AUTOBUILD_CANDIDATE_COUNT];
    };

    struct s_ordered_production_policy {
        bool enabled;
        int16_t order[EMERGENCY_DEFENSE_PRODUCT_COUNT];
    };

    struct s_economic_production_policy {
        bool enabled;
        int16_t product;
    };

    struct s_expansion_production_policy {
        bool enabled;
        int16_t colony_base_minimum_industry;
        int16_t colony_ship_minimum_industry;
    };

    struct s_strategic_allocation {
        production_rules::s_cutoff cutoff;
        production_rules::s_candidate candidates[STRATEGIC_ALLOCATION_CANDIDATE_COUNT];
    };

    struct s_strategic_production {
        s_ordered_production_policy emergency_defense;
        s_economic_production_policy economic_emergency;
        s_expansion_production_policy expansion;
        s_strategic_allocation allocation;
    };

    struct s_rules {
        s_autobuild_rules autobuild = {};
        s_strategic_production strategic = {};
        uint8_t ship_upgrade_bonus[GAME_DIFFICULTY_COUNT] = {2, 10, 35, 50, 115};
        int8_t human_opponent_bias[GAME_DIFFICULTY_COUNT] = {1, 1, 2, 3, 4};
        uint8_t hull_size_weights[HULL_CLASS_COUNT] = {1, 2, 3, 5, 8, 13, 0, 0, 0};
        bool colony_bases_prewarp = false;
    };

    extern s_rules rules;

    struct s_opponent_view {
        const s_player* players;
        uint32_t player_count;
        uint8_t human_opponent_bias;
    };

    s_opponent_view Opponents(const game_world::s_view* world);

    /** Returns the refit cost for replacing one ship design with another.
     * @param owner Player paying for the refit.
     * @param old_design Existing ship design.
     * @param new_design Replacement ship design.
     */
    int16_t __cdecl Refit_Cost(const s_player* owner, const struct s_ship_design* old_design, const struct s_ship_design* new_design);

    /** Returns an average strength estimate for a ship against relevant opponents.
     * @param ship Ship whose combat strength is being evaluated.
     */
    int16_t __cdecl Mean_Ship_Strength(const s_opponent_view* opponents, const AIDATA::s_ship_assessment* assessment,
        const AIDATA::s_empire_assessment* owner_info, const struct s_ship_data* ship);

    /** Computes build-related needs for one AI player.
     * @param player_idx Player index to evaluate.
     */
    void __cdecl Compute_Needs(AIDATA::s_state* ai, const game_world::s_view* world, int16_t player_idx);

    /** Recomputes empire-wide build needs for all AI players. */
    void __cdecl Compute_Empire_Building_Needs(AIDATA::s_state* ai, const game_world::s_view* world);

    /** Returns the highest hostile treaty state currently affecting a player.
     * @param player_idx Player index to inspect.
     */
    int16_t __cdecl Player_War_Level(const game_world::s_view* world, int16_t player_idx);

    /** Computes whether a player currently needs more colony ships.
     * @param needs Production needs to update.
     */
    void __cdecl Compute_Colony_Ship_Need(AIDATA::s_build_needs* needs);

    /** Computes how many freighter groups a player still needs.
     * @param needs Production needs to update.
     * @param surplus_freighters Current empire freighter surplus.
     */
    void __cdecl Compute_Freighter_Need(AIDATA::s_build_needs* needs, int16_t surplus_freighters);

    /** Buys out eligible colony production for one player while funds allow.
     * @param player_idx Player index whose colonies may buy production outright.
     */
    void __cdecl Empire_Buy_Outright(AIDATA::s_state* ai, const game_world::s_view* world, int16_t player_idx);

    /** Marks which buildings a colony is currently allowed to construct.
     * @param colony Colony to inspect.
     * @param buildable_flags Output flags indexed by product id.
     */
    void __cdecl Find_Buildable_Buildings(struct s_colony* colony, uint16_t* buildable_flags);

    /** Scores one building choice for a colony's AI production logic.
     * @param colony Colony being evaluated.
     * @param building_id Building or product id to score.
     */
    int16_t __cdecl Colony_Building_Score(AIDATA::s_state* ai, const game_world::s_view* world, uint32_t colony_id, struct s_colony* colony, int16_t building_id);

    /** Builds normalized deterministic production facts for one colony.
     * @param colony Colony to inspect.
     * @param war_level Effective strategic war level, or minus one to calculate it.
     * @param context Output production context.
     */
    void Build_Production_Context(AIDATA::s_state* ai, const game_world::s_view* world, uint32_t colony_id, const struct s_colony* colony, int32_t war_level, production_rules::s_context* context);

    /** Reassigns colony production when the current build should be replaced.
     * @param colony Colony whose production is being checked.
     */
    void __cdecl Assign_Colony_Building(AIDATA::s_state* ai, const game_world::s_view* world, uint32_t colony_id, struct s_colony* colony);

    /** Chooses a new production target for a colony.
     * @param colony Colony whose production is being assigned.
     */
    void __cdecl Assign_Colony_New_Building(AIDATA::s_state* ai, const game_world::s_view* world, uint32_t colony_id, struct s_colony* colony);

    /** Returns whether a colony contains foreign population from a treaty partner.
     * @param colony Colony to inspect.
     */
    bool __cdecl Colony_Is_Conquered(const game_world::s_view* world, struct s_colony* colony);

    /** Forces one listed colony to switch to a specific production item.
     * @param id_list List of colony indexes to update.
     * @param list_index Index within id_list to process.
     * @param list_count Number of active ids in the list.
     * @param production_id Product id to assign.
     */
    void __cdecl Force_Produce(const game_world::s_view* world, uint32_t* id_list, uint32_t list_index, int32_t* list_count, int16_t production_id);

    /** Returns a simple industry value estimate for a colony.
     * @param colony Colony to evaluate.
     */
    int16_t __cdecl Industry_Worth(struct s_colony* colony);

    /** Returns the display name for a colony production item.
     * @param product_id Building or project id to name.
     */
    char* __cdecl Product_Name(int16_t product_id);

    /** Assigns a fresh autobuild target without recalculating current-turn production.
     * @param colony Colony to update.
     */
    void __cdecl Player_Colony_Autobuild(AIDATA::s_state* ai, const game_world::s_view* world, uint32_t colony_id, s_colony* colony);
} // namespace AIBUILD