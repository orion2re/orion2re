#include "pch.h"

namespace {
    /** Draws one application name inside the grouped technology list popup. */
    void __cdecl Draw_Tech_List_App(int16_t x, int16_t y, uint8_t tech_app_id) {
        char buffer[80];

        buffer[0] = '^';
        strlcpy(buffer + 1, TECH::Technology_Applications_Name(tech_app_id, MOX::PLAYER_NUM), sizeof(buffer) - 1);
        BILL::Squeeze_Print(x, y, buffer, 0xD5);
    }
} // namespace

namespace TECH {
    char hyper_field_name_buffer[64];

    int32_t tech_select_pos[8] = {
        0x001E00B0,
        0x001F0193,
        0x008700B0,
        0x00870193,
        0x00F000B0,
        0x00F00193,
        0x015B00B0,
        0x015B0193,
    };

    int32_t tech_change_pos[8] = {
        0x001E005F,
        0x001F0142,
        0x0087005F,
        0x00870142,
        0x00F0005F,
        0x00F00142,
        0x015B005F,
        0x015B0142,
    };

    int16_t tech_field_y1[4] = {0, 34, 49, 64};

    int16_t tech_field_y2[4] = {33, 48, 63, 78};

    int16_t app_name_y[8] = {19, 34, 49, 64, 79, 0, 0, 0};

    int16_t list_window_x_offsets[8] = {205, 6, 205, 6, 205, 6, 205, 6};

    int16_t tech_button_pos[16] = {
        21,
        30,
        248,
        31,
        21,
        135,
        248,
        135,
        21,
        240,
        248,
        240,
        21,
        347,
        248,
        347,
    };

    uint8_t entry_to_group[8] = {4, 2, 6, 8, 7, 1, 3, 5};

    s_colors tech_select_colors[5] = {
        {{241, 244, 244, 244, 243, 243, 244, 245}},
        {{241, 247, 247, 247, 245, 246, 247, 248}},
        {{241, 248, 248, 248, 247, 248, 248, 249}},
        {{241, 245, 245, 245, 244, 245, 245, 245}},
        {{241, 244, 244, 244, 243, 243, 244, 244}},
    };

    s_colors tech_change_colors[5] = {
        {{225, 228, 228, 228, 227, 227, 228, 229}},
        {{225, 231, 231, 231, 229, 230, 231, 232}},
        {{225, 232, 232, 232, 231, 232, 232, 232}},
        {{225, 229, 229, 229, 228, 229, 229, 229}},
        {{225, 228, 228, 228, 227, 227, 228, 228}},
    };

    int16_t g_scrn_y;
    int16_t g_scrn_x;
    int16_t g_first_tech_field;
    s_fieldsav_data tech_field_save;
    s_list_item* g_selected_list_entry_p;
    int16_t g_last_tech_field;
    uint8_t g_cursor_box_color;
    uint8_t g_changing_tech;
    s_list_item_research* g_selected_entry_p;
    char tech_group_names[10][20];
    s_list_item_research* g_entries_p;
    // Not allocated from screen_seg: pool blocks are only 2-byte aligned,
    // and this struct holds a pointer.
    s_list_item_research research_entries[8];
    s_colors tech_color[5];

    s_list_state g_tech_list_header = {
        0,
        73,
        223,
        424,
        app_name_y,
        tech_field_y1,
        tech_field_y2,
        8,
        0,
        nullptr,
        nullptr,
        0,
        0,
        4,
        3,
        (void*)Draw_Tech_List_App,
        10,
        0xFF,
        &tech_color[1],
        {&tech_color[4], &tech_color[3], &tech_color[0], &tech_color[3]},
        &tech_color[2],
    };

    void __cdecl Tech_Select() {
        void* animation_buffer;

        palette::Save_Palette();
        animation_buffer = allocate::Allocate_First_Block(MOX::screen_seg, 0x7C830);
        file_ani::Set_File_Animation_Buffer(animation_buffer);
        SCIENCE::Show_Off_Researched_Tech(MOX::PLAYER_NUM);
        MOX::player[MOX::PLAYER_NUM].current_research_field = 0;
        MOX::player[MOX::PLAYER_NUM].research_breakthrough = 0;
        TECH::_Tech_Select(0);
        ERIC::Fast_Fade_Out();
        TECH::Reset_Screen();
    }

    void __cdecl _Tech_Select(uint8_t changing_tech) {
        int16_t extra_fields_cnt;
        int16_t first_btn_field;
        int16_t last_btn_field;
        int16_t accept_btn_id;
        int i;
        int16_t player_race;
        char science_race_lbx_name[64];
        MOX2::action_func_t auto_func;
        s_animation_header* tech_select_lbx_images[9];
        s_animation_header* entry_buttons[8];
        s_animation_header* science_lbx = nullptr;
        uint8_t saved_current_field;
        int16_t input_val;

        RUSS::Mox_Update();
        fields::Clear_Fields();
        video::Set_Refresh_Stencil();
        MOX2::Get_Auto_Function(&auto_func);
        allocate::Push_Block(MOX::screen_seg);
        fields::Disable_Cancel();

        MOX::cur_plyr_ptr = &MOX::player[MOX::PLAYER_NUM];
        g_changing_tech = changing_tech;

        saved_current_field = MOX::cur_plyr_ptr->current_research_field;

        s_list_item_research* entries = research_entries;
        MOX::tech_desc_seg = allocate::Allocate_Next_Block(MOX::screen_seg, 0x57C);

        RUSS::Mox_Update();

        if (changing_tech == 1) {
            g_scrn_x = 0x50;
            g_scrn_y = 0;
            for (i = 0; i < 5; ++i) {
                tech_color[i] = tech_change_colors[i];
            }
            g_cursor_box_color = 0xEF;

            tech_select_lbx_images[0] = (s_animation_header*)farload::Far_Reload_Next("TECHSEL.LBX", 0xE, MOX::screen_seg);
            RUSS::Mox_Update();
            entry_buttons[0] = (s_animation_header*)farload::Far_Reload_Next("TECHSEL.LBX", 0xF, MOX::screen_seg);
            entry_buttons[1] = (s_animation_header*)farload::Far_Reload_Next("TECHSEL.LBX", 0x10, MOX::screen_seg);
            entry_buttons[2] = (s_animation_header*)farload::Far_Reload_Next("TECHSEL.LBX", 0x11, MOX::screen_seg);
            entry_buttons[3] = (s_animation_header*)farload::Far_Reload_Next("TECHSEL.LBX", 0x12, MOX::screen_seg);
            RUSS::Mox_Update();
            entry_buttons[4] = (s_animation_header*)farload::Far_Reload_Next("TECHSEL.LBX", 0x13, MOX::screen_seg);
            entry_buttons[5] = (s_animation_header*)farload::Far_Reload_Next("TECHSEL.LBX", 0x14, MOX::screen_seg);
            entry_buttons[6] = (s_animation_header*)farload::Far_Reload_Next("TECHSEL.LBX", 0x15, MOX::screen_seg);
            RUSS::Mox_Update();
            entry_buttons[7] = (s_animation_header*)farload::Far_Reload_Next("TECHSEL.LBX", 0x16, MOX::screen_seg);

            tech_select_lbx_images[8] = (s_animation_header*)farload::Far_Reload_Next("TECHSEL.LBX", 0x1B, MOX::screen_seg);
        } else {
            g_scrn_x = 0xA1;
            g_scrn_y = 0;
            for (i = 0; i < 5; ++i) {
                tech_color[i] = tech_select_colors[i];
            }
            g_cursor_box_color = 0xFF;

            tech_select_lbx_images[0] = (s_animation_header*)farload::Far_Reload_Next("TECHSEL.LBX", 0, MOX::screen_seg);
            RUSS::Mox_Update();
            entry_buttons[0] = (s_animation_header*)farload::Far_Reload_Next("TECHSEL.LBX", 1, MOX::screen_seg);
            entry_buttons[1] = (s_animation_header*)farload::Far_Reload_Next("TECHSEL.LBX", 2, MOX::screen_seg);
            entry_buttons[2] = (s_animation_header*)farload::Far_Reload_Next("TECHSEL.LBX", 3, MOX::screen_seg);
            RUSS::Mox_Update();
            entry_buttons[3] = (s_animation_header*)farload::Far_Reload_Next("TECHSEL.LBX", 4, MOX::screen_seg);
            entry_buttons[4] = (s_animation_header*)farload::Far_Reload_Next("TECHSEL.LBX", 5, MOX::screen_seg);
            entry_buttons[5] = (s_animation_header*)farload::Far_Reload_Next("TECHSEL.LBX", 6, MOX::screen_seg);
            RUSS::Mox_Update();
            entry_buttons[6] = (s_animation_header*)farload::Far_Reload_Next("TECHSEL.LBX", 7, MOX::screen_seg);
            entry_buttons[7] = (s_animation_header*)farload::Far_Reload_Next("TECHSEL.LBX", 8, MOX::screen_seg);

            tech_select_lbx_images[8] = (s_animation_header*)farload::Far_Reload_Next("TECHSEL.LBX", 0xD, MOX::screen_seg);
        }

        RUSS::Mox_Update();

        g_tech_list_header.marker_color = g_cursor_box_color;

        if (changing_tech == 1) {
            accept_btn_id = fields::Add_Button_Field(
                g_scrn_x + 0xBD, g_scrn_y + 0x1C4, "", tech_select_lbx_images[8], "\x1B", '(');
            MOX::cur_plyr_ptr->current_research_field = 0;
            Init_Entry_Positions(entries);
            extra_fields_cnt = Init_Entry_Data(entries, MOX::cur_plyr_ptr->research_accumulated);
            MOX::cur_plyr_ptr->current_research_field = static_cast<char>(saved_current_field);
        } else {
            accept_btn_id = -1;
            Init_Entry_Positions(entries);
            if (static_cast<uint8_t>(MOX::cur_plyr_ptr->current_research_field) >= TECH_FIELD_BIOLOGY) {
                MOX::cur_plyr_ptr->current_research_field = 0;
            }
            extra_fields_cnt = Init_Entry_Data(entries, 0);
        }

        g_first_tech_field = fields::Get_Field_Counter();

        for (i = 0; i < 8; ++i) {
            g_last_tech_field = fields::Add_Hidden_Field(
                entries[i].x - 2, entries[i].y + 18, entries[i].x + 215, entries[i].y + 99, "", 0);
        }

        RUSS::Mox_Update();

        if (extra_fields_cnt == 0) {
            allocate::Pop_Block(MOX::screen_seg);
            MOX2::Put_Auto_Function(&auto_func);
            fields::Clear_Fields();
            fields::Deactivate_Help_List();
            return;
        }

        first_btn_field = fields::Get_Field_Counter();
        if (allow_future_technology_browsing) {
            Setup_Entry_Buttons(entries, entry_buttons);
        }

        last_btn_field = fields::Get_Field_Counter() - 1;

        fields::Add_Hidden_Field(0, 0, 0x27F, 0x1DF, "", 0);

        g_entries_p = entries;

        player_race = MOX::player[MOX::PLAYER_NUM].race;
        snprintf(science_race_lbx_name, sizeof(science_race_lbx_name), "SR_R%x_SC.LBX", player_race);

        RUSS::Mox_Update();

        if (changing_tech == 0) {
            file_ani::Open_File_Animation(science_race_lbx_name, 0);
            science_lbx = (s_animation_header*)farload::Far_Reload_Next("SCIENCE.LBX", 0, MOX::screen_seg);
        }

        while (true) {
            RUSS::Mox_Update();

            if (changing_tech == 0) {
                video::Set_Page_Off();
                graphics::Fill(0, 0, 0x27F, 0x1DF, 0);

                if (MOX::player[MOX::PLAYER_NUM].race == 7) {
                    file_ani::Set_File_Animation_Frame(0x11);
                } else {
                    file_ani::Set_File_Animation_Frame(0);
                }
                file_ani::Draw_File_Animation();

                graphics::Fill(g_scrn_x + 4, g_scrn_y + 4, g_scrn_x + 0x1D7, g_scrn_y + 0x1D8, 0);
                animate::Draw_Palette(tech_select_lbx_images[0]);
                palette::Refresh_Palette();
                animate::Draw_Palette(science_lbx);
                palette::Refresh_Palette();
            }

            fields::Set_Input_Delay(5);

            if (changing_tech == 1) {
                BILLHELP::Technology_Change_Help();
            } else {
                BILLHELP::Technology_Select_Help();
            }

            RUSS::Mox_Update();
            MOX::FULL_DRAW_SCREEN_FLAG = 0;

            fields::Assign_Auto_Function(Draw_Tech_Select, 4);

            graphics::Fill(g_scrn_x + 4, g_scrn_y + 4, g_scrn_x + 0x1D7, g_scrn_y + 0x1D8, 0);
            animate::Draw(g_scrn_x, g_scrn_y, tech_select_lbx_images[0]);
            palette::Refresh_Palette();
            Display_All_Entry_Text(g_entries_p);

            RUSS::Mox_Update();
            video::Copy_Off_To_Back();
            Draw_Tech_Select();

            if (changing_tech == 0) {
                JIM::Bill_Picture_To_Mouse(tech_select_lbx_images[8]);
            }

            ERIC::Fast_Fade_In();
            palette::Reset_Cycle_Palette_Color();
            fields::Clear_Mouse_Buffer();

            RUSS::Mox_Update();

            bool redraw = false;

            while (!redraw) {
                if (changing_tech == 0) {
                    JIM::Play_Streaming_Music(0x75, -2, 1);
                }

                input_val = fields::Get_Input();

                if (MOX::FULL_DRAW_SCREEN_FLAG != 0) {
                    redraw = true;
                    break;
                }

                if (input_val < 0) {
                    input_val = -input_val;
                    if (input_val < first_btn_field || input_val > last_btn_field) {
                        if (Set_Selected_Entry(entries, input_val) != nullptr) {
                            g_selected_entry_p = Get_Selected_Entry(g_entries_p);
                            int16_t app_id = g_selected_entry_p->app_ids[g_selected_entry_p->selected_app_index];
                            if (app_id != 0) {
                                Display_Selected_Entry_Text(g_selected_entry_p);
                                Draw_Application_Description(0x119, app_id);
                                redraw = true;
                                break;
                            }
                        }
                        input_val = 0;
                    }
                }

                if (input_val <= 0) {
                    timer::Mark_Time();
                    Draw_Tech_Select();
                    timer::Release_Time(2);
                    RUSS::Mox_Sync_Update();
                } else {
                    if (input_val == accept_btn_id) {
                        allocate::Pop_Block(MOX::screen_seg);
                        MOX2::Put_Auto_Function(&auto_func);
                        fields::Clear_Fields();
                        fields::Deactivate_Help_List();
                        return;
                    } else if (input_val < first_btn_field) {
                        int16_t blink_cnt = 3;
                        g_selected_entry_p = Get_Selected_Entry(entries);
                        if (g_selected_entry_p == nullptr) {
                            input_val = 0;
                            continue;
                        }
                        do {
                            timer::Mark_Time();
                            if ((blink_cnt & 1) == 0) {
                                Display_Selected_Entry_Text(g_selected_entry_p);
                            }
                            --blink_cnt;
                            RUSS::Mox_Sync_Update();
                            timer::Release_Time(4);
                        } while (blink_cnt != 0);

                        if (!Commit_Selected_Entry(
                                MOX::cur_plyr_ptr, g_selected_entry_p)) {
                            input_val = 0;
                            continue;
                        }
                        allocate::Pop_Block(MOX::screen_seg);
                        MOX2::Put_Auto_Function(&auto_func);
                        fields::Clear_Fields();
                        fields::Deactivate_Help_List();
                        return;
                    } else if (input_val <= last_btn_field) {
                        int16_t btn_idx = input_val - first_btn_field;
                        s_list_item_research* clicked_entry = &entries[btn_idx];
                        animate::Set_Animation_Frame(clicked_entry->button_image, 1);
                        animate::Draw(
                            tech_button_pos[btn_idx * 2] + g_scrn_x,
                            tech_button_pos[btn_idx * 2 + 1] + g_scrn_y,
                            clicked_entry->button_image);
                        video::Copy_Off_To_Back();
                        clicked_entry->button_value = 0;
                        _Tech_List(clicked_entry, changing_tech);
                        redraw = true;
                        break;
                    }
                }
            }
        }
    }

    void __cdecl Init_Entry_Positions(s_list_item_research* research_list) {
        for (int i = 0; i < 8; ++i) {
            s_list_item_research& entry = research_list[i];

            entry.id = (int16_t)TECH::entry_to_group[i];

            int32_t pos;
            if (TECH::g_changing_tech) {
                pos = TECH::tech_change_pos[i];
            } else {
                pos = TECH::tech_select_pos[i];
            }

            // Unpack HIWORD/LOWORD into y and x coordinates
            entry.x = (int16_t)pos;
            entry.y = (int16_t)(pos >> 16);

            entry.field_name[0] = '\0';
            entry.tech_field_id = 0;
            entry.current_app_index = 0;
            entry.selected_app_index = 0;
            entry.visible_app_count = 0;
            entry.label_x = entry.x + 10;
            entry.list_win_x = TECH::list_window_x_offsets[i] + TECH::g_scrn_x;

            for (int j = 0; j < 4; ++j) {
                entry.app_label_y[j] = TECH::app_name_y[j] + entry.y + 21;
                entry.app_names[j][0] = '\0';
            }
        }
    }

    void Setup_Entry_Buttons(s_list_item_research* items, s_animation_header** images) {
        for (int i = 0; i < 8; ++i) {
            if (items[i].tech_field_id != 0) {
                items[i].button_value = 0;
                items[i].button_field_id = fields::Add_Radio_Button_Field(
                    TECH::tech_button_pos[i * 2] + TECH::g_scrn_x,
                    TECH::tech_button_pos[i * 2 + 1] + TECH::g_scrn_y,
                    "",
                    images[i],
                    &items[i].button_value,
                    "",
                    40);
                items[i].button_image = images[i];
            }
        }
    }

    void __cdecl Draw_Tech_Select() {
        int16_t input_field = fields::Scan_Input();

        if (input_field >= g_first_tech_field && input_field <= g_last_tech_field) {
            int32_t index = input_field - g_first_tech_field;
            g_selected_entry_p = &g_entries_p[index];
            input_field = g_selected_entry_p->app_click_field_ids[g_selected_entry_p->visible_app_count - 1];
        }

        if (input_field > 0) {
            Set_Selected_Entry(g_entries_p, input_field);
        }

        g_selected_entry_p = Get_Selected_Entry(g_entries_p);
        Display_Selected_Entry_Text(g_selected_entry_p);
        Draw_Little_Arrow(g_selected_entry_p);

        palette::Cycle_Palette_Color(g_cursor_box_color, 8, 32, 0, 40, 63, 8, 8);
    }

    void __cdecl Display_All_Entry_Text(s_list_item_research* items) {
        s_list_item_research* current_item = items;
        s_list_item_research* end_item = items + 8;

        do {
            TECH::Display_Entry_Text(current_item);
            current_item++;
        } while (current_item != end_item);
    }

    s_list_item_research* __cdecl Set_Selected_Entry(s_list_item_research* items, int16_t target_field_id) {
        bool is_searching = true;
        s_list_item_research* result = nullptr;

        for (int i = 0; i < 8; ++i) {
            items[i].current_app_index = 0;

            if (is_searching && items[i].tech_field_id != 0 && items[i].visible_app_count > 0) {
                for (int16_t j = 0; j < items[i].visible_app_count; ++j) {
                    if (items[i].app_click_field_ids[j] == target_field_id) {
                        is_searching = false;
                        result = &items[i];
                        items[i].current_app_index = 1;
                        items[i].selected_app_index = j;
                    }
                }
            }
        }
        return result;
    }

    int16_t __cdecl Init_Entry_Data(s_list_item_research* entries, int32_t research_cost_offset) {
        char no_available_app_text[80];
        JIM::Get_Text_Message("billtext.lbx", 62, no_available_app_text, sizeof(no_available_app_text));
        int16_t valid_count = 0;

        s_list_item_research* item = entries;
        for (int i = 0; i < 8; ++i, ++item) {
            s_research_choice choice;
            if (!Read_Research_Choice(MOX::cur_plyr_ptr, item->id, false, &choice) || choice.field == 0) continue;
            const int16_t tech_field_id = choice.field;
            item->visible_app_count = (int8_t)choice.count;
            for (int app = 0; app < choice.count; ++app) {
                const int16_t application = choice.applications[app];
                strlcpy(item->app_names[app], application == 0 ? no_available_app_text : Technology_Applications_Name(application, MOX::PLAYER_NUM), sizeof(item->app_names[app]));
                item->app_ids[app] = application;
                item->app_click_field_ids[app] = fields::Add_Hidden_Field(
                    item->x,
                    tech_field_y1[app] + item->y + 0x15,
                    item->x + 0xDA,
                    tech_field_y2[app] + item->y + 0x15,
                    "",
                    0);
            }

            item->tech_field_id = tech_field_id;

            int32_t cost = COLCALC::Player_Research_Cost(MOX::cur_plyr_ptr, tech_field_id) - research_cost_offset;
            if (cost < 0) {
                cost = 0;
            }
            item->research_cost_remaining = cost;

            char fmt[8];
            const char* fmt_ptr;
            switch (MOX::settings.language) {
                case 0: fmt_ptr = "%i RP"; break;
                case 1: fmt_ptr = "%i FP"; break;
                case 3: fmt_ptr = "%i RP"; break;
                case 4: fmt_ptr = "%i PR"; break;
                case 2:
                default: fmt_ptr = "%i RP"; break;
            }
            strlcpy(fmt, fmt_ptr, sizeof(fmt));
            snprintf(item->buf, sizeof(item->buf), fmt, item->research_cost_remaining);

            strlcpy(item->field_name, Technology_Fields_Name(tech_field_id), sizeof(item->field_name));

            ++valid_count;
        }
        return valid_count;
    }

    void __cdecl Display_Entry_Text(s_list_item_research* item) {
        if (item == nullptr || item->tech_field_id == 0) {
            return;
        }

        bool is_current_field = (static_cast<uint8_t>(MOX::cur_plyr_ptr->current_research_field) == item->tech_field_id);

        s_colors* color;
        if (is_current_field) {
            color = &TECH::tech_color[2];
        } else {
            color = &TECH::tech_color[0];
        }

        fonts::Set_Font_Style(4, color);
        BILL::Squeeze_Print(item->x, item->y + 21, item->field_name, 218);

        fonts::Set_Font_Style(3, color);
        fonts::Print_Right(item->x + 212, item->y + 2, item->buf);

        bool is_creative = false;
        if (is_current_field) {
            if (MOX::cur_plyr_ptr->traits[TRAIT_CREATIVE] == 1) {
                is_creative = true;
            } else {
                int16_t tid = item->tech_field_id;
                if (tid == 0x37 || tid == 0x39 || tid == 0x1D || tid == 0x16 || tid == 0x1C || tid == 0x17) {
                    is_creative = true;
                }
            }
        }

        int16_t i = 0;
        do {
            s_colors* app_color = &TECH::tech_color[0];

            if (is_current_field) {
                if (is_creative) {
                    app_color = &TECH::tech_color[2];
                } else {
                    uint8_t cur_app = static_cast<uint8_t>(MOX::cur_plyr_ptr->current_research_application);
                    if (strcasecmp(item->app_names[i], TECHDATA::technology_applications[cur_app].name) == 0) {
                        app_color = &TECH::tech_color[2];
                    }
                }
            }

            fonts::Set_Font_Style(3, app_color);
            BILL::Squeeze_Print(item->label_x, item->app_label_y[i], item->app_names[i], 208);

            i++;
        } while (i < item->visible_app_count);
    }

    void __cdecl Draw_Little_Arrow(s_list_item_research* entry) {
        if (entry == nullptr) {
            return;
        }

        if (entry->current_app_index == 0) {
            return;
        }

        int16_t tech_id = entry->tech_field_id;
        if (tech_id == 0) {
            return;
        }

        int16_t i = entry->selected_app_index;
        int16_t upper_bound = i + 1;

        if (MOX::player[MOX::PLAYER_NUM].traits[TRAIT_CREATIVE] == 1 ||
            tech_id == 0x37 ||
            tech_id == 0x39 ||
            tech_id == 0x1D ||
            tech_id == 0x16 ||
            tech_id == 0x1C ||
            tech_id == 0x17) {
            i = 0;
            upper_bound = entry->visible_app_count;
        }

        for (; i < upper_bound; i++) {
            uint8_t color = g_cursor_box_color;
            int16_t label_x = entry->label_x;
            int16_t y = entry->app_label_y[i] + 5;
            int16_t x1 = label_x - 9;
            int16_t x2 = label_x - 2;
            int16_t x3 = label_x - 5;
            int16_t y0 = entry->app_label_y[0] - 3;

            line::Line(x1, y0, x1, y, color);
            line::Line(x1, y, x2, y, color);
            line::Line(x3, y - 3, x2, y, color);
            line::Line(x3, y + 3, x2, y, color);
        }
    }

    void __cdecl Draw_Application_Description(int32_t unused_x, uint16_t app_idx) {
        char text_buffer[80];
        char lbx_name[20];
        char cost_str[20];
        char desc_buffer[1420];

        (void)unused_x;

        fields::Deactivate_Auto_Function();
        allocate::Push_Block(MOX::screen_seg);
        fields::Disable_Draw_All_Fields();

        void* out_buffer = allocate::Allocate_Next_Block(MOX::screen_seg, 0x57C);

        TEXTBOX::Get_Help_Lbx_Name(lbx_name, sizeof(lbx_name));
        s_help_record* loaded_data = (s_help_record*)farload::Far_Reload_Data(lbx_name, 0, out_buffer, app_idx, 1, sizeof(s_help_record));

        int32_t cost = New_Get_Tech_Cost(app_idx, 1, MOX::PLAYER_NUM);
        JIM::Get_Text_Message("billtext.lbx", 0x3D, text_buffer, sizeof(text_buffer));

        snprintf(cost_str, sizeof(cost_str), "%i", cost);
        strlcat(text_buffer, cost_str, sizeof(text_buffer));

        const char* suffix;
        switch (MOX::settings.language) {
            case 0:
                suffix = " RP";
                break;
            case 1:
                suffix = " FP";
                break;
            case 2:
                suffix = " RP";
                break;
            case 3:
                suffix = " RP";
                break;
            case 4:
                suffix = " PR";
                break;
            default:
                suffix = " RP";
                break;
        }
        strlcat(text_buffer, suffix, sizeof(text_buffer));

        /* fmtpara: \aF3. selects font 3, \x1A3 fully justifies the body text,
           then \r starts a new paragraph and \aY+3.\x1A2 centers the cost line. */
        snprintf(
            desc_buffer,
            sizeof(desc_buffer),
            "\aF3.\x1A"
            "3%s\r\aY+3.\x1A"
            "2%s",
            loaded_data->body,
            text_buffer);
        TEXTBOX::Text_Box(loaded_data->title, desc_buffer);

        allocate::Pop_Block(MOX::screen_seg);
        fields::Activate_Auto_Function();
        fields::Clear_Mouse_Buffer();
        fields::Enable_Draw_All_Fields();
    }

    int16_t __cdecl _Tech_List(s_list_item_research* entry, uint8_t is_change_mode) {
        char msg_buf[80];
        char title_buf[32];
        s_animation_header* win_bg_1;
        s_animation_header* win_bg_2;
        MOX2::action_func_t auto_funcs[11];
        int16_t clear_field;
        int16_t text_x;
        int16_t fill_x2;
        s_animation_header* bg_capture;
        int16_t win_x;
        int16_t start_field;
        int16_t button_2_id;
        int16_t list_bottom_field;
        int16_t hidden_field_1;
        int16_t button_1_id;
        s_animation_header* pic;
        s_animation_header* pic_00;
        int16_t input;

        RUSS::Mox_Update();
        JIM::Get_Text_Message("billtext.lbx", 0x3F, msg_buf, sizeof(msg_buf));
        JIM::Get_Text_Message("billtext.lbx", 0x40, tech_group_names[0], sizeof(tech_group_names[0]));
        JIM::Get_Text_Message("billtext.lbx", 0x41, tech_group_names[1], sizeof(tech_group_names[0]));
        RUSS::Mox_Update();
        JIM::Get_Text_Message("billtext.lbx", 0x42, tech_group_names[2], sizeof(tech_group_names[0]));
        JIM::Get_Text_Message("billtext.lbx", 0x43, tech_group_names[3], sizeof(tech_group_names[0]));
        RUSS::Mox_Update();
        JIM::Get_Text_Message("billtext.lbx", 0x44, tech_group_names[4], sizeof(tech_group_names[0]));
        JIM::Get_Text_Message("billtext.lbx", 0x45, tech_group_names[5], sizeof(tech_group_names[0]));
        RUSS::Mox_Update();
        JIM::Get_Text_Message("billtext.lbx", 0x46, tech_group_names[6], sizeof(tech_group_names[0]));
        JIM::Get_Text_Message("billtext.lbx", 0x47, tech_group_names[7], sizeof(tech_group_names[0]));
        RUSS::Mox_Update();
        JIM::Get_Text_Message("billtext.lbx", 0x48, tech_group_names[8], sizeof(tech_group_names[0]));
        JIM::Get_Text_Message("billtext.lbx", 0x49, tech_group_names[9], sizeof(tech_group_names[0]));
        RUSS::Mox_Update();

        MOX2::Get_Auto_Function(auto_funcs);
        allocate::Push_Block(MOX::screen_seg);
        FIELDSAV::Save_Field_Stats(&tech_field_save);
        fields::Disable_Cancel();

        if (is_change_mode == 1) {
            win_bg_1 = (s_animation_header*)farload::Far_Reload_Next("TECHSEL.LBX", 0x17, MOX::screen_seg);
            win_bg_2 = (s_animation_header*)farload::Far_Reload_Next("TECHSEL.LBX", 0x18, MOX::screen_seg);
            RUSS::Mox_Update();
            pic = (s_animation_header*)farload::Far_Reload_Next("TECHSEL.LBX", 0x19, MOX::screen_seg);
            pic_00 = (s_animation_header*)farload::Far_Reload_Next("TECHSEL.LBX", 0x1A, MOX::screen_seg);
        } else {
            win_bg_1 = (s_animation_header*)farload::Far_Reload_Next("TECHSEL.LBX", 0x09, MOX::screen_seg);
            win_bg_2 = (s_animation_header*)farload::Far_Reload_Next("TECHSEL.LBX", 0x0A, MOX::screen_seg);
            RUSS::Mox_Update();
            pic = (s_animation_header*)farload::Far_Reload_Next("TECHSEL.LBX", 0x0B, MOX::screen_seg);
            pic_00 = (s_animation_header*)farload::Far_Reload_Next("TECHSEL.LBX", 0x0C, MOX::screen_seg);
        }
        RUSS::Mox_Update();

        g_tech_list_header.items = (s_list_item*)allocate::Allocate_Next_Block(MOX::screen_seg, 0x33E0);
        win_x = entry->list_win_x;
        g_tech_list_header.x = win_x + 0x10;

        uint32_t bg_size = bitmap::Get_Bitmap_Size(0x1B5, 0x190);
        bg_capture = (s_animation_header*)allocate::Allocate_Next_Block(MOX::screen_seg, bg_size);
        bitmap::Create_Blank_Bitmap(0x1B5, 0x190, bg_capture);
        bitmap::Screen_Bitmap_Capture(0x54, 0x28, 0x207, 0x1B7, bg_capture);

        if ((win_x - g_scrn_x) <= 6) {
            if (is_change_mode != 0) {
                BILLHELP::Tech_Change_List_Left_Help();
            } else {
                BILLHELP::Tech_Select_List_Left_Help();
            }
        } else {
            if (is_change_mode != 0) {
                BILLHELP::Tech_Change_List_Right_Help();
            } else {
                BILLHELP::Tech_Select_List_Right_Help();
            }
        }

        Get_Group_List(&g_tech_list_header, entry->id);
        LIST::Init_List_Data(&g_tech_list_header);

        button_1_id = fields::Add_Button_Field(win_x + 0xF7, 0x4D, "", pic, "", 0);
        button_2_id = fields::Add_Button_Field(win_x + 0xF8, 0x18E, "", pic_00, "", 0);
        start_field = fields::Get_Field_Counter();
        fields::Assign_Auto_Function(Draw_Tech_List, 4);

        fill_x2 = win_x + 0x10B;
        text_x = win_x + 0x86;
        clear_field = start_field - 1;

        while (true) {
            MOX::FULL_DRAW_SCREEN_FLAG = 0;
            graphics::Fill(0x54, 0x28, 0x207, 0x1B7, 0);
            animate::Draw(0x54, 0x28, bg_capture);
            graphics::Fill(win_x, 0x1F, fill_x2, 0x1C1, 0);
            animate::Draw(win_x, 0x1F, win_bg_1);

            strlcpy(title_buf, tech_group_names[entry->id], sizeof(title_buf));
            strlcat(title_buf, msg_buf, sizeof(title_buf));

            fonts::Set_Font_Style(4, tech_color);
            fonts::Print_Centered(text_x, 0x2F, title_buf);

            LIST::Display_List_Page(&g_tech_list_header);
            video::Copy_Off_To_Back();
            LIST::Set_List_Up_Down_Field_Drawing(&g_tech_list_header, button_1_id, button_2_id);

            RUSS::Mox_Sync_Update();
            timer::Mark_Time();

            fields::Clear_Fields_Above(clear_field);
            list_bottom_field = LIST::Add_Fields_To_List_Page(&g_tech_list_header);

            hidden_field_1 = fields::Add_Hidden_Field(0, 0, 0x27F, 0x1DF, "\x1B", 0);
            fields::Add_Hidden_Field(0, 0, 0x27F, 0x1DF, "", 0);

            timer::Release_Time(4);

            while (true) {
                input = fields::Get_Input();
                if (MOX::FULL_DRAW_SCREEN_FLAG != 0) {
                    break;
                }

                if (input > 0) {
                    if (input == button_2_id) {
                        if (LIST::Next_List_Page(&g_tech_list_header) != 0) {
                            break;
                        }
                        continue;
                    }
                    if (input == button_1_id) {
                        if (LIST::Previous_List_Page(&g_tech_list_header) != 0) {
                            break;
                        }
                        continue;
                    }
                } else if (input == 0 || input == -hidden_field_1) {
                    timer::Mark_Time();
                    Draw_Tech_List();
                    RUSS::Mox_Sync_Update();
                    timer::Release_Time(4);
                    continue;
                } else {
                    int16_t abs_input = -input;
                    if (abs_input >= start_field && abs_input <= list_bottom_field) {
                        g_selected_list_entry_p = LIST::Get_Selected_List_Entry(&g_tech_list_header);
                        if (g_selected_list_entry_p != nullptr) {
                            uint8_t app_id = g_selected_list_entry_p->tech_app_ids[g_selected_list_entry_p->current_app_index];
                            if (app_id != 0) {
                                LIST::Display_Selected_List_Text(&g_tech_list_header, g_selected_list_entry_p);
                                Draw_Application_Description(0x119, app_id);
                                break;
                            }
                        }
                        continue;
                    }
                }

                int16_t match_val = (input > 0) ? input : -input;
                if (match_val == hidden_field_1) {
                    animate::Draw(win_x + 0xBB, 0x1B2, win_bg_2);
                    timer::Mark_Time();
                    RUSS::Mox_Sync_Update();
                    timer::Release_Time(4);
                    MOX2::Put_Auto_Function(auto_funcs);
                    FIELDSAV::Restore_Field_Stats(&tech_field_save);
                    allocate::Pop_Block(MOX::screen_seg);
                    fields::Deactivate_Help_List();
                    return input;
                }
            }
        }
    }

    /** Builds a star-and-roman-numeral planet name for technology text. */
    void __cdecl Do_Get_Planet_Name(int16_t planet_idx, char* out_str, uint8_t localized_flag) {
        char buffer[200];

        strlcpy(buffer, " ", sizeof(buffer));

        int16_t star_idx = MOX::planet[planet_idx].star_index;
        char planet_num = HAROLD::Planet_Number(planet_idx);

        const char* numeral_str;
        if (localized_flag == 0) {
            numeral_str = MOX::roman_literals[planet_num + 1];
        } else {
            numeral_str = MOX::roman_literals[planet_num + 1];
            if (strcmp(numeral_str, "I") == 0) {
                numeral_str = HAROLD::H_Message(0xa0);
            }
        }

        s_star_data* star = &MOX::star[star_idx];

        snprintf(buffer, sizeof(buffer), "%s %s", star->name, numeral_str);

        if (strlen(buffer) > 30) {
            buffer[30] = '\0';
        }

        strlcpy(out_str, buffer, 31);
    }

    int16_t __cdecl Get_Hyper_Tech_App_ID(int16_t tech_id) {
        switch (tech_id) {
            case TECH_FIELD_BIOLOGY:
                return TECH_APP_BIOLOGY;
            case TECH_FIELD_POWER:
                return TECH_APP_POWER;
            case TECH_FIELD_PHYSICS_2:
                return TECH_APP_PHYSICS;
            case TECH_FIELD_CONSTRUCTION:
                return TECH_APP_CONSTRUCTION;
            case TECH_FIELD_FIELDS:
                return TECH_APP_FIELDS;
            case TECH_FIELD_CHEMISTRY_2:
                return TECH_APP_CHEMISTRY;
            case TECH_FIELD_COMPUTERS:
                return TECH_APP_COMPUTERS;
            case TECH_FIELD_SOCIOLOGY:
                return TECH_APP_SOCIOLOGY;
            default:
                return tech_id;
        }
    }

    char* __cdecl Technology_Fields_Name(int16_t tech_field_id) {
        if (tech_field_id < TECH_FIELD_BIOLOGY) {
            return TECHDATA::technology_fields[tech_field_id].name;
        }

        strlcpy(hyper_field_name_buffer, MOX::hyper_field_title, sizeof(hyper_field_name_buffer));
        return hyper_field_name_buffer;
    }

    int32_t __cdecl New_Get_Tech_Cost(int16_t id, uint8_t is_tech_app, int16_t player_idx) {
        int16_t tech_field_id;
        if (is_tech_app != 0) {
            tech_field_id = TECHDATA::technology_applications[id].tech_field_id;
        } else {
            tech_field_id = id;
        }
        return COLCALC::Player_Research_Cost(&MOX::player[player_idx], tech_field_id);
    }

    void __cdecl Get_Group_List(s_list_state* state, int16_t group_idx) {
        uint8_t num_items = 0;
        s_list_item* item = state->items;
        uint16_t u_group_idx = static_cast<uint16_t>(group_idx);
        int16_t field_id = static_cast<int8_t>(MOX::first_field_in_group[u_group_idx]);

        while (field_id != 0) {
            uint8_t status = MOX::cur_plyr_ptr->tech_fields[field_id];
            item->group_id = static_cast<uint8_t>(field_id);

            strlcpy(item->group_name, Technology_Fields_Name(static_cast<uint16_t>(field_id)), sizeof(item->group_name));

            if (MOX::cur_plyr_ptr->current_research_field == field_id) {
                status = 4;
            } else if (field_id >= TECH_FIELD_BIOLOGY &&
                MOX::cur_plyr_ptr->hyper_advanced_tech[field_id - TECH_FIELD_BIOLOGY] > 20) {
                status = 3;
            }

            if (status != 3) {
                if (status == 0) {
                    for (int i = 0; i < 4; ++i) {
                        int16_t tech_id = TECHDATA::technology_fields[field_id].tech[i];

                        if (MOX::cur_plyr_ptr->tech_applications[tech_id] == TECH_RESEARCH_STATUS_RESEARCHED) {
                            status = 1;
                            break;
                        }
                    }
                }

                if (status >= 1) {
                    uint8_t app_count = 0;
                    for (int i = 0; i < 4; ++i) {
                        int16_t tech_id = TECHDATA::technology_fields[field_id].tech[i];

                        uint8_t app_status = MOX::cur_plyr_ptr->tech_applications[tech_id];

                        if (app_status >= 1) {
                            if (status == 4) {
                                app_status = 4;
                            } else if (app_status == 1 && status == 2) {
                                app_status = 2;
                            }

                            item->tech_app_ids[app_count] = static_cast<uint8_t>(tech_id);
                            item->tech_app_status[app_count] = app_status;
                            app_count++;
                        }
                    }

                    if (app_count != 0) {
                        item->app_count = app_count;
                        item->group_color_index = status;
                        num_items++;
                        item++;
                    }
                }
            }

            field_id = TECHDATA::technology_fields[field_id].next_field_id;
        }

        state->num_items = num_items;
    }

    void __cdecl Draw_Tech_List() {
        int16_t input = fields::Scan_Input();
        if (input > 0) {
            LIST::Set_Selected_List_Entry(&g_tech_list_header, input);
        }

        g_selected_list_entry_p = LIST::Get_Selected_List_Entry(&g_tech_list_header);

        LIST::Display_Selected_List_Text_For_Tech_List(&g_tech_list_header, g_selected_list_entry_p);

        palette::Cycle_Palette_Color(g_cursor_box_color, 8, 32, 0, 40, 63, 8, 8);
    }

    char* __cdecl Technology_Applications_Name(int16_t tech_app_id, int16_t player_idx) {
        if (tech_app_id < TECH_APP_BIOLOGY) {
            return TECHDATA::technology_applications[tech_app_id].name;
        }
        Copy_Application_Name(tech_app_id, player_idx, MOX::temp_string_buf, sizeof(MOX::temp_string_buf));
        return MOX::temp_string_buf;
    }

    void __cdecl Display_Selected_Entry_Text(s_list_item_research* entry) {
        if (entry != nullptr && entry->current_app_index != 0 && entry->tech_field_id != 0) {
            fonts::Set_Font_Style(4, &tech_color[1]);
            BILL::Squeeze_Print(entry->x, entry->y + 0x15, entry->field_name, 0xDA);

            fonts::Set_Font_Style(3, &tech_color[1]);
            fonts::Print_Right(entry->x + 0xD4, entry->y + 2, entry->buf);

            if (entry->visible_app_count > entry->selected_app_index) {
                BILL::Squeeze_Print(entry->label_x,
                    entry->app_label_y[entry->selected_app_index],
                    entry->app_names[entry->selected_app_index],
                    0xD0);
            }

            graphics::Box(entry->x - 2, entry->y + 0x12, 0xDA, 0x52, 1, g_cursor_box_color);
        }
    }

    void __cdecl Tech_Change() {
        allocate::Allocate_First_Block(MOX::screen_seg, 4);
        video::Set_Page_Off();
        fields::Draw_Visible_Fields();
        video::Copy_Off_To_Back();
        int8_t previous_field = MOX::player[MOX::PLAYER_NUM].current_research_field;
        _Tech_Select(1);
        if (MOX::player[MOX::PLAYER_NUM].current_research_field != previous_field) {
            MOX::player[MOX::PLAYER_NUM].research_breakthrough = 0;
        }
        MOX::current_screen = MOX::return_screen;
    }

    void __cdecl Reset_Screen() {
        palette::Restore_Palette();
        BILL::Turn_Off_Palettes(0, 0xff);
        palette::Clear_Palette_Changes(0, 0xff);
        fields::Deactivate_Help_List();
        COLONY::Reset_Mouse_Picture();
        file_ani::Reset_File_Animation_Buffer();
        JIM::Play_Background_Music();
    }
} // namespace TECH