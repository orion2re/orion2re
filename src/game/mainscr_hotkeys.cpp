#include "pch.h"

#include "config/config_loader.h"

namespace MAINSCR {
    int16_t all_tech_cheat = -1000;
    int16_t leader_cheat = -1000;
    int16_t more_money_cheat = -1000;
    int16_t get_tech_cheat = -1000;
    int16_t omniscience_cheat = -1000;
    int16_t all_ai_cheat = -1000;
    int16_t hi_score_cheat = -1000;
    int16_t cycle_player_cheat = -1000;
    int16_t reload_config_command = -1000;
    int16_t reload_interface_command = -1000;
    int16_t last_field;

    bool __cdecl Check_Config_Reload_Hot_Key(int16_t key) {
        const bool reload_all = key == reload_config_command;
        if (!reload_all && key != reload_interface_command) {
            return false;
        }

        config::s_cfg_diagnostic diagnostic;
        const bool network_game = MOX::game_type == 2 || MOX::game_type == 3;
        if (reload_all && network_game) {
            HAROLD::User_Box(
                &MOX::draw_main_screen_to_back,
                "You may not reload config in a multiplayer game.",
                0);
        } else if (!(reload_all
                           ? config::Reload_All_Active_Config_Groups(&diagnostic)
                           : config::Reload_Active_Config_Groups(
                                 network_game, &diagnostic))) {
            HAROLD::User_Box(
                &MOX::draw_main_screen_to_back,
                diagnostic.message.c_str(),
                0);
        } else {
            if (reload_all) {
                HAROLD::User_Box(
                    &MOX::draw_main_screen_to_back, "config reloaded", 0);
            }
            MOX::draw_main_screen_to_back = 1;
        }
        return true;
    }

    void __cdecl Add_Cheats() {
        all_tech_cheat = fields::Add_Multi_Hot_Key_Field("EINSTEIN");
        more_money_cheat = fields::Add_Multi_Hot_Key_Field("MOOLA");
        get_tech_cheat = fields::Add_Multi_Hot_Key_Field("MENLO");
        omniscience_cheat = fields::Add_Multi_Hot_Key_Field("ISEEALL");
        reload_config_command = fields::Add_Multi_Hot_Key_Field("RE");
        reload_interface_command = fields::Add_Multi_Hot_Key_Field("RI");
        if (MOX::game_type < 2) {
            cycle_player_cheat = fields::Add_Multi_Hot_Key_Field("X");
        }
        leader_cheat = fields::Add_Multi_Hot_Key_Field("ALE");
        all_ai_cheat = fields::Add_Multi_Hot_Key_Field("ALLAI");
        hi_score_cheat = fields::Add_Multi_Hot_Key_Field("SCOR");
        last_field = fields::fields_count - 1;
    }
} // namespace MAINSCR
