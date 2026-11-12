#include "pch.h"

namespace FLT1 {

    int16_t fleet_screen_bad_ship_id;
    int16_t combat_filter_button;
    uint8_t relocate_button_mode;
    int16_t support_filter_button;
    void* multi_button_support_seg;
    uint8_t merging_relocations;
    void* multi_button_combat_seg;
    s_mouse_list mouse_list_from[4] = {
        {2, 0, 0, 0, 639, 479},
        {17, 0, 15, 52, 319, 233},
        {17, 0, 447, 383, 517, 405},
        {1, 0, 345, 428, 625, 458},
    };
    s_mouse_list mouse_list_to[4] = {
        {2, 0, 0, 0, 639, 479},
        {18, 0, 15, 52, 319, 233},
        {1, 0, 447, 383, 517, 405},
        {1, 0, 345, 428, 625, 458},
    };
    s_mouse_list mouse_list_all_to[4] = {
        {2, 0, 0, 0, 639, 479},
        {16, 0, 15, 52, 319, 233},
        {1, 0, 447, 383, 517, 405},
        {1, 0, 345, 428, 625, 458},
    };
    void __cdecl Initialize_Scroll_Bar(s_scroll_bar* scroll_bar) {
        int16_t scroll_height;

        scroll_bar->visible_rows = 5;
        scroll_bar->track_len_px = 0xea /* 234 */;

        scroll_height = SCROLL::Get_Vertical_Scroll_Bar_Height(scroll_bar);
        scroll_bar->thumb_height_px = scroll_height;

        scroll_bar->last_thumb_center_y = -1;
        scroll_bar->type = 0;
        scroll_bar->row_step = 1;

        scroll_bar->thumb_center_y = scroll_bar->track_len_px - (scroll_height / 2);
    }

    void __cdecl Draw_Fltscrn_Big_Ship_Icons(int16_t screen_id) {
        int16_t damage_width;
        int16_t damage_y_off;
        int16_t damage_x_off;

        if (screen_id == 0x1d) {
            damage_width = 3;
            damage_y_off = 0x39;
            damage_x_off = 0x39;
        } else {
            damage_x_off = 0x38;
            damage_y_off = 0x39;
            damage_width = 1;
        }

        int16_t index_global = MOX::scroll_bar->first_visible_row * MOX::big_icon_display_columns;

        bool done = false;
        int16_t index = 0;

        while (index_global < MOX::n_fltscrn_big_icons) {
            if (done) {
                break;
            }

            int16_t ship_idx = MOX::fltscrn_big_icon[index_global].ship_idx;

            s_animation_header* pic_seg = KEN::Get_Ship_Id_Picture_Seg(ship_idx);
            int16_t width = animate::Get_Width(pic_seg);
            int16_t height = animate::Get_Height(pic_seg);

            int16_t x;
            int16_t y;
            FLT2::Get_Fltscrn_Big_Icon_XY(index, &x, &y);

            int32_t width_diff = 0x39 - width;
            int16_t half_w_diff = (int16_t)((width_diff - (width_diff >> 31)) >> 1);
            int32_t height_diff = 0x39 - height;
            int16_t half_h_diff = (int16_t)((height_diff - (height_diff >> 31)) >> 1);

            (void)HACCESS::Ship_Owner(ship_idx);

            if (MOX::scanned_big_ship == index_global && MOX::current_screen == SCREEN_FLEET) {
                Draw_Box_Around_Scanned_Ship(index_global);
            }

            if (MOX::fltscrn_big_icon[index_global].selected != 0) {
                int16_t draw_x;
                int16_t draw_y;
                if (MOX::current_screen == SCREEN_FLEET) {
                    draw_y = y + 1;
                    draw_x = x - 2;
                } else {
                    draw_y = y - 1;
                    draw_x = x - 1;
                }
                animate::Draw(draw_x, draw_y, MOX::selected_box_seg);
            }

            pic_seg = KEN::Get_Ship_Id_Picture_Seg(ship_idx);
            animate::Draw(x + half_w_diff + 1, y + half_h_diff + 2, pic_seg);

            if (HACCESS::Is_Combat_Ship(ship_idx)) {
                FLEETPOP::Draw_Damage_Bars(x, y, damage_width, damage_y_off, damage_x_off, ship_idx);
            }

            index++;
            if (MOX::big_icon_display_rows * MOX::big_icon_display_columns <= index) {
                done = true;
            }

            index_global++;
        }
    }

    void __cdecl Draw_Fltscrn_Ship_Destination_Lines(int16_t screen_id) {
        bool draw_dest = false;
        int16_t icon_idx = -1;
        int16_t view_x1, view_y1, view_x2, view_y2;
        int16_t out_x, out_y;
        s_colors temp_color;

        s_colors local_green = MAINSCR::ship_direction_line_green_colors;
        s_colors local_red = MAINSCR::ship_direction_line_red_colors;

        if (screen_id == SCREEN_FLEET) {
            if (relocate_button_mode != 1 && MOX::small_ship_stack_ptr > -1) {
                int16_t first_sel = First_Selected_Ship();
                if (first_sel != -1) {
                    if (MOX::galaxy_map_scanned_star > -1 && MOX::galaxy_map_scanned_star < MOX::NUM_STARS) {
                        icon_idx = F_Ship_Icon_From_Node_Ptr(MOX::ship_stack_start[MOX::small_ship_stack_ptr]);
                        if (icon_idx > -1) {
                            draw_dest = true;
                        }
                    }
                }
            }
            view_x1 = 15;
            view_y1 = 52;
            view_x2 = 305;
            view_y2 = 182;
        } else {
            view_x1 = 306;
            view_y1 = 235;
            view_x2 = 318;
            view_y2 = 169;
        }

        if (draw_dest) {
            if (MOX::g_ship_move_info.moving == 0 || MOX::star[MOX::galaxy_map_scanned_star].spectral_class == STAR_CLASS_BLACK_HOLE) {
                temp_color = local_red;
            } else {
                temp_color = local_green;
            }

            MOVEBOX::Get_Galaxy_Map_Star_XY(
                MOX::galaxy_map_scanned_star,
                view_x1,
                view_y1,
                view_x2,
                view_y2,
                0,
                0,
                1,
                &out_x,
                &out_y);

            SHIPS::Draw_Ship_Destination_Line(
                out_x + 3, out_y + 3, MOX::ship_icon[icon_idx].x, MOX::ship_icon[icon_idx].y, &temp_color);
        }

        if (MOX::ship_icon_count > 0) {
            for (int16_t i = 0; i < MOX::ship_icon_count; ++i) {
                draw_dest = false;
                int16_t ship_id = HACCESS::Ship_Icon_Ship_Id(i);
                int16_t owner = HACCESS::Ship_Owner(ship_id);

                if (owner == MOX::PLAYER_NUM) {
                    int16_t abs_loc = HAROLD::Absolute_Location(MOX::ship[ship_id].location);
                    if (abs_loc != MOX::NUM_STARS && MOX::ship[ship_id].location >= SHIP_LOCATION_MOVING_OFFSET && MOX::ship[ship_id].location < SHIP_LOCATION_LIMIT) {
                        draw_dest = true;
                        temp_color = local_green;
                    }
                } else {
                    if (SHIPS::Enemy_Ship_Heading_Toward_Our_Colony(MOX::ship_icon[i].node_idx)) {
                        draw_dest = true;
                        temp_color = local_red;
                    }
                }

                if (draw_dest) {
                    int16_t actual_star_id;
                    HAROLD::Actual_Star_Id(MOX::ship[ship_id].location, &actual_star_id);

                    MOVEBOX::Get_Galaxy_Map_Star_XY(
                        actual_star_id,
                        view_x1,
                        view_y1,
                        view_x2,
                        view_y2,
                        0,
                        0,
                        1,
                        &out_x,
                        &out_y);

                    SHIPS::Draw_Ship_Destination_Line(
                        out_x + 3, out_y + 3, MOX::ship_icon[i].x, MOX::ship_icon[i].y, &temp_color);
                }
            }
        }
    }

    void __cdecl Draw_Fltscrn_Small_Ship_Icons() {
        for (int16_t i = MOX::ship_icon_count - 1; i >= 0; --i) {
            s_animation_header* anim;
            int16_t node_idx = MOX::ship_icon[i].node_idx;
            int16_t owner = HACCESS::Ship_Stack_Owner(node_idx);

            if (owner < MAX_PLAYERS) {
                uint8_t color = MOX::player[owner].color;
                animate::Reset_Animation_Frame(MOX::fltscrn_small_ship_icon_seg[color]);
                anim = MOX::fltscrn_small_ship_icon_seg[color];
            } else {
                int16_t ship_idx = MOX::ship_node[node_idx].ship_idx;
                int8_t ship_owner = MOX::ship[ship_idx].owner;
                animate::Reset_Animation_Frame(MOX::fltscrn_monster_ship_icon[ship_owner - MAX_PLAYERS]);
                anim = MOX::fltscrn_monster_ship_icon[ship_owner - MAX_PLAYERS];
            }

            animate::Draw(MOX::ship_icon[i].x, MOX::ship_icon[i].y, anim);
        }
    }

    void __cdecl Draw_Flashing_Small_Ship_Icon(void) {
        int16_t node_idx;
        int16_t owner_idx;
        int16_t icon_idx;
        s_animation_header* flashing_anim;

        node_idx = MOX::ship_stack_start[MOX::small_ship_stack_ptr];
        owner_idx = HACCESS::Ship_Owner(MOX::ship_node[node_idx].ship_idx);

        if (owner_idx < MAX_PLAYERS) {
            uint8_t color_idx = (uint8_t)MOX::player[owner_idx].color;
            flashing_anim = MOX::fltscrn_flashing_ship_pict_seg[color_idx];
        } else {
            flashing_anim = MOX::fltscrn_flashing_monster_ship_icon[owner_idx - MAX_PLAYERS];
        }

        icon_idx = FLT1::F_Ship_Icon_From_Node_Ptr(node_idx);

        if (icon_idx != -1) {
            animate::Draw(MOX::ship_icon[icon_idx].x, MOX::ship_icon[icon_idx].y, flashing_anim);
        }
    }

    /**
     * Finds the index of the first ship that is marked as selected in the fleet screen big icons.
     * Returns -1 if no ships are selected.
     */
    int16_t __cdecl First_Selected_Ship() {
        int16_t result = -1;
        int16_t i = 0;

        if (MOX::n_fltscrn_big_icons > 0) {
            do {
                // The assembly checks if the result is already found at the start of the loop iteration.
                if (result != -1) {
                    break;
                }

                if (MOX::fltscrn_big_icon[i].selected != 0) {
                    result = MOX::fltscrn_big_icon[i].ship_idx;
                }

                i++;
            } while (i < MOX::n_fltscrn_big_icons);
        }

        return result;
    }

    void __cdecl Set_Small_Ship_Stack_Pointer(bool check_box) {
        MOX::small_ship_stack_ptr = -1;
        MOX::fltscrn_stack_owner = -1;

        if (MOVEBOX::Moveable_Box_Selected(2) && check_box) {
            MOX::small_ship_stack_ptr = MOX::fleet_box_ship_stack;
        } else {
            for (int16_t i = 0; i < MOX::ship_icon_count; ++i) {
                int16_t owner_player = HACCESS::Ship_Stack_Owner(MOX::ship_icon[i].node_idx);
                if (owner_player == MOX::PLAYER_NUM) {
                    int16_t stack_idx = HACCESS::Ship_Stack_From_Ship_Icon(i);
                    if (stack_idx > -1) {
                        MOX::small_ship_stack_ptr = stack_idx;
                        break;
                    }
                }
            }
        }

        if (MOX::small_ship_stack_ptr > -1) {
            FLT2::Set_Fleet_Screen_Stack_Owner();
            MOX::old_fltscrn_stack_owner = MOX::fltscrn_stack_owner;
        }

        Restore_Current_Fleet_Summary();
    }

    void __cdecl Allocate_Fltscrn_Next_Screen_Seg() {
        MOX::fltscrn_big_icon = (s_big_icon*)allocate::Allocate_Next_Block(
            (void*)(uintptr_t)MOX::screen_seg, SHIP_LIST_CAPACITY * sizeof(*MOX::fltscrn_big_icon));
        MOX::fltscrn_icon_selection_status = (int8_t*)allocate::Allocate_Next_Block(
            (void*)(uintptr_t)MOX::screen_seg, SHIP_LIST_CAPACITY * sizeof(*MOX::fltscrn_icon_selection_status));
        MOX::fltscrn_ship_icon_selection_list = (int16_t*)allocate::Allocate_Next_Block(
            (void*)(uintptr_t)MOX::screen_seg, SHIP_LIST_CAPACITY * sizeof(*MOX::fltscrn_ship_icon_selection_list));
        MOX::officer_id_list = (int16_t*)allocate::Allocate_Next_Block(
            (void*)(uintptr_t)MOX::screen_seg, MAX_LEADERS * sizeof(*MOX::officer_id_list));
        MOX::scroll_bar = (s_scroll_bar*)allocate::Allocate_Next_Block(
            (void*)(uintptr_t)MOX::screen_seg, sizeof(*MOX::scroll_bar));

        if (MOX::scroll_bar) {
            memset(MOX::scroll_bar, 0, sizeof(*MOX::scroll_bar));
        }

        allocate::Get_Remaining_Block_Space((void*)(uintptr_t)MOX::screen_seg);
    }

    void __cdecl Draw_Fleet_Screen() {
        if (MOX::draw_fleet_screen_to_back != 0 || MOX::FULL_DRAW_SCREEN_FLAG != 0) {
            graphics::Reset_Window();
            graphics::Fill(0, 0, 639, 479, 0);
            animate::Draw(0, 0, MOX::fleet_background_seg);
            FLT1::Draw_Fltscrn_Small_Ship_Icons();
            video::Copy_Off_To_Back();

            if (MOX::draw_fleet_screen_to_back != 0) {
                MOX::draw_fleet_screen_to_back = 0;
            }
            if (MOX::FULL_DRAW_SCREEN_FLAG != 0) {
                MOX::FULL_DRAW_SCREEN_FLAG = 0;
            }
        }

        if (MOX::galaxy_map_scanned_star > -1) {
            FLT2::Print_Fltscrn_Scanned_Star_Name();
        }

        if (MOX::scanned_big_ship > -1 && MOX::scanned_big_ship < MAX_SHIPS && MOX::current_screen == SCREEN_FLEET) {
            graphics::Set_Window(15, 282, 320, 465);
            video::Clip_On();
            FLT2::Print_Scanned_Ship_Data(MOX::fltscrn_big_icon[MOX::scanned_big_ship].ship_idx);
            video::Clip_Off();
            graphics::Reset_Window();
        }

        FLT1::Draw_Fltscrn_Relocation_Lines(SCREEN_FLEET);
        FLT1::Draw_Fltscrn_Ship_Destination_Lines(SCREEN_FLEET);
        MOVEBOX::Draw_Galaxy_Map_Box(nullptr, nullptr, 15, 52, 305, 182, 0, 0, 0, 0, 1, -1);

        if (FLT1::relocate_button_mode != 1) {
            if (mouse::Mouse_Button() == 1) {
                int16_t icon_limit = MOX::big_icon_display_rows * MOX::big_icon_display_columns;
                int16_t start_idx = MOX::scroll_bar->first_visible_row * MOX::big_icon_display_columns;

                for (int16_t i = 0; i < icon_limit; ++i) {
                    int16_t icon_idx = start_idx + i;
                    if (icon_idx >= MOX::n_fltscrn_big_icons) {
                        break;
                    }

                    int16_t input_id = fields::Auto_Input();
                    if (MOX::fltscrn_big_icon[icon_idx].button_id == input_id) {
                        if (MAINSCR::fleet_ship_selection_status == -1) {
                            MAINSCR::fleet_ship_selection_status = (MOX::fltscrn_big_icon[icon_idx].selected == 0);
                        }
                        MOX::fltscrn_big_icon[icon_idx].selected = MAINSCR::fleet_ship_selection_status;
                        if (MOX::fltscrn_big_icon[icon_idx].selected == 0) {
                            MOX::draw_main_screen_to_back = 1;
                        }
                        FLT1::Draw_Box_Around_Scanned_Ship(icon_idx);
                    }
                }
            }
        }

        if (MOX::scanned_small_ship > -1) {
            OFFICER::Print_Galmap_Scanned_Ship(MOX::scanned_small_ship);
        }

        if (MOX::small_ship_stack_ptr > -1) {
            FLT1::Draw_Flashing_Small_Ship_Icon();
        }

        if (MOX::n_fltscrn_big_icons > 0) {
            if (SCROLL::Draw_Generic_Vertical_Scroll_Bar(MOX::scroll_bar, FLT1::Fill_FltScrn_Scroll_Bar)) {
                MOX::scanned_big_ship = -1;
            }
        }

        if (MOX::fltscrn_stack_owner > -1 && MOX::skip_draw_ships == 0 && MOX::small_ship_stack_ptr > -1) {
            FLT1::Draw_Fltscrn_Big_Ship_Icons(SCREEN_FLEET);
        } else {
            MOX::skip_draw_ships = 0;
        }

        s_animation_header* relocate_anim;
        if (FLT1::relocate_button_mode == 2) {
            relocate_anim = (s_animation_header*)MOX::dimmed_relocate_button_seg;
        } else {
            if (FLT1::relocate_button_mode == 0) {
                animate::Reset_Animation_Frame(MOX::relocate_fleet_button_seg);
            } else {
                animate::Set_Animation_Frame(MOX::relocate_fleet_button_seg, 1);
            }
            relocate_anim = MOX::relocate_fleet_button_seg;
        }
        animate::Draw(441, 380, relocate_anim);

        if (MOX::add_scrap_button == 0) {
            animate::Draw(549, 380, (s_animation_header*)MOX::dimmed_scrap_button_seg);
        }
        if (MOX::add_all_button == 0) {
            animate::Draw(348, 380, (s_animation_header*)MOX::dimmed_all_button_seg);
        }
        if (MOX::add_officers_button == 0) {
            animate::Draw(342, 430, (s_animation_header*)MOX::dimmed_officers_button_seg);
        }

        int32_t next_idx = (int32_t)MOX::multi_colored_line_start + 1;
        MOX::multi_colored_line_start = (int16_t)(next_idx % 8);
    }

    void Fleet_Screen() {
        int16_t saved_map_scale = MOX::cur_map_scale;

        union {
            uint32_t full;
            int16_t words[2];
        } scan_val;

        scan_val.words[1] = -1;
        int16_t relocate_target_star_idx = -1;
        int16_t exit_flag = 0;
        bool redraw_skip = false;
        bool reinit_scroll = false;

        int16_t hovered_icon_idx;
        int16_t clicked_icon_idx;
        int16_t scanned_star_id;
        int16_t clicked_star_id;
        int16_t scrap_list[MAX_SHIPS];
        int16_t scrap_count;

        SYS::Clear_System_Display_Loaded_Flag();
        MOX::big_icon_display_columns = 4;
        MOX::big_icon_display_rows = 5;
        MOX::scroll_bar = nullptr;
        relocate_button_mode = 0;
        MOX::big_icon_display_max_icons = 20;
        MOX::big_icon_x_offset = 0x15B;
        MOX::big_icon_y_offset = 0x35;

        Reload_Fleet_Screen();
        fields::Assign_Auto_Function((void (*)())Draw_Fleet_Screen, 1);
        MOX::icon_overlay_x = -1;
        MOX::icon_overlay_y = -1;
        MOX::drawing_officer_popup = 0;
        MOX::n_fltscrn_big_icons_added = 0;
        COLONY::using_colony_screen_palette = 0;
        MOX::n_fltscrn_big_icons = 0;
        MOX::n_fltscrn_rows = 0;
        MOX::galaxy_star_anim_count = 0;
        MOX::n_big_icons_selected = 0;
        MOX::scanned_small_ship = -1;
        MOX::scanned_big_ship = -1;

        FLT::Set_Fltscrn_Small_Ship_Icon_XYs(15, 52, 305, 182);
        FLT2::Set_Initial_Stack_Pointer();

        MOX::old_small_ship_stack_ptr = -1;
        MOX::global_var = 0;
        MOX::skip_draw_ships = 1;
        fleet_screen_bad_ship_id = -1;

        fields::Clear_Fields();

        int16_t btn_relocate, btn_scrap, btn_close, btn_scroll_up, btn_scroll_down;
        int16_t btn_next_ship, btn_prev_ship, btn_add_officers, btn_all_ships, btn_scroll_field;

        Add_Fleet_Screen_Fields(&btn_relocate, &btn_scrap, &btn_close, &btn_scroll_up, &btn_scroll_down, &btn_next_ship, &btn_prev_ship, &btn_add_officers, &btn_all_ships, &btn_scroll_field);

        Initialize_Scroll_Bar(MOX::scroll_bar);
        EVANHELP::Set_Fleet_Screen_Help_List(MOX::n_fltscrn_big_icons_added);
        fields::Disable_Draw_All_Fields();
        MOX::refresh_mode = 2;
        video::Set_Refresh_Stencil();
        MOX::draw_fleet_screen_to_back = 1;

        mouse::Set_Mouse_List(&MOX::mouse_list_none, 1);
        ERIC::Fast_Fade_Out();
        fonts::Load_Palette(8, 0, 255);
        palette::Set_Palette_Changes(0, 255);

        if (MOX::n_fltscrn_big_icons > 0) {
            KEN::Load_Player_Ship_Palette(MOX::ship[MOX::fltscrn_big_icon[0].ship_idx].owner);
        }
        palstore::Darken_Palette(100);
        video::Set_Page_Off();
        Draw_Fleet_Screen();

        animate::Set_Animation_Frame((s_animation_header*)multi_button_support_seg, 1);
        animate::Draw(0x1A9, 0x1B3, (s_animation_header*)multi_button_support_seg);
        animate::Set_Animation_Frame((s_animation_header*)multi_button_combat_seg, 1);
        animate::Draw(0x1E7, 0x1B3, (s_animation_header*)multi_button_combat_seg);

        palette::Clear_Palette_Changes(0, 255);
        RUSS::Mox_Sync_Update();
        mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);
        ERIC::Fast_Fade_In();
        fields::Enable_Draw_All_Fields();

        do {
            MAINSCR::fleet_ship_selection_status = -1;
            timer::Mark_Time();
            MOX::fix_palette = 0;

            int16_t input = fields::Get_Input();
            scan_val.words[0] = fields::Scan_Input();

            if (input < 0 && relocate_button_mode == 1) {
                scan_val.words[1] = -1;
                relocate_target_star_idx = -1;
                relocate_button_mode = 0;
                mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);
            } else if (input == (int16_t)0xFBAF) {
                HACCESS::Clear_All_Star_Relocations(MOX::PLAYER_NUM);
                char msg_buf_76[200];
                strlcpy(msg_buf_76, HAROLD::H_Message(0x76), sizeof(msg_buf_76));
                HAROLD::User_Box(&MOX::draw_fleet_screen_to_back, msg_buf_76, 0);
            } else if (input == (int16_t)0xFC13) {
                if (merging_relocations == 0) {
                    relocate_button_mode = 2;
                    merging_relocations = 1;
                    mouse::Set_Mouse_List(mouse_list_all_to, 4);
                } else {
                    relocate_button_mode = 0;
                    merging_relocations = 0;
                    mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);
                }
            }

            if (MOX::PLAYER_NUM == MOX::fltscrn_stack_owner) {
                int16_t ret = FLT2::Scan_Fltscrn_Big_Icons(scan_val.full, input, &hovered_icon_idx, &clicked_icon_idx);
                if (hovered_icon_idx >= 0) {
                    MOX::scanned_big_ship = hovered_icon_idx;
                    Restore_Current_Fleet_Summary();
                }
                if (ret == 1) {
                    fields::Deactivate_Help_List();
                    CMBTDRW1::Detailed_View_Ship(MOX::fltscrn_big_icon[clicked_icon_idx].ship_idx);
                    EVANHELP::Set_Fleet_Screen_Help_List(MOX::n_fltscrn_big_icons_added);
                    MOX::draw_fleet_screen_to_back = 1;
                }
            }

            int16_t ret_map = MAINSCR::Scan_Galaxy_Map_Fields(scan_val.full, input, &scanned_star_id, &clicked_star_id);
            if (scanned_star_id >= 0) {
                MOX::galaxy_map_scanned_star = scanned_star_id;
                MOX::scanned_small_ship = -1;
            }

            if (ret_map == 1 && relocate_button_mode == 1) {
                Cancel_Star_Relocation(clicked_star_id);
            } else if (ret_map == 0) {
                if (merging_relocations == 0) {
                    if (relocate_button_mode == 1) {
                        Star_Relocation(&scan_val.words[1], &relocate_target_star_idx, clicked_star_id);
                    } else if (MOX::star[clicked_star_id].spectral_class == STAR_CLASS_BLACK_HOLE) {
                        char msg_buf_78[80];
                        strlcpy(msg_buf_78, HAROLD::H_Message(0x78), sizeof(msg_buf_78));
                        HAROLD::User_Box(&MOX::draw_fleet_screen_to_back, msg_buf_78, 3);
                    } else {
                        int16_t first_ship = First_Selected_Ship();
                        if (first_ship >= 0) {
                            if (HACCESS::Player_Can_Order_Ship(MOX::PLAYER_NUM, &MOX::ship[first_ship])) {
                                FLT2::Fltscrn_Move_Ships(clicked_star_id);
                            } else {
                                Cant_Order_Ship_Message(&MOX::ship[first_ship]);
                            }
                        }
                    }
                } else {
                    merging_relocations = 0;
                    mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);
                    relocate_button_mode = 0;
                    HACCESS::Set_All_Star_Relocations(clicked_star_id, MOX::PLAYER_NUM);
                    char msg_buf_77[80];
                    snprintf(msg_buf_77, sizeof(msg_buf_77), HAROLD::H_Message(0x77), MOX::star[clicked_star_id].name);
                    HAROLD::User_Box(&MOX::draw_fleet_screen_to_back, msg_buf_77, 0);
                    scan_val.words[1] = -1;
                    relocate_target_star_idx = -1;
                }
            }

            int16_t ret_small = Scan_Small_Ship_Icons(scan_val.full, input, &hovered_icon_idx, &clicked_icon_idx);
            if (ret_small == 0) {
                MOX::draw_fleet_screen_to_back = 1;
                MOX::scroll_bar->thumb_center_y = MOX::scroll_bar->track_len_px;
                if (MOX::old_small_ship_stack_ptr != MOX::small_ship_stack_ptr) {
                    if (MOX::old_small_ship_stack_ptr == -1 || MOX::small_ship_stack_ptr == -1 ||
                        HACCESS::Ship_Stack_Owner(MOX::ship_stack_start[MOX::old_small_ship_stack_ptr]) != HACCESS::Ship_Stack_Owner(MOX::ship_stack_start[MOX::small_ship_stack_ptr])) {
                        MOX::skip_draw_ships = 1;
                    }
                    MOX::scanned_big_ship = -1;
                }
            } else if (ret_small == 4 && hovered_icon_idx >= 0) {
                MOX::scanned_small_ship = hovered_icon_idx;
                MOX::galaxy_map_scanned_star = -1;
            }

            if (input == btn_all_ships && MOX::n_fltscrn_big_icons > 0 &&
                HACCESS::Ship_Owner(MOX::fltscrn_big_icon[0].ship_idx) == MOX::PLAYER_NUM) {
                Set_Fltscrn_Big_Icons(MOX::n_big_icons_selected != MOX::n_fltscrn_big_icons);
            }

            if (input == btn_close) {
                MOX::current_screen = SCREEN_MAIN;
                fields::Clear_Fields();
                fields::Deactivate_Auto_Function();
                MOX::galaxy_map_scanned_star = -1;
                OFFICER::Move_From_Limbo_To_Pool();
                exit_flag = 1;
            }

            if (input == btn_scrap) {
                Build_Scrap_List(scrap_list, &scrap_count);
                uint8_t scrapped = Scrap_Ships(MOX::PLAYER_NUM, scrap_list, scrap_count, 1);
                scan_val.full = (scan_val.full & 0xFFFFFF00) | scrapped;

                int16_t selected_count = N_Fltscrn_Ships_Selected();
                MOX::draw_fleet_screen_to_back = 1;
                MOX::old_small_ship_stack_ptr = -1;

                if (selected_count == MOX::n_fltscrn_big_icons && (int8_t)scan_val.words[0] != 0) {
                    MOX::n_fltscrn_big_icons = 0;
                    MOX::galaxy_map_scanned_star = -1;
                    SHIPS::Build_Ship_Icons();
                    FLT::Set_Fltscrn_Small_Ship_Icon_XYs(15, 52, 305, 182);
                    Set_Small_Ship_Stack_Pointer(0);
                }
                MOX::scanned_big_ship = -1;
            }

            if (input == btn_add_officers) {
                if (MOX::add_officers_button == 0) {
                    char msg_buf_79[200];
                    strlcpy(msg_buf_79, HAROLD::H_Message(0x79), sizeof(msg_buf_79));
                    HAROLD::User_Box(&MOX::draw_fleet_screen_to_back, msg_buf_79, 0);
                } else {
                    MOX::current_screen = SCREEN_OFFICERS;
                    MOX::return_screen = SCREEN_FLEET;
                    exit_flag = 1;
                    MOX::temp_star_handle = MOX::small_ship_stack_ptr;
                }
            }

            if (input == btn_scroll_down) {
                SCROLL::Scroll_Down_One_Item(MOX::scroll_bar);
            } else if (input == btn_scroll_up) {
                SCROLL::Scroll_Up_One_Item(MOX::scroll_bar);
            }

            if (input == btn_relocate && relocate_button_mode != 2) {
                if (relocate_button_mode == 0) {
                    mouse::Set_Mouse_List(mouse_list_from, 4);
                    relocate_button_mode = 1;
                } else if (relocate_button_mode == 1) {
                    mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);
                    scan_val.words[1] = -1;
                    relocate_button_mode = 0;
                    relocate_target_star_idx = -1;
                }
            }

            if (input == btn_next_ship || input == (int16_t)0xFC16) {
                int16_t old_ptr = MOX::small_ship_stack_ptr;
                Next_Ship_Icon();
                if (old_ptr != MOX::small_ship_stack_ptr) {
                    reinit_scroll = true;
                    MOX::scroll_bar->thumb_center_y = -1;
                    MOX::draw_fleet_screen_to_back = 1;
                    MOX::skip_draw_ships = 1;
                    MOX::scanned_big_ship = -1;
                }
            }

            if (input == btn_prev_ship || input == (int16_t)0xFC17) {
                int16_t old_ptr = MOX::small_ship_stack_ptr;
                Previous_Ship_Icon();
                if (old_ptr != MOX::small_ship_stack_ptr) {
                    MOX::scanned_big_ship = -1;
                    MOX::scroll_bar->thumb_center_y = -1;
                    MOX::draw_fleet_screen_to_back = 1;
                    MOX::skip_draw_ships = 1;
                    reinit_scroll = true;
                }
            }

            if (input == support_filter_button || input == combat_filter_button) {
                if (input == support_filter_button) {
                    if (MOX::fltscrn_support_button_status == 0 && MOX::fltscrn_combat_button_status == 0) {
                        MOX::fltscrn_combat_button_status = 1;
                    }
                } else {
                    if (MOX::fltscrn_support_button_status == 0 && MOX::fltscrn_combat_button_status == 0) {
                        MOX::fltscrn_support_button_status = 1;
                    }
                }

                if (MOX::small_ship_stack_ptr >= 0 && MOX::small_ship_stack_ptr < MOX::ship_stack_count) {
                    if (FLT2::Fltscrn_Ships_To_Be_Displayed(MOX::ship_stack_start[MOX::small_ship_stack_ptr]) == 0) {
                        Next_Ship_Icon();
                    }
                }

                redraw_skip = true;
                MOX::scroll_bar->thumb_center_y = MOX::scroll_bar->track_len_px;
                MOX::draw_fleet_screen_to_back = 1;
                MOX::skip_draw_ships = 1;
                MOX::scroll_bar->first_visible_row = -1;
                MOX::scanned_big_ship = -1;
            }

            fields::Clear_Fields();
            Add_Fleet_Screen_Fields(&btn_relocate, &btn_scrap, &btn_close, &btn_scroll_up, &btn_scroll_down, &btn_next_ship, &btn_prev_ship, &btn_add_officers, &btn_all_ships, &btn_scroll_field);

            if (reinit_scroll) {
                Initialize_Scroll_Bar(MOX::scroll_bar);
                reinit_scroll = false;
            }

            EVANHELP::Set_Fleet_Screen_Help_List(MOX::n_fltscrn_big_icons_added);

            if (MOX::fix_palette != 0) {
                int16_t owner = HACCESS::Ship_Owner(MOX::ship_node[MOX::ship_stack_start[MOX::small_ship_stack_ptr]].ship_idx);
                FLT2::Fix_Palette(owner);
            }

            if (exit_flag == 0) {
                if (MOX::small_ship_stack_ptr >= 0) {
                    MOX::n_big_icons_selected = Update_Selection_Flags();
                }
                if (redraw_skip) {
                    redraw_skip = false;
                } else {
                    video::Set_Page_Off();
                    Draw_Fleet_Screen();
                    RUSS::Mox_Sync_Update();
                }
            }

            timer::Release_Time(1);

        } while (exit_flag == 0);

        if (MOVEBOX::Moveable_Box_Selected(2) != 0) {
            MOX::fleet_box_ship_stack = MOX::small_ship_stack_ptr;
            if (MOX::small_ship_stack_ptr == -1) {
                HACCESS::Clear_Fleet_Box_Selected_Ship_Status();
                MOVEBOX::Vanish_Fleet_Movement_Box();
            }
        }

        mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);
        MOX::cur_map_scale = saved_map_scale;
        MOX::reloaded_ship_palette = 0;
    }

    int16_t __cdecl Scan_Small_Ship_Icons(int16_t scanned_field_id, int16_t clicked_field_id, int16_t* scanned_icon_idx, int16_t* clicked_icon_idx) {
        int16_t result = -1;
        bool found = false;

        *scanned_icon_idx = -1;
        *clicked_icon_idx = -1;

        for (int16_t i = 0; i < MOX::ship_icon_count; ++i) {
            if (found) {
                break;
            }
            if (MOX::ship_icon[i].stack_id == clicked_field_id) {
                result = 0;
                SHIPS::Overlapped_Ship_Icon_Button(&i, 7, 4);
                *clicked_icon_idx = i;
                MOX::small_ship_stack_ptr = HACCESS::Ship_Stack_From_Ship_Icon(i);
                Restore_Current_Fleet_Summary();

                if (MOX::old_small_ship_stack_ptr != MOX::small_ship_stack_ptr) {
                    MOX::old_small_ship_stack_ptr = -1;
                }

                found = true;
            }
        }

        if (result == -1) {
            found = false;
            for (int16_t i = 0; i < MOX::ship_icon_count; ++i) {
                if (found) {
                    break;
                }
                if (MOX::ship_icon[i].stack_id == scanned_field_id) {
                    result = 4;
                    found = true;
                    *scanned_icon_idx = i;
                }
            }
        }

        return result;
    }

    void __cdecl Cancel_Star_Relocation(int16_t star_idx) {
        char message[100];
        const char* format;
        s_star_data* star;

        star = &MOX::star[star_idx];

        format = HAROLD::H_Message(137);

        snprintf(message, sizeof(message), format, star->name);

        HACCESS::Clear_Star_Relocation(star_idx, MOX::PLAYER_NUM);

        HAROLD::User_Box(&MOX::draw_fleet_screen_to_back, message, 3);
    }

    void __cdecl Star_Relocation(int16_t* star_from_ptr, int16_t* star_to_ptr, int16_t star_idx) {
        if (*star_from_ptr == -1) {
            if (Okay_To_Set_Relocate_Star2(star_idx)) {
                *star_from_ptr = star_idx;
                mouse::Set_Mouse_List(mouse_list_to, 4);
            }
        } else {
            if (Okay_To_Set_Relocate_To_Star(*star_from_ptr, star_idx)) {
                *star_to_ptr = star_idx;
                mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);

                int16_t star_from = *star_from_ptr;
                if (*star_to_ptr == star_from) {
                    Cancel_Star_Relocation(star_from);
                } else {
                    HACCESS::Set_Relocation(star_from, MOX::PLAYER_NUM, star_idx);
                }

                mouse::Set_Mouse_List(mouse_list_from, 4);
                *star_from_ptr = -1;
                *star_to_ptr = -1;
            }
        }
    }

    void __cdecl Cant_Order_Ship_Message(const s_ship_data* ship) {
        char buffer[80];
        const char* format;

        format = HAROLD::H_Message(0x75 /* HESTR_075_S_CANNOT_RECEIVE_ORDER */);
        snprintf(buffer, sizeof(buffer), format, ship->d.name);

        HAROLD::User_Box(&MOX::draw_fleet_screen_to_back, buffer, 0);

        fleet_screen_bad_ship_id = -1;
    }

    /** Collects the currently selected fleet-screen ships into the scrap list. */
    void __cdecl Build_Scrap_List(int16_t* scrap_list, int16_t* list_count) {
        *list_count = 0;

        if (MOX::n_fltscrn_big_icons > 0) {
            for (int16_t i = 0; i < MOX::n_fltscrn_big_icons; ++i) {
                s_big_icon& icon = MOX::fltscrn_big_icon[i];

                if (icon.ship_idx > -1 && icon.ship_idx < MAX_SHIPS && icon.selected != 0) {
                    scrap_list[*list_count] = icon.ship_idx;
                    *list_count += 1;
                }
            }
        }
    }

    /** Awards any eligible reverse-engineered technology from the scrapped ships. */
    void __cdecl Plunder_Ships_For_Tech(int16_t* ship_indices, int16_t count, int8_t flag) {
        for (int16_t i = 0; i < count; ++i) {
            s_ship_data* ship = &MOX::ship[ship_indices[i]];
            if (ship->d.previous_owner != ship->owner) {
                Get_Technology_From_Scrapped_Ship(ship, MOX::PLAYER_NUM, flag);
            }
        }
    }

    /** Sums the BC value of all selected ships on the fleet screen. */
    int32_t __cdecl Get_Scrap_Ship_Value() {
        int32_t total_value = 0;

        if (MOX::n_fltscrn_big_icons > 0) {
            for (int16_t i = 0; i < MOX::n_fltscrn_big_icons; ++i) {
                if (MOX::fltscrn_big_icon[i].selected != 0) {
                    total_value += maintain::Ship_Scrap_Value(&MOX::ship[MOX::fltscrn_big_icon[i].ship_idx]);
                }
            }
        }

        return total_value;
    }

    /** Counts selected fleet-screen ships from the current scroll position onward. */
    int16_t __cdecl N_Fltscrn_Ships_Selected() {
        int16_t selected_count = 0;
        int16_t start_index = MOX::scroll_bar->first_visible_row * MOX::big_icon_display_columns;

        for (int16_t i = start_index; i < MOX::n_fltscrn_big_icons; ++i) {
            if (MOX::fltscrn_big_icon[i].selected != 0) {
                selected_count++;
            }
        }

        return selected_count;
    }

    void __cdecl Next_Ship_Icon() {
        if (MOX::ship_stack_count <= 0) {
            return;
        }

        int16_t initial_ptr = MOX::small_ship_stack_ptr;
        int16_t original_owner = HACCESS::Ship_Stack_Owner(MOX::ship_stack_start[initial_ptr]);
        int16_t current_ptr = initial_ptr;
        bool done = false;

        do {
            current_ptr++;
            if (MOX::ship_stack_count <= current_ptr) {
                current_ptr = 0;
            }

            int16_t current_ship_stack = MOX::ship_stack_start[current_ptr];
            int16_t current_owner = HACCESS::Ship_Stack_Owner(current_ship_stack);

            if (current_owner == MOX::PLAYER_NUM && original_owner == MOX::PLAYER_NUM) {
                if (FLT2::Fltscrn_Ships_To_Be_Displayed(current_ship_stack) > 0) {
                    MOX::small_ship_stack_ptr = current_ptr;
                    done = true;
                }
            } else {
                current_owner = HACCESS::Ship_Stack_Owner(current_ship_stack);
                if (current_owner != MOX::PLAYER_NUM && original_owner != MOX::PLAYER_NUM) {
                    if (FLT2::Fltscrn_Ships_To_Be_Displayed(current_ship_stack) > 0) {
                        MOX::small_ship_stack_ptr = current_ptr;
                        done = true;
                    }
                }
            }

            if (current_ptr == initial_ptr) {
                done = true;
            }
        } while (!done);

        Restore_Current_Fleet_Summary();
    }

    void __cdecl Previous_Ship_Icon() {
        if (MOX::ship_stack_count <= 0) {
            return;
        }

        int16_t initial_ptr = MOX::small_ship_stack_ptr;
        int16_t initial_owner = HACCESS::Ship_Stack_Owner(MOX::ship_stack_start[initial_ptr]);
        int16_t current_ptr = initial_ptr;
        bool found = false;

        do {
            current_ptr--;
            if (current_ptr < 0) {
                current_ptr = MOX::ship_stack_count - 1;
            }

            int16_t ship_idx = MOX::ship_stack_start[current_ptr];
            int16_t owner = HACCESS::Ship_Stack_Owner(ship_idx);

            bool is_match = false;
            if (owner == MOX::PLAYER_NUM) {
                if (MOX::PLAYER_NUM == initial_owner) {
                    is_match = true;
                }
            } else {
                // Redundant call preserved from original binary behavior
                owner = HACCESS::Ship_Stack_Owner(ship_idx);
                if (owner != MOX::PLAYER_NUM && MOX::PLAYER_NUM != initial_owner) {
                    is_match = true;
                }
            }

            if (is_match) {
                if (FLT2::Fltscrn_Ships_To_Be_Displayed(ship_idx) > 0) {
                    MOX::small_ship_stack_ptr = current_ptr;
                    found = true;
                }
            }

            if (initial_ptr == current_ptr) {
                found = true;
            }

        } while (!found);

        Restore_Current_Fleet_Summary();
    }

    // Suggested name: Update_Selection_Flags
    int16_t __cdecl Update_Selection_Flags() {
        int16_t selected_count = 0;
        int16_t first_node_idx = MOX::ship_stack_start[MOX::small_ship_stack_ptr];
        int16_t ship_idx = MOX::ship_node[first_node_idx].ship_idx;

        if (HACCESS::Ship_Owner(ship_idx) == MOX::PLAYER_NUM) {
            if (MOX::n_fltscrn_big_icons > 0) {
                for (int16_t i = 0; i < MOX::n_fltscrn_big_icons; ++i) {
                    int8_t is_selected = MOX::fltscrn_big_icon[i].selected;
                    MOX::fltscrn_icon_selection_status[i] = is_selected;

                    if (is_selected != 0) {
                        if (selected_count == 0) {
                            MOX::screen_mouse_list_type = MOX::fltscrn_big_icon[i].ship_idx;
                        }
                        if (selected_count == 1) {
                            MOX::temp_star_handle = MOX::fltscrn_big_icon[i].ship_idx;
                        }
                        selected_count++;
                    }
                }
            }
        }

        return selected_count;
    }

    void __cdecl Draw_Box_Around_Scanned_Ship(int16_t icon_idx) {
        s_big_icon* icon = &MOX::fltscrn_big_icon[icon_idx];
        int16_t x = icon->screen_x;
        int16_t y = icon->screen_y;

        if (MOX::current_screen == SCREEN_FLEET) {
            animate::Draw(x - 3, y - 1, MOX::scanned_box_seg);
        } else {
            animate::Draw(x - 2, y - 2, MOX::scanned_box_seg);
        }
    }

    /**
     * Searches the ship icon array for a record matching the provided node index.
     * Returns the index of the first match found, or -1 if no match exists.
     */
    void __cdecl Reload_Fleet_Screen(void) {
        int16_t matching_officer_count = 0;

        MAINSCR::Allocate_First_Screen_Seg(5);

        MOX::fleet_background_seg = (s_animation_header*)farload::Far_Reload_Next(PTR_s_FLEET_LBX, 0, MOX::screen_seg);
        MOX::fleet_scroll_anim_seg = (s_animation_header*)farload::Far_Reload_Next(PTR_s_FLEET_LBX, 1, MOX::screen_seg);
        MOX::up_scroll_button_seg = (s_animation_header*)farload::Far_Reload_Next(PTR_s_FLEET_LBX, 2, MOX::screen_seg);
        MOX::prev_fleet_button_seg = (s_animation_header*)farload::Far_Reload_Next(PTR_s_FLEET_LBX, 3, MOX::screen_seg);
        MOX::next_fleet_button_seg = (s_animation_header*)farload::Far_Reload_Next(PTR_s_FLEET_LBX, 4, MOX::screen_seg);
        MOX::down_scroll_button_seg = (s_animation_header*)farload::Far_Reload_Next(PTR_s_FLEET_LBX, 5, MOX::screen_seg);
        MOX::fleet_all_button_seg = (s_animation_header*)farload::Far_Reload_Next(PTR_s_FLEET_LBX, 6, MOX::screen_seg);
        MOX::relocate_fleet_button_seg = (s_animation_header*)farload::Far_Reload_Next(PTR_s_FLEET_LBX, 7, MOX::screen_seg);
        MOX::scrap_button_seg = (s_animation_header*)farload::Far_Reload_Next(PTR_s_FLEET_LBX, 8, MOX::screen_seg);
        FLT1::multi_button_support_seg = farload::Far_Reload_Next(PTR_s_FLEET_LBX, 9, MOX::screen_seg);
        FLT1::multi_button_combat_seg = farload::Far_Reload_Next(PTR_s_FLEET_LBX, 10, MOX::screen_seg);
        MOX::officers_button_seg = (s_animation_header*)farload::Far_Reload_Next(PTR_s_FLEET_LBX, 11, MOX::screen_seg);
        MOX::fleet_return_button_seg = (s_animation_header*)farload::Far_Reload_Next(PTR_s_FLEET_LBX, 12, MOX::screen_seg);
        MOX::dimmed_all_button_seg = farload::Far_Reload_Next(PTR_s_FLEET_LBX, 13, MOX::screen_seg);
        MOX::dimmed_relocate_button_seg = farload::Far_Reload_Next(PTR_s_FLEET_LBX, 14, MOX::screen_seg);
        MOX::dimmed_scrap_button_seg = farload::Far_Reload_Next(PTR_s_FLEET_LBX, 15, MOX::screen_seg);
        MOX::dimmed_officers_button_seg = farload::Far_Reload_Next(PTR_s_FLEET_LBX, 16, MOX::screen_seg);
        MOX::selected_box_seg = (s_animation_header*)farload::Far_Reload_Next(PTR_s_FLEET_LBX, 17, MOX::screen_seg);
        MOX::scanned_box_seg = (s_animation_header*)farload::Far_Reload_Next(PTR_s_FLEET_LBX, 18, MOX::screen_seg);

        MOX::temp_officers_pict = (s_animation_header**)allocate::Allocate_Next_Block(MOX::screen_seg, MAX_LEADERS * sizeof(s_animation_header*));
        FLT1::Allocate_Fltscrn_Next_Screen_Seg();

        for (int16_t i = 0; i < MAX_LEADERS; ++i) {
            s_leader_data* leader = &MOX::leaders[i];
            if (leader->player_index == MOX::PLAYER_NUM && leader->status == 1 && leader->type == LEADER_TYPE_SHIP) {
                MOX::temp_officers_pict[matching_officer_count] = (s_animation_header*)FLT1::Load_Fltscrn_Officer_Picture(MOX::PLAYER_NUM, i, MOX::screen_seg, 0);
                MOX::officer_id_list[matching_officer_count] = i;
                matching_officer_count++;
            }
        }

        FLT1::Reload_Small_Ship_Icons();
        FLT1::Load_Galaxy_Stars();
    }

    void* __cdecl Load_Fltscrn_Officer_Picture(int16_t player_num, int16_t leader_idx, void* buffer, uint8_t reload_flag) {
        (void)player_num;
        int16_t entry_num = (int16_t)MOX::leaders[leader_idx].pict_num + 0x2d;

        if (reload_flag != 0) {
            return farload::Far_Reload("FLEET.LBX", entry_num, buffer);
        } else {
            return farload::Far_Reload_Next("FLEET.LBX", entry_num, buffer);
        }
    }

    void __cdecl Add_Fleet_Screen_Fields(int16_t* relocate_fleet_field_id, int16_t* scrap_button_field_id, int16_t* fleet_return_button_field_id, int16_t* up_scroll_button_field_id, int16_t* down_scroll_button_field_id, int16_t* next_fleet_button_field_id, int16_t* prev_fleet_button_field_id, int16_t* officers_button_field_id, int16_t* all_button_field_id, int16_t* scroll_field_id) {
        int16_t temp_field_id;

        if (MOX::n_big_icons_selected > 0 && MOX::PLAYER_NUM == MOX::fltscrn_stack_owner && FLT1::relocate_button_mode != 1) {
            MOX::add_scrap_button = 1;
            *scrap_button_field_id = fields::Add_Button_Field(549, 380, "", MOX::scrap_button_seg, "S", 40);
        } else {
            MOX::add_scrap_button = 0;
            *scrap_button_field_id = -1000;
        }

        if (MOX::n_fltscrn_big_icons > 0 && MOX::PLAYER_NUM == MOX::fltscrn_stack_owner && FLT1::relocate_button_mode != 1) {
            MOX::add_all_button = 1;
            *all_button_field_id = fields::Add_Button_Field(348, 380, "", MOX::fleet_all_button_seg, "A", 40);
        } else {
            MOX::add_all_button = 0;
            *all_button_field_id = -1000;
        }

        *fleet_return_button_field_id = fields::Add_Button_Field(
            556, 430, "", MOX::fleet_return_button_seg, "\x1b", /* ESC control byte, not a printable space. */
            40);

        *up_scroll_button_field_id = -1000;
        *down_scroll_button_field_id = -1000;
        *prev_fleet_button_field_id = -1000;
        *next_fleet_button_field_id = -1000;

        if (MOX::n_fltscrn_big_icons > 20) {
            *up_scroll_button_field_id = fields::Add_Button_Field(606, 59, "", MOX::up_scroll_button_seg, "-", 40);
            *down_scroll_button_field_id = fields::Add_Button_Field(605, 325, "", MOX::down_scroll_button_seg, "+", 40);
        }
        *prev_fleet_button_field_id = fields::Add_Button_Field(19, 249, "", MOX::prev_fleet_button_seg, ",", 40);
        *next_fleet_button_field_id = fields::Add_Button_Field(283, 249, "", MOX::next_fleet_button_seg, ".", 40);

        MOX::scroll_bar->dn_btn = *down_scroll_button_field_id;
        MOX::scroll_bar->up_btn = *up_scroll_button_field_id;

        if (FLT1::relocate_button_mode == 2) {
            FLT1::relocate_button_mode = 2;
            *relocate_fleet_field_id = 0;
        } else {
            int h = animate::Get_Height(MOX::relocate_fleet_button_seg);
            int w = animate::Get_Width(MOX::relocate_fleet_button_seg);
            *relocate_fleet_field_id = fields::Add_Hidden_Field(441, 380, w + 441, h + 380, "R", 41);
        }

        bool has_officer = FLT1::At_Least_One_Officer(MOX::PLAYER_NUM);
        if (has_officer) {
            *officers_button_field_id = fields::Add_Button_Field(342, 430, "", MOX::officers_button_seg, "L", 40);
            MOX::add_officers_button = 1;
        } else {
            int h = animate::Get_Height(MOX::officers_button_seg);
            int w = animate::Get_Width(MOX::officers_button_seg);
            *officers_button_field_id = fields::Add_Hidden_Field(342, 430, w + 342, h + 430, "", 41);
            MOX::add_officers_button = 0;
        }

        FLT2::Add_Fltscrn_Small_Icon_Fields();

        if (MOX::small_ship_stack_ptr > -1) {
            FLT2::Fleet_Screen_Big_Icon_Fields(scroll_field_id, 605, 86, 14, 234);
            MOX::scroll_bar->total_rows = MOX::n_fltscrn_rows;
        }

        MOVEBOX::Add_Galaxy_Map_Fields_2(15, 52, 305, 182, 0, 0, 0, 0, 1, &temp_field_id, MOX::galaxy_map_star_field, &MOX::galaxy_map_grid_field);

        MAINSCR::debug_field = fields::Add_Hidden_Field(0, 470, 10, 479, "", 41);

        FLT1::support_filter_button = fields::Add_Radio_Button_Field(425, 435, "", (s_animation_header*)FLT1::multi_button_support_seg, &MOX::fltscrn_support_button_status, "U", 41);
        FLT1::combat_filter_button = fields::Add_Radio_Button_Field(487, 435, "", (s_animation_header*)FLT1::multi_button_combat_seg, &MOX::fltscrn_combat_button_status, "C", 41);

        fields::Add_Hidden_Field(0, 0, 639, 479, "", 0);
    }

    void __cdecl Reload_Small_Ship_Icons() {
        int16_t entry_idx = 19;

        allocate::Get_Remaining_Block_Space(MOX::screen_seg);

        for (int i = 0; i < MAX_PLAYERS; ++i) {
            int16_t current_entry = entry_idx++;

            MOX::fltscrn_small_ship_icon_seg[i] = (s_animation_header*)farload::Far_Reload_Next(
                "FLEET.LBX",
                (int)current_entry,
                MOX::screen_seg);

            MOX::fltscrn_flashing_ship_pict_seg[i] = (s_animation_header*)farload::Far_Reload_Next(
                "FLEET.LBX",
                (int)current_entry,
                MOX::screen_seg);
        }

        for (int i = 0; i < 7; ++i) {
            int16_t current_entry = entry_idx++;

            MOX::fltscrn_monster_ship_icon[i] = (s_animation_header*)farload::Far_Reload_Next(
                "FLEET.LBX",
                (int)current_entry,
                MOX::screen_seg);

            MOX::fltscrn_flashing_monster_ship_icon[i] = (s_animation_header*)farload::Far_Reload_Next(
                "FLEET.LBX",
                (int)current_entry,
                MOX::screen_seg);
        }

        allocate::Get_Remaining_Block_Space(MOX::screen_seg);
    }

    /**
     * @brief Checks if the given player has at least one officer (leader) assigned.
     *
     * Iterates through the global leaders array to find any leader that belongs
     * to the given player and has an active status (status >= 0).
     *
     * @return true if at least one active officer is found, false otherwise.
     */
    bool __cdecl At_Least_One_Officer(int16_t player_num) {
        (void)player_num;

        for (int16_t i = 0; i < MAX_LEADERS; ++i) {
            if (MOX::leaders[i].player_index == MOX::PLAYER_NUM && MOX::leaders[i].status >= 0) {
                return true;
            }
        }
        return false;
    }

    /** Loads the animated galaxy-star assets used by the fleet screen. */
    void __cdecl Load_Galaxy_Stars(void) {
        int lbx_entry = 0x22 /* 34 */;

        for (int i = 0; i < 11; ++i) {
            MOX::fleet_galaxy_star_seg[i] = (s_animation_header*)farload::Far_Reload_Next(
                "FLEET.LBX",
                lbx_entry++,
                MOX::screen_seg);
        }
    }

    void __cdecl Draw_Fltscrn_Relocation_Lines(int16_t screen_id) {
        s_colors colors;
        int16_t map_left;
        int16_t view_y;
        int16_t view_x;
        int16_t zoom;
        int16_t x1, y1;
        int16_t x2, y2;

        colors.colors[0] = 6;
        colors.colors[1] = 7;
        colors.colors[2] = 7;
        colors.colors[3] = 8;
        colors.colors[4] = 8;
        colors.colors[5] = 9;
        colors.colors[6] = 9;
        colors.colors[7] = 10;

        if (screen_id == SCREEN_FLEET) {
            map_left = 52 /* 0x34 */;
            view_y = 15 /* 0x0F */;
            view_x = 305 /* 0x131 */;
            zoom = 182 /* 0xB6 */;
        } else {
            map_left = 235 /* 0xEB */;
            view_y = 306 /* 0x132 */;
            view_x = 318 /* 0x13E */;
            zoom = 169 /* 0xA9 */;
        }

        for (int16_t star_idx = 0; star_idx < MOX::NUM_STARS; ++star_idx) {
            if (HACCESS::Star_Has_Relocation(star_idx, MOX::PLAYER_NUM)) {
                int16_t relocation_target_idx = HACCESS::Relocation(star_idx, MOX::PLAYER_NUM);

                MOVEBOX::Get_Galaxy_Map_Star_XY(star_idx, view_y, map_left, view_x, zoom, 0, 0, 1, &x1, &y1);
                MOVEBOX::Get_Galaxy_Map_Star_XY(relocation_target_idx, view_y, map_left, view_x, zoom, 0, 0, 1, &x2, &y2);

                SHIPS::Draw_Ship_Destination_Line(x2 + 3, y2 + 3, x1, y1, &colors);
            }
        }
    }

    /** Validates a relocation-source selection and restores the source cursor set. */
    uint8_t __cdecl Okay_To_Set_Relocate_Star2(int16_t star_idx) {
        uint8_t result;

        result = FLT1::Okay_To_Set_Relocate_Star(star_idx, 0);
        mouse::Set_Mouse_List(FLT1::mouse_list_from, 4);

        return result;
    }

    /** Scraps the selected ships and optionally awards BC and recovered technology. */
    int8_t __cdecl Scrap_Ships(int16_t player_idx, int16_t* ship_indices, int16_t num_ships, int8_t interactive) {
        bool give_bc = true;
        bool has_officer = false;

        int16_t star_id = HAROLD::XY_To_Star_Id(MOX::ship[ship_indices[0]].x, MOX::ship[ship_indices[0]].y);

        if (star_id == -1 || !HAROLD::Player_Has_Colony(player_idx, star_id)) {
            give_bc = false;
            if (interactive != 0) {
                int16_t msg_id;
                if (num_ships == 1) {
                    msg_id = MISC::Is_Vowel(MOX::player[player_idx].race_name[0]) ? 123 /* 0x7b */ : 122 /* 0x7a */;
                } else {
                    msg_id = MISC::Is_Vowel(MOX::player[player_idx].race_name[0]) ? 125 /* 0x7d */ : 124 /* 0x7c */;
                }

                char msg_buf[300];
                snprintf(msg_buf, sizeof(msg_buf), HAROLD::H_Message(msg_id), MOX::player[player_idx].race_name);

                if (HAROLD::User_Box(&MOX::draw_fleet_screen_to_back, msg_buf, 1) == 0) {
                    char cancel_msg_buf[200];
                    strlcpy(cancel_msg_buf, HAROLD::H_Message(127 /* 0x7f */), sizeof(cancel_msg_buf));
                    HAROLD::User_Box(&MOX::draw_fleet_screen_to_back, cancel_msg_buf, 3);
                    return 0;
                }
            }
        }

        if (num_ships > 0) {
            for (int i = 0; i < num_ships; ++i) {
                int16_t ship_idx = ship_indices[i];
                if (MOX::ship[ship_idx].officer_index > -1) {
                    if (interactive != 0) {
                        char leader_name_buf[200];
                        strlcpy(leader_name_buf, OFFICER::Leader_Name(MOX::ship[ship_idx].officer_index, 1), sizeof(leader_name_buf));

                        char msg_buf[300];
                        snprintf(msg_buf, sizeof(msg_buf), HAROLD::H_Message(126 /* 0x7e */), MOX::ship[ship_idx].d.name, leader_name_buf);

                        if (HAROLD::User_Box(&MOX::draw_fleet_screen_to_back, msg_buf, 1) == 0) {
                            char cancel_msg_buf[200];
                            strlcpy(cancel_msg_buf, HAROLD::H_Message(127 /* 0x7f */), sizeof(cancel_msg_buf));
                            HAROLD::User_Box(&MOX::draw_fleet_screen_to_back, cancel_msg_buf, 3);
                            return 0;
                        }
                    }
                    has_officer = true;
                }
            }
        }

        for (int i = 0; i < num_ships; ++i) {
            int16_t officer_idx = MOX::ship[ship_indices[i]].officer_index;
            if (officer_idx != -1 &&
                !SHIPMOVE::Confirm_Officer_Change(officer_idx, -1, 0)) {
                return 0;
            }
        }

        int32_t scrap_value = Get_Scrap_Ship_Value();

        if (interactive != 0 && scrap_value > 0) {
            int16_t msg_id = (num_ships == 1) ? 128 /* 0x80 */ : 129 /* 0x81 */;

            char msg_buf[300];
            snprintf(msg_buf, sizeof(msg_buf), HAROLD::H_Message(msg_id), scrap_value);

            if (HAROLD::User_Box(&MOX::draw_fleet_screen_to_back, msg_buf, 1) == 0) {
                char cancel_msg_buf[200];
                strlcpy(cancel_msg_buf, HAROLD::H_Message(127 /* 0x7f */), sizeof(cancel_msg_buf));
                HAROLD::User_Box(&MOX::draw_fleet_screen_to_back, cancel_msg_buf, 3);
                return 0;
            }

            if (give_bc) {
                MOX::player[player_idx].bc += scrap_value;
                Plunder_Ships_For_Tech(ship_indices, num_ships, interactive);
            }
        }

        if (has_officer) {
            if (num_ships > 0) {
                for (int i = 0; i < num_ships; ++i) {
                    int16_t ship_idx = ship_indices[i];
                    int16_t officer_idx = MOX::ship[ship_idx].officer_index;
                    if (officer_idx > -1) {
                        MOX::leaders[officer_idx].status = 0;
                        MOX::leaders[officer_idx].location = -1;
                        MOX::leaders[officer_idx].eta = 0;
                        MOX::ship[ship_idx].officer_index = -1;
                    }
                }
            }
        }

        if (num_ships > 0) {
            for (int i = 0; i < num_ships; ++i) {
                SHIPS::Kill_Ship(ship_indices[i]);
            }
        }

        SHIPMOVE::Recalculate_All_Moving_Fleets();
        COLCALC::Do_Player_Colony_Calculations(player_idx);
        return 1;
    }

    void __cdecl Set_Fltscrn_Big_Icons(int16_t select_flag) {
        if (select_flag != 0) {
            if (MOX::n_fltscrn_big_icons > 0) {
                for (int16_t i = 0; i < MOX::n_fltscrn_big_icons; ++i) {
                    MOX::fltscrn_big_icon[i].selected = 1;
                }
            }
        } else {
            if (MOX::n_fltscrn_big_icons > 0) {
                for (int16_t i = 0; i < MOX::n_fltscrn_big_icons; ++i) {
                    MOX::fltscrn_big_icon[i].selected = 0;
                }
            }
        }
    }

    uint8_t __cdecl Okay_To_Set_Relocate_To_Star(int16_t src_star_idx, int16_t dst_star_idx) {
        uint8_t result;
        char message[100];
        const char* fmt;

        result = FLT1::Okay_To_Set_Relocate_Star(dst_star_idx, 1);
        mouse::Set_Mouse_List(FLT1::mouse_list_to, 4);

        if (result != 0) {
            if (struct_::Test_Bit_Field(MOX::star[src_star_idx].black_hole_blocks, dst_star_idx) != 0) {
                fmt = HAROLD::H_Message(0x82 /* 130 */);
                snprintf(message, sizeof(message), fmt, MOX::star[src_star_idx].name, MOX::star[dst_star_idx].name);

                result = 0;
                HAROLD::User_Box(&MOX::draw_fleet_screen_to_back, message, 3);
            }
        }

        return result;
    }

    bool __cdecl Okay_To_Set_Relocate_Star(int16_t star_idx, uint8_t prompt_flag) {
        int16_t guarded_by_race = 0;
        char msg_buf[80];
        char race_name_buf[20];
        char sprintf_buf[120];

        msg_buf[0] = '\0';
        bool ok_to_relocate = true;

        s_star_data* star = &MOX::star[star_idx];

        if (star->spectral_class == STAR_CLASS_BLACK_HOLE) {
            ok_to_relocate = false;
            if (prompt_flag == 0) {
                strlcpy(msg_buf, HAROLD::H_Message(0x84), sizeof(msg_buf));
            } else {
                strlcpy(msg_buf, HAROLD::H_Message(0x83), sizeof(msg_buf));
            }
        } else {
            if (HAROLD::Player_Has_Visited(MOX::PLAYER_NUM, star_idx)) {
                if (HAROLD::Star_Guarded_By_Monster(star_idx, &guarded_by_race)) {
                    HACCESS::Race_Name(guarded_by_race, race_name_buf, sizeof(race_name_buf));
                    mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);

                    if (prompt_flag != 0) {
                        snprintf(sprintf_buf, sizeof(sprintf_buf), HAROLD::H_Message(0x87), star->name, race_name_buf);
                        ok_to_relocate = (bool)HAROLD::User_Box(&MOX::draw_fleet_screen_to_back, sprintf_buf, 1);
                    } else {
                        ok_to_relocate = false;
                    }
                } else {
                    if (prompt_flag == 0 && !HACCESS::Player_Has_Colony_In_System(MOX::PLAYER_NUM, star_idx)) {
                        snprintf(msg_buf, sizeof(msg_buf), HAROLD::H_Message(0x88), star->name);
                        ok_to_relocate = false;
                    }
                }
            } else {
                ok_to_relocate = false;
                if (prompt_flag == 0) {
                    strlcpy(msg_buf, HAROLD::H_Message(0x86), sizeof(msg_buf));
                } else {
                    strlcpy(msg_buf, HAROLD::H_Message(0x85), sizeof(msg_buf));
                }
            }
        }

        if (!ok_to_relocate && msg_buf[0] != '\0') {
            HAROLD::User_Box(&MOX::draw_fleet_screen_to_back, msg_buf, 3);
        }

        return ok_to_relocate;
    }
} // namespace FLT1