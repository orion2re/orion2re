#include "pch.h"

namespace PLNTSUM {
    constexpr int PLANET_CLIMATE_EXTENT = PLANET_CLIMATE_COUNT;
    constexpr int PLANET_SIZE_EXTENT = PLANET_SIZE_COUNT;
    constexpr int PLANET_PICTURE_COUNT = PLANET_CLIMATE_EXTENT * PLANET_SIZE_EXTENT;

    void __cdecl Summary_Screen_Print_Data(void) {
        int16_t monster_race = -1;
        int first_visible = static_cast<int>(MOX::scroll_bar->first_visible_row);
        int end_index = first_visible + static_cast<int>(MOX::scroll_bar->total_rows);

        if (first_visible + 8 <= end_index) {
            end_index = first_visible + 8;
        }

        for (int16_t scroll_row = 0; scroll_row < (end_index - first_visible); ++scroll_row) {
            int16_t summary_idx = static_cast<int16_t>(first_visible + scroll_row);
            int16_t y_base = PLNTSUM::plntscrn_delta_y[scroll_row] + 0x27;
            s_plntsum_planet* summary = &PLNTSUM::planet_summary[summary_idx];

            if (summary->explored != -1) {
                s_planet_data* planet = &MOX::planet[summary->explored];
                int16_t star_idx = planet->star_index;
                int16_t mineral_class = summary->mineral_class;
                int16_t climate_idx = summary->climate;
                int16_t gravity_class = static_cast<int16_t>(planet->gravity_class);

                char color_choice = 0;
                if (PLNTSUM::send_choice == 0 || PLNTSUM::send_choice == 1) {
                    if (scroll_row == (int16_t)PLNTSUM::box_field_scanned) {
                        color_choice = (PLNTSUM::can_reach_scanned_planet != 0) ? 1 : 2;
                    }
                } else if ((int16_t)PLNTSUM::box_field_scanned == scroll_row) {
                    color_choice = 1;
                }

                s_colors font_colors;
                Colony_Owner_Colors(planet->colony_index, &font_colors, color_choice);

                graphics::Set_Window(0x12, y_base, 0x68, PLNTSUM::plntscrn_delta_y[scroll_row + 1] + y_base);
                video::Clip_On();
                fonts::Set_Font_Style(1, &font_colors);

                int16_t special_id = 0;
                char system_special = MOX::star[star_idx].system_special;
                if (system_special > SYSTEM_SPECIAL_NO_SPECIAL && system_special != SYSTEM_SPECIAL_ORION_STAR) {
                    special_id = static_cast<int16_t>(system_special);
                }
                if (planet->planet_special == SYSTEM_SPECIAL_ANCIENT_ARTIFACTS) {
                    special_id = SYSTEM_SPECIAL_ANCIENT_ARTIFACTS;
                }

                if (special_id > SYSTEM_SPECIAL_NO_SPECIAL && HACCESS::Special_Is_Planet_Special(special_id)) {
                    char* special_str = nullptr;
                    char special_buf[80];

                    if (MOX::star[star_idx].system_special == SYSTEM_SPECIAL_SPACE_MONSTER) {
                        int16_t ship_idx = HAROLD::First_Ship_Orbiting_Star(&MOX::star[star_idx]);
                        if (ship_idx != -1) {
                            special_str = MOX::ship[ship_idx].d.name;
                        } else if (special_id == SYSTEM_SPECIAL_ANCIENT_ARTIFACTS) {
                            strlcpy(special_buf, HAROLD::H_Message(140), sizeof(special_buf));
                            special_str = special_buf;
                        } else {
                            special_str = MOX::planet_special_string[special_id];
                        }
                    } else if (planet->planet_special == special_id) {
                        special_str = MOX::planet_special_string[special_id];
                    }

                    if (special_str) {
                        fonts::Print_Centered(0x3d, y_base, special_str);
                    }
                }

                fonts::Set_Font_Style(2, &font_colors);
                int16_t font_h = fonts::Get_Font_Height();
                int16_t y_offset = (25 - font_h) - 4;

                char planet_name_buf[32];
                strlcpy(planet_name_buf, HACCESS::Get_Planet_Name_No_Prime(summary->explored), sizeof(planet_name_buf));
                MISC::Set_Fitted_Font_Style(planet_name_buf, &font_colors, 2, 2, 0, 0x56, 1);
                int16_t fitted_name_h = fonts::Get_Font_Height();
                fonts::Print_Centered(0x3d, y_base + (25 - fitted_name_h) * 2, planet_name_buf);

                char owner_label[100];
                bool has_owner_label = false;

                if (planet->colony_index != -1) {
                    char* race_name = MOX::player[MOX::colony[planet->colony_index].owner].race_name;
                    snprintf(owner_label, sizeof(owner_label), "(%s)", race_name);
                    has_owner_label = true;
                } else if (HAROLD::Star_Guarded_By_Monster(star_idx, &monster_race)) {
                    char race_buf[20];
                    HACCESS::Race_Name(monster_race, race_buf, sizeof(race_buf));
                    snprintf(owner_label, sizeof(owner_label), "(%s)", race_buf);
                    has_owner_label = true;
                }

                if (has_owner_label) {
                    fonts::Set_Font_Style(1, &font_colors);
                    fonts::Print_Centered(0x3d, (y_base - fonts::Get_Font_Height()) + 0x30, owner_label);
                }

                video::Clip_Off();
                graphics::Reset_Window();

                int16_t y_climate = y_base + y_offset;
                char* climate_str = MOX::planet_climate_string[climate_idx];
                MISC::Set_Fitted_Font_Style(climate_str, &font_colors, 2, 2, -1, 0x41, 1);
                int16_t climate_h = fonts::Get_Font_Height();
                fonts::Print_Centered(0x8c, y_climate, climate_str);

                char work_buf[100];
                char food_text[16];
                fonts::Set_Font_Style(1, &font_colors);
                COLCALC::Format_Food2(
                    (uint16_t)summary->food_per_farmer,
                    food_text,
                    sizeof(food_text));
                snprintf(work_buf, sizeof(work_buf), "%s Food", food_text);
                fonts::Print_Centered(0x8c, y_climate + climate_h + 4, work_buf);

                char* gravity_str = MOX::planet_gravity_string[gravity_class];
                fonts::Set_Font_Style(2, &font_colors);
                int16_t gravity_h = fonts::Get_Font_Height();
                int16_t penalty = Productivity_Penalty(MOX::PLAYER_NUM, gravity_class);
                int16_t y_gravity;

                if (penalty == 0) {
                    y_gravity = y_climate + (gravity_h / 2);
                } else {
                    fonts::Print_Centered(0xd9, y_climate, gravity_str);
                    fonts::Set_Font_Style(1, &font_colors);
                    snprintf(work_buf, sizeof(work_buf), HAROLD::H_Message(0x142), (int)penalty);
                    y_gravity = y_climate + gravity_h + 4;
                }
                fonts::Print_Centered(0xd9, y_gravity, (penalty == 0) ? gravity_str : work_buf);

                fonts::Set_Font_Style(2, &font_colors);
                int16_t mineral_h = fonts::Get_Font_Height();
                fonts::Print_Centered(0x137, y_climate, MOX::mineral_class_string[mineral_class]);
                fonts::Set_Font_Style(1, &font_colors);
                snprintf(work_buf, sizeof(work_buf), HAROLD::H_Message(0x152), (int)summary->minerals);
                fonts::Print_Centered(0x137, y_climate + mineral_h + 4, work_buf);

                fonts::Set_Font_Style(2, &font_colors);
                int16_t size_h = fonts::Get_Font_Height();
                fonts::Print_Centered(0x182, y_climate, MOX::planet_size_string[planet->size]);
                fonts::Set_Font_Style(1, &font_colors);
                snprintf(work_buf, sizeof(work_buf), HAROLD::H_Message(0x143), (int)summary->max_pop);
                fonts::Print_Centered(0x182, y_climate + size_h + 4, work_buf);

                if (summary->colony_eta >= 0) {
                    PLNTSUM::Draw_Colony_ETA(summary_idx, scroll_row);
                }
                if (summary->outpost_eta >= 0) {
                    PLNTSUM::Draw_Outpost_ETA(summary_idx, scroll_row);
                }
            }
        }
    }

    void __cdecl Send_Nearest_Ship(int16_t planet_idx) {
        char msg_buffer[120];
        int16_t ship_list[2];
        s_ship_move_info move_info;
        int16_t nearest_ship_idx;
        int16_t summary_idx;
        int16_t target_star_idx;
        int16_t ship_origin_star_idx;
        bool should_continue = true;
        bool omniscient;
        uint8_t ship_type;
        char race_name_buffer[32];

        ship_list[0] = -1;
        ship_list[1] = -1;

        target_star_idx = MOX::planet[planet_idx].star_index;
        omniscient = HAROLD::Player_Is_Omniscient(MOX::PLAYER_NUM);
        summary_idx = Planet_Summary_Id(planet_idx);
        nearest_ship_idx = Nearest_Ship_Id(summary_idx);

        ship_type = MOX::ship[nearest_ship_idx].d.ship_type;
        ship_origin_star_idx = HAROLD::Absolute_Location(MOX::ship[nearest_ship_idx].location);

        if (((ship_origin_star_idx != target_star_idx) && (ship_type == SHIP_TYPE_COLONY) && (PLNTSUM::planet_summary[summary_idx].colony_eta > 0)) ||
            ((ship_type == SHIP_TYPE_OUTPOST) && (PLNTSUM::planet_summary[summary_idx].outpost_eta > 0))) {
            snprintf(msg_buffer, sizeof(msg_buffer), HAROLD::H_Message(0x134 /* HESTR_134_YOU_ALREADY_HAVE_ONE_C */), &MOX::star[target_star_idx]);
            mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);
            should_continue = (GENDRAW::Confirmation_Box(msg_buffer) != 0);
            PLNTSUM::screen->refresh_fields = true;
            if (!should_continue) {
                return;
            }
        }

        if (nearest_ship_idx <= -1 || MOX::ship[nearest_ship_idx].location < SHIP_LOCATION_MOVING_OFFSET) {
            if (nearest_ship_idx == -1) {
                should_continue = false;
            }
        } else {
            bool visited_origin = HAROLD::Player_Has_Visited(MOX::PLAYER_NUM, ship_origin_star_idx);
            bool visited_dest = HAROLD::Player_Has_Visited(MOX::PLAYER_NUM, target_star_idx);

            if (ship_origin_star_idx == target_star_idx) {
                snprintf(msg_buffer, sizeof(msg_buffer), HAROLD::H_Message(0x138 /* HESTR_138_S_ALREADY_HAS_A_COURSE */), &MOX::ship[nearest_ship_idx], &MOX::star[target_star_idx]);
                GENDRAW::Message_Box(msg_buffer);
                PLNTSUM::screen->refresh_fields = true;
                return;
            }

            if (omniscient || (visited_dest && visited_origin)) {
                snprintf(msg_buffer, sizeof(msg_buffer), HAROLD::H_Message(0x139 /* HESTR_139_S_HAS_SET_A_COURSE_FOR */), &MOX::ship[nearest_ship_idx], &MOX::star[ship_origin_star_idx], &MOX::star[target_star_idx]);
            } else if (visited_dest) {
                snprintf(msg_buffer, sizeof(msg_buffer), HAROLD::H_Message(0x13A /* HESTR_13A_S_HAS_SET_A_COURSE_SHO */), &MOX::ship[nearest_ship_idx], &MOX::star[target_star_idx]);
            } else if (visited_origin) {
                snprintf(msg_buffer, sizeof(msg_buffer), HAROLD::H_Message(0x139 /* HESTR_139_S_HAS_SET_A_COURSE_FOR */), &MOX::ship[nearest_ship_idx], &MOX::star[ship_origin_star_idx]);
            } else {
                snprintf(msg_buffer, sizeof(msg_buffer), HAROLD::H_Message(0x13A /* HESTR_13A_S_HAS_SET_A_COURSE_SHO */), &MOX::ship[nearest_ship_idx]);
            }

            mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);
            should_continue = (GENDRAW::Confirmation_Box(msg_buffer) != 0);
            PLNTSUM::screen->refresh_fields = true;
        }

        if (should_continue) {
            int16_t best_ship_idx;
            if (PLNTSUM::send_choice == 1) {
                best_ship_idx = PLNTSUM::planet_summary[summary_idx].best_outpost_ship_idx;
            } else {
                best_ship_idx = PLNTSUM::planet_summary[summary_idx].best_colony_ship_idx;
            }

            ship_list[0] = best_ship_idx;
            ship_list[1] = -1;

            SHIPMOVE::Ships_Try_To_Move_To(ship_list, target_star_idx, &move_info);

            if (move_info.moving == 0) {
                if (move_info.hyperspace_flux != 0) {
                    GENDRAW::Warning_Box(HAROLD::H_Message(0x13F /* HESTR_13F_SHIP_MOVEMENT_IS_NOT_A */));
                    PLNTSUM::screen->refresh_fields = true;
                }
            } else {
                bool user_confirmed_move = true;
                int16_t monster_race_summary_id;
                if (HAROLD::Star_Guarded_By_Monster(target_star_idx, &monster_race_summary_id)) {
                    HACCESS::Race_Name(monster_race_summary_id, race_name_buffer, sizeof(race_name_buffer));
                    mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);

                    int16_t msg_id = (PLNTSUM::send_choice == 0) ? 0x13C /* HESTR_13C_S_IS_GUARDED_BY_A_S_DO */ : 0x13B /* HESTR_13B_S_IS_GUARDED_BY_A_S_DO */;
                    snprintf(msg_buffer, sizeof(msg_buffer), HAROLD::H_Message(msg_id), &MOX::star[target_star_idx], race_name_buffer);
                    user_confirmed_move = (GENDRAW::Confirmation_Box(msg_buffer) != 0);
                }

                if (user_confirmed_move) {
                    SHIPMOVE::Make_Ships_Move_To(ship_list, target_star_idx, &move_info);

                    int16_t confirm_msg_id = (move_info.turns_left == 1) ? 0x13D : 0x13E;

                    snprintf(msg_buffer, sizeof(msg_buffer), HAROLD::H_Message(confirm_msg_id), &MOX::ship[best_ship_idx], &MOX::star[target_star_idx], (uint32_t)(uint8_t)move_info.turns_left);
                    GENDRAW::Message_Box(msg_buffer);
                    PLNTSUM::screen->refresh_fields = true;
                    PLNTSUM::last_ship_moved = best_ship_idx;
                    PLNTSUM::Set_Auto_Colonize_Flag(&MOX::ship[best_ship_idx], planet_idx);
                    PLNTSUM::Update_Ship_Etas();
                }
            }
        }
    }
    void __cdecl Allocate_Space_For_Summary_Screen() {
        planet_summary = (s_plntsum_planet*)allocate::Allocate_Next_Block(MOX::screen_seg, sizeof(*planet_summary) * MAX_PLANETS);
        memset(planet_summary, 0xFF, sizeof(*planet_summary) * MAX_PLANETS);

        screen = (s_plntsum_screen*)allocate::Allocate_Next_Block_Aligned(
            MOX::screen_seg, sizeof(*screen), alignof(s_plntsum_screen));
        memset(screen, 0, sizeof(*screen));

        ship_summary = (s_plntsum_ship*)allocate::Allocate_Next_Block(MOX::screen_seg, sizeof(*ship_summary) * MAX_SHIPS);
        memset(ship_summary, 0xFF, sizeof(*ship_summary) * MAX_SHIPS);

        MOX::scroll_bar = (s_scroll_bar*)allocate::Allocate_Next_Block(MOX::screen_seg, sizeof(*MOX::scroll_bar));
        memset(MOX::scroll_bar, 0, sizeof(*MOX::scroll_bar));
    }

    void __cdecl Load_Planet_Summary_Pictures(void) {
        allocate::Get_Remaining_Block_Space(MOX::screen_seg);

        screen->pics[0] = (s_animation_header*)farload::Far_Reload_Next(PTR_s_PLNTSUM_LBX_00556AC0, 0, MOX::screen_seg);
        screen->pics[1] = (s_animation_header*)farload::Far_Reload_Next(PTR_s_PLNTSUM_LBX_00556AC0, 1, MOX::screen_seg);
        screen->pics[2] = (s_animation_header*)farload::Far_Reload_Next(PTR_s_PLNTSUM_LBX_00556AC0, 2, MOX::screen_seg);
        screen->pics[3] = (s_animation_header*)farload::Far_Reload_Next(PTR_s_PLNTSUM_LBX_00556AC0, 3, MOX::screen_seg);

        RUSS::Mox_Update();

        screen->pics[4] = (s_animation_header*)farload::Far_Reload_Next(PTR_s_PLNTSUM_LBX_00556AC0, 4, MOX::screen_seg);
        screen->pics[5] = (s_animation_header*)farload::Far_Reload_Next(PTR_s_PLNTSUM_LBX_00556AC0, 5, MOX::screen_seg);
        screen->pics[6] = (s_animation_header*)farload::Far_Reload_Next(PTR_s_PLNTSUM_LBX_00556AC0, 6, MOX::screen_seg);
        screen->pics[7] = (s_animation_header*)farload::Far_Reload_Next(PTR_s_PLNTSUM_LBX_00556AC0, 7, MOX::screen_seg);

        RUSS::Mox_Update();

        screen->pics[8] = (s_animation_header*)farload::Far_Reload_Next(PTR_s_PLNTSUM_LBX_00556AC0, 8, MOX::screen_seg);

        send_colony_ship_button_seg = (s_animation_header*)farload::Far_Reload_Next(PTR_s_PLNTSUM_LBX_00556AC0, 9, MOX::screen_seg);
        send_outpost_ship_button_seg = (s_animation_header*)farload::Far_Reload_Next(PTR_s_PLNTSUM_LBX_00556AC0, 10, MOX::screen_seg);

        screen->pics[9] = (s_animation_header*)farload::Far_Reload_Next(PTR_s_PLNTSUM_LBX_00556AC0, 11, MOX::screen_seg);
        screen->pics[10] = (s_animation_header*)farload::Far_Reload_Next(PTR_s_PLNTSUM_LBX_00556AC0, 12, MOX::screen_seg);
        screen->pics[12] = (s_animation_header*)farload::Far_Reload_Next(PTR_s_PLNTSUM_LBX_00556AC0, 13, MOX::screen_seg);

        RUSS::Mox_Update();

        screen->pics[11] = (s_animation_header*)farload::Far_Reload_Next(PTR_s_PLNTSUM_LBX_00556AC0, 14, MOX::screen_seg);

        allocate::Get_Remaining_Block_Space(MOX::screen_seg);

        for (int i = 0; i < 11; ++i) {
            MOX::planet_summary_galaxy_star_seg[i] = (s_animation_header*)farload::Far_Reload_Next(PTR_s_PLNTSUM_LBX_00556AC0, 15 + i, MOX::screen_seg);
            RUSS::Mox_Update();
        }

        allocate::Get_Remaining_Block_Space(MOX::screen_seg);
        Load_Player_Icon();
        palette::Set_Palette_Changes(0, 255);
        reduce::Clear_Reduced_Palette_Groups();
        allocate::Get_Remaining_Block_Space(MOX::screen_seg);
    }

    void __cdecl Scan_Summary_Box_Fields(int16_t input_field_id, int16_t clicked_field_id, int16_t* last_input_field_id_ptr) {
        int first_row = (int)MOX::scroll_bar->first_visible_row;
        int total_rows = (int)MOX::scroll_bar->total_rows;
        int max_visible = first_row + 8;
        int end_idx = (first_row + total_rows < max_visible) ? (first_row + total_rows) : max_visible;

        PLNTSUM::can_reach_scanned_planet = false;
        PLNTSUM::box_field_scanned = -1;

        int count = end_idx - first_row;
        if (count > 0) {
            for (int16_t i = 0; i < (int16_t)count; ++i) {
                int current_row = first_row + i;
                int16_t explored_idx = PLNTSUM::planet_summary[current_row].explored;

                if (explored_idx != -1) {
                    int16_t star_idx = MOX::planet[explored_idx].star_index;

                    // Handle interaction: Click on fields/icons
                    if (PLNTSUM::screen->fields[i] == clicked_field_id) {
                        if (PLNTSUM::send_choice == 0 || PLNTSUM::send_choice == 1) {
                            int16_t planet_id = PLNTSUM::Planet_Summary_Id(explored_idx);
                            int16_t ship_id = PLNTSUM::Nearest_Ship_Id(planet_id);
                            if (ship_id > -1) {
                                PLNTSUM::Send_Nearest_Ship(explored_idx);
                                PLNTSUM::Determine_Button_Status();
                                PLNTSUM::Reset_Ship_To_Star_Distances();
                            }
                        }
                    } else if (PLNTSUM::screen->colony_icons[i] == clicked_field_id) {
                        PLNTSUM::Undo_Send_Order(explored_idx, SHIP_TYPE_COLONY);
                        PLNTSUM::Determine_Button_Status();
                        PLNTSUM::Reset_Ship_To_Star_Distances();
                    } else if (PLNTSUM::screen->output_icons[i] == clicked_field_id) {
                        PLNTSUM::Undo_Send_Order(explored_idx, SHIP_TYPE_OUTPOST);
                        PLNTSUM::Determine_Button_Status();
                        PLNTSUM::Reset_Ship_To_Star_Distances();
                    }
                    // Handle interaction: Hover/Input on fields
                    else if (PLNTSUM::screen->fields[i] == input_field_id && current_row < total_rows) {
                        PLNTSUM::box_field_scanned = (int8_t)i;
                        if (explored_idx > -1) {
                            int16_t planet_id = PLNTSUM::Planet_Summary_Id(explored_idx);
                            int16_t ship_id = PLNTSUM::Nearest_Ship_Id(planet_id);
                            if (ship_id != -1 && (PLNTSUM::send_choice == 0 || PLNTSUM::send_choice == 1)) {
                                PLNTSUM::can_reach_scanned_planet = true;
                            }
                        }

                        if (*last_input_field_id_ptr != input_field_id) {
                            MOX::scanned_field = star_idx + 1000 /* 0x3E8 */;
                            int16_t target_id = -1;
                            if (PLNTSUM::send_choice == 0) {
                                target_id = PLNTSUM::planet_summary[current_row].best_colony_ship_idx;
                            } else if (PLNTSUM::send_choice == 1) {
                                target_id = PLNTSUM::planet_summary[current_row].best_outpost_ship_idx;
                            }

                            if (target_id != -1) {
                                int16_t targets[2];
                                targets[0] = target_id;
                                targets[1] = -1;
                                SHIPMOVE::Ships_Try_To_Move_To(targets, star_idx, &MOX::g_ship_move_info);
                                MOX::scanned_field = star_idx;
                            }
                            *last_input_field_id_ptr = input_field_id;
                        }
                    } else if (PLNTSUM::screen->colony_icons[i] == input_field_id || PLNTSUM::screen->output_icons[i] == input_field_id) {
                        PLNTSUM::can_reach_scanned_planet = true;
                    }
                }
            }
        }
    }

    void __cdecl Load_Player_Icon() {
        int entry_num = (int)(short)((uint8_t)MOX::player[MOX::PLAYER_NUM].color + 0x4C);

        s_animation_header* player_icon = (s_animation_header*)farload::Far_Reload_Next(
            PLNTSUM::PTR_s_PLNTSUM_LBX_00556AC0,
            entry_num,
            MOX::screen_seg);

        PLNTSUM::screen->pics[13] = player_icon;
    }

    void __cdecl Wrapper_For_Planet_Summary_Galaxy_Map() {
        short star_x, star_y;
        uint32_t color = 0xE2;

        graphics::Set_Window(443, 17, 623, 133);
        video::Clip_On();

        if (MOX::scanned_field != -1) {
            int16_t star_idx = MOX::scanned_field;
            if (star_idx < 1000) {
                Draw_Nearest_Ship_Directional_Line(star_idx);
            } else if (star_idx < MOX::NUM_STARS + 1000) {
                Draw_Nearest_Ship_Directional_Line(star_idx - 1000);
            }

            if (star_idx >= 1000 && star_idx < MOX::NUM_STARS + 1000) {
                star_idx -= 1000;
            }

            MOVEBOX::Get_Galaxy_Map_Star_XY(star_idx, 443, 17, 180, 116, 0, 0, 2, &star_x, &star_y);

            int8_t owner = MOX::star[star_idx].owner;
            if (owner > -1) {
                color = (uint32_t)(MOX::player[owner].color * 6 + 212);
            }

            graphics::Box(star_x - 1, star_y - 1, 9, 9, 1, (uint8_t)color);
        }

        for (int16_t i = 0; i < PLNTSUM::n_ships_in_summary; ++i) {
            bool found_colony = Summary_Found_Colony_Ship(i);
            bool found_outpost = Summary_Found_Outpost_Ship(i);

            if (found_colony || found_outpost) {
                short marker_x, marker_y;
                Get_Colony_Outpost_Marker_XY(i, &marker_x, &marker_y);
                Print_Colony_Outpost_Marker(marker_x, marker_y, i, found_colony, found_outpost);
            }
        }

        video::Clip_Off();
        graphics::Reset_Window();

        MOVEBOX::Draw_Galaxy_Map_Box(nullptr, nullptr, 443, 17, 180, 116, 0, 0, 0, 0, 2, -1);

        PLNTSUM::planet_summary->scan_highlight_pulse += (short)(PLNTSUM::planet_summary->scan_highlight_pulse_step * 2);

        int8_t step = PLNTSUM::planet_summary->scan_highlight_pulse_step;
        if (step > 0) {
            if (PLNTSUM::planet_summary->scan_highlight_pulse >= 3) {
                PLNTSUM::planet_summary->scan_highlight_pulse_step = -step;
            }
        } else if (step < 0) {
            if (PLNTSUM::planet_summary->scan_highlight_pulse <= 0) {
                PLNTSUM::planet_summary->scan_highlight_pulse_step = -step;
            }
        }
    }

    int16_t __cdecl Nearest_Ship_Id(int16_t index) {
        if (PLNTSUM::send_choice == 0) {
            return PLNTSUM::planet_summary[index].best_colony_ship_idx;
        }

        if (PLNTSUM::send_choice == 1) {
            return PLNTSUM::planet_summary[index].best_outpost_ship_idx;
        }

        return -1;
    }

    void __cdecl Draw_Nearest_Ship_Directional_Line(int16_t star_idx) {
        int16_t summary_idx;
        s_colors colors;
        uint8_t colony_selectable;
        uint8_t outpost_selectable;
        bool colony_active = false;
        bool outpost_active = false;
        int16_t ship_idx;
        int16_t marker_x, marker_y;
        int16_t star_x, star_y;
        int16_t dist;

        summary_idx = Summary_Planet_Index(star_idx);

        colors.colors[0] = 0xE2;
        colors.colors[1] = 0xE3;
        colors.colors[2] = 0xE4;
        colors.colors[3] = 0xE5;
        colors.colors[4] = 0xE4;
        colors.colors[5] = 0xE3;
        colors.colors[6] = 0xE3;
        colors.colors[7] = 0xE2;

        colony_selectable = (uint8_t)Send_Colony_Button_Selectable();
        outpost_selectable = (uint8_t)Send_Outpost_Button_Selectable();

        if (summary_idx == -1) {
            return;
        }

        if (!colony_selectable && !outpost_selectable) {
            return;
        }

        if (PLNTSUM::draw_colony_button_status == 1) {
            colony_active = true;
        } else if (PLNTSUM::draw_outpost_button_status == 1) {
            outpost_active = true;
        } else if (!colony_selectable && outpost_selectable) {
            outpost_active = true;
        } else {
            colony_active = true;
        }

        if (colony_active) {
            ship_idx = PLNTSUM::planet_summary[summary_idx].best_colony_ship_idx;
        } else {
            ship_idx = PLNTSUM::planet_summary[summary_idx].best_outpost_ship_idx;
        }

        if (ship_idx == -1) {
            return;
        }

        bool ship_status_is_one = (MOX::ship[ship_idx].status == 1);

        Get_Colony_Outpost_Marker_XY(PLNTSUM::planet_summary[summary_idx].ship_summary_idx, &marker_x, &marker_y);

        if (colony_active && ship_idx > -1 && PLNTSUM::can_reach_star[star_idx] != 0 && !ship_status_is_one) {
            MOVEBOX::Get_Galaxy_Map_Star_XY(star_idx, 0x1BB, 0x11, 0xB4, 0x74, 0, 0, 0, &star_x, &star_y);
            dist = special::Range(marker_x, marker_y, star_x + 4, star_y + 3);
            if (dist > 10) {
                SHIPS::Draw_Directional_Multi_Colored_Line(marker_x + 7, marker_y + 2, star_x + 2, star_y + 2, &colors, MOX::multi_colored_line_start);

                int32_t next_idx = (int32_t)MOX::multi_colored_line_start + 1;
                int32_t abs_val = (next_idx >= 0) ? next_idx : -next_idx;
                int32_t result = abs_val % 8;
                MOX::multi_colored_line_start = (int16_t)((next_idx >= 0) ? result : -result);
            }
        }

        if (outpost_active && ship_idx > -1 && PLNTSUM::can_reach_star[star_idx] != 0 && !ship_status_is_one) {
            MOVEBOX::Get_Galaxy_Map_Star_XY(star_idx, 0x1BB, 0x11, 0xB4, 0x74, 0, 0, 0, &star_x, &star_y);
            dist = special::Range(marker_x, marker_y, star_x + 4, star_y + 3);
            if (dist > 10) {
                SHIPS::Draw_Directional_Multi_Colored_Line(marker_x + 8, marker_y + 2, star_x + 2, star_y + 2, &colors, MOX::multi_colored_line_start);

                int32_t next_idx = (int32_t)MOX::multi_colored_line_start + 1;
                int32_t abs_val = (next_idx >= 0) ? next_idx : -next_idx;
                int32_t result = abs_val % 8;
                MOX::multi_colored_line_start = (int16_t)((next_idx >= 0) ? result : -result);
            }
        }
    }

    void __cdecl Print_Colony_Outpost_Marker(int16_t x, int16_t y, int16_t ship_summary_idx, int8_t colony, int8_t outpost) {
        (void)ship_summary_idx;
        s_colors inactive_colors;
        s_colors active_colors;

        inactive_colors.colors[0] = 0;
        inactive_colors.colors[1] = ':';
        inactive_colors.colors[2] = ':';
        inactive_colors.colors[3] = 'C';
        inactive_colors.colors[4] = 'B';
        inactive_colors.colors[5] = 'A';
        inactive_colors.colors[6] = '?';
        inactive_colors.colors[7] = '?';
        active_colors.colors[0] = 0;

        for (int16_t i = 1; i < 8; ++i) {
            int16_t color_idx = PLNTSUM::planet_summary->scan_highlight_pulse + i;
            if (color_idx < 1) {
                color_idx = 1;
            }
            if (color_idx > 7) {
                color_idx = 7;
            }
            active_colors.colors[i] = inactive_colors.colors[color_idx];
        }

        if (outpost != 0 && colony != 0) {
            int16_t cur_x = x + 3;
            int16_t cur_y = y - 1;

            if (MOX::scanned_field > -1 && MOX::scanned_field < 1000) {
                if (PLNTSUM::send_choice == 1) {
                    fonts::Set_Font_Style(0, &active_colors);
                    int16_t bottom = fonts::Get_Font_Height() + cur_y;
                    int16_t right = fonts::Get_String_Width("oc") + cur_x;
                    graphics::Fill(cur_x, cur_y, right, bottom, 1);
                    fonts::Print(cur_x, cur_y, "o");
                    fonts::Set_Font_Style(0, &inactive_colors);
                    int16_t text_x = fonts::Get_String_Width("o") + cur_x;
                    fonts::Print(text_x, cur_y, "c");
                    return;
                }

                if (PLNTSUM::send_choice == 0) {
                    fonts::Set_Font_Style(0, &inactive_colors);
                    int16_t bottom = fonts::Get_Font_Height() + cur_y;
                    int16_t right = fonts::Get_String_Width("oc") + cur_x;
                    graphics::Fill(cur_x, cur_y, right, bottom, 1);
                    fonts::Print(cur_x, cur_y, "o");
                    fonts::Set_Font_Style(0, &active_colors);
                    int16_t text_x = fonts::Get_String_Width("o") + cur_x;
                    fonts::Print(text_x, cur_y, "c");
                    return;
                }
            }

            fonts::Set_Font_Style(0, &inactive_colors);
            fonts::Print(cur_x, cur_y, "oc");
        } else if (outpost != 0) {
            int16_t cur_x = x + 6;
            int16_t cur_y = y - 1;

            if (MOX::scanned_field > -1 && MOX::scanned_field < 1000) {
                fonts::Set_Font_Style(0, &active_colors);
                int16_t bottom = fonts::Get_Font_Height() + cur_y;
                int16_t right = fonts::Get_String_Width("o") + cur_x;
                graphics::Fill(cur_x, cur_y, right, bottom, 1);
            } else {
                fonts::Set_Font_Style(0, &inactive_colors);
            }

            fonts::Print(cur_x, cur_y, "o");
        } else if (colony != 0) {
            int16_t cur_x = x + 6;
            int16_t cur_y = y - 1;

            if (MOX::scanned_field > -1 && MOX::scanned_field < 1000) {
                fonts::Set_Font_Style(0, &active_colors);
                int16_t bottom = fonts::Get_Font_Height() + cur_y;
                int16_t right = fonts::Get_String_Width("c") + cur_x;
                graphics::Fill(cur_x, cur_y, right, bottom, 1);
            } else {
                fonts::Set_Font_Style(0, &inactive_colors);
            }

            fonts::Print(cur_x, cur_y, "c");
        }
    }

    void __cdecl Get_Colony_Outpost_Marker_XY(int16_t summary_idx, int16_t* out_x, int16_t* out_y) {
        s_plntsum_ship* summary = &PLNTSUM::ship_summary[summary_idx];
        int16_t ship_idx = summary->outpost_ship_idx;

        if (ship_idx <= summary->colony_ship_idx) {
            ship_idx = summary->colony_ship_idx;
        }

        s_ship_data* ship = &MOX::ship[ship_idx];
        int16_t location = ship->location;

        bool is_space_location = (location >= SHIP_LOCATION_MOVING_OFFSET && location < SHIP_LOCATION_WORMHOLE_OFFSET);
        bool is_travel_location = (location >= SHIP_LOCATION_WORMHOLE_OFFSET && location < SHIP_LOCATION_LIMIT);

        if (is_space_location) {
            MOVEBOX::Get_Galaxy_Map_Space_XY(summary->ship_icon_id, 443, 17, 180, 116, out_x, out_y);
            *out_x += 2;
            return;
        }

        int16_t star_idx = location;
        if (is_travel_location) {
            star_idx = HAROLD::XY_To_Star_Id(ship->x, ship->y);
        }

        MOVEBOX::Get_Galaxy_Map_Star_XY(star_idx, 443, 17, 180, 116, 0, 0, 2, out_x, out_y);
        *out_x += 2;
    }

    void Draw_Rotating_Planets() {
        int16_t first_row = MOX::scroll_bar->first_visible_row;
        int16_t total_rows = MOX::scroll_bar->total_rows;
        int32_t upper_bound = first_row + total_rows;

        // Draw at most 8 rows starting from the scrollbar position
        if (first_row + 8 < upper_bound) {
            upper_bound = first_row + 8;
        }

        int32_t count = (int16_t)upper_bound - first_row;
        if (count > 0) {
            for (int16_t i = 0; i < (int16_t)count; ++i) {
                int16_t explored_planet_idx = PLNTSUM::planet_summary[first_row + i].explored;

                if (explored_planet_idx != -1) {
                    s_planet_data* planet = &MOX::planet[explored_planet_idx];

                    // Planet animation index is determined by climate type and planet size
                    int16_t anim_idx = (int16_t)planet->climate * PLANET_SIZE_COUNT + (int16_t)planet->size;
                    s_animation_header* anim = PLNTSUM::p_rotating_planet[anim_idx];

                    int16_t width = animate::Get_Width(anim);
                    int16_t height = animate::Get_Height(anim);

                    // Center the planet image within the designated list cell area (86x25)
                    int16_t x = (86 /* 0x56 */ - width) / 2 + 18 /* 0x12 */;
                    int16_t y = PLNTSUM::plntscrn_delta_y[i] + (25 /* 0x19 */ - height) / 2 + 45 /* 0x2d */;

                    animate::Draw(x, y, anim);
                }
            }
        }
    }

    void __cdecl Fill_Summary_Screen_Scroll_Bar(void) {
        int16_t y_base;
        uint8_t color = 150 /* 0x96 */;
        int16_t x = 423 /* 0x1a7 */;

        graphics::Set_Window(421 /* 0x1a5 */, 37 /* 0x25 */, 431 /* 0x1af */, 444 /* 0x1bc */);
        video::Clip_On();

        // Calculate visual offset for the scroll thumb based on scrollbar position
        y_base = MOX::scroll_bar->track_len_px - (MOX::scroll_bar->thumb_height_px / 2) - MOX::scroll_bar->thumb_center_y;

        for (int16_t i = 0; i < 10; ++i) {
            int16_t y1 = y_base + 38 /* 0x26 */;
            int16_t y2 = MOX::scroll_bar->thumb_height_px + y_base + 37 /* 0x25 */;

            line::Line(x, y1, x, y2, color);

            // Assembly implementation of parity check: alternates color decrement
            if ((i % 2) == 0) {
                color--;
            }
            x++;
        }

        video::Clip_Off();
        graphics::Reset_Window();

        // Redraw the scroll background frame or overlay
        animate::Draw(421 /* 0x1a5 */, 37 /* 0x25 */, PLNTSUM::screen->pics[12]);

        PLNTSUM::recalc_first_planet = false;
    }

    void __cdecl Load_Planet_Pictures() {
        for (int16_t i = 0; i < PLANET_PICTURE_COUNT; ++i) {
            s_animation_header* segment = Load_Planet_Segment(i);
            p_rotating_planet[i] = segment;
            RUSS::Mox_Update();
        }
    }

    void __cdecl Draw_Mini_Planet_Summary_Screen() {
        if (MOX::FULL_DRAW_SCREEN_FLAG != 0) {
            video::Set_Page_Back();
            graphics::Fill(0, 0, 639, 479, 0);
            animate::Draw(0, 0, PLNTSUM::screen->pics[0]);
            video::Copy_Back_To_Off();
            MOX::FULL_DRAW_SCREEN_FLAG = 0;
            video::Set_Page_Off();
        }

        graphics::Fill(0, 0, 639, 479, 0);
        animate::Draw(0, 0, PLNTSUM::screen->pics[0]);
        graphics::Reset_Window();
        video::Clip_Off();

        int16_t input_id = fields::Auto_Input();
        if ((int16_t)PLNTSUM::plntsum_scroll_bar == input_id) {
            PLNTSUM::recalc_first_planet = true;
        }

        PLNTSUM::Draw_Rotating_Planets();
        PLNTSUM::Wrapper_For_Planet_Summary_Galaxy_Map();
        SCROLL::Draw_Generic_Vertical_Scroll_Bar(MOX::scroll_bar, PLNTSUM::Fill_Summary_Screen_Scroll_Bar);
        PLNTSUM::Summary_Screen_Print_Data();

        if (MOX::scanned_field > -1) {
            PLNTSUM::Summary_Screen_Print_Scanned();
        }

        for (int16_t i = 0; i < (int16_t)PLNTSUM::n_ships_in_summary; ++i) {
            if (PLNTSUM::fade_flag != 0) {
                break;
            }

            int16_t ship_icon_id = PLNTSUM::ship_summary[i].ship_icon_id;
            int16_t node_idx = MOX::ship_icon[ship_icon_id].node_idx;
            int16_t ship_idx = MOX::ship_node[node_idx].ship_idx;
            s_ship_data* ship_ptr = &MOX::ship[ship_idx];

            if (ship_ptr->location >= SHIP_LOCATION_MOVING_OFFSET) {
                uint8_t type = ship_ptr->d.ship_type;
                if (type == SHIP_TYPE_COLONY || type == SHIP_TYPE_OUTPOST) {
                    PLNTSUM::Draw_Nearest_Ship_Directional_Line(ship_ptr->location);
                }
            }
        }

        animate::Set_Animation_Frame(PLNTSUM::send_colony_ship_button_seg, (int)PLNTSUM::draw_colony_button_status);
        animate::Draw(454, 386, PLNTSUM::send_colony_ship_button_seg);

        animate::Set_Animation_Frame(PLNTSUM::send_outpost_ship_button_seg, (int)PLNTSUM::draw_outpost_button_status);
        animate::Draw(454, 413, PLNTSUM::send_outpost_ship_button_seg);

        PLNTSUM::skip_draw_planets = 0;
    }

    void __cdecl Summary_Screen_Print_Scanned() {
        char msg_buffer[200];
        char suffix_buffer[100];
        char star_name_buffer[100];
        int8_t status = 0; // 0: neutral, 1: red, 2: green
        int16_t scanned_field = MOX::scanned_field;
        int16_t planet_idx = Summary_Planet_Index(scanned_field);

        if (PLNTSUM::planet_summary[planet_idx].blocked == 1) {
            snprintf(msg_buffer, sizeof(msg_buffer), HAROLD::H_Message(324 /* HESTR_144_BLACK_HOLE_BLOCKS_TRAV */), MOX::star[scanned_field].name);
            fonts::Set_Font_Style(1, &PLNTSUM::red_colors);
            int16_t y = (fonts::Get_Font_Height() / 2) + 143 /* 0x8F */;
            fonts::Print_Centered(533 /* 0x215 */, y, msg_buffer);
            return;
        }

        if (scanned_field < 1000 || scanned_field >= (MOX::NUM_STARS + 1000)) {
            if (PLNTSUM::send_choice != 0 && PLNTSUM::send_choice != 1) {
                return;
            }

            bool visited = HAROLD::Player_Has_Visited(MOX::PLAYER_NUM, scanned_field) || HAROLD::Player_Is_Omniscient(MOX::PLAYER_NUM);

            if (MOX::g_ship_move_info.moving != 0) {
                status = 2;
                if (visited) {
                    if (MOX::g_ship_move_info.turns_left == 1) {
                        snprintf(msg_buffer, sizeof(msg_buffer), HAROLD::H_Message(105 /* HESTR_069_D_TURN_TO_S */), (int)(uint8_t)MOX::g_ship_move_info.turns_left, MOX::star[scanned_field].name);
                    } else {
                        snprintf(msg_buffer, sizeof(msg_buffer), HAROLD::H_Message(106 /* HESTR_06A_D_TURNS_TO_S */), (int)(uint8_t)MOX::g_ship_move_info.turns_left, MOX::star[scanned_field].name);
                    }
                } else {
                    snprintf(msg_buffer, sizeof(msg_buffer), HAROLD::H_Message(332 /* HESTR_14C_ETA_D_TURNS */), (int)(uint8_t)MOX::g_ship_move_info.turns_left);
                }
            } else {
                status = 1;
                strlcpy(msg_buffer, HAROLD::H_Message(328 /* HESTR_148_OUT_OF_RANGE */), sizeof(msg_buffer));
            }
        } else {
            int16_t star_id = scanned_field - 1000;
            bool visited = HAROLD::Player_Has_Visited(MOX::PLAYER_NUM, star_id) || HAROLD::Player_Is_Omniscient(MOX::PLAYER_NUM);

            if (MOX::star[star_id].spectral_class == STAR_CLASS_BLACK_HOLE) {
                strlcpy(msg_buffer, HAROLD::H_Message(325 /* HESTR_145_BLACK_HOLE */), sizeof(msg_buffer));
                status = 1;
            } else if (!visited) {
                if (PLNTSUM::send_choice == 0 || PLNTSUM::send_choice == 1) {
                    if (PLNTSUM::can_reach_star[star_id] != 0) {
                        int16_t eta = Get_ETA_To_Star(star_id);
                        if (eta > 0) {
                            if (eta == 1) {
                                snprintf(msg_buffer, sizeof(msg_buffer), HAROLD::H_Message(331 /* HESTR_14B_ETA_D_TURN */), (int)eta);
                            } else {
                                snprintf(msg_buffer, sizeof(msg_buffer), HAROLD::H_Message(332 /* HESTR_14C_ETA_D_TURNS */), (int)eta);
                            }
                        }
                        status = 2;
                    } else {
                        status = 1;
                        strlcpy(msg_buffer, HAROLD::H_Message(327 /* HESTR_147_STAR_SYSTEM_UNREACHABL */), sizeof(msg_buffer));
                    }
                } else {
                    strlcpy(msg_buffer, HAROLD::H_Message(148 /* HESTR_094_UNEXPLORED */), sizeof(msg_buffer));
                    status = 0;
                }
            } else {
                strlcpy(star_name_buffer, MOX::star[star_id].name, sizeof(star_name_buffer));
                suffix_buffer[0] = '\0';
                if (PLNTSUM::send_choice == 0 || PLNTSUM::send_choice == 1) {
                    if (PLNTSUM::can_reach_star[star_id] == 0) {
                        strlcpy(suffix_buffer, HAROLD::H_Message(326 /* HESTR_146_CANNOT_REACH */), sizeof(suffix_buffer));
                        status = 1;
                    } else {
                        int16_t eta = Get_ETA_To_Star(star_id);
                        if (eta > 0) {
                            if (eta == 1) {
                                snprintf(suffix_buffer, sizeof(suffix_buffer), HAROLD::H_Message(336 /* HESTR_150_D_TURN_TO */), (int)eta);
                            } else {
                                snprintf(suffix_buffer, sizeof(suffix_buffer), HAROLD::H_Message(337 /* HESTR_151_D_TURNS_TO */), (int)eta);
                            }
                        }
                        status = 2;
                    }
                }
                snprintf(msg_buffer, sizeof(msg_buffer), "%s%s", suffix_buffer, star_name_buffer);
            }
        }

        s_colors* p_colors;
        if (status == 1) {
            p_colors = &PLNTSUM::red_colors;
        } else if (status == 2) {
            p_colors = &PLNTSUM::green_colors;
        } else {
            p_colors = &PLNTSUM::hi_neutral_colors;
        }

        fonts::Set_Font_Style(2, p_colors);
        int16_t y = ((fonts::Get_Font_Height() + 2) / 2) + 143 /* 0x8F */;
        fonts::Print_Centered(533 /* 0x215 */, y, msg_buffer);
    }

    void Planet_Summary_Screen() {
        int16_t exit_flag = 0;
        int16_t last_scanned_status = -1;
        char warning_msg[100];

        MAINSCR::Allocate_First_Screen_Seg(8);
        fade_flag = 1;
        fields::Set_Field_Outline_Color(215 /* 0xd7 */);
        EVANHELP::Set_PlntSum_Screen_Help_List();
        n_ships_in_summary = 0;
        send_choice = -1;
        COLONY::using_colony_screen_palette = 0;
        MOX::scanned_field = -1;
        planet_summary_refresh_marker = 0;
        last_scanned_status = -1;
        recalc_first_planet = false;
        MOX::multi_colored_line_start = 0;
        MOX::galaxy_star_anim_count = 0;
        last_ship_moved = -1;
        planet_summary_reserved_counter = 0;
        planet_summary_state_marker = 0;
        skip_draw_planets = 0;
        can_reach_scanned_planet = false;
        draw_colony_button_status = 0;
        DESBOX::incr_value = 1;
        draw_outpost_button_status = 0;

        video::Set_Refresh_Full();
        Check_For_Colony_And_Outpost_Ships();

        if (found_colony_ship != 0) {
            draw_colony_button_status = 1;
        }
        if (found_outpost_ship != 0) {
            draw_outpost_button_status = 1;
        }

        fields::Assign_Auto_Function(Draw_Mini_Planet_Summary_Screen, 1);
        fields::Clear_Fields();
        MISC::Fade_To_Black();
        fonts::Load_Palette(6, 0, 254 /* 0xfe */);

        Allocate_Space_For_Summary_Screen();
        Load_Planet_Summary_Pictures();

        planet_summary->scan_highlight_pulse = 0;
        planet_summary->scan_highlight_pulse_step = 1;

        Set_Planet_Explored_Flags();
        Filter_Explored_Planets(&MOX::scroll_bar->total_rows);

        MOX::scroll_bar->thumb_center_y = 407 /* 0x197 */;
        MOX::scroll_bar->visible_rows = 8;
        MOX::scroll_bar->track_len_px = 407 /* 0x197 */;
        MOX::scroll_bar->thumb_height_px = SCROLL::Get_Vertical_Scroll_Bar_Height(MOX::scroll_bar);
        MOX::scroll_bar->thumb_center_y = MOX::scroll_bar->track_len_px - (MOX::scroll_bar->thumb_height_px / 2);
        MOX::scroll_bar->last_thumb_center_y = -1;
        MOX::scroll_bar->type = 2;
        MOX::scroll_bar->row_step = 1;

        p_rotating_planet = (s_animation_header**)allocate::Allocate_Next_Block_Aligned(
            MOX::screen_seg, sizeof(*p_rotating_planet) * PLANET_PICTURE_COUNT, alignof(s_animation_header*));
        Load_Planet_Pictures();
        Determine_Button_Status();
        Add_Plntsum_Fields();

        do {
            int16_t prev_sort_choice = sort_choice;
            int16_t input_id = fields::Get_Input();
            int16_t scanned_id = fields::Scan_Input();
            screen->scanned_input = scanned_id;
            int16_t current_scanned = screen->scanned_input;

            if (screen->esc_button == input_id) {
                MOX::current_screen = SCREEN_MAIN;
                exit_flag = 1;
            }

            if (sort_choice != prev_sort_choice) {
                if (input_id == screen->buttons[0] || input_id == screen->buttons[1] || input_id == screen->buttons[2]) {
                    Sort_Planet_Summary();
                    MOX::scanned_field = -1;
                }
            }

            if (input_id == screen->buttons[3] || input_id == screen->buttons[4] || input_id == screen->buttons[5] ||
                input_id == screen->buttons[6] || input_id == screen->buttons[7]) {
                Filter_Explored_Planets(&MOX::scroll_bar->total_rows);
                last_scanned_status = -1;
                MOX::scroll_bar->first_visible_row = 0;
                if (MOX::scroll_bar->total_rows > 0) {
                    MOX::scroll_bar->thumb_center_y = MOX::scroll_bar->track_len_px;
                }

                Determine_Button_Status();
                screen->refresh_fields = true;
            }

            if (input_id == screen->up_button && MOX::scroll_bar->total_rows > 8) {
                SCROLL::Scroll_Up_One_Item(MOX::scroll_bar);
                last_scanned_status = -1;
            }

            if (input_id == screen->dn_button && MOX::scroll_bar->total_rows > 8) {
                SCROLL::Scroll_Down_One_Item(MOX::scroll_bar);
                last_scanned_status = -1;
            }

            if (input_id == plntsum_scroll_bar) {
                recalc_first_planet = true;
                last_scanned_status = -1;
            }

            uint8_t handled_star = 0;
            for (int16_t i = 0; i < MOX::NUM_STARS && !handled_star; ++i) {
                if (screen->planet_icons[i] == input_id && MOX::scroll_bar->total_rows > 8) {
                    int16_t row = First_Planet_In_Summary_List(i);
                    if (row <= -1) {
                        s_star_data* p_star = &MOX::star[i];
                        const char* msg_fmt = HAROLD::H_Message(0x14f /* HESTR_14F_NO_PLANETS_IN_THE_S_ST */);
                        snprintf(warning_msg, sizeof(warning_msg), msg_fmt, p_star);
                        GENDRAW::Warning_Box(warning_msg);
                    } else {
                        MOX::scroll_bar->first_visible_row = row;
                        MOX::scroll_bar->thumb_center_y = ((-407 / (int)MOX::scroll_bar->total_rows) - 1) * MOX::scroll_bar->first_visible_row + 407;
                    }
                    handled_star = 1;
                }
                if (screen->planet_icons[i] == current_scanned) {
                    MOX::scanned_field = i + 1000;
                    handled_star = 1;
                }
            }

            Scan_Summary_Box_Fields(current_scanned, input_id, &last_scanned_status);

            Handle_Send_Button(input_id);
            Refresh_Plntsum_Fields();

            if (exit_flag == 0) {
                if (fade_flag != 0) {
                    video::Set_Page_Off();
                    Draw_Mini_Planet_Summary_Screen();
                    video::Copy_Off_To_Back();
                    ERIC::Fast_Fade_In();
                    fade_flag = 0;
                } else {
                    video::Set_Page_Off();
                    Draw_Mini_Planet_Summary_Screen();
                    RUSS::Mox_Sync_Update();
                }
            }

        } while (exit_flag == 0);

        fields::Clear_Fields();
        fields::Disable_Draw_All_Fields();
        video::Set_Refresh_Stencil();
        mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);
        MISC::Fade_To_Black();
        fields::Enable_Draw_All_Fields();
        MOX::scroll_bar = nullptr;
        if (MOVEBOX::Moveable_Box_Selected(2) != 0 && last_ship_moved != -1) {
            MOX::first_node_ptr_in_stack_ = -1;
        }
        MAINSCR::Draw_Mini_Main_Screen();
        fields::Set_Field_Outline_Color(10);
    }

    s_animation_header* __cdecl Load_Planet_Segment(int16_t planet_anim_idx) {
        allocate::Get_Remaining_Block_Space(MOX::screen_seg);

        s_animation_header* segment = (s_animation_header*)farload::Far_Reload_Next(
            PTR_s_PLNTSUM_LBX_00556AC0,
            (int32_t)(int16_t)(planet_anim_idx + 0x1A),
            MOX::screen_seg);

        allocate::Get_Remaining_Block_Space(MOX::screen_seg);

        return segment;
    }

    void __cdecl Colony_Owner_Colors(int16_t colony_idx, s_colors* colors, int8_t highlight_type) {
        uint8_t glow_color;

        if (colony_idx != -1) {
            Player_Colors(MOX::colony[colony_idx].owner, colors, highlight_type, &glow_color);
        } else {
            Player_Colors(-1, colors, highlight_type, &glow_color);
        }
    }

    void __cdecl Player_Colors(int16_t player_idx, s_colors* colors_out, int8_t type, uint8_t* base_color_out) {
        if (player_idx == -1) {
            if (type == 1) {
                MOX::mox_font_colors_offset = 0x42;
                s_colors* c = MISC::Font_Colors2(4, 0x40, 0xFF);
                *colors_out = *c;
                *base_color_out = colors_out->colors[1];
            } else if (type == 2) {
                MOX::mox_font_colors_offset = 0x40;
                s_colors* c = MISC::Font_Colors2(4, 0x00, 0x8A);
                *colors_out = *c;
                *base_color_out = colors_out->colors[1];
            } else {
                MOX::mox_font_colors_offset = 0x43;
                s_colors* c = MISC::Font_Colors2(4, 0x40, 0x41);
                *colors_out = *c;
                *base_color_out = colors_out->colors[1];
            }
            return;
        }

        uint8_t base_color = static_cast<uint8_t>(MOX::player[player_idx].color * 6) - 0x31;
        *base_color_out = base_color;

        if (type == 0) {
            MOX::mox_font_colors_offset = base_color + 2;
            s_colors* c = MISC::Font_Colors2(4, base_color, base_color + 4);
            *colors_out = *c;
        } else if (type == 1) {
            MOX::mox_font_colors_offset = base_color + 3;
            s_colors* c = MISC::Font_Colors2(4, base_color, base_color + 5);
            *colors_out = *c;
        } else if (type == 2) {
            MOX::mox_font_colors_offset = base_color + 1;
            s_colors* c = MISC::Font_Colors2(4, base_color, base_color + 3);
            *colors_out = *c;
        }
    }

} // namespace PLNTSUM