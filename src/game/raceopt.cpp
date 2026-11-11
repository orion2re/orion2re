#include "pch.h"
#include "string_overrides.h"

namespace raceopt {

    namespace {
        char primary_pick_labels[PRIMARY_PICK_COUNT][PRIMARY_CHOICE_COUNT][32];

        int Primary_Pick_Table_Size() {
            return (int)PRIMARY_PICK_COUNT * (int)PRIMARY_CHOICE_COUNT;
        }

        int8_t Primary_Pick_Cost(int primary_pick, int choice) {
            return pick_rules.primary[primary_pick][choice].cost;
        }

        int8_t Flat_Primary_Pick_Cost(int index) {
            return Primary_Pick_Cost(index / PRIMARY_CHOICE_COUNT,
                index % PRIMARY_CHOICE_COUNT);
        }

        int8_t Pick_Cost_From_Ui_Index(int index) {
            if (index < Primary_Pick_Table_Size()) {
                return Flat_Primary_Pick_Cost(index);
            }
            index -= Primary_Pick_Table_Size();
            if (index < GOVERNMENT_PICK_COUNT) {
                return pick_rules.government[index];
            }
            return pick_rules.special[index - GOVERNMENT_PICK_COUNT];
        }

        int Government_Pick_From_Value(int8_t government) {
            for (int pick = 0; pick < GOVERNMENT_PICK_COUNT; ++pick) {
                if (selectable_governments[pick] == government) {
                    return pick;
                }
            }
            return GOVERNMENT_PICK_FEUDAL;
        }

        void Format_Primary_Pick_Label(char* output, size_t output_size, int primary_pick, int8_t value) {
            char value_text[16];
            Format_Primary_Pick_Value(value_text, sizeof(value_text), primary_pick, value);
            const int language = (uint8_t)MOX::settings.language;
            const char* format = "%s";
            if (primary_pick == PRIMARY_PICK_GROWTH) {
                const char* formats[] = {
                    "%s%% Growth",
                    "%s%% Zuwachs",
                    "Croissance (%s%%)",
                    "%s%% Crecimiento",
                    "Crescita %s%%",
                };
                format = formats[language < 5 ? language : 0];
            } else if (primary_pick == PRIMARY_PICK_FOOD) {
                const char* formats[] = {
                    "%s Food",
                    "%s Nahrung",
                    "%s Nourriture",
                    "%s Alimentos",
                    "Cibo %s",
                };
                format = formats[language < 5 ? language : 0];
            } else if (primary_pick == PRIMARY_PICK_PRODUCTION) {
                const char* formats[] = {
                    "%s Production",
                    "%s Produktion",
                    "%s Production",
                    "%s Produccio}n",
                    "Produzione %s",
                };
                format = formats[language < 5 ? language : 0];
            } else if (primary_pick == PRIMARY_PICK_RESEARCH) {
                const char* formats[] = {
                    "%s Research",
                    "%s Forschung",
                    "%s Recherche",
                    "%s Investigacio}n",
                    "Ricerca %s",
                };
                format = formats[language < 5 ? language : 0];
            } else if (primary_pick == PRIMARY_PICK_INCOME) {
                const char* formats[] = {
                    "%s BC",
                    "%s MC",
                    "%s BC",
                    "%s BC",
                    "%s BC",
                };
                format = formats[language < 5 ? language : 0];
            }
            snprintf(output, output_size, format, value_text);
        }

        void Refresh_Primary_Pick_Labels() {
            for (int primary_pick = 0; primary_pick < PRIMARY_PICK_COUNT;
                 ++primary_pick) {
                for (int choice = 0; choice < PRIMARY_CHOICE_COUNT; ++choice) {
                    Format_Primary_Pick_Label(
                        primary_pick_labels[primary_pick][choice],
                        sizeof(primary_pick_labels[primary_pick][choice]),
                        primary_pick,
                        pick_rules.primary[primary_pick][choice].value);
                    opt_string[primary_pick * 4 + choice + 1] =
                        primary_pick_labels[primary_pick][choice];
                }
            }
        }
    } // namespace

    int16_t negative_picks;
    int16_t exit_ken;
    s_colors color_opt;
    char* opt_string[64];
    uint8_t* accept_button_seg;
    uint8_t* default_button_seg;
    int16_t option_button[22];
    int16_t defalt;
    int16_t group_button[41];
    int16_t accept;
    int16_t picks;
    s_animation_header* off_button_seg;
    int16_t option_toggle[24];
    int16_t button_variable[10];
    s_animation_header* special_button_seg;
    int16_t name_button;
    char cur_race_name[16];

    void Format_Primary_Pick_Value(char* output, size_t output_size, int primary_pick, int8_t value) {
        const bool half_units = primary_pick == PRIMARY_PICK_FOOD ||
            primary_pick == PRIMARY_PICK_INCOME;
        const int magnitude = value < 0 ? -(int)value : value;
        const char sign = value < 0 ? '-' : '+';
        if (!half_units) {
            snprintf(output, output_size, "%c%i", sign, magnitude);
            return;
        }
        const char decimal = MOX::settings.language == 1 ||
                MOX::settings.language == 2
            ? ','
            : '.';
        if ((magnitude & 1) != 0) {
            snprintf(output, output_size, "%c%i%c5", sign, magnitude / 2, decimal);
        } else {
            snprintf(output, output_size, "%c%i", sign, magnitude / 2);
        }
    }

    void __cdecl Setup_Evolutionary_Upgrade(int16_t player_idx) {
        s_player& plr = MOX::player[player_idx];
        int8_t original_traits0 = plr.traits[TRAIT_CURRENT_GOVERNMENT];

        if (plr.objectives == PLAYER_OBJECTIVE_HUMAN) {
            struct_::Copy_Structure(
                &plr.evolutionary_upgrade_traits[1],
                &plr.traits[1],
                sizeof(plr.traits) - sizeof(plr.traits[TRAIT_CURRENT_GOVERNMENT]));

            plr.evolutionary_upgrade_traits[TRAIT_CURRENT_GOVERNMENT] = original_traits0 & 0x0E;

            Evolutionary_Mutation_Screen(plr.evolutionary_upgrade_traits);

            plr.traits[TRAIT_CURRENT_GOVERNMENT] = original_traits0;
        } else {
            struct_::Copy_Structure(
                &plr.traits[1],
                &plr.evolutionary_upgrade_traits[1],
                sizeof(plr.traits) - sizeof(plr.traits[TRAIT_CURRENT_GOVERNMENT]));
        }

        plr.picked_evolutionary_upgrade = 1;
    }

    void __cdecl Evolutionary_Mutation_Screen(int8_t* traits) {
        MOX2::action_func_t previous_auto_function;
        int16_t previous_button_values[10];
        int8_t baseline_flags[TRAIT_COUNT];
        char message_text[200];
        char message_line[100];

        MOX2::Get_Auto_Function(&previous_auto_function);
        ERIC::Fast_Fade_Out();
        fonts::Load_Palette(9, 0, 255);
        Convert_Player_To_Flags_EM(traits);
        struct_::Copy_Structure(baseline_flags, traits, sizeof(baseline_flags));

        for (int16_t i = 1; i < 10; ++i) {
            for (int16_t j = 2; j >= 0; --j) {
                if (pick_rules.primary[i - 1][j].value == baseline_flags[i]) {
                    baseline_flags[i] = j + 1;
                }
            }
        }

        Set_Race_Option_Help_List();
        fields::Clear_Fields();
        Reload_Option_Strings();
        Reload_Racial_Option_Screen();
        Calculate_EM_Picks();

        int16_t orig_picks = picks;
        fields::Assign_Auto_Function(Draw_Racial_Option_Screen, 1);

        color_opt.colors[0] = 0x8E;
        color_opt.colors[1] = 0x9D;
        color_opt.colors[2] = 0x9C;
        color_opt.colors[3] = 0x9D;
        color_opt.colors[4] = 0x9D;
        color_opt.colors[5] = 0x9D;
        color_opt.colors[6] = 0x9D;
        color_opt.colors[7] = 0x9D;
        fonts::Set_Font_Style(4, &color_opt);

        Add_Race_Options_Fields_EM(baseline_flags, orig_picks);

        for (int i = 0; i < 10; ++i) {
            previous_button_values[i] = button_variable[i];
        }

        video::Set_Page_Back();
        Draw_Race_Option_Background();
        video::Copy_Back_To_Off();
        video::Set_Page_Off();
        Draw_Racial_Option_Screen();
        palstore::Darken_Palette(100);
        palette::Clear_Palette_Changes(0, 255);
        RUSS::Mox_Sync_Update();
        ERIC::Fast_Fade_In();

        KEN::Ken_Get_Text_Message(77, message_line, sizeof(message_line));
        strlcpy(message_text, message_line, sizeof(message_text));
        KEN::Ken_Get_Text_Message(78, message_line, sizeof(message_line));
        strlcat(message_text, message_line, sizeof(message_text));
        GENDRAW::Message_Box(message_text);

        video::Set_Page_Back();
        Draw_Race_Option_Background();
        video::Copy_Back_To_Off();
        video::Set_Page_Off();
        Draw_Racial_Option_Screen();
        RUSS::Mox_Sync_Update();

        bool done = false;
        while (!done) {
            MOX::FULL_DRAW_SCREEN_FLAG = 0;
            int16_t input = fields::Get_Input();

            for (int i = 0; i < 36; ++i) {
                if (group_button[i] == input) {
                    int group = i / 4;
                    int choice = i % 4;

                    if (group == 1 && option_toggle[9] == 1) {
                        KEN::Ken_Get_Text_Message(73, message_text, 120);
                        GENDRAW::Warning_Box(message_text);
                        video::Set_Page_Back();
                        Draw_Race_Option_Background();
                        video::Copy_Back_To_Off();
                        video::Set_Page_Off();
                        Draw_Racial_Option_Screen();
                        RUSS::Mox_Sync_Update();
                        button_variable[1] = 0;
                    } else {
                        if (previous_button_values[group] == choice) {
                            button_variable[group] = 0;
                            Calculate_EM_Picks();
                        } else {
                            int cost_idx = i + (-3 - i) / 4;
                            if (Pick_Cost_From_Ui_Index(cost_idx) + negative_picks <
                                pick_rules.negative_limit) {
                                KEN::Ken_Get_Text_Message(74, message_text, 80);
                                GENDRAW::Warning_Box(message_text);
                                video::Set_Page_Back();
                                Draw_Race_Option_Background();
                                video::Copy_Back_To_Off();
                                video::Set_Page_Off();
                                Draw_Racial_Option_Screen();
                                RUSS::Mox_Sync_Update();
                                button_variable[group] = previous_button_values[group];
                            } else {
                                button_variable[group] = choice;
                            }
                        }
                    }

                    int cost_idx2 = i + (-3 - i) / 4;
                    if (button_variable[group] < baseline_flags[group + 1] &&
                        Pick_Cost_From_Ui_Index(cost_idx2) >= 0) {
                        button_variable[group] = baseline_flags[group + 1];
                    }
                }
            }

            for (int i = 0; i < 22; ++i) {
                if (option_button[i] == input) {
                    option_toggle[i] = 1 - option_toggle[i];

                    if (option_toggle[i] == 0) {
                        if (pick_rules.special[i] < 0) {
                            option_toggle[i] = 1;
                        }
                        Calculate_EM_Picks();
                    } else {
                        Calculate_EM_Picks();
                        if (negative_picks < pick_rules.negative_limit) {
                            KEN::Ken_Get_Text_Message(74, message_text, 80);
                            GENDRAW::Warning_Box(message_text);
                            video::Set_Page_Back();
                            Draw_Race_Option_Background();
                            video::Copy_Back_To_Off();
                            video::Set_Page_Off();
                            Draw_Racial_Option_Screen();
                            RUSS::Mox_Sync_Update();
                            option_toggle[i] = 0;
                        }

                        if (i == SPECIAL_PICK_LOW_GRAVITY) {
                            option_toggle[SPECIAL_PICK_HIGH_GRAVITY] = 0;
                        } else if (i == SPECIAL_PICK_HIGH_GRAVITY) {
                            option_toggle[SPECIAL_PICK_LOW_GRAVITY] = 0;
                        } else if (i == SPECIAL_PICK_LITHOVORE) {
                            option_toggle[SPECIAL_PICK_CYBERNETIC] = 0;
                            button_variable[1] = 0;
                        } else if (i == SPECIAL_PICK_CYBERNETIC) {
                            option_toggle[SPECIAL_PICK_LITHOVORE] = 0;
                        } else if (i == SPECIAL_PICK_REPULSIVE) {
                            option_toggle[SPECIAL_PICK_CHARISMATIC] = 0;
                        } else if (i == SPECIAL_PICK_CHARISMATIC) {
                            option_toggle[SPECIAL_PICK_REPULSIVE] = 0;
                        } else if (i == SPECIAL_PICK_UNCREATIVE) {
                            option_toggle[SPECIAL_PICK_CREATIVE] = 0;
                        } else if (i == SPECIAL_PICK_CREATIVE) {
                            option_toggle[SPECIAL_PICK_UNCREATIVE] = 0;
                        }
                    }
                }
            }

            if (accept == input) {
                if (picks < 0) {
                    KEN::Ken_Get_Text_Message(75, message_text, 80);
                    GENDRAW::Warning_Box(message_text);
                    video::Set_Page_Back();
                    Draw_Race_Option_Background();
                    video::Copy_Back_To_Off();
                    video::Set_Page_Off();
                    Draw_Racial_Option_Screen();
                    RUSS::Mox_Sync_Update();
                } else {
                    done = true;
                }
            }

            Calculate_EM_Picks();

            if (MOX::FULL_DRAW_SCREEN_FLAG != 0) {
                video::Set_Page_Back();
                Draw_Race_Option_Background();
                video::Copy_Back_To_Off();
            }

            if (!done) {
                Add_Race_Options_Fields_EM(baseline_flags, orig_picks);
                video::Set_Page_Off();
                Draw_Racial_Option_Screen();
                RUSS::Mox_Sync_Update();

                for (int i = 0; i < 10; ++i) {
                    previous_button_values[i] = button_variable[i];
                }
            }
        }

        Convert_Flags_To_Player_EM(traits);
        fields::Deactivate_Help_List();
        fields::Deactivate_Auto_Function();
        MOX2::Put_Auto_Function(&previous_auto_function);
        fields::Clear_Fields();
        allocate::Pop_Block(MOX::screen_seg);
        MISC::Fade_Into_Screen(MAINSCR::Draw_Mini_Main_Screen, &MOX::draw_main_screen_to_back, 0);
    }

    int16_t __cdecl Player_Score_Modifier(int16_t player_idx) {
        int16_t score_modifier = 100;

        Convert_Player_To_Flags_EM(MOX::player[player_idx].traits);
        Calculate_Picks();

        if (MOX::player[player_idx].tech_applications[TECH_APP_EVOLUTIONARY_MUTATION] == TECH_RESEARCH_STATUS_RESEARCHED) {
            picks += pick_rules.mutation_bonus;
        }

        if (picks > 0) {
            score_modifier = (picks * 5 + 50) * 2;
        }

        return score_modifier;
    }

    void __cdecl Reload_Option_Strings() {
        char* strings_data = string_overrides::Load_Raw_Table(
            "racestuf.lbx",
            (uint32_t)(uint8_t)(MOX::settings.language),
            MOX::screen_seg, 2, 64, 1, true);

        size_t offset = 0;
        int16_t index = 1;

        opt_string[0] = strings_data;

        do {
            while (strings_data[offset] != '\0') {
                offset++;
            }
            while (strings_data[offset] == '\0') {
                offset++;
            }
            opt_string[index] = strings_data + offset;
            index++;
        } while (index < 64);

        Refresh_Primary_Pick_Labels();
    }

    void __cdecl Calculate_EM_Picks() {
        Calculate_Picks();
        picks += pick_rules.mutation_bonus;
    }

    void __cdecl Draw_Race_Option_Background() {
        int16_t col_x[4] = {80, 254, 442, 643};
        int16_t y_coords[5] = {72, 147, 222, 297, 372};
        s_colors color1 = {{0x91, 0x95, 0x95, 0x95, 0x95, 0x95, 0x95, 0x95}};

        s_animation_header* bg1 = (s_animation_header*)buffer::Buffer_Reload("RACEOPT.LBX", 4, MOX::global_cache_seg);
        animate::Draw(0, 0, bg1);

        s_animation_header* bg2 = (s_animation_header*)buffer::Buffer_Reload("patch13.lbx", 0, MOX::global_cache_seg);
        animate::Draw(131, 0, bg2);

        int16_t col_idx = 0;
        int16_t y_idx = 0;
        int16_t opt_string_idx = 0;

        // First section: columns 0 and 1
        while (opt_string_idx < 36) {
            fonts::Set_Font_Style_Shadow_Down(4, &color_opt);
            fonts::Set_Outline_Color(119, 0);

            int16_t current_y = y_coords[y_idx];
            int16_t header_x = col_x[col_idx] - 22;

            fonts::Print(header_x, current_y - 22, opt_string[opt_string_idx]);
            opt_string_idx++;

            int16_t col_start_x = col_x[col_idx];
            int16_t right_align_x = col_x[col_idx + 1] - 50;

            for (int i = 0; i < 3; i++) {
                if (group_button[opt_string_idx] > 0) {
                    animate::Set_Animation_Frame(special_button_seg, 0);
                    animate::Draw(col_start_x - 20, current_y, special_button_seg);
                }

                fonts::Set_Font_Style_Shadow_Down(2, &color1);

                char num_buf[12];
                char str_buf[12];

                str_buf[0] = '\0';
                str_buf[1] = '\0';

                int cost_idx = opt_string_idx + (-3 - opt_string_idx) / 4;
                snprintf(num_buf, sizeof(num_buf), "%i", Pick_Cost_From_Ui_Index(cost_idx));
                strlcat(str_buf, num_buf, sizeof(str_buf));

                fonts::Print(col_start_x, current_y, opt_string[opt_string_idx]);

                int16_t dots_start = col_start_x + fonts::Get_String_Width(opt_string[opt_string_idx]) - 1;
                int16_t dots_end = col_x[col_idx + 1] - fonts::Get_String_Width(str_buf) - 50;

                while (dots_start < dots_end - 4) {
                    fonts::Print(dots_start, current_y, ".");
                    dots_start += 4;
                }

                fonts::Print_Right(right_align_x, current_y, str_buf);

                opt_string_idx++;
                current_y += 13;
            }

            y_idx++;
            if (y_idx >= 5) {
                y_idx = 0;
                col_idx++;
            }
        }

        // Second section: column 1 bottom piece
        fonts::Set_Font_Style_Shadow_Down(4, &color_opt);
        fonts::Set_Outline_Color(119, 0);

        int16_t current_y = y_coords[y_idx];
        int16_t header_x = col_x[col_idx] - 22;
        fonts::Print(header_x, current_y - 22, opt_string[opt_string_idx]);
        opt_string_idx++;

        int16_t col_start_x = col_x[col_idx];
        int16_t right_align_x = col_x[col_idx + 1] - 50;

        for (int i = 0; i < GOVERNMENT_COUNT / 2; i++) {
            animate::Set_Animation_Frame(special_button_seg, 0);
            animate::Draw(col_start_x - 20, current_y, special_button_seg);

            fonts::Set_Font_Style_Shadow_Down(2, &color1);

            char num_buf[12];
            char str_buf[12];

            int cost_idx = opt_string_idx + (-3 - opt_string_idx) / 4;
            snprintf(num_buf, sizeof(num_buf), "%i", Pick_Cost_From_Ui_Index(cost_idx));
            strlcpy(str_buf, num_buf, sizeof(str_buf));

            int16_t dots_start = col_start_x + fonts::Get_String_Width(opt_string[opt_string_idx]) - 1;
            int16_t dots_end = col_x[col_idx + 1] - fonts::Get_String_Width(str_buf) - 50;

            while (dots_start < dots_end - 4) {
                fonts::Print(dots_start, current_y, ".");
                dots_start += 4;
            }

            fonts::Print(col_start_x, current_y, opt_string[opt_string_idx]);
            fonts::Print_Right(right_align_x, current_y, str_buf);
            opt_string_idx++;
            current_y += 13;
        }

        // Third section: column 2
        col_idx++;
        fonts::Set_Font_Style_Shadow_Down(4, &color_opt);
        fonts::Set_Outline_Color(119, 0);

        fonts::Print(col_x[col_idx] - 22, 50, opt_string[opt_string_idx]);
        opt_string_idx++;

        current_y = 72;
        int16_t block_sizes[4] = {22, 0, 0, 0};

        int16_t loop_idx = 0;
        while (opt_string_idx < 64) {
            int16_t count = block_sizes[loop_idx];
            if (count > 0) {
                int16_t col_2_start_x = col_x[col_idx];
                int16_t col_2_right_align_x = col_x[col_idx + 1] - 50;

                for (int i = 0; i < count; i++) {
                    animate::Set_Animation_Frame(special_button_seg, 0);
                    animate::Draw(col_2_start_x - 20, current_y, special_button_seg);

                    fonts::Set_Font_Style_Shadow_Down(2, &color1);

                    char num_buf[12];
                    char str_buf[12];

                    snprintf(num_buf, sizeof(num_buf), "%i", Pick_Cost_From_Ui_Index(opt_string_idx - 11));
                    strlcpy(str_buf, num_buf, sizeof(str_buf));

                    int16_t dots_start = col_2_start_x + fonts::Get_String_Width(opt_string[opt_string_idx]) - 1;
                    int16_t dots_end = col_x[col_idx + 1] - fonts::Get_String_Width(str_buf) - 50;

                    while (dots_start < dots_end - 4) {
                        fonts::Print(dots_start, current_y, ".");
                        dots_start += 4;
                    }

                    fonts::Print(col_2_start_x, current_y, opt_string[opt_string_idx]);
                    fonts::Print_Right(col_2_right_align_x, current_y, str_buf);
                    opt_string_idx++;
                    current_y += 16;
                }
            }
            current_y += 9;
            loop_idx++;
        }

        fonts::Set_Font_Style_Shadow_Down(4, &color_opt);
    }

    void __cdecl Calculate_Picks() {
        negative_picks = 0;
        picks = pick_rules.starting;

        for (int i = 0; i < 9; ++i) {
            if (button_variable[i] != 0) {
                int8_t cost = pick_rules.primary[i][button_variable[i] - 1].cost;
                picks -= cost;
                if (cost < 0) {
                    negative_picks += cost;
                }
            }
        }

        int8_t cost = pick_rules.government[button_variable[9]];
        picks -= cost;
        if (cost < 0) {
            negative_picks += cost;
        }

        for (int i = 0; i < 22; ++i) {
            if (option_toggle[i] == 1) {
                int8_t opt_cost = pick_rules.special[i];
                picks -= opt_cost;
                if (opt_cost < 0) {
                    negative_picks += opt_cost;
                }
            }
        }
    }

    void __cdecl Reload_Racial_Option_Screen() {
        allocate::Push_Block(MOX::screen_seg);

        special_button_seg = (s_animation_header*)
            farload::Far_Reload_Next("RACEOPT.LBX", 1, MOX::screen_seg);
        RUSS::Mox_Update();

        default_button_seg = (uint8_t*)farload::Far_Reload_Next("RACEOPT.LBX", 2, MOX::screen_seg);
        RUSS::Mox_Update();

        accept_button_seg = (uint8_t*)farload::Far_Reload_Next("RACEOPT.LBX", 3, MOX::screen_seg);
        RUSS::Mox_Update();

        off_button_seg = (s_animation_header*)
            farload::Far_Reload_Next("RACEOPT.LBX", 5, MOX::screen_seg);
        RUSS::Mox_Update();

        Reload_Option_Strings();
    }

    void __cdecl Draw_Racial_Option_Screen() {
        int16_t x_coords[4] = {0x50, 0xFE, 0x1BA, 0x283};
        int16_t y_coords[5] = {0x48, 0x93, 0xDE, 0x129, 0x174};

        int16_t block_size = 0x16;
        int16_t loop3_i = 0;
        int16_t loop3_j = 0;
        int16_t cost_diff = 0;

        s_colors disabled_value_colors;
        disabled_value_colors.colors[0] = 0x89;
        disabled_value_colors.colors[1] = 0x91;
        disabled_value_colors.colors[2] = 0x91;
        disabled_value_colors.colors[3] = 0x91;
        disabled_value_colors.colors[4] = 0x91;
        disabled_value_colors.colors[5] = 0x91;
        disabled_value_colors.colors[6] = 0x91;
        disabled_value_colors.colors[7] = 0x91;

        s_colors disabled_cost_color = {{0x89, 0x95, 0x95, 0x95, 0x95, 0x95, 0x95, 0x95}};
        char ability_cost_text[12];

        fonts::Set_Font_Style_Shadow_Down(3, &color_opt);
        fonts::Set_Outline_Color(0x77, 0);

        fonts::Print_Integer_Right(0x12F, 0x1C1, picks);

        int16_t modified_picks = (picks + 10) * 10;
        if (modified_picks < 0) {
            modified_picks = 0;
        }

        fonts::Print_Integer_Right(0x1B7, 0x1C1, modified_picks);
        fonts::Print_Right(0x1C5, 0x1C1, "%");

        int16_t col_idx = 0;
        int16_t item_idx = 0;
        int16_t loop1_i = 0;

        do {
            int16_t loop1_j = 0;
            int16_t y = y_coords[loop1_i];

            item_idx++;

            do {
                int group_idx = item_idx / 4;
                int16_t btn_var = button_variable[group_idx];
                int16_t cost;

                if (btn_var == 0) {
                    cost = pick_rules.primary[group_idx][loop1_j].cost;
                } else {
                    cost = pick_rules.primary[group_idx][loop1_j].cost -
                        pick_rules.primary[group_idx][btn_var - 1].cost;
                }

                int neg_idx = item_idx + (-3 - item_idx) / 4;

                bool draw_off = false;
                if (group_button[item_idx] < 1 && (btn_var - loop1_j != 1))
                    draw_off = true;
                else if (picks - cost < 0 && (btn_var - loop1_j != 1))
                    draw_off = true;
                else if (Pick_Cost_From_Ui_Index(neg_idx) + negative_picks <
                        pick_rules.negative_limit &&
                    (btn_var - loop1_j != 1))
                    draw_off = true;

                if (draw_off) {
                    animate::Draw(x_coords[col_idx] - 0x14, y, off_button_seg);
                } else if (btn_var - loop1_j == 1) {
                    animate::Set_Animation_Frame(special_button_seg, 1);
                    animate::Draw(x_coords[col_idx] - 0x14, y, special_button_seg);
                }

                if (item_idx > 3 && item_idx < 8 && option_toggle[9] == 1) {
                    animate::Draw(x_coords[col_idx] - 0x14, y, off_button_seg);
                }

                bool draw_text = false;
                if (btn_var - loop1_j == 1) {
                    draw_text = true;
                    fonts::Set_Font_Style_Shadow_Down(2, &color_opt);
                } else {
                    if (Pick_Cost_From_Ui_Index(neg_idx) + negative_picks <
                            pick_rules.negative_limit ||
                        picks - cost < 0 ||
                        (item_idx > 3 && item_idx < 8 && option_toggle[9] == 1) ||
                        group_button[item_idx] == -1000) {
                        draw_text = true;
                        fonts::Set_Font_Style_Shadow_Down(2, &disabled_value_colors);
                    }
                }

                if (draw_text) {
                    char ability_cost_digits[12];
                    snprintf(ability_cost_digits, sizeof(ability_cost_digits), "%i", Pick_Cost_From_Ui_Index(neg_idx));
                    strlcpy(ability_cost_text, ability_cost_digits, sizeof(ability_cost_text));

                    fonts::Print(x_coords[col_idx], y, opt_string[item_idx]);
                    int16_t str_w = fonts::Get_String_Width(opt_string[item_idx]);

                    int dots_start = x_coords[col_idx] - 1 + str_w;
                    int target_x = x_coords[col_idx + 1] - fonts::Get_String_Width(ability_cost_text) - 0x32;

                    for (int x = dots_start; x < target_x - 4; x += 4) {
                        fonts::Print(x, y, ".");
                    }

                    fonts::Print_Right(x_coords[col_idx + 1] - 0x32, y, ability_cost_text);
                }

                y += 0xD;
                item_idx++;
                loop1_j++;
            } while (loop1_j < 3);

            loop1_i++;
            if (loop1_i >= 5) {
                loop1_i = 0;
                col_idx++;
            }
        } while (item_idx < 36);

        int16_t loop2_j = 0;
        int16_t y2 = y_coords[loop1_i];

        item_idx++;

        do {
            int16_t neg_idx = item_idx + (-3 - item_idx) / 4;
            cost_diff = Pick_Cost_From_Ui_Index(neg_idx) -
                pick_rules.government[button_variable[9]];

            bool draw_off = false;
            if (group_button[item_idx] < 0)
                draw_off = true;
            else if (picks - cost_diff < 0 && loop2_j != button_variable[9])
                draw_off = true;
            else if (negative_picks + Pick_Cost_From_Ui_Index(neg_idx) <
                    pick_rules.negative_limit &&
                loop2_j != button_variable[9])
                draw_off = true;

            if (draw_off) {
                animate::Draw(x_coords[col_idx] - 0x14, y2, off_button_seg);
            } else if (loop2_j == button_variable[9]) {
                animate::Set_Animation_Frame(special_button_seg, 1);
                animate::Draw(x_coords[col_idx] - 0x14, y2, special_button_seg);
            }

            bool draw_text2 = false;
            if (loop2_j == button_variable[9]) {
                draw_text2 = true;
                fonts::Set_Font_Style_Shadow_Down(2, &color_opt);
            } else {
                if (Pick_Cost_From_Ui_Index(neg_idx) + negative_picks <
                        pick_rules.negative_limit ||
                    picks - cost_diff < 0) {
                    draw_text2 = true;
                    fonts::Set_Font_Style_Shadow_Down(2, &disabled_value_colors);
                }
            }

            if (draw_text2) {
                char ability_cost_digits[12];
                snprintf(ability_cost_digits, sizeof(ability_cost_digits), "%i", Pick_Cost_From_Ui_Index(neg_idx));
                strlcpy(ability_cost_text, ability_cost_digits, sizeof(ability_cost_text));

                int16_t str_w = fonts::Get_String_Width(opt_string[item_idx]);

                int dots_start = x_coords[col_idx] - 1 + str_w;
                int target_x = x_coords[col_idx + 1] - fonts::Get_String_Width(ability_cost_text) - 0x32;

                for (int x = dots_start; x < target_x - 4; x += 4) {
                    fonts::Print(x, y2, ".");
                }

                fonts::Print(x_coords[col_idx], y2, opt_string[item_idx]);
                fonts::Print_Right(x_coords[col_idx + 1] - 0x32, y2, ability_cost_text);
            }

            y2 += 0xD;
            loop2_j++;
            item_idx++;
        } while (loop2_j < GOVERNMENT_COUNT / 2);

        loop3_i = 0;
        loop3_j = 0;
        int16_t y3 = 0x48;
        col_idx++;

        item_idx++;
        if (item_idx < 64) {
            int16_t block_sizes[4] = {0x16, 0, 0, 0};

            do {
                int16_t k = 0;
                int16_t limit = block_sizes[loop3_i];

                if (limit > 0) {
                    do {
                        bool draw_text3 = true;

                        if (option_toggle[loop3_j] == 1) {
                            animate::Set_Animation_Frame(special_button_seg, 1);
                            animate::Draw(x_coords[col_idx] - 0x14, y3, special_button_seg);
                            fonts::Set_Font_Style_Shadow_Down(2, &color_opt);
                        } else {
                            cost_diff = Real_Ability_Cost(k);
                            if (option_button[loop3_j] < 0 ||
                                picks - cost_diff < 0 ||
                                Pick_Cost_From_Ui_Index(item_idx - 11) +
                                        negative_picks <
                                    pick_rules.negative_limit) {
                                animate::Draw(x_coords[col_idx] - 0x14, y3, off_button_seg);
                                fonts::Set_Font_Style_Shadow_Down(2, &disabled_value_colors);
                            } else {
                                draw_text3 = false;
                            }
                        }

                        if (draw_text3) {
                            char ability_cost_digits[12];
                            snprintf(ability_cost_digits, sizeof(ability_cost_digits), "%i", Pick_Cost_From_Ui_Index(item_idx - 11));
                            strlcpy(ability_cost_text, ability_cost_digits, sizeof(ability_cost_text));

                            int16_t str_w = fonts::Get_String_Width(opt_string[item_idx]);

                            int dots_start = x_coords[col_idx] - 1 + str_w;
                            int target_x = x_coords[col_idx + 1] - fonts::Get_String_Width(ability_cost_text) - 0x32;

                            for (int x = dots_start; x < target_x - 4; x += 4) {
                                fonts::Print(x, y3, ".");
                            }

                            fonts::Print(x_coords[col_idx], y3, opt_string[item_idx]);
                            fonts::Print_Right(x_coords[col_idx + 1] - 0x32, y3, ability_cost_text);
                        }

                        y3 += 0x10;
                        loop3_j++;
                        k++;
                        item_idx++;
                    } while (k < limit);
                }

                y3 += 9;
                loop3_i++;
            } while (item_idx < 64);
        }

        fonts::Set_Font_Style_Shadow_Down(4, &color_opt);
    }

    void __cdecl Convert_Player_To_Flags_EM(int8_t* player_flags) {
        button_variable[9] = Government_Pick_From_Value(player_flags[0]);

        for (int16_t i = 0; i < 9; ++i) {
            uint8_t bonus_value = static_cast<uint8_t>(player_flags[i + 1]);

            if (pick_rules.primary[i][PRIMARY_CHOICE_PENALTY].value ==
                (int8_t)bonus_value) {
                button_variable[i] = 1;
            } else if (pick_rules.primary[i][PRIMARY_CHOICE_BONUS].value ==
                (int8_t)bonus_value) {
                button_variable[i] = 2;
            } else if (pick_rules.primary[i][PRIMARY_CHOICE_MAJOR_BONUS].value ==
                (int8_t)bonus_value) {
                button_variable[i] = 3;
            } else {
                button_variable[i] = 0;
            }
        }

        for (int special_pick = 0; special_pick < SPECIAL_PICK_COUNT;
             ++special_pick) {
            const s_special_pick_identity& identity =
                special_pick_identities[special_pick];
            option_toggle[special_pick] =
                player_flags[identity.trait] == identity.selected_value;
        }
    }

    void __cdecl Set_Race_Option_Help_List() {
        MOX::help_entry_list[0].help_id = 0x1BB;
        MOX::help_entry_list[0].x1 = 0x036;
        MOX::help_entry_list[0].y1 = 0x031;
        MOX::help_entry_list[0].x2 = 0x0D1;
        MOX::help_entry_list[0].y2 = 0x06F;

        MOX::help_entry_list[1].help_id = 0x1BC;
        MOX::help_entry_list[1].x1 = 0x036;
        MOX::help_entry_list[1].y1 = 0x07B;
        MOX::help_entry_list[1].x2 = 0x0D1;
        MOX::help_entry_list[1].y2 = 0x0B9;

        MOX::help_entry_list[2].help_id = 0x1BD;
        MOX::help_entry_list[2].x1 = 0x036;
        MOX::help_entry_list[2].y1 = 0x0C5;
        MOX::help_entry_list[2].x2 = 0x0D0;
        MOX::help_entry_list[2].y2 = 0x103;

        MOX::help_entry_list[3].help_id = 0x1BE;
        MOX::help_entry_list[3].x1 = 0x036;
        MOX::help_entry_list[3].y1 = 0x10F;
        MOX::help_entry_list[3].x2 = 0x0D0;
        MOX::help_entry_list[3].y2 = 0x14D;

        MOX::help_entry_list[4].help_id = 0x1BF;
        MOX::help_entry_list[4].x1 = 0x036;
        MOX::help_entry_list[4].y1 = 0x159;
        MOX::help_entry_list[4].x2 = 0x0D0;
        MOX::help_entry_list[4].y2 = 0x19B;

        MOX::help_entry_list[5].help_id = 0x1C0;
        MOX::help_entry_list[5].x1 = 0x0E5;
        MOX::help_entry_list[5].y1 = 0x031;
        MOX::help_entry_list[5].x2 = 0x18B;
        MOX::help_entry_list[5].y2 = 0x070;

        MOX::help_entry_list[6].help_id = 0x1C1;
        MOX::help_entry_list[6].x1 = 0x0E5;
        MOX::help_entry_list[6].y1 = 0x07B;
        MOX::help_entry_list[6].x2 = 0x18B;
        MOX::help_entry_list[6].y2 = 0x0BA;

        MOX::help_entry_list[7].help_id = 0x1C2;
        MOX::help_entry_list[7].x1 = 0x0E5;
        MOX::help_entry_list[7].y1 = 0x0C5;
        MOX::help_entry_list[7].x2 = 0x18B;
        MOX::help_entry_list[7].y2 = 0x104;

        MOX::help_entry_list[8].help_id = 0x1C3;
        MOX::help_entry_list[8].x1 = 0x0E5;
        MOX::help_entry_list[8].y1 = 0x10F;
        MOX::help_entry_list[8].x2 = 0x18B;
        MOX::help_entry_list[8].y2 = 0x14E;

        MOX::help_entry_list[9].help_id = 0x1C4;
        MOX::help_entry_list[10].help_id = 0x1C5;
        MOX::help_entry_list[11].help_id = 0x1C6;
        MOX::help_entry_list[12].help_id = 0x1C7;
        MOX::help_entry_list[13].help_id = 0x1C8;
        MOX::help_entry_list[14].help_id = 0x1C9;
        MOX::help_entry_list[15].help_id = 0x1CA;
        MOX::help_entry_list[16].help_id = 0x1CB;
        MOX::help_entry_list[17].help_id = 0x1CC;
        MOX::help_entry_list[18].help_id = 0x1CD;
        MOX::help_entry_list[19].help_id = 0x1CE;
        MOX::help_entry_list[20].help_id = 0x1CF;
        MOX::help_entry_list[21].help_id = 0x1D0;
        MOX::help_entry_list[22].help_id = 0x1D1;
        MOX::help_entry_list[23].help_id = 0x1D2;
        MOX::help_entry_list[24].help_id = 0x1D3;
        MOX::help_entry_list[25].help_id = 0x1D4;
        MOX::help_entry_list[26].help_id = 0x1D5;
        MOX::help_entry_list[27].help_id = 0x1D6;
        MOX::help_entry_list[28].help_id = 0x1D7;
        MOX::help_entry_list[29].help_id = 0x1D8;
        MOX::help_entry_list[30].help_id = 0x1D9;

        int16_t y = 0x48;
        for (int16_t i = 0; i < 22; ++i) {
            MOX::help_entry_list[i + 9].x1 = 0x1A6;
            MOX::help_entry_list[i + 9].y1 = y;
            MOX::help_entry_list[i + 9].x2 = 0x252;
            MOX::help_entry_list[i + 9].y2 = y + 9;
            y += 0x10;
        }

        MOX::help_entry_list[31].help_id = 0x1DF;
        MOX::help_entry_list[31].x1 = 0x0BE;
        MOX::help_entry_list[31].y1 = 0x1BA;
        MOX::help_entry_list[31].x2 = 0x135;
        MOX::help_entry_list[31].y2 = 0x1D0;

        MOX::help_entry_list[32].help_id = 0x1E0;
        MOX::help_entry_list[32].x1 = 0x146;
        MOX::help_entry_list[32].y1 = 0x1BA;
        MOX::help_entry_list[32].x2 = 0x1C9;
        MOX::help_entry_list[32].y2 = 0x1D1;

        MOX::help_entry_list[33].help_id = 0x1DE;
        MOX::help_entry_list[33].x1 = 0x049;
        MOX::help_entry_list[33].y1 = 0x1BE;
        MOX::help_entry_list[33].x2 = 0x092;
        MOX::help_entry_list[33].y2 = 0x1D7;

        MOX::help_entry_list[34].help_id = 0x1E1;
        MOX::help_entry_list[34].x1 = 0x1F8;
        MOX::help_entry_list[34].y1 = 0x1BF;
        MOX::help_entry_list[34].x2 = 0x23E;
        MOX::help_entry_list[34].y2 = 0x1D6;

        MOX::help_entry_list[35].help_id = 0x1DA;
        MOX::help_entry_list[35].x1 = 0x0E3;
        MOX::help_entry_list[35].y1 = 0x173;
        MOX::help_entry_list[35].x2 = 0x18E;
        MOX::help_entry_list[35].y2 = 0x17E;

        MOX::help_entry_list[36].help_id = 0x1DB;
        MOX::help_entry_list[36].x1 = 0x0E3;
        MOX::help_entry_list[36].y1 = 0x180;
        MOX::help_entry_list[36].x2 = 0x18E;
        MOX::help_entry_list[36].y2 = 0x18B;

        MOX::help_entry_list[37].help_id = 0x1DC;
        MOX::help_entry_list[37].x1 = 0x0E3;
        MOX::help_entry_list[37].y1 = 0x18D;
        MOX::help_entry_list[37].x2 = 0x18E;
        MOX::help_entry_list[37].y2 = 0x198;

        MOX::help_entry_list[38].help_id = 0x1DD;
        MOX::help_entry_list[38].x1 = 0x0E3;
        MOX::help_entry_list[38].y1 = 0x19A;
        MOX::help_entry_list[38].x2 = 0x18E;
        MOX::help_entry_list[38].y2 = 0x1A5;

        fields::Set_Help_List(MOX::help_entry_list, 0x27);
    }

    void __cdecl Convert_Flags_To_Player_EM(int8_t* flags_out) {
        for (int16_t i = 1; i < 10; ++i) {
            const int16_t val = button_variable[i - 1];
            if (val == 0) {
                flags_out[i] = 0;
            } else {
                flags_out[i] = pick_rules.primary[i - 1][val - 1].value;
            }
        }

        for (int trait = TRAIT_LOW_G_WORLD; trait < TRAIT_COUNT; ++trait) {
            flags_out[trait] = 0;
        }
        for (int special_pick = 0; special_pick < SPECIAL_PICK_COUNT;
             ++special_pick) {
            if (option_toggle[special_pick] != 0) {
                const s_special_pick_identity& identity =
                    special_pick_identities[special_pick];
                flags_out[identity.trait] = identity.selected_value;
            }
        }
    }

    int16_t __cdecl Real_Ability_Cost(int16_t ability_idx) {
        int16_t cost = pick_rules.special[ability_idx];

        if (option_toggle[ability_idx] == 1) {
            cost = -cost;
        }

        if (ability_idx == SPECIAL_PICK_RICH_HOME_WORLD &&
            option_toggle[SPECIAL_PICK_POOR_HOME_WORLD] == 1) {
            cost -= pick_rules.special[SPECIAL_PICK_POOR_HOME_WORLD];
        }

        if (ability_idx == SPECIAL_PICK_POOR_HOME_WORLD &&
            option_toggle[SPECIAL_PICK_RICH_HOME_WORLD] == 1) {
            cost -= pick_rules.special[SPECIAL_PICK_RICH_HOME_WORLD];
        }

        if (ability_idx == SPECIAL_PICK_LOW_GRAVITY &&
            option_toggle[SPECIAL_PICK_HIGH_GRAVITY] == 1) {
            cost -= pick_rules.special[SPECIAL_PICK_HIGH_GRAVITY];
        } else if (ability_idx == SPECIAL_PICK_HIGH_GRAVITY &&
            option_toggle[SPECIAL_PICK_LOW_GRAVITY] == 1) {
            cost -= pick_rules.special[SPECIAL_PICK_LOW_GRAVITY];
        }

        if (ability_idx == SPECIAL_PICK_LITHOVORE) {
            if (option_toggle[SPECIAL_PICK_CYBERNETIC] == 1) {
                cost -= pick_rules.special[SPECIAL_PICK_LITHOVORE];
            }
            if (button_variable[1] != 0) {
                cost -= pick_rules.primary[PRIMARY_PICK_FOOD]
                                     [button_variable[1] - 1]
                                         .cost;
            }
        } else if (ability_idx == SPECIAL_PICK_CYBERNETIC &&
            option_toggle[SPECIAL_PICK_LITHOVORE] == 1) {
            cost -= pick_rules.special[SPECIAL_PICK_CYBERNETIC];
        }

        if (ability_idx == SPECIAL_PICK_REPULSIVE &&
            option_toggle[SPECIAL_PICK_CHARISMATIC] == 1) {
            cost -= pick_rules.special[SPECIAL_PICK_CHARISMATIC];
        } else if (ability_idx == SPECIAL_PICK_CHARISMATIC &&
            option_toggle[SPECIAL_PICK_REPULSIVE] == 1) {
            cost -= pick_rules.special[SPECIAL_PICK_REPULSIVE];
        }

        if (ability_idx == SPECIAL_PICK_CREATIVE &&
            option_toggle[SPECIAL_PICK_UNCREATIVE] == 1) {
            cost -= pick_rules.special[SPECIAL_PICK_UNCREATIVE];
        } else if (ability_idx == SPECIAL_PICK_UNCREATIVE &&
            option_toggle[SPECIAL_PICK_CREATIVE] == 1) {
            cost -= pick_rules.special[SPECIAL_PICK_CREATIVE];
        }

        return cost;
    }

    void __cdecl Add_Race_Options_Fields_EM(int8_t* baseline_flags, int16_t available_picks) {
        int16_t x_coords[4] = {80, 254, 442, 643};
        int16_t y_coords[5] = {72, 147, 222, 297, 372};
        int16_t block_sizes[7] = {22};

        fields::Clear_Fields();
        exit_ken = -1000;
        accept = fields::Add_Button_Field(506, 448, "", (s_animation_header*)accept_button_seg, "", 40);
        defalt = -1000;

        int16_t col_idx = 0;
        int16_t row_idx = 0;
        int16_t group_btn_idx = 0;

        do {
            if ((group_btn_idx % 4) == 0) {
                group_button[group_btn_idx] = -1000;
                group_btn_idx++;
            }

            int16_t sub_idx = 0;
            int16_t y = y_coords[row_idx] + 9;

            do {
                int16_t group_idx = group_btn_idx / 4;
                int16_t cost = 0;

                if (baseline_flags[group_idx + 1] != 0) {
                    cost = pick_rules.primary[group_idx]
                                        [baseline_flags[group_idx + 1] - 1]
                                            .cost;
                }

                int8_t ability_cost = pick_rules.primary[group_idx][sub_idx].cost;

                bool is_valid = false;
                if (ability_cost < 0) {
                    if (baseline_flags[group_idx + 1] == 1) {
                        is_valid = true;
                    }
                } else if (ability_cost > 0) {
                    if (baseline_flags[group_idx + 1] <= sub_idx + 1 || baseline_flags[group_idx + 1] < button_variable[group_idx]) {
                        is_valid = true;
                    }
                }

                if (is_valid && (ability_cost - cost <= available_picks)) {
                    group_button[group_btn_idx] = fields::Add_Hidden_Field(
                        x_coords[col_idx] - 20, y - 9, x_coords[col_idx + 1] - 50, y, "", 41);
                } else {
                    group_button[group_btn_idx] = -1000;
                }

                y += 13;
                sub_idx++;
                group_btn_idx++;
            } while (sub_idx < 3);

            row_idx++;
            if (row_idx >= 5) {
                row_idx = 0;
                col_idx++;
            }
        } while (group_btn_idx < 36);

        group_button[group_btn_idx++] = -1000;

        for (int16_t i = 0; i < GOVERNMENT_COUNT / 2; i++) {
            group_button[group_btn_idx++] = -1000;
        }

        int16_t arr_idx = 0;
        int16_t y2 = 72;
        int16_t opt_btn_idx = 0;

        col_idx++;
        group_btn_idx++;

        while (group_btn_idx < 64) {
            int16_t inner_cnt = 0;
            int16_t count = block_sizes[arr_idx];

            if (count > 0) {
                group_btn_idx += count;
                do {
                    if (opt_btn_idx >= SPECIAL_PICK_LARGE_HOME_WORLD &&
                        opt_btn_idx <= SPECIAL_PICK_ARTIFACTS_HOME_WORLD) {
                        option_button[opt_btn_idx] = -1000;
                    } else {
                        int8_t cost2 = pick_rules.special[opt_btn_idx];
                        bool valid2 = true;
                        const s_special_pick_identity& identity =
                            special_pick_identities[opt_btn_idx];
                        const bool baseline_selected =
                            baseline_flags[identity.trait] == identity.selected_value;

                        if (cost2 < 0) {
                            if (!baseline_selected) {
                                valid2 = false;
                            }
                        } else if (cost2 > 0) {
                            if (baseline_selected) {
                                valid2 = false;
                            }
                        }

                        if (valid2 && cost2 <= available_picks) {
                            option_button[opt_btn_idx] = fields::Add_Hidden_Field(
                                x_coords[col_idx] - 20, y2, x_coords[col_idx] + 152, y2 + 9, "", 41);
                        } else {
                            option_button[opt_btn_idx] = -1000;
                        }
                    }

                    y2 += 16;
                    opt_btn_idx++;
                    inner_cnt++;
                } while (inner_cnt < count);
            }

            y2 += 9;
            arr_idx++;
        }
    }

    void __cdecl Add_Race_Options_Fields() {
        const int16_t column_positions[4] = {0x50, 0x00FE, 0x01BA, 0x0283};
        const int16_t row_positions[5] = {0x48, 0x93, 0x00DE, 0x0129, 0x0174};

        int16_t row_index;
        int16_t column_index;
        int16_t group_button_index;
        int16_t option_button_index;
        int16_t top;
        int16_t left;
        int16_t right;
        int16_t bottom;

        if (raceopt::name_button > 0) {
            fields::Clear_Fields_Above(raceopt::name_button);
        } else {
            fields::Clear_Fields();
            fonts::Set_Font_Style(4, &raceopt::color_opt);
            raceopt::name_button = fields::Add_Continuous_String_Input_Field(
                0x00F8,
                0x000F,
                0x0096,
                0x000E,
                raceopt::cur_race_name,
                0x000F,
                0,
                (s_animation_header*)0,
                0,
                0);
        }

        raceopt::exit_ken = fields::Add_Hot_Key("\x1B");
        raceopt::accept = fields::Add_Button_Field(
            0x01FA,
            0x01C0,
            "",
            (s_animation_header*)raceopt::accept_button_seg,
            "a",
            0x0028);
        raceopt::defalt = fields::Add_Button_Field(
            0x004C,
            0x01C0,
            "",
            (s_animation_header*)raceopt::default_button_seg,
            "c",
            0x0028);

        row_index = 0;
        column_index = 0;
        group_button_index = 0;

        while (group_button_index < 0x24) {
            if ((group_button_index & 3) == 0) {
                raceopt::group_button[group_button_index] = static_cast<int16_t>(0xFC18);
                ++group_button_index;
            }

            bottom = static_cast<int16_t>(row_positions[row_index] + 9);
            right = static_cast<int16_t>(column_positions[column_index + 1] - 0x32);
            left = static_cast<int16_t>(column_positions[column_index] - 0x14);

            for (int16_t i = 0; i < 3; ++i) {
                raceopt::group_button[group_button_index] = fields::Add_Hidden_Field(
                    left,
                    static_cast<int16_t>(bottom - 9),
                    right,
                    bottom,
                    "",
                    0x0029);
                ++group_button_index;
                bottom = static_cast<int16_t>(bottom + 0x0D);
            }

            ++row_index;
            if (row_index >= 5) {
                row_index = 0;
                ++column_index;
            }
        }

        raceopt::group_button[group_button_index] = static_cast<int16_t>(0xFC18);
        ++group_button_index;

        bottom = static_cast<int16_t>(row_positions[row_index] + 9);
        right = static_cast<int16_t>(column_positions[column_index + 1] - 0x32);
        left = static_cast<int16_t>(column_positions[column_index] - 0x14);

        for (int16_t i = 0; i < GOVERNMENT_COUNT / 2; ++i) {
            raceopt::group_button[group_button_index] = fields::Add_Hidden_Field(
                left,
                static_cast<int16_t>(bottom - 9),
                right,
                bottom,
                "",
                0x0029);
            ++group_button_index;
            bottom = static_cast<int16_t>(bottom + 0x0D);
        }

        top = 0x48;
        ++group_button_index;
        row_index = 0;
        ++column_index;
        option_button_index = 0;

        while (group_button_index < 0x40) {
            int16_t remaining = 0x16;
            if (remaining > 0) {
                left = static_cast<int16_t>(column_positions[column_index] - 0x14);
                right = static_cast<int16_t>(column_positions[column_index] + 0x98);

                group_button_index = static_cast<int16_t>(group_button_index + remaining);

                do {
                    raceopt::option_button[option_button_index] = fields::Add_Hidden_Field(
                        left,
                        top,
                        right,
                        static_cast<int16_t>(top + 9),
                        "",
                        0x0029);
                    ++option_button_index;
                    --remaining;
                    top = static_cast<int16_t>(top + 0x10);
                } while (remaining != 0);
            }

            top = static_cast<int16_t>(top + 9);
            ++row_index;
        }
    }

    void __cdecl Convert_Flags_To_Player(s_player* player) {
        player->traits[TRAIT_CURRENT_GOVERNMENT] =
            selectable_governments[button_variable[9]];

        for (int16_t trait_index = 1; trait_index < 10; ++trait_index) {
            const int16_t selection = button_variable[trait_index - 1];
            if (selection == 0) {
                player->traits[trait_index] = 0;
            } else {
                player->traits[trait_index] =
                    pick_rules.primary[trait_index - 1][selection - 1].value;
            }
        }

        for (int trait = TRAIT_LOW_G_WORLD; trait < TRAIT_COUNT; ++trait) {
            player->traits[trait] = 0;
        }
        for (int special_pick = 0; special_pick < SPECIAL_PICK_COUNT;
             ++special_pick) {
            if (option_toggle[special_pick] != 0) {
                const s_special_pick_identity& identity =
                    special_pick_identities[special_pick];
                player->traits[identity.trait] = identity.selected_value;
            }
        }
    }

    void __cdecl Convert_Player_To_Flags(s_player* player) {
        int16_t trait_index = 0;

        button_variable[9] = Government_Pick_From_Value(
            player->traits[TRAIT_CURRENT_GOVERNMENT]);

        for (trait_index = 0; trait_index < 9; ++trait_index) {
            button_variable[trait_index] =
                static_cast<int16_t>(player->traits[trait_index + 1]);
        }

        for (int special_pick = 0; special_pick < SPECIAL_PICK_COUNT;
             ++special_pick) {
            const s_special_pick_identity& identity =
                special_pick_identities[special_pick];
            option_toggle[special_pick] =
                player->traits[identity.trait] == identity.selected_value;
        }
    }

} // namespace raceopt
