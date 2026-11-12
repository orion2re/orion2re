#include "pch.h"

namespace COMBAT1 {
    void __cdecl Initiate_Boarding_Action(int16_t attacker_idx, int16_t defender_idx) {
        bool can_board = true;
        int16_t range = CMBTAI::Range_To_Ship_Squares(attacker_idx, defender_idx);
        int16_t has_transporters = COMBINIT::Does_Combat_Ship_Have_Special(attacker_idx, SPECIAL_TRANSPORTERS);
        int16_t max_range = has_transporters == 1
            ? BOARDING::board_range.transporters
            : BOARDING::board_range.normal;
        s_combat_data* target = &MOX::combat_data[defender_idx];
        char msg_buffer[80];
        int msg_idx = 0;

        const BOARDING::e_boarding_filter boarding_filter =
            BOARDING::Filter_Boarding(attacker_idx, defender_idx);
        if (boarding_filter != BOARDING::e_boarding_filter::allowed) {
            can_board = false;
            const char* warning = nullptr;
            if (boarding_filter ==
                BOARDING::e_boarding_filter::transporter_blocked_by_damper) {
                warning = "Damper Field prevents transporter boarding.";
            } else if (boarding_filter ==
                BOARDING::e_boarding_filter::target_in_black_hole) {
                warning = "Ships trapped in a black hole may not be boarded.";
            } else if (boarding_filter ==
                BOARDING::e_boarding_filter::attacker_in_black_hole) {
                warning = "Ships trapped in a black hole may not board.";
            } else if (boarding_filter ==
                BOARDING::e_boarding_filter::target_phased_out) {
                warning = "Phased out ships may not be boarded.";
            }
            if (warning != nullptr) {
                palette::Refresh_Palette();
                GENDRAW::Warning_Box(warning);
                CMBTDRW1::Refresh_Combat_Screen_Full();
            }
        } else if (target->owner > 8) {
            can_board = false;
            msg_idx = 0x3E; // Monsters cannot be boarded
        } else if (defender_idx == 0) {
            can_board = false;
            msg_idx = 0x3F; // Planets cannot be boarded
        } else if (MOX::combat_data[MOX::cur_ship].owner == target->owner) {
            can_board = false;
            msg_idx = (target->is_captured == 0) ? 0x40 : 0x41; // Cannot board own ship
        } else if (range > max_range) {
            can_board = false;
            msg_idx = 0x42; // Out of range
        } else if (has_transporters != 1 && COMBAT1::Calc_Current_Speed(defender_idx) > 0) {
            can_board = false;
            msg_idx = 0x43; // Ship moving too fast
        } else if (MOX::combat_data[MOX::cur_ship].marine_count == 0) {
            can_board = false;
            msg_idx = 0x44; // No marines
        } else if (has_transporters == 1) {
            if (range > BOARDING::board_range.normal ||
                MOX::combat_data[defender_idx].current_speed > 0) {
                if (BOARDING::boarding.damper_survival_divisor == 0 &&
                    COMBINIT::Does_Combat_Ship_Have_Special(
                        defender_idx, SPECIAL_DAMPER_FIELD) != 0) {
                    can_board = false;
                    palette::Refresh_Palette();
                    GENDRAW::Warning_Box(
                        "Damper Field prevents transporter boarding.");
                    CMBTDRW1::Refresh_Combat_Screen_Full();
                } else {
                    int16_t bearing = CMBTDRW1::Relative_Bearing(defender_idx, attacker_idx);
                    int16_t shield_idx = CMBTFIRE::Facing_Shield(bearing);
                    if (target->shield_arc_current[shield_idx] > 0) {
                        can_board = false;
                        KEN::Ken_Get_Text_Message(0x45, msg_buffer, sizeof(msg_buffer));
                        palette::Refresh_Palette();
                        GENDRAW::Warning_Box(msg_buffer);
                        CMBTDRW1::Refresh_Combat_Screen_Full();
                    }
                    if (COMBINIT::Does_Combat_Ship_Have_Special(defender_idx, SPECIAL_HARD_SHIELDS) != 0) {
                        can_board = false;
                        msg_idx = 0x46;
                    }
                }
            }
        }

        if (!can_board) {
            if (msg_idx != 0) {
                KEN::Ken_Get_Text_Message((int16_t)msg_idx, msg_buffer, sizeof(msg_buffer));
                palette::Refresh_Palette();
                GENDRAW::Warning_Box(msg_buffer);
                CMBTDRW1::Refresh_Combat_Screen_Full();
            }
        } else {
            int16_t marines_sent;
            int16_t result = CMBTFIR2::Init_Boarding_Action(MOX::cur_ship, &marines_sent);
            if (result != 0) {
                MOX::combat_data[MOX::cur_ship].marine_count -= (int8_t)marines_sent;

                if (has_transporters == 1 &&
                    (range > BOARDING::board_range.normal ||
                        MOX::combat_data[defender_idx].current_speed > 0)) {
                    if (COMBINIT::Does_Combat_Ship_Have_Special(defender_idx, SPECIAL_DAMPER_FIELD) != 0) {
                        marines_sent = marines_sent / BOARDING::boarding.damper_survival_divisor;
                    }
                }

                if (MOX::net_flag != 0 && (result == 1 || result == 2)) {
                    int16_t msg_player = MOX::combat_attacker_player;
                    if (MOX::combat_data[attacker_idx].owner == MOX::combat_attacker_player) {
                        msg_player = MOX::combat_defender_player;
                    }
                    if (MOX::PLAYER_NUM != msg_player) {
                        COMBAT1::Send_Tactical_Combat_Message(msg_player, TACTICAL_NET_MSG_BOARDING_ACTION, defender_idx, marines_sent, result);
                    }
                }

                if (result == 1) { // Raid
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
                    CMBTFIR2::Resolve_Raid(MOX::cur_ship, defender_idx, &marines_sent);
                    MOX::combat_data[MOX::cur_ship].marine_count += (int8_t)marines_sent;
                } else if (result == 2) { // Capture
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
                    CMBTFIR2::Resolve_Capture(MOX::cur_ship, defender_idx, marines_sent, target->x * 20, target->y * 20);
                }

                MOX::board_flag = 0;
                if (MOX::net_flag != 0) {
                    NETMOX::Net_Debug_Log("Wait_Until_Net_Opponent_Finished: caller=%s:%d", __FILE__, __LINE__);
                    CMBTMOV1::Wait_Until_Net_Opponent_Finished(1);
                }
            }
        }
    }
} // namespace COMBAT1
