#include "pch.h"

namespace {
    constexpr int16_t kDeploymentMinY = 4;
    constexpr uint64_t kCombatScrollTickMilliseconds = 55;
    uint64_t previous_combat_scroll_time;

    struct s_deployment_cursor {
        int16_t x;
        int16_t x_step;
        int16_t y_down = MOX::combat_grid_height / 2;
        int16_t y_up = MOX::combat_grid_height / 2;
        bool deploy_down = true;
    };

    void Take_Deployment_Candidate(
        s_deployment_cursor* cursor,
        int16_t ship_size,
        int16_t* out_x,
        int16_t* out_y,
        bool* out_deployed_down) {
        // A column is full only after both vertical cursors are exhausted.
        if (cursor->y_down > MOX::combat_grid_height - 4 &&
            cursor->y_up < kDeploymentMinY) {
            cursor->x += cursor->x_step;
            cursor->y_down = MOX::combat_grid_height / 2;
            cursor->y_up = MOX::combat_grid_height / 2;
            cursor->deploy_down = !cursor->deploy_down;
        }

        bool deployed_down = cursor->deploy_down;
        if (deployed_down && cursor->y_down > MOX::combat_grid_height - 4) {
            deployed_down = false;
        } else if (!deployed_down && cursor->y_up < kDeploymentMinY) {
            deployed_down = true;
        }

        const int16_t spacing =
            COMBINIT::Ship_Size_Offset(ship_size) + 1;
        *out_x = cursor->x;
        if (deployed_down) {
            *out_y = cursor->y_down;
            cursor->y_down += spacing;
        } else {
            cursor->y_up -= spacing;
            *out_y = cursor->y_up;
        }
        *out_deployed_down = deployed_down;
    }
} // namespace

namespace COMBINIT {
    int16_t __cdecl Ship_Size_Offset(int16_t size) {
        switch (size) {
            case HULL_FRIGATE:
                return 1;
            case HULL_DESTROYER:
            case HULL_CRUISER:
                return 2;
            case HULL_BATTLESHIP:
            case HULL_TITAN:
            case HULL_DOOM_STAR:
                return 3;
            default:
                return 0;
        }
    }

    int __cdecl qsort_combat_data_by_size(const void* a, const void* b) {
        const s_combat_data* data_a = (const s_combat_data*)a;
        const s_combat_data* data_b = (const s_combat_data*)b;

        if (data_b->size_class == data_a->size_class) {
            return 0;
        }

        if (data_b->size_class > data_a->size_class) {
            return 1;
        }

        return -1;
    }

    int8_t __cdecl Ship_Can_Deploy_At(int16_t x, int16_t y, int16_t ship_idx) {
        int8_t result = 1;
        int16_t size_threshold = 0;

        if (MOX::cmbt_colony_id != -1) {
            int16_t planet_idx = MOX::colony[MOX::cmbt_colony_id].planet;
            int8_t planet_size = MOX::planet[planet_idx].size;

            switch (planet_size) {
                case PLANET_SIZE_TINY:
                    size_threshold = 3;
                    break;
                case PLANET_SIZE_SMALL:
                    size_threshold = 4;
                    break;
                case PLANET_SIZE_MEDIUM:
                    size_threshold = 6;
                    break;
                case PLANET_SIZE_LARGE:
                    size_threshold = 7;
                    break;
                case PLANET_SIZE_HUGE:
                    size_threshold = 8;
                    break;
                default:
                    break;
            }

            // Check proximity to the primary combat object (usually a colony/starbase)
            if (MOX::combat_data->x <= x + 2 && x <= MOX::combat_data->x + size_threshold) {
                if (MOX::combat_data->y + size_threshold >= y && y + 2 >= MOX::combat_data->y) {
                    result = 0;
                }
            }
        }

        // Check proximity to another specific unit if provided
        if (ship_idx > -1) {
            if (MOX::combat_data[ship_idx].x <= x + 2 && x <= MOX::combat_data[ship_idx].x + 3) {
                if (MOX::combat_data[ship_idx].y + 3 >= y && y + 2 >= MOX::combat_data[ship_idx].y) {
                    result = 0;
                }
            }
        }

        return result;
    }

    void __cdecl Deploy_Ships() {
        int16_t stationary_ship_idx = -1;
        int16_t defender_deploy_x;
        int16_t attacker_deploy_x;
        int16_t defender_deploy_x_step;
        int16_t attacker_deploy_x_step;
        int8_t attacker_facing;
        int8_t defender_facing;
        int8_t saved_size;

        int16_t side_counts[2] = {};
        for (int16_t i = 1; i < MOX::combat_total_ship_count; ++i) {
            if (!Is_Combat_Satellite(&MOX::combat_data[i])) {
                ++side_counts[MOX::combat_data[i].owner == MOX::combat_attacker_player];
            }
        }
        const int16_t largest_side = side_counts[0] > side_counts[1]
            ? side_counts[0]
            : side_counts[1];
        MOX::combat_grid_width = ORIGINAL_COMBAT_GRID_WIDTH;
        // More vertical lanes increase area without changing the opening firing range.
        MOX::combat_grid_height = largest_side > ORIGINAL_MAX_COMBAT_SHIPS_PER_SIDE
            ? (int16_t)((ORIGINAL_COMBAT_GRID_HEIGHT * largest_side +
                            ORIGINAL_MAX_COMBAT_SHIPS_PER_SIDE - 1) /
                  ORIGINAL_MAX_COMBAT_SHIPS_PER_SIDE)
            : ORIGINAL_COMBAT_GRID_HEIGHT;

        if ((MOX::PLAYER_NUM == MOX::combat_attacker_player) || (MOX::net_flag == 1)) {
            defender_deploy_x = 0x37 /* 55 */;
            attacker_deploy_x = 0x19 /* 25 */;
            defender_deploy_x_step = 3;
            attacker_deploy_x_step = -3;
            MOX::combat_data[0].x = 65;
            attacker_facing = 0;
            defender_facing = 8;
        } else {
            defender_deploy_x = 0x19 /* 25 */;
            attacker_deploy_x = 0x37 /* 55 */;
            defender_deploy_x_step = -3;
            attacker_deploy_x_step = 3;
            MOX::combat_data[0].x = 10;
            attacker_facing = 8;
            defender_facing = 0;
        }

        MOX::combat_data[0].y = MOX::combat_grid_height / 2;
        const int8_t planet_size = MOX::combat_data[0].size_class;
        MOX::combat_data[0].size_class = 100;

        for (int16_t i = 1; i < MOX::combat_total_ship_count; ++i) {
            if (Is_Combat_Satellite(&MOX::combat_data[i])) {
                saved_size = MOX::combat_data[i].size_class;
                MOX::combat_data[i].size_class = 99;
            }
        }

        qsort(MOX::combat_data, MOX::combat_total_ship_count, sizeof(*MOX::combat_data), qsort_combat_data_by_size);

        MOX::combat_data[0].size_class = planet_size;

        for (int16_t i = 1; i < MOX::combat_total_ship_count; ++i) {
            if (Is_Combat_Satellite(&MOX::combat_data[i])) {
                MOX::combat_data[i].size_class = saved_size;
            }
        }

        s_deployment_cursor attacker_cursor = {
            attacker_deploy_x,
            attacker_deploy_x_step,
        };
        s_deployment_cursor defender_cursor = {
            defender_deploy_x,
            defender_deploy_x_step,
        };

        for (int16_t i = 1; i < MOX::combat_total_ship_count; ++i) {
            s_combat_data* ship = &MOX::combat_data[i];
            if (Is_Combat_Satellite(ship)) {
                int16_t x_pos;
                if (defender_deploy_x < 40) {
                    x_pos = MOX::combat_data[0].x + 11;
                } else {
                    x_pos = MOX::combat_data[0].x - 4;
                }
                ship->x = x_pos;
                ship->y = MOX::combat_data[0].y + 1;
                stationary_ship_idx = i;
            } else {
                if (ship->owner == MOX::combat_attacker_player) {
                    int16_t x;
                    int16_t y;
                    bool deployed_down;
                    Take_Deployment_Candidate(
                        &attacker_cursor,
                        ship->size_class,
                        &x,
                        &y,
                        &deployed_down);
                    ship->x = x;
                    ship->y = y;
                    attacker_cursor.deploy_down = !deployed_down;
                    ship->facing_dir = attacker_facing;
                } else {
                    int16_t x;
                    int16_t y;
                    bool deployed_down;
                    do {
                        Take_Deployment_Candidate(
                            &defender_cursor,
                            ship->size_class,
                            &x,
                            &y,
                            &deployed_down);
                    } while (!Ship_Can_Deploy_At(
                        x, y, stationary_ship_idx));

                    ship->x = x;
                    ship->y = y;
                    defender_cursor.deploy_down = !deployed_down;
                    ship->facing_dir = defender_facing;
                }
            }
        }
    }

    void __cdecl Snap_Center_Combat_Screen(int16_t target_x, int16_t target_y) {
        int16_t new_combat_x;
        int16_t new_combat_y;
        int16_t delta_x;
        int16_t delta_y;
        int16_t near_x;
        int16_t mid_x;
        int16_t far_x;

        new_combat_x = target_x - 16;
        new_combat_y = target_y - 9;

        if (new_combat_x < 0) {
            new_combat_x = 0;
        }
        if (new_combat_x > MOX::combat_grid_width - COMBAT_GRID_VIEW_WIDTH) {
            new_combat_x = MOX::combat_grid_width - COMBAT_GRID_VIEW_WIDTH;
        }

        if (new_combat_y < 0) {
            new_combat_y = 0;
        }
        if (new_combat_y > MOX::combat_grid_height - COMBAT_GRID_VIEW_HEIGHT) {
            new_combat_y = MOX::combat_grid_height - COMBAT_GRID_VIEW_HEIGHT;
        }

        delta_x = MOX::cur_combat_x - new_combat_x;
        delta_y = MOX::cur_combat_y - new_combat_y;

        near_x = MOX::combat_near_x + (delta_x * -160);
        MOX::combat_near_y += (delta_y * -160) % 480;

        mid_x = MOX::combat_mid_x + (delta_x * -80);
        MOX::combat_mid_y += (delta_y * -80) % 480;

        far_x = MOX::combat_far_x + (delta_x * -20);
        MOX::combat_far_y += (delta_y * -20) % 480;

        // Normalize negative coordinates for wrapping
        if (near_x < 0) {
            near_x += ((0x27F - near_x) / 0x280) * 0x280;
        }
        if (MOX::combat_near_y < 0) {
            MOX::combat_near_y += ((0x1DF - MOX::combat_near_y) / 0x1E0) * 0x1E0;
        }

        if (mid_x < 0) {
            mid_x += ((0x27F - mid_x) / 0x280) * 0x280;
        }
        if (MOX::combat_mid_y < 0) {
            MOX::combat_mid_y += ((0x1DF - MOX::combat_mid_y) / 0x1E0) * 0x1E0;
        }

        if (far_x < 0) {
            far_x += ((0x27F - far_x) / 0x280) * 0x280;
        }
        if (MOX::combat_far_y < 0) {
            MOX::combat_far_y += ((0x1DF - MOX::combat_far_y) / 0x1E0) * 0x1E0;
        }

        // Apply wrapping within screen resolution (640x480)
        MOX::combat_near_x = near_x % 0x280;
        MOX::combat_near_y = MOX::combat_near_y % 0x1E0;
        MOX::combat_mid_x = mid_x % 0x280;
        MOX::combat_mid_y = MOX::combat_mid_y % 0x1E0;
        MOX::combat_far_x = far_x % 0x280;
        MOX::combat_far_y = MOX::combat_far_y % 0x1E0;

        MOX::cur_combat_x = new_combat_x;
        MOX::cur_combat_y = new_combat_y;

        COMBAT1::Add_Tactical_Combat_Fields();
        COMBAT1::Assign_Combat_Grids(MOX::cur_ship);
        CMBTDRW1::Refresh_Combat_Screen_Full();
    }

    void __cdecl Scroll_Combat_Screen() {
        uint32_t scroll_timer = 0;
        int16_t next_combat_x;
        int16_t next_combat_y;
        int16_t scroll_speed;
        int16_t old_combat_x;
        int16_t old_combat_y;
        int16_t dir_x;
        int16_t dir_y;
        int16_t diff_x;
        int16_t diff_y;

        while (true) {
            if (mouse::Pointer_X() >= 5 &&
                mouse::Pointer_X() <= 0x27B /* 635 */ &&
                mouse::Pointer_Y() >= 5 &&
                mouse::Pointer_Y() <= 0x1DB /* 475 */) {
                COMBAT1::Assign_Combat_Grids(MOX::cur_ship);
                COMBAT1::Add_Tactical_Combat_Fields();
                CMBTDRW1::Refresh_Combat_Screen_Full();
                MOX::last_ship_status = -1;
                return;
            }

            old_combat_x = MOX::cur_combat_x;
            old_combat_y = MOX::cur_combat_y;
            next_combat_x = old_combat_x;
            next_combat_y = old_combat_y;

            if (scroll_timer != UINT32_MAX) {
                ++scroll_timer;
            }
            if (scroll_timer > 4) {
                scroll_speed = 3;
            } else if (scroll_timer > 2) {
                scroll_speed = 2;
            } else {
                scroll_speed = 1;
            }

            if (scroll_interval != 0) {
                const uint64_t current_time = SDL_GetTicks();
                const uint64_t interval =
                    (uint64_t)scroll_interval * kCombatScrollTickMilliseconds;
                if (current_time - previous_combat_scroll_time < interval) {
                    SDL_Delay(1);
                    continue;
                }
                previous_combat_scroll_time = current_time;
            }

            if (mouse::Pointer_X() < 5) {
                next_combat_x = old_combat_x - scroll_speed;
            } else if (mouse::Pointer_X() > 0x27B /* 635 */) {
                next_combat_x = old_combat_x + scroll_speed;
            }

            if (mouse::Pointer_Y() < 5) {
                next_combat_y = old_combat_y - scroll_speed;
            } else if (mouse::Pointer_Y() > 0x1DB /* 475 */) {
                next_combat_y = old_combat_y + scroll_speed;
            }

            if (next_combat_x < 0) {
                next_combat_x = 0;
            } else if (next_combat_x > MOX::combat_grid_width - COMBAT_GRID_VIEW_WIDTH) {
                next_combat_x = MOX::combat_grid_width - COMBAT_GRID_VIEW_WIDTH;
            }

            if (next_combat_y < 0) {
                next_combat_y = 0;
            } else if (next_combat_y > MOX::combat_grid_height - COMBAT_GRID_VIEW_HEIGHT) {
                next_combat_y = MOX::combat_grid_height - COMBAT_GRID_VIEW_HEIGHT;
            }

            diff_x = (int16_t)(old_combat_x - next_combat_x);
            if (diff_x < 0) {
                dir_x = 1;
            } else {
                dir_x = (diff_x > 0) ? -1 : 0;
            }

            diff_y = (int16_t)(old_combat_y - next_combat_y);
            if (diff_y < 0) {
                dir_y = 1;
            } else {
                dir_y = (diff_y > 0) ? -1 : 0;
            }

            if (next_combat_x != old_combat_x) {
                MOX::combat_near_x += dir_x * -0x50;                        // -80
                MOX::combat_mid_x += dir_x * -0x28;                         // -40
                MOX::combat_far_x += (int16_t)(((int32_t)dir_x * -20) / 2); // -10
            }

            if (next_combat_y != old_combat_y) {
                MOX::combat_near_y += dir_y * -0x50;                        // -80
                MOX::combat_mid_y += dir_y * -0x28;                         // -40
                MOX::combat_far_y += (int16_t)(((int32_t)dir_y * -20) / 2); // -10
            }

            if (MOX::combat_near_x < 0) MOX::combat_near_x += 0x280; // 640
            if (MOX::combat_near_y < 0) MOX::combat_near_y += 0x1E0; // 480
            if (MOX::combat_mid_x < 0) MOX::combat_mid_x += 0x280;
            if (MOX::combat_mid_y < 0) MOX::combat_mid_y += 0x1E0;
            if (MOX::combat_far_x < 0) MOX::combat_far_x += 0x280;
            if (MOX::combat_far_y < 0) MOX::combat_far_y += 0x1E0;

            MOX::combat_near_x %= 0x280;
            MOX::combat_near_y %= 0x1E0;
            MOX::combat_mid_x %= 0x280;
            MOX::combat_mid_y %= 0x1E0;
            MOX::combat_far_x %= 0x280;
            MOX::combat_far_y %= 0x1E0;

            MOX::cur_combat_x = next_combat_x;
            MOX::cur_combat_y = next_combat_y;

            video::Set_Page_Off();
            CMBTDRW1::Scroll_Draw_Combat_Screen();
            RUSS::Mox_Sync_Update();
        }
    }
} // namespace COMBINIT
