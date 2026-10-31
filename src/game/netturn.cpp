#include "pch.h"
#include "game/game_events.h"

#include "lua/lua_network.h"

namespace NETTURN {

    char* waiting_message_fmt;

    int32_t __cdecl Determine_Lowest_Active_Host_Player(const s_player* players, int32_t num_players) {
        if (players == nullptr || num_players <= 0) {
            return -1;
        }

        for (int32_t i = 0; i < num_players; ++i) {
            if (players[i].objectives == PLAYER_OBJECTIVE_HUMAN && players[i].network_player_id != -1 && players[i].eliminated == 0) {
                return i;
            }
        }

        return -1;
    }

    void __cdecl Net_Next_Turn() {
        game_events::s_resolution_scope resolution;
        s_net_chat chat_buffer;
        s_net_election_msg election_msg;

        CMBTFIR2::Save_Artemis_Random_Seed();

        if (MOX::player[MOX::PLAYER_NUM].surrender_to != -1) {
            NETMOX::Human_Surrenders(MOX::PLAYER_NUM);
            NETSTART::Mox_Net_Uninit();
            MOX::current_screen = 10;
            MOX::return_screen = 10;
            MOX::previous_screen = 10;
            return;
        }

        NETMOX::net.human_diplomacy_request_mask = 0;
        NETMOX::net.players_active_mask = 0;
        memset(&chat_buffer, 0, sizeof(chat_buffer));
        CHAT::chat_info = &chat_buffer;

        MULTPLAY::Load_Net_Next_Turn_Screen(0);
        fields::Assign_Auto_Function(MULTPLAY::Draw_Net_Next_Turn_Screen, 1);
        MULTPLAY::Add_Net_Next_Turn_Fields();

        NETMOX::net.sync_state = NET_SYNC_STATE_NEXT_TURN_READY;
        NETMOX::net.players_hit_next_turn[MOX::PLAYER_NUM] = 1;

        if (NETMOX::net.host_election_player_rank[MOX::PLAYER_NUM] == -1) {
            NETMOX::net.host_election_player_rank[MOX::PLAYER_NUM] = (int8_t)MOX::PLAYER_NUM;
        }

        election_msg.hit_next_turn = 1;

        for (int i = 0; i < MOX::NUM_PLAYERS; ++i) {
            if (MOX::player[i].objectives == PLAYER_OBJECTIVE_HUMAN) {
                if (MOX::PLAYER_NUM != i) {
                    election_msg.rank = (int32_t)NETMOX::net.host_election_player_rank[MOX::PLAYER_NUM];
                    NETMOX::Mox_Send_Message(MOX::player[i].network_player_id, NET_PACKET_HOST_ELECTION_RANK, &election_msg, sizeof(election_msg));
                }
            }
            MULTPLAY::Chat_Box_Input_Loop();
        }

        while (true) {
            int elected_host_player_idx = Determine_Lowest_Active_Host_Player(MOX::player, MOX::NUM_PLAYERS);

            if (elected_host_player_idx != -1) {
                NETMOX::net.local_net_id = MOX::player[elected_host_player_idx].network_player_id;
            }

            MULTPLAY::Chat_Box_Input_Loop();

            if (NETMOX::net.local_net_id != -1) {
                std::string lua_diagnostic;
                if (!orion2re::lua::Prepare_Lua_Gameplay_Host_Authority(
                        NETMOX::net.local_net_id,
                        NETMOX::net.net_host_id,
                        &lua_diagnostic)) {
                    exitm::Exit_With_Message(lua_diagnostic.c_str());
                }
                if (NETMOX::net.local_net_id == NETMOX::net.net_host_id) {
                    Host_Next_Turn();
                } else {
                    Client_Next_Turn();
                }

                NETMOX::net.players_active_mask = 0;
                NETMOX::net.human_diplomacy_request_mask = 0;
                CHAT::chat_info = nullptr;

                if (MOX::player[MOX::PLAYER_NUM].eliminated == 0) {
                    ERICNET::Mox_Play_Sound(0x33);
                    MOX::previous_screen = SCREEN_NET_NEXT_TURN;
                    MOX::current_screen = SCREEN_REPORTS;
                    return;
                }

                ERIC::Play_Loose_Flic();
                while (netcode::Net_Is_Sending()) {
                    RUSS::Mox_Update();
                }

                NETSTART::Mox_Net_Uninit();
                MOX::game_type = 0;
                MOX::previous_screen = SCREEN_NET_NEXT_TURN;
                MOX::current_screen = SCREEN_MAIN_MENU;
                return;
            }
        }
    }

    char* __cdecl Load_Netturn_Strings(char* string_block) {
        waiting_message_fmt = string_block;

        char* next_ptr = waiting_message_fmt + strlen(waiting_message_fmt) + 1;

        return next_ptr + strlen(next_ptr) + 1;
    }

    void __cdecl Host_Next_Turn() {
        NETMOX::Net_Debug_Log(
            "Host_Next_Turn_: start local_player=%d stardate=%u connected=%d",
            (int)MOX::PLAYER_NUM,
            (unsigned int)MOX::stardate,
            (int)NETMOX::net.num_connected_players);
        NETMOX::Clear_Leader_Deassignments();
        NETMOX::net.host_sync_counter = 0;

        if (MOX::NUM_PLAYERS > 0) {
            for (int i = 0; i < MOX::NUM_PLAYERS; ++i) {
                if (MOX::player[i].objectives != PLAYER_OBJECTIVE_HUMAN || MOX::PLAYER_NUM == i || MOX::player[i].eliminated != 0) {
                    NETMOX::net.host_sync_counter++;
                    NETMOX::net.host_orders_received_flags[i] = 1;
                }
            }
        }

        NETMOX::net.sync_state = NET_SYNC_STATE_HOST_WAITING_FOR_ORDERS;
        if (NETMOX::net.host_sync_counter >= MOX::NUM_PLAYERS) {
            NETMOX::net.sync_state = NET_SYNC_STATE_HOST_BEGIN_TURN;
        }

        while (NETMOX::net.sync_state != NET_SYNC_STATE_HOST_BEGIN_TURN) {
            for (int i = 0; i < MOX::NUM_PLAYERS; ++i) {
                MULTPLAY::Chat_Box_Input_Loop();

                if (NETMOX::net.players_hit_next_turn[i] != 0 && NETMOX::net.player_is_active_flags[i] != 0) {
                    NETMOX::net.player_is_active_flags[i] = 0;
                    NETMOX::net.host_active_player_id = (int8_t)i;
                }

                if (NETMOX::net.host_active_player_id != -1) {
                    break;
                }
            }

            if (NETMOX::net.host_active_player_id != -1) {
                NETMOX::net.sync_state = NET_SYNC_STATE_HOST_RECEIVING_PLAYER_DATA;
                NETMOX::Net_Debug_Log(
                    "Host_Next_Turn_: requesting game orders from player=%d host_sync_counter=%d",
                    (int)NETMOX::net.host_active_player_id,
                    (int)NETMOX::net.host_sync_counter);
                NETMOX::Mox_Send_Message(MOX::player[NETMOX::net.host_active_player_id].network_player_id, NET_PACKET_SEND_GAME_ORDERS_REQUEST, nullptr, 0);

                while (NETMOX::net.sync_state == NET_SYNC_STATE_HOST_RECEIVING_PLAYER_DATA) {
                    MULTPLAY::Chat_Box_Input_Loop();
                }

                NETMOX::Net_Debug_Log(
                    "Host_Next_Turn_: received game orders from player=%d bytes=%u next_sync=%u",
                    (int)NETMOX::net.host_active_player_id,
                    (unsigned int)NETMOX::net.total_bytes_received,
                    (unsigned int)NETMOX::net.sync_state);
                NETMOX::Decode_Game_Orders(NETMOX::net.network_data_ptr, NETMOX::net.total_bytes_received);
                NETMOX::net.network_data_ptr = nullptr;
                allocate::Pop_Block(MOX::screen_seg);
                NETMOX::net.host_orders_received_flags[NETMOX::net.host_active_player_id] = 1;
                NETMOX::net.host_active_player_id = -1;
            }
        }

        MAINSCR::Update_Ship_Icons_In_Between_Turns();
        COLCALC::No_Op1();
        NETMOX::net.sync_phase_active_flag = 0;

        memset(
            &NETMOX::net.player_net_info,
            0,
            offsetof(s_network, sync_state) - offsetof(s_network, player_net_info));

        if (MOX::NUM_PLAYERS > 0) {
            for (int i = 0; i < MOX::NUM_PLAYERS; ++i) {
                NETMOX::net.player_net_info[i].combat_attacker_id = -1;
                NETMOX::net.player_net_info[i].player_state = NET_PLAYER_STATE_COMBAT_READY;
            }
        }

        NETMOX::net.sync_state = NET_SYNC_STATE_COMBAT_READY;
        NEXTTURN::Next_Turn();

        NETMOX::net.host_election_winner_id = NETMOX::net.net_host_id;
        Net_Init_Host_Election();
        NETMOX::net.field130_0x1a4 = 0;

        if (MOX::NUM_PLAYERS > 0) {
            for (int i = 0; i < MOX::NUM_PLAYERS; ++i) {
                if (MOX::PLAYER_NUM != i && MOX::player[i].network_player_id != -1 && MOX::player[i].eliminated != 0) {
                    NETMOX::Mox_Send_Message(MOX::player[i].network_player_id, NET_PACKET_FORCE_PLAYER_ELIMINATED, nullptr, 0);
                    MOX::player[i].objectives = 0;
                    MOX::player[i].network_player_id = -1;
                    NETMOX::net.num_connected_players--;
                    RUSS::Mox_Update();
                }
            }
        }

        NETMOX::Net_Debug_Log(
            "Host_Next_Turn_: broadcasting differential stardate=%u connected=%d",
            (unsigned int)MOX::stardate,
            (int)NETMOX::net.num_connected_players);
        NETMOX::Broadcast_Game_Data_Differential(0);

        if (MOX::player[MOX::PLAYER_NUM].eliminated == 0) {
            NETMOX::net.player_race_selected_flags[MOX::PLAYER_NUM] = 1;
            memset(NETMOX::net.player_phase_ready_sent_flags, 0, sizeof(NETMOX::net.player_phase_ready_sent_flags));
            NETMOX::net.sync_state = NET_SYNC_STATE_READY;

            if (MOX::NUM_PLAYERS > 0) {
                for (int i = 0; i < MOX::NUM_PLAYERS; ++i) {
                    NETMOX::net.player_net_info[i].player_state = NET_PLAYER_STATE_READY;
                    NETMOX::net.player_net_info[i].diplomacy_target_id = -1;
                }
            }

            MAINSCR::Update_Ship_Icons_In_Between_Turns();

            if (NETMOX::net.num_connected_players == 1) {
                NETSTART::Mox_Net_Uninit();
                MOX::game_type = 0;
                REPORT::something_interesting_happened[MOX::PLAYER_NUM] = 1;
            }
        }

        NETMOX::Net_Debug_Log(
            "Host_Next_Turn_: done stardate=%u sync=%u connected=%d",
            (unsigned int)MOX::stardate,
            (unsigned int)NETMOX::net.sync_state,
            (int)NETMOX::net.num_connected_players);
    }

    int8_t __cdecl Must_Broadcast_AI_Player_Data() {
        int16_t num_players = MOX::NUM_PLAYERS;

        for (int i = 0; i < num_players; ++i) {
            uint8_t sync_mask = NETMOX::net.players_sync_mask[i];

            for (int j = 0; j < num_players; ++j) {
                if ((sync_mask >> j) & 1) {
                    return 1;
                }
            }
        }

        return 0;
    }

    void __cdecl Net_Init_Host_Election() {
        memset(
            NETMOX::net.host_election_player_rank,
            -1,
            sizeof(NETMOX::net.host_election_player_rank));
        memset(
            NETMOX::net.host_election_rank_sync_cache,
            -1,
            sizeof(NETMOX::net.host_election_rank_sync_cache));
        memset(
            NETMOX::net.players_hit_next_turn,
            0,
            sizeof(NETMOX::net.players_hit_next_turn));

        NETMOX::net.local_net_id = -1;
        NETMOX::net.host_active_player_id = -1;
        NETMOX::net.field23_0xc6 = 0;
        NETMOX::net.diplomacy_initiator_id = -1;

        memset(
            NETMOX::net.host_orders_received_flags,
            0,
            sizeof(NETMOX::net.host_orders_received_flags));

        memset(
            NETMOX::net.player_is_active_flags,
            0,
            sizeof(NETMOX::net.player_is_active_flags));
        memset(
            NETMOX::net.player_race_selected_flags,
            0,
            sizeof(NETMOX::net.player_race_selected_flags));
    }

    void __cdecl Client_Next_Turn() {
        uint8_t star_has_nonplayer;
        s_combfind_star_has_player star_has_player[MAX_PLAYERS];

        NETMOX::Net_Debug_Log(
            "Client_Next_Turn_: start local_player=%d stardate=%u local_net_id=%d host_id=%d sync=%u",
            (int)MOX::PLAYER_NUM,
            (unsigned int)MOX::stardate,
            (int)NETMOX::net.local_net_id,
            (int)NETMOX::net.net_host_id,
            (unsigned int)NETMOX::net.sync_state);

        NETMOX::net.network_data_ptr = nullptr;
        NETMOX::net.total_bytes_received = 0;

        NETMOX::Mox_Send_Message(NETMOX::net.local_net_id, NET_PACKET_TURN_END_REQUEST, nullptr, 0);
        NETMOX::Net_Debug_Log("Client_Next_Turn_: sent TURN_END_REQUEST");

        do {
            MULTPLAY::Chat_Box_Input_Loop();
        } while (NETMOX::net.sync_state == NET_SYNC_STATE_NEXT_TURN_READY);

        NETMOX::Net_Debug_Log(
            "Client_Next_Turn_: left NEXT_TURN_READY sync=%u",
            (unsigned int)NETMOX::net.sync_state);

        COMBFIND::current_star_has_player = star_has_player;
        COMBFIND::current_star_has_nonplayer = &star_has_nonplayer;

        while (NETTURN::Must_Broadcast_AI_Player_Data()) {
            MULTPLAY::Chat_Box_Input_Loop();
        }

        NETMOX::Send_Game_Orders();
        NETMOX::Net_Debug_Log(
            "Client_Next_Turn_: sent game orders sync=%u",
            (unsigned int)NETMOX::net.sync_state);
        COMBFIND::Client_Combat_Loop();

        COMBFIND::current_star_has_player = nullptr;
        COMBFIND::current_star_has_nonplayer = nullptr;

        strlcpy(MOX::temp_string, NETTURN::waiting_message_fmt, sizeof(MOX::temp_string));
        NETMOX::Net_Debug_Log("Client_Next_Turn_: waiting for broadcast game data payload");

        while (NETMOX::net.network_data_ptr == nullptr && MOX::player[MOX::PLAYER_NUM].eliminated == 0) {
            MULTPLAY::Chat_Box_Input_Loop();
        }

        if (MOX::player[MOX::PLAYER_NUM].eliminated == 0) {
            NETMOX::Net_Debug_Log(
                "Client_Next_Turn_: received payload bytes=%u sync=%u",
                (unsigned int)NETMOX::net.total_bytes_received,
                (unsigned int)NETMOX::net.sync_state);
            while (!NETMOX::Decode_Broadcast_Game_Data_Differential(NETMOX::net.network_data_ptr, NETMOX::net.total_bytes_received, 0)) {
                exitm::Exit_With_Message("craziness");
            }

            memset(NETMOX::net.player_phase_ready_sent_flags, 0, sizeof(NETMOX::net.player_phase_ready_sent_flags));
            NETMOX::net.network_data_ptr = nullptr;
            allocate::Pop_Block(MOX::screen_seg);
            NETMOX::net.sync_state = NET_SYNC_STATE_READY;

            MAINSCR::Update_Ship_Icons_In_Between_Turns();

            if (NETMOX::net.num_connected_players == 1) {
                NETSTART::Mox_Net_Uninit();
                MOX::game_type = 0;
            }

            NETMOX::Net_Debug_Log(
                "Client_Next_Turn_: decode complete stardate=%u sync=%u connected=%d",
                (unsigned int)MOX::stardate,
                (unsigned int)NETMOX::net.sync_state,
                (int)NETMOX::net.num_connected_players);
        }

        COMBFIND::current_star_has_player = nullptr;
        COMBFIND::current_star_has_nonplayer = nullptr;
    }
} // namespace NETTURN