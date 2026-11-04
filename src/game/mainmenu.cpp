#include "pch.h"
#include "string_overrides.h"

#include "config/mod_manager.h"

namespace MAINMENU {
    s_main_menu* mainmenu;
    s_animation_header* main_menu_background;
    s_animation_header* mainmenu_first_seg;
    s_animation_header* mainmenu_last_seg;
    void* _gametype;
    int16_t autoload_save_slot = -1;
    int16_t max_credits;
    int32_t credit_count;
    int16_t credits_font_style;
    uint8_t credit_num;
    uint8_t credits_font_spacing;
    s_animation_header* bitmap;
    s_credit_y_slot credit_y_slot;
    int32_t credit_x;
    int16_t delay_count;
    int16_t fade_in_count;
    int16_t fade_out_count;
    char (*credits)[2][127];
    uint8_t credits_column_one;
    int16_t credits_field = -1000;

    namespace {
        constexpr int16_t MOD_LIST_LEFT = 70;
        constexpr int16_t MOD_LIST_RIGHT = 366;
        constexpr int16_t MOD_LIST_TOP = 200;
        constexpr int16_t MOD_LIST_BOTTOM = 412;
        constexpr int16_t MOD_LIST_MIN_ROW_HEIGHT = 14;
        constexpr int16_t MAIN_MENU_TEXT_BOTTOM = 0x1BB - 1;
        constexpr int16_t CREDITS_FIELD_LEFT = 26;
        constexpr int16_t CREDITS_FIELD_TOP = 422;
        constexpr int16_t CREDITS_FIELD_RIGHT = 174;
        constexpr int16_t CREDITS_FIELD_BOTTOM = 443;
        bool mod_list_active;

        const char* Credits_Label() {
            switch (MOX::settings.language) {
                case 1:
                    return "Mitwirkende";
                case 2:
                    return "Credits";
                case 3:
                    return "Creditos";
                case 4:
                    return "Riconoscimenti";
                default:
                    return "Credits";
            }
        }

        const char* Loaded_Mods_Label() {
            switch (MOX::settings.language) {
                case 1:
                    return "Geladene Mods";
                case 2:
                    return "Mods charges";
                case 3:
                    return "Mods cargados";
                case 4:
                    return "Mod caricate";
                default:
                    return "Loaded Mods";
            }
        }

        const char* No_Mods_Label() {
            switch (MOX::settings.language) {
                case 1:
                    return "Keine Mods geladen";
                case 2:
                    return "Aucun mod charge";
                case 3:
                    return "No hay mods cargados";
                case 4:
                    return "Nessuna mod caricata";
                default:
                    return "No mods loaded";
            }
        }

        std::string Fit_Mod_Name(const std::string& name, int16_t width) {
            if (width <= 0) {
                return {};
            }
            if (fonts::Get_String_Width("...") > width) {
                return {};
            }
            constexpr size_t MAX_MEASURED_TEXT_LENGTH = 255;
            std::string fitted = name.substr(0, MAX_MEASURED_TEXT_LENGTH);
            if (fitted.size() == name.size() &&
                fonts::Get_String_Width(fitted.c_str()) <= width) {
                return fitted;
            }
            while (!fitted.empty()) {
                fitted.pop_back();
                const std::string candidate = fitted + "...";
                if (fonts::Get_String_Width(candidate.c_str()) <= width) {
                    return candidate;
                }
            }
            return "...";
        }
    } // namespace

    void __cdecl Main_Menu_Fade_Function() {
        animate::Draw(0, 0, main_menu_background);
    }

    void __cdecl Main_Menu_Fade_Function2() {
        animate::Draw(0, 0, mainmenu_first_seg);
    }

    int16_t __cdecl Do_Main_Menu_File_Animation() {
        bool finished = false;
        bool input_detected = false;
        int16_t result_code = -1;
        int32_t current_sound_handle = 0;

        file_ani::Open_File_Animation("MAINMENU.LBX", 0);
        uint32_t buffer_size = MAINMENU::Get_File_Animation_Block_Space();
        int16_t total_frames = file_ani::Get_File_Animation_Total_Frames();

        void* buffer = allocate::Allocate_Next_Block(MOX::screen_seg, buffer_size);
        file_ani::Set_File_Animation_Buffer(buffer);

        palette::Set_Palette_Changes(0, 0xFF);
        animate::Draw_Palette(main_menu_background);

        fields::Clear_Fields();

        int16_t button_fields[6];
        button_fields[0] = fields::Add_Hidden_Field(10, 20, 25, 35, "C", ')');
        button_fields[1] = fields::Add_Hidden_Field(20, 30, 35, 45, "S", ')');
        button_fields[2] = fields::Add_Hidden_Field(30, 40, 45, 55, "L", ')');
        button_fields[3] = fields::Add_Hidden_Field(40, 50, 55, 65, "M", ')');
        button_fields[4] = fields::Add_Hidden_Field(50, 60, 65, 75, "H", ')');
        button_fields[5] = fields::Add_Hidden_Field(60, 70, 75, 85, "Q", ')');

        /* fmtpara: "\x1b" begins an inline color-attribute command; it is preserved here as the original non-printing field token. */
        int16_t hotkey_field = fields::Add_Hot_Key("\x1b");
        int16_t fullscreen_field = fields::Add_Hidden_Field(0, 0, 639, 479, "\x1b", ')');

        fields::Assign_Auto_Function(file_ani::thunk_Draw_File_Animation, 1);

        while (!finished) {
            timer::Mark_Time();

            int16_t input = fields::Get_Input();
            int16_t current_frame = file_ani::Get_File_Animation_Frame();

            if (current_frame == 5) {
                current_sound_handle = ERICNET::Mox_Play_Sound(0x35);
            } else if (current_frame == 46) {
                sound::Stop_Sound(current_sound_handle);
                current_sound_handle = ERICNET::Mox_Play_Sound(0x36);
            }

            if (input == fullscreen_field || input == hotkey_field) {
                finished = true;
                input_detected = true;
            } else if (current_frame == total_frames) {
                finished = true;
            } else {
                for (int32_t i = 0; i < 6; ++i) {
                    if (input == button_fields[i]) {
                        static const uint16_t mapping[] = {0x43, 0x53, 0x4C, 0x4D, 0x48, 0x51}; // C, S, L, M, H, Q
                        result_code = mapping[i];
                        finished = true;
                        input_detected = true;
                        break;
                    }
                }
            }

            if (!finished) {
                video::Set_Page_Off();
                file_ani::thunk_Draw_File_Animation();
                RUSS::Mox_Sync_Update();
            }

            timer::Release_Time(1);
        }

        if (input_detected) {
            sound::Stop_Sound(current_sound_handle);
            ERICNET::Mox_Play_Sound(0x36);
        }

        return result_code;
    }

    void __cdecl Add_Main_Menu_Fields() {
        char save_filename[16];
        char find_data[16];

        uint8_t game_type = FILEDEF::Get_Game_Type_From_Saved_Game(9);
        snprintf(save_filename, sizeof(save_filename), "save%d.gam", 10);
        fields::Clear_Fields();

        bool can_continue = false;
        if (game_type == 0 || game_type == 1) {
            if (dos::DIR(save_filename, find_data, sizeof(find_data)) == 1) {
                can_continue = true;
            }
        }

        if (can_continue) {
            mainmenu->fields[MAIN_MENU_BTN_CONTINUE].field = fields::Add_Hidden_Field(0x19F, 0xAC, 0x237, 0xC1, "C", 0x29);
        } else {
            mainmenu->fields[MAIN_MENU_BTN_CONTINUE].field = -1000;
        }

        if (MOX::disallow_saves_code == 0 && dos::DIR("save?.gam", find_data, sizeof(find_data)) == 1) {
            mainmenu->fields[MAIN_MENU_BTN_LOAD].field = fields::Add_Hidden_Field(0x19F, 0xC3, 0x237, 0xD7, "L", 0x29);
        } else {
            mainmenu->fields[MAIN_MENU_BTN_LOAD].field = -1000;
        }

        mainmenu->fields[MAIN_MENU_BTN_NEW].field = fields::Add_Hidden_Field(0x19F, 0xD9, 0x237, 0xEE, "N", 0x29);
        mainmenu->fields[MAIN_MENU_BTN_MULTIPLAY].field = fields::Add_Hidden_Field(0x19F, 0xF0, 0x237, 0x104, "M", 0x29);
        mainmenu->fields[MAIN_MENU_BTN_HELP].field = fields::Add_Hidden_Field(0x19F, 0x106, 0x237, 0x11B, "H", 0x29);
        mainmenu->fields[MAIN_MENU_BTN_QUIT].field = fields::Add_Hidden_Field(0x19F, 0x11D, 0x237, 0x132, "Q", 0x29);
        credits_field = mod_list_active
            ? fields::Add_Hidden_Field(
                  CREDITS_FIELD_LEFT,
                  CREDITS_FIELD_TOP,
                  CREDITS_FIELD_RIGHT,
                  CREDITS_FIELD_BOTTOM,
                  "",
                  0x29)
            : -1000;
    }

    void __cdecl Reload_Main_Menu_Screen() {
        int16_t entry_index = 3;

        RUSS::Mox_Update();

        main_menu_background = (s_animation_header*)farload::Far_Reload_Next("MAINMENU.LBX", 2, MOX::screen_seg);

        RUSS::Mox_Update();

        for (int32_t i = 0; i < 6; ++i) {
            mainmenu->fields[i].field0_0x0 = (s_animation_header*)farload::Far_Reload_Next("MAINMENU.LBX", entry_index, MOX::screen_seg);
            mainmenu->fields[i].field1_0x4 = (s_animation_header*)farload::Far_Reload_Next("MAINMENU.LBX", entry_index + 1, MOX::screen_seg);
            mainmenu->fields[i].field2_0x8 = (s_animation_header*)farload::Far_Reload_Next("MAINMENU.LBX", entry_index + 2, MOX::screen_seg);

            mainmenu->fields[i].field = -1000;

            entry_index += 3;
            RUSS::Mox_Update();
        }
    }

    void __cdecl Set_Main_Menu_Screen_Help_List_Aux(s_help_box* help_data, int16_t count) {
        fields::Deactivate_Help_List();
        fields::Set_Help_List(help_data, count);
    }

    void __cdecl Set_Main_Menu_Screen_Help_List() {
        Set_Main_Menu_Screen_Help_List_Aux(EVANHELP::main_menu_screen_help_list, 6);
    }

    void __cdecl Load_Mainmenu_Load_Game_Popup() {
        s_game_anims* anims = LOADSAVE::game_anims;
        const char* lbx = "GAME.LBX";
        void* seg = MOX::screen_seg;

        anims->pictures[11] = (s_animation_header*)farload::Far_Reload_Next(lbx, 0x14, seg);
        anims->pictures[12] = (s_animation_header*)farload::Far_Reload_Next(lbx, 0x15, seg);
        anims->pictures[13] = (s_animation_header*)farload::Far_Reload_Next(lbx, 0x16, seg);
        anims->pictures[16] = (s_animation_header*)farload::Far_Reload_Next(lbx, 0x17, seg);
        anims->pictures[17] = (s_animation_header*)farload::Far_Reload_Next(lbx, 0x18, seg);
        anims->pictures[18] = (s_animation_header*)farload::Far_Reload_Next(lbx, 0x19, seg);
        anims->pictures[19] = (s_animation_header*)farload::Far_Reload_Next(lbx, 0x1a, seg);
    }

    int8_t __cdecl Mainmenu_Load_Game_Popup(int16_t* save_game_list) {
        LOADSAVE::draw_game_popup_to_back = 1;
        MOX::screen_data = 2;

        mainmenu_first_seg = (s_animation_header*)farload::Far_Reload("MAINMENU.LBX", 0x15, MOX::screen_seg);

        LOADSAVE::game_anims = (s_game_anims*)allocate::Allocate_Next_Block(MOX::screen_seg, sizeof(*LOADSAVE::game_anims));
        memset(LOADSAVE::game_anims, 0, sizeof(*LOADSAVE::game_anims));

        LOADSAVE::game_popup_fields = (s_game_popup_fields*)allocate::Allocate_Next_Block(MOX::screen_seg, sizeof(*LOADSAVE::game_popup_fields));
        memset(LOADSAVE::game_popup_fields, 0xFF, sizeof(*LOADSAVE::game_popup_fields));

        LOADSAVE::Load_Month_Names();
        Load_Mainmenu_Load_Game_Popup();
        LOADSAVE::Set_Up_Load_Save_Popup();

        int8_t result = LOADSAVE::Do_Load_Game_Popup(save_game_list);

        fields::Clear_Fields();

        return result;
    }

    int8_t __cdecl Do_Mainmenu_Load_Screen() {
        int8_t result;
        int16_t popup_status = 0;

        mainmenu_last_seg = (s_animation_header*)farload::Far_Reload("MAINMENU.LBX", 21, MOX::screen_seg);

        video::Set_Refresh_Stencil();
        video::Set_Page_Back();

        // Clear background with black (0)
        graphics::Fill(0, 0, 649, 479, 0);

        animate::Draw(0, 0, mainmenu_last_seg);
        video::Copy_Back_To_Off();
        RUSS::Mox_Sync_Update();
        fields::Deactivate_Help_List();

        MOX::previous_screen = SCREEN_MAIN_MENU;

        do {
            result = Mainmenu_Load_Game_Popup(&popup_status);
        } while (popup_status != 1);

        return result;
    }

    void __cdecl Draw_Main_Menu_Screen() {
        s_colors color_buffer;

        if (MOX::FULL_DRAW_SCREEN_FLAG != 0) {
            video::Set_Page_Off();
            graphics::Fill(0, 0, 0x27F, 0x1DF, 0);
            animate::Draw(0, 0, main_menu_background);
            video::Copy_Off_To_Back();
            RUSS::Mox_Sync_Update();
            credit_x = 0x42;
            credit_y_slot.dword = 0x1A6;
            delay_count = 10;
            MOX::FULL_DRAW_SCREEN_FLAG = 0;
        }

        s_animation_header* anim_to_draw = nullptr;
        int16_t draw_y = 0;

        if (mainmenu->fields[MAIN_MENU_BTN_CONTINUE].field == MOX::scanned_field) {
            anim_to_draw = mainmenu->fields[MAIN_MENU_BTN_CONTINUE].field1_0x4;
            draw_y = 0xAC;
        } else if (mainmenu->fields[MAIN_MENU_BTN_LOAD].field == MOX::scanned_field) {
            anim_to_draw = mainmenu->fields[MAIN_MENU_BTN_LOAD].field1_0x4;
            draw_y = 0xC3;
        } else if (mainmenu->fields[MAIN_MENU_BTN_NEW].field == MOX::scanned_field) {
            anim_to_draw = mainmenu->fields[MAIN_MENU_BTN_NEW].field1_0x4;
            draw_y = 0xD9;
        } else if (mainmenu->fields[MAIN_MENU_BTN_MULTIPLAY].field == MOX::scanned_field) {
            anim_to_draw = mainmenu->fields[MAIN_MENU_BTN_MULTIPLAY].field1_0x4;
            draw_y = 0xF0;
        } else if (mainmenu->fields[MAIN_MENU_BTN_HELP].field == MOX::scanned_field) {
            anim_to_draw = mainmenu->fields[MAIN_MENU_BTN_HELP].field1_0x4;
            draw_y = 0x106;
        } else if (mainmenu->fields[MAIN_MENU_BTN_QUIT].field == MOX::scanned_field) {
            anim_to_draw = mainmenu->fields[MAIN_MENU_BTN_QUIT].field1_0x4;
            draw_y = 0x11D;
        }

        if (anim_to_draw != nullptr) {
            animate::Draw(0x19F, draw_y, anim_to_draw);
        }

        // Special handling for disabled/state-specific buttons (ID -1000)
        if (mainmenu->fields[MAIN_MENU_BTN_CONTINUE].field == -1000) { // -1000
            animate::Draw(0x19F, 0xAC, mainmenu->fields[MAIN_MENU_BTN_CONTINUE].field2_0x8);
        }
        if (mainmenu->fields[MAIN_MENU_BTN_LOAD].field == -1000) { // -1000
            animate::Draw(0x19F, 0xC3, mainmenu->fields[MAIN_MENU_BTN_LOAD].field2_0x8);
        }

        if (mod_list_active) {
            Draw_Loaded_Mods();
        } else if (credit_num != 0) {
            Draw_Credits();
        }

        s_colors* colors = MISC::Font_Colors2(1, 0xD2, 0xD7);
        color_buffer = *colors;

        fonts::Set_Font_Style(1, &color_buffer);
        int16_t font_h = fonts::Get_Font_Height();
        fonts::Print_Centered(0x205, 0x1BB - (font_h + 1), GAME_VERSION_LABEL);
    }

    void __cdecl Draw_Loaded_Mods() {
        s_colors colors = *MISC::Font_Colors2(4, 0xD5, 0xDC);
        fonts::Set_Outline_Color(0xFD, 0xFD);
        fonts::Set_Font_Style_Outline(2, &colors);
        fonts::Set_Font_Spacing(1);
        fonts::Print_Centered((MOD_LIST_LEFT + MOD_LIST_RIGHT) / 2,
            MOD_LIST_TOP,
            Loaded_Mods_Label());

        int16_t row_height = fonts::Get_Font_Height() + 2;
        if (row_height < MOD_LIST_MIN_ROW_HEIGHT) {
            row_height = MOD_LIST_MIN_ROW_HEIGHT;
        }
        int16_t y = MOD_LIST_TOP + row_height;
        size_t max_rows = static_cast<size_t>(
            (MOD_LIST_BOTTOM - y) / row_height + 1);
        if (max_rows == 0) {
            max_rows = 1;
        }

        const config::t_loaded_mod_list& loaded_mods =
            config::Get_Loaded_Mods();
        if (loaded_mods.empty()) {
            fonts::Print_Centered((MOD_LIST_LEFT + MOD_LIST_RIGHT) / 2,
                y,
                No_Mods_Label());
        } else {
            const bool has_overflow = loaded_mods.size() > max_rows;
            const size_t visible_count = has_overflow
                ? max_rows - 1
                : loaded_mods.size();
            for (size_t index = 0; index < visible_count; ++index) {
                const config::s_loaded_mod& loaded_mod = loaded_mods[index];
                const std::string prefix =
                    std::to_string(loaded_mod.priority_index + 1) + ". ";
                const std::string suffix = "  " +
                    Fit_Mod_Name(loaded_mod.manifest.version_text,
                        (MOD_LIST_RIGHT - MOD_LIST_LEFT) / 3);
                const int16_t name_width = static_cast<int16_t>(
                    MOD_LIST_RIGHT - MOD_LIST_LEFT - 8 -
                    fonts::Get_String_Width(prefix.c_str()) -
                    fonts::Get_String_Width(suffix.c_str()));
                const std::string name = Fit_Mod_Name(
                    loaded_mod.manifest.name, name_width);
                fonts::Print(MOD_LIST_LEFT + 4, y, (prefix + name).c_str());
                fonts::Print_Right(MOD_LIST_RIGHT - 4, y, suffix.c_str());
                y += row_height;
            }
            if (has_overflow) {
                const std::string remaining = "+" +
                    std::to_string(loaded_mods.size() - visible_count) +
                    " more";
                fonts::Print_Centered((MOD_LIST_LEFT + MOD_LIST_RIGHT) / 2,
                    y,
                    remaining.c_str());
            }
        }

        colors = *MISC::Font_Colors2(4,
            credits_field == MOX::scanned_field ? 0xD5 : 0xD2,
            credits_field == MOX::scanned_field ? 0xDC : 0xD7);
        fonts::Set_Outline_Color(0xFD, 0xFD);
        fonts::Set_Font_Style_Outline(3, &colors);
        fonts::Set_Font_Spacing(1);
        const int16_t credits_y =
            MAIN_MENU_TEXT_BOTTOM - fonts::Get_Font_Height();
        fonts::Print_Centered((CREDITS_FIELD_LEFT + CREDITS_FIELD_RIGHT) / 2,
            credits_y,
            Credits_Label());
    }

    void __cdecl Copy_Credit_String_To_Bitmap(int16_t index, s_animation_header* bitmap) {
        char buffer[100];
        char(*credits)[2][127] = MAINMENU::credits;

        if (credits[index][0][0] == '\0' && credits[index][1][0] == '\0') {
            return;
        }

        strlcpy(buffer, credits[index][0], sizeof(buffer));
        fonts::Print_To_Bitmap(0, 0, buffer, bitmap);

        int16_t x_pos = 0;
        if (buffer[0] != '\0') {
            x_pos = fonts::Get_String_Width(buffer) + 4;
        }

        if (credits[index][0][0] != '\0' && credits[index][1][0] != '\0') {
            while (x_pos < (int16_t)MAINMENU::credits_column_one) {
                fonts::Print_To_Bitmap(x_pos, 0, ".", bitmap);
                x_pos += fonts::Get_String_Width(".");
            }
        }

        strlcpy(buffer, credits[index][1], sizeof(buffer));
        fonts::Print_To_Bitmap((int16_t)MAINMENU::credits_column_one + 4, 0, buffer, bitmap);
    }

    void __cdecl Fade_Credit_Colors(int8_t amount, s_colors* colors) {
        for (int16_t i = 1; i < 8; ++i) {
            colors->colors[i] += amount;
        }
    }

    void __cdecl Draw_Credits_Screen() {
        Draw_Credits();
    }

    void __cdecl Credits_Screen() {
        video::Set_Refresh_Full();
        fields::Clear_Fields();
        RUSS::Mox_Update();

        mainmenu_first_seg = (s_animation_header*)farload::Far_Reload(
            "MAINMENU.LBX", 1, MOX::screen_seg);
        RUSS::Mox_Update();
        main_menu_background = (s_animation_header*)farload::Far_Reload_Next(
            "MAINMENU.LBX", 2, MOX::screen_seg);
        RUSS::Mox_Update();

        file_ani::Open_File_Animation("MAINMENU.LBX", 0);
        const uint32_t buffer_size = Get_File_Animation_Block_Space();
        void* animation_buffer =
            allocate::Allocate_Next_Block(MOX::screen_seg, buffer_size);
        file_ani::Set_File_Animation_Buffer(animation_buffer);

        video::Set_Refresh_Stencil();
        video::Set_Page_Back();
        graphics::Fill(0, 0, 639, 479, 0);
        animate::Draw(0, 0, mainmenu_first_seg);
        video::Copy_Back_To_Off();
        video::Set_Page_Off();
        palette::Set_Palette_Changes(0, 0xFF);
        animate::Draw_Palette(main_menu_background);
        file_ani::Set_File_Animation_Frame(0);
        file_ani::Draw_File_Animation();
        video::Copy_Off_To_Back();
        RUSS::Mox_Sync_Update();

        Load_Credits();
        RUSS::Mox_Update();

        fields::Clear_Fields();
        const int16_t dismiss_field =
            fields::Add_Hidden_Field(0, 0, 639, 479, "\x1B", 0x29);
        fields::Assign_Auto_Function(Draw_Credits_Screen, 2);
        credit_x = 66;
        credit_y_slot.y = 422;
        delay_count = 10;

        bool screen_exit = false;
        while (!screen_exit) {
            timer::Mark_Time();
            const int16_t input = fields::Get_Input();
            screen_exit = input == dismiss_field || input == -1;
            if (!screen_exit) {
                video::Set_Page_Off();
                Draw_Credits_Screen();
                RUSS::Mox_Sync_Update();
            }
            timer::Release_Time(3);
        }

        fields::Deactivate_Auto_Function();
        fields::Clear_Fields();
        MOX::current_screen = SCREEN_MAIN_MENU;
        MOX::FULL_DRAW_SCREEN_FLAG = 0;
    }

    void __cdecl Draw_Credits() {
        s_colors fade_out_palette = {0xD3, 0xD5, 0xD4, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5};
        s_colors fade_in_palette = {0xD4, 0xD9, 0xD7, 0xD9, 0xD9, 0xD9, 0xD9, 0xD9};
        s_colors default_palette = {0xD5, 0xDC, 0xDA, 0xDC, 0xDC, 0xDC, 0xDC, 0xDC};
        s_colors active_palette;

        if (MAINMENU::delay_count <= 0) {
            MAINMENU::credit_y_slot.y -= 2;
            MAINMENU::credit_count = 0x1A6 /* 422 */ - MAINMENU::credit_y_slot.y;

            int16_t fade_in_line_idx = -1;
            if (MAINMENU::credit_y_slot.y <= 0xB3 /* 179 */) {
                fade_in_line_idx = (int16_t)((0xB3 - MAINMENU::credit_y_slot.y) / 14);
            }

            int16_t fade_out_line_idx = (int16_t)(MAINMENU::credit_count / 14);
            int16_t current_y = MAINMENU::credit_y_slot.y;

            for (int16_t i = 0; i < MAINMENU::max_credits; ++i) {
                if (i == fade_in_line_idx) {
                    active_palette = fade_in_palette;
                    int16_t remainder = (MAINMENU::credit_y_slot.y - 0xB3) % 14;
                    int8_t step = (int8_t)((remainder / 3) * 2);
                    MAINMENU::Fade_Credit_Colors(step, &active_palette);
                } else if (i == fade_out_line_idx) {
                    active_palette = fade_out_palette;
                    int32_t remainder = MAINMENU::credit_count % 14;
                    int8_t step = (int8_t)(((remainder + (remainder < 0 ? 3 : 0)) >> 2) * 2);
                    MAINMENU::Fade_Credit_Colors(step, &active_palette);
                } else {
                    active_palette = default_palette;
                }

                fonts::Set_Outline_Color(0xFD, 0xFD);
                fonts::Set_Font_Style_Outline(MAINMENU::credits_font_style, &active_palette);
                fonts::Set_Font_Spacing((int16_t)MAINMENU::credits_font_spacing);

                bitmap::Create_Blank_Bitmap(289 /* 0x121 */, 14, MAINMENU::bitmap);
                MAINMENU::Copy_Credit_String_To_Bitmap(i, MAINMENU::bitmap);

                graphics::Set_Window(66 /* 0x42 */, 179 /* 0xB3 */, 354 /* 0x162 */, 422 /* 0x1A6 */);
                video::Clip_On();
                animate::Draw(66 /* 0x42 */, current_y, MAINMENU::bitmap);
                video::Clip_Off();

                current_y += 14;
            }
        } else {
            MAINMENU::delay_count--;
        }

        // Check if credits have finished scrolling off the top
        if (abs(MAINMENU::credit_y_slot.y - 179) > MAINMENU::max_credits * 14) {
            MAINMENU::credit_x = 0x42 /* 66 */;
            MAINMENU::credit_y_slot.dword = 0x1A6 /* 422 */;
            MAINMENU::delay_count = 10;
        }
    }

    void __cdecl Main_Menu_Screen() {
        uint8_t fade_data;
        int16_t last_input_scan = -1;

        video::Set_Refresh_Full();
        fields::Clear_Fields();
        RUSS::Mox_Update();

        mainmenu_first_seg = (s_animation_header*)farload::Far_Reload("MAINMENU.LBX", 1, MOX::screen_seg);
        RUSS::Mox_Update();
        mod_list_active = show_mod_list;
        if (mod_list_active) {
            credit_num = 0;
        } else {
            MAINMENU::Load_Credits();
        }
        RUSS::Mox_Update();

        MOX::game_type = 0;
        mainmenu = (s_main_menu*)allocate::Allocate_First_Block(MOX::global_data_seg, sizeof(s_main_menu));
        _gametype = allocate::Allocate_Next_Block(MOX::global_data_seg, 40);
        RUSS::Mox_Update();

        // Initialize main menu structure fields to -1
        memset(mainmenu, 0xFF, sizeof(s_main_menu));

        LOADSAVE::game_popup_fields = (s_game_popup_fields*)allocate::Allocate_Next_Block(MOX::global_data_seg, sizeof(*LOADSAVE::game_popup_fields));
        MOX::save_game_dates = (char*)allocate::Allocate_Next_Block(MOX::global_data_seg, 1000);
        MOX::save_game_stardates = (char*)allocate::Allocate_Next_Block(MOX::global_data_seg, 1000);
        RUSS::Mox_Update();

        if (MOX2::pan_main_menu_panel != 0) {
            Reload_Main_Menu_Screen();
            MOX2::pan_main_menu_panel = 0;
            MISC::Fade_Into_Screen(Main_Menu_Fade_Function2, &fade_data, 5);
            video::Set_Refresh_Full();
            last_input_scan = Do_Main_Menu_File_Animation();
        } else {
            bool full_redraw = (MOX::previous_screen == SCREEN_GAME ||
                MOX::previous_screen == SCREEN_HALL_OF_FAME ||
                MOX::previous_screen == SCREEN_MULTI_PLAYER ||
                MOX::previous_screen == SCREEN_MAIN_NEXT_TURN ||
                MOX::previous_screen == SCREEN_NEW_GAME ||
                MOX::previous_screen == SCREEN_CREDITS ||
                MOX::FULL_DRAW_SCREEN_FLAG != 0);

            if (full_redraw) {
                MOX::FULL_DRAW_SCREEN_FLAG = 0;
                Reload_Main_Menu_Screen();
                MISC::Fade_Into_Screen(Main_Menu_Fade_Function, &fade_data, 5);
            } else if (MOX::previous_screen == SCREEN_MAIN_MENU) {
                Reload_Main_Menu_Screen();
                video::Set_Page_Off();
                graphics::Fill(0, 0, 639, 479, 0);
                animate::Draw(0, 0, (s_animation_header*)main_menu_background);
                RUSS::Mox_Sync_Update();
            } else {
                goto STENCIL_SETUP;
            }
        }

        Add_Main_Menu_Fields();

    STENCIL_SETUP:
        video::Set_Refresh_Stencil();
        video::Set_Page_Back();
        graphics::Fill(0, 0, 639, 479, 0);
        animate::Draw(0, 0, (s_animation_header*)main_menu_background);
        video::Copy_Back_To_Off();
        RUSS::Mox_Sync_Update();

        int16_t hotkey_n = fields::Add_Hot_Key("N");
        bool screen_exit = false;

        fields::Assign_Auto_Function(Draw_Main_Menu_Screen, 2);
        Set_Main_Menu_Screen_Help_List();
        credit_x = 66;         // 0x42
        credit_y_slot.y = 422; // 0x1a6
        delay_count = 10;

        do {
            int32_t input = fields::Get_Input();
            int16_t input_field = (int16_t)input;
            MOX::scanned_field = fields::Scan_Input();
            timer::Mark_Time();

            const int16_t autoload_slot = autoload_save_slot;
            autoload_save_slot = -1;
            // Continue (SAVE10.GAM), or a one-shot startup save request.
            if (autoload_slot >= 0 || input_field == mainmenu->fields[MAIN_MENU_BTN_CONTINUE].field || (last_input_scan == 0x43 && mainmenu->fields[MAIN_MENU_BTN_CONTINUE].field != -1000)) {
                if (FILEDEF::Load_Game(autoload_slot >= 0 ? autoload_slot : 9) != 0) {
                    if (MOX::game_type != 2 && MOX::game_type != 3) {
                        MOX::current_screen = (MOX::game_type == 1) ? SCREEN_HOTSEAT_SELECT_PLAYER : SCREEN_REPORTS;
                    } else {
                        GENDRAW::Warning_Box(HAROLD::H_Message(207));
                        MOX::current_screen = SCREEN_MAIN_MENU;
                    }
                } else {
                    MOX::current_screen = SCREEN_MAIN_MENU;
                }
                screen_exit = true;
            }
            // New Game
            else if (input_field == mainmenu->fields[MAIN_MENU_BTN_NEW].field || input_field == hotkey_n || last_input_scan == 0x4e) {
                MOX::current_screen = SCREEN_NEW_GAME;
                MOX::return_screen = SCREEN_MAIN;
                screen_exit = true;
            }
            // Load Game
            else if (input_field == mainmenu->fields[MAIN_MENU_BTN_LOAD].field || last_input_scan == 0x4c) {
                if (Do_Mainmenu_Load_Screen() != 0) {
                    if (MOX::game_type == 2 || MOX::game_type == 3) {
                        GENDRAW::Warning_Box(HAROLD::H_Message(208));
                        MOX::current_screen = SCREEN_MAIN_MENU;
                    } else {
                        MOX::current_screen = (MOX::game_type == 1) ? SCREEN_HOTSEAT_SELECT_PLAYER : SCREEN_REPORTS;
                    }
                } else {
                    if (MOX::previous_screen == SCREEN_MAIN) {
                        MOX::current_screen = SCREEN_MAIN;
                    } else {
                        MOX::current_screen = SCREEN_MAIN_MENU;
                    }
                }
                screen_exit = true;
            }
            // Multi-player
            else if (input_field == mainmenu->fields[MAIN_MENU_BTN_MULTIPLAY].field || last_input_scan == 0x4d) {
                MOX::current_screen = SCREEN_MULTI_PLAYER;
                MOX::return_screen = SCREEN_MAIN_MENU;
                screen_exit = true;
            }
            // Hall of Fame
            else if (input_field == mainmenu->fields[MAIN_MENU_BTN_HELP].field || last_input_scan == 0x48) {
                MOX::current_screen = SCREEN_HALL_OF_FAME;
                MOX::return_screen = SCREEN_MAIN_MENU;
                screen_exit = true;
            }
            // Credits
            else if (mod_list_active && input_field == credits_field) {
                MOX::current_screen = SCREEN_CREDITS;
                MOX::return_screen = SCREEN_MAIN_MENU;
                screen_exit = true;
            }
            // Quit
            else if (input_field == mainmenu->fields[MAIN_MENU_BTN_QUIT].field || last_input_scan == 0x51) {
                MOX::current_screen = SCREEN_EXIT;
                screen_exit = true;
            }

            if (!screen_exit) {
                video::Set_Page_Off();
                Draw_Main_Menu_Screen();
                RUSS::Mox_Sync_Update();
            }

            timer::Release_Time(3);

            if (screen_exit) {
                MAINSCR2::main_screen_fade_in_flag = 1;
                MOX::FULL_DRAW_SCREEN_FLAG = 0;
                fields::Deactivate_Auto_Function();
                return;
            }
        } while (true);
    }

    void __cdecl Load_Credits() {
        char path[20];
        char* data;
        int16_t credit_idx;
        int32_t entry_idx;

        Init_Credits(path, sizeof(path));
        RUSS::Mox_Update();

        // Load entry 0 to get the total number of credits
        data = (char*)farload::Far_Reload_Next_Data(path, 0, MOX::screen_seg, 0, 1, 127);
        entry_idx = 1;
        RUSS::Mox_Update();

        max_credits = (int16_t)atoi(data);
        credits = (char(*)[2][127])allocate::Allocate_Next_Block(MOX::screen_seg, sizeof(*credits) * (uint32_t)max_credits);

        for (credit_idx = 0; credit_idx < max_credits; ++credit_idx) {
            RUSS::Mox_Update();

            // Load first line of credit entry
            allocate::Push_Block(MOX::screen_seg);
            data = (char*)farload::Far_Reload_Next_Data(path, 0, MOX::screen_seg, entry_idx++, 1, 127);
            allocate::Pop_Block(MOX::screen_seg);
            strlcpy(credits[credit_idx][0], data, sizeof(credits[credit_idx][0]));
            string_overrides::Apply_String(path, credit_idx * 2 + 1, credits[credit_idx][0], sizeof(credits[credit_idx][0]));

            // Load second line of credit entry
            allocate::Push_Block(MOX::screen_seg);
            data = (char*)farload::Far_Reload_Next_Data(path, 0, MOX::screen_seg, entry_idx++, 1, 127);
            allocate::Pop_Block(MOX::screen_seg);
            strlcpy(credits[credit_idx][1], data, sizeof(credits[credit_idx][1]));
            string_overrides::Apply_String(path, credit_idx * 2 + 2, credits[credit_idx][1], sizeof(credits[credit_idx][1]));
        }

        credit_num = 1;
        credit_x = 66;
        delay_count = 0;
        credit_count = 0;
        fade_in_count = 0;
        credit_y_slot.dword = 0x1A6;
        fade_out_count = 0;

        uint32_t bitmap_size = bitmap::Get_Bitmap_Size(289, 14);
        bitmap = (s_animation_header*)allocate::Allocate_Next_Block(MOX::screen_seg, bitmap_size);
        RUSS::Mox_Update();
    }

    int32_t __cdecl Get_File_Animation_Block_Space() {
        s_animation_header* header = init::io_buffer;
        int16_t index = header->current_frame;
        int32_t current_val = Animation_Frame_Offset_(header, index);
        int32_t next_val = Animation_Frame_Offset_(header, index + 1);
        return (next_val - current_val) + 1;
    }

    void __cdecl Init_Credits(char* filename, uint32_t filename_size) {
        switch (MOX::settings.language) {
            case 1: /* German */
                credits_font_style = 2;
                credits_font_spacing = 2;
                credits_column_one = 0x9a;
                strlcpy(filename, "GERCRDTS.LBX", filename_size);
                return;

            case 2: /* French */
                credits_column_one = 0xa4;
                credits_font_spacing = 1;
                credits_font_style = 2;
                strlcpy(filename, "FRECRDTS.LBX", filename_size);
                return;

            case 3: /* Spanish */
                credits_column_one = 0x92;
                credits_font_spacing = 1;
                credits_font_style = 2;
                strlcpy(filename, "SPACRDTS.LBX", filename_size);
                return;

            case 4:
                credits_column_one = 0x92;
                credits_font_spacing = 1;
                credits_font_style = 2;
                strlcpy(filename, "ITACRDTS.LBX", filename_size);
                return;

            default: /* English */
                credits_column_one = 0x7c;
                credits_font_spacing = 1;
                credits_font_style = 3;
                strlcpy(filename, "CREDITS.LBX", filename_size);
                return;
        }
    }
} // namespace MAINMENU