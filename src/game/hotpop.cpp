#include "pch.h"
#include "player_roster.h"

namespace HOTPOP {
    int16_t hotseat_human_players;
    s_animation_header* hot_pop_background_seg;
    s_animation_header* grid_background[4];

    void __cdecl Fade_Into_Hotseat_Screen() {
        ERIC::Fast_Fade_Out();

        fonts::Load_Palette(11, 0, 255);
        palette::Set_Palette_Changes(0, 255);
        palette::Refresh_Palette();
        palstore::Darken_Palette(100);

        mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);

        video::Set_Page_Off();
        animate::Draw(0, 0, HOTPOP::grid_background[0]);

        palette::Clear_Palette_Changes(0, 255);
        RUSS::Mox_Sync_Update();

        ERIC::Fast_Fade_In();

        video::Set_Page_Back();
        animate::Draw(0, 0, HOTPOP::grid_background[0]);

        video::Copy_Back_To_Off();
    }

    void __cdecl Reload_Hotseat_Screen() {
        grid_background[0] = (s_animation_header*)farload::Far_Reload(MULTPLAY::s_MULTIGM_LBX, 0, MOX::screen_seg);
        hot_pop_background_seg = (s_animation_header*)farload::Far_Reload_Next(MULTPLAY::s_MULTIGM_LBX, 0x0B, MOX::screen_seg);
        grid_background[1] = (s_animation_header*)farload::Far_Reload_Next(MULTPLAY::s_MULTIGM_LBX, 0x0C, MOX::screen_seg);
        grid_background[2] = (s_animation_header*)farload::Far_Reload_Next(MULTPLAY::s_MULTIGM_LBX, 0x0D, MOX::screen_seg);
        grid_background[3] = (s_animation_header*)farload::Far_Reload_Next(MULTPLAY::s_MULTIGM_LBX, 0x0E, MOX::screen_seg);
    }

    void __cdecl Init_Hotseat_Screen(int16_t* out_join_button_field, int16_t* out_accept_button_field, int16_t* out_cancel_button_field) {
        int16_t base_x;
        int16_t base_y;
        int16_t anim_w;
        int16_t anim_h;

        HOTPOP::Reload_Hotseat_Screen();

        if (HOTPOP::hotseat_human_players > 0) {
            HOTPOP::Fade_Into_Hotseat_Screen();
        }

        fields::Clear_Fields();

        anim_w = animate::Get_Width(HOTPOP::hot_pop_background_seg);
        base_x = (640 - anim_w) / 2;

        anim_h = animate::Get_Height(HOTPOP::hot_pop_background_seg);
        base_y = (480 - anim_h) / 2;

        if (HOTPOP::hotseat_human_players < MAX_PLAYERS) {
            *out_join_button_field = fields::Add_Button_Field(base_x + 125, base_y + 99, "", grid_background[1], "", 40);
        } else {
            *out_join_button_field = -1000;
        }

        *out_accept_button_field = fields::Add_Button_Field(base_x + 125, base_y + 136, "", grid_background[2], "", 40);

        /* "\x1b" is a non-printing fmtpara ESC control byte reused here as a field-label sentinel. */
        *out_cancel_button_field = fields::Add_Button_Field(base_x + 140, base_y + 200, "", grid_background[3], "\x1b", 40);

        fields::Assign_Auto_Function(HOTPOP::Draw_Hotseat_Screen, 2);
    }

    void __cdecl Hotseat_Screen(void) {
        int16_t btn_join;
        int16_t btn_accept;
        int16_t btn_cancel;
        int16_t input;
        int16_t player_idx;
        int16_t exit_code;
        s_player* temp_player;
        int16_t center_x;
        int16_t center_y;
        char msg_players_plural[20];
        char remaining_player_label[20];
        char warning_buf[200];

        MOX::hot_seat_flag = 3;
        Init_Hotseat_Screen(&btn_join, &btn_accept, &btn_cancel);

        center_x = (640 - animate::Get_Width(HOTPOP::hot_pop_background_seg)) / 2;
        center_y = (480 - animate::Get_Height(HOTPOP::hot_pop_background_seg)) / 2;

        HOTPOP::hotseat_human_players = 0;
        player_idx = 0;
        allocate::Push_Block(MOX::global_data_seg);
        temp_player = static_cast<s_player*>(allocate::Allocate_Next_Block_Aligned(
            MOX::global_data_seg, sizeof(s_player), alignof(s_player)));
        exit_code = 0;

        struct_::Clear_Structure(temp_player, sizeof(s_player));

        do {
            input = fields::Get_Input();

            if (HOTPOP::hotseat_human_players == 0) {
                input = btn_join;
            }

            if (input == btn_cancel) {
                MOX::current_screen = SCREEN_MULTI_PLAYER;
                exit_code = 1;
            }

            if (input == btn_accept) {
                if (HOTPOP::hotseat_human_players == 1) {
                    if (GENDRAW::Confirmation_Box((char*)HAROLD::H_Message(162)) == 0) {
                        video::Set_Page_Back();
                        animate::Draw(0, 0, grid_background[0]);
                        video::Copy_Back_To_Off();
                    } else {
                        MOX::game_type = 0;
                        exit_code = 2;
                    }
                } else {
                    strlcpy(msg_players_plural, HAROLD::H_Message(165), sizeof(msg_players_plural));

                    if ((MOX::settings.number_of_players - HOTPOP::hotseat_human_players) == 1) {
                        strlcpy(remaining_player_label, HAROLD::H_Message(164), sizeof(remaining_player_label));
                    } else {
                        strlcpy(remaining_player_label, HAROLD::H_Message(165), sizeof(remaining_player_label));
                    }

                    snprintf(warning_buf, sizeof(warning_buf), HAROLD::H_Message(163), HOTPOP::hotseat_human_players, msg_players_plural, (MOX::settings.number_of_players - HOTPOP::hotseat_human_players), remaining_player_label);

                    if (GENDRAW::Confirmation_Box(warning_buf) == 0) {
                        video::Set_Page_Back();
                        animate::Draw(0, 0, grid_background[0]);
                        video::Copy_Back_To_Off();
                    } else {
                        exit_code = 2;
                    }
                }
            }

            if (input == btn_join) {
                MOX::local_player = legacy_world::Decode_Player_Slot(player_idx);
                if (RACESEL::Race_Selection_Screen(temp_player) == 0) {
                    EVANHELP::Set_Hotseat_Setup_Screen_Help_List(center_x, center_y);
                    if (HOTPOP::hotseat_human_players == 0) {
                        MOX::current_screen = SCREEN_MULTI_PLAYER;
                        exit_code = 1;
                    }
                } else {
                    EVANHELP::Set_Hotseat_Setup_Screen_Help_List(center_x, center_y);
                    player_roster::Install(game::current_state, player_idx, *temp_player);
                    game::current_state.players[player_idx].objectives = 100;
                    player_idx++;
                    HOTPOP::hotseat_human_players++;
                }

                Init_Hotseat_Screen(&btn_join, &btn_accept, &btn_cancel);
                if (MOX::settings.number_of_players == HOTPOP::hotseat_human_players) {
                    exit_code = 2;
                }
            }

            if (exit_code != 0) {
                fields::Clear_Fields();
                allocate::Pop_Block(MOX::global_data_seg);
                fields::Deactivate_Auto_Function();

                if (exit_code == 2) {
                    if (INITGAME::Init_New_Game(
                        MOX::settings.game_difficulty,
                        MOX::settings.galaxy_size,
                        MOX::settings.galaxy_age,
                        NEWGAME::Player_Count_For_Initialization(),
                        MOX::settings.strategic_combat_flag,
                        MOX::settings.starting_civilization_level)) {
                        MOX::current_screen = SCREEN_HOTSEAT_SELECT_PLAYER;
                    } else {
                        MOX::current_screen = SCREEN_MULTI_PLAYER;
                    }
                }
                return;
            }

            video::Set_Page_Off();
            HOTPOP::Draw_Hotseat_Screen();
            RUSS::Mox_Sync_Update();

        } while (true);
    }

    void __cdecl Draw_Hotseat_Screen(void) {
        int16_t bg_x;
        int16_t bg_y;
        s_colors active_colors;
        s_colors inactive_colors;
        s_colors unused_colors;

        bg_x = (640 - animate::Get_Width(HOTPOP::hot_pop_background_seg)) / 2;
        bg_y = (480 - animate::Get_Height(HOTPOP::hot_pop_background_seg)) / 2;

        memset(&unused_colors, 0xA7, sizeof(unused_colors));

        animate::Draw(bg_x, bg_y, HOTPOP::hot_pop_background_seg);

        inactive_colors = *MISC::Font_Colors2(1, 0x9a, 0x9c);
        active_colors = *MISC::Font_Colors2(1, 0x9a, 0x9f);

        fonts::Set_Font_Style_Shadow_Down(1, &active_colors);

        if ((int8_t)MOX::settings.number_of_players > 0) {
            int16_t text_x = bg_x + 14;
            int16_t text_y = bg_y + 70;

            for (int16_t i = 0; i < (int8_t)MOX::settings.number_of_players; ++i) {
                if (i < (int16_t)HOTPOP::hotseat_human_players) {
                    fonts::Set_Font_Style_Shadow_Down(1, &active_colors);
                } else {
                    fonts::Set_Font_Style_Shadow_Down(1, &inactive_colors);
                }

                fonts::Print_Integer(text_x, text_y, i + 1);
                text_y += 9;
            }
        }
    }

} // namespace HOTPOP
