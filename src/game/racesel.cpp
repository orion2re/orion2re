#include "pch.h"
#include "fields.h"
#include "raceopt.h"

namespace RACESEL {

    static constexpr char LAST_RACE_FILENAME[] = "ORION2RE.RAC";

    static bool Is_Valid_Last_Race_Base_Race(uint8_t race_idx) {
        return race_idx < STOCK_RACE_COUNT;
    }

    int16_t custom_flag;
    int16_t displayed_race;
    int16_t old_race;
    int16_t race_button_status[14];
    uint8_t* race_specials[14] = {
        reinterpret_cast<uint8_t*>(&stock_race[0]),
        reinterpret_cast<uint8_t*>(&stock_race[1]),
        reinterpret_cast<uint8_t*>(&stock_race[2]),
        reinterpret_cast<uint8_t*>(&stock_race[3]),
        reinterpret_cast<uint8_t*>(&stock_race[4]),
        reinterpret_cast<uint8_t*>(&stock_race[5]),
        reinterpret_cast<uint8_t*>(&stock_race[6]),
        reinterpret_cast<uint8_t*>(&stock_race[7]),
        reinterpret_cast<uint8_t*>(&stock_race[8]),
        reinterpret_cast<uint8_t*>(&stock_race[9]),
        reinterpret_cast<uint8_t*>(&stock_race[10]),
        reinterpret_cast<uint8_t*>(&stock_race[11]),
        reinterpret_cast<uint8_t*>(&stock_race[12]),
        nullptr,
    };
    s_animation_header* race_button[14];
    char last_race_name[15];
    s_animation_header* last_race_seg;
    uint8_t color_flag[MAX_PLAYERS];
    uint8_t race_flag[14];
    char* temp_ruler_name;

    void __cdecl Convert_Custom_Race_Flags(uint8_t* custom_race_flags) {
        for (int16_t bonus_idx = 0; bonus_idx < 9; ++bonus_idx) {
            const uint8_t bonus_value = custom_race_flags[bonus_idx + 1];

            if (raceopt::pick_rules.primary[bonus_idx][raceopt::PRIMARY_CHOICE_PENALTY].value ==
                (int8_t)bonus_value) {
                custom_race_flags[bonus_idx + 1] = 1;
            } else if (raceopt::pick_rules.primary[bonus_idx][raceopt::PRIMARY_CHOICE_BONUS].value ==
                (int8_t)bonus_value) {
                custom_race_flags[bonus_idx + 1] = 2;
            } else if (raceopt::pick_rules.primary[bonus_idx]
                                             [raceopt::PRIMARY_CHOICE_MAJOR_BONUS]
                                                 .value ==
                (int8_t)bonus_value) {
                custom_race_flags[bonus_idx + 1] = 3;
            } else {
                custom_race_flags[bonus_idx + 1] = 0;
            }
        }
    }

    void __cdecl Set_Race_Select_Help() {
        MOX::help_entry_list[0].help_id = 630;
        MOX::help_entry_list[0].x1 = 351;
        MOX::help_entry_list[0].y1 = 90;
        MOX::help_entry_list[0].x2 = 473;
        MOX::help_entry_list[0].y2 = 134;

        MOX::help_entry_list[1].help_id = 631;
        MOX::help_entry_list[1].x1 = 351;
        MOX::help_entry_list[1].y1 = 138;
        MOX::help_entry_list[1].x2 = 473;
        MOX::help_entry_list[1].y2 = 182;

        MOX::help_entry_list[2].help_id = 632;
        MOX::help_entry_list[2].x1 = 351;
        MOX::help_entry_list[2].y1 = 186;
        MOX::help_entry_list[2].x2 = 473;
        MOX::help_entry_list[2].y2 = 230;

        MOX::help_entry_list[3].help_id = 633;
        MOX::help_entry_list[3].x1 = 351;
        MOX::help_entry_list[3].y1 = 234;
        MOX::help_entry_list[3].x2 = 473;
        MOX::help_entry_list[3].y2 = 278;

        MOX::help_entry_list[4].help_id = 634;
        MOX::help_entry_list[4].x1 = 351;
        MOX::help_entry_list[4].y1 = 282;
        MOX::help_entry_list[4].x2 = 473;
        MOX::help_entry_list[4].y2 = 326;

        MOX::help_entry_list[5].help_id = 635;
        MOX::help_entry_list[5].x1 = 351;
        MOX::help_entry_list[5].y1 = 330;
        MOX::help_entry_list[5].x2 = 473;
        MOX::help_entry_list[5].y2 = 374;

        MOX::help_entry_list[6].help_id = 636;
        MOX::help_entry_list[6].x1 = 351;
        MOX::help_entry_list[6].y1 = 378;
        MOX::help_entry_list[6].x2 = 473;
        MOX::help_entry_list[6].y2 = 422;

        MOX::help_entry_list[7].help_id = 637;
        MOX::help_entry_list[7].x1 = 477;
        MOX::help_entry_list[7].y1 = 90;
        MOX::help_entry_list[7].x2 = 599;
        MOX::help_entry_list[7].y2 = 134;

        MOX::help_entry_list[8].help_id = 638;
        MOX::help_entry_list[8].x1 = 477;
        MOX::help_entry_list[8].y1 = 138;
        MOX::help_entry_list[8].x2 = 599;
        MOX::help_entry_list[8].y2 = 182;

        MOX::help_entry_list[9].help_id = 639;
        MOX::help_entry_list[9].x1 = 477;
        MOX::help_entry_list[9].y1 = 186;
        MOX::help_entry_list[9].x2 = 599;
        MOX::help_entry_list[9].y2 = 230;

        MOX::help_entry_list[10].help_id = 640;
        MOX::help_entry_list[10].x1 = 477;
        MOX::help_entry_list[10].y1 = 234;
        MOX::help_entry_list[10].x2 = 599;
        MOX::help_entry_list[10].y2 = 278;

        MOX::help_entry_list[11].help_id = 641;
        MOX::help_entry_list[11].x1 = 477;
        MOX::help_entry_list[11].y1 = 282;
        MOX::help_entry_list[11].x2 = 599;
        MOX::help_entry_list[11].y2 = 326;

        MOX::help_entry_list[12].help_id = 642;
        MOX::help_entry_list[12].x1 = 477;
        MOX::help_entry_list[12].y1 = 330;
        MOX::help_entry_list[12].x2 = 599;
        MOX::help_entry_list[12].y2 = 374;

        MOX::help_entry_list[13].help_id = 643;
        MOX::help_entry_list[13].x1 = 477;
        MOX::help_entry_list[13].y1 = 378;
        MOX::help_entry_list[13].x2 = 599;
        MOX::help_entry_list[13].y2 = 422;

        MOX::help_entry_list[14].help_id = 644;
        MOX::help_entry_list[14].x1 = 0;
        MOX::help_entry_list[14].y1 = 0;
        MOX::help_entry_list[14].x2 = 639;
        MOX::help_entry_list[14].y2 = 479;

        fields::Set_Help_List(MOX::help_entry_list, 15);
    }

    void __cdecl Reload_Race_Selection_Screen() {
        allocate::Push_Block(MOX::screen_seg);

        FILE* file = fopen_case(LAST_RACE_FILENAME, "rb");
        if (file != nullptr) {
            // Load template for custom race (LBX entry 12) into the 14th slot
            race_specials[13] = (uint8_t*)farload::Far_Reload_Next_Data("RACESTUF.LBX", 7, MOX::screen_seg, STOCK_RACE_TRILARIAN, 1, TRAIT_COUNT);

            // Overwrite template with saved race data
            uint8_t saved_traits[TRAIT_COUNT];
            uint8_t saved_race_index = 0;
            char saved_race_name[sizeof(last_race_name)];
            if (fread(saved_traits, sizeof(saved_traits), 1, file) == 1 &&
                fread(&saved_race_index, sizeof(saved_race_index), 1, file) == 1 &&
                fread(saved_race_name, sizeof(saved_race_name), 1, file) == 1) {
                memcpy(race_specials[13], saved_traits, sizeof(saved_traits));
                Convert_Custom_Race_Flags(race_specials[13]);
                memcpy(last_race_name, saved_race_name, sizeof(last_race_name));
                last_race_name[sizeof(last_race_name) - 1] = '\0';
            }

            RUSS::Mox_Update();
            fclose(file);
        }

        for (int i = 0; i < 14; ++i) {
            race_button[i] = (s_animation_header*)farload::Far_Reload_Next("RACESEL.LBX", i + 1, MOX::screen_seg);
            RUSS::Mox_Update();
        }

        last_race_seg = (s_animation_header*)farload::Far_Reload_Next("patch13.lbx", 1, MOX::screen_seg);
    }

    int16_t Race_Selection_Screen(s_player* plr) {
        int16_t field_ids[14];
        int16_t hotkey_field;
        char msg_buffer[100];
        int16_t done_flag = 0;

        fields::Disable_Cancel();
        fields::Deactivate_Help_List();
        fields::Clear_Fields();
        ERIC::Fast_Fade_Out();
        fonts::Load_Palette(9, 0, 0xFF);

        custom_flag = 0;
        displayed_race = STOCK_RACE_ELERIAN;
        for (int i = 0; i < 14; ++i) {
            race_button_status[i] = 0;
        }
        race_button_status[STOCK_RACE_ELERIAN] = 1;

        Reload_Race_Selection_Screen();
        RACESEL::Set_Race_Select_Help();
        video::Set_Page_Back();
        auto raceopt_anim = (s_animation_header*)buffer::Buffer_Reload("RACEOPT.LBX", 0, MOX::global_cache_seg);
        RUSS::Mox_Update();
        animate::Draw(0, 0, raceopt_anim);
        video::Copy_Back_To_Off();

        if (MOX::game_type == 0 || MOX::game_type == 1) {
            hotkey_field = fields::Add_Hot_Key("\x1B");
        } else {
            hotkey_field = -1000;
        }

        RUSS::Mox_Update();
        for (int i = 0; i < 14; ++i) {
            field_ids[i] = fields::Add_Radio_Button_Field(
                (i / 7) * 126 + 351, (i % 7) * 48 + 90, "", race_button[i], &race_button_status[i], "", 40);
        }

        fields::Assign_Auto_Function(Draw_Race_Selection_Screen, 2);
        old_race = 0;
        video::Set_Page_Off();
        Draw_Race_Selection_Screen();
        palstore::Darken_Palette(100);
        palette::Clear_Palette_Changes(0, 0xFF);
        RUSS::Mox_Sync_Update();
        ERIC::Fast_Fade_In();

        while (true) {
            int16_t input_field = fields::Get_Input();
            int16_t scan_idx = fields::Scan_Input();
            displayed_race = scan_idx - field_ids[0];
            if (displayed_race < 0 || displayed_race > 13) {
                displayed_race = old_race;
            }

            for (int i = STOCK_RACE_ALKARI; i < STOCK_RACE_COUNT; ++i) {
                if (input_field == field_ids[i] && race_flag[i] == 0) {
                    uint8_t can_request =
                        (MOX::game_type == 0 || MOX::game_type == 1) ? 1 : NETSTART::Request_Race((uint8_t)i);

                    if (!can_request) {
                        KEN::Ken_Get_Text_Message(72, msg_buffer, sizeof(msg_buffer));
                        GENDRAW::Message_Box(msg_buffer);
                        video::Set_Page_Back();
                        auto anim = (s_animation_header*)buffer::Buffer_Reload("RACEOPT.LBX", 0, MOX::global_cache_seg);
                        animate::Draw(0, 0, anim);
                        video::Copy_Back_To_Off();
                    } else {
                        if (custom_flag == 0) {
                            timer::Mark_Time();
                            timer::Release_Time(5);
                            fields::Clear_Fields();
                            struct_::Copy_Structure(plr->traits, race_specials[i], sizeof(plr->traits));
                            plr->race = (uint8_t)i;
                            strlcpy(plr->race_name, MOX::race_names[i], sizeof(plr->race_name));
                            fields::Deactivate_Help_List();

                            int screen_state;
                            do {
                                screen_state = Naming_Popup(plr->name, sizeof(plr->name), i);
                                video::Set_Page_Back();
                                auto anim = (s_animation_header*)buffer::Buffer_Reload("RACEOPT.LBX", 0, MOX::global_cache_seg);
                                animate::Draw(0, 0, anim);
                                video::Copy_Back_To_Off();
                                RUSS::Mox_Sync_Update();
                                old_race = i;

                                if (screen_state == 1) {
                                    screen_state += Flag_Screen(&plr->color);
                                    if (screen_state != 2) {
                                        Reload_Race_Selection_Screen();
                                    }
                                }
                            } while (screen_state == 1);

                            if (screen_state == 2) {
                                for (int trait_idx = TRAIT_POP_GROWTH_BONUS; trait_idx < TRAIT_LOW_G_WORLD; ++trait_idx) {
                                    if (plr->traits[trait_idx] != 0) {
                                        plr->traits[trait_idx] = raceopt::pick_rules
                                                                     .primary[trait_idx - 1][plr->traits[trait_idx] - 1]
                                                                     .value;
                                    }
                                }
                                done_flag = 1;
                                race_flag[i] = 1;
                            } else {
                                if (MOX::game_type != 0 && MOX::game_type != 1) {
                                    Unrequest_Race(i);
                                } else {
                                    race_flag[i] = 0;
                                }
                                RACESEL::Set_Race_Select_Help();
                                video::Set_Page_Back();
                                auto anim = (s_animation_header*)buffer::Buffer_Reload("RACEOPT.LBX", 0, MOX::global_cache_seg);
                                animate::Draw(0, 0, anim);
                                video::Copy_Back_To_Off();
                                old_race = 0;
                                for (int j = 0; j < 14; ++j) race_button_status[j] = 0;
                                race_button_status[displayed_race] = 1;
                                if (MOX::game_type == 0 || MOX::game_type == 1) hotkey_field = fields::Add_Hot_Key("\x1B");
                                for (int j = 0; j < 14; ++j) {
                                    field_ids[j] = fields::Add_Radio_Button_Field((j / 7) * 126 + 351, (j % 7) * 48 + 90, "", race_button[j], &race_button_status[j], "", 40);
                                }
                                video::Set_Page_Off();
                                Draw_Race_Selection_Screen();
                                RUSS::Mox_Sync_Update();
                                fields::Assign_Auto_Function(Draw_Race_Selection_Screen, 2);
                            }
                        } else {
                            struct_::Copy_Structure(plr->traits, race_specials[i], sizeof(plr->traits));
                            plr->race = (uint8_t)i;
                            strlcpy(plr->race_name, MOX::race_names[i], sizeof(plr->race_name));
                            fields::Deactivate_Auto_Function();
                            fields::Clear_Fields();
                            if (Racial_Option_Screen(plr) == 1) {
                                done_flag = 1;
                                race_flag[i] = 1;
                            } else {
                                if (MOX::game_type != 0 && MOX::game_type != 1) {
                                    Unrequest_Race(i);
                                } else {
                                    race_flag[i] = 0;
                                }
                                allocate::Pop_Block(MOX::screen_seg);
                                Reload_Race_Selection_Screen();
                                video::Set_Page_Back();
                                auto anim = (s_animation_header*)buffer::Buffer_Reload("RACEOPT.LBX", 0, MOX::global_cache_seg);
                                animate::Draw(0, 0, anim);
                                video::Copy_Back_To_Off();
                                if (MOX::game_type == 0 || MOX::game_type == 1) hotkey_field = fields::Add_Hot_Key("\x1B");
                                for (int j = 0; j < STOCK_RACE_COUNT; ++j) {
                                    field_ids[j] = fields::Add_Radio_Button_Field((j / 7) * 126 + 351, (j % 7) * 48 + 90, "", race_button[j], &race_button_status[j], "", 40);
                                }
                                field_ids[13] = fields::Add_Radio_Button_Field(477, 378, "", race_button[13], &race_button_status[13], "", 40);
                                fields::Assign_Auto_Function(Draw_Race_Selection_Screen, 2);
                                RACESEL::Set_Race_Select_Help();
                                custom_flag = 0;
                            }
                        }
                    }
                }
            }

            if (input_field == hotkey_field) {
                if (custom_flag != 1) {
                    fields::Clear_Fields();
                    fields::Deactivate_Auto_Function();
                    allocate::Pop_Block(MOX::screen_seg);
                    return 0;
                }
                custom_flag = 0;
                fields::Clear_Fields_Above(field_ids[STOCK_RACE_TRILARIAN]);
                field_ids[13] = fields::Add_Radio_Button_Field(477, 378, "", race_button[13], &race_button_status[13], "", 40);
            }

            if (input_field == field_ids[13]) {
                FILE* race_file = fopen_case(LAST_RACE_FILENAME, "rb");
                if (custom_flag == 0) {
                    custom_flag = 1;
                    fields::Clear_Fields_Above(field_ids[STOCK_RACE_TRILARIAN]);
                    if (!race_file) {
                        field_ids[13] = -1000;
                    } else {
                        field_ids[13] = fields::Add_Radio_Button_Field(477, 378, "", (s_animation_header*)last_race_seg, &race_button_status[13], "", 40);
                        fclose(race_file);
                    }
                } else if (race_file) {
                    uint8_t loaded_race;
                    uint8_t discarded_traits[TRAIT_COUNT];
                    char loaded_race_name[sizeof(plr->race_name)];
                    const bool loaded =
                        fread(discarded_traits, sizeof(discarded_traits), 1, race_file) == 1 &&
                        fread(&loaded_race, sizeof(loaded_race), 1, race_file) == 1 &&
                        fread(loaded_race_name, sizeof(loaded_race_name), 1, race_file) == 1;

                    if (!loaded || !Is_Valid_Last_Race_Base_Race(loaded_race)) {
                        fclose(race_file);
                        field_ids[13] = -1000;
                        continue;
                    }

                    struct_::Copy_Structure(plr->traits, race_specials[13], sizeof(plr->traits));
                    plr->race = loaded_race;
                    uint8_t can_request =
                        (MOX::game_type == 0 || MOX::game_type == 1) ? 1 : NETSTART::Request_Race(loaded_race);

                    if (!can_request) {
                        fclose(race_file);
                        KEN::Ken_Get_Text_Message(72, msg_buffer, sizeof(msg_buffer));
                        GENDRAW::Message_Box(msg_buffer);
                        video::Set_Page_Back();
                        auto anim = (s_animation_header*)buffer::Buffer_Reload("RACEOPT.LBX", 0, MOX::global_cache_seg);
                        animate::Draw(0, 0, anim);
                        video::Copy_Back_To_Off();
                    } else {
                        memcpy(plr->race_name, loaded_race_name, sizeof(plr->race_name));
                        plr->race_name[sizeof(plr->race_name) - 1] = '\0';
                        fields::Deactivate_Auto_Function();
                        fields::Clear_Fields();
                        old_race = loaded_race;
                        fclose(race_file);
                        if (Racial_Option_Screen(plr) == 1) {
                            done_flag = 1;
                            race_flag[loaded_race] = 1;
                        } else {
                            if (MOX::game_type != 0 && MOX::game_type != 1) {
                                Unrequest_Race(loaded_race);
                            } else {
                                race_flag[loaded_race] = 0;
                            }
                            allocate::Pop_Block(MOX::screen_seg);
                            Reload_Race_Selection_Screen();
                            video::Set_Page_Back();
                            auto anim = (s_animation_header*)buffer::Buffer_Reload("RACEOPT.LBX", 0, MOX::global_cache_seg);
                            animate::Draw(0, 0, anim);
                            video::Copy_Back_To_Off();
                            if (MOX::game_type == 0 || MOX::game_type == 1) hotkey_field = fields::Add_Hot_Key("\x1B");
                            for (int j = 0; j < STOCK_RACE_COUNT; ++j) {
                                field_ids[j] = fields::Add_Radio_Button_Field((j / 7) * 126 + 351, (j % 7) * 48 + 90, "", race_button[j], &race_button_status[j], "", 40);
                            }
                            field_ids[13] = fields::Add_Radio_Button_Field(477, 378, "", race_button[13], &race_button_status[13], "", 40);
                            fields::Assign_Auto_Function(Draw_Race_Selection_Screen, 2);
                            RACESEL::Set_Race_Select_Help();
                            custom_flag = 0;
                        }
                    }
                }
            }

            if (done_flag) break;

            if (MOX::FULL_DRAW_SCREEN_FLAG != 0) {
                video::Set_Page_Back();
                graphics::Fill(0, 0, 639, 479, 0);
                auto anim = (s_animation_header*)buffer::Buffer_Reload("RACEOPT.LBX", 0, MOX::global_cache_seg);
                animate::Draw(0, 0, anim);
                video::Copy_Back_To_Off();
                MOX::FULL_DRAW_SCREEN_FLAG = 0;
            }

            video::Set_Page_Off();
            Draw_Race_Selection_Screen();
            RUSS::Mox_Sync_Update();
        }

        fields::Clear_Fields();
        fields::Deactivate_Auto_Function();
        allocate::Pop_Block(MOX::screen_seg);
        ERIC::Fast_Fade_Out();
        return 1;
    }

    int16_t __cdecl Racial_Option_Screen(s_player* player) {
        int16_t finished;
        int16_t input_id;
        int16_t saved_button_variables[10];
        char warning_text[80];
        char* player_race_name = player->race_name;

        raceopt::name_button = 0;
        raceopt::Set_Race_Option_Help_List();
        fields::Clear_Fields();
        raceopt::Convert_Player_To_Flags(player);
        raceopt::Reload_Racial_Option_Screen();
        memcpy(raceopt::cur_race_name, player_race_name, strlen(player_race_name) + 1);
        raceopt::Calculate_Picks();

        fields::Assign_Auto_Function(raceopt::Draw_Racial_Option_Screen, 1);

        raceopt::color_opt.colors[0] = 0x8E;
        raceopt::color_opt.colors[1] = 0x9D;
        raceopt::color_opt.colors[2] = 0x9C;
        raceopt::color_opt.colors[3] = 0x9D;
        raceopt::color_opt.colors[4] = 0x9D;
        raceopt::color_opt.colors[5] = 0x9D;
        raceopt::color_opt.colors[6] = 0x9D;
        raceopt::color_opt.colors[7] = 0x9D;

        RUSS::Mox_Update();
        fonts::Set_Font_Style(4, &raceopt::color_opt);
        raceopt::Add_Race_Options_Fields();
        memcpy(saved_button_variables, raceopt::button_variable, sizeof(saved_button_variables));
        RUSS::Mox_Update();
        raceopt::Add_Race_Options_Fields();
        RUSS::Mox_Update();

        video::Set_Page_Back();
        raceopt::Draw_Race_Option_Background();
        video::Copy_Back_To_Off();
        video::Set_Page_Off();
        raceopt::Draw_Racial_Option_Screen();
        RUSS::Mox_Sync_Update();

        while (true) {
            finished = 0;
            MOX::FULL_DRAW_SCREEN_FLAG = 0;
            input_id = fields::Get_Input();

            if (input_id != 0) {
                for (int32_t group_button_index = 0; group_button_index < 0x24; ++group_button_index) {
                    if (raceopt::group_button[group_button_index] == input_id) {
                        int16_t selected_group = static_cast<int16_t>(group_button_index / 4);

                        if (selected_group == 1 && raceopt::option_toggle[9] == 1) {
                            KEN::Ken_Get_Text_Message(0x49, warning_text, sizeof(warning_text));
                            GENDRAW::Warning_Box(warning_text);
                            video::Set_Page_Back();
                            raceopt::Draw_Race_Option_Background();
                            video::Copy_Back_To_Off();
                            video::Set_Page_Off();
                            raceopt::Draw_Racial_Option_Screen();
                            RUSS::Mox_Sync_Update();
                            raceopt::button_variable[selected_group] = 0;
                        } else {
                            int16_t selected_value = static_cast<int16_t>(group_button_index % 4);

                            if (saved_button_variables[selected_group] == selected_value) {
                                raceopt::button_variable[selected_group] = 0;
                                raceopt::Calculate_Picks();
                            } else {
                                int32_t pick_total =
                                    (int32_t)raceopt::pick_rules.primary[selected_group]
                                                                   [selected_value - 1]
                                                                       .cost +
                                    static_cast<int32_t>(raceopt::negative_picks);

                                if (pick_total < raceopt::pick_rules.negative_limit) {
                                    KEN::Ken_Get_Text_Message(0x4A, warning_text, sizeof(warning_text));
                                    GENDRAW::Warning_Box(warning_text);
                                    video::Set_Page_Back();
                                    raceopt::Draw_Race_Option_Background();
                                    video::Copy_Back_To_Off();
                                    video::Set_Page_Off();
                                    raceopt::Draw_Racial_Option_Screen();
                                    RUSS::Mox_Sync_Update();
                                    raceopt::button_variable[selected_group] = saved_button_variables[selected_group];
                                    raceopt::Calculate_Picks();
                                } else {
                                    raceopt::button_variable[selected_group] = selected_value;
                                }
                            }
                        }
                    }
                }

                for (int32_t group_button_index = 0x25; group_button_index < 0x29; ++group_button_index) {
                    if (raceopt::group_button[group_button_index] == input_id) {
                        raceopt::Calculate_Picks();

                        if (group_button_index - raceopt::button_variable[9] == 0x25) {
                            raceopt::button_variable[9] = static_cast<int16_t>(group_button_index - 0x25);
                        } else {
                            int32_t pick_total =
                                (int32_t)raceopt::pick_rules.government[group_button_index - 0x25] +
                                static_cast<int32_t>(raceopt::negative_picks);

                            if (pick_total < raceopt::pick_rules.negative_limit) {
                                KEN::Ken_Get_Text_Message(0x4A, warning_text, sizeof(warning_text));
                                GENDRAW::Warning_Box(warning_text);
                                video::Set_Page_Back();
                                raceopt::Draw_Race_Option_Background();
                                video::Copy_Back_To_Off();
                                video::Set_Page_Off();
                                raceopt::Draw_Racial_Option_Screen();
                                RUSS::Mox_Sync_Update();
                                raceopt::button_variable[9] = saved_button_variables[9];
                                raceopt::Calculate_Picks();
                            } else {
                                raceopt::button_variable[9] = static_cast<int16_t>(group_button_index - 0x25);
                            }
                        }
                    }
                }

                for (int32_t option_index = 0; option_index < 0x16; ++option_index) {
                    if (raceopt::option_button[option_index] == input_id) {
                        raceopt::option_toggle[option_index] =
                            static_cast<int16_t>(1 - raceopt::option_toggle[option_index]);

                        if (raceopt::option_toggle[option_index] == 0) {
                            raceopt::Calculate_Picks();
                        } else {
                            raceopt::Calculate_Picks();

                            if (raceopt::negative_picks < raceopt::pick_rules.negative_limit) {
                                KEN::Ken_Get_Text_Message(0x4A, warning_text, sizeof(warning_text));
                                GENDRAW::Warning_Box(warning_text);
                                video::Set_Page_Back();
                                raceopt::Draw_Race_Option_Background();
                                video::Copy_Back_To_Off();
                                video::Set_Page_Off();
                                raceopt::Draw_Racial_Option_Screen();
                                RUSS::Mox_Sync_Update();
                                raceopt::option_toggle[option_index] = 0;
                            }

                            if (option_index == raceopt::SPECIAL_PICK_RICH_HOME_WORLD) {
                                raceopt::option_toggle[raceopt::SPECIAL_PICK_POOR_HOME_WORLD] = 0;
                            } else if (option_index == raceopt::SPECIAL_PICK_POOR_HOME_WORLD) {
                                raceopt::option_toggle[raceopt::SPECIAL_PICK_RICH_HOME_WORLD] = 0;
                            } else if (option_index == raceopt::SPECIAL_PICK_LOW_GRAVITY) {
                                raceopt::option_toggle[raceopt::SPECIAL_PICK_HIGH_GRAVITY] = 0;
                            } else if (option_index == raceopt::SPECIAL_PICK_HIGH_GRAVITY) {
                                raceopt::option_toggle[raceopt::SPECIAL_PICK_LOW_GRAVITY] = 0;
                            } else if (option_index == raceopt::SPECIAL_PICK_LITHOVORE) {
                                raceopt::option_toggle[raceopt::SPECIAL_PICK_CYBERNETIC] = 0;
                                raceopt::button_variable[1] = 0;
                            } else if (option_index == raceopt::SPECIAL_PICK_CYBERNETIC) {
                                raceopt::option_toggle[raceopt::SPECIAL_PICK_LITHOVORE] = 0;
                            } else if (option_index == raceopt::SPECIAL_PICK_REPULSIVE) {
                                raceopt::option_toggle[raceopt::SPECIAL_PICK_CHARISMATIC] = 0;
                            } else if (option_index == raceopt::SPECIAL_PICK_CHARISMATIC) {
                                raceopt::option_toggle[raceopt::SPECIAL_PICK_REPULSIVE] = 0;
                            } else if (option_index == raceopt::SPECIAL_PICK_UNCREATIVE) {
                                raceopt::option_toggle[raceopt::SPECIAL_PICK_CREATIVE] = 0;
                            } else if (option_index == raceopt::SPECIAL_PICK_CREATIVE) {
                                raceopt::option_toggle[raceopt::SPECIAL_PICK_UNCREATIVE] = 0;
                            }
                        }
                    }
                }

                if (input_id == raceopt::defalt) {
                    for (int32_t i = 0; i < 9; ++i) {
                        raceopt::button_variable[i] = 0;
                    }

                    raceopt::button_variable[9] = 1;

                    for (int32_t i = 0; i < 0x16; ++i) {
                        raceopt::option_toggle[i] = 0;
                    }
                }

                if (input_id == raceopt::exit_ken) {
                    fields::Clear_Fields();
                    fields::Deactivate_Auto_Function();
                    allocate::Pop_Block(MOX::screen_seg);
                    return 0;
                }

                if (input_id == raceopt::accept) {
                    if (raceopt::picks < 0) {
                        KEN::Ken_Get_Text_Message(0x4B, warning_text, sizeof(warning_text));
                        GENDRAW::Warning_Box(warning_text);
                        video::Set_Page_Back();
                        raceopt::Draw_Race_Option_Background();
                        video::Copy_Back_To_Off();
                        video::Set_Page_Off();
                        raceopt::Draw_Racial_Option_Screen();
                        RUSS::Mox_Sync_Update();
                    } else {
                        raceopt::Convert_Flags_To_Player(player);
                        fields::Assign_Auto_Function(SCORE::nullsub, 2);

                        video::Set_Page_Back();
                        animate::Draw(0, 0, (s_animation_header*)buffer::Buffer_Reload("RACEOPT.LBX", 4, MOX::global_cache_seg));
                        animate::Draw(0, 0x1AE, (s_animation_header*)buffer::Buffer_Reload("RACEOPT.LBX", 6, MOX::global_cache_seg));
                        video::Set_Page_Off();

                        int16_t popup_result;
                        while (true) {
                            fields::Deactivate_Help_List();
                            popup_result = RACESEL::Naming_Popup(player->name, sizeof(player->name), static_cast<int16_t>(player->race));

                            video::Set_Page_Back();
                            animate::Draw(0, 0, (s_animation_header*)buffer::Buffer_Reload("RACEOPT.LBX", 4, MOX::global_cache_seg));
                            animate::Draw(0, 0x1AE, (s_animation_header*)buffer::Buffer_Reload("RACEOPT.LBX", 6, MOX::global_cache_seg));
                            video::Copy_Back_To_Off();
                            RUSS::Mox_Sync_Update();

                            if (popup_result != 1) {
                                break;
                            }

                            popup_result = static_cast<int16_t>(
                                popup_result + RACESEL::Flag_Screen((uint8_t*)&player->color));

                            if (popup_result != 1) {
                                break;
                            }
                        }

                        if (popup_result == 2) {
                            FILE* race_file = fopen_case(LAST_RACE_FILENAME, "wb");
                            if (race_file != nullptr) {
                                fwrite(player->traits, sizeof(player->traits), 1, race_file);
                                fwrite(&player->race, sizeof(player->race), 1, race_file);
                                memcpy(player_race_name, raceopt::cur_race_name, strlen(raceopt::cur_race_name) + 1);
                                fwrite(player_race_name, sizeof(player->race_name), 1, race_file);
                                fclose(race_file);
                            }

                            MOX::current_screen = MOX::return_screen;
                            ERIC::Fast_Fade_Out();
                            finished = 1;
                        } else {
                            raceopt::Set_Race_Option_Help_List();
                            raceopt::name_button = 0;
                            fields::Assign_Auto_Function(raceopt::Draw_Racial_Option_Screen, 2);
                        }
                    }
                }

                memcpy(saved_button_variables, raceopt::button_variable, sizeof(saved_button_variables));
                raceopt::Calculate_Picks();
            }

            if (MOX::FULL_DRAW_SCREEN_FLAG != 0) {
                video::Set_Page_Back();
                raceopt::Draw_Race_Option_Background();
                video::Copy_Back_To_Off();
            }

            if (finished != 0) {
                fields::Deactivate_Help_List();
                fields::Deactivate_Auto_Function();
                allocate::Pop_Block(MOX::screen_seg);
                return 1;
            }

            raceopt::Add_Race_Options_Fields();
            video::Set_Page_Off();
            raceopt::Draw_Racial_Option_Screen();
            RUSS::Mox_Sync_Update();
        }
    }

    /**
 * @brief Cancels a race request for the local player and notifies the host.
 * @param race_idx The index of the race being unrequested.
 */
    void __cdecl Unrequest_Race(uint8_t race_idx) {
        NETMOX::net.race_selection_changed = 1;
        RACESEL::race_flag[race_idx] = 0;

        // Send network message to the host about updated race selection status
        NETMOX::Mox_Send_Message(NETMOX::net.host_election_id, NET_PACKET_PLAYER_RACE_DESELECT_REQUEST, &race_idx, sizeof(race_idx));
    }

    void __cdecl Draw_Naming_Popup() {
        s_animation_header* anim;

        if (MOX::FULL_DRAW_SCREEN_FLAG != 0) {
            video::Set_Page_Off();
            graphics::Fill(0, 0, 0x27F, 0x1DF, 0);
            anim = (s_animation_header*)buffer::Buffer_Reload("RACEOPT.LBX", 0, MOX::global_cache_seg);
            animate::Draw(0, 0, anim);
        }
    }

    void __cdecl Message_Build(int16_t* message_count, int16_t split_at_count, char* output_text, uint32_t output_text_size) {
        int16_t next_count = static_cast<int16_t>(*message_count + 1);
        *message_count = next_count;

        if (split_at_count == next_count) {
            if (next_count > 1) {
                char separator_text[80];

                KEN::Ken_Get_Text_Message(0x68, separator_text, sizeof(separator_text));
                strlcat(output_text, separator_text, output_text_size);
            }
        } else if (next_count > 1) {
            strlcat(output_text, DESIGN::s_comma_space, output_text_size);
        }
    }

    int8_t __cdecl Naming_Popup(char* output_name, uint32_t output_name_size, int16_t race_idx) {
        char original_name[20];
        char popup_text[80];

        fields::Clear_Fields();
        RUSS::Mox_Sync_Update();
        fields::Assign_Auto_Function(RACESEL::Draw_Naming_Popup, 1);
        allocate::Push_Block(MOX::screen_seg);

        RACESEL::temp_ruler_name = (char*)farload::Far_Reload_Next_Data(
            "RACENAME.LBX",
            0,
            MOX::screen_seg,
            (int32_t)game_random::Random(8) + (int32_t)race_idx * 8 - 1,
            1,
            sizeof(original_name));

        strlcpy(original_name, RACESEL::temp_ruler_name, sizeof(original_name));

        KEN::Ken_Get_Text_Message(0x6A, popup_text, sizeof(popup_text));

        const int16_t center_y = namestar::Star_Name_Popup_Screen_Center_Y();
        const int16_t center_x = namestar::Star_Name_Popup_Screen_Center_X();

        const int8_t popup_result = namestar::Input_Box_Popup_Animated(
            RACESEL::temp_ruler_name,
            sizeof(original_name),
            sizeof(original_name),
            0x66,
            popup_text,
            center_x,
            center_y);

        strlcpy(
            output_name,
            (RACESEL::temp_ruler_name[0] != '\0') ? RACESEL::temp_ruler_name : original_name,
            output_name_size);

        allocate::Pop_Block(MOX::screen_seg);
        fields::Clear_Fields();

        return popup_result;
    }

    void __cdecl Draw_Flag_Screen() {
        s_animation_header* animation =
            (s_animation_header*)buffer::Buffer_Reload("racesel.lbx", 0x20, MOX::global_cache_seg);

        animate::Draw(0x37, 0x43, animation);

        void* cache_seg = MOX::global_cache_seg;

        for (int16_t race_index = 0; race_index < MAX_PLAYERS; ++race_index) {
            if (RACESEL::color_flag[race_index] == 0) {
                int16_t x_adjust = 0;
                if (race_index == 3 || race_index == 7) {
                    x_adjust = -1;
                }

                animation = (s_animation_header*)buffer::Buffer_Reload(
                    "racesel.lbx",
                    static_cast<int32_t>(race_index) + static_cast<int32_t>(RACESEL::old_race) * MAX_PLAYERS + 0x22,
                    cache_seg);

                const int16_t y =
                    static_cast<int16_t>((race_index / 4) * 0x88 + 0x91);
                const int16_t x =
                    static_cast<int16_t>(((race_index & 3) * 0x7c) + x_adjust + 0x60);

                animate::Draw(x, y, animation);
                cache_seg = MOX::global_cache_seg;
            }
        }
    }

    int16_t __cdecl Flag_Screen(uint8_t* selected_color) {
        int16_t cancel_field_id;
        int16_t selection_made;
        int16_t field_ids[MAX_PLAYERS];
        char message_text[0x50];

        RUSS::Mox_Sync_Update();

        s_animation_header* palette = (s_animation_header*)buffer::Buffer_Reload(
            "racesel.lbx",
            0x20,
            MOX::global_cache_seg);
        animate::Draw_Palette(palette);

        RUSS::Mox_Sync_Update();
        fields::Assign_Auto_Function(RACESEL::Draw_Flag_Screen, 2);
        RACESEL::Set_Flag_Screen_Help();

        selection_made = 0;

        while (true) {
            fields::Clear_Fields();

            for (int16_t flag_index = 0; flag_index < MAX_PLAYERS; ++flag_index) {
                int16_t column_adjust = 0;
                if (flag_index == 3 || flag_index == 7) {
                    column_adjust = -1;
                }

                if (RACESEL::color_flag[flag_index] == 0) {
                    const int16_t row_offset = static_cast<int16_t>((flag_index / 4) * 0x88);
                    const int16_t column_offset = static_cast<int16_t>((flag_index % 4) * 0x7C + column_adjust);

                    field_ids[flag_index] = fields::Add_Hidden_Field(
                        static_cast<int16_t>(column_offset + 0x60),
                        static_cast<int16_t>(row_offset + 0x90),
                        static_cast<int16_t>(column_offset + 0xB3),
                        static_cast<int16_t>(row_offset + 0xF2),
                        "",
                        0x29);
                } else {
                    field_ids[flag_index] = -1000;
                }
            }

            cancel_field_id = fields::Add_Hot_Key("\x1B");
            const int16_t input_field_id = fields::Get_Input();

            for (int16_t flag_index = 0; flag_index < MAX_PLAYERS; ++flag_index) {
                if (field_ids[flag_index] == input_field_id) {
                    uint8_t accepted;

                    if (MOX::game_type == 0 || MOX::game_type == 1) {
                        accepted = 1;
                    } else {
                        accepted = NETSTART::Request_Banner_Color(static_cast<uint8_t>(flag_index));
                    }

                    if (accepted != 0) {
                        selection_made = 1;
                        *selected_color = static_cast<uint8_t>(flag_index);
                        RACESEL::color_flag[flag_index] = 1;
                    } else {
                        KEN::Ken_Get_Text_Message(0x69, message_text, sizeof(message_text));
                        GENDRAW::Message_Box(message_text);
                    }
                }
            }

            if (cancel_field_id == input_field_id) {
                fields::Clear_Fields();
                fields::Deactivate_Auto_Function();
                MOX::hot_seat_flag = 0;
                return 0;
            }

            if (selection_made != 0) {
                fields::Clear_Fields();
                fields::Deactivate_Auto_Function();
                MOX::cmbt_mouse_list_clock.icon = 7;
                mouse::Set_Mouse_List(&MOX::cmbt_mouse_list_clock, 1);
                video::Set_Page_Off();
                RACESEL::Draw_Flag_Screen();
                RUSS::Mox_Sync_Update();
                return 1;
            }

            video::Set_Page_Off();
            RACESEL::Draw_Flag_Screen();
            RUSS::Mox_Sync_Update();
        }
    }

    void __cdecl Set_Flag_Screen_Help() {
        MOX::help_entry_list[0].help_id = 0x02A4;
        MOX::help_entry_list[0].x1 = 0;
        MOX::help_entry_list[0].x2 = 0x027F;
        MOX::help_entry_list[0].y2 = 0x01DF;
        MOX::help_entry_list[0].y1 = 0;
        fields::Set_Help_List(MOX::help_entry_list, 1);
    }

    void __cdecl Draw_Race_Selection_Screen(void) {
        s_colors shadow_colors = {{0x78, 0x7c, 0x7c, 0x7c, 0x7c, 0x7c, 0x7c, 0x7c}};
        int16_t line_index = 0;
        int16_t total_lines = 0;
        char bonus_text[16];
        char paragraph[1024];
        char text_message[100];
        s_animation_header* animation;

        paragraph[0] = '\0';

        if (RACESEL::custom_flag == 0) {
            animation = (s_animation_header*)buffer::Buffer_Reload("RACESEL.LBX", 0x21, MOX::global_cache_seg);
            animate::Draw(0x16e, 0x34, animation);
        } else {
            animation = (s_animation_header*)buffer::Buffer_Reload("RACESEL.LBX", 0x1d, MOX::global_cache_seg);
            animate::Draw(0x16f, 0x30, animation);
        }

        if (RACESEL::old_race != RACESEL::displayed_race) {
            RACESEL::race_button_status[static_cast<int32_t>(RACESEL::old_race)] = 0;
            RACESEL::race_button_status[static_cast<int32_t>(RACESEL::displayed_race)] = 1;
        }

        if (RACESEL::race_flag[static_cast<int32_t>(RACESEL::displayed_race)] == 0) {
            animation = (s_animation_header*)buffer::Buffer_Reload(
                "RACESEL.LBX",
                static_cast<int32_t>(RACESEL::displayed_race) + 0x0f,
                MOX::global_cache_seg);
            animate::Draw(0x36, 0x3f, animation);
        } else {
            fonts::Set_Font_Style_Shadow_Down(4, &shadow_colors);
            KEN::Ken_Get_Text_Message(0x48, text_message, sizeof(text_message));
            BILL::_Print_Paragraph(0x3c, 0xaf, 0x118, text_message, 2);
        }

        RACESEL::old_race = RACESEL::displayed_race;

        if ((((RACESEL::displayed_race == 13) && (RACESEL::race_specials[13] != nullptr)) ||
                ((RACESEL::displayed_race != 13) &&
                    (RACESEL::race_flag[static_cast<int32_t>(RACESEL::displayed_race)] == 0))) &&
            (((RACESEL::custom_flag == 0) && (RACESEL::displayed_race != 13)) ||
                ((RACESEL::custom_flag == 1) && (RACESEL::displayed_race == 13)))) {
            uint8_t* const race_specials = RACESEL::race_specials[static_cast<int32_t>(RACESEL::displayed_race)];

            line_index = 0;
            total_lines = 0;

            for (int16_t ability_id = TRAIT_POP_GROWTH_BONUS; ability_id < TRAIT_LOW_G_WORLD; ++ability_id) {
                if (race_specials[ability_id] != 0) {
                    ++total_lines;
                }
            }

            for (int16_t ability_id = TRAIT_LOW_G_WORLD; ability_id < TRAIT_COUNT; ++ability_id) {
                if (race_specials[ability_id] != 0) {
                    ++total_lines;
                }
            }

            ++total_lines;

            for (int16_t ability_id = TRAIT_POP_GROWTH_BONUS; ability_id < TRAIT_LOW_G_WORLD; ++ability_id) {
                if (race_specials[ability_id] != 0) {
                    RACESEL::Message_Build(&line_index, total_lines, paragraph, sizeof(paragraph));
                    strlcat(paragraph, MOX::special_ability_names[ability_id], sizeof(paragraph));

                    const int32_t special_level = static_cast<int8_t>(race_specials[ability_id]);
                    const int primary_pick = ability_id - TRAIT_POP_GROWTH_BONUS;
                    const int8_t bonus = raceopt::pick_rules.primary[primary_pick]
                                                               [special_level - 1]
                                                                   .value;
                    strlcat(
                        paragraph,
                        ability_id == TRAIT_PRODUCTION_BONUS ? ": " : " ",
                        sizeof(paragraph));
                    raceopt::Format_Numeric_Trait(
                        bonus_text, sizeof(bonus_text), ability_id, bonus);

                    strlcat(paragraph, bonus_text, sizeof(paragraph));
                }
            }

            for (int16_t ability_id = TRAIT_LOW_G_WORLD; ability_id < TRAIT_COUNT; ++ability_id) {
                if (race_specials[ability_id] != 0) {
                    RACESEL::Message_Build(&line_index, total_lines, paragraph, sizeof(paragraph));
                    const int name_id = ability_id == TRAIT_RICH_HOME_WORLD &&
                            (int8_t)race_specials[ability_id] < 0
                        ? TRAIT_COUNT
                        : ability_id;
                    strlcat(paragraph, MOX::special_ability_names[name_id], sizeof(paragraph));
                }
            }

            RACESEL::Message_Build(&line_index, total_lines, paragraph, sizeof(paragraph));
            strlcat(paragraph, MOX::government_labels[static_cast<int8_t>(race_specials[0])], sizeof(paragraph));

            fonts::Set_Font_Style_Shadow_Down(2, &shadow_colors);
            fonts::Set_Outline_Color(0, 0);
            BILL::_Print_Paragraph(0x32, 0x187, 0x127, paragraph, 0);

            if (RACESEL::displayed_race == 13) {
                fonts::Set_Font_Style_Shadow_Down(4, &shadow_colors);
                fonts::Set_Outline_Color(0, 0);
                fonts::Print_Centered(0xc5, 0x2a, RACESEL::last_race_name);
            }
        }

        animation = (s_animation_header*)buffer::Buffer_Reload("RACESEL.LBX", 0, MOX::global_cache_seg);
        animate::Draw(0x30, 0x3a, animation);
    }

} // namespace RACESEL