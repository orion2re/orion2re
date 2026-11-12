#include "pch.h"

namespace COMBAT1 {
    void __cdecl Set_Legal_Moves(int16_t ship_idx, int16_t range, int16_t check_cost) {
        int16_t footprint;
        int16_t ship_x;
        int16_t ship_y;
        int16_t x_pixel;
        int16_t y_pixel;

        const bool unused_sub_space_teleporter =
            COMBINIT::Does_Combat_Ship_Have_Special(
                ship_idx, SPECIAL_SUB_SPACE_TELEPORTER) != 0 &&
            COMBINIT::Does_Combat_Ship_Have_Special(
                ship_idx, SPECIAL_PHASE_SHIFTER) == 0 &&
            CMBTFIR2::Used_Teleporter(ship_idx) == 0;
        if (unused_sub_space_teleporter) {
            range = CMBTMOV1::sub_space_teleporter_range;
        }

        s_combat_data* ship = &MOX::combat_data[ship_idx];
        footprint = (ship->size_class + 1) / 2;
        if (footprint > 2) {
            footprint = 2;
        }

        ship_x = ship->x;
        ship_y = ship->y;
        x_pixel = 0;

        for (int16_t x = 0; x < MOX::combat_grid_width; ++x) {
            y_pixel = 0;
            for (int16_t y = 0; y < MOX::combat_grid_height; ++y) {
                int16_t* grid_cell = &MOX::combat_grid[x][y];

                if (*grid_cell == -1) {
                    int16_t dx = ship_x - x;
                    if (dx < 0) dx = -dx;

                    int16_t dy = ship_y - y;
                    if (dy < 0) dy = -dy;

                    if (dx <= range && dy <= range) {
                        *grid_cell = 2000;

                        if (footprint == 1) {
                            for (int16_t f_dx = 0; f_dx < 2; ++f_dx) {
                                for (int16_t f_dy = 0; f_dy < 2; ++f_dy) {
                                    if ((x + f_dx >= MOX::combat_grid_width) || (y + f_dy >= MOX::combat_grid_height)) {
                                        *grid_cell = -1;
                                    } else {
                                        int16_t cell_val = MOX::combat_grid[x + f_dx][y + f_dy];
                                        if (cell_val != -1 && cell_val != 2000 && cell_val != ship_idx) {
                                            *grid_cell = -1;
                                        }
                                    }
                                }
                            }
                        } else if (footprint == 2) {
                            for (int16_t f_dx = 0; f_dx <= 2; ++f_dx) {
                                for (int16_t f_dy = 0; f_dy <= 2; ++f_dy) {
                                    if ((x + f_dx >= MOX::combat_grid_width) || (y + f_dy >= MOX::combat_grid_height)) {
                                        *grid_cell = -1;
                                    } else {
                                        int16_t cell_val = MOX::combat_grid[x + f_dx][y + f_dy];
                                        if (cell_val != -1 && cell_val != 2000 && cell_val != ship_idx) {
                                            *grid_cell = -1;
                                        }
                                    }
                                }
                            }
                        }

                        if ((MOX::combat_grid_width - footprint < x) || (MOX::combat_grid_height - footprint < y)) {
                            *grid_cell = -1;
                        }

                        if (*grid_cell == 2000 && check_cost == 1) {
                            int16_t cost = CMBTMOV1::Move_Cost(ship_idx, x, y);
                            if (ship->movement_left < cost) {
                                *grid_cell = -1;
                            }

                            if (COMBINIT::Does_Combat_Ship_Have_Special(ship_idx, SPECIAL_SUB_SPACE_TELEPORTER)) {
                                if (CMBTFIR2::Used_Teleporter(ship_idx) == 0 &&
                                    CMBTFIRE::Special_Is_Turned_On(
                                        ship_idx, SPECIAL_SUB_SPACE_TELEPORTER) == 1) {
                                    const bool in_teleporter_range =
                                        CMBTMOV1::Teleporter_Destination_In_Range(
                                            ship_idx, x_pixel, y_pixel);
                                    if (CMBTMOV1::round_teleporter != 0 &&
                                        !in_teleporter_range) {
                                        *grid_cell = -1;
                                    } else if (*grid_cell == -1 &&
                                        in_teleporter_range) {
                                        *grid_cell = 2000;
                                    }
                                }
                            }
                        }
                    }
                }
                y_pixel += 20;
            }
            x_pixel += 20;
        }
    }

    void __cdecl Set_Combat_Help_Fields() {
        int16_t entry_idx = 0;

        if (MOX::order_info_flag == 1) {
            MOX::help_entry_list[0].help_id = 0x269;
            MOX::help_entry_list[0].x1 = 0x124;
            MOX::help_entry_list[0].y1 = 0x1be;
            MOX::help_entry_list[0].x2 = 0x176;
            MOX::help_entry_list[0].y2 = 0x1d8;

            MOX::help_entry_list[1].help_id = 0x26f;
            MOX::help_entry_list[1].x1 = 0x10f;
            MOX::help_entry_list[1].y1 = 0x16a;
            MOX::help_entry_list[1].x2 = 0x14a;
            MOX::help_entry_list[1].y2 = 0x182;

            MOX::help_entry_list[2].help_id = 0x270;
            MOX::help_entry_list[2].x1 = 0x14c;
            MOX::help_entry_list[2].y1 = 0x16a;
            MOX::help_entry_list[2].x2 = 0x18a;
            MOX::help_entry_list[2].y2 = 0x182;

            MOX::help_entry_list[3].help_id = 0x271;
            MOX::help_entry_list[3].x1 = 0x10f;
            MOX::help_entry_list[3].y1 = 0x185;
            MOX::help_entry_list[3].x2 = 0x14a;
            MOX::help_entry_list[3].y2 = 0x19e;

            MOX::help_entry_list[4].help_id = 0x272;
            MOX::help_entry_list[4].x1 = 0x14d;
            MOX::help_entry_list[4].y1 = 0x185;
            MOX::help_entry_list[4].x2 = 0x18a;
            MOX::help_entry_list[4].y2 = 0x19e;

            MOX::help_entry_list[5].help_id = 0x273;
            MOX::help_entry_list[5].x1 = 0x10f;
            MOX::help_entry_list[5].y1 = 0x1a2;
            MOX::help_entry_list[5].x2 = 0x14a;
            MOX::help_entry_list[5].y2 = 0x1b9;

            MOX::help_entry_list[6].help_id = 0x274;
            MOX::help_entry_list[6].x1 = 0x14d;
            MOX::help_entry_list[6].y1 = 0x1a2;
            MOX::help_entry_list[6].x2 = 0x18a;
            MOX::help_entry_list[6].y2 = 0x1b9;
            entry_idx = 7;
        } else {
            MOX::help_entry_list[0].help_id = 0x2b7;
            MOX::help_entry_list[0].x1 = 0x11a;
            MOX::help_entry_list[0].y1 = 0x15e;
            MOX::help_entry_list[0].x2 = 0x180;
            MOX::help_entry_list[0].y2 = 0x173;

            MOX::help_entry_list[1].help_id = 0x2b8;
            MOX::help_entry_list[1].x1 = 0x108;
            MOX::help_entry_list[1].y1 = 0x17f;
            MOX::help_entry_list[1].x2 = 0x197;
            MOX::help_entry_list[1].y2 = 0x18c;

            MOX::help_entry_list[2].help_id = 0x2b9;
            MOX::help_entry_list[2].x1 = 0x108;
            MOX::help_entry_list[2].y1 = 0x18f;
            MOX::help_entry_list[2].x2 = 0x197;
            MOX::help_entry_list[2].y2 = 0x19c;

            MOX::help_entry_list[3].help_id = 0x2ba;
            MOX::help_entry_list[3].x1 = 0x108;
            MOX::help_entry_list[3].y1 = 0x19f;
            MOX::help_entry_list[3].x2 = 0x197;
            MOX::help_entry_list[3].y2 = 0x1ac;

            MOX::help_entry_list[4].help_id = 0x2bb;
            MOX::help_entry_list[4].x1 = 0x108;
            MOX::help_entry_list[4].y1 = 0x1af;
            MOX::help_entry_list[4].x2 = 0x197;
            MOX::help_entry_list[4].y2 = 0x1bc;

            MOX::help_entry_list[5].help_id = 0x2bc;
            MOX::help_entry_list[5].x1 = 0x108;
            MOX::help_entry_list[5].y1 = 0x1bf;
            MOX::help_entry_list[5].x2 = 0x197;
            MOX::help_entry_list[5].y2 = 0x1cc;
            entry_idx = 6;
        }

        MOX::help_entry_list[entry_idx].help_id = 0x26a;
        MOX::help_entry_list[entry_idx].x1 = 0x0a;
        MOX::help_entry_list[entry_idx].y1 = 0x168;
        MOX::help_entry_list[entry_idx].x2 = 0x6a;
        MOX::help_entry_list[entry_idx].y2 = 0x1d7;
        entry_idx++;

        MOX::help_entry_list[entry_idx].x1 = 0x1f6;
        MOX::help_entry_list[entry_idx].y1 = 0x168;
        MOX::help_entry_list[entry_idx].x2 = 0x272;
        MOX::help_entry_list[entry_idx].y2 = 0x1d7;
        MOX::help_entry_list[entry_idx].help_id = 0x26d;
        if (MOX::missile_warning != 1) {
            MOX::help_entry_list[entry_idx].help_id = 0x26c;
        }
        entry_idx++;

        MOX::help_entry_list[entry_idx].help_id = 0x26e;
        MOX::help_entry_list[entry_idx].x1 = 0x18f;
        MOX::help_entry_list[entry_idx].y1 = 0x168;
        MOX::help_entry_list[entry_idx].x2 = 0x1ef;
        MOX::help_entry_list[entry_idx].y2 = 0x1d7;
        entry_idx++;

        const bool has_current_ship = MOX::cur_ship >= 0 && MOX::cur_ship < MOX::combat_total_ship_count;
        if (has_current_ship && MOX::ship_status_display == 0) {
            for (int j = 0; j < 8; j++) {
                uint16_t tech_app_id = TECHDATA::weapons[MOX::combat_data[MOX::cur_ship].weapons[j].weapon_id].tech_app_id;
                if (tech_app_id != 0) {
                    MOX::help_entry_list[entry_idx + j].help_id = tech_app_id;
                } else {
                    MOX::help_entry_list[entry_idx + j].help_id = -1;
                }
            }
        } else if (has_current_ship) {
            int special_count = 0;
            for (int k = 0; k < 8; k++) {
                MOX::help_entry_list[entry_idx + k].help_id = -1;
            }
            for (int j = 0; j < 39 && special_count < 8; j++) {
                if (struct_::Test_Bit_Field(MOX::combat_data[MOX::cur_ship].special_device_flags, j) == 1) {
                    MOX::help_entry_list[entry_idx + special_count].help_id = TECHDATA::specials[j].tech_app_id;
                    special_count++;
                }
            }
        } else {
            for (int j = 0; j < 8; j++) {
                MOX::help_entry_list[entry_idx + j].help_id = -1;
            }
        }

        MOX::help_entry_list[entry_idx].x1 = 0x72;
        MOX::help_entry_list[entry_idx].y1 = 0x17b;
        MOX::help_entry_list[entry_idx].x2 = 0x108;
        MOX::help_entry_list[entry_idx].y2 = 0x185;
        entry_idx++;

        MOX::help_entry_list[entry_idx].x1 = 0x72;
        MOX::help_entry_list[entry_idx].y1 = 0x186;
        MOX::help_entry_list[entry_idx].x2 = 0x108;
        MOX::help_entry_list[entry_idx].y2 = 0x190;
        entry_idx++;

        MOX::help_entry_list[entry_idx].x1 = 0x72;
        MOX::help_entry_list[entry_idx].y1 = 0x191;
        MOX::help_entry_list[entry_idx].x2 = 0x108;
        MOX::help_entry_list[entry_idx].y2 = 0x19b;
        entry_idx++;

        MOX::help_entry_list[entry_idx].x1 = 0x72;
        MOX::help_entry_list[entry_idx].y1 = 0x19c;
        MOX::help_entry_list[entry_idx].x2 = 0x108;
        MOX::help_entry_list[entry_idx].y2 = 0x1a6;
        entry_idx++;

        MOX::help_entry_list[entry_idx].x1 = 0x72;
        MOX::help_entry_list[entry_idx].y1 = 0x1a7;
        MOX::help_entry_list[entry_idx].x2 = 0x108;
        MOX::help_entry_list[entry_idx].y2 = 0x1b1;
        entry_idx++;

        MOX::help_entry_list[entry_idx].x1 = 0x72;
        MOX::help_entry_list[entry_idx].y1 = 0x1b2;
        MOX::help_entry_list[entry_idx].x2 = 0x108;
        MOX::help_entry_list[entry_idx].y2 = 0x1bc;
        entry_idx++;

        MOX::help_entry_list[entry_idx].x1 = 0x72;
        MOX::help_entry_list[entry_idx].y1 = 0x1bd;
        MOX::help_entry_list[entry_idx].x2 = 0x108;
        MOX::help_entry_list[entry_idx].y2 = 0x1c7;
        entry_idx++;

        MOX::help_entry_list[entry_idx].x1 = 0x72;
        MOX::help_entry_list[entry_idx].y1 = 0x1c8;
        (void)(MOX::ship_status_display == 0);
        MOX::help_entry_list[entry_idx].x2 = 0x108;
        MOX::help_entry_list[entry_idx].y2 = 0x1d3;
        entry_idx++;

        MOX::help_entry_list[entry_idx].help_id = 0x275;
        MOX::help_entry_list[entry_idx].x1 = 0x71;
        MOX::help_entry_list[entry_idx].y1 = 0x168;
        MOX::help_entry_list[entry_idx].x2 = 0x109;
        MOX::help_entry_list[entry_idx].y2 = 0x1d7;
        entry_idx++;

        fields::Set_Help_List(MOX::help_entry_list, entry_idx);
    }
} // namespace COMBAT1
