#include "pch.h"

namespace CMBTMOV1 {
    namespace {
        bool Tactical_Action_Is_Valid(const s_net_tactical_combat_message* message) {
            const int16_t arg1 = message->arg1;
            switch (message->message_code) {
                case TACTICAL_NET_MSG_FIRE_WEAPON:
                    return arg1 >= 0 && arg1 < (message->arg2 == 0 ? MOX::combat_total_ship_count : MAX_COMBAT_FLYERS);
                case TACTICAL_NET_MSG_TOGGLE_WEAPON:
                    return arg1 >= 0 && arg1 < 8;
                case TACTICAL_NET_MSG_MOVE_SHIP:
                    return arg1 >= 0 && arg1 < MOX::combat_grid_width &&
                        message->arg2 >= 0 && message->arg2 < MOX::combat_grid_height;
                case TACTICAL_NET_MSG_REMOVE_STASIS:
                case TACTICAL_NET_MSG_SELECT_SHIP:
                case TACTICAL_NET_MSG_BOARDING_ACTION:
                    return arg1 >= 0 && arg1 < MOX::combat_total_ship_count;
                case TACTICAL_NET_MSG_SELF_DESTRUCT:
                case TACTICAL_NET_MSG_AUTO_TURN:
                case TACTICAL_NET_MSG_SHIP_DONE:
                case TACTICAL_NET_MSG_CYCLE_MISSILE_WARNING:
                case TACTICAL_NET_MSG_TOGGLE_AUTO_COMBAT:
                case TACTICAL_NET_MSG_WAIT:
                case TACTICAL_NET_MSG_RETREAT:
                case TACTICAL_NET_MSG_TURN_TO_FACE:
                case TACTICAL_NET_MSG_END_TURN:
                case TACTICAL_NET_MSG_SIDE_WAIT:
                    return true;
                default:
                    return false;
            }
        }
    } // namespace

    void __cdecl Tactical_Combat_Receive_Message(int32_t sender_net_id, uint16_t message_id, int32_t message_size, s_net_tactical_combat_message* message) {
        (void)sender_net_id;
        if (message == nullptr || message_size < static_cast<int32_t>(sizeof(message->viewport_x))) return;

        if (message_id >= TACTICAL_NET_PACKET_ACTION && message_id < DIP_NET_PACKET_MESSAGE) {
            switch (message_id) {
                case TACTICAL_NET_PACKET_ACTION:
                    if (message_size < static_cast<int32_t>(sizeof(*message)) || !Tactical_Action_Is_Valid(message)) {
                        NETMOX::Net_Debug_Log("tactical action dropped: invalid packet or arguments");
                        return;
                    }
                    NETMOX::Net_Debug_Log(
                        "tactical action recv sender_net_id=%d local_player=%d action=%d args=%d,%d,%d pending=%d choice=%d turn=%d cur_ship=%d",
                        (int)sender_net_id,
                        (int)MOX::PLAYER_NUM,
                        (int)message->message_code,
                        (int)message->arg1,
                        (int)message->arg2,
                        (int)message->arg3,
                        (int)MOX::new_message.message_code,
                        (int)MOX::tactical_net_choice,
                        (int)MOX::combat_turn,
                        MOX::cur_ship);
                    if (MOX::new_message.message_code == 0) {
                        struct_::Copy_Structure(&MOX::new_message, message, sizeof(MOX::new_message));
                        MOX::tactical_net_choice = MOX::new_message.message_code;
                        return;
                    }

                    NETMOX::Net_Debug_Log(
                        "Tactical_Combat_Receive_Message: overwriting pending action pending=%d incoming=%d choice=%d",
                        (int)MOX::new_message.message_code,
                        (int)message->message_code,
                        (int)MOX::tactical_net_choice);
                    struct_::Copy_Structure(&MOX::new_message, message, sizeof(MOX::new_message));
                    MOX::tactical_net_choice = MOX::new_message.message_code;
                    return;

                case TACTICAL_NET_PACKET_AUTO_COMBAT_DISABLED: {
                    int16_t player_idx = message->viewport_x;
                    if (player_idx < 0 || player_idx >= 2) break;
                    MOX::auto_combat_flag[player_idx] = 0;

                    if (MOX::cur_ship < 0 || MOX::cur_ship >= MOX::combat_total_ship_count) break;
                    if (MOX::combat_data[MOX::cur_ship].owner == MOX::combat_attacker_player) {
                        if (player_idx == 0) {
                            MOX::auto_combat = 0;
                            return;
                        }
                    } else {
                        if (player_idx == 1) {
                            MOX::auto_combat = 0;
                            return;
                        }
                    }
                    break;
                }

                case TACTICAL_NET_PACKET_MISSILE_WARNING:
                    MOX::saved_msl_warn = MOX::msl_impact_warning_flag;
                    MOX::msl_impact_warning_flag = message->viewport_x;
                    return;

                case TACTICAL_NET_PACKET_WAIT_FOR_OPPONENT:
                    NETMOX::Net_Debug_Log(
                        "tactical barrier recv sender_net_id=%d local_player=%d turn=%d cur_ship=%d cur_ptr=%d counter_before=%u choice=%d",
                        (int)sender_net_id,
                        (int)MOX::PLAYER_NUM,
                        (int)MOX::combat_turn,
                        MOX::cur_ship,
                        (int)MOX::cur_ptr,
                        (unsigned int)MOX::combat_done_waiting,
                        (int)MOX::tactical_net_choice);
                    MOX::combat_done_waiting++;
                    NETMOX::Net_Debug_Log(
                        "tactical barrier queued local_player=%d counter_after=%u",
                        (int)MOX::PLAYER_NUM,
                        (unsigned int)MOX::combat_done_waiting);
                    return;

                case TACTICAL_NET_PACKET_INIT_SYNC:
                    if (message->viewport_x == 1) {
                        MOX::combat_init = message->viewport_x;
                    }
                    break;
            }
        }
    }
} // namespace CMBTMOV1
