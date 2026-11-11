#include "pch.h"

namespace NEXTTURN {
    int16_t Get_Winner() {
        if (diplodef::council_winner != -1) {
            return (int16_t)diplodef::council_winner;
        }

        int16_t winner_idx = -1;
        int16_t num_players = (int16_t)MOX::NUM_PLAYERS;

        for (int16_t i = 0; i < num_players; ++i) {
            if (MOX::player[i].captured_antares != 0 || MOX::player[i].eliminated == 0) {
                winner_idx = i;
            }
        }

        return winner_idx;
    }

    void Do_Autosave() {
        int32_t autosave_interval = 1;

        if (MOX::game_type == 2 || MOX::game_type == 3) {
            autosave_interval = 1;
        }

        if (MOX::settings.auto_saves != 0) {
            if (((int32_t)MOX::stardate % autosave_interval) == 0) {
                strlcpy(MOX::save_game_description[9].description, "(Auto Save)", sizeof(MOX::save_game_description[9].description));
                FILEDEF::Save_Game(9);
            }
        }
    }

    void Next_Turn() {
        if (MOX::game_type == 0 || MOX::game_type == 1) {
            Do_Autosave();
        }

        HAROLD::Confirm_Planet_And_Star_Index_Link(HAROLD::s_in_next_turn);
        Next_Turn_Calc();

        MOX::current_screen = MOX::return_screen;

        if (MOX::game_type != 2 && MOX::game_type != 3 && RUSS::all_ai_game == 0) {
            if (COLCALC::N_Living_Human_Players() < 1) {
                ERIC::Fast_Fade_Out();
                video::Set_Page_Off();
                graphics::Fill(0, 0, 0x27f, 0x1df, 0);
                RUSS::Mox_Sync_Update();
                video::Copy_Off_To_Back();
                video::Set_Refresh_Stencil();
                ERIC::Play_Loose_Flic();
                SCORE::End_Of_Game_Hi_Score(MOX::PLAYER_NUM);
                MOX::current_screen = SCREEN_MAIN_MENU;
            } else {
                if (COLCALC::N_Living_Players() < 2) {
                    int16_t winner = Get_Winner();
                    ERIC::Fast_Fade_Out();
                    ERIC::Play_Normal_Win_Flic();
                    SCORE::End_Of_Game_Hi_Score(winner);
                    MOX::current_screen = SCREEN_MAIN_MENU;
                } else if (diplodef::council_winner != -1) {
                    if (MOX::PLAYER_NUM != diplodef::council_winner) {
                        ERIC::Play_Loose_Flic();
                    }
                    SCORE::End_Of_Game_Hi_Score(MOX::PLAYER_NUM);
                    MOX::current_screen = SCREEN_MAIN_MENU;
                } else {
                    for (int i = 0; i < MOX::NUM_PLAYERS; ++i) {
                        if (MOX::player[i].captured_antares != 0) {
                            fields::Deactivate_Auto_Function();
                            ERIC::Play_All_Flics();
                            SCORE::End_Of_Game_Hi_Score((int16_t)i);
                            MOX::current_screen = SCREEN_MAIN_MENU;
                        }
                    }
                }
            }
        }

        for (int i = 0; i < MOX::NUM_PLAYERS; ++i) {
            MOX::player[i].player_done_flags = 0;
        }

        MOX::old_system_index = -1;
    }
} // namespace NEXTTURN
