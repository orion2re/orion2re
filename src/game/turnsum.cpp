#include "pch.h"

namespace {
    int16_t turn_sum_list_y[16] = {
        0,
        10,
        20,
        30,
        40,
        50,
        60,
        70,
        80,
        90,
        100,
        110,
        120,
        130,
        140,
        0,
    };

    int16_t turn_sum_field_y2[16] = {
        9,
        19,
        29,
        39,
        49,
        59,
        69,
        79,
        89,
        99,
        109,
        119,
        129,
        139,
        149,
        0,
    };

    /**
     * Draws the caret marker and formatted text for one turn summary message entry.
     * @param x Left edge of the entry.
     * @param y Top edge of the entry.
     * @param msg_idx Message table index to render.
     */
    void __cdecl Draw_Turn_Summary_Popup_App(int16_t x, int16_t y, uint8_t msg_idx) {
        if (msg_idx <= 0xFA) {
            char buffer[256];

            fonts::Set_Font_LF(10);
            MSG_::sprintf_msg(&game::world, buffer, sizeof(buffer), &MSG_::msgs[msg_idx]);
            fonts::Print(x, y, "^");
            FMTPARA::Print_Formatted_Paragraph(x + 13, y, 320, buffer, 0);
        }
    }
} // namespace

namespace TURNSUM {
    s_colors turn_sum_normal_color = {{110, 114, 113, 113, 113, 114, 114, 115}};

    s_colors turn_sum_high_color = {{110, 117, 116, 115, 116, 116, 117, 118}};

    s_list_state g_turn_sum_header = {
        92,
        103,
        320,
        373,
        turn_sum_list_y,
        turn_sum_list_y,
        turn_sum_field_y2,
        8,
        0,
        nullptr,
        nullptr,
        0,
        0,
        4,
        4,
        (void*)Draw_Turn_Summary_Popup_App,
        13,
        0,
        &turn_sum_high_color,
        {&turn_sum_normal_color, &turn_sum_normal_color, &turn_sum_normal_color, &turn_sum_normal_color},
        &turn_sum_normal_color,
    };

    uint8_t g_turn_message_ix;

    s_list_item* g_selected_msg_p;

    void __cdecl Draw_Turn_Summary_Popup() {
        g_turn_sum_header.group_colors[0] = &turn_sum_high_color;

        int16_t input_field = fields::Scan_Input();

        if (input_field >= INFO::g_first_list_field && input_field <= INFO::g_last_list_field) {
            LIST::Set_Selected_List_Entry(&g_turn_sum_header, input_field);
        }

        g_selected_msg_p = LIST::Get_Selected_List_Entry(&g_turn_sum_header);
        LIST::Display_List_Text(&g_turn_sum_header, g_selected_msg_p);

        g_turn_sum_header.group_colors[0] = &turn_sum_normal_color;
    }

    void __cdecl Turn_Summary_Popup() {
        int16_t num_msgs;
        MOX2::action_func_t auto_func;
        s_list_item list_items[100];

        MOX::current_screen = SCREEN_MAIN;
        num_msgs = MSG_::N_Msgs_For_Player(MOX::PLAYER_NUM);

        if (num_msgs <= 0) {
            if (MOX::current_screen == SCREEN_MAIN) {
                MOX::current_screen = SCREEN_REPORTS;
            }
            return;
        }

        if (MOX::return_screen == SCREEN_REPORTS) {
            g_turn_message_ix = 0;
        } else {
            ERIC::Fast_Fade_Out();
            fonts::Load_Palette(0, 0, 0xFF);
            BILL::Turn_Off_Palettes(0, 0xFF);
        }

        fields::Clear_Mouse_Buffer();
        fields::Deactivate_Help_List();
        MOX2::Get_Auto_Function(&auto_func);
        fields::Clear_Fields();
        video::Set_Refresh_Stencil();

        s_animation_header* popup_background_anim = (s_animation_header*)(farload::Far_Reload("turnsum.lbx", 0, MOX::screen_seg));
        s_animation_header* scroll_up_button_image = (s_animation_header*)(farload::Far_Reload_Next("turnsum.lbx", 1, MOX::screen_seg));
        s_animation_header* scroll_down_button_image = (s_animation_header*)(farload::Far_Reload_Next("turnsum.lbx", 2, MOX::screen_seg));
        s_animation_header* ok_button_image = (s_animation_header*)(farload::Far_Reload_Next("turnsum.lbx", 3, MOX::screen_seg));

        while (true) {
            MOX::FULL_DRAW_SCREEN_FLAG = 0;
            fields::Clear_Fields();
            fields::Assign_Auto_Function(TURNSUM::Draw_Turn_Summary_Popup, 2);

            int16_t field_btn_up = fields::Add_Button_Field(0x1B9, 0x5F, "", scroll_up_button_image, "", 0);
            int16_t field_btn_down = fields::Add_Button_Field(0x1B9, 0x161, "", scroll_down_button_image, "", 0);

            g_turn_sum_header.items = list_items;
            INFO::Init_Turn_Summary_List(&g_turn_sum_header, num_msgs);

            if (MOX::return_screen == SCREEN_REPORTS &&
                REPORT::sort_serious_turn_summary_first) {
                g_turn_message_ix = list_items[0].group_id;
            }
            g_selected_msg_p = LIST::Select_And_Page_By_Head_ID(&g_turn_sum_header, g_turn_message_ix);

            MAINSCR::Draw_Mini_Main_Screen();
            BILLHELP::Turn_Summary_Popup_Help();

            while (true) {
                fields::Clear_Fields_Above(field_btn_down);
                INFO::g_first_list_field = fields::Get_Field_Counter();
                INFO::g_last_list_field = LIST::Add_Fields_To_List_Page(&g_turn_sum_header);

                int16_t field_btn_ok = fields::Add_Button_Field(0xF2, 0x176, "", ok_button_image, "\x1B", 0);
                fields::Add_Hidden_Field(0x54, 0x33, 0x1D0, 0x189, "\x1B", 0);
                int16_t field_hidden_bg = fields::Add_Hidden_Field(0, 0, 0x27F, 0x1DF, "\x1B", 0);

                animate::Draw(0x54, 0x33, popup_background_anim);
                LIST::Display_List_Page(&g_turn_sum_header);
                LIST::Set_List_Up_Down_Field_Drawing(&g_turn_sum_header, field_btn_up, field_btn_down);

                video::Copy_Off_To_Back();
                palette::Clear_Palette_Changes(0, 0xFF);
                RUSS::Mox_Sync_Update();
                ERIC::Fast_Fade_In();

                bool full_draw_requested = false;

                while (true) {
                    int16_t input_field = fields::Get_Input();

                    if (MOX::FULL_DRAW_SCREEN_FLAG != 0) {
                        full_draw_requested = true;
                        break;
                    }

                    if (input_field >= INFO::g_first_list_field && input_field <= INFO::g_last_list_field) {
                        uint8_t msg_idx = g_selected_msg_p->group_id;
                        if (MSG_::Goto_Msg_Colony(&MSG_::msgs[msg_idx]) != 0) {
                            input_field = field_hidden_bg;
                            MOX::current_screen = SCREEN_COLONY;
                            MOX::return_screen = SCREEN_TURN_SUMMARY;
                            g_turn_message_ix = msg_idx;
                        }
                    } else {
                        if (input_field > 0) {
                            if (input_field >= field_hidden_bg || input_field == field_btn_ok) {
                            } else if (input_field == field_btn_down) {
                                if (LIST::Next_List_Page(&g_turn_sum_header) != 0) {
                                    break;
                                }
                            } else if (input_field == field_btn_up) {
                                if (LIST::Previous_List_Page(&g_turn_sum_header) != 0) {
                                    break;
                                }
                            }
                        } else if (input_field == 0) {
                            timer::Mark_Time();
                            TURNSUM::Draw_Turn_Summary_Popup();
                            RUSS::Mox_Sync_Update();
                            timer::Release_Time(2);
                        }
                    }

                    if (input_field >= field_hidden_bg || input_field == field_btn_ok) {
                        MOX2::Put_Auto_Function(&auto_func);
                        fields::Clear_Fields();
                        if (MOX::current_screen == SCREEN_MAIN) {
                            MOX::current_screen = SCREEN_REPORTS;
                        }
                        return;
                    }
                }

                if (full_draw_requested) {
                    break;
                }
            }
        }
    }

} // namespace TURNSUM