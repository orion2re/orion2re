#include "pch.h"

namespace MAINSCR2 {
    s_mouse_list mouse_list_from[5] = {
        {17, 0, 0, 0, 639, 479},
        {1, 0, 0, 21, 0, 479},
        {1, 0, 0, 0, 639, 21},
        {1, 0, 527, 0, 639, 479},
        {1, 0, 11, 434, 617, 479},
    };
    s_mouse_list mouse_list_to[5] = {
        {18, 0, 0, 0, 639, 479},
        {1, 0, 0, 21, 0, 479},
        {1, 0, 0, 0, 639, 21},
        {1, 0, 527, 0, 639, 479},
        {1, 0, 11, 434, 617, 479},
    };
    uint8_t main_screen_fade_in_flag;
    uint8_t force_main_screen_fade;
    uint8_t in_reports_phase;

    void Show_Command_Points_Screen() {
        fields::Deactivate_Help_List();
        fields::Assign_Auto_Function(MAINSCR::Draw_Mini_Main_Screen, 1);
        fields::Clear_Fields();

        // Screen bounds are 640x480 (0..639, 0..479)
        fields::Add_Hidden_Field(0, 0, 639, 479, "\x1B", 41);

        fields::Disable_Draw_All_Fields();
        video::Set_Page_Off();
        graphics::Fill(0, 0, 639, 479, 0);

        MAINSCR::Draw_Mini_Main_Screen();
        RUSS::Mox_Sync_Update();
        fields::Enable_Draw_All_Fields();

        COLCALC::Show_Command_Points(MOX::local_player);

        MOX::current_screen = MOX::return_screen;
    }

    void __cdecl Draw_Main_Screen_For_Star_Distan() {
        int16_t star_idx = MOX::screen_mouse_list_type;
        const auto previous_star = MOX::screen_handoff_star;
        s_colors distance_outline_colors = {{0x70, 0x72, 0x71, 0x72, 0x72, 0x72, 0x72, 0x72}};

        MAINSCR::Draw_Mini_Main_Screen();
        MAINSCR::Draw_Main_Main_Screen();
        graphics::Set_Window(22, 22, 527, 421);
        video::Clip_On();

        if (star_idx > -1) {
            MAINSCR::Highlight_Star(star_idx);
            bool found = false;

            if (std::ssize(game::current_state.stars) > 0) {
                for (int16_t i = 0; i < std::ssize(game::current_state.star_order); ++i) {
                    const auto star_id = game::current_state.star_order[i];
                    if (found) {
                        break;
                    }

                    if (MOX::star_button[i] == MOX::scanned_field && i != star_idx) {
                        found = true;
                        MOX::screen_handoff_star = star_id;
                    }

                    const auto target = game::Star_Row(game::current_state, MOX::screen_handoff_star);
                    if (target >= 0 && target != star_idx) {
                        if (previous_star == MOX::screen_handoff_star) {
                            int16_t x1, y1;
                            int16_t x2, y2;

                            MAINSCR::Get_Star_Draw_Coords(star_idx, &x1, &y1);
                            MAINSCR::Get_Star_Draw_Coords(target, &x2, &y2);

                            int32_t distance = GEO::Parsecs_Between_Stars(star_idx, target);

                            char distance_text[80];
                            fonts::Set_Outline_Color(0, 0);
                            fonts::Set_Font_Style_Outline(2, &distance_outline_colors);
                            snprintf(distance_text, sizeof(distance_text), HAROLD::H_Message(235), distance);

                            int16_t width = fonts::Get_String_Width(distance_text);
                            if (x2 + width + 10 <= 522) {
                                fonts::Print(x2 + 10, y2, distance_text);
                            } else {
                                fonts::Print_Right(522, y2, distance_text);
                            }

                            SHIPS::Draw_Directional_Multi_Colored_Line(
                                x1, y1, x2, y2, &MAINSCR::ship_direction_line_green_colors, MOX::multi_colored_line_start);
                        }
                    }
                }
            }
        }

        MOX::multi_colored_line_start = (MOX::multi_colored_line_start + 1) % 8;

        video::Clip_Off();
        graphics::Reset_Window();
    }

    void __cdecl Reports_Screen() {
        int8_t report_mode;
        bool should_fade = false;

        MOX::current_screen = SCREEN_MAIN;
        report_mode = REPORT::Must_Display_Reports(MOX::local_player);

        if (report_mode != 0) {
            mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);
        }

        video::Set_Refresh_Stencil();
        fields::Assign_Auto_Function(MAINSCR::Draw_Mini_Main_Screen, 1);

        if (MOX::stardate_at_beg_of_turn == 0) {
            if (report_mode == 2 || report_mode == 3 || report_mode == 5 || report_mode == 6 ||
                (report_mode == 7 && MOX::settings.show_gnn_report)) {
                should_fade = false;
            } else {
                should_fade = true;
            }
        } else {
            if (MOX::previous_report == 3 && report_mode != 3) {
                should_fade = true;
            }
        }

        if (report_mode == 3) {
            if ((int8_t)MOX::previous_screen != SCREEN_MAIN_MENU) {
                should_fade = false;
            } else if (MOX::at_least_one_tactical_combat != 0 && report_mode != 0) {
                should_fade = true;
            }
        } else if (MOX::at_least_one_tactical_combat != 0 && report_mode != 0) {
            should_fade = true;
        }

        if (should_fade && MAINSCR::already_faded_into_main_screen == 0) {
            MOX::at_least_one_tactical_combat = 0;
            MAINSCR::Allocate_First_Screen_Seg(5);
            fields::Clear_Fields();
            MAINSCR::Clear_Main_Screen_Fields();
            MAINSCR::Update_Star_Owners();
            SHIPS::Build_Ship_Icons();
            ERIC::Fast_Fade_Out();
            MAINSCR::Add_Map_Fields();
            MAINSCR::Fade_Into_Main_Screen();
        }

        MAINSCR2::Do_Begin_Of_Turn();

        if (MOX::current_screen == SCREEN_COLONY) {
            MOX::return_screen = SCREEN_REPORTS;
        }

        if (MOX::at_least_one_report != 0) {
            MAINSCR2::main_screen_fade_in_flag = 1;
            SYS::Clear_System_Display_Loaded_Flag();
        } else {
            MAINSCR2::main_screen_fade_in_flag = 0;
        }

        if (MOVEBOX::Moveable_Box_Selected(2) != 0 && MOX::at_least_one_report != 0) {
            MAINSCR2::force_main_screen_fade = 1;
            MOX::reloaded_ship_palette = 1;
        }

        if (report_mode == 0) {
            MOX::stardate_at_beg_of_turn = MOX::stardate;
        }

        fields::Deactivate_Auto_Function();
    }

    void __cdecl Distance_Between_Stars_Screen() {
        fields::Clear_Fields();
        MAINSCR::Clear_Main_Screen_Fields();

        graphics::Set_Window(22, 22, 527, 421);
        video::Clip_On();
        MAINSCR::Add_Star_Fields();
        video::Clip_Off();
        graphics::Reset_Window();

        int16_t hotkey = fields::Add_Hot_Key("\x1B");
        int16_t starmap_field = fields::Add_Hidden_Field(22, 22, 506, 400, "", 41);

        bool done = false;
        int16_t full_screen_field = fields::Add_Hidden_Field(0, 0, 639, 479, "", 0);

        fields::Assign_Auto_Function(Draw_Main_Screen_For_Star_Distan, 2);

        MOX::screen_mouse_list_type = -1;
        MOX::screen_handoff_star = game::NO_STAR;

        video::Set_Page_Off();
        MAINSCR::Draw_Main_Screen_Filled(0);
        palette::Set_Palette_Changes(0, 255);
        remap::Update_Glass_Remap_Colors();

        MISC::_Darken_Fill(0, 0, 21, 479);
        MISC::_Darken_Fill(0, 0, 639, 21);
        MISC::_Darken_Fill(527, 0, 639, 479);
        MISC::_Darken_Fill(21, 421, 527, 479);

        video::Copy_Off_To_Back();

        do {
            timer::Mark_Time();

            int16_t input = fields::Get_Input();
            MOX::scanned_field = fields::Scan_Input();

            if (MOX::screen_mouse_list_type == -1) {
                mouse::Set_Mouse_List(mouse_list_from, sizeof(mouse_list_from) / sizeof(mouse_list_from[0]));
            } else {
                mouse::Set_Mouse_List(mouse_list_to, sizeof(mouse_list_to) / sizeof(mouse_list_to[0]));
            }

            for (int16_t i = 0; i < std::ssize(game::current_state.stars); ++i) {
                int16_t btn = MOX::star_button[i];
                if (input == btn) {
                    MOX::screen_mouse_list_type = i;
                } else if (input + btn == 0) {
                    done = true;
                }
            }

            if (input == full_screen_field ||
                input + full_screen_field == 0 ||
                input + starmap_field == 0 ||
                input == -1009 ||
                input == hotkey) {
                done = true;
            }

            if (!done) {
                video::Set_Page_Off();
                Draw_Main_Screen_For_Star_Distan();
                RUSS::Mox_Sync_Update();
            }

            timer::Release_Time(1);
        } while (!done);

        mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);
        MOX::current_screen = SCREEN_MAIN;
    }

    void __cdecl Scrap_Freighters() {
        char message_text[200];

        int16_t selected_box = MOVEBOX::Moveable_Box_Selected(2);
        int16_t scrap_amount = SETTLER::N_Freighters_Player_Can_Scrap(MOX::local_player);

        if (scrap_amount <= 0) {
            strlcpy(message_text, HAROLD::H_Message(HESTR_0EC_ALL_FREIGHTERS_ARE_CUR), sizeof(message_text));
            HAROLD::User_Box(&MOX::draw_main_screen_to_back, message_text, 3);
        } else {
            fields::Assign_Auto_Function(MAINSCR::Draw_Mini_Main_Screen, 1);
            EVANHELP::Set_Scrap_Freighters_Help_List();
            fields::Clear_Fields();
            MOX::moveable_box[2].id = 0;
            fields::Disable_Draw_All_Fields();
            video::Set_Page_Off();
            graphics::Fill(0, 0, 0x27f, 0x1df, 0);
            MAINSCR::Draw_Mini_Main_Screen();
            RUSS::Mox_Sync_Update();
            fields::Enable_Draw_All_Fields();

            int16_t count = MAINPUPS::Slider_Popup(0, scrap_amount, 0, (char*)HAROLD::H_Message(0xed), 0);
            if (count >= 0) {
                s_player* plr = &game::current_state.players.at(MOX::local_player.id);
                int16_t box_result = 1;

                if (plr->n_freighters <= count || plr->surplus_freighters <= count) {
                    const char* str1 = HAROLD::H_Message((count == 1) ? 0xf0 /* HESTR_0F0_FREIGHTER */ : 0xf1 /* HESTR_0F1_FREIGHTERS */);
                    const char* str2 = HAROLD::H_Message((plr->surplus_freighters == 1) ? 0xf0 : 0xf1);
                    snprintf(message_text, sizeof(message_text), HAROLD::H_Message(0xee), plr->surplus_freighters, str2, count, str1);
                    box_result = HAROLD::User_Box(&MOX::draw_main_screen_to_back, message_text, 1);
                }

                if (box_result != 0 && count > 0) {
                    const char* str3 = HAROLD::H_Message((count == 1) ? 0xf0 : 0xf1);
                    snprintf(message_text, sizeof(message_text), HAROLD::H_Message(0xef), count, str3);
                    HAROLD::User_Box(&MOX::draw_main_screen_to_back, message_text, 0);

                    int16_t new_freighters = plr->n_freighters - count;
                    if (new_freighters <= 0) {
                        new_freighters = 0;
                    }
                    plr->n_freighters = new_freighters;

                    COLCALC::Do_Player_Colony_Calculations(MOX::local_player);
                }
            }
        }

        MOX::current_screen = MOX::return_screen;
        if (selected_box != 0) {
            MOX::moveable_box[2].id = 1;
        }

        fields::Clear_Fields();
        fields::Deactivate_Auto_Function();
    }

    namespace {
        struct s_founding_choice {
            game::id_ship ship = game::NO_SHIP;
            game::id_star star = game::NO_STAR;
            game::id_player player = MOX::local_player;
            bool single_ship = MOX::fleet_box.count == 1;
        };

        s_founding_choice Capture_Founding_Choice(int type) {
            s_founding_choice choice;
            if (choice.player.id >= MAX_PLAYERS) return choice;
            const auto owner = factions::From_Player(choice.player);
            const auto& state = game::current_state;
            for (int i = 0; i < MOX::fleet_box.count && i < std::ssize(MOX::fleet_movement_icon); ++i) {
                const auto node = MOX::fleet_movement_icon[i].ship_node_idx;
                if (node < 0 || node >= std::ssize(MOX::ship_node)) continue;
                const auto* ship = game::Find_Legacy_Ship_At_Row(state, MOX::ship_node[node].ship_idx);
                if (ship && ship->id != game::NO_SHIP && ship->owner == owner && ship->status == 0 &&
                    ship->d.ship_type == type && ship->position.kind == game::e_ship_location::at_star &&
                    ship->position.target == game::e_ship_target::star && state.stars.contains(ship->position.star_id)) {
                    choice.ship = ship->id;
                    choice.star = ship->position.star_id;
                    break;
                }
            }
            return choice;
        }
    }

    void __cdecl Building_Outposts_In_Main_Screen() {
        int8_t saved_expanding_help = MOX::settings.expanding_help;
        bool skip_draw = false;

        const auto choice = Capture_Founding_Choice(SHIP_TYPE_OUTPOST);

        if (MOX::settings.expanding_help != 0) {
            MOX::settings.expanding_help = 0;
        }

        fields::Assign_Auto_Function(MAINSCR::Draw_Mini_Main_Screen, 1);
        fields::Clear_Fields();
        MOVEBOX::Vanish_Fleet_Movement_Box();
        fields::Disable_Draw_All_Fields();
        video::Set_Page_Off();
        graphics::Fill(0, 0, 0x27f, 0x1df, 0);
        MAINSCR::Draw_Mini_Main_Screen();
        RUSS::Mox_Sync_Update();
        fields::Enable_Draw_All_Fields();

        const auto star_row = game::Star_Row(game::current_state, choice.star);
        const auto site = star_row >= 0 ? MAINPUPS::New_Outpost_Selection_Popup(star_row) : game::NO_PLANET;
        if (MOX::local_player == choice.player && COLONIZE::Found_From_Ship(choice.ship, choice.star, site, choice.player, true)) {
            COLCALC::Update_Player_Stats(&game::current_state.players[choice.player.id]);
            skip_draw = choice.single_ship;
            MOX::first_node_ptr_in_stack_ = -1;
            MOX::fleet_icon_buttons_added--;
        }

        if (!skip_draw) {
            video::Set_Page_Off();
            MAINSCR::Draw_Main_Screen_Filled(1);
            MAINSCR::Draw_Main_Screen();
            RUSS::Mox_Sync_Update();
            KEN::Load_Player_Ship_Palette(factions::From_Player(MOX::local_player));

            MOX::moveable_box[2].id = 1;

            SHIPS::Build_Ship_Icons();
            MOX::eta_star_id = game::NO_STAR;
            MOX::fleet_box_ship_stack = HACCESS::Ship_Stack_From_Saved_Array();

            if (MOX::fleet_box_ship_stack == -1) {
                MOVEBOX::Vanish_Fleet_Movement_Box();
            }
        }

        MOX::current_screen = MOX::return_screen;
        MOX::settings.expanding_help = saved_expanding_help;

        fields::Clear_Fields();
        fields::Deactivate_Auto_Function();
        SYS::Clear_System_Display_Loaded_Flag();
    }

    void __cdecl Planet_Colonization_In_Main_Screen() {
        int8_t saved_expanding_help = MOX::settings.expanding_help;
        bool do_animation = false;
        bool single_fleet_ship = false;

        const auto choice = Capture_Founding_Choice(SHIP_TYPE_COLONY);
        if (const auto row = game::Star_Row(game::current_state, choice.star); row >= 0) namestar::Can_Change_Star_Name(row);

        if (MOX::settings.expanding_help != 0) {
            MOX::settings.expanding_help = 0;
        }

        fields::Assign_Auto_Function(MAINSCR::Draw_Mini_Main_Screen, 1);
        fields::Clear_Fields();
        MOVEBOX::Vanish_Fleet_Movement_Box();
        fields::Disable_Draw_All_Fields();
        video::Set_Page_Off();
        graphics::Fill(0, 0, 639, 479, 0);
        MAINSCR::Draw_Mini_Main_Screen();
        RUSS::Mox_Sync_Update();
        fields::Enable_Draw_All_Fields();

        const auto star_row = game::Star_Row(game::current_state, choice.star);
        const auto planet_idx = star_row >= 0 ? MAINPUPS::New_Colony_Selection_Popup(star_row, 1) : game::NO_PLANET;
        if (MOX::local_player == choice.player && COLONIZE::Found_From_Ship(choice.ship, choice.star, planet_idx, choice.player, false)) {
            do_animation = MOX::settings.animations_on != 0;
            COLCALC::Update_Player_Stats(&game::current_state.players[choice.player.id]);
            single_fleet_ship = choice.single_ship;
            MOX::first_node_ptr_in_stack_ = -1;
            MOX::fleet_icon_buttons_added--;
        }

        if (do_animation) {
            MOX::screen_star_id = choice.star;
            COLONY::screen_colony_id = game::current_state.planets.at(planet_idx).colony_id;
            COLLAND::Colony_Landing_Screen();
            MISC::Fade_Into_Screen(MAINSCR::Draw_Mini_Main_Screen, &MOX::draw_main_screen_to_back, 0);
        }

        if (!single_fleet_ship) {
            video::Set_Page_Off();
            MAINSCR::Draw_Main_Screen_Filled(1);
            MAINSCR::Draw_Main_Screen();
            RUSS::Mox_Sync_Update();
            KEN::Load_Player_Ship_Palette(factions::From_Player(MOX::local_player));
            MOX::moveable_box[2].id = 1;
            SHIPS::Build_Ship_Icons();
            MOX::fleet_box_ship_stack = HACCESS::Ship_Stack_From_Saved_Array();

            if (MOX::fleet_box_ship_stack == -1) {
                MOVEBOX::Vanish_Fleet_Movement_Box();
            }
        }

        MOX::current_screen = MOX::return_screen;
        MOX::settings.expanding_help = saved_expanding_help;
        fields::Clear_Fields();
        fields::Deactivate_Auto_Function();
        SYS::Clear_System_Display_Loaded_Flag();
    }

    void __cdecl Begin_Of_Turn(int8_t* reports_available) {
        bool is_main_palette = true;

        *reports_available = 0;
        MAINSCR2::Main_Screen_Report_Handler(reports_available);

        if (MOX::global_esc == 1) {
            return;
        }

        MAINSCR::Allocate_First_Screen_Seg(5);

        MOX::eta_star_id = game::NO_STAR;
        MOX::icon_overlay_x = -1;
        MOX::icon_overlay_y = -1;

        MAINSCR::Clear_Main_Screen_Fields();
        MAINSCR::Update_Star_Owners();
        SHIPS::Build_Ship_Icons();

        if (MOX::game_type != 1) {
            if (MOVEBOX::Box_Was_Open(2) || MOVEBOX::Moveable_Box_Selected(2) != 0) {
                MAINSCR2::Check_Fleet_Movement_Box_Status();
            }
        }

        fields::Disable_Draw_All_Fields();
        fields::Assign_Auto_Function(MAINSCR::Draw_Main_Screen, 2);

        if (reports_available == nullptr) {
            is_main_palette = MAINSCR::Is_Main_Screen_Palette();
        }

        MOVEBOX::Box_Was_Open(0);
        bool box_2_was_open = MOVEBOX::Box_Was_Open(2);
        MOVEBOX::Restore_Boxes();

        if (box_2_was_open || MOVEBOX::Moveable_Box_Selected(2) != 0) {
            FLEETPOP::Reload_Ship_Selection_Status();
        }

        if (reports_available != nullptr) {
            if (box_2_was_open) {
                MAINSCR2::Restore_Fleet_Box_Palette();
                fields::Enable_Draw_All_Fields();
                return;
            }
        } else {
            if (box_2_was_open) {
                MOX::moveable_box[2].id = 1;
            }
            MAINSCR::Redraw_Main_Screen(!is_main_palette);
        }

        fields::Enable_Draw_All_Fields();
    }

    void __cdecl Do_Begin_Of_Turn() {
        int8_t cancel_turn;
        int16_t next_turn_out;
        s_colors font_colors;
        char msg_buffer[80];

        in_reports_phase = 1;
        MOX::global_var = 0;

        Begin_Of_Turn(&cancel_turn);

        MAINSCR::officer_screen_return_screen = SCREEN_REPORTS;

        bool enemy_ship_coming = HACCESS::Enemy_Ship_Going_To_My_Colony();

        if (MOX::settings.end_of_turn_wait != 0 ||
            cancel_turn != 0 ||
            REPORT::something_interesting_happened[MOX::Legacy_Local_Player_Slot()] != 0 ||
            MAINSCR::next_turn_hit == 0 ||
            MOX::game_type != 0 ||
            enemy_ship_coming) {
            MAINSCR::check_next_turn_exit_field = 0;
            mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);
            MAINSCR::next_turn_hit = 0;
            fields::Clear_Fields();

        } else {
            MAINSCR::check_next_turn_exit_field = 1;

            fields::Disable_Draw_All_Fields();
            fields::Assign_Auto_Function(MAINSCR::Draw_Mini_Main_Screen, 1);
            video::Set_Page_Off();
            MAINSCR::Draw_Mini_Main_Screen();

            HAROLD::Get_Player_Font_Colors(factions::From_Player(MOX::local_player), 3, &font_colors, false);
            fonts::Set_Font_Style(3, &font_colors);

            int32_t stardate_mod = MOX::stardate % 10;
            int32_t stardate_div = MOX::stardate / 10;
            char* race_name = game::current_state.players.at(MOX::local_player.id).race_name;
            char* format = (char*)HAROLD::H_Message(0xE9);

            snprintf(msg_buffer, sizeof(msg_buffer), format, race_name, stardate_div, stardate_mod);

            int16_t str_width = fonts::Get_String_Width(msg_buffer);
            int16_t font_height = fonts::Get_Font_Height();

            int16_t box_w = str_width + 8;
            int16_t box_h = font_height + 4;

            int16_t box_x = (506 - box_w) / 2 + 21;
            int16_t box_y = (400 - box_h) / 2 + 21;

            palette::Set_Palette_Changes(0, 0xFF);
            remap::Update_Glass_Remap_Colors();

            MISC::Draw_Darkened_Box(box_x, box_y, box_w, box_h, font_colors.colors[1]);
            fonts::Print(box_x + 8, box_y + 6, msg_buffer);

            RUSS::Mox_Sync_Update();
            fields::Enable_Draw_All_Fields();

            MAINSCR::Do_Next_Turn(&next_turn_out, 1);
        }

        in_reports_phase = 0;
    }

    void __cdecl Unloading_Transports_In_Main_Screen() {
        int8_t original_expanding_help = MOX::settings.expanding_help;
        bool unloaded_all = false;

        const auto choice = Capture_Founding_Choice(SHIP_TYPE_TRANSPORT);
        const auto transports = HACCESS::Selected_Fleet_Ship_Keys(SHIP_TYPE_TRANSPORT);
        const auto fleet_count = MOX::fleet_box.count;

        if (MOX::settings.expanding_help != 0) {
            MOX::settings.expanding_help = 0;
        }

        fields::Assign_Auto_Function(MAINSCR::Draw_Mini_Main_Screen, 1);
        fields::Clear_Fields();
        MOVEBOX::Vanish_Fleet_Movement_Box();
        fields::Disable_Draw_All_Fields();
        video::Set_Page_Off();

        graphics::Fill(0, 0, 639 /* 0x27f */, 479 /* 0x1df */, 0);
        MAINSCR::Draw_Mini_Main_Screen();
        RUSS::Mox_Sync_Update();
        fields::Enable_Draw_All_Fields();

        auto& state = game::current_state;
        const auto star_row = game::Star_Row(state, choice.star);
        const auto planet_id = star_row >= 0 && !transports.empty()
            ? MAINPUPS::Drop_Troops_Selection_Popup(star_row) : game::NO_PLANET;

        const auto planet = state.planets.find(planet_id);
        if (choice.player != game::NO_PLAYER && MOX::local_player == choice.player && planet != state.planets.end() && planet->second.star_id == choice.star) {
            const auto colony = state.colonies.find(planet->second.colony_id);
            if (colony != state.colonies.end() && colony->second.owner == choice.player && colony->second.planet_id == planet_id) {
                const auto unloaded = invasion::Unload_Transports(colony->second, transports);
                if (unloaded > 0) {
                    unloaded_all = fleet_count == unloaded;
                    MOX::first_node_ptr_in_stack_ = -1;
                    MOX::fleet_icon_buttons_added--;
                }
            }
        }

        if (!unloaded_all) {
            MAINSCR::Update_Ship_Icons_In_Between_Turns();
            video::Set_Page_Off();
            MAINSCR::Draw_Main_Screen_Filled(1);
            MAINSCR::Draw_Main_Screen();
            RUSS::Mox_Sync_Update();
            KEN::Load_Player_Ship_Palette(factions::From_Player(MOX::local_player));
            MOX::moveable_box[2].id = 1;

            MOX::fleet_box_ship_stack = HACCESS::Ship_Stack_From_Saved_Array();
            if (MOX::fleet_box_ship_stack == -1) {
                MOVEBOX::Vanish_Fleet_Movement_Box();
            }
        }

        MOX::current_screen = MOX::return_screen;
        MOX::settings.expanding_help = original_expanding_help;
        fields::Clear_Fields();
        fields::Deactivate_Auto_Function();
        SYS::Clear_System_Display_Loaded_Flag();
    }

    void __cdecl Redraw_Main_Screen_After_Popups(uint8_t redraw) {
        if (redraw) {
            SYS::skip_draw_system_planets = (int8_t)MOVEBOX::Moveable_Box_Selected(0);
            MAINSCR::Fade_Into_Main_Screen();
        }
    }

    void __cdecl Main_Screen_Report_Handler(int8_t* reports_available) {
        uint8_t pending_reports;
        bool is_popup_report;

        pending_reports = REPORT::Must_Display_Reports(MOX::local_player);
        *reports_available = pending_reports;

        if (MOX::stardate_at_beg_of_turn != 0 && MOX::stardate != MOX::stardate_at_beg_of_turn) {
            MAINSCR::already_faded_into_main_screen = 0;
        }

        if (*reports_available == 0) {
            if (MOX::at_least_one_tactical_combat != 0) {
                if (MOVEBOX::Moveable_Box_Selected(2) != 0 || MOVEBOX::Moveable_Box_Selected(0) != 0) {
                    MAINSCR2::force_main_screen_fade = 1;
                }
                MOX::at_least_one_tactical_combat = 0;
            }
            return;
        }

        MOX::at_least_one_report = 1;
        MOX::mainscr_palette = 0xff;
        fields::Assign_Auto_Function(MAINSCR::Draw_Mini_Main_Screen, 1);

        if (MOX::stardate != MOX::stardate_at_beg_of_turn) {
            if (MAINSCR::Is_Main_Screen_Palette() != 0) {
                video::Set_Page_Off();
                MAINSCR::Draw_Mini_Main_Screen();
                RUSS::Mox_Sync_Update();
            } else {
                MAINSCR::Fade_Into_Main_Screen();
            }
            MOX::stardate_at_beg_of_turn = MOX::stardate;
        }

        do {
            MAINSCR::already_faded_into_main_screen = 0;
            SYS::Clear_System_Display_Ship_Fields();
            SYS::Clear_System_Display_Loaded_Flag();
            MAINSCR::Allocate_First_Screen_Seg(5);

            MOX::previous_report = REPORT::Display_Report(MOX::local_player);

            if (MOX::previous_report == 2 ||
                MOX::previous_report == 5 ||
                MOX::previous_report == 6 ||
                (MOX::previous_report == 7 && MOX::settings.show_gnn_report != 0)) {
                is_popup_report = true;
            } else {
                is_popup_report = false;
            }

            if (MOX::global_esc == 1) {
                return;
            }

            pending_reports = REPORT::Must_Display_Reports(MOX::local_player);

            if (is_popup_report && pending_reports != 3) {
                MAINSCR2::Redraw_Main_Screen_After_Popups(1);
                MAINSCR::already_faded_into_main_screen = 1;
            }

        } while (pending_reports != 0);

        fields::Assign_Auto_Function(MAINSCR::Draw_Main_Screen, 1);
    }

    void __cdecl Check_Fleet_Movement_Box_Status() {
        MOX::fleet_box_ship_stack = HACCESS::Ship_Stack_From_Saved_Array();

        if (MOX::fleet_box_ship_stack == -1) {
            MOVEBOX::Vanish_Fleet_Movement_Box();
            MOX::saved_boxes[2].is_open = 0;
            MOX::saved_boxes[2].field_id = MOX::box_off_flag[2];
        }
    }

    void __cdecl Restore_Fleet_Box_Palette() {
        MOX::fleet_box_ship_stack = HACCESS::Ship_Stack_From_Saved_Array();

        if (MOX::fleet_box_ship_stack == -1) {
            MOX::moveable_box[2].id = MOX::box_off_flag[2];
        } else {
            MOX::moveable_box[2].id = 1;
            MAINSCR::Fade_Into_Main_Screen();
            force_main_screen_fade = 0;
        }
    }
} // namespace MAINSCR2
