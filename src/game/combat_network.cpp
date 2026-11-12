#include "pch.h"

namespace COMBAT1 {
    int32_t __cdecl Send_Tactical_Combat_Message(int16_t player_idx, int16_t message_code, int16_t arg1, int16_t arg2, int16_t arg3) {
        s_net_tactical_combat_message combat_msg;

        combat_msg.viewport_x = MOX::cur_combat_x;
        combat_msg.viewport_y = MOX::cur_combat_y;
        combat_msg.random_seed = game_random::Get_Random_Seed();
        combat_msg.message_code = message_code;
        combat_msg.arg1 = arg1;
        combat_msg.arg2 = arg2;
        combat_msg.arg3 = arg3;

        int16_t net_id = MOX::player[player_idx].network_player_id;

        NETMOX::Net_Debug_Log(
            "tactical action send local_player=%d target_player=%d target_net_id=%d action=%d args=%d,%d,%d turn=%d cur_ship=%d cur_ptr=%d",
            (int)MOX::PLAYER_NUM,
            (int)player_idx,
            (int)net_id,
            (int)message_code,
            arg1,
            arg2,
            arg3,
            (int)MOX::combat_turn,
            MOX::cur_ship,
            (int)MOX::cur_ptr);
        return NETMOX::Mox_Send_Message(net_id, TACTICAL_NET_PACKET_ACTION, &combat_msg, sizeof(combat_msg));
    }

    int32_t Send_Combat_Side_Wait_Message(int16_t player_idx) {
        return Send_Tactical_Combat_Message(
            player_idx, TACTICAL_NET_MSG_SIDE_WAIT, 0, 0, 0);
    }
} // namespace COMBAT1
