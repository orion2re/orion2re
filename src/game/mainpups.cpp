#include "pch.h"
#include "string_overrides.h"
#include "game_events.h"

namespace MAINPUPS {
    char* g_msg_ptr;

    s_animation_header* anims[38];

    uint8_t on_officer_screen_flag;

    int16_t g_officer = -1;

    int16_t officer_gains_level_state;

    int16_t last_scanned_field;
    int16_t g_state;
    int8_t selection_state_flag;
    uint8_t g_using_colony_ship;
    int16_t g_max;
    int16_t g_min;
    char* g_msg;
    int16_t g_var;
    int16_t g_temp_tax_rate;
    int16_t field_state;
    s_colors main_scr_font_colors = {{0xA0, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9}};

    uint8_t g_special;
    int16_t g_x;
    int16_t g_y;
    int16_t g_star_n = -1;
    int16_t g_player_n = -1;
    int16_t field_mode;
    int16_t g_scanned_star = -1;
    int16_t g_old_scanned_star = -1;
    uint8_t no_flicker_between_mainpups_flag;
    int16_t autofunction_ctr;
    int16_t num_field_saves;
    s_fieldsav_data stats_array[3];
    uint8_t explode_flag = 1;
    int16_t g_refresh_mode;
    MOX2::action_func_t eric_saved_autofunction2;
    MOX2::action_func_t eric_saved_autofunction;

    MOX2::action_func_t temp_af;

    s_colony_combat_popup_data* g_data_ptr;

    int16_t mp_fields[26];

    void __cdecl Load_Colony_Fields() {
        allocate::Push_Block(MOX::screen_seg);
        anims[0] = (s_animation_header*)farload::Far_Reload_Next(COLONY::MAINPUPS_LBX, 0, MOX::screen_seg);
    }

    void __cdecl Load_SLIDER_Popup() {
        allocate::Push_Block(MOX::screen_seg);
        anims[14] = (s_animation_header*)farload::Far_Reload_Next(COLONY::MAINPUPS_LBX, 0x34, MOX::screen_seg);
        anims[34] = (s_animation_header*)farload::Far_Reload_Next(COLONY::MAINPUPS_LBX, 0x4a, MOX::screen_seg);
        anims[33] = (s_animation_header*)farload::Far_Reload_Next(COLONY::MAINPUPS_LBX, 0x4b, MOX::screen_seg);
        anims[35] = (s_animation_header*)farload::Far_Reload_Next(COLONY::MAINPUPS_LBX, 0x4c, MOX::screen_seg);
        anims[4] = (s_animation_header*)farload::Far_Reload_Next(COLONY::MAINPUPS_LBX, 0x4d, MOX::screen_seg);
    }

    void __cdecl Load_Tax_Rate_Popup() {
        allocate::Push_Block(MOX::screen_seg);
        anims[27] = (s_animation_header*)farload::Far_Reload_Next(COLONY::MAINPUPS_LBX, 0x3E, MOX::screen_seg);
        anims[28] = (s_animation_header*)farload::Far_Reload_Next(COLONY::MAINPUPS_LBX, 0x3F, MOX::screen_seg);
        anims[29] = (s_animation_header*)farload::Far_Reload_Next(COLONY::MAINPUPS_LBX, 0x40, MOX::screen_seg);
        anims[30] = (s_animation_header*)farload::Far_Reload_Next(COLONY::MAINPUPS_LBX, 0x41, MOX::screen_seg);
        anims[31] = (s_animation_header*)farload::Far_Reload_Next(COLONY::MAINPUPS_LBX, 0x42, MOX::screen_seg);
        anims[32] = (s_animation_header*)farload::Far_Reload_Next(COLONY::MAINPUPS_LBX, 0x43, MOX::screen_seg);
        anims[35] = (s_animation_header*)farload::Far_Reload_Next(COLONY::MAINPUPS_LBX, 0x44, MOX::screen_seg);
        anims[36] = (s_animation_header*)farload::Far_Reload_Next(COLONY::MAINPUPS_LBX, 0x45, MOX::screen_seg);
    }

    void __cdecl Load_Colony_Combat_Popup() {
        allocate::Push_Block(MOX::screen_seg);

        anims[17] = (s_animation_header*)farload::Far_Reload_Next(COLONY::MAINPUPS_LBX, 53 /* 0x35 */, MOX::screen_seg);
        anims[15] = (s_animation_header*)farload::Far_Reload_Next(COLONY::MAINPUPS_LBX, 54 /* 0x36 */, MOX::screen_seg);
        anims[16] = (s_animation_header*)farload::Far_Reload_Next(COLONY::MAINPUPS_LBX, 55 /* 0x37 */, MOX::screen_seg);
        anims[18] = (s_animation_header*)farload::Far_Reload_Next(COLONY::MAINPUPS_LBX, 60 /* 0x3c */, MOX::screen_seg);
        anims[3] = (s_animation_header*)farload::Far_Reload_Next(COLONY::MAINPUPS_LBX, 78 /* 0x4e */, MOX::screen_seg);
        anims[19] = (s_animation_header*)farload::Far_Reload_Next(COLONY::MAINPUPS_LBX, 79 /* 0x4f */, MOX::screen_seg);
        anims[20] = (s_animation_header*)farload::Far_Reload_Next(COLONY::MAINPUPS_LBX, 80 /* 0x50 */, MOX::screen_seg);
        anims[21] = (s_animation_header*)farload::Far_Reload_Next(COLONY::MAINPUPS_LBX, 81 /* 0x51 */, MOX::screen_seg);
        anims[9] = (s_animation_header*)farload::Far_Reload_Next(COLONY::MAINPUPS_LBX, 82 /* 0x52 */, MOX::screen_seg);

        anims[22] = (s_animation_header*)farload::Far_Reload_Next(COLONY::COLONY2_LBX, 46 /* 0x2e */, MOX::screen_seg);
        anims[23] = (s_animation_header*)farload::Far_Reload_Next(COLONY::COLONY2_LBX, 47 /* 0x2f */, MOX::screen_seg);
        anims[24] = (s_animation_header*)farload::Far_Reload_Next(COLONY::COLONY2_LBX, 48 /* 0x30 */, MOX::screen_seg);

        anims[25] = (s_animation_header*)farload::Far_Reload_Next(COLONY::MAINPUPS_LBX, 61 /* 0x3d */, MOX::screen_seg);
        anims[26] = (s_animation_header*)farload::Far_Reload_Next(COLONY::MAINPUPS_LBX, 73 /* 0x49 */, MOX::screen_seg);

        Load_Combat_Colony_Picture_To_Screen_Seg(g_data_ptr->colony_idx);
    }

    void __cdecl Load_System_Discovery_Popup() {
        allocate::Push_Block(MOX::screen_seg);

        anims[0] = (s_animation_header*)farload::Far_Reload_Next(COLONY::MAINPUPS_LBX, 0, MOX::screen_seg);
        anims[1] = (s_animation_header*)farload::Far_Reload_Next(COLONY::MAINPUPS_LBX, 1, MOX::screen_seg);
    }

    void __cdecl Get_Picture_Draw_XY(s_animation_header* anim) {
        g_x = 0x140 - (animate::Get_Width(anim) / 2);
        g_y = 0xF0 - (animate::Get_Height(anim) / 2);
    }

    void __cdecl Do_Popup_Setup(int16_t star_idx, s_animation_header* popup_anim) {
        if (MOX::current_screen != SCREEN_MAIN) {
            explode_flag = 0;
        }

        g_refresh_mode = video::Get_Refresh_Mode();
        video::Set_Refresh_Stencil();
        Get_Picture_Draw_XY(popup_anim);

        if (star_idx == -1) {
            star_idx = 0;
            if (MOX::player[MOX::PLAYER_NUM].home_planet_id > 0) {
                star_idx = MOX::planet[MOX::player[MOX::PLAYER_NUM].home_planet_id].star_index;
            }
        }

        g_star_n = star_idx;
        COLONY::sys_disp_star_handle = star_idx;

        fields::Clear_Mouse_Buffer();
        fields::Set_Input_Delay(3);
        mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);
        graphics::Reset_Window();
        video::Clip_Off();
        video::Copy_Off_To_Back();
        video::Set_Page_Off();

        if (explode_flag != 0) {
            if (MAINSCR::Star_On_Screen(star_idx)) {
                MAINSCR::Center_Map(MOX::star[star_idx].x, MOX::star[star_idx].y);
            }
        }

        if (autofunction_ctr == 0) {
            MOX2::Get_Auto_Function(&eric_saved_autofunction);
            MOX2::Execute_Auto_Function(&eric_saved_autofunction);

            bool skip_update = false;
            if (explode_flag != 0) {
                if (MAINSCR::Star_On_Screen(star_idx)) {
                    skip_update = true;
                }
            }

            if (!skip_update && no_flicker_between_mainpups_flag == 0) {
                RUSS::Mox_Sync_Update();
                COLONY::full_draw = 1;
                MOX2::Execute_Auto_Function(&eric_saved_autofunction);
            }
        } else if (autofunction_ctr == 1) {
            MOX2::Get_Auto_Function(&eric_saved_autofunction2);
            COLONY::full_draw = 1;
            MOX2::Execute_Auto_Function(&eric_saved_autofunction);
            COLONY::full_draw = 1;
            MOX2::Execute_Auto_Function(&eric_saved_autofunction2);
        }

        if (autofunction_ctr == 0 || autofunction_ctr == 1) {
            autofunction_ctr++;
        }

        if (explode_flag != 0) {
            int16_t draw_x, draw_y;
            MAINSCR::Get_Star_Draw_Coords(star_idx, &draw_x, &draw_y);
            fields::Draw_Visible_Fields();

            if (MOX::settings.expanding_help != 0 && num_field_saves < 1) {
                if (MOX::cur_map_scale != MOX::max_map_scale) {
                    if (!MAINSCR::Star_On_Screen(star_idx)) {
                        MAINSCR::Center_Map(MOX::star[star_idx].x, MOX::star[star_idx].y);
                        MAINSCR::Get_Star_Draw_Coords(star_idx, &draw_x, &draw_y);
                    }
                }

                int16_t height = animate::Get_Height(popup_anim);
                int16_t width = animate::Get_Width(popup_anim);
                MAINSCR::Draw_Exploding_Plasma_Box(draw_x, draw_y, g_x, g_y, width, height, -1);
            }
        }

        if (no_flicker_between_mainpups_flag != 0) {
            no_flicker_between_mainpups_flag = 0;
        }

        COLONY::full_draw = 1;
    }

    void __cdecl Eric_Push_Fields() {
        FIELDSAV::Save_Field_Stats(&stats_array[num_field_saves]);
        num_field_saves++;
    }

    void __cdecl Add_Tax_Rate_Fields() {
        Eric_Push_Fields();

        mp_fields[25] = fields::Add_Button_Field(
            0xed, 0xfe, ESTRINGS::E_Strings(0xc), anims[35], ESTRINGS::E_Strings(0x87), 0x28);

        mp_fields[19] = fields::Add_Multi_Button_Field(
            0x93, 0xc6, ESTRINGS::E_Strings(0xc), anims[27], &g_temp_tax_rate, 0, ESTRINGS::E_Strings(0x8e), 0x29);

        mp_fields[20] = fields::Add_Multi_Button_Field(
            0xc1, 0xc6, ESTRINGS::E_Strings(0xc), anims[28], &g_temp_tax_rate, 10, ESTRINGS::E_Strings(0x93), 0x29);

        mp_fields[21] = fields::Add_Multi_Button_Field(
            0xea, 0xc6, ESTRINGS::E_Strings(0xc), anims[29], &g_temp_tax_rate, 20, ESTRINGS::E_Strings(0x95), 0x29);

        mp_fields[22] = fields::Add_Multi_Button_Field(
            0x113, 0xc6, ESTRINGS::E_Strings(0xc), anims[30], &g_temp_tax_rate, 30, ESTRINGS::E_Strings(0x97), 0x29);

        mp_fields[23] = fields::Add_Multi_Button_Field(
            0x13c, 0xc6, ESTRINGS::E_Strings(0xc), anims[31], &g_temp_tax_rate, 40, ESTRINGS::E_Strings(0x99), 0x29);

        mp_fields[24] = fields::Add_Multi_Button_Field(
            0x165, 0xc6, ESTRINGS::E_Strings(0xc), anims[32], &g_temp_tax_rate, 50, ESTRINGS::E_Strings(0x9b), 0x29);
    }

    void __cdecl Draw_Tax_Rate_Popup() {
        if (COLONY::full_draw != 0 || MOX::FULL_DRAW_SCREEN_FLAG != 0) {
            MOX::draw_main_screen_to_back = 1;
            Mainpups_Tested_Execute_Autofunction();
            COLONY::full_draw = 1;
            animate::Draw(0, 0, anims[36]);
            video::Copy_Off_To_Back();
            COLONY::full_draw = 0;
        }
    }

    void __cdecl Evaluate_Tax_Rate_Input(int16_t* input_val, int16_t* scan_val, int16_t* exit_flag) {
        if (mp_fields[25] == *input_val) {
            *exit_flag = 1;
            MOX::player[MOX::PLAYER_NUM].tax_rate = static_cast<uint8_t>(g_temp_tax_rate);
        } else if (*input_val != 0) {
            MOX::player[MOX::PLAYER_NUM].tax_rate = static_cast<uint8_t>(g_temp_tax_rate);
            COLCALC::Do_Player_Colony_Calculations(MOX::PLAYER_NUM);
            COLONY::full_draw = 1;
        }

        if (MOX::FULL_DRAW_SCREEN_FLAG != 0) {
            COLONY::full_draw = 1;
        }
    }

    void __cdecl Eric_Pop_Fields() {
        if (num_field_saves > 0) {
            num_field_saves--;
            FIELDSAV::Restore_Field_Stats(&stats_array[num_field_saves]);
        }
    }

    void __cdecl Do_Popup_Cleanup() {
        Eric_Pop_Fields();
        fields::Deactivate_Auto_Function();

        if (autofunction_ctr == 1) {
            MOX2::Put_Auto_Function(&eric_saved_autofunction);
            autofunction_ctr--;
        } else if (autofunction_ctr == 2) {
            MOX2::Put_Auto_Function(&eric_saved_autofunction2);
            autofunction_ctr--;
        }

        video::Copy_Back_To_Off();
        MOX::FULL_DRAW_SCREEN_FLAG = 1;
        allocate::Pop_Block(MOX::screen_seg);
        ERIC::Set_Refresh_Mode(g_refresh_mode);
        MOX::draw_system_display_without_button = 0;
        explode_flag = 1;
        fields::Deactivate_Help_List();
        MOX::need_to_restore_help_list = 1;
    }

    void __cdecl Clear_System_Special_Slots() {
        anims[7] = nullptr;
        anims[0] = nullptr;
        g_msg_ptr = nullptr;
    }

    void __cdecl Clear_New_Officer_Slots() {
        anims[10] = nullptr;
        MAINPUPS::g_msg_ptr = nullptr;
    }

    void __cdecl Update_Officer_Gains_Level_Fields() {
        MAINPUPS::Eric_Push_Fields();
        MAINPUPS::Add_Officer_Gains_Level_Fields();
        ERICHELP::Set_Officer_Information_Popup_Help_List();
    }

    void __cdecl Add_Officer_Gains_Level_Fields() {
        char* hotkey;
        int16_t sound;

        sound = 0;
        hotkey = ESTRINGS::E_Strings(12 /* 0xc */);

        // Creates a full-screen (640x480) hidden input field
        MAINPUPS::mp_fields[0] = fields::Add_Hidden_Field(0, 0, 639 /* 0x27f */, 479 /* 0x1df */, hotkey, (int8_t)sound);
    }

    void __cdecl Build_Generic_Planet_Or_Colony_Description_String_Formatted(int16_t planet_idx, char* buffer, uint32_t buffer_size) {
        if (buffer == nullptr || buffer_size == 0) {
            return;
        }

        char chunk[256];
        buffer[0] = '\0';
        s_planet_data* p = &MOX::planet[planet_idx];

        if (p->colony_index == -1) {
            if (p->planet_type != PLANET_TYPE_PLANET) {
                char* planet_name = HACCESS::Get_Planet_Name(planet_idx);
                snprintf(chunk, sizeof(chunk), ESTRINGS::E_Strings(0x56), planet_name);
                strlcat(buffer, chunk, buffer_size);
                strlcat(buffer, ESTRINGS::E_Strings(0x133), buffer_size);
                return;
            }

            s_potential_colony_info potential_info;
            COLCALC::Compute_Potential_Colony_Info(&MOX::player[MOX::PLAYER_NUM], p, &potential_info);

            char* planet_name = HACCESS::Get_Planet_Name(planet_idx);
            /* fmtpara \x1A0 and \x1A1 toggle inline paragraph justification for these stat lines. */
            snprintf(chunk, sizeof(chunk), ESTRINGS::E_Strings(0x52), "\x1A"
                                                                       "0",
                planet_name);
            strlcat(buffer, chunk, buffer_size);

            char* climate_str = MOX::planet_climate_string[p->climate];
            char* size_str = MOX::planet_size_string[p->size];
            snprintf(chunk, sizeof(chunk), ESTRINGS::E_Strings(0x4D), "\x1A"
                                                                       "0",
                size_str,
                climate_str);
            strlcat(buffer, chunk, buffer_size);

            char* mineral_str = MOX::mineral_class_string[p->mineral_class];
            snprintf(chunk, sizeof(chunk), ESTRINGS::E_Strings(0x6F), "\x1A"
                                                                       "0",
                mineral_str);
            strlcat(buffer, chunk, buffer_size);

            char* gravity_str = MOX::planet_gravity_string[p->gravity_class];
            snprintf(chunk, sizeof(chunk), ESTRINGS::E_Strings(0x53), "\x1A"
                                                                       "0",
                gravity_str);
            strlcat(buffer, chunk, buffer_size);

            if ((potential_info.food % 2) == 0) {
                snprintf(chunk, sizeof(chunk), ESTRINGS::E_Strings(0x65), "\x1A"
                                                                           "0",
                    "\x1A"
                    "1",
                    potential_info.food / 2);
            } else {
                snprintf(chunk, sizeof(chunk), ESTRINGS::E_Strings(0x64), "\x1A"
                                                                           "0",
                    "\x1A"
                    "1",
                    potential_info.food / 2,
                    potential_info.food % 2);
            }
            strlcat(buffer, chunk, buffer_size);

            snprintf(chunk, sizeof(chunk), ESTRINGS::E_Strings(0x6B), "\x1A"
                                                                       "0",
                "\x1A"
                "1",
                potential_info.industry);
            strlcat(buffer, chunk, buffer_size);

            snprintf(chunk, sizeof(chunk), ESTRINGS::E_Strings(0x74), "\x1A"
                                                                       "0",
                "\x1A"
                "1",
                potential_info.research);
            strlcat(buffer, chunk, buffer_size);

            snprintf(chunk, sizeof(chunk), ESTRINGS::E_Strings(0x6C), "\x1A"
                                                                       "0",
                "\x1A"
                "1",
                potential_info.maintenance);
            strlcat(buffer, chunk, buffer_size);

            snprintf(chunk, sizeof(chunk), ESTRINGS::E_Strings(0x79), "\x1A"
                                                                       "0",
                "\x1A"
                "1",
                potential_info.worker_bonus);
            strlcat(buffer, chunk, buffer_size);

            int16_t max_pop = COLCALC::Planet_Max_Population_For_Player(p, MOX::PLAYER_NUM);
            snprintf(chunk, sizeof(chunk), ESTRINGS::E_Strings(0x6D), "\x1A"
                                                                       "0",
                "\x1A"
                "1",
                max_pop);
            strlcat(buffer, chunk, buffer_size);
        } else {
            s_colony* c = &MOX::colony[p->colony_index];
            char* race_name = MOX::player[c->owner].race_name;
            char* planet_name = HACCESS::Get_Planet_Name(planet_idx);

            /* fmtpara \x1A0 and \x1A1 toggle inline paragraph justification for these stat lines. */
            snprintf(chunk, sizeof(chunk), ESTRINGS::E_Strings(0x49), "\x1A"
                                                                       "0",
                planet_name,
                race_name);
            strlcat(buffer, chunk, buffer_size);

            char* climate_str = MOX::planet_climate_string[static_cast<uint8_t>(c->climate)];
            char* size_str = MOX::planet_size_string[p->size];
            snprintf(chunk, sizeof(chunk), ESTRINGS::E_Strings(0x4D), "\x1A"
                                                                       "0",
                size_str,
                climate_str);
            strlcat(buffer, chunk, buffer_size);

            char* mineral_str = MOX::mineral_class_string[p->mineral_class];
            snprintf(chunk, sizeof(chunk), ESTRINGS::E_Strings(0x6F), "\x1A"
                                                                       "0",
                mineral_str);
            strlcat(buffer, chunk, buffer_size);

            char* gravity_str = MOX::planet_gravity_string[p->gravity_class];
            snprintf(chunk, sizeof(chunk), ESTRINGS::E_Strings(0x53), "\x1A"
                                                                       "0",
                gravity_str);
            strlcat(buffer, chunk, buffer_size);

            snprintf(chunk, sizeof(chunk), ESTRINGS::E_Strings(0x5E), "\x1A"
                                                                       "0",
                "\x1A"
                "1",
                static_cast<uint8_t>(c->food2_per_farmer) / 2);
            strlcat(buffer, chunk, buffer_size);

            snprintf(chunk, sizeof(chunk), ESTRINGS::E_Strings(0x5F), "\x1A"
                                                                       "0",
                "\x1A"
                "1",
                static_cast<uint8_t>(c->industry_per_worker));
            strlcat(buffer, chunk, buffer_size);

            snprintf(chunk, sizeof(chunk), ESTRINGS::E_Strings(0x60), "\x1A"
                                                                       "0",
                "\x1A"
                "1",
                static_cast<uint8_t>(c->research_per_scientist));
            strlcat(buffer, chunk, buffer_size);

            if (c->owner == MOX::PLAYER_NUM) {
                snprintf(chunk, sizeof(chunk), ESTRINGS::E_Strings(0x70), "\x1A"
                                                                           "0",
                    "\x1A"
                    "1",
                    c->morale * 5);
                strlcat(buffer, chunk, buffer_size);
            }

            int16_t max_pop = COLCALC::Planet_Max_Population_For_Player(p, c->owner);
            snprintf(chunk, sizeof(chunk), ESTRINGS::E_Strings(0x71), "\x1A"
                                                                       "0",
                "\x1A"
                "1",
                static_cast<uint8_t>(c->n_pops),
                max_pop);
            strlcat(buffer, chunk, buffer_size);
        }

        if (p->planet_special != SYSTEM_SPECIAL_NO_SPECIAL) {
            char* special_str = MOX::planet_special_string[p->planet_special];
            snprintf(chunk, sizeof(chunk), ESTRINGS::E_Strings(0x50), "\x1A"
                                                                       "0",
                special_str);
            strlcat(buffer, chunk, buffer_size);
        }
    }

    int16_t __cdecl Dragon_New_Star_Popup(int16_t player_idx, int16_t star_idx) {
        int16_t ret_val;
        int16_t scanned_input;
        int16_t input_val;
        int16_t flag;
        s_colors colors;

        colors.colors[0] = 0x9f;
        colors.colors[1] = 0xa0;
        colors.colors[2] = 0xa1;
        colors.colors[3] = 0xa2;
        colors.colors[4] = 0;
        colors.colors[5] = 0;
        colors.colors[6] = 0;
        colors.colors[7] = 0;

        input_val = 0;
        flag = 0;

        g_player_n = player_idx;
        field_mode = 0;
        last_scanned_field = -1;

        fonts::Set_Font_Style_Shadow_Down(1, &colors);
        Load_Dragon_New_Star_Popup();
        Do_Popup_Setup(star_idx, anims[37]);
        fields::Deactivate_Auto_Function();
        fields::Assign_Auto_Function(Draw_Dragon_New_Star_Popup, 1);
        ERICHELP::Set_Dragon_New_Star_Popup_Help_List();
        Update_Dragon_New_Star_Fields();
        Draw_Dragon_New_Star_Popup();
        RUSS::Mox_Sync_Update();
        video::Set_Refresh_Differential();

        do {
            input_val = fields::Get_Input();
            scanned_input = fields::Scan_Input();
            ret_val = Evaluate_Dragon_New_Star_Input(&input_val, &scanned_input, &flag, star_idx);
            Draw_Dragon_New_Star_Popup();
            RUSS::Mox_Sync_Update();
        } while (flag == 0);

        Do_Popup_Cleanup();
        fields::Deactivate_Help_List();

        g_player_n = -1;
        g_star_n = -1;

        return ret_val;
    }

    void __cdecl Player_Attacked_Popup(int16_t attacker_idx, int16_t defender_idx, int16_t star_idx, int16_t colony_idx) {
        char attacker_name[500];
        char location_name[500];
        char defender_name[500];
        char formatted_msg[sizeof(attacker_name) + sizeof(location_name) + sizeof(defender_name) + 64];

        MOX2::Get_Auto_Function(&eric_saved_autofunction);

        if (colony_idx == -1) {
            strlcpy(location_name, MOX::star[star_idx].name, sizeof(location_name));
        } else {
            strlcpy(location_name, HACCESS::Get_Planet_Name(MOX::colony[colony_idx].planet), sizeof(location_name));
        }

        if (attacker_idx >= 0 && attacker_idx < MAX_PLAYERS) {
            if (MOX::settings.language == 4) {
                snprintf(attacker_name, sizeof(attacker_name), "%s", MOX::player[attacker_idx].race_name);
            } else {
                snprintf(attacker_name, sizeof(attacker_name), ESTRINGS::E_Strings(0x3A), MOX::player[attacker_idx].race_name);
            }
        } else {
            int16_t str_id;
            switch (attacker_idx) {
                case MAX_PLAYERS: str_id = 0x1FF; break;
                case 9: str_id = 0x202; break;
                case 10: str_id = 0x9F; break;
                case 11: str_id = 0xA0; break;
                case 12: str_id = 0xA1; break;
                case 13: str_id = 0xA2; break;
                case 14: str_id = 0xA3; break;
                default: str_id = 0xA4; break;
            }
            strlcpy(attacker_name, ESTRINGS::E_Strings(str_id), sizeof(attacker_name));
        }

        if (defender_idx >= 0 && defender_idx < MAX_PLAYERS) {
            snprintf(defender_name, sizeof(defender_name), ESTRINGS::E_Strings(0x46), MOX::player[defender_idx].race_name);
        } else {
            strlcpy(defender_name, ESTRINGS::E_Strings(0xA5), sizeof(defender_name));
        }

        if (MOX::settings.language == 4) {
            snprintf(
                formatted_msg,
                sizeof(formatted_msg),
                "SI COMBATTE NEL SISTEMA %s\n\n  Attaccante: %s\n  Difensore: %s",
                location_name,
                attacker_name,
                defender_name);
        } else {
            int16_t format_id = (colony_idx == -1) ? 0x31 : 0x30;
            snprintf(formatted_msg, sizeof(formatted_msg), ESTRINGS::E_Strings(format_id), attacker_name, defender_name, location_name);
        }

        ERICHELP::Set_Player_Attacked_Popup_Help_List();
        GENDRAW::Message_Box_Exploding_Star(formatted_msg, star_idx);
        fields::Deactivate_Help_List();
        fields::Deactivate_Auto_Function();
        MOX2::Put_Auto_Function(&eric_saved_autofunction);
    }

    int16_t __cdecl Defense_Colony_Selection_Popup(int16_t star_idx, int16_t player_idx) {
        int16_t selected_defense;
        s_animation_header* popup_buffer;
        int16_t scanned_field = 0;
        int16_t input_event = 0;
        int16_t leave_popup = 0;
        s_colors colors;

        colors.colors[0] = 0x9F;
        colors.colors[1] = 0xA0;
        colors.colors[2] = 0xA1;
        colors.colors[3] = 0xA2;
        colors.colors[4] = 0;
        colors.colors[5] = 0;
        colors.colors[6] = 0;
        colors.colors[7] = 0;

        last_scanned_field = -1;
        COLONY::sys_disp_star_handle = star_idx;
        MOX::PLAYER_NUM = player_idx;
        g_player_n = player_idx;

        fonts::Set_Font_Style_Shadow_Down(1, &colors);
        Load_Colony_Defense_Selection_Popup();

        popup_buffer = (s_animation_header*)buffer::Buffer_Reload("BUFFER0.LBX", 0x49, MOX::global_cache_seg);
        Do_Popup_Setup(star_idx, popup_buffer);

        fields::Deactivate_Auto_Function();
        fields::Assign_Auto_Function(Draw_Defense_Selection_Popup, 1);

        Update_Colony_Defense_Fields();
        last_scanned_field = mp_fields[1];

        Draw_Defense_Selection_Popup();
        RUSS::Mox_Sync_Update();

        do {
            timer::Release_Time(2);
            timer::Mark_Time();

            input_event = fields::Get_Input();
            scanned_field = fields::Scan_Input();

            selected_defense = Evaluate_Defense_Selection_Input(&input_event, &scanned_field, &leave_popup);

            Draw_Defense_Selection_Popup();
            RUSS::Mox_Sync_Update();
        } while (leave_popup == 0);

        Do_Popup_Cleanup();

        return selected_defense;
    }

    /**
     * Adds UI fields for a slider control including the scroll bar, directional buttons, 
     * and a background hidden field.
     * 
     * @param min_val The lower limit and initial value of the slider.
     * @param max_val The upper limit of the slider range.
     * @param value_ptr Pointer to the variable that will store the slider's state.
     */
    void __cdecl Add_Slider_Fields(int16_t min_val, int16_t max_val, int16_t* value_ptr) {
        char* tooltip_generic = ESTRINGS::E_Strings(12);

        // Main scroll bar field
        mp_fields[11] = fields::Add_Scroll_Field(
            g_x + 37 /* 0x25 */,
            g_y + 149 /* 0x95 */,
            min_val,
            max_val + 1,
            min_val,
            max_val,
            207 /* 0xcf */,
            13 /* 0xd */,
            value_ptr,
            tooltip_generic,
            0);

        // Slider navigation buttons
        mp_fields[10] = fields::Add_Button_Field(
            g_x + 15 /* 0xf */,
            g_y + 154 /* 0x9a */,
            tooltip_generic,
            anims[34],
            ESTRINGS::E_Strings(137 /* 0x89 */),
            40 /* 0x28 */
        );

        mp_fields[9] = fields::Add_Button_Field(
            g_x + 256 /* 0x100 */,
            g_y + 154 /* 0x9a */,
            tooltip_generic,
            anims[33],
            ESTRINGS::E_Strings(11 /* 0xb */),
            40 /* 0x28 */
        );

        mp_fields[2] = fields::Add_Button_Field(
            g_x + 15 /* 0xf */,
            g_y + 174 /* 0xae */,
            tooltip_generic,
            anims[35],
            ESTRINGS::E_Strings(135 /* 0x87 */),
            40 /* 0x28 */
        );

        mp_fields[3] = fields::Add_Button_Field(
            g_x + 208 /* 0xd0 */,
            g_y + 174 /* 0xae */,
            tooltip_generic,
            anims[4],
            ESTRINGS::E_Strings(12 /* 0xc */),
            40 /* 0x28 */
        );

        // Hidden field covering the screen to block underlying interactions
        mp_fields[0] = fields::Add_Hidden_Field(
            0,
            0,
            639 /* 0x27f */,
            479 /* 0x1df */,
            tooltip_generic,
            0);
    }

    void __cdecl Load_Slider_Fields() {
        Eric_Push_Fields();
        Add_Slider_Fields(g_min, g_max, &g_var);
    }

    int16_t __cdecl Slider_Popup(int16_t min, int16_t max, int16_t initial_value, char* msg, int16_t popup_style) {
        int16_t input = 0;
        last_scanned_field = -1;
        int16_t done = 0;
        int16_t scanned_field;

        g_min = min;
        g_max = max;
        g_msg = msg;
        g_var = initial_value;

        Load_SLIDER_Popup();
        Do_Popup_Setup(popup_style, anims[14]);
        remap::Calculate_Picture_Remap_Colors(anims[14]);
        fields::Deactivate_Auto_Function();
        fields::Assign_Auto_Function(Draw_Slider_Popup, 1);
        Load_Slider_Fields();
        Draw_Slider_Popup();
        RUSS::Mox_Sync_Update();

        do {
            input = fields::Get_Input();
            scanned_field = fields::Scan_Input();
            Draw_Slider_Popup();
            RUSS::Mox_Sync_Update();
            Evaluate_Slider_Input(&input, &scanned_field, &done);
        } while (done == 0);

        Do_Popup_Cleanup();
        return g_var;
    }

    int16_t __cdecl New_Colony_Selection_Popup(int16_t star_idx, uint8_t using_colony_ship) {
        s_colors colors;
        colors.colors[0] = 0x9f;
        colors.colors[1] = 0xa0;
        colors.colors[2] = 0xa1;
        colors.colors[3] = 0xa2;
        colors.colors[4] = 0;
        colors.colors[5] = 0;
        colors.colors[6] = 0;
        colors.colors[7] = 0;

        int16_t input = 0;
        int16_t done = 0;
        int16_t scan_input;

        g_using_colony_ship = using_colony_ship;
        COLONY::sys_disp_star_handle = star_idx;
        last_scanned_field = -1;

        fonts::Set_Font_Style_Shadow_Down(1, &colors);
        Load_Colony_Fields();
        Set_G_Special_Value(star_idx);
        Clear_System_Special_Slots();

        s_animation_header* popup_bg = (s_animation_header*)buffer::Buffer_Reload("BUFFER0.LBX", 0x49, MOX::global_cache_seg);
        Do_Popup_Setup(star_idx, popup_bg);

        fields::Deactivate_Help_List();
        fields::Deactivate_Auto_Function();
        fields::Assign_Auto_Function(Draw_Selection_Popup, 1);

        Update_Selection_Fields();
        Draw_Selection_Popup();
        RUSS::Mox_Sync_Update();

        int16_t result;
        do {
            timer::Release_Time(2);
            timer::Mark_Time();
            input = fields::Get_Input();
            scan_input = fields::Scan_Input();
            result = Evaluate_Selection_Input(&input, &scan_input, &done);
            Draw_Selection_Popup();
            RUSS::Mox_Sync_Update();
        } while (done == 0);

        if (namestar::Can_Change_Star_Name(star_idx) != 0 &&
            MOX::settings.animations_on == 0 &&
            result != -1) {
            int16_t y = namestar::Star_Name_Popup_Screen_Center_Y();
            int16_t x = namestar::Star_Name_Popup_Screen_Center_X();
            namestar::Change_Star_Name_Popup(x, y, star_idx, 0);
        }

        Do_Popup_Cleanup();

        return result;
    }

    int16_t __cdecl New_Outpost_Selection_Popup(int16_t star_id) {
        selection_state_flag = 1;
        int16_t result = New_Colony_Selection_Popup(star_id, 0);
        selection_state_flag = 0;
        return result;
    }

    int16_t __cdecl Drop_Troops_Selection_Popup(int16_t target_idx) {
        selection_state_flag = 2;
        int16_t result = New_Colony_Selection_Popup(target_idx, 0);
        selection_state_flag = 0;
        return result;
    }

    bool __cdecl Random_New_Officer_Popup(int16_t player_idx) {
        int16_t star_idx = MOX::planet[ERIC::Player_Capitol_World(player_idx)].star_index;

        s_colors colors;
        colors.colors[0] = 0x9F;
        colors.colors[1] = 0xA0;
        colors.colors[2] = 0xA1;
        colors.colors[3] = 0xA2;
        colors.colors[4] = 0;
        colors.colors[5] = 0;
        colors.colors[6] = 0;
        colors.colors[7] = 0;

        g_player_n = player_idx;

        if (on_officer_screen_flag == 0) {
            g_officer = MOX::player[player_idx].officer_for_hire;
        }

        int32_t cost = OFFICER::Calculate_Officer_Cost(player_idx, g_officer);

        field_state = 2;
        if (cost <= MOX::player[player_idx].bc) {
            field_state = 0;
        }

        if (star_idx == -1) {
            int16_t colony_idx = ERIC::Pick_Random_Colony(player_idx);
            star_idx = MOX::planet[MOX::colony[colony_idx].planet].star_index;
        }

        COLONY::sys_disp_star_handle = star_idx;

        fonts::Set_Font_Style_Shadow_Down(1, &colors);
        Clear_New_Officer_Slots();

        if (on_officer_screen_flag != 0) {
            Load_New_Officer_Popup_For_Officer_Screen();
        } else {
            Load_New_Officer_Popup();
        }

        Do_Popup_Setup(star_idx, anims[10]);

        fields::Deactivate_Auto_Function();
        fields::Assign_Auto_Function(Draw_Random_New_Officer_Popup, 1);
        Update_Random_New_Officer_Fields();

        last_scanned_field = mp_fields[1];

        video::Set_Refresh_Stencil();
        video::Set_Page_Off();

        Draw_Random_New_Officer_Popup();
        RUSS::Mox_Sync_Update();

        bool done = false;
        bool accepted = false;
        const bool show_result_window =
            MOX::settings.game_difficulty == GAME_DIFFICULTY_TUTOR &&
            on_officer_screen_flag == 0;

        do {
            int16_t input = fields::Get_Input();
            int16_t scan_input = fields::Scan_Input();

            OFFICER::Wrapper_For_Officer_Popup_Help_Fields(input, g_officer);

            if (field_state == 0) {
                if (input == mp_fields[7]) {
                    int32_t hire_cost = OFFICER::Calculate_Officer_Cost(MOX::PLAYER_NUM, g_officer);
                    if (hire_cost <= MOX::player[g_player_n].bc) {
                        field_state = 3;
                        accepted = true;
                    } else {
                        field_state = 2;
                        accepted = false;
                    }

                    Eric_Pop_Fields();
                    Update_Random_New_Officer_Fields();

                    if (show_result_window) {
                        COLONY::full_draw = 1;
                    } else {
                        done = true;
                    }

                    fields::Clear_Mouse_Buffer();
                    fields::Set_Input_Delay(3);
                } else if (input == mp_fields[8]) {
                    field_state = 1;
                    accepted = false;

                    Eric_Pop_Fields();
                    Update_Random_New_Officer_Fields();

                    if (show_result_window) {
                        COLONY::full_draw = 1;
                    } else {
                        field_state = 0;
                        done = true;
                    }

                    fields::Clear_Mouse_Buffer();
                    fields::Set_Input_Delay(3);
                } else if (input == -mp_fields[8]) {
                    int32_t info_cost = OFFICER::Calculate_Officer_Cost(g_player_n, g_officer);
                    s_leader_data* leader = &MOX::leaders[g_officer];
                    char* level_name = MOX::officer_level_names[leader->level];
                    char buffer[500];

                    snprintf(buffer, sizeof(buffer), ESTRINGS::E_Strings(0x1fb), level_name, leader, info_cost);
                    TEXTBOX::Text_Box(ESTRINGS::E_Strings(0x1bf), buffer);

                    fields::Deactivate_Auto_Function();
                    fields::Assign_Auto_Function(Draw_Random_New_Officer_Popup, 1);
                    Eric_Pop_Fields();
                    Update_Random_New_Officer_Fields();

                    COLONY::full_draw = 1;
                } else if (input == -mp_fields[7]) {
                    int32_t info_cost = OFFICER::Calculate_Officer_Cost(g_player_n, g_officer);
                    s_leader_data* leader = &MOX::leaders[g_officer];
                    char* level_name = MOX::officer_level_names[leader->level];
                    char buffer[500];

                    snprintf(buffer, sizeof(buffer), ESTRINGS::E_Strings(0x1fa), level_name, leader, info_cost);
                    TEXTBOX::Text_Box(ESTRINGS::E_Strings(0x141), buffer);

                    fields::Deactivate_Auto_Function();
                    fields::Assign_Auto_Function(Draw_Random_New_Officer_Popup, 1);
                    Eric_Pop_Fields();
                    Update_Random_New_Officer_Fields();

                    COLONY::full_draw = 1;
                }
            } else if (input > 0) {
                done = true;
            }

            Draw_Random_New_Officer_Popup();
            RUSS::Mox_Sync_Update();

        } while (!done);

        g_player_n = -1;
        Do_Popup_Cleanup();

        return accepted;
    }

    void __cdecl New_System_Discovery_Popup(int16_t star_idx, uint8_t special_flag) {
        int16_t input_val = 0;
        int16_t scanned_val = 0;
        int16_t leader_idx = 0;
        s_colors colors;

        colors.colors[0] = 0x9f;
        colors.colors[1] = 0xa0;
        colors.colors[2] = 0xa1;
        colors.colors[3] = 0xa2;
        colors.colors[4] = 0;
        colors.colors[5] = 0;
        colors.colors[6] = 0;
        colors.colors[7] = 0;

        COLONY::sys_disp_star_handle = star_idx;

        fonts::Set_Font_Style_Shadow_Down(1, &colors);
        Load_System_Discovery_Popup();

        g_special = special_flag;
        Clear_System_Special_Slots();

        s_animation_header* anim = (s_animation_header*)buffer::Buffer_Reload("BUFFER0.LBX", 0x49, MOX::global_cache_seg);
        Do_Popup_Setup(star_idx, anim);

        video::Copy_Off_To_Back();
        fields::Deactivate_Auto_Function();
        fields::Assign_Auto_Function(Draw_Discovery_Popup, 1);
        fields::Deactivate_Help_List();
        Update_Discovery_Fields();

        last_scanned_field = mp_fields[1];

        Draw_Discovery_Popup();
        RUSS::Mox_Sync_Update();

        do {
            timer::Release_Time(2);
            timer::Mark_Time();

            input_val = fields::Get_Input();
            scanned_val = fields::Scan_Input();

            if (g_special == SYSTEM_SPECIAL_NO_SPECIAL) {
                Evaluate_Discovery_Input(&input_val, &scanned_val);
            } else {
                if (g_special == SYSTEM_SPECIAL_MAROONED_HERO) {
                    int16_t prev_x = g_x;
                    int16_t prev_y = g_y;

                    g_special = SYSTEM_SPECIAL_NO_SPECIAL;

                    uint8_t can_join = OFFICER::Marooned_Hero_At_Star_Can_Join(
                        star_idx, &leader_idx, MOX::PLAYER_NUM);

                    if (can_join == 0) {
                        Marooned_Officer_Fails_Popup(star_idx, leader_idx);
                    } else {
                        Marooned_Officer_Joins_Popup(star_idx, leader_idx);
                    }

                    input_val = 0;
                    COLONY::full_draw = 1;
                    g_x = prev_x;
                    g_y = prev_y;

                    fields::Deactivate_Auto_Function();
                    fields::Assign_Auto_Function(Draw_Discovery_Popup, 1);
                } else if (input_val != 0) {
                    input_val = 0;
                    g_special = SYSTEM_SPECIAL_NO_SPECIAL;
                    COLONY::full_draw = 1;
                }

                if (MOX::FULL_DRAW_SCREEN_FLAG != 0) {
                    COLONY::full_draw = 1;
                }
            }

            Draw_Discovery_Popup();
            RUSS::Mox_Sync_Update();

        } while (input_val < 1 || g_special != SYSTEM_SPECIAL_NO_SPECIAL);

        Do_Popup_Cleanup();
    }

    void __cdecl Officer_Gains_Level_Popup(int16_t star_handle, int16_t officer_idx) {
        game_events::s_leader_view_scope leader_view(leader_read::RESULT);
        s_colors colors;
        int16_t input;

        colors.colors[0] = 0x9f;
        colors.colors[1] = 0xa0;
        colors.colors[2] = 0xa1;
        colors.colors[3] = 0xa2;
        colors.colors[4] = 0;
        colors.colors[5] = 0;
        colors.colors[6] = 0;
        colors.colors[7] = 0;

        COLONY::sys_disp_star_handle = star_handle;
        fonts::Set_Font_Style_Shadow_Down(1, &colors);
        g_officer = officer_idx;

        Clear_New_Officer_Slots();
        Load_Officer_Gains_Level_Popup();
        Do_Popup_Setup(star_handle, anims[10]);

        fields::Deactivate_Auto_Function();
        fields::Assign_Auto_Function(Draw_Officer_Gains_Level_Popup, 1);

        Update_Officer_Gains_Level_Fields();
        last_scanned_field = mp_fields[1];

        video::Set_Page_Off();
        game_events::Ready_Leader_View(leader_read::RESULT);
        Draw_Officer_Gains_Level_Popup();
        RUSS::Mox_Sync_Update();

        do {
            input = fields::Get_Input();
            fields::Scan_Input();
            game_events::Ready_Leader_View(leader_read::RESULT);
            Draw_Officer_Gains_Level_Popup();
            RUSS::Mox_Sync_Update();
        } while (input == 0);

        leader_view.Close();
        Do_Popup_Cleanup();
    }

    const char* __cdecl System_Special_Desc(int16_t desc_id) {
        if (g_msg_ptr == nullptr) {
            g_msg_ptr = (char*)farload::Far_Reload_Next_Data(
                ESTRINGS::E_Strings(0x170),
                desc_id,
                MOX::screen_seg,
                0,
                1,
                0x258 /* 600 */
            );
            string_overrides::Apply_String(ESTRINGS::E_Strings(0x170), desc_id + 1, g_msg_ptr, 600);
        }
        return g_msg_ptr;
    }

    s_animation_header* __cdecl System_Special_Anim(int16_t star_idx, int16_t anim_type) {
        if (anims[7] == nullptr) {
            if (anim_type == SYSTEM_SPECIAL_SPACE_MONSTER) {
                int16_t monster_idx = RUSS::Star_To_Monster(star_idx);
                anims[7] = (s_animation_header*)farload::Far_Reload_Next(COLONY::MAINPUPS_LBX, monster_idx + 15, MOX::screen_seg);
            } else {
                anims[7] = (s_animation_header*)farload::Far_Reload_Next(COLONY::MAINPUPS_LBX, anim_type + 4, MOX::screen_seg);
            }
        }

        if (anims[0] == nullptr) {
            anims[0] = (s_animation_header*)farload::Far_Reload_Next(COLONY::MAINPUPS_LBX, 0, MOX::screen_seg);
        }

        return anims[7];
    }

    void __cdecl Draw_Special_Info_Popup(int16_t x, int16_t y, s_animation_header* anim, const char* title, const char* text) {
        s_colors font_colors;
        font_colors.colors[0] = 0x6E; /* 'n' */
        font_colors.colors[1] = 0x71; /* 'q' */
        font_colors.colors[2] = 0x73; /* 's' */
        font_colors.colors[3] = 0;
        font_colors.colors[4] = 0;
        font_colors.colors[5] = 0;
        font_colors.colors[6] = 0;
        font_colors.colors[7] = 0;

        int16_t h = animate::Get_Height(anims[0]);
        int16_t w = animate::Get_Width(anims[0]);

        graphics::Fill(x + 3, y + 3, x + w - 3, y + h - 3, 0);

        animate::Draw(x, y, anims[0]);
        animate::Draw(x + 4, y + 4, anim);

        fonts::Set_Font_Style(1, &font_colors);
        fonts::Print_Centered(x + 0xDE, y + 10, title);

        BILL::_Print_Paragraph(x + 0x78, y + 0x17, 0xCD, text, 2);
    }

    int16_t __cdecl Colony_Selection_Popup(
        s_invasion_side_info* attacker,
        s_invasion_side_info* defender,
        int32_t random_seed,
        int16_t colony_idx,
        int16_t bombardment_result,
        int8_t has_stellar_converter) {
        s_colony_combat_popup_data data;

        g_data_ptr = &data;

        MOX2::Get_Auto_Function(&temp_af);
        MOX2::Execute_Auto_Function(&temp_af);

        RUSS::Mox_Sync_Update();

        Update_CCP_Display_Data_Invasion(
            g_data_ptr,
            attacker,
            defender,
            random_seed,
            colony_idx,
            bombardment_result,
            has_stellar_converter);

        return Colony_Combat_Popup(&data, 0);
    }

    void __cdecl Update_CCP_Display_Data_Invasion(
        s_colony_combat_popup_data* ccp_data,
        s_invasion_side_info* attacker,
        s_invasion_side_info* defender,
        int32_t random_seed,
        int16_t colony_idx,
        int16_t bombardment_result,
        int8_t has_stellar_converter) {
        ccp_data->bombardment_result = bombardment_result;
        ccp_data->attacker = attacker;
        ccp_data->colony_idx = colony_idx;
        ccp_data->defender = defender;
        ccp_data->random_seed = random_seed;
        ccp_data->has_stellar_converter = has_stellar_converter;
        ccp_data->attacker_player_id = static_cast<int8_t>(attacker->player_id);
        ccp_data->defender_player_id = static_cast<int8_t>(defender->player_id);

        for (int i = 0; i < 4; ++i) {
            ccp_data->attacker_unit_counts[i] = attacker->unit_counts[i];
            ccp_data->defender_unit_counts[i] = defender->unit_counts[i];
        }

        ccp_data->n_pops = static_cast<int16_t>(static_cast<uint8_t>(MOX::colony[colony_idx].n_pops));
        ccp_data->n_bldgs = COLONY::N_Bldgs(colony_idx);
        ccp_data->best_shield_bldg = Get_Colony_Best_Shield_Bldg(colony_idx);
    }

    int16_t __cdecl Colony_Combat_Popup(s_colony_combat_popup_data* popup_data, int16_t popup_mode) {
        bool invasion_started = false;
        int16_t done = 0;
        int16_t var_1C = 0;
        int16_t scan_val = 0;
        int16_t input_val = 0;
        bool invasion_done = false;
        int16_t bomb_hit_val1;
        int16_t bomb_hit_val2;
        s_colors colors;
        char buffer[500];

        colors.colors[0] = 0x9F;
        colors.colors[1] = 0xA0;
        colors.colors[2] = 0xA1;
        colors.colors[3] = 0xA2;
        colors.colors[4] = 0;
        colors.colors[5] = 0;
        colors.colors[6] = 0;
        colors.colors[7] = 0;

        COLGCBT::g_ctr = 0;
        MAINPUPS::last_scanned_field = -1;
        g_data_ptr = popup_data;

        fonts::Load_Palette(0, 0, 0xFF);
        palette::Clear_Palette_Changes(0, 0xFF);
        fonts::Set_Font_Style_Shadow_Down(1, &colors);

        MAINPUPS::Load_Colony_Combat_Popup();
        FLCQUEUE::Free_Flic_Queue_And_Pointers();

        s_animation_header* anim = COLONY::C_Anims(0xD);
        remap::Calculate_Picture_Remap_Colors(anim);

        int16_t star_idx = MOX::planet[MOX::colony[MAINPUPS::g_data_ptr->colony_idx].planet].star_index;
        MAINPUPS::Do_Popup_Setup(star_idx, anims[3]);

        fields::Deactivate_Auto_Function();
        fields::Assign_Auto_Function(MAINPUPS::Draw_Colony_Combat_Popup, 1);

        MAINPUPS::Update_Colony_Combat_Fields(popup_mode, &var_1C);
        MAINPUPS::Draw_Colony_Combat_Popup();
        RUSS::Mox_Sync_Update();
        video::Set_Refresh_Stencil();

        if (popup_mode == 2) {
            invasion_started = true;
        } else if (popup_mode == 1) {
            MAINPUPS::Setup_To_Apply_Bomb_Hits(MAINPUPS::g_data_ptr->bomb, &bomb_hit_val1, &bomb_hit_val2);
        }

        game_random::Set_Random_Seed(MAINPUPS::g_data_ptr->random_seed);
        MAINPUPS::g_state = popup_mode;

        while (true) {
            timer::Mark_Time();
            COLONY::full_draw = 1;
            MAINPUPS::Eric_Pop_Fields();
            MAINPUPS::Update_Colony_Combat_Fields(popup_mode, &var_1C);

            input_val = fields::Get_Input();
            scan_val = fields::Scan_Input();

            if (popup_mode == 2) {
                if (MAINPUPS::g_data_ptr->n_pops <= 0) {
                    invasion_done = true;
                }
                while (true) {
                    bool is_combat_active = ((uint8_t)MAINPUPS::g_data_ptr->attacker->current_troop_type_idx < 4) &&
                        ((uint8_t)MAINPUPS::g_data_ptr->defender->current_troop_type_idx < 4);

                    if (is_combat_active && invasion_started) {
                        if (COLGCBT::g_ctr % 10 != 0 && !invasion_done) {
                            break;
                        }
                        invasion::Ground_Combat_Round(MAINPUPS::g_data_ptr->attacker, MAINPUPS::g_data_ptr->defender);
                        MAINPUPS::Update_CCP_Display_Data_Invasion(
                            MAINPUPS::g_data_ptr,
                            MAINPUPS::g_data_ptr->attacker,
                            MAINPUPS::g_data_ptr->defender,
                            game_random::Get_Random_Seed(),
                            MAINPUPS::g_data_ptr->colony_idx,
                            MAINPUPS::g_data_ptr->bombardment_result,
                            MAINPUPS::g_data_ptr->has_stellar_converter);
                        COLONY::full_draw = 1;
                    } else {
                        invasion_started = true;
                        if (input_val != 0) {
                            if (!invasion_done) {
                                input_val = 0;
                                invasion_done = true;
                            }
                        }
                    }

                    if (!invasion_done) {
                        break;
                    }

                    if ((uint8_t)MAINPUPS::g_data_ptr->attacker->current_troop_type_idx >= 4 ||
                        (uint8_t)MAINPUPS::g_data_ptr->defender->current_troop_type_idx >= 4) {
                        break;
                    }
                }
            } else if (popup_mode == 1) {
                MAINPUPS::Apply_Next_Bomb_Hit(MAINPUPS::g_data_ptr->bomb, &var_1C, &bomb_hit_val1, &bomb_hit_val2);
            }

            int16_t result = MAINPUPS::Evaluate_Colony_Combat_Input(&input_val, &scan_val, &done, &var_1C);
            MAINPUPS::Draw_Colony_Combat_Popup();
            RUSS::Mox_Sync_Update();

            if (popup_mode == 2) {
                s_invasion_side_info* attacker = MAINPUPS::g_data_ptr->attacker;
                s_invasion_side_info* defender = MAINPUPS::g_data_ptr->defender;

                if ((uint8_t)attacker->current_troop_type_idx >= 4 || (uint8_t)defender->current_troop_type_idx >= 4) {
                    int16_t str_id;
                    int16_t player_id = attacker->player_id;

                    if ((uint8_t)attacker->current_troop_type_idx < 4) {
                        str_id = 0x3F;
                    } else {
                        str_id = 0x3C;
                    }

                    char* race_name = MOX::player[player_id].race_name;
                    char* planet_name = HACCESS::Get_Planet_Name(MOX::colony[MAINPUPS::g_data_ptr->colony_idx].planet);

                    char* fmt = ESTRINGS::E_Strings(str_id);
                    snprintf(buffer, sizeof(buffer), fmt, planet_name, race_name);
                    GENDRAW::Help(buffer);

                    COLONY::full_draw = 1;
                    done = 1;
                }
            }

            COLGCBT::g_ctr++;
            timer::Release_Time(1);

            if (done != 0) {
                MAINPUPS::Do_Popup_Cleanup();
                FLCQUEUE::Allocate_Flic_Queue_And_Pointers();
                sound::Stop_All_Sounds();
                if (result == 3) {
                    ERIC::Fast_Fade_Out();
                }
                return result;
            }
        }
    }

    void __cdecl Update_Colony_Combat_Fields(int16_t popup_mode, int16_t* combat_state_ptr) {
        Eric_Push_Fields();
        Add_Colony_Combat_Fields(popup_mode, combat_state_ptr);
    }

    void __cdecl Update_CCP_Display_Data_Bombing(
        s_colony_combat_popup_data* ccp_data,
        s_combat_bomb_result* bomb,
        s_invasion_side_info* attacker,
        s_invasion_side_info* defender,
        int32_t random_seed,
        int16_t colony_idx,
        int16_t bombing_result,
        int8_t has_stellar_converter) {
        ccp_data->bomb = bomb;

        Update_CCP_Display_Data_Invasion(
            g_data_ptr,
            attacker,
            defender,
            random_seed,
            colony_idx,
            bombing_result,
            has_stellar_converter);

        ccp_data->defender_unit_counts[1] -= g_data_ptr->bomb->infantry_killed;
        ccp_data->defender_unit_counts[0] -= g_data_ptr->bomb->tanks_killed;
    }

    int16_t __cdecl Colony_Bombing_Popup(
        s_combat_bomb_result* bomb,
        s_invasion_side_info* attacker,
        s_invasion_side_info* defender,
        int32_t random_seed,
        int16_t colony_idx,
        int16_t bombing_result,
        int8_t has_stellar_converter) {
        s_colony_combat_popup_data popup_data;

        g_data_ptr = &popup_data;

        Update_CCP_Display_Data_Bombing(
            &popup_data,
            bomb,
            attacker,
            defender,
            random_seed,
            colony_idx,
            bombing_result,
            has_stellar_converter);

        explode_flag = 0;

        return Colony_Combat_Popup(&popup_data, 1);
    }

    int8_t __cdecl Player_Has_Colony_Or_Outpost_At_Star(int16_t player_idx, int16_t star_idx) {
        for (int16_t i = 0; i < 5; ++i) {
            int16_t planet_idx = MOX::star[star_idx].planet_index[i];

            if (planet_idx != -1) {
                int16_t colony_idx = MOX::planet[planet_idx].colony_index;

                if (colony_idx != -1) {
                    if (MOX::colony[colony_idx].owner == player_idx) {
                        return 1;
                    }
                }
            }
        }

        return 0;
    }

    void __cdecl Apply_Next_Bomb_Hit(s_combat_bomb_result* bomb, int16_t* state, int16_t* bomb_hits, int16_t* bio_hits) {
        s_colony* colony = &MOX::colony[bomb->colony_idx];

        if (*state == 1 || *state == 3 || COLGCBT::g_ctr % 5 != 0) {
            return;
        }

        do {
            if (*bio_hits + *bomb_hits <= 0) {
                if (*state == 2) {
                    *bio_hits = 0;
                    g_data_ptr->bombardment_result = 0;
                }
                *state = 3;
            } else {
                if (*bio_hits == 0) {
                    int32_t hits_processed = BOMB::Resolve_Bomb_Hit(colony, bomb, *bomb_hits);
                    if (hits_processed == 0) {
                        *bomb_hits = 0;
                    } else {
                        int loops = hits_processed;
                        if (*bomb_hits == hits_processed) {
                            *bomb_hits -= hits_processed;
                            g_data_ptr->bombardment_result -= hits_processed;
                            loops = 1;
                        } else {
                            *bomb_hits -= hits_processed;
                            g_data_ptr->bombardment_result -= hits_processed;
                        }

                        if (loops > 5) {
                            loops = 5;
                        }

                        for (int i = loops; i > 0; --i) {
                            int combat_planet_w = animate::Get_Width(MOX::combat_planet_seg);
                            int anim25_w = animate::Get_Width(anims[25]);

                            int diff = (combat_planet_w / 2) - anim25_w;
                            int abs_diff = diff < 0 ? -diff : diff;

                            int anim3_h = animate::Get_Height(anims[3]);
                            int base_y = (anim3_h + g_y * 2 - 30) / 2;
                            int rand_y = base_y + game_random::Random(abs_diff * 2) - abs_diff;

                            int anim3_w = animate::Get_Width(anims[3]);
                            int base_x = (anim3_w + g_x * 2 - 30) / 2;
                            int rand_x = base_x + game_random::Random(abs_diff * 2) - abs_diff;

                            FLCQUEUE::Add_Flic_To_End_Of_Queue(rand_x, rand_y, anims[25], 1, 0);
                            ERICNET::Mox_Play_Sound(24 /* 0x18 */);
                        }
                    }
                } else {
                    BOMB::Resolve_Biological_Hit(colony, bomb);
                    (*bio_hits)--;
                    g_data_ptr->bombardment_result--;

                    int anim3_h = animate::Get_Height(anims[3]);
                    int base_y = (anim3_h + g_y * 2 - 30) / 2;
                    int rand_y = base_y + game_random::Random(40) - 20;

                    int anim3_w = animate::Get_Width(anims[3]);
                    int base_x = (anim3_w + g_x * 2 - 30) / 2;
                    int rand_x = base_x + game_random::Random(40) - 20;

                    FLCQUEUE::Add_Flic_To_End_Of_Queue(rand_x, rand_y, anims[26], 1, 0);
                    ERICNET::Mox_Play_Sound(32 /* 0x20 */);
                }

                Update_CCP_Display_Data_Bombing(
                    g_data_ptr,
                    g_data_ptr->bomb,
                    g_data_ptr->attacker,
                    g_data_ptr->defender,
                    g_data_ptr->random_seed,
                    g_data_ptr->colony_idx,
                    g_data_ptr->bombardment_result,
                    g_data_ptr->has_stellar_converter);
                COLONY::full_draw = 1;
            }
        } while (*state == 2);
    }

    void __cdecl Setup_To_Apply_Bomb_Hits(s_combat_bomb_result* result, int16_t* p_bomb_hits_remaining, int16_t* p_bio_hits_remaining) {
        *p_bomb_hits_remaining = result->bomb_hits_remaining;
        *p_bio_hits_remaining = result->bio_hits_remaining;

        result->infantry_killed = 0;
        result->tanks_killed = 0;
        result->population_killed = 0;

        memset(result->buildings_destroyed, 0, sizeof(result->buildings_destroyed));

        result->bio_pop_killed = 0;
        result->damage_occurred = 0;
        result->colony_wiped_out = 0;
    }

    void __cdecl Update_Discovery_Fields() {
        Eric_Push_Fields();
        COLONY::Clear_Sys_Disp_Fields();

        MOX::draw_system_display_without_button = 1;

        SYS::Wrapper_For_Informational_System_Fields(
            COLONY::sys_disp_star_handle,
            g_x,
            g_y,
            13,
            47,
            nullptr);

        char* hotkey = ESTRINGS::E_Strings(12);
        mp_fields[0] = fields::Add_Hidden_Field(0, 0, 639, 479, hotkey, 0);
    }

    void __cdecl Draw_Discovery_Popup() {
        char msg_buffer[500];
        s_star_data* star_ptr;
        const char* format;
        s_animation_header* anim;

        video::Set_Page_Off();

        if (COLONY::full_draw != 0 || MOX::FULL_DRAW_SCREEN_FLAG != 0) {
            MAINPUPS::Mainpups_Tested_Execute_Autofunction();
            video::Copy_Off_To_Back();
            MOX::FULL_DRAW_SCREEN_FLAG = 0;
            COLONY::full_draw = 0;
        }

        star_ptr = &MOX::star[COLONY::sys_disp_star_handle];
        format = ESTRINGS::E_Strings(456 /* 0x1c8 */);
        snprintf(msg_buffer, sizeof(msg_buffer), format, star_ptr->name);

        anim = (s_animation_header*)buffer::Buffer_Reload("BUFFER0.LBX", 73 /* 0x49 */, MOX::global_cache_seg);
        MAINPUPS::Get_Picture_Draw_XY(anim);

        SYS::Draw_System_Display_Popup(
            COLONY::sys_disp_star_handle,
            1,
            msg_buffer,
            MAINPUPS::g_x,
            MAINPUPS::g_y,
            0,
            0,
            13 /* 0xd */,
            47 /* 0x2f */
        );

        if (MAINPUPS::g_special != SYSTEM_SPECIAL_NO_SPECIAL && MAINPUPS::g_special != SYSTEM_SPECIAL_MAROONED_HERO) {
            SYS::Draw_System_Special_Popup(
                static_cast<int16_t>(MAINPUPS::g_x + 6),
                static_cast<int16_t>(MAINPUPS::g_y + 76 /* 0x4c */),
                (int16_t)MAINPUPS::g_special,
                MAINPUPS::g_star_n);
        }
    }

    void __cdecl Update_Selection_Fields() {
        Eric_Push_Fields();
        COLONY::Clear_Sys_Disp_Fields();
        Add_Selection_Fields();
        ERICHELP::Set_Planet_Selection_Popup_Help_List();
    }

    int16_t __cdecl Evaluate_Selection_Input(int16_t* selected_field, int16_t* field_data, int16_t* refresh_flag) {
        int16_t saved_x = g_x;
        int16_t saved_y = g_y;
        int16_t selection_result = 0;

        if (g_special == SYSTEM_SPECIAL_NO_SPECIAL) {
            if (*selected_field == mp_fields[2]) {
                selection_result = -1;
                *refresh_flag = 1;
            } else {
                selection_result = Check_System_Display_Fields_Selection(selected_field, field_data, refresh_flag);
            }
        } else if (g_special == SYSTEM_SPECIAL_MAROONED_HERO) {
            int16_t marooned_hero_idx;
            uint8_t can_join;

            g_special = SYSTEM_SPECIAL_NO_SPECIAL;
            can_join = OFFICER::Marooned_Hero_At_Star_Can_Join(g_star_n, &marooned_hero_idx, MOX::PLAYER_NUM);
            if (can_join == 0) {
                Marooned_Officer_Fails_Popup(g_star_n, marooned_hero_idx);
            } else {
                Marooned_Officer_Joins_Popup(g_star_n, marooned_hero_idx);
            }
            g_x = saved_x;
            COLONY::full_draw = 1;
            g_y = saved_y;
            Eric_Pop_Fields();
            Update_Selection_Fields();
            video::Copy_Back_To_Off();
            Draw_Selection_Popup();
            fields::Assign_Auto_Function(Draw_Selection_Popup, 1);
            ERICHELP::Set_New_System_Discovery_Popup_Help_List();
        } else if (*selected_field != 0) {
            g_special = SYSTEM_SPECIAL_NO_SPECIAL;
            COLONY::full_draw = 1;
        }

        if (MOX::FULL_DRAW_SCREEN_FLAG != 0) {
            COLONY::full_draw = 1;
        }

        return selection_result;
    }

    void __cdecl Load_New_Officer_Popup() {
        allocate::Push_Block(MOX::screen_seg);

        anims[10] = (s_animation_header*)farload::Far_Reload_Next(COLONY::MAINPUPS_LBX, 0x30, MOX::screen_seg);
        anims[12] = (s_animation_header*)farload::Far_Reload_Next(COLONY::MAINPUPS_LBX, 0x31, MOX::screen_seg);
        anims[13] = (s_animation_header*)farload::Far_Reload_Next(COLONY::MAINPUPS_LBX, 0x32, MOX::screen_seg);
        anims[11] = OFFICER::Load_Officer_Picture(g_player_n, g_officer, MOX::screen_seg, 0, 0);

        OFFICER::Load_Officer_Skill_Icons(MOX::screen_seg);
    }

    void __cdecl Load_New_Officer_Popup_For_Officer_Screen() {
        allocate::Push_Block(MOX::screen_seg);
        anims[10] = (s_animation_header*)farload::Far_Reload_Next(COLONY::MAINPUPS_LBX, 0x39, MOX::screen_seg);
        anims[12] = (s_animation_header*)farload::Far_Reload_Next(COLONY::MAINPUPS_LBX, 0x3A, MOX::screen_seg);
        anims[13] = (s_animation_header*)farload::Far_Reload_Next(COLONY::MAINPUPS_LBX, 0x3B, MOX::screen_seg);
        anims[11] = OFFICER::Load_Officer_Picture(g_player_n, g_officer, MOX::screen_seg, 0, 0);
        if (on_officer_screen_flag == 0) {
            OFFICER::Load_Officer_Skill_Icons(MOX::screen_seg);
        }
    }

    void __cdecl Draw_Selection_Popup() {
        char popup_text[500];
        bool draw_selection_popup = false;
        int16_t string_id = 0;

        fonts::Set_Font_Style(4, &main_scr_font_colors);

        if (COLONY::full_draw != 0 || MOX::FULL_DRAW_SCREEN_FLAG != 0) {
            Mainpups_Tested_Execute_Autofunction();
            video::Copy_Off_To_Back();
            COLONY::full_draw = 0;
        }

        switch (selection_state_flag) {
            case 0:
                string_id = (g_using_colony_ship != 0) ? 0x1ce : 0x1cd;
                draw_selection_popup = true;
                break;

            case 1:
                string_id = 0x1cb;
                draw_selection_popup = true;
                break;

            case 2:
                string_id = 0x1cf;
                draw_selection_popup = true;
                break;

            case 3:
                string_id = 0x1cc;
                draw_selection_popup = true;
                break;

            default:
                break;
        }

        if (draw_selection_popup) {
            s_star_data* selected_star = &MOX::star[g_star_n];
            const char* format = ESTRINGS::E_Strings(string_id);
            snprintf(popup_text, sizeof(popup_text), format, selected_star->name);

            SYS::Draw_System_Display_Popup(
                COLONY::sys_disp_star_handle,
                1,
                popup_text,
                g_x,
                g_y,
                0,
                0,
                0x0d,
                0x2f);
        }

        fonts::Set_Font_Style(1, &main_scr_font_colors);

        if (g_special != SYSTEM_SPECIAL_NO_SPECIAL && g_special != SYSTEM_SPECIAL_MAROONED_HERO) {
            SYS::Draw_System_Special_Popup(
                g_x,
                static_cast<int16_t>(g_y + 0x32),
                g_special,
                g_star_n);
        }
    }

    void __cdecl Update_Random_New_Officer_Fields() {
        char* button_text;
        char* base_text;
        s_animation_header* animation;
        char hotkey = '(';
        int16_t hidden_field_id;

        MAINPUPS::Eric_Push_Fields();
        fields::Clear_Mouse_Buffer();
        fields::Set_Input_Delay(3);

        if (MAINPUPS::field_state == 0) {
            hotkey = '(';
            button_text = ESTRINGS::E_Strings(0x1b4);
            animation = MAINPUPS::anims[12];
            base_text = ESTRINGS::E_Strings(0xc);
            MAINPUPS::mp_fields[8] = fields::Add_Button_Field(MAINPUPS::g_x + 0x2c, MAINPUPS::g_y + 0xe2, base_text, animation, button_text, hotkey);

            hotkey = '(';
            button_text = ESTRINGS::E_Strings(0x13c);
            animation = MAINPUPS::anims[13];
            base_text = ESTRINGS::E_Strings(0xc);
            MAINPUPS::mp_fields[7] = fields::Add_Button_Field(MAINPUPS::g_x + 0xaf, MAINPUPS::g_y + 0xe2, base_text, animation, button_text, hotkey);

            MAINPUPS::mp_fields[0] = -1000;
        } else {
            hotkey = '(';
            if (MAINPUPS::field_state == 3) {
                button_text = ESTRINGS::E_Strings(0x180);
                animation = MAINPUPS::anims[12];
                base_text = ESTRINGS::E_Strings(0xc);
                MAINPUPS::mp_fields[8] = fields::Add_Button_Field(MAINPUPS::g_x + 0x2c, MAINPUPS::g_y + 0xe2, base_text, animation, button_text, hotkey);

                hotkey = '(';
                button_text = ESTRINGS::E_Strings(0x231);
                animation = MAINPUPS::anims[13];
                base_text = ESTRINGS::E_Strings(0xc);
                MAINPUPS::mp_fields[7] = fields::Add_Button_Field(MAINPUPS::g_x + 0xaf, MAINPUPS::g_y + 0xe2, base_text, animation, button_text, hotkey);
            } else {
                button_text = ESTRINGS::E_Strings(0x180);
                animation = MAINPUPS::anims[12];
                base_text = ESTRINGS::E_Strings(0xc);
                MAINPUPS::mp_fields[8] = fields::Add_Button_Field(MAINPUPS::g_x + 0x2c, MAINPUPS::g_y + 0xe2, base_text, animation, button_text, hotkey);
                MAINPUPS::mp_fields[7] = -1000;
            }

            hidden_field_id = 0;
            base_text = ESTRINGS::E_Strings(0xc);
            MAINPUPS::mp_fields[0] = fields::Add_Hidden_Field(0, 0, 0x27f, 0x1df, base_text, (int8_t)hidden_field_id);
        }

        OFFICER::Wrapper_To_Add_Officer_Popup_Help_Fields(MAINPUPS::g_x, MAINPUPS::g_y + 0x35, MAINPUPS::g_officer);
    }

    void __cdecl Evaluate_Slider_Input(int16_t* input, int16_t* scanned_field, int16_t* done) {
        (void)scanned_field;

        const int16_t value = *input;

        if (value == mp_fields[3]) {
            *done = 1;
        }

        if (value == mp_fields[2]) {
            *done = 1;
            g_var = -1;
        }

        if (value == mp_fields[9]) {
            ++g_var;
        }

        if (value == mp_fields[10]) {
            --g_var;
        }
    }

    void __cdecl Mainpups_Tested_Execute_Autofunction() {
        if (MAINPUPS::autofunction_ctr == 1) {
            MOX::FULL_DRAW_SCREEN_FLAG = 1;
            COLONY::full_draw = 1;
            MOX::dont_draw_main_screen_to_back = 1;
            MOX2::Execute_Auto_Function(&MAINPUPS::eric_saved_autofunction);
        } else if (MAINPUPS::autofunction_ctr == 2) {
            MOX::FULL_DRAW_SCREEN_FLAG = 1;
            COLONY::full_draw = 1;
            MAINPUPS::autofunction_ctr = 0;
            MOX::dont_draw_main_screen_to_back = 1;
            MOX2::Execute_Auto_Function(&MAINPUPS::eric_saved_autofunction);

            MOX::FULL_DRAW_SCREEN_FLAG = 1;
            COLONY::full_draw = 1;
            MOX::dont_draw_main_screen_to_back = 1;
            MOX2::Execute_Auto_Function(&MAINPUPS::eric_saved_autofunction2);
            MAINPUPS::autofunction_ctr = 2;
        }

        MOX::FULL_DRAW_SCREEN_FLAG = 0;
        MOX::dont_draw_main_screen_to_back = 0;
    }

    void __cdecl Draw_Random_New_Officer_Popup() {
        char message_buffer[500];
        char tech_buffer[500];
        char* leader_name;
        const char* format_string;
        int32_t officer_cost;
        int16_t maintenance_cost;

        if (COLONY::full_draw == 0 && MOX::FULL_DRAW_SCREEN_FLAG == 0) {
            return;
        }

        MOX::draw_main_screen_to_back = 1;
        Mainpups_Tested_Execute_Autofunction();

        animate::Draw(MAINPUPS::g_x, MAINPUPS::g_y, anims[10]);
        OFFICER::Wrapper_For_Colony_Officer_Popup_Data(MAINPUPS::g_x,
            MAINPUPS::g_y + 0x35,
            MAINPUPS::g_officer);
        animate::Draw(MAINPUPS::g_x + 0x0d, MAINPUPS::g_y + 0x37, anims[11]);

        const int16_t field_state = MAINPUPS::field_state;

        if (MAINPUPS::on_officer_screen_flag != 0 ||
            field_state == 0 ||
            field_state == 2) {
            MAINPUPS::Leader_Technology_String(MAINPUPS::g_player_n,
                MAINPUPS::g_officer,
                tech_buffer,
                sizeof(tech_buffer));
            maintenance_cost = OFFICER::Officer_Maintenance(MAINPUPS::g_player_n,
                MAINPUPS::g_officer);
            officer_cost = OFFICER::Calculate_Officer_Cost(MAINPUPS::g_player_n,
                MAINPUPS::g_officer);
            leader_name = OFFICER::Leader_Name(MAINPUPS::g_officer, 0);

            if (MAINPUPS::on_officer_screen_flag != 0) {
                format_string = HAROLD::H_Message(HESTR_125_S_WILL_COST_D_BCS_INIT);
            } else if (field_state == 2) {
                format_string = ESTRINGS::E_Strings(5);
            } else {
                format_string = ESTRINGS::E_Strings(2);
            }

            snprintf(message_buffer,
                sizeof(message_buffer),
                format_string,
                leader_name,
                officer_cost,
                maintenance_cost,
                tech_buffer);

            if (tech_buffer[0] != '\0' &&
                strstr(message_buffer, tech_buffer) == nullptr) {
                strlcat(message_buffer, "\n", sizeof(message_buffer));
                strlcat(message_buffer, tech_buffer, sizeof(message_buffer));
            }
        } else if (field_state == 1) {
            officer_cost = OFFICER::Calculate_Officer_Cost(MAINPUPS::g_player_n,
                MAINPUPS::g_officer);
            leader_name = OFFICER::Leader_Name(MAINPUPS::g_officer, 0);
            format_string = ESTRINGS::E_Strings(6);
            snprintf(message_buffer, sizeof(message_buffer), format_string, leader_name, officer_cost);
        } else if (field_state == 3) {
            officer_cost = OFFICER::Calculate_Officer_Cost(MAINPUPS::g_player_n,
                MAINPUPS::g_officer);
            leader_name = OFFICER::Leader_Name(MAINPUPS::g_officer, 0);
            format_string = ESTRINGS::E_Strings(4);
            snprintf(message_buffer, sizeof(message_buffer), format_string, leader_name, officer_cost);
        }

        BILL::Squeeze_Paragraph_Centered(MAINPUPS::g_x + 0x0e,
            MAINPUPS::g_y + 0x95,
            0x119,
            0x3e,
            message_buffer,
            2);

        leader_name = OFFICER::Leader_Name(MAINPUPS::g_officer, 1);
        ERIC::Print_Paragraph_Centered(MAINPUPS::g_x + 0x0b,
            MAINPUPS::g_y + 0x0c,
            0x11d,
            0x21,
            leader_name);
        video::Copy_Off_To_Back();
    }

    void __cdecl Evaluate_Discovery_Input(int16_t* input_1, int16_t* input_2) {
        Do_Evaluate_Discovery_Input(input_1, input_2);
    }

    void __cdecl Do_Evaluate_Discovery_Input(int16_t* mouse_x, int16_t* mouse_y) {
        int16_t out_val1;
        int16_t out_val2;
        int16_t scanned_ship_idx;
        int16_t out_val4;
        int16_t scanned_icon_idx;

        // Scans the display fields at the given mouse coordinates.
        // Arg order derived from disassembly: *param_2 (Y), *param_1 (X)
        MAINSCR::Scan_System_Display_Fields(*mouse_y, *mouse_x, MOX::planet_buttons, &out_val1, &out_val2, &scanned_ship_idx, &out_val4, &scanned_icon_idx);

        MOX::system_display_ship_scanned = scanned_ship_idx;

        if (scanned_icon_idx > -1) {
            MOX::system_icon_scanned = scanned_icon_idx;
        }
    }

    void __cdecl Marooned_Officer_Fails_Popup(int16_t star_idx, int16_t officer_idx) {
        officer_gains_level_state = 2;
        Officer_Gains_Level_Popup(star_idx, officer_idx);
        officer_gains_level_state = 0;
    }

    void __cdecl Marooned_Officer_Joins_Popup(int16_t star_idx, int16_t officer_idx) {
        officer_gains_level_state = 1;
        Officer_Gains_Level_Popup(star_idx, officer_idx);
        officer_gains_level_state = 0;
    }

    void __cdecl Load_Dragon_New_Star_Popup() {
        allocate::Push_Block(MOX::screen_seg);

        anims[4] = (s_animation_header*)farload::Far_Reload_Next(COLONY::MAINPUPS_LBX, 0x46, MOX::screen_seg);
        anims[35] = (s_animation_header*)farload::Far_Reload_Next(COLONY::MAINPUPS_LBX, 0x47, MOX::screen_seg);
        anims[37] = (s_animation_header*)farload::Far_Reload_Next(COLONY::MAINPUPS_LBX, 0x48, MOX::screen_seg);

        COLONY::Load_Galaxy_Map_Anims();
    }

    void __cdecl Update_Dragon_New_Star_Fields() {
        Eric_Push_Fields();
        COLONY::Clear_Galaxy_Map_Fields();

        mp_fields[2] = fields::Add_Button_Field(
            186,
            313,
            ESTRINGS::E_Strings(12),
            anims[4],
            ESTRINGS::E_Strings(135),
            40);

        if (field_mode == 0) {
            mp_fields[25] = fields::Add_Button_Field(
                381,
                313,
                ESTRINGS::E_Strings(12),
                anims[35],
                ESTRINGS::E_Strings(561),
                40);
        } else {
            mp_fields[25] = -1000;
        }

        MOVEBOX::Add_Galaxy_Map_Fields_2(
            302,
            156,
            148,
            94,
            0,
            0,
            0,
            0,
            0,
            &COLONY::galaxy_map_name_field,
            MOX::galaxy_map_star_field,
            &MOX::galaxy_map_grid_field);
    }

    void __cdecl Load_Colony_Defense_Selection_Popup() {
        allocate::Push_Block(MOX::screen_seg);

        anims[8] = (s_animation_header*)farload::Far_Reload_Next(COLONY::MAINPUPS_LBX, 2, MOX::screen_seg);
        anims[9] = (s_animation_header*)farload::Far_Reload_Next(COLONY::MAINPUPS_LBX, 0x2f /* 47 */, MOX::screen_seg);
    }

    void __cdecl Update_Colony_Defense_Fields() {
        Eric_Push_Fields();
        COLONY::Clear_Sys_Disp_Fields();

        SYS::Wrapper_For_Informational_System_Fields(
            COLONY::sys_disp_star_handle,
            g_x,
            g_y,
            0xd,
            0x2f,
            &mp_fields[6]);
    }

    void __cdecl Draw_Defense_Selection_Popup() {
        char buffer[500];

        if (COLONY::full_draw != 0 || MOX::FULL_DRAW_SCREEN_FLAG != 0) {
            MAINPUPS::Mainpups_Tested_Execute_Autofunction();
            video::Copy_Off_To_Back();
            COLONY::full_draw = 0;
        }

        s_star_data* star = &MOX::star[COLONY::sys_disp_star_handle];
        char* race_name = MOX::player[MAINPUPS::g_player_n].race_name;
        char* format = ESTRINGS::E_Strings(0x45);

        snprintf(buffer, sizeof(buffer), format, race_name, star->name);

        SYS::Draw_System_Display_Popup(
            COLONY::sys_disp_star_handle,
            1,
            buffer,
            MAINPUPS::g_x,
            MAINPUPS::g_y,
            0,
            0,
            13,
            47);
    }

    int16_t __cdecl Evaluate_Defense_Selection_Input(int16_t* field_idx, int16_t* mouse_status, int16_t* selection_made) {
        if (*field_idx == mp_fields[6]) {
            *selection_made = 1;
            return -1;
        }

        return Check_System_Display_Fields_Defense_Selection(field_idx, mouse_status, selection_made);
    }

    void __cdecl Draw_Officer_Gains_Level_Popup() {
        char message_buffer[500];

        if (COLONY::full_draw == 0 && MOX::FULL_DRAW_SCREEN_FLAG == 0) {
            return;
        }

        COLONY::full_draw = 1;
        MOX::dont_draw_main_screen_to_back = 1;
        MOX::FULL_DRAW_SCREEN_FLAG = 1;

        Mainpups_Tested_Execute_Autofunction();
        Get_Picture_Draw_XY(anims[10]);

        animate::Draw(g_x, g_y, anims[10]);
        OFFICER::Wrapper_For_Colony_Officer_Popup_Data(g_x, g_y + 53, g_officer);
        animate::Draw(g_x + 13, g_y + 55, anims[11]);

        if (officer_gains_level_state == 0) {
            char* name = OFFICER::Leader_Name(g_officer, 0);
            char* format = ESTRINGS::E_Strings(1);
            snprintf(message_buffer, sizeof(message_buffer), format, name);
        } else if (officer_gains_level_state == 1) {
            char* name = OFFICER::Leader_Name(g_officer, 0);
            char* format = ESTRINGS::E_Strings(584);
            snprintf(message_buffer, sizeof(message_buffer), format, name);
        } else if (officer_gains_level_state == 2) {
            char* name = OFFICER::Leader_Name(g_officer, 0);
            char* format = ESTRINGS::E_Strings(585);
            snprintf(message_buffer, sizeof(message_buffer), format, name);
        }
        // Preserve the original binary behavior for unexpected state values by
        // intentionally leaving message_buffer untouched.

        ERIC::Print_Paragraph_Centered(g_x + 14, g_y + 149, 281, 62, message_buffer);

        leader_read::s_leader disclosed;
        const bool leader_readable = leader_read::Read_Result(MOX::PLAYER_NUM, g_officer, &disclosed);
        const char* leader_full_name = leader_readable ? disclosed.name : OFFICER::Leader_Name(g_officer, 1);
        ERIC::Print_Paragraph_Centered(g_x + 11, g_y + 12, 285, 33, leader_full_name);

        video::Copy_Off_To_Back();
        game_events::Publish_Leader_View(leader_read::RESULT, &disclosed, leader_readable ? 1 : 0);
        COLONY::full_draw = 0;
    }

    int16_t __cdecl Get_Colony_Best_Shield_Bldg(int16_t colony_idx) {
        int16_t best_shield_id = -1;

        if (MOX::colony[colony_idx].buildings[BUILDING_RADIATION_SHIELD] != 0) {
            best_shield_id = BUILDING_RADIATION_SHIELD;
        }

        if (MOX::colony[colony_idx].buildings[BUILDING_FLUX_SHIELD] != 0) {
            best_shield_id = BUILDING_FLUX_SHIELD;
        }

        if (MOX::colony[colony_idx].buildings[BUILDING_BARRIER_SHIELD] != 0) {
            best_shield_id = BUILDING_BARRIER_SHIELD;
        }

        return best_shield_id;
    }

    int16_t __cdecl Check_System_Display_Fields_Selection(int16_t* mouse_x, int16_t* mouse_y, int16_t* selection_confirmed) {
        int16_t planet_scanned;
        int16_t icon_scanned;
        int16_t ship_scanned;
        int16_t dummy_a;
        int16_t dummy_8;

        auto redraw_selection_popup = []() {
            Eric_Pop_Fields();
            Update_Selection_Fields();
            video::Copy_Back_To_Off();
            Draw_Selection_Popup();
            fields::Assign_Auto_Function(Draw_Selection_Popup, 1);
        };

        auto fail_selection = [&]() -> int16_t {
            redraw_selection_popup();
            return -1;
        };

        auto fail_with_help = [&](int16_t help_string_id, bool mark_full_draw, bool redraw_twice) -> int16_t {
            GENDRAW::Help(ESTRINGS::E_Strings(help_string_id));
            if (mark_full_draw) {
                COLONY::full_draw = 1;
            }
            if (redraw_twice) {
                redraw_selection_popup();
            }
            return fail_selection();
        };

        int16_t scan_res = MAINSCR::Scan_System_Display_Fields(
            *mouse_y, *mouse_x, MOX::planet_buttons, &dummy_a, &planet_scanned, &ship_scanned, &dummy_8, &icon_scanned);

        MOX::system_display_ship_scanned = ship_scanned;
        if (icon_scanned > -1) {
            MOX::system_icon_scanned = icon_scanned;
        }

        if (scan_res == 0 && planet_scanned > -1) {
            int16_t planet_idx = HACCESS::System_Planet_Scanned_To_Planet_Id(COLONY::sys_disp_star_handle, planet_scanned);

            if (selection_state_flag == 0) {
                if (MOX::planet[planet_idx].planet_type != PLANET_TYPE_PLANET) {
                    return fail_with_help(0x236, true, true);
                }

                int16_t col_idx = MOX::planet[planet_idx].colony_index;
                if (col_idx != -1 && MOX::colony[col_idx].outpost_flag == 0) {
                    return fail_with_help(0x236, true, true);
                }
            }

            if (selection_state_flag == 1) {
                if (MOX::planet[planet_idx].colony_index != -1) {
                    return fail_with_help(0x236, true, true);
                }
            }

            if (selection_state_flag == 2) {
                int16_t col_idx = MOX::planet[planet_idx].colony_index;
                if (col_idx == -1) {
                    return fail_with_help(0x23b, true, true);
                }

                if (MOX::colony[col_idx].outpost_flag != 0) {
                    return fail_with_help(0x23b, true, true);
                }

                if (MOX::colony[col_idx].owner != MOX::PLAYER_NUM) {
                    return fail_with_help(0x23b, true, true);
                }
            }

            if (selection_state_flag == 3) {
                if (MOX::planet[planet_idx].planet_type == PLANET_TYPE_PLANET) {
                    return fail_with_help(0x235, false, false);
                }

                *selection_confirmed = 1;
                return planet_idx;
            }

            if (selection_state_flag != 0) {
                *selection_confirmed = 1;
                return planet_idx;
            }

            if (New_Colony_Confirmation_Box(planet_idx) != 0) {
                *selection_confirmed = 1;
                return planet_idx;
            }

            return fail_selection();
        }

        return -1;
    }

    void __cdecl Draw_Slider_Popup() {
        uint8_t font_style[8];
        int16_t slider_offset;
        int16_t draw_x;

        memset(font_style, 0, sizeof(font_style));
        font_style[0] = 0x6e; /* 'n' */
        font_style[1] = 0x71; /* 'q' */
        font_style[2] = 0x73; /* 's' */

        fonts::Set_Remap_Font_Style(2, (s_colors*)font_style);

        if (COLONY::full_draw != 0 || MOX::FULL_DRAW_SCREEN_FLAG != 0) {
            MAINPUPS::Mainpups_Tested_Execute_Autofunction();

            s_animation_header* anim = anims[14];
            animate::Remap_Draw(MAINPUPS::g_x, MAINPUPS::g_y, anim);

            BILL::Squeeze_Formatted_Paragraph_Centered(MAINPUPS::g_x + 19, MAINPUPS::g_y + 19, 252 /* 0xFC */, 92 /* 0x5C */, MAINPUPS::g_msg, 2);
            video::Copy_Off_To_Back();
            COLONY::full_draw = 0;
        }

        fonts::Print_Integer_Centered(MAINPUPS::g_x + 32, MAINPUPS::g_y + 132, (int)MAINPUPS::g_min);
        fonts::Print_Integer_Centered(MAINPUPS::g_x + 145, MAINPUPS::g_y + 130, (int)MAINPUPS::g_var);
        fonts::Print_Integer_Centered(MAINPUPS::g_x + 257, MAINPUPS::g_y + 132, (int)MAINPUPS::g_max);

        // Calculate handle horizontal position: 207 is track width
        slider_offset = (int16_t)((207 * (MAINPUPS::g_var - MAINPUPS::g_min)) / (MAINPUPS::g_max - MAINPUPS::g_min) + 37);

        draw_x = MAINPUPS::g_x + slider_offset;
        line::Line(draw_x, MAINPUPS::g_y + 153, draw_x, MAINPUPS::g_y + 164, 32 /* ' ' */);

        draw_x++;
        line::Line(draw_x, MAINPUPS::g_y + 153, draw_x, MAINPUPS::g_y + 164, 36 /* '$' */);

        draw_x++;
        line::Line(draw_x, MAINPUPS::g_y + 153, draw_x, MAINPUPS::g_y + 164, 40 /* '(' */);

        draw_x++;
        line::Line(draw_x, MAINPUPS::g_y + 153, draw_x, MAINPUPS::g_y + 164, 36 /* '$' */);

        draw_x++;
        line::Line(draw_x, MAINPUPS::g_y + 153, draw_x, MAINPUPS::g_y + 164, 32 /* ' ' */);
    }

    void __cdecl Leader_Technology_String(int16_t player_idx, int16_t leader_idx, char* output_buf, uint32_t output_buf_size) {
        if (output_buf == nullptr || output_buf_size == 0) {
            return;
        }

        char chunk[256];
        output_buf[0] = '\0';

        for (int16_t i = 0; i < 3; ++i) {
            uint8_t tech_id = MOX::leaders[leader_idx].tech_application[i];
            if (tech_id != 0) {
                const char* prefix;
                if (i == 0) {
                    prefix = ESTRINGS::E_Strings(208 /* 0xD0 */);
                } else {
                    prefix = ", ";
                }

                strlcat(output_buf, prefix, output_buf_size);

                char* tech_name = TECHDATA::technology_applications[tech_id].name;
                const char* format = ESTRINGS::E_Strings(7);
                snprintf(chunk, sizeof(chunk), format, tech_name);
                strlcat(output_buf, chunk, output_buf_size);
            }
        }

        if (leader_idx ==
            OFFICER::progression.special_leaders.advanced_government_leader_id) {
            int8_t trait0 = MOX::player[player_idx].traits[TRAIT_CURRENT_GOVERNMENT];
            // Check if the trait index is even
            if ((trait0 % 2) == 0) {
                const char* gov_label = MOX::government_labels[trait0 + 1];
                const char* format = ESTRINGS::E_Strings(207 /* 0xCF */);
                snprintf(chunk, sizeof(chunk), format, gov_label);
                strlcat(output_buf, chunk, output_buf_size);
            }
        }

        if (output_buf[0] == '\0') {
            const char* format = ESTRINGS::E_Strings(12 /* 0xC */);
            strlcpy(output_buf, format, output_buf_size);
        }
    }

    void __cdecl Add_Selection_Fields() {
        MOX::draw_system_display_without_button = 1;

        SYS::Wrapper_For_Informational_System_Fields(
            COLONY::sys_disp_star_handle,
            g_x,
            g_y,
            13,
            47,
            &mp_fields[2]);

        char* hotkey = ESTRINGS::E_Strings(12);
        mp_fields[0] = fields::Add_Hidden_Field(0, 0, 639, 479, hotkey, 0);
    }

    void __cdecl Load_Combat_Colony_Picture_To_Screen_Seg(int16_t colony_idx) {
        int16_t planet_idx = MOX::colony[colony_idx].planet;
        int16_t climate = MOX::planet[planet_idx].climate;
        int16_t size = MOX::planet[planet_idx].size;

        int16_t pic_idx1 = size + (climate * 6);
        char* filename = ESTRINGS::E_Strings(0xDF);
        MOX::combat_planet_seg = (s_animation_header*)farload::Far_Reload_Next(filename, pic_idx1, MOX::screen_seg);

        int16_t pic_idx2 = (climate * 6) + 5;
        filename = ESTRINGS::E_Strings(0xDF);
        MOX::combat_planet_seg2 = (s_animation_header*)farload::Far_Reload_Next(filename, pic_idx2, MOX::screen_seg);

        animate::Draw_Palette(MOX::combat_planet_seg2);
        palette::Refresh_Palette();
        palette::Set_Palette_Changes(0, 255 /* 0xFF */);
        remap::Update_Glass_Remap_Colors();
    }

    int16_t __cdecl Evaluate_Colony_Combat_Input(int16_t* field_id_ptr, int16_t* unused_input, int16_t* close_popup_flag, int16_t* selection_idx_ptr) {
        (void)unused_input;
        int16_t field_id = *field_id_ptr;

        if (field_id == mp_fields[6]) {
            *close_popup_flag = 1;
            return -1;
        }

        if (field_id == -mp_fields[6]) {
            TEXTBOX::Text_Box(ESTRINGS::E_Strings(0x182), ESTRINGS::E_Strings(0x260));
        } else if (field_id == mp_fields[12]) {
            *close_popup_flag = 1;
            return 3;
        } else if (field_id == -mp_fields[12]) {
            TEXTBOX::Text_Box(ESTRINGS::E_Strings(0x150), ESTRINGS::E_Strings(0x261));
        } else if (field_id == mp_fields[13]) {
            *close_popup_flag = 1;
            MAINPUPS::no_flicker_between_mainpups_flag = 1;
            return 2;
        } else if (field_id == -mp_fields[13]) {
            TEXTBOX::Text_Box(ESTRINGS::E_Strings(0xcd), ESTRINGS::E_Strings(0x262));
        } else if (field_id == mp_fields[14]) {
            *close_popup_flag = 1;
            return 1;
        } else if (field_id == -mp_fields[14]) {
            TEXTBOX::Text_Box(ESTRINGS::E_Strings(0x108), ESTRINGS::E_Strings(0x263));
        } else if (field_id == mp_fields[15]) {
            if (ERIC::Player_Can_Mind_Control_Colony_With_Help(MAINPUPS::g_data_ptr->attacker_player_id, MAINPUPS::g_data_ptr->colony_idx)) {
                *close_popup_flag = 1;
                return 4;
            }
        } else if (field_id == -mp_fields[15]) {
            if (ERIC::Player_Can_Mind_Control_Colony(MAINPUPS::g_data_ptr->attacker_player_id, MAINPUPS::g_data_ptr->colony_idx)) {
                TEXTBOX::Text_Box(ESTRINGS::E_Strings(0x175), ESTRINGS::E_Strings(0x264));
            } else {
                ERIC::Player_Can_Mind_Control_Colony_With_Help(MAINPUPS::g_data_ptr->attacker_player_id, MAINPUPS::g_data_ptr->colony_idx);
            }
        } else if (field_id == mp_fields[16]) {
            *selection_idx_ptr = 2;
        } else if (field_id == -mp_fields[16]) {
            TEXTBOX::Text_Box(ESTRINGS::E_Strings(0x114), ESTRINGS::E_Strings(0x265));
        } else if (field_id == mp_fields[17]) {
            *selection_idx_ptr = 1;
        } else if (field_id == -mp_fields[17]) {
            TEXTBOX::Text_Box(ESTRINGS::E_Strings(0xd9), ESTRINGS::E_Strings(0x266));
        } else if (field_id == mp_fields[18]) {
            *selection_idx_ptr = 0;
            fields::Clear_Mouse_Buffer();
            fields::Set_Input_Delay(1);
            timer::Mark_Time();
        } else if ((int32_t)mp_fields[17] + field_id == 0) {
            TEXTBOX::Text_Box(ESTRINGS::E_Strings(0xc7), ESTRINGS::E_Strings(0x267));
        } else {
            *close_popup_flag = 0;
            return 0;
        }

        COLONY::full_draw = 1;
        *close_popup_flag = 0;
        return 0;
    }

    void __cdecl Draw_Dragon_New_Star_Popup() {
        char msg_buffer[300];

        if (g_scanned_star != g_old_scanned_star) {
            g_old_scanned_star = g_scanned_star;
            COLONY::full_draw = 1;
        }

        if (COLONY::full_draw != 0 || MOX::FULL_DRAW_SCREEN_FLAG != 0) {
            MOX::draw_main_screen_to_back = 1;
            Mainpups_Tested_Execute_Autofunction();
            COLONY::full_draw = 1;

            animate::Draw(0, 0, anims[37]);

            s_animation_header* anim_c = COLONY::C_Anims(12);
            s_animation_header* anim_b = COLONY::C_Anims(11);

            MOVEBOX::Draw_Galaxy_Map_Box(anim_b, anim_c, 302, 156, 148, 94, 0, 0, 0, 0, 0, 0);

            if (field_mode == 1) {
                snprintf(msg_buffer, sizeof(msg_buffer), ESTRINGS::E_Strings(464), MOX::player[g_player_n].name);
            } else {
                int16_t str_idx = (field_mode == 2) ? 583 : 555;
                snprintf(msg_buffer, sizeof(msg_buffer), ESTRINGS::E_Strings(str_idx), COMBFIND::dragon_bribe_bc, MOX::player[g_player_n].name, MOX::star[g_star_n].name);
            }

            BILL::Squeeze_Formatted_Paragraph_Centered(188, 271, 262, 35, msg_buffer, 2);
            video::Copy_Off_To_Back();
            COLONY::full_draw = 0;
        }

        if (g_scanned_star != -1) {
            fonts::Print_Centered(376, 254, MOX::star[g_scanned_star].name);
        }
    }

    int16_t __cdecl Evaluate_Dragon_New_Star_Input(int16_t* field_idx_ptr, int16_t* mouse_pos_ptr, int16_t* status_ptr, int16_t target_star_val) {
        MAINPUPS::g_scanned_star = -1;
        int16_t field_idx = *field_idx_ptr;

        if (MAINPUPS::mp_fields[2] == field_idx) {
            *status_ptr = 1;
            return -1;
        }

        if (MAINPUPS::mp_fields[25] == field_idx) {
            if (MOX::player[MAINPUPS::g_player_n].bc >=
                static_cast<int32_t>(COMBFIND::dragon_bribe_bc)) {
                MAINPUPS::field_mode = 1;
                COLONY::full_draw = 1;
                return 0;
            } else {
                COLONY::full_draw = 1;
                MAINPUPS::field_mode = 2;
                return 0;
            }
        }

        if (MAINPUPS::field_mode == 1) {
            int16_t scanned_star_id;
            int16_t scanned_val;
            int16_t scan_res = MAINSCR::Scan_Galaxy_Map_Fields(*mouse_pos_ptr, field_idx, &scanned_star_id, &scanned_val);

            if (scan_res == 4 /* FIELD_SCAN_STAR */) {
                MAINPUPS::g_scanned_star = scanned_star_id;
            } else if (scan_res == 0 /* FIELD_SCAN_EMPTY */) {
                if (scanned_val != target_star_val) {
                    *status_ptr = 1;
                    return scanned_val;
                }

                // Show help string 576 (0x240)
                GENDRAW::Help(ESTRINGS::E_Strings(576 /* 0x240 */));
                COLONY::full_draw = 1;
            }
        }

        if (MOX::FULL_DRAW_SCREEN_FLAG != 0) {
            COLONY::full_draw = 1;
        }

        return 0;
    }

    void __cdecl Load_Officer_Gains_Level_Popup() {
        allocate::Push_Block(MOX::screen_seg);

        anims[10] = (s_animation_header*)farload::Far_Reload_Next(COLONY::MAINPUPS_LBX, 0x33, MOX::screen_seg);

        anims[11] = OFFICER::Load_Officer_Picture(
            MOX::PLAYER_NUM,
            MAINPUPS::g_officer,
            MOX::screen_seg,
            0,
            0);

        OFFICER::Load_Officer_Skill_Icons(MOX::screen_seg);
    }

    int8_t __cdecl New_Colony_Confirmation_Box(int16_t planet_idx) {
        s_potential_colony_info potential_info;
        char msg_buffer[2000];
        char chunk[256];
        int16_t food_val;
        int16_t food_rem;

        ERICHELP::Set_New_Colony_Confirmation_Box_Help_List();

        COLCALC::Compute_Potential_Colony_Info(
            &MOX::player[MOX::PLAYER_NUM],
            &MOX::planet[planet_idx],
            &potential_info);

        char* planet_name = HACCESS::Get_Planet_Name(planet_idx);
        char* format_title = ESTRINGS::E_Strings(0xD1); /* 209 */
        snprintf(msg_buffer, sizeof(msg_buffer), format_title, planet_name);

        food_val = potential_info.food;
        food_rem = food_val % 2;

        if (food_rem != 0) {
            char* format_food_odd = ESTRINGS::E_Strings(0x64); /* 100 */
            snprintf(chunk, sizeof(chunk), format_food_odd, ESTRINGS::s_0_0055110c, ESTRINGS::s_1_00551110, (int)(food_val / 2), (int)food_rem);
        } else {
            char* format_food_even = ESTRINGS::E_Strings(0x65); /* 101 */
            snprintf(chunk, sizeof(chunk), format_food_even, ESTRINGS::s_0_0055110c, ESTRINGS::s_1_00551110, (int)(food_val / 2));
        }
        strlcat(msg_buffer, chunk, sizeof(msg_buffer));

        char* format_ind = ESTRINGS::E_Strings(0x6B); /* 107 */
        snprintf(chunk, sizeof(chunk), format_ind, ESTRINGS::s_0_0055110c, ESTRINGS::s_1_00551110, (int)potential_info.industry);
        strlcat(msg_buffer, chunk, sizeof(msg_buffer));

        char* format_res = ESTRINGS::E_Strings(0x73); /* 115 */
        snprintf(chunk, sizeof(chunk), format_res, ESTRINGS::s_0_0055110c, ESTRINGS::s_1_00551110, (int)potential_info.research);
        strlcat(msg_buffer, chunk, sizeof(msg_buffer));

        char* format_maint = ESTRINGS::E_Strings(0x6C); /* 108 */
        snprintf(chunk, sizeof(chunk), format_maint, ESTRINGS::s_0_0055110c, ESTRINGS::s_1_00551110, (int)potential_info.maintenance);
        strlcat(msg_buffer, chunk, sizeof(msg_buffer));

        char* format_bonus = ESTRINGS::E_Strings(0x78); /* 120 */
        snprintf(chunk, sizeof(chunk), format_bonus, ESTRINGS::s_0_0055110c, ESTRINGS::s_1_00551110, (int)potential_info.worker_bonus);
        strlcat(msg_buffer, chunk, sizeof(msg_buffer));

        int16_t max_pop = COLCALC::Planet_Max_Population_For_Player(&MOX::planet[planet_idx], MOX::PLAYER_NUM);
        char* format_pop = ESTRINGS::E_Strings(0x6E); /* 110 */
        snprintf(chunk, sizeof(chunk), format_pop, ESTRINGS::s_0_0055110c, ESTRINGS::s_1_00551110, (int)max_pop);
        strlcat(msg_buffer, chunk, sizeof(msg_buffer));

        int8_t result = (int8_t)GENDRAW::Confirmation_Box(msg_buffer);
        fields::Deactivate_Help_List();

        return result;
    }

    void __cdecl Draw_Colony_Combat_Popup() {
        s_colors colors_cfg = {0};
        colors_cfg.colors[1] = 0x11;
        colors_cfg.colors[2] = 0x13;

        char text_buffer[500];

        if (COLONY::full_draw != 0 || MOX::FULL_DRAW_SCREEN_FLAG != 0) {
            Mainpups_Tested_Execute_Autofunction();

            int16_t anim_w = animate::Get_Width(anims[3]);
            int16_t anim_h = animate::Get_Height(anims[3]);

            graphics::Fill(g_x + 5, g_y + 5, anim_w - 10, anim_h - 10, 0);
            animate::Draw(g_x, g_y, anims[3]);

            int16_t x_center = (g_x * 2 + anim_w) / 2;
            int16_t y_center = (g_y * 2 + anim_h) / 2;
            ERIC::Draw_Centered(x_center, y_center, MOX::combat_planet_seg);

            s_animation_header* c_anims = COLONY::C_Anims(13);
            remap::Calculate_Picture_Remap_Colors(c_anims);
            fonts::Set_Font_Style(3, &colors_cfg);

            if (g_state == 0 || g_state == 1) {
                char* planet_name = HACCESS::Get_Planet_Name(MOX::colony[g_data_ptr->colony_idx].planet);
                snprintf(text_buffer, sizeof(text_buffer), ESTRINGS::E_Strings(458), planet_name);
            } else if (g_state == 2) {
                char* planet_name = HACCESS::Get_Planet_Name(MOX::colony[g_data_ptr->colony_idx].planet);
                snprintf(text_buffer, sizeof(text_buffer), ESTRINGS::E_Strings(337), planet_name);
            }

            fonts::Print_Centered(x_center, g_y + 20, text_buffer);

            video::Copy_Off_To_Back();
            COLONY::full_draw = 0;
        }

        fonts::Set_Font_Style(1, &colors_cfg);

        int16_t list_idx = 0;

        if (g_data_ptr->n_bldgs > 0) {
            animate::Remap_Draw(g_x + 20, g_y + 50, anims[23]);
            snprintf(text_buffer, sizeof(text_buffer), ESTRINGS::E_Strings(19), g_data_ptr->n_bldgs);
            fonts::Print(g_x + 55, g_y + 50, text_buffer);
            list_idx = 1;
        }

        if (g_data_ptr->best_shield_bldg != -1) {
            int16_t y_offs = list_idx * 30;
            animate::Remap_Draw(g_x + 20, g_y + y_offs + 50, anims[24]);

            char* bldg_name = COLONY::CR_Building_Name(g_data_ptr->best_shield_bldg);
            snprintf(text_buffer, sizeof(text_buffer), ESTRINGS::E_Strings(70), bldg_name);
            fonts::Print(g_x + 55, g_y + y_offs + 50, text_buffer);
            list_idx++;
        }

        if (g_data_ptr->n_pops > 0) {
            int16_t y_offs = list_idx * 30;

            uint32_t pop_val = MOX::colony[g_data_ptr->colony_idx].pop[0];
            int16_t anim_idx = (pop_val >> 7) & 3;
            uint8_t race = MOX::player[g_data_ptr->defender_player_id].race;

            s_animation_header* pop_anim = COLONY::People_Anim(anim_idx, 2, race);
            animate::Remap_Draw(g_x + 20, g_y + y_offs + 50, pop_anim);

            snprintf(text_buffer, sizeof(text_buffer), ESTRINGS::E_Strings(20), g_data_ptr->n_pops);
            fonts::Print(g_x + 55, g_y + y_offs + 50, text_buffer);
            list_idx++;
        }

        int16_t y_offs = list_idx * 30;
        for (int16_t unit_idx = 1; unit_idx >= 0; unit_idx--) {
            if (g_data_ptr->defender_unit_counts[unit_idx] > 0) {
                uint8_t race = MOX::player[g_data_ptr->defender_player_id].race;
                s_animation_header* unit_anim = COLONY::Player_Troop_Anim(g_data_ptr->defender_player_id, unit_idx, race);

                animate::Remap_Draw(g_x + 20, g_y + y_offs + 50, unit_anim);

                char* troop_name = COLONY::Player_Troop_Name(g_data_ptr->defender_player_id, unit_idx, race);

                if (MOX::settings.language == 4) {
                    snprintf(text_buffer, sizeof(text_buffer), "%s: %d", troop_name, g_data_ptr->defender_unit_counts[unit_idx]);
                } else {
                    snprintf(text_buffer, sizeof(text_buffer), ESTRINGS::E_Strings(14), g_data_ptr->defender_unit_counts[unit_idx], troop_name);
                }

                fonts::Print(g_x + 55, g_y + y_offs + 50, text_buffer);
                y_offs += 30;
            }
        }

        int16_t right_list_idx = 0;
        if (g_data_ptr->attacker_unit_counts[1] > 0) {
            uint8_t attacker_race = MOX::player[g_data_ptr->attacker_player_id].race;
            s_animation_header* att_anim = COLONY::Player_Troop_Anim(g_data_ptr->attacker_player_id, 1, attacker_race);

            int16_t anim_w = animate::Get_Width(anims[3]);
            animate::Remap_Draw(g_x + anim_w - 40, g_y + 50, att_anim);

            char* att_troop_name = COLONY::Player_Troop_Name(g_data_ptr->attacker_player_id, 1, attacker_race);
            char* fmt = ESTRINGS::E_Strings(14);

            snprintf(text_buffer, sizeof(text_buffer), fmt, g_data_ptr->attacker_unit_counts[1], att_troop_name);
            anim_w = animate::Get_Width(anims[3]);
            fonts::Print_Right(g_x + anim_w - 45, g_y + 50, text_buffer);
            right_list_idx = 1;
        }

        if (g_data_ptr->bombardment_result >= 0) {
            int16_t right_y_offs = right_list_idx * 30;
            int16_t anim_w = animate::Get_Width(anims[3]);

            animate::Remap_Draw(g_x + anim_w - 40, g_y + right_y_offs + 50, anims[22]);

            if (g_state == 0 || g_state == 2) {
                snprintf(text_buffer, sizeof(text_buffer), ESTRINGS::E_Strings(21), g_data_ptr->bombardment_result);
            } else {
                snprintf(text_buffer, sizeof(text_buffer), ESTRINGS::E_Strings(18), g_data_ptr->bombardment_result);
            }

            anim_w = animate::Get_Width(anims[3]);
            fonts::Print_Right(g_x + anim_w - 45, g_y + right_y_offs + 50, text_buffer);
        }

        FLCQUEUE::Draw_Flic_Queue();
    }

    void __cdecl Add_Colony_Combat_Fields(int16_t popup_mode, int16_t* combat_state_ptr) {
        int16_t si = 1;

        mp_fields[6] = fields::Add_Button_Field(
            g_x + 0xDD,
            animate::Get_Height(anims[3]) + g_y - 0x27,
            ESTRINGS::E_Strings(0xC),
            anims[9],
            ESTRINGS::E_Strings(0x87),
            0x28);

        if (popup_mode == 1) {
            if (g_data_ptr->bombardment_result < 0 || *combat_state_ptr == 0 || *combat_state_ptr == 3 || *combat_state_ptr == 2) {
                mp_fields[18] = -1000;
            } else {
                mp_fields[18] = fields::Add_Button_Field(
                    g_x + 0xDD,
                    animate::Get_Height(anims[3]) + g_y - 0x40,
                    ESTRINGS::E_Strings(0xC),
                    anims[16],
                    ESTRINGS::E_Strings(0xC1),
                    0x28);
                si = 2;
            }

            if (g_data_ptr->bombardment_result <= 0 || *combat_state_ptr == 1 || *combat_state_ptr == 3 || *combat_state_ptr == 2) {
                mp_fields[17] = -1000;
            } else {
                mp_fields[17] = fields::Add_Button_Field(
                    g_x + 0xDD,
                    animate::Get_Height(anims[3]) - (si * 25) + g_y - 0x27,
                    ESTRINGS::E_Strings(0xC),
                    anims[18],
                    ESTRINGS::E_Strings(0x13C),
                    0x28);
                si++;
            }

            if (g_data_ptr->bombardment_result <= 0 || *combat_state_ptr == 2 || *combat_state_ptr == 3) {
                mp_fields[16] = -1000;
            } else {
                mp_fields[16] = fields::Add_Button_Field(
                    g_x + 0xDD,
                    animate::Get_Height(anims[3]) - (si * 25) + g_y - 0x27,
                    ESTRINGS::E_Strings(0xC),
                    anims[19],
                    ESTRINGS::E_Strings(0xA6),
                    0x28);
                si++;
            }
        }

        if (g_data_ptr->bombardment_result < 0 || popup_mode == 1 || popup_mode == 2) {
            if (mp_fields[13] != -1000) {
                fields::Clear_Mouse_Buffer();
                fields::Set_Input_Delay(3);
            }
            mp_fields[13] = -1000;
        } else {
            mp_fields[13] = fields::Add_Button_Field(
                g_x + 0xDD,
                animate::Get_Height(anims[3]) - (si * 25) + g_y - 0x27,
                ESTRINGS::E_Strings(0xC),
                anims[16],
                ESTRINGS::E_Strings(0xC1),
                0x28);
            si++;
        }

        if (g_data_ptr->attacker_unit_counts[UNIT_MARINES] <= 0 ||
            (g_data_ptr->n_pops <= 0 && g_data_ptr->n_bldgs <= 0) ||
            popup_mode == 2) {
            mp_fields[12] = -1000;
        } else {
            mp_fields[12] = fields::Add_Button_Field(
                g_x + 0xDD,
                animate::Get_Height(anims[3]) - (si * 25) + g_y - 0x27,
                ESTRINGS::E_Strings(0xC),
                anims[15],
                ESTRINGS::E_Strings(0x147),
                0x28);
            si++;
        }

        if (MOX::player[g_data_ptr->attacker_player_id].traits[TRAIT_TELEPATHIC] == 0 || popup_mode == 2 || g_data_ptr->n_pops <= 0) {
            mp_fields[15] = -1000;
        } else {
            s_animation_header* anim;
            if (ERIC::Player_Can_Mind_Control_Colony(g_data_ptr->attacker_player_id, g_data_ptr->colony_idx)) {
                anim = anims[20];
            } else {
                anim = anims[21];
            }
            mp_fields[15] = fields::Add_Button_Field(
                g_x + 0xDD,
                animate::Get_Height(anims[3]) - (si * 25) + g_y - 0x27,
                ESTRINGS::E_Strings(0xC),
                anim,
                ESTRINGS::E_Strings(0x16D),
                0x28);
            si++;
        }

        if (g_data_ptr->has_stellar_converter == 0 || popup_mode == 2 || g_data_ptr->n_pops <= 0) {
            mp_fields[14] = -1000;
        } else {
            mp_fields[14] = fields::Add_Button_Field(
                g_x + 0xDD,
                animate::Get_Height(anims[3]) - (si * 25) + g_y - 0x27,
                ESTRINGS::E_Strings(0xC),
                anims[17],
                ESTRINGS::E_Strings(0xFB),
                0x28);
        }

        mp_fields[0] = fields::Add_Hidden_Field(0, 0, 0x27F, 0x1DF, ESTRINGS::E_Strings(0xC), 0);
    }

    int16_t __cdecl Check_System_Display_Fields_Defense_Selection(int16_t* mouse_x, int16_t* mouse_y, int16_t* selection_made) {
        int16_t scanned_planet_field;
        int16_t scanned_planet_icon;
        int16_t scanned_ship_field;
        int16_t scanned_ship_icon;
        int16_t scanned_system_icon;

        int16_t scan_result = MAINSCR::Scan_System_Display_Fields(
            *mouse_y,
            *mouse_x,
            MOX::planet_buttons,
            &scanned_planet_field,
            &scanned_planet_icon,
            &scanned_ship_field,
            &scanned_ship_icon,
            &scanned_system_icon);

        MOX::system_display_ship_scanned = scanned_ship_field;

        if (MOX::FULL_DRAW_SCREEN_FLAG != 0) {
            COLONY::full_draw = 1;
            MAINPUPS::Eric_Pop_Fields();
            MAINPUPS::Update_Colony_Defense_Fields();
            video::Copy_Back_To_Off();
            MAINPUPS::Draw_Defense_Selection_Popup();
            fields::Assign_Auto_Function(MAINPUPS::Draw_Defense_Selection_Popup, 1);
        }

        if (scanned_system_icon > -1) {
            MOX::system_icon_scanned = scanned_system_icon;
        }

        if (scan_result == 0) {
            if (scanned_planet_icon > -1) {
                int16_t planet_id = HACCESS::System_Planet_Scanned_To_Planet_Id(
                    COLONY::sys_disp_star_handle,
                    scanned_planet_icon);

                int16_t colony_idx = MOX::planet[planet_id].colony_index;

                if (colony_idx == -1) {
                    int16_t str_id;
                    if (MOX::planet[planet_id].planet_type == PLANET_TYPE_PLANET) {
                        str_id = 0x250;
                    } else {
                        str_id = 0x24F;
                    }

                    GENDRAW::Help(ESTRINGS::E_Strings(str_id));
                    COLONY::full_draw = 1;
                    MAINPUPS::Eric_Pop_Fields();
                    MAINPUPS::Update_Colony_Defense_Fields();
                    video::Copy_Back_To_Off();
                    MAINPUPS::Draw_Defense_Selection_Popup();
                    fields::Assign_Auto_Function(MAINPUPS::Draw_Defense_Selection_Popup, 1);
                } else {
                    if (MOX::colony[colony_idx].owner != MAINPUPS::g_player_n) {
                        *selection_made = 1;
                        return colony_idx + 15;
                    }

                    GENDRAW::Help(ESTRINGS::E_Strings(0x252));
                    COLONY::full_draw = 1;
                    MAINPUPS::Eric_Pop_Fields();
                    MAINPUPS::Update_Colony_Defense_Fields();
                    video::Copy_Back_To_Off();
                    MAINPUPS::Draw_Defense_Selection_Popup();
                    fields::Assign_Auto_Function(MAINPUPS::Draw_Defense_Selection_Popup, 1);
                }
            }

            if (scanned_ship_icon > -1) {
                int16_t ship_id = HACCESS::System_Ship_Scanned_To_Ship_Id(scanned_ship_icon);
                int16_t owner = (int16_t)MOX::ship[ship_id].owner;

                if (MAINPUPS::g_player_n == owner) {
                    GENDRAW::Help(ESTRINGS::E_Strings(0x253));
                    COLONY::full_draw = 1;
                    MAINPUPS::Eric_Pop_Fields();
                    MAINPUPS::Update_Colony_Defense_Fields();
                    video::Copy_Back_To_Off();
                    MAINPUPS::Draw_Defense_Selection_Popup();
                    fields::Assign_Auto_Function(MAINPUPS::Draw_Defense_Selection_Popup, 1);
                } else {
                    if (MAINPUPS::Player_Has_Colony_Or_Outpost_At_Star(owner, COLONY::sys_disp_star_handle) != 0) {
                        GENDRAW::Help(ESTRINGS::E_Strings(0x24E));
                        COLONY::full_draw = 1;
                        MAINPUPS::Eric_Pop_Fields();
                        MAINPUPS::Update_Colony_Defense_Fields();
                        video::Copy_Back_To_Off();
                        MAINPUPS::Draw_Defense_Selection_Popup();
                        fields::Assign_Auto_Function(MAINPUPS::Draw_Defense_Selection_Popup, 1);
                        return -1;
                    } else {
                        *selection_made = 1;
                        return owner;
                    }
                }
            }
        }

        return -1;
    }

    void __cdecl Set_G_Special_Value(int16_t star_idx) {
        if (((MOX::star[star_idx].just_visited >> static_cast<uint8_t>(MOX::PLAYER_NUM)) & 1) != 0) {
            g_special = (uint8_t)REPORT::Draw_Special(&MOX::star[star_idx], MOX::PLAYER_NUM);
            MOX::star[star_idx].just_visited &= static_cast<uint8_t>(~(1 << static_cast<uint8_t>(MOX::PLAYER_NUM)));

            if (MOX::star[star_idx].system_special >= 50) {
                MOX::star[star_idx].system_special = SYSTEM_SPECIAL_NO_SPECIAL;
            }
        } else {
            g_special = SYSTEM_SPECIAL_NO_SPECIAL;
        }
    }
} // namespace MAINPUPS
