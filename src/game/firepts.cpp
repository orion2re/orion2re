#include "pch.h"

namespace FIREPTS {
    uint8_t* fp_data_ptr[MAX_PLAYERS];
    uint8_t* fp_seg;

    void __cdecl Load_FP_Data_From_LBX() {
        fp_seg = (uint8_t*)allocate::Allocate_Next_Block(MOX::screen_seg, 0x1B90);

        for (int16_t i = 0; i < MAX_PLAYERS; ++i) {
            if (i == 0) {
                fp_data_ptr[i] = (uint8_t*)farload::Far_Reload_Data(
                    COLONY::FIREPTS_LBX, 0, fp_seg, i, 1, 0x310);
            } else {
                fp_data_ptr[i] = (uint8_t*)farload::Far_Reload_Next_Data(
                    COLONY::FIREPTS_LBX, 0, fp_seg, i, 1, 0x310);
            }
        }
    }

    int16_t __cdecl N_Fire_Points(int16_t weapon_id, int16_t shot_count) {
        int16_t result = 0;

        if (shot_count <= 10 || BEAMS::g_firing_reflected_beam_flag != 0) {
            result = 1;
        } else if (shot_count < 30) {
            result = 2;
        } else if (shot_count < 60) {
            result = 3;
        } else if (shot_count <= 98) {
            result = 4;
        } else if (shot_count == 99) {
            result = 5;
        } else {
            result = 0;
        }

        if (weapon_id == WEAPON_MAULER_DEVICE && result <= 2) {
            result = 2;
        }

        return result;
    }

    void __cdecl Calc_FP_Offsets(int16_t* fp_x, int16_t* fp_y, int16_t weapon_id, int16_t shot_count, game::owner_slot attacker, int16_t attacker_picture_num, uint8_t attacker_facing_dir, uint8_t target_bearing, int16_t fp_idx) {

        int16_t n_points = N_Fire_Points(weapon_id, shot_count);

        if ((n_points % 2) == 1 && (n_points - fp_idx) == 1) {
            *fp_x = 0;
            *fp_y = 0;
            return;
        }

        int16_t color_idx = 0;
        if (const auto player = factions::Player(attacker); player != game::NO_PLAYER) {
            color_idx = game::current_state.players[player.id].color;
        }

        int32_t point_idx = (int32_t)fp_idx + (uint8_t)target_bearing;
        while (point_idx < 0) {
            point_idx += 4;
        }
        point_idx = (point_idx % 4) + (int32_t)attacker_picture_num * 4;
        int16_t* fp_data = (int16_t*)fp_data_ptr[color_idx];

        int32_t offset_x = fp_data[point_idx * 2] - 0x1A;
        int32_t offset_y = fp_data[point_idx * 2 + 1] - 0x18;

        int32_t rotation_angle = ((int32_t)attacker_facing_dir * -360) / 16;

        angle::Rotate_Coordinates(rotation_angle, &offset_x, &offset_y);

        *fp_x = (int16_t)offset_x;
        *fp_y = (int16_t)offset_y;
    }
} // namespace FIREPTS
