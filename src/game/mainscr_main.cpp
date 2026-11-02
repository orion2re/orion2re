#include "pch.h"
#include "game/game_events.h"

#include "game/config_refresh.h"

namespace MAINSCR {
    /**
     * Draws the main-screen HUD summaries for stardate, economy, command points, food, freighters, and research.
     */
    void __cdecl Print_Main_Screen_Data() {
        char sep = (MOX::settings.language == 1 || MOX::settings.language == 2) ? ',' : '.';
        s_colors colors1, colors2;
        char buffer[128];
        char monetary_unit[16];
        char research_unit[16];

        memset(&colors1, 0, sizeof(colors1));
        memset(&colors2, 0, sizeof(colors2));

        fonts::Set_Font_Style(0, &colors1);
        fonts::Get_Font_Height();
        snprintf(buffer, sizeof(buffer), "%d%c%d", (int)(MOX::stardate / 10), (int)sep, (int)(MOX::stardate % 10));
        MISC::Set_Fitted_Font_Style(buffer, &colors2, 2, 2, -1, 0x43, 1);
        int16_t stardate_font_style = MISC::Get_Font_Style();
        s_colors* p_stardate_colors = MISC::Font_Colors2(stardate_font_style, 14, 23);
        memcpy(&colors2, p_stardate_colors, sizeof(colors2));
        fonts::Set_Outline_Color(2, 1);
        fonts::Set_Font_Style_Outline(stardate_font_style, &colors2);
        fonts::Print_Centered(580, 29, buffer);

        if (EVENTS::Event_Check_Warp_Beast()) {
            MISC::Get_Mox_Font_Colors(3, 'I', 'L', &colors1);
            fonts::Set_Font_Style(3, &colors1);
            MISC::Increase_Font_Color_Range(((int)MOX::red_color_pulse_value * 5) / 9);
            char beast_buffer[128];
            strlcpy(beast_buffer, ESTRINGS::E_Strings(ESTR_BEAST), sizeof(beast_buffer));
            int16_t beast_right = (int16_t)(25 + fonts::Get_String_Width(beast_buffer));

            line::Line(beast_right, 21, 25, 21, colors1.colors[0]);
            line::Line(beast_right, 22, 25, 22, colors1.colors[1]);
            fonts::Print(25, 24, beast_buffer);

            int16_t fh = fonts::Get_Font_Height();
            line::Line(beast_right, fh + 25, 25, fh + 25, colors1.colors[1]);
            line::Line(beast_right, fh + 26, 25, fh + 26, colors1.colors[0]);
        }

        if (EVENTS::Event_Check_Hyperspace_Flux()) {
            MISC::Get_Mox_Font_Colors(3, 'I', 'L', &colors1);
            fonts::Set_Font_Style(3, &colors1);
            MISC::Increase_Font_Color_Range(((int)MOX::red_color_pulse_value * 5) / 9);
            const char* flux_msg = HAROLD::H_Message(0xe0);
            char flux_buffer[128];
            strlcpy(flux_buffer, flux_msg, sizeof(flux_buffer));
            int16_t flux_width = fonts::Get_String_Width(flux_buffer);
            int16_t flux_x = 526 - flux_width;

            line::Line(flux_x, 21, 526, 21, colors1.colors[0]);
            line::Line(flux_x, 22, 526, 22, colors1.colors[1]);
            fonts::Print_Right(526, 24, flux_buffer);

            int16_t fh = fonts::Get_Font_Height();
            line::Line(flux_x, fh + 25, 526, fh + 25, colors1.colors[1]);
            line::Line(flux_x, fh + 26, 526, fh + 26, colors1.colors[0]);
        }

        fonts::Get_Font_Height();
        HAROLD::Get_Monetary_Unit_String(monetary_unit);
        s_player* plr = &MOX::player[MOX::PLAYER_NUM];

        if (main_screen_tax_rate && plr->tax_rate != 0) {
            snprintf(buffer, sizeof(buffer), "%d%%", (int)plr->tax_rate);
            MISC::Set_Fitted_Font_Style(buffer, &colors2, 2, 2, 4, 0x43, 1);
            Set_Post_Fitted_Font_Style(14, 23);
            fonts::Set_Outline_Color(2, 1);
            fonts::Print_Centered(579, (int16_t)(96 - fonts::Get_Font_Height()), buffer);
        }

        char* surplus_bc_comma = MISC::Get_Comma_String(plr->surplus_bc);
        const char* bc_sign = (plr->surplus_bc < 0) ? "" : "+";
        snprintf(buffer, sizeof(buffer), "%s%s %s", bc_sign, surplus_bc_comma, monetary_unit);
        MISC::Set_Fitted_Font_Style(buffer, &colors2, 2, 2, 4, 0x43, 1);
        int16_t bc_style = MISC::Get_Font_Style();
        MISC::Get_Font_Spacing();
        MISC::Get_Mox_Font_Colors(bc_style, 75, 77, &colors1);
        MISC::Get_Mox_Font_Colors(bc_style, 14, 23, &colors2);

        char* bc_comma = MISC::Get_Comma_String(plr->bc);
        snprintf(buffer, sizeof(buffer), "%s %s", bc_comma, monetary_unit);

        if (plr->bc < 0) {
            MISC::Set_Fitted_Font_Style(buffer, &colors1, 2, 2, 4, 0x43, 1);
            Set_Post_Fitted_Font_Style('K', 'M');
            MISC::Increase_Font_Color_Range(((int)MOX::red_color_pulse_value * 4) / 7);
            fonts::Set_Outline_Color(0x49, 0x49);
        } else {
            MISC::Set_Fitted_Font_Style(buffer, &colors2, 2, 2, 4, 0x43, 1);
            fonts::Set_Outline_Color(2, 1);
        }
        fonts::Print_Centered(579, 96, buffer);

        surplus_bc_comma = MISC::Get_Comma_String(plr->surplus_bc);
        bc_sign = (plr->surplus_bc < 0) ? "" : "+";
        snprintf(buffer, sizeof(buffer), "%s%s %s", bc_sign, surplus_bc_comma, monetary_unit);
        MISC::Set_Fitted_Font_Style(buffer, &colors2, 2, 2, 4, 0x43, 1);
        bc_style = MISC::Get_Font_Style();
        int16_t bc_spacing = MISC::Get_Font_Spacing();
        MISC::Get_Mox_Font_Colors(bc_style, 75, 77, &colors1);
        MISC::Get_Mox_Font_Colors(bc_style, 14, 23, &colors2);

        if (plr->surplus_bc < 0) {
            fonts::Set_Outline_Color(0x49, 0x49);
            fonts::Set_Font_Style_Outline(bc_style, &colors1);
            fonts::Set_Font_Spacing(bc_spacing);
            MISC::Increase_Font_Color_Range(((int)MOX::red_color_pulse_value * 4) / 7);
        } else {
            fonts::Set_Font_Style_Outline(bc_style, &colors2);
            fonts::Set_Outline_Color(2, 1);
            fonts::Set_Font_Spacing(bc_spacing);
        }
        fonts::Print_Centered(579, fonts::Get_Font_Height() + 96, buffer);

        int16_t cp_surplus = plr->command_points - plr->command_points_used;
        const char* cp_sign = (cp_surplus < 0) ? "" : "+";
        snprintf(buffer, sizeof(buffer), "%s%d (%d)", cp_sign, (int)cp_surplus, (int)plr->command_points);
        MISC::Set_Fitted_Font_Style(buffer, &colors2, 2, 2, 4, 0x43, 0);
        int16_t current_style = (int16_t)fonts::current_font_style;
        int16_t current_spacing = MISC::Get_Font_Spacing();
        MISC::Get_Mox_Font_Colors(current_style, 'K', 'M', &colors1);
        MISC::Get_Mox_Font_Colors(current_style, 14, 23, &colors2);

        char cp_left[64];
        snprintf(cp_left, sizeof(cp_left), "%s%d", cp_sign, (int)cp_surplus);
        int16_t cp_left_w = fonts::Get_String_Width(cp_left);
        char cp_right[64];
        snprintf(cp_right, sizeof(cp_right), "(%d)", (int)plr->command_points);
        int16_t cp_total_w = (int16_t)(fonts::Get_String_Width(cp_right) + cp_left_w + 4);

        if (cp_surplus < 0) {
            fonts::Set_Font_Style_Outline(current_style, &colors1);
            fonts::Set_Outline_Color(0x49, 0x49);
            fonts::Set_Font_Spacing(current_spacing);
            MISC::Increase_Font_Color_Range(((int)MOX::red_color_pulse_value * 4) / 7);
        } else {
            fonts::Set_Font_Style_Outline(current_style, &colors2);
            fonts::Set_Outline_Color(2, 1);
            fonts::Set_Font_Spacing(current_spacing);
        }
        int16_t cp_half_w = (int16_t)(cp_total_w / 2);
        fonts::Print(581 - cp_half_w, 181, cp_left);
        fonts::Set_Font_Style(current_style, &colors2);
        fonts::Set_Font_Spacing(current_spacing);
        fonts::Print(581 + cp_left_w + 4 - cp_half_w, 181, cp_right);

        fonts::Get_Font_Height();
        char* food_comma = MISC::Get_Comma_String(plr->surplus_food);
        const char* food_sign = (plr->surplus_food < 0) ? "" : "+";
        snprintf(buffer, sizeof(buffer), "%s%s", food_sign, food_comma);
        if (plr->surplus_food < 0) {
            MISC::Set_Fitted_Font_Style(buffer, &colors1, 2, 2, 4, 0x43, 1);
            Set_Post_Fitted_Font_Style('K', 'M');
            fonts::Set_Outline_Color(0x49, 0x49);
            MISC::Increase_Font_Color_Range(((int)MOX::red_color_pulse_value * 4) / 7);
        } else {
            MISC::Set_Fitted_Font_Style(buffer, &colors2, 2, 2, 4, 0x43, 1);
            Set_Post_Fitted_Font_Style(14, 23);
            fonts::Set_Outline_Color(2, 1);
        }
        fonts::Print_Centered(579, 255, buffer);

        const char* fr_sign = (plr->surplus_freighters < 0) ? "" : "+";
        snprintf(buffer, sizeof(buffer), "%s%d (%d)", fr_sign, (int)plr->surplus_freighters, (int)plr->n_freighters);
        MISC::Set_Fitted_Font_Style(buffer, &colors1, 2, 2, 4, 0x43, 1);
        int16_t fr_style = MISC::Get_Font_Style();
        MISC::Get_Mox_Font_Colors(fr_style, 'K', 'M', &colors1);
        MISC::Get_Mox_Font_Colors(fr_style, 14, 23, &colors2);
        int16_t fr_spacing = MISC::Get_Font_Spacing();
        int16_t fr_total_w = fonts::Get_String_Width(buffer);

        snprintf(buffer, sizeof(buffer), "%s%d", fr_sign, (int)plr->surplus_freighters);
        if (plr->surplus_freighters < 0) {
            fonts::Set_Font_Style_Outline(fr_style, &colors1);
            fonts::Set_Font_Spacing(fr_spacing);
            MISC::Increase_Font_Color_Range(((int)MOX::red_color_pulse_value * 4) / 7);
            fonts::Set_Outline_Color(0x49, 0x49);
        } else {
            fonts::Set_Font_Style_Outline(fr_style, &colors2);
            fonts::Set_Font_Spacing(fr_spacing);
            fonts::Set_Outline_Color(2, 1);
        }
        int16_t fr_half_w = (int16_t)((fr_total_w + 4) / 2);
        fonts::Print(581 - fr_half_w, 330, buffer);

        fonts::Set_Outline_Color(2, 1);
        int16_t fr_left_w = fonts::Get_String_Width(buffer);
        snprintf(buffer, sizeof(buffer), "(%d)", (int)plr->n_freighters);
        fonts::Set_Font_Style_Outline(fr_style, &colors2);
        fonts::Set_Font_Spacing(fr_spacing);
        fonts::Print(fr_left_w - fr_half_w + 583, 330, buffer);

        Get_Research_Unit_String(research_unit);
        if (plr->research_breakthrough != 0) {
            fonts::Set_Outline_Color(2, 1);
            fonts::Set_Font_Style_Outline(0, &colors2);
            fonts::Print_Centered(579, 403, HAROLD::H_Message(0x183));
            return;
        }
        if (plr->current_research_field == 0) {
            fonts::Set_Outline_Color(2, 1);
            MISC::Set_Fitted_Font_Style(buffer, &colors2, 2, 2, 4, 0x43, 1);
            Set_Post_Fitted_Font_Style(14, 23);
            fonts::Print_Centered(579, 403, HAROLD::H_Message(0x188));
            return;
        }

        int16_t turns = COLCALC::Player_N_Turns_Until_Research_Complete(plr);
        if (turns > 0) {
            int32_t chance = COLCALC::Chance_For_Research_Breakthrough(plr);
            if (chance > 0) {
                snprintf(buffer, sizeof(buffer), "%d%%", (int)chance);
                MISC::Set_Fitted_Font_Style(buffer, &colors2, 2, 2, 4, 0x43, 1);
                Set_Post_Fitted_Font_Style(14, 23);
                fonts::Set_Outline_Color(2, 1);
                fonts::Print_Centered(579, (int16_t)(403 - 2 * fonts::Get_Font_Height()), buffer);
            }
            const char* turns_msg = HAROLD::H_Message(turns == 1 ? 0xe1 : 0xe2);
            snprintf(buffer, sizeof(buffer), turns_msg, (int)turns);
            char at_buffer[sizeof(buffer) + 1];
            snprintf(at_buffer, sizeof(at_buffer), "@%s", buffer);
            MISC::Set_Fitted_Font_Style(at_buffer, &colors2, 2, 2, 4, 0x43, 1);
            Set_Post_Fitted_Font_Style(14, 23);
            int16_t fh = fonts::Get_Font_Height();
            int16_t turns_y = (int16_t)(403 - fh);
            fonts::Set_Outline_Color(2, 1);
            int16_t msg_w = fonts::Get_String_Width(buffer);
            int16_t start_x = (int16_t)((62 - msg_w) / 2 + 548);
            fonts::Print(start_x, turns_y + 3, "@");

            const char* turns_msg2 = HAROLD::H_Message(turns == 1 ? 0xe1 : 0xe2);
            snprintf(buffer, sizeof(buffer), turns_msg2, (int)turns);
            fonts::Print((int16_t)(fonts::Get_String_Width("@") + start_x), turns_y, buffer);

            char* res_comma = MISC::Get_Comma_String(plr->research_produced);
            snprintf(buffer, sizeof(buffer), "%s %s", res_comma, research_unit);
            MISC::Set_Fitted_Font_Style(buffer, &colors2, 2, 2, 4, 0x43, 1);
            Set_Post_Fitted_Font_Style(14, 23);
            fonts::Set_Outline_Color(2, 1);
            fonts::Print_Centered(579, turns_y + fonts::Get_Font_Height(), buffer);
        } else if (turns < 0) {
            snprintf(buffer, sizeof(buffer), "0 %s", research_unit);
            MISC::Set_Fitted_Font_Style(buffer, &colors2, 2, 2, 4, 0x43, 1);
            Set_Post_Fitted_Font_Style(14, 23);
            fonts::Set_Outline_Color(2, 1);
            fonts::Print_Centered(579, 403, buffer);
        } else {
            int32_t chance = COLCALC::Chance_For_Research_Breakthrough(plr);
            snprintf(buffer, sizeof(buffer), "%d%%", (int)chance);
            MISC::Set_Fitted_Font_Style(buffer, &colors2, 2, 2, 4, 0x43, 1);
            Set_Post_Fitted_Font_Style(14, 23);
            fonts::Set_Outline_Color(2, 1);
            fonts::Print_Centered(579, 391, buffer);

            snprintf(buffer, sizeof(buffer), "%d %s", (int)plr->research_produced, research_unit);
            MISC::Set_Fitted_Font_Style(buffer, &colors2, 2, 2, 4, 0x43, 1);
            Set_Post_Fitted_Font_Style(14, 23);
            fonts::Set_Outline_Color(2, 1);
            fonts::Print_Centered(579, (int16_t)(391 + fonts::Get_Font_Height()), buffer);
        }
    }

    /**
     * Runs the main-screen event loop, including map interaction, popups, and screen transitions.
     */
    void __cdecl Main_Screen() {
        game_events::s_fleet_box_view_scope fleet_box_view;
        game_events::s_settlement_view_scope settlement_view(MOX::PLAYER_NUM);
        int16_t exit_flag = 0;
        int16_t hovered_target_id = 0;
        int16_t boxes_saved_flag = 0;
        uint8_t icon_selected_flag = 0;
        int16_t planet_idx = 0;
        int16_t selected_target_id = 0;
        int16_t system_hover_selection_id = 0;
        int16_t m_planet_idx = 0;
        int16_t m_ship_idx = 0;
        int16_t ship_idx = 0;

        int16_t ship_ids[SHIP_LIST_CAPACITY];
        char text_buffer[256];
        char msg_buffer[512];

        bool is_main_palette = MAINSCR::Is_Main_Screen_Palette();
        moving_a_stack = 0;
        track_time_flag = 1;
        fields::Assign_Auto_Function(MAINSCR::Draw_Main_Screen, 1);
        MAINSCR::Allocate_First_Screen_Seg(5);
        SYS::Clear_System_Display_Loaded_Flag();
        mouse::Clear_Mouse_XY();
        EVANHELP::Set_Main_Screen_Help_List();
        MAINSCR::Clear_Twinkling_Star_Data();
        MOX::fleet_ship_scanned = -1;
        MOX::galaxy_map_scanned_star = -1;
        fields::Set_Field_Outline_Color(20);
        fields::Disable_Cancel();
        zoom_box_increment_value = 1;
        officer_screen_return_screen = SCREEN_UNKNOWN;
        MOX::draw_system_display_without_button = 0;
        MOX::skip_fields = 0;
        main_screen_scanned_star = -1;

        if (MOX::game_type != 1) {
            if (MOVEBOX::Moveable_Box_Selected(2)) {
                if (HACCESS::Get_Stack_Start_From_Saved_Ship_Array() == 0) {
                    MOVEBOX::Vanish_Fleet_Movement_Box();
                }
                MOX::ship_icon_player_id = -1;
            }
        }

        video::Set_Refresh_Stencil();
        MOX::draw_main_screen_to_back = 1;
        icon_selected_flag = 0;
        MOX::zoom_count = HAROLD::Map_Scale_To_Zoom_Count();
        exit_flag = 0;

        fields::Clear_Fields();
        MAINSCR::Clear_Main_Screen_Fields();
        MAINSCR::Add_Cheats();

        if (!is_main_palette && MOX::previous_screen == SCREEN_REPORTS) {
            if (MOVEBOX::Moveable_Box_Selected(0) && MOX::at_least_one_report) {
                SYS::skip_draw_system_planets = 1;
            }
        }

        MAINSCR::Update_Star_Owners();
        SHIPS::Build_Ship_Icons();
        SHIPS::Set_Ship_Icon_XYs();

        if (clear_fleet_fields == 0) {
            if (FLEETPOP::Some_Icon_Selected()) {
                FLEETPOP::Reload_Ship_Selection_Status();
            }
        } else {
            MOX::first_node_ptr_in_stack_ = -1;
        }

        if (MAINSCR2::force_main_screen_fade == 0 &&
            (is_main_palette || (MAINSCR2::main_screen_fade_in_flag == 0 && MOX::at_least_one_tactical_combat == 0))) {
            if (MOVEBOX::Moveable_Box_Selected(0)) {
                MOX::system_star_id = -1;
                SYS::Clear_System_Display_Loaded_Flag();
            }
            fonts::Load_Palette(0, 0, 0xad);
            if (MOX::previous_screen == SCREEN_TECH_CHANGE) {
                video::Set_Page_Off();
                MAINSCR::Draw_Mini_Main_Screen();
                video::Copy_Off_To_Back();
                RUSS::Mox_Sync_Update();
                video::Set_Refresh_Stencil();
            }
            MAINSCR::Add_Map_Fields();
        } else {
            MAINSCR::Fade_Into_Main_Screen();
            MOX::draw_fleet_screen_to_back = 1;
        }

        track_time_flag = 0;
        clear_fleet_fields = 1;

        std::string loaded_config_message =
            SAVEGAME::Consume_Loaded_Config_Message();
        if (!loaded_config_message.empty()) {
            GENDRAW::Message_Box(loaded_config_message.c_str());
        }

        if (MOX::given_chance_to_rename_home_star[MOX::PLAYER_NUM] == 0) {
            if (MOX::stardate == 35000) {
                fields::Deactivate_Help_List();
                MAINSCR::Change_Home_Star_Name();
                EVANHELP::Set_Main_Screen_Help_List();
            }
            MOX::given_chance_to_rename_home_star[MOX::PLAYER_NUM] = 1;
        }

        MAINSCR2::main_screen_fade_in_flag = 1;
        MOX::at_least_one_tactical_combat = 0;

        if (MOX::game_type != 0) {
            uint8_t player_num = (uint8_t)MOX::PLAYER_NUM;
            uint8_t mask = (uint8_t)(1u << player_num);
            if ((((uint8_t)warn_about_cheaters >> player_num) & 1u) != 0) {
                warn_about_cheaters &= (uint8_t)~mask;
                MAINSCR::Warn_About_Cheating_Players();
            }
        }

        fleet_box_view.Initialized();
        settlement_view.Initialized();
        while (exit_flag == 0 && MOX::global_esc == 0 &&
            !CONFIG_REFRESH::Resource_Reload_Pending()) {
            if (MOX::game_type == 2 || MOX::game_type == 3) {
                if (MAINSCR::Check_For_Diplomacy_Request() == 1) {
                    fields::Assign_Auto_Function(MAINSCR::Draw_Main_Screen, 1);
                    fields::Clear_Fields();
                    MAINSCR::Clear_Main_Screen_Fields();
                    MAINSCR::Add_Cheats();
                    ERIC::Fast_Fade_Out();
                    MAINSCR::Add_Map_Fields();

                    if (MOVEBOX::Moveable_Box_Selected(2)) {
                        already_faded_into_main_screen = 0;
                        MOX::reloaded_ship_palette = 0;
                    } else if (MOVEBOX::Moveable_Box_Selected(0)) {
                        SYS::Clear_System_Display_Loaded_Flag();
                    }
                    MAINSCR::Fade_Into_Main_Screen();
                }
            }

            fleet_ship_selection_status = -1;
            timer::Mark_Time();

            int16_t input;
            {
                game_events::s_main_view_input_scope main_view_input;
                game_events::Publish(game_events::MAP_OBSERVATION_POINT);
                input = fields::Get_Input();
            }
            if (input < 0) {
                if (merging_relocations != 0) {
                    merging_relocations = 0;
                }
                if (MOX::zoom_in == 1) {
                    MAINSCR::Exit_Zooming_Mode(&boxes_saved_flag);
                }
            }

            if (RUSS::all_ai_game != 0) {
                // Disabled on purpose so all-AI mode can remain on the main screen
                // without automatically advancing the turn.
                // video::Set_Page_Off();
                // MAINSCR::Draw_Main_Screen();
                // RUSS::Mox_Sync_Update();
                // input = next_button;
            }
            if (MOX::player[MOX::PLAYER_NUM].surrender_to != -1) {
                video::Set_Page_Off();
                MAINSCR::Draw_Main_Screen();
                RUSS::Mox_Sync_Update();
                input = next_button;
            }

            if (input != 0) game_events::Publish(game_events::MAIN_VIEW_INPUT_INTERRUPTED);

            int16_t mouse_input = fields::Scan_Input();
            if (MOX::main_screen_grid_field == input || (-MOX::main_screen_grid_field == input)) {
                int mx = mouse::Pointer_X();
                int my = mouse::Pointer_Y();
                icon_selected_flag = static_cast<uint8_t>((input <= 0) + 1);

                if (MOVEBOX::Moveable_Box_Selected(2)) {
                    MAINSCR::Check_Stars_XY(mx, my, static_cast<int8_t>(icon_selected_flag));
                    if (MOX::mouse_XY.star_idx == -1) {
                        MAINSCR::Check_Ships_XY(mx, my, static_cast<int8_t>(icon_selected_flag));
                    }
                } else {
                    MAINSCR::Check_Ships_XY(mx, my, static_cast<int8_t>(icon_selected_flag));
                    if (MOX::mouse_XY.ship_icon_idx == -1) {
                        MAINSCR::Check_Stars_XY(mx, my, static_cast<int8_t>(icon_selected_flag));
                    }
                }
            } else {
                MOX::mouse_XY.ship_icon_idx = -1;
                MOX::mouse_XY.star_idx = -1;
            }

            if (MOX::skip_fields == 0) {
                icon_selected_flag = 0;

                int16_t mouse_status = MAINSCR::Get_Mouse_Field(0, &hovered_target_id, &selected_target_id);
                main_screen_scanned_star = hovered_target_id;

                if ((mouse_status == 0 || mouse_status == 4) && MOVEBOX::Moveable_Box_Selected(2)) {
                    icon_selected_flag = FLEETPOP::Some_Icon_Selected();
                }

                if (mouse_status == 0 && MOX::star[selected_target_id].spectral_class != STAR_CLASS_BLACK_HOLE && merging_relocations != 0) {
                    merging_relocations = 0;
                    mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);
                    HACCESS::Set_All_Star_Relocations(selected_target_id, MOX::PLAYER_NUM);

                    const char* msg = HAROLD::H_Message(HESTR_0DD_ALL_NEW_SHIPS_BUILT_WI);
                    snprintf(text_buffer, sizeof(text_buffer), msg, &MOX::star[selected_target_id]);
                    HAROLD::User_Box(&MOX::draw_main_screen_to_back, text_buffer, 0);
                } else {
                    if (mouse_status == 0 && selected_target_id >= 0) {
                        if (!icon_selected_flag) {
                            int16_t colony_idx = -1;
                            if (MOX::settings.auto_select_colony && MOX::star[selected_target_id].spectral_class != STAR_CLASS_BLACK_HOLE) {
                                colony_idx = HACCESS::Last_Colony_Selected(selected_target_id, MOX::PLAYER_NUM);
                            }
                            if (colony_idx == -1) {
                                MAINSCR::Do_System_Popup(selected_target_id);
                            } else {
                                MAINSCR::Do_Colony_Screen(colony_idx, &exit_flag);
                            }
                        }
                    }

                    if (icon_selected_flag) {
                        if (selected_target_id > -1 && mouse_status == 0 && MOX::star[selected_target_id].spectral_class != STAR_CLASS_BLACK_HOLE) {
                            HACCESS::Get_Fleet_Box_Selected_Ship_Ids(ship_ids, SHIP_LIST_CAPACITY, 0xff);
                            SHIPMOVE::Ships_Try_To_Move_To(ship_ids, selected_target_id, &MOX::g_ship_move_info);

                            if (MOX::g_ship_move_info.moving == 0) {
                                int msg_id = 0;
                                if (MOX::g_ship_move_info.blackhole_blocks != 0)
                                    msg_id = HESTR_021_A_BLACK_HOLE_BLOCKS_MO;
                                else if (MOX::g_ship_move_info.insufficient_fuel != 0)
                                    msg_id = HESTR_022_ONE_OR_MORE_SHIPS_DOES;
                                else if (MOX::g_ship_move_info.immobile != 0)
                                    msg_id = HESTR_023_ONE_OR_MORE_SHIPS_CHOS;
                                else if (MOX::g_ship_move_info.hyperspace_flux != 0)
                                    msg_id = HESTR_024_SHIP_MOVEMENT_IS_NOT_A;

                                if (msg_id != 0) {
                                    const char* msg = HAROLD::H_Message(msg_id);
                                    strlcpy(msg_buffer, msg, sizeof(msg_buffer));
                                    HAROLD::User_Box(&MOX::draw_main_screen_to_back, msg_buffer, 3);
                                }
                            } else {
                                int ships_count = FLEETPOP::Fleet_Buttons_Chosen();
                                if (!SHIPMOVE::Apply_Player_Movement_Order(
                                        ship_ids, selected_target_id, &MOX::g_ship_move_info)) {
                                    continue;
                                }
                                MOX::eta_star_id = -1;

                                int16_t next_icon = 0;
                                if (ships_count == MOX::fleet_box.count) {
                                    MOX::just_moved_stack = 1;
                                } else {
                                    next_icon = FLEETPOP::First_Unselected_Icon();
                                }

                                SHIPS::Build_Ship_Icons();
                                SHIPS::Set_Ship_Icon_XYs();

                                int16_t stack_ship = next_icon;
                                if (ships_count == MOX::fleet_box.count) {
                                    stack_ship = 0;
                                }

                                int16_t ship_id = HACCESS::Fleet_Movement_Ship_Id(stack_ship);
                                MOX::fleet_box_ship_stack = HACCESS::Ship_Stack_From_Ship_Id(ship_id);

                                FLEETPOP::Set_Fleet_Movement_Icon_Selections(0, 0);
                                HACCESS::Clear_Fleet_Box_Selected_Ship_Status();

                                if (MOX::fleet_box_ship_stack == -1) {
                                    MOVEBOX::Vanish_Fleet_Movement_Box();
                                }

                                MOX::draw_main_screen_to_back = 1;
                                MOX::first_node_ptr_in_stack_ = -1;
                            }
                        } else if (mouse_status == 4 && MOVEBOX::Moveable_Box_Selected(2)) {
                            MOX::fleet_ship_scanned = -1;
                            if (MOX::star[hovered_target_id].spectral_class == STAR_CLASS_BLACK_HOLE) {
                                MOX::eta_star_id = -1;
                            } else {
                                MOX::eta_star_id = hovered_target_id;
                            }
                        }
                    }
                }
            }

            if (MOVEBOX::Moveable_Box_Selected(0) && MOX::skip_fields == 0) {
                if (MOX::system_star_id > -1 && MOX::system_star_id < MOX::NUM_STARS && input != MOX::moveable_box[0].close_btn) {
                    int mx = mouse::Pointer_X();
                    int my = mouse::Pointer_Y();

                    int res = MAINSCR::Scan_System_Display_Fields(mouse_input, input, MOX::planet_buttons, &m_planet_idx, &planet_idx, &m_ship_idx, &ship_idx, &system_hover_selection_id);

                    MOX::system_display_ship_scanned = m_ship_idx;
                    if (system_hover_selection_id == -1 && MOX::system_icon_scanned > 2000 && MOX::system_icon_scanned < 2004) {
                        MOX::system_icon_scanned = -1;
                    } else if (system_hover_selection_id > -1) {
                        MOX::system_icon_scanned = system_hover_selection_id;
                    }

                    // Boundary check against moveable box
                    if (mx < MOX::moveable_box[0].x || mx > MOX::moveable_box[0].x + MOX::moveable_box[0].width ||
                        my < MOX::moveable_box[0].y || my > MOX::moveable_box[0].y + MOX::moveable_box[0].height) {
                        MOX::system_icon_scanned = -1;
                    }

                    if (res == 0) {
                        if (ship_idx < 0) {
                            int16_t p_idx = HAROLD::Planet_Index(MOX::moveable_box[0].id, planet_idx);

                            int16_t colony_id = MOX::planet[p_idx].colony_index;
                            if (colony_id > -1 && colony_id < MOX::NUM_COLONIES && MOX::colony[colony_id].owner == MOX::PLAYER_NUM) {
                                MAINSCR::Do_Colony_Screen(colony_id, &exit_flag);
                            }
                        } else {
                            int16_t ship_icon = HACCESS::Ship_Icon_Id_From_Node_Ptr(ship_idx);
                            MAINSCR::Do_Fleet_Popup(ship_icon, 0);
                        }
                    }
                }
            }

            MAINSCR::Scan_Moveable_Box_Fields(MOX::skip_fields, input, mouse_input, &exit_flag);

            if (MOX::skip_fields == 0) {
                int field_res = MAINSCR::Get_Mouse_Field(1, &hovered_target_id, &selected_target_id);
                main_screen_ship_icon_scanned = hovered_target_id;
                if (hovered_target_id != -1) {
                    MOX::eta_star_id = -1;
                }
                if (field_res == 0) {
                    MAINSCR::Do_Fleet_Popup(selected_target_id, 1);
                }

                MAINSCR::Check_Function_Keys(input, &exit_flag);
                MAINSCR::Check_Hot_Keys(input, 0);
            }

            if (CONFIG_REFRESH::Resource_Reload_Pending()) {
                if (MOX::zoom_in != 0) {
                    MAINSCR::Exit_Zooming_Mode(&boxes_saved_flag);
                }
                break;
            }

            if (input == exit_game_hot_key && MOX::skip_fields == 0) {
                const char* msg = HAROLD::H_Message(0xde);
                exitm::Exit_Normally(msg);
                // This will exit application
            }

            if (input == game_button && MOX::skip_fields == 0) {
                MOX::current_screen = SCREEN_GAME;
                exit_flag = 1;
                MOX::screen_data = 0;
                MOX::return_screen = SCREEN_MAIN;
            }

            const int8_t active_save_slot = MOX::settings.active_save_slot;
            if (MOX::skip_fields == 0 && active_save_slot > -1 && active_save_slot < 10 && input == -0x455) {
                if (MOX::game_type == 0 || MOX::game_type == 1) {
                    LOADSAVE::Auto_Save_Last_Saved_Game(&exit_flag);
                } else {
                    const char* msg = HAROLD::H_Message(0x20);
                    strlcpy(msg_buffer, msg, sizeof(msg_buffer));
                    HAROLD::User_Box(&MOX::draw_main_screen_to_back, msg_buffer, 3);
                }
            }

            if (MOX::skip_fields == 0 && active_save_slot > -1 && active_save_slot < 10 && input == -0x456) {
                LOADSAVE::Auto_Load_Last_Saved_Game(&exit_flag);
            }

            if (input == fleets_button && MOX::skip_fields == 0) {
                if (MOVEBOX::Moveable_Box_Selected(2) || MOVEBOX::Moveable_Box_Selected(0)) {
                    fields::Clear_Fields();
                    video::Set_Page_Off();
                    MAINSCR::Draw_Mini_Main_Screen();
                    RUSS::Mox_Sync_Update();
                }
                MOX::return_screen = SCREEN_MAIN;
                MOX::current_screen = SCREEN_FLEET;
                officer_screen_return_screen = SCREEN_FLEET;
                exit_flag = 1;
            }

            if (input == colonies_button && MOX::skip_fields == 0) {
                MOX::current_screen = SCREEN_COLONY_SUMMARY;
                exit_flag = 1;
                MOX::return_screen = SCREEN_MAIN;
            }

            if (input == info_button && MOX::skip_fields == 0) {
                MOX::current_screen = SCREEN_INFO;
                exit_flag = 1;
                MOX::return_screen = SCREEN_MAIN;
            }

            if (input == next_button && MOX::skip_fields == 0) {
                RUSS::Assert_Settings();
                MOX::auto_turn_exit = 0;
                next_turn_hit = 1;
                MOX::global_exit_field = -1000;
                MAINSCR::Do_Next_Turn(&exit_flag, 0);
            }

            if (input == races_button && MOX::skip_fields == 0) {
                MOX::current_screen = SCREEN_RACE;
                exit_flag = 1;
                MOX::return_screen = SCREEN_MAIN;
            }

            if (input == planets_button && MOX::skip_fields == 0) {
                MOX::current_screen = SCREEN_PLANET_SUMMARY;
                exit_flag = 1;
                MOX::return_screen = SCREEN_MAIN;
            }

            if (input == leaders_button && MOX::skip_fields == 0) {
                int16_t design_num;
                if (MOX::officer_scrn_type == LEADER_TYPE_SHIP) {
                    if (MOVEBOX::Moveable_Box_Selected(2)) {
                        design_num = MOX::fleet_box_ship_stack;
                    } else {
                        design_num = MAINSCR::Get_Ship_Stack_For_Officers_Screen();
                    }
                } else {
                    design_num = MAINSCR::Get_Star_Id_For_Officers_Screen();
                }
                MOX::temp_star_handle = design_num;
                officer_screen_return_screen = SCREEN_MAIN;
                MOX::current_screen = SCREEN_OFFICERS;
                exit_flag = 1;
                MOX::return_screen = SCREEN_MAIN;
            }

            if (input == research_window_field && MOX::skip_fields == 0) {
                MOX::dont_draw_main_screen_to_back = 1;
                fields::Clear_Fields();
                MAINSCR::Clear_Main_Screen_Fields();
                video::Set_Page_Off();
                MAINSCR::Draw_Mini_Main_Screen();
                video::Copy_Off_To_Back();
                RUSS::Mox_Sync_Update();
                video::Set_Refresh_Stencil();
                if (MOVEBOX::Moveable_Box_Selected(2)) {
                    MAINSCR2::force_main_screen_fade = 1;
                }
                MOX::return_screen = SCREEN_MAIN;
                clear_fleet_fields = 0;
                MOX::current_screen = SCREEN_TECH_CHANGE;
                exit_flag = 1;
            } else if (input == freighters_window_field) {
                if (MOX::player[MOX::PLAYER_NUM].n_freighters < 1) {
                    const char* msg = HAROLD::H_Message(HESTR_0DF_YOU_DO_NOT_HAVE_ANY_FR);
                    strlcpy(msg_buffer, msg, sizeof(msg_buffer));
                    HAROLD::User_Box(&MOX::draw_main_screen_to_back, msg_buffer, 3);
                } else {
                    MOX::return_screen = SCREEN_MAIN;
                    clear_fleet_fields = 0;
                    MOX::current_screen = HESTR_023_ONE_OR_MORE_SHIPS_CHOS;
                    exit_flag = 1;
                }
            } else if (input == command_points_window_field && MOX::skip_fields == 0) {
                MOX::return_screen = SCREEN_MAIN;
                clear_fleet_fields = 0;
                MOX::current_screen = HESTR_02B_COST;
                exit_flag = 1;
            } else if (input == treasury_window_field) {
                input = -1;
                fields::Clear_Fields();
                MAINSCR::Clear_Main_Screen_Fields();
                fields::Assign_Auto_Function(MAINSCR::Draw_Mini_Main_Screen, 1);
                video::Set_Page_Off();
                MAINSCR::Draw_Mini_Main_Screen();
                video::Copy_Off_To_Back();
                video::Set_Refresh_Stencil();
                RUSS::Mox_Sync_Update();
                MAINSCR::Tax_Rate_Popup();
                COLCALC::Do_Player_Colony_Calculations(MOX::PLAYER_NUM);
                MOX::current_screen = SCREEN_MAIN;
                exit_flag = 1;
                clear_fleet_fields = 0;
            }

            // Zoom Logic
            if ((input == zoom_out_button && MOX::cur_map_scale < MOX::max_map_scale) ||
                (input == zoom_in_button && HAROLD::Map_Scale_To_Zoom_Count() > 0)) {
                Clear_Twinkling_Star_Data();
                fields::Deactivate_Help_List();
            }

            if (input == zoom_in_button && HAROLD::Map_Scale_To_Zoom_Count() > 0 && MOX::skip_fields == 0) {
                if (boxes_saved_flag == 0) {
                    MOVEBOX::Save_And_Remove_Boxes();
                }
                boxes_saved_flag += 1;

                if (MOX::zoom_in == 0) {
                    fields::Deactivate_Help_List();
                    MOX::zoom_in = 1;
                    palette::Set_Palette_Changes(0, 0xff);
                    remap::Update_Glass_Remap_Colors();
                    video::Set_Refresh_Full();
                    video::Set_Page_Off();
                    MAINSCR::Draw_Mini_Main_Screen();
                    allocate::Push_Block(MOX::screen_seg);
                    uint32_t bitmap_size = bitmap::Get_Bitmap_Size(0x1fb, 0x191);
                    temp_bitmap = (s_animation_header*)allocate::Allocate_Next_Block(MOX::screen_seg, bitmap_size);
                    bitmap::Screen_Bitmap_Capture(0x16, 0x16, 0x20f, 0x1a5, temp_bitmap);
                    allocate::Pop_Block(MOX::screen_seg);
                    MISC::_Darken_Fill(0x16, 0x16, 0x20f, 0x1a5);
                    video::Copy_Off_To_Back();
                    video::Set_Refresh_Stencil();
                    fields::Assign_Auto_Function(MAINSCR::Draw_Main_Screen_For_Zooming, 1);
                }

                MOX::zoom_in = 1;
                fields::Clear_Fields_Above(last_field);
                MAINSCR::Add_Map_Fields();
                mouse::Set_Mouse_List(MOX::mouse_list_zoom_mode, 5);

                HAROLD::Decrement_Zoom_Count();

                MOX::draw_main_screen_to_back = 1;
            } else if (input == zoom_out_button && MOX::zoom_in == 1 && MOX::skip_fields == 0) {
                if (HAROLD::Increment_Zoom_Count_With_Limit() == 0) {
                    MAINSCR::Exit_Zooming_Mode(&boxes_saved_flag);
                }
            } else if (input == zoom_out_button && MOX::cur_map_scale < MOX::max_map_scale && MOX::skip_fields == 0) {
                int old_x = MOX::cur_map_x;
                int old_y = MOX::cur_map_y;
                int z1 = HAROLD::Map_Scale_To_Zoom_Count();
                HAROLD::Increment_Zoom_Count();
                MOX::cur_map_scale = HAROLD::Get_Map_Scale(MOX::zoom_count);
                int z2 = HAROLD::Map_Scale_To_Zoom_Count();

                // Centering logic
                int diff_w = (z1 - z2) * 0x1fa;
                MOX::cur_map_x += (diff_w + ((diff_w < 0) ? 3 : 0)) / 4;

                int diff_h = (z1 - z2) * 400;
                MOX::cur_map_y += (diff_h + ((diff_h < 0) ? 3 : 0)) / 4;

                // Boundaries
                int scale_diff = MOX::max_map_scale - MOX::cur_map_scale;
                int max_x = (scale_diff * 0x1fa) / 10;
                int max_y = (scale_diff * 400) / 10;

                if (max_x < MOX::cur_map_x) MOX::cur_map_x = max_x;
                if (max_y < MOX::cur_map_y) MOX::cur_map_y = max_y;
                if (MOX::cur_map_x < 0) MOX::cur_map_x = 0;
                if (MOX::cur_map_y < 0) MOX::cur_map_y = 0;
                if (MOX::max_map_scale == MOX::cur_map_scale) {
                    MOX::cur_map_x = 0;
                    MOX::cur_map_y = 0;
                }

                int limit_x = HAROLD::Get_Up_Scaled_Value(MOX::MAP_MAX_X - 0x280);
                if (limit_x < MOX::cur_map_x) {
                    MOX::cur_map_x = HAROLD::Get_Up_Scaled_Value(MOX::MAP_MAX_X - 0x280);
                }

                int limit_y = MOX::MAP_MAX_Y - HAROLD::Get_Up_Scaled_Value(400);
                if (limit_y < MOX::cur_map_y) {
                    MOX::cur_map_y = limit_y;
                }

                MOX::draw_main_screen_to_back = 1;
                MAINSCR::Get_New_Paralax_XYs(MOX::cur_map_x - old_x, MOX::cur_map_y - old_y, 1);
            } else if (input == whole_screen_field && MOX::zoom_in == 1 && MOX::skip_fields == 0) {
                MOVEBOX::Restore_Boxes();
                boxes_saved_flag = 0;
                MOX::zoom_in = 0;
                EVANHELP::Set_Main_Screen_Help_List();
                mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);
                fields::Assign_Auto_Function(MAINSCR::Draw_Main_Screen, 1);

                MOX::cur_map_x += HAROLD::Get_Up_Scaled_Value(zoom_box_x - 0x15);
                MOX::cur_map_y += HAROLD::Get_Up_Scaled_Value(zoom_box_y - 0x15);
                MOX::cur_map_scale = HAROLD::Get_Map_Scale(MOX::zoom_count);

                MOX::far_star_x = 0x16;
                MOX::mid_star_x = 0x16;
                MOX::near_star_x = 0x16;
                MOX::far_star_y = 0x16;
                MOX::mid_star_y = 0x16;
                MOX::near_star_y = 0x16;
                MOX::draw_main_screen_to_back = 1;
            }

            // Scroll map logic
            if (-MOX::main_screen_grid_field == input && MOX::skip_fields == 0 && MOX::max_map_scale != MOX::cur_map_scale) {
                int target_y = HAROLD::Get_Up_Scaled_Value(MOX::main_screen_grid_y) + MOX::cur_map_y;
                int target_x = HAROLD::Get_Up_Scaled_Value(MOX::main_screen_grid_x) + MOX::cur_map_x;
                MAINSCR::Scroll_Map(target_x, target_y);
            }

            // Redraw cycle
            if (exit_flag == 0 && MOX::global_esc == 0) {
                settlement_view.Initialized();
                game_events::s_observation_scope main_draw(game_events::MAIN_VIEW_DRAW_ENTERED, game_events::MAIN_VIEW_DRAW_LEAVING);
                if (MOX::skip_fields == 0) {
                    fields::Clear_Fields_Above(last_field);
                    fields::Enable_Draw_All_Fields();
                    fields::Enable_Mouse_Pressed_Field_Update();
                } else {
                    MOX::skip_fields = 0;
                    fields::Disable_Draw_All_Fields();
                    fields::Clear_Fields();
                    MAINSCR::Clear_Main_Screen_Fields();
                    MAINSCR::Add_Cheats();
                }
                MAINSCR::Add_Map_Fields();

                if (MOX::zoom_in == 1) {
                    video::Set_Page_Off();
                    MAINSCR::Draw_Main_Screen_For_Zooming();
                } else {
                    video::Set_Page_Off();
                    MAINSCR::Draw_Main_Screen();
                }
                RUSS::Mox_Sync_Update();
            }

            // Mouse click logic
            if (MOVEBOX::Moveable_Box_Selected(2)) {
                if (mouse_btn_pressed != 0) {
                    if (mouse::Mouse_Button() == 0) {
                        mouse_btn_pressed = 0;
                        fields::Set_Input_Delay(2);
                    }
                }
                FLEETPOP::Save_Ship_Selection_Status();
                if (!FLEETPOP::Some_Icon_Selected()) {
                    MOX::eta_star_id = -1;
                }
            }

            timer::Release_Time(1);
        }

        // Cleanup
        settlement_view.Close();
        MAINSCR::Set_Reload_Fleet_Box_Palette_Flag();
        fields::Deactivate_Auto_Function();
        MOX::first_fleet_movement_icon = 0;
        if (clear_fleet_fields != 0) {
            MOX::first_node_ptr_in_stack_ = -1;
        }

        MOX::eta_star_id = -1;
        MOX::stencil_box_id = -1;
        MOX::draw_main_screen_stencilled = 0;

        SYS::Clear_System_Display_Loaded_Flag();
        MOX::animated_star = -1;
        fields::Deactivate_Help_List();
        already_faded_into_main_screen = 0;
        MOX::reloaded_ship_palette = 0;
    }

    /**
     * Draws one main-screen frame, including map contents, overlays, and movable boxes.
     */
    void __cdecl Draw_Main_Screen() {
        game_events::s_settlement_frame settlements;
        game_events::s_observation_scope frame(game_events::MAIN_SCREEN_FRAME_ENTERED, game_events::MAIN_SCREEN_FRAME_LEAVING);
        if (MOX::stencil_box_id >= 0 && MOX::stencil_box_id <= 4) {
            Draw_Main_Screen_While_Moving_Boxes();
            video::Clip_Off();
            graphics::Reset_Window();
        } else {
            graphics::Set_Window(0x16, 0x16, 0x20F, 0x1A5);
            video::Clip_On();

            if (MOX::draw_main_screen_to_back == 1 || MOX::FULL_DRAW_SCREEN_FLAG != 0) {
                Draw_Main_Screen_Filled(1);
                MOX::draw_main_screen_to_back = 0;
            } else {
                Draw_Wormhole_Links();
                Draw_Relocation_Links();
                Print_Star_Names();
            }

            graphics::Set_Window(0x16, 0x16, 0x20F, 0x1A5);
            video::Clip_On();

            if (MOX::settings.end_of_turn_wait != 0) {
                s_colors colors = {{0, 10, 12, 14, 16, 18, 20, 22}};
                char text_buffer[80];
                int32_t stardate = (int32_t)MOX::stardate;
                const char* format = HAROLD::H_Message(0xDA);

                snprintf(text_buffer, sizeof(text_buffer), format, stardate / 10, stardate % 10);

                fonts::Set_Font_Style(1, &colors);
                fonts::Print_Right(0x27F, 0, text_buffer);
            }

            Draw_Black_Holes();
            Draw_Gate_Icons();
            SHIPS::Do_Ship_Destination_Lines();
            const bool draw_star_highlights_after_stars =
                MAP_SCALE::Is_Extended_Max_Map_View_(
                    MOX::NUM_STARS,
                    ORIGINAL_MAX_STARS,
                    MOX::cur_map_scale,
                    MOX::max_map_scale);

            if (MOX::eta_star_id > -1 && MOX::fleet_ship_scanned == -1) {
                Draw_ETA_Destination_Line();
                if (!draw_star_highlights_after_stars) {
                    Highlight_Star(MOX::eta_star_id);
                }
            }

            if (main_screen_scanned_star != -1 && !draw_star_highlights_after_stars) {
                Highlight_Star(main_screen_scanned_star);
            }

            Draw_Stars();
            Draw_Ship_Icons(false);

            if (draw_star_highlights_after_stars) {
                if (MOX::eta_star_id > -1 && MOX::fleet_ship_scanned == -1) {
                    Highlight_Star(MOX::eta_star_id);
                }
                if (main_screen_scanned_star != -1) {
                    Highlight_Star(main_screen_scanned_star);
                }
            }

            if (mouse::Mouse_Button() == 1) {
                if (MOVEBOX::Moveable_Box_Selected(2)) {
                    if (FLEETPOP::Can_Select_Fleet_Box_Ship(0) && MOVEBOX::Mouse_Within_Fleet_Box()) {
                        int16_t scanned_field = fields::Scan_Input();
                        if (scanned_field != MOX::moveable_box[2].drag_field) {
                            if (mouse_btn_pressed == 0) {
                                fltpop_field_selection_status = (FLEETPOP::Icon_Selection_Status(scanned_field) == 0);
                            }
                            Set_Painted_Fleet_Fields(scanned_field);
                        }
                        mouse_btn_pressed = 1;
                    }
                }
            }

            for (int16_t i = 4; i >= 0; --i) {
                int16_t box_id = MOX::moveable_box_order[i];
                if (MOVEBOX::Moveable_Box_Selected(box_id)) {
                    if (box_id == 0) {
                        Highlight_Star(MOX::moveable_box[0].id);
                    }
                    if (box_id == 2) {
                        Draw_Fleet_Box_Selected_Ship_Seg();
                    }
                }
            }

            if (main_screen_ship_icon_scanned > -1 && main_screen_ship_icon_scanned < MOX::ship_icon_count) {
                Draw_Scanned_Ship_Seg();
            }

            for (int16_t i = 4; i >= 0; --i) {
                int16_t box_id = MOX::moveable_box_order[i];
                if (MOVEBOX::Moveable_Box_Selected(box_id) && box_id != (int16_t)MOX::stencil_box_id) {
                    int16_t btn = mouse::Mouse_Button();
                    if (btn != 0) {
                        int16_t target_field = MOX::moveable_box[box_id].drag_field;
                        if (target_field == fields::Auto_Input() || MOX::moveable_box[box_id].active == 1) {
                            MOVEBOX::Move_Moveable_Box(box_id, btn);
                        }
                    }
                    MOVEBOX::Draw_Moveable_Box(box_id);
                }
            }

            if (MOX::galaxy_map_scanned_star > -1 && MOX::galaxy_map_scanned_star < MOX::NUM_STARS) {
                Print_Galaxy_Map_Scanned_Star();
            }

            video::Clip_Off();
            graphics::Reset_Window();
            Print_Main_Screen_Data();

            int32_t next_idx = (int32_t)MOX::multi_colored_line_start + 1;
            MOX::multi_colored_line_start = (int16_t)(next_idx % 8);
        }

        if (MOX::game_type == 2 || MOX::game_type == 3) {
            Draw_Diplomacy_Request_Lights();
        }
        if (MOX::game_type == 1 || MOX::game_type == 2 || MOX::game_type == 3) {
            Draw_Player_Done_Lights();
        }

        Check_Mouse_XY_Fields();

        if (MOX::FULL_DRAW_SCREEN_FLAG != 0) {
            MOX::skip_fields = 1;
            MOX::FULL_DRAW_SCREEN_FLAG = 0;
        }

        MOX::red_color_pulse_value += MOX::red_color_pulse_incr;
        if (MOX::red_color_pulse_value == 0 || MOX::red_color_pulse_value == 8) {
            MOX::red_color_pulse_incr = -MOX::red_color_pulse_incr;
        }
    }
} // namespace MAINSCR