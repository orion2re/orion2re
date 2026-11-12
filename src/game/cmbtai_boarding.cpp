#include "pch.h"

namespace CMBTAI {
    int16_t __cdecl Boarding_Action_Type(int16_t attacker_idx, int16_t target_idx) {
        s_invasion_side_info attacker_invasion;
        s_invasion_side_info target_invasion;
        int16_t result = 0;
        bool can_board = true;
        int16_t attacker_owner = MOX::combat_data[attacker_idx].owner;

        if (target_idx == -1) {
            return 0;
        }

        int16_t dist = Range_To_Ship_Squares(attacker_idx, target_idx);
        bool has_transporters = COMBINIT::Does_Combat_Ship_Have_Special(
                                    attacker_idx, SPECIAL_TRANSPORTERS) != 0;
        int16_t reach = has_transporters
            ? BOARDING::board_range.transporters
            : BOARDING::board_range.normal;

        if (MOX::combat_data[target_idx].owner > 8 ||
            attacker_owner > 8 ||
            !COMBAT1::Combat_Ship_Is_Functional(target_idx) ||
            BOARDING::Filter_Boarding(attacker_idx, target_idx) !=
                BOARDING::e_boarding_filter::allowed ||
            MOX::combat_data[target_idx].special_status_flag == 4 ||
            CMBTFIR2::Already_Boarded(attacker_idx) != 0 ||
            target_idx == 0 ||
            MOX::combat_data[MOX::cur_ship].owner == MOX::combat_data[target_idx].owner ||
            dist > reach ||
            (!has_transporters && COMBAT1::Calc_Current_Speed(target_idx) > 0) ||
            MOX::combat_data[MOX::cur_ship].marine_count == 0) {
            can_board = false;
        } else if (has_transporters) {
            const bool uses_transporters =
                dist > BOARDING::board_range.normal ||
                MOX::combat_data[target_idx].current_speed > 0;
            if (uses_transporters) {
                int16_t bearing = CMBTDRW1::Relative_Bearing(target_idx, attacker_idx);
                int16_t shield_idx = CMBTFIRE::Facing_Shield(bearing);
                if (MOX::combat_data[target_idx].shield_arc_current[shield_idx] > 0 ||
                    COMBINIT::Does_Combat_Ship_Have_Special(
                        target_idx, SPECIAL_HARD_SHIELDS) != 0 ||
                    (BOARDING::boarding.damper_survival_divisor == 0 &&
                        COMBINIT::Does_Combat_Ship_Have_Special(
                            target_idx, SPECIAL_DAMPER_FIELD) != 0)) {
                    can_board = false;
                }
            }
        }

        if (can_board) {
            int16_t opp_player = (attacker_owner == MOX::combat_attacker_player) ? MOX::combat_defender_player : MOX::combat_attacker_player;

            if (MOX::combat_attacker_player < 9 && MOX::combat_defender_player < 9 && CMBTFIR2::Already_Boarded(attacker_idx) == 0) {
                if (attacker_owner == 8 /* Antarans */) {
                    result = 1;
                } else {
                    invasion::Get_Boarding_Info(attacker_owner, 1, 0, opp_player, 1, 0, &attacker_invasion, &target_invasion);

                    int16_t attacker_total = CMBTFIR2::Crew_Boarding_Combat_Bonus(attacker_idx) +
                        CMBTFIR2::Get_Fleet_Commando_Bonus(attacker_owner) +
                        attacker_invasion.unit_strengths[1];

                    int16_t target_total = CMBTFIR2::Crew_Boarding_Combat_Bonus(target_idx) +
                        (COMBINIT::Does_Combat_Ship_Have_Special(target_idx, SPECIAL_SECURITY_STATIONS)
                                ? invasion::troop.bonus.security_stations
                                : 0) +
                        CMBTFIR2::Get_Fleet_Commando_Bonus(opp_player) +
                        CMBTFIR2::Get_Fleet_Security_Bonus(opp_player) +
                        target_invasion.unit_strengths[1];

                    if (target_total - 20 < attacker_total) {
                        if (MOX::player[attacker_owner].traits[TRAIT_TELEPATHIC] != 0) {
                            result = 2;
                        } else {
                            result = 1;
                            if (MOX::combat_data[target_idx].marine_count <= MOX::combat_data[attacker_idx].marine_count &&
                                target_total - 10 < attacker_total) {
                                result = 2;
                            }
                        }
                    }
                }
            } else {
                result = 0;
            }
        }

        return result;
    }

    void __cdecl Resolve_Ai_Boarding(int16_t attacker_idx, int16_t target_idx, int16_t action_type) {
        if (action_type != 0) {
            int16_t dist = Range_To_Ship_Squares(attacker_idx, target_idx);
            bool has_transporters = (COMBINIT::Does_Combat_Ship_Have_Special(attacker_idx, SPECIAL_TRANSPORTERS) == 1);
            const bool uses_transporters =
                has_transporters &&
                (dist > BOARDING::board_range.normal ||
                    MOX::combat_data[target_idx].current_speed > 0);
            const bool damper_attrition =
                uses_transporters &&
                COMBINIT::Does_Combat_Ship_Have_Special(
                    target_idx, SPECIAL_DAMPER_FIELD) != 0;

            if (damper_attrition &&
                BOARDING::boarding.damper_survival_divisor == 0) {
                return;
            }

            s_combat_data* attacker = &MOX::combat_data[attacker_idx];
            int16_t marines_to_send = (uint8_t)attacker->marine_count >> 1;
            if (marines_to_send == 0) {
                marines_to_send = 1;
            }
            attacker->marine_count -= (int8_t)marines_to_send;

            if (damper_attrition) {
                marines_to_send /=
                    BOARDING::boarding.damper_survival_divisor;
                if (marines_to_send < 1) {
                    action_type = 0;
                }
            }

            if (MOX::net_flag != 0) {
                if ((action_type == 1 || action_type == 2) && (MOX::PLAYER_NUM != MOX::net_destination)) {
                    COMBAT1::Send_Tactical_Combat_Message(MOX::net_destination, TACTICAL_NET_MSG_BOARDING_ACTION, target_idx, marines_to_send, action_type);
                }
            }

            if (action_type == 1 /* RAID */) {
                CMBTFIR2::Set_Boarded_Flag(attacker_idx);
                CMBTFIR2::Resolve_Raid(attacker_idx, target_idx, &marines_to_send);
                attacker->marine_count += (int8_t)marines_to_send;
            } else if (action_type == 2 /* CAPTURE */) {
                CMBTFIR2::Set_Boarded_Flag(attacker_idx);
                CMBTFIR2::Set_Capturing_Flag(target_idx);
                s_combat_data* target = &MOX::combat_data[target_idx];
                CMBTFIR2::Resolve_Capture(attacker_idx, target_idx, marines_to_send, target->x * 20, target->y * 20);
            }

            MOX::board_flag = 0;
            if (MOX::net_flag != 0) {
                NETMOX::Net_Debug_Log("Wait_Until_Net_Opponent_Finished: caller=%s:%d", __FILE__, __LINE__);
                CMBTMOV1::Wait_Until_Net_Opponent_Finished(1);
            }
        }
    }
} // namespace CMBTAI
