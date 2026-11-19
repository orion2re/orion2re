#pragma once
#include <cstdint>
#include <span>

namespace COLONIZE {
    extern uint8_t first_pop_to_production;

    // Assigns available capacity in priority order. Neither the input nor the
    // output identifies a colony; entries in the output are player numbers.
    size_t Plan_Colony_Reservations(std::span<const int16_t> demand,
        size_t first_player, std::span<int16_t> reserved_for_player);

    // Allocates free colony slots to players based on pending colonization opportunities.
    void Allocate_New_Colony_Slots();
    // Runs AI colonization for every active AI player.
    void __cdecl All_AI_Colonize();

    // Determines which player, if any, has colonization priority at each star.
    void Find_Colonize_Players(::game::s_world_state* world_state);

    // Counts planets at a star that the specified player can colonize.
    int16_t Star_N_Colonizable_Planets_For_Player(::game::s_world_state* world_state, s_star_data* star, int16_t player_idx);

    // Counts uncolonized planets at a star that are valid outpost targets.
    int16_t Star_N_Outpostable_Planets(::game::s_world_state* world_state, s_star_data* star);

    // Counts uncolonized planets present at a star.
    int16_t Star_N_Uncolonized_Planets(::game::s_world_state* world_state, s_star_data* star);

    // Performs colonization decisions for one AI player.
    void __cdecl AI_Colonize(::game::s_world_state* world_state, int16_t player_idx);

    // Creates a new colony for the specified player on the target planet.
    void __cdecl Make_New_Colony(::game::id_planet planet_idx, int16_t player_idx);
    // Creates either a colony or an outpost for the specified player on the target planet.
    void __cdecl Make_New_Colony_Or_Outpost(::game::s_world_state* world_state, ::game::id_planet planet_idx, int16_t player_idx, int16_t is_outpost);
    // Creates a new outpost for the specified player on the target planet.
    void __cdecl Make_New_Outpost(::game::id_planet planet_idx, int16_t player_idx);
} // namespace COLONIZE