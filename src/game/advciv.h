#pragma once
#include <cstdint>
#include <map>
#include "object_ids.h"

struct s_planet_data;
struct s_player;
struct s_adv_civ_player;
struct s_parsec;
namespace game { struct s_game_state; }

namespace ADVCIV {
    extern s_adv_civ_player* adv_civ_player;
    extern s_parsec* parsec;

    // Includes homeworlds and reserved picks whose colonies have not been created yet.
    using planned_colony_owners = std::map<game::id_star, game::id_player>;

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

    /** Returns the target number of advanced-civilization planets per player. */
    int16_t Num_Adv_Civ_Planets();

    /** Builds each player's list of candidate planets from reachable stars. */
    void Build_Adv_Civ_Planet_List(const game::s_game_state& state);

    /** Initializes colony data for selected non-homeworld planets. */
    void __cdecl Init_Non_Homeworld_Colonies();

    /** Applies one weighted upgrade step to a planet.
     * @param planet Planet to modify.
     * @param upgrade_weights Weights for size, climate, and mineral upgrades.
     */
    int8_t __cdecl Twiddle_Planet(s_planet_data& planet, uint8_t* upgrade_weights);

    /** Compares two candidate planets by descending worth for sorting.
     * @param a Pointer to the first candidate planet record.
     * @param b Pointer to the second candidate planet record.
     */
    int __cdecl qsort_planet_worth(const void* a, const void* b);

    /** Resolves planned ownership, independently of the star's current slot. */
    game::id_player Planned_Colony_Owner(const planned_colony_owners& owners, game::id_star star);

    /** Scores a candidate against the current advanced-start plan. */
    void Get_Adv_Civ_Planet_Worthiness(game::s_game_state& state, const planned_colony_owners& owners,
        s_planet_data& planet, s_player& player, int32_t* worth, int8_t* has_colony, int16_t* proximity_bonus);
} // namespace ADVCIV