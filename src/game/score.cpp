#include "pch.h"

namespace SCORE {
    s_hof hof;

    char default_names[10][20] = {
        "Moise",
        "Irma",
        "Justin",
        "Jalen",
        "Cereal",
        "Lan_Doan",
        "Cadfael",
        "J W R",
        "Ripping_Fang",
        "Chewy",
    };

    int16_t entry_to_flash;

    char difficulty_string[GAME_DIFFICULTY_COUNT][15];

    void __cdecl nullsub() {
        return;
    }

    int32_t __cdecl qsort_hof_scores_descending(const void* p1, const void* p2) {
        const int16_t* a = (const int16_t*)p1;
        const int16_t* b = (const int16_t*)p2;
        if (b[0] < a[0]) {
            return -1;
        }
        if (a[0] < b[0]) {
            return 1;
        }
        if (b[1] < a[1]) {
            return -1;
        }
        if (a[1] < b[1]) {
            return 1;
        }
        return 0;
    }

    int32_t __cdecl qsort_hof_scores_ascending(const void* a, const void* b) {
        const int16_t* p1 = (const int16_t*)a;
        const int16_t* p2 = (const int16_t*)b;

        if (p2[0] < p1[0]) {
            return 1;
        }
        if (p1[0] < p2[0]) {
            return -1;
        }
        if (p2[0] == p1[0]) {
            if (p2[1] < p1[1]) {
                return 1;
            }
            if (p1[1] < p2[1]) {
                return -1;
            }
        }
        return 0;
    }

    void __cdecl Load_Hall_Of_Fame_Screen() {
        hi = (s_hiscore*)allocate::Allocate_First_Block(MOX::screen_seg, sizeof(s_hiscore));
        memset(hi, 0, sizeof(s_hiscore));

        hi->background_anim = (s_animation_header*)farload::Far_Reload_Next(
            "SCORE.LBX",
            0xF,
            MOX::screen_seg);

        hi->mouse_cursor_anim = (s_animation_header*)farload::Far_Reload_Next(
            "SCORE.LBX",
            0x10,
            MOX::screen_seg);
    }

    void __cdecl Draw_Hi_Score_Screen() {
        if (SCORE::hi->is_first_draw != 0) {
            graphics::Fill(0, 0, 639, 479, 0);
            SCORE::hi->is_first_draw = 0;
        }

        animate::Draw(0, 0, SCORE::hi->main_background_anim);

        SCORE::Draw_Time_Score();
        SCORE::Draw_Population_Score();
        SCORE::Draw_Elimination_Score();
        SCORE::Draw_Captured_Colonies_Score();
        SCORE::Draw_Technology_Score();
        SCORE::Draw_Orion_Score();
        SCORE::Draw_Antares_Score();
        SCORE::Draw_Council_Score();
        SCORE::Draw_Total_Score();
    }

    int16_t __cdecl Do_Calculate_Hi_Score(int16_t player_idx) {
        int16_t final_score;

        allocate::Push_Block(MOX::global_data_seg);

        SCORE::hi = (s_hiscore*)allocate::Allocate_First_Block(MOX::global_data_seg, sizeof(s_hiscore));

        SCORE::Calculate_Hi_Scores(player_idx);

        final_score = SCORE::hi->score_final;

        allocate::Pop_Block(MOX::global_data_seg);

        return final_score;
    }

    void __cdecl End_Of_Game_Hi_Score(int16_t player_idx) {
        char buffer[500];
        int16_t current_score;
        int16_t max_hof_score = 0;
        int16_t rank_index = -1;
        bool cheated = false;

        current_score = Hi_Score_Screen(player_idx);
        fields::Assign_Auto_Function(SCORE::nullsub, 1);
        SCORE::hi = (s_hiscore*)allocate::Allocate_First_Block(MOX::screen_seg, sizeof(s_hiscore));

        for (int i = 0; i < 10; ++i) {
            if (hof.scores[i] > max_hof_score) {
                max_hof_score = hof.scores[i];
            }
        }

        Sort_HOF_Scores(SCORE::hi->sort_list);

        for (int i = 0; i < 10; ++i) {
            if (SCORE::hi->sort_list[i][0] <= current_score) {
                rank_index = SCORE::hi->sort_list[i][1];
            }
        }

        if (rank_index > -1) {
            if (MOX::player[player_idx].cheated != 0) {
                MISC::Get_LBX_Message(0x17, buffer);
                GENDRAW::Warning_Box(buffer);
                mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);
                cheated = true;
                rank_index = -1;
            } else {
                if (current_score >= max_hof_score) {
                    snprintf(buffer, sizeof(buffer), HAROLD::H_Message(0x27), MOX::player[player_idx].name);
                } else {
                    snprintf(buffer, sizeof(buffer), HAROLD::H_Message(0x28), MOX::player[player_idx].name);
                }
                GENDRAW::Warning_Box(buffer);
            }

            if (!cheated) {
                for (int i = 0; i < rank_index; ++i) {
                    strlcpy(hof.player_names[i], hof.player_names[i + 1], sizeof(hof.player_names[i]));
                    hof.scores[i] = hof.scores[i + 1];
                    hof.race_ids[i] = hof.race_ids[i + 1];
                    hof.difficulties[i] = hof.difficulties[i + 1];
                    strlcpy(hof.race_names[i], hof.race_names[i + 1], sizeof(hof.race_names[i]));
                }

                strlcpy(hof.player_names[rank_index], MOX::player[player_idx].name, sizeof(hof.player_names[rank_index]));
                hof.scores[rank_index] = current_score;
                hof.race_ids[rank_index] = (int16_t)(uint8_t)MOX::player[player_idx].race;
                hof.difficulties[rank_index] = MOX::settings.game_difficulty;
                strlcpy(hof.race_names[rank_index], MOX::player[player_idx].race_name, sizeof(hof.race_names[rank_index]));
            }

            if (rank_index > -1 && !cheated) {
                Write_HOF_File();
            }
        }

        Hall_Of_Fame_Screen(rank_index);
        MOX::current_screen = SCREEN_MAIN_MENU;
        MOX::previous_screen = SCREEN_HALL_OF_FAME;
    }

    void __cdecl Create_Default_HOF_File() {
        FILE* file = fopen_case("HOF.M2", "wb");

        memset(&hof, 0, sizeof(s_hof));

        hof.version = 0x82;

        int16_t indices[STOCK_RACE_COUNT];
        for (int16_t i = 0; i < STOCK_RACE_COUNT; ++i) {
            indices[i] = i;
        }

        for (int i = 0; i < 10; ++i) {
            int16_t idx1 = (int16_t)game_random::Random(11) + 1;
            int16_t idx2 = (int16_t)game_random::Random(11) + 1;
            if (idx2 != idx1) {
                int16_t swapped_index = indices[idx1];
                indices[idx1] = indices[idx2];
                indices[idx2] = swapped_index;
            }
        }

        for (int i = 0; i < 8; ++i) {
            int16_t idx1 = (int16_t)game_random::Random(8) + 1;
            int16_t idx2 = (int16_t)game_random::Random(8) + 1;
            if (idx2 != idx1) {
                char temp_name[20];
                strlcpy(temp_name, default_names[idx1], sizeof(temp_name));
                strlcpy(default_names[idx1], default_names[idx2], sizeof(default_names[idx1]));
                strlcpy(default_names[idx2], temp_name, sizeof(default_names[idx2]));
            }
        }

        int16_t score_base = 1500;
        int16_t rand_range = 10;

        for (int i = 0; i < 10; ++i) {
            int16_t r1 = (int16_t)game_random::Random(rand_range + 1);
            int16_t r2 = (int16_t)game_random::Random(5);

            hof.scores[i] = score_base + (r2 * 10) + ((r1 - 1) * 15);
            hof.difficulties[i] = (int8_t)(i % GAME_DIFFICULTY_COUNT);
            strlcpy(hof.player_names[i], default_names[i], sizeof(hof.player_names[i]));

            int16_t race_idx = indices[i];
            if (i < 2) {
                strlcpy(hof.race_names[i], MOX::race_names[STOCK_RACE_HUMAN], sizeof(hof.race_names[i]));
            } else {
                strlcpy(hof.race_names[i], MOX::race_names[race_idx], sizeof(hof.race_names[i]));
            }

            score_base -= 125;
            rand_range--;
        }

        if (file) {
            fwrite(&hof, sizeof(s_hof), 1, file);
            fclose(file);
        }
    }

    int8_t __cdecl Read_HOF_File() {
        FILE* file;
        int16_t sort_buffer[10][2];

        file = fopen_case("HOF.M2", "rb");
        if (file == nullptr) {
            return 0;
        }

        s_hof loaded_hof;
        const bool loaded = fread(&loaded_hof, sizeof(loaded_hof), 1, file) == 1;
        fclose(file);
        if (!loaded) {
            return 0;
        }

        hof = loaded_hof;

        Sort_HOF_Scores(sort_buffer);
        Move_Sorted_HOF_Data(sort_buffer);

        return 1;
    }

    int16_t __cdecl HOF_Version_Number() {
        if (!Read_HOF_File()) {
            return 0;
        }

        return SCORE::hof.version;
    }

    void __cdecl Hall_Of_Fame_Screen(int16_t entry_to_flash_idx) {
        int16_t input_val;
        int16_t scan_val;
        bool exit_screen = false;

        SCORE::entry_to_flash = entry_to_flash_idx;

        strlcpy(SCORE::difficulty_string[GAME_DIFFICULTY_TUTOR], ESTRINGS::E_Strings(0x221), sizeof(SCORE::difficulty_string[GAME_DIFFICULTY_TUTOR]));
        strlcpy(SCORE::difficulty_string[GAME_DIFFICULTY_EASY], ESTRINGS::E_Strings(0x118), sizeof(SCORE::difficulty_string[GAME_DIFFICULTY_EASY]));
        strlcpy(SCORE::difficulty_string[GAME_DIFFICULTY_AVERAGE], ESTRINGS::E_Strings(0xbe), sizeof(SCORE::difficulty_string[GAME_DIFFICULTY_AVERAGE]));
        strlcpy(SCORE::difficulty_string[GAME_DIFFICULTY_HARD], ESTRINGS::E_Strings(0x13e), sizeof(SCORE::difficulty_string[GAME_DIFFICULTY_HARD]));
        strlcpy(SCORE::difficulty_string[GAME_DIFFICULTY_IMPOSSIBLE], ESTRINGS::E_Strings(0x148), sizeof(SCORE::difficulty_string[GAME_DIFFICULTY_IMPOSSIBLE]));

        if (SCORE::HOF_Version_Number() != 0x82) {
            SCORE::Create_Default_HOF_File();
        }

        fields::Disable_Cancel();
        fields::Deactivate_Help_List();
        SCORE::Read_HOF_File();
        fields::Assign_Auto_Function(SCORE::Draw_Hall_Of_Fame_Screen, 1);
        SCORE::Load_Hall_Of_Fame_Screen();
        SCORE::Init_Hall_Of_Fame_Screen();

        SCORE::hi->current_input_state = -1000;

        do {
            SCORE::hi->prev_input_state = SCORE::hi->current_input_state;
            input_val = fields::Get_Input();
            scan_val = fields::Scan_Input();
            SCORE::hi->current_input_state = scan_val;

            if (SCORE::hi->current_input_state != SCORE::hi->prev_input_state) {
                SCORE::hi->highlighted_entry_idx = -1;
            }

            if (SCORE::hi->screen_field_handle == input_val) {
                exit_screen = true;
            } else if (SCORE::hi->exit_hotkey_handle == input_val) {
                SCORE::Create_Default_HOF_File();
                SCORE::hi->timer_ticks = 0;
                SCORE::entry_to_flash = -1;
                SCORE::Init_Hall_Of_Fame_Screen();
            }

            for (int16_t i = 0; i < 10; ++i) {
                if (SCORE::hi->entry_field_handles[i] == SCORE::hi->current_input_state &&
                    SCORE::hi->prev_input_state == SCORE::hi->current_input_state &&
                    SCORE::hi->timer_ticks > 1000) {
                    SCORE::hi->highlighted_entry_idx = (int8_t)i;
                }
            }

            if (!exit_screen) {
                video::Set_Page_Off();
                SCORE::Draw_Hall_Of_Fame_Screen();
                RUSS::Mox_Sync_Update();
            }

        } while (!exit_screen);

        ERIC::Fast_Fade_Out();
        MOX::current_screen = MOX::return_screen;
        fields::Clear_Fields();
        fields::Deactivate_Auto_Function();
    }

    void __cdecl Draw_Hall_Of_Fame_Screen() {
        int16_t min_font_style = 4;
        int16_t min_race_font_spacing = 2;
        int16_t min_diff_font_spacing = 2;

        for (int16_t i = 0; i < 10; ++i) {
            int16_t record_idx = hi->sort_list[i][1];

            MISC::Set_Fitted_Font_Style(hof.race_names[record_idx], &hi->colors, 4, 2, -1, 67, 1);
            int16_t style = MISC::Get_Font_Style();
            if (style < min_font_style) {
                min_font_style = style;
            }
            int16_t spacing = MISC::Get_Font_Spacing();
            if (spacing < min_race_font_spacing) {
                min_race_font_spacing = spacing;
            }

            MISC::Set_Fitted_Font_Style(difficulty_string[hof.difficulties[record_idx]], &hi->colors, 4, 2, -1, 86, 1);
            style = MISC::Get_Font_Style();
            if (style < min_font_style) {
                min_font_style = style;
            }
            spacing = MISC::Get_Font_Spacing();
            if (spacing < min_diff_font_spacing) {
                min_diff_font_spacing = spacing;
            }
        }

        if (hi->is_first_draw != 0 || hi->highlighted_entry_idx < -2) {
            hi->highlighted_entry_idx = -1;
            graphics::Fill(0, 0, 639, 479, 0);
            animate::Draw(0, 0, hi->background_anim);
            MOX::FULL_DRAW_SCREEN_FLAG = 0;
        }

        int16_t draw_y = 135;
        for (int16_t i = 0; i < 10; ++i) {
            int16_t record_idx = hi->sort_list[i][1];
            int8_t flash_offset = 0;
            if (entry_to_flash == record_idx) {
                flash_offset = hi->flash_color_offset;
            }

            hi->temp_colors.colors[0] = hi->colors.colors[0];
            for (int j = 1; j < 8; ++j) {
                hi->temp_colors.colors[j] = hi->colors.colors[j] + flash_offset;
            }

            fonts::Set_Font_Style(4, &hi->temp_colors);
            fonts::Set_Font_Spacing(1);
            graphics::Set_Window(164, draw_y, 281, draw_y + 20);
            video::Clip_On();
            fonts::Print(164, draw_y, hof.player_names[record_idx]);

            if (MOX::settings.language != 0) {
                fonts::Set_Font_Style(min_font_style, &hi->temp_colors);
                fonts::Set_Font_Spacing(min_race_font_spacing);
            }
            graphics::Set_Window(292, draw_y, 354, draw_y + 20);
            video::Clip_On();
            int16_t font_h = fonts::Get_Font_Height();
            fonts::Print(292, (draw_y - font_h) + 16, hof.race_names[record_idx]);

            if (MOX::settings.language != 0) {
                fonts::Set_Font_Style(min_font_style, &hi->temp_colors);
                fonts::Set_Font_Spacing(min_diff_font_spacing);
            }
            graphics::Set_Window(364, draw_y, 449, draw_y + 20);
            video::Clip_On();
            font_h = fonts::Get_Font_Height();
            fonts::Print(364, (draw_y - font_h) + 16, difficulty_string[hof.difficulties[record_idx]]);

            if (MOX::settings.language != 0) {
                fonts::Set_Font_Style(4, &hi->temp_colors);
                fonts::Set_Font_Spacing(1);
            }
            graphics::Set_Window(458, draw_y, 500, draw_y + 20);
            video::Clip_On();
            fonts::Print_Integer_Right(500, draw_y, hof.scores[record_idx]);

            draw_y += 29;
        }

        graphics::Reset_Window();
        video::Clip_Off();

        if (hi->is_first_draw == 0) {
            graphics::Set_Window(0, 0, 640, 480);
            video::Clip_On();
            int16_t mx = mouse::Pointer_X();
            int16_t my = mouse::Pointer_Y();
            if (mx > 636) mx = 636;
            if (my > 476) my = 476;
            animate::Draw(mx, my, hi->mouse_cursor_anim);
            graphics::Reset_Window();
            video::Clip_Off();
        }

        if (entry_to_flash > -1 && entry_to_flash < 10) {
            hi->flash_timer_ticks++;
            if ((uint8_t)hi->flash_timer_ticks % 6 == 0) {
                hi->flash_color_offset += hi->flash_direction;
                if (hi->flash_color_offset == -1) {
                    hi->flash_direction = 1;
                    hi->flash_timer_ticks = 0;
                } else if (hi->flash_color_offset == 6) {
                    hi->flash_direction = -1;
                    hi->flash_timer_ticks = 0;
                }
            }
        }

        hi->is_first_draw = 0;
    }

    int16_t __cdecl Hi_Score_Screen(int16_t player_idx) {
        bool finished = false;

        fields::Deactivate_Help_List();

        if (SCORE::Read_HOF_File() == 0) {
            SCORE::Create_Default_HOF_File();
        }

        fields::Assign_Auto_Function(SCORE::Draw_Hi_Score_Screen, 1);
        fields::Clear_Fields();
        SCORE::Load_Hi_Score_Screen();

        SCORE::hi->player_idx = (int8_t)player_idx;
        SCORE::Calculate_Hi_Scores(player_idx);
        SCORE::hi->highlighted_entry_idx = -1;

        SCORE::Get_Hi_Score_Colors();
        SCORE::Add_Hi_Score_Screen_Fields();
        SCORE::Init_Hi_Score(player_idx);
        SCORE::Build_Captured_Colonists_Array(player_idx);

        MISC::Fade_Into_Screen(SCORE::Draw_Hi_Score_Screen, &SCORE::hi->is_first_draw, 12);
        remap::Update_Glass_Remap_Colors();

        while (true) {
            int16_t input = fields::Get_Input();
            if (SCORE::hi->screen_field_handle == input) {
                finished = true;
            }

            if (finished) {
                break;
            }

            video::Set_Page_Off();
            SCORE::Draw_Hi_Score_Screen();
            RUSS::Mox_Sync_Update();
        }

        video::Set_Page_Off();
        SCORE::Draw_Hi_Score_Screen();
        video::Copy_Off_To_Back();
        video::Set_Refresh_Stencil();

        MOX::current_screen = MOX::return_screen;
        fields::Clear_Fields();
        fields::Deactivate_Auto_Function();

        return SCORE::hi->score_final;
    }

    void __cdecl Init_Hall_Of_Fame_Screen() {
        fields::Clear_Fields();

        hi->highlighted_entry_idx = -2;
        hi->is_first_draw = 1;

        for (int i = 0; i < 10; ++i) {
            hi->sort_list[i][0] = hof.scores[i];
            hi->sort_list[i][1] = (int16_t)i;
        }

        qsort(hi->sort_list, 10, sizeof(hi->sort_list[0]), qsort_hof_scores_descending);

        int16_t y = 131;
        for (int i = 0; i < 10; ++i) {
            int16_t handle = fields::Add_Hidden_Field(139, y, 155, y + 22, "", 0);
            hi->entry_field_handles[i] = handle;
            y += 29;
        }

        hi->screen_field_handle = fields::Add_Hidden_Field(0, 0, 639, 479, "\x1B", 41);

        Get_Hall_Of_Fame_Colors();

        hi->exit_hotkey_handle = fields::Add_Multi_Hot_Key_Field("C");

        fields::Disable_Draw_All_Fields();
        mouse::Set_Mouse_List(&MOX::mouse_list_none, 1);
        ERIC::Fast_Fade_Out();
        video::Set_Page_Off();
        graphics::Fill(0, 0, 639, 479, 0);
        RUSS::Mox_Sync_Update();

        fonts::Load_Palette(12, 0, 255);
        palette::Set_Palette_Changes(0, 255);
        palette::Refresh_Palette();
        palstore::Darken_Palette(100);

        strlcpy(difficulty_string[GAME_DIFFICULTY_TUTOR], ESTRINGS::E_Strings(545), sizeof(difficulty_string[GAME_DIFFICULTY_TUTOR]));
        strlcpy(difficulty_string[GAME_DIFFICULTY_EASY], ESTRINGS::E_Strings(280), sizeof(difficulty_string[GAME_DIFFICULTY_EASY]));
        strlcpy(difficulty_string[GAME_DIFFICULTY_AVERAGE], ESTRINGS::E_Strings(190), sizeof(difficulty_string[GAME_DIFFICULTY_AVERAGE]));
        strlcpy(difficulty_string[GAME_DIFFICULTY_HARD], ESTRINGS::E_Strings(318), sizeof(difficulty_string[GAME_DIFFICULTY_HARD]));
        strlcpy(difficulty_string[GAME_DIFFICULTY_IMPOSSIBLE], ESTRINGS::E_Strings(328), sizeof(difficulty_string[GAME_DIFFICULTY_IMPOSSIBLE]));

        video::Set_Page_Off();
        Draw_Hall_Of_Fame_Screen();

        palette::Clear_Palette_Changes(0, 255);
        ERIC::Fast_Fade_In();
        video::Copy_Off_To_Back();
        RUSS::Mox_Sync_Update();
        fields::Enable_Draw_All_Fields();

        animate::Draw_Palette(hi->background_anim);
    }

    void __cdecl Sort_HOF_Scores(int16_t sort_list[10][2]) {
        for (int16_t i = 0; i < 10; ++i) {
            sort_list[i][0] = hof.scores[i];
            sort_list[i][1] = i;
        }
        qsort(sort_list, 10, sizeof(sort_list[0]), qsort_hof_scores_ascending);
    }

    void __cdecl Move_Sorted_HOF_Data(int16_t sort_list[10][2]) {
        s_hof local_hof;

        local_hof.version = hof.version;

        for (int16_t i = 0; i < 10; ++i) {
            int16_t idx = sort_list[i][1];

            strlcpy(local_hof.player_names[i], hof.player_names[idx], sizeof(local_hof.player_names[i]));
            local_hof.scores[i] = hof.scores[idx];
            local_hof.race_ids[i] = hof.race_ids[idx];
            local_hof.difficulties[i] = hof.difficulties[idx];
            strlcpy(local_hof.race_names[i], hof.race_names[idx], sizeof(local_hof.race_names[i]));
        }

        hof = local_hof;
    }

    void __cdecl Write_HOF_File() {
        FILE* file;

        file = fopen_case("HOF.M2", "wb");
        if (file != nullptr) {
            fwrite(&hof, sizeof(s_hof), 1, file);
            fclose(file);
        }
    }

    void __cdecl Draw_Reg_Pop() {
        SCORE::Draw_Pop(0, 4, SCORE::hi->reg_pop);
    }

    void __cdecl Draw_Population_Score() {
        char buffer[80];
        int raw_population = (int)SCORE::hi->raw_population;
        char* format = (char*)HAROLD::H_Message(0x15C /* HESTR_15C_D_POPULATION */);

        snprintf(buffer, sizeof(buffer), format, raw_population);

        SCORE::Draw_Generic_Score(
            SCORE::hi->score_population,
            buffer,
            SCORE::Draw_Reg_Pop,
            0x168);
    }

    void __cdecl Draw_Elimination_Score() {
        char message_buffer[80];
        int16_t num_defeated = SCORE::hi->num_opponents_defeated;

        if (num_defeated == 0) {
            const char* msg = HAROLD::H_Message(0x158 /* HESTR_158_PLAYERS_ELIMINATED */);
            strlcpy(message_buffer, msg, sizeof(message_buffer));
        } else {
            int16_t msg_id = (num_defeated == 1) ? 0x159 : 0x15a;
            const char* fmt = HAROLD::H_Message(msg_id);
            snprintf(message_buffer, sizeof(message_buffer), fmt, (int32_t)num_defeated);
        }

        SCORE::Draw_Generic_Score(SCORE::hi->score_elimination, message_buffer, SCORE::Draw_Eliminated_Race_Pictures, 0x168);
    }

    void __cdecl Draw_Captured_Colonies_Score(void) {
        char buffer[80];
        int16_t message_id;
        const char* format;

        if (SCORE::hi->raw_captured_colonies == 1) {
            message_id = 0x154 /* 340 */;
        } else {
            message_id = 0x155 /* 341 */;
        }

        format = HAROLD::H_Message(message_id);
        snprintf(buffer, sizeof(buffer), format, (int32_t)SCORE::hi->raw_captured_colonies);

        SCORE::Draw_Generic_Score(SCORE::hi->score_captured_colonists, buffer, SCORE::Draw_Cap_Pop, 0x168 /* 360 */);
    }

    void __cdecl Draw_Cap_Pop() {
        SCORE::Draw_Pop(1, 2, SCORE::hi->raw_captured_population);
    }

    void __cdecl Draw_Time_Score() {
        char buffer[80];
        int16_t score = SCORE::hi->score_time;
        int16_t stardate_calc = (int16_t)((uint16_t)MOX::stardate + 30536 /* 0x7748 */);

        fonts::Set_Font_Style(4, &SCORE::hi->colors);
        fonts::Set_Font_Spacing(1);
        int16_t font_height = fonts::Get_Font_Height();

        const char* format = HAROLD::H_Message(350 /* 0x15E */);
        snprintf(buffer, sizeof(buffer), format, (int32_t)stardate_calc);

        SCORE::Print_Score_String(140 /* 0x8C */, 139 /* 0x8B */, score, buffer);

        // next_draw_y update: font_height + 141 (0x8D) + 5 (BX calculation)
        SCORE::hi->next_draw_y = font_height + 146 /* 0x92 */;
    }

    void __cdecl Draw_Eliminated_Race_Pictures(void) {
        int8_t player_idx;
        int16_t base_y;
        int16_t current_x;
        int16_t icon_y;
        bool any_drawn;
        int16_t i;
        int16_t icon_width;
        int16_t icon_height;
        char buffer[40];

        player_idx = SCORE::hi->player_idx;
        base_y = SCORE::hi->next_draw_y;
        current_x = 0x8e;
        icon_y = base_y + 2;
        any_drawn = false;
        i = 0;

        if (MOX::NUM_PLAYERS > 0) {
            do {
                uint16_t defeated_score = MOX::player[player_idx].player_defeated[i];
                if (defeated_score != 0) {
                    uint8_t race = (uint8_t)MOX::player[i].race;
                    s_animation_header* icon_anim = SCORE::hi->race_icon_anims[race];

                    icon_width = animate::Get_Width(icon_anim);
                    icon_height = animate::Get_Height(icon_anim);
                    any_drawn = true;

                    animate::Draw(current_x, icon_y, icon_anim);
                    graphics::Set_Window(current_x, icon_y, current_x + icon_width, icon_height + icon_y);
                    video::Clip_On();

                    fonts::Set_Font_Style_Outline(1, &SCORE::hi->colors);
                    fonts::Set_Outline_Color(2, 1);
                    fonts::Set_Font_Spacing(1);

                    snprintf(buffer, sizeof(buffer), "%d.%d", defeated_score / 10, defeated_score % 10);

                    int16_t centerX = current_x + (icon_width / 2);
                    fonts::Print_Centered(centerX, base_y + 3, buffer);

                    fonts::Set_Font_Style_Outline(1, &SCORE::hi->colors);
                    fonts::Set_Outline_Color(2, 1);

                    int16_t name_width = fonts::Get_String_Width(MOX::player[i].name);
                    int16_t font_height = fonts::Get_Font_Height();
                    int16_t name_y = (icon_height - font_height) + icon_y - 1;

                    if (icon_width < name_width) {
                        fonts::Print(current_x + 1, name_y, MOX::player[i].name);
                    } else {
                        fonts::Print_Centered(centerX, name_y, MOX::player[i].name);
                    }

                    graphics::Reset_Window();
                    video::Clip_Off();

                    animate::Draw(current_x - 1, base_y + 1, SCORE::hi->race_icon_frame_anim);

                    // Draw background/border boxes
                    graphics::Box(current_x - 2, base_y, icon_width + 3, icon_height + 3, 1, 0x4b /* 'K' */);
                    graphics::Box(current_x - 1, base_y + 1, icon_width + 1, icon_height + 1, 1, 0x0f);

                    current_x = current_x + icon_width + 4;
                }
                i++;
            } while (i < MOX::NUM_PLAYERS);
        }

        if (any_drawn) {
            SCORE::hi->next_draw_y += icon_height + 4;
        }
    }

    void __cdecl Draw_Technology_Score() {
        char* label = (char*)HAROLD::H_Message(349 /* HESTR_15D_TECHNOLOGY */);
        SCORE::Draw_Generic_Score(SCORE::hi->score_technology, label, nullptr, 360);
    }

    void __cdecl Draw_Orion_Score() {
        char* label = (char*)HAROLD::H_Message(0x15B /* HESTR_15B_CAPTURED_ORION */);
        Draw_Generic_Score(hi->score_orion, label, nullptr, 0);
    }

    void __cdecl Draw_Antares_Score() {
        char* message = (char*)HAROLD::H_Message(0x153);
        Draw_Generic_Score(hi->score_antares, message, nullptr, 0);
    }

    void __cdecl Draw_Council_Score(void) {
        int16_t council_score = SCORE::hi->score_council;

        if (council_score > 0) {
            const char* message = HAROLD::H_Message(342 /* HESTR_156_WON_COUNCIL_VOTE */);
            SCORE::Draw_Generic_Score(council_score, (char*)message, 0, 0);
        }
    }

    void __cdecl Draw_Total_Score() {
        int16_t next_draw_y = hi->next_draw_y;
        fonts::Set_Font_Style(4, &hi->colors);
        fonts::Set_Font_Spacing(1);

        char label_buffer[80];
        strlcpy(label_buffer, HAROLD::H_Message(0x157), sizeof(label_buffer));
        int16_t label_width = fonts::Get_String_Width(label_buffer);

        char score_buffer[80];
        if (hi->score_raw_subtotal == hi->score_final) {
            snprintf(score_buffer, sizeof(score_buffer), "%d", (int)hi->score_final);
        } else {
            char modifier_str[16];
            int16_t modifier = hi->score_modifier_pct;
            if (modifier < 100) {
                snprintf(modifier_str, sizeof(modifier_str), ".%d", (int)modifier);
            } else {
                snprintf(modifier_str, sizeof(modifier_str), "%d.%d", (int)(modifier / 100), (int)(modifier % 100));
            }
            snprintf(score_buffer, sizeof(score_buffer), "(%d x %s) %d", (int)hi->score_raw_subtotal, modifier_str, (int)hi->score_final);
        }

        int16_t score_width = fonts::Get_String_Width(score_buffer);
        int16_t line_x_start = 500 - (score_width + 2);

        line::Line(line_x_start, next_draw_y + 6, 498 /* 0x1f2 */, next_draw_y + 6, 0xD1);
        line::Line(line_x_start, next_draw_y + 7, 498 /* 0x1f2 */, next_draw_y + 7, 0xD2);
        line::Line(line_x_start, next_draw_y + 8, 498 /* 0x1f2 */, next_draw_y + 8, 0xD1);

        fonts::Get_Font_Height();
        s_colors* dot_colors_ptr = MISC::Font_Colors2(4, 0xD0, 0xD1);
        s_colors dot_colors = *dot_colors_ptr;

        char dot_str[4];
        strlcpy(dot_str, ".", sizeof(dot_str));
        int16_t dot_width = fonts::Get_String_Width(dot_str);

        int16_t y = next_draw_y + 10;
        fonts::Set_Font_Style(4, &hi->colors);
        fonts::Set_Font_Spacing(1);
        fonts::Print(144 /* 0x90 */, y, label_buffer);

        int16_t current_x = label_width + dot_width + 144 /* 0x90 */;
        fonts::Set_Font_Style(4, &dot_colors);
        fonts::Set_Font_Spacing(1);

        while (current_x < line_x_start) {
            fonts::Print(current_x, y, dot_str);
            current_x += dot_width;
        }

        fonts::Set_Font_Style(4, &hi->colors);
        fonts::Set_Font_Spacing(1);
        fonts::Print(current_x, y, score_buffer);
    }

    void __cdecl Print_Score_String(int16_t x, int16_t y, int16_t score, const char* label) {
        s_colors dot_colors;
        char dot_str[4];
        char label_buf[84];
        char score_buf[84];
        int16_t dot_width;
        int16_t label_width;
        int16_t score_width;
        s_colors* palette_colors;

        palette_colors = MISC::Font_Colors2(4, 0xD0, 0xD1);
        dot_colors = *palette_colors;

        // Copy dot string "."
        strlcpy(dot_str, ".", sizeof(dot_str));
        dot_width = fonts::Get_String_Width(dot_str);

        // Copy label string
        strlcpy(label_buf, label, sizeof(label_buf));
        label_width = fonts::Get_String_Width(label_buf);

        // Format score string
        snprintf(score_buf, sizeof(score_buf), "%d", (int)score);
        score_width = fonts::Get_String_Width(score_buf) + 3;

        // Print label
        fonts::Set_Font_Style(4, &SCORE::hi->colors);
        fonts::Set_Font_Spacing(1);
        int16_t print_y = y + 4;
        fonts::Print(x + 4, print_y, label_buf);

        // Print dots between label and score
        fonts::Set_Font_Style(4, &dot_colors);
        fonts::Set_Font_Spacing(1);

        int16_t limit_x = 499 /* 0x1F3 */ - score_width;
        int16_t current_x = x + label_width + dot_width + 4;

        while (current_x < limit_x) {
            fonts::Print(current_x, print_y, dot_str);
            current_x += dot_width;
        }

        // Print score value aligned to the right
        fonts::Set_Font_Style(4, &SCORE::hi->colors);
        fonts::Set_Font_Spacing(1);
        fonts::Print_Right(499 /* 0x1F3 */, print_y, score_buf);
    }

    void __cdecl Draw_Generic_Score(int16_t value, const char* label, void(__cdecl* callback)(), int width) {
        int16_t current_y = SCORE::hi->next_draw_y;

        fonts::Set_Font_Style(4, &SCORE::hi->colors);
        fonts::Set_Font_Spacing(1);

        int16_t font_height = fonts::Get_Font_Height();

        SCORE::Print_Score_String(140, current_y, value, label);

        SCORE::hi->next_draw_y = font_height + current_y + 2;

        if (callback != nullptr) {
            callback();
        }
    }

    void __cdecl Get_Hall_Of_Fame_Colors() {
        s_colors* colors;

        MOX::mox_font_colors_offset = 0xf0;

        colors = MISC::Font_Colors2(0x4, 0xef, 0xf4);

        // Copy the 8-byte color structure
        memcpy(&SCORE::hi->colors, colors, sizeof(s_colors));

        SCORE::hi->flash_color_offset = 3;
        SCORE::hi->flash_direction = 1;
        SCORE::hi->flash_timer_ticks = 0;
    }

    void __cdecl Add_Hi_Score_Screen_Fields() {
        hi->screen_field_handle = fields::Add_Hidden_Field(0, 0, 639, 479, "\x1B", 41);
    }

    void __cdecl Init_Hi_Score(int16_t player_idx) {
        Get_Total_Population(player_idx);
        uint32_t saved_seed = game_random::Get_Random_Seed();
        Set_Hi_Score_Pop_Icon_Ids(player_idx);
        game_random::Set_Random_Seed(saved_seed);
    }

    void __cdecl Set_Hi_Score_Pop_Icon_Ids(int16_t player_idx) {
        int16_t race_counts[14];
        bool limit_reached = false;
        int16_t collected_count = 0;

        int16_t total_pop_limit = SCORE::Get_Total_Population(player_idx);
        memset(race_counts, 0, sizeof(race_counts));

        allocate::Push_Block(MOX::screen_seg);

        uint32_t alloc_size = total_pop_limit * sizeof(s_hiscore_tmp_pop);
        s_hiscore_tmp_pop* temp_buffer = (s_hiscore_tmp_pop*)allocate::Allocate_Next_Block(MOX::screen_seg, alloc_size);
        memset(temp_buffer, 0, alloc_size);

        for (int16_t col_idx = 0; col_idx < MOX::NUM_COLONIES; ++col_idx) {
            if (limit_reached) break;

            if (MOX::colony[col_idx].owner == player_idx) {
                for (int16_t pop_idx = 0; pop_idx < MOX::colony[col_idx].n_pops; ++pop_idx) {
                    if (limit_reached) break;

                    int16_t effective_player_idx = COLONY::Get_Effective_Pop_Player(col_idx, pop_idx);
                    if (effective_player_idx < 0 || effective_player_idx >= MAX_PLAYERS) {
                        continue;
                    }

                    int16_t race_id = (int16_t)(uint8_t)MOX::player[effective_player_idx].race;

                    if (race_id < 14) {
                        if (collected_count == total_pop_limit) {
                            limit_reached = true;
                        } else {
                            race_counts[race_id]++;
                            temp_buffer[collected_count].colony_idx = col_idx;
                            temp_buffer[collected_count].race_idx = (uint8_t)race_id;
                            temp_buffer[collected_count].pop_idx = (uint8_t)pop_idx;
                            collected_count++;
                        }
                    }
                }
            }
        }

        int16_t population_limit = total_pop_limit;

        if (population_limit > 0) {
            for (int16_t i = population_limit; i > 0; --i) {
                int16_t idx1 = (int16_t)game_random::Random(population_limit) - 1;
                int16_t idx2 = (int16_t)game_random::Random(population_limit) - 1;

                s_hiscore_tmp_pop swap_tmp = temp_buffer[idx1];
                temp_buffer[idx1] = temp_buffer[idx2];
                temp_buffer[idx2] = swap_tmp;
            }
        }

        for (int16_t race_id = 0; race_id < 14; ++race_id) {
            if (race_counts[race_id] > 0) {
                // Calculation: round( (race_counts / final_count) * 16 )
                int32_t num_to_draw = ((race_counts[race_id] * 160) / population_limit + 5) / 10;

                // Enforce maximum of 16 icons total in high score report
                if (SCORE::hi->reg_pop + num_to_draw > 16 && num_to_draw > 1) {
                    num_to_draw = 16 - SCORE::hi->reg_pop;
                }

                for (int16_t j = 0; j < (int16_t)num_to_draw; ++j) {
                    if (SCORE::hi->reg_pop >= 16) break;

                    auto& pop_unit = SCORE::hi->pop_units[SCORE::hi->reg_pop];
                    int16_t colony_idx = pop_unit.race_id;
                    int16_t pop_idx = pop_unit.icon_id;

                    SCORE::Get_From_Tmp_Pop(
                        temp_buffer,
                        race_id,
                        population_limit,
                        &colony_idx,
                        &pop_idx);

                    pop_unit.race_id = colony_idx;
                    pop_unit.icon_id = pop_idx;

                    SCORE::hi->reg_pop++;
                }
            }
        }

        allocate::Pop_Block(MOX::screen_seg);
    }

    int __cdecl ascending_qsort_uchar(const void* a, const void* b) {
        return (int)(*(const uint8_t*)a) - (int)(*(const uint8_t*)b);
    }

    void __cdecl Build_Captured_Colonists_Array(int16_t player_idx) {
        uint32_t original_seed;
        uint16_t weights[MAX_PLAYERS];
        int16_t captured_count;
        int16_t colonies;

        captured_count = 0;
        original_seed = game_random::Get_Random_Seed();
        memset(weights, 0, sizeof(weights));

        for (int16_t i = 0; i < MAX_PLAYERS; ++i) {
            if (i != player_idx && MOX::player[player_idx].first_contact[i] == 1) {
                weights[i] = 2;
            }

            if (MOX::player[player_idx].player_defeated[i] != 0) {
                weights[i] = 5;
            }
        }

        colonies = SCORE::hi->raw_captured_colonies;
        if (colonies > 16) {
            colonies = 16;
        }

        SCORE::hi->raw_captured_population = colonies;

        if (SCORE::hi->raw_captured_population > 0) {
            for (int16_t i = 0; i < SCORE::hi->raw_captured_population; ++i) {
                int16_t choice = RUSS::Get_Weighted_Choice_Short(weights, MAX_PLAYERS);
                if (choice >= 0 && choice < MOX::NUM_PLAYERS && choice != player_idx) {
                    captured_count++;
                    SCORE::hi->captured_pop_race_ids[i] = MOX::player[choice].race;
                }
            }
        }

        qsort(SCORE::hi->captured_pop_race_ids, 16, sizeof(SCORE::hi->captured_pop_race_ids[0]), SCORE::ascending_qsort_uchar);

        SCORE::hi->raw_captured_population = captured_count;
        game_random::Set_Random_Seed(original_seed);
    }

    void __cdecl Load_Hi_Score_Screen() {
        uint32_t bitmap_size;
        int16_t lbx_index;

        hi = (s_hiscore*)allocate::Allocate_First_Block(MOX::screen_seg, sizeof(s_hiscore));
        memset(hi, 0, sizeof(s_hiscore));

        bitmap_size = bitmap::Get_Bitmap_Size(600, 300);
        hi->report_bitmap = (s_animation_header*)allocate::Allocate_Next_Block(MOX::screen_seg, bitmap_size);
        bitmap::Create_Blank_Bitmap(600, 300, hi->report_bitmap);

        bitmap_size = bitmap::Get_Bitmap_Size(50, 50);
        MAINSCR::temp_bitmap = (s_animation_header*)allocate::Allocate_Next_Block(MOX::screen_seg, bitmap_size);

        hi->main_background_anim = (s_animation_header*)farload::Far_Reload_Next("SCORE.LBX", 0, MOX::screen_seg);
        hi->race_icon_frame_anim = (s_animation_header*)farload::Far_Reload_Next("SCORE.LBX", 1, MOX::screen_seg);

        lbx_index = 2;
        for (int i = 0; i < STOCK_RACE_COUNT; ++i) {
            hi->race_icon_anims[i] = (s_animation_header*)farload::Far_Reload_Next("SCORE.LBX", lbx_index, MOX::screen_seg);
            lbx_index++;
        }
    }

    void __cdecl Get_Hi_Score_Colors() {
        // Set main high score colors
        SCORE::hi->colors = *MISC::Font_Colors2(4, 208 /* 0xD0 */, 210 /* 0xD2 */);

        // Set alternative colors for UI highlights
        SCORE::hi->alt_colors = *MISC::Font_Colors2(4, 208 /* 0xD0 */, 210 /* 0xD2 */);

        // Change global font color offset before fetching temporary colors
        MOX::mox_font_colors_offset = 0xF3;
        SCORE::hi->temp_colors = *MISC::Font_Colors2(4, 208 /* 0xD0 */, 210 /* 0xD2 */);
    }

    void __cdecl Draw_Pop(uint8_t is_captured, int16_t max_pop, int16_t current_pop) {
        int16_t space;
        s_animation_header* anim;
        int16_t y_start;
        int16_t y_offset = 0;
        int16_t x_offset = 0;
        int16_t row_anim_count = 0;
        int16_t drawn_count = 0;

        y_start = SCORE::hi->next_draw_y;

        anim = COLONY::C_Anims(13);
        remap::Calculate_Picture_Remap_Colors(anim);

        if (current_pop < max_pop * 48) {
            space = 12;
        } else if (current_pop < max_pop * 58) {
            space = 10;
        } else if (current_pop < max_pop * 72) {
            space = 8;
        } else if (current_pop < max_pop * 97) {
            space = 6;
        } else {
            space = 4;
        }

        if (current_pop > 0) {
            for (int16_t i = 0; i < current_pop; ++i) {
                if (i < max_pop * 97) {
                    if (is_captured == 0) {
                        anim = COLONY::Colony_Pop_Anim(
                            SCORE::hi->pop_units[i].race_id,
                            SCORE::hi->pop_units[i].icon_id);
                    } else {
                        anim = COLONY::Colony_Pop_Icon(
                            (uint16_t)(uint8_t)SCORE::hi->captured_pop_race_ids[i]);
                    }

                    if (current_pop > max_pop * 27) {
                        if ((row_anim_count & 1) == 0) {
                            animate::Remap_Draw(x_offset + 140, y_start, anim);
                            y_offset = 0;
                        } else {
                            animate::Remap_Draw(x_offset + 140, y_start + space, anim);
                            y_offset = space;
                        }
                        x_offset += space;
                        drawn_count++;

                        if (x_offset + 24 > 360) {
                            y_start += 30;
                            y_offset = 0;
                            row_anim_count = 0;
                            x_offset = 0;
                        }
                    } else {
                        x_offset = (drawn_count % 25) * 22;
                        y_offset = (drawn_count / 25) * 22;
                        animate::Remap_Draw(x_offset + 140, y_start + y_offset, anim);
                        drawn_count++;
                    }
                    row_anim_count++;
                }
            }

            if (current_pop > 0) {
                SCORE::hi->next_draw_y = y_start + y_offset + 28;
                return;
            }
        }

        SCORE::hi->next_draw_y += 3;
    }

    void __cdecl Get_From_Tmp_Pop(s_hiscore_tmp_pop* tmp_pop_list, uint16_t race_idx, int16_t entry_count, int16_t* out_colony_idx, int16_t* out_pop_idx) {
        int16_t first_match_colony = -1;
        int16_t first_match_pop = -1;
        bool found_unused = false;
        int16_t unused_index = 0;

        for (int16_t i = 0; i < entry_count; ++i) {
            s_hiscore_tmp_pop* entry = &tmp_pop_list[i];

            if (entry->race_idx == (uint8_t)race_idx) {
                if (first_match_colony == -1) {
                    first_match_colony = entry->colony_idx;
                    first_match_pop = (int16_t)entry->pop_idx;
                }

                if (entry->is_used == 0) {
                    *out_colony_idx = entry->colony_idx;
                    *out_pop_idx = (int16_t)entry->pop_idx;
                    found_unused = true;
                    unused_index = i;
                }
            }
        }

        if (found_unused) {
            tmp_pop_list[unused_index].is_used = 1;
        } else if ((std::uintptr_t)out_colony_idx == (std::uintptr_t)-1) {
            *out_colony_idx = first_match_colony;
            *out_pop_idx = first_match_pop;
        }
    }
} // namespace SCORE