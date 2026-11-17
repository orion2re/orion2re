#pragma once
#include <cstdint>
#include <map>

namespace ADVCIV {
    extern int8_t* star_colony_owner;
    extern std::map<game::id_planet, int8_t> planet_selected_flag;
    extern s_adv_civ_player* adv_civ_player;
    extern s_parsec* parsec;
    extern s_adv_planet_worth* planet_worth_to_player;

    /** Returns whether another player owns a colony at the specified star.
     * @param player_idx Player to exclude from the ownership check.
     * @param star_idx Star index to inspect.
     */
    bool __cdecl Enemy_Has_Colony_At_Star(int16_t player_idx, int16_t star_idx);

    /** Builds and assigns advanced-civilization starting colonies. */
    void __cdecl Advanced_Civilization_Colonies(::game::s_world_state* world_state);

    /** Placeholder for per-player advanced-civilization fleet setup.
     * @param player_index Player index passed through by the caller.
     */
    void __cdecl Init_Advanced_Civilization_Fleet(int16_t player_index);

    /** Invokes the advanced-civilization fleet setup placeholder for each player. */
    void __cdecl Assign_Advanced_Civilization_Starting_Ships();

    /** Fills the inter-star distance table used during colony selection.
     * @param parsec_distances Output buffer of per-star parsec distances.
     */
    void __cdecl Build_Parsec_Star_Distances(s_parsec* parsec_distances);

    /** Chooses non-homeworld planets for advanced-civilization expansion. */
    void __cdecl Choose_Adv_Civ_Planets(::game::s_world_state* world_state);

    /** Returns the target number of advanced-civilization planets per player. */
    int16_t Num_Adv_Civ_Planets();

    /** Builds candidate star data for each advanced-civilization player. */
    void __cdecl Build_Adv_Civ_Star_List(::game::s_world_state* world_state);

    /** Builds each player's list of candidate planets from reachable stars. */
    void __cdecl Build_Adv_Civ_Planet_List(::game::s_world_state* world_state);

    /** Returns the next available candidate planet for the specified player.
     * @param player_idx Player whose candidate list is searched.
     * @param availability_map Per-planet selection map where -1 means unclaimed.
     */
    int16_t Get_Next_Adv_Civ_Planet(game::s_world_state* world_state, int16_t player_idx, const std::map<game::id_planet, int8_t>& availability_map);

    /** Improves selected planets so weaker starts better match the strongest one. */
    void __cdecl Twiddle_Selected_Adv_Civ_Planets(::game::s_world_state* world_state);

    /** Initializes colony data for selected non-homeworld planets. */
    void __cdecl Init_Non_Homeworld_Colonies(::game::s_world_state* world_state);

    /** Applies one weighted upgrade step to a planet.
     * @param planet_idx Planet index to modify.
     * @param upgrade_weights Weights for size, climate, and mineral upgrades.
     */
    int8_t __cdecl Twiddle_Planet(::game::s_world_state* world_state, ::game::id_planet planet_idx, uint8_t* upgrade_weights);

    /** Returns the average worthiness of a player's selected planets.
     * @param player_idx Player index whose selected planets are evaluated.
     */
    int32_t __cdecl Get_Players_Planet_Worthiness_Average(::game::s_world_state* world_state, int16_t player_idx);

    /** Compares two candidate planets by descending worth for sorting.
     * @param a Pointer to the first candidate planet record.
     * @param b Pointer to the second candidate planet record.
     */
    int __cdecl qsort_planet_worth(const void* a, const void* b);

    /** Computes worth data for one candidate planet for one player.
     * @param player_idx Player index used for the evaluation.
     * @param planet_idx Planet index being evaluated.
     * @param worth Output total worth score.
     * @param has_colony Output flag for an existing colony at the same star.
     * @param proximity_bonus Output proximity bonus added to the worth score.
     */
    void __cdecl Get_Adv_Civ_Planet_Worthiness(::game::s_world_state* world_state, int16_t player_idx, ::game::id_planet planet_idx, int32_t* worth, int8_t* has_colony, int16_t* proximity_bonus);

    /** Recomputes worth values for all candidate planets for all players. */
    void __cdecl Get_Worth_For_All_Planets();

    /** Returns whether the specified player has a colony at the specified star.
     * @param player_idx Player index to test.
     * @param star_idx Star index to inspect.
     */
    bool __cdecl Player_Has_Colony_At_Star(int16_t player_idx, int16_t star_idx);

    /** Computes the colony-proximity bonus for a candidate planet.
     * @param player_idx Player index used for the distance scan.
     * @param planet_idx Planet index whose star is being scored.
     */
    int16_t __cdecl Proximity_Bonus(::game::s_world_state* world_state, int16_t player_idx, ::game::id_planet planet_idx);
} // namespace ADVCIV