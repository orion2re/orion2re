#include "pch.h"
#include "ship_read.h"

namespace FLT2 {
    s_colors normal_colors = {{111, 116, 114, 116, 116, 116, 116, 116}};
    s_colors neutral_colors = {{4, 16, 12, 16, 16, 16, 16, 16}};
    s_colors red_colors = {{75, 129, 78, 129, 129, 129, 129, 129}};

    void __cdecl Get_Small_Icon_Animation_Size(int16_t icon_idx, int16_t* width_out, int16_t* height_out) {
        int16_t node_idx = MOX::ship_icon[icon_idx].node_idx;
        int16_t owner = HACCESS::Ship_Stack_Owner(node_idx);

        s_animation_header* anim;
        if (owner < MAX_PLAYERS) {
            anim = MOX::fltscrn_small_ship_icon_seg[MOX::player[owner].color];
        } else {
            int16_t ship_idx = MOX::ship_node[node_idx].ship_idx;
            int8_t ship_owner = MOX::ship[ship_idx].owner;
            anim = MOX::fltscrn_monster_ship_icon[ship_owner - MAX_PLAYERS];
        }

        *width_out = animate::Get_Width(anim);
        *height_out = animate::Get_Height(anim);
    }

    void __cdecl Add_Fltscrn_Small_Icon_Fields() {
        int16_t width;
        int16_t height;

        for (int16_t i = 0; i < MOX::ship_icon_count; ++i) {
            Get_Small_Icon_Animation_Size(i, &width, &height);

            MOX::ship_icon[i].stack_id = fields::Add_Hidden_Field(
                MOX::ship_icon[i].x,
                MOX::ship_icon[i].y,
                MOX::ship_icon[i].x + width,
                MOX::ship_icon[i].y + height,
                "",
                0x29);
        }
    }

    void __cdecl Move_Ship_To_Front_Of_List(int16_t icon_idx) {
        if (MOX::n_fltscrn_big_icons > 1) {
            s_big_icon first_icon = MOX::fltscrn_big_icon[0];
            MOX::fltscrn_big_icon[0] = MOX::fltscrn_big_icon[icon_idx];
            MOX::fltscrn_big_icon[icon_idx] = first_icon;
        }

        OFFICER::fleet_icon_ship_id_to_move = -1;
    }

    void __cdecl Set_Initial_Stack_Pointer() {
        FLT1::Set_Small_Ship_Stack_Pointer(true);
    }

    void Fleet_Screen_Big_Icon_Fields(int16_t* button_id_ptr, int16_t x, int16_t y, int16_t w, int16_t h) {
        if (MOX::small_ship_stack_ptr > -1 &&
            (MOX::old_small_ship_stack_ptr != MOX::small_ship_stack_ptr ||
                MOX::scroll_bar->first_visible_row != MOX::old_first_fltscrn_icon ||
                MOX::scroll_bar->first_visible_row == -1)) {
            MOX::old_small_ship_stack_ptr = MOX::small_ship_stack_ptr;
            MOX::old_first_fltscrn_icon = MOX::scroll_bar->first_visible_row;
            FLT2::Build_Fltscrn_Big_Icon_Fields(MOX::small_ship_stack_ptr, MOX::scroll_bar->first_visible_row);

            int16_t ship_idx = MOX::ship_stack_start[MOX::small_ship_stack_ptr];
            int16_t owner = HACCESS::Ship_Stack_Owner(ship_idx);
            KEN::Load_Player_Ship_Palette(owner);
        } else {
            if (OFFICER::fleet_icon_ship_id_to_move > -1 && MOX::n_fltscrn_big_icons > 1) {
                bool moved = false;
                for (int16_t i = 0; i < MOX::n_fltscrn_big_icons && !moved; ++i) {
                    if (MOX::fltscrn_big_icon[i].ship_idx == OFFICER::fleet_icon_ship_id_to_move) {
                        FLT2::Move_Ship_To_Front_Of_List(i);
                        moved = true;
                    }
                }
            }
        }

        if (MOX::n_fltscrn_big_icons > 0) {
            MOX::n_fltscrn_rows = (int8_t)(MOX::n_fltscrn_big_icons / MOX::big_icon_display_columns);
            if ((MOX::n_fltscrn_big_icons % MOX::big_icon_display_columns) > 0) {
                MOX::n_fltscrn_rows++;
            }

            if (MOX::scroll_bar->first_visible_row < 0 || MOX::scroll_bar->first_visible_row >= MAX_SHIPS) {
                MOX::scroll_bar->first_visible_row = 0;
            }

            FLT2::Add_Fltscrn_Big_Icon_Fields(MOX::scroll_bar->first_visible_row * MOX::big_icon_display_columns);
        } else {
            MOX::scroll_bar->first_visible_row = 0;
        }

        if (MOX::n_fltscrn_rows <= MOX::scroll_bar->first_visible_row) {
            MOX::scroll_bar->first_visible_row = 0;
        }

        *button_id_ptr = -1000;
        if (MOX::big_icon_display_columns < MOX::n_fltscrn_big_icons) {
            *button_id_ptr = fields::Add_Scroll_Field(x, y, 0, h, 0, h, w, h, &MOX::scroll_bar->thumb_center_y, "", 0);
        }
    }

    void __cdecl Clear_All_Selection_Status() {
        for (int i = 0; i < (int)MOX::n_fltscrn_big_icons; ++i) {
            MOX::fltscrn_big_icon[i].selected = 0;
        }
    }

    void __cdecl Get_Fltscrn_Big_Icon_XY(int16_t icon_idx, int16_t* out_x, int16_t* out_y) {
        // Preserve the original unused screen check.
        (void)(MOX::current_screen == SCREEN_FLEET);

        int16_t columns = MOX::big_icon_display_columns;

        // Keep 16-bit intermediates in the coordinate math.
        int16_t remainder = (int16_t)(icon_idx % columns);
        *out_x = (int16_t)((int16_t)(remainder * 62) + MOX::big_icon_x_offset);

        int16_t quotient = (int16_t)(icon_idx / columns);
        *out_y = (int16_t)((int16_t)(quotient * 60) + MOX::big_icon_y_offset);
    }

    int16_t __cdecl Get_Filter_Button_Status() {
        if (MOX::current_screen == SCREEN_OFFICERS ||
            (MOX::fltscrn_combat_button_status == 1 && MOX::fltscrn_support_button_status == 1)) {
            return 0;
        }

        if (MOX::fltscrn_combat_button_status == 1) {
            return 2;
        }

        if (MOX::fltscrn_support_button_status == 1) {
            return 1;
        }

        return -1;
    }

    int16_t __cdecl Fltscrn_Ships_To_Be_Displayed(int16_t ship_stack_head_node_idx) {
        int16_t total_ships;
        int16_t colony_ships_count;
        int16_t outposts_count;
        int16_t transports_count;
        int16_t ship_sizes[6];

        SYS::N_Ships_By_Type_In_Stack(ship_stack_head_node_idx, &total_ships, &colony_ships_count, &outposts_count, &transports_count, ship_sizes);

        int16_t filter_status = FLT2::Get_Filter_Button_Status();

        if (filter_status == 0) {
            return total_ships;
        }

        if (filter_status == 1) {
            return colony_ships_count + outposts_count + transports_count;
        }

        if (filter_status == 2) {
            return total_ships - (colony_ships_count + outposts_count + transports_count);
        }

        return 0;
    }

    bool __cdecl Ok_To_Add_Ship(int16_t ship_idx) {
        int16_t ship_type = (int8_t)MOX::ship[ship_idx].d.ship_type;
        int16_t filter_status = Get_Filter_Button_Status();

        if (filter_status == 0) {
            return true;
        }

        if (filter_status == 1) {
            if (ship_type == SHIP_TYPE_TRANSPORT || ship_type == SHIP_TYPE_COLONY || ship_type == SHIP_TYPE_OUTPOST) {
                return true;
            }
            return false;
        }

        if (filter_status == 2) {
            return ship_type == SHIP_TYPE_COMBAT;
        }

        return false;
    }

    void __cdecl Fix_Palette(int16_t player_idx) {
        KEN::Load_Player_Ship_Palette(player_idx);

        MOX::skip_draw_ships = 1;

        video::Set_Page_Off();
        FLT1::Draw_Fleet_Screen();
        RUSS::Mox_Sync_Update();
        palette::Refresh_Palette();
    }

    void __cdecl Build_Fltscrn_Big_Icon_Fields(int16_t stack_idx, int16_t first_visible_row) {
        (void)first_visible_row;

        int16_t ship_idx_list[MAX_SHIPS];
        int16_t selected_count = 0;
        int16_t target_icon_idx = -1;
        int16_t next_node_idx = -1;

        MOX::n_fltscrn_big_icons_added = 0;

        memset(ship_idx_list, 0xff, sizeof(ship_idx_list));

        if (stack_idx >= 0 && stack_idx < MOX::ship_stack_count) {
            next_node_idx = MOX::ship_stack_start[stack_idx];
        }

        if (MOX::n_fltscrn_big_icons > 0) {
            for (int16_t i = 0; i < MOX::n_fltscrn_big_icons; ++i) {
                if (MOX::fltscrn_big_icon[i].selected != 0) {
                    ship_idx_list[selected_count] = MOX::fltscrn_big_icon[i].ship_idx;
                    selected_count++;
                }
            }
        }

        memset(MOX::fltscrn_big_icon, 0, SHIP_LIST_CAPACITY * sizeof(*MOX::fltscrn_big_icon));

        for (int i = 0; i < SHIP_LIST_CAPACITY; ++i) {
            MOX::fltscrn_big_icon[i].button_id = -1000;
        }

        MOX::n_fltscrn_big_icons = 0;

        while (next_node_idx != -1 && MOX::n_fltscrn_big_icons < MAX_SHIPS) {
            int16_t ship_idx = MOX::ship_node[next_node_idx].ship_idx;

            if (FLT2::Ok_To_Add_Ship(ship_idx) && MOX::ship[ship_idx].status < 3) {
                MOX::fltscrn_big_icon[MOX::n_fltscrn_big_icons].ship_idx = ship_idx;

                if (OFFICER::fleet_icon_ship_id_to_move != -1 &&
                    OFFICER::fleet_icon_ship_id_to_move == ship_idx) {
                    target_icon_idx = MOX::n_fltscrn_big_icons;
                }
                MOX::n_fltscrn_big_icons++;
            }
            next_node_idx = MOX::ship_node[next_node_idx].next_node;
        }

        if (MOX::n_fltscrn_big_icons > 0) {
            for (int16_t i = 0; i < MOX::n_fltscrn_big_icons; ++i) {
                if (ERIC::Sint_Is_In_List(MOX::fltscrn_big_icon[i].ship_idx, ship_idx_list, selected_count)) {
                    MOX::fltscrn_big_icon[i].selected = 1;
                }
            }
        }

        if (target_icon_idx != -1 && MOX::n_fltscrn_big_icons > 1) {
            FLT2::Move_Ship_To_Front_Of_List(target_icon_idx);
        }

        FLT2::Set_Fleet_Screen_Stack_Owner();
    }

    void __cdecl Set_Fleet_Screen_Stack_Owner() {
        MOX::fltscrn_stack_owner = -1;
        if (MOX::small_ship_stack_ptr > -1) {
            MOX::fltscrn_stack_owner = HACCESS::Ship_Owner(
                MOX::ship_node[MOX::ship_stack_start[MOX::small_ship_stack_ptr]].ship_idx);
        }
    }

    void __cdecl Add_Fltscrn_Big_Icon_Fields(int16_t base_index) {
        int16_t height_offset;
        int16_t width_offset;
        int16_t search_idx;
        int16_t owner;
        int16_t x;
        int16_t y;
        int16_t icon_idx;

        if (MOX::current_screen == SCREEN_FLEET) {
            width_offset = 0x3a;
            height_offset = 0x39;
        } else {
            width_offset = 0x39;
            height_offset = 0x39;
        }

        search_idx = 0;
        while (MOX::fltscrn_big_icon[base_index + search_idx].ship_idx == -1) {
            search_idx++;
        }

        owner = HACCESS::Ship_Owner(MOX::fltscrn_big_icon[base_index + search_idx].ship_idx);
        MOX::n_fltscrn_big_icons_added = 0;

        if (MOX::big_icon_display_max_icons > 0) {
            do {
                if (MOX::n_fltscrn_big_icons_added >= MOX::n_fltscrn_big_icons) {
                    break;
                }
                if (MOX::n_fltscrn_big_icons_added + base_index >= MOX::n_fltscrn_big_icons) {
                    break;
                }

                FLT2::Get_Fltscrn_Big_Icon_XY(MOX::n_fltscrn_big_icons_added, &x, &y);

                icon_idx = base_index + MOX::n_fltscrn_big_icons_added;
                MOX::fltscrn_big_icon[icon_idx].screen_x = x;
                MOX::fltscrn_big_icon[icon_idx].screen_y = y;

                if (MOX::PLAYER_NUM == owner) {
                    MOX::fltscrn_big_icon[icon_idx].button_id = fields::Add_Hidden_Field(
                        x,
                        y,
                        x + width_offset,
                        y + height_offset,
                        "",
                        0x29);
                } else {
                    MOX::fltscrn_big_icon[icon_idx].button_id = -1000;
                }

                MOX::n_fltscrn_big_icons_added++;

            } while (MOX::n_fltscrn_big_icons_added < MOX::big_icon_display_max_icons);
        }
    }

    void __cdecl Print_Fltscrn_Scanned_Star_Name() {
        char status_text[80];
        s_colors status_colors;
        int16_t referenced_star_idx = 0;
        uint8_t visited;
        uint8_t has_moving_ships = 0;
        int16_t status = -1;
        int16_t owner = -1;
        int16_t val = 0;

        visited = HAROLD::Player_Has_Visited(MOX::PLAYER_NUM, MOX::galaxy_map_scanned_star);

        for (int i = 0; i < SHIP_LIST_CAPACITY; ++i) {
            MOX::fltscrn_ship_icon_selection_list[i] = -1;
        }

        status_colors = normal_colors;

        if (FLT1::relocate_button_mode != 1 &&
            Build_Fltscrn_Ship_List(MOX::fltscrn_ship_icon_selection_list) != 0) {
            has_moving_ships = 1;
            SHIPMOVE::Ships_Try_To_Move_To(
                MOX::fltscrn_ship_icon_selection_list,
                MOX::galaxy_map_scanned_star,
                &MOX::g_ship_move_info);
        } else {
            MOX::g_ship_move_info.blackhole_blocks = 0;
            MOX::g_ship_move_info.insufficient_fuel = 0;
            MOX::g_ship_move_info.immobile = 0;
            MOX::g_ship_move_info.pad = 0;
            MOX::g_ship_move_info.turns_left = 0;
            MOX::g_ship_move_info.moving = 0;
            MOX::g_ship_move_info.out_of_range = 0;
            MOX::g_ship_move_info.has_navigator = 0;
            MOX::g_ship_move_info.traveling_speed = 0;
            MOX::g_ship_move_info.parsecs = 0;
            MOX::g_ship_move_info.uses_wormhole = 0;
            MOX::g_ship_move_info.has_jumpgate_route = 0;
            MOX::g_ship_move_info.has_stargate_route = 0;
            MOX::g_ship_move_info.hyperspace_flux = 0;

            if (visited == 0 &&
                MOX::player[MOX::PLAYER_NUM].traits[TRAIT_OMNISCIENCE] == 0 &&
                OFFICER::One_Leader_With_Galactic_Lore(MOX::PLAYER_NUM) == 0) {
                if (HAROLD::Contact_With_One_Colony(MOX::galaxy_map_scanned_star) == 0) {
                    status = 8;
                } else {
                    status = 7;
                    val = MOX::galaxy_map_scanned_star;
                }
            } else {
                status = 7;
                val = MOX::galaxy_map_scanned_star;
            }
        }

        if (MOX::g_ship_move_info.blackhole_blocks != 0) {
            status = 3;
        } else if (MOX::g_ship_move_info.immobile != 0) {
            status = 4;
        } else if (MOX::g_ship_move_info.out_of_range != 0) {
            if (visited != 0) {
                status = 5;
                val = MOX::g_ship_move_info.parsecs;
                referenced_star_idx = MOX::galaxy_map_scanned_star;
            } else {
                status = 6;
                val = MOX::g_ship_move_info.parsecs;
            }
        } else if (MOX::g_ship_move_info.hyperspace_flux != 0) {
            status = 9;
        } else if (MOX::g_ship_move_info.moving != 0) {
            if (MOX::star[MOX::galaxy_map_scanned_star].spectral_class == STAR_CLASS_BLACK_HOLE) {
                status = 3;
            } else {
                owner = MOX::star[MOX::galaxy_map_scanned_star].owner;
                if (MOX::g_ship_move_info.turns_left == 0) {
                    status = 0;
                    val = MOX::galaxy_map_scanned_star;
                } else {
                    if (visited != 0) {
                        status = 1;
                        val = MOX::g_ship_move_info.turns_left;
                        referenced_star_idx = MOX::galaxy_map_scanned_star;
                    } else {
                        status = 2;
                        val = MOX::g_ship_move_info.turns_left;
                    }
                }
            }
        }

        switch (status) {
            case 0:
                snprintf(status_text, sizeof(status_text), HAROLD::H_Message(0x67), MOX::star[val].name);
                status_colors = normal_colors;
                break;
            case 1:
                if (val == 1) {
                    snprintf(status_text, sizeof(status_text), HAROLD::H_Message(0x69), val, MOX::star[referenced_star_idx].name);
                } else {
                    snprintf(status_text, sizeof(status_text), HAROLD::H_Message(0x6A), val, MOX::star[referenced_star_idx].name);
                }
                status_colors = normal_colors;
                break;
            case 2:
                if (val == 1) {
                    snprintf(status_text, sizeof(status_text), HAROLD::H_Message(0x6B), val);
                } else {
                    snprintf(status_text, sizeof(status_text), HAROLD::H_Message(0x6C), val);
                }
                status_colors = normal_colors;
                break;
            case 3:
                strlcpy(status_text, HAROLD::H_Message(0x21), sizeof(status_text));
                status_colors = neutral_colors;
                break;
            case 4:
                strlcpy(status_text, HAROLD::H_Message(0x6E), sizeof(status_text));
                status_colors = neutral_colors;
                break;
            case 5:
                if (val == 1) {
                    snprintf(status_text, sizeof(status_text), HAROLD::H_Message(0x6F), val, MOX::star[referenced_star_idx].name);
                } else {
                    snprintf(status_text, sizeof(status_text), HAROLD::H_Message(0x70), val, MOX::star[referenced_star_idx].name);
                }
                status_colors = neutral_colors;
                break;
            case 6:
                snprintf(status_text, sizeof(status_text), HAROLD::H_Message(0xEB), val);
                status_colors = neutral_colors;
                break;
            case 7:
                owner = MOX::star[MOX::galaxy_map_scanned_star].owner;
                if (owner < 0 || owner >= MAX_PLAYERS) {
                    owner = -1;
                    status_colors = neutral_colors;
                }
                {
                    int8_t leader_idx = (int8_t)MOX::star[val].officer_index[MOX::PLAYER_NUM];
                    if (leader_idx == -1 || HAROLD::Player_Has_Colony(MOX::PLAYER_NUM, val) == 0) {
                        strlcpy(status_text, MOX::star[val].name, sizeof(status_text));
                    } else {
                        s_leader_data* leader = &MOX::leaders[leader_idx];
                        if (leader->eta <= 0) {
                            snprintf(status_text, sizeof(status_text), MOX::s_fmt_scanned_star_info, MOX::star[val].name, leader->name);
                        } else if (leader->eta == 1) {
                            snprintf(status_text, sizeof(status_text), HAROLD::H_Message(0x92), MOX::star[val].name, leader->name, leader->eta);
                        } else {
                            snprintf(status_text, sizeof(status_text), HAROLD::H_Message(0x93), MOX::star[val].name, leader->name, leader->eta);
                        }
                    }
                }
                break;
            case 8:
                strlcpy(status_text, HAROLD::H_Message(0x94), sizeof(status_text));
                status_colors = neutral_colors;
                break;
            case 9:
                strlcpy(status_text, HAROLD::H_Message(0x73), sizeof(status_text));
                status_colors = neutral_colors;
                break;
            default:
                break;
        }

        MISC::Set_Fitted_Font_Style(status_text, &status_colors, 3, 2, 0, 0xBD, 1);

        if (owner != -1 && has_moving_ships == 0) {
            uint8_t color_idx = MOX::main_palette_player_colors[(uint8_t)MOX::player[owner].color];
            if (color_idx == 0x20) {
                color_idx = 0xF;
            }
            s_colors* temp_colors = MISC::Font_Colors2(fonts::current_font_style, color_idx, color_idx + 6);
            status_colors = *temp_colors;
            MISC::Set_Font_Colors(&status_colors);
        }

        int16_t str_w = fonts::Get_String_Width(status_text);
        int16_t h = fonts::Get_Font_Height();

        fonts::Print(169 - (str_w / 2), ((23 - h) / 2) + 248, status_text);
    }

    void __cdecl Print_Scanned_Ship_Data(int16_t ship_idx) {
        int16_t defense_bonus;
        int16_t missile_defense_bonus;
        int16_t attack_bonus;
        char str_9a[20];
        char buffer[120];
        char unused_msg_95[20];
        char unused_msg_96[20];
        char unused_msg_97[20];
        char unused_msg_98[20];

        uint8_t ship_type = MOX::ship[ship_idx].d.ship_type;
        bool is_ship_type_2 = (ship_type == SHIP_TYPE_TRANSPORT);
        bool is_ship_type_1 = (ship_type == SHIP_TYPE_COLONY);
        bool is_ship_type_4 = (ship_type == SHIP_TYPE_OUTPOST);

        // Preserve the original message loads even though the copied strings are not reused here.
        strlcpy(unused_msg_95, HAROLD::H_Message(0x95), sizeof(unused_msg_95));
        strlcpy(unused_msg_96, HAROLD::H_Message(0x96), sizeof(unused_msg_96));
        strlcpy(unused_msg_97, HAROLD::H_Message(0x97), sizeof(unused_msg_97));
        strlcpy(unused_msg_98, HAROLD::H_Message(0x98), sizeof(unused_msg_98));

        INITSHIP::Get_Ship_Combat_Bonuses(ship_idx, &defense_bonus, &missile_defense_bonus, &attack_bonus);

        if (is_ship_type_2 || is_ship_type_1 || is_ship_type_4) {
            allocate::Push_Block(MOX::screen_seg);
            void* help_data_block = allocate::Allocate_Next_Block(MOX::screen_seg, 10000);
            allocate::Allocate_Next_Block(MOX::screen_seg, sizeof(s_help_record));

            int16_t help_idx = 0;
            if (ship_type == SHIP_TYPE_COLONY) {
                help_idx = 0x29;
            } else if (ship_type == SHIP_TYPE_TRANSPORT) {
                help_idx = 0xBD;
            } else if (ship_type == SHIP_TYPE_OUTPOST) {
                help_idx = 0x6D;
            }

            char lbx_name[20];
            TEXTBOX::Get_Help_Lbx_Name(lbx_name, sizeof(lbx_name));

            s_help_record* lbx_data = (s_help_record*)farload::Far_Reload_Next_Data(lbx_name, 0, help_data_block, help_idx, 1, sizeof(s_help_record));
            MOX::mox_font_colors_offset = 0x72;

            s_colors temp_colors;
            MISC::Get_Mox_Font_Colors(3, 111, 116, &temp_colors);

            fonts::Set_Font_Style(3, &temp_colors);
            ERIC::Print_Paragraph_Centered_Vertically(0x12, 0x11A, 0x12B, 0xB7, lbx_data->body, 0);

            allocate::Pop_Block(MOX::screen_seg);
            return;
        }

        bool has_captain = FLT2::Draw_Captains_Picture(ship_idx) != 0;

        fonts::Set_Font_Style(3, &FLT2::normal_colors);
        fonts::Print(0x12, 0x11F, MOX::ship[ship_idx].d.name);

        fonts::Set_Font_Style(2, &FLT2::normal_colors);
        int16_t y_cursor = fonts::Get_Font_Height() + 2;

        fonts::Set_Font_Style(2, &FLT2::normal_colors);
        fonts::Set_Font_Spacing(1);

        char* crew_desc = FLT2::Crew_Description_String(ship_idx);
        const uint16_t exp = MOX::ship[ship_idx].crew_experience;
        ship_read::Format_Fleet_Crew_Text(crew_desc, exp, buffer, sizeof(buffer));

        fonts::Get_String_Width(buffer);
        fonts::Print(0x12, y_cursor + 0x11F, buffer);

        y_cursor = fonts::Get_Font_Height() + y_cursor + 2;

        int16_t shield_idx = MOX::ship[ship_idx].d.shield_type;
        fonts::Print(0x12, y_cursor + 0x11F, TECHDATA::shields[shield_idx].name);

        int16_t fh = fonts::Get_Font_Height();
        int16_t EBP_y = fh + y_cursor + 0x121;
        y_cursor = fh + y_cursor + 2;

        fonts::Print(0x12, EBP_y, HAROLD::H_Message(0x99));

        const char* sign1 = (attack_bonus >= 0) ? "+" : "";
        snprintf(buffer, sizeof(buffer), "%s%d", sign1, attack_bonus);
        fonts::Print_Right(0x85, EBP_y, buffer);

        strlcpy(str_9a, HAROLD::H_Message(0x9A), sizeof(str_9a));

        int16_t w;
        if (!has_captain) {
            w = fonts::Get_String_Width(str_9a);
            if (w + 0x9B < 0x10E) {
                w = 0x73;
            }
            fonts::Print(0xAD, EBP_y, str_9a);

            const char* sign2 = (defense_bonus >= 0) ? "+" : "";
            snprintf(buffer, sizeof(buffer), "%s%d", sign2, defense_bonus);
            fonts::Print(w + 0xAD, EBP_y, buffer);
        } else {
            w = fonts::Get_String_Width(str_9a);

            fh = fonts::Get_Font_Height();
            EBP_y = fh + y_cursor + 0x121;
            y_cursor = fh + y_cursor + 2;

            fonts::Print(0x12, EBP_y, str_9a);

            const char* sign2 = (defense_bonus >= 0) ? "+" : "";

            snprintf(buffer, sizeof(buffer), "%s%d", sign2, defense_bonus);
            if (w < 0x73) {
                w = 0x73;
            }
            w += 0x12;
            fonts::Print(w, EBP_y, buffer);
        }

        y_cursor = fonts::Get_Font_Height() + y_cursor + 2;

        int16_t loc = MOX::ship[ship_idx].location;
        if (loc >= SHIP_LOCATION_MOVING_OFFSET && loc <= MOX::NUM_STARS + SHIP_LOCATION_WORMHOLE_OFFSET) {
            int16_t star_idx = HAROLD::Absolute_Location(loc);
            bool visited = HAROLD::Player_Has_Visited(MOX::PLAYER_NUM, star_idx);

            s_colors temp_colors;
            for (int i = 0; i < 8; ++i) {
                temp_colors.colors[i] = FLT2::normal_colors.colors[i] + 3;
            }
            fonts::Set_Font_Style(2, &temp_colors);

            bool has_info = false;
            if (visited || MOX::player[MOX::PLAYER_NUM].traits[TRAIT_OMNISCIENCE] != 0 || OFFICER::One_Leader_With_Galactic_Lore(MOX::PLAYER_NUM)) {
                has_info = true;
            } else {
                has_info = HAROLD::Contact_With_One_Colony(star_idx);
            }

            const char* loc_str;
            if (MOX::NUM_STARS == star_idx) {
                loc_str = HAROLD::H_Message(0x68);
            } else {
                if (!has_info) {
                    loc_str = HAROLD::H_Message(0x9C);
                } else {
                    snprintf(buffer, sizeof(buffer), (const char*)HAROLD::H_Message(0x9B), MOX::star[star_idx].name);
                    fonts::Get_String_Width(buffer);
                    loc_str = buffer;
                }
            }
            fonts::Print(0x12, y_cursor + 0x11F, loc_str);
            fonts::Set_Font_Style(2, &FLT2::normal_colors);
        }

        fh = fonts::Get_Font_Height();
        int16_t ESI_y = fh + y_cursor + 0x121;
        y_cursor = fh + y_cursor + 2;

        fonts::Print(0x12, ESI_y, HAROLD::H_Message(0x9D));
        fonts::Print(0xAD, ESI_y, HAROLD::H_Message(0x9E));

        int16_t base_y = fonts::Get_Font_Height() + y_cursor + 1;
        int16_t wep_y = base_y;
        int16_t spec_y = base_y;

        fonts::Set_Font_Style(1, &FLT2::normal_colors);
        fonts::Set_Font_Spacing(1);

        bool no_weapons = false;
        int16_t weapons_printed = 0;

        for (int i = 0; i < 8; ++i) {
            if (no_weapons) {
                break;
            }
            if (MOX::ship[ship_idx].d.ship_weapon[i].type < 0 || MOX::ship[ship_idx].d.ship_weapon[i].count < 1) {
                no_weapons = true;
            } else {
                char arc_str[20];
                DESIGN::Weapon_Arc_String(MOX::ship[ship_idx].d.ship_weapon[i].firing_arc, arc_str, sizeof(arc_str));
                int16_t wep_type = MOX::ship[ship_idx].d.ship_weapon[i].type;
                int8_t count = MOX::ship[ship_idx].d.ship_weapon[i].count;

                const char* wep_name;
                if (count == 1) {
                    wep_name = TECHDATA::weapons[wep_type].name;
                } else {
                    wep_name = TECHDATA::weapons[wep_type].name_plural;
                }

                snprintf(buffer, sizeof(buffer), "%d %s (%s)", count, wep_name, arc_str);
                fonts::Print(0x17, wep_y + 0x11F, buffer);
                wep_y += fonts::Get_Font_Height() + 1;
                weapons_printed++;
            }
        }

        if (weapons_printed == 0) {
            fonts::Print(0x17, wep_y + 0x11F, HAROLD::H_Message(0x9F));
        }

        int16_t specials_printed = 0;

        for (int i = 0; i < 39; ++i) {
            if (struct_::Test_Bit_Field(MOX::ship[ship_idx].d.special_device_flags, i) != 0) {
                s_colors* colors;
                if (struct_::Test_Bit_Field(MOX::ship[ship_idx].special_device_damage_flags, i) == 0) {
                    colors = &FLT2::normal_colors;
                } else {
                    colors = &FLT2::red_colors;
                }
                fonts::Set_Font_Style(1, colors);
                fonts::Set_Font_Spacing(1);
                fonts::Print(0xBC, spec_y + 0x11F, TECHDATA::specials[i].name);
                spec_y += fonts::Get_Font_Height() + 1;
                specials_printed++;
            }
        }

        if (specials_printed == 0) {
            fonts::Print(0xBC, spec_y + 0x11F, HAROLD::H_Message(0x9F));
        }
    }

    char* __cdecl Crew_Description_String(int16_t ship_idx) {
        ship_read::Crew_Text(MOX::ship[ship_idx].crew_quality, MOX::temp_string, sizeof(MOX::temp_string));
        return MOX::temp_string;
    }

    void __cdecl Fltscrn_Move_Ships(int16_t target_star) {
        char msg_buf[200];
        char msg_buf2[88];

        memset(MOX::fltscrn_ship_icon_selection_list, 0xFF, SHIP_LIST_CAPACITY * sizeof(*MOX::fltscrn_ship_icon_selection_list));

        if (Build_Fltscrn_Ship_List(MOX::fltscrn_ship_icon_selection_list) != 0) {
            SHIPMOVE::Ships_Try_To_Move_To(MOX::fltscrn_ship_icon_selection_list, target_star, &MOX::g_ship_move_info);

            if (MOX::g_ship_move_info.moving != 0) {
                if (!SHIPMOVE::Apply_Player_Movement_Order(
                        MOX::fltscrn_ship_icon_selection_list,
                        target_star,
                        &MOX::g_ship_move_info)) {
                    return;
                }
                SHIPS::Build_Ship_Icons();
                FLT::Set_Fltscrn_Small_Ship_Icon_XYs(0xF, 0x34, 0x131, 0xB6);
                MOX::draw_fleet_screen_to_back = 1;
                MOX::old_small_ship_stack_ptr = -1;
                FLT1::Set_Fltscrn_Big_Icons(0);
                return;
            }

            if (MOX::g_ship_move_info.blackhole_blocks != 0) {
                strlcpy(msg_buf2, HAROLD::H_Message(0x21), sizeof(msg_buf2));
                snprintf(msg_buf, sizeof(msg_buf), "%s to %s", msg_buf2, MOX::star[target_star].name);
                HAROLD::User_Box(&MOX::draw_fleet_screen_to_back, msg_buf, 3);
                return;
            }

            if (MOX::g_ship_move_info.out_of_range != 0) {
                if (HAROLD::Player_Has_Visited(MOX::PLAYER_NUM, target_star) ||
                    HAROLD::Player_Is_Omniscient(MOX::PLAYER_NUM)) {
                    snprintf(msg_buf, sizeof(msg_buf), HAROLD::H_Message(0x90), MOX::star[target_star].name);
                } else {
                    strlcpy(msg_buf, HAROLD::H_Message(0x91), sizeof(msg_buf));
                }
                HAROLD::User_Box(&MOX::draw_fleet_screen_to_back, msg_buf, 3);
                return;
            }

            if (MOX::g_ship_move_info.hyperspace_flux != 0) {
                strlcpy(msg_buf, HAROLD::H_Message(0x24), sizeof(msg_buf));
                HAROLD::User_Box(&MOX::draw_fleet_screen_to_back, msg_buf, 3);
            }
        }
    }

    uint8_t __cdecl Draw_Captains_Picture(int16_t ship_idx) {
        int16_t leader_idx = 0;
        s_colors highlight_colors = {{0x4C, 0x4D, 0x4E, 0x4F, 0x3F, 0x40, 0x41, 0x42}};
        char captain_label[80];

        int16_t portrait_idx = 0;
        uint8_t found_captain = 0;

        for (int16_t i = 0; i < MAX_LEADERS; i++) {
            if (MOX::leaders[i].player_index == MOX::PLAYER_NUM &&
                MOX::leaders[i].status == 1 &&
                MOX::leaders[i].type == LEADER_TYPE_SHIP) {
                if (MOX::leaders[i].location == ship_idx) {
                    leader_idx = i;
                    found_captain = 1;
                    break;
                }
                portrait_idx++;
            }
        }

        if (found_captain) {
            s_animation_header* pict = MOX::temp_officers_pict[portrait_idx];
            int16_t width = animate::Get_Width(pict);
            int16_t height = animate::Get_Height(pict);

            char* leader_name = OFFICER::Leader_Name(leader_idx, 1);
            snprintf(captain_label, sizeof(captain_label), "%s", leader_name);

            fonts::Set_Font_Style(0, &normal_colors);
            ERIC::Print_Paragraph_Centered_Vertically(0xAD, 0x11F, 0x42, height, captain_label, 2);
            animate::Draw(0xF2, 0x11C, pict);

            if (MOX::leaders[leader_idx].eta > 0) {
                OFFICER::Print_ETA_On_Officer_Picture(leader_idx, 0xF2, 0x11C, width);
                width += 2;
                height += 2;
                MISC::Draw_Highlight_Box(0xF0, 0x11A, width, height, &highlight_colors.colors[0]);
            } else {
                width += 2;
                height += 2;
                MISC::Draw_Highlight_Box(0xF0, 0x11A, width, height, &highlight_colors.colors[4]);
            }
        }

        return found_captain;
    }

    int16_t __cdecl Build_Fltscrn_Ship_List(int16_t* ship_list) {
        int16_t retval = 1;
        int16_t error_flag = 0;
        int16_t count = 0;

        if (MOX::n_fltscrn_big_icons > 0) {
            for (int16_t i = 0; i < MOX::n_fltscrn_big_icons; i++) {
                if (error_flag != 0) {
                    break;
                }
                if (MOX::fltscrn_big_icon[i].selected != 0) {
                    ship_list[count] = MOX::fltscrn_big_icon[i].ship_idx;
                    count++;
                    if (count >= MAX_SHIPS) {
                        HAROLD::User_Box(&MOX::draw_fleet_screen_to_back, "Too many ships in list", 0);
                        error_flag = 1;
                        retval = 0;
                    }
                }
            }
        }

        if (count == 0) {
            retval = 0;
        }

        return retval;
    }

    int16_t __cdecl Scan_Fltscrn_Big_Icons(int16_t btn_id1, int16_t btn_id2, int16_t* out_idx1, int16_t* out_idx2) {
        int16_t first_icon = MOX::scroll_bar->first_visible_row * MOX::big_icon_display_columns;
        int16_t last_icon = first_icon + 0x14;

        if (last_icon >= MOX::n_fltscrn_big_icons) {
            last_icon = MOX::n_fltscrn_big_icons;
        }

        bool stop_flag = false;
        *out_idx2 = -1;
        *out_idx1 = -1;
        int16_t result = -1;

        if (first_icon < 0 || last_icon < 0) {
            stop_flag = true;
        }

        for (int16_t i = first_icon; i < last_icon; ++i) {
            if (stop_flag) {
                break;
            }

            if (btn_id2 == MOX::fltscrn_big_icon[i].button_id) {
                result = 0;
                stop_flag = true;
                *out_idx2 = i;
                *out_idx1 = i;
            } else if (btn_id2 + MOX::fltscrn_big_icon[i].button_id == 0) {
                result = 1;
                stop_flag = true;
                *out_idx2 = i;
                *out_idx1 = i;
            }
        }

        if (result == -1) {
            for (int16_t i = first_icon; i < last_icon; ++i) {
                if (stop_flag) {
                    break;
                }

                if (MOX::fltscrn_big_icon[i].button_id == btn_id1) {
                    result = 4;
                    *out_idx1 = i;
                }
            }
        }

        return result;
    }

} // namespace FLT2