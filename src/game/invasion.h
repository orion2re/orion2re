#pragma once

#include <span>
#include "object_ids.h"

namespace invasion {
    extern uint8_t ground_commando_defender_x5;
    extern uint8_t ground_commando_attacker_x2;

    struct s_technology_capture_chance {
        uint16_t building_player;
        uint8_t building_ai;
        uint16_t population_player;
        uint8_t population_ai;
    };

    struct s_settings {
        s_technology_capture_chance technology_chance;
    };

    extern s_settings settings;

    struct s_troop_type {
        uint8_t tanks;
        int8_t militia;
        int8_t rebels;
    };

    struct s_troop_bonus {
        int8_t low_g;
        uint8_t subterranean;
        uint8_t security_stations;
        uint8_t anti_grav_harness;
        uint8_t personal_shield;
        uint8_t powered_armor;
        uint8_t battleoids;
        uint8_t high_g_hits;
        uint8_t powered_armor_hits;
        uint8_t battleoids_hits;
    };

    struct s_troop_rules {
        s_troop_type type;
        s_troop_bonus bonus;
    };

    extern s_troop_rules troop;

    /// Unloads transport ships into a colony until its infantry cap is reached.
    int Unload_Transports(s_colony& colony, std::span<const game::id_ship> transports);

    /// Returns the maximum infantry garrison the colony can support.
    int32_t __cdecl Colony_Infantry_Limit(s_colony* colony);

    /// Computes player-wide ground combat modifiers for either the attacker or defender role.
    void __cdecl Compute_Player_Ground_Combat_Bonuses(int player_idx, s_invasion_player_bonuses* bonuses, int8_t is_defender);

    /// Checks whether a colony triggers a rebellion this turn.
    void __cdecl Check_Rebellion(game::id_colony colony_id, s_colony* colony);

    /// Builds attacker and defender combat state for a colony rebellion.
    void __cdecl Get_Rebellion_Info(int16_t attacker_player_id, int16_t marines_count, game::id_colony colony_id, s_colony* colony, s_invasion_side_info* side_a, s_invasion_side_info* side_b);

    /// Returns the number of militia units a colony can field from its population.
    int __cdecl Colony_N_Militia(s_colony* colony);

    /// Applies the best eligible officer bonus from the attacking ships.
    void Compute_Attacker_Officer_Bonus(std::span<const game::id_ship> keys, s_invasion_player_bonuses* bonuses);

    /// Applies the defending colony officer bonus, if present.
    void __cdecl Compute_Colony_Officer_Bonus(s_colony* colony, s_invasion_player_bonuses* bonuses);

    /// Transfers a colony to a new owner and records battle-result side effects.
    void __cdecl Change_Colony_Ownership(game::id_colony colony_id, s_colony* colony, game::id_player new_owner, s_battle_results_info* results);

    /// Updates colony population ownership flags after a capture.
    void __cdecl Change_Pop_Ownership(game::id_colony colony_idx, game::id_player new_owner);

    /// Restores surviving rebel troops to the captured colony.
    void __cdecl Resolve_Rebellion_Troops(s_invasion_side_info info);

    /// Removes excess population entries until the colony satisfies race limits.
    void __cdecl Enforce_Population_Limits_At_Colony(s_colony* colony);

    /// Runs rebellion checks across all active colonies.
    void __cdecl Check_All_Rebellions();

    /// Advances the side's active troop type to the next available unit.
    void __cdecl Find_Troops(s_invasion_side_info* side_info);

    /// Reassigns a player's home planet after losing their current one.
    void __cdecl Assign_New_Home_Planet(const game::s_game_state& state, int8_t player_idx, game::id_colony excluded_colony);

    /// Converts combat modifiers into per-unit ground combat strength and hit point values.
    void __cdecl Compute_Ground_Combat_Info(s_invasion_side_info* side, s_invasion_player_bonuses* own_mods, s_invasion_player_bonuses* opponent_mods);

    /// Builds attacker and defender combat state for a boarding action.
    void __cdecl Get_Boarding_Info(int16_t attacker_player_id, int16_t attacker_unit_count, int16_t attacker_additional_modifier, int16_t defender_player_id, int16_t defender_unit_count, int16_t defender_additional_modifier, s_invasion_side_info* side_a, s_invasion_side_info* side_b);

    /// Resolves one simultaneous round of ground combat.
    void __cdecl Ground_Combat_Round(s_invasion_side_info* side1, s_invasion_side_info* side2);

    /// Resolves ground combat until one side runs out of available troop types.
    void __cdecl Resolve_Ground_Combat(s_invasion_side_info* side_a, s_invasion_side_info* side_b);

    /// Returns the tank cap imposed by tank-barracks-style colony limits.
    int __cdecl Colony_Tank_Barracks_Limit(s_colony* colony);

    /// Returns the maximum tank garrison the colony can support.
    int32_t __cdecl Colony_Tank_Limit(s_colony* colony);

    /// Returns the infantry cap imposed by infantry-barracks-style colony limits.
    int16_t __cdecl Colony_Infantry_Barracks_Limit(s_colony* colony);

    /// Builds attacker and defender combat state for a colony invasion.
    void Get_Invasion_Info(int16_t attacker, std::span<const game::id_ship> keys, game::id_colony colony_id, s_colony* colony, s_invasion_side_info* attacker_side, s_invasion_side_info* defender_side);

    /// Resolves a full colony invasion and applies the result.
    bool Invade(int16_t attacker, std::span<const game::id_ship> keys, game::id_colony colony_idx, s_battle_results_info* results);

    /// Builds ground combat state for an existing colony garrison.
    void __cdecl Compute_Colony_Ground_Combat_Info(game::id_colony colony_id, s_colony* colony, s_invasion_side_info* side_info, bool include_rebels);
} // namespace invasion
