#include "pch.h"

#include "config/config_loader.h"
#include "game/boarding.h"
#include "combat1_turn_order.h"

namespace COMBAT1 {
    static bool time_warp_phase_active;
    static int16_t reload_config_button;
    static int16_t reload_interface_button;

    static int16_t Advance_Next_Ship(
        int16_t active_player_id, COMBAT_SHIP_ACTION action);
    static int16_t Finish_Next_Ship(int16_t result);

    static bool Process_Interface_Reload_Input(int16_t input_value) {
        const bool reload_all = input_value == reload_config_button;
        if (!reload_all && input_value != reload_interface_button) {
            return false;
        }

        config::s_cfg_diagnostic diagnostic;
        const bool network_game = MOX::game_type == 2 || MOX::game_type == 3;
        if (reload_all && network_game) {
            GENDRAW::Warning_Box(
                "You may not reload config in a multiplayer game.");
            return true;
        }
        if (!(reload_all
                    ? config::Reload_All_Active_Config_Groups(&diagnostic)
                    : config::Reload_Active_Config_Groups(
                          network_game, &diagnostic))) {
            GENDRAW::Warning_Box(diagnostic.message.c_str());
        } else if (reload_all) {
            GENDRAW::Message_Box("config reloaded");
        }
        return true;
    }

    static int16_t Get_Tactical_Input() {
        for (;;) {
            const int16_t input_value = fields::Get_Input();
            if (!Process_Interface_Reload_Input(input_value)) {
                return input_value;
            }
        }
    }

    int32_t end_of_turn_button;

    uint16_t start_of_turn;

    void __cdecl Add_Tactical_Combat_Fields(void) {
        reload_config_button = -100;
        reload_interface_button = -100;
        int16_t auto_combat_idx = 0;
        const char* info_hotkey;
        int16_t info_y_coord;

        if (MOX::cur_ship >= 0 && MOX::cur_ship < MOX::combat_total_ship_count) {
            if (MOX::combat_data[MOX::cur_ship].owner != MOX::combat_attacker_player) {
                auto_combat_idx = 1;
            }
        }

        Set_Combat_Help_Fields();

        if (MOX::auto_combat_flag[auto_combat_idx] == 1 ||
            (MOX::game_type != 1 && MOX::cur_ship >= 0 && MOX::cur_ship < MOX::combat_total_ship_count &&
                MOX::combat_data[MOX::cur_ship].owner != MOX::PLAYER_NUM &&
                MOX::take_control_flag == 0)) {
            for (int i = 0; i < MAX_COMBAT_FLYERS; ++i) {
                MOX::missile_field[i] = -1000;
            }
            MOX::take_control_button = -1000;
            MOX::space_field = -1000;
            MOX::map_field = -1000;
            MOX::msl_warning_button = -1000;
            MOX::speedx2_button = -100;
            MOX::draw_legal_boxes_button = -100;
            MOX::msl_impact_warning_button = -100;
            MOX::self_destruct_button = -100;
            MOX::shields_button = -100;
            MOX::grid_button = -100;
            MOX::scan_button = -100;
            COMBAT1::end_of_turn_button = -100;
            MOX::board_button = -100;
            MOX::flee_button = -100;
            MOX::wait_button = -100;
            side_wait_button = -100;
            MOX::done_button = -100;
            for (int i = 0; i < 8; ++i) {
                MOX::weapon_button[i] = -1000;
            }
            MOX::center_button = -1000;
            MOX::weapons_button = -1000;
            MOX::specials_button = -1000;
            MOX::cmbt_info_button = -1000;
            MOX::super_fast_button = -1000;
            return;
        }

        fields::Clear_Fields();
        reload_config_button = fields::Add_Multi_Hot_Key_Field("RE");
        reload_interface_button = fields::Add_Multi_Hot_Key_Field("RI");

        if (MOX::order_info_flag == 1) {
            Add_Missile_Fields();
            MOX::take_control_button = fields::Add_Hot_Key("[");
            MOX::space_field = fields::Add_Grid_Field(0, 0, 10, 10, 64, 36, &MOX::xpos, &MOX::ypos, 0);

            if (MOX::missile_warning == 1) {
                MOX::msl_warning_button = fields::Add_Hidden_Field(505, 370, 622, 464, "", 0);
                MOX::map_field = -1000;
            } else {
                MOX::map_field = fields::Add_Grid_Field(500, 364, 1, 1, 121, 102, &MOX::map_x, &MOX::map_y, 0);
                MOX::msl_warning_button = -1000;
            }

            MOX::weapons_button = fields::Add_Multi_Button_Field(114, 361, "", (s_animation_header*)MOX::weapons_button_seg, &MOX::ship_status_display, 0, "", 41);
            MOX::specials_button = fields::Add_Multi_Button_Field(187, 361, "", (s_animation_header*)MOX::specials_button_seg, &MOX::ship_status_display, 1, "", 41);
            MOX::auto_button = fields::Add_Radio_Button_Field(273, 364, "", (s_animation_header*)MOX::auto_button_seg, &MOX::auto_cmbt_button_value, "a", 40);
            MOX::scan_button = disable_scan == 0
                ? fields::Add_Radio_Button_Field(337, 364, "", (s_animation_header*)MOX::scan_button_seg, &MOX::skan_flag, "s", 40)
                : -100;
            MOX::board_button = fields::Add_Radio_Button_Field(273, 392, "", (s_animation_header*)MOX::board_button_seg, &MOX::board_flag, "b", 40);
            MOX::flee_button = fields::Add_Button_Field(337, 392, "", (s_animation_header*)MOX::flee_button_seg, "r", 40);
            if (ship_initiative == 2 && strict_wait_stage) {
                MOX::wait_button = -100;
            } else {
                MOX::wait_button = fields::Add_Button_Field(273, 420, "", (s_animation_header*)MOX::wait_button_seg, "w", 40);
            }
            side_wait_button = Add_Combat_Side_Wait_Field();
            MOX::done_button = fields::Add_Button_Field(337, 420, "", (s_animation_header*)MOX::cmbt_done_button_seg, "d", 40);
            COMBAT1::end_of_turn_button = (int)fields::Add_Hot_Key("t");

            MOX::draw_legal_boxes_button = -100;
            MOX::msl_impact_warning_button = -100;
            MOX::speedx2_button = -100;
            MOX::shields_button = -100;
            MOX::grid_button = -100;
            MOX::self_destruct_button = -100;

            if (MOX::combat_attacker_player < MAX_PLAYERS && MOX::player[MOX::combat_attacker_player].objectives == PLAYER_OBJECTIVE_HUMAN &&
                MOX::combat_defender_player < MAX_PLAYERS && MOX::player[MOX::combat_defender_player].objectives == PLAYER_OBJECTIVE_HUMAN) {
                MOX::super_fast_button = -1000;
            } else {
                MOX::super_fast_button = fields::Add_Hot_Key("z");
            }

            info_hotkey = "O";
            info_y_coord = 448;
        } else {
            for (int i = 0; i < MAX_COMBAT_FLYERS; ++i) {
                MOX::missile_field[i] = -1000;
            }
            MOX::take_control_button = -100;
            MOX::space_field = -100;
            MOX::map_field = fields::Add_Grid_Field(500, 364, 1, 1, 121, 102, &MOX::map_x, &MOX::map_y, 0);
            MOX::msl_warning_button = -100;
            MOX::weapons_button = -100;
            MOX::specials_button = -100;
            MOX::msl_impact_warning_button = fields::Add_Hidden_Field(264, 383, 407, 396, "", 41);
            MOX::speedx2_button = fields::Add_Hidden_Field(264, 399, 407, 412, "", 41);
            MOX::draw_legal_boxes_button = fields::Add_Hidden_Field(264, 415, 407, 428, "", 41);
            MOX::shields_button = fields::Add_Hidden_Field(264, 431, 407, 444, "", 41);
            MOX::grid_button = fields::Add_Hidden_Field(264, 447, 407, 460, "", 41);
            MOX::self_destruct_button = fields::Add_Button_Field(283, 348, "", (s_animation_header*)MOX::self_destruct_seg, "", 40);
            MOX::auto_button = -100;
            MOX::scan_button = -100;
            COMBAT1::end_of_turn_button = -100;
            MOX::super_fast_button = -1000;
            MOX::board_button = -100;
            MOX::flee_button = -100;
            MOX::wait_button = -100;
            side_wait_button = -100;
            MOX::done_button = -100;
            info_hotkey = "\x1B";
            info_y_coord = 301;
        }

        MOX::cmbt_info_button = fields::Add_Button_Field(298, info_y_coord, "", (s_animation_header*)MOX::cmbt_info_button_seg, info_hotkey, 40);

        int16_t weapon_y = 389;
        for (int i = 0; i < 8; ++i) {
            MOX::weapon_button[i] = fields::Add_Hidden_Field(120, weapon_y - 10, 255, weapon_y, "", 41);
            weapon_y += 11;
        }

        MOX::center_button = fields::Add_Hidden_Field(15, 379, 100, 460, "c", 41);
    }

    bool Combat_Ship_Participates_In_Current_Pass(int16_t ship_idx) {
        return Combat_Ship_Participates_In_Pass(
            ship_idx, time_warp_phase_active);
    }

    static void Process_Retreating_Ships(int16_t active_player_id, int16_t time_warp_only) {
        for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
            if (!Combat_Ship_Participates_In_Pass(i, time_warp_only != 0)) {
                continue;
            }

            s_combat_data* ship = &MOX::combat_data[i];
            if ((MOX::combat_init == 0 && ship->owner != active_player_id) ||
                ship->is_retreating != 1) {
                continue;
            }

            if (ship->current_speed <= 0 || Enemy_Dissipator_Exists(ship->owner) != 0) {
                ship->is_retreating = 0;
                continue;
            }

            if (MOX::net_flag != 0 && MOX::combat_init != 0) {
                NETMOX::Net_Debug_Log(
                    "Wait_Until_Net_Opponent_Finished: caller=%s:%d",
                    __FILE__,
                    __LINE__);
                CMBTMOV1::Wait_Until_Net_Opponent_Finished(1);
            }

            MOX::cur_ship = i;
            Retreat_Ship(i);
        }
    }

    static void End_Side_Turn() {
        const int8_t owner = MOX::combat_data[MOX::cur_ship].owner;

        for (int16_t i = 0; i < MAX_COMBAT_SHIPS; ++i) {
            const int16_t ship_idx = MOX::ship_turn_order[i];
            if (ship_idx == -10) {
                break;
            }
            if (ship_idx >= 0 &&
                ship_idx < MOX::combat_total_ship_count &&
                MOX::combat_data[ship_idx].owner == owner) {
                MOX::ship_turn_order[i] = -1;
            }
        }

        MOX::side_end_of_turn[owner] = 1;
        MOX::cur_ship = -1;
    }

    static void Advance_After_Ship_Owner_Change(
        int8_t original_owner,
        int16_t active_player_id,
        int16_t* next_ship_flag) {
        if (MOX::combat_data[MOX::cur_ship].owner == original_owner) {
            return;
        }

        const COMBAT_SHIP_ACTION action = ship_initiative == 2
            ? COMBAT_SHIP_ACTION_NONE
            : COMBAT_SHIP_ACTION_DONE;
        *next_ship_flag = Advance_Next_Ship(active_player_id, action);
    }

    void __cdecl Do_Combat_Turn(int16_t active_player_id, int16_t time_warp_only) {
        time_warp_phase_active = time_warp_only != 0;
        MOX::player_mode[MOX::combat_attacker_player] = CMBTAI::Get_Player_Mode(MOX::combat_attacker_player);
        MOX::player_mode[MOX::combat_defender_player] = CMBTAI::Get_Player_Mode(MOX::combat_defender_player);

        for (int16_t i = 0; i < 15; ++i) {
            MOX::side_end_of_turn[i] = 0;
        }
        Reset_Combat_Side_Waits();

        if (MOX::combat_total_ship_count > 0) {
            for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
                if (Combat_Ship_Participates_In_Pass(i, time_warp_only != 0)) {
                    if (MOX::combat_init != 0 || MOX::combat_data[i].owner == active_player_id) {
                        Init_Ship_For_Start_Of_Turn(i);
                    }
                }
            }
        }

        Process_Retreating_Ships(active_player_id, time_warp_only);
        RUSS::Mox_Update();

        int16_t order_idx = 0;
        if (MOX::combat_total_ship_count > 0) {
            for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
                if (Combat_Ship_Participates_In_Turn_Order(
                        i, time_warp_only != 0)) {
                    if (MOX::combat_init != 0 || MOX::combat_data[i].owner == active_player_id) {
                        MOX::ship_turn_order[order_idx] = i;
                        ++order_idx;
                    }
                }
            }
        }

        if (order_idx == 0) {
            time_warp_phase_active = false;
            return;
        }

        if (order_idx != 0) {
            if (MOX::combat_init != 0) {
                qsort(MOX::ship_turn_order, order_idx, sizeof(MOX::ship_turn_order[0]), qsort_ship_initiative);
            }
            MOX::ship_turn_order[order_idx++] = -10;
            while (order_idx < MAX_COMBAT_SHIPS) {
                MOX::ship_turn_order[order_idx++] = -1;
            }
            Reset_Strict_Combat_Sequence();

            RUSS::Mox_Update();
            COMBAT1::start_of_turn = 1;
            MOX::cur_ptr = -1;
            int16_t next_ship_flag = Advance_Next_Ship(
                active_player_id, COMBAT_SHIP_ACTION_NONE);

            if (next_ship_flag == 1) {
                time_warp_phase_active = false;
                return;
            }

            if (next_ship_flag != 1) {
                fields::Assign_Auto_Function(CMBTDRW1::Draw_Main_Combat_Screen, 2);
                MOX::skan_flag = 0;
                MOX::board_flag = 0;
                RUSS::Mox_Update();

                if (MOX::combat_total_ship_count > 0) {
                    for (int16_t i = 1; i < MOX::combat_total_ship_count; ++i) {
                        if (COMBINIT::Is_Combat_Satellite(&MOX::combat_data[i]) &&
                            MOX::combat_data[i].unit_status == 0) {
                            if (CMBTMOV1::Ship_Is_In_Stasis(i) == 0) {
                                CMBTMOV1::Rotate_Ship(i, (MOX::combat_data[i].facing_dir + 2) & 0xf, (MOX::combat_data[i].x - MOX::cur_combat_x) * 20, (MOX::combat_data[i].y - MOX::cur_combat_y) * 20);
                            }
                        }
                    }
                }

                COMBAT1::start_of_turn = 0;

                while (next_ship_flag == 0 &&
                    MOX::ship_turn_order[MOX::cur_ptr] >= 0) {
                    timer::Mark_Time();
                    MOX::FULL_DRAW_SCREEN_FLAG = 0;
                    int16_t m_left = MOX::combat_data[MOX::cur_ship].movement_left;

                    if (Ship_Controlled_By_Computer(MOX::cur_ship) != 0 &&
                        (MOX::net_flag == 0 || MOX::combat_data[MOX::cur_ship].owner == MOX::PLAYER_NUM)) {
                        if (MOX::net_flag != 0 && MOX::PLAYER_NUM != MOX::net_destination) {
                            Send_Tactical_Combat_Message(MOX::net_destination, TACTICAL_NET_MSG_AUTO_TURN, 0, 0, 0);
                        }
                        CMBTAI::Do_Auto_Ship_Turn();
                        if (MOX::FULL_DRAW_SCREEN_FLAG != 0) {
                            Add_Tactical_Combat_Fields();
                            Assign_Combat_Grids(MOX::cur_ship);
                            CMBTDRW1::Refresh_Combat_Screen_Full();
                        }
                        MOX::ship_turn_order[MOX::cur_ptr] = -1;
                        next_ship_flag = Advance_Next_Ship(
                            active_player_id, COMBAT_SHIP_ACTION_AUTOMATIC);
                        if (MOX::net_flag != 0) {
                            NETMOX::Net_Debug_Log("Wait_Until_Net_Opponent_Finished: caller=%s:%d", __FILE__, __LINE__);
                            CMBTMOV1::Wait_Until_Net_Opponent_Finished(1);
                        }
                        timer::Release_Time(1);
                        continue;
                    } else {
                        MOX::cur_ship = MOX::ship_turn_order[MOX::cur_ptr];
                        if (CMBTMOV1::Ship_Is_In_Stasis(MOX::cur_ship) == 0) {
                            if (MOX::combat_init != 0 || MOX::combat_data[MOX::cur_ship].owner == active_player_id) {
                                if (MOX::net_flag == 0 || MOX::combat_data[MOX::cur_ship].owner == MOX::PLAYER_NUM) {
                                    int16_t input_val = 0;
                                    if (CMBTFIRE::Is_Combat_Ship_Dead(MOX::cur_ship) != 0) {
                                        input_val = MOX::done_button;
                                    } else {
                                        input_val = Get_Tactical_Input();
                                    }

                                    for (int16_t m_idx = 0; m_idx < MAX_COMBAT_FLYERS; ++m_idx) {
                                        if (MOX::missile_field[m_idx] == input_val) {
                                            if (CMBTMIS::Missile_Owner(m_idx) != MOX::combat_data[MOX::cur_ship].owner) {
                                                if (MOX::net_flag != 0 && MOX::PLAYER_NUM != MOX::net_destination) {
                                                    Send_Tactical_Combat_Message(MOX::net_destination, TACTICAL_NET_MSG_FIRE_WEAPON, m_idx, 1, 0);
                                                }
                                                CMBTFIRE::Fire_Ship(MOX::cur_ship, MOX::combat_data[MOX::cur_ship].x * 20, MOX::combat_data[MOX::cur_ship].y * 20, m_idx, MOX::missile[m_idx].x, MOX::missile[m_idx].y, 1);
                                                Add_Tactical_Combat_Fields();
                                                Assign_Combat_Grids(MOX::cur_ship);
                                                for (int16_t i = 0; i < 8; ++i) {
                                                    if (MOX::combat_data[MOX::cur_ship].weapons[i].active == 0) {
                                                        MOX::combat_data[MOX::cur_ship].weapons[i].active = 1;
                                                    }
                                                }
                                                if (MOX::net_flag != 0) {
                                                    NETMOX::Net_Debug_Log("Wait_Until_Net_Opponent_Finished: caller=%s:%d", __FILE__, __LINE__);
                                                    CMBTMOV1::Wait_Until_Net_Opponent_Finished(1);
                                                }
                                            }
                                            input_val = 0;
                                            break;
                                        }
                                    }

                                    if (input_val == MOX::msl_impact_warning_button) {
                                        MOX::msl_impact_warning_flag = 1 - MOX::msl_impact_warning_flag;
                                        MOX::saved_msl_warning = MOX::msl_impact_warning_flag;
                                        if (MOX::net_flag != 0) {
                                            NETMOX::Mox_Send_Message(MOX::player[MOX::net_destination].network_player_id, TACTICAL_NET_PACKET_MISSILE_WARNING, &MOX::msl_impact_warning_flag, sizeof(MOX::msl_impact_warning_flag));
                                        }
                                        input_val = 0;
                                    }

                                    if (Consume_Combat_Side_Wait_Input(&input_val)) {
                                        if (MOX::net_flag != 0 && MOX::PLAYER_NUM != MOX::net_destination) {
                                            Send_Combat_Side_Wait_Message(MOX::net_destination);
                                        }
                                        next_ship_flag = Finish_Next_Ship(
                                            Select_After_Combat_Side_Wait());
                                        if (MOX::net_flag != 0) {
                                            NETMOX::Net_Debug_Log("Wait_Until_Net_Opponent_Finished: caller=%s:%d", __FILE__, __LINE__);
                                            CMBTMOV1::Wait_Until_Net_Opponent_Finished(1);
                                        }
                                    }

                                    if (input_val == MOX::speedx2_button) {
                                        MOX::speedx2_flag = 1 - MOX::speedx2_flag;
                                        input_val = 0;
                                    }

                                    if (input_val == MOX::grid_button) {
                                        MOX::grid_flag = 1 - MOX::grid_flag;
                                        CMBTDRW1::Refresh_Combat_Screen_Full();
                                        input_val = 0;
                                    }

                                    if (input_val == MOX::draw_legal_boxes_button) {
                                        MOX::draw_legal_boxes_flag = 1 - MOX::draw_legal_boxes_flag;
                                        CMBTDRW1::Refresh_Combat_Screen_Full();
                                        input_val = 0;
                                    }

                                    if (input_val == MOX::shields_button) {
                                        MOX::shields_flag = 1 - MOX::shields_flag;
                                        input_val = 0;
                                    }

                                    if (input_val == MOX::super_fast_button) {
                                        MOX::super_fast_flag = 1;
                                        if (MOX::combat_data[MOX::cur_ship].owner == MOX::combat_attacker_player) {
                                            MOX::auto_combat_flag[0] = 1 - MOX::auto_combat_flag[0];
                                            MOX::auto_combat = MOX::auto_combat_flag[0];
                                        } else {
                                            MOX::auto_combat_flag[1] = 1 - MOX::auto_combat_flag[1];
                                            MOX::auto_combat = MOX::auto_combat_flag[1];
                                        }
                                        CMBTDRW1::Draw_Super_Fast_Screen();
                                    }

                                    if (input_val == MOX::self_destruct_button) {
                                        char buf[80];
                                        if (MOX::cur_ship == 0) {
                                            KEN::Ken_Get_Text_Message(56, buf, sizeof(buf));
                                            palette::Refresh_Palette();
                                            GENDRAW::Message_Box(buf);
                                        } else {
                                            KEN::Ken_Get_Text_Message(57, buf, sizeof(buf));
                                            palette::Refresh_Palette();
                                            if (GENDRAW::Do_Confirmation_Box(buf) != 0) {
                                                MOX::order_info_flag = 1 - MOX::order_info_flag;
                                                CMBTDRW1::Move_Options_Background();
                                                if (MOX::net_flag != 0 && MOX::PLAYER_NUM != MOX::net_destination) {
                                                    Send_Tactical_Combat_Message(MOX::net_destination, TACTICAL_NET_MSG_SELF_DESTRUCT, 0, 0, 0);
                                                }
                                                MOX::combat_data[MOX::cur_ship].drive_current_hits = 0;
                                                int8_t saved_ftl_type = MOX::combat_data[MOX::cur_ship].ftl_type;
                                                MOX::combat_data[MOX::cur_ship].ftl_type = 1;
                                                CMBTFIRE::Destroy_Ship(MOX::cur_ship, MOX::combat_data[MOX::cur_ship].x * 20, MOX::combat_data[MOX::cur_ship].y * 20, CMBTFIRE::Is_Combat_Ship_Dead(MOX::cur_ship));
                                                MOX::combat_data[MOX::cur_ship].ftl_type = saved_ftl_type;
                                                MOX::ship_turn_order[MOX::cur_ptr] = -1;
                                                next_ship_flag = Next_Ship(active_player_id);
                                                if (MOX::net_flag != 0) {
                                                    NETMOX::Net_Debug_Log("Wait_Until_Net_Opponent_Finished: caller=%s:%d", __FILE__, __LINE__);
                                                    CMBTMOV1::Wait_Until_Net_Opponent_Finished(1);
                                                }
                                            }
                                        }
                                        CMBTDRW1::Refresh_Combat_Screen_Full();
                                        input_val = 0;
                                    }

                                    if (input_val == MOX::msl_warning_button) {
                                        if (MOX::net_flag != 0 && MOX::PLAYER_NUM != MOX::net_destination) {
                                            Send_Tactical_Combat_Message(MOX::net_destination, TACTICAL_NET_MSG_CYCLE_MISSILE_WARNING, 0, 0, 0);
                                        }
                                        MOX::missile_warning = (MOX::missile_warning + 1) % 3;
                                        if (MOX::net_flag != 0) {
                                            NETMOX::Net_Debug_Log("Wait_Until_Net_Opponent_Finished: caller=%s:%d", __FILE__, __LINE__);
                                            CMBTMOV1::Wait_Until_Net_Opponent_Finished(1);
                                        }
                                        Add_Tactical_Combat_Fields();
                                        Assign_Combat_Grids(MOX::cur_ship);
                                        input_val = 0;
                                    }

                                    if (input_val == MOX::auto_button) {
                                        if (MOX::net_flag != 0 && MOX::PLAYER_NUM != MOX::net_destination) {
                                            Send_Tactical_Combat_Message(MOX::net_destination, TACTICAL_NET_MSG_TOGGLE_AUTO_COMBAT, 0, 0, 0);
                                        }
                                        if (MOX::combat_data[MOX::cur_ship].owner == MOX::combat_attacker_player) {
                                            MOX::auto_combat_flag[0] = 1 - MOX::auto_combat_flag[0];
                                            MOX::auto_combat = MOX::auto_combat_flag[0];
                                        } else {
                                            MOX::auto_combat_flag[1] = 1 - MOX::auto_combat_flag[1];
                                            MOX::auto_combat = MOX::auto_combat_flag[1];
                                        }
                                        if (MOX::auto_combat == 0) {
                                            MOX::super_fast_flag = 0;
                                        }
                                        fields::Clear_Fields();
                                        MOX::auto_button = fields::Add_Hidden_Field(0, 0, 639, 479, "", 0);
                                        if (MOX::net_flag != 0) {
                                            NETMOX::Net_Debug_Log("Wait_Until_Net_Opponent_Finished: caller=%s:%d", __FILE__, __LINE__);
                                            CMBTMOV1::Wait_Until_Net_Opponent_Finished(1);
                                        }
                                        input_val = 0;
                                    }

                                    if (input_val == MOX::weapons_button) {
                                        MOX::last_ship_status = -1;
                                        Set_Combat_Help_Fields();
                                    }

                                    if (input_val == MOX::specials_button) {
                                        MOX::last_ship_status = -1;
                                        Set_Combat_Help_Fields();
                                    }

                                    if (input_val == MOX::wait_button) {
                                        if (MOX::net_flag != 0 && MOX::PLAYER_NUM != MOX::net_destination) {
                                            Send_Tactical_Combat_Message(MOX::net_destination, TACTICAL_NET_MSG_WAIT, 0, 0, 0);
                                        }
                                        next_ship_flag = Advance_Next_Ship(
                                            active_player_id,
                                            COMBAT_SHIP_ACTION_WAIT);
                                        if (MOX::net_flag != 0) {
                                            NETMOX::Net_Debug_Log("Wait_Until_Net_Opponent_Finished: caller=%s:%d", __FILE__, __LINE__);
                                            CMBTMOV1::Wait_Until_Net_Opponent_Finished(1);
                                        }
                                        input_val = 0;
                                    }

                                    for (int16_t w_idx = 0; w_idx < 8; ++w_idx) {
                                        if (MOX::weapon_button[w_idx] == input_val) {
                                            if (MOX::ship_status_display == 0) {
                                                if (MOX::net_flag != 0 && MOX::PLAYER_NUM != MOX::net_destination) {
                                                    Send_Tactical_Combat_Message(MOX::net_destination, TACTICAL_NET_MSG_TOGGLE_WEAPON, w_idx, 0, 0);
                                                }
                                                MOX::combat_data[MOX::cur_ship].weapons[w_idx].active -= 1;
                                                if (MOX::combat_data[MOX::cur_ship].weapons[w_idx].active < -1) {
                                                    MOX::combat_data[MOX::cur_ship].weapons[w_idx].active = 1;
                                                }
                                                if (MOX::net_flag != 0) {
                                                    NETMOX::Net_Debug_Log("Wait_Until_Net_Opponent_Finished: caller=%s:%d", __FILE__, __LINE__);
                                                    CMBTMOV1::Wait_Until_Net_Opponent_Finished(1);
                                                }
                                                MOX::last_ship_status = -1;
                                            }
                                            input_val = 0;
                                            break;
                                        }
                                    }

                                    if (input_val == MOX::cmbt_info_button) {
                                        MOX::order_info_flag = 1 - MOX::order_info_flag;
                                        CMBTDRW1::Move_Options_Background();
                                        input_val = 0;
                                    }

                                    if (input_val == MOX::scan_button) {
                                        MOX::board_flag = 0;
                                        Assign_Combat_Grids(MOX::cur_ship);
                                        input_val = 0;
                                    }

                                    if (input_val == MOX::flee_button) {
                                        if (MOX::combat_data[MOX::cur_ship].is_retreating == 0) {
                                            int16_t msg_id = 0;
                                            if (Enemy_Dissipator_Exists(MOX::combat_data[MOX::cur_ship].owner) == 0) {
                                                if (MOX::cur_ship == 0 ||
                                                    COMBINIT::Is_Combat_Satellite(
                                                        &MOX::combat_data[MOX::cur_ship])) {
                                                    msg_id = 59;
                                                } else if (MOX::combat_data[MOX::cur_ship].current_speed < 1) {
                                                    msg_id = 79;
                                                }
                                            } else {
                                                msg_id = 58;
                                            }
                                            if (msg_id != 0) {
                                                char buf[80];
                                                KEN::Ken_Get_Text_Message(msg_id, buf, sizeof(buf));
                                                palette::Refresh_Palette();
                                                GENDRAW::Message_Box(buf);
                                                CMBTDRW1::Refresh_Combat_Screen_Full();
                                            } else {
                                                if (MOX::net_flag != 0 && MOX::PLAYER_NUM != MOX::net_destination) {
                                                    Send_Tactical_Combat_Message(MOX::net_destination, TACTICAL_NET_MSG_RETREAT, 0, 0, 0);
                                                }
                                                MOX::combat_data[MOX::cur_ship].is_retreating = 1;
                                                MOX::ship_turn_order[MOX::cur_ptr] = -1;
                                                next_ship_flag = Next_Ship(active_player_id);
                                                if (MOX::net_flag != 0) {
                                                    NETMOX::Net_Debug_Log("Wait_Until_Net_Opponent_Finished: caller=%s:%d", __FILE__, __LINE__);
                                                    CMBTMOV1::Wait_Until_Net_Opponent_Finished(1);
                                                }
                                            }
                                        }
                                        input_val = 0;
                                    }

                                    if (input_val == MOX::board_button) {
                                        MOX::skan_flag = 0;
                                        if (CMBTFIR2::Already_Boarded(MOX::cur_ship) != 0) {
                                            char buf[80];
                                            KEN::Ken_Get_Text_Message(60, buf, sizeof(buf));
                                            palette::Refresh_Palette();
                                            GENDRAW::Message_Box(buf);
                                            CMBTDRW1::Refresh_Combat_Screen_Full();
                                            MOX::board_flag = 0;
                                        } else {
                                            Add_Tactical_Combat_Fields();
                                            Assign_Combat_Grids(MOX::cur_ship);
                                        }
                                        input_val = 0;
                                    }

                                    if (input_val == MOX::map_field) {
                                        COMBINIT::Snap_Center_Combat_Screen((MOX::map_x * 2) / 3, (MOX::map_y * 2 * MOX::combat_grid_height) / (3 * ORIGINAL_COMBAT_GRID_HEIGHT));
                                        input_val = 0;
                                    }

                                    int16_t ptr_x = mouse::Pointer_X();
                                    if ((ptr_x < 5 && MOX::cur_combat_x > 0) ||
                                        (mouse::Pointer_X() > 635 && MOX::cur_combat_x < MOX::combat_grid_width - COMBAT_GRID_VIEW_WIDTH) ||
                                        (mouse::Pointer_Y() < 5 && MOX::cur_combat_y > 0) ||
                                        (mouse::Pointer_Y() > 475 && MOX::cur_combat_y < MOX::combat_grid_height - COMBAT_GRID_VIEW_HEIGHT)) {
                                        COMBINIT::Scroll_Combat_Screen();
                                    }

                                    if (input_val == MOX::center_button) {
                                        COMBINIT::Snap_Center_Combat_Screen(MOX::combat_data[MOX::cur_ship].x, MOX::combat_data[MOX::cur_ship].y);
                                    }

                                    if ((int16_t)(input_val + MOX::space_field) == 0) {
                                        int16_t face_x = MOX::cur_combat_x + MOX::xpos / 2;
                                        int16_t face_y = MOX::cur_combat_y + MOX::ypos / 2;
                                        int8_t original_owner = MOX::combat_data[MOX::cur_ship].owner;
                                        if (MOX::net_flag != 0 && MOX::PLAYER_NUM != MOX::net_destination) {
                                            Send_Tactical_Combat_Message(MOX::net_destination, TACTICAL_NET_MSG_TURN_TO_FACE, face_x, face_y, 0);
                                        }
                                        CMBTMOV1::Turn_To_Face(MOX::cur_ship, face_x, face_y);
                                        Advance_After_Ship_Owner_Change(original_owner, active_player_id, &next_ship_flag);
                                        if (MOX::net_flag != 0) {
                                            NETMOX::Net_Debug_Log("Wait_Until_Net_Opponent_Finished: caller=%s:%d", __FILE__, __LINE__);
                                            CMBTMOV1::Wait_Until_Net_Opponent_Finished(1);
                                        }
                                        input_val = 0;
                                    }

                                    if (input_val == MOX::space_field) {
                                        int16_t tx = MOX::cur_combat_x + MOX::xpos / 2;
                                        int16_t ty = MOX::cur_combat_y + MOX::ypos / 2;
                                        int16_t img_num = mouse::Get_Pointer_Image_Number();

                                        if (img_num >= 4 && img_num <= 11 && MOX::combat_data[MOX::cur_ship].size_class != 0) {
                                            if (MOX::combat_data[MOX::cur_ship].size_class <= 2) {
                                                tx = MOX::cur_combat_x - 1 + (MOX::xpos + 1) / 2;
                                                ty = MOX::cur_combat_y - 1 + (MOX::ypos + 1) / 2;
                                            } else {
                                                tx = MOX::cur_combat_x - 1 + MOX::xpos / 2;
                                                ty = MOX::cur_combat_y - 1 + MOX::ypos / 2;
                                            }
                                        }

                                        int16_t tgt_idx = -1;
                                        if (tx >= 0 && tx < MOX::combat_grid_width && ty >= 0 && ty < MOX::combat_grid_height) {
                                            tgt_idx = MOX::combat_grid[tx][ty];
                                        }
                                        if (MOX::board_flag + MOX::skan_flag <= 0) {
                                            if (tgt_idx == 2000) {
                                                int8_t orig_owner = MOX::combat_data[MOX::cur_ship].owner;
                                                if (MOX::net_flag != 0 && MOX::PLAYER_NUM != MOX::net_destination) {
                                                    Send_Tactical_Combat_Message(MOX::net_destination, TACTICAL_NET_MSG_MOVE_SHIP, tx, ty, 0);
                                                }
                                                int16_t move_cost = CMBTMOV1::Move_Cost(MOX::cur_ship, tx, ty);
                                                if (MOX::combat_data[MOX::cur_ship].movement_left < 0) {
                                                    MOX::combat_data[MOX::cur_ship].movement_left = 0;
                                                }
                                                int16_t used_tp1 = CMBTFIR2::Used_Teleporter(MOX::cur_ship);
                                                int16_t act_cost = CMBTMOV1::Move_Ship(MOX::cur_ship, tx, ty, move_cost);
                                                int16_t used_tp2 = CMBTFIR2::Used_Teleporter(MOX::cur_ship);
                                                if (used_tp1 == used_tp2) {
                                                    MOX::combat_data[MOX::cur_ship].movement_left -= act_cost;
                                                    if (MOX::combat_data[MOX::cur_ship].movement_left < 0) {
                                                        MOX::combat_data[MOX::cur_ship].movement_left = 0;
                                                    }
                                                }
                                                Assign_Combat_Grids(MOX::cur_ship);
                                                CMBTDRW1::Refresh_Combat_Screen_Full();
                                                Advance_After_Ship_Owner_Change(orig_owner, active_player_id, &next_ship_flag);
                                                if (MOX::net_flag != 0) {
                                                    NETMOX::Net_Debug_Log("Wait_Until_Net_Opponent_Finished: caller=%s:%d", __FILE__, __LINE__);
                                                    CMBTMOV1::Wait_Until_Net_Opponent_Finished(1);
                                                }
                                            } else if (tgt_idx >= 0 && tgt_idx < MOX::combat_total_ship_count && MOX::combat_data[tgt_idx].unit_status == 0) {
                                                if (MOX::combat_data[MOX::cur_ship].owner == MOX::combat_data[tgt_idx].owner) {
                                                    if (MOX::combat_init == 0) {
                                                        if (MOX::net_flag != 0 && MOX::PLAYER_NUM != MOX::net_destination) {
                                                            Send_Tactical_Combat_Message(MOX::net_destination, TACTICAL_NET_MSG_SELECT_SHIP, tgt_idx, 0, 0);
                                                        }
                                                        Set_Cur_Ship_To(tgt_idx);
                                                        if (MOX::net_flag != 0) {
                                                            NETMOX::Net_Debug_Log("Wait_Until_Net_Opponent_Finished: caller=%s:%d", __FILE__, __LINE__);
                                                            CMBTMOV1::Wait_Until_Net_Opponent_Finished(1);
                                                        }
                                                    }
                                                } else {
                                                    if (CMBTMOV1::Ship_Is_In_Stasis(tgt_idx) != 0) {
                                                        char buf[80];
                                                        KEN::Ken_Get_Text_Message(61, buf, sizeof(buf));
                                                        palette::Set_Palette_Gradient(144, 32, 0, 0, 0, 0, 0, 0);
                                                        palette::Refresh_Palette();
                                                        if (GENDRAW::Do_Confirmation_Box(buf) != 0) {
                                                            if (MOX::net_flag != 0 && MOX::PLAYER_NUM != MOX::net_destination) {
                                                                Send_Tactical_Combat_Message(MOX::net_destination, TACTICAL_NET_MSG_REMOVE_STASIS, tgt_idx, 0, 0);
                                                            }
                                                            CMBTFIRE::Remove_Stasis_Beam(tgt_idx);
                                                            if (MOX::net_flag != 0) {
                                                                NETMOX::Net_Debug_Log("Wait_Until_Net_Opponent_Finished: caller=%s:%d", __FILE__, __LINE__);
                                                                CMBTMOV1::Wait_Until_Net_Opponent_Finished(1);
                                                            }
                                                        }
                                                        CMBTDRW1::Refresh_Combat_Screen_Full();
                                                    } else if (MOX::combat_data[tgt_idx].special_status_flag == 4) {
                                                        char buf[80];
                                                        KEN::Ken_Get_Text_Message(80, buf, sizeof(buf));
                                                        palette::Refresh_Palette();
                                                        GENDRAW::Message_Box(buf);
                                                        CMBTDRW1::Refresh_Combat_Screen_Full();
                                                    } else {
                                                        if (MOX::net_flag != 0 && MOX::PLAYER_NUM != MOX::net_destination) {
                                                            Send_Tactical_Combat_Message(MOX::net_destination, TACTICAL_NET_MSG_FIRE_WEAPON, tgt_idx, 0, 0);
                                                        }
                                                        CMBTFIRE::Fire_Ship(MOX::cur_ship, MOX::combat_data[MOX::cur_ship].x * 20, MOX::combat_data[MOX::cur_ship].y * 20, tgt_idx, MOX::combat_data[tgt_idx].x * 20, MOX::combat_data[tgt_idx].y * 20, 0);
                                                        Add_Tactical_Combat_Fields();
                                                        Assign_Combat_Grids(MOX::cur_ship);
                                                        for (int16_t w = 0; w < 8; ++w) {
                                                            if (MOX::combat_data[MOX::cur_ship].weapons[w].active == 0) {
                                                                MOX::combat_data[MOX::cur_ship].weapons[w].active = 1;
                                                            }
                                                        }
                                                        if (MOX::net_flag != 0) {
                                                            NETMOX::Net_Debug_Log("Wait_Until_Net_Opponent_Finished: caller=%s:%d", __FILE__, __LINE__);
                                                            CMBTMOV1::Wait_Until_Net_Opponent_Finished(1);
                                                        }
                                                    }
                                                }
                                            }
                                        } else if (MOX::skan_flag != 0 && tgt_idx >= 0 && tgt_idx < MOX::combat_total_ship_count && MOX::combat_data[tgt_idx].unit_status == 0) {
                                            CMBTDRW1::Display_Combat_View_Ship(tgt_idx);
                                        } else if (MOX::board_flag != 0 && tgt_idx >= 0 && tgt_idx < MOX::combat_total_ship_count && MOX::combat_data[tgt_idx].unit_status == 0) {
                                            Initiate_Boarding_Action(MOX::cur_ship, tgt_idx);
                                        }
                                        input_val = 0;
                                    }

                                    if (input_val == end_of_turn_button) {
                                        if (MOX::net_flag != 0 && MOX::PLAYER_NUM != MOX::net_destination) {
                                            Send_Tactical_Combat_Message(MOX::net_destination, TACTICAL_NET_MSG_END_TURN, MOX::cur_combat_x, MOX::cur_combat_y, 0);
                                        }
                                        if (MOX::combat_init == 0) {
                                            next_ship_flag = 1;
                                        } else {
                                            End_Side_Turn();
                                            next_ship_flag = Next_Ship(active_player_id);
                                        }
                                        if (MOX::net_flag != 0) {
                                            NETMOX::Net_Debug_Log("Wait_Until_Net_Opponent_Finished: caller=%s:%d", __FILE__, __LINE__);
                                            CMBTMOV1::Wait_Until_Net_Opponent_Finished(1);
                                        }
                                        input_val = 0;
                                    }

                                    if (input_val == MOX::done_button) {
                                        if (MOX::net_flag != 0 && MOX::PLAYER_NUM != MOX::net_destination) {
                                            Send_Tactical_Combat_Message(MOX::net_destination, TACTICAL_NET_MSG_SHIP_DONE, 0, 0, 0);
                                        }
                                        CMBTFIR2::Set_Combat_Ship_Done(MOX::cur_ship);
                                        MOX::ship_turn_order[MOX::cur_ptr] = -1;
                                        next_ship_flag = Next_Ship(active_player_id);
                                        if (MOX::net_flag != 0) {
                                            NETMOX::Net_Debug_Log("Wait_Until_Net_Opponent_Finished: caller=%s:%d", __FILE__, __LINE__);
                                            CMBTMOV1::Wait_Until_Net_Opponent_Finished(1);
                                        }
                                        fields::Set_Input_Delay(3);
                                    }

                                    if (next_ship_flag == 0) {
                                        if (MOX::FULL_DRAW_SCREEN_FLAG == 0 && MOX::combat_data[MOX::cur_ship].movement_left == m_left) {
                                            video::Set_Page_Off();
                                            CMBTDRW1::Draw_Main_Combat_Screen();
                                            RUSS::Mox_Sync_Update();
                                        } else {
                                            Add_Tactical_Combat_Fields();
                                            Assign_Combat_Grids(MOX::cur_ship);
                                            CMBTDRW1::Refresh_Combat_Screen_Full();
                                        }
                                    }

                                } else {
                                    int16_t net_processed = 0;
                                    while (net_processed == 0) {
                                        if (MOX::tactical_net_choice == 0) {
                                            timer::Mark_Time();
                                            if (MOX::net_flag != 0) {
                                                if (Get_Tactical_Input() == MOX::auto_button) {
                                                    CMBTAI::Toggle_Auto_Combat_Flags();
                                                }
                                            }
                                            video::Set_Page_Off();
                                            CMBTDRW1::Draw_Main_Combat_Screen();
                                            RUSS::Mox_Sync_Update();
                                            Get_Tactical_Input();
                                            timer::Release_Time(1);
                                        } else {
                                            net_processed = 1;
                                            COMBINIT::Snap_Center_Combat_Screen(MOX::new_message.viewport_x + 16, MOX::new_message.viewport_y + 9);
                                            int16_t f10 = MOX::new_message.arg1;
                                            int16_t f12 = MOX::new_message.arg2;
                                            int16_t f14 = MOX::new_message.arg3;
                                            int16_t f8 = MOX::new_message.message_code;
                                            game_random::Set_Random_Seed(MOX::new_message.random_seed);
                                            struct_::Clear_Structure(&MOX::new_message, sizeof(MOX::new_message));

                                            NETMOX::Net_Debug_Log(
                                                "tactical action consume local_player=%d action=%d args=%d,%d,%d turn=%d cur_ship=%d cur_ptr=%d",
                                                (int)MOX::PLAYER_NUM,
                                                (int)f8,
                                                f10,
                                                f12,
                                                f14,
                                                (int)MOX::combat_turn,
                                                MOX::cur_ship,
                                                (int)MOX::cur_ptr);
                                            switch (f8) {
                                                case TACTICAL_NET_MSG_SELF_DESTRUCT:
                                                    MOX::combat_data[MOX::cur_ship].drive_current_hits = 0;
                                                    {
                                                        int8_t saved_ftl_type = MOX::combat_data[MOX::cur_ship].ftl_type;
                                                        MOX::combat_data[MOX::cur_ship].ftl_type = 1;
                                                        CMBTFIRE::Destroy_Ship(MOX::cur_ship, MOX::combat_data[MOX::cur_ship].x * 20, MOX::combat_data[MOX::cur_ship].y * 20, CMBTFIRE::Is_Combat_Ship_Dead(MOX::cur_ship));
                                                        MOX::combat_data[MOX::cur_ship].ftl_type = saved_ftl_type;
                                                    }
                                                    MOX::tactical_net_choice = TACTICAL_NET_MSG_SELF_DESTRUCT;
                                                    MOX::ship_turn_order[MOX::cur_ptr] = -1;
                                                    next_ship_flag = Next_Ship(active_player_id);
                                                    break;
                                                case TACTICAL_NET_MSG_AUTO_TURN:
                                                    CMBTAI::Do_Auto_Ship_Turn();
                                                    MOX::ship_turn_order[MOX::cur_ptr] = -1;
                                                    next_ship_flag = Advance_Next_Ship(
                                                        active_player_id,
                                                        COMBAT_SHIP_ACTION_AUTOMATIC);
                                                    break;
                                                case TACTICAL_NET_MSG_SHIP_DONE:
                                                    MOX::ship_turn_order[MOX::cur_ptr] = -1;
                                                    next_ship_flag = Next_Ship(active_player_id);
                                                    break;
                                                case TACTICAL_NET_MSG_MOVE_SHIP: {
                                                    int8_t orig_owner = MOX::combat_data[MOX::cur_ship].owner;
                                                    int16_t move_cost = CMBTMOV1::Move_Cost(MOX::cur_ship, f10, f12);
                                                    if (MOX::combat_data[MOX::cur_ship].movement_left < 0) {
                                                        MOX::combat_data[MOX::cur_ship].movement_left = 0;
                                                    }
                                                    int16_t used_tp1 = CMBTFIR2::Used_Teleporter(MOX::cur_ship);
                                                    int16_t act_cost = CMBTMOV1::Move_Ship(MOX::cur_ship, f10, f12, move_cost);
                                                    int16_t used_tp2 = CMBTFIR2::Used_Teleporter(MOX::cur_ship);
                                                    if (used_tp1 == used_tp2) {
                                                        MOX::combat_data[MOX::cur_ship].movement_left -= act_cost;
                                                        if (MOX::combat_data[MOX::cur_ship].movement_left < 0) {
                                                            MOX::combat_data[MOX::cur_ship].movement_left = 0;
                                                        }
                                                    }
                                                    Assign_Combat_Grids(MOX::cur_ship);
                                                    CMBTDRW1::Refresh_Combat_Screen_Full();
                                                    Advance_After_Ship_Owner_Change(orig_owner, active_player_id, &next_ship_flag);
                                                    break;
                                                }
                                                case TACTICAL_NET_MSG_FIRE_WEAPON:
                                                    if (f12 == 0) {
                                                        CMBTFIRE::Fire_Ship(MOX::cur_ship, MOX::combat_data[MOX::cur_ship].x * 20, MOX::combat_data[MOX::cur_ship].y * 20, f10, MOX::combat_data[f10].x * 20, MOX::combat_data[f10].y * 20, f12);
                                                    } else {
                                                        CMBTFIRE::Fire_Ship(MOX::cur_ship, MOX::combat_data[MOX::cur_ship].x * 20, MOX::combat_data[MOX::cur_ship].y * 20, f10, MOX::missile[f10].x, MOX::missile[f10].y, f12);
                                                    }
                                                    Add_Tactical_Combat_Fields();
                                                    Assign_Combat_Grids(MOX::cur_ship);
                                                    for (int16_t i = 0; i < 8; ++i) {
                                                        if (MOX::combat_data[MOX::cur_ship].weapons[i].active == 0) {
                                                            MOX::combat_data[MOX::cur_ship].weapons[i].active = 1;
                                                        }
                                                    }
                                                    break;
                                                case TACTICAL_NET_MSG_CYCLE_MISSILE_WARNING:
                                                    MOX::missile_warning = (MOX::missile_warning + 1) % 3;
                                                    Add_Tactical_Combat_Fields();
                                                    Assign_Combat_Grids(MOX::cur_ship);
                                                    break;
                                                case TACTICAL_NET_MSG_TOGGLE_AUTO_COMBAT:
                                                    fields::Clear_Fields();
                                                    MOX::auto_button = fields::Add_Hidden_Field(0, 0, 639, 479, "", 0);
                                                    if (MOX::combat_data[MOX::cur_ship].owner == MOX::combat_attacker_player) {
                                                        MOX::auto_combat_flag[0] = 1 - MOX::auto_combat_flag[0];
                                                        MOX::auto_combat = MOX::auto_combat_flag[0];
                                                    } else {
                                                        MOX::auto_combat_flag[1] = 1 - MOX::auto_combat_flag[1];
                                                        MOX::auto_combat = MOX::auto_combat_flag[1];
                                                    }
                                                    break;
                                                case TACTICAL_NET_MSG_WAIT:
                                                    next_ship_flag = Advance_Next_Ship(
                                                        active_player_id,
                                                        COMBAT_SHIP_ACTION_WAIT);
                                                    break;
                                                case TACTICAL_NET_MSG_SIDE_WAIT:
                                                    next_ship_flag = Finish_Next_Ship(
                                                        Select_After_Combat_Side_Wait());
                                                    break;
                                                case TACTICAL_NET_MSG_TOGGLE_WEAPON:
                                                    MOX::combat_data[MOX::cur_ship].weapons[f10].active -= 1;
                                                    if (MOX::combat_data[MOX::cur_ship].weapons[f10].active < -1) {
                                                        MOX::combat_data[MOX::cur_ship].weapons[f10].active = 1;
                                                    }
                                                    break;
                                                case TACTICAL_NET_MSG_RETREAT:
                                                    MOX::combat_data[MOX::cur_ship].is_retreating = 1;
                                                    MOX::ship_turn_order[MOX::cur_ptr] = -1;
                                                    next_ship_flag = Next_Ship(active_player_id);
                                                    break;
                                                case TACTICAL_NET_MSG_TURN_TO_FACE: {
                                                    int8_t original_owner = MOX::combat_data[MOX::cur_ship].owner;
                                                    CMBTMOV1::Turn_To_Face(MOX::cur_ship, f10, f12);
                                                    Advance_After_Ship_Owner_Change(original_owner, active_player_id, &next_ship_flag);
                                                    break;
                                                }
                                                case TACTICAL_NET_MSG_REMOVE_STASIS:
                                                    CMBTFIRE::Remove_Stasis_Beam(f10);
                                                    break;
                                                case TACTICAL_NET_MSG_SELECT_SHIP:
                                                    if (MOX::combat_init == 0) {
                                                        Set_Cur_Ship_To(f10);
                                                    }
                                                    break;
                                                case TACTICAL_NET_MSG_BOARDING_ACTION:
                                                    if (f14 == 1) {
                                                        if (MOX::combat_data[MOX::cur_ship].special_status_flag == 1) {
                                                            if (MOX::apply_damage_to_planet == 1 && MOX::super_fast_flag == 0) {
                                                                CMBTSPEC::Draw_Cloak(MOX::cur_ship, 2);
                                                            }
                                                            MOX::combat_data[MOX::cur_ship].special_status_flag = 2;
                                                            Recalculate_Combat_Ship(MOX::cur_ship);
                                                        } else if (MOX::combat_data[MOX::cur_ship].special_status_flag == 4) {
                                                            if (MOX::apply_damage_to_planet == 1 && MOX::super_fast_flag == 0) {
                                                                CMBTSPEC::Draw_Cloak(MOX::cur_ship, 5);
                                                            }
                                                            MOX::combat_data[MOX::cur_ship].special_status_flag = 6;
                                                        }
                                                        CMBTFIR2::Set_Boarded_Flag(MOX::cur_ship);
                                                        MOX::combat_data[MOX::cur_ship].marine_count -= (int8_t)f12;
                                                        CMBTFIR2::Resolve_Raid(MOX::cur_ship, f10, &f12);
                                                        MOX::combat_data[MOX::cur_ship].marine_count += (int8_t)f12;
                                                    } else {
                                                        if (MOX::combat_data[MOX::cur_ship].special_status_flag == 1) {
                                                            if (MOX::apply_damage_to_planet == 1 && MOX::super_fast_flag == 0) {
                                                                CMBTSPEC::Draw_Cloak(MOX::cur_ship, 2);
                                                            }
                                                            MOX::combat_data[MOX::cur_ship].special_status_flag = 2;
                                                            Recalculate_Combat_Ship(MOX::cur_ship);
                                                        } else if (MOX::combat_data[MOX::cur_ship].special_status_flag == 4) {
                                                            if (MOX::apply_damage_to_planet == 1 && MOX::super_fast_flag == 0) {
                                                                CMBTSPEC::Draw_Cloak(MOX::cur_ship, 5);
                                                            }
                                                            MOX::combat_data[MOX::cur_ship].special_status_flag = 6;
                                                        }
                                                        CMBTFIR2::Set_Boarded_Flag(MOX::cur_ship);
                                                        MOX::combat_data[MOX::cur_ship].marine_count -= (int8_t)f12;
                                                        CMBTFIR2::Resolve_Capture(MOX::cur_ship, f10, f12, MOX::combat_data[f10].x * 20, MOX::combat_data[f10].y * 20);
                                                    }
                                                    break;
                                                case TACTICAL_NET_MSG_END_TURN:
                                                    if (MOX::combat_init == 0) {
                                                        next_ship_flag = 1;
                                                    } else {
                                                        End_Side_Turn();
                                                        next_ship_flag = Next_Ship(active_player_id);
                                                    }
                                                    break;
                                            }
                                            MOX::tactical_net_choice = 0;
                                            NETMOX::Net_Debug_Log("Wait_Until_Net_Opponent_Finished: caller=%s:%d", __FILE__, __LINE__);
                                            CMBTMOV1::Wait_Until_Net_Opponent_Finished(1);
                                        }
                                    }
                                }
                            }
                        } else {
                            MOX::ship_turn_order[MOX::cur_ptr] = -1;
                            next_ship_flag = Next_Ship(active_player_id);
                        }
                    }

                    if (next_ship_flag == 0) {
                        if (MOX::FULL_DRAW_SCREEN_FLAG == 0 && MOX::combat_data[MOX::cur_ship].movement_left == m_left) {
                            video::Set_Page_Off();
                            CMBTDRW1::Draw_Main_Combat_Screen();
                            RUSS::Mox_Sync_Update();
                        } else {
                            Add_Tactical_Combat_Fields();
                            Assign_Combat_Grids(MOX::cur_ship);
                            CMBTDRW1::Refresh_Combat_Screen_Full();
                        }
                    }
                    timer::Release_Time(1);
                }
            }
        }

        if (MOX::combat_init == 0 && MOX::combat_total_ship_count > 0) {
            for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
                if (Combat_Ship_Participates_In_Current_Pass(i)) {
                    if (absorber_keep_charge == 0 &&
                        MOX::combat_data[i].owner == active_player_id) {
                        MOX::combat_data[i].reflected_damage_pool = 0;
                    }
                }
            }
        }

        Seeking_Missiles(active_player_id, time_warp_only != 0);

        if (MOX::combat_total_ship_count > 0) {
            for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
                if (Combat_Ship_Participates_In_Current_Pass(i)) {
                    if (MOX::combat_init != 0 ||
                        MOX::combat_data[i].owner == active_player_id) {
                        for (int16_t j = 0; j < 8; ++j) {
                            if (MOX::combat_data[i].weapons[j].status == 2) {
                                MOX::combat_data[i].weapons[j].status = 1;
                            }
                        }
                        Update_Ordinary_Cloak(i);
                        const int8_t rephase_state =
                            rephase_cloak_next_turn != 0 ? 6 : 7;
                        if (MOX::combat_data[i].special_status_flag == rephase_state &&
                            MOX::combat_data[i].special_status_timer > 0 &&
                            COMBINIT::Does_Combat_Ship_Have_Special(
                                i, SPECIAL_PHASING_CLOAK) != 0) {
                            CMBTSPEC::Draw_Cloak(i, 4);
                            MOX::combat_data[i].special_status_flag = 4;

                            const uint16_t tractor_target_idx = MOX::combat_data[i].special_cooldown;
                            MOX::combat_data[i].special_cooldown = COMBAT_SHIP_NONE;
                            if (tractor_target_idx < MOX::combat_total_ship_count) {
                                Recalculate_Combat_Ship(tractor_target_idx);
                            }

                            for (int16_t tractor_ship_idx = 0;
                                 tractor_ship_idx < MOX::combat_total_ship_count;
                                 ++tractor_ship_idx) {
                                if (MOX::combat_data[tractor_ship_idx].special_cooldown == i) {
                                    MOX::combat_data[tractor_ship_idx].special_cooldown = COMBAT_SHIP_NONE;
                                }
                            }

                            Recalculate_Combat_Ship(i);
                            CMBTDRW1::Refresh_Combat_Screen_Full();

                            for (int16_t m = 0; m < MAX_COMBAT_FLYERS; ++m) {
                                if (MOX::missile[m].type > 0 && MOX::missile[m].target_unit_idx == i && MOX::missile[m].source_unit_idx != i) {
                                    CMBTMIS::Retarget_Missile(m);
                                }
                            }
                        }
                        MOX::combat_data[i].ai_range_tactic = (int8_t)CMBTFIR2::Max_Defensive_Fire_Range(i);
                    }
                }
            }
        }

        if (MOX::net_flag != 0) {
            if (MOX::PLAYER_NUM != active_player_id) {
                MOX::msl_impact_warning_flag = MOX::saved_msl_warn;
            }
            if (MOX::net_flag != 0) {
                NETMOX::Net_Debug_Log("Wait_Until_Net_Opponent_Finished: caller=%s:%d", __FILE__, __LINE__);
                CMBTMOV1::Wait_Until_Net_Opponent_Finished(1);
            }
        }
        time_warp_phase_active = false;
    }

    static int16_t Finish_Next_Ship(int16_t result) {
        if (result == 0) {
            if ((MOX::combat_init != 0 && center_on_next_ship) ||
                (CMBTDRW1::Ship_Completely_On_Screen(MOX::cur_ship) == 0 &&
                    COMBAT1::start_of_turn != 0)) {
                COMBINIT::Snap_Center_Combat_Screen(MOX::combat_data[MOX::cur_ship].x, MOX::combat_data[MOX::cur_ship].y);
            }

            COMBAT1::Add_Tactical_Combat_Fields();
            COMBAT1::Assign_Combat_Grids(MOX::cur_ship);
            CMBTDRW1::Refresh_Combat_Screen_Full();

            int8_t owner = MOX::combat_data[MOX::cur_ship].owner;
            if (owner == MOX::combat_attacker_player) {
                MOX::auto_combat = MOX::auto_combat_flag[0];
            } else {
                MOX::auto_combat = MOX::auto_combat_flag[1];
            }

            if (MOX::net_flag != 0) {
                if (owner == MOX::combat_attacker_player) {
                    MOX::net_destination = MOX::combat_defender_player;
                } else {
                    MOX::net_destination = MOX::combat_attacker_player;
                }

                if (owner == MOX::PLAYER_NUM) {
                    MOX::msl_impact_warning_flag = MOX::saved_msl_warning;
                    NETMOX::Mox_Send_Message(MOX::player[MOX::net_destination].network_player_id, TACTICAL_NET_PACKET_MISSILE_WARNING, &MOX::msl_impact_warning_flag, sizeof(MOX::msl_impact_warning_flag));
                }
                NETMOX::Net_Debug_Log("Wait_Until_Net_Opponent_Finished: caller=%s:%d", __FILE__, __LINE__);
                CMBTMOV1::Wait_Until_Net_Opponent_Finished(1);
            }
        }

        if (MOX::super_fast_flag != 0) {
            CMBTDRW1::Draw_Super_Fast_Screen();
        }
        return result;
    }

    static int16_t Advance_Next_Ship(
        int16_t active_player_id, COMBAT_SHIP_ACTION action) {
        (void)active_player_id;
        if (ship_initiative == 2) {
            return Finish_Next_Ship(Select_Next_Strict_Ship(action));
        }
        return Finish_Next_Ship(Select_Next_Classic_Ship(action));
    }

    int16_t __cdecl Next_Ship(int16_t unused_player_idx) {
        return Advance_Next_Ship(
            unused_player_idx, COMBAT_SHIP_ACTION_DONE);
    }

    void __cdecl Retreat_Ship(int16_t ship_idx) {
        if (!Validate_Retreating_Ship(ship_idx)) {
            return;
        }

        s_combat_data* ship = &MOX::combat_data[ship_idx];

        if (MOX::apply_damage_to_planet == 1 && MOX::super_fast_flag == 0) {
            if (CMBTDRW1::Ship_On_Screen(ship_idx) == 0) {
                COMBINIT::Snap_Center_Combat_Screen(ship->x, ship->y);
            }
            CMBTDRW1::Draw_Retreating_Ship(ship_idx);
            if (!Validate_Retreating_Ship(ship_idx)) {
                return;
            }
        }

        ship->unit_status = 5;
        ship->is_retreating = 2;
        CMBTFIRE::Retarget_Missiles_Aimed_At(ship_idx);
        COMBAT1::Remove_Continuing_Effect_Specials(ship_idx);
    }

    void __cdecl Add_Missile_Fields() {
        MOX::cur_missile_count = 0;

        graphics::Set_Window(0, 0, 0x27F, 0x15E);
        video::Clip_On();

        for (int16_t i = 0; i < MAX_COMBAT_FLYERS; ++i) {
            s_missile* missile = &MOX::missile[i];

            if (missile->type > 0 && CMBTMIS::On_Combat_Screen((missile->x - 8) / 20, (missile->y - 8) / 20)) {
                int16_t local_x = missile->x - (MOX::cur_combat_x * 20);
                int16_t local_y = missile->y - (MOX::cur_combat_y * 20);

                MOX::missile_field[i] = fields::Add_Clipped_Hidden_Field(
                    local_x - 16,
                    local_y - 16,
                    local_x - 8,
                    local_y - 8,
                    "",
                    0);

                MOX::combat_mouse_grid[i + COMBAT_MOUSE_FLYER_BASE].x1 = local_x - 16;
                MOX::combat_mouse_grid[i + COMBAT_MOUSE_FLYER_BASE].x2 = local_x - 8;
                MOX::combat_mouse_grid[i + COMBAT_MOUSE_FLYER_BASE].y1 = local_y - 16;
                MOX::combat_mouse_grid[i + COMBAT_MOUSE_FLYER_BASE].y2 = local_y - 8;
                MOX::combat_mouse_grid[i + COMBAT_MOUSE_FLYER_BASE].frame = 0;
                MOX::combat_mouse_grid[i + COMBAT_MOUSE_FLYER_BASE].icon = 12;

                MOX::cur_missile_count++;
            } else {
                MOX::missile_field[i] = -1000;

                MOX::combat_mouse_grid[i + COMBAT_MOUSE_FLYER_BASE].x1 = 0;
                MOX::combat_mouse_grid[i + COMBAT_MOUSE_FLYER_BASE].x2 = 0;
                MOX::combat_mouse_grid[i + COMBAT_MOUSE_FLYER_BASE].y1 = 0;
                MOX::combat_mouse_grid[i + COMBAT_MOUSE_FLYER_BASE].y2 = 0;
                MOX::combat_mouse_grid[i + COMBAT_MOUSE_FLYER_BASE].frame = 0;
                MOX::combat_mouse_grid[i + COMBAT_MOUSE_FLYER_BASE].icon = 0;
            }
        }

        video::Clip_Off();
        graphics::Reset_Window();
    }

    void __cdecl Assign_Mouse_Images() {
        if (MOX::game_type != 1 && MOX::combat_data[MOX::cur_ship].owner != MOX::PLAYER_NUM) {
            MOX::cmbt_mouse_list_clock.icon = MOX::combat_clock_frame;
            mouse::Set_Mouse_List(&MOX::cmbt_mouse_list_clock, 1);
            return;
        }

        if (MOX::order_info_flag != 1) {
            mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);
            return;
        }

        int grid_idx = 1;
        int16_t x_val = 0;

        for (int i = 0; i < COMBAT_GRID_VIEW_WIDTH; ++i) {
            int16_t y_val = 0;
            for (int j = 0; j < COMBAT_GRID_VIEW_HEIGHT; ++j) {
                MOX::combat_mouse_grid[grid_idx].x1 = x_val;
                MOX::combat_mouse_grid[grid_idx].x2 = x_val + 0x13;
                MOX::combat_mouse_grid[grid_idx].y1 = y_val;

                if (j == COMBAT_GRID_VIEW_MAX_Y) {
                    MOX::combat_mouse_grid[grid_idx].y2 = y_val + 0x16;
                } else {
                    MOX::combat_mouse_grid[grid_idx].y2 = y_val + 0x13;
                }

                grid_idx++;
                y_val += 0x14;
            }
            x_val += 0x14;
        }

        int16_t size_info = 0;
        if (MOX::combat_data[MOX::cur_ship].size_class > 0) {
            if (MOX::combat_data[MOX::cur_ship].size_class <= 2) {
                size_info = 10;
            } else {
                size_info = 20;
            }
        }

        for (int grid_x = COMBAT_GRID_VIEW_MAX_X; grid_x >= 0; --grid_x) {
            for (int grid_y = COMBAT_GRID_VIEW_MAX_Y; grid_y >= 0; --grid_y) {
                int idx = grid_x * COMBAT_GRID_VIEW_HEIGHT + grid_y + 1;
                int16_t target_ship = -1;
                int16_t world_x = grid_x + MOX::cur_combat_x;
                int16_t world_y = grid_y + MOX::cur_combat_y;

                if (world_x >= 0 && world_x < MOX::combat_grid_width && world_y >= 0 && world_y < MOX::combat_grid_height) {
                    target_ship = MOX::combat_grid[world_x][world_y];
                }

                if (MOX::skan_flag == 1) {
                    if (target_ship < 0 || target_ship >= MOX::combat_total_ship_count) {
                        MOX::combat_mouse_grid[idx].icon = 14;
                        MOX::combat_mouse_grid[idx].frame = 12;
                    } else {
                        MOX::combat_mouse_grid[idx].icon = 13;
                        MOX::combat_mouse_grid[idx].frame = 12;
                    }
                } else if (MOX::board_flag == 1) {
                    if (target_ship < 0 || target_ship >= MOX::combat_total_ship_count ||
                        MOX::combat_data[target_ship].owner == MOX::combat_data[MOX::cur_ship].owner) {
                        MOX::combat_mouse_grid[idx].icon = 2;
                        MOX::combat_mouse_grid[idx].frame = 12;
                    } else {
                        MOX::combat_mouse_grid[idx].icon = 3;
                        MOX::combat_mouse_grid[idx].frame = 12;
                    }
                } else {
                    if (target_ship < 0) {
                        MOX::combat_mouse_grid[idx].icon = 2;
                        MOX::combat_mouse_grid[idx].frame = 12;
                    } else if (target_ship >= MOX::combat_total_ship_count) {
                        if (target_ship == 2000) {
                            int16_t facing = CMBTMOV1::Get_Facing(
                                MOX::combat_data[MOX::cur_ship].x,
                                MOX::combat_data[MOX::cur_ship].y,
                                MOX::cur_combat_x + grid_x,
                                MOX::cur_combat_y + grid_y);
                            MOX::combat_mouse_grid[idx].icon = (facing / 2) + 4;
                            MOX::combat_mouse_grid[idx].frame = 12;
                        }
                    } else {
                        if (MOX::combat_data[target_ship].owner == MOX::combat_data[MOX::cur_ship].owner) {
                            if (target_ship == MOX::cur_ship) {
                                MOX::combat_mouse_grid[idx].icon = 2;
                                MOX::combat_mouse_grid[idx].frame = 12;
                            } else {
                                MOX::combat_mouse_grid[idx].icon = 1;
                                MOX::combat_mouse_grid[idx].frame = 0;
                            }
                        } else {
                            if (CMBTMOV1::Ship_Is_In_Stasis(target_ship) == 0) {
                                MOX::combat_mouse_grid[idx].icon = 12;
                                MOX::combat_mouse_grid[idx].frame = 12;
                            } else {
                                MOX::combat_mouse_grid[idx].icon = 1;
                                MOX::combat_mouse_grid[idx].frame = 0;
                            }
                        }
                    }
                }
            }
        }

        if (size_info > 0) {
            int idx_base = 558;
            int16_t x_val_base = size_info + 620;

            for (int grid_x = COMBAT_GRID_VIEW_MAX_X; grid_x >= 0; --grid_x) {
                int16_t y_end = size_info + 340;

                for (int grid_y = COMBAT_GRID_VIEW_MAX_Y; grid_y >= 0; --grid_y) {
                    int idx = idx_base + grid_y + 1;
                    int16_t icon = MOX::combat_mouse_grid[idx].icon;

                    if (icon >= 4 && icon <= 11) {
                        if (size_info == 20) {
                            if (grid_x < COMBAT_GRID_VIEW_MAX_X && grid_y < COMBAT_GRID_VIEW_MAX_Y) {
                                MOX::combat_mouse_grid[idx + COMBAT_GRID_VIEW_HEIGHT + 1].icon = icon;
                            }
                        } else if (size_info == 10) {
                            int16_t icon19 = MOX::combat_mouse_grid[idx + COMBAT_GRID_VIEW_HEIGHT + 1].icon;
                            if (icon19 < 4 || icon19 > 11) {
                                MOX::combat_mouse_grid[idx + COMBAT_GRID_VIEW_HEIGHT + 1].x1 = 0;
                                MOX::combat_mouse_grid[idx + COMBAT_GRID_VIEW_HEIGHT + 1].x2 = 0;
                                MOX::combat_mouse_grid[idx + COMBAT_GRID_VIEW_HEIGHT + 1].y1 = 0;
                                MOX::combat_mouse_grid[idx + COMBAT_GRID_VIEW_HEIGHT + 1].y2 = 0;
                            }

                            int16_t icon1 = MOX::combat_mouse_grid[idx + 1].icon;
                            if (icon1 < 4 || icon1 > 11) {
                                MOX::combat_mouse_grid[idx + 1].x1 = 0;
                                MOX::combat_mouse_grid[idx + 1].x2 = 0;
                                MOX::combat_mouse_grid[idx + 1].y1 = 0;
                                MOX::combat_mouse_grid[idx + 1].y2 = 0;
                            }

                            int16_t icon18 = MOX::combat_mouse_grid[idx + COMBAT_GRID_VIEW_HEIGHT].icon;
                            if (icon18 < 4 || icon18 > 11) {
                                MOX::combat_mouse_grid[idx + COMBAT_GRID_VIEW_HEIGHT].x1 = 0;
                                MOX::combat_mouse_grid[idx + COMBAT_GRID_VIEW_HEIGHT].x2 = 0;
                                MOX::combat_mouse_grid[idx + COMBAT_GRID_VIEW_HEIGHT].y1 = 0;
                                MOX::combat_mouse_grid[idx + COMBAT_GRID_VIEW_HEIGHT].y2 = 0;
                            }
                        }

                        MOX::combat_mouse_grid[idx].x1 = x_val_base;
                        MOX::combat_mouse_grid[idx].x2 = x_val_base + 0x13;
                        MOX::combat_mouse_grid[idx].y1 = y_end;
                        MOX::combat_mouse_grid[idx].y2 = y_end + 0x13;
                    }
                    y_end -= 20;
                }
                x_val_base -= 20;
                idx_base -= COMBAT_GRID_VIEW_HEIGHT;
            }
        }

        mouse::Set_Mouse_List(MOX::combat_mouse_grid, COMBAT_MOUSE_COUNT);
    }

    void __cdecl Set_Cur_Ship_To(int16_t ship_idx) {
        if (ship_idx == 0) {
            if (CMBTFIRE::Does_Combat_Planet_Have_Defenses() <= 0) {
                return;
            }
        }

        int16_t cur_ptr_idx = -1;
        int16_t total_ships = MOX::combat_total_ship_count;

        for (int16_t i = 0; i < total_ships; ++i) {
            int16_t order_val = MOX::ship_turn_order[i];
            if (order_val == -10) {
                break;
            }
            if (ship_idx == order_val) {
                cur_ptr_idx = i;
            }
        }

        if (cur_ptr_idx == -1) {
            for (int16_t i = 0; i < total_ships; ++i) {
                if (MOX::ship_turn_order[i] == -1) {
                    cur_ptr_idx = i;
                    MOX::ship_turn_order[i] = ship_idx;
                    break;
                }
            }
        }

        MOX::cur_ptr = cur_ptr_idx;
        MOX::cur_ship = ship_idx;

        CMBTDRW1::Refresh_Combat_Screen_Full();
    }

    void __cdecl Assign_Combat_Grids(int16_t active_ship_idx) {
        s_combat_data* ship;
        int16_t dim;
        int16_t i;
        int16_t j;

        for (i = 0; i < MOX::combat_grid_width; ++i) {
            for (j = 0; j < MOX::combat_grid_height; ++j) {
                MOX::combat_grid[i][j] = -1;
            }
        }

        if (MOX::order_info_flag == 1) {
            for (int16_t ship_idx = 0; ship_idx < MOX::combat_total_ship_count; ++ship_idx) {
                ship = &MOX::combat_data[ship_idx];

                if (ship->unit_status == 0) {
                    MOX::combat_grid[ship->x][ship->y] = ship_idx;

                    if (ship_idx != active_ship_idx || MOX::skan_flag == 1 || MOX::board_flag == 1) {
                        if (ship_idx == 0) {
                            if (MOX::cmbt_colony_id == -1) {
                                dim = 6;
                            } else {
                                int8_t planet_size = MOX::planet[MOX::colony[MOX::cmbt_colony_id].planet].size;
                                switch (planet_size) {
                                    case PLANET_SIZE_TINY: dim = 3; break;
                                    case PLANET_SIZE_SMALL: dim = 4; break;
                                    case PLANET_SIZE_MEDIUM: dim = 6; break;
                                    case PLANET_SIZE_LARGE: dim = 7; break;
                                    case PLANET_SIZE_HUGE: dim = 8; break;
                                    default: dim = 6; break;
                                }
                            }

                            for (i = 0; i < dim; ++i) {
                                for (j = 0; j < dim; ++j) {
                                    MOX::combat_grid[ship->x + i][ship->y + j] = 0;
                                }
                            }
                        } else {
                            dim = (ship->size_class + 1) / 2;
                            if (dim == 1) {
                                for (i = 0; i < 2; ++i) {
                                    for (j = 0; j < 2; ++j) {
                                        MOX::combat_grid[ship->x + i][ship->y + j] = ship_idx;
                                    }
                                }
                            } else if (dim > 1 && dim < 4) {
                                for (i = 0; i < 3; ++i) {
                                    for (j = 0; j < 3; ++j) {
                                        MOX::combat_grid[ship->x + i][ship->y + j] = ship_idx;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        if (MOX::cur_ship == active_ship_idx && active_ship_idx >= 0) {
            COMBAT1::Set_Legal_Moves(active_ship_idx, MOX::combat_data[active_ship_idx].movement_left, 1);
            COMBAT1::Assign_Mouse_Images();
        }
    }

    int16_t __cdecl Ship_Initiative_Value(int16_t ship_idx) {
        int16_t initiative = -5;

        if (ship_idx != -1) {
            s_combat_data* ship = &MOX::combat_data[ship_idx];
            initiative = (ship->tac_val_attack / 10) + ship->current_speed;
        }

        return initiative;
    }


} // namespace COMBAT1
