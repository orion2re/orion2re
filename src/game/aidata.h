#pragma once

#include "aidata_state.h"

namespace AIDATA {
    struct s_uncolonized_planet_food_worth {
        uint16_t biological;
        uint16_t cybernetic;
    };

    struct s_uncolonized_planet_special_worth {
        uint32_t gold_deposits;
        uint32_t gem_deposits;
        uint32_t ancient_artifacts;
        uint32_t orion;
    };

    struct s_planet_valuation_rules {
        s_uncolonized_planet_food_worth food = {75, 38};
        s_uncolonized_planet_special_worth special = {1280, 2560, 640, 1600};
    };

    extern s_planet_valuation_rules planet_valuation_rules;

    /*
     * Returns the player with the strongest population claim on a colony.
     * @param colony Colony to inspect.
     */
    id_player Colony_Dominant_Player(int32_t player_count, const s_colony* colony);

    /*
     * Updates per-star AI range summaries for each player.
     */
    void __cdecl Compute_Star_Player_Range_Info(s_state* ai, const game::s_view* world);

    /*
     * Estimates how valuable a colony is to a specific player.
     * @param colony Colony being evaluated.
     * @param evaluating_player Player perspective used for the estimate.
     */
    int16_t __cdecl Colony_Worth_To_Player(s_state* ai, const game::s_view* world, id_colony colony_id, s_colony* colony, s_player* evaluating_player);

    /*
     * Estimates the strategic range value of a colony to a specific player.
     * @param colony Colony being evaluated.
     * @param evaluating_player Player perspective used for the estimate.
     */
    int16_t __cdecl Proximity_Worth_To_Player(s_state* ai, const game::s_view* world, s_colony* colony, s_player* evaluating_player);

    /*
     * Computes the base per-player value table for all known planets.
     */
    void __cdecl Compute_Base_Planet_Values(s_state* ai, const game::s_view* world);

    /*
     * Computes per-player planet values adjusted by same-system context.
     */
    void __cdecl Compute_Contextual_Planet_Values(s_state* ai, const game::s_view* world);

    /*
     * Scores how worthwhile it is to refit a ship against current tech.
     * @param ship Ship to evaluate.
     */
    int32_t __cdecl Refit_Desirability(const s_player* owner, const s_technology_assessment* ai_tech, const s_ship_data* ship);

    /*
     * Returns a cached route, or no value when the destination is unreachable.
     * @param player_id Player whose travel rules are used.
     * @param source_star Starting star.
     * @param destination_star Destination star.
     * @param use_extended_range Whether to use the extended-range result.
     */
    std::optional<s_route> Player_N_Turns_From_Star_To_Star_With_Intermediate(
        s_state* ai, const game::s_view* world, id_player player_id, id_star source_star, id_star destination_star, bool use_extended_range);

    /*
     * Returns a cached ship route, or no value when the destination is unreachable.
     * @param ship_id Ship to evaluate.
     * @param destination_star Destination star.
     */
    std::optional<s_route> Ship_N_Turns_To_Star_With_Intermediate(s_state* ai, const game::s_view* world, id_ship ship_id, id_star destination_star);

    /*
     * Compares refit entries by desirability for qsort.
     * @param lhs Left entry.
     * @param rhs Right entry.
     */
    int __cdecl airefit_cmp(const void* lhs, const void* rhs);

    /*
     * Orders combat ships by owner and map position for qsort.
     * @param lhs Left ship index.
     * @param rhs Right ship index.
     */
    int __cdecl sort_ships(void* context, const void* lhs, const void* rhs);

    /*
     * Checks whether a target star is reachable from a source star or its wormhole exit.
     * @param target_star_idx Star being tested.
     * @param source_star_idx Source star.
     * @param max_range_parsecs Maximum allowed distance in parsecs.
     */
    bool Star_In_Range_Of_Given_Star(const game::s_view* world, id_star target_star_idx, id_star source_star_idx, int32_t max_range_parsecs);

    /*
     * Builds AI caches for ships, colonies, stars, and per-player planning data.
     */
    bool Compute_AI_Data(s_state* ai, const game::s_view* world);
    // Null on success; otherwise a stable diagnostic identifying the invalid input.
    const char* Validate_Inputs(const game::s_view* world);

    /*
     * Marks which players can reach a star within normal and extended range.
     * @param star_idx Star being tested.
     * @param in_normal_range_out Receives players within normal range.
     * @param in_extended_range_out Receives players within extended range.
     */
    void Find_Players_In_Range(const game::s_view* world, id_star star_idx, int8_t* in_normal_range_out, int8_t* in_extended_range_out);

    /*
     * Estimates how valuable settling an uncolonized planet is to a player.
     * @param planet Planet being evaluated.
     * @param evaluating_player Player perspective used for the estimate.
     */
    int32_t __cdecl Uncolonized_Planet_Worth_To_Player(const game::s_view* world, s_planet_data* planet, s_player* evaluating_player);
} // namespace AIDATA