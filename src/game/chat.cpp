#include "pch.h"

namespace CHAT {
    s_net_chat* chat_info;

    void __cdecl Receive_Chat_Msg(uint8_t player_id, const char* text) {
        size_t len = strlen(text) + 1;

        if (chat_info->count == 14) {
            memmove(chat_info, chat_info->messages + 1, sizeof(chat_info->messages) - sizeof(chat_info->messages[0]));
            chat_info->count = 13;
        }

        chat_info->messages[chat_info->count].player_id = player_id;
        memcpy(chat_info->messages[chat_info->count].text, text, len);
        chat_info->count++;
    }

    void __cdecl Send_Chat_Msg(char* msg) {
        for (int32_t player_index = 0; player_index < game::current_state.player_count; ++player_index) {
            if (game::current_state.players[player_index].objectives == PLAYER_OBJECTIVE_HUMAN) {
                int32_t message_len = (int32_t)(strlen(msg) + 1);
                NETMOX::Mox_Send_Message(game::current_state.players[player_index].network_player_id, NET_PACKET_CHAT, msg, (uint16_t)message_len);
                RUSS::Mox_Update();
            }
        }
    }

    void __cdecl Send_GNN_Chat_Msg(char* msg) {
        if (game::current_state.player_count <= 0) {
            return;
        }

        for (int32_t player_index = 0; player_index < game::current_state.player_count; ++player_index) {
            if (game::current_state.players[player_index].objectives == PLAYER_OBJECTIVE_HUMAN) {
                int32_t len = (int32_t)strlen(msg);

                if (len > 80) {
                    msg[80] = '\0';
                    len = 80;
                }

                NETMOX::Mox_Send_Message(game::current_state.players[player_index].network_player_id, NET_PACKET_GNN_CHAT, msg, (uint16_t)(len + 1));
                RUSS::Mox_Update();
            }
        }
    }
} // namespace CHAT
