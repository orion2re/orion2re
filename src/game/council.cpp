#include "pch.h"
#include "string_overrides.h"

namespace council {
    const char lbx_00559a68[12] = "council.lbx";
    const char LBX_00559a74[12] = "council.LBX";

    s_mouse_list council_blank_mouse = {0, 0, 0, 0, 639, 479};
    s_colors council_normal_colors = {{150, 154, 155, 151, 152, 153, 154, 155}};
    s_colors council_highlight_colors = {{150, 155, 155, 152, 153, 155, 155, 155}};
    uint8_t council_nominee_[2];
    int16_t council_total_votes;
    int16_t council_nominee_vote_total[2];
    uint8_t council_members[MAX_PLAYERS];
    uint8_t council_state;
    char* council_vote_message;
    s_animation_header* council_dark_race_seg[MAX_PLAYERS];
    int8_t council_voter;
    char* council_no_votes_message;
    int16_t council_vote_cast;
    int16_t council_member_votes[MAX_PLAYERS];
    s_animation_header* council_light_race_seg[MAX_PLAYERS];
    char* council_message;
    uint8_t player_voted_winner_message_id[20];
    char* council_votes_message;
    int16_t council_nominee_votes[2];
    void* council_mouse_bitmap;
    uint8_t winning_player;
    s_animation_header* council_nominees_seg[2];
    uint8_t council_total_members;
    char* council_votes_total_message;
    uint8_t council_vote_for;

    void Check_For_Council_Meeting() {
        int16_t active_players = 0;
        for (int16_t i = 0; i < MOX::NUM_PLAYERS; ++i) {
            if (MOX::player[i].eliminated == 0) {
                active_players++;
            }
        }

        int16_t owned_stars = 0;
        for (int16_t i = 0; i < game::Current_Star_Count(); ++i) {
            if (game::current_state.stars.at(legacy_world::Star_Id_At_Position(i)).owner != -1) {
                owned_stars++;
            }
        }

        int32_t current_turn_offset = MOX::stardate - 35000;
        if (current_turn_offset < meeting_schedule.first_meeting_delay) {
            return;
        }

        if ((int32_t)diplodef::next_council_meeting_turn > current_turn_offset &&
            diplodef::COUNCIL_FLAG != 0) {
            return;
        }

        if (active_players <= 2) {
            return;
        }

        int32_t num_stars = game::Current_Star_Count();
        int16_t half_stars = (int16_t)((num_stars - (num_stars >> 31)) >> 1);
        if (owned_stars < half_stars) {
            return;
        }

        council::Main_Council_Screen();
        diplodef::next_council_meeting_turn = (int16_t)(current_turn_offset + meeting_schedule.meeting_interval);
    }

    void __cdecl Control_Main_Council_Screen() {
        int16_t field_id;
        int16_t input_id;
        int16_t done = 0;

        fields::Assign_Auto_Function(Draw_Main_Council_Screen, 2);

        while (done == 0) {
            timer::Mark_Time();
            fields::Clear_Fields();

            field_id = fields::Add_Hidden_Field(0, 0, 639, 479, "", 0);
            input_id = fields::Get_Input();

            if (field_id == input_id) {
                done = 1;
            }

            if (done == 0) {
                Draw_Main_Council_Screen();
                RUSS::Mox_Sync_Update();
                timer::Release_Time(2);
            }
        }

        fields::Clear_Fields();
        fields::Deactivate_Auto_Function();
    }

    void __cdecl Main_Council_Screen() {
        fields::Deactivate_Help_List();
        fields::Enable_Draw_All_Fields();
        graphics::Reset_Window();
        video::Clip_Off();
        mouse::Set_Mouse_List(&council::council_blank_mouse, 1);
        JIM::Play_Streaming_Music(0x77 /* MUSIC_COUNCIL */, -1, 1);
        ERIC::Set_Fade_Percent(0);
        ERIC::Fast_Fade_Out();
        fields::Set_Input_Delay(1);
        fields::Clear_Fields();
        video::Set_Refresh_Stencil();
        MOX2::Get_Auto_Function(&EVENTS::jim_saved_autofunction);
        fields::Deactivate_Auto_Function();
        fields::Clear_Fields();
        council::Reload_Main_Council_Screen();
        council::council_state = 0;
        diplodef::COUNCIL_FLAG = 1;
        council::council_nominee_vote_total[0] = 0;
        council::council_nominee_vote_total[1] = 0;
        fields::Assign_Auto_Function(council::Draw_Main_Council_Screen, 2);

        while (council::council_state != 0x63 /* 'c' */) {
            council::Get_Council_Message();
            council::Control_Main_Council_Screen();
            council::Next_Council_State();
            if (council::council_state == 2) {
                council::Council_Votes();
                council::Council_Finished_Voting();
                council::Council_Accept_Ruling();
            }
        }

        fields::Deactivate_Auto_Function();
        fields::Clear_Fields();
        REPORT::something_interesting_happened[MOX::PLAYER_NUM] = 1;
        file_ani::Reset_File_Animation_Buffer();
        JIM::Play_Background_Music();
        ERIC::Set_Fade_Percent(0);
        ERIC::Fast_Fade_Out();
        RUSS::Release_All_Blocks(MOX::screen_seg);
        MOX2::Put_Auto_Function(&EVENTS::jim_saved_autofunction);
        MISC::Fade_Into_Screen(MAINSCR::Draw_Main_Screen, &MOX::draw_main_screen_to_back, 0);
    }

    void __cdecl Draw_Main_Council_Screen() {
        int16_t x_coords[6] = {44, 520, 143, 444, 215, 377};
        int16_t y_coords[6] = {96, 100, 148, 149, 169, 168};
        char buffer[40];

        video::Set_Page_Off();
        fonts::Set_Outline_Color(0, 0);

        if (MOX::settings.animations_on == 1) {
            file_ani::Draw_Multi_File_Animation(0);
            if (file_ani::Get_Multi_File_Animation_Frame(0) == 0) {
                file_ani::Set_Multi_File_Animation_Frame(0, 1);
            }
        }

        fonts::Set_Outline_Color(0, 0);
        fonts::Set_Font_Style_Outline(2, &council::council_normal_colors);

        int16_t print_x = 30;
        for (int i = 0; i < 2; ++i) {
            if (council::council_nominee_vote_total[i] < 1) {
                strlcpy(buffer, council::council_no_votes_message, sizeof(buffer));
                JIM::Decode_Text_Race(buffer, council::council_nominee_[i], sizeof(buffer));
            } else {
                char* msg = (council::council_nominee_vote_total[i] < 2) ? council::council_vote_message : council::council_votes_message;
                strlcpy(buffer, msg, sizeof(buffer));
                JIM::Decode_Text_Name_And_Stat(buffer, council::council_nominee_[i], council::council_nominee_vote_total[i], sizeof(buffer));
            }
            fonts::Print(print_x, 30, buffer);
            print_x += 421;
        }

        snprintf(buffer, sizeof(buffer), "%d %s", (int)council::council_total_votes, council::council_votes_total_message);
        fonts::Print_Centered(320, 30, buffer);

        for (int i = 0; i < council::council_total_members; ++i) {
            s_animation_header* anim;
            if ((council::council_state == 2 || council::council_state == 3) && council::council_members[i] == council::council_voter) {
                anim = council::council_light_race_seg[i];
            } else {
                anim = council::council_dark_race_seg[i];
            }
            animate::Draw(x_coords[i], y_coords[i], anim);
        }

        int16_t nominee_x = 115;
        for (int i = 0; i < 2; ++i) {
            int16_t frame = animate::Get_Current_Frame(council::council_nominees_seg[i]);
            animate::Draw(nominee_x, 225, council::council_nominees_seg[i]);
            if (council::council_nominee_[i] != council::council_voter) {
                animate::Set_Animation_Frame(council::council_nominees_seg[i], frame);
            }
            nominee_x += 205;
        }

        fonts::Set_Outline_Color(0, 0);
        fonts::Set_Font_Style_Outline(4, &council::council_normal_colors);

        int16_t msg_h = BILL::_Get_Paragraph_Max_Height(550, council::council_message);
        BILL::_Print_Paragraph(55, 470 - msg_h, 550, council::council_message, 2);

        JIM::Play_Streaming_Music(119 /* 0x77 */, -1, 1);
        RUSS::Mox_Update();
    }

    void Council_Player_Vote() {
        char list_items[4][25];
        char title_msg[48];
        int16_t vote_choice = 0;

        int8_t list_metadata;
        uint8_t active_mask[4];

        fields::Assign_Auto_Function(council::Draw_Main_Council_Screen, 2);

        council::council_vote_cast = council::Get_Num_Votes((uint8_t)MOX::PLAYER_NUM);
        council::council_voter = (uint8_t)MOX::PLAYER_NUM;

        for (int i = 0; i < 4; ++i) {
            list_items[i][0] = '\0';
        }

        JIM::Get_Text_Message("jimtext.lbx", 23 /* COUNCIL_VOTE_CAST_INFO */, title_msg, sizeof(title_msg));
        JIM::Decode_Text_Stat(title_msg, council::council_vote_cast, sizeof(title_msg));

        if (MOX::PLAYER_NUM == council::council_nominee_[0]) {
            JIM::Get_Text_Message("jimtext.lbx", 24 /* ME */, list_items[0], sizeof(list_items[0]));
        } else {
            snprintf(list_items[0], sizeof(list_items[0]), "  %s", MOX::player[council::council_nominee_[0]].name);
        }

        if (MOX::PLAYER_NUM == council::council_nominee_[1]) {
            JIM::Get_Text_Message("jimtext.lbx", 24 /* ME */, list_items[1], sizeof(list_items[1]));
        } else {
            snprintf(list_items[1], sizeof(list_items[1]), "  %s", MOX::player[council::council_nominee_[1]].name);
        }

        JIM::Get_Text_Message("jimtext.lbx", 25 /* ABSTAIN */, list_items[2], sizeof(list_items[2]));

        active_mask[0] = 1;
        active_mask[1] = 1;
        active_mask[2] = 1;
        active_mask[3] = 1;
        fonts::Set_Outline_Color(0, 0);
        fonts::Set_Font_Style(4, &council::council_normal_colors);
        fonts::Set_Font_Highlight_Colors(&council::council_highlight_colors);
        fonts::Set_Font_LF(4);

        council::council_message[0] = '\0';
        list_metadata = 0;

        int16_t selection = fields::Get_List_Field(
            214, 388, 180, title_msg, (char*)list_items, 25, &list_metadata, active_mask, 17, nullptr, 0, nullptr, 0, 0);

        if (selection == 0) {
            council::winning_player = council::council_nominee_[0];
            vote_choice = 1;
            MOX::player[council::council_voter].council_voted_for = council::council_nominee_[0];
        } else if (selection == 1) {
            council::winning_player = council::council_nominee_[1];
            vote_choice = 2;
            MOX::player[council::council_voter].council_voted_for = council::council_nominee_[1];
        } else if (selection == 2) {
            vote_choice = 0;
            MOX::player[council::council_voter].council_voted_for = -1;
        } else {
            vote_choice = 0;
        }

        council::Council_Vote_Result((int8_t)vote_choice, (uint8_t)MOX::PLAYER_NUM);
        fields::Clear_Fields();
        fields::Deactivate_Auto_Function();
        council::council_voter = -1;
    }

    void __cdecl Council_Vote_Result(int8_t vote_choice, uint8_t player_idx) {
        uint8_t nominee_id;

        if (vote_choice == 0) {
            council::council_state = 3;
            diplomac::Change_Relations(-6, player_idx, council::council_nominee_[1], 0, 0, 0);
            diplomac::Change_Relations(-6, player_idx, council::council_nominee_[0], 0, 0, 0);
            MOX::player[player_idx].council_voted_for = -1;
        } else if (vote_choice == 1) {
            council::council_nominee_vote_total[0] += council::council_vote_cast;
            council::council_state = 2;
            council::council_vote_for = 0;
            diplomac::Change_Relations(24, player_idx, council::council_nominee_[0], 16, 0, 0);
            diplomac::Change_Relations(-12, player_idx, council::council_nominee_[1], 9, 0, 0);
            nominee_id = council::council_nominee_[0];
            MOX::player[player_idx].council_voted_for = nominee_id;
        } else if (vote_choice == 2) {
            council::council_nominee_vote_total[1] += council::council_vote_cast;
            council::council_state = 2;
            council::council_vote_for = 1;
            diplomac::Change_Relations(24, player_idx, council::council_nominee_[1], 16, 0, 0);
            diplomac::Change_Relations(-12, player_idx, council::council_nominee_[0], 9, 0, 0);
            nominee_id = council::council_nominee_[1];
            MOX::player[player_idx].council_voted_for = nominee_id;
        }

        council::Get_Council_Message();
        council::Control_Main_Council_Screen();
    }

    int16_t __cdecl Get_Num_Votes(uint8_t player_idx) {
        int16_t total_pop = MOX::player[player_idx].total_pop;

        if ((total_pop % 10) == 0) {
            return total_pop / 10;
        }

        return (total_pop / 10) + 1;
    }

    void __cdecl Next_Council_State() {
        switch (council::council_state) {
            case 0:
                council::council_state = 1;
                break;
            case 1:
                council::council_state = 2;
                break;
            case 2:
            case 3:
                /* No operation for these states. */
                break;
            case 4:
            case 5:
            case 6:
                council::council_state = 99 /* 0x63 */;
                break;
            default:
                break;
        }
    }

    void __cdecl Council_Votes() {
        if (council_total_members != 0) {
            for (int i = 0; i < (int)council_total_members; ++i) {
                uint8_t member_id = council_members[i];

                if ((int16_t)member_id == MOX::PLAYER_NUM) {
                    continue;
                }

                uint8_t nominee0 = council_nominee_[0];
                uint8_t nominee1 = council_nominee_[1];

                if (member_id == nominee0 || member_id == nominee1) {
                    continue;
                }

                int16_t vote0 = Vote_Check(nominee0, member_id, nominee1);
                int16_t vote1 = Vote_Check(nominee1, member_id, nominee0);

                int16_t vote_choice;
                if (vote0 == 1) {
                    /* If member votes for both nominees, they abstain. */
                    vote_choice = (vote1 == 1) ? 0 : 1;
                } else {
                    /* If member supports only nominee 1, cast the vote for nominee 1. */
                    vote_choice = (vote1 == 1) ? 2 : 0;
                }

                council_vote_cast = council_member_votes[i];
                council_voter = member_id;
                Council_Vote_Result((int8_t)vote_choice, member_id);
            }
        }

        /* Process automatic votes for nominees if they are not the player. */
        if ((int16_t)council_nominee_[0] != MOX::PLAYER_NUM) {
            int16_t votes = council_nominee_votes[0];
            int16_t vote_choice = (votes >= 1) ? 1 : 0;

            council_vote_cast = votes;
            council_voter = council_nominee_[0];
            Council_Vote_Result((int8_t)vote_choice, council_nominee_[0]);
        }

        if ((int16_t)council_nominee_[1] != MOX::PLAYER_NUM) {
            int16_t votes = council_nominee_votes[1];
            int16_t vote_choice = (votes >= 1) ? 2 : 0;

            council_vote_cast = votes;
            council_voter = council_nominee_[1];
            Council_Vote_Result((int8_t)vote_choice, council_nominee_[1]);
        }

        Council_Player_Vote();
    }

    void __cdecl Reload_Main_Council_Screen() {
        s_animation_header* mouse_h;
        int16_t max_votes;
        int16_t cur_votes;
        int16_t player_idx;

        council_mouse_bitmap = allocate::Allocate_First_Block(MOX::screen_seg, 917 /* 0x395 */);
        mouse_h = (s_animation_header*)farload::Far_Reload_Next(council::LBX_00559a74, 0, MOX::screen_seg);
        bitmap::Clear_Mouse_Image(0);
        bitmap::Create_Blank_Bitmap(30, 30, council_mouse_bitmap);
        bitmap::Draw_Picture_To_Bitmap((s_animation_header*)council_mouse_bitmap, mouse_h);
        video::Set_Page_Off();
        animate::Draw_Palette(mouse_h);
        palette::Set_Palette_Changes(0, 255);
        graphics::Fill(0, 0, 639, 479, 0);
        bitmap::Draw_Bitmap_To_Mouse(0, 0, (s_animation_header*)council_mouse_bitmap, 0);

        max_votes = 0;
        player_idx = 0;
        if (MOX::NUM_PLAYERS > 0) {
            do {
                if (MOX::player[player_idx].eliminated == 0) {
                    cur_votes = Get_Num_Votes((uint8_t)player_idx);
                    if (max_votes < cur_votes) {
                        max_votes = cur_votes;
                        council_nominee_[0] = (uint8_t)player_idx;
                    }
                }
                player_idx++;
            } while (player_idx < MOX::NUM_PLAYERS);
        }

        max_votes = 0;
        player_idx = 0;
        if (MOX::NUM_PLAYERS > 0) {
            do {
                if (MOX::player[player_idx].eliminated == 0 && (uint8_t)player_idx != council_nominee_[0]) {
                    cur_votes = Get_Num_Votes((uint8_t)player_idx);
                    if (max_votes < cur_votes) {
                        max_votes = cur_votes;
                        council_nominee_[1] = (uint8_t)player_idx;
                    }
                }
                player_idx++;
            } while (player_idx < MOX::NUM_PLAYERS);
        }

        council_total_members = 0;
        for (int16_t i = 0; i < MOX::NUM_PLAYERS; ++i) {
            if (MOX::player[i].eliminated == 0 && (uint8_t)i != council_nominee_[0] && (uint8_t)i != council_nominee_[1]) {
                council_members[council_total_members] = (uint8_t)i;
                council_total_members++;
            }
        }

        RUSS::Mox_Update();
        council_total_votes = 0;
        for (int16_t i = 0; i < (int16_t)council_total_members; ++i) {
            cur_votes = Get_Num_Votes(council_members[i]);
            council_member_votes[i] = cur_votes;
            council_total_votes += cur_votes;
        }

        cur_votes = Get_Num_Votes(council_nominee_[0]);
        council_nominee_votes[0] = cur_votes;
        council_total_votes += cur_votes;

        cur_votes = Get_Num_Votes(council_nominee_[1]);
        council_nominee_votes[1] = cur_votes;
        council_total_votes += cur_votes;

        if (council_total_members != 0) {
            for (int16_t i = 0; i < (int16_t)council_total_members; ++i) {
                for (int16_t j = 0; j < (int16_t)council_total_members - 1; ++j) {
                    if (council_member_votes[j] < council_member_votes[j + 1]) {
                        strings::Swap_Short(&council_member_votes[j], &council_member_votes[j + 1]);
                        strings::Swap_Char(&council_members[j], &council_members[j + 1]);
                    }
                }
            }
        }

        for (int16_t i = 0; i < (int16_t)council_total_members; ++i) {
            uint8_t race = (uint8_t)MOX::player[council_members[i]].race;
            int16_t resource_idx = (int16_t)race * 6 + i + 2;
            if (i == 0) {
                council_light_race_seg[i] = (s_animation_header*)farload::Far_Reload(council::lbx_00559a68, resource_idx, MOX::screen_seg);
            } else {
                council_light_race_seg[i] = (s_animation_header*)farload::Far_Reload_Next(council::lbx_00559a68, resource_idx, MOX::screen_seg);
            }
            council_dark_race_seg[i] = (s_animation_header*)farload::Far_Reload_Next(council::lbx_00559a68, (int16_t)race * 6 + i + 80 /* 0x50 */, MOX::screen_seg);
        }

        RUSS::Mox_Update();
        uint8_t nominee0_race = MOX::player[council_nominee_[0]].race;
        council_nominees_seg[0] = (s_animation_header*)farload::Far_Reload_Next(council::lbx_00559a68, (int16_t)nominee0_race * 2 + 158 /* 0x9E */, MOX::screen_seg);
        uint8_t nominee1_race = MOX::player[council_nominee_[1]].race;
        council_nominees_seg[1] = (s_animation_header*)farload::Far_Reload_Next(council::lbx_00559a68, (int16_t)nominee1_race * 2 + 159 /* 0x9F */, MOX::screen_seg);

        council_no_votes_message = (char*)allocate::Allocate_Next_Block(MOX::screen_seg, 30 /* 0x1E */);
        council_vote_message = (char*)allocate::Allocate_Next_Block(MOX::screen_seg, 30 /* 0x1E */);
        council_votes_message = (char*)allocate::Allocate_Next_Block(MOX::screen_seg, 30 /* 0x1E */);
        council_votes_total_message = (char*)allocate::Allocate_Next_Block(MOX::screen_seg, 30 /* 0x1E */);
        council_message = (char*)allocate::Allocate_Next_Block(MOX::screen_seg, 210 /* 0xD2 */);

        int32_t ani_buffer_size = allocate::Get_Remaining_Block_Space(MOX::screen_seg) - 100;
        void* ani_buffer = allocate::Allocate_Next_Block(MOX::screen_seg, ani_buffer_size);
        file_ani::Set_File_Animation_Buffer(ani_buffer);
        video::Set_Refresh_Stencil();
        file_ani::Open_Multi_File_Animation(council::LBX_00559a74, 1, 0);
        video::Set_Page_Off();
        file_ani::Draw_Multi_File_Animation(0);
        video::Copy_Off_To_Back();
        RUSS::Mox_Update();
        mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);
        Load_Council_Text_Messages();
        RUSS::Mox_Update();
        council_voter = -1;
    }

    void __cdecl Get_Council_Message() {
        char temp_buffer[80];

        switch (council_state) {
            case 0:
                JIM::Get_Text_Message("jimtext.lbx", 15, council_message, 200);
                break;

            case 1:
                JIM::Get_Text_Message("jimtext.lbx", 16, council_message, 200);
                JIM::Decode_Text_Name_And_Name(council_message, council_nominee_[0], council_nominee_[1], 200);
                break;

            case 2:
                JIM::Get_Text_Message("jimtext.lbx", 17, council_message, 200);
                JIM::Decode_Text_Name_And_Stat(council_message, council_nominee_[council_vote_for], council_vote_cast, 200);
                break;

            case 3:
                JIM::Get_Text_Message("jimtext.lbx", 18, council_message, 200);
                JIM::Decode_Text_Name_And_Stat(council_message, council_voter, council_vote_cast, 200);
                break;

            case 4:
                JIM::Get_Text_Message("jimtext.lbx", 19, council_message, 200);
                JIM::Decode_Text_Race(council_message, council_nominee_[council_vote_for], 200);
                break;

            case 5:
                JIM::Get_Text_Message("jimtext.lbx", 20, council_message, 200);
                break;

            case 6:
                JIM::Get_Text_Message("jimtext.lbx", 21, council_message, 200);
                if (diplodef::COUNCIL_FLAG == 2) {
                    JIM::Get_Text_Message("jimtext.lbx", 22, temp_buffer, sizeof(temp_buffer));
                    JIM::Decode_Text_Race(temp_buffer, winning_player, sizeof(temp_buffer));
                    strlcat(council_message, temp_buffer, 210);
                }
                break;

            default:
                break;
        }
    }

    void __cdecl Council_Finished_Voting() {
        int16_t total_votes = council::council_total_votes;
        int32_t threshold = (total_votes * 2 + 2) / 3;

        if (council::council_nominee_vote_total[0] >= threshold) {
            council::council_vote_for = 0;
            council::council_state = 4;

            if ((int16_t)council::council_nominee_[0] == MOX::PLAYER_NUM) {
                diplodef::COUNCIL_FLAG = 2;
            } else {
                diplodef::COUNCIL_FLAG = 3;
            }
        } else if (council::council_nominee_vote_total[1] >= threshold) {
            council::council_vote_for = 1;
            council::council_state = 4;

            if ((int16_t)council::council_nominee_[1] == MOX::PLAYER_NUM) {
                diplodef::COUNCIL_FLAG = 2;
            } else {
                diplodef::COUNCIL_FLAG = 3;
            }
        } else {
            council::council_state = 5;
        }

        council::Get_Council_Message();
        council::Control_Main_Council_Screen();
        council::Next_Council_State();
    }

    void Council_Accept_Ruling() {
        char choice_labels[3][25];
        char prompt_text[40];
        int8_t list_metadata[2];
        uint8_t active_mask[4];

        fields::Assign_Auto_Function(Draw_Main_Council_Screen, 2);

        if (diplodef::COUNCIL_FLAG != 1) {
            *council::council_message = '\0';
            fonts::Set_Outline_Color(0, 0);
            fonts::Set_Font_Style(4, &council::council_normal_colors);
            fonts::Set_Font_Highlight_Colors(&council::council_highlight_colors);
            fonts::Set_Font_LF(4);

            JIM::Get_Text_Message("jimtext.lbx", 0x1A, choice_labels[0], sizeof(choice_labels[0]));
            JIM::Get_Text_Message("jimtext.lbx", 0x1B, choice_labels[1], sizeof(choice_labels[1]));
            JIM::Get_Text_Message("jimtext.lbx", 0x1C, prompt_text, sizeof(prompt_text));

            choice_labels[2][0] = '\0';
            list_metadata[0] = 0;
            list_metadata[1] = 0;
            active_mask[0] = 1;
            active_mask[1] = 1;
            active_mask[2] = 1;
            active_mask[3] = 1;

            int16_t selection = fields::Get_List_Field(
                0xD6, 0x184, 0xB4, prompt_text, choice_labels[0], 0x19, list_metadata, active_mask, 0x11, nullptr, 0, nullptr, 0, 0);

            if (selection == 0) {
                council::council_state = 0x63 /* 99 */;
                diplodef::council_winner = council::council_nominee_[council::council_vote_for];

                if (MOX::PLAYER_NUM == diplodef::council_winner) {
                    Council_Player_Voted_Winner();
                    ERIC::Play_Diplomatic_Win_Flic();
                }

                MOX::player[diplodef::council_winner].won_council_vote = 1;
            } else if (selection == 1) {
                council::council_state = 6;
                if (diplodef::COUNCIL_FLAG == 2) {
                    council::winning_player = council::council_nominee_[0];
                    if ((int16_t)council::council_nominee_[0] == MOX::PLAYER_NUM) {
                        council::winning_player = council::council_nominee_[1];
                    }
                } else {
                    council::winning_player = council::council_nominee_[council::council_vote_for];
                }

                council::Get_Council_Message();
                council::Control_Main_Council_Screen();
                council::Next_Council_State();

                for (int16_t i = 0; i < MOX::NUM_PLAYERS; ++i) {
                    if (council::winning_player != i && MOX::PLAYER_NUM != i) {
                        if (MOX::player[i].eliminated == 0) {
                            if (COLCALC::May_Surrender_Empire(i, council::winning_player)) {
                                COLCALC::Surrender_Empire(i, council::winning_player);
                            }
                        }
                    }
                }

                COLCALC::Do_Surrenders();

                diplomac::Break_Trade(council::winning_player, MOX::PLAYER_NUM);
                diplomac::Break_Research(council::winning_player, MOX::PLAYER_NUM);
                diplomac::Break_Tribute(council::winning_player, MOX::PLAYER_NUM);
                diplomac::Break_Treaties(council::winning_player, MOX::PLAYER_NUM);
                diplomac::Declare_War(council::winning_player, MOX::PLAYER_NUM);
            }
        }

        fields::Deactivate_Auto_Function();
        fields::Clear_Fields();
    }

    void Council_Player_Voted_Winner() {
        memset(player_voted_winner_message_id, 0xff, sizeof(player_voted_winner_message_id));

        fields::Assign_Auto_Function(Draw_Council_Player_Voted_Winner, 2);

        /* Select the nominee index opposite to the player's vote. */
        int16_t other_nominee_idx = council_nominee_[council_vote_for == 0 ? 1 : 0];
        Drive_Council_Player_Voted_Winner(other_nominee_idx, 0);

        if (council_total_members != 0) {
            for (int i = 0; i < (int)council_total_members; ++i) {
                uint8_t member_idx = council_members[i];
                council_members[i] = member_idx + 100;
                Drive_Council_Player_Voted_Winner((int16_t)member_idx, 0);
            }
        }
    }

    void __cdecl Load_Council_Text_Messages() {
        JIM::Get_Text_Message(DIP_SCRN::s_jimtext_lbx_00552360, 11, council_no_votes_message, 30);
        JIM::Get_Text_Message(DIP_SCRN::s_jimtext_lbx_00552360, 12, council_vote_message, 30);
        JIM::Get_Text_Message(DIP_SCRN::s_jimtext_lbx_00552360, 13, council_votes_message, 30);
        JIM::Get_Text_Message(DIP_SCRN::s_jimtext_lbx_00552360, 14, council_votes_total_message, 30);
    }

    bool __cdecl Vote_Check(int16_t candidate_idx, int16_t voter_idx, int16_t opponent_idx) {
        int16_t vote_score;
        int8_t treaty_type = MOX::player[voter_idx].treaty[candidate_idx];

        if (treaty_type >= 4 /* TREATY_ALLIANCE */) {
            vote_score = -1;
        } else if (treaty_type == 2 /* TREATY_PEACE */) {
            vote_score = 200;
        } else {
            /* Base score calculated from diplomatic standing and trust. */
            vote_score = MOX::player[voter_idx].broken_treaties_modifier[candidate_idx] +
                ((MOX::player[voter_idx].trust_worthiness[candidate_idx] -
                     MOX::player[voter_idx].diplomacy_proposal_rejection[candidate_idx] * 10) *
                    2) +
                MOX::player[voter_idx].relations[candidate_idx];

            /* If the voter is allied with the opposing candidate, increase the score requirement. */
            if (MOX::player[voter_idx].treaty[opponent_idx] >= 4 /* TREATY_ALLIANCE */) {
                vote_score += 100;
            }

            /* Favor the current Orion owner. */
            if (HACCESS::Orion_Owner() == candidate_idx) {
                vote_score += 40;
            }

            /* Apply diplomatic trait modifiers. */
            if (MOX::player[candidate_idx].traits[TRAIT_CHARISMATIC] == 1) {
                vote_score += 40;
            }
            if (MOX::player[candidate_idx].traits[TRAIT_REPULSIVE] == 1) {
                vote_score -= 100;
            }

            /* Apply the relevant diplomacy tech modifier. */
            if (MOX::player[candidate_idx].tech_applications[TECH_APP_XENO_PSYCHOLOGY] == TECH_RESEARCH_STATUS_RESEARCHED) {
                vote_score += 30;
            }

            /* Favor the same candidate as in the previous council vote. */
            if (MOX::player[voter_idx].council_voted_for == (int8_t)candidate_idx) {
                vote_score += 50;
            }

            /* Apply active treaty bonuses. */
            if (MOX::player[voter_idx].treaty[candidate_idx] == 1 /* TREATY_NON_AGGRESSION */) {
                vote_score += 50;
            }
            if (MOX::player[voter_idx].trade_treaty[candidate_idx] != 0) {
                vote_score += 25;
            }
            if (MOX::player[voter_idx].research_treaty[candidate_idx] != 0) {
                vote_score += 25;
            }

            if (MOX::player[candidate_idx].objectives == PLAYER_OBJECTIVE_HUMAN) {
                if (MOX::settings.game_difficulty > GAME_DIFFICULTY_AVERAGE) {
                    vote_score = (int16_t)((vote_score * 6) / (MOX::settings.game_difficulty + 6));
                }
            }
        }

        /* Determine whether the vote is cast based on the final score. */
        return (int32_t)game_random::Random(200) <= (int32_t)vote_score;
    }

    void __cdecl Get_Player_Voted_Winner_Message(int16_t unused_callback_arg) {
        (void)unused_callback_arg;
        int16_t message_idx;
        do {
            /* BUG: message_idx can be -1 if Random_(14) returns 0. */
            message_idx = (int16_t)game_random::Random(14) - 1;
        } while (player_voted_winner_message_id[message_idx] != 0xFF);

        player_voted_winner_message_id[message_idx] = (uint8_t)message_idx;

        /* Calculate the entry index for the current language block. */
        int32_t message_entry_idx = (int16_t)((int16_t)MOX::settings.language * 14 + message_idx);

        string_overrides::Load_Message("councmsg.lbx", message_entry_idx, 0, 200,
            message_idx + 1, council_message, 200);
    }

    void Drive_Council_Player_Voted_Winner(int16_t player_idx, int16_t unused_callback_arg) {
        Get_Player_Voted_Winner_Message(unused_callback_arg);

        int16_t race_anim_id = (int16_t)((uint8_t)MOX::player[player_idx].race + 0xB8);
        file_ani::Open_Multi_File_Animation("council.LBX", race_anim_id, 1);

        bool exit_loop = false;
        while (!exit_loop) {
            timer::Mark_Time();
            fields::Clear_Fields();

            int16_t hidden_field_id = fields::Add_Hidden_Field(0, 0, 639, 479, "", 0);
            int16_t input_id = fields::Get_Input();

            if (hidden_field_id == input_id) {
                exit_loop = true;
            }

            if (!exit_loop) {
                Draw_Council_Player_Voted_Winner();
                RUSS::Mox_Sync_Update();
                timer::Release_Time(2);
            }
        }
    }

    void __cdecl Draw_Council_Player_Voted_Winner() {
        int16_t x_coords[6] = {44, 520, 143, 444, 215, 377};
        int16_t y_coords[6] = {96, 100, 148, 149, 169, 168};

        video::Set_Page_Off();
        fonts::Set_Outline_Color(0, 0);

        if (MOX::settings.animations_on == 1) {
            file_ani::Draw_Multi_File_Animation(0);
            if (file_ani::Get_Multi_File_Animation_Frame(0) == 0) {
                file_ani::Set_Multi_File_Animation_Frame(0, 1);
            }
            file_ani::Draw_Multi_File_Animation(1);
        }

        for (int i = 0; i < (int)council_total_members; ++i) {
            if (council_members[i] < 100) {
                animate::Draw(x_coords[i], y_coords[i], council_dark_race_seg[i]);
            }
        }

        fonts::Set_Outline_Color(0, 0);
        fonts::Set_Font_Style_Outline(4, &council_normal_colors);

        int16_t msg_height = BILL::_Get_Paragraph_Max_Height(550, council_message);
        BILL::_Print_Paragraph(55, 470 - msg_height, 550, council_message, 2);

        JIM::Play_Streaming_Music(119 /* 0x77 */, -1, 1);
        RUSS::Mox_Update();
    }
} // namespace council
