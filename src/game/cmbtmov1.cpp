#include "pch.h"

#include "cmbtmov1_geometry.h"

namespace CMBTMOV1 {
    void __cdecl Rotate_Ship(int16_t ship_idx, int16_t target_facing, int16_t screen_x, int16_t screen_y) {
        int16_t center_x;
        int16_t center_y;
        int16_t moved_missiles[MAX_COMBAT_FLYERS];
        int16_t moved_count;
        int16_t step;
        int16_t saved_dont_draw;
        int16_t current_facing;
        s_combat_data* ship = &MOX::combat_data[ship_idx];

        current_facing = ship->facing_dir;
        COMBINIT::Ship_Center_Offsets(ship_idx, &center_x, &center_y);

        center_x += ship->x * 20;
        center_y += ship->y * 20;

        int16_t diff = current_facing - target_facing;
        if (diff < 0) {
            step = (abs(diff) > 8) ? -1 : 1;
        } else {
            step = (diff > 8) ? 1 : -1;
        }

        saved_dont_draw = MOX::dont_draw_ship;
        MOX::dont_draw_ship = ship_idx;

        while (ship->facing_dir != target_facing && ship->unit_status == 0) {
            timer::Mark_Time();

            current_facing += step;
            if (current_facing < 0) {
                current_facing += 16;
            }
            current_facing %= 16;

            ship->facing_dir = (int8_t)current_facing;
            moved_count = 0;

            if (!COMBINIT::Does_Combat_Ship_Have_Special(ship_idx, SPECIAL_INERTIAL_NULLIFIER)) {
                for (int16_t i = 0; i < MAX_COMBAT_FLYERS; ++i) {
                    s_missile* missile = &MOX::missile[i];
                    if (CMBTMIS::Missile_Targets_Ship(missile, ship_idx)) {
                        int16_t move_speed = 5;
                        if (ship->current_speed != 0) {
                            move_speed = (int16_t)((missile->speed * 5) / ship->current_speed);
                        }
                        if (move_speed < 2) {
                            move_speed = 2;
                        }

                        if (CMBTMIS::Move_Missile(i, center_x, center_y, move_speed) != 0) {
                            moved_missiles[moved_count++] = i;
                        }
                    }
                }
            }

            if (MOX::super_fast_flag == 0 && CMBTDRW1::Ship_On_Screen(ship_idx) != 0) {
                video::Set_Page_Off();
                for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
                    const int16_t tractor_target_idx = MOX::combat_data[i].special_cooldown == COMBAT_SHIP_NONE
                        ? int16_t{-1}
                        : MOX::combat_data[i].special_cooldown;
                    if (MOX::combat_data[i].unit_status == 0 && tractor_target_idx != -1) {
                        CMBTSPEC::Draw_Continuing_Tractor(i);
                    }
                }
                CMBTDRW1::Draw_Main_Combat_Screen();
                graphics::Set_Window(0, 0, 639, 350);
                video::Clip_On();
                CMBTDRW1::Draw_Ship(ship_idx, screen_x, screen_y);
                video::Clip_Off();
                graphics::Reset_Window();

                for (int16_t i = 0; i < MAX_COMBAT_FLYERS; ++i) {
                    s_missile* missile = &MOX::missile[i];
                    if (CMBTMIS::Missile_Targets_Ship(missile, ship_idx)) {
                        CMBTMIS::Draw_Missile(i, missile->x - MOX::cur_combat_x * 20, missile->y - MOX::cur_combat_y * 20, 0);
                    }
                }
                RUSS::Mox_Sync_Update();
                timer::Release_Time(1);
            }

            for (int16_t i = 0; i < moved_count; ++i) {
                if (ship->unit_status == 0 && MOX::missile[moved_missiles[i]].target_unit_idx == ship_idx) {
                    CMBTMIS::Resolve_Missile(moved_missiles[i], center_x, center_y);
                }
            }

            if (CMBTFIRE::Is_Combat_Ship_Dead(ship_idx) == 0) {
                int16_t fire_x = screen_x + MOX::cur_combat_x * 20;
                int16_t fire_y = screen_y + MOX::cur_combat_y * 20;
                CMBTFIR2::Defensive_Fire_Check(ship_idx, fire_x, fire_y, fire_x, fire_y);
            }
        }

        MOX::dont_draw_ship = saved_dont_draw;
    }

    void __cdecl Wait_Until_Net_Opponent_Finished(int16_t show_wait_message) {
        int16_t opponent_idx;
        char message_buffer[80];

        opponent_idx = MOX::combat_attacker_player;
        if (MOX::PLAYER_NUM == MOX::combat_attacker_player) {
            opponent_idx = MOX::combat_defender_player;
        }

        NETMOX::Net_Debug_Log(
            "tactical barrier send local_player=%d opponent=%d turn=%d cur_ship=%d cur_ptr=%d counter_before=%u choice=%d show=%d",
            (int)MOX::PLAYER_NUM,
            (int)opponent_idx,
            (int)MOX::combat_turn,
            MOX::cur_ship,
            (int)MOX::cur_ptr,
            (unsigned int)MOX::combat_done_waiting,
            (int)MOX::tactical_net_choice,
            (int)show_wait_message);
        NETMOX::Mox_Send_Message(MOX::player[opponent_idx].network_player_id, TACTICAL_NET_PACKET_WAIT_FOR_OPPONENT, &opponent_idx, sizeof(opponent_idx));

        while (MOX::combat_done_waiting == 0) {
            if (show_wait_message == 0) {
                RUSS::Mox_Update();
            } else {
                timer::Mark_Time();
                video::Set_Page_Off();
                CMBTDRW1::Draw_Main_Combat_Screen();
                fonts::Set_Font_Style(3, &MOX::combat_green);
                KEN::Ken_Get_Text_Message(55, message_buffer, sizeof(message_buffer));
                fonts::Print(20, 8, message_buffer);
                RUSS::Mox_Sync_Update();
                timer::Release_Time(1);
            }
            fields::Get_Input();
        }

        MOX::combat_done_waiting -= 1;
        NETMOX::Net_Debug_Log(
            "tactical barrier complete local_player=%d opponent=%d turn=%d cur_ship=%d cur_ptr=%d counter_after=%u choice=%d",
            (int)MOX::PLAYER_NUM,
            (int)opponent_idx,
            (int)MOX::combat_turn,
            MOX::cur_ship,
            (int)MOX::cur_ptr,
            (unsigned int)MOX::combat_done_waiting,
            (int)MOX::tactical_net_choice);
    }

    int16_t __cdecl Move_Ship(int16_t ship_idx, int16_t dest_x, int16_t dest_y, int16_t move_cost_unused) {
        (void)move_cost_unused;
        int16_t tractor_count = 0;
        int16_t test_move_result = 0;
        int16_t used_teleporter = CMBTFIR2::Used_Teleporter(ship_idx) != 0;
        int16_t move_cost = 0;
        int16_t defensive_fire_counter = 0;
        int16_t old_missile_warning_gt_0 = MOX::missile_warning > 0;
        int16_t old_shields_flag = MOX::shields_flag;

        int16_t dest_x_px = dest_x * 20;
        int16_t dest_y_px = dest_y * 20;

        int16_t tractor_ships[MAX_SHIPS + 1];
        int16_t tractor_target_idx;
        s_combat_data* ship = &MOX::combat_data[ship_idx];

        if (CMBTSPEC::Try_Get_Active_Tractor_Target(ship_idx, &tractor_target_idx)) {
            tractor_ships[tractor_count++] = ship_idx;
        }

        for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
            const int16_t active_tractor_target_idx = MOX::combat_data[i].special_cooldown == COMBAT_SHIP_NONE
                ? int16_t{-1}
                : MOX::combat_data[i].special_cooldown;
            if (active_tractor_target_idx == ship_idx &&
                CMBTSPEC::Try_Get_Active_Tractor_Target(i, &tractor_target_idx)) {
                tractor_ships[tractor_count++] = i;
            }
        }

        bool has_teleporter = COMBINIT::Does_Combat_Ship_Have_Special(ship_idx, SPECIAL_SUB_SPACE_TELEPORTER) != 0 ||
            COMBINIT::Does_Combat_Ship_Have_Special(ship_idx, SPECIAL_PHASE_SHIFTER) != 0;

        bool skip_normal_move = false;

        if (has_teleporter && !used_teleporter) {
            if (Teleporter_Destination_In_Range(
                    ship_idx, dest_x_px, dest_y_px)) {
                CMBTFIR2::Set_Teleporter_Used(ship_idx);
                Draw_Teleporting_Ship(ship_idx, dest_x, dest_y);
                skip_normal_move = true;
            }
        }

        if (!skip_normal_move) {
            int16_t center_x, center_y;
            COMBINIT::Ship_Center_Offsets(ship_idx, &center_x, &center_y);

            MOX::shields_flag = 0;

            if (MOX::combat_grid[dest_x][dest_y] != 2000) {
                return 0;
            }

            int16_t start_x_px = ship->x * 20;
            int16_t start_y_px = ship->y * 20;
            int16_t speed_px;

            if (MOX::net_flag != 0) {
                speed_px = 6;
            } else {
                if (MOX::speedx2_flag == 1) {
                    speed_px = 12;
                } else {
                    speed_px = 2;
                }
            }

            int16_t cur_x_px = start_x_px;
            int16_t cur_y_px = start_y_px;
            MOX::dont_draw_ship = ship_idx;

            if (MOX::super_fast_flag == 0) {
                CMBTSPEC::Start_Ship_Engine_SFX();
                video::Set_Page_Off();
                CMBTDRW1::Full_Draw_Combat_Screen();

                if (MOX::ship_status_display == 0) {
                    CMBTDRW1::Draw_Weapon_Status_Display(-1, -1);
                } else {
                    CMBTDRW1::Draw_Special_Status_Display();
                }

                graphics::Interlaced_Fill(401, 362, 489, 374, 0, 199);
                graphics::Interlaced_Fill(401, 379, 488, 467, 0, 199);
                CMBTDRW1::Draw_Internals_Display(401, 379);

                MOX::last_ship_status = 666;
                MOX::draw_active_missiles = 0;
                CMBTDRW1::Draw_Main_Combat_Screen();
                MOX::draw_active_missiles = 1;
                fields::Draw_Visible_Fields();
                video::Copy_Off_To_Back();
            }

            fields::Disable_Draw_All_Fields();

            int16_t facing_dir = ship->facing_dir;

            if (MOX::msl_impact_warning_flag == 1 && MOX::auto_msl_warning_off == 0 && MOX::missile_warning != 2) {
                test_move_result = Test_Move_Until_Impact(ship_idx, &dest_x_px, &dest_y_px, &facing_dir);
                if (test_move_result == 1 || dest_x * 20 != dest_x_px || dest_y * 20 != dest_y_px) {
                    MOX::missile_warning = 1;
                }
            }

            if (test_move_result != 0) {
                int16_t turns = Facing_Distance(facing_dir, ship->facing_dir);
                move_cost = turns * Turn_Cost(ship_idx);
            } else {
                move_cost = Move_Cost(ship_idx, dest_x_px / 20, dest_y_px / 20);
            }

            if (move_cost > 0 && test_move_result == 0) {
                facing_dir = Get_Facing(ship->x, ship->y, dest_x_px / 20, dest_y_px / 20);
                if (old_missile_warning_gt_0) {
                    MOX::missile_warning = 0;
                }
            }

            if (ship->facing_dir != facing_dir) {
                Rotate_Ship(ship_idx, facing_dir, cur_x_px - MOX::cur_combat_x * 20, cur_y_px - MOX::cur_combat_y * 20);
            }

            if (ship->unit_status == 0 && test_move_result == 0 && (start_x_px != dest_x_px || start_y_px != dest_y_px)) {
                cur_x_px = start_x_px;
                cur_y_px = start_y_px;
                int16_t hit_missiles[MAX_COMBAT_FLYERS];

                while (cur_x_px != dest_x_px || cur_y_px != dest_y_px) {
                    if (CMBTFIRE::Is_Combat_Ship_Dead(ship_idx) != 0) {
                        break;
                    }

                    special::Absolute_Interpolate_Line(
                        &cur_x_px, &cur_y_px, start_x_px, start_y_px, dest_x_px, dest_y_px, speed_px);

                    ship->x = cur_x_px / 20;
                    ship->y = cur_y_px / 20;

                    int16_t hit_missile_count = 0;
                    facing_dir = Get_Facing(cur_x_px, cur_y_px, dest_x_px, dest_y_px);

                    for (int16_t m = 0; m < MAX_COMBAT_FLYERS; ++m) {
                        s_missile* msl = &MOX::missile[m];
                        if (CMBTMIS::Missile_Targets_Ship(msl, ship_idx)) {
                            int16_t msl_speed = 5;
                            if (ship->current_speed != 0) {
                                msl_speed = ((uint8_t)msl->speed * speed_px) / ship->current_speed;
                            }
                            if (msl_speed < 2) {
                                msl_speed = 2;
                            }
                            // ORACLE: The original passes the full 16-bit distance;
                            // values above 127 must not be truncated to signed int8_t.
                            if (CMBTMIS::Move_Missile(m, center_x + cur_x_px, center_y + cur_y_px, msl_speed) != 0) {
                                hit_missiles[hit_missile_count++] = m;
                            }
                        }
                    }

                    if (MOX::super_fast_flag == 0) {
                        graphics::Set_Window(0, 0, 639, 350);
                        video::Clip_On();
                        video::Set_Page_Off();

                        if (tractor_count > 0) {
                            for (int16_t t = 0; t < tractor_count; ++t) {
                                CMBTSPEC::Draw_Moving_Tractor(tractor_ships[t], cur_x_px, cur_y_px);
                            }
                            CMBTDRW1::Draw_Main_Combat_Screen();
                            graphics::Set_Window(0, 0, 639, 350);
                            video::Clip_On();
                        }

                        CMBTDRW1::Draw_Ship(ship_idx, cur_x_px - MOX::cur_combat_x * 20, cur_y_px - MOX::cur_combat_y * 20);

                        for (int16_t m = 0; m < MAX_COMBAT_FLYERS; ++m) {
                            s_missile* msl = &MOX::missile[m];
                            if (CMBTMIS::Missile_Targets_Ship(msl, ship_idx)) {
                                CMBTMIS::Draw_Missile(m, msl->x - MOX::cur_combat_x * 20, msl->y - MOX::cur_combat_y * 20, 0);
                            }
                        }

                        RUSS::Mox_Sync_Update();
                        MOX::ship_frame++;
                        if (MOX::ship_frame == 32000) {
                            MOX::ship_frame = 0;
                        }

                        graphics::Reset_Window();
                        video::Clip_Off();
                    }

                    for (int16_t i = 0; i < hit_missile_count; ++i) {
                        if (ship->unit_status == 0) {
                            s_missile* msl = &MOX::missile[hit_missiles[i]];
                            if (msl->target_unit_idx == ship_idx) {
                                CMBTMIS::Resolve_Missile(hit_missiles[i], center_x + cur_x_px, center_y + cur_y_px);
                            }
                        }
                    }

                    if (CMBTFIRE::Is_Combat_Ship_Dead(ship_idx) == 0) {
                        if (ship->special_status_flag != 4) {
                            if (defensive_fire_counter % 3 == 0) {
                                CMBTFIR2::Defensive_Fire_Check(ship_idx, cur_x_px, cur_y_px, dest_x_px, dest_y_px);
                            }
                        }
                    }
                    defensive_fire_counter++;
                }
            }

            if (MOX::super_fast_flag == 0) {
                CMBTSPEC::End_Ship_Engine_SFX();
            }
        }

        ship->x = dest_x_px / 20;
        ship->y = dest_y_px / 20;
        ship->ai_range_tactic =
            (int8_t)CMBTFIR2::Max_Defensive_Fire_Range(ship_idx);
        MOX::dont_draw_ship = -1;
        MOX::cur_ship = ship_idx;
        MOX::shields_flag = old_shields_flag;

        if (tractor_count > 0) {
            for (int16_t t = 0; t < tractor_count; ++t) {
                int16_t t_ship_idx = tractor_ships[t];
                int16_t target_ship_idx;
                if (CMBTSPEC::Try_Get_Active_Tractor_Target(t_ship_idx, &target_ship_idx)) {
                    if (CMBTAI::Range_To_Ship(t_ship_idx, target_ship_idx) > 4) {
                        MOX::combat_data[t_ship_idx].special_cooldown = COMBAT_SHIP_NONE;
                        COMBAT1::Recalculate_Combat_Ship(target_ship_idx);
                    }
                }
            }
        }

        fields::Enable_Draw_All_Fields();
        return move_cost;
    }

    void __cdecl Turn_To_Face(int16_t ship_idx, int16_t target_x, int16_t target_y) {
        if (MOX::combat_data[ship_idx].movement_left == 0) {
            return;
        }

        int16_t turn_cost = CMBTMOV1::Turn_Cost(ship_idx);
        int16_t target_facing = CMBTMOV1::Get_Facing(
            MOX::combat_data[ship_idx].x,
            MOX::combat_data[ship_idx].y,
            target_x,
            target_y);

        int16_t facing_dir = MOX::combat_data[ship_idx].facing_dir;
        int16_t warning_already_active = MOX::missile_warning == 1;

        if (MOX::msl_impact_warning_flag == 1 &&
            MOX::auto_msl_warning_off == 0 &&
            MOX::missile_warning != 2) {
            int16_t safe_facing = target_facing;
            if (Rotate_Ship_Until_Impact(ship_idx, &safe_facing) != 0) {
                MOX::missile_warning = 1;
                target_facing = warning_already_active ? facing_dir : safe_facing;
            }
        }

        int16_t diff = facing_dir - target_facing;

        int16_t sign;
        if (diff < 0) {
            if (abs(diff) <= 8) {
                sign = 1;
            } else {
                sign = -1;
            }
        } else {
            if (diff <= 8) {
                sign = -1;
            } else {
                sign = 1;
            }
        }

        if (diff < 0) {
            diff += 16;
        }
        if (diff > 8) {
            diff = 16 - diff;
        }

        int32_t movement_left = MOX::combat_data[ship_idx].movement_left;
        int32_t cost = diff * turn_cost;

        if (cost <= movement_left) {
            MOX::combat_data[ship_idx].movement_left -= (int16_t)cost;
        } else {
            int16_t turns = (turn_cost + movement_left - 1) / turn_cost;
            int16_t new_facing = facing_dir + (turns * sign);

            if (new_facing < 0) {
                new_facing += 16;
            }

            MOX::combat_data[ship_idx].movement_left = 0;
            target_facing = new_facing % 16;
        }

        int32_t dx = (MOX::combat_data[ship_idx].x - MOX::cur_combat_x) * 20;
        int32_t dy = (MOX::combat_data[ship_idx].y - MOX::cur_combat_y) * 20;

        CMBTMOV1::Rotate_Ship(ship_idx, target_facing, (int16_t)dx, (int16_t)dy);
        COMBAT1::Assign_Combat_Grids(ship_idx);
        CMBTDRW1::Refresh_Combat_Screen_Full();
    }

    void __cdecl Draw_Teleporting_Ship(int16_t ship_idx, int16_t target_x, int16_t target_y) {
        int16_t off_x, off_y;
        int16_t ship_screen_x, ship_screen_y;
        int16_t target_screen_x, target_screen_y;
        s_animation_header* anim;
        int32_t sound_handle;
        int16_t step;
        int16_t step_10;
        int16_t counter;
        int16_t vanish_out, vanish_in;
        int32_t pitch;

        if (MOX::super_fast_flag != 0) {
            return;
        }

        if (!CMBTMIS::On_Combat_Screen(target_x, target_y)) {
            COMBINIT::Snap_Center_Combat_Screen(target_x, target_y);
        }

        MOX::dont_draw_ship = ship_idx;
        CMBTDRW1::Get_Draw_Ship_Offsets(ship_idx, &off_x, &off_y);

        ship_screen_x = (MOX::combat_data[ship_idx].x - MOX::cur_combat_x) * 20 + off_x;
        ship_screen_y = (MOX::combat_data[ship_idx].y - MOX::cur_combat_y) * 20 + off_y;

        target_screen_x = (target_x - MOX::cur_combat_x) * 20 + off_x;
        target_screen_y = (target_y - MOX::cur_combat_y) * 20 + off_y;

        allocate::Push_Block(MOX::screen_seg);
        anim = (s_animation_header*)allocate::Allocate_Next_Block(MOX::screen_seg, 0x2584);
        video::Set_Page_Off();
        CMBTDRW1::Full_Draw_Combat_Screen();

        if (MOX::speedx2_flag == 1 || MOX::net_flag != 0) {
            step = 2;
        } else {
            step = 1;
        }

        sound_handle = ERICNET::Mox_Play_Sound(0x37);
        pitch = (2 - (int32_t)MOX::combat_data[ship_idx].size_class) * 2500;
        sound::Pitch_Bend_Sound(sound_handle, pitch);

        counter = 0;
        step_10 = step * 10;
        vanish_out = 0;
        vanish_in = 100;

        do {
            CMBTDRW1::Draw_Main_Combat_Screen();
            graphics::Set_Window(0, 0, 639, 350);
            video::Clip_On();

            // Ship vanishing at current location
            CMBTDRW1::Draw_Ship_To_Bitmap(ship_idx, 0, 0, 0);
            bitmap::Vanish_Bitmap(anim, MOX::draw_ship_bitmap, vanish_out);
            animate::Draw(ship_screen_x, ship_screen_y, anim);

            // Ship appearing at target location
            CMBTDRW1::Draw_Ship_To_Bitmap(ship_idx, 0, 0, 0);
            bitmap::Vanish_Bitmap(anim, MOX::draw_ship_bitmap, vanish_in);
            animate::Draw(target_screen_x, target_screen_y, anim);

            video::Clip_Off();
            graphics::Reset_Window();
            RUSS::Mox_Sync_Update();

            counter += step;
            vanish_out += step_10;
            vanish_in -= step_10;
        } while (counter < 10);

        video::Clip_Off();
        allocate::Pop_Block(MOX::screen_seg);
        sound::Fadeout_Sound(sound_handle, 10);
    }


} // namespace CMBTMOV1