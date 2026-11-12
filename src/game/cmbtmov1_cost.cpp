#include "pch.h"

#include "cmbtmov1_geometry.h"

namespace CMBTMOV1 {
    bool __cdecl Teleporter_Destination_In_Range(
        int16_t ship_idx, int16_t target_x_px, int16_t target_y_px) {
        const int32_t ship_x_px = MOX::combat_data[ship_idx].x * 20;
        const int32_t ship_y_px = MOX::combat_data[ship_idx].y * 20;
        const int32_t range_px = sub_space_teleporter_range * 20;

        if (round_teleporter == 0) {
            return abs(ship_x_px - target_x_px) <= range_px &&
                abs(ship_y_px - target_y_px) <= range_px;
        }

        const int16_t distance = special::Range(
            (int16_t)ship_x_px,
            (int16_t)ship_y_px,
            target_x_px,
            target_y_px);
        return distance / 20 <= sub_space_teleporter_range;
    }

    int16_t __cdecl Turn_Cost(int16_t ship_idx) {
        int16_t cost = 2;

        // Check for specific ship special systems that affect turning cost
        if (COMBINIT::Does_Combat_Ship_Have_Special(ship_idx, SPECIAL_INERTIAL_STABILIZER)) {
            return 1;
        }

        if (COMBINIT::Does_Combat_Ship_Have_Special(ship_idx, SPECIAL_INERTIAL_NULLIFIER)) {
            cost = 0;
        }

        return cost;
    }

    int16_t __cdecl Move_Cost(int16_t ship_idx, int16_t target_x, int16_t target_y) {
        s_combat_data* ship = &MOX::combat_data[ship_idx];

        int16_t ship_x = (int16_t)ship->x;
        int16_t ship_y = (int16_t)ship->y;
        int16_t ship_facing = (int16_t)ship->facing_dir;

        if (target_x == ship_x && target_y == ship_y) {
            return 0;
        }

        int16_t dx = target_x - ship_x;
        int16_t dy = target_y - ship_y;
        int32_t dist_sq = (int32_t)dx * dx + (int32_t)dy * dy;

        int16_t dist = 0;
        if (dist_sq != 0) {
            dist = 1;
            while (dist < MOX::combat_grid_width + MOX::combat_grid_height) {
                if ((int32_t)dist_sq < (int32_t)dist * (int32_t)dist + 1) {
                    break;
                }
                dist++;
            }
        }

        int16_t target_facing = Get_Facing(ship_x, ship_y, target_x, target_y);
        int16_t facing_distance = Facing_Distance(target_facing, ship_facing);

        int16_t turn_unit_cost = Turn_Cost(ship_idx);

        return dist + turn_unit_cost * facing_distance;
    }

    int16_t __cdecl Get_Facing(int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
        int16_t dx = (int16_t)(x2 - x1);
        int16_t dy = (int16_t)(y2 - y1);
        int16_t angle = angle::Get_Angle(dx, dy);

        angle = -angle;
        if (angle < 0) {
            angle += 360;
        }

        int16_t scaled = (int16_t)((uint16_t)angle << 2);
        int16_t quotient16 = (int16_t)(((int32_t)scaled + 45) / 90);
        int32_t result = quotient16;
        int32_t sign = result >> 31;

        result = ((result ^ sign) - sign) & 0x0F;
        result = (result ^ sign) - sign;

        return (int16_t)result;
    }
} // namespace CMBTMOV1
