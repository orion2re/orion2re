#include "pch.h"

#include "game/savegame.h"
#include "lua/lua_network.h"

namespace NETTURN {

    char* waiting_message_fmt;

    namespace {
        const std::vector<uint8_t>* turn_checkpoint;

        bool Serialize_Autosave(std::vector<uint8_t>& bytes) {
            bytes.resize(SAVEGAME::BUFFER_CAPACITY);
            SERIAL::Writer_ writer(bytes.data(), SAVEGAME::BUFFER_CAPACITY, "Multiplayer autosave overflow");
            std::string error;
            const char description[sizeof(SAVEGAME::s_metadata::description)] = "(Auto Save)";
            if (!SAVEGAME::Write_Game(&writer, description, game_random::Get_Random_Seed(), &error)) {
                SDL_Log("Unable to serialize multiplayer autosave: %s", error.c_str());
                bytes.clear();
                return false;
            }
            bytes.resize(writer.Length());
            return true;
        }

        struct Turn_Checkpoint {
            std::vector<uint8_t> bytes;

            Turn_Checkpoint() {
                if (!MULTPLAY::silent_player_drop) {
                    Serialize_Autosave(bytes);
                }
                turn_checkpoint = &bytes;
            }

            ~Turn_Checkpoint() { turn_checkpoint = nullptr; }
        };
    }

    const s_player* __cdecl Determine_Lowest_Active_Host_Player(const s_player* players, int32_t num_players) {
        if (players == nullptr || num_players <= 0) {
            return nullptr;
        }

        for (int32_t i = 0; i < num_players; ++i) {
            if (players[i].objectives == PLAYER_OBJECTIVE_HUMAN && players[i].network_player_id != -1 && players[i].eliminated == 0) {
                return &players[i];
            }
        }

        return nullptr;
    }

    void Confirm_Player_Drop(game::id_player player) {
        const auto& name = game::current_state.players.at(player.id).name;
        const bool mid_turn = turn_checkpoint != nullptr;
        bool save_failed = false;
        while (true) {
            char message[320];
            snprintf(message, sizeof(message),
                "%sPlayer %.*s has disconnected. Continue with AI controlling their empire?\n\n"
                "Choose No to %s and return to the main menu.",
                save_failed ? "Autosave failed.\n\n" : "",
                (int)sizeof(name), name[0] != '\0' ? name : "(unnamed)",
                mid_turn ? "autosave from the start of turn resolution" : "autosave");
            int choice = platform::Show_Yes_No_Message("Player disconnected", message);
            if (choice < 0) {
                choice = GENDRAW::Confirmation_Box(message);
            }
            if (choice != 0) {
                return;
            }
            std::vector<uint8_t> current_save;
            if (!mid_turn) {
                Serialize_Autosave(current_save);
            }
            const auto& save = mid_turn ? *turn_checkpoint : current_save;
            if (!save.empty() && FILEDEF::Write_Autosave(save.data(), save.size())) {
                netcode::Net_End_Game_After_Save(player);
                throw netcode::session_lost{netcode::e_session_end_reason::local_save};
            }
            save_failed = true;
        }
    }

    void __cdecl Net_Next_Turn() {
        s_net_chat chat_buffer;
        s_net_election_msg election_msg;

        CMBTFIR2::Save_Artemis_Random_Seed();

        if (legacy_world::Encode_Player_Slot(game::current_state.players.at(MOX::local_player.id).surrender_to) != -1) {
            NETMOX::Human_Surrenders(MOX::local_player);
            NETSTART::Mox_Net_Uninit();
            MOX::current_screen = 10;
            MOX::return_screen = 10;
            MOX::previous_screen = 10;
            return;
        }

        NETMOX::net.human_diplomacy_request_mask.reset();
        NETMOX::net.players_active_mask.reset();
        memset(&chat_buffer, 0, sizeof(chat_buffer));
        CHAT::chat_info = &chat_buffer;

        MULTPLAY::Load_Net_Next_Turn_Screen(0);
        fields::Assign_Auto_Function(MULTPLAY::Draw_Net_Next_Turn_Screen, 1);
        MULTPLAY::Add_Net_Next_Turn_Fields();

        NETMOX::net.sync_state = NET_SYNC_STATE_NEXT_TURN_READY;
        NETMOX::net.players_hit_next_turn[MOX::Legacy_Local_Player_Slot()] = 1;

        if (NETMOX::net.host_election_player_rank[MOX::Legacy_Local_Player_Slot()] == -1) {
            NETMOX::net.host_election_player_rank[MOX::Legacy_Local_Player_Slot()] = (int8_t)MOX::Legacy_Local_Player_Slot();
        }

        election_msg.hit_next_turn = 1;

        for (int i = 0; i < game::current_state.player_count; ++i) {
            if (game::current_state.players[i].objectives == PLAYER_OBJECTIVE_HUMAN) {
                if (MOX::Legacy_Local_Player_Slot() != i) {
                    election_msg.rank = (int32_t)NETMOX::net.host_election_player_rank[MOX::Legacy_Local_Player_Slot()];
                    NETMOX::Mox_Send_Message(game::current_state.players[i].network_player_id, NET_PACKET_HOST_ELECTION_RANK, &election_msg, sizeof(election_msg));
                }
            }
            MULTPLAY::Chat_Box_Input_Loop();
        }

        while (true) {
            if (const auto* host = Determine_Lowest_Active_Host_Player(game::current_state.players.data(), game::current_state.player_count)) {
                NETMOX::net.local_net_id = host->network_player_id;
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

                NETMOX::net.players_active_mask.reset();
                NETMOX::net.human_diplomacy_request_mask.reset();
                CHAT::chat_info = nullptr;

                if (game::current_state.players.at(MOX::local_player.id).eliminated == 0) {
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
            (int)MOX::Legacy_Local_Player_Slot(),
            (unsigned int)MOX::stardate,
            (int)NETMOX::net.num_connected_players);
        NETMOX::Clear_Leader_Deassignments();
        NETMOX::net.host_sync_counter = 0;

        if (game::current_state.player_count > 0) {
            for (int i = 0; i < game::current_state.player_count; ++i) {
                if (game::current_state.players[i].objectives != PLAYER_OBJECTIVE_HUMAN || MOX::Legacy_Local_Player_Slot() == i || game::current_state.players[i].eliminated != 0) {
                    NETMOX::net.host_sync_counter++;
                    NETMOX::net.host_orders_received_flags[i] = 1;
                }
            }
        }

        NETMOX::net.sync_state = NET_SYNC_STATE_HOST_WAITING_FOR_ORDERS;
        if (NETMOX::net.host_sync_counter >= game::current_state.player_count) {
            NETMOX::net.sync_state = NET_SYNC_STATE_HOST_BEGIN_TURN;
        }

        while (NETMOX::net.sync_state != NET_SYNC_STATE_HOST_BEGIN_TURN) {
            for (int i = 0; i < game::current_state.player_count; ++i) {
                MULTPLAY::Chat_Box_Input_Loop();
                if (NETMOX::net.sync_state == NET_SYNC_STATE_HOST_BEGIN_TURN) {
                    break;
                }

                if (NETMOX::net.players_hit_next_turn[i] != 0 && NETMOX::net.player_is_active_flags[i] != 0) {
                    NETMOX::net.player_is_active_flags[i] = 0;
                    NETMOX::net.host_active_player_id = game::current_state.players.at(i).id;
                }

                if (NETMOX::net.host_active_player_id.get() != game::NO_PLAYER) {
                    break;
                }
            }

            if (NETMOX::net.host_active_player_id.get() != game::NO_PLAYER) {
                NETMOX::net.sync_state = NET_SYNC_STATE_HOST_RECEIVING_PLAYER_DATA;
                NETMOX::Net_Debug_Log(
                    "Host_Next_Turn_: requesting game orders from player=%d host_sync_counter=%d",
                    legacy_world::Encode_Player_Slot(NETMOX::net.host_active_player_id.get()),
                    (int)NETMOX::net.host_sync_counter);
                NETMOX::Mox_Send_Message(game::current_state.players.at(NETMOX::net.host_active_player_id.get().id).network_player_id, NET_PACKET_SEND_GAME_ORDERS_REQUEST, nullptr, 0);

                while (NETMOX::net.sync_state == NET_SYNC_STATE_HOST_RECEIVING_PLAYER_DATA) {
                    MULTPLAY::Chat_Box_Input_Loop();
                }

                NETMOX::Net_Debug_Log(
                    "Host_Next_Turn_: received game orders from player=%d bytes=%u next_sync=%u",
                    legacy_world::Encode_Player_Slot(NETMOX::net.host_active_player_id.get()),
                    (unsigned int)NETMOX::net.total_bytes_received,
                    (unsigned int)NETMOX::net.sync_state);
                // A disconnect can end the wait without delivering a packet.
                if (NETMOX::net.network_data_ptr == nullptr) continue;
                const auto player = NETMOX::net.host_active_player_id.get().id;
                const bool valid = player < game::current_state.player_count;
                const bool connected = valid && game::current_state.players[player].network_player_id != -1;
                const bool accepted = !connected || NETMOX::Decode_Game_Orders(
                    NETMOX::net.network_data_ptr, NETMOX::net.total_bytes_received, game::current_state.players[player].id);
                NETMOX::net.network_data_ptr = nullptr;
                NETMOX::net.total_bytes_received = 0;
                allocate::Pop_Block(MOX::screen_seg);
                NETMOX::net.host_active_player_id = game::NO_PLAYER;
                if (!accepted) {
                    exitm::Exit_With_Message("Invalid multiplayer game orders; turn not advanced.");
                    return;
                }
                if (valid && !NETMOX::net.host_orders_received_flags[player]) {
                    NETMOX::net.host_orders_received_flags[player] = 1;
                    if (++NETMOX::net.host_sync_counter >= game::current_state.player_count) {
                        NETMOX::net.sync_state = NET_SYNC_STATE_HOST_BEGIN_TURN;
                    }
                }
            }
        }

        MAINSCR::Update_Ship_Icons_In_Between_Turns();
        COLCALC::No_Op1();
        NETMOX::net.sync_phase_active_flag = 0;

        std::ranges::fill(NETMOX::net.player_net_info, s_net_player_info{});

        if (game::current_state.player_count > 0) {
            for (int i = 0; i < game::current_state.player_count; ++i) {
                NETMOX::net.player_net_info[i].combat_attacker_id = game::NO_OWNER;
                NETMOX::net.player_net_info[i].player_state = NET_PLAYER_STATE_COMBAT_READY;
            }
        }

        NETMOX::net.sync_state = NET_SYNC_STATE_COMBAT_READY;
        {
            const Turn_Checkpoint checkpoint;
            NEXTTURN::Next_Turn();
        }

        NETMOX::net.host_election_winner_id = NETMOX::net.net_host_id;
        Net_Init_Host_Election();
        NETMOX::net.field130_0x1a4 = 0;

        if (game::current_state.player_count > 0) {
            for (int i = 0; i < game::current_state.player_count; ++i) {
                if (MOX::Legacy_Local_Player_Slot() != i && game::current_state.players[i].network_player_id != -1 && game::current_state.players[i].eliminated != 0) {
                    NETMOX::Mox_Send_Message(game::current_state.players[i].network_player_id, NET_PACKET_FORCE_PLAYER_ELIMINATED, nullptr, 0);
                    game::current_state.players[i].objectives = 0;
                    game::current_state.players[i].network_player_id = -1;
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

        if (game::current_state.players.at(MOX::local_player.id).eliminated == 0) {
            NETMOX::net.player_race_selected_flags[MOX::Legacy_Local_Player_Slot()] = 1;
            memset(NETMOX::net.player_phase_ready_sent_flags, 0, sizeof(NETMOX::net.player_phase_ready_sent_flags));
            NETMOX::net.sync_state = NET_SYNC_STATE_READY;

            if (game::current_state.player_count > 0) {
                for (int i = 0; i < game::current_state.player_count; ++i) {
                    NETMOX::net.player_net_info[i].player_state = NET_PLAYER_STATE_READY;
                    NETMOX::net.player_net_info[i].diplomacy_target_id = game::NO_PLAYER;
                }
            }

            MAINSCR::Update_Ship_Icons_In_Between_Turns();

            if (NETMOX::net.num_connected_players == 1) {
                NETSTART::Mox_Net_Uninit();
                MOX::game_type = 0;
                REPORT::something_interesting_happened[MOX::Legacy_Local_Player_Slot()] = 1;
            }
        }

        NETMOX::Net_Debug_Log(
            "Host_Next_Turn_: done stardate=%u sync=%u connected=%d",
            (unsigned int)MOX::stardate,
            (unsigned int)NETMOX::net.sync_state,
            (int)NETMOX::net.num_connected_players);
    }

    int8_t __cdecl Must_Broadcast_AI_Player_Data() {
        int16_t num_players = game::current_state.player_count;

        for (int i = 0; i < num_players; ++i) {
            const auto sync_mask = NETMOX::net.players_sync_mask[i];

            for (int j = 0; j < num_players; ++j) {
                if (sync_mask.test_slot(j)) {
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
        NETMOX::net.host_active_player_id = game::NO_PLAYER;
        NETMOX::net.field23_0xc6 = 0;
        NETMOX::net.diplomacy_initiator_id = game::NO_PLAYER;

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
            (int)MOX::Legacy_Local_Player_Slot(),
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
        const Turn_Checkpoint checkpoint;
        COMBFIND::Client_Combat_Loop();

        COMBFIND::current_star_has_player = nullptr;
        COMBFIND::current_star_has_nonplayer = nullptr;

        strlcpy(MOX::temp_string, NETTURN::waiting_message_fmt, sizeof(MOX::temp_string));
        NETMOX::Net_Debug_Log("Client_Next_Turn_: waiting for broadcast game data payload");

        while (NETMOX::net.network_data_ptr == nullptr && game::current_state.players.at(MOX::local_player.id).eliminated == 0) {
            MULTPLAY::Chat_Box_Input_Loop();
        }

        if (game::current_state.players.at(MOX::local_player.id).eliminated == 0) {
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
