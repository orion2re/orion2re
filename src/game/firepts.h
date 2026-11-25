#pragma once

namespace FIREPTS {
    extern uint8_t* fp_data_ptr[MAX_PLAYERS];
    extern uint8_t* fp_seg;

    /* Loads the FIREPTS.LBX fire-point tables into the shared buffer. */
    void __cdecl Load_FP_Data_From_LBX();

    /*
     * Returns how many fire points to use for a volley.
     * weapon_id: Weapon identifier for special-case handling.
     * shot_count: Number of shots in the volley.
     */
    int16_t __cdecl N_Fire_Points(int16_t weapon_id, int16_t shot_count);

    /*
     * Computes one rotated local fire-point offset for a volley.
     * fp_x: Receives the horizontal offset.
     * fp_y: Receives the vertical offset.
     * weapon_id: Weapon identifier used to choose the fire-point pattern.
     * shot_count: Number of shots in the volley.
     * attacker_owner: Player index used to select the fire-point color set.
     * attacker_ship_id: Unused ship identifier.
     * attacker_picture_num: Sprite frame used to choose the source point pair.
     * attacker_facing_dir: Facing direction used for rotation.
     * target_bearing: Relative bearing bucket added to the point index.
     * fp_idx: Zero-based fire-point index within the volley.
     */
    void __cdecl Calc_FP_Offsets(int16_t* fp_x, int16_t* fp_y, int16_t weapon_id, int16_t shot_count, game::owner_slot attacker, int16_t attacker_picture_num, uint8_t attacker_facing_dir, uint8_t target_bearing, int16_t fp_idx);
} // namespace FIREPTS