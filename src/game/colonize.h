#pragma once
#include <cstdint>
#include "object_ids.h"

namespace COLONIZE {
    extern uint8_t first_pop_to_production;

    // Allocates free colony slots to players based on pending colonization opportunities.
    void Allocate_New_Colony_Slots();
    // Runs AI colonization for every active AI player.
    void __cdecl All_AI_Colonize();

    // Determines which player, if any, has colonization priority at each star.
    void Find_Colonize_Players();

    // Counts planets at a star that the specified player can colonize.
    int16_t Star_N_Colonizable_Planets_For_Player(s_star_data* star, int16_t player_idx);

    // Counts uncolonized planets at a star that are valid outpost targets.
    int16_t Star_N_Outpostable_Planets(s_star_data* star);

    // Counts uncolonized planets present at a star.
    int16_t Star_N_Uncolonized_Planets(s_star_data* star);

    // Performs colonization decisions for one AI player.
    void __cdecl AI_Colonize(int16_t player_idx);

    // Creates a new colony for the specified player on the target planet.
    void __cdecl Make_New_Colony(game::id_planet planet_id, int16_t player_idx);
    // Creates a new outpost for the specified player on the target planet.
    void __cdecl Make_New_Outpost(game::id_planet planet_id, int16_t player_idx);
} // namespace COLONIZE