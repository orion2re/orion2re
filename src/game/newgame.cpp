#include "pch.h"

#include "config/config_decode.h"
namespace NEWGAME {

    int16_t g_x = 15;
    int16_t g_y = 5;
    s_colors color_ken;
    int16_t opp_button_variable;
    int16_t diff_button_variable;
    int16_t xenon_attacks_button_variable;
    int16_t civ_button_variable;
    uint16_t combat_button_variable;
    int16_t gal_size_button_variable;
    int16_t random_events_button_variable;
    int16_t gal_age_button_variable;
    int16_t fields[10];
    s_animation_header* anims[7];
    s_animation_header* tech_level_anims[STARTING_CIVILIZATION_LEVEL_COUNT];
    s_animation_header* difficulty_anims[GAME_DIFFICULTY_COUNT];
    s_animation_header* num_players_anims[MAX_PLAYERS];
    s_animation_header* gal_age_anims[GALAXY_AGE_COUNT];
    s_animation_header* gal_size_anims[GALAXY_SIZE_COUNT - 1];
    s_animation_header* background_seg;
    char* gal_size_button_strings[GALAXY_SIZE_COUNT - 1];
    char* civ_button_strings[STARTING_CIVILIZATION_LEVEL_COUNT];
    char* diff_button_strings[GAME_DIFFICULTY_COUNT];
    char* gal_age_button_strings[GALAXY_AGE_COUNT];
    char* combat_button_strings[2];
    char localized_gal_size_label[20];
    char localized_postwarp_label[20];
    char localized_random_label[20];
    char* opp_button_strings[MAX_PLAYERS];
    int16_t accept_hotkey;

    namespace {

        enum {
            NEW_GAME_GALAXY_SIZE_HUGE = 3,
            NEW_GAME_GALAXY_SIZE_MAXIMUM = 4,
            NEW_GAME_GALAXY_SIZE_COUNT = 5,
            NEW_GAME_CIVILIZATION_POST_WARP = 2,
            NEW_GAME_CIVILIZATION_ADVANCED = 3,
            NEW_GAME_PLAYER_COUNT_RANDOM = MAX_PLAYERS - 1,
            NEW_GAME_RANDOM_PICTURE_ENTRY = 31,
            NEW_GAME_POST_WARP_PICTURE_ENTRY = 32,
        };

        bool random_player_count;

        int16_t Galaxy_Size_Button_From_Setting(uint8_t galaxy_size) {
            switch (galaxy_size) {
                case GALAXY_SIZE_CLUSTER:
                    return NEW_GAME_GALAXY_SIZE_HUGE;
                case GALAXY_SIZE_HUGE:
                    return NEW_GAME_GALAXY_SIZE_HUGE;
                case GALAXY_SIZE_MAXIMUM:
                    return NEW_GAME_GALAXY_SIZE_MAXIMUM;
                default:
                    return galaxy_size;
            }
        }

        uint8_t Galaxy_Size_Setting_From_Button(int16_t button) {
            switch (button) {
                case NEW_GAME_GALAXY_SIZE_HUGE:
                    return GALAXY_SIZE_HUGE;
                case NEW_GAME_GALAXY_SIZE_MAXIMUM:
                    return GALAXY_SIZE_MAXIMUM;
                default:
                    return (uint8_t)button;
            }
        }

        int16_t Civilization_Button_From_Setting(uint8_t civilization_level) {
            if (civilization_level == STARTING_CIVILIZATION_LEVEL_POST_WARP) {
                return NEW_GAME_CIVILIZATION_POST_WARP;
            }
            if (civilization_level == STARTING_CIVILIZATION_LEVEL_ADVANCED) {
                return NEW_GAME_CIVILIZATION_ADVANCED;
            }
            return civilization_level;
        }

        uint8_t Civilization_Setting_From_Button(int16_t button) {
            if (button == NEW_GAME_CIVILIZATION_POST_WARP) {
                return STARTING_CIVILIZATION_LEVEL_POST_WARP;
            }
            if (button == NEW_GAME_CIVILIZATION_ADVANCED) {
                return STARTING_CIVILIZATION_LEVEL_ADVANCED;
            }
            return (uint8_t)button;
        }

        bool Show_Net_Game_Name() {
            return MOX::game_type == 2 || (MOX::game_type == 3 && NETMOX::net.mode == 1);
        }

        void Ensure_Settings_Net_Game_Name() {
            if (MOX::settings.net_game_name[0] != '\0') {
                return;
            }

            memcpy(MOX::settings.net_game_name, "Moo II ", sizeof("Moo II "));
            MOX::settings.net_game_name[6] = (char)('a' + ((int32_t)clock() % 26));
        }

    } // namespace

    int16_t Player_Count_For_Initialization() {
        if (random_player_count ||
            MOX::settings.number_of_players == NEW_GAME_RANDOM_PLAYER_COUNT) {
            return NEW_GAME_RANDOM_PLAYER_COUNT;
        }
        return MOX::settings.number_of_players;
    }

    int16_t __cdecl Newgame_Screen(void) {
        memcpy(NEWGAME::color_ken.colors, "y{z|||||", sizeof(NEWGAME::color_ken.colors));

        struct_::Clear_Structure(MOX::star, MAX_STARS * sizeof(*MOX::star));
        struct_::Clear_Structure(MOX::planet, MAX_PLANETS * sizeof(*MOX::planet));
        struct_::Clear_Structure(MOX::colony, MAX_COLONIES * sizeof(*MOX::colony));
        struct_::Clear_Structure(MOX::ship, MAX_SHIPS * sizeof(*MOX::ship));
        struct_::Clear_Structure(MOX::player, MAX_PLAYERS * sizeof(*MOX::player));
        struct_::Clear_Structure(MOX::leaders, MAX_LEADERS * sizeof(*MOX::leaders));

        int16_t result = 0;

        mouse::Set_Mouse_List(&COLGCBT::mouse_list_blank, 1);
        ERIC::Fast_Fade_Out();
        fonts::Load_Palette(9, 0, 0xff);

        if (MOX::game_type == 0 || MOX::game_type == 1) {
            memset(RACESEL::color_flag, 0, sizeof(RACESEL::color_flag));
            memset(RACESEL::race_flag, 0, sizeof(RACESEL::race_flag));
        }

        NEWGAME::Reload_Newgame_Screen();
        NEWGAME::Globals_To_Temps();
        NEWGAME::Update_Fields();

        COLONY::full_draw = 1;
        video::Set_Refresh_Stencil();
        ERICHELP::Set_New_Game_Screen_Help_List();
        fields::Assign_Auto_Function(NEWGAME::Draw_Newgame_Screen, 2);
        video::Set_Page_Off();
        NEWGAME::Draw_Newgame_Screen();
        palstore::Darken_Palette(100);
        palette::Clear_Palette_Changes(0, 0xff);

        bool exit_screen = false;
        ERIC::Fast_Fade_In();
        mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);

        while (!exit_screen) {
            int16_t input = fields::Get_Input();
            FAKEBUTS::Evaluate_Click_Thru_Fields(input);

            if (input == NEWGAME::fields[6] || input == NEWGAME::fields[7]) {
                MOX::current_screen = MOX::return_screen;
                exit_screen = true;
            }

            if (input == NEWGAME::fields[8] || input == NEWGAME::accept_hotkey) {
                NEWGAME::Temps_To_Globals();
                if (MOX::game_type == 1) {
                    MOX::current_screen = SCREEN_HOTSEAT;
                    result = 1;
                    exit_screen = true;
                } else if (MOX::game_type == 2 || MOX::game_type == 3) {
                    MOX::current_screen = SCREEN_START_NET;
                    result = 1;
                    exit_screen = true;
                } else {
                    result = RACESEL::Race_Selection_Screen(MOX::player);
                    if (result == 1) {
                        MOX::player->objectives = 100 /* 0x64 */;
                        exit_screen = true;
                    } else {
                        NEWGAME::Reload_Newgame_Screen();
                        video::Set_Refresh_Stencil();
                        COLONY::full_draw = 1;
                        NEWGAME::Globals_To_Temps();
                        NEWGAME::Update_Fields();
                        fields::Assign_Auto_Function(NEWGAME::Draw_Newgame_Screen, 2);
                        ERICHELP::Set_New_Game_Screen_Help_List();
                    }
                }
            }

            if (!exit_screen) {
                fonts::Set_Font_Style(3, &NEWGAME::color_ken);
                NEWGAME::Draw_Newgame_Screen();
                fonts::Set_Font_Style(3, &NEWGAME::color_ken);
                RUSS::Mox_Sync_Update();
            }
        }

        if (result == 1) {
            NEWGAME::Temps_To_Globals();
        }

        fields::Clear_Fields();
        FAKEBUTS::Clear_Click_Thru_Fields();
        fields::Deactivate_Help_List();
        fields::Deactivate_Auto_Function();

        if (MOX::game_type == 2 || MOX::game_type == 3) {
            strlcpy(MOX::settings.net_game_name, NETMOX::net.cur_game_name, sizeof(MOX::settings.net_game_name));
        }

        if (result == 1 && random_player_count) {
            MOX::settings.number_of_players = MAX_PLAYERS;
        }

        return result;
    }

    void __cdecl Globals_To_Temps() {
        diff_button_variable = (int16_t)(int8_t)MOX::settings.game_difficulty;
        gal_size_button_variable = Galaxy_Size_Button_From_Setting(MOX::settings.galaxy_size);
        gal_age_button_variable = (int16_t)(int8_t)MOX::settings.galaxy_age;
        random_player_count =
            MOX::settings.number_of_players == NEW_GAME_RANDOM_PLAYER_COUNT;
        opp_button_variable = random_player_count
            ? NEW_GAME_PLAYER_COUNT_RANDOM
            : (int16_t)((int8_t)MOX::settings.number_of_players - 2);
        // A player count inherited from a save can be invalid (0, 1, ...): the Players box is then empty and the
        // game would start with 0 players (divide by zero in SHIPMOVE::Allocate_New_Ship_Slots). Default: 4 players.
        if (!random_player_count && (opp_button_variable < 0 || opp_button_variable > MAX_PLAYERS - 2)) {
            opp_button_variable = 2;
        }
        combat_button_variable = (uint16_t)(MOX::settings.strategic_combat_flag == 0);
        civ_button_variable = Civilization_Button_From_Setting(
            MOX::settings.starting_civilization_level);
        random_events_button_variable = (uint8_t)MOX::settings.random_events;
        xenon_attacks_button_variable = MOX::settings.xenons_exist;
    }

    void __cdecl Reload_Newgame_Screen() {
        background_seg = (s_animation_header*)farload::Far_Reload("RACEOPT.LBX", 0, MOX::screen_seg);
        RUSS::Mox_Update();

        int16_t index = 0;
        for (int i = 0; i < GALAXY_AGE_COUNT; ++i) {
            gal_age_anims[i] = (s_animation_header*)farload::Far_Reload_Next(COLONY::NEWGAME_LBX, i + 1, MOX::screen_seg);
            index = i + 1;
        }

        index++;
        RUSS::Mox_Update();

        for (int i = 0; i < GAME_DIFFICULTY_COUNT; ++i) {
            difficulty_anims[i] = (s_animation_header*)farload::Far_Reload_Next(COLONY::NEWGAME_LBX, index + i, MOX::screen_seg);
        }
        index += GAME_DIFFICULTY_COUNT;
        RUSS::Mox_Update();

        for (int i = 0; i < GALAXY_SIZE_MAXIMUM; ++i) {
            gal_size_anims[i] = (s_animation_header*)farload::Far_Reload_Next(COLONY::NEWGAME_LBX, index + i, MOX::screen_seg);
        }
        index += GALAXY_SIZE_MAXIMUM;
        RUSS::Mox_Update();

        for (int i = 0; i < MAX_PLAYERS - 1; ++i) {
            num_players_anims[i] = (s_animation_header*)farload::Far_Reload_Next(COLONY::NEWGAME_LBX, index + i, MOX::screen_seg);
        }
        index += MAX_PLAYERS - 1;
        num_players_anims[NEW_GAME_PLAYER_COUNT_RANDOM] =
            (s_animation_header*)farload::Far_Reload_Next(COLONY::NEWGAME_LBX, NEW_GAME_RANDOM_PICTURE_ENTRY, MOX::screen_seg);
        RUSS::Mox_Update();

        for (int i = 0; i < 3; ++i) {
            tech_level_anims[i] = (s_animation_header*)farload::Far_Reload_Next(COLONY::NEWGAME_LBX, index + i, MOX::screen_seg);
        }
        index += 3;
        tech_level_anims[NEW_GAME_CIVILIZATION_ADVANCED] =
            tech_level_anims[STARTING_CIVILIZATION_LEVEL_ADVANCED];
        tech_level_anims[NEW_GAME_CIVILIZATION_POST_WARP] =
            (s_animation_header*)farload::Far_Reload_Next(COLONY::NEWGAME_LBX, NEW_GAME_POST_WARP_PICTURE_ENTRY, MOX::screen_seg);
        RUSS::Mox_Update();

        anims[1] = (s_animation_header*)farload::Far_Reload_Next(COLONY::NEWGAME_LBX, (int32_t)index, MOX::screen_seg);
        anims[0] = (s_animation_header*)farload::Far_Reload_Next(COLONY::NEWGAME_LBX, (int32_t)index + 1, MOX::screen_seg);
        anims[2] = (s_animation_header*)farload::Far_Reload_Next(COLONY::NEWGAME_LBX, (int32_t)index + 2, MOX::screen_seg);
        anims[3] = (s_animation_header*)farload::Far_Reload_Next(COLONY::NEWGAME_LBX, (int32_t)index + 3, MOX::screen_seg);
        anims[4] = (s_animation_header*)farload::Far_Reload_Next(COLONY::NEWGAME_LBX, (int32_t)index + 4, MOX::screen_seg);
        anims[6] = (s_animation_header*)farload::Far_Reload_Next(COLONY::NEWGAME_LBX, (int32_t)index + 5, MOX::screen_seg);
        anims[5] = (s_animation_header*)farload::Far_Reload_Next(COLONY::NEWGAME_LBX, (int32_t)index + 6, MOX::screen_seg);

        RUSS::Mox_Update();
        Load_New_Game_Strings();
    }

    void __cdecl Draw_Pictures() {
        int16_t col = 0;
        int16_t x_offset = 0;
        do {
            int16_t row = 0;
            int16_t y_offset = 0;
            do {
                if ((row != 1) || (col < 2)) {
                    s_animation_header* anim = nullptr;

                    if ((col == 0) && (row == 0)) {
                        anim = difficulty_anims[diff_button_variable];
                    } else if ((col == 1) && (row == 0)) {
                        int16_t preview_idx = gal_size_button_variable;
                        if (preview_idx == NEW_GAME_GALAXY_SIZE_MAXIMUM) {
                            preview_idx = NEW_GAME_GALAXY_SIZE_HUGE;
                        }
                        anim = gal_size_anims[preview_idx];
                    } else if ((col == 2) && (row == 0)) {
                        anim = gal_age_anims[gal_age_button_variable];
                    } else if ((col == 0) && (row == 1)) {
                        anim = num_players_anims[opp_button_variable];
                    } else if ((col == 1) && (row == 1)) {
                        anim = tech_level_anims[civ_button_variable];
                    }

                    animate::Draw(
                        x_offset + g_x + 121,
                        g_y + y_offset + 119,
                        anim);
                }
                y_offset += 140;
                row++;
            } while (row < 2);

            x_offset += 155;
            col++;
        } while (col < 3);
    }

    void __cdecl Update_Fields() {
        FAKEBUTS::Clear_Click_Thru_Fields();
        fields::Clear_Fields();
        fonts::Set_Font_Style(3, &color_ken);

        const char* common_help_text = ESTRINGS::E_Strings(0x0C);
        int16_t gx = g_x;
        int16_t gy = g_y;

        // Difficulty
        int16_t difficulty_hidden_field = fields::Add_Hidden_Field(gx + 0x79, gy + 0x77, gx + 0xBC, gy + 0xB8, common_help_text, 0);
        fields[0] = FAKEBUTS::Add_Click_Thru_Field(gx + 0x69, gy + 0xCC, gx + 0xCD, gy + 0xE0, diff_button_strings, GAME_DIFFICULTY_COUNT, &diff_button_variable, 0, common_help_text, 0, difficulty_hidden_field);

        // Galaxy Size
        int16_t galaxy_size_hidden_field = fields::Add_Hidden_Field(gx + 0x114, gy + 0x77, gx + 0x157, gy + 0xB8, common_help_text, 0);
        fields[1] = FAKEBUTS::Add_Click_Thru_Field(gx + 0x105, gy + 0xCC, gx + 0x169, gy + 0xE0, gal_size_button_strings, NEW_GAME_GALAXY_SIZE_COUNT, &gal_size_button_variable, 0, common_help_text, 0, galaxy_size_hidden_field);

        // Galaxy Age
        int16_t galaxy_age_hidden_field = fields::Add_Hidden_Field(gx + 0x1AF, gy + 0x77, gx + 0x1F2, gy + 0xB8, common_help_text, 0);
        fields[2] = FAKEBUTS::Add_Click_Thru_Field(gx + 0x1A1, gy + 0xCC, gx + 0x205, gy + 0xE0, gal_age_button_strings, GALAXY_AGE_COUNT, &gal_age_button_variable, 0, common_help_text, 0, galaxy_age_hidden_field);

        // Opponents
        int16_t opponents_hidden_field = fields::Add_Hidden_Field(gx + 0x79, gy + 0x103, gx + 0xBC, gy + 0x144, common_help_text, 0);
        fields[3] = FAKEBUTS::Add_Click_Thru_Field(gx + 0x69, gy + 0x15D, gx + 0xCD, gy + 0x171, opp_button_strings, MAX_PLAYERS, &opp_button_variable, 0, common_help_text, 0, opponents_hidden_field);

        // Starting Civilization Level
        int16_t civilization_hidden_field = fields::Add_Hidden_Field(gx + 0x114, gy + 0x103, gx + 0x157, gy + 0x144, common_help_text, 0);
        fields[5] = FAKEBUTS::Add_Click_Thru_Field(gx + 0x105, gy + 0x15D, gx + 0x169, gy + 0x171, civ_button_strings, STARTING_CIVILIZATION_LEVEL_COUNT, &civ_button_variable, 0, common_help_text, 0, civilization_hidden_field);

        // Radio Buttons: Random Events, Xenon Attacks, Strategic Combat
        fields::Add_Radio_Button_Field(gx + 0x17C, gy + 0x127, ESTRINGS::E_Strings(0x0C), anims[0], &random_events_button_variable, ESTRINGS::E_Strings(0x0C), 0x29);
        fields::Add_Radio_Button_Field(gx + 0x17C, gy + 0x14A, ESTRINGS::E_Strings(0x0C), anims[2], &xenon_attacks_button_variable, ESTRINGS::E_Strings(0x0C), 0x29);
        fields::Add_Radio_Button_Field(gx + 0x17C, gy + 0x103, ESTRINGS::E_Strings(0x0C), anims[1], (int16_t*)&combat_button_variable, ESTRINGS::E_Strings(0x0C), 0x29);

        if (Show_Net_Game_Name()) {
            Ensure_Settings_Net_Game_Name();
            strlcpy(NETMOX::net.cur_game_name, MOX::settings.net_game_name, sizeof(NETMOX::net.cur_game_name));
            fields::Add_Continuous_String_Input_Field(gx + 0xE1, gy + 0x18A, 0xA8, 0x12, NETMOX::net.cur_game_name, 8, 0, 0, 0, 0);
        }

        // Accept and Cancel buttons
        fields[6] = fields::Add_Button_Field(gx + 0x64, gy + 0x182, ESTRINGS::E_Strings(0x0C), anims[3], ESTRINGS::E_Strings(0x0C), 0x28);
        fields[8] = fields::Add_Button_Field(gx + 0x1A2, gy + 0x183, ESTRINGS::E_Strings(0x0C), anims[4], ESTRINGS::E_Strings(0x0C), 0x28);
        fields[7] = fields::Add_Hot_Key(ESTRINGS::E_Strings(0x87));
        accept_hotkey = fields::Add_Hot_Key(" ");
    }

    void __cdecl Draw_Newgame_Screen() {
        fonts::Set_Font_Style_Shadow_Down(4, &color_ken);
        fonts::Set_Outline_Color(0, 0x40);
        animate::Draw(g_x, g_y, anims[6]);
        fonts::Set_Font_Style(2, &color_ken);
        video::Set_Page_Off();

        if (COLONY::full_draw != 0) {
            graphics::Fill(0, 0, 0x27F, 0x1DF, 0);
            animate::Draw(0, 0, background_seg);
            fonts::Set_Font_Style(3, &color_ken);
            video::Copy_Off_To_Back();
            COLONY::full_draw = 0;
        }

        FAKEBUTS::Draw_Click_Thru_Fields();
        Draw_Pictures();

        if (Show_Net_Game_Name()) {
            animate::Draw(g_x + 0xD9, g_y + 0x179, anims[5]);
        }

        fonts::Set_Font_Style(3, &color_ken);
        fonts::Set_Alias_Color(0x9A);
    }

    void Load_New_Game_Strings() {
        diff_button_strings[GAME_DIFFICULTY_TUTOR] = ESTRINGS::E_Strings(0x221 /* ESTR_TUTOR */);
        diff_button_strings[GAME_DIFFICULTY_EASY] = ESTRINGS::E_Strings(0x118 /* ESTR_EASY */);
        diff_button_strings[GAME_DIFFICULTY_AVERAGE] = ESTRINGS::E_Strings(0xbe /* ESTR_AVERAGE */);
        diff_button_strings[GAME_DIFFICULTY_HARD] = ESTRINGS::E_Strings(0x13e /* ESTR_HARD */);
        diff_button_strings[GAME_DIFFICULTY_IMPOSSIBLE] = ESTRINGS::E_Strings(0x148 /* ESTR_IMPOSSIBLE */);

        gal_size_button_strings[GALAXY_SIZE_SMALL] = ESTRINGS::E_Strings(0x1e0 /* ESTR_SMALL */);
        // Special case for Italian language
        if (MOX::settings.language == 4) {
            snprintf(localized_gal_size_label, sizeof(localized_gal_size_label), "Piccola");
            gal_size_button_strings[GALAXY_SIZE_SMALL] = localized_gal_size_label;
        }

        gal_size_button_strings[GALAXY_SIZE_MEDIUM] = ESTRINGS::E_Strings(0x173 /* ESTR_MEDIUM */);

        if (MOX::settings.language == 2) {
            char* medium_str = ESTRINGS::E_Strings(0x173 /* ESTR_MEDIUM */);
            snprintf(localized_gal_size_label, sizeof(localized_gal_size_label), "%sen", medium_str);
            gal_size_button_strings[GALAXY_SIZE_LARGE] = localized_gal_size_label;
        } else if (MOX::settings.language == 4) {
            snprintf(localized_gal_size_label, sizeof(localized_gal_size_label), "Media");
            gal_size_button_strings[GALAXY_SIZE_LARGE] = localized_gal_size_label;
        }

        gal_size_button_strings[GALAXY_SIZE_LARGE] = ESTRINGS::E_Strings(0x168);
        gal_size_button_strings[NEW_GAME_GALAXY_SIZE_HUGE] = ESTRINGS::E_Strings(0x143);
#if MAX_STARS > ORIGINAL_MAX_STARS
        gal_size_button_strings[NEW_GAME_GALAXY_SIZE_MAXIMUM] = const_cast<char*>("Maximum");
#endif

        gal_age_button_strings[GALAXY_AGE_YOUNG] = ESTRINGS::E_Strings(0x179);
        gal_age_button_strings[GALAXY_AGE_AVERAGE] = ESTRINGS::E_Strings(0xbe /* ESTR_AVERAGE */);
        gal_age_button_strings[GALAXY_AGE_OLD] = ESTRINGS::E_Strings(0x192);

        combat_button_strings[0] = ESTRINGS::E_Strings(0x1f7);
        combat_button_strings[1] = ESTRINGS::E_Strings(0x1f0);

        opp_button_strings[0] = ESTRINGS::E_Strings(0x94);
        opp_button_strings[1] = ESTRINGS::E_Strings(0x96);
        opp_button_strings[2] = ESTRINGS::E_Strings(0x98);
        opp_button_strings[3] = ESTRINGS::E_Strings(0x9a);
        opp_button_strings[4] = ESTRINGS::E_Strings(0x9c);
        opp_button_strings[5] = ESTRINGS::E_Strings(0x9d);
        opp_button_strings[6] = ESTRINGS::E_Strings(0x9e);
        switch (MOX::settings.language) {
            case 1:
                strlcpy(localized_random_label, "Willk#rlich", sizeof(localized_random_label));
                break;
            case 2:
                strlcpy(localized_random_label, "Hasard", sizeof(localized_random_label));
                break;
            case 3:
            case 4:
                strlcpy(localized_random_label, "Aleatorio", sizeof(localized_random_label));
                break;
            default:
                strlcpy(localized_random_label, "Random", sizeof(localized_random_label));
                break;
        }
        opp_button_strings[NEW_GAME_PLAYER_COUNT_RANDOM] = localized_random_label;

        civ_button_strings[0] = ESTRINGS::E_Strings(0x1ad);
        civ_button_strings[1] = ESTRINGS::E_Strings(0xbe /* ESTR_AVERAGE */);
        switch (MOX::settings.language) {
            case 3:
                strlcpy(localized_postwarp_label, "Poscurva", sizeof(localized_postwarp_label));
                break;
            case 4:
                strlcpy(localized_postwarp_label, "Postcurvatura", sizeof(localized_postwarp_label));
                break;
            default:
                strlcpy(localized_postwarp_label, "Post-warp", sizeof(localized_postwarp_label));
                break;
        }
        civ_button_strings[NEW_GAME_CIVILIZATION_POST_WARP] = localized_postwarp_label;
        civ_button_strings[NEW_GAME_CIVILIZATION_ADVANCED] = ESTRINGS::E_Strings(0xa9);
    }

    void Temps_To_Globals() {
        MOX::settings.game_difficulty = (uint8_t)diff_button_variable;
        MOX::settings.galaxy_size = Galaxy_Size_Setting_From_Button(gal_size_button_variable);
        MOX::settings.galaxy_age = (uint8_t)gal_age_button_variable;
        random_player_count = opp_button_variable == NEW_GAME_PLAYER_COUNT_RANDOM;
        MOX::settings.number_of_players = random_player_count
            ? NEW_GAME_RANDOM_PLAYER_COUNT
            : (uint8_t)opp_button_variable + 2;
        MOX::settings.starting_civilization_level =
            Civilization_Setting_From_Button(civ_button_variable);
        MOX::settings.strategic_combat_flag = (combat_button_variable == 0);
        MOX::settings.random_events = (int8_t)random_events_button_variable;
        MOX::settings.xenons_exist = (uint8_t)xenon_attacks_button_variable;
    }

} // namespace NEWGAME