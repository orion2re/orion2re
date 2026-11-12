#include "pch.h"

namespace CMBTDRW1 {
    int16_t __cdecl Relative_Bearing(int16_t ship_a_idx, int16_t ship_b_idx) {
        int16_t x1, y1;
        int16_t x2, y2;
        int16_t bearing;
        int8_t facing;

        // Get coordinates for ship A
        COMBINIT::Ship_Center_Offsets(ship_a_idx, &x1, &y1);
        x1 += MOX::combat_data[ship_a_idx].x * 20;
        y1 += MOX::combat_data[ship_a_idx].y * 20;

        // Get coordinates for ship B
        COMBINIT::Ship_Center_Offsets(ship_b_idx, &x2, &y2);
        x2 += MOX::combat_data[ship_b_idx].x * 20;
        y2 += MOX::combat_data[ship_b_idx].y * 20;

        // Calculate angle between ships. Y-axis is inverted in screen space calculation here.
        bearing = angle::Get_Angle(x2 - x1, y1 - y2);

        facing = MOX::combat_data[ship_a_idx].facing_dir;

        // Subtract ship A's heading from the absolute bearing.
        // Heading is in 16 discrete directions (0-15), where each unit is 22.5 degrees.
        // 22.5 * facing = 22 * facing + facing / 2
        bearing = bearing - (facing / 2) - (facing * 22);

        // Normalize angle to [0, 359]
        if (bearing < 0) {
            bearing += 360;
        }

        return bearing;
    }

    int16_t __cdecl Relative_Bearing_XY(int16_t ship_idx, int16_t target_x, int16_t target_y) {
        int16_t off_x;
        int16_t off_y;
        int16_t result_bearing;
        s_combat_data* ship;

        COMBINIT::Ship_Center_Offsets(ship_idx, &off_x, &off_y);

        ship = &MOX::combat_data[ship_idx];

        off_x += (int16_t)ship->x * 20;
        off_y += (int16_t)ship->y * 20;

        result_bearing = angle::Get_Angle((int32_t)(target_x - off_x), (int32_t)(off_y - target_y));

        result_bearing -= (int16_t)(ship->facing_dir / 2);
        result_bearing += (int16_t)(ship->facing_dir * -22);

        if (result_bearing < 0) {
            result_bearing += 360;
        }

        return result_bearing;
    }

    uint8_t __cdecl Get_Weapon_Arc(int16_t bearing) {
        uint8_t arc_flags = WEAPON_FIRING_ARC_NONE;

        // Front arc bit (approx. +/- 60 degrees)
        if (bearing <= 60 || bearing >= 300) {
            arc_flags = WEAPON_FIRING_ARC_FORWARD;
        }

        // Side-extended front arc bit (approx. +/- 120 degrees)
        if (bearing <= 120 || bearing >= 240) {
            arc_flags += WEAPON_FIRING_ARC_FORWARD_EXTENDED;
        }

        // Rear-extended back arc bit (approx. 60 to 300 degrees)
        if (bearing >= 60 && bearing <= 300) {
            arc_flags += WEAPON_FIRING_ARC_BACK_EXTENDED;
        }

        // Back arc bit (approx. 120 to 240 degrees)
        if (bearing >= 120 && bearing <= 240) {
            arc_flags += WEAPON_FIRING_ARC_REAR;
        }

        return arc_flags;
    }

    void __cdecl Get_Draw_Ship_Offsets(int16_t ship_idx, int16_t* out_x, int16_t* out_y) {
        *out_x = 0;
        *out_y = 0;

        if (ship_idx == 0) {
            if (MOX::cmbt_colony_id == -1) {
                *out_x = 7;
                *out_y = 7;
                return;
            }

            static const int16_t planet_offsets[PLANET_SIZE_COUNT][2] = {
                {2, 2},
                {0, 1},
                {7, 7},
                {4, 4},
                {6, 6},
            };

            int16_t planet_size =
                MOX::planet[MOX::colony[MOX::cmbt_colony_id].planet].size;
            if (planet_size < PLANET_SIZE_TINY ||
                planet_size >= PLANET_SIZE_COUNT) {
                return;
            }

            *out_x = planet_offsets[planet_size][0];
            *out_y = planet_offsets[planet_size][1];
            return;
        }

        int8_t facing_dir = MOX::combat_data[ship_idx].facing_dir;

        switch ((MOX::combat_data[ship_idx].size_class + 1) / 2) {
            case 0:
                *out_x = -16;
                *out_y = -16;
                break;
            case 1:
                *out_x = -6;
                *out_y = -6;
                break;
            case 2:
            case 3:
                *out_x = 4;
                *out_y = 4;
                break;
            default:
                break;
        }

        int facing_quadrant = (facing_dir - 1) / 4;
        if (facing_quadrant <= 0) {
            facing_quadrant = 0;
        }

        if (facing_quadrant == 1) {
            *out_x -= 2;
        } else if (facing_quadrant == 2) {
            *out_x -= 2;
            *out_y -= 1;
        } else if (facing_quadrant == 3) {
            *out_y -= 1;
        }

        *out_x -= 2;
        *out_y -= 3;
    }
} // namespace CMBTDRW1
