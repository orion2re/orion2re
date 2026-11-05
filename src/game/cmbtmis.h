#pragma once
#include <cstdint>

namespace CMBTMIS {
    extern int8_t missile_initiative;
    extern bool missile_cross_round_merge;
    extern bool smarter_fighter_retargeting;
    extern uint16_t fighter_max_pursuit_turns;

    enum e_fighter_pursuit_fallback : uint8_t {
        FIGHTER_PURSUIT_RETURN,
        FIGHTER_PURSUIT_PURSUE,
    };

    extern e_fighter_pursuit_fallback fighter_pursuit_fallback;

    struct s_missile_speed_base {
        uint16_t nuclear_missile;
        uint16_t merculite_missile;
        uint16_t pulson_missile;
        uint16_t zeon_missile;
        uint16_t anti_matter_torpedo;
        uint16_t proton_torpedo;
        uint16_t plasma_torpedo;
        uint16_t plasma_breath;
        uint16_t interceptor;
        uint16_t assault_shuttle;
        uint16_t bomber;
        uint16_t heavy_fighter;
    };

    extern s_missile_speed_base speed_base;
    extern uint8_t proton_torpedo_apply_speed_base;
    extern uint8_t speed_fast_bonus;
    extern uint8_t plasma_torpedo_dissipation;

    enum e_lightning_field_mode : uint8_t {
        LIGHTNING_FIELD_MODE_CLASSIC,
        LIGHTNING_FIELD_MODE_UNIFORM,
    };

    extern uint8_t lightning_field_ratio;
    extern e_lightning_field_mode lightning_field_mode;

    struct s_flyer_hit_points {
        uint8_t nuclear_missile;
        uint8_t merculite_missile;
        uint8_t pulson_missile;
        uint8_t zeon_missile;
        uint8_t interceptor;
        uint8_t assault_shuttle;
        uint8_t bomber;
        uint8_t heavy_fighter;
    };

    extern s_flyer_hit_points flyer_hit_points;

    struct s_fighter_shots {
        uint8_t interceptor;
        uint8_t bomber;
        uint8_t heavy_fighter;
    };

    extern s_fighter_shots fighter_shots;

    struct s_flyer_combat_modifier {
        int8_t nuclear_missile_defense_bonus;
        int8_t merculite_missile_defense_bonus;
        int8_t pulson_missile_defense_bonus;
        int8_t zeon_missile_defense_bonus;
        int32_t fighters_offense_bonus;
    };

    extern s_flyer_combat_modifier flyer_combat_modifier;

    struct s_missile_defense {
        uint8_t inertial_stabilizer_missile_defense;
        uint8_t inertial_nullifier_missile_defense;
        uint8_t ecm_jammer_missile_defense;
        uint8_t multi_wave_jammer_missile_defense;
        uint8_t wide_area_jammer_missile_defense;
        uint8_t wide_area_jammer_fleet_missile_defense;
        uint8_t cloaking_device_missile_defense;
        uint8_t cloaking_device_missile_miss_percent;
        uint8_t displacement_device_missiles_miss_percent;
        uint8_t tachyon_scanner;
        uint8_t neutron_scanner;
        uint8_t sensors;
    };

    extern s_missile_defense missile_defense;

    inline bool Missile_Defense_Roll_Succeeds(uint32_t roll, uint8_t chance) {
        return roll <= chance;
    }

    inline int16_t Final_Flyer_Target(const s_missile* missiles, int16_t missile_idx) {
        const s_missile* missile = &missiles[missile_idx];
        while (missile->target_is_missile != 0) {
            missile = &missiles[missile->target_unit_idx];
        }
        return missile->target_unit_idx;
    }

    inline bool Missile_Targets_Ship(const s_missile* missile, int16_t ship_idx) {
        return missile->type > WEAPON_NO_WEAPONS &&
            missile->target_is_missile == 0 &&
            missile->target_unit_idx == ship_idx;
    }

    extern int16_t missile_group_offset[16][16];
    extern int16_t torpedo_group_offset[16][8];

    /** Returns the display group used for a missile entry's animation. */
    int16_t __cdecl Missile_Type(int16_t missile_idx);

    /** Returns whether a combat-grid coordinate lies inside the current viewport. */
    int8_t __cdecl On_Combat_Screen(int16_t x, int16_t y);

    /** Computes a step-sized movement delta from one missile-space position toward another. */
    void __cdecl Coord_Movement(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t dist, int16_t* dx, int16_t* dy);

    /** Returns the owning player id for a missile entry. */
    int16_t __cdecl Missile_Owner(int16_t missile_idx);

    /** Retargets a missile or fighter group when its current target is no longer valid. */
    void __cdecl Retarget_Missile(int16_t missile_idx);

    /** Clears transient pursuit observations at the start of a tactical battle. */
    void Reset_Fighter_Pursuit_History();

    /** Observes pursuit progress and reconsiders a sortie once per combat round. */
    void Reconsider_Fighter_Target(int16_t missile_idx);

    /** Advances a missile toward a target point and reports whether it reached hit range. */
    char __cdecl Move_Missile(int16_t missile_idx, int16_t target_x, int16_t target_y, int16_t move_dist);

    /** Returns whether a missile is within hit range of a target point. */
    int8_t __cdecl Did_Missile_Hit(int16_t missile_idx, int16_t x, int16_t y);

    /** Computes the facing from a missile toward a target point. */
    int16_t __cdecl Get_Facing(int16_t missile_idx, int16_t target_x, int16_t target_y);

    /** Returns the incoming bearing used to select a missile impact's shield arc. */
    int16_t __cdecl Missile_Shield_Bearing(int16_t missile_idx, int16_t target_idx);

    /** Returns the movement speed for a missile or fighter weapon type. */
    int32_t __cdecl Missile_Speed(int16_t type, uint16_t specials, int16_t owner);

    /** Returns Plasma Torpedo damage after distance-based dissipation. */
    int64_t Plasma_Torpedo_Remaining_Damage(
        const s_missile* missile, int64_t base_damage, bool resolving_impact);

    /** Returns the hit strength associated with a missile or fighter weapon id. */
    int32_t __cdecl Get_Missile_Hits(int16_t missile_id, int16_t ship_idx, uint16_t flags);

    /** Returns whether a missile currently lies inside the visible combat viewport. */
    int8_t __cdecl Missile_On_Combat_Screen(int16_t missile_idx);

    /** Distributes returning fighter-bay units among surviving weapon slots. */
    void __cdecl Land_Returning_Fighters(
        s_combat_data* source, int16_t type, int16_t returned_count);

    /** Resolves a missile impact or fighter attack sequence at the target point. */
    int16_t __cdecl Resolve_Missile(
        int16_t missile_idx,
        int16_t x,
        int16_t y,
        bool is_seeking = false);

    /** Returns whether the current target for a missile is still valid and alive. */
    int16_t __cdecl Is_Missile_Target_Alive(int16_t missile_idx);

    /** Returns the shield arc index with the lowest current strength. */
    int16_t __cdecl Find_Weakest_Shield(int16_t ship_idx);

    /** Plays fighter attack visuals and damage messaging for a resolved strike. */
    void __cdecl Fighter_Combat_SFX(int16_t missile_idx, int16_t x, int16_t y, int16_t dmg, int16_t sys_dmg);

    /** Expands a missile group into per-sprite screen offsets. */
    void __cdecl Get_Multiple_Missile_Coords(int16_t missile_idx, int16_t quantity, int16_t* out_x, int16_t* out_y);

    /** Expands a torpedo group into per-sprite screen offsets. */
    void __cdecl Get_Multiple_Torpedo_Coords(int16_t missile_idx, int16_t group_size, int16_t* out_x, int16_t* out_y);

    /** Draws the fighter sprites for a fighter-group missile entry. */
    void __cdecl Draw_Fighter(int16_t missile_idx, int16_t base_x, int16_t base_y, int16_t combat_view_type);

    /** Clears a missile entry and retargets anti-missile rockets that were tracking it. */
    void __cdecl Kill_Missile(int16_t missile_idx);

    /** Merges a launched missile stack with the first compatible existing stack. */
    void __cdecl Merge_Missile(int16_t missile_idx);

    /** Returns how many sprites should be drawn for a missile entry. */
    int16_t __cdecl Picture_Count(int16_t missile_idx);

    /** Draws a missile or fighter group at the supplied screen position. */
    void __cdecl Draw_Missile(int16_t missile_idx, int16_t screen_x, int16_t screen_y, int16_t draw_fullscreen_view);

    /** Spawns and launches a missile or fighter group from a ship weapon slot. */
    void __cdecl Fire_Missile(int16_t source_idx, int16_t target_idx, int16_t weapon_slot, int16_t is_anti_rocket);

    /** Draws the status panel for a selected missile or fighter group. */
    void __cdecl Draw_Missile_Status(int16_t x_off, int16_t y_off, int16_t missile_idx);

    /** Draws all active missiles in back-to-front order for the current combat view. */
    void __cdecl Draw_Missiles();

    /** Returns the offensive combat value used for fighter attacks. */
    int64_t __cdecl Fighter_Ocv(int16_t player_idx);

    /** Returns the defensive combat value for a missile or fighter weapon type. */
    int64_t __cdecl Missile_Dcv(int16_t weapon_idx, int16_t player_idx);

    /** Finds the next free slot in the combat missile table. */
    void __cdecl Next_Missile_Id(int16_t* missile_id, int16_t owner = -1);

    /** Computes the initial facing for a missile toward its current target. */
    int16_t __cdecl Missile_Facing(int16_t missile_idx, int16_t target_idx, int16_t target_is_missile);
    void Record_Missile_Hit(int16_t slot, int32_t destroyed);
} // namespace CMBTMIS