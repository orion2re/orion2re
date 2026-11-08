#pragma once

#include "ai_world.h"

#include <cstdint>

namespace AIPOWER {
    /** Returns the best available ship shield class for the given player or special faction index. */
    int16_t __cdecl Get_Best_Ship_Shield_Value(const s_tech_shield_data* shields, int16_t player_idx);

    /**
     * Estimates a ship's combat strength against a target using a specific shield value.
     * @param ship_idx Attacking ship index.
     * @param target_idx Target player or faction index for defense calculations.
     * @param shield_strength Shield value applied to incoming damage.
     */
    int16_t __cdecl Ship_Strength_Vs_Shield(const AIDATA::s_world_view* world, int16_t ship_idx, int16_t target_idx, int16_t shield_strength);

    /** Returns the attacking ship's estimated combat strength against the target's best shields. */
    uint16_t __cdecl Ship_Strength_Vs_Ships(const AIDATA::s_world_view* world, int16_t attacker_idx, int16_t target_idx);

    /** Returns the attacking ship's estimated combat strength against a target in a nebula where shields are disabled. */
    uint16_t __cdecl Ship_Strength_Vs_Ships_In_Nebula(const AIDATA::s_world_view* world, int16_t attacker_idx, int16_t target_idx);

    /**
     * Calculates ship strength against each planetary shield tier.
     * @param ship_idx Attacking ship index.
     * @param shield_strengths Output array for the four shield tiers.
     */
    void __cdecl Ship_Strength_Vs_Planetary_Shield(const AIDATA::s_world_view* world, int16_t ship_idx, uint16_t* shield_strengths);

    /** Returns the maximum hit total for the ship under the current combat mode. */
    int16_t __cdecl Max_Ship_Hits(const AIDATA::s_world_view* world, int16_t ship_idx);

    /** Returns the ship's remaining hit total after structural and armor damage. */
    int16_t __cdecl Current_Ship_Hits(const AIDATA::s_world_view* world, int16_t ship_idx);

    /** Returns the generic ship attack bonus used for strategic strength estimates. */
    int16_t __cdecl Base_Generic_Ocv(const AIDATA::s_world_view* world, int16_t player_idx);

    /** Returns the estimated strength of a star base, battlestation, or fortress against ships. */
    uint16_t __cdecl Satelite_Strength_Vs_Ships(const AIDATA::s_world_view* world, int16_t building_type, int16_t owner_idx, int16_t target_player_idx);

    /** Returns a colony's space-defense strength against the specified attacking player. */
    uint16_t __cdecl Colony_Space_Strength_Vs_Player(const AIDATA::s_world_view* world, int16_t colony_idx, int16_t player_idx);

    /** Returns a colony's ground-defense strength against the specified attacking player. */
    uint16_t __cdecl Colony_Ground_Strength_Vs_Player(const AIDATA::s_world_view* world, int16_t colony_idx, int16_t player_idx);

    /** Returns the estimated missile-base strength for a defending player against the target player. */
    uint32_t __cdecl Missile_Base_Strength(const AIDATA::s_world_view* world, int16_t player_idx, int16_t target_idx);

    /** Returns the estimated phasor-base strength for a defending player against the target player. */
    uint32_t __cdecl Phasor_Base_Strength(const AIDATA::s_world_view* world, int16_t player_idx, int16_t target_idx);

    /** Returns the estimated fighter-garrison strength for a defending player against the target player. */
    uint16_t __cdecl Fighter_Garrison_Strength(const AIDATA::s_world_view* world, int16_t player_idx, int16_t target_player_idx);

    /** Returns the generic ship defense bonus used for strategic strength estimates. */
    int16_t __cdecl Base_Generic_Dcv(const AIDATA::s_world_view* world, int16_t player_idx);

    /** Scales a weapon-strength estimate by the enabled weapon modifications. */
    uint32_t __cdecl Modified_Weapon_Strength(const s_tech_weapon_mod_data* modifications, uint32_t weapon_strength, uint16_t modification_flags);

    /** Converts a hit total into the percentage modifier used by strength calculations. */
    int16_t __cdecl Strength_Hits_Modifier(int16_t strength);
} // namespace AIPOWER