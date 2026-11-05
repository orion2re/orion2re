#pragma once
#include <cstdint>

#define COMBAT_PLANET_DEFENSE_MISSILE_BASE 0x1
#define COMBAT_PLANET_DEFENSE_GROUND_BATTERIES 0x2
#define COMBAT_PLANET_DEFENSE_STELLAR_CONVERTER 0x4
#define COMBAT_PLANET_DEFENSE_FIGHTER_GARRISON 0x8

namespace CMBTFIRE {
    struct s_weapon_range {
        uint8_t proton_torpedo;
        uint8_t dragon_breath;
        uint8_t anti_missile_rocket;
        uint8_t gyro_destabilizer;
        uint8_t tractor_beam;
        uint8_t plasma_web;
        uint8_t caustic_slime;
        uint8_t stasis_field;
        uint8_t black_hole_generator;
    };

    struct s_spherical_range_to_ship {
        uint8_t pulsar;
        uint8_t spatial_compressor;
        uint8_t plasma_flux;
        uint8_t drive_explosion;
    };

    struct s_spherical_range_to_missile {
        uint32_t pulsar;
        uint32_t spatial_compressor;
        uint32_t plasma_flux;
        uint32_t drive_explosion;
    };

    typedef enum SPHERICAL_EFFECT {
        SPHERICAL_EFFECT_PULSAR = 0,
        SPHERICAL_EFFECT_SPATIAL_COMPRESSOR = 1,
        SPHERICAL_EFFECT_PLASMA_FLUX = 2,
        SPHERICAL_EFFECT_DRIVE_EXPLOSION = 3,
    } SPHERICAL_EFFECT;

    enum e_high_energy_focus_presence : uint8_t {
        HIGH_ENERGY_FOCUS_ABSENT = 0,
        HIGH_ENERGY_FOCUS_PRESENT = 1,
        HIGH_ENERGY_FOCUS_PRESENCE_COUNT = 2,
    };

    struct s_mount_damage {
        uint16_t pd;
        uint16_t nomod;
        uint16_t heavy;
    };

    extern s_mount_damage mount_dmg[HIGH_ENERGY_FOCUS_PRESENCE_COUNT];

    extern uint16_t civilian_armor[ARMOR_COUNT];
    extern uint16_t ground_defense_armor_multiplier;
    extern uint8_t marine_kill_damage;
    extern uint8_t marine_kill_damage_blocker;
    extern uint16_t analyzer_bonus;
    extern uint8_t fighter_beam_no_plus_one;
    extern uint8_t fighters_use_point_defense;
    extern uint8_t heavy_fighter_can_scramble;
    extern uint8_t double_range_accuracy_penalty;
    extern uint8_t double_range_damage_penalty;
    extern uint8_t planet_non_enveloping;
    extern uint8_t simplified_beam_formula;
    extern uint8_t hit_threshold_fixed_value;
    extern uint8_t ion_affects_antarans;
    extern uint8_t absorber_damage_reduction;
    extern uint8_t damper_field_ratio;
    extern uint8_t defensive_range_bonus;
    extern uint8_t broken_engine_explodes;
    extern s_weapon_range weapon_range;
    extern s_spherical_range_to_ship spherical_range_to_ship;
    extern s_spherical_range_to_missile spherical_range_to_missile;

    /** Returns whether Enveloping damage should use all four directions for this target. */
    bool Enveloping_Applies_To_Target(int16_t target_ship_idx);

    /** Returns the incoming damage blocked and stored by an Energy Absorber. */
    int32_t Energy_Absorber_Blocked_Damage(int32_t damage);

    /** Returns the damage remaining after applying a Damper Field. */
    int32_t Damper_Field_Remaining_Damage(int32_t damage);

    /** Returns one planetary ground-defense building's armor-scaled structure. */
    int32_t Ground_Defense_Structure(int16_t armor_bonus);

    /** Applies the supplied weapon damage to the defending planet or colony. */
    int16_t __cdecl Apply_Damage_To_Planet(int16_t* damage, uint16_t specials, uint16_t weapon_flags);

    /** Destroys up to the requested number of colony defense structures at the current combat planet. */
    void __cdecl Destroy_Colony_Defense(int16_t n_to_destroy);

    /** Returns the current combat death state for the specified ship. */
    int16_t __cdecl Is_Combat_Ship_Dead(int16_t ship_idx);

    /** Finalizes destruction or disabling for a combat ship and triggers any matching effects. */
    void __cdecl Destroy_Ship(int16_t ship_idx, int16_t draw_x, int16_t draw_y, int16_t death_state);

    /** Finalizes ship destruction and identifies the live combat ship responsible when known. */
    void __cdecl Destroy_Ship_By(int16_t ship_idx, int16_t draw_x, int16_t draw_y, int16_t death_state, int16_t killer_ship_idx);

    /** Fires all legal ready weapons from the source ship at the selected target. */
    int16_t __cdecl Fire_Ship(int16_t source_ship_idx, int16_t sx, int16_t sy, int16_t target_ship_idx, int16_t tx, int16_t ty, int16_t flag);

    /** Triggers pre-fire weapon specials that activate before the main attack resolution. */
    void __cdecl Pre_Weapon_Specials(int16_t unit_idx, int16_t target_idx, int16_t target_type);

    /** Converts reflected damage into an Energy Absorber counterattack against the target ship. */
    void __cdecl Energy_Absorber_Attack(int16_t unit_idx, int16_t target_idx);

    /** Resolves a beam weapon shot against a ship or missile target. */
    int16_t __cdecl Fire_Beam_Weapon(int16_t source, int16_t target, int16_t weapon_id, uint16_t specials, uint16_t weapon_flags, int16_t* hit_x, int16_t* hit_y, int16_t* damage);

    /** Computes beam hit and damage modifiers for the given range and special flags. */
    void __cdecl Get_Beam_Weapon_Modifiers(int16_t ship_idx, int16_t range, uint16_t wpn_modifier_flags, int16_t wpn_idx, int32_t* hit_bonus_out, int32_t* dmg_mod_out);

    /** Resolves one ship weapon slot as a bombing attack against the defending planet. */
    int16_t __cdecl Fire_Bomb(int16_t ship_idx, int16_t unused_target_idx, int16_t weapon_slot_idx);

    /** Resolves a special-weapon activation against the selected ship or missile target. */
    void __cdecl Fire_Special_Weapon(int16_t ship_idx, int16_t x, int16_t y, int16_t target_idx, int16_t weapon_slot_idx, int16_t target_type);

    /** Applies the Black Hole Generator effect to the target ship. */
    void __cdecl BHG(int16_t source_idx, int16_t target_idx);

    /** Clears the stasis state and weapon availability effects from a ship. */
    void __cdecl Remove_Stasis_Beam(int16_t ship_idx);

    /** Retargets or destroys missiles that were aiming at the specified unit. */
    void __cdecl Retarget_Missiles_Aimed_At(int16_t unit_idx);

    /** Places the target ship into stasis and records the source ship. */
    void __cdecl Place_Ship_In_Stasis(int16_t ship_idx, int16_t source_idx);

    /** Returns a bitfield describing which planetary defenses are currently present. */
    int16_t __cdecl Does_Combat_Planet_Have_Defenses();

    /** Returns whether the specified weapon may legally attack the selected target. */
    int16_t __cdecl Target_Is_Legal_For_Weapon(int16_t weapon_id, uint16_t specials, int16_t target_idx, int16_t target_type);

    /** Maps a relative bearing angle to the shield-facing index used for damage application. */
    int16_t __cdecl Facing_Shield(int16_t angle);

    /** Returns the beam range penalty or bonus for the specified ship and range. */
    int16_t __cdecl Get_Beam_Range_To_Hit_Bonus(int16_t ship_idx, int16_t range);

    /** Applies weapon damage to a combat ship, including shields, armor bypass, and internals. */
    uint16_t __cdecl Apply_Ship_Damage(
        int16_t target_ship_idx,
        int16_t* dmg_out,
        int16_t* sys_dmg,
        int16_t shield_arc,
        uint16_t specials,
        uint16_t weapon_flags,
        int16_t has_structural_analyzer,
        int16_t force_armor_bypass,
        int16_t internal_only);

    /** Resolves fighter-beam attacks launched by the specified missile stack. */
    void __cdecl Fire_Fighter_Beam(int16_t missile_idx, int16_t shield_arc, int16_t target_is_missile, int16_t* total_damage, int16_t* total_struct_damage);

    /** Resolves fighter-bomb attacks launched by the specified missile stack. */
    void __cdecl Fire_Fighter_Bomb(int16_t missile_idx, int16_t damage_modifier, int16_t* total_damage, int16_t* total_internal_damage);

    /** Returns whether the indexed combat special is currently active on the ship. */
    bool __cdecl Special_Is_On(int16_t ship_idx, int16_t weapon_idx);

    /** Returns whether the requested special-device bit corresponds to an enabled active special. */
    int8_t __cdecl Special_Is_Turned_On(int16_t ship_idx, int16_t special_idx);

    /** Returns whether the specified weapon class may target missiles. */
    int16_t __cdecl Weapon_Can_Fire_At_Missiles(int16_t weapon_idx, uint16_t flags);

    /** Returns whether the selected weapon slot can currently reach the target. */
    int16_t __cdecl Weapon_In_Range(int16_t weapon_slot_idx, int16_t ship_idx, int16_t target_idx, int16_t target_type, int16_t range);

    /** Returns whether a spherical effect reaches the selected ship or flyer. */
    bool Spherical_Effect_In_Range(
        int16_t source_ship_idx,
        int16_t target_idx,
        int16_t target_type,
        SPHERICAL_EFFECT effect);

    /** Applies damage to the leading missile in a missile stack and reports destroyed missiles. */
    void __cdecl Apply_Damage_To_Missile(int16_t missile_idx, int16_t* damage_io, uint16_t specials, uint16_t weapon_flags);

    /** Draws the Energy Absorber combat effect for the specified ship. */
    void __cdecl Draw_Energy_Absorber(int16_t ship_idx, int16_t x, int16_t y);

    /** Draws the Black Hole Generator overlay for the specified ship. */
    void __cdecl Draw_BHG(int16_t ship_idx, int16_t x_adj, int16_t y_adj);
    void Record_Unit_Hit(int16_t slot, int32_t amount, int32_t secondary_amount = 0, bool biological = false);
} // namespace CMBTFIRE