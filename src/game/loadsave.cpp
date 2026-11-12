#include "pch.h"

namespace LOADSAVE {
    char months[12][6];

    uint8_t draw_game_popup_to_back;
    uint8_t dont_draw_darkened_buttons;
    uint8_t draw_down;
    char empty_save_slot_message[20];

    int16_t music_level_field = -1000;
    int16_t sfx_level_field = -1000;
    uint8_t print_data_to_bitmap;
    s_animation_header* bitmap;

    s_game_anims* game_anims;

    s_game_popup_fields* game_popup_fields;
    uint8_t loaded_new_game;
    uint8_t loaded_debug_save;

    void __cdecl Allocate_Data_Space_For_Game_Popup() {
        // Allocate space for game animations and clear the block
        game_anims = (s_game_anims*)allocate::Allocate_First_Block(MOX::screen_seg, sizeof(*game_anims));
        memset(game_anims, 0, sizeof(*game_anims));

        // Allocate buffers for game dates and stardates (1000 bytes each)
        MOX::save_game_dates = (char*)allocate::Allocate_Next_Block(MOX::screen_seg, 1000);
        MOX::save_game_stardates = (char*)allocate::Allocate_Next_Block(MOX::screen_seg, 1000);

        // Allocate space for the popup fields structure.
        game_popup_fields = (s_game_popup_fields*)allocate::Allocate_Next_Block(MOX::screen_seg, sizeof(*game_popup_fields));
    }

    void __cdecl Load_Month_Names() {
        auto copy_message = [](int32_t msg_id, char* dest) {
            const char* src = HAROLD::H_Message((int16_t)msg_id);
            if (src) {
                const char* s = src;
                char* d = dest;
                while ((*d++ = *s++) != '\0');
            }
        };

        copy_message(0xBE, months[0]);  // January
        copy_message(0xBF, months[1]);  // February
        copy_message(0xC0, months[2]);  // March
        copy_message(0xC1, months[3]);  // April
        copy_message(0xC2, months[4]);  // May
        copy_message(0xC3, months[5]);  // June
        copy_message(0xC4, months[6]);  // July
        copy_message(0xC5, months[7]);  // August
        copy_message(0xC6, months[8]);  // September
        copy_message(0xC7, months[9]);  // October
        copy_message(0xC8, months[10]); // November
        copy_message(0xC9, months[11]); // December
    }

    void __cdecl Restore_Main_Screen() {
        video::Set_Page_Off();
        MAINSCR::Draw_Mini_Main_Screen();
        video::Copy_Off_To_Back();
        video::Set_Refresh_Stencil();
        MOX::FULL_DRAW_SCREEN_FLAG = 0;
    }

    void __cdecl Restore_Game_Popup() {
        if (MOX::current_screen == SCREEN_MAIN_MENU) {
            video::Set_Page_Back();
            graphics::Fill(0, 0, 639, 479, 0);
            animate::Draw(0, 0, reinterpret_cast<s_animation_header*>(MAINMENU::mainmenu_first_seg));
        } else {
            LOADSAVE::Restore_Main_Screen();
        }

        video::Set_Page_Off();
        fields::Invoke_Auto_Function();
        RUSS::Mox_Sync_Update();
    }

    int16_t __cdecl Game_Popup_User_Box(const char* message, int16_t type) {
        int16_t result = 0;

        if (type == 0) {
            video::Set_Refresh_Full();
            draw_game_popup_to_back = 1;
            GENDRAW::Message_Box(message);
            fields::Clear_Fields();
            Restore_Game_Popup();
        } else if (type == 1) {
            video::Set_Refresh_Full();
            draw_game_popup_to_back = 1;
            result = GENDRAW::Do_Confirmation_Box(message);
            fields::Clear_Fields();
            Restore_Game_Popup();
        } else if (type == 3) {
            video::Set_Refresh_Full();
            draw_game_popup_to_back = 1;
            GENDRAW::Warning_Box(message);
            fields::Clear_Fields();
            Restore_Game_Popup();
        }

        return result;
    }

    bool __cdecl Active_Save_Slot_Invalid() {
        const int8_t save_slot = static_cast<int8_t>(MOX::settings.active_save_slot);

        if (save_slot >= 0 && save_slot < 10 &&
            LOADSAVE::game_popup_fields->slot_occupied_flags[save_slot] > 0) {
            return false;
        }

        return true;
    }

    void __cdecl Set_Up_Load_Save_Popup() {
        FILEDEF::Get_Saved_Game_Descriptions(LOADSAVE::game_popup_fields->slot_occupied_flags);

        for (int i = 0; i < 10; ++i) {
            char* desc = MOX::save_game_description[i].description;
            int32_t len = 0;
            while (len < 37 && desc[len] != '\0') {
                len++;
            }
            if (len < 37) {
                for (int j = len; j < 37; ++j) {
                    desc[j] = '\0';
                }
            }
        }

        sound::Sound_Update();
        Get_Save_Game_Date_Strings(MOX::save_game_dates);
        sound::Sound_Update();

        Get_Star_Date_Strings(LOADSAVE::game_popup_fields->slot_occupied_flags, MOX::save_game_stardates);

        for (int i = 0; i < 10; ++i) {
            char* star_date_entry = MOX::save_game_stardates + (i * 25);
            char* desc_entry = MOX::save_game_description[i].description;

            if (desc_entry[0] == '\0' && star_date_entry[0] != '\0') {
                const char* fmt = HAROLD::H_Message(203);
                snprintf(MOX::save_game_description[i].description, sizeof(MOX::save_game_description[i].description), fmt, i + 1);
            }
        }

        if (LOADSAVE::Active_Save_Slot_Invalid() != 0) {
            MOX::settings.active_save_slot = -1;
        }
    }

    void __cdecl Add_Game_Popup_Fields(int16_t* out_x, int16_t* out_y) {
        music_level_field = -1000;
        sfx_level_field = -1000;

        switch ((int16_t)MOX::screen_data) {
            case 0: {
                MOX::popup_base_x = 0x90;
                MOX::popup_base_y = 0x19;
                *out_x = animate::Get_Width(game_anims->pictures[0]);
                *out_y = animate::Get_Height(game_anims->pictures[0]);

                if (MOX::game_type == 0 || MOX::game_type == 1) {
                    game_popup_fields->btn_main_load_id = fields::Add_Button_Field(MOX::popup_base_x + 0x93, MOX::popup_base_y + 0x2b, "", game_anims->pictures[2], "L", '(');
                    game_popup_fields->btn_main_new_id = fields::Add_Button_Field(MOX::popup_base_x + 0x28, MOX::popup_base_y + 0x58, "", game_anims->pictures[3], "N", '(');
                } else {
                    game_popup_fields->btn_main_load_id = -1000;
                    game_popup_fields->btn_main_new_id = -1000;
                }

                game_popup_fields->btn_main_save_id = fields::Add_Button_Field(MOX::popup_base_x + 0x28, MOX::popup_base_y + 0x2b, "", game_anims->pictures[1], "S", '(');
                game_popup_fields->btn_main_quit_id = fields::Add_Button_Field(MOX::popup_base_x + 0x93, MOX::popup_base_y + 0x58, "", game_anims->pictures[4], "Q", '(');
                game_popup_fields->btn_main_options_id = fields::Add_Button_Field(MOX::popup_base_x + 0x28, MOX::popup_base_y + 0x133, "", game_anims->pictures[5], "O", '(');
                game_popup_fields->btn_main_close_id = fields::Add_Button_Field(MOX::popup_base_x + 0x97, MOX::popup_base_y + 0x133, "", game_anims->pictures[6], "\x1B", '(');

                music_level_field = fields::Add_Scroll_Field(MOX::popup_base_x + 0x3e, MOX::popup_base_y + 0xc2, 0, 0x9c, 0, 0x9c, 0x9b, 0xc, &game_popup_fields->music_level, "", '\0');
                sfx_level_field = fields::Add_Scroll_Field(MOX::popup_base_x + 0x3e, MOX::popup_base_y + 0xd8, 0, 0x9c, 0, 0x9c, 0x9b, 0xc, &game_popup_fields->sound_fx_level, "", '\0');
                break;
            }
            case 1: {
                MOX::popup_base_x = 0x90;
                MOX::popup_base_y = 0x19;
                *out_x = animate::Get_Width(game_anims->pictures[8]);
                *out_y = animate::Get_Height(game_anims->pictures[8]);

                int16_t offset_y = 0;
                for (int16_t i = 0; i < 13; ++i) {
                    int16_t bx = MOX::popup_base_x + 0x1a;
                    int16_t by = MOX::popup_base_y + 0x29 + offset_y;
                    s_animation_header* pic = game_anims->pictures[9];
                    game_popup_fields->option_names[i] = fields::Add_Hidden_Field(bx, by, animate::Get_Width(pic) + bx, animate::Get_Height(pic) + by, "", '(');

                    game_popup_fields->option_toggles[i] = fields::Add_Hidden_Field(MOX::popup_base_x + 0x31, by, MOX::popup_base_x + 0xf3, animate::Get_Height(pic) + by, "", ')');
                    offset_y += 0x11;
                }
                game_popup_fields->btn_settings_accept_id = fields::Add_Button_Field(MOX::popup_base_x + 0x61, MOX::popup_base_y + 0x152, "", game_anims->pictures[10], "A", '(');
                break;
            }
            case 2:
            case 3: {
                if (MOX::current_screen == SCREEN_MAIN_MENU) {
                    int32_t dx = 0x280 - animate::Get_Width(game_anims->pictures[0xb]);
                    MOX::popup_base_x = (int16_t)((dx - (dx >> 31)) >> 1);
                    int32_t dy = 0x1e0 - animate::Get_Height(game_anims->pictures[0xb]);
                    MOX::popup_base_y = (int16_t)((dy - (dy >> 31)) >> 1);
                } else {
                    MOX::popup_base_x = 0x90;
                    MOX::popup_base_y = 0x19;
                }

                s_animation_header* main_pic;
                if (MOX::screen_data == 2) {
                    main_pic = game_anims->pictures[0xb];
                } else {
                    main_pic = game_anims->pictures[0xe];
                }
                *out_x = animate::Get_Width(main_pic);
                *out_y = animate::Get_Height(main_pic);

                Check_For_Saved_Games(game_popup_fields->slot_status);
                s_colors colors_style, colors_special;
                Get_Game_Popup_Colors(2, &colors_style, &colors_special);
                fonts::Set_Font_Special_Colors(&colors_special);

                for (int16_t i = 0; i < 10; ++i) {
                    if (MOX::settings.active_save_slot == i) {
                        Embed_Special_Color_Codes(MOX::save_game_description[i].description, 0x25);
                    } else if (MOX::save_game_description[i].description[0] == 0x03) {
                        Remove_Embedded_Special_Codes(MOX::save_game_description[i].description);
                    }
                }

                if (MOX::screen_data == 2) {
                    for (int16_t i = 0; i < 10; ++i) {
                        if (game_popup_fields->slot_status[i] == 2) {
                            memcpy(MOX::save_game_description[i].description, "* INVALID *", sizeof("* INVALID *"));
                        }
                        game_popup_fields->saved_game_types[i] = FILEDEF::Get_Game_Type_From_Saved_Game(i);
                        game_popup_fields->saved_game_fields[i] = fields::Add_Hidden_Field(MOX::popup_base_x + 0x1d, MOX::popup_base_y + 0x18 + i * 0x1f, MOX::popup_base_x + 0xe2, MOX::popup_base_y + 0x18 + i * 0x1f + 0x18, "", ')');
                    }
                    game_popup_fields->debug_hotspot_id = fields::Add_Hidden_Field(0x275, 0x1d5, 0x27f, 0x1df, "D", '3');
                    game_popup_fields->btn_popup_load_id = fields::Add_Button_Field(MOX::popup_base_x + 0x25, MOX::popup_base_y + 0x151, "", game_anims->pictures[0xc], "L", '(');
                } else {
                    for (int16_t i = 0; i < 10; ++i) {
                        fonts::Set_Font_Style(2, &colors_style);
                        if (game_popup_fields->slot_status[i] == 2) {
                            memcpy(MOX::save_game_description[i].description, "* INVALID *", sizeof("* INVALID *"));
                        }
                        game_popup_fields->saved_game_types[i] = FILEDEF::Get_Game_Type_From_Saved_Game(i);
                        game_popup_fields->saved_game_fields[i] = fields::Add_Continuous_String_Input_Field(MOX::popup_base_x + 0x1a, MOX::popup_base_y + 0x18 + i * 0x1f, 200, fonts::Get_Font_Height(), MOX::save_game_description[i].description, 0x1e, 0, 0, ')', 0);
                    }
                    for (int16_t i = 0; i < 10; ++i) {
                        int16_t y_start = MOX::popup_base_y + 0x18 + i * 0x1f;
                        int16_t font_h = fonts::Get_Font_Height();
                        game_popup_fields->save_slot_fields[i] = fields::Add_Hidden_Field(MOX::popup_base_x + 0x1d, y_start + font_h + 1, MOX::popup_base_x + 0xf4, y_start + font_h * 2, "", ')');
                    }
                    game_popup_fields->debug_hotspot_id = -1000;
                    game_popup_fields->btn_popup_save_id = fields::Add_Button_Field(MOX::popup_base_x + 0x25, MOX::popup_base_y + 0x152, "", game_anims->pictures[0xf], "S", '(');
                }
                game_popup_fields->btn_popup_close_id = fields::Add_Button_Field(MOX::popup_base_x + 0xab, MOX::popup_base_y + 0x152, "", game_anims->pictures[0xd], "C", '(');
                break;
            }
            default:
                break;
        }

        game_popup_fields->field_space = fields::Add_Hidden_Field(MOX::popup_base_x, MOX::popup_base_y, *out_x + MOX::popup_base_x, *out_y + MOX::popup_base_y, "", '\0');
        game_popup_fields->screen_cancel_field_id = fields::Add_Hidden_Field(0, 0, 0x27f, 0x1df, "\x1B", ')');
    }

    int8_t __cdecl Do_Load_Game_Popup(int16_t* exit_flag) {
        int16_t exit_loop = 0;
        int8_t load_success = 0;
        int16_t unused1, unused2;
        char msg_buffer[80];

        Load_Month_Names();

        const char* empty_msg_src = HAROLD::H_Message(388); // 0x184
        char* empty_msg_dst = empty_save_slot_message;
        while (*empty_msg_src) {
            *empty_msg_dst++ = *empty_msg_src++;
        }
        *empty_msg_dst = '\0';

        bool from_menu = (MOX::current_screen == SCREEN_MAIN_MENU);
        loaded_debug_save = 0;

        if (!from_menu) {
            EVANHELP::Set_Load_Game_Screen_Help_List();
            fields::Clear_Fields();
            MOX::global_var = static_cast<int8_t>(fields::Add_Hidden_Field(0, 0, 639, 479, "\x1B", 41));
        }

        fields::Assign_Auto_Function(_Draw_Load_Save_Game_Popup, 1);
        fields::Clear_Fields();
        Add_Game_Popup_Fields(&unused1, &unused2);
        MOX::global_var = 0;

        do {
            timer::Mark_Time();
            int16_t input_id = fields::Get_Input();
            int16_t selected_slot = -1;

            for (int16_t i = 0; i < 10; ++i) {
                if (game_popup_fields->saved_game_fields[i] == input_id) {
                    selected_slot = i;
                    break;
                }
            }

            if (game_popup_fields->btn_popup_load_id == input_id) {
                if (selected_slot == -1) {
                    if (MOX::settings.active_save_slot > -1 && MOX::settings.active_save_slot < 10) {
                        draw_down = 1;
                        selected_slot = static_cast<int16_t>(MOX::settings.active_save_slot);
                    }
                }
            }

            if (selected_slot >= 0 && selected_slot < 10) {
                if (game_popup_fields->slot_status[selected_slot] == 0) {
                    if (MOX::settings.animations_on != 0 && MOX::previous_screen == SCREEN_MAIN) {
                        fields::Clear_Fields();
                        game_anims->animating = 1;
                        video::Set_Page_Off();
                        Animate_Game_Popup_Pic_Height(game_anims->pictures[11], 1);
                    }

                    load_success = static_cast<int8_t>(FILEDEF::Load_Game(selected_slot));
                    MOX::settings.active_save_slot = static_cast<int8_t>(selected_slot);
                    Save_Session_Related_Settings();

                    if (load_success == 0) {
                        const char* msg = HAROLD::H_Message(184); // 0xb8
                        snprintf(msg_buffer, sizeof(msg_buffer), msg, static_cast<int>(selected_slot + 1));
                        Game_Popup_User_Box(msg_buffer, 3);
                        MOX::return_screen = MOX::previous_screen;
                    } else {
                        loaded_new_game = 1;
                        MOX::return_screen = (MOX::game_type == 1) ? SCREEN_HOTSEAT_SELECT_PLAYER : SCREEN_REPORTS;
                    }

                    if (!from_menu && MOX::stardate_at_beg_of_turn != 0) {
                        MISC::Fade_Into_Screen(MAINSCR::Draw_Mini_Main_Screen, &MOX::draw_main_screen_to_back, 0);
                    }
                    exit_loop = 1;
                    *exit_flag = 1;
                } else {
                    Bad_Game_File_Message(static_cast<uint16_t>(static_cast<uint8_t>(game_popup_fields->slot_status[selected_slot])), selected_slot);
                    exit_loop = 2;
                }
            }

            if (game_popup_fields->btn_popup_close_id == input_id) {
                MOX::screen_data = 0;
                exit_loop = 1;
                if (from_menu) {
                    *exit_flag = 1;
                }
            } else if (game_popup_fields->screen_cancel_field_id == input_id) {
                load_success = 0;
                exit_loop = 1;
                *exit_flag = 1;
            }

            if (exit_loop == 0) {
                video::Set_Page_Off();
                fields::Invoke_Auto_Function();
                RUSS::Mox_Sync_Update();
                timer::Release_Time(1);
            }
        } while (exit_loop == 0);

        for (int i = 0; i < 10; ++i) {
            Remove_Embedded_Special_Codes(MOX::save_game_description[i].description);
        }

        return load_success;
    }

    void __cdecl Do_Options_Game_Popup(int16_t* exit_code) {
        uint8_t initial_language = MOX::settings.language;
        bool exit_loop = false;
        int16_t dummy_x;
        int16_t dummy_y;

        EVANHELP::Set_Options_Game_Screen_Help_List();
        fields::Clear_Fields();
        int16_t hidden_field = fields::Add_Hidden_Field(0, 0, 0x27F, 0x1DF, "\x1B", 0x29);
        MOX::global_var = (int8_t)hidden_field;

        fields::Clear_Fields();
        fields::Assign_Auto_Function(_Draw_Options_Game_Popup, 1);
        Add_Game_Popup_Fields(&dummy_x, &dummy_y);

        MOX::global_var = 0;

        while (true) {
            timer::Mark_Time();
            int16_t input = fields::Get_Input();

            for (int16_t i = 0; i < 13; ++i) {
                if (game_popup_fields->option_names[i] == input || game_popup_fields->option_toggles[i] == input) {
                    game_popup_fields->options[i] = (game_popup_fields->options[i] == 0) ? 1 : 0;
                    break;
                }
            }

            if (game_popup_fields->screen_cancel_field_id == input) {
                exit_loop = true;
                *exit_code = 1;
            } else if (game_popup_fields->btn_settings_accept_id == input) {
                exit_loop = true;
                MOX::screen_data = 0;
            }

            if (exit_loop) {
                if ((uint16_t)initial_language != game_popup_fields->language) {
                    MOX::settings.language = (uint8_t)game_popup_fields->language;
                    if (MOX::settings.language > 4) {
                        MOX::settings.language = 0;
                    }
                    LOADSAVE::Reload_Language_Strings();
                    HAROLD::Write_Language_Setting_File();
                }
                return;
            }

            video::Set_Page_Off();
            _Draw_Options_Game_Popup();
            RUSS::Mox_Sync_Update();
            timer::Release_Time(1);
        }
    }

    void __cdecl Do_Save_Game_Popup(int16_t* out_should_close) {
        char temp_buffer[64];
        int16_t popup_accept_field, popup_cancel_field;
        int16_t exit_popup = 0;
        bool saved_successfully = false;
        int16_t active_slot_idx = -1;

        strlcpy(empty_save_slot_message, HAROLD::H_Message(0x184), sizeof(empty_save_slot_message));

        Load_Month_Names();
        EVANHELP::Set_Save_Game_Screen_Help_List();
        fields::Clear_Fields();

        MOX::global_var = (int8_t)fields::Add_Hidden_Field(0, 0, 0x27f, 0x1df, "\x1B", 0x29);
        fields::Clear_Fields();
        fields::Assign_Auto_Function(_Draw_Load_Save_Game_Popup, 1);
        Add_Game_Popup_Fields(&popup_accept_field, &popup_cancel_field);
        MOX::global_var = 0;

        do {
            timer::Mark_Time();
            bool copy_saved_field_text = false;
            bool trigger_save = false;
            int16_t input_id = fields::Get_Input();

            if (input_id != 0) {
                for (int16_t i = 0; i < 10; ++i) {
                    if (input_id == game_popup_fields->saved_game_fields[i]) {
                        if (active_slot_idx == i) {
                            copy_saved_field_text = true;
                            trigger_save = true;
                        }
                        active_slot_idx = i;
                        break;
                    }

                    if (input_id == game_popup_fields->save_slot_fields[i]) {
                        if (active_slot_idx == i) {
                            trigger_save = true;
                            draw_down = 2;
                            break;
                        }
                        fields::input_field_active = 1;
                        fields::cursor_position = 0;
                        fields::cursor_on = 0;
                        fields::active_input_field_number = game_popup_fields->saved_game_fields[i];

                        strlcpy(fields::continuous_string, MOX::save_game_description[i].description, sizeof(fields::continuous_string));

                        fields::active_input_field_first = 1;
                        active_slot_idx = i;
                        break;
                    }
                }
            }

            if (input_id == game_popup_fields->btn_popup_save_id) {
                if (active_slot_idx < 0 || active_slot_idx >= 10) {
                    int8_t save_slot = MOX::settings.active_save_slot;
                    if (save_slot >= 0 && save_slot < 10) {
                        active_slot_idx = save_slot;
                        trigger_save = true;
                    }
                } else {
                    trigger_save = true;
                }
            }

            if (trigger_save && active_slot_idx >= 0 && active_slot_idx < 10) {
                MOX::settings.active_save_slot = static_cast<int8_t>(active_slot_idx);
                char* target_desc = MOX::save_game_description[active_slot_idx].description;

                if (target_desc[0] == '\0' || strcmp(target_desc, empty_save_slot_message) == 0) {
                    int16_t colonies = COLXPORT::N_Colonies(MOX::PLAYER_NUM);
                    s_player* plr = &MOX::player[MOX::PLAYER_NUM];
                    const char* colony_str = HAROLD::H_Message(colonies == 1 ? 0xbc : 0xbd);

                    snprintf(temp_buffer, sizeof(temp_buffer), "%.19s, %.14s, %d %.11s", plr->name, plr->race_name, (int)colonies, colony_str);
                    strlcpy(MOX::save_game_description[active_slot_idx].description, temp_buffer, sizeof(MOX::save_game_description[active_slot_idx].description));
                } else if (copy_saved_field_text) {
                    strlcpy(MOX::save_game_description[active_slot_idx].description, fields::continuous_string, sizeof(MOX::save_game_description[active_slot_idx].description));
                }

                FILEDEF::Save_Game(active_slot_idx);
                Save_Session_Related_Settings();
                saved_successfully = true;
                Get_Save_Game_Date_Strings(MOX::save_game_dates);
                Get_Star_Date_Strings(game_popup_fields->slot_occupied_flags, MOX::save_game_stardates);
                exit_popup = 1;
                *out_should_close = 1;
            }

            if (input_id == game_popup_fields->btn_popup_close_id) {
                MOX::screen_data = 0;
                exit_popup = 1;
            } else if (input_id < 0) {
                int32_t abs_input = abs((int32_t)input_id);
                if (game_popup_fields->saved_game_fields[0] <= abs_input &&
                    abs_input <= game_popup_fields->saved_game_fields[9]) {
                    MOX::screen_data = 0;
                    exit_popup = 1;
                }
            } else if (input_id == game_popup_fields->screen_cancel_field_id) {
                exit_popup = 1;
                *out_should_close = 1;
            }

            if (exit_popup == 0) {
                video::Set_Page_Off();
                _Draw_Load_Save_Game_Popup();
                RUSS::Mox_Sync_Update();
                timer::Release_Time(1);
            }
        } while (exit_popup == 0);

        for (int i = 0; i < 10; ++i) {
            Remove_Embedded_Special_Codes(MOX::save_game_description[i].description);
        }

        if (saved_successfully && MOX::settings.animations_on != '\0') {
            fields::Clear_Fields();
            Animate_Game_Popup_Pic_Height(game_anims->pictures[0xe], 1);
            game_anims->animating = 1;
        }
    }

    void __cdecl Check_For_Saved_Games(int8_t* slot_status) {
        char filename_buffer[20];
        char itoa_buffer[12];
        int16_t game_index = 0;

        do {
            snprintf(itoa_buffer, sizeof(itoa_buffer), "%i", game_index + 1);

            char* dest = filename_buffer;
            const char* src = "SAVE";
            while ((*dest++ = *src++) != '\0');
            dest--;

            src = itoa_buffer;
            while ((*dest++ = *src++) != '\0');
            dest--;

            src = ".GAM";
            while ((*dest++ = *src++) != '\0');

            slot_status[game_index] = 0;

            if (!dos::DIR(filename_buffer, filename_buffer, sizeof(filename_buffer))) {
                slot_status[game_index] = 1;
            } else {
                if (LOADSAVE::Check_Magic_Number(filename_buffer) < 1) {
                    slot_status[game_index] = 2;
                } else {
                    if (LOADSAVE::Get_Game_Type_From_Saved_Game(filename_buffer)) {
                        int8_t game_type = (int8_t)FILEDEF::Get_Game_Type_From_Saved_Game(game_index);
                        if (game_type == 3 || game_type == 2) {
                            slot_status[game_index] = 3;
                        }
                    } else {
                        slot_status[game_index] = 2;
                    }
                }
            }

            game_index++;
        } while (game_index < 10);
    }

    void __cdecl Get_Star_Date_Strings(int32_t* star_dates, char* output_buf) {
        for (int16_t i = 0; i < 10; ++i) {
            char local_str_buf[25];
            memset(local_str_buf, 0, sizeof(local_str_buf));

            int32_t star_date_value = star_dates[i];

            if (star_date_value > 0) {
                // Fetch the prefix (e.g., "Stardate " or "D ")
                const char* prefix = HAROLD::H_Message(202);
                strlcpy(local_str_buf, prefix, sizeof(local_str_buf));

                char itoa_buf[12];
                snprintf(itoa_buf, sizeof(itoa_buf), "%i", star_date_value);

                size_t current_len = strlen(local_str_buf);
                size_t itoa_len = strlen(itoa_buf);

                // Append itoa result while inserting a decimal point before the last digit
                for (size_t j = 0; j < itoa_len; ++j) {
                    if ((itoa_len - j) == 1) {
                        local_str_buf[current_len++] = '.';
                    }
                    local_str_buf[current_len++] = itoa_buf[j];
                }
                local_str_buf[current_len] = '\0';

            } else if (star_date_value < 0) {
                // If the value is negative, mark it as invalid
                memcpy(local_str_buf, "* INVALID *", sizeof("* INVALID *"));
            }

            // Copy the resulting string (25 bytes) to the correct slot in the output buffer
            memcpy(output_buf + (i * 25), local_str_buf, sizeof(local_str_buf));
        }
    }

    void __cdecl Get_Save_Game_Date_Strings(char* output_buffer) {
        char slot_num_str[12];
        char filename[20];
        char date_str[28];

        for (int16_t i = 0; i < 10; ++i) {
            snprintf(slot_num_str, sizeof(slot_num_str), "%i", i + 1);

            char* dest = filename;
            const char* src = "SAVE";
            while ((*dest++ = *src++) != '\0');
            dest--;

            src = slot_num_str;
            while ((*dest++ = *src++) != '\0');
            dest--;

            src = ".GAM";
            while ((*dest++ = *src++) != '\0');

            for (int j = 0; j < 25; ++j) {
                date_str[j] = 0;
            }

            if (File_Exists(filename)) {
                Get_File_Date_String(filename, date_str);
            }

            char* out_dest = &output_buffer[i * 25];
            for (int j = 0; j < 25; ++j) {
                out_dest[j] = date_str[j];
            }
        }
    }

    int32_t __cdecl Check_Magic_Number(const char* filename) {
        int32_t magic_number = 0;
        FILE* file_ptr = fopen_case(filename, "rb");

        if (file_ptr != nullptr) {
            if (fseek(file_ptr, 0x29, SEEK_SET) == 0) {
                if (fread(&magic_number, sizeof(magic_number), 1, file_ptr) != 1) {
                    magic_number = 0;
                }
            }
            fclose(file_ptr);
        }

        return magic_number;
    }

    void __cdecl _Draw_Main_Game_Popup_To_Bitmap(s_animation_header* bitmap_ptr) {
        int16_t saved_x = MOX::popup_base_x;
        int16_t saved_y = MOX::popup_base_y;

        bitmap = bitmap_ptr;
        print_data_to_bitmap = 1;
        MOX::popup_base_x = 0;
        MOX::popup_base_y = 0;

        _Draw_Main_Game_Popup();

        print_data_to_bitmap = 0;
        MOX::popup_base_x = saved_x;
        MOX::popup_base_y = saved_y;
    }

    void __cdecl _Draw_Load_Save_Game_Popup_To_Bitmap(s_animation_header* bitmap) {
        print_data_to_bitmap = 1;
        LOADSAVE::bitmap = bitmap;
        _Draw_Load_Save_Game_Popup();
        print_data_to_bitmap = 0;
    }

    void __cdecl _Draw_Options_Game_Popup_To_Bitmap(s_animation_header* bitmap) {
        print_data_to_bitmap = 1;
        LOADSAVE::bitmap = bitmap;
        _Draw_Options_Game_Popup();
        print_data_to_bitmap = 0;
    }

    bool __cdecl Get_Game_Type_From_Saved_Game(const char* filename) {
        bool is_valid = false;
        FILE* file = fopen_case(filename, "rb");

        if (file != nullptr) {
            uint32_t game_type_header;
            if (fread(&game_type_header, sizeof(game_type_header), 1, file) == 1 &&
                Is_Valid_Game_Type(game_type_header)) {
                is_valid = true;
            }

            fclose(file);
        }

        return is_valid;
    }

    void __cdecl _Draw_Load_Save_Game_Popup() {
        s_animation_header* popup_frame;
        if (MOX::screen_data == 2) {
            popup_frame = game_anims->pictures[0xB];
        } else {
            popup_frame = game_anims->pictures[0xE];
        }

        if (MOX::current_screen == SCREEN_MAIN_MENU) {
            animate::Draw(0, 0, (s_animation_header*)MAINMENU::mainmenu_first_seg);
            animate::Draw(MOX::popup_base_x, MOX::popup_base_y, popup_frame);
            MOX::FULL_DRAW_SCREEN_FLAG = 0;
        } else if (print_data_to_bitmap == 0) {
            if (MOX::FULL_DRAW_SCREEN_FLAG != 0) {
                Restore_Main_Screen();
                MOX::FULL_DRAW_SCREEN_FLAG = 0;
                animate::Draw(MOX::popup_base_x, MOX::popup_base_y, popup_frame);
            } else if (print_data_to_bitmap == 0) {
                animate::Draw(MOX::popup_base_x, MOX::popup_base_y, popup_frame);
            }
        }

        for (int i = 0; i < 10; ++i) {
            char stardate_buf[25];
            char date_buf[25];

            memcpy(stardate_buf, MOX::save_game_stardates + (i * 25), 25);

            bool is_valid_slot = (game_popup_fields->slot_status[i] != 2);
            int16_t x_icon, y_icon;
            if (print_data_to_bitmap == 0) {
                x_icon = MOX::popup_base_x + 0xE4;
                y_icon = MOX::popup_base_y + 0x24 + (i * 31);
            } else {
                x_icon = 0xE4;
                y_icon = (i * 31) + 0x24;
            }

            s_animation_header* game_type_icon = 0;
            switch (game_popup_fields->saved_game_types[i]) {
                case 0: game_type_icon = game_anims->pictures[0x10]; break;
                case 1: game_type_icon = game_anims->pictures[0x11]; break;
                case 2:
                case 3: game_type_icon = game_anims->pictures[0x12]; break;
            }

            if (game_type_icon) {
                if (print_data_to_bitmap == 0) {
                    animate::Draw(x_icon, y_icon, game_type_icon);
                } else {
                    bitmap::Add_Picture_To_Bitmap(x_icon, y_icon, game_type_icon, bitmap);
                }
            }

            int16_t text_x, text_y;
            if (print_data_to_bitmap == 0) {
                text_x = MOX::popup_base_x + 0x20;
                text_y = MOX::popup_base_y + 0x18 + (i * 31);
                if (MOX::screen_data == 2) {
                    Get_Game_Popup_Colors(2, &game_popup_fields->color_normal, &game_popup_fields->color_active);
                    s_colors* color = (MOX::settings.active_save_slot == (char)i) ? &game_popup_fields->color_active : &game_popup_fields->color_normal;
                    fonts::Set_Font_Style(2, color);
                    const char* desc = is_valid_slot ? MOX::save_game_description[i].description : "* INVALID *";
                    fonts::Print(text_x, text_y, desc);
                }
            } else {
                text_x = 0x20;
                text_y = (i * 31) + 0x18;
                Get_Game_Popup_Colors(2, &game_popup_fields->color_normal, &game_popup_fields->color_active);
                s_colors* color = (MOX::settings.active_save_slot == (char)i) ? &game_popup_fields->color_active : &game_popup_fields->color_normal;
                fonts::Set_Font_Style(2, color);

                int16_t print_x = (MOX::screen_data == 3) ? 0x24 : 0x20;
                if (is_valid_slot) {
                    if (MOX::save_game_description[i].description[0] != '\0') {
                        fonts::Print_To_Bitmap(print_x, text_y, MOX::save_game_description[i].description, bitmap);
                    }
                } else if (MOX::save_game_description[i].description[0] != '\0') {
                    fonts::Print_To_Bitmap(print_x, text_y, "* INVALID *", bitmap);
                }
            }

            // Print stardate and real-world date
            Get_Game_Popup_Colors(1, &game_popup_fields->color_normal, &game_popup_fields->color_active);
            s_colors* color_1 = (MOX::settings.active_save_slot == (char)i) ? &game_popup_fields->color_active : &game_popup_fields->color_normal;
            fonts::Set_Font_Style(1, color_1);

            if (MOX::screen_data == 3) text_x += 2;

            if (print_data_to_bitmap == 0) {
                if (strcmp(empty_save_slot_message, MOX::save_game_description[i].description) != 0) {
                    fonts::Print(text_x, text_y + 14, stardate_buf);
                }
            } else {
                fonts::Print_To_Bitmap(text_x, text_y + 14, stardate_buf, bitmap);
            }

            memcpy(date_buf, MOX::save_game_dates + (i * 25), 25);

            if (print_data_to_bitmap == 0) {
                fonts::Print(text_x + 100, text_y + 14, date_buf);
            } else {
                fonts::Print_To_Bitmap(text_x + 100, text_y + 14, date_buf, bitmap);
            }
        }
    }

    void __cdecl Get_Game_Popup_Colors(int16_t font_idx, s_colors* out_colors_primary, s_colors* out_colors_secondary) {
        s_colors* colors;

        if (MOX::current_screen == SCREEN_MAIN_MENU) {
            // Colors for main menu context
            colors = MISC::Font_Colors2(font_idx, 0xFE, 0xFF);
            *out_colors_primary = *colors;

            colors = MISC::Font_Colors2(font_idx, 0xFB, 0xFC);
            *out_colors_secondary = *colors;
        } else {
            // Colors for general game context
            MOX::mox_font_colors_offset = 0x59;                // 'Y'
            colors = MISC::Font_Colors2(font_idx, 0x56, 0x5B); // 'V', '['
            *out_colors_primary = *colors;

            MOX::mox_font_colors_offset = 0x5B;                // '['
            colors = MISC::Font_Colors2(font_idx, 0x59, 0x7E); // 'Y', '~'
            *out_colors_secondary = *colors;
        }
    }

    void __cdecl Bad_Game_File_Message(uint8_t type, int16_t file_index) {
        char buffer[256];
        buffer[0] = '\0';
        int16_t message_id = 0;

        if (type == 1) {
            message_id = 0xB2; // 178
        } else if (type == 2) {
            message_id = 0xB3; // 179
        } else if (type == 3) {
            message_id = 0xB4; // 180
        }

        if (message_id != 0) {
            const char* format = HAROLD::H_Message(message_id);
            snprintf(buffer, sizeof(buffer), format, (int)(int16_t)(file_index + 1));
        }

        Game_Popup_User_Box(buffer, 3);
    }

    bool __cdecl Is_Valid_Game_Type(uint32_t game_type) {
        return game_type == 0xe0 || game_type == 0xe1;
    }

    void __cdecl Animate_Game_Popup_Pic_Height(s_animation_header* animation, uint16_t type) {
        int16_t height = animate::Get_Height(animation);
        LOADSAVE::Animate_Game_Popup(animation, height, -1, type);
    }

    void __cdecl Animate_Game_Popup(s_animation_header* header, int16_t y_offset, int16_t direction, uint16_t type) {
        uint16_t w = animate::Get_Width(header);
        uint16_t h = animate::Get_Height(header);

        allocate::Push_Block(MOX::screen_seg);
        int32_t bitmap_size = bitmap::Get_Bitmap_Size(w, h);
        s_animation_header* temp_bitmap = (s_animation_header*)allocate::Allocate_Next_Block(MOX::screen_seg, bitmap_size + 100);

        bitmap::Create_Blank_Bitmap(w, h, temp_bitmap);
        bitmap::Draw_Picture_To_Bitmap((s_animation_header*)temp_bitmap, header);

        switch (type) {
            case 0:
                LOADSAVE::_Draw_Main_Game_Popup_To_Bitmap(temp_bitmap);
                break;
            case 1:
            case 2:
                LOADSAVE::_Draw_Load_Save_Game_Popup_To_Bitmap(temp_bitmap);
                break;
            case 3:
                LOADSAVE::_Draw_Options_Game_Popup_To_Bitmap(temp_bitmap);
                break;
            default:
                break;
        }

        MAINSCR::Draw_Main_Main_Screen();
        video::Copy_Off_To_Back();
        video::Set_Refresh_Stencil();

        int16_t frame_index = 1;
        bool finished = false;

        do {
            // Calculate the step progress based on percentage (frame_index / 100)
            y_offset += (int16_t)((frame_index * (int32_t)h) / 100) * direction;

            // Clamp to height if sliding in
            if (direction > 0 && y_offset > h) {
                y_offset = h;
            }

            int16_t window_bottom = y_offset + 0x1A; // 26 pixels offset
            video::Set_Page_Off();
            graphics::Set_Window(0x90, 0x19, w + 0x90, window_bottom);
            video::Clip_On();

            int16_t draw_y;
            if (frame_index == 14 && direction > 0) {
                draw_y = 0x19; // Draw at fixed top (25)
            } else {
                draw_y = (y_offset - h) + 0x15; // Vertical slide calculation
            }

            animate::Draw(0x90, draw_y, (s_animation_header*)temp_bitmap);
            video::Clip_Off();

            if (direction < 1) {
                // Sliding out condition: window has moved up past boundary
                finished = (window_bottom - 0x19) <= 0x19;
            } else {
                // Sliding in condition: full height reached
                finished = (window_bottom - 0x19) > h;
            }

            RUSS::Mox_Sync_Update();
            frame_index++;
        } while (!finished);

        video::Copy_Off_To_Back();
        allocate::Pop_Block(MOX::screen_seg);
    }

    void __cdecl Save_Session_Related_Settings() {
        uint8_t file_buffer[sizeof(MOX::settings)];

        FILE* file = fopen_case(GAME_SETTINGS_FILENAME, "rb");
        if (file != nullptr) {
            const bool loaded = fread(file_buffer, sizeof(file_buffer), 1, file) == 1;
            fclose(file);

            if (loaded) {
                RUSS::Assert_Settings();

                // Update the session-related portion of the settings (211 bytes)
                for (int i = 0; i < 211; ++i) {
                    file_buffer[i] = ((uint8_t*)&MOX::settings)[i];
                }

                RUSS::Assert_Settings();

                file = fopen_case(GAME_SETTINGS_FILENAME, "wb");
                if (file != nullptr) {
                    if (fwrite(file_buffer, sizeof(file_buffer), 1, file) == 1) {
                        fclose(file);
                        return;
                    }
                    fclose(file);
                }
            }
        }

        // The settings file is missing, short, or could not be rewritten
        // (e.g. deleted mid-session, or left short by a crashed earlier
        // write). Recreate it from the live in-memory settings instead of
        // killing the game: they are the authoritative copy mid-session,
        // and resetting them to defaults here would discard the player's
        // current choices. If even that fails, skip the write with a
        // warning; the game keeps running and retries on the next save.
        if (!FILEDEF::Save_Game_Settings()) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "loadsave: unable to save session settings to %s",
                        GAME_SETTINGS_FILENAME);
        }
    }

    void __cdecl Draw_Popup_Pic1() {
        animate::Draw(
            (int16_t)MOX::popup_base_x + 147,
            (int16_t)MOX::popup_base_y + 43,
            game_anims->pictures[23]);
    }

    void __cdecl Draw_Popup_Pic2() {
        animate::Draw(
            (int16_t)MOX::popup_base_x + 40,
            (int16_t)MOX::popup_base_y + 88,
            game_anims->pictures[24]);
    }

    void __cdecl Animate_Game_Popup_Pic(s_animation_header* animation_header) {
        LOADSAVE::Animate_Game_Popup(animation_header, 0, 1, 0);
    }

    void __cdecl Update_Game_Settings() {
        s_game_popup_fields* fields = LOADSAVE::game_popup_fields;

        MOX::settings.end_of_turn_summary = (uint8_t)fields->options[0];
        MOX::settings.end_of_turn_wait = (uint8_t)fields->options[1];
        MOX::settings.enemy_moves = (uint8_t)fields->options[2];
        MOX::settings.expanding_help = (uint8_t)fields->options[3];
        MOX::settings.auto_select_ships = (uint8_t)fields->options[4];
        MOX::settings.animations_on = (uint8_t)fields->options[5];
        MOX::settings.auto_select_colony = (uint8_t)fields->options[6];
        MOX::settings.show_relocation_lines = (uint8_t)fields->options[7];
        MOX::settings.show_gnn_report = (uint8_t)fields->options[8];
        MOX::settings.auto_delete_tg_housing = (uint8_t)fields->options[9];
        MOX::settings.auto_saves = (uint8_t)fields->options[10];
        MOX::settings.only_show_serious_turn_summary_reports = (uint8_t)fields->options[11];
        MOX::settings.ship_initiative = (uint8_t)fields->options[12];

        MOX::settings.language = (uint8_t)fields->language;

        if (static_cast<uint8_t>(MOX::settings.language) >= 5) {
            MOX::settings.language = 0;
        }
    }

    void __cdecl Set_Current_Game_Option_Flags() {
        // Scaling logic for levels: ((level * 100) / 100) * 155) / 100
        // Simplifies to (level * 155) / 100
        game_popup_fields->music_level = (int16_t)(((int32_t)MOX::settings.music_level * 155) / 100);
        game_popup_fields->sound_fx_level = (int16_t)(((int32_t)MOX::settings.sound_fx_level * 155) / 100);

        game_popup_fields->music_on = MOX::settings.music_on;
        game_popup_fields->sound_fx_on = MOX::settings.sound_fx_on;

        game_popup_fields->options[0] = MOX::settings.end_of_turn_summary;

        if (MOX::game_type == 0 ||
            keep_end_of_turn_wait_in_multiplayer != 0) {
            game_popup_fields->options[1] = MOX::settings.end_of_turn_wait;
        } else {
            game_popup_fields->options[1] = 0;
        }

        game_popup_fields->options[2] = MOX::settings.enemy_moves;
        game_popup_fields->options[3] = MOX::settings.expanding_help;
        game_popup_fields->options[4] = MOX::settings.auto_select_ships;
        game_popup_fields->options[5] = MOX::settings.animations_on;
        game_popup_fields->options[6] = MOX::settings.auto_select_colony;
        game_popup_fields->options[7] = MOX::settings.show_relocation_lines;
        game_popup_fields->options[8] = MOX::settings.show_gnn_report;
        game_popup_fields->options[9] = MOX::settings.auto_delete_tg_housing;
        game_popup_fields->options[10] = MOX::settings.auto_saves;
        game_popup_fields->options[11] = MOX::settings.only_show_serious_turn_summary_reports;
        game_popup_fields->options[12] = MOX::settings.ship_initiative;

        game_popup_fields->language = MOX::settings.language;
        if (game_popup_fields->language >= 5) {
            game_popup_fields->language = 0;
        }
    }

    void __cdecl Load_Game_Popup_Pictures() {
        RUSS::Mox_Update();

        game_anims->pictures[0] = (s_animation_header*)farload::Far_Reload_Next("GAME.LBX", 0, MOX::screen_seg);
        game_anims->pictures[1] = (s_animation_header*)farload::Far_Reload_Next("GAME.LBX", 1, MOX::screen_seg);
        game_anims->pictures[2] = (s_animation_header*)farload::Far_Reload_Next("GAME.LBX", 2, MOX::screen_seg);
        game_anims->pictures[3] = (s_animation_header*)farload::Far_Reload_Next("GAME.LBX", 3, MOX::screen_seg);
        game_anims->pictures[4] = (s_animation_header*)farload::Far_Reload_Next("GAME.LBX", 4, MOX::screen_seg);
        game_anims->pictures[5] = (s_animation_header*)farload::Far_Reload_Next("GAME.LBX", 5, MOX::screen_seg);

        RUSS::Mox_Update();

        game_anims->pictures[6] = (s_animation_header*)farload::Far_Reload_Next("GAME.LBX", 6, MOX::screen_seg);
        game_anims->pictures[7] = (s_animation_header*)farload::Far_Reload_Next("GAME.LBX", 7, MOX::screen_seg);
        game_anims->pictures[8] = (s_animation_header*)farload::Far_Reload_Next("GAME.LBX", 29, MOX::screen_seg);
        game_anims->pictures[9] = (s_animation_header*)farload::Far_Reload_Next("GAME.LBX", 9, MOX::screen_seg);
        game_anims->pictures[10] = (s_animation_header*)farload::Far_Reload_Next("GAME.LBX", 10, MOX::screen_seg);

        RUSS::Mox_Update();

        game_anims->pictures[11] = (s_animation_header*)farload::Far_Reload_Next("GAME.LBX", 11, MOX::screen_seg);
        game_anims->pictures[12] = (s_animation_header*)farload::Far_Reload_Next("GAME.LBX", 12, MOX::screen_seg);
        game_anims->pictures[13] = (s_animation_header*)farload::Far_Reload_Next("GAME.LBX", 13, MOX::screen_seg);
        game_anims->pictures[14] = (s_animation_header*)farload::Far_Reload_Next("GAME.LBX", 14, MOX::screen_seg);
        game_anims->pictures[15] = (s_animation_header*)farload::Far_Reload_Next("GAME.LBX", 15, MOX::screen_seg);
        game_anims->pictures[16] = (s_animation_header*)farload::Far_Reload_Next("GAME.LBX", 16, MOX::screen_seg);

        RUSS::Mox_Update();

        game_anims->pictures[17] = (s_animation_header*)farload::Far_Reload_Next("GAME.LBX", 17, MOX::screen_seg);
        game_anims->pictures[18] = (s_animation_header*)farload::Far_Reload_Next("GAME.LBX", 18, MOX::screen_seg);
        game_anims->pictures[19] = (s_animation_header*)farload::Far_Reload_Next("GAME.LBX", 19, MOX::screen_seg);
        game_anims->pictures[23] = (s_animation_header*)farload::Far_Reload_Next("GAME.LBX", 27, MOX::screen_seg);
        game_anims->pictures[24] = (s_animation_header*)farload::Far_Reload_Next("GAME.LBX", 28, MOX::screen_seg);

        RUSS::Mox_Update();

        s_animation_header* first_pic = game_anims->pictures[0];
        int16_t h = animate::Get_Height(first_pic) + 1;
        int16_t w = animate::Get_Width(first_pic) + 1;
        uint32_t size = bitmap::Get_Bitmap_Size(w, h);

        MAINSCR::temp_bitmap = (s_animation_header*)allocate::Allocate_Next_Block(MOX::screen_seg, size);
    }

    void __cdecl Do_Main_Game_Popup(int16_t* out_finished) {
        bool closed_with_exit = false;
        bool event_processed = false;
        char text_buffer[200];

        fields::Assign_Auto_Function(LOADSAVE::_Draw_Main_Game_Popup, 1);
        EVANHELP::Set_Main_Game_Screen_Help_List();

        while (true) {
            timer::Mark_Time();
            int16_t input = fields::Get_Input();

            if (input == game_popup_fields->btn_main_close_id) {
                draw_game_popup_to_back = 1;
                event_processed = true;
                if (MOX::screen_data == 0) {
                    *out_finished = 1;
                }
            } else if (input == game_popup_fields->btn_main_new_id) {
                const char* msg = HAROLD::H_Message(0xBA); // "QUIT TO MAIN MENU?"
                strlcpy(text_buffer, msg, sizeof(text_buffer));

                if (Game_Popup_User_Box(text_buffer, 1)) {
                    MOX::game_type = 0;
                    MOX::return_screen = SCREEN_NEW_GAME;
                    event_processed = true;
                    *out_finished = 1;
                } else {
                    event_processed = true;
                    video::Set_Page_Back();
                    graphics::Fill(0, 0, 639, 479, 0);
                    MAINSCR::Draw_Mini_Main_Screen();
                    MAINSCR::Draw_Main_Main_Screen();
                    video::Copy_Back_To_Off();
                }
            } else if (input == game_popup_fields->btn_main_quit_id) {
                const char* msg = HAROLD::H_Message(0xBB); // "QUIT TO WINDOWS?"
                strlcpy(text_buffer, msg, sizeof(text_buffer));

                if (Game_Popup_User_Box(text_buffer, 1)) {
                    FILEDEF::Save_Game(9);
                    MOX::settings.active_save_slot = 9;
                    Save_Session_Related_Settings();

                    if (MOX::game_type == 2 || MOX::game_type == 3) {
                        NETMOX::Human_Abdicates(static_cast<int32_t>(MOX::PLAYER_NUM));
                        netcode::Net_Uninit();
                    }

                    MOX::return_screen = SCREEN_EXIT;
                    event_processed = true;
                    *out_finished = 1;
                } else {
                    event_processed = true;
                    video::Set_Page_Back();
                    graphics::Fill(0, 0, 639, 479, 0);
                    MAINSCR::Draw_Mini_Main_Screen();
                    MAINSCR::Draw_Main_Main_Screen();
                    video::Copy_Back_To_Off();
                }
            } else if (input == game_popup_fields->btn_main_options_id) {
                draw_game_popup_to_back = 1;
                MOX::screen_data = 1;
                event_processed = true;
            } else if (input == game_popup_fields->btn_main_load_id) {
                event_processed = true;
                fields::Assign_Auto_Function(LOADSAVE::_Draw_Load_Save_Game_Popup, 1);
                draw_game_popup_to_back = 1;
                MOX::screen_data = 2;
            } else if (input == game_popup_fields->btn_main_save_id) {
                if (MOX::disallow_saves_code == 0) {
                    draw_game_popup_to_back = 1;
                    MOX::screen_data = 3;
                    event_processed = true;
                } else {
                    const char* msg = HAROLD::H_Message(0xB9); // "CAN'T SAVE NOW"
                    strlcpy(text_buffer, msg, sizeof(text_buffer));
                    event_processed = true;
                    Game_Popup_User_Box(text_buffer, 0);
                    *out_finished = 1;
                }
            } else if (input == music_level_field) {
                Set_Music_For_Game_Popup();
            } else if (input == sfx_level_field) {
                Set_Sound_For_Game_Popup(1);
            }

            if (input == game_popup_fields->screen_cancel_field_id) {
                closed_with_exit = true;
                event_processed = true;
                *out_finished = 1;
            }

            if (event_processed) {
                if (closed_with_exit && MOX::settings.animations_on != 0) {
                    fields::Clear_Fields();
                    Animate_Game_Popup_Pic_Height(game_anims->pictures[0], 0);
                    game_anims->animating = 1;
                }
                return;
            }

            video::Set_Page_Off();
            LOADSAVE::_Draw_Main_Game_Popup();
            RUSS::Mox_Sync_Update();
            timer::Release_Time(1);
        }
    }

    void __cdecl _Draw_Main_Game_Popup() {
        int16_t input_result = fields::Auto_Input();

        int16_t x1 = MOX::popup_base_x + 62;      // 0x3e
        int16_t y1 = MOX::popup_base_y + 194;     // 0xc2
        int16_t y1_end = MOX::popup_base_y + 207; // 0xcf
        int16_t x1_end = game_popup_fields->music_level + x1;

        int16_t x2 = x1;
        int16_t y2 = MOX::popup_base_y + 216;     // 0xd8
        int16_t y2_end = MOX::popup_base_y + 229; // 0xe5
        int16_t x2_end = game_popup_fields->sound_fx_level + x1;

        // Junk/Optimization artifact loop detected in assembly
        volatile char stack_buf[8];
        for (int i = 1; i < 8; ++i) {
            stack_buf[i] = static_cast<char>(i / 2) - 103;
        }

        if (print_data_to_bitmap == 0) {
            MAINSCR::Draw_Mini_Main_Screen();
            animate::Draw(MOX::popup_base_x, MOX::popup_base_y, game_anims->pictures[0]);

            if (dont_draw_darkened_buttons == 0) {
                Draw_Popup_Pic1();
                Draw_Popup_Pic2();
                dont_draw_darkened_buttons = 0;
            }
            MOX::FULL_DRAW_SCREEN_FLAG = 0;
        }

        if (MOX::game_type != 0) {
            if (print_data_to_bitmap != 0) {
                // Draw picture 0x17 (23)
                s_animation_header* pic23 = game_anims->pictures[23];
                int16_t h23 = animate::Get_Height(pic23);
                int16_t w23 = animate::Get_Width(pic23);
                bitmap::Create_Blank_Bitmap(w23, h23, MAINSCR::temp_bitmap);
                bitmap::Add_Picture_To_Bitmap(0, 0, pic23, (s_animation_header*)MAINSCR::temp_bitmap);
                bitmap::Add_Picture_To_Bitmap(147, 43, (s_animation_header*)MAINSCR::temp_bitmap, bitmap); // 0x93, 0x2b

                // Draw picture 0x18 (24)
                s_animation_header* pic24 = game_anims->pictures[24];
                int16_t h24 = animate::Get_Height(pic24);
                int16_t w24 = animate::Get_Width(pic24);
                bitmap::Create_Blank_Bitmap(w24, h24, MAINSCR::temp_bitmap);
                bitmap::Add_Picture_To_Bitmap(0, 0, pic24, (s_animation_header*)MAINSCR::temp_bitmap);
                bitmap::Add_Picture_To_Bitmap(40, 88, (s_animation_header*)MAINSCR::temp_bitmap, bitmap); // 0x28, 0x58
            }
        }

        // Draw picture 7 in Window 1
        if (print_data_to_bitmap != 0) {
            bitmap::Create_Blank_Bitmap(x1_end - x1 + 1, y1_end - y1 + 1, MAINSCR::temp_bitmap);
            bitmap::Add_Picture_To_Bitmap(0, 0, game_anims->pictures[7], (s_animation_header*)MAINSCR::temp_bitmap);
            bitmap::Add_Picture_To_Bitmap(x1, y1, (s_animation_header*)MAINSCR::temp_bitmap, bitmap);
        } else {
            graphics::Set_Window(x1, y1, x1_end, y1_end);
            video::Clip_On();
            animate::Draw(x1, y1, game_anims->pictures[7]);
            graphics::Reset_Window();
            video::Clip_Off();
        }

        // Draw picture 7 in Window 2
        if (print_data_to_bitmap != 0) {
            bitmap::Create_Blank_Bitmap(x2_end - x1 + 1, y2_end - y2 + 1, MAINSCR::temp_bitmap);
            bitmap::Add_Picture_To_Bitmap(0, 0, game_anims->pictures[7], (s_animation_header*)MAINSCR::temp_bitmap);
            bitmap::Add_Picture_To_Bitmap(x1, y2, (s_animation_header*)MAINSCR::temp_bitmap, bitmap);
        } else {
            graphics::Set_Window(x1, y2, x2_end, y2_end);
            video::Clip_On();
            animate::Draw(x1, y2, game_anims->pictures[7]);
            graphics::Reset_Window();
            video::Clip_Off();
        }

        if (music_level_field == input_result) {
            Set_Music_For_Game_Popup();
            return;
        }

        if (sfx_level_field == input_result) {
            Set_Sound_For_Game_Popup(0);
        }
    }

    void __cdecl _Draw_Options_Game_Popup() {
        s_colors outline_colors;
        MOX::mox_font_colors_offset = 0x55;                    // 'U'
        s_colors* palette = MISC::Font_Colors2(2, 0x55, 0x7E); // 'U', '~'

        // Copy the palette colors locally
        memcpy(outline_colors.colors, palette->colors, sizeof(outline_colors.colors));

        if (print_data_to_bitmap == 0) {
            if (MOX::FULL_DRAW_SCREEN_FLAG != 0) {
                Restore_Main_Screen();
            }
            if (print_data_to_bitmap == 0) {
                animate::Draw(MOX::popup_base_x, MOX::popup_base_y, game_anims->pictures[8]);
            }
        }

        fonts::Set_Font_Style_Outline(2, &outline_colors);
        fonts::Set_Outline_Color(0, static_cast<uint16_t>(outline_colors.colors[0]));
        fonts::Set_Font_Spacing(1);

        for (int i = 0; i < 13; ++i) {
            int16_t x = MOX::popup_base_x + 0x1A;
            int16_t y = MOX::popup_base_y + 0x29 + (i * 0x11);

            animate::Set_Animation_Frame(game_anims->pictures[9], game_popup_fields->options[i]);

            if (print_data_to_bitmap == 0) {
                animate::Draw(x, y, game_anims->pictures[9]);
            } else {
                bitmap::Add_Picture_To_Bitmap(0, 0, game_anims->pictures[9], MAINSCR::temp_bitmap);
                bitmap::Add_Picture_To_Bitmap(x - MOX::popup_base_x, y - MOX::popup_base_y, MAINSCR::temp_bitmap, bitmap);
            }
        }

        int16_t draw_x, draw_y;
        if (print_data_to_bitmap == 0) {
            draw_x = MOX::popup_base_x + 0x1A;
            draw_y = MOX::popup_base_y + 0x29;
        } else {
            draw_x = 0x1A;
            draw_y = 0x29;
        }

        if (print_data_to_bitmap != 0) {
            int16_t btn_width = animate::Get_Width(game_anims->pictures[9]);
            int16_t text_x1 = draw_x + 4 + btn_width;
            int16_t text_x2 = text_x1 + 0x96;

            fonts::Print_To_Bitmap(text_x1, draw_y + 0x02, HAROLD::H_Message(0xA6), bitmap);
            fonts::Print_To_Bitmap(text_x2, draw_y + 0x02, "(ALT_F1)", bitmap);
            fonts::Print_To_Bitmap(text_x1, draw_y + 0x13, HAROLD::H_Message(0xA7), bitmap);
            fonts::Print_To_Bitmap(text_x2, draw_y + 0x13, "(ALT_F2)", bitmap);
            fonts::Print_To_Bitmap(text_x1, draw_y + 0x24, HAROLD::H_Message(0xA8), bitmap);
            fonts::Print_To_Bitmap(text_x2, draw_y + 0x24, "(ALT_F3)", bitmap);
            fonts::Print_To_Bitmap(text_x1, draw_y + 0x35, HAROLD::H_Message(0xA9), bitmap);
            fonts::Print_To_Bitmap(text_x1, draw_y + 0x46, HAROLD::H_Message(0xAA), bitmap);
            fonts::Print_To_Bitmap(text_x2, draw_y + 0x46, "(ALT_F5)", bitmap);
            fonts::Print_To_Bitmap(text_x1, draw_y + 0x57, HAROLD::H_Message(0xAB), bitmap);
            fonts::Print_To_Bitmap(text_x2, draw_y + 0x57, "(ALT_F6)", bitmap);
            fonts::Print_To_Bitmap(text_x1, draw_y + 0x68, HAROLD::H_Message(0xAC), bitmap);
            fonts::Print_To_Bitmap(text_x2, draw_y + 0x68, "(ALT_F7)", bitmap);
            fonts::Print_To_Bitmap(text_x1, draw_y + 0x79, HAROLD::H_Message(0xAD), bitmap);
            fonts::Print_To_Bitmap(text_x2, draw_y + 0x79, "(ALT_F8)", bitmap);
            fonts::Print_To_Bitmap(text_x1, draw_y + 0x8A, HAROLD::H_Message(0xAE), bitmap);
            fonts::Print_To_Bitmap(text_x1, draw_y + 0x9B, HAROLD::H_Message(0xAF), bitmap);
            fonts::Print_To_Bitmap(text_x1, draw_y + 0xAC, HAROLD::H_Message(0xB0), bitmap);
            fonts::Print_To_Bitmap(text_x1, draw_y + 0xBD, HAROLD::H_Message(0xB1), bitmap);
            fonts::Print_To_Bitmap(text_x1, draw_y + 0xCE, HAROLD::H_Message(0x187), bitmap);

            for (int i = 0; i < 13; ++i) {
                animate::Set_Animation_Frame(game_anims->pictures[9], game_popup_fields->options[i]);
                bitmap::Add_Picture_To_Bitmap(0x1A, draw_y + (i * 0x11), game_anims->pictures[9], bitmap);
            }
        } else {
            int16_t btn_width = animate::Get_Width(game_anims->pictures[9]);
            int16_t text_x1 = draw_x + 4 + btn_width;
            int16_t text_x2 = text_x1 + 0x96;

            fonts::Print(text_x1, draw_y + 0x02, HAROLD::H_Message(0xA6));
            fonts::Print(text_x2, draw_y + 0x02, "(ALT-F1)");
            fonts::Print(text_x1, draw_y + 0x13, HAROLD::H_Message(0xA7));
            fonts::Print(text_x2, draw_y + 0x13, "(ALT-F2)");
            fonts::Print(text_x1, draw_y + 0x24, HAROLD::H_Message(0xA8));
            fonts::Print(text_x2, draw_y + 0x24, "(ALT-F3)");
            fonts::Print(text_x1, draw_y + 0x35, HAROLD::H_Message(0xA9));
            fonts::Print(text_x1, draw_y + 0x46, HAROLD::H_Message(0xAA));
            fonts::Print(text_x2, draw_y + 0x46, "(ALT-F4)");
            fonts::Print(text_x1, draw_y + 0x57, HAROLD::H_Message(0xAB));
            fonts::Print(text_x2, draw_y + 0x57, "(ALT-F5)");
            fonts::Print(text_x1, draw_y + 0x68, HAROLD::H_Message(0xAC));
            fonts::Print(text_x2, draw_y + 0x68, "(ALT-F6)");
            fonts::Print(text_x1, draw_y + 0x79, HAROLD::H_Message(0xAD));
            fonts::Print(text_x2, draw_y + 0x79, "(ALT-F7)");
            fonts::Print(text_x1, draw_y + 0x8A, HAROLD::H_Message(0xAE));
            fonts::Print(text_x2, draw_y + 0x8A, "(ALT-F8)");
            fonts::Print(text_x1, draw_y + 0x9B, HAROLD::H_Message(0xAF));
            fonts::Print(text_x1, draw_y + 0xAC, HAROLD::H_Message(0xB0));
            fonts::Print(text_x1, draw_y + 0xBD, HAROLD::H_Message(0xB1));
            fonts::Print(text_x1, draw_y + 0xCE, HAROLD::H_Message(0x187));
        }
    }

    void __cdecl _Game_Popup() {
        int16_t exit_condition = 0;
        bool first_loop_drawn = true;
        int16_t dummy_p1;
        int16_t dummy_p2;
        uint8_t settings_copy[sizeof(MOX::settings)];

        MOX::first_time_drawn = 1;
        loaded_new_game = 0;
        fields::Clear_Fields();
        MOX::global_var = 0;

        memcpy(settings_copy, &MOX::settings, sizeof(settings_copy));

        if (MOX::screen_data < 0 || MOX::screen_data > 3) {
            MOX::screen_data = 0;
        }

        Allocate_Data_Space_For_Game_Popup();
        Restore_Main_Screen();
        Load_Game_Popup_Pictures();
        allocate::Get_Remaining_Block_Space(MOX::screen_seg);

        memset(game_popup_fields, 0xFF, sizeof(*game_popup_fields));

        Set_Current_Game_Option_Flags();

        do {
            fields::Deactivate_Auto_Function();
            draw_game_popup_to_back = 1;

            if (first_loop_drawn) {
                if (MOX::settings.animations_on == 0) {
                    first_loop_drawn = false;
                    Restore_Main_Screen();
                } else {
                    first_loop_drawn = false;
                    video::Set_Page_Off();
                    Animate_Game_Popup_Pic(LOADSAVE::game_anims->pictures[0]);
                }
            } else {
                Restore_Main_Screen();
            }

            if (MOX::screen_data == 2 || MOX::screen_data == 3) {
                sound::Sound_Update();
                Set_Up_Load_Save_Popup();
                MOX::return_screen = (int16_t)(int8_t)MOX::previous_screen;
            }

            fields::Clear_Fields();
            Add_Game_Popup_Fields(&dummy_p1, &dummy_p2);

            switch (MOX::screen_data) {
                case 0: Do_Main_Game_Popup(&exit_condition); break;
                case 1: Do_Options_Game_Popup(&exit_condition); break;
                case 2: Do_Load_Game_Popup(&exit_condition); break;
                case 3: Do_Save_Game_Popup(&exit_condition); break;
            }
        } while (exit_condition == 0);

        if (loaded_debug_save == 0) {
            Update_Game_Settings();
            Save_Session_Related_Settings();
        }

        MOX::current_screen = MOX::return_screen;
        fields::Deactivate_Auto_Function();
        fields::Clear_Fields();

        if (MOX::settings.animations_on != 0 && game_anims->animating == 0 && loaded_debug_save == 0) {
            dont_draw_darkened_buttons = 1;
            video::Set_Page_Off();
            _Draw_Main_Game_Popup();
            RUSS::Mox_Sync_Update();
            timer::Mark_Time();
            timer::Release_Time(7);
            video::Set_Page_Off();
            Animate_Game_Popup_Pic_Height(game_anims->pictures[0], 0);
        }

        if (MOX::current_screen == SCREEN_NEW_GAME) {
            ERIC::Fast_Fade_Out();
        } else if (loaded_new_game == 0) {
            fields::Clear_Fields();
            fields::Disable_Draw_All_Fields();
            MOX::draw_main_screen_to_back = 1;
            MAINSCR::Allocate_First_Screen_Seg(5);
            video::Set_Page_Off();
            MAINSCR::Draw_Mini_Main_Screen();
            RUSS::Mox_Sync_Update();
            fields::Enable_Draw_All_Fields();
        }
    }

    void __cdecl Auto_Save_Last_Saved_Game(int16_t* result_flag) {
        int32_t save_slots[10];
        char message_buffer[0xC8];

        FILEDEF::Get_Saved_Game_Descriptions(save_slots);

        int save_slot = (int8_t)MOX::settings.active_save_slot;
        s_save_game_description* save_description = &MOX::save_game_description[save_slot];
        const char* message_format = HAROLD::H_Message(0xCD);

        snprintf(message_buffer, sizeof(message_buffer), message_format, save_slot + 1, save_description->description);

        int16_t choice = HAROLD::User_Box(&MOX::draw_main_screen_to_back, message_buffer, 1);
        if (choice != 0) {
            FILEDEF::Load_Game((int8_t)MOX::settings.active_save_slot);
            MOX::return_screen = SCREEN_HOTSEAT_SELECT_PLAYER;
            if (MOX::game_type != 1) {
                MOX::return_screen = SCREEN_REPORTS;
            }
            *result_flag = 1;
            return;
        }

        MOX::FULL_DRAW_SCREEN_FLAG = 1;
        *result_flag = 0;
    }

    void __cdecl Embed_Special_Color_Codes(char* text, int16_t max_len) {
        int16_t length = static_cast<int16_t>(strlen(text));
        char encoded[202];

        memset(encoded, 0, sizeof(encoded));

        if (length <= 0xC8 && length < max_len - 2 && *text != '\x03') {
            encoded[0] = '\x03';
            memcpy(encoded + 1, text, length);
            encoded[length + 1] = '\x01';
            memcpy(text, encoded, length + 2);
        }
    }

    void __cdecl Remove_Embedded_Special_Codes(char* text) {
        if (*text == '\x03') {
            char special_code[2] = {'\x01', '\0'};
            int16_t copy_length = static_cast<int16_t>(strlen(text));
            memmove(text, text + 1, static_cast<size_t>(copy_length));
            int16_t special_code_pos = MISC::Substring(text, special_code);
            text[special_code_pos] = '\0';
        }
    }

    void __cdecl Set_Music_For_Game_Popup() {
        short music_volume = static_cast<short>((static_cast<int32_t>(game_popup_fields->music_level) * 100) / 0x9B);

        if (music_volume <= 5) {
            music_volume = 0;
            sound::Stop_Stream();
            JIM::Stop_Background_Music();
            MOX::settings.music_on = 0;
            game_popup_fields->music_on = 0;
        } else {
            game_popup_fields->music_on = 1;
            if (MOX::settings.music_on == 0) {
                sound::Stream_On();
                MOX::settings.music_on = 1;
                JIM::Play_Background_Music();
            }
        }

        MOX::settings.music_level = static_cast<int8_t>(music_volume);
        sound::Set_Stream_Max_Volume(music_volume);
        sound::Set_Stream_Volume(music_volume);
    }

    void __cdecl Set_Sound_For_Game_Popup(char play_sound_effect) {
        s_game_popup_fields* popup = LOADSAVE::game_popup_fields;

        MOX::global_var = 0;

        int32_t volume = ((int32_t)popup->sound_fx_level * 100) / 0x9b;
        MOX::settings.sound_fx_level = (int8_t)volume;

        if (MOX::settings.sound_fx_level > 5) {
            if (MOX::settings.sound_fx_on == 0) {
                sound::Sound_On();
            }

            MOX::settings.sound_fx_on = 1;
            sound::Set_Sound_Max_Volume(MOX::settings.sound_fx_level);

            if (play_sound_effect != 0) {
                ERICNET::Mox_Play_Sound(0x21);
            }

            popup->sound_fx_on = 1;
            return;
        }

        MOX::settings.sound_fx_level = 0;
        sound::Set_Sound_Max_Volume(0);
        MOX::settings.sound_fx_on = 0;
        popup->sound_fx_on = 0;
    }

    void __cdecl Get_File_Date_String(const char* file_path, char* out_string) {
        FILE* file = fopen_case(file_path, "r");
        if (file == nullptr) {
            return;
        }

        fclose(file);

        int16_t second;
        int16_t minute;
        int16_t hour;
        int16_t year;
        int16_t day;
        int16_t month;

        MISC::Get_Time_Stamp(file_path, &month, &day, &year, &hour, &minute, &second);

        if (month < 0 || month >= 12 || day < 1 || day > 31 || year < 0 || year > 9999 ||
            hour < 0 || hour > 23 || minute < 0 || minute > 59 ||
            ((((month == 0) && (day == 0)) && (year == 0)) && ((hour == 0) && (second == 0)))) {
            out_string[0] = '\0';
            return;
        }

        snprintf(
            out_string,
            25,
            "%.3s %2d, %-4d  %2d:%2d",
            LOADSAVE::months[month],
            static_cast<int>(day),
            static_cast<int>(year),
            static_cast<int>(hour),
            static_cast<int>(minute));

        if (out_string[0x0e] == ' ') {
            out_string[0x0e] = '0';
        }
        if (out_string[0x11] == ' ') {
            out_string[0x11] = '0';
        }
    }

    void __cdecl Reload_Language_Strings() {
        HAROLD::Load_H_Strings();
        RUSS::Load_Russ_Strings();
        ESTRINGS::Load_E_Strings();
        HAROLD::Load_Language_Fonts();
        HAROLD::Load_Leader_Titles();
        HAROLD::Set_Language_Race_Names();
        TECHINIT::Load_Tech_Names();
        INITSHIP::Update_Ship_Names_To_New_Language();
    }

    void __cdecl Auto_Load_Last_Saved_Game(int16_t* out_flag) {
        int32_t save_descriptions_buffer[10];
        char text_buffer[200];

        FILEDEF::Get_Saved_Game_Descriptions(save_descriptions_buffer);

        int32_t slot = MOX::settings.active_save_slot;
        const char* format_str = HAROLD::H_Message(0xcc);

        snprintf(text_buffer, sizeof(text_buffer), format_str, slot + 1, MOX::save_game_description[slot].description);

        int16_t box_res = HAROLD::User_Box(&MOX::draw_main_screen_to_back, text_buffer, 1);

        if (box_res != 0) {
            FILEDEF::Save_Game(static_cast<int16_t>(MOX::settings.active_save_slot));
            MOX::return_screen = MOX::current_screen;
            MAINSCR2::force_main_screen_fade = 1;
            *out_flag = 1;
        } else {
            MOX::FULL_DRAW_SCREEN_FLAG = 1;
            *out_flag = 0;
        }
    }
} // namespace LOADSAVE