#pragma once

#include "game/deps.h"

namespace NETSTART {
    inline game::id_player Assign_New_Game_Network_Roster_(std::array<s_player, MAX_PLAYERS>& players,
        s_net_pack_start_game* message,
        const int16_t* users,
        int connected_players,
        int16_t local_net_id) {
        auto local_player = game::NO_PLAYER;
        for (int index = 0; index < MAX_PLAYERS; ++index) {
            // Lua negotiation and broadcasts use -1, not zero, for AI slots.
            players[index].network_player_id = -1;
            message->net_ids[index] = 0;
            if (index < connected_players) {
                players[index].network_player_id = users[index];
                message->net_ids[index] = users[index];
                if (users[index] == local_net_id) {
                    local_player = players[index].id;
                }
            }
        }
        if (local_player == game::NO_PLAYER) {
            players[0].network_player_id = local_net_id;
            message->net_ids[0] = local_net_id;
            local_player = players.front().id;
        }
        return local_player;
    }
} // namespace NETSTART