#include "pch.h"
#include "player_roster.h"

#include "legacy_ids.h"
#include "ship_ids.h"

namespace DIP_SCRN {
    /**
     * @brief Resolves a treaty proposal and determines whether it succeeds.
     *
     * @param proposer_idx The player offering the treaty.
     * @param target_idx The player receiving the treaty proposal.
     * @param treaty_type The treaty type being proposed.
     * @return 1 when the treaty is accepted, otherwise 0.
     */
    int16_t __cdecl Diplomacy_Determine_Treaty_Proposal(game::id_player proposer, game::id_player target, int16_t treaty_type) {
        const auto proposer_idx = legacy_world::Encode_Player_Slot(proposer);
        const auto target_idx = legacy_world::Encode_Player_Slot(target);
        int16_t proposal_result;
        int16_t success;
        int16_t offered_tribute;
        int16_t offered_star_idx;
        int16_t offered_bc;
        int16_t offered_tech_app;
        int16_t offer_type;

        uint16_t initial_response = response_message;

        if (game::current_state.players[target_idx].objectives == PLAYER_OBJECTIVE_HUMAN) {
            proposal_result = Check_Human_Treaty_Proposal(proposer, target, treaty_type);
        } else {
            proposal_result = diplomac::Check_Treaty_Proposal(target, proposer, treaty_type, &response_message, &offer_type, &offered_tech_app, &offered_bc, &offered_star_idx, &offered_tribute);
        }

        if ((proposal_result == 0) || (proposal_result == 3)) {
            success = (proposal_result != 0) ? 1 : 0;
        } else {
            if (game::current_state.players[target_idx].objectives == PLAYER_OBJECTIVE_HUMAN) {
                proposal_result = Diplomacy_Human_Need_Better_Offer(proposer, target);
            } else {
                proposal_result = Diplomacy_Need_Better_Offer(offer_type, offered_tech_app, offered_bc, offered_star_idx, offered_tribute);
            }

            if (proposal_result == 1) {
                success = 1;
                switch (treaty_type) {
                    case 1:
                        response_message = 0x83;
                        break;
                    case 2:
                        response_message = 0x84;
                        break;
                    case 3:
                        response_message = 0x87;
                        break;
                    case 7:
                        response_message = 0x85;
                        break;
                    case 8:
                        response_message = 0x86;
                        break;
                    default:
                        break;
                }
            } else {
                response_message = 0x8A;
                success = 0;
            }
        }

        if (game::current_state.players[target_idx].objectives == PLAYER_OBJECTIVE_HUMAN) {
            if (success == 1) {
                Get_Human_Form_Treaty_Message(treaty_type, 3);
            } else {
                Get_Human_Form_Treaty_Message(treaty_type, 0);
            }
        } else {
            npcdiplo::NPC_Message_Rejection_Check(target, proposer, &response_message);
        }

        if (initial_response == response_message) {
            response_message = 0x8A;
        }

        return success;
    }

    /**
     * @brief Prompts the player to choose which active treaty to break.
     *
     * @param player_idx The player initiating the treaty break.
     * @param target_idx The player whose treaty is being broken.
     */
    void __cdecl Diplomacy_Break_Treaty(game::id_player player, game::id_player target) {
        const auto player_idx = player.id;
        const auto target_idx = target.id;
        uint8_t option_flags[7] = {0};

        if (game::current_state.players[player_idx].trade_treaty[target_idx] == 1) {
            option_flags[0] = 1;
        }
        if (game::current_state.players[player_idx].research_treaty[target_idx] == 1) {
            option_flags[1] = 1;
        }
        if (game::current_state.players[player_idx].tribute_treaty[target_idx] > 0) {
            option_flags[2] = 1;
        }
        if (game::current_state.players[player_idx].treaty[target_idx] == 1) {
            option_flags[3] = 1;
        }
        if (game::current_state.players[player_idx].treaty[target_idx] == 2) {
            option_flags[4] = 1;
        }
        option_flags[5] = 1;

        fields::Assign_Auto_Function(Draw_Diplomacy_Synch_Mode, 2);
        fields::Set_Input_Delay(1);

        for (int i = 0; i < 7; ++i) {
            strlcpy(diplomacy_list_choice[i], break_treaty_diplomacy_list[i], sizeof(diplomacy_list_choice[i]));
        }

        fonts::Set_Font_Style(4, &diplomacy_normal_colors);
        fonts::Set_Font_Highlight_Colors(&diplomacy_highlight_colors);
        fonts::Set_Font_Special_Colors(&diplomacy_special_colors);
        fonts::Set_Font_LF(4);
        Setup_Diplomacy_Help(7, break_treaty_help_entry);

        int8_t list_pos = 0;
        int16_t choice = fields::Get_List_Field(
            10, 118, 245, break_treaty_title, (char*)diplomacy_list_choice, 50, &list_pos, option_flags, 17, nullptr, 0, nullptr, 0, 0);

        if (choice > 4) {
            return;
        }

        switch (choice) {
            case 0:
                old_response_message = -1;
                Diplomacy_Break_Treaty_Resolution(player, target, 7);
                break;
            case 1:
                old_response_message = -1;
                Diplomacy_Break_Treaty_Resolution(player, target, 8);
                break;
            case 2:
                old_response_message = -1;
                if (game::current_state.players[player_idx].tribute_treaty[target_idx] == 1) {
                    Diplomacy_Break_Treaty_Resolution(player, target, 9);
                } else {
                    Diplomacy_Break_Treaty_Resolution(player, target, 10);
                }
                break;
            case 3:
                old_response_message = -1;
                Diplomacy_Break_Treaty_Resolution(player, target, 1);
                break;
            case 4:
                old_response_message = -1;
                Diplomacy_Break_Treaty_Resolution(player, target, 2);
                break;
        }
    }

    void __cdecl Get_Human_Form_Treaty_Message(int16_t treaty_type, int16_t msg_type) {
        switch (treaty_type) {
            case 1:
                if (msg_type != 3) {
                    JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 0x27, diplomacy_message, 0xFA);
                } else {
                    JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 0x26, diplomacy_message, 0xFA);
                }
                break;
            case 2:
                if (msg_type != 3) {
                    JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 0x29, diplomacy_message, 0xFA);
                } else {
                    JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 0x28, diplomacy_message, 0xFA);
                }
                break;
            case 7:
                if (msg_type != 3) {
                    JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 0x21, diplomacy_message, 0xFA);
                } else {
                    JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 0x20, diplomacy_message, 0xFA);
                }
                break;
            case 8:
                if (msg_type != 3) {
                    JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 0x23, diplomacy_message, 0xFA);
                } else {
                    JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 0x22, diplomacy_message, 0xFA);
                }
                break;
            case 9:
            case 10:
                if (msg_type != 3) {
                    JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 0x25, diplomacy_message, 0xFA);
                } else {
                    JIM::Get_Text_Message(s_jimtext2_lbx_00552340, 0x24, diplomacy_message, 0xFA);
                }
                break;
            default:
                break;
        }
    }

    void __cdecl Get_Diplomacy_Demand_Treaty_List(game::id_player first, game::id_player second, int16_t treaty_type, int16_t* out_count, game::id_player* out_list) {
        *out_count = 0;
        const auto player1 = first.id;
        const auto player2 = second.id;

        if (game::current_state.player_count <= 0) {
            return;
        }

        for (int16_t i = 0; i < game::current_state.player_count; ++i) {
            if (player2 != i && player1 != i) {
                if (game::current_state.players[i].contact[player2] == 1 && game::current_state.players[i].contact[player1] == 1) {
                    bool add_to_list = false;

                    if (treaty_type == 2) {
                        if (game::current_state.players[i].treaty[player2] == 2) {
                            add_to_list = true;
                        }
                    } else if (treaty_type == 3) {
                        if (game::current_state.players[i].treaty[player2] == 4 || game::current_state.players[i].treaty[player2] == 5) {
                            add_to_list = true;
                        }
                    } else if (treaty_type == 5) {
                        if (game::current_state.players[i].treaty[player2] < 4) {
                            add_to_list = true;
                        }
                    }

                    if (add_to_list) {
                        out_list[*out_count] = game::current_state.players[i].id;
                        *out_count += 1;
                    }
                }
            }
        }
    }

    int16_t __cdecl Diplomacy_Generic_List(int16_t item_count, char* list_strings, int8_t add_cancel, char* background) {
        if (item_count > 8) {
            item_count = 8;
        }

        uint8_t item_states[10];
        for (int i = 0; i < 10; ++i) {
            item_states[i] = 1;
        }

        fields::Clear_Fields();
        fields::Assign_Auto_Function(Draw_Diplomacy_Synch_Mode, 2);
        Setup_Full_Screen_Diplomacy_Help(0x2b6);
        fields::Set_Input_Delay(1);

        for (int16_t i = 0; i < item_count; ++i) {
            const char* src = reinterpret_cast<const char*>(list_strings + i * 40);
            strlcpy(diplomacy_list_choice[i], src, sizeof(diplomacy_list_choice[i]));
        }

        int16_t cancel_idx;
        if (add_cancel == 1) {
            JIM::Get_Text_Message(s_jimtext_lbx, 46, diplomacy_list_choice[item_count], 50);
            diplomacy_list_choice[item_count + 1][0] = MOX::s__00551100[0];
            cancel_idx = item_count;
        } else {
            cancel_idx = item_count - 1;
            diplomacy_list_choice[item_count][0] = MOX::s__00551100[0];
        }

        fonts::Set_Font_Style(4, &diplomacy_normal_colors);
        fonts::Set_Font_Highlight_Colors(&diplomacy_highlight_colors);
        fonts::Set_Font_Special_Colors(&diplomacy_special_colors);
        fonts::Set_Font_LF(4);

        int8_t selected_idx = 0;

        int16_t result = fields::Get_List_Field(
            10, 0x76, 0xf5, (char*)background, (char*)diplomacy_list_choice, 50, &selected_idx, item_states, 0x11, nullptr, 0, nullptr, 0, 0);

        if (result == -1 || result == cancel_idx) {
            result = -1;
        }

        return result;
    }

    void __cdecl Diplomacy_Offer_Gift(game::id_player player, game::id_player target) {
        const auto player_idx = player.id;
        const auto target_idx = target.id;
        uint8_t options[16] = {0};
        int16_t differential_tech_count = 0;
        uint8_t differential_tech_ids[TECH_APP_COUNT];
        int32_t differential_tech_scores[TECH_APP_COUNT];

        options[0] = 1;
        options[1] = 1;

        if (game::current_state.players[target_idx].tribute_treaty[player_idx] > 0) {
            options[0] = 0;
            options[1] = 0;
        } else {
            int16_t player_tribute = game::current_state.players[player_idx].tribute_treaty[target_idx];
            if (player_tribute > 0) {
                options[0] = 0;
                options[1] = 0;
            } else if (player_tribute == 1) {
                options[0] = 0;
            } else if (player_tribute == 2) {
                options[0] = 0;
                options[1] = 0;
            }
        }

        differential_tech_count = 0;
        npcdiplo::Get_Differential_Tech_List(target, player, 0, 0xc343cb00, differential_tech_ids, differential_tech_scores, &differential_tech_count, 1);

        if (differential_tech_count > 0 && noreport == 0) {
            options[2] = 1;
        }

        if (Check_Surrender_System(player, target)) {
            options[3] = 1;
        }

        if (game::current_state.players[player_idx].bc > 100) {
            options[4] = 1;
        }

        options[5] = 1;

        fields::Assign_Auto_Function(Draw_Diplomacy_Synch_Mode, 2);
        fields::Set_Input_Delay(1);

        for (int i = 0; i < 7; ++i) {
            strlcpy(diplomacy_list_choice[i], offer_diplomacy_list[i], sizeof(diplomacy_list_choice[i]));
        }

        fonts::Set_Font_Style(4, &diplomacy_normal_colors);
        fonts::Set_Font_Highlight_Colors(&diplomacy_highlight_colors);
        fonts::Set_Font_Special_Colors(&diplomacy_special_colors);
        fonts::Set_Font_LF(4);

        Setup_Diplomacy_Help(7, offer_help_entry);

        int8_t title = 0;

        int16_t choice = fields::Get_List_Field(
            10, 0x76, 0xf5, offer_title, (char*)diplomacy_list_choice, 0x32, &title, options, 0x11, nullptr, 0, nullptr, 0, 0);

        switch (choice) {
            case 0:
                old_response_message = -1;
                diplomac::Start_Tribute_Treaty(player, target, 1);
                if (game::current_state.players[target_idx].objectives != PLAYER_OBJECTIVE_HUMAN) {
                    diplomac::Get_Gift_Response(player, target, 2, 1, &response_message);
                    npcdiplo::NPC_Proposal_Rejection_Accept(target, player, 1);
                } else {
                    Get_Human_Gift_Response(player, target, 2, 1);
                }
                break;

            case 1:
                old_response_message = -1;
                diplomac::Start_Tribute_Treaty(player, target, 2);
                if (game::current_state.players[target_idx].objectives != PLAYER_OBJECTIVE_HUMAN) {
                    diplomac::Get_Gift_Response(player, target, 2, 2, &response_message);
                    npcdiplo::NPC_Proposal_Rejection_Accept(target, player, 1);
                } else {
                    Get_Human_Gift_Response(player, target, 2, 2);
                }
                break;

            case 2: {
                int16_t tech_idx = Diplomacy_Generic_Exchange_Tech_List(player, offer_title, differential_tech_ids, differential_tech_count, 0);
                if (tech_idx == -1) {
                    *diplomacy_message = MOX::s__00551100[0];
                } else {
                    uint8_t tech_app = differential_tech_ids[tech_idx];
                    old_response_message = -1;
                    COLCALC::Player_Gets_Tech_App(&game::current_state.players[target_idx], tech_app);

                    if (game::current_state.players[target_idx].objectives != PLAYER_OBJECTIVE_HUMAN) {
                        diplomac::Get_Gift_Response(player, target, 3, tech_app, &response_message);
                        npcdiplo::NPC_Proposal_Rejection_Accept(target, player, 1);
                        npcdiplo::Get_Exchange_Tech_List(player, target, &exchange_tech_count, target_tech_list, exchange_tech_list);
                    } else {
                        Get_Human_Gift_Response(player, target, 3, tech_app);
                    }
                }
                break;
            }

            case 3: {
                int16_t system_idx = Diplomacy_System_Display(player, current_ambassador, 0);
                if (system_idx == -1) {
                    *diplomacy_message = MOX::s__00551100[0];
                } else {
                    const auto selected = game::Star_Key_At_Slot(game::current_state, system_idx);
                    if (selected == game::NO_STAR) break;
                    old_response_message = -1;
                    if (game::current_state.players[target_idx].objectives != PLAYER_OBJECTIVE_HUMAN) {
                        diplomac::Get_Gift_Response(player, target, 4, system_idx, &response_message);
                        npcdiplo::NPC_Proposal_Rejection_Accept(target, player, 1);
                    } else {
                        Get_Human_Gift_Response(player, target, 4, system_idx);
                    }

                    Commit_System_Offer(selected, player, target);
                }
                break;
            }

            case 4:
                Diplomacy_Offer_Money(player, target, 1);
                break;

            default:
                break;
        }
    }

    void Diplomacy_Mouse_Init(game::id_player ambassador) {
        diplomacy_mouse_bitmap = (uint8_t*)allocate::Allocate_First_Block(MOX::screen_seg, 0x395);
        s_animation_header* diplomat_anim = (s_animation_header*)
            farload::Far_Reload_Next("DIPLOMAT.LBX", game::current_state.players.at(ambassador.id).race, MOX::screen_seg);

        bitmap::Clear_Mouse_Image(0);
        bitmap::Create_Blank_Bitmap(30, 30, diplomacy_mouse_bitmap);
        bitmap::Draw_Picture_To_Bitmap((s_animation_header*)diplomacy_mouse_bitmap, diplomat_anim);

        video::Set_Page_Off();
        graphics::Fill(0, 0, 0x27F, 0x1DF, 0);

        animate::Draw_Palette(diplomat_anim);
        remap::Update_Glass_Remap_Colors();
        palette::Set_Palette_Changes(0, 0xFF);

        graphics::Fill(0, 0, 0x27F, 0x1DF, 0);
        bitmap::Draw_Bitmap_To_Mouse(0, 0, (s_animation_header*)diplomacy_mouse_bitmap, 0);

        diplomacy_system_star_background_seg = (s_animation_header*)farload::Far_Reload_Next("dipstars.lbx", 0, MOX::screen_seg);
        for (int i = 0; i < 11; ++i) {
            MOX::fleet_galaxy_star_seg[i] = (s_animation_header*)
                farload::Far_Reload_Next("dipstars.lbx", i + 1, MOX::screen_seg);
        }

        main_diplomacy_title = (char*)allocate::Allocate_Next_Block(MOX::screen_seg, 30);
        for (int i = 0; i < 9; ++i) {
            main_diplomacy_list[i] = (char*)allocate::Allocate_Next_Block(MOX::screen_seg, DIP_SCRN_MAIN_LIST_CAPACITY);
        }

        for (int i = 0; i < 11; ++i) {
            main_net_diplomacy_list[i] = (char*)allocate::Allocate_Next_Block(MOX::screen_seg, DIP_SCRN_MAIN_LIST_CAPACITY);
        }

        for (int i = 0; i < 5; ++i) {
            main_repulsive_diplomacy_list[i] = (char*)allocate::Allocate_Next_Block(MOX::screen_seg, DIP_SCRN_SMALL_LIST_CAPACITY);
        }

        for (int i = 0; i < 6; ++i) {
            main_net_repulsive_diplomacy_list[i] = (char*)allocate::Allocate_Next_Block(MOX::screen_seg, DIP_SCRN_SMALL_LIST_CAPACITY);
        }

        propose_treaty_title = (char*)allocate::Allocate_Next_Block(MOX::screen_seg, 40);
        for (int i = 0; i < 7; ++i) {
            propose_treaty_diplomacy_list[i] = (char*)allocate::Allocate_Next_Block(MOX::screen_seg, 30);
        }

        break_treaty_title = (char*)allocate::Allocate_Next_Block(MOX::screen_seg, 30);
        for (int i = 0; i < 7; ++i) {
            break_treaty_diplomacy_list[i] = (char*)allocate::Allocate_Next_Block(MOX::screen_seg, 40);
        }

        demand_title = (char*)allocate::Allocate_Next_Block(MOX::screen_seg, 30);
        for (int i = 0; i < 11; ++i) {
            demand_diplomacy_list[i] = (char*)allocate::Allocate_Next_Block(MOX::screen_seg, 40);
        }

        offer_title = (char*)allocate::Allocate_Next_Block(MOX::screen_seg, 30);
        for (int i = 0; i < 7; ++i) {
            offer_diplomacy_list[i] = (char*)allocate::Allocate_Next_Block(MOX::screen_seg, 30);
        }

        for (int i = 0; i < 3; ++i) {
            agree_diplomacy_list[i] = (char*)allocate::Allocate_Next_Block(MOX::screen_seg, DIP_SCRN_MAIN_LIST_CAPACITY);
        }

        choose_a_system_buffer = (char*)allocate::Allocate_Next_Block(MOX::screen_seg, 40);
        system_cancel_buffer = (char*)allocate::Allocate_Next_Block(MOX::screen_seg, 20);
        system_million_colonists_buffer = (char*)allocate::Allocate_Next_Block(MOX::screen_seg, 30);
        system_homeworld_buffer = (char*)allocate::Allocate_Next_Block(MOX::screen_seg, 30);
        system_x_star_system_buffer = (char*)allocate::Allocate_Next_Block(MOX::screen_seg, 35);
        player_number_name_buffer = (char*)allocate::Allocate_Next_Block(MOX::screen_seg, 40);
        player_name_ambassador_buffer = (char*)allocate::Allocate_Next_Block(MOX::screen_seg, 40);
        you_were_framed_buffer = (char*)allocate::Allocate_Next_Block(MOX::screen_seg, 40);
        system_unsurrenderable_buffer = (char*)allocate::Allocate_Next_Block(MOX::screen_seg, 40);
        enter_your_message_buffer = (char*)allocate::Allocate_Next_Block(MOX::screen_seg, 40);
        percent_tribute_treaty_buffer = (char*)allocate::Allocate_Next_Block(MOX::screen_seg, 40);
        statement_trade_treaty_buffer = (char*)allocate::Allocate_Next_Block(MOX::screen_seg, 40);
        statement_research_treaty_buffer = (char*)allocate::Allocate_Next_Block(MOX::screen_seg, 40);
        net_listen_mode_option_buffer = (char*)allocate::Allocate_Next_Block(MOX::screen_seg, 20);
        net_listen_mode_good_bye_buffer = (char*)allocate::Allocate_Next_Block(MOX::screen_seg, 20);

        file_ani::Set_File_Animation_Buffer(allocate::Allocate_Next_Block(MOX::screen_seg, 0x7C830));
        Load_Diplomacy_Text_Messages();

        diplomacy_current_music = -1;
        diplomacy_good_music = game::current_state.players.at(ambassador.id).race + 1;
        diplomacy_bad_music = (int16_t)game_random::Random(3) + 13;
    }

    void __cdecl Start_Diplomacy_Music(int16_t music_type) {
        int16_t new_music;

        if (music_type == 0) {
            new_music = diplomacy_bad_music;
        } else {
            new_music = diplomacy_good_music;
        }

        if (new_music != diplomacy_current_music) {
            diplomacy_current_music = new_music;
        }
    }

    void __cdecl Get_Diplomacy_Statement(int16_t statement_id, game::id_player speaker, game::id_player listener, char* out_text, size_t out_text_size) {
        const auto speaker_player_id = speaker.id;
        const auto listener_player_id = listener.id;
        if (out_text == nullptr || out_text_size == 0) {
            return;
        }

        out_text[0] = '\0';

        char filename[16];
        switch (MOX::settings.language) {
            case 1: strlcpy(filename, s_DIPLOMSG_LBX, sizeof(filename)); break;
            case 2: strlcpy(filename, s_DIPLOMSF_LBX, sizeof(filename)); break;
            case 3: strlcpy(filename, s_DIPLOMSS_LBX, sizeof(filename)); break;
            case 4: strlcpy(filename, s_DIPLOMSI_LBX, sizeof(filename)); break;
            case 5: strlcpy(filename, s_DIPLOMSP_LBX, sizeof(filename)); break;
            case 0:
            default: strlcpy(filename, s_DIPLOMSE_LBX, sizeof(filename)); break;
        }

        bool add_framed = false;
        if (statement_id == 0x14 || statement_id == 0x16 || statement_id == 0x1D || statement_id == 0x1F ||
            statement_id == 0x28 || statement_id == 0x2A || statement_id == 0x31 || statement_id == 0x33 ||
            statement_id == 0x3A || statement_id == 0x3C || statement_id == 0x48 || statement_id == 0x4A ||
            statement_id == 0x8F || statement_id == 0x91) {
            add_framed = true;
        }

        int file_msg_id = statement_id;
        if (add_framed || statement_id == 0x6A) {
            file_msg_id = statement_id - 1;
        }

        uint8_t buf[5202];
        farload::Farload_Data_Static(filename, file_msg_id, buf, 0, 1, 5202);

        int count = buf[1];
        int rnd_idx = game_random::Random(count) - 1;
        char* record = reinterpret_cast<char*>(buf) + 2 + rnd_idx * 200;

        for (int i = 0; record[i] != '\0'; ++i) {
            uint8_t c = record[i];
            if (c < 0x80 || c > 0x95) {
                char text_char[2];
                text_char[0] = static_cast<char>(c);
                text_char[1] = '\0';

                if (strlcat(out_text, text_char, out_text_size) >= out_text_size) {
                    break;
                }
                continue;
            }
            const char* replacement = nullptr;
            char temp_buf[64];

            switch (c) {
                case 0x80:
                    replacement = game::current_state.players[speaker_player_id].name;
                    break;
                case 0x81:
                    replacement = game::current_state.players[speaker_player_id].race_name;
                    break;
                case 0x82:
                    replacement = game::current_state.players[listener_player_id].race_name;
                    break;
                case 0x83:
                    replacement = game::current_state.players[listener_player_id].name;
                    break;
                case 0x84:
                    replacement = An(game::current_state.players[speaker_player_id].race_name);
                    break;
                case 0x85:
                    replacement = An(game::current_state.players[listener_player_id].race_name);
                    break;
                case 0x86:
                    if (const auto star = game::current_state.stars.find(game::current_state.players[listener_player_id].diplomacy_system[speaker_player_id]);
                        star != game::current_state.stars.end()) replacement = star->second.name;
                    break;
                case 0x87:
                case 0x88:
                    replacement = game::current_state.players.at(game::current_state.players[listener_player_id].diplomacy_proposal_war_player[speaker_player_id].id).race_name;
                    break;
                case 0x89:
                    replacement = TECH::Technology_Applications_Name(game::current_state.players[listener_player_id].diplomacy_proposal_exchange_tech[speaker_player_id], listener);
                    break;
                case 0x8A: {
                    int treaty = game::current_state.players[listener_player_id].treaty[speaker_player_id];
                    if (treaty == 1 || treaty == 2) {
                        replacement = MOX::treaty_labels[treaty];
                    } else if (game::current_state.players[listener_player_id].trade_treaty[speaker_player_id] != 0) {
                        replacement = statement_trade_treaty_buffer;
                    } else if (game::current_state.players[listener_player_id].research_treaty[speaker_player_id] != 0) {
                        replacement = statement_research_treaty_buffer;
                    }
                    break;
                }
                case 0x8B:
                    replacement = game::current_state.players.at(game::current_state.players[listener_player_id].diplomacy_proposal_war_player[speaker_player_id].id).name;
                    break;
                case 0x8C: {
                    int gift = game::current_state.players[listener_player_id].last_gift[speaker_player_id];
                    if (gift >= 1 && gift <= 4) {
                        JIM::Get_Text_Message(s_jimtext2_lbx, 0x4A + gift, temp_buf, 40);
                        replacement = temp_buf;
                    } else {
                        replacement = s_FWEE;
                    }
                    break;
                }
                case 0x8D:
                    replacement = TECHDATA::buildings[game::current_state.players[listener_player_id].diplomacy_value[speaker_player_id]].name;
                    break;
                case 0x8E:
                    if (MOX::settings.language == 0) {
                        snprintf(temp_buf, sizeof(temp_buf), s_fmt_dot, MOX::stardate / 10, MOX::stardate % 10);
                    } else {
                        snprintf(temp_buf, sizeof(temp_buf), s_fmt_comma, MOX::stardate / 10, MOX::stardate % 10);
                    }
                    replacement = temp_buf;
                    break;
                case 0x8F:
                    break;
                case 0x90:
                case 0x95:
                    if (game::current_state.players[listener_player_id].diplomacy_proposal_tribute_bribe[speaker_player_id] == 0) {
                        if (game::current_state.players[listener_player_id].diplomacy_proposal_tech_bribe1[speaker_player_id] != 0) {
                            replacement = TECH::Technology_Applications_Name(game::current_state.players[listener_player_id].diplomacy_proposal_tech_bribe1[speaker_player_id], listener);
                        } else if (game::current_state.players[listener_player_id].diplomacy_proposal_gold_bribe[speaker_player_id] == 0) {
                            if (const auto slot = game::Star_Slot(game::current_state, game::current_state.players[listener_player_id].diplomacy_proposal_system_bribe[speaker_player_id]); slot >= 0) {
                                JIM::Get_Text_Message(s_jimtext2_lbx, 0x3A, temp_buf, 50);
                                JIM::Decode_Text_System(temp_buf, slot, 50);
                                replacement = temp_buf;
                            }
                        } else {
                            snprintf(temp_buf, sizeof(temp_buf), s_fmt_BC, game::current_state.players[listener_player_id].diplomacy_proposal_gold_bribe[speaker_player_id]);
                            replacement = temp_buf;
                        }
                    } else {
                        strlcpy(temp_buf, percent_tribute_treaty_buffer, sizeof(temp_buf));
                        int stat_val = (game::current_state.players[listener_player_id].diplomacy_proposal_tribute_bribe[speaker_player_id] == 1) ? 5 : 10;
                        JIM::Decode_Text_Stat(temp_buf, stat_val, 40);
                        replacement = temp_buf;
                    }
                    break;
                case 0x91:
                    replacement = game::current_state.players.at(game::current_state.players[listener_player_id].diplomacy_proposal_war_player[speaker_player_id].id).race_name;
                    break;
                case 0x92: {
                    int broken = game::current_state.players[listener_player_id].last_broken_treaty[speaker_player_id];
                    if (broken == 7) {
                        replacement = statement_trade_treaty_buffer;
                    } else if (broken == 8) {
                        replacement = statement_research_treaty_buffer;
                    } else {
                        replacement = MOX::treaty_labels[broken];
                    }
                    break;
                }
                case 0x93:
                    if (const auto star = game::current_state.stars.find(game::current_state.players[listener_player_id].diplomacy_system[speaker_player_id]);
                        star != game::current_state.stars.end()) replacement = star->second.name;
                    break;
                case 0x94:
                    replacement = game::current_state.players.at(game::current_state.players[listener_player_id].trust_breaker_player[speaker_player_id].id).race_name;
                    break;
            }

            if (replacement) {
                if (strlcat(out_text, replacement, out_text_size) >= out_text_size) {
                    break;
                }
            }
        }

        if (add_framed) {
            strlcat(out_text, you_were_framed_buffer, out_text_size);
        }

        Start_Diplomacy_Music(buf[0]);
    }

    /**
     * Checks if player_a has at least one system (up to 3) that could potentially
     * surrender to player_b.
     */
    bool __cdecl Check_Surrender_System(game::id_player player_a, game::id_player player_b) {
        int16_t star_idx = 0;
        int16_t surrender_count = 0;

        if (std::ssize(game::current_state.stars) > 0) {
            do {
                // Limit search once we find at least 3 systems
                if (surrender_count >= 3) {
                    break;
                }

                // has_colony is treated as a bitmask where bit N indicates player N has a colony
                const auto star = game::Star_Key_At_Row(game::current_state, star_idx);
                if (game::current_state.stars.at(star).has_colony.test(player_a)) {
                    if (COLCALC::May_Surrender_Star(game::current_state, star, player_a, player_b)) {
                        surrender_count++;
                    }
                }

                star_idx++;
            } while (star_idx < std::ssize(game::current_state.stars));
        }

        return surrender_count >= 1;
    }

    int16_t __cdecl Diplomacy_Need_Better_Offer(int16_t type, int16_t tech_app, int16_t bc, int16_t star_idx, int16_t tribute) {
        const auto selected = game::Star_Key_At_Slot(game::current_state, star_idx);
        uint8_t selectable[16] = {0};
        char text_buffer[40];

        selectable[0] = 1;
        selectable[1] = 1;
        selectable[2] = 1;

        int16_t msg_idx = (game_random::Random(2) == 1) ? 0x4A : 0x4B;
        JIM::Get_Text_Message(DIP_SCRN::s_jimtext_lbx_00552360, msg_idx, text_buffer, 40);

        fields::Clear_Fields();
        fields::Assign_Auto_Function(Draw_Diplomacy_Synch_Mode, 2);
        fields::Deactivate_Help_List();
        fields::Set_Input_Delay(1);

        for (int16_t i = 0; i < 3; ++i) {
            char* src = agree_diplomacy_list[i];
            strlcpy(diplomacy_list_choice[i], src, sizeof(diplomacy_list_choice[i]));
        }

        fonts::Set_Font_Style(4, &diplomacy_normal_colors);
        fonts::Set_Font_Highlight_Colors(&diplomacy_highlight_colors);
        fonts::Set_Font_Special_Colors(&diplomacy_special_colors);
        fonts::Set_Font_LF(4);

        int8_t current_idx = 0;
        int16_t result = fields::Get_List_Field(
            10, 118, 245, text_buffer, diplomacy_list_choice[0], 50, &current_idx, selectable, 17, nullptr, 0, nullptr, 0, 0);

        if (result == -1 || result == 1) {
            return 0;
        }

        switch (type) {
            case 1:
                game::current_state.players.at(MOX::local_player.id).bc -= bc;
                game::current_state.players.at(current_ambassador.id).bc += bc;
                return 1;
            case 2:
                return Commit_System_Offer(selected, MOX::local_player, current_ambassador) ? 1 : 0;
            case 3:
                game::current_state.players.at(MOX::local_player.id).tribute_treaty[current_ambassador.id] = tribute;
                return 1;
            case 4:
                COLCALC::Player_Gets_Tech_App(&game::current_state.players.at(current_ambassador.id), tech_app);
                return 1;
            default:
                return 1;
        }
    }

    void __cdecl Get_Human_Gift_Response(game::id_player sender, game::id_player target, int16_t gift_type, int16_t gift_arg) {
        if (MOX::game_type == 1) {
            DIP_SCRN::old_response_message = DIP_SCRN::response_message;
            DIP_SCRN::Get_Human_Offer_Gift_Message(sender, gift_type, gift_arg);
            DIP_SCRN::Diplomacy_Load_New_Ambassador(sender);
            DIP_SCRN::Diplomacy_Display_Response();
            DIP_SCRN::Diplomacy_Load_New_Ambassador(target);
        } else {
            DIP_SCRN::net_diplomacy_message.sender_player_id = static_cast<uint8_t>(sender.id);
            DIP_SCRN::net_diplomacy_message.target_player_id = static_cast<uint8_t>(target.id);
            DIP_SCRN::net_diplomacy_message.arg1 = gift_type;
            DIP_SCRN::net_diplomacy_message.msg_type = DIP_NET_MSG_GIFT_RESPONSE;
            DIP_SCRN::net_diplomacy_message.arg2 = gift_arg;

            NETMOX::Mox_Send_Message(game::current_state.players.at(target.id).network_player_id, DIP_NET_PACKET_MESSAGE, &DIP_SCRN::net_diplomacy_message, sizeof(DIP_SCRN::net_diplomacy_message));
            RUSS::Mox_Update();
        }

        switch (gift_type) {
            case 1:
                DIP_SCRN::response_message = 0xA3;
                break;
            case 2:
                DIP_SCRN::response_message = 0xA2;
                break;
            case 3:
                DIP_SCRN::response_message = 0xA1;
                break;
            case 4:
                DIP_SCRN::response_message = 0xA4;
                break;
            default:
                break;
        }
    }

    /**
     * @brief Scans a star system for colonies belonging to a specific player.
     *
     * @param star_idx Index of the star system to scan.
     * @param player_idx Index of the player whose colonies to search for.
     * @param capitol Output: ID of the planet with the player's Capitol, or NO_PLANET.
     * @param owned_count Output: Total number of colonies owned by the player in this system.
     * @param planet_ids Output: IDs of the planets owned by the player, in orbit order.
     * @return The player's owner slot if the system has their colonies, otherwise the star's owner.
     */
    game::owner_slot Diplomacy_System_Scan(const game::s_game_state& state, game::id_star star_id, game::id_player player,
        game::id_planet* capitol, int16_t* owned_count, game::id_planet* planet_ids) {
        *capitol = game::NO_PLANET;
        *owned_count = 0;

        const auto system = state.stars.find(star_id);
        if (system == state.stars.end()) return game::NO_OWNER;
        const auto& star = system->second;
        if (!star.has_colony.test(player)) return star.owner;

        for (const auto planet_id : star.orbit_planet_ids) {
            if (planet_id == game::NO_PLANET) continue;
            const auto colony_id = state.planets.at(planet_id).colony_id;
            if (colony_id == game::NO_COLONY) continue;
            const auto& colony = state.colonies.at(colony_id);
            if (colony.owner != player) continue;
            planet_ids[(*owned_count)++] = planet_id;
            if (colony.buildings[BUILDING_CAPITOL] != 0) *capitol = planet_id;
        }

        return factions::From_Player(player);
    }

    int16_t __cdecl Diplomacy_Offer_Money(game::id_player player, game::id_player target, int8_t do_transaction) {
        const auto player_idx = player.id;
        const auto target_idx = target.id;
        int16_t result_bc = -1;
        int16_t choice_count;
        int16_t bc_options[5];
        int8_t selection_idx = 0;
        uint8_t selection_flags[15];
        char title_text[40];

        Setup_Full_Screen_Diplomacy_Help(0x267);

        memset(selection_flags, 1, 15);

        for (int i = 0; i < 15; ++i) {
            diplomacy_list_choice[i][0] = '\0';
        }

        int current_bc = game::current_state.players[player_idx].bc;
        if (current_bc > 32000) {
            current_bc = 32000;
        }

        int16_t rounded_bc = (int16_t)(current_bc / 20) * 20;

        if (rounded_bc < 100) {
            choice_count = rounded_bc / 20;
            bc_options[0] = 20;
            bc_options[1] = 40;
            bc_options[2] = 60;
            bc_options[3] = 80;
            bc_options[4] = 100;
        } else {
            choice_count = 5;
            bc_options[0] = (int16_t)((rounded_bc / 5) / 20) * 20;
            bc_options[1] = (int16_t)((rounded_bc * 2 / 5) / 20) * 20;
            bc_options[2] = (int16_t)((rounded_bc * 3 / 5) / 20) * 20;
            bc_options[3] = (int16_t)((rounded_bc * 4 / 5) / 20) * 20;
            bc_options[4] = (int16_t)(rounded_bc / 20) * 20;
        }

        for (int i = 0; i < choice_count; ++i) {
            snprintf(diplomacy_list_choice[i], sizeof(diplomacy_list_choice[i]), s__d_BC_00552384, (int)bc_options[i]);
            if (game::current_state.players[player_idx].bc < (int)bc_options[i]) {
                selection_flags[i] = 0;
            }
        }

        JIM::Get_Text_Message(DIP_SCRN::s_jimtext_lbx_00552360, 0x2E, diplomacy_list_choice[choice_count], 0x28);

        JIM::Get_Text_Message(DIP_SCRN::s_jimtext2_lbx_00552340, 8, title_text, 0x28);

        int16_t selection = fields::Get_List_Field(
            10, 0x76, 0xF5, title_text, diplomacy_list_choice[0], 0x32, &selection_idx, selection_flags, 0x11, 0, 0, 0, 0, 0);

        if (selection >= 0 && selection < choice_count) {
            result_bc = bc_options[selection];
            if (do_transaction == 1) {
                old_response_message = -1;
                game::current_state.players[player_idx].bc -= (int)result_bc;
                game::current_state.players[target_idx].bc += (int)result_bc;

                if (game::current_state.players[target_idx].objectives == PLAYER_OBJECTIVE_HUMAN) {
                    Get_Human_Gift_Response(player, target, 1, result_bc);
                } else {
                    diplomac::Get_Gift_Response(player, target, 1, result_bc, &response_message);
                    if ((int)(MOX::stardate * 5 - 175000) <= (int)result_bc) {
                        npcdiplo::NPC_Proposal_Rejection_Accept(target, player, 1);
                    }
                }
            }
        }

        return result_bc;
    }

    void Resolve_Delayed_Diplomacy_Orders() {
        for (int16_t i = 0; i < game::current_state.player_count; ++i) {
            for (int16_t j = 0; j < game::current_state.player_count; ++j) {
                if (i == j) {
                    continue;
                }

                // Only process diplomatic orders if both parties are still in the game
                if (game::current_state.players[j].eliminated == 0 && game::current_state.players[i].eliminated == 0) {
                    int8_t order = game::current_state.players[i].delayed_diplomacy_orders[j];

                    if (order == 0) {
                        diplomac::Declare_War(game::current_state.players[i].id, game::current_state.players[j].id);
                    } else if (order == 1) {
                        diplomac::Declare_Peace(game::current_state.players[i].id, game::current_state.players[j].id);
                    } else if (order == 2) {
                        diplomac::Break_Treaties(game::current_state.players[i].id, game::current_state.players[j].id);
                    }

                    // Reset the order to "no order" (-1) once processed or if invalid
                    game::current_state.players[i].delayed_diplomacy_orders[j] = -1;
                }
            }
        }
    }

    void __cdecl Set_Opportunity_Attacks() {
        if (game::current_state.player_count <= 0) {
            return;
        }

        for (int16_t p_idx = 0; p_idx < (int16_t)game::current_state.player_count; ++p_idx) {
            for (int16_t target_idx = 0; target_idx < (int16_t)game::current_state.player_count; ++target_idx) {
                s_player& player = game::current_state.players[p_idx];
                s_player& target = game::current_state.players[target_idx];

                bool valid_opportunity = false;

                // Condition for player looking for an opportunity:
                // Must not be eliminated, must not already have reached 100 objectives,
                // and hyperspace flux must not be present.
                if (player.eliminated == 0 && player.objectives != PLAYER_OBJECTIVE_HUMAN) {
                    if (EVENTS::Event_Check_Hyperspace_Flux() == 0) {
                        // Condition for the target:
                        // Must have reached 100 objectives, must be a different player,
                        // must not be eliminated, and must be in contact and within range.
                        if (target.objectives == PLAYER_OBJECTIVE_HUMAN && p_idx != target_idx && target.eliminated == 0 &&
                            player.contact[target_idx] == 1 && player.within_range[target_idx] == 1) {
                            valid_opportunity = true;
                        }
                    }
                }

                if (valid_opportunity) {
                    auto attack = AIMOVE::Find_Opportunity_Attack(&AIDATA::state, &game::current_state, MOX::settings.game_difficulty, MOX::settings.strategic_combat_flag, player.id, target.id, 100);
                    player.opportunity_planet_id[target_idx] = attack ? attack->planet_id : game::NO_PLANET;
                    player.opportunity_planet_value[target_idx] = attack ? attack->colony_value : 0;
                    player.opportunity_actual_attacker_ratio[target_idx] = attack ? attack->strength_ratio_percent : 0;
                } else {
                    player.opportunity_planet_id[target_idx] = game::NO_PLANET;
                    player.opportunity_planet_value[target_idx] = 0;
                    player.opportunity_actual_attacker_ratio[target_idx] = 0;
                }
            }
        }
    }

    void __cdecl Show_Sneak_Attack_Message(game::id_player attacker) {
        Npc_Diplomacy_Screen(attacker, 1);
    }

    void __cdecl Diplomacy_Display_Response() {
        bool done = false;

        fields::Assign_Auto_Function(Draw_Diplomacy_Synch_Mode, 2);
        fields::Deactivate_Help_List();

        while (!done) {
            timer::Mark_Time();
            fields::Clear_Fields();

            int16_t hidden_field_idx = fields::Add_Hidden_Field(0, 0, 0x27f, 0x1df, s_empty_string_00551100, 0);
            int16_t input_idx = fields::Get_Input();

            if (hidden_field_idx == input_idx) {
                done = true;
            }

            if (done) {
                break;
            }

            Draw_Diplomacy_Synch_Mode();
            RUSS::Mox_Sync_Update();
            timer::Release_Time(2);
        }

        fields::Clear_Fields();
        fields::Deactivate_Auto_Function();
    }

    void __cdecl Add_Diplomacy_Turn_Summary_Messages() {
        for (int16_t i = 0; i < game::current_state.player_count; ++i) {
            for (int16_t j = i + 1; j < game::current_state.player_count; ++j) {
                if (i == j) {
                    continue;
                }

                if (game::current_state.players[j].eliminated != 0) {
                    continue;
                }

                if (game::current_state.players[i].eliminated != 0) {
                    continue;
                }

                if (game::current_state.players.at(MOX::local_player.id).contact[j] == 1 ||
                    game::current_state.players.at(MOX::local_player.id).contact[i] == 1) {
                    if (game::current_state.players[i].last_turn_treaty[j] != game::current_state.players[i].treaty[j]) {
                        switch (game::current_state.players[i].treaty[j]) {
                            case 1:
                                MSG_::Add_Nonaggression_Msg(game::current_state.players[i].id, game::current_state.players[j].id);
                                break;
                            case 2:
                                MSG_::Add_Alliance_Msg(game::current_state.players[i].id, game::current_state.players[j].id);
                                break;
                            case 3:
                                MSG_::Add_Peace_Msg(game::current_state.players[i].id, game::current_state.players[j].id);
                                break;
                            case 4:
                            case 5:
                            case 6:
                                MSG_::Add_War_Msg(game::current_state.players[i].id, game::current_state.players[j].id);
                                break;
                        }
                    }
                }
            }
        }
    }

    void __cdecl Get_Main_Diplomacy_Choices() {
        bool exit_flag = false;

        if (game::current_state.players.at(current_ambassador.id).objectives != PLAYER_OBJECTIVE_HUMAN) {
            npcdiplo::Get_Exchange_Tech_List(
                MOX::local_player,
                current_ambassador,
                &exchange_tech_count,
                target_tech_list,
                exchange_tech_list);
        }

        while (!exit_flag) {
            uint8_t main_options[9];
            memset(main_options, 1, sizeof(main_options));

            Diplomacy_Set_Main_Options(MOX::local_player, current_ambassador, main_options);

            int16_t worst_modifier = diplomac::Find_Worst_Modifier(MOX::local_player, current_ambassador);

            if (worst_modifier <= -100) {
                if (old_response_message != response_message) {
                    Diplomacy_Display_Response();
                }
                response_message = 0x82;
                Diplomacy_Display_Response();
                exit_flag = true;
            } else {
                fields::Assign_Auto_Function(Draw_Diplomacy_Synch_Mode, 2);
                fields::Set_Input_Delay(1);

                for (int i = 0; i < 9; ++i) {
                    strlcpy(diplomacy_list_choice[i], main_diplomacy_list[i], sizeof(diplomacy_list_choice[i]));
                }

                fonts::Set_Outline_Color(0, 0xF8);
                fonts::Set_Font_Style_Outline(4, &diplomacy_normal_colors);
                fonts::Set_Font_Highlight_Colors(&diplomacy_highlight_colors);
                fonts::Set_Font_Special_Colors(&diplomacy_special_colors);
                fonts::Set_Font_LF(4);

                Setup_Diplomacy_Help(9, main_diplomacy_help_entry);

                int8_t search_string = 0;

                int16_t choice = fields::Get_List_Field(
                    10, 0x76, 0xF5, main_diplomacy_title, (char*)diplomacy_list_choice, 50, &search_string, main_options, 0x11, nullptr, 0, nullptr, 0, 0);

                switch (choice) {
                    case 0:
                        *diplomacy_message = MOX::s__00551100[0];
                        Diplomacy_Propose_Treaty(MOX::local_player, current_ambassador);
                        break;
                    case 1:
                        *diplomacy_message = MOX::s__00551100[0];
                        Diplomacy_Break_Treaty(MOX::local_player, current_ambassador);
                        break;
                    case 2:
                        *diplomacy_message = MOX::s__00551100[0];
                        Diplomacy_Demand(MOX::local_player, current_ambassador);
                        break;
                    case 3:
                        *diplomacy_message = MOX::s__00551100[0];
                        Diplomacy_Offer_Gift(MOX::local_player, current_ambassador);
                        break;
                    case 4:
                        *diplomacy_message = MOX::s__00551100[0];
                        Diplomacy_Exchange_Technology(MOX::local_player, current_ambassador);
                        break;
                    case 5:
                        if (Diplomacy_Declare_War_Resolution(MOX::local_player, current_ambassador) == 1) {
                            exit_flag = true;
                            Diplomacy_Display_Response();
                        }
                        break;
                    case 6:
                        if (Diplomacy_Surrender(MOX::local_player, current_ambassador) == 1) {
                            exit_flag = true;
                        }
                        break;
                    case 7:
                    case -1:
                        exit_flag = true;
                        break;
                    default:
                        break;
                }
            }
        }

        fields::Assign_Auto_Function(Draw_Diplomacy_Synch_Mode, 2);
        fields::Set_Input_Delay(1);
    }

    void Diplomacy_Screen(game::id_player player) {
        if (!player_roster::Contains(game::current_state, player)) return;
        diplomacy_current_music = -1;
        synch_up_established_flag = 0;

        fields::Deactivate_Help_List();
        fields::Enable_Draw_All_Fields();
        graphics::Reset_Window();
        video::Clip_Off();
        mouse::Set_Mouse_List(&diplomacy_blank_mouse, 1);
        fields::Deactivate_Auto_Function();
        fields::Clear_Fields();

        old_response_message = -1;
        net_diplomacy_esc_pressed = 0;
        current_ambassador = player;

        ERIC::Set_Fade_Percent(0);
        ERIC::Fast_Fade_Out();

        fields::Set_Input_Delay(1);
        fields::Clear_Fields();
        video::Set_Refresh_Stencil();
        fields::Deactivate_Auto_Function();

        int16_t modifier = diplomac::Find_Worst_Modifier(MOX::local_player, current_ambassador);
        if (modifier <= -100 || diplodef::COUNCIL_FLAG == 2 || diplodef::COUNCIL_FLAG == 3) {
            response_message = 0x7E;
            if (game::current_state.players.at(MOX::local_player.id).treaty[current_ambassador.id] < 4) {
                response_message = 0x7F;
            }
            ambassador_option = 0;
        } else {
            response_message = 0x80;
            if (game::current_state.players.at(MOX::local_player.id).relations[current_ambassador.id] < -12) {
                response_message = 0x81;
            }
            ambassador_option = 1;
        }

        bool do_net_logic = false;
        if (game::current_state.players.at(current_ambassador.id).objectives == PLAYER_OBJECTIVE_HUMAN) {
            if (MOX::game_type != 0 && MOX::game_type != 1) {
                do_net_logic = true;
            }
        } else {
            if (MOX::game_type != 0 && MOX::game_type != 1) {
                ambassador_option = 1;
            }
        }

        if (do_net_logic) {
            if (NETMOX::net.is_diplomacy_initiator == 1) {
                talker_ambassador = MOX::local_player;
                listen_ambassador = NETMOX::net.diplomacy_target_id.get();
            } else {
                talker_ambassador = NETMOX::net.diplomacy_target_id.get();
                listen_ambassador = MOX::local_player;
            }
            ambassador_option = 1;
            net_diplomacy_chat_message[0] = MOX::s__00551100[0];
            Diplomacy_Fade_In();
            Get_Net_Diplomacy_Choices();
        } else {
            Diplomacy_Fade_In();
            if (ambassador_option == 1) {
                if (game::current_state.players.at(MOX::local_player.id).traits[0x13] == 1 ||
                    game::current_state.players.at(current_ambassador.id).traits[0x13] == 1) {
                    Get_Main_Repulsive_Diplomacy_Choices(MOX::local_player, current_ambassador, 0);
                } else {
                    Get_Main_Diplomacy_Choices();
                }
            } else {
                Diplomacy_Display_Response();
            }
        }

        JIM::jim_current_stream = -1;
        JIM::Play_Background_Music();
        fields::Deactivate_Auto_Function();
        fields::Clear_Fields();
        file_ani::Reset_File_Animation_Buffer();
        RUSS::Release_All_Blocks(MOX::screen_seg);
        ERIC::Set_Fade_Percent(0);
        ERIC::Fast_Fade_Out();
    }

    void Npc_Diplomacy_Screen(game::id_player player, int8_t is_sneak_attack) {
        const auto* ambassador = player_roster::Find(game::current_state, player);
        if (!ambassador || ambassador->eliminated == 1) {
            return;
        }

        fields::Deactivate_Help_List();
        fields::Enable_Draw_All_Fields();
        graphics::Reset_Window();
        video::Clip_Off();
        mouse::Set_Mouse_List(&diplomacy_blank_mouse, 1);
        MOX2::Get_Auto_Function(&EVENTS::jim_saved_autofunction);
        fields::Deactivate_Auto_Function();
        fields::Clear_Fields();
        current_ambassador = player;
        old_response_message = -1;
        ERIC::Set_Fade_Percent(0);
        ERIC::Fast_Fade_Out();
        fields::Set_Input_Delay(1);
        fields::Clear_Fields();
        video::Set_Refresh_Stencil();
        fields::Deactivate_Auto_Function();
        ambassador_option = 1;

        if (is_sneak_attack == 1) {
            response_message = (uint8_t)game::current_state.players.at(current_ambassador.id).sneak_attack_message;
        } else {
            response_message = (uint8_t)game::current_state.players.at(current_ambassador.id).diplomacy_message[MOX::Legacy_Local_Player_Slot()];
        }

        Diplomacy_Fade_In();

        int16_t original_response = response_message;

        if ((response_message >= 0x53 && response_message <= 0x55) ||
            (response_message >= 0x61 && response_message <= 0x68) ||
            response_message == 0x7d || response_message == 0x56 ||
            response_message == 0x69 || response_message == 0x6a ||
            (response_message >= 0x1c && response_message <= 0x24)) {
            response_message = 0x80;
            if (game::current_state.players.at(MOX::local_player.id).relations[current_ambassador.id] < -12) {
                response_message = 0x81;
            }
            ambassador_option = 2;
        }

        if (ambassador_option == 1) {
            Diplomacy_Display_Response();
            switch (response_message) {
                case 0x30:
                case 0x31:
                case 0x32:
                case 0x33:
                    diplomac::Break_Trade(MOX::local_player, current_ambassador);
                    diplomac::Break_Research(MOX::local_player, current_ambassador);
                    diplomac::Break_Tribute(MOX::local_player, current_ambassador);
                    diplomac::Break_Treaties(MOX::local_player, current_ambassador);
                    break;
                case 0x39:
                case 0x3a:
                    diplomac::Declare_War(MOX::local_player, current_ambassador);
                    break;
                case 0x60:
                case 0x6b:
                    Npc_Give_Gift(current_ambassador, MOX::local_player);
                    break;
            }
        } else {
            if (game::current_state.players.at(current_ambassador.id).objectives != PLAYER_OBJECTIVE_HUMAN) {
                Diplomacy_Display_Response();
            }
            response_message = original_response;

            if (Get_Non_Player_Proposals() == 0) {
                npcdiplo::NPC_Proposal_Rejection_Accept(current_ambassador, MOX::local_player, 1);
                switch (response_message) {
                    case 0x1c:
                    case 0x1d:
                    case 0x1e:
                    case 0x1f:
                    case 0x20:
                    case 0x21:
                    case 0x22:
                    case 0x23:
                    case 0x24:
                        response_message = 0x26;
                        Diplomacy_Display_Response();
                        Accepted_Npc_Demand_Resolution(MOX::local_player, current_ambassador);
                        Npc_Give_Gift(MOX::local_player, current_ambassador);
                        break;
                    case 0x53:
                        diplomac::Declare_Peace(MOX::local_player, current_ambassador);
                        break;
                    case 0x54:
                        Npc_Give_Gift(MOX::local_player, current_ambassador);
                        diplomac::Declare_Peace(MOX::local_player, current_ambassador);
                        Accepted_Npc_Demand_Resolution(MOX::local_player, current_ambassador);
                        break;
                    case 0x55:
                        Npc_Give_Gift(current_ambassador, MOX::local_player);
                        diplomac::Declare_Peace(MOX::local_player, current_ambassador);
                        break;
                    case 0x56:
                        diplomac::Declare_War(MOX::local_player, game::current_state.players.at(current_ambassador.id).diplomacy_proposal_war_player[MOX::Legacy_Local_Player_Slot()]);
                        response_message = 0x58;
                        Diplomacy_Display_Response();
                        break;
                    case 0x61:
                        diplomac::Start_Treaty(MOX::local_player, current_ambassador, 1);
                        break;
                    case 0x62:
                        diplomac::Start_Treaty(MOX::local_player, current_ambassador, 2);
                        break;
                    case 0x63:
                        diplomac::Start_Trade_Treaty(MOX::local_player, current_ambassador);
                        break;
                    case 0x64:
                        diplomac::Start_Research_Treaty(MOX::local_player, current_ambassador);
                        break;
                    case 0x66:
                        diplomac::Declare_War(MOX::local_player, game::current_state.players.at(current_ambassador.id).diplomacy_proposal_war_player[MOX::Legacy_Local_Player_Slot()]);
                        break;
                    case 0x67:
                        Npc_Give_Gift(current_ambassador, MOX::local_player);
                        diplomac::Declare_War(MOX::local_player, game::current_state.players.at(current_ambassador.id).diplomacy_proposal_war_player[MOX::Legacy_Local_Player_Slot()]);
                        break;
                    case 0x68:
                        diplomac::Break_Treaties(MOX::local_player, game::current_state.players.at(current_ambassador.id).diplomacy_proposal_war_player[MOX::Legacy_Local_Player_Slot()]);
                        break;
                    case 0x69:
                    case 0x6a:
                        response_message = 0x26;
                        Diplomacy_Display_Response();
                        Accepted_Npc_Demand_Resolution(MOX::local_player, current_ambassador);
                        Npc_Give_Gift(MOX::local_player, current_ambassador);
                        break;
                }
            } else {
                npcdiplo::NPC_Proposal_Rejection_Accept(current_ambassador, MOX::local_player, 0);
                switch (response_message) {
                    case 0x1c:
                    case 0x1d:
                    case 0x1e:
                    case 0x1f:
                    case 0x20:
                    case 0x21:
                    case 0x22:
                    case 0x23:
                    case 0x24:
                        diplomac::Break_Trade(MOX::local_player, current_ambassador);
                        diplomac::Break_Research(MOX::local_player, current_ambassador);
                        diplomac::Break_Tribute(MOX::local_player, current_ambassador);
                        diplomac::Break_Treaties(MOX::local_player, current_ambassador);
                        response_message = 0x25;
                        Diplomacy_Display_Response();
                        break;
                    case 0x56:
                        diplomac::Break_Treaties(MOX::local_player, current_ambassador);
                        response_message = 0x57;
                        Diplomacy_Display_Response();
                        break;
                    case 0x68:
                        diplomac::Break_Treaties(MOX::local_player, current_ambassador);
                        break;
                    case 0x69:
                        diplomac::Change_Relations(-0x32, current_ambassador, MOX::local_player, 0, 0, 0);
                        response_message = 0x25;
                        Diplomacy_Display_Response();
                        break;
                    case 0x6a:
                        if (game::current_state.players.at(current_ambassador.id).opportunity_planet_id[MOX::Legacy_Local_Player_Slot()] != game::NO_PLANET) {
                            game::current_state.players.at(current_ambassador.id).sneak_attack_planet = game::current_state.players.at(current_ambassador.id).opportunity_planet_id[MOX::Legacy_Local_Player_Slot()];
                            game::current_state.players.at(current_ambassador.id).sneak_attack_player = MOX::local_player;
                            game::current_state.players.at(current_ambassador.id).sneak_attack_message =
                                (uint8_t)game::current_state.players.at(current_ambassador.id).diplomacy_demand_rejection_message[MOX::Legacy_Local_Player_Slot()];
                        } else {
                            diplomac::Declare_War(current_ambassador, MOX::local_player);
                            response_message = 0xb3;
                            Diplomacy_Display_Response();
                        }
                        break;
                }
            }
        }

        sound::Fade_Stream_Down(10);
        fields::Deactivate_Auto_Function();
        MOX2::Put_Auto_Function(&EVENTS::jim_saved_autofunction);
        fields::Clear_Fields();
        file_ani::Reset_File_Animation_Buffer();
        RUSS::Release_All_Blocks(MOX::screen_seg);
        ERIC::Set_Fade_Percent(0);
        ERIC::Fast_Fade_Out();
        JIM::jim_current_stream = -1;
        JIM::Play_Background_Music();
    }

    void __cdecl Draw_Diplomacy_Synch_Mode() {
        video::Set_Page_Off();

        if (MOX::FULL_DRAW_SCREEN_FLAG == 1) {
            Setup_Back_Page();
            Setup_Ambassador_Pic(1);
        }

        if (ambassador_option != 0 && MOX::settings.animations_on == 1) {
            file_ani::Draw_File_Animation_Stencil();
            if (file_ani::Get_File_Animation_Frame() == 0) {
                file_ani::Set_File_Animation_Frame(1);
            }
        }

        fonts::Set_Outline_Color(0, 0xF8);
        fonts::Set_Font_Style_Outline(4, &diplomacy_normal_colors);

        if (old_response_message != (int16_t)response_message) {
            if (response_message < 1000) {
                old_response_message = (int16_t)response_message;
                Get_Diplomacy_Statement(response_message, MOX::local_player, current_ambassador, diplomacy_message, sizeof(diplomacy_message));
            } else {
                response_message = 0;
                old_response_message = 0;
            }
        }

        int16_t max_height = FMTPARA::Get_Formatted_Paragraph_Max_Height(0x1D6, diplomacy_message);
        int16_t y_pos = 0x1B8 - (max_height / 2);

        FMTPARA::Print_Formatted_Paragraph(0x50, y_pos, 0x1D6, diplomacy_message, 2);

        uint8_t c = static_cast<uint8_t>(max_height / 2);
        switch (game::current_state.players.at(current_ambassador.id).color) {
            case 0: c = 0xF3; break;
            case 1: c = 0xF0; break;
            case 2: c = 0xFD; break;
            case 3: c = 0xE3; break;
            case 4: c = 0xF5; break;
            case 5: c = 0xEB; break;
            case 6: c = 0xF4; break;
            case 7: c = 0xF1; break;
        }

        s_colors shadow_color;
        memset(&shadow_color, c, sizeof(s_colors));
        shadow_color.colors[0] = 0;

        fonts::Set_Outline_Color(0, 0);
        fonts::Set_Font_Style_Shadow_Down(4, &shadow_color);

        char local_str[sizeof(game::current_state.players.at(current_ambassador.id).name) + sizeof(receive_chat_message) + sizeof(" sez: ")];

        if (game::current_state.players.at(current_ambassador.id).objectives == PLAYER_OBJECTIVE_HUMAN) {
            strlcpy(local_str, player_number_name_buffer, sizeof(local_str));
            JIM::Decode_Text_Name_And_Stat(local_str, current_ambassador, legacy_world::Encode_Player_Slot(current_ambassador) + 1, 0x78);
        } else {
            strlcpy(local_str, player_name_ambassador_buffer, sizeof(local_str));
            JIM::Decode_Text_Race(local_str, current_ambassador, 0x78);
        }

        fonts::Print_Centered(0x140, 0xA, local_str);

        if (MOX::game_type != 0 && MOX::game_type != 1) {
            if (game::current_state.players.at(current_ambassador.id).objectives == PLAYER_OBJECTIVE_HUMAN) {
                if (receive_chat_message[0] != '\0') {
                    fonts::Set_Outline_Color(0xF8, 0xF8);
                    fonts::Set_Font_Style_Shadow_Down(4, &diplomacy_normal_colors);

                    snprintf(local_str, sizeof(local_str), "%s sez: %s", game::current_state.players.at(current_ambassador.id).name, receive_chat_message);
                    fonts::Print_Centered(0x140, 0x1D, local_str);
                }
            }
        }

        if (diplomacy_current_music != -1) {
            JIM::Play_Streaming_Music(diplomacy_current_music + 100, -1, 1);
        }

        RUSS::Mox_Update();

        if (net_diplomacy_esc_pressed == 1) {
            fields::Exit_List_Field();
        }

        fonts::Set_Outline_Color(0xF8, 0xF8);
    }

    void __cdecl Setup_Back_Page() {
        MOX::FULL_DRAW_SCREEN_FLAG = 0;

        int anim_index = (static_cast<uint8_t>(game::current_state.players.at(current_ambassador.id).race) * 2) + 13;
        file_ani::Open_File_Animation("DIPLOMAT.LBX", anim_index);

        video::Set_Page_Off();

        graphics::Fill(0, 0, 0x27F, 0x1DF, 0);

        file_ani::Draw_File_Animation();
        video::Copy_Off_To_Back();
    }

    void __cdecl Draw_Diplomacy_Fade_In_Screen() {
        video::Set_Page_Off();
        file_ani::Draw_File_Animation_Stencil();
        JIM::Play_Streaming_Music(diplomacy_current_music + 100, -1, 1);
    }

    void __cdecl Setup_Ambassador_Pic(int16_t frame_selector) {
        uint8_t race = static_cast<uint8_t>(game::current_state.players.at(current_ambassador.id).race);

        file_ani::Open_File_Animation(
            "DIPLOMAT.LBX",
            race * 2 + 14);

        file_ani::Set_File_Animation_Frame(frame_selector == 0 ? 1 : 0);

        if (MOX::settings.animations_on == 0) {
            file_ani::Draw_File_Animation_Stencil();
        }
    }

    void __cdecl Diplomacy_Set_Main_Options(game::id_player player, game::id_player target, uint8_t* options) {
        const auto player_id = player.id;
        const auto target_id = target.id;
        int32_t tech_list_buffer[TECH_APP_COUNT];
        int16_t diff_tech_count;

        if (game::current_state.players[player_id].treaty[target_id] < 4) {
            if (game::current_state.players[target_id].objectives == PLAYER_OBJECTIVE_HUMAN) {
                exchange_tech_count = 0;
                npcdiplo::Get_Differential_Tech_List(target, player, 0, 0xC343CB00, target_tech_list, tech_list_buffer, &diff_tech_count, 1);
                if (diff_tech_count > 0) {
                    npcdiplo::Get_Differential_Tech_List(player, target, 0, 0xC343CB00, target_tech_list, tech_list_buffer, &exchange_tech_count, 1);
                }
            }
            options[4] = 0;
            if (exchange_tech_count > 0 && noreport == 0) {
                options[4] = 1;
            }
        } else {
            options[1] = 0;
            options[5] = 0;
            options[2] = 0;
            options[4] = 0;
        }

        options[1] = 0;
        if (game::current_state.players[player_id].trade_treaty[target_id] == 1 ||
            game::current_state.players[player_id].research_treaty[target_id] == 1 ||
            game::current_state.players[player_id].tribute_treaty[target_id] > 0 ||
            game::current_state.players[player_id].treaty[target_id] == 1 ||
            game::current_state.players[player_id].treaty[target_id] == 2) {
            options[1] = 1;
        }

        if (diplomac::Valid_Treaty_Proposal(player, target, 7) == 0 &&
            diplomac::Valid_Treaty_Proposal(player, target, 8) == 0 &&
            diplomac::Valid_Treaty_Proposal(player, target, 3) == 0 &&
            diplomac::Valid_Treaty_Proposal(player, target, 1) == 0 &&
            diplomac::Valid_Treaty_Proposal(player, target, 2) == 0) {
            options[0] = 0;
        }

        options[6] = 0;
        if (COLCALC::May_Surrender_Empire(player, target)) {
            options[6] = 1;
        }
    }

    int16_t __cdecl Diplomacy_Generic_Exchange_Tech_List(game::id_player player, char* list_title, uint8_t* tech_ids, int16_t num_techs, int8_t mode) {
        int16_t page_size = (mode == 1) ? 8 : 11;

        fields::Clear_Fields();
        fields::Assign_Auto_Function(Draw_Diplomacy_Synch_Mode, 2);
        fields::Set_Input_Delay(1);
        fonts::Set_Font_Style(4, &diplomacy_normal_colors);
        fonts::Set_Font_Highlight_Colors(&diplomacy_highlight_colors);
        fonts::Set_Font_Special_Colors(&diplomacy_special_colors);
        fonts::Set_Font_LF(4);

        char str_prev[40];
        char str_next[40];
        char str_cancel[40];

        JIM::Get_Text_Message(s_jimtext_lbx, 76, str_prev, 40);
        JIM::Get_Text_Message(s_jimtext_lbx, 77, str_next, 40);
        JIM::Get_Text_Message(s_jimtext_lbx, (mode == 1) ? 70 : 46, str_cancel, 40);

        int16_t start_idx = 0;
        int16_t selected_idx = -1;
        bool loop_done = false;

        while (!loop_done) {
            int16_t btn_prev = -1;
            int16_t btn_next = -1;

            uint8_t list_highlight[16];
            memset(list_highlight, 1, 15);

            strlcpy(diplomacy_list_choice[0], str_prev, sizeof(diplomacy_list_choice[0]));

            int16_t item_values[16];
            item_values[0] = -1;

            int16_t has_prev = 0;
            if (start_idx != 0) {
                btn_prev = 0;
                has_prev = 1;
            }

            int16_t items_to_process;
            if (page_size + start_idx < num_techs) {
                items_to_process = page_size + has_prev;
                btn_next = items_to_process;
            } else {
                items_to_process = num_techs - start_idx;
            }

            if (items_to_process > 0) {
                for (int16_t i = 0; i < items_to_process; i++) {
                    int16_t target_idx = i + has_prev;
                    char* tech_name = TECH::Technology_Applications_Name(tech_ids[start_idx + i], player);
                    snprintf(diplomacy_list_choice[target_idx], sizeof(diplomacy_list_choice[target_idx]), "  %s", tech_name);
                    item_values[target_idx] = (uint8_t)tech_ids[start_idx + i];
                }
            }

            int16_t list_idx = items_to_process;

            if (btn_next != -1) {
                strlcpy(diplomacy_list_choice[list_idx], str_next, sizeof(diplomacy_list_choice[list_idx]));
                item_values[list_idx] = -1;
                list_idx++;
            }

            if (btn_prev == 0 && btn_next == -1) {
                list_idx++;
            }

            strlcpy(diplomacy_list_choice[list_idx], str_cancel, sizeof(diplomacy_list_choice[list_idx]));
            item_values[list_idx] = 0x265;
            diplomacy_list_choice[list_idx + 1][0] = s_empty_string_00551100[0];

            fonts::Set_Font_LF(4);
            Setup_Diplomacy_Help(list_idx + 2, item_values);

            int8_t selection_buffer = 0;

            int16_t action = fields::Get_List_Field(
                10, 118, 245, list_title, diplomacy_list_choice[0], 50, &selection_buffer, list_highlight, 17, nullptr, 0, nullptr, 0, 0);

            if (action == -1 || action == list_idx) {
                selected_idx = -1;
                loop_done = true;
            } else if (action == btn_prev) {
                start_idx -= page_size;
            } else if (action == btn_next) {
                start_idx += page_size;
            } else {
                loop_done = true;
                if (btn_prev == -1) {
                    selected_idx = action + start_idx;
                } else {
                    selected_idx = start_idx - 1 + action;
                }
            }
        }

        return selected_idx;
    }

    int16_t __cdecl Get_Non_Player_Proposals() {
        int16_t default_retval = 1;
        int16_t cancel_sentinel = -1000;
        uint8_t hotkey_flags[10];
        memset(hotkey_flags, 1, sizeof(hotkey_flags));

        int8_t list_pos = 0;
        char text_buffer[80];
        uint8_t tech_list[TECH_APP_COUNT];
        int32_t aiStack_350[TECH_APP_COUNT];
        int16_t target_system_id;
        int16_t result = -1;

        fields::Deactivate_Help_List();
        JIM::Get_Text_Message(s_jimtext_lbx, 0x4A, text_buffer, 80);

        switch (response_message) {
            case 0x1c:
            case 0x1d:
            case 0x1e:
            case 0x1f:
            case 0x20:
            case 0x21:
            case 0x22:
            case 0x23:
            case 0x24:
            case 0x53:
            case 0x54:
            case 0x55:
            case 0x56:
            case 0x61:
            case 0x62:
            case 99:
            case 100:
            case 0x66:
            case 0x67:
            case 0x68:
            case 0x69:
            case 0x6a:
            case 0x7d:
                JIM::Get_Text_Message(s_jimtext_lbx, 0x4F, diplomacy_list_choice[0], 50);
                JIM::Get_Text_Message(s_jimtext_lbx, 0x50, diplomacy_list_choice[1], 50);
                diplomacy_list_choice[2][0] = '\0';
                break;
            default:
                break;
        }

        fields::Assign_Auto_Function(Draw_Diplomacy_Synch_Mode, 2);
        fields::Set_Input_Delay(1);

        fonts::Set_Font_Style(4, &diplomacy_normal_colors);
        fonts::Set_Font_Highlight_Colors(&diplomacy_highlight_colors);
        fonts::Set_Font_Special_Colors(&diplomacy_special_colors);
        fonts::Set_Font_LF(4);
        Setup_Full_Screen_Diplomacy_Help(0x2b6);

        if (response_message == 0x65) {
            default_retval = -1;
            cancel_sentinel = -1;

            npcdiplo::Get_Differential_Tech_List(
                MOX::local_player,
                current_ambassador,
                0,
                game::current_state.players.at(current_ambassador.id).diplomacy_proposal_exchange_max_value[MOX::Legacy_Local_Player_Slot()],
                tech_list,
                aiStack_350,
                &target_system_id,
                1);

            JIM::Get_Text_Message(s_jimtext_lbx, 0x51, text_buffer, 80);

            result = Diplomacy_Generic_Exchange_Tech_List(
                current_ambassador,
                text_buffer,
                tech_list,
                target_system_id,
                1);

            if (result != -1) {
                COLCALC::Player_Gets_Tech_App(
                    &game::current_state.players.at(MOX::local_player.id),
                    tech_list[result]);
                COLCALC::Player_Gets_Tech_App(
                    &game::current_state.players.at(current_ambassador.id),
                    game::current_state.players.at(current_ambassador.id).diplomacy_proposal_exchange_tech[MOX::Legacy_Local_Player_Slot()]);
            }
        } else if (response_message == 0x7d) {
            fields::Clear_Fields();
            fields::Add_Hidden_Field(0, 0, 0x27F, 0x1DF, s_empty_string, 0);
            result = fields::Get_Input();
            while (result == 0) {
                timer::Mark_Time();
                Draw_Diplomacy_Synch_Mode();
                RUSS::Mox_Sync_Update();
                timer::Release_Time(2);
                result = fields::Get_Input();
            }
            result = 1;
        } else {
            result = fields::Get_List_Field(
                10, 0x76, 0xF5, text_buffer, (char*)diplomacy_list_choice, 50, &list_pos, hotkey_flags, 0x11, nullptr, 0, nullptr, 0, 0);
        }

        if (result == -1) {
            result = default_retval;
        }

        if (cancel_sentinel == result) {
            result = -1;
        }

        return result;
    }

    void __cdecl Accepted_Npc_Demand_Resolution(game::id_player player, game::id_player ambassador) {
        int16_t relation_change = game_random::Random(13) + 11;
        diplomac::Change_Relations(relation_change, player, ambassador, 0, 0, 0);

        game::current_state.players[player.id].threats[ambassador.id] = '\0';
        game::current_state.players[ambassador.id].threats[player.id] = '\0';
    }

    void __cdecl Setup_Diplomacy_Help(int16_t count, int16_t* help_ids) {
        fields::Deactivate_Help_List();

        if (MOX::game_type == 0 || MOX::game_type == 1 ||
            game::current_state.players.at(current_ambassador.id).objectives != PLAYER_OBJECTIVE_HUMAN) {
            count--;
            int16_t font_h = fonts::Get_Font_Height();
            int16_t step = font_h + fonts::Get_Font_Vertical_Spacing();
            int16_t y = step + 0x76;

            for (int16_t i = 0; i < count; i++) {
                jim_help_list[i].x1 = 10;
                jim_help_list[i].y1 = y;
                jim_help_list[i].x2 = 0xFF;
                jim_help_list[i].y2 = y + fonts::Get_Font_Height();
                jim_help_list[i].help_id = help_ids[i];
                y += step;
            }

            jim_help_list[count].x1 = 0;
            jim_help_list[count].y1 = 0;
            jim_help_list[count].x2 = 0x27F;
            jim_help_list[count].y2 = 0x1DF;
            jim_help_list[count].help_id = 0x2B6;

            fields::Set_Help_List(jim_help_list, count + 1);
        }
    }

    void __cdecl Diplomacy_Fade_In() {
        Diplomacy_Mouse_Init(current_ambassador);
        Get_Diplomacy_Statement(response_message, MOX::local_player, current_ambassador, diplomacy_message, sizeof(diplomacy_message));
        Setup_Back_Page();
        mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);
        if (ambassador_option != 0) {
            uint16_t skip_anim = 0;
            fields::Assign_Auto_Function(Draw_Diplomacy_Fade_In_Screen, 2);
            file_ani::Set_File_Animation_Frame(1);
            if (MOX::settings.animations_on == 0) {
                skip_anim = 1;
            }
            for (int16_t i = 0; i < 38; i++) {
                if (skip_anim != 0) {
                    break;
                }
                timer::Mark_Time();
                fields::Clear_Fields();
                int16_t hidden_field = fields::Add_Hidden_Field(0, 0, 639, 479, s_empty_string_00551100, 0);
                int16_t input = fields::Get_Input();
                if (hidden_field == input || key::Keyboard_Status()) {
                    skip_anim = 1;
                }
                if (skip_anim == 0) {
                    Draw_Diplomacy_Fade_In_Screen();
                    RUSS::Mox_Sync_Update();
                    timer::Release_Time(2);
                }
            }
            Setup_Ambassador_Pic(skip_anim);
        }
    }

    void __cdecl Setup_Full_Screen_Diplomacy_Help(int16_t help_id) {
        fields::Deactivate_Help_List();

        if (MOX::game_type == 0 ||
            MOX::game_type == 1 ||
            game::current_state.players.at(current_ambassador.id).objectives != PLAYER_OBJECTIVE_HUMAN) {
            jim_help_list[0].x2 = 0x27F;
            jim_help_list[0].x1 = 0;
            jim_help_list[0].y1 = 0;
            jim_help_list[0].y2 = 0x1DF;
            jim_help_list[0].help_id = help_id;

            fields::Set_Help_List(jim_help_list, 1);
        }
    }

    void __cdecl Diplomacy_Propose_Treaty(game::id_player player, game::id_player target) {
        const auto player_idx = player.id;
        const auto target_idx = target.id;
        uint8_t valid_treaties[7];
        int8_t hotkeys = 0;

        valid_treaties[0] = 1;
        valid_treaties[1] = 1;
        valid_treaties[2] = 1;
        valid_treaties[3] = 1;
        valid_treaties[4] = 1;
        valid_treaties[5] = 1;
        valid_treaties[6] = 1;

        valid_treaties[0] = static_cast<uint8_t>(diplomac::Valid_Treaty_Proposal(player, target, 7));
        valid_treaties[1] = static_cast<uint8_t>(diplomac::Valid_Treaty_Proposal(player, target, 8));
        valid_treaties[2] = static_cast<uint8_t>(diplomac::Valid_Treaty_Proposal(player, target, 3));
        valid_treaties[3] = static_cast<uint8_t>(diplomac::Valid_Treaty_Proposal(player, target, 1));
        valid_treaties[4] = static_cast<uint8_t>(diplomac::Valid_Treaty_Proposal(player, target, 2));

        fields::Assign_Auto_Function(Draw_Diplomacy_Synch_Mode, 2);
        fields::Set_Input_Delay(1);

        for (int16_t i = 0; i < 7; ++i) {
            strlcpy(diplomacy_list_choice[i], propose_treaty_diplomacy_list[i], sizeof(diplomacy_list_choice[i]));
        }

        fonts::Set_Font_Style(4, &diplomacy_normal_colors);
        fonts::Set_Font_Highlight_Colors(&diplomacy_highlight_colors);
        fonts::Set_Font_Special_Colors(&diplomacy_special_colors);
        fonts::Set_Font_LF(4);

        Setup_Diplomacy_Help(7, propose_treaty_help_entry);

        int16_t treaty_status = 0;

        int16_t choice = fields::Get_List_Field(
            10, 0x76, 0xf5, propose_treaty_title, reinterpret_cast<char*>(diplomacy_list_choice), 50, &hotkeys, valid_treaties, 0x11, nullptr, 0, nullptr, 0, 0);

        switch (choice) {
            case 0:
                old_response_message = -1;
                treaty_status = Diplomacy_Determine_Treaty_Proposal(player, target, 7);
                diplomac::Adjust_Diplomat_Modifiers(player, target);
                game::current_state.players[player_idx].trade_modifier[target_idx] -= 20;
                if (treaty_status != 1) {
                    return;
                }
                diplomac::Start_Trade_Treaty(player, target);
                break;
            case 1:
                old_response_message = -1;
                treaty_status = Diplomacy_Determine_Treaty_Proposal(player, target, 8);
                diplomac::Adjust_Diplomat_Modifiers(player, target);
                game::current_state.players[player_idx].trade_modifier[target_idx] -= 20;
                if (treaty_status != 1) {
                    return;
                }
                diplomac::Start_Research_Treaty(player, target);
                break;
            case 2:
                old_response_message = -1;
                treaty_status = Diplomacy_Determine_Treaty_Proposal(player, target, 3);
                diplomac::Adjust_Diplomat_Modifiers(player, target);
                game::current_state.players[player_idx].peace_modifier[target_idx] -= 20;
                if (treaty_status != 1) {
                    return;
                }
                diplomac::Declare_Peace(player, target);
                break;
            case 3:
                old_response_message = -1;
                treaty_status = Diplomacy_Determine_Treaty_Proposal(player, target, 1);
                diplomac::Adjust_Diplomat_Modifiers(player, target);
                game::current_state.players[player_idx].treaty_modifier[target_idx] -= 20;
                if (treaty_status != 1) {
                    return;
                }
                diplomac::Start_Treaty(player, target, 1);
                break;
            case 4:
                old_response_message = -1;
                treaty_status = Diplomacy_Determine_Treaty_Proposal(player, target, 2);
                diplomac::Adjust_Diplomat_Modifiers(player, target);
                game::current_state.players[player_idx].treaty_modifier[target_idx] -= 20;
                if (treaty_status != 1) {
                    return;
                }
                diplomac::Start_Treaty(player, target, 2);
                break;
            default:
                break;
        }

        if (treaty_status == 1) {
            if (game::current_state.players[target_idx].objectives != PLAYER_OBJECTIVE_HUMAN) {
                npcdiplo::Get_Exchange_Tech_List(
                    player,
                    target,
                    &exchange_tech_count,
                    target_tech_list,
                    exchange_tech_list);
            }
        }
    }

    void __cdecl Get_Main_Repulsive_Diplomacy_Choices(game::id_player speaker, game::id_player listener, int8_t net_mode) {
        const auto speaker_player_id = speaker.id;
        const auto listener_player_id = listener.id;
        int16_t done = 0;

        while (true) {
            uint8_t flags[5];
            flags[0] = 1;
            flags[1] = 1;
            flags[2] = 1;
            flags[3] = 1;
            flags[4] = 1;

            if (game::current_state.players[speaker_player_id].treaty[listener_player_id] < 4) {
                flags[0] = 0;
            } else {
                flags[1] = 0;
            }

            flags[2] = 0;
            if (COLCALC::May_Surrender_Empire(speaker, listener)) {
                flags[2] = 1;
            }

            if (net_mode == 0 && diplomac::Find_Worst_Modifier(speaker, listener) < -99) {
                if (old_response_message != response_message) {
                    Diplomacy_Display_Response();
                }
                response_message = 0x82;
                Diplomacy_Display_Response();
                done = 1;
            } else {
                if (net_diplomacy_esc_pressed == 1) {
                    done = 1;
                }

                fields::Assign_Auto_Function(Draw_Diplomacy_Synch_Mode, 2);
                fields::Set_Input_Delay(1);
                fonts::Set_Outline_Color(0, 0xF8);
                fonts::Set_Font_Style_Outline(4, &diplomacy_normal_colors);
                fonts::Set_Font_Highlight_Colors(&diplomacy_highlight_colors);
                fonts::Set_Font_Special_Colors(&diplomacy_special_colors);
                fonts::Set_Font_LF(4);

                int16_t list_size = 0;
                int16_t* help_entry = nullptr;

                if (net_mode == 1) {
                    for (int i = 0; i < 6; ++i) {
                        strlcpy(diplomacy_list_choice[i], main_net_repulsive_diplomacy_list[i], sizeof(diplomacy_list_choice[i]));
                    }
                    help_entry = main_net_repulsive_diplomacy_help_entry;
                    list_size = 6;
                } else {
                    for (int i = 0; i < 5; ++i) {
                        strlcpy(diplomacy_list_choice[i], main_repulsive_diplomacy_list[i], sizeof(diplomacy_list_choice[i]));
                    }
                    help_entry = main_repulsive_diplomacy_help_entry;
                    list_size = 5;
                }

                Setup_Diplomacy_Help(list_size, help_entry);

                int8_t list_pos = 0;
                int16_t choice = fields::Get_List_Field(
                    10, 0x76, 0xF5, main_diplomacy_title, diplomacy_list_choice[0], 0x32, &list_pos, flags, 0x11, nullptr, 0, nullptr, 0, 0);

                if (net_mode == 1 && choice == 3) {
                    choice = 5;
                }

                switch (choice) {
                    case 0: {
                        *diplomacy_message = s_empty_string_00551100[0];
                        old_response_message = -1;
                        int16_t proposal = Diplomacy_Determine_Treaty_Proposal(speaker, listener, 3);
                        diplomac::Adjust_Diplomat_Modifiers(speaker, listener);
                        if (proposal == 1) {
                            diplomac::Declare_Peace(speaker, listener);
                        }
                        break;
                    }
                    case 1:
                        if (Diplomacy_Declare_War_Resolution(speaker, listener) == 1) {
                            done = 1;
                            Diplomacy_Display_Response();
                            net_diplomacy_esc_pressed = 1;
                        }
                        break;
                    case 2:
                        if (Diplomacy_Surrender(speaker, listener) == 1) {
                            done = 1;
                            net_diplomacy_esc_pressed = 1;
                        }
                        break;
                    case 5:
                        Diplomacy_Main_Net_Chat(listener);
                        if (net_diplomacy_esc_pressed == 1) {
                            done = 1;
                        }
                        break;
                    case -1:
                    case 3:
                    case 4:
                        done = 1;
                        if (net_mode == 1) {
                            net_diplomacy_message.sender_player_id = static_cast<uint8_t>(speaker_player_id);
                            net_diplomacy_message.target_player_id = static_cast<uint8_t>(listener_player_id);
                            net_diplomacy_esc_pressed = 1;
                            net_diplomacy_message.arg2 = -1;
                            net_diplomacy_message.msg_type = 0x82;

                            NETMOX::Mox_Send_Message(
                                game::current_state.players[speaker_player_id].network_player_id, DIP_NET_PACKET_MESSAGE, (uint8_t*)&net_diplomacy_message, 8);
                            RUSS::Mox_Update();
                        }
                        break;
                }
            }

            if (done != 0) {
                fields::Assign_Auto_Function(Draw_Diplomacy_Synch_Mode, 2);
                fields::Set_Input_Delay(1);
                return;
            }
        }
    }

    void __cdecl Npc_Give_Gift(game::id_player player, game::id_player target) {
        const auto player_idx = player.id;
        const auto target_idx = target.id;
        if (game::current_state.players[player_idx].diplomacy_proposal_tribute_bribe[target_idx] != 0) {
            if (game::current_state.players[player_idx].diplomacy_proposal_tribute_bribe[target_idx] == 1) {
                diplomac::Start_Tribute_Treaty(player, target, 1);
            } else {
                diplomac::Start_Tribute_Treaty(player, target, 2);
            }
            return;
        }

        if (game::current_state.players[player_idx].diplomacy_proposal_tech_bribe1[target_idx] != 0) {
            COLCALC::Player_Gets_Tech_App(
                &game::current_state.players[target_idx],
                game::current_state.players[player_idx].diplomacy_proposal_tech_bribe1[target_idx]);
            return;
        }

        if (game::current_state.players[player_idx].diplomacy_proposal_gold_bribe[target_idx] != 0) {
            game::current_state.players[target_idx].bc += game::current_state.players[player_idx].diplomacy_proposal_gold_bribe[target_idx];
            game::current_state.players[player_idx].bc -= game::current_state.players[player_idx].diplomacy_proposal_gold_bribe[target_idx];

            if (game::current_state.players[player_idx].bc < 1) {
                game::current_state.players[player_idx].bc = 0;
            }
            return;
        }

        if (const auto star = game::current_state.players[player_idx].diplomacy_proposal_system_bribe[target_idx]; star != game::NO_STAR) {
            if (COLCALC::May_Surrender_Star(game::current_state, star, player, target)) {
                COLCALC::Surrender_Star(game::current_state, star, player, target);
            }
        }
    }
} // namespace DIP_SCRN
