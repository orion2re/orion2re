#include "pch.h"

namespace CMBTMOV1 {
    int8_t __cdecl Ship_Is_In_Stasis(int16_t ship_idx) {
        const int16_t stasis_source_idx = MOX::combat_data[ship_idx].stasis_source_idx == COMBAT_SHIP_NONE
            ? int16_t{-1}
            : MOX::combat_data[ship_idx].stasis_source_idx;
        return stasis_source_idx != -1;
    }

    bool __cdecl Ship_In_Black_Hole(int16_t ship_idx) {
        return MOX::combat_data[ship_idx].black_hole_source_idx != COMBAT_SHIP_NONE;
    }

    int16_t __cdecl Rotate_Ship_Until_Impact(int16_t ship_idx, int16_t* target_facing) {
        if (COMBINIT::Does_Combat_Ship_Have_Special(ship_idx, SPECIAL_INERTIAL_NULLIFIER)) {
            return 0;
        }

        int16_t impact_detected = 0;
        uint8_t missile_tracked[MAX_COMBAT_FLYERS];
        int16_t saved_missile_x[MAX_COMBAT_FLYERS];
        int16_t saved_missile_y[MAX_COMBAT_FLYERS];
        int16_t saved_missile_travel[MAX_COMBAT_FLYERS];
        int8_t saved_missile_facing[MAX_COMBAT_FLYERS];
        int16_t sx, sy;
        int16_t rotation_step;
        int8_t current_facing;

        memset(missile_tracked, 0, sizeof(missile_tracked));

        s_combat_data* ship = &MOX::combat_data[ship_idx];
        current_facing = ship->facing_dir;

        COMBINIT::Ship_Center_Offsets(ship_idx, &sx, &sy);
        sx += (int16_t)ship->x * 20;
        sy += (int16_t)ship->y * 20;

        int16_t diff = (int16_t)current_facing - *target_facing;
        if (diff < 0) {
            rotation_step = ((-diff) > 8) ? -1 : 1;
        } else {
            rotation_step = (diff > 8) ? 1 : -1;
        }

        if ((int16_t)current_facing != *target_facing) {
            while (impact_detected == 0) {
                current_facing = (int8_t)((current_facing + rotation_step + 16) % 16);

                for (int32_t i = 0; i < MAX_COMBAT_FLYERS; ++i) {
                    s_missile* missile = &MOX::missile[i];
                    if (CMBTMIS::Missile_Targets_Ship(missile, ship_idx) &&
                        missile->source_unit_idx != ship_idx) {
                        int16_t move_speed = 5;
                        if (ship->current_speed != 0) {
                            move_speed = (int16_t)(((int32_t)missile->speed * 5) / (int32_t)ship->current_speed);
                        }
                        if (move_speed < 2) {
                            move_speed = 2;
                        }

                        if (missile_tracked[i] == 0) {
                            saved_missile_x[i] = missile->x;
                            saved_missile_y[i] = missile->y;
                            saved_missile_travel[i] = missile->travel_dist;
                            saved_missile_facing[i] = missile->facing_dir;
                            missile_tracked[i] = 1;
                        }

                        if (CMBTMIS::Move_Missile(i, sx, sy, move_speed)) {
                            if (CMBTAI::Will_Missile_Do_Damage(ship_idx, i) != 0 && missile->is_active == 1) {
                                impact_detected = 1;
                            }
                        }
                    }
                }

                if ((int16_t)current_facing == *target_facing) {
                    break;
                }
            }
        }

        if (impact_detected != 0) {
            current_facing = (int8_t)((current_facing - rotation_step + 16) % 16);
            *target_facing = (int16_t)current_facing;
        }

        for (int32_t i = 0; i < MAX_COMBAT_FLYERS; ++i) {
            if (missile_tracked[i] == 1) {
                MOX::missile[i].x = saved_missile_x[i];
                MOX::missile[i].y = saved_missile_y[i];
                MOX::missile[i].travel_dist = saved_missile_travel[i];
                MOX::missile[i].facing_dir = saved_missile_facing[i];
            }
        }

        return impact_detected;
    }

    int16_t __cdecl Test_Move_Until_Impact(int16_t ship_idx, int16_t* dest_x, int16_t* dest_y, int16_t* dest_facing) {
        int16_t impact_flag = 0;
        int16_t time_spent = 0;

        int8_t missile_checked[MAX_COMBAT_FLYERS];
        int16_t missile_x_saved[MAX_COMBAT_FLYERS];
        int16_t missile_y_saved[MAX_COMBAT_FLYERS];
        int16_t missile_travel_dist_saved[MAX_COMBAT_FLYERS];
        int8_t missile_facing_saved[MAX_COMBAT_FLYERS];

        struct_::Clear_Structure(missile_checked, sizeof(missile_checked));

        int16_t target_x_val = *dest_x;
        int16_t target_y_val = *dest_y;

        int16_t center_x, center_y;
        COMBINIT::Ship_Center_Offsets(ship_idx, &center_x, &center_y);

        s_combat_data* ship = &MOX::combat_data[ship_idx];

        int16_t start_x = ship->x * 20;
        int16_t start_y = ship->y * 20;

        int16_t speed;
        if (MOX::speedx2_flag == 1 || MOX::net_flag != 0) {
            speed = 12;
        } else {
            speed = 2;
        }

        int16_t current_x = start_x;
        int16_t current_y = start_y;

        int16_t facing = Get_Facing(start_x, start_y, target_x_val, target_y_val);

        if (ship->facing_dir != facing) {
            time_spent = Rotate_Ship_Until_Impact(ship_idx, &facing);
            impact_flag = time_spent;
        }

        *dest_facing = facing;

        int16_t legal_x = current_x;
        int16_t legal_y = current_y;

        if (impact_flag == 0) {
            while (true) {
                if (current_x == target_x_val && current_y == target_y_val) {
                    break;
                }

                if (Ship_In_Legal_Square(MOX::cur_ship, current_x, current_y, target_x_val, target_y_val)) {
                    legal_x = current_x;
                    legal_y = current_y;
                }

                special::Absolute_Interpolate_Line(&current_x, &current_y, start_x, start_y, target_x_val, target_y_val, speed);

                for (int16_t i = 0; i < MAX_COMBAT_FLYERS; ++i) {
                    s_missile* missile = &MOX::missile[i];
                    if (CMBTMIS::Missile_Targets_Ship(missile, ship_idx) &&
                        missile->source_unit_idx != ship_idx) {
                        int16_t missile_speed = 5;
                        if (ship->current_speed != 0) {
                            uint8_t m_speed = missile->speed;
                            missile_speed = (m_speed * speed) / ship->current_speed;
                        }
                        if (missile_speed < 2) {
                            missile_speed = 2;
                        }

                        if (missile_checked[i] == 0) {
                            missile_x_saved[i] = missile->x;
                            missile_y_saved[i] = missile->y;
                            missile_travel_dist_saved[i] = missile->travel_dist;
                            missile_facing_saved[i] = missile->facing_dir;
                            missile_checked[i] = 1;
                        }

                        if (CMBTMIS::Move_Missile(i, current_x + center_x, current_y + center_y, missile_speed)) {
                            if (CMBTAI::Will_Missile_Do_Damage(ship_idx, i)) {
                                if (missile->is_active == 1) {
                                    impact_flag = 1;
                                }
                            }
                        }
                    }
                }

                if (impact_flag != 0) {
                    break;
                }
            }
        }

        if (impact_flag != 0) {
            if (legal_x == start_x && legal_y == start_y) {
                current_x = legal_x;
                current_y = legal_y;
            } else {
                if (legal_x > target_x_val) {
                    current_x = legal_x - (legal_x % 20) + 20;
                } else {
                    current_x = legal_x - (legal_x % 20);
                }

                if (legal_y > target_y_val) {
                    current_y = legal_y - (legal_y % 20) + 20;
                } else {
                    current_y = legal_y - (legal_y % 20);
                }
            }
        }

        *dest_x = current_x;
        *dest_y = current_y;

        for (int16_t i = 0; i < MAX_COMBAT_FLYERS; ++i) {
            if (missile_checked[i] == 1) {
                s_missile* missile = &MOX::missile[i];
                missile->x = missile_x_saved[i];
                missile->y = missile_y_saved[i];
                missile->travel_dist = missile_travel_dist_saved[i];
                missile->facing_dir = missile_facing_saved[i];
            }
        }

        return time_spent;
    }

    int16_t __cdecl Ship_In_Legal_Square(int16_t ship_idx, int16_t x, int16_t y, int16_t target_x, int16_t target_y) {
        int16_t grid_x;
        int16_t grid_y;

        if (target_x < x) {
            grid_x = (x - (x % 20)) + 20;
        } else {
            grid_x = x - (x % 20);
        }

        if (target_y < y) {
            grid_y = (y - (y % 20)) + 20;
        } else {
            grid_y = y - (y % 20);
        }

        int16_t cell_x = grid_x / 20;
        int16_t cell_y = grid_y / 20;

        if (cell_x < 0 || cell_x >= MOX::combat_grid_width || cell_y < 0 || cell_y >= MOX::combat_grid_height) {
            return 0;
        }

        int16_t cell_val = MOX::combat_grid[cell_x][cell_y];

        if (cell_val == 2000 || cell_val == ship_idx) {
            return 1;
        }

        return 0;
    }
} // namespace CMBTMOV1
