#pragma once
#include <cstdint>

namespace CMBTAI {
    extern uint8_t ships_without_ammo_stay;
    extern uint8_t human_fleet_auto_retreat;

    /** Returns the AI combat posture for the specified player. */
    int8_t __cdecl Get_Player_Mode(int16_t player_idx);

    /** Checks whether the specified player should retreat from tactical combat. */
    int16_t __cdecl Retreat_Check(int16_t player_idx);

    /** Returns the effective per-shot damage value used by AI weapon scoring. */
    int16_t __cdecl Get_Effective_Weapon_Damage(int16_t ship_idx, int16_t slot_idx);

    /** Returns the total remaining survivability score for a combat unit. */
    int32_t __cdecl Get_Total_Hits(int16_t ship_idx);

    /** Estimates total missile pressure for a player and reports leftover beam strength. */
    int16_t __cdecl Get_Effective_Missile_Strength(int16_t player_idx, int16_t* beam_strength);

    /** Estimates how many missiles a point-defense weapon can remove from a volley. */
    int16_t __cdecl Average_Missile_Kills(int16_t ship_idx, int16_t weapon_slot_idx, int16_t missile_count, int16_t missile_type, int16_t* remaining_pd_count, int16_t missile_idx);

    int16_t __cdecl Get_Long_Range_Beam_Strength(int16_t player_idx);

    int16_t __cdecl Get_Effective_Beam_Strength(int16_t player_idx);

    void __cdecl Toggle_Auto_Combat_Flags();
    void Refresh_Auto_Control_Input();

    void __cdecl Do_Auto_Ship_Turn(void);

    int16_t __cdecl Is_Ship_Out_Of_Ammo(int16_t ship_idx);

    int16_t __cdecl Check_For_Launched_Fighters(int16_t source_unit_idx);

    void __cdecl Release_Smallest_Ship_From_Stasis(int16_t owner_idx);

    int16_t __cdecl Ai_Stasis_Check(int16_t player_idx);

    int16_t __cdecl Select_Ship_To_Target(int16_t ship_idx);

    int32_t __cdecl Target_Ship_Value(int16_t attacker_idx, int16_t target_idx, int16_t distance_flag);

    int16_t __cdecl Missile_Overkill_Check(int16_t target_idx);

    int16_t __cdecl Choose_Target(int16_t ship_idx);

    int8_t __cdecl Combat_Ship_Class(int16_t ship_idx, int16_t target_idx);

    int16_t __cdecl Auto_Move_Ship(int16_t ship_idx, int16_t target_idx);

    bool __cdecl Does_Ship_Have_Missiles_Incoming(int16_t ship_idx);

    /** Selects the best current target, preferring either ships or missiles by score. */
    int16_t __cdecl Get_Current_Target(int16_t ship_idx, int16_t reference_target_idx);

    int16_t __cdecl Boarding_Action_Type(int16_t attacker_idx, int16_t target_idx);

    void __cdecl Resolve_Ai_Boarding(int16_t attacker_idx, int16_t target_idx, int16_t action_type);

    int16_t __cdecl Is_Ship_Targeted_By_Missile(int16_t ship_idx);

    void __cdecl Crystal_Control(int16_t ship_idx);

    int8_t __cdecl Ai_Self_Destruct_Check(int16_t ship_idx);

    /** Returns ship edge-to-edge distance in tactical squares. */
    int16_t __cdecl Range_To_Ship_Squares(int16_t ship1_idx, int16_t ship2_idx);

    int16_t __cdecl Range_To_Ship(int16_t ship1_idx, int16_t ship2_idx);

    int16_t __cdecl Range_To_XY(int16_t ship_idx, int16_t target_x_px, int16_t target_y_px);

    /** Returns corner-origin ship-to-point distance in legacy tactical range units. */
    int16_t __cdecl Range_To_XY_From_Corner(int16_t ship_idx, int16_t target_x_px, int16_t target_y_px);

    int16_t __cdecl Range_To_Missile(int16_t unit_idx, int16_t missile_idx);

    int16_t __cdecl Will_Missile_Do_Damage(int16_t target_idx, int16_t missile_idx);

    bool __cdecl Safe_To_Fire_Sphere_Weapon(int16_t source_ship_idx);

    /** Scores a missile target for interception priority. */
    int32_t __cdecl Target_Missile_Value(int16_t unit_idx, int32_t missile_idx, int16_t range_sensitive);

    extern int16_t fleet_target;

    /** Returns the best missile target and writes its priority score. */
    int16_t __cdecl Get_Current_Missile_Target(int16_t unit_idx, int32_t* value_out);

    /** Returns the best ship target and writes its priority score. */
    int16_t __cdecl Get_Current_Ship_Target(int16_t current_ship_idx, int16_t reference_target_idx, int32_t* out_value);

    int32_t __cdecl Expected_Beam_Bomb_Damage(int16_t attacker_idx, int16_t weapon_slot, int16_t target_idx, int16_t count, int16_t distance, int16_t target_type_flag);

    /** Estimates the damage a missile volley is expected to inflict on a target ship. */
    int16_t __cdecl Expected_Missile_Damage(int16_t target_ship_idx, int16_t weapon_id, int16_t weapon_count, uint16_t weapon_specials, int16_t attacker_player_idx);

    int16_t __cdecl Expected_Weapon_Damage(int16_t source_idx, int16_t weapon_slot, int16_t target_idx, int16_t target_type, int16_t range);

    /** Tracks the best-scoring move coordinates found for the current AI move search. */
    int16_t __cdecl Evaluate_Possible_Move(int16_t score, int16_t move_x, int16_t move_y, int16_t move_type, int16_t* best_move_count, int16_t* best_move_xs, int16_t* best_move_ys, int16_t best_score);

    int16_t __cdecl Convert_Ordinal_To_Msl_Type(int16_t ordinal);
} // namespace CMBTAI