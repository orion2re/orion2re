#pragma once

#include <cstdint>
#include "aidata_state.h"

namespace aitech {
    struct s_rules {
        uint8_t hypertech_threshold[TECH_APP_COUNT - TECH_APP_BIOLOGY] = {};
    };

    extern s_rules rules;

    /// Applies pending evolutionary trait upgrades for players that have selected one.
    void __cdecl Apply_Evolutionary_Upgrades(const game_world::s_view* world);

    /// Populates aggregate AI technology metrics for each active player.
    /// @param ai AI workspace receiving the computed technology data.
    void __cdecl Compute_AI_Tech_Info(AIDATA::s_state* ai, const game_world::s_view* world);

    /// Recomputes AI technology priorities for all active AI players.
    void __cdecl All_AI_Tech_Select(AIDATA::s_state* ai, const game_world::s_view* world);

    /// Chooses the next research application for a player when a reselection is needed.
    /// @param player Player whose research target may be updated.
    void __cdecl AI_Tech_Select(const game_world::s_view* world, s_player* player);

    /// Returns the highest researched ship shield index for a player.
    /// @param player Player whose researched shield technologies are checked.
    int32_t __cdecl Player_Best_Ship_Shield(const s_tech_shield_data* shields, const s_player* player);

    /// Returns the highest researched armor index for a player.
    /// @param player Player whose researched armor technologies are checked.
    int32_t __cdecl Player_Best_Armor(const s_tech_armor_data* armor, const s_player* player);

    /// Returns the highest researched rifle index for a player.
    /// @param player Player whose researched rifle technologies are checked.
    int32_t __cdecl Player_Best_Rifle(const s_tech_data_rifle* rifles, const s_player* player);

    /// Returns the highest researched personal shield index for a player.
    /// @param player Player whose researched personal shield technologies are checked.
    int32_t __cdecl Player_Best_Personal_Shield(const s_tech_data_personal_shield* shields, const s_player* player);

    /// Returns the highest researched battle computer index for a player.
    /// @param player Player whose researched computer technologies are checked.
    int32_t __cdecl Player_Best_Computer(const s_tech_computer_data* computers, const s_player* player);

    /// Returns the highest researched fuel-cell index for a player.
    /// @param player Player whose researched fuel technologies are checked.
    int32_t __cdecl Player_Best_Fuel(const s_tech_fuel_data* fuel, const s_player* player);

    /// Returns the best researched planetary shield tier for a player.
    /// @param player Player whose planetary shield technologies are checked.
    int32_t __cdecl Player_Best_Planetary_Shield(const s_player* player);

    /// Returns the highest researched engine drive index for a player.
    /// @param player Player whose researched drive technologies are checked.
    int32_t __cdecl Player_Best_Drive(const s_tech_drive_data* drives, const s_player* player);

    /// Counts nearby-system proximity pressure from another player's colonies.
    /// @param player_id Player whose nearby range is evaluated.
    /// @param other_player_id Player whose colonies contribute to the score.
    int16_t System_Proximity_Count(const game_world::s_view* world, uint32_t player_id, uint32_t other_player_id);

    /// Returns the scaled cost of the most advanced completed technology field for a player.
    /// @param player_idx Index of the player to inspect.
    int32_t Get_Max_Technology_Value(const game_world::s_view* world, uint32_t player_idx);
} // namespace aitech