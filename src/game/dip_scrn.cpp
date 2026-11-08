#include "pch.h"

namespace DIP_SCRN {
    s_mouse_list diplomacy_hand_mouse = {19, 0, 0, 0, 639, 479};
    s_mouse_list diplomacy_blank_mouse = {0, 0, 0, 0, 639, 479};
    uint8_t noreport = 0;
    uint8_t ambassador_option;
    int16_t main_diplomacy_help_entry[8] = {567, 568, 569, 570, 571, 572, 573, 574};
    int16_t response_message;
    uint16_t synch_up_established_flag;
    int16_t old_response_message;
    int16_t propose_treaty_help_entry[7] = {576, 577, 578, 579, 580, 613, 0};
    char* main_diplomacy_title;
    char* propose_treaty_title;
    uint8_t talker_ambassador;
    int16_t diplomacy_current_music;
    int16_t net_diplomacy_esc_pressed;
    uint8_t target_tech_list[TECH_APP_COUNT];
    uint8_t listen_ambassador;
    int16_t exchange_tech_count;
    char* main_diplomacy_list[9];
    char* propose_treaty_diplomacy_list[7];
    uint8_t exchange_tech_list[TECH_APP_COUNT];
    char chat_message[100];
    int16_t diplomacy_quit_chat_field;
    int16_t net_diplomacy_treaty_type;
    int16_t diplomacy_chat_field;
    char net_diplomacy_chat_message[100];
    char* player_number_name_buffer;
    char* player_name_ambassador_buffer;
    char receive_chat_message[110];
    uint8_t current_ambassador;
    char diplomacy_message[250];
    int16_t main_net_diplomacy_help_entry[10] = {567, 568, 569, 570, 571, 572, 573, -1, -1, 574};
    int16_t net_diplomacy_tech_offered;
    const char s_empty_string_00551100[] = "";
    const char s_empty_string[] = "";
    const char s_empty[] = "";
    const char s_jimtext_lbx[] = "jimtext.lbx";
    const char s__00551100[] = "";
    const char s_jimtext2_lbx_00552340[] = "jimtext2.lbx";
    const char s_jimtext2_lbx[] = "jimtext2.lbx";
    const char s_jimtext_lbx_00552360[] = "jimtext.lbx";
    const char s__d_BC_00552384[] = "  %d BC";
    const char s_fmt_s_0055236c[] = "  %s";
    const char s__s_0055236c[] = "  %s";
    const char s_fmt_d_bc_00552384[] = "  %d BC";
    const char s_fmt_BC[] = "%d BC";
    const char s_fmt_comma[] = "%d,%d";
    const char s_fmt_dot[] = "%d.%d";
    const char s_FWEE[] = "FWEE!";
    const char s_DIPLOMSP_LBX[] = "DIPLOMSP.LBX";
    const char s_DIPLOMSI_LBX[] = "DIPLOMSI.LBX";
    const char s_DIPLOMSS_LBX[] = "DIPLOMSS.LBX";
    const char s_DIPLOMSF_LBX[] = "DIPLOMSF.LBX";
    const char s_DIPLOMSG_LBX[] = "DIPLOMSG.LBX";
    const char s_DIPLOMSE_LBX[] = "DIPLOMSE.LBX";
    uint16_t chat_width;
    uint16_t chat_base_y;
    uint16_t chat_base_x;
    uint16_t chat_height;
    uint8_t chat_mode_flag;
    s_colors diplomacy_normal_colors = {{248, 253, 253, 253, 253, 253, 253, 253}};
    s_colors diplomacy_highlight_colors = {{250, 255, 255, 255, 255, 255, 255, 255}};
    s_colors diplomacy_special_colors = {{248, 250, 250, 250, 250, 250, 250, 250}};
    int16_t break_treaty_help_entry[7] = {582, 583, 584, 585, 586, 613, -1};
    char* break_treaty_title;
    char* break_treaty_diplomacy_list[7];
    char diplomacy_list_choice[15][50];
    s_help_box jim_help_list[15];
    int16_t demand_help_entry[11] = {588, 589, 590, 591, 592, 593, 594, 595, 596, 613, 0};
    int16_t main_repulsive_diplomacy_help_entry[7] = {578, 572, 573, 574, 0, 0, 0};
    int16_t main_net_repulsive_diplomacy_help_entry[7] = {578, 572, 573, -1, 574, 0, 0};
    char* main_repulsive_diplomacy_list[7];
    char* main_net_repulsive_diplomacy_list[6];
    s_dipscrn_net_message net_diplomacy_message;
    int16_t offer_help_entry[7] = {598, 599, 600, 601, 602, 613, -1};
    char* offer_diplomacy_list[7];
    char* offer_title;
    int16_t diplomacy_system_planet_list[5];
    int16_t diplomacy_system_population;
    uint8_t diplomacy_system_receiver;
    int16_t diplomacy_system_planet_count;
    int16_t diplomacy_scan_input_flag;
    uint8_t diplomacy_system_giver;
    char* demand_diplomacy_list[11];
    char* net_listen_mode_option_buffer;
    char* system_unsurrenderable_buffer;
    char* main_net_diplomacy_list[11];
    char* demand_title;
    char* agree_diplomacy_list[4];
    char* system_cancel_buffer;
    int16_t diplomacy_system_homeworld_flag;
    s_animation_header* diplomacy_system_star_background_seg;
    int16_t diplomacy_system_fields[MAX_STARS];
    uint8_t* diplomacy_mouse_bitmap;
    char* system_homeworld_buffer;
    int16_t diplomacy_good_music;
    char* statement_research_treaty_buffer;
    int16_t diplomacy_bad_music;
    char* choose_a_system_buffer;
    char* enter_your_message_buffer;
    char* statement_trade_treaty_buffer;
    char* system_million_colonists_buffer;
    char* net_listen_mode_good_bye_buffer;
    char* system_x_star_system_buffer;
    char* percent_tribute_treaty_buffer;
    char* you_were_framed_buffer;

    void __cdecl Diplomacy_Load_New_Ambassador(int16_t player_idx) {
        mouse::Set_Mouse_List(&DIP_SCRN::diplomacy_blank_mouse, 1);
        fields::Deactivate_Auto_Function();
        fields::Clear_Fields();
        ERIC::Set_Fade_Percent(0);
        ERIC::Set_Fade_Speed_Percent(10);
        ERIC::Fast_Fade_Out();
        DIP_SCRN::Diplomacy_Mouse_Init(player_idx);

        uint8_t race = (uint8_t)MOX::player[player_idx].race;

        file_ani::Open_File_Animation("DIPLOMAT.LBX", (uint16_t)race * 2 + 13);
        video::Set_Page_Off();
        file_ani::Draw_File_Animation();
        video::Copy_Off_To_Back();

        DIP_SCRN::current_ambassador = (uint8_t)player_idx;

        file_ani::Open_File_Animation("DIPLOMAT.LBX", (uint16_t)race * 2 + 14);
        mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);
        file_ani::Draw_File_Animation_Stencil();
    }

    /**
     * @brief Prepares a diplomatic gift offer message based on the gift type.
     * 
     * @param player_idx The index of the player offering the gift.
     * @param offer_type The type of gift (1: BC, 2: Peace/Treaty, 3: Tech, 4: System).
     * @param value The amount of BC, tech index, or star system index.
     */
    void __cdecl Get_Human_Offer_Gift_Message(int16_t player_idx, int16_t offer_type, int16_t value) {
        switch (offer_type) {
            case 1:
                JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 18 /* 0x12 */, diplomacy_message, sizeof(diplomacy_message));
                JIM::Decode_Text_Name_And_Stat(diplomacy_message, player_idx, value, 250);
                break;

            case 2:
                JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 15 /* 0x0F */, diplomacy_message, sizeof(diplomacy_message));
                if (value == 1) {
                    JIM::Decode_Text_Name_And_Stat(diplomacy_message, player_idx, 5, 250);
                } else {
                    JIM::Decode_Text_Name_And_Stat(diplomacy_message, player_idx, 10, 250);
                }
                break;

            case 3:
                JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 16 /* 0x10 */, diplomacy_message, sizeof(diplomacy_message));
                JIM::Decode_Text_Name_And_Tech(diplomacy_message, player_idx, value, 250);
                break;

            case 4:
                JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 17 /* 0x11 */, diplomacy_message, sizeof(diplomacy_message));
                JIM::Decode_Text_Name_And_System(diplomacy_message, player_idx, value, 250);
                break;

            default:
                break;
        }
    }

    void __cdecl Diplomacy_Break_Treaty_Resolution(int16_t sender_id, int16_t target_id, int16_t treaty_type) {
        if (MOX::player[target_id].objectives == PLAYER_OBJECTIVE_HUMAN) {
            if (MOX::game_type == 1) {
                Diplomacy_Load_New_Ambassador(sender_id);
                Get_Human_Break_Treaty_Message(target_id, treaty_type);
                response_message = 0;
                old_response_message = 0;
                Diplomacy_Display_Response();
                Diplomacy_Load_New_Ambassador(target_id);
            } else {
                net_diplomacy_message.sender_player_id = (uint8_t)sender_id;
                net_diplomacy_message.target_player_id = (uint8_t)target_id;
                net_diplomacy_message.arg1 = treaty_type;
                net_diplomacy_message.msg_type = DIP_NET_MSG_BREAK_TREATY;
                net_diplomacy_message.arg2 = response_message;
                NETMOX::Mox_Send_Message(MOX::player[target_id].network_player_id, DIP_NET_PACKET_MESSAGE, &net_diplomacy_message, sizeof(net_diplomacy_message));
                RUSS::Mox_Update();
            }
        }

        switch (treaty_type) {
            case 1:
            case 2:
                diplomac::Break_Treaties(sender_id, target_id);
                if (MOX::player[target_id].objectives == PLAYER_OBJECTIVE_HUMAN) {
                    response_message = 153 /* 0x99 */;
                } else {
                    diplomac::Get_Break_Treaty_Message(sender_id, target_id, &response_message);
                }
                break;

            case 7:
                diplomac::Break_Trade(sender_id, target_id);
                if (MOX::player[target_id].objectives == PLAYER_OBJECTIVE_HUMAN) {
                    response_message = 156 /* 0x9c */;
                } else {
                    diplomac::Get_Break_Trade_Message(sender_id, target_id, &response_message);
                }
                break;

            case 8:
                diplomac::Break_Research(sender_id, target_id);
                if (MOX::player[target_id].objectives == PLAYER_OBJECTIVE_HUMAN) {
                    response_message = 158 /* 0x9e */;
                } else {
                    diplomac::Get_Break_Research_Message(sender_id, target_id, &response_message);
                }
                break;

            case 9:
            case 10:
                diplomac::Break_Tribute(sender_id, target_id);
                if (MOX::player[target_id].objectives == PLAYER_OBJECTIVE_HUMAN) {
                    response_message = 171 /* 0xab */;
                } else {
                    diplomac::Get_Break_Tribute_Treaty_Message(sender_id, target_id, &response_message);
                }
                break;

            default:
                break;
        }
    }

    int16_t __cdecl Diplomacy_Declare_War_Resolution(int16_t sender_id, int16_t target_id) {
        char buffer1[400] = {0};
        char buffer2[80];
        int16_t result = 0;

        Setup_Full_Screen_Diplomacy_Help(0x266);

        JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 0x3B, buffer1, 0x28);
        JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 0x3C, buffer2, 0x50);

        if (Diplomacy_Generic_List(1, buffer1, 1, buffer2) != -1) {
            result = 1;

            if (MOX::player[target_id].objectives == PLAYER_OBJECTIVE_HUMAN) {
                if (MOX::game_type == 1) {
                    Diplomacy_Load_New_Ambassador(sender_id);
                    Get_Human_Declare_War_Message(sender_id);
                    Diplomacy_Display_Response();
                    Diplomacy_Load_New_Ambassador(target_id);
                } else {
                    net_diplomacy_message.arg2 = (int16_t)response_message;
                    net_diplomacy_message.sender_player_id = (uint8_t)sender_id;
                    net_diplomacy_message.target_player_id = (uint8_t)target_id;
                    net_diplomacy_message.arg1 = 0;
                    net_diplomacy_message.msg_type = DIP_NET_MSG_DECLARE_WAR;

                    NETMOX::Mox_Send_Message(MOX::player[target_id].network_player_id, DIP_NET_PACKET_MESSAGE, &net_diplomacy_message, sizeof(net_diplomacy_message));
                    RUSS::Mox_Update();
                }
                response_message = 0xA0;
            } else {
                diplomac::Get_Declare_War_Message(sender_id, target_id, &response_message);
            }

            diplomac::Declare_War(sender_id, target_id);
            diplomac::Adjust_Diplomat_Modifiers(sender_id, target_id);
        }

        return result;
    }

    int16_t __cdecl Diplomacy_Surrender(int16_t sender_player_id, int16_t target_player_id) {
        int16_t status = 0;
        char msg6_buffer[40];
        char msg7_buffer[80];

        Setup_Full_Screen_Diplomacy_Help(612);

        JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 6, msg6_buffer, 40);
        JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 7, msg7_buffer, 80);

        if (DIP_SCRN::Diplomacy_Generic_List(1, msg6_buffer, 1, msg7_buffer) != -1) {
            if (COLCALC::May_Surrender_Empire(sender_player_id, target_player_id)) {
                COLCALC::Surrender_Empire(sender_player_id, target_player_id);
            }

            status = 1;
            s_player* target_plr = &MOX::player[target_player_id];

            if (target_plr->objectives == PLAYER_OBJECTIVE_HUMAN) {
                if (MOX::game_type == 1) {
                    DIP_SCRN::Diplomacy_Load_New_Ambassador(sender_player_id);
                    DIP_SCRN::Get_Human_Surrender_Message(sender_player_id);
                    DIP_SCRN::Diplomacy_Display_Response();
                    DIP_SCRN::Diplomacy_Load_New_Ambassador(target_player_id);
                } else {
                    DIP_SCRN::net_diplomacy_message.arg2 = DIP_SCRN::response_message;
                    DIP_SCRN::net_diplomacy_message.sender_player_id = (uint8_t)sender_player_id;
                    DIP_SCRN::net_diplomacy_message.target_player_id = (uint8_t)target_player_id;
                    DIP_SCRN::net_diplomacy_message.arg1 = 0;
                    DIP_SCRN::net_diplomacy_message.msg_type = DIP_NET_MSG_SURRENDER;

                    NETMOX::Mox_Send_Message(target_plr->network_player_id, DIP_NET_PACKET_MESSAGE, &DIP_SCRN::net_diplomacy_message, sizeof(DIP_SCRN::net_diplomacy_message));
                    RUSS::Mox_Update();
                }
            }
        }

        return status;
    }

    void __cdecl Diplomacy_Chat(int16_t player_idx, uint8_t target_player_id) {
        if (DIP_SCRN::chat_mode_flag == 0) {
            DIP_SCRN::Setup_Full_Screen_Diplomacy_Help(0x2b6 /* HELP_NET_CHAT */);
            fields::Clear_Fields();
            DIP_SCRN::chat_mode_flag = 1;
            fonts::Set_Font_Style_Shadow_Down(4, &DIP_SCRN::diplomacy_normal_colors);

            DIP_SCRN::chat_message[0] = *DIP_SCRN::s__00551100;
            fields::continuous_string[0] = DIP_SCRN::s__00551100[0];

            DIP_SCRN::diplomacy_quit_chat_field = fields::Add_Hidden_Field(0, 0, 0x27f, 0x1df, DIP_SCRN::s__00551100, 0);

            DIP_SCRN::diplomacy_chat_field = fields::Add_Continuous_String_Input_Field(
                DIP_SCRN::chat_base_x,
                DIP_SCRN::chat_base_y,
                DIP_SCRN::chat_width - 20,
                DIP_SCRN::chat_height,
                DIP_SCRN::chat_message,
                0x50, // Max 80 chars
                0,
                0,
                0x29, // Font ID
                0);

            fields::active_input_field_first = 1;
            fields::active_input_field_number = DIP_SCRN::diplomacy_chat_field;
            fields::input_field_active = 1;
            fields::cursor_position = 0;
            fields::cursor_on = 0;
        } else {
            if (DIP_SCRN::net_diplomacy_esc_pressed == 1) {
                DIP_SCRN::chat_mode_flag = 0;
            }

            int16_t input_field = fields::Get_Input();

            if (DIP_SCRN::net_diplomacy_esc_pressed == 1) {
                DIP_SCRN::chat_mode_flag = 0;
            }

            if (input_field == DIP_SCRN::diplomacy_chat_field || input_field == DIP_SCRN::diplomacy_quit_chat_field) {
                fields::Clear_Fields();
                DIP_SCRN::chat_mode_flag = 0;

                strlcpy(DIP_SCRN::net_diplomacy_chat_message, DIP_SCRN::chat_message, sizeof(DIP_SCRN::net_diplomacy_chat_message));

                NETMOX::Mox_Send_Message(
                    MOX::player[player_idx].network_player_id,
                    DIP_NET_PACKET_CHAT,
                    DIP_SCRN::net_diplomacy_chat_message,
                    sizeof(DIP_SCRN::net_diplomacy_chat_message));

                RUSS::Mox_Update();
                DIP_SCRN::net_diplomacy_chat_message[0] = *DIP_SCRN::s__00551100;
            }
        }
    }

    /**
     * @brief Runs the network diplomacy chat loop until the chat session closes.
     *
     * @param player_idx The local player participating in the chat session.
     * @param target_player_idx The remote player associated with the session.
     */
    void __cdecl Diplomacy_Main_Net_Chat(int16_t player_idx, int16_t target_player_idx) {
        do {
            timer::Mark_Time();
            DIP_SCRN::Diplomacy_Chat(player_idx, (uint8_t)target_player_idx);

            if (DIP_SCRN::chat_mode_flag != 1) {
                break;
            }

            fields::Invoke_Auto_Function();
            fonts::Set_Font_Style_Shadow_Down(4, &DIP_SCRN::diplomacy_normal_colors);
            fonts::Print(10, 117, DIP_SCRN::enter_your_message_buffer);

            graphics::Darken_Fill(DIP_SCRN::chat_base_x,
                DIP_SCRN::chat_base_y,
                DIP_SCRN::chat_base_x + DIP_SCRN::chat_width,
                DIP_SCRN::chat_base_y + DIP_SCRN::chat_height - 1);

            RUSS::Mox_Sync_Update();
            timer::Release_Time(2);

        } while (DIP_SCRN::chat_mode_flag == 1);
    }

    /**
     * @brief Gets the human player's response to a treaty proposal.
     *
     * @param player_id The player proposing the treaty.
     * @param target_player_id The player receiving the proposal.
     * @param treaty_type The treaty type being proposed.
     * @return The selected response code for the proposal.
     */
    int16_t __cdecl Check_Human_Treaty_Proposal(int16_t player_id, int16_t target_player_id, int16_t treaty_type) {
        int16_t choice;

        switch (treaty_type) {
            case 1:
                response_message = 0x61;
                break;
            case 2:
                response_message = 0x62;
                break;
            case 3:
                response_message = 0x53;
                break;
            case 7:
                response_message = 0x63;
                break;
            case 8:
                response_message = 0x64;
                break;
        }

        if (MOX::game_type == 1 /* SINGLE_PLAYER */) {
            DIP_SCRN::Diplomacy_Load_New_Ambassador(player_id);
            choice = DIP_SCRN::Get_Human_Treaty_Choice(player_id, target_player_id, treaty_type);
            DIP_SCRN::Diplomacy_Load_New_Ambassador(target_player_id);
            return choice;
        }

        DIP_SCRN::net_diplomacy_message.sender_player_id = (uint8_t)player_id;
        DIP_SCRN::net_diplomacy_message.target_player_id = (uint8_t)target_player_id;
        DIP_SCRN::net_diplomacy_message.arg1 = treaty_type;
        DIP_SCRN::net_diplomacy_message.msg_type = DIP_NET_MSG_TREATY_PROPOSAL;
        DIP_SCRN::net_diplomacy_message.arg2 = (int16_t)response_message;

        NETMOX::Mox_Send_Message(MOX::player[target_player_id].network_player_id, DIP_NET_PACKET_MESSAGE, &DIP_SCRN::net_diplomacy_message, sizeof(DIP_SCRN::net_diplomacy_message));
        RUSS::Mox_Update();

        DIP_SCRN::Get_Listen_Mode_Message(target_player_id);
        DIP_SCRN::net_diplomacy_message.arg2 = -1;

        do {
            DIP_SCRN::Diplomacy_Listen_Mode(target_player_id, (uint8_t)player_id);
            if (DIP_SCRN::net_diplomacy_message.arg2 != -1) {
                break;
            }
        } while (DIP_SCRN::net_diplomacy_esc_pressed == 0);

        if (DIP_SCRN::net_diplomacy_esc_pressed == 1) {
            DIP_SCRN::net_diplomacy_message.arg2 = 0;
        }

        return DIP_SCRN::net_diplomacy_message.arg2;
    }

    void __cdecl Get_Diplomacy_Tech_List(int16_t player_a_idx, int16_t player_b_idx, int16_t* tech_count_ptr, uint8_t* tech_list_buffer) {
        int16_t local_count;
        uint8_t local_tech_list[TECH_APP_COUNT];
        int32_t local_buffer[TECH_APP_COUNT]; // Buffer for NPC diplomacy calculations

        // Initialize output buffer
        memset(tech_list_buffer, 0, 10 * sizeof(*tech_list_buffer));

        aitech::Get_Max_Technology_Value(&AIDATA::state.world, player_a_idx);

        // Get list of technologies that player B has but player A does not
        npcdiplo::Get_Differential_Tech_List(
            player_b_idx,
            player_a_idx,
            0,
            (int32_t)0xC343CB00,
            local_tech_list,
            local_buffer,
            &local_count,
            1);

        // Diplomacy UI can only display up to 10 technologies for trade
        if (local_count > 10) {
            local_count = 10;
        }

        *tech_count_ptr = local_count;

        if (local_count > 0) {
            memcpy(tech_list_buffer, local_tech_list, local_count);
        }
    }

    int16_t __cdecl Get_Human_Treaty_Choice(int16_t player_a, int16_t player_b, int16_t treaty_type) {
        char choice_text[3][40];
        char title_text[40];
        int16_t num_choices = 2;
        int16_t selected_idx;
        int16_t result = 0;

        JIM::Get_Text_Message("jimtext.lbx", 0x4F, choice_text[0], 40);
        JIM::Get_Text_Message("jimtext.lbx", 0x50, choice_text[1], 40);

        choice_text[2][0] = DIP_SCRN::s_empty[0];

        if (DIP_SCRN::Check_Better_Offer_Available(player_a, player_b) == 1) {
            num_choices = 3;
            JIM::Get_Text_Message("jimtext.lbx", 0x52, choice_text[2], 40);
        }

        DIP_SCRN::response_message = 0;
        DIP_SCRN::old_response_message = 0;

        DIP_SCRN::Get_Human_Propose_Treaty_Message(treaty_type);
        fields::Deactivate_Help_List();

        JIM::Get_Text_Message("jimtext.lbx", 0x4A, title_text, 40);

        selected_idx = DIP_SCRN::Diplomacy_Generic_List(num_choices, (char*)choice_text, 0, title_text);

        if (num_choices == 2 && selected_idx == -1) {
            selected_idx = 1;
        } else if (num_choices == 3 && selected_idx == -1) {
            selected_idx = 2;
        }

        if (selected_idx == 0) {
            result = 3;
            switch (treaty_type) {
                case 1: DIP_SCRN::response_message = 0x83; break;
                case 2: DIP_SCRN::response_message = 0x84; break;
                case 3: DIP_SCRN::response_message = 0x87; break;
                case 7: DIP_SCRN::response_message = 0x85; break;
                case 8: DIP_SCRN::response_message = 0x86; break;
                default: break;
            }
        } else if (selected_idx == 1) {
            DIP_SCRN::response_message = 0x8A;
            result = 0;
        } else if (selected_idx == 2) {
            result = 1;
        }

        return result;
    }

    void __cdecl Get_Listen_Mode_Message(int16_t race_idx) {
        response_message = 0;
        old_response_message = 0;

        JIM::Get_Text_Message(s_jimtext2_lbx, 12, diplomacy_message, 250);
        JIM::Decode_Text_Race(diplomacy_message, race_idx, 250);
    }

    void __cdecl Diplomacy_Listen_Mode(int16_t player_idx, uint8_t target_player_id) {
        int16_t field_cancel;
        int16_t field_goodbye;
        int16_t hotkey_space;
        int16_t input_field;

        fields::Assign_Auto_Function(Draw_Diplomacy_Listen_Mode, 2);
        timer::Mark_Time();

        if (chat_mode_flag == 0) {
            diplomacy_chat_field = (int16_t)0xfc18 /* -1000 */;
            fields::Clear_Fields();
            Setup_Full_Screen_Diplomacy_Help(0x2b6);

            field_cancel = fields::Add_Hidden_Field(15, 139, 157, 162, "C", 0);
            field_goodbye = fields::Add_Hidden_Field(15, 165, 157, 188, "G", 0);
            hotkey_space = fields::Add_Hot_Key("\x1B");

            input_field = fields::Get_Input();

            if (input_field == field_goodbye || input_field == hotkey_space) {
                net_diplomacy_message.sender_player_id = (uint8_t)player_idx;
                net_diplomacy_message.target_player_id = target_player_id;
                net_diplomacy_message.arg2 = -1;
                net_diplomacy_message.msg_type = DIP_NET_MSG_GOODBYE;

                NETMOX::Mox_Send_Message(
                    MOX::player[player_idx].network_player_id,
                    DIP_NET_PACKET_MESSAGE,
                    &net_diplomacy_message,
                    sizeof(net_diplomacy_message));
                RUSS::Mox_Update();
                net_diplomacy_esc_pressed = 1;
            }

            if (input_field == field_cancel) {
                Diplomacy_Chat(player_idx, target_player_id);
            }
        } else {
            Diplomacy_Chat(player_idx, target_player_id);
        }

        Draw_Diplomacy_Listen_Mode();

        if (chat_mode_flag == 1) {
            graphics::Darken_Fill(
                chat_base_x,
                chat_base_y,
                chat_base_x + chat_width,
                chat_base_y + chat_height - 1);
        }

        RUSS::Mox_Sync_Update();
        timer::Release_Time(2);
        fields::Assign_Auto_Function(Draw_Diplomacy_Synch_Mode, 2);
    }

    void __cdecl Get_Human_Declare_War_Message(int16_t race_idx) {
        JIM::Get_Text_Message(s_jimtext2_lbx, 14, diplomacy_message, 250);
        JIM::Decode_Text_Race(diplomacy_message, (int16_t)race_idx, 250);
    }

    void __cdecl Get_Human_Break_Treaty_Message(int16_t race_idx, int16_t treaty_type) {
        switch (treaty_type) {
            case 1:
                JIM::Get_Text_Message(s_jimtext2_lbx, 30, diplomacy_message, 250);
                JIM::Decode_Text_Race(diplomacy_message, race_idx, 250);
                break;
            case 2:
                JIM::Get_Text_Message(s_jimtext2_lbx, 31, diplomacy_message, 250);
                JIM::Decode_Text_Race(diplomacy_message, race_idx, 250);
                break;
            case 7:
                JIM::Get_Text_Message(s_jimtext2_lbx, 27, diplomacy_message, 250);
                JIM::Decode_Text_Race(diplomacy_message, race_idx, 250);
                break;
            case 8:
                JIM::Get_Text_Message(s_jimtext2_lbx, 28, diplomacy_message, 250);
                JIM::Decode_Text_Race(diplomacy_message, race_idx, 250);
                break;
            case 9:
            case 10:
                JIM::Get_Text_Message(s_jimtext2_lbx, 29, diplomacy_message, 250);
                JIM::Decode_Text_Race(diplomacy_message, race_idx, 250);
                break;
            default:
                break;
        }
    }

    void __cdecl Get_Human_Propose_Treaty_Message(int16_t treaty_type) {
        switch (treaty_type) {
            case 1:
                JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 45, diplomacy_message, 250);
                break;
            case 2:
                JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 46, diplomacy_message, 250);
                break;
            case 3:
                JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 66, diplomacy_message, 250);
                break;
            case 7:
                JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 42, diplomacy_message, 250);
                break;
            case 8:
                JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 43, diplomacy_message, 250);
                break;
            case 9:
            case 10:
                JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 44, diplomacy_message, 250);
                break;
            default:
                break;
        }
    }

    void __cdecl Get_Human_Surrender_Message(int16_t race_idx) {
        JIM::Get_Text_Message(DIP_SCRN::s_jimtext2_lbx, 13, DIP_SCRN::diplomacy_message, 250);
        JIM::Decode_Text_Race(DIP_SCRN::diplomacy_message, race_idx, 250);
    }

    void __cdecl Draw_Diplomacy_Listen_Mode(void) {
        Draw_Diplomacy_Synch_Mode();
        fonts::Set_Font_Style_Shadow_Down(4, &diplomacy_normal_colors);

        if (chat_mode_flag == 0) {
            fonts::Print(10, 117 /* 0x75 */, net_listen_mode_option_buffer);
            fonts::Print(10, 141 /* 0x8D */, main_net_diplomacy_list[7]);
            fonts::Print(18 /* 0x12 */, 166 /* 0xA6 */, net_listen_mode_good_bye_buffer);
        } else {
            fonts::Print(10, 117 /* 0x75 */, enter_your_message_buffer);
        }
    }

    /**
     * @brief Checks if there are better offers (systems, tech, or significant BC) available from a player.
     * 
     * @param player_idx The index of the player offering.
     * @param other_player_idx The index of the player being offered to.
     * @return 1 if a better offer is available, 0 otherwise.
     */
    int16_t __cdecl Diplomacy_System_Display(int16_t giver_idx, int16_t receiver_idx, int16_t display_mode) {
        int16_t y, x;
        int16_t exit_field_id;
        int16_t input;
        int16_t scanned_input;
        int16_t result_star_idx = -1;
        bool done = false;
        s_animation_header* saved_star_bg_seg;

        MOX::current_screen = 6;
        saved_star_bg_seg = MOX::star_bg_seg;
        MOX::star_bg_seg = DIP_SCRN::diplomacy_system_star_background_seg;
        DIP_SCRN::diplomacy_system_giver = (uint8_t)giver_idx;
        DIP_SCRN::diplomacy_system_receiver = (uint8_t)receiver_idx;
        COLONY::using_colony_screen_palette = 1;
        MOX::star_bg_index = 0;

        if (display_mode == 1) {
            DIP_SCRN::Setup_Full_Screen_Diplomacy_Help(0x261);
        } else {
            DIP_SCRN::Setup_Full_Screen_Diplomacy_Help(0x268);
        }

        fields::Clear_Fields();
        fields::Assign_Auto_Function(DIP_SCRN::Draw_Diplomacy_System_Display, 2);

        int16_t text_msg_idx = (display_mode == 1) ? 0x3F : 0x37;
        JIM::Get_Text_Message(DIP_SCRN::s_jimtext2_lbx, text_msg_idx, DIP_SCRN::diplomacy_message, 0xFA);
        JIM::Decode_Text_Name_And_Name(DIP_SCRN::diplomacy_message, receiver_idx, giver_idx, 0xFA);

        DIP_SCRN::diplomacy_scan_input_flag = -1;
        fields::Clear_Fields();

        for (int16_t i = 0; i < MOX::NUM_STARS; ++i) {
            MOVEBOX::Get_Galaxy_Map_Star_XY(i, 10, 0xAB, 0xB4, 0x6E, 0, 0, 1, &x, &y);
            int16_t field_id = fields::Add_Hidden_Field(x, y, x + 5, y + 5, s__00551100, 0);
            DIP_SCRN::diplomacy_system_fields[i + 1] = field_id;
        }

        exit_field_id = fields::Add_Hidden_Field(0xF, 0x8B, 0x9D, 0xA2, s__00551100, 0);

        while (true) {
            timer::Mark_Time();
            DIP_SCRN::diplomacy_scan_input_flag = -1;
            input = fields::Get_Input();
            scanned_input = fields::Scan_Input();
            MOX::galaxy_map_scanned_star = -1;
            DIP_SCRN::diplomacy_scan_input_flag = scanned_input;

            if (scanned_input > 0) {
                if (scanned_input == exit_field_id) {
                    DIP_SCRN::diplomacy_scan_input_flag = 1000;
                } else if (scanned_input >= DIP_SCRN::diplomacy_system_fields[1] && scanned_input <= DIP_SCRN::diplomacy_system_fields[MOX::NUM_STARS]) {
                    if (DIP_SCRN::Diplomacy_System_Scan(scanned_input - 1, giver_idx, &DIP_SCRN::diplomacy_system_homeworld_flag, &DIP_SCRN::diplomacy_system_planet_count, DIP_SCRN::diplomacy_system_planet_list) == giver_idx) {
                        MOX::galaxy_map_scanned_star = scanned_input - 1;
                        DIP_SCRN::diplomacy_system_population = COLCALC::Get_System_Population(giver_idx, MOX::galaxy_map_scanned_star);
                        JIM::Get_Text_Message(DIP_SCRN::s_jimtext2_lbx, 0x38, DIP_SCRN::system_million_colonists_buffer, 0x1E);
                        JIM::Decode_Text_Stat(DIP_SCRN::system_million_colonists_buffer, DIP_SCRN::diplomacy_system_population, 0x1E);
                        JIM::Get_Text_Message(DIP_SCRN::s_jimtext2_lbx, 0x3A, DIP_SCRN::system_x_star_system_buffer, 0x23);
                        JIM::Decode_Text_System(DIP_SCRN::system_x_star_system_buffer, MOX::galaxy_map_scanned_star, 0x23);
                    } else {
                        DIP_SCRN::diplomacy_scan_input_flag = -1;
                    }
                } else {
                    DIP_SCRN::diplomacy_scan_input_flag = -1;
                }
            } else {
                DIP_SCRN::diplomacy_scan_input_flag = -1;
            }

            if (input == exit_field_id || DIP_SCRN::net_diplomacy_esc_pressed == 1) {
                done = true;
            }

            if (input != 0 && !done) {
                DIP_SCRN::diplomacy_scan_input_flag = -1;
                for (int16_t i = 0; i < MOX::NUM_STARS; ++i) {
                    if (done) break;
                    if (DIP_SCRN::diplomacy_system_fields[i + 1] == input) {
                        if (DIP_SCRN::Diplomacy_System_Scan(input - 1, giver_idx, &DIP_SCRN::diplomacy_system_homeworld_flag, &DIP_SCRN::diplomacy_system_planet_count, DIP_SCRN::diplomacy_system_planet_list) == giver_idx) {
                            if (COLCALC::May_Surrender_Star(input - 1, (int16_t)DIP_SCRN::diplomacy_system_giver, (int16_t)DIP_SCRN::diplomacy_system_receiver)) {
                                if (DIP_SCRN::diplomacy_system_homeworld_flag == -1) {
                                    done = true;
                                    result_star_idx = i;
                                }
                            }
                        }
                    }
                }
            }

            if (done) break;

            DIP_SCRN::Draw_Diplomacy_System_Display();
            RUSS::Mox_Sync_Update();
            timer::Release_Time(2);
        }

        fields::Clear_Fields();
        fields::Deactivate_Auto_Function();
        MOX::star_bg_seg = saved_star_bg_seg;
        COLONY::using_colony_screen_palette = 0;

        return result_star_idx;
    }

    int16_t __cdecl Check_Better_Offer_Available(int16_t player_idx, int16_t other_player_idx) {
        if (Check_Surrender_System(player_idx, other_player_idx)) {
            return 1;
        }

        int16_t tech_count;
        uint8_t tech_list[12];
        Get_Diplomacy_Tech_List(player_idx, other_player_idx, &tech_count, tech_list);

        if (tech_count > 0) {
            return 1;
        }

        // Check if player has more than 100 BC
        if (MOX::player[player_idx].bc > 100) {
            return 1;
        }

        return 0;
    }

    void __cdecl Draw_Diplomacy_System() {
        graphics::Box(9, 0xaa, 0xb6, 0x70, 1, 0xfb);
        graphics::Darken_Fill(0xa, 0xab, 0xbe, 0x118);
        MOVEBOX::Draw_Galaxy_Map_Box(nullptr, nullptr, 0xa, 0xab, 0xb4, 0x6e, 0, 0, 0, 0, 4, 0);
    }

    void __cdecl Draw_Diplomacy_System_Display() {
        Draw_Diplomacy_Synch_Mode();
        Draw_Diplomacy_System();

        fonts::Set_Font_Style_Shadow_Down(4, &diplomacy_normal_colors);
        fonts::Print(10, 0x75, choose_a_system_buffer);

        if (diplomacy_scan_input_flag == 1000) {
            fonts::Set_Font_Style_Shadow_Down(4, &diplomacy_highlight_colors);
            diplomacy_scan_input_flag = -1;
        }

        fonts::Print(0x12, 0x8D, system_cancel_buffer);

        if (diplomacy_scan_input_flag != -1) {
            Draw_Diplomacy_System_Text(diplomacy_scan_input_flag - 1, diplomacy_system_homeworld_flag);
        }
    }

    void __cdecl Load_Diplomacy_Text_Messages() {
        RUSS::Mox_Update();
        JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x1E, main_diplomacy_title, 0x1E);
        JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x1F, main_diplomacy_list[0], 0x1E);
        JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x20, main_diplomacy_list[1], 0x1E);
        RUSS::Mox_Update();

        JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x21, main_diplomacy_list[2], 0x1E);
        JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x22, main_diplomacy_list[3], 0x1E);
        RUSS::Mox_Update();

        JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x23, main_diplomacy_list[4], 0x1E);
        JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x24, main_diplomacy_list[5], 0x1E);
        RUSS::Mox_Update();

        JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x25, main_diplomacy_list[6], 0x1E);
        JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x26, main_diplomacy_list[7], 0x1E);

        *main_diplomacy_list[8] = *s__00551100;
        RUSS::Mox_Update();

        strlcpy(main_net_diplomacy_list[0], main_diplomacy_list[0], DIP_SCRN_MAIN_LIST_CAPACITY);
        strlcpy(main_net_diplomacy_list[1], main_diplomacy_list[1], DIP_SCRN_MAIN_LIST_CAPACITY);
        strlcpy(main_net_diplomacy_list[2], main_diplomacy_list[2], DIP_SCRN_MAIN_LIST_CAPACITY);
        strlcpy(main_net_diplomacy_list[3], main_diplomacy_list[3], DIP_SCRN_MAIN_LIST_CAPACITY);
        strlcpy(main_net_diplomacy_list[4], main_diplomacy_list[4], DIP_SCRN_MAIN_LIST_CAPACITY);
        strlcpy(main_net_diplomacy_list[5], main_diplomacy_list[5], DIP_SCRN_MAIN_LIST_CAPACITY);
        strlcpy(main_net_diplomacy_list[6], main_diplomacy_list[6], DIP_SCRN_MAIN_LIST_CAPACITY);

        JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 0x41, main_net_diplomacy_list[7], 0x28);
        JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x27, main_net_diplomacy_list[8], 0x28);

        strlcpy(main_net_diplomacy_list[9], main_diplomacy_list[7], DIP_SCRN_MAIN_LIST_CAPACITY);
        *main_net_diplomacy_list[10] = *s__00551100;

        JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x2B, main_repulsive_diplomacy_list[0], 0x1E);

        strlcpy(main_repulsive_diplomacy_list[1], main_diplomacy_list[5], DIP_SCRN_SMALL_LIST_CAPACITY);
        strlcpy(main_repulsive_diplomacy_list[2], main_diplomacy_list[6], DIP_SCRN_SMALL_LIST_CAPACITY);
        strlcpy(main_repulsive_diplomacy_list[3], main_diplomacy_list[7], DIP_SCRN_SMALL_LIST_CAPACITY);

        *main_repulsive_diplomacy_list[4] = *s__00551100;
        RUSS::Mox_Update();

        strlcpy(main_net_repulsive_diplomacy_list[0], main_repulsive_diplomacy_list[0], DIP_SCRN_SMALL_LIST_CAPACITY);
        strlcpy(main_net_repulsive_diplomacy_list[1], main_diplomacy_list[5], DIP_SCRN_SMALL_LIST_CAPACITY);
        strlcpy(main_net_repulsive_diplomacy_list[2], main_diplomacy_list[6], DIP_SCRN_SMALL_LIST_CAPACITY);
        strlcpy(main_net_repulsive_diplomacy_list[3], main_net_diplomacy_list[7], DIP_SCRN_SMALL_LIST_CAPACITY);
        strlcpy(main_net_repulsive_diplomacy_list[4], main_diplomacy_list[7], DIP_SCRN_SMALL_LIST_CAPACITY);

        *main_net_repulsive_diplomacy_list[5] = *s__00551100;
        RUSS::Mox_Update();

        JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x28, propose_treaty_title, 0x28);
        JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x29, propose_treaty_diplomacy_list[0], 0x1E);
        JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x2A, propose_treaty_diplomacy_list[1], 0x1E);
        RUSS::Mox_Update();

        JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x2B, propose_treaty_diplomacy_list[2], 0x1E);
        JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x2C, propose_treaty_diplomacy_list[3], 0x1E);
        RUSS::Mox_Update();

        JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x2D, propose_treaty_diplomacy_list[4], 0x1E);
        JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x2E, propose_treaty_diplomacy_list[5], 0x1E);

        *propose_treaty_diplomacy_list[6] = *s__00551100;
        RUSS::Mox_Update();

        JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x2F, break_treaty_title, 0x1E);
        RUSS::Mox_Update();

        JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x30, break_treaty_diplomacy_list[0], 0x28);
        JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x31, break_treaty_diplomacy_list[1], 0x28);
        RUSS::Mox_Update();

        JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x32, break_treaty_diplomacy_list[2], 0x28);
        JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x33, break_treaty_diplomacy_list[3], 0x28);
        RUSS::Mox_Update();

        JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x34, break_treaty_diplomacy_list[4], 0x28);
        JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x2E, break_treaty_diplomacy_list[5], 0x28);

        *break_treaty_diplomacy_list[6] = *s__00551100;
        RUSS::Mox_Update();

        JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x35, demand_title, 0x1E);
        RUSS::Mox_Update();

        JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x36, demand_diplomacy_list[0], 0x28);
        JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x37, demand_diplomacy_list[1], 0x28);
        RUSS::Mox_Update();

        JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x38, demand_diplomacy_list[2], 0x28);
        RUSS::Mox_Update();

        JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x39, demand_diplomacy_list[3], 0x28);
        JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x3A, demand_diplomacy_list[4], 0x28);
        RUSS::Mox_Update();

        JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x3B, demand_diplomacy_list[5], 0x28);
        JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x3C, demand_diplomacy_list[6], 0x28);
        RUSS::Mox_Update();

        JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x3D, demand_diplomacy_list[7], 0x28);
        JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x3E, demand_diplomacy_list[8], 0x28);
        RUSS::Mox_Update();

        JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x2E, demand_diplomacy_list[9], 0x23);
        *demand_diplomacy_list[10] = *s__00551100;
        RUSS::Mox_Update();

        JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x3F, offer_title, 0x1E);
        JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x40, offer_diplomacy_list[0], 0x1E);
        RUSS::Mox_Update();

        JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x41, offer_diplomacy_list[1], 0x1E);
        JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x42, offer_diplomacy_list[2], 0x1E);
        RUSS::Mox_Update();

        JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x43, offer_diplomacy_list[3], 0x1E);
        RUSS::Mox_Update();

        JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x44, offer_diplomacy_list[4], 0x1E);
        JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x2E, offer_diplomacy_list[5], 0x1E);
        *offer_diplomacy_list[6] = *s__00551100;

        JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x45, agree_diplomacy_list[0], DIP_SCRN_MAIN_LIST_CAPACITY);
        JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x46, agree_diplomacy_list[1], DIP_SCRN_MAIN_LIST_CAPACITY);
        *agree_diplomacy_list[2] = *s__00551100;
        RUSS::Mox_Update();

        JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 0x02, choose_a_system_buffer, 0x28);
        RUSS::Mox_Update();

        JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x2E, system_cancel_buffer, 0x14);
        JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 0x38, system_million_colonists_buffer, 0x1E);
        RUSS::Mox_Update();

        JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 0x39, system_homeworld_buffer, 0x1E);
        JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 0x3A, system_x_star_system_buffer, 0x23);
        JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 0x43, player_number_name_buffer, 0x28);
        JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 0x44, player_name_ambassador_buffer, 0x28);
        JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 0x45, you_were_framed_buffer, 0x28);
        JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 0x46, system_unsurrenderable_buffer, 0x28);
        JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 0x47, enter_your_message_buffer, 0x28);
        JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 0x48, percent_tribute_treaty_buffer, 0x28);
        JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 0x49, statement_trade_treaty_buffer, 0x28);
        JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 0x4A, statement_research_treaty_buffer, 0x28);
        JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 0x0A, net_listen_mode_option_buffer, 0x14);
        JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 0x0B, net_listen_mode_good_bye_buffer, 0x14);
        RUSS::Mox_Update();
    }

    void __cdecl Diplomacy_Exchange_Technology(int16_t player_1, int16_t player_2) {
        char diplomacy_list[3][40];

        for (int i = 0; i < 3; i++) {
            strlcpy(diplomacy_list[i], agree_diplomacy_list[i], sizeof(diplomacy_list[i]));
        }

        uint16_t accepted = 0;

        if (exchange_tech_count > 0) {
            char text_msg[40];
            JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 3, text_msg, 40);

            int16_t selected_tech_idx = Diplomacy_Generic_Exchange_Tech_List(
                player_1, player_2, text_msg, target_tech_list, exchange_tech_count, 0);

            if (selected_tech_idx != -1) {
                uint16_t tech_1 = target_tech_list[selected_tech_idx];
                uint16_t tech_2 = 0;

                if (MOX::player[player_2].objectives == PLAYER_OBJECTIVE_HUMAN) {
                    if (MOX::game_type == 1) {
                        Diplomacy_Load_New_Ambassador(player_1);
                        Get_Human_Tech_Exchange_First_Message(player_1, player_2, tech_1);
                        JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 4, text_msg, 40);

                        int32_t tech_categories[TECH_APP_COUNT];
                        memset(tech_categories, 0, sizeof(tech_categories));
                        memset(tech_categories, 1, 15);

                        npcdiplo::Get_Differential_Tech_List(
                            player_2, player_1, 0, 0xC343CB00, target_tech_list, tech_categories, &exchange_tech_count, 1);

                        int16_t ex_selected_tech_idx = Diplomacy_Generic_Exchange_Tech_List(
                            player_2, player_1, text_msg, target_tech_list, exchange_tech_count, 0);

                        Diplomacy_Load_New_Ambassador(player_2);

                        if (ex_selected_tech_idx != -1) {
                            tech_2 = target_tech_list[ex_selected_tech_idx];
                            Get_Human_Tech_Exchange_Message(player_2, player_1, tech_1, tech_2);
                            JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 5, text_msg, 40);

                            int16_t action = Diplomacy_Generic_List(2, (char*)diplomacy_list, 0, text_msg);
                            if (action == -1) {
                                Get_Human_Tech_Exchange_Decline_Message();
                            } else {
                                accepted = 1;
                                Get_Human_Tech_Exchange_Accept_Message();
                            }
                        }
                    } else {
                        net_diplomacy_message.arg2 = 0;
                        net_diplomacy_message.sender_player_id = (uint8_t)player_1;
                        net_diplomacy_message.msg_type = DIP_NET_MSG_TECH_EXCHANGE_PROPOSE;
                        net_diplomacy_message.target_player_id = (uint8_t)player_2;
                        net_diplomacy_message.arg1 = tech_1;

                        NETMOX::Mox_Send_Message(MOX::player[player_2].network_player_id, DIP_NET_PACKET_MESSAGE, &net_diplomacy_message, sizeof(net_diplomacy_message));
                        RUSS::Mox_Update();

                        Get_Listen_Mode_Message(player_2);
                        net_diplomacy_message.arg2 = -2;

                        do {
                            Diplomacy_Listen_Mode(player_2, (uint8_t)player_1);
                            if (net_diplomacy_message.arg2 != -2) {
                                break;
                            }
                        } while (net_diplomacy_esc_pressed == 0);

                        if (net_diplomacy_esc_pressed == 1) {
                            net_diplomacy_message.arg2 = -1;
                        }

                        int16_t ex_selected_tech_idx = net_diplomacy_message.arg2;
                        if (ex_selected_tech_idx != -1) {
                            tech_2 = (uint16_t)ex_selected_tech_idx;
                            Get_Human_Tech_Exchange_Message(player_2, player_1, tech_1, tech_2);
                            JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 5, text_msg, 40);

                            int16_t action = Diplomacy_Generic_List(2, (char*)diplomacy_list, 0, text_msg);

                            net_diplomacy_message.arg1 = action;
                            net_diplomacy_message.sender_player_id = (uint8_t)player_1;
                            net_diplomacy_message.msg_type = DIP_NET_MSG_TECH_EXCHANGE_DECISION;
                            net_diplomacy_message.target_player_id = (uint8_t)player_2;
                            net_diplomacy_message.arg2 = tech_2;

                            NETMOX::Mox_Send_Message(MOX::player[player_2].network_player_id, DIP_NET_PACKET_MESSAGE, &net_diplomacy_message, sizeof(net_diplomacy_message));
                            RUSS::Mox_Update();

                            if (action == -1) {
                                Get_Human_Tech_Exchange_Decline_Message();
                            } else {
                                accepted = 1;
                                Get_Human_Tech_Exchange_Accept_Message();
                            }
                        }
                    }
                } else if (exchange_tech_list[selected_tech_idx] == 0) {
                    old_response_message = -1;
                    response_message = 0xAE;
                    npcdiplo::NPC_Message_Rejection_Check(player_2, player_1, &response_message);
                } else {
                    tech_2 = exchange_tech_list[selected_tech_idx];
                    Get_Human_Tech_Exchange_Message(player_2, player_1, tech_1, tech_2);
                    JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 5, text_msg, 40);

                    int16_t action = Diplomacy_Generic_List(2, (char*)diplomacy_list, 0, text_msg);
                    if (action == -1) {
                        *diplomacy_message = MOX::s__00551100[0];
                    } else {
                        accepted = 1;
                        Get_Human_Tech_Exchange_Accept_Message();
                    }
                }

                if (accepted == 1) {
                    diplomac::Change_Relations(12, player_1, player_2, 0, 0, 0);
                    COLCALC::Player_Gets_Tech_App(&MOX::player[player_1], tech_1);
                    COLCALC::Player_Gets_Tech_App(&MOX::player[player_2], tech_2);

                    if (MOX::player[player_2].objectives != PLAYER_OBJECTIVE_HUMAN) {
                        npcdiplo::Get_Exchange_Tech_List(player_1, player_2, &exchange_tech_count, target_tech_list, exchange_tech_list);
                        diplomac::Adjust_Diplomat_Modifiers(player_1, player_2);
                        MOX::player[player_1].tech_exchange_modifier[player_2] -= 20;
                    }
                }
            }
        }
    }

    int16_t __cdecl Diplomacy_Human_Need_Better_Offer(int16_t player_idx, int16_t target_player_idx) {
        char list_items[10][40];
        char list_title[40];
        uint8_t tech_list[12];
        int16_t result;
        int16_t tech_list_count;
        int16_t list_count;
        int16_t offer_system_id;
        int16_t offer_money_id;
        int16_t offer_tech_id;
        int16_t selection;
        int16_t system_sel;
        int16_t confirm;
        int16_t money_amount;
        int16_t tech_sel;
        int16_t tech_app_id;

        chat_mode_flag = 0;
        result = 0;
        offer_tech_id = -10;
        offer_system_id = -10;
        offer_money_id = -10;

        JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 51, diplomacy_message, 250);
        JIM::Decode_Text_Race(diplomacy_message, target_player_idx, 250);

        list_count = 0;

        if (Check_Surrender_System(player_idx, target_player_idx)) {
            JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 52, list_items[list_count], 40);
            offer_system_id = list_count;
            list_count++;
        }

        Get_Diplomacy_Tech_List(player_idx, target_player_idx, &tech_list_count, tech_list);
        if (tech_list_count > 0) {
            JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 53, list_items[list_count], 40);
            offer_tech_id = list_count;
            list_count++;
        }

        if (MOX::player[player_idx].bc > 100) {
            JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 64, list_items[list_count], 40);
            offer_money_id = list_count;
            list_count++;
        }

        list_items[list_count][0] = s__00551100[0];

        if (list_count > 0) {
            result = 0;
            fields::Deactivate_Help_List();
            JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 54, list_title, 40);
            selection = Diplomacy_Generic_List(list_count, (char*)list_items, 1, list_title);

            if (selection == offer_system_id) {
                system_sel = Diplomacy_System_Display(player_idx, current_ambassador, 0);
                if (system_sel != -1) {
                    snprintf(list_items[0], sizeof(list_items[0]), s_fmt_s_0055236c, MOX::star[system_sel].name);
                    if (MOX::game_type == 1) {
                        Diplomacy_Load_New_Ambassador(target_player_idx);
                        confirm = Diplomacy_Generic_System_List(system_sel, player_idx, target_player_idx, 0);
                        if (confirm != -1) {
                            if (COLCALC::May_Surrender_Star(system_sel, player_idx, target_player_idx)) {
                                COLCALC::Surrender_Star(system_sel, player_idx, target_player_idx);
                            }
                            result = 1;
                        }
                    } else {
                        net_diplomacy_message.sender_player_id = (uint8_t)player_idx;
                        net_diplomacy_message.target_player_id = (uint8_t)target_player_idx;
                        net_diplomacy_message.arg1 = system_sel;
                        net_diplomacy_message.msg_type = DIP_NET_MSG_BETTER_OFFER_SYSTEM;
                        net_diplomacy_message.arg2 = response_message;

                        NETMOX::Mox_Send_Message(MOX::player[target_player_idx].network_player_id, DIP_NET_PACKET_MESSAGE, &net_diplomacy_message, sizeof(net_diplomacy_message));
                        RUSS::Mox_Update();
                        Get_Listen_Mode_Message(target_player_idx);
                        net_diplomacy_message.arg2 = -1;
                        do {
                            Diplomacy_Listen_Mode(target_player_idx, (uint8_t)player_idx);
                            if (net_diplomacy_message.arg2 != -1) break;
                        } while (net_diplomacy_esc_pressed == 0);

                        if (net_diplomacy_esc_pressed == 1) {
                            net_diplomacy_message.arg2 = 0;
                        }

                        result = net_diplomacy_message.arg2;
                        if (result == 1) {
                            if (COLCALC::May_Surrender_Star(system_sel, player_idx, target_player_idx)) {
                                COLCALC::Surrender_Star(system_sel, player_idx, target_player_idx);
                            }
                        }
                    }
                }
            } else if (selection == offer_tech_id) {
                if (tech_list_count > 8) {
                    tech_list_count = 8;
                }
                if (tech_list_count > 0) {
                    for (int i = 0; i < tech_list_count; i++) {
                        char* tech_name = TECH::Technology_Applications_Name(tech_list[i], MOX::PLAYER_NUM);
                        snprintf(list_items[i], sizeof(list_items[i]), s_fmt_s_0055236c, tech_name);
                    }
                }
                tech_sel = Diplomacy_Generic_List(tech_list_count, (char*)list_items, 1, offer_title);
                if (tech_sel != -1) {
                    tech_app_id = tech_list[tech_sel];
                    char* tech_name = TECH::Technology_Applications_Name(tech_app_id, player_idx);
                    snprintf(list_items[0], sizeof(list_items[0]), s_fmt_s_0055236c, tech_name);

                    if (MOX::game_type == 1) {
                        Diplomacy_Load_New_Ambassador(player_idx);
                        JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 9, list_title, 40);
                        confirm = Diplomacy_Generic_List(1, (char*)list_items, 1, list_title);
                        if (confirm != -1) {
                            COLCALC::Player_Gets_Tech_App(&MOX::player[target_player_idx], tech_app_id);
                            result = 1;
                        }
                        Diplomacy_Load_New_Ambassador(target_player_idx);
                    } else {
                        net_diplomacy_message.sender_player_id = (uint8_t)player_idx;
                        net_diplomacy_message.target_player_id = (uint8_t)target_player_idx;
                        net_diplomacy_message.arg1 = tech_app_id;
                        net_diplomacy_message.msg_type = DIP_NET_MSG_BETTER_OFFER_TECH;
                        net_diplomacy_message.arg2 = response_message;

                        NETMOX::Mox_Send_Message(MOX::player[target_player_idx].network_player_id, DIP_NET_PACKET_MESSAGE, &net_diplomacy_message, sizeof(net_diplomacy_message));
                        RUSS::Mox_Update();
                        net_diplomacy_message.arg2 = -1;
                        Get_Listen_Mode_Message(target_player_idx);
                        do {
                            Diplomacy_Listen_Mode(target_player_idx, (uint8_t)player_idx);
                            if (net_diplomacy_message.arg2 != -1) break;
                        } while (net_diplomacy_esc_pressed == 0);

                        if (net_diplomacy_esc_pressed == 1) {
                            net_diplomacy_message.arg2 = 0;
                        }

                        result = net_diplomacy_message.arg2;
                        if (result == 1) {
                            COLCALC::Player_Gets_Tech_App(&MOX::player[target_player_idx], tech_app_id);
                        }
                    }
                }
            } else if (selection == offer_money_id) {
                money_amount = Diplomacy_Offer_Money(player_idx, target_player_idx, 0);
                if (money_amount > 0) {
                    snprintf(list_items[0], sizeof(list_items[0]), s_fmt_d_bc_00552384, money_amount);
                    if (MOX::game_type == 1) {
                        Diplomacy_Load_New_Ambassador(player_idx);
                        JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 9, list_title, 40);
                        confirm = Diplomacy_Generic_List(1, (char*)list_items, 1, list_title);
                        if (confirm != -1) {
                            MOX::player[player_idx].bc -= money_amount;
                            MOX::player[target_player_idx].bc += money_amount;
                            result = 1;
                        } else {
                            result = 0;
                        }
                        Diplomacy_Load_New_Ambassador(target_player_idx);
                    } else {
                        net_diplomacy_message.sender_player_id = (uint8_t)player_idx;
                        net_diplomacy_message.target_player_id = (uint8_t)target_player_idx;
                        net_diplomacy_message.arg1 = money_amount;
                        net_diplomacy_message.msg_type = DIP_NET_MSG_BETTER_OFFER_BC;
                        net_diplomacy_message.arg2 = response_message;

                        NETMOX::Mox_Send_Message(MOX::player[target_player_idx].network_player_id, DIP_NET_PACKET_MESSAGE, &net_diplomacy_message, sizeof(net_diplomacy_message));
                        RUSS::Mox_Update();
                        net_diplomacy_message.arg2 = -1;
                        Get_Listen_Mode_Message(target_player_idx);
                        do {
                            Diplomacy_Listen_Mode(target_player_idx, (uint8_t)player_idx);
                            if (net_diplomacy_message.arg2 != -1) break;
                        } while (net_diplomacy_esc_pressed == 0);

                        if (net_diplomacy_esc_pressed == 1) {
                            net_diplomacy_message.arg2 = 0;
                        }

                        result = net_diplomacy_message.arg2;
                        if (result == 1) {
                            MOX::player[player_idx].bc -= money_amount;
                            MOX::player[target_player_idx].bc += money_amount;
                        }
                    }
                }
            }
        }

        return result;
    }

    void __cdecl Diplomacy_Demand(int16_t player_num, int16_t ambassador_idx) {
        uint8_t demand_list_flags[17];
        for (int i = 0; i < 17; ++i) {
            demand_list_flags[i] = 0;
        }

        int16_t treaties_1_count = 0;
        uint8_t treaties_1[12];
        Get_Diplomacy_Demand_Treaty_List(player_num, ambassador_idx, 5, &treaties_1_count, treaties_1);
        if (treaties_1_count > 0) {
            demand_list_flags[0] = 1;
        }

        int16_t treaties_2_count = 0;
        uint8_t treaties_2[12];
        if (MOX::game_type == 0) {
            Get_Diplomacy_Demand_Treaty_List(player_num, ambassador_idx, 3, &treaties_2_count, treaties_2);
            if (treaties_2_count > 0) {
                demand_list_flags[1] = 1;
            }
        }

        int16_t treaties_3_count = 0;
        uint8_t treaties_3[12];
        Get_Diplomacy_Demand_Treaty_List(player_num, ambassador_idx, 2, &treaties_3_count, treaties_3);
        if (treaties_3_count > 0) {
            demand_list_flags[2] = 1;
        }

        demand_list_flags[3] = 1;
        demand_list_flags[4] = 1;

        int p2_tribute_to_p1 = MOX::player[ambassador_idx].tribute_treaty[player_num];
        if (p2_tribute_to_p1 > 0) {
            demand_list_flags[3] = 0;
            demand_list_flags[4] = 0;
        } else if (MOX::player[player_num].tribute_treaty[ambassador_idx] > 0) {
            demand_list_flags[3] = 0;
            demand_list_flags[4] = 0;
        } else if (p2_tribute_to_p1 == 1) {
            demand_list_flags[3] = 0;
        } else if (p2_tribute_to_p1 == 2) {
            demand_list_flags[3] = 0;
            demand_list_flags[4] = 0;
        }

        demand_list_flags[5] = 1;

        int16_t differential_tech_count = 0;
        uint8_t differential_tech_ids[TECH_APP_COUNT];
        int32_t differential_tech_scores[TECH_APP_COUNT];
        npcdiplo::Get_Differential_Tech_List(player_num, ambassador_idx, 0, 0xc343cb00, differential_tech_ids, differential_tech_scores, &differential_tech_count, 1);
        if (differential_tech_count > 0 && noreport == 0) {
            demand_list_flags[6] = 1;
        }

        if (Check_Surrender_System(ambassador_idx, player_num) == 1) {
            demand_list_flags[7] = 1;
        }

        if (SHIPMOVE::Player_Has_Ships_Pestering_Other_Player(ambassador_idx, (uint8_t)player_num) == 1) {
            demand_list_flags[8] = 1;
        }

        demand_list_flags[9] = 1;

        fields::Assign_Auto_Function(Draw_Diplomacy_Synch_Mode, 2);
        fields::Set_Input_Delay(1);

        for (int i = 0; i < 11; ++i) {
            strlcpy(diplomacy_list_choice[i], demand_diplomacy_list[i], sizeof(diplomacy_list_choice[i]));
        }

        fonts::Set_Font_Style(4, &diplomacy_normal_colors);
        fonts::Set_Font_Highlight_Colors(&diplomacy_highlight_colors);
        fonts::Set_Font_Special_Colors(&diplomacy_special_colors);
        fonts::Set_Font_LF(4);
        Setup_Diplomacy_Help(11, demand_help_entry);

        int8_t list_index = 0;
        int16_t selection = fields::Get_List_Field(
            10, 0x76, 0xF5, demand_title, (char*)diplomacy_list_choice, 0x32, &list_index, demand_list_flags, 17, nullptr, 0, nullptr, 0, 0);

        switch (selection) {
            case 0: {
                char treaties_str_list[15][40];
                for (int i = 0; i < treaties_1_count; ++i) {
                    snprintf(treaties_str_list[i], sizeof(treaties_str_list[i]), s__s_0055236c, MOX::player[treaties_1[i]].name);
                }
                char title[40];
                JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x47, title, 40);
                int16_t list_sel = Diplomacy_Generic_List(treaties_1_count, (char*)treaties_str_list, 1, title);
                if (list_sel == -1) break;

                old_response_message = -1;
                uint8_t treaty_val = treaties_1[list_sel];
                int16_t response;
                if (MOX::player[ambassador_idx].objectives == PLAYER_OBJECTIVE_HUMAN) {
                    Get_Human_Demand_Response(player_num, ambassador_idx, 0, treaty_val, &response);
                } else {
                    diplomac::Get_Demand_Response(player_num, ambassador_idx, 0, treaty_val, &response, &response_message);
                }
                diplomac::Adjust_Diplomat_Modifiers(player_num, ambassador_idx);
                if (response == 1) {
                    Diplomacy_Give_In_To_Demand(player_num, ambassador_idx, 0, treaty_val);
                    return;
                }
                break;
            }
            case 1: {
                char treaties_str_list[15][40];
                for (int i = 0; i < treaties_2_count; ++i) {
                    snprintf(treaties_str_list[i], sizeof(treaties_str_list[i]), s__s_0055236c, MOX::player[treaties_2[i]].name);
                }
                char title[40];
                JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x48, title, 40);
                int16_t list_sel = Diplomacy_Generic_List(treaties_2_count, (char*)treaties_str_list, 1, title);
                if (list_sel == -1) break;

                old_response_message = -1;
                uint8_t treaty_val = treaties_2[list_sel];
                int16_t response;
                if (MOX::player[ambassador_idx].objectives == PLAYER_OBJECTIVE_HUMAN) {
                    Get_Human_Demand_Response(player_num, ambassador_idx, 1, treaty_val, &response);
                } else {
                    diplomac::Get_Demand_Response(player_num, ambassador_idx, 1, treaty_val, &response, &response_message);
                }
                diplomac::Adjust_Diplomat_Modifiers(player_num, ambassador_idx);
                if (response == 1) {
                    Diplomacy_Give_In_To_Demand(player_num, ambassador_idx, 1, treaty_val);
                    return;
                }
                break;
            }
            case 2: {
                char treaties_str_list[15][40];
                for (int i = 0; i < treaties_3_count; ++i) {
                    snprintf(treaties_str_list[i], sizeof(treaties_str_list[i]), s__s_0055236c, MOX::player[treaties_3[i]].name);
                }
                char title[42];
                JIM::Get_Text_Message(s_jimtext_lbx_00552360, 0x49, title, sizeof(title));
                int16_t list_sel = Diplomacy_Generic_List(treaties_3_count, (char*)treaties_str_list, 1, title);
                if (list_sel == -1) break;

                old_response_message = -1;
                uint8_t treaty_val = treaties_3[list_sel];
                int16_t response;
                if (MOX::player[ambassador_idx].objectives == PLAYER_OBJECTIVE_HUMAN) {
                    Get_Human_Demand_Response(player_num, ambassador_idx, 2, treaty_val, &response);
                } else {
                    diplomac::Get_Demand_Response(player_num, ambassador_idx, 2, treaty_val, &response, &response_message);
                }
                diplomac::Adjust_Diplomat_Modifiers(player_num, ambassador_idx);
                if (response == 1) {
                    Diplomacy_Give_In_To_Demand(player_num, ambassador_idx, 2, treaty_val);
                    return;
                }
                break;
            }
            case 3: {
                old_response_message = -1;
                int16_t response;
                if (MOX::player[ambassador_idx].objectives == PLAYER_OBJECTIVE_HUMAN) {
                    Get_Human_Demand_Response(player_num, ambassador_idx, 3, 0, &response);
                } else {
                    diplomac::Get_Demand_Response(player_num, ambassador_idx, 3, 0, &response, &response_message);
                }
                diplomac::Adjust_Diplomat_Modifiers(player_num, ambassador_idx);
                if (response == 1) {
                    Diplomacy_Give_In_To_Demand(player_num, ambassador_idx, 3, 0);
                    return;
                }
                break;
            }
            case 4: {
                old_response_message = -1;
                int16_t response;
                if (MOX::player[ambassador_idx].objectives == PLAYER_OBJECTIVE_HUMAN) {
                    Get_Human_Demand_Response(player_num, ambassador_idx, 4, 0, &response);
                } else {
                    diplomac::Get_Demand_Response(player_num, ambassador_idx, 4, 0, &response, &response_message);
                }
                diplomac::Adjust_Diplomat_Modifiers(player_num, ambassador_idx);
                if (response == 1) {
                    Diplomacy_Give_In_To_Demand(player_num, ambassador_idx, 4, 0);
                    return;
                }
                break;
            }
            case 5: {
                old_response_message = -1;
                int16_t response;
                if (MOX::player[ambassador_idx].objectives == PLAYER_OBJECTIVE_HUMAN) {
                    Get_Human_Demand_Response(player_num, ambassador_idx, 5, 0, &response);
                } else {
                    diplomac::Get_Demand_Response(player_num, ambassador_idx, 5, 0, &response, &response_message);
                }
                diplomac::Adjust_Diplomat_Modifiers(player_num, ambassador_idx);
                return;
            }
            case 6: {
                int16_t selection_tech = Diplomacy_Generic_Exchange_Tech_List(player_num, ambassador_idx, demand_title, differential_tech_ids, differential_tech_count, 0);
                if (selection_tech == -1) break;

                old_response_message = -1;
                uint8_t tech_val = differential_tech_ids[selection_tech];
                int16_t response;
                if (MOX::player[ambassador_idx].objectives == PLAYER_OBJECTIVE_HUMAN) {
                    Get_Human_Demand_Response(player_num, ambassador_idx, 6, tech_val, &response);
                } else {
                    diplomac::Get_Demand_Response(player_num, ambassador_idx, 6, tech_val, &response, &response_message);
                }
                diplomac::Adjust_Diplomat_Modifiers(player_num, ambassador_idx);
                if (response == 1) {
                    Diplomacy_Give_In_To_Demand(player_num, ambassador_idx, 6, tech_val);
                    return;
                }
                break;
            }
            case 7: {
                int16_t selection_sys = Diplomacy_System_Display(ambassador_idx, player_num, 1);
                if (selection_sys == -1) break;

                old_response_message = -1;
                int16_t response;
                if (MOX::player[ambassador_idx].objectives == PLAYER_OBJECTIVE_HUMAN) {
                    Get_Human_Demand_Response(player_num, ambassador_idx, 7, selection_sys, &response);
                } else {
                    diplomac::Get_Demand_Response(player_num, ambassador_idx, 7, selection_sys, &response, &response_message);
                }
                diplomac::Adjust_Diplomat_Modifiers(player_num, ambassador_idx);
                if (response == 1) {
                    Diplomacy_Give_In_To_Demand(player_num, ambassador_idx, 7, selection_sys);
                    return;
                }
                break;
            }
            case 8: {
                old_response_message = -1;
                int16_t response;
                if (MOX::player[ambassador_idx].objectives == PLAYER_OBJECTIVE_HUMAN) {
                    Get_Human_Demand_Response(player_num, ambassador_idx, 8, 0, &response);
                } else {
                    diplomac::Get_Demand_Response(player_num, ambassador_idx, 8, 0, &response, &response_message);
                }
                diplomac::Adjust_Diplomat_Modifiers(player_num, ambassador_idx);
                if (response == 1) {
                    Diplomacy_Give_In_To_Demand(player_num, ambassador_idx, 8, 0);
                }
                return;
            }
            default:
                break;
        }
    }

    void __cdecl Start_Net_Treaty() {
        switch (net_diplomacy_treaty_type) {
            case 1:
                diplomac::Start_Treaty(
                    static_cast<int16_t>(talker_ambassador),
                    static_cast<int16_t>(listen_ambassador),
                    1);
                break;
            case 2:
                diplomac::Start_Treaty(
                    static_cast<int16_t>(talker_ambassador),
                    static_cast<int16_t>(listen_ambassador),
                    2);
                break;
            case 3:
                diplomac::Declare_Peace(
                    static_cast<int16_t>(talker_ambassador),
                    static_cast<int16_t>(listen_ambassador));
                break;
            case 7:
                diplomac::Start_Trade_Treaty(
                    static_cast<int16_t>(talker_ambassador),
                    static_cast<int16_t>(listen_ambassador));
                break;
            case 8:
                diplomac::Start_Research_Treaty(
                    static_cast<int16_t>(talker_ambassador),
                    static_cast<int16_t>(listen_ambassador));
                break;
            default:
                break;
        }

        Get_Human_Form_Treaty_Message(
            static_cast<int16_t>(talker_ambassador),
            static_cast<int16_t>(listen_ambassador),
            net_diplomacy_treaty_type,
            3);
    }

    void __cdecl Get_Net_Diplomacy_Choices() {
        int8_t selected;
        int16_t tech_count;
        int16_t done_flag;
        uint8_t options[11];
        char title_buf[40];
        uint8_t tech_list[TECH_APP_COUNT];
        char msg_buf[400];
        int32_t costs[TECH_APP_COUNT];

        chat_message[0] = s__00551100[0];
        chat_base_x = 10;
        chat_base_y = 0x8D;
        chat_width = 300;
        old_response_message = 0;
        chat_mode_flag = 0;
        response_message = 0;
        diplomacy_message[0] = s__00551100[0];
        chat_height = 0x32;

        if (MOX::PLAYER_NUM != talker_ambassador) {
            Get_Listen_Mode_Message(talker_ambassador);
            Send_Diplomacy_Handshake(talker_ambassador, listen_ambassador);
            do {
                Diplomacy_Synch_Mode(listen_ambassador, talker_ambassador);
            } while (synch_up_established_flag == 0);
        } else {
            Get_Listen_Mode_Message(listen_ambassador);
            Send_Diplomacy_Handshake(listen_ambassador, talker_ambassador);
            do {
                Diplomacy_Synch_Mode(talker_ambassador, listen_ambassador);
            } while (synch_up_established_flag == 0);
        }

        done_flag = 0;
        old_response_message = 0;
        response_message = 0;
        diplomacy_message[0] = s__00551100[0];

        if (MOX::PLAYER_NUM != talker_ambassador) {
            Get_Listen_Mode_Message(talker_ambassador);
        }

        fields::Set_Input_Delay(1);

        while (true) {
            if (net_diplomacy_esc_pressed != 0) {
                return;
            }

            fields::Assign_Auto_Function(Draw_Diplomacy_Synch_Mode, 2);

            if (MOX::PLAYER_NUM != talker_ambassador) {
                if (chat_mode_flag == 0) {
                    fields::Clear_Fields();
                }

                if (net_diplomacy_esc_pressed == 1) {
                    done_flag = 1;
                }

                if (old_response_message == response_message || response_message == 0x80) {
                    Diplomacy_Listen_Mode(talker_ambassador, listen_ambassador);
                } else {
                    int32_t resp = response_message;
                    chat_mode_flag = 0;

                    if (resp <= DIP_NET_MSG_TREATY_PROPOSAL) {
                        if (resp == DIP_NET_MSG_TREATY_PROPOSAL) {
                            response_message = net_diplomacy_message.arg2;
                            net_diplomacy_treaty_type = net_diplomacy_message.arg1;
                            int16_t choice = Get_Human_Treaty_Choice(talker_ambassador, listen_ambassador, net_diplomacy_message.arg1);
                            old_response_message = response_message;
                            net_diplomacy_message.msg_type = response_message;
                            net_diplomacy_message.arg2 = choice;
                            net_diplomacy_message.sender_player_id = static_cast<uint8_t>(talker_ambassador);
                            net_diplomacy_message.target_player_id = static_cast<uint8_t>(listen_ambassador);
                            NETMOX::Mox_Send_Message(MOX::player[talker_ambassador].network_player_id, DIP_NET_PACKET_MESSAGE, reinterpret_cast<uint8_t*>(&net_diplomacy_message), sizeof(net_diplomacy_message));
                            RUSS::Mox_Update();
                            response_message = 0;
                            old_response_message = 0;

                            if (choice == 0 || choice == 1) {
                                Get_Listen_Mode_Message(talker_ambassador);
                            } else if (choice == 3) {
                                Start_Net_Treaty();
                            }
                        } else if (resp == 0x82) {
                            if (net_diplomacy_esc_pressed == 0) {
                                Diplomacy_Display_Response();
                            }
                            done_flag = 1;
                        }
                    } else if (resp <= DIP_NET_MSG_BETTER_OFFER_BC) {
                        switch (resp) {
                            case DIP_NET_MSG_BREAK_TREATY: {
                                response_message = net_diplomacy_message.arg2;
                                Get_Human_Break_Treaty_Message(talker_ambassador, net_diplomacy_message.arg1);
                                switch (net_diplomacy_message.arg1) {
                                    case 1:
                                    case 2:
                                        diplomac::Break_Treaties(talker_ambassador, listen_ambassador);
                                        break;
                                    case 7:
                                        diplomac::Break_Trade(talker_ambassador, listen_ambassador);
                                        break;
                                    case 8:
                                        diplomac::Break_Research(talker_ambassador, listen_ambassador);
                                        break;
                                    case 9:
                                    case 10:
                                        diplomac::Break_Tribute(talker_ambassador, listen_ambassador);
                                        break;
                                }
                                response_message = 0;
                                old_response_message = 0;
                                break;
                            }
                            case DIP_NET_MSG_DECLARE_WAR: {
                                Get_Human_Declare_War_Message(talker_ambassador);
                                response_message = 0;
                                old_response_message = 0;
                                diplomac::Declare_War(talker_ambassador, listen_ambassador);
                                break;
                            }
                            case DIP_NET_MSG_SURRENDER: {
                                response_message = 0;
                                old_response_message = 0;
                                Get_Human_Surrender_Message(talker_ambassador);
                                Diplomacy_Display_Response();
                                done_flag = 1;
                                break;
                            }
                            case DIP_NET_MSG_GIFT_RESPONSE: {
                                response_message = 0;
                                old_response_message = 0;
                                Get_Human_Offer_Gift_Message(talker_ambassador, net_diplomacy_message.arg1, net_diplomacy_message.arg2);
                                response_message = 0;
                                old_response_message = 0;
                                switch (net_diplomacy_message.arg1) {
                                    case 1:
                                        MOX::player[talker_ambassador].bc -= net_diplomacy_message.arg2;
                                        MOX::player[listen_ambassador].bc += net_diplomacy_message.arg2;
                                        break;
                                    case 2:
                                        diplomac::Start_Tribute_Treaty(talker_ambassador, listen_ambassador, net_diplomacy_message.arg2);
                                        break;
                                    case 3:
                                        COLCALC::Player_Gets_Tech_App(&MOX::player[listen_ambassador], net_diplomacy_message.arg2);
                                        break;
                                    case 4:
                                        if (COLCALC::May_Surrender_Star(net_diplomacy_message.arg2, talker_ambassador, listen_ambassador) == 1) {
                                            COLCALC::Surrender_Star(net_diplomacy_message.arg2, talker_ambassador, listen_ambassador);
                                        }
                                        break;
                                }
                                break;
                            }
                            case DIP_NET_MSG_DEMAND: {
                                Get_Human_Demand_Message(talker_ambassador, listen_ambassador, net_diplomacy_message.arg1, net_diplomacy_message.arg2);
                                int16_t demand_choice = Get_Human_Demand_Choice(listen_ambassador, talker_ambassador, net_diplomacy_message.arg1, net_diplomacy_message.arg2);
                                response_message = 0xA8;
                                if (demand_choice == 1) {
                                    Diplomacy_Give_In_To_Demand(talker_ambassador, listen_ambassador, net_diplomacy_message.arg1, net_diplomacy_message.arg2);
                                    response_message = 0xA9;
                                }
                                old_response_message = response_message;
                                net_diplomacy_message.msg_type = response_message;
                                net_diplomacy_message.arg2 = demand_choice;
                                net_diplomacy_message.sender_player_id = static_cast<uint8_t>(talker_ambassador);
                                net_diplomacy_message.target_player_id = static_cast<uint8_t>(listen_ambassador);
                                NETMOX::Mox_Send_Message(MOX::player[talker_ambassador].network_player_id, DIP_NET_PACKET_MESSAGE, reinterpret_cast<uint8_t*>(&net_diplomacy_message), sizeof(net_diplomacy_message));
                                RUSS::Mox_Update();
                                Get_Listen_Mode_Message(talker_ambassador);
                                break;
                            }
                            case DIP_NET_MSG_BETTER_OFFER_SYSTEM: {
                                response_message = old_response_message;
                                int16_t list_choice = Diplomacy_Generic_System_List(net_diplomacy_message.arg1, talker_ambassador, listen_ambassador, 0);
                                if (list_choice == -1) {
                                    Get_Listen_Mode_Message(talker_ambassador);
                                    list_choice = 0;
                                } else {
                                    if (COLCALC::May_Surrender_Star(net_diplomacy_message.arg1, talker_ambassador, listen_ambassador) == 1) {
                                        COLCALC::Surrender_Star(net_diplomacy_message.arg1, talker_ambassador, listen_ambassador);
                                    }
                                    Start_Net_Treaty();
                                    list_choice = 1;
                                }
                                net_diplomacy_message.msg_type = response_message;
                                net_diplomacy_message.arg2 = list_choice;
                                net_diplomacy_message.sender_player_id = static_cast<uint8_t>(talker_ambassador);
                                net_diplomacy_message.target_player_id = static_cast<uint8_t>(listen_ambassador);
                                NETMOX::Mox_Send_Message(MOX::player[talker_ambassador].network_player_id, DIP_NET_PACKET_MESSAGE, reinterpret_cast<uint8_t*>(&net_diplomacy_message), sizeof(net_diplomacy_message));
                                RUSS::Mox_Update();
                                response_message = 0;
                                old_response_message = 0;
                                break;
                            }
                            case DIP_NET_MSG_BETTER_OFFER_TECH: {
                                response_message = old_response_message;
                                char* tech_name = TECH::Technology_Applications_Name(net_diplomacy_message.arg1, talker_ambassador);
                                snprintf(msg_buf, sizeof(msg_buf), "  %s", tech_name);
                                JIM::Get_Text_Message(s_jimtext2_lbx, 9, title_buf, 40);
                                int16_t list_choice = Diplomacy_Generic_List(1, msg_buf, 1, title_buf);
                                if (list_choice == -1) {
                                    Get_Listen_Mode_Message(talker_ambassador);
                                    list_choice = 0;
                                } else {
                                    COLCALC::Player_Gets_Tech_App(&MOX::player[listen_ambassador], net_diplomacy_message.arg1);
                                    Start_Net_Treaty();
                                    list_choice = 1;
                                }
                                net_diplomacy_message.msg_type = response_message;
                                net_diplomacy_message.arg2 = list_choice;
                                net_diplomacy_message.sender_player_id = static_cast<uint8_t>(talker_ambassador);
                                net_diplomacy_message.target_player_id = static_cast<uint8_t>(listen_ambassador);
                                NETMOX::Mox_Send_Message(MOX::player[talker_ambassador].network_player_id, DIP_NET_PACKET_MESSAGE, reinterpret_cast<uint8_t*>(&net_diplomacy_message), sizeof(net_diplomacy_message));
                                RUSS::Mox_Update();
                                response_message = 0;
                                old_response_message = 0;
                                break;
                            }
                            case DIP_NET_MSG_TECH_EXCHANGE_PROPOSE: {
                                response_message = 0;
                                old_response_message = 0;
                                Get_Human_Tech_Exchange_First_Message(talker_ambassador, listen_ambassador, net_diplomacy_message.arg1);
                                net_diplomacy_tech_offered = net_diplomacy_message.arg1;
                                JIM::Get_Text_Message(s_jimtext2_lbx, 4, title_buf, 40);
                                npcdiplo::Get_Differential_Tech_List(listen_ambassador, talker_ambassador, 0, 0xC343CB00, (uint8_t*)tech_list, costs, &tech_count, 1);
                                int16_t exch_choice = Diplomacy_Generic_Exchange_Tech_List(listen_ambassador, talker_ambassador, title_buf, tech_list, tech_count, 0);
                                if (exch_choice >= 0) {
                                    exch_choice = tech_list[exch_choice];
                                }
                                net_diplomacy_message.msg_type = response_message;
                                net_diplomacy_message.arg2 = exch_choice;
                                net_diplomacy_message.sender_player_id = static_cast<uint8_t>(talker_ambassador);
                                net_diplomacy_message.target_player_id = static_cast<uint8_t>(listen_ambassador);
                                NETMOX::Mox_Send_Message(MOX::player[talker_ambassador].network_player_id, DIP_NET_PACKET_MESSAGE, reinterpret_cast<uint8_t*>(&net_diplomacy_message), sizeof(net_diplomacy_message));
                                RUSS::Mox_Update();
                                Get_Listen_Mode_Message(talker_ambassador);
                                break;
                            }
                            case DIP_NET_MSG_TECH_EXCHANGE_DECISION: {
                                response_message = 0;
                                old_response_message = 0;
                                if (net_diplomacy_message.arg1 == -1) {
                                    Get_Human_Tech_Exchange_Decline_Message();
                                } else {
                                    COLCALC::Player_Gets_Tech_App(&MOX::player[talker_ambassador], net_diplomacy_tech_offered);
                                    COLCALC::Player_Gets_Tech_App(&MOX::player[listen_ambassador], net_diplomacy_message.arg2);
                                    Get_Human_Tech_Exchange_Accept_Message();
                                }
                                response_message = 0;
                                old_response_message = 0;
                                break;
                            }
                            case DIP_NET_MSG_SWITCH_TALKER: {
                                response_message = 0;
                                old_response_message = 0;
                                snprintf(diplomacy_message, sizeof(diplomacy_message), "%s", s__00551100);
                                break;
                            }
                            case DIP_NET_MSG_BETTER_OFFER_BC: {
                                response_message = old_response_message;
                                snprintf(msg_buf, sizeof(msg_buf), s__d_BC_00552384, net_diplomacy_message.arg1);
                                JIM::Get_Text_Message(s_jimtext2_lbx, 9, title_buf, 40);
                                int16_t list_choice = Diplomacy_Generic_List(1, msg_buf, 1, title_buf);
                                if (list_choice == -1) {
                                    Get_Listen_Mode_Message(talker_ambassador);
                                    list_choice = 0;
                                } else {
                                    MOX::player[talker_ambassador].bc -= net_diplomacy_message.arg1;
                                    MOX::player[listen_ambassador].bc += net_diplomacy_message.arg1;
                                    Start_Net_Treaty();
                                    list_choice = 1;
                                }
                                net_diplomacy_message.msg_type = response_message;
                                net_diplomacy_message.arg2 = list_choice;
                                net_diplomacy_message.sender_player_id = static_cast<uint8_t>(talker_ambassador);
                                net_diplomacy_message.target_player_id = static_cast<uint8_t>(listen_ambassador);
                                NETMOX::Mox_Send_Message(MOX::player[talker_ambassador].network_player_id, DIP_NET_PACKET_MESSAGE, reinterpret_cast<uint8_t*>(&net_diplomacy_message), sizeof(net_diplomacy_message));
                                RUSS::Mox_Update();
                                response_message = 0;
                                old_response_message = 0;
                                break;
                            }
                        }
                    }
                }
            } else {
                chat_mode_flag = 0;

                if (MOX::player[talker_ambassador].traits[TRAIT_REPULSIVE] == 1 || MOX::player[listen_ambassador].traits[TRAIT_REPULSIVE] == 1) {
                    Get_Main_Repulsive_Diplomacy_Choices(talker_ambassador, listen_ambassador, 1);
                    fields::Clear_Fields();
                } else {
                    for (int i = 0; i < 11; ++i) {
                        options[i] = 1;
                    }
                    Diplomacy_Set_Main_Options(talker_ambassador, listen_ambassador, options);

                    for (int i = 0; i < 11; ++i) {
                        strlcpy(diplomacy_list_choice[i], main_net_diplomacy_list[i], sizeof(diplomacy_list_choice[i]));
                    }

                    fonts::Set_Font_Style(4, &diplomacy_normal_colors);
                    fonts::Set_Font_Highlight_Colors(&diplomacy_highlight_colors);
                    fonts::Set_Font_Special_Colors(&diplomacy_special_colors);
                    fonts::Set_Font_LF(4);
                    Setup_Diplomacy_Help(11, main_net_diplomacy_help_entry);

                    selected = 0;
                    int16_t main_choice = fields::Get_List_Field(
                        10, 118, 245, main_diplomacy_title, reinterpret_cast<char*>(diplomacy_list_choice), 50, &selected, options, 17, nullptr, 0, nullptr, 0, 0);

                    switch (main_choice) {
                        case 0:
                            diplomacy_message[0] = s__00551100[0];
                            Diplomacy_Propose_Treaty(talker_ambassador, listen_ambassador);
                            response_message = 0;
                            old_response_message = 0;
                            chat_mode_flag = 0;
                            fields::Clear_Fields();
                            break;
                        case 1:
                            diplomacy_message[0] = s__00551100[0];
                            Diplomacy_Break_Treaty(talker_ambassador, listen_ambassador);
                            old_response_message = 0;
                            response_message = 0;
                            diplomacy_message[0] = s__00551100[0];
                            fields::Clear_Fields();
                            break;
                        case 2:
                            diplomacy_message[0] = s__00551100[0];
                            Diplomacy_Demand(talker_ambassador, listen_ambassador);
                            fields::Clear_Fields();
                            break;
                        case 3:
                            diplomacy_message[0] = s__00551100[0];
                            Diplomacy_Offer_Gift(talker_ambassador, listen_ambassador);
                            fields::Clear_Fields();
                            break;
                        case 4:
                            diplomacy_message[0] = s__00551100[0];
                            Diplomacy_Exchange_Technology(talker_ambassador, listen_ambassador);
                            fields::Clear_Fields();
                            break;
                        case 5:
                            if (Diplomacy_Declare_War_Resolution(talker_ambassador, listen_ambassador) == 1) {
                                Diplomacy_Display_Response();
                            }
                            fields::Clear_Fields();
                            break;
                        case 6:
                            if (Diplomacy_Surrender(talker_ambassador, listen_ambassador) == 1) {
                                done_flag = 1;
                            }
                            fields::Clear_Fields();
                            break;
                        case 7:
                            Diplomacy_Main_Net_Chat(listen_ambassador, talker_ambassador);
                            if (net_diplomacy_esc_pressed == 1) {
                                done_flag = 1;
                            }
                            fields::Clear_Fields();
                            break;
                        case 8:
                            Diplomacy_Net_Switcheroo();
                            Get_Listen_Mode_Message(talker_ambassador);
                            fields::Clear_Fields();
                            break;
                        case -1:
                        case 9:
                            net_diplomacy_message.sender_player_id = static_cast<uint8_t>(talker_ambassador);
                            net_diplomacy_message.target_player_id = static_cast<uint8_t>(listen_ambassador);
                            net_diplomacy_message.arg2 = -1;
                            net_diplomacy_message.msg_type = 0x82;
                            done_flag = 1;
                            NETMOX::Mox_Send_Message(MOX::player[listen_ambassador].network_player_id, DIP_NET_PACKET_MESSAGE, reinterpret_cast<uint8_t*>(&net_diplomacy_message), sizeof(net_diplomacy_message));
                            RUSS::Mox_Update();
                            fields::Clear_Fields();
                            break;
                        default:
                            fields::Clear_Fields();
                            break;
                    }
                }
            }

            if (done_flag != 0) {
                return;
            }
        }
    }

    void __cdecl Get_Human_Tech_Exchange_Message(int16_t player_2, int16_t player_1, int16_t tech_1, int16_t tech_2) {
        JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 0x2F, diplomacy_message, 0xFA);
        JIM::Decode_Text_Name_Tech_And_Other_Tech(diplomacy_message, player_2, player_1, tech_2, tech_1, 0xFA);
    }

    void __cdecl Get_Human_Tech_Exchange_First_Message(int16_t player_1, int16_t player_2, int16_t tech_id) {
        JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 0x30, diplomacy_message, 0xFA);
        JIM::Decode_Text_Name_And_Other_Tech(diplomacy_message, player_1, player_2, tech_id, 0xFA);
    }

    void __cdecl Get_Human_Tech_Exchange_Accept_Message() {
        JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 0x31, diplomacy_message, 0xFA);
    }

    void __cdecl Get_Human_Tech_Exchange_Decline_Message() {
        JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 0x32, diplomacy_message, 0xFA);
    }

    /**
     * @brief Gets the human player's response to a diplomacy demand.
     *
     * @param player_id The player making the demand.
     * @param target_player_id The player receiving the demand.
     * @param demand_type The demand category being resolved.
     * @param demand_arg Extra demand data such as a tech, treaty target, or system.
     * @param out_choice Output for the selected human response.
     */
    void __cdecl Get_Human_Demand_Response(int16_t player_id, int16_t target_player_id, int16_t demand_type, int16_t demand_arg, int16_t* out_choice) {
        int16_t choice_1_response = 0xA9;
        int16_t choice_else_response = 0xA8;

        if (MOX::game_type == 1 /* SINGLE_PLAYER */) {
            old_response_message = response_message;
            Get_Human_Demand_Message(player_id, target_player_id, demand_type, demand_arg);
            Diplomacy_Load_New_Ambassador(player_id);

            // The binary keeps the demander's ambassador loaded for both the prompt and
            // follow-up response handling, and uses the same player ordering here.
            int16_t choice = Get_Human_Demand_Choice(player_id, target_player_id, demand_type, demand_arg);
            *out_choice = choice;

            old_response_message = -1;
            response_message = choice_1_response;

            if (*out_choice != 1) {
                response_message = choice_else_response;
            }

            Diplomacy_Load_New_Ambassador(player_id);
        } else {
            net_diplomacy_message.sender_player_id = (uint8_t)player_id;
            net_diplomacy_message.target_player_id = (uint8_t)target_player_id;
            net_diplomacy_message.arg1 = demand_type;
            net_diplomacy_message.msg_type = DIP_NET_MSG_DEMAND;
            net_diplomacy_message.arg2 = demand_arg;

            NETMOX::Mox_Send_Message(
                MOX::player[target_player_id].network_player_id,
                DIP_NET_PACKET_MESSAGE,
                &net_diplomacy_message,
                8);

            RUSS::Mox_Update();
            Get_Listen_Mode_Message(target_player_id);
            net_diplomacy_message.arg2 = -1;

            do {
                Diplomacy_Listen_Mode(target_player_id, (uint8_t)player_id);
            } while (net_diplomacy_message.arg2 == -1 && net_diplomacy_esc_pressed == 0);

            if (net_diplomacy_esc_pressed == 1) {
                net_diplomacy_message.arg2 = 0;
            }

            *out_choice = net_diplomacy_message.arg2;
        }
    }

    const char* __cdecl An(char* text) {
        if (MOX::settings.language != 0) {
            return s__00551100;
        }

        char first_char = *text;

        // Check if the first character is a vowel (case-insensitive)
        if (first_char == 'A' || first_char == 'a' ||
            first_char == 'E' || first_char == 'e' ||
            first_char == 'I' || first_char == 'i' ||
            first_char == 'O' || first_char == 'o' ||
            first_char == 'U' || first_char == 'u') {
            return "an";
        }

        return "a";
    }

    void __cdecl Diplomacy_Give_In_To_Demand(int16_t asking_player_idx, int16_t giving_player_idx, int16_t demand_type, int16_t demand_data) {
        switch (demand_type) {
            case 0:
                MOX::player[giving_player_idx].delayed_diplomacy_orders[demand_data] = 0;
                break;

            case 1:
                MOX::player[giving_player_idx].delayed_diplomacy_orders[demand_data] = 1;
                break;

            case 2:
                MOX::player[giving_player_idx].delayed_diplomacy_orders[demand_data] = 2;
                break;

            case 3:
                diplomac::Start_Tribute_Treaty(giving_player_idx, asking_player_idx, 1);
                break;

            case 4:
                diplomac::Start_Tribute_Treaty(giving_player_idx, asking_player_idx, 2);
                break;

            case 6:
                COLCALC::Player_Gets_Tech_App(&MOX::player[asking_player_idx], demand_data);
                break;

            case 7:
                if (COLCALC::May_Surrender_Star(demand_data, giving_player_idx, asking_player_idx)) {
                    COLCALC::Surrender_Star(demand_data, giving_player_idx, asking_player_idx);
                }
                break;

            case 8: {
                int32_t r1 = game_random::Random(50);
                int32_t r2 = game_random::Random(50);
                MOX::player[giving_player_idx].stop_blockading_duration[asking_player_idx] = (int8_t)r1 + (int8_t)r2 + 10;
            } break;

            case 5:
            default:
                break;
        }
    }

    void __cdecl Diplomacy_Synch_Mode(int16_t p1, int16_t p2) {
        old_response_message = 0;
        response_message = 0;

        fields::Assign_Auto_Function(Draw_Diplomacy_Listen_Mode, 2);
        timer::Mark_Time();
        fields::Clear_Fields();

        Draw_Diplomacy_Synch_Mode();
        RUSS::Mox_Sync_Update();

        timer::Release_Time(2);
        fields::Assign_Auto_Function(Draw_Diplomacy_Synch_Mode, 2);
    }

    void __cdecl Get_Human_Demand_Message(int16_t player_idx, int16_t unused_target_idx, int16_t demand_type, int16_t item_idx) {
        switch (demand_type) {
            case 0:
                JIM::Get_Text_Message(s_jimtext2_lbx, 0x13, diplomacy_message, 0xfa);
                JIM::Decode_Text_Name_And_Name(diplomacy_message, player_idx, item_idx, 0xfa);
                break;
            case 1:
                JIM::Get_Text_Message(s_jimtext2_lbx, 0x14, diplomacy_message, 0xfa);
                JIM::Decode_Text_Name_And_Name(diplomacy_message, player_idx, item_idx, 0xfa);
                break;
            case 2:
                JIM::Get_Text_Message(s_jimtext2_lbx, 0x15, diplomacy_message, 0xfa);
                JIM::Decode_Text_Name_And_Name(diplomacy_message, player_idx, item_idx, 0xfa);
                break;
            case 3:
                JIM::Get_Text_Message(s_jimtext2_lbx, 0x16, diplomacy_message, 0xfa);
                JIM::Decode_Text_Name_And_Stat(diplomacy_message, player_idx, 5, 0xfa);
                break;
            case 4:
                JIM::Get_Text_Message(s_jimtext2_lbx, 0x16, diplomacy_message, 0xfa);
                JIM::Decode_Text_Name_And_Stat(diplomacy_message, player_idx, 10, 0xfa);
                break;
            case 5:
                JIM::Get_Text_Message(s_jimtext2_lbx, 0x17, diplomacy_message, 0xfa);
                JIM::Decode_Text_Race(diplomacy_message, player_idx, 0xfa);
                break;
            case 6:
                JIM::Get_Text_Message(s_jimtext2_lbx, 0x18, diplomacy_message, 0xfa);
                JIM::Decode_Text_Name_And_Tech(diplomacy_message, player_idx, item_idx, 0xfa);
                break;
            case 7:
                JIM::Get_Text_Message(s_jimtext2_lbx, 0x19, diplomacy_message, 0xfa);
                JIM::Decode_Text_Name_And_System(diplomacy_message, player_idx, item_idx, 0xfa);
                break;
            case 8:
                JIM::Get_Text_Message(s_jimtext2_lbx, 0x1a, diplomacy_message, 0xfa);
                JIM::Decode_Text_Race(diplomacy_message, player_idx, 0xfa);
                break;
            default:
                break;
        }
    }

    /**
     * @brief Displays a list for the human player to select a demand (e.g., star system or tech).
     * 
     * @param player_idx The index of the player making the demand.
     * @param other_player_idx The index of the player being demanded from.
     * @param demand_type The type of demand being made (e.g., 7 for a Star System).
     * @param demand_item_id An additional identifier used for system selection.
     * @return int16_t 1 if a choice was made (or for system demands), 0 if cancelled.
     */
    int16_t __cdecl Get_Human_Demand_Choice(int16_t player_idx, int16_t other_player_idx, int16_t demand_type, int16_t demand_item_id) {
        char cancel_btn_text[40];
        char demand_text[80];
        int16_t result = 0;

        if (demand_type == 7) {
            if (DIP_SCRN::Diplomacy_Generic_System_List(demand_item_id, player_idx, other_player_idx, 1) != -1) {
                result = 1;
            }
        } else {
            JIM::Get_Text_Message(DIP_SCRN::s_jimtext_lbx, 79 /* MSG_DEMAND_HEADER */, demand_text, 40);
            JIM::Get_Text_Message(DIP_SCRN::s_jimtext_lbx, 80 /* MSG_DEMAND_SUBHEADER */, &demand_text[40], 40);

            fields::Deactivate_Help_List();

            JIM::Get_Text_Message(DIP_SCRN::s_jimtext2_lbx, 0 /* MSG_CANCEL */, cancel_btn_text, 40);

            if (DIP_SCRN::Diplomacy_Generic_List(2, demand_text, 0, cancel_btn_text) == 0) {
                result = 1;
            }
        }

        return result;
    }

    int16_t __cdecl Diplomacy_Generic_System_List(int16_t star_idx, int16_t giver_player_idx, int16_t receiver_player_idx, int16_t mode_flag) {
        s_animation_header* saved_bg_seg;
        int16_t result;
        int8_t current_selection;
        uint8_t enabled_flags[10];
        char list_title[40];

        COLONY::using_colony_screen_palette = 1;
        MOX::star_bg_index = 0;
        saved_bg_seg = MOX::star_bg_seg;
        MOX::current_screen = 6;
        MOX::galaxy_map_scanned_star = star_idx;
        diplomacy_scan_input_flag = star_idx;
        MOX::star_bg_seg = diplomacy_system_star_background_seg;
        diplomacy_system_giver = (uint8_t)giver_player_idx;
        diplomacy_system_receiver = (uint8_t)receiver_player_idx;

        Diplomacy_System_Scan(star_idx, giver_player_idx, &diplomacy_system_homeworld_flag, &diplomacy_system_planet_count, diplomacy_system_planet_list);

        diplomacy_system_population = COLCALC::Get_System_Population(giver_player_idx, star_idx);

        JIM::Get_Text_Message(s_jimtext2_lbx, 0x38 /* 56 */, system_million_colonists_buffer, 0x1E /* 30 */);
        JIM::Decode_Text_Stat(system_million_colonists_buffer, diplomacy_system_population, 0x1E);

        JIM::Get_Text_Message(s_jimtext2_lbx, 0x3A /* 58 */, system_x_star_system_buffer, 0x23 /* 35 */);
        JIM::Decode_Text_System(system_x_star_system_buffer, star_idx, 0x23);

        memset(enabled_flags, 1, sizeof(enabled_flags));

        JIM::Get_Text_Message(s_jimtext_lbx, 0x4B /* 75 */, list_title, 0x28 /* 40 */);

        if (mode_flag == 1) {
            Get_Human_Demand_Message(receiver_player_idx, giver_player_idx, 7, star_idx);
        } else {
            JIM::Get_Text_Message(s_jimtext_lbx, 0x4E /* 78 */, diplomacy_message, 0xFA /* 250 */);
        }

        JIM::Get_Text_Message(s_jimtext_lbx, 0x4F /* 79 */, diplomacy_list_choice[0], 0x32 /* 50 */);
        JIM::Get_Text_Message(s_jimtext_lbx, 0x50 /* 80 */, diplomacy_list_choice[1], 0x32);
        diplomacy_list_choice[2][0] = s_empty[0];

        fields::Clear_Fields();
        fields::Assign_Auto_Function(Diplomacy_Generic_System_List_Draw, 2);
        fields::Set_Input_Delay(1);

        fonts::Set_Font_Style(4, &diplomacy_normal_colors);
        fonts::Set_Font_Highlight_Colors(&diplomacy_highlight_colors);
        fonts::Set_Font_Special_Colors(&diplomacy_special_colors);
        fonts::Set_Font_LF(4);

        current_selection = 0;
        result = fields::Get_List_Field(10, 102, 245, list_title, (const char*)diplomacy_list_choice, 50, &current_selection, enabled_flags, 17, 0, 0, nullptr, 0, 0);

        MOX::star_bg_seg = saved_bg_seg;
        COLONY::using_colony_screen_palette = 0;

        if (result == -1 || result == 1) {
            result = -1;
        }

        return result;
    }

    /**
     * @brief Draws the selected system summary and colony list for the diplomacy UI.
     *
     * @param star_idx The scanned star-system index.
     * @param selected_planet_idx The highlighted planet index, or -1 when none is highlighted.
     */
    void __cdecl Draw_Diplomacy_System_Text(int16_t star_idx, int16_t selected_planet_idx) {
        char text_buffer[80];
        bool use_special_colors = false;

        if (selected_planet_idx == -1) {
            fonts::Set_Font_Style_Shadow_Down(4, &DIP_SCRN::diplomacy_normal_colors);
        } else {
            use_special_colors = true;
            fonts::Set_Font_Style_Shadow_Down(4, &DIP_SCRN::diplomacy_special_colors);
        }

        strlcpy(text_buffer, DIP_SCRN::system_x_star_system_buffer, sizeof(text_buffer));

        if (!COLCALC::May_Surrender_Star(star_idx, DIP_SCRN::diplomacy_system_giver, DIP_SCRN::diplomacy_system_receiver)) {
            fonts::Set_Font_Style_Shadow_Down(4, &DIP_SCRN::diplomacy_special_colors);
            strlcat(text_buffer, DIP_SCRN::system_unsurrenderable_buffer, sizeof(text_buffer));
            use_special_colors = true;
        }

        fonts::Print(10, 287 /* 0x11f */, text_buffer);

        if (selected_planet_idx != -1 || use_special_colors) {
            fonts::Set_Font_Style_Shadow_Down(3, &DIP_SCRN::diplomacy_special_colors);
        } else {
            fonts::Set_Font_Style_Shadow_Down(3, &DIP_SCRN::diplomacy_normal_colors);
        }

        fonts::Print(10, 309 /* 0x135 */, DIP_SCRN::system_million_colonists_buffer);

        if (DIP_SCRN::diplomacy_system_planet_count > 0) {
            int y_offset = 32;
            for (int16_t i = 0; i < DIP_SCRN::diplomacy_system_planet_count; ++i) {
                int16_t planet_idx = DIP_SCRN::diplomacy_system_planet_list[i];
                s_planet_data* planet = &MOX::planet[planet_idx];

                snprintf(text_buffer, sizeof(text_buffer), "%s, %s", MOX::planet_size_string[planet->size], MOX::planet_climate_string[planet->climate]);

                if (planet_idx == selected_planet_idx) {
                    strlcat(text_buffer, DIP_SCRN::system_homeworld_buffer, sizeof(text_buffer));
                }

                fonts::Print(10, y_offset + 291 /* 0x123 */, text_buffer);
                y_offset += 14;
            }
        }
    }

    void __cdecl Send_Diplomacy_Handshake(int16_t sender_player_id, int16_t target_player_id) {
        DIP_SCRN::net_diplomacy_message.sender_player_id = (uint8_t)sender_player_id;
        DIP_SCRN::net_diplomacy_message.target_player_id = (uint8_t)target_player_id;

        // Note: net_diplomacy_message.arg1 is left uninitialized by this function.

        DIP_SCRN::net_diplomacy_message.arg2 = -1;
        DIP_SCRN::net_diplomacy_message.msg_type = DIP_NET_MSG_HANDSHAKE;

        NETMOX::Mox_Send_Message(
            MOX::player[sender_player_id].network_player_id,
            DIP_NET_PACKET_MESSAGE,
            &DIP_SCRN::net_diplomacy_message,
            sizeof(DIP_SCRN::net_diplomacy_message));

        RUSS::Mox_Update();
    }

    /**
     * @brief Redraws the diplomacy system selection list and current system details.
     */
    void __cdecl Diplomacy_Generic_System_List_Draw() {
        Draw_Diplomacy_Synch_Mode();
        Draw_Diplomacy_System();

        Draw_Diplomacy_System_Text(
            diplomacy_scan_input_flag,
            diplomacy_system_homeworld_flag);
    }

    /**
     * @brief Swaps the active talker and listener roles during network diplomacy.
     */
    void __cdecl Diplomacy_Net_Switcheroo() {
        uint8_t original_talker = (uint8_t)DIP_SCRN::talker_ambassador;
        uint8_t original_listen = DIP_SCRN::listen_ambassador;

        DIP_SCRN::old_response_message = -1;
        DIP_SCRN::net_diplomacy_message.arg2 = -1;

        // Perform Ambassador swap
        DIP_SCRN::talker_ambassador = original_listen;
        DIP_SCRN::listen_ambassador = original_talker;

        // Populate message with original state before variables were swapped
        DIP_SCRN::net_diplomacy_message.sender_player_id = original_listen;
        DIP_SCRN::net_diplomacy_message.target_player_id = original_talker;
        DIP_SCRN::net_diplomacy_message.msg_type = DIP_NET_MSG_SWITCH_TALKER;

        // Send message to the new talker (who was the original listener)
        NETMOX::Mox_Send_Message(
            MOX::player[DIP_SCRN::talker_ambassador].network_player_id,
            DIP_NET_PACKET_MESSAGE,
            &DIP_SCRN::net_diplomacy_message,
            sizeof(s_dipscrn_net_message));

        RUSS::Mox_Update();

        DIP_SCRN::response_message = 0;
        DIP_SCRN::old_response_message = 0;
    }

    /**
     * @brief Returns whether a diplomacy message ID expects player interaction.
     *
     * @param msg_id The diplomacy message identifier to test.
     * @return 1 if the message is interactive, otherwise 0.
     */
    int8_t __cdecl Diplomacy_Message_Is_Interactive(int16_t msg_id) {
        if (msg_id >= 0x53 && msg_id <= 0x55) {
            return 1;
        }

        if (msg_id >= 0x61 && msg_id <= 0x68) {
            return 1;
        }

        if (msg_id == 0x7D) {
            return 1;
        }

        if (msg_id == 0x56) {
            return 1;
        }

        if (msg_id == 0x69) {
            return 1;
        }

        if (msg_id == 0x6A) {
            return 1;
        }

        if (msg_id >= 0x1C && msg_id <= 0x24) {
            return 1;
        }

        return 0;
    }

    void __cdecl Diplomacy_Receive_Message(uint32_t from_player_id, uint16_t msg_code, uint32_t payload_len_or_unused, s_dipscrn_net_message* message) {
        (void)from_player_id;
        (void)payload_len_or_unused;

        if (msg_code < DIP_NET_PACKET_MESSAGE || msg_code >= DIP_NET_PACKET_RANGE_END) {
            return;
        }

        if (msg_code == DIP_NET_PACKET_MESSAGE) {
            net_diplomacy_chat_message[0] = s__00551100[0];
            receive_chat_message[0] = s__00551100[0];

            net_diplomacy_message.sender_player_id = message->sender_player_id;
            net_diplomacy_message.target_player_id = message->target_player_id;
            net_diplomacy_message.msg_type = message->msg_type;
            net_diplomacy_message.arg2 = message->arg2;
            net_diplomacy_message.arg1 = message->arg1;

            switch (net_diplomacy_message.msg_type) {
                case DIP_NET_MSG_GOODBYE:
                    net_diplomacy_esc_pressed = 1;
                    return;

                case DIP_NET_MSG_SWITCH_TALKER:
                    net_diplomacy_message.msg_type = 0;
                    response_message = 0;
                    old_response_message = 0;
                    strlcpy(diplomacy_message, s__00551100, sizeof(diplomacy_message));
                    // Fallthrough to default behavior
                    break;

                case DIP_NET_MSG_HANDSHAKE:
                    synch_up_established_flag = 1;
                    return;

                case DIP_NET_MSG_UNUSED_0x3F6:
                    return;

                default:
                    break;
            }

            response_message = net_diplomacy_message.msg_type;
            talker_ambassador = net_diplomacy_message.sender_player_id;
            listen_ambassador = net_diplomacy_message.target_player_id;
            return;
        }

        if (msg_code == DIP_NET_PACKET_CHAT) {
            strlcpy(receive_chat_message, (char*)message, sizeof(receive_chat_message));
        }
    }
} // namespace DIP_SCRN