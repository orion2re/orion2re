#pragma once
#include <span>

#include "aidata_state.h"

namespace AIMOVE {
    inline constexpr int32_t ENEMY_STRENGTH_TURN_WINDOW = AIDATA::FORECAST_TURN_COUNT;

    struct s_staging_point {
        uint32_t star_id = 0;
        uint32_t strength = 0;
    };

    struct s_nonplayer_arrival {
        uint8_t arrival_turn = 0;
        int32_t strength = 0;
    };

    struct s_interception_target {
        uint32_t star_id = 0;
        uint16_t player_mask = 0;
        std::optional<uint8_t> earliest_arrival;
        std::array<int32_t, AIDATA::FORECAST_TURN_COUNT> strengths = {};
        std::array<bool, MAX_PLAYERS> present_players = {};
    };

    struct s_nonplayer_destination {
        uint32_t star_id = 0;
        uint8_t distance_parsecs = 0;
        bool has_lucky = false;
        bool has_unlucky = false;
        uint32_t max_lucky_empire_strength = 0;
        uint32_t max_unlucky_empire_strength = 0;
    };

    struct s_attack_info {
        int16_t target_player;
        int16_t minimum_strength_ratio_percent;
        std::optional<uint32_t> target_planet;
        int16_t colony_value;
        int16_t strength_ratio_percent;
    };

    struct s_attack_choice {
        uint32_t planet_id;
        int16_t colony_value;
        int32_t strength_ratio_percent;
    };

    struct s_threat {
        uint32_t source_star;
        s_attack_choice attack;
    };

    struct s_ai_movement_context {
        int16_t player_idx;
        s_player* player;
        AIDATA::s_movement_workspace* work;
        const game_world::s_view* world;
    };
    struct s_rules {
        uint8_t eel_spawn_frequency = 30;
        uint8_t max_eels = 4;
    };

    extern s_rules rules;

    /**
     * Runs the full movement phase for one AI empire.
     * @param player_idx AI player to process.
     */
    void __cdecl Move_AI(AIDATA::s_state* ai, const game_world::s_view* world, int16_t player_idx);

    int __cdecl Sort_colonizable(const void* a, const void* b);

    /**
     * Removes colonizable planets that already have an assigned colony ship or queued colony ship.
     * @param candidates In/out list of colonizable planet candidates.
     */
    void Delete_Already_Assigned_Planets(s_ai_movement_context* ctx, std::vector<AIDATA::s_colonization_candidate>* candidates);

    /**
     * Launches the AI's prepared sneak attack if the target is still valid.
     */
    void __cdecl Launch_Sneak_Attack(AIDATA::s_state* ai, s_ai_movement_context* ctx);

    /**
     * Launches attacks of the requested mission type from all eligible staging stars.
     * @param mission_type Mission code to assign to launched fleets.
     * @param min_pop Minimum remaining population threshold used by attack scoring.
     */
    void __cdecl Launch_Attacks(AIDATA::s_state* ai, s_ai_movement_context* ctx, int mission_type, int min_pop);

    /**
     * Evaluates the best attack target reachable from a staging star.
     * @param star_idx Source staging star.
     * @param target_info Optional output filter and detail structure.
     * @param mission_type Mission code to search for.
     * @param min_pop Minimum remaining population threshold used by attack scoring.
     */
    void __cdecl Find_Best_Attack_From_Star(AIDATA::s_state* ai, s_ai_movement_context* ctx, int16_t star_idx, s_attack_info* target_info, int mission_type, int min_pop);

    int8_t __cdecl AI_May_Attack_Player(const game_world::s_view* world, int16_t player_idx, int16_t target_idx);

    uint16_t __cdecl Compute_Attack_Worthiness(
        AIDATA::s_state* ai, s_ai_movement_context* ctx,
        int16_t colony_idx,
        std::span<const uint32_t> ships,
        uint8_t eta,
        int8_t* out_attack_type,
        uint8_t* out_flee,
        s_attack_info* target_info,
        int32_t min_pop);

    int8_t __cdecl Player_Is_Hostile_To_Player(const game_world::s_view* world, int player_idx_a, int player_idx_b);

    int32_t __cdecl N_Enemy_Stars_Reachable(const game_world::s_view* world, int16_t player_idx, int16_t player_bit, int16_t source_star_idx);

    void __cdecl Intercept_Invaders(AIDATA::s_state* ai, s_ai_movement_context* ctx);

    void __cdecl Interceptions(AIDATA::s_state* ai, s_ai_movement_context* ctx, s_interception_target* intercepts, int* count, const std::unordered_map<uint32_t, uint32_t>* star_to_intercept_idx, int is_desperate);

    void Assign_New_Colony_Ship_Destinations(AIDATA::s_state* ai, s_ai_movement_context* ctx, std::vector<AIDATA::s_colonization_candidate>* planets);

    bool __cdecl Star_Has_Monster(const game_world::s_view* world, int16_t star_idx);

    void Get_Best_Unescorted_Colony_Ship(s_ai_movement_context* ctx, std::vector<uint32_t>* ships, uint32_t destination);

    std::optional<uint32_t> I_Soonest_Enemy_Colony_Ship(const game_world::s_view* world, std::span<const uint32_t> ships, uint32_t target_location);

    bool Get_Best_Escorted_Colony_Ship(AIDATA::s_state* ai, s_ai_movement_context* ctx, std::vector<uint32_t>* colony_ships, uint32_t star_idx, int16_t max_turns);

    void __cdecl Move_Transports(AIDATA::s_state* ai, s_ai_movement_context* ctx);

    void __cdecl Move_To_Stage_Points(AIDATA::s_state* ai, s_ai_movement_context* ctx);

    void __cdecl Move_To_Unexplored_Stars(AIDATA::s_state* ai, s_ai_movement_context* ctx);

    void __cdecl Move_NPCs(const game_world::s_view* world);

    int __cdecl Sort_npc_dests(const void* a, const void* b);

    std::optional<uint32_t> Npc_Dest(const game_world::s_view* world, uint32_t ship_idx, const uint8_t* star_flags);

    void __cdecl Move_All_AI(AIDATA::s_state* ai, const game_world::s_view* world);

    /**
     * Marks stars the AI should avoid because of temporary blockade restrictions.
     * @param player_idx AI player whose flee map is being built.
     */
    void __cdecl Mark_Flee_Stars(const game_world::s_view* world, AIDATA::s_movement_workspace* work, int16_t player_idx);

    /**
     * Gathers all currently colonizable planets within the AI player's extended range.
     * @param candidates Output list of candidate planets.
     */
    void Get_Colonizable_Planets(AIDATA::s_state* ai, s_ai_movement_context* ctx, std::vector<AIDATA::s_colonization_candidate>* candidates);

    void Gather_Player_Ships_At_Star(AIDATA::s_state* ai, s_ai_movement_context* ctx, int16_t player_id, uint32_t star_id, std::vector<uint32_t>* ships);

    void __cdecl Gather_Intercepting_Ships(AIDATA::s_state* ai, s_ai_movement_context* ctx, uint32_t* ships_list, int32_t* ship_count, const std::unordered_map<uint32_t, uint32_t>* intercept_map);

    void __cdecl Knock_Out_Impossible_Interceptions(AIDATA::s_state* ai, s_ai_movement_context* ctx, s_interception_target* intercepts, int* count, int keep_one_entry);

    uint16_t __cdecl Ship_Min_Strength_Versus_Players_At_Star(AIDATA::s_state* ai, const game_world::s_view* world, uint32_t ship_idx, uint16_t player_mask, uint32_t star_idx);

    std::optional<s_threat> Player_Threatens_Player(AIDATA::s_state* ai, const game_world::s_view* world, int32_t player_id, int32_t target_player_id);

    std::optional<uint32_t> Best_Uncolonized_Undestined_Planet(AIDATA::s_state* ai, const game_world::s_view* world, int16_t player_idx);

    uint32_t __cdecl Ship_Rough_Strength_At_Star(AIDATA::s_state* ai, s_ai_movement_context* ctx, uint32_t ship_idx, uint32_t star_idx);

    void __cdecl Get_Enemy_Strength_On_Turn(AIDATA::s_state* ai, s_ai_movement_context* ctx, uint32_t star_id, int* turn_strengths, int* cumulative_strengths);

    int __cdecl Number_Of_Attacks_Underway(const game_world::s_view* world, int16_t player_idx, int16_t target_idx);

    std::optional<s_attack_choice> Find_Opportunity_Attack(AIDATA::s_state* ai, const game_world::s_view* world, int16_t player_idx, int16_t target_idx, int16_t threshold);

    /**
     * Queries legacy movement for a direct or intermediate route.
     * @param ships Ship IDs to route.
     * @param destination_star Final destination star.
     */
    std::optional<AIDATA::s_route> Move_Ships_With_Possible_Intermediate(s_ai_movement_context* ctx, std::span<const uint32_t> ships, uint32_t destination_star);

    int32_t __cdecl Enemy_Colony_Worth_To_Player(AIDATA::s_state* ai, const game_world::s_view* world, int16_t colony_idx, int16_t player_idx);

    /**
     * Looks for an opportunity attack from a staging star with a minimum score threshold.
     * @param player_id Attacking player.
     * @param target_player_idx Defending player to evaluate.
     * @param min_value Minimum acceptable attack value.
     * @param star_id Source staging star.
     */
    std::optional<s_attack_choice> Find_Opportunity_Attack_Aux(
        AIDATA::s_state* ai, const game_world::s_view* world,
        int16_t player_id,
        int16_t target_player_idx,
        int16_t min_value,
        uint32_t star_id);

    int __cdecl Sort_candidate_stars(void* context, const void* candidate_star_a, const void* candidate_star_b);
} // namespace AIMOVE