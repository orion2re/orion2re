#pragma once
#include <cstdint>

struct s_planet_data;
namespace game { struct s_game_state; }

namespace ADVCIV {
    extern int8_t* star_colony_owner;
    extern s_adv_civ_player* adv_civ_player;
    extern s_parsec* parsec;

    /** Returns whether another player owns a colony at the specified star.
     * @param player_idx Player to exclude from the ownership check.
     * @param star_idx Star index to inspect.
     */
    bool __cdecl Enemy_Has_Colony_At_Star(int16_t player_idx, int16_t star_idx);

    /** Builds and assigns advanced-civilization starting colonies. */
    void __cdecl Advanced_Civilization_Colonies();

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
    void Choose_Adv_Civ_Planets(game::s_game_state& state);

    /** Returns the target number of advanced-civilization planets per player. */
    int16_t Num_Adv_Civ_Planets();

    /** Builds candidate star data for each advanced-civilization player. */
    void Build_Adv_Civ_Star_List(game::s_game_state& state);

    /** Builds each player's list of candidate planets from reachable stars. */
    void Build_Adv_Civ_Planet_List(const game::s_game_state& state);

    /** Returns the next available candidate planet for the specified player.
     * @param player_idx Player whose candidate list is searched.
     * @return Position in that player's ranked candidate list, or -1.
     */
    int16_t Get_Next_Adv_Civ_Planet(const game::s_game_state& state, int16_t player_idx);

    /** Improves selected planets so weaker starts better match the strongest one. */
    void Twiddle_Selected_Adv_Civ_Planets(game::s_game_state& state);

    /** Initializes colony data for selected non-homeworld planets. */
    void __cdecl Init_Non_Homeworld_Colonies();

    /** Applies one weighted upgrade step to a planet.
     * @param planet Planet to modify.
     * @param upgrade_weights Weights for size, climate, and mineral upgrades.
     */
    int8_t __cdecl Twiddle_Planet(s_planet_data& planet, uint8_t* upgrade_weights);

    /** Returns the average worthiness of a player's selected planets.
     * @param player_idx Player index whose selected planets are evaluated.
     */
    int32_t Get_Players_Planet_Worthiness_Average(game::s_game_state& state, int16_t player_idx);

    /** Compares two candidate planets by descending worth for sorting.
     * @param a Pointer to the first candidate planet record.
     * @param b Pointer to the second candidate planet record.
     */
    int __cdecl qsort_planet_worth(const void* a, const void* b);

    /** Computes worth data for one candidate planet for one player.
     * @param player_idx Player index used for the evaluation.
     * @param planet Planet being evaluated.
     * @param worth Output total worth score.
     * @param has_colony Output flag for an existing colony at the same star.
     * @param proximity_bonus Output proximity bonus added to the worth score.
     */
    void __cdecl Get_Adv_Civ_Planet_Worthiness(s_planet_data& planet, int16_t player_idx, int32_t* worth, int8_t* has_colony, int16_t* proximity_bonus);

    /** Recomputes worth values for all candidate planets for all players. */
    void Get_Worth_For_All_Planets(game::s_game_state& state);

    /** Returns whether the specified player has a colony at the specified star.
     * @param player_idx Player index to test.
     * @param star_idx Star index to inspect.
     */
    bool __cdecl Player_Has_Colony_At_Star(int16_t player_idx, int16_t star_idx);

    /** Computes the colony-proximity bonus for a candidate planet.
     * @param player_idx Player index used for the distance scan.
     * @param target_star_idx Index of the candidate planet's star.
     */
    int16_t __cdecl Proximity_Bonus(int16_t player_idx, int16_t target_star_idx);
} // namespace ADVCIV