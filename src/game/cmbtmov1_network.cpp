#include "pch.h"

namespace CMBTMOV1 {
    namespace {
        bool Tactical_Action_Ship_Is_Valid(int16_t ship_idx) {
            return ship_idx >= 0 && ship_idx < MOX::combat_total_ship_count;
        }

        bool Tactical_Action_Grid_Pos_Is_Valid(int16_t x, int16_t y) {
            return x >= 0 && x < MOX::combat_grid_width && y >= 0 && y < MOX::combat_grid_height;
        }

        // A peer-controlled tactical action is queued only when every index
        // it carries is in range for the combat state it will be applied to.
        // The consumer in combat1.cpp indexes combat_data, missile and
        // combat_grid with these values without further checks, so an
        // out-of-range action is dropped here instead of crashing or
        // corrupting combat state. Actions that carry no indices, and
        // TURN_TO_FACE (whose target is only used in arithmetic), need no
        // check. Unknown message codes are dropped as well.
        bool Tactical_Action_Args_Are_Valid(const s_net_tactical_combat_message* message) {
            switch (message->message_code) {
                case TACTICAL_NET_MSG_FIRE_WEAPON:
                    if (message->arg2 == 0) {
                        return Tactical_Action_Ship_Is_Valid(message->arg1);
                    }
                    return message->arg1 >= 0 && message->arg1 < MAX_COMBAT_FLYERS;
                case TACTICAL_NET_MSG_TOGGLE_WEAPON:
                    return message->arg1 >= 0 && message->arg1 < 8;
                case TACTICAL_NET_MSG_MOVE_SHIP:
                    return Tactical_Action_Grid_Pos_Is_Valid(message->arg1, message->arg2);
                case TACTICAL_NET_MSG_REMOVE_STASIS:
                case TACTICAL_NET_MSG_SELECT_SHIP:
                case TACTICAL_NET_MSG_BOARDING_ACTION:
                    return Tactical_Action_Ship_Is_Valid(message->arg1);
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

        if (message_id >= TACTICAL_NET_PACKET_ACTION && message_id < DIP_NET_PACKET_MESSAGE) {
            switch (message_id) {
                case TACTICAL_NET_PACKET_ACTION: {
                    // The action handler copies the full 16-byte message, so
                    // a short packet must be dropped before any field is
                    // read. (The other tactical packets legitimately carry
                    // only a 2-byte payload, so this check is action-only.)
                    if (message_size < (int32_t)sizeof(s_net_tactical_combat_message)) {
                        NETMOX::Net_Debug_Log(
                            "tactical action dropped: short packet size=%d",
                            (int)message_size);
                        return;
                    }
                    if (!Tactical_Action_Args_Are_Valid(message)) {
                        NETMOX::Net_Debug_Log(
                            "tactical action dropped: out-of-range action=%d args=%d,%d,%d",
                            (int)message->message_code,
                            (int)message->arg1,
                            (int)message->arg2,
                            (int)message->arg3);
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
                }

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
