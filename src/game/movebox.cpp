#include "pch.h"
#include "game/game_events.h"

namespace MOVEBOX {
    void __cdecl Draw_Galaxy_Map_Box(s_animation_header* anim1, s_animation_header* unused_anim2, int16_t x_base, int16_t y_base, int16_t width, int16_t height, int16_t map_x_off, int16_t map_y_off, int16_t x_adj, int16_t y_adj, int16_t view_mode, int16_t exit_flag) {
        (void)unused_anim2;

        s_colors font_colors;
        for (int32_t i = 2; i < 8; ++i) {
            font_colors.colors[i] = (uint8_t)(i + 100);
        }

        int16_t y_start = y_base + y_adj;
        int16_t x_start = x_base + x_adj;

        int32_t scale_x = 506000 / width;
        int32_t scale_y = 400000 / height;

        if (exit_flag == 1) return;

        if (exit_flag > -1) {
            int16_t box_w = width;
            int16_t box_h = height;
            if (COLONY::using_colony_screen_palette == 0) {
                box_w = MOX::moveable_box[1].width;
                box_h = MOX::moveable_box[1].height;
            }

            if (view_mode != 4) {
                if (anim1 == nullptr || COLONY::using_colony_screen_palette != 0) {
                    graphics::Set_Window(x_start, y_start, x_start + box_w, y_start + box_h);
                    video::Clip_On();
                    if (MOX::current_screen != SCREEN_RACE && COLONY::using_colony_screen_palette == 0) {
                        graphics::Fill(x_start, y_start, x_start + box_w, y_start + box_h, 0);
                    }
                    video::Clip_Off();
                    graphics::Reset_Window();
                } else {
                    for (int32_t i = 0; i < 8; ++i) {
                        font_colors.colors[i] = (i == 0) ? 0x62 : (uint8_t)(i + 100);
                    }
                    if (MOX::current_screen == SCREEN_MAIN) {
                        fonts::Set_Font_Spacing(2);
                        fonts::Set_Font_Style_Shadow_Down(3, &font_colors);
                        fonts::Set_Outline_Color(0x62, 0x65);
                        const char* msg = HAROLD::H_Message(0xF3);
                        fonts::Print_Centered(x_start + 87, y_start + 13, msg);
                        MISC::Set_Font_Default_Spacing();
                    }
                }
            }
        }

        graphics::Set_Window(map_x_off + x_start, map_y_off + y_start, map_x_off + width + x_start, map_y_off + height + y_start);
        video::Clip_On();

        if (MOX::NUM_STARS > 0) {
            for (int16_t i = 0; i < MOX::NUM_STARS; ++i) {
                int32_t sx = x_start + map_x_off + ((MOX::star[i].x * 1000 / MOX::max_map_scale) * 10) / scale_x;
                int32_t sy = y_start + map_y_off + ((MOX::star[i].y * 1000 / MOX::max_map_scale) * 10) / scale_y;

                int8_t owner = MOX::star[i].owner;
                const bool has_visible_owner = owner >= 0 && owner < MAX_PLAYERS;
                const uint8_t owner_color = has_visible_owner
                    ? static_cast<uint8_t>(MOX::player[owner].color)
                    : 0;
                const uint16_t color_idx = MAP_SCALE::Overview_Star_Color_Index_(
                    MOX::star[i].spectral_class == STAR_CLASS_BLACK_HOLE,
                    COLONY::using_colony_screen_palette != 0,
                    owner,
                    MAX_PLAYERS,
                    owner_color);

                if (view_mode == 0) {
                    animate::Draw(sx - 1, sy - 1, MOX::star_colors_seg[color_idx]);
                } else if (view_mode == 1 || view_mode == 4) {
                    animate::Reset_Animation_Frame(MOX::fleet_galaxy_star_seg[color_idx]);
                    animate::Draw(sx - 2, sy - 2, MOX::fleet_galaxy_star_seg[color_idx]);
                } else if (view_mode == 2) {
                    int16_t scanned = MOX::scanned_field;
                    if (scanned >= 1000) scanned -= 1000;
                    s_animation_header* summ_anim = MOX::planet_summary_galaxy_star_seg[color_idx];
                    if (i == scanned) {
                        animate::Set_Animation_Frame(summ_anim, (int)MOX::galaxy_star_anim_count);
                        int32_t count = animate::Get_Animation_Frame_Count(summ_anim);
                        MOX::galaxy_star_anim_count = (uint8_t)((MOX::galaxy_star_anim_count + 1) % count);
                    } else {
                        animate::Reset_Animation_Frame(summ_anim);
                    }
                    animate::Draw(sx - 2, sy - 2, summ_anim);
                } else if (view_mode == 3) {
                    if (MOX::galaxy_map_scanned_star != i) {
                        animate::Reset_Animation_Frame(MOX::colony_galaxy_star_seg[color_idx]);
                    }
                    animate::Draw(sx - 1, sy - 1, MOX::colony_galaxy_star_seg[color_idx]);
                }
            }
        }

        video::Clip_Off();
        graphics::Reset_Window();

        if (MOX::current_screen == SCREEN_FLEET) {
            if (MOX::galaxy_map_scanned_star > -1) {
                Do_Fleet_Screen_Stuff(map_x_off, map_y_off, scale_x, scale_y);
            }
            return;
        }

        if (MOX::current_screen == SCREEN_OFFICERS) {
            Do_Officer_Screen_Stuff(map_x_off, map_y_off, scale_x, scale_y);
        }
    }

    int16_t __cdecl Moveable_Box_Selected(int16_t box_index) {
        return (int16_t)(MOX::box_off_flag[box_index] < MOX::moveable_box[box_index].id);
    }

    void __cdecl Vanish_Fleet_Movement_Box() {
        game_events::Publish(game_events::FLEET_BOX_INFO_CLEARED);
        MOX::moveable_box[2].id = MOX::box_off_flag[2];

        if (MOX::fleet_box.count > 0) {
            for (int16_t i = 0; i < MOX::fleet_box.count; ++i) {
                MOX::fleet_movement_icon[i].field = -1000;
            }
        }

        MOX::fleet_movement_all_button = -1000;
        MOX::fleet_box_colonize_button = -1000;
        MOX::moveable_box[2].drag_field = -1000;
        MOX::moveable_box[2].close_btn = -1000;

        MOX::mouse_XY.ship_icon_idx = -1;
        MOX::eta_star_id = -1;
        MOX::fleet_ship_scanned = -1;

        HACCESS::Clear_Fleet_Box_Selected_Ship_Status();
        EVANHELP::Set_Main_Screen_Help_List();

        MOX::reloaded_ship_palette = 0;
    }

    void __cdecl Vanish_System_Display_Box() {
        MOX::moveable_box[0].id = MOX::box_off_flag[0];

        for (int32_t i = 0; i < 5; ++i) {
            MOX::planet_buttons[i] = -1000;
        }

        for (int32_t i = 0; i < 8; ++i) {
            MOX::system_display_ships[i].button = -1000;
        }

        MOX::moveable_box[0].drag_field = -1000;
        MOX::moveable_box[0].close_btn = -1000;
    }

    bool __cdecl Mouse_Within_Fleet_Box() {
        bool within = false;
        int16_t px = mouse::Pointer_X();
        int16_t py = mouse::Pointer_Y();

        const s_movable_box& box = MOX::moveable_box[2];

        int32_t bottom_val = (int32_t)box.height + (int32_t)box.y;
        if (bottom_val >= 0x1A5) {
            bottom_val = 0x1A5;
        }

        int16_t bottom = (int16_t)bottom_val;
        int16_t right = (int16_t)(box.width + box.x);

        if (px >= box.x && px <= right && py >= box.y && py <= bottom) {
            within = true;
        }

        return within;
    }

    void __cdecl Box_Fleet_Screen_Scanned_Star() {
        s_star_data* star = &MOX::star[MOX::galaxy_map_scanned_star];

        int32_t x_scaled = (star->x * 1000) / (int32_t)MOX::max_map_scale;
        int16_t x = (int16_t)(((x_scaled * 10) / 1659) + 15) - 5;

        int32_t y_scaled = (star->y * 1000) / (int32_t)MOX::max_map_scale;
        int16_t y = (int16_t)((y_scaled * 10) / 2197) + 47;

        graphics::Box(x, y, 11, 12, 1, 113);
    }

    void __cdecl Box_Fleet_Screen_Scanned_Ship() {
        int16_t ship_idx = MOX::scanned_small_ship;
        auto& icon = MOX::ship_icon[ship_idx];

        graphics::Box(
            icon.x + 4,
            icon.y + 2,
            11,
            11,
            1,
            141);
    }

    void __cdecl Do_Fleet_Screen_Stuff(int16_t map_x_off, int16_t map_y_off, int32_t scale_x, int32_t scale_y) {
        if (MOX::galaxy_map_scanned_star > -1) {
            MOVEBOX::Box_Fleet_Screen_Scanned_Star();
        }

        if (MOX::scanned_small_ship > -1) {
            int16_t stack_id = HACCESS::Ship_Stack_From_Ship_Icon(MOX::scanned_small_ship);
            if (stack_id > -1 && stack_id != MOX::small_ship_stack_ptr) {
                MOVEBOX::Box_Fleet_Screen_Scanned_Ship();
            }
        }
    }

    void __cdecl Do_Officer_Screen_Stuff(int16_t map_x_off, int16_t map_y_off, int16_t scale_x, int16_t scale_y) {
        bool is_fleet_officer = OFFICER::Is_Fleet_Officer_Screen();
        int16_t scanned_star_idx = MOX::galaxy_map_scanned_star;

        if (scanned_star_idx >= 0) {
            s_star_data& star = MOX::star[scanned_star_idx];
            int8_t owner = star.owner;

            uint8_t color1, color2;
            if (owner < 0 || owner >= MAX_PLAYERS) {
                color1 = 33;
                color2 = 38;
            } else {
                uint8_t player_color_idx = MOX::player[owner].color;
                uint8_t palette_color = MOX::main_palette_player_colors[player_color_idx];
                if (palette_color == 32) {
                    color1 = 8;
                    color2 = 22;
                } else {
                    color1 = palette_color + 1;
                    color2 = palette_color + 5;
                }
            }

            s_colors font_colors;
            MISC::Get_Mox_Font_Colors(3, color1, color2, &font_colors);
            MOVEBOX::Box_Officer_Screen_Scanned_Star();
            fonts::Set_Font_Style(3, &font_colors);

            char buffer[40];
            bool can_see_info = HAROLD::Player_Has_Visited(MOX::PLAYER_NUM, scanned_star_idx) ||
                HAROLD::Player_Is_Omniscient(MOX::PLAYER_NUM) ||
                HAROLD::Contact_With_One_Colony(scanned_star_idx);

            if (can_see_info) {
                int8_t off_idx = star.officer_index[MOX::PLAYER_NUM];
                bool has_colony = HAROLD::Player_Has_Colony(MOX::PLAYER_NUM, scanned_star_idx);

                if (off_idx == -1 || !has_colony) {
                    strlcpy(buffer, star.name, sizeof(buffer));
                } else {
                    s_leader_data& leader = MOX::leaders[off_idx];
                    int8_t eta = leader.eta;

                    if (eta <= 0) {
                        snprintf(buffer, sizeof(buffer), MOX::s_fmt_scanned_star_info, star.name, leader.name);
                    } else {
                        const char* fmt;
                        if (eta == 1) {
                            fmt = HAROLD::H_Message(146);
                        } else {
                            fmt = HAROLD::H_Message(147);
                        }
                        snprintf(buffer, sizeof(buffer), fmt, star.name, leader.name, eta);
                    }
                }
            } else {
                strlcpy(buffer, HAROLD::H_Message(148), sizeof(buffer));
            }

            MISC::Set_Fitted_Font_Style(buffer, &font_colors, 3, 2, 0, 203, 1);
            fonts::Print_Centered(466, 418, buffer);
        }

        if (!is_fleet_officer && OFFICER::officer_star_displayed >= 0) {
            MOVEBOX::Box_Officer_Screen_Chosen_Star(map_x_off, map_y_off, scale_x, scale_y);
        }
    }

    void __cdecl Move_Box_To_Top(int16_t box_id) {
        int16_t reordered_box_ids[5] = {0, 0, 0, 0, 0};
        const int16_t reordered_box_count =
            (int16_t)(sizeof(reordered_box_ids) / sizeof(reordered_box_ids[0]));
        int16_t found_idx = -1;

        for (int16_t i = 0; i < reordered_box_count; ++i) {
            if (MOX::moveable_box_order[i] == box_id) {
                found_idx = i;
                break;
            }
        }

        int16_t target_box_id;
        if (found_idx >= 0) {
            target_box_id = MOX::moveable_box_order[found_idx];
            MOX::moveable_box_order[found_idx] = -1;
        } else {
            target_box_id = MOX::moveable_box_reorder_fallback;
            MOX::moveable_box_reorder_fallback = -1;
        }

        reordered_box_ids[0] = target_box_id;

        for (int16_t i = 1; i < reordered_box_count; ++i) {
            for (int16_t j = 0; j < reordered_box_count; ++j) {
                if (MOX::moveable_box_order[j] != -1) {
                    reordered_box_ids[i] = MOX::moveable_box_order[j];
                    MOX::moveable_box_order[j] = -1;
                    break;
                }
            }
        }

        for (int16_t i = 0; i < reordered_box_count; ++i) {
            MOX::moveable_box_order[i] = reordered_box_ids[i];
        }
    }

    int16_t __cdecl N_Moveable_Boxes_Selected(int16_t excluded_box_idx) {
        int16_t box_index = 0;
        int16_t selected_count = 0;

        for (; box_index < 5; ++box_index) {
            if (box_index != excluded_box_idx) {
                if (MOVEBOX::Moveable_Box_Selected(box_index) != 0) {
                    ++selected_count;
                }
            }
        }

        return selected_count;
    }

    void __cdecl Remove_Box_Check() {
    }

    void __cdecl Get_Galaxy_Map_Star_XY(int16_t star_idx,
        int16_t base_x,
        int16_t base_y,
        int16_t map_width,
        int16_t map_height,
        int16_t offset_x,
        int16_t offset_y,
        int16_t star_marker_mode,
        int16_t* out_x,
        int16_t* out_y) {
        const int32_t x_divisor = 506000 / static_cast<int32_t>(map_width) + 1;
        const int32_t y_divisor = 400000 / static_cast<int32_t>(map_height) + 1;
        const int32_t map_scale = static_cast<int32_t>(MOX::max_map_scale);

        s_star_data* const star = &MOX::star[star_idx];

        const int32_t scaled_x = (static_cast<int32_t>(star->x) * 1000) / map_scale;
        const int32_t scaled_y = (static_cast<int32_t>(star->y) * 1000) / map_scale;

        const int16_t star_center_offset = (star_marker_mode == 0) ? 2 : 3;

        int16_t screen_x = static_cast<int16_t>((scaled_x * 10) / x_divisor);
        screen_x = static_cast<int16_t>(screen_x - star_center_offset);
        screen_x = static_cast<int16_t>(screen_x + base_x);
        screen_x = static_cast<int16_t>(screen_x + offset_x);
        *out_x = screen_x;

        int16_t screen_y = static_cast<int16_t>((scaled_y * 10) / y_divisor);
        screen_y = static_cast<int16_t>(screen_y - star_center_offset);
        screen_y = static_cast<int16_t>(screen_y + base_y);
        screen_y = static_cast<int16_t>(screen_y + offset_y);
        *out_y = screen_y;
    }

    void __cdecl Get_Galaxy_Map_Space_XY(int16_t ship_icon_idx,
        int16_t origin_x,
        int16_t origin_y,
        int16_t x_zoom,
        int16_t y_zoom,
        int16_t* out_x,
        int16_t* out_y) {
        const s_ship_icon& icon = MOX::ship_icon[ship_icon_idx];

        const int32_t scaled_y =
            ((static_cast<int32_t>(icon.y) - 21) *
                ((static_cast<int32_t>(y_zoom) * 1000) / 400 + 1)) /
            1000;

        const int32_t scaled_x =
            ((static_cast<int32_t>(icon.x) - 21) *
                ((static_cast<int32_t>(x_zoom) * 1000) / 506 + 1)) /
            1000;

        *out_x = static_cast<int16_t>(origin_x + scaled_x);
        *out_y = static_cast<int16_t>(origin_y + scaled_y);
    }

    void __cdecl Box_Officer_Screen_Chosen_Star(int16_t left, int16_t top, int16_t x_divisor, int16_t y_divisor) {
        const int16_t displayed_star_index = OFFICER::officer_star_displayed;
        const s_star_data& star = MOX::star[displayed_star_index];

        const int32_t box_x =
            static_cast<int32_t>(left) +
            0x132 +
            (((static_cast<int32_t>(star.x) * 1000) / static_cast<int32_t>(MOX::max_map_scale)) * 10) /
                static_cast<int32_t>(x_divisor);

        const int32_t box_y =
            static_cast<int32_t>(top) +
            0xEB +
            (((static_cast<int32_t>(star.y) * 1000) / static_cast<int32_t>(MOX::max_map_scale)) * 10) /
                static_cast<int32_t>(y_divisor);

        if (displayed_star_index == OFFICER::officer_star_chosen) {
            graphics::Box((int16_t)(box_x - 5), (int16_t)(box_y - 5), 0x0B, 0x0C, 1, (uint8_t)(OFFICER::offscrn_flash_value + 0x6E));
            return;
        }

        graphics::Box((int16_t)(box_x - 5), (int16_t)(box_y - 5), 0x0B, 0x0C, 1, 0x1E);
    }

    void __cdecl Box_Officer_Screen_Scanned_Star() {
        const int16_t scanned_star_index = MOX::galaxy_map_scanned_star;
        s_star_data* const scanned_star = &MOX::star[scanned_star_index];
        const int32_t map_scale = static_cast<int32_t>(MOX::max_map_scale);

        const int32_t scaled_x = ((((int32_t)scanned_star->x * 1000) / map_scale) * 10) / 0x637;
        const int32_t scaled_y = ((((int32_t)scanned_star->y * 1000) / map_scale) * 10) / 0x93e;

        const int16_t box_x = (int16_t)((uint16_t)(scaled_x + 0x132) - 5);
        const int16_t box_y = (int16_t)((uint16_t)scaled_y + 0xE6);

        graphics::Box(box_x, box_y, 0x0B, 0x0C, 1, 0x71);
    }

    void __cdecl Add_Galaxy_Map_Fields_2(
        int16_t map_x,
        int16_t map_y,
        int16_t map_width,
        int16_t map_height,
        int16_t offset_x,
        int16_t offset_y,
        uint32_t unused_arg7,
        uint32_t unused_arg8,
        int16_t field_style,
        int16_t* out_special_field_id,
        int16_t* star_field_ids,
        int16_t* out_grid_field_id) {
        (void)unused_arg7;
        (void)unused_arg8;

        const int16_t window_top = static_cast<int16_t>(map_y + offset_y);
        const int16_t window_left = static_cast<int16_t>(map_x + offset_x);

        graphics::Set_Window(
            window_left,
            window_top,
            static_cast<int16_t>(offset_x + map_width + map_x),
            static_cast<int16_t>(offset_y + map_height + map_y));

        if (MOX::current_screen == SCREEN_COLONY) {
            *out_special_field_id = static_cast<int16_t>(0xFC18);
        }

        if (MOX::NUM_STARS > 0) {
            for (int16_t star_index = 0; star_index < MOX::NUM_STARS; ++star_index) {
                int16_t star_y;
                int16_t star_x;

                MOVEBOX::Get_Galaxy_Map_Star_XY(
                    star_index,
                    map_x,
                    map_y,
                    map_width,
                    map_height,
                    offset_x,
                    offset_y,
                    field_style,
                    &star_x,
                    &star_y);

                int16_t field_id;
                if (field_style == 0) {
                    field_id = fields::Add_Hidden_Field(
                        star_x,
                        star_y,
                        static_cast<int16_t>(star_x + 5),
                        static_cast<int16_t>(star_y + 5),
                        MOX::s__00551100,
                        0x29);
                } else {
                    field_id = fields::Add_Hidden_Field(
                        static_cast<int16_t>(star_x - 3),
                        static_cast<int16_t>(star_y - 3),
                        static_cast<int16_t>(star_x + 8),
                        static_cast<int16_t>(star_y + 9),
                        MOX::s__00551100,
                        0x29);
                }

                star_field_ids[star_index] = field_id;
            }
        }

        if (MOX::current_screen == SCREEN_COLONY) {
            *out_grid_field_id = fields::Add_Grid_Field(
                window_left,
                window_top,
                1,
                1,
                map_width,
                map_height,
                &MOX::galaxy_map_grid_y,
                &MOX::galaxy_map_grid_x,
                0);
        }

        graphics::Reset_Window();
    }

    void __cdecl Draw_Moveable_Box(int16_t box_id) {
        const int32_t box_index = box_id;
        s_movable_box& box = MOX::moveable_box[box_index];

        if (static_cast<int16_t>(MOX::box_off_flag[box_index]) >= box.id) {
            return;
        }

        if (SYS::skip_draw_system_display != 0) {
            SYS::skip_draw_system_display = 0;
            return;
        }

        if (box.index == 0) {
            SYS::Wrapper_For_Normal_System_Popup();
            return;
        }

        if (box.index == 1) {
            const int32_t pict_index = box_index * 5;

            MOVEBOX::Draw_Galaxy_Map_Box(
                MOX::moveable_box_pict[pict_index],
                MOX::moveable_box_pict[pict_index + 1],
                box.x,
                box.y,
                MOX::galaxy_map_height,
                MOX::galaxy_map_width,
                8,
                0x26,
                box.content_offset_x,
                box.content_offset_y,
                0,
                0);
            return;
        }

        if (box.index != 2) {
            return;
        }

        FLEETPOP::Draw_Fleet_Movement_Box();
    }

    int8_t __cdecl Box_Was_Open(int16_t box_index) {
        return MOX::saved_boxes[box_index].is_open;
    }

    void __cdecl Restore_Boxes() {
        for (int16_t box_index = 0; box_index < 5; ++box_index) {
            if (MOX::saved_boxes[box_index].field_id != -1000) {
                const int8_t was_open = MOX::saved_boxes[box_index].is_open;

                MOX::moveable_box[box_index].id = MOX::saved_boxes[box_index].field_id;

                if (was_open != 0) {
                    if (box_index == 0) {
                        SYS::Init_System_Display(MOX::moveable_box[0].id);
                    } else if (box_index == 2) {
                        if (HACCESS::Get_Stack_Start_From_Saved_Ship_Array() == 0) {
                            MOVEBOX::Vanish_Fleet_Movement_Box();
                        }
                    }
                }

                MOX::saved_boxes[box_index].is_open = 0;
                MOX::saved_boxes[box_index].field_id = -1000;
            }
        }
    }

    void __cdecl Save_And_Remove_Boxes() {
        for (int16_t box_idx = 0; box_idx < 5; ++box_idx) {
            const int16_t is_open = (int16_t)MOVEBOX::Moveable_Box_Selected(box_idx);

            MOX::saved_boxes[box_idx].field_id = MOX::moveable_box[box_idx].id;
            MOX::saved_boxes[box_idx].is_open = static_cast<int8_t>(is_open);

            if (box_idx == 0 && is_open != 0) {
                MOVEBOX::Vanish_System_Display_Box();
            }

            if (box_idx == 2 && is_open != 0) {
                MOVEBOX::Vanish_Fleet_Movement_Box();
            }

            MOX::mouse_XY.star_idx = -1;
            MOX::moveable_box[box_idx].id =
                static_cast<int16_t>(MOX::box_off_flag[MOX::moveable_box[box_idx].index]);
        }
    }

    void __cdecl Clear_Moveable_Box_Fields(int16_t box_index) {
        MOX::moveable_box[box_index].close_btn = -1000;
        MOX::moveable_box[box_index].drag_field = -1000;
        MOX::moveable_box[box_index].window_field_id = -1000;

        if (box_index == 0) {
            for (int16_t i = 0; i < 5; ++i) {
                MOX::planet_buttons[i] = -1000;
            }
            return;
        }

        if (box_index == 1) {
            MOX::galaxy_map_grid_field = -1000;

            for (int16_t i = 0; i < MOX::NUM_STARS; ++i) {
                MOX::galaxy_map_star_field[i] = -1000;
            }
            return;
        }

        if (box_index == 2) {
            MOX::fleet_box_scroll_up_button = -1000;
            MOX::fleet_box_scroll_dn_button = -1000;
            MOX::fleet_movement_bar_field = -1000;
            MOX::moveable_box[2].close_btn = -1000;
            MOX::moveable_box[2].drag_field = -1000;

            if (MOVEBOX::Moveable_Box_Selected(2) != 0) {
                for (int16_t i = 0; i < MOX::fleet_box.count; ++i) {
                    MOX::fleet_movement_icon[i].field = -1000;
                }
            }
        }
    }

    uint16_t __cdecl Box_Field_Selected(int16_t box_index, int16_t field_id) {
        if ((MOX::moveable_box[box_index].drag_field == field_id) ||
            (MOX::moveable_box[box_index].window_field_id == field_id)) {
            return 1;
        }

        if ((MOX::moveable_box[1].index == box_index) &&
            (field_id == MOX::galaxy_map_grid_field)) {
            return 1;
        }

        if ((MOX::moveable_box[2].index == box_index) &&
            ((field_id == MOX::fleet_movement_bar_field) ||
                (field_id == MOX::fleet_movement_all_button))) {
            return 1;
        }

        return 0;
    }

    void __cdecl Move_Moveable_Box(int16_t moveable_box_idx, int16_t mouse_button_state) {
        int16_t special_clamp_height = 0;
        int16_t box_height;
        int16_t box_width;

        if (moveable_box_idx == 2) {
            special_clamp_height = 0x1c;

            if (MOX::mouse_button_pressed == 2) {
                MOX::fleet_box.mouse_pressed = 1;
            } else {
                MOX::fleet_box.mouse_pressed = 0;
            }
        } else if (moveable_box_idx == 0) {
            special_clamp_height = 0x2b;
        }

        if (moveable_box_idx == 2) {
            box_width = MOX::fleet_box.width;
            box_height = MOX::fleet_box.height;

            if (MOX::fleet_box.colonize_btn_active != 0) {
                MOX::fleet_box.colonize_btn_active = 2;
            }
        } else {
            s_movable_box& movable_box = MOX::moveable_box[moveable_box_idx];

            if (MOVEBOX::Moveable_Box_Selected(moveable_box_idx) != 0) {
                box_width = movable_box.width;
                box_height = movable_box.height;
            } else {
                box_width = movable_box.unselected_width;
                box_height = movable_box.unselected_height;
            }
        }

        if (mouse_button_state != 0) {
            s_movable_box& movable_box = MOX::moveable_box[moveable_box_idx];

            if (fields::Auto_Input() == movable_box.drag_field || movable_box.active == 1) {
                fields::Deactivate_Help_List();
                MOX::mouse_button_pressed = static_cast<int8_t>(mouse_button_state);
                fields::Disable_Mouse_Pressed_Field_Update();
                fields::Disable_Draw_All_Fields();
                MOX::skip_fields = 1;

                if (MOX::stencil_box_id == -1) {
                    MOX::stencil_box_id = moveable_box_idx;
                    MOX::draw_main_screen_stencilled = 1;
                }

                fields::Disable_Draw_All_Fields();

                movable_box.content_offset_x =
                    static_cast<int16_t>(mouse::Pointer_X() - fields::Pressed_Mouse_X());
                movable_box.content_offset_y =
                    static_cast<int16_t>(mouse::Pointer_Y() - fields::Pressed_Mouse_Y());

                if (static_cast<int32_t>(movable_box.content_offset_x) + movable_box.x < 0x16) {
                    movable_box.content_offset_x = static_cast<int16_t>(0x16 - movable_box.x);
                }

                if (static_cast<int32_t>(movable_box.content_offset_x) + movable_box.x >
                    0x20e - box_width) {
                    movable_box.content_offset_x =
                        static_cast<int16_t>(0x20f - movable_box.x - box_width);
                }

                if (static_cast<int32_t>(movable_box.content_offset_y) + movable_box.y < 0x16) {
                    movable_box.content_offset_y = static_cast<int16_t>(0x16 - movable_box.y);
                }

                if (mouse_button_state == 2 && moveable_box_idx == 0) {
                    if (static_cast<int32_t>(movable_box.content_offset_y) + special_clamp_height +
                            movable_box.y >
                        0x1a4) {
                        movable_box.content_offset_y =
                            static_cast<int16_t>(0x1a5 - special_clamp_height - movable_box.y);
                    }
                } else {
                    if (static_cast<int32_t>(movable_box.content_offset_y) + movable_box.y >
                        0x1a4 - box_height) {
                        movable_box.content_offset_y =
                            static_cast<int16_t>(0x1a5 - movable_box.y - box_height);
                    }
                }

                movable_box.active = 1;
                return;
            }
        }

        MOX::draw_main_screen_to_back = 1;
        fields::Enable_Draw_All_Fields();
        EVANHELP::Set_Main_Screen_Help_List();
        fields::Clear_Fields();
        MOVEBOX::Clear_Moveable_Box_Fields(moveable_box_idx);
        MOX::skip_fields = 1;

        {
            s_movable_box& movable_box = MOX::moveable_box[moveable_box_idx];

            if (movable_box.active == 1) {
                movable_box.x = static_cast<int16_t>(
                    movable_box.x + (mouse::Pointer_X() - fields::Pressed_Mouse_X()));

                const int16_t pointer_y = mouse::Pointer_Y();
                const int16_t pressed_mouse_y = fields::Pressed_Mouse_Y();
                MOX::stencil_box_id = -2;
                movable_box.y =
                    static_cast<int16_t>(movable_box.y + (pointer_y - pressed_mouse_y));
            }

            if (movable_box.x < 0x16) {
                movable_box.x = 0x16;
            }

            if (movable_box.x > 0x20e - box_width) {
                movable_box.x = static_cast<int16_t>(0x20f - box_width);
            }

            if (movable_box.y < 0x16) {
                movable_box.y = 0x16;
            }

            if (MOX::mouse_button_pressed == 2 && moveable_box_idx == 0) {
                if (movable_box.y > 0x1a4 - special_clamp_height) {
                    movable_box.y = static_cast<int16_t>(0x1a5 - special_clamp_height);
                }
            } else {
                if (movable_box.y > 0x1a4 - box_height) {
                    movable_box.y = static_cast<int16_t>(0x1a5 - box_height);
                }
            }

            movable_box.content_offset_x = 0;
            movable_box.content_offset_y = 0;
            movable_box.active = 0;
        }

        MOVEBOX::Move_Box_To_Top(moveable_box_idx);

        if (moveable_box_idx == 2) {
            MOX::fleet_box.x = MOX::moveable_box[2].x;
            MOX::fleet_box.y = MOX::moveable_box[2].y;
        }

        MOX::draw_main_screen_to_back = 1;
        EVANHELP::Set_Main_Screen_Help_List();
    }

    void __cdecl Moveable_Box_Dimensions(
        int16_t box_index,
        int16_t* left,
        int16_t* top,
        int16_t* right,
        int16_t* bottom) {
        *left = MOX::moveable_box[box_index].x;
        *top = MOX::moveable_box[box_index].y;

        if (box_index == 2) {
            *right = *left + MOX::fleet_box.width;
            *bottom = *top + MOX::fleet_box.field8_0xe;
            return;
        }

        *right = MOX::moveable_box[box_index].width + *left;
        *bottom = MOX::moveable_box[box_index].height + *top;
    }

    void __cdecl Add_Whole_Window_Field(int16_t moveable_box_index) {
        int16_t height;
        int16_t width;
        int16_t y;
        int16_t x;

        Moveable_Box_Dimensions(moveable_box_index, &x, &y, &width, &height);
        MOX::moveable_box[moveable_box_index].window_field_id =
            fields::Add_Hidden_Field(x, y, width, height, MOX::s__00551100, 0);
    }
} // namespace MOVEBOX