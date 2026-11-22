#include "pch.h"
#include "legacy_world.h"
#include "star_destination_serialization.h"
#include "star_surrender_serialization.h"

#include "game/leader_serialization.h"
#include "game/planet_serialization.h"
#include "game/star_serialization.h"
#include "game/ship_serialization.h"
#include "game/notice_serialization.h"
#include "game/event_serialization.h"
#include "game/colony_serialization.h"
#include "game/colony_roster_serialization.h"
#include "game/player_network_serialization.h"
#include "game/netproto.h"
#include "game/legacy_world.h"
#include "lua/lua_network.h"

namespace NETMOX {
    s_network net;

    uint8_t sending_msg;

    char* lost_net_user_error;

    char* only_1_host_allowed_error;

    char* net_snapshot_diff_buffer_error;

    char* net_snapshot_cache_error;

    s_net_snapshot_cache net_snapshot_cache;

    char* incompatible_versions_error;

    char* ambassador_left_before_you_arrived_fmt;

    char* net_send_error;

    uint8_t in_receive_message;

    uint8_t is_updating;

    uint8_t next_update_player_counter;

    namespace {
        uint32_t suspended_net_update_depth;
        uint8_t deassigned_leaders[MAX_LEADERS];

        constexpr uint32_t O2GD_MAGIC = 0x4447324F;
        constexpr uint16_t O2GD_VERSION = 1;
        constexpr uint16_t O2GD_HEADER_SIZE = 20;
        constexpr uint32_t NATIVE_SNAPSHOT_VERSION_TAG = 0x494E324F; // O2NI

        void Write_Combat_Destination(NETPROTO::Packet_Writer_& writer, int16_t position) {
            SERIAL::Write_Star_Destination(writer, game::Destination_At_Row(game::current_state, position));
        }

        std::optional<game::s_star_destination> Read_Combat_Destination(NETPROTO::Packet_Reader_& reader) {
            const auto destination = SERIAL::Read_Star_Destination(reader);
            return game::Destination_Row(game::current_state, destination) < 0 ? std::nullopt : std::optional{destination};
        }

        struct Scoped_Net_Update_Suspension_ {
            Scoped_Net_Update_Suspension_() {
                ++suspended_net_update_depth;
            }

            ~Scoped_Net_Update_Suspension_() {
                --suspended_net_update_depth;
            }
        };

#pragma pack(push, 1)
        struct s_net_pack_player_disconnected_info_ {
            int16_t player_idx;
            s_net_pack_drop_out_info drop_out_info;
        };
#pragma pack(pop)

        constexpr size_t k_star_player_presence_payload_bytes = MAX_PLAYERS * sizeof(s_combfind_star_has_player) + sizeof(uint8_t);

        bool Is_Valid_Game_Player(int player_idx) {
            return player_idx >= 0 && player_idx < MOX::NUM_PLAYERS && player_idx < MAX_PLAYERS;
        }

        bool Is_Connected_Game_Player(int player_idx) {
            return Is_Valid_Game_Player(player_idx) && MOX::player[player_idx].network_player_id != -1;
        }

        int Elect_Disconnect_Recovery_Coordinator(int disconnected_player_idx) {
            for (int i = 0; i < MOX::NUM_PLAYERS && i < MAX_PLAYERS; ++i) {
                if (i != disconnected_player_idx && MOX::player[i].network_player_id != -1) {
                    return i;
                }
            }
            return -1;
        }

        bool Is_Local_Player_Slot(int player_idx) {
            return Is_Valid_Game_Player(player_idx) && player_idx == MOX::PLAYER_NUM;
        }

        void Promote_Recovery_Coordinator_If_Needed(int disconnected_player_idx, int16_t disconnected_network_id) {
            int coordinator_idx = Elect_Disconnect_Recovery_Coordinator(disconnected_player_idx);
            if (coordinator_idx < 0) {
                return;
            }

            int16_t coordinator_network_id = MOX::player[coordinator_idx].network_player_id;
            if (coordinator_network_id == -1) {
                return;
            }

            if (net.host_election_id == disconnected_network_id || net.host_election_id == -1) {
                net.host_election_id = coordinator_network_id;
            }
            if (net.host_election_winner_id == disconnected_network_id || net.host_election_winner_id == -1) {
                net.host_election_winner_id = coordinator_network_id;
            }
            if (net.local_net_id == disconnected_network_id || net.local_net_id == -1) {
                net.local_net_id = coordinator_network_id;
            }
        }

        void Apply_Drop_Out_Bookkeeping(int player_idx, int16_t disconnected_network_id, bool promote_recovery_coordinator) {
            if (promote_recovery_coordinator) {
                Promote_Recovery_Coordinator_If_Needed(player_idx, disconnected_network_id);
            }

            if (net.num_connected_players > 0) {
                net.num_connected_players--;
            }
            net.players_hit_next_turn[player_idx] = 1;

            if (net.local_net_id == net.net_host_id) {
                net.player_is_active_flags[player_idx] = 0;
                if (net.sync_state != NET_SYNC_STATE_HOST_WAITING_FOR_ORDERS &&
                    net.sync_state != NET_SYNC_STATE_HOST_RECEIVING_PLAYER_DATA &&
                    net.sync_state != NET_SYNC_STATE_HOST_BEGIN_TURN) {
                    return;
                }

                if (net.host_active_player_id == player_idx &&
                    net.sync_state == NET_SYNC_STATE_HOST_RECEIVING_PLAYER_DATA) {
                    net.host_active_player_id = -1;
                    net.sync_state = NET_SYNC_STATE_HOST_WAITING_FOR_ORDERS;
                }
                // A payload already received was counted by the packet handler.
                const bool payload_received = net.host_active_player_id == player_idx && net.network_data_ptr != nullptr;
                if (!net.host_orders_received_flags[player_idx] && !payload_received) {
                    net.host_orders_received_flags[player_idx] = 1;
                    net.host_sync_counter++;
                }

                if (net.host_sync_counter >= MOX::NUM_PLAYERS) {
                    net.sync_state = NET_SYNC_STATE_HOST_BEGIN_TURN;
                }
            }
        }

        void Build_Drop_Out_Info_From_Local_State(int player_idx, s_net_pack_drop_out_info* packet) {
            MOX::player[player_idx].network_player_id = -1;
            RUSS::Make_Player_Into_AI(player_idx);

            packet->player_data = NETPROTO::Encode_Player(MOX::player[player_idx]);

            NETPROTO::Write_Star_Surrender(packet->star_surrender_to, game::current_state, player_idx);
        }

        void Apply_Drop_Out_Info(int32_t player_idx, s_net_pack_drop_out_info* drop_out_info, bool promote_recovery_coordinator) {
            if (!Is_Valid_Game_Player(player_idx) || drop_out_info == nullptr) {
                return;
            }

            s_player decoded;
            if (!NETPROTO::Decode_Player(&drop_out_info->player_data, sizeof(drop_out_info->player_data), decoded)) return;
            NETPROTO::star_surrender_updates surrender;
            if (!NETPROTO::Read_Star_Surrender(drop_out_info->star_surrender_to, game::current_state, surrender)) return;
            int16_t disconnected_network_id = MOX::player[player_idx].network_player_id;
            if (MOX::player[player_idx].network_player_id == net.host_election_winner_id) {
                net.host_election_winner_id = -1;
            }

            RUSS::Make_Player_Into_AI(player_idx);

            MOX::player[player_idx] = decoded;

            NETPROTO::Apply_Star_Surrender(game::current_state, player_idx, surrender);

            Apply_Drop_Out_Bookkeeping(player_idx, disconnected_network_id, promote_recovery_coordinator);
        }
    } // namespace

    int32_t __cdecl Net_Id_To_Player_Id(int32_t player_idx) {
        for (int32_t i = 0; i < MOX::NUM_PLAYERS; ++i) {
            if (MOX::player[i].network_player_id == player_idx) {
                return i;
            }
        }
        return -1;
    }

    char* __cdecl Load_Netmox_Strings(char* strings) {
        net_snapshot_cache_error = strings;
        strings += strlen(strings) + 1;

        net_snapshot_diff_buffer_error = strings;
        strings += strlen(strings) + 1;

        incompatible_versions_error = strings;
        strings += strlen(strings) + 1;

        only_1_host_allowed_error = strings;
        strings += strlen(strings) + 1;

        net_send_error = strings;
        strings += strlen(strings) + 1;

        lost_net_user_error = strings;
        strings += strlen(strings) + 1;

        strings += strlen(strings) + 1;

        ambassador_left_before_you_arrived_fmt = strings;
        strings += strlen(strings) + 1;

        return strings;
    }

    int8_t __cdecl Net_May_Begin_AI_Diplomacy() {
        if (NETMOX::net.sync_state >= NET_SYNC_STATE_AI_DIPLOMACY_REQUESTED) {
            if (NETMOX::net.sync_state <= NET_SYNC_STATE_AI_DIPLOMACY_DONE) {
                return 0;
            }

            if (NETMOX::net.sync_state == NET_SYNC_STATE_AI_DIPLOMACY_READY) {
                return 1;
            }
        }

        // The binary returns an uninitialized stack byte here; use a stable "not allowed" fallback.
        return 0;
    }

    void __cdecl Human_Abdicates(int player_idx) {
        Send_Drop_Out_Info(player_idx, 0x37);
    }

    void __cdecl Human_Surrenders(int player_idx) {
        Send_Drop_Out_Info(player_idx, 0x38);
    }

    void __cdecl Ditch_Leader(game::s_game_state& state, int32_t leader_idx) {
        auto& leader = state.leaders.at(leader_idx);
        leader.eta = 0;
        leader.status = -1;
        leader.Clear_Location();
        leader.player_index = -1;
    }

    void __cdecl Mark_Leader_Deassigned(int16_t leader_idx) {
        if (net.is_network_active != 0 &&
            leader_idx >= 0 &&
            leader_idx < MAX_LEADERS) {
            deassigned_leaders[leader_idx] = 1;
        }
    }

    void __cdecl Clear_Leader_Deassignments() {
        memset(deassigned_leaders, 0, sizeof(deassigned_leaders));
    }

    int32_t __cdecl Net_Diplomacy_With() {
        return net.diplomacy_target_id;
    }

    void __cdecl Net_Cancel_Human_Diplomacy_Request() {
        if (MOX::game_type >= 2 && MOX::game_type <= 3) {
            int32_t target_id = net.diplomacy_target_id;
            net.diplomacy_target_id = -1;
            net.sync_state = NET_SYNC_STATE_READY;

            int16_t network_player_id = MOX::player[target_id].network_player_id;
            if (network_player_id != -1) {
                Mox_Send_Message(network_player_id, NET_PACKET_CANCEL_HUMAN_DIPLOMACY, nullptr, 0);
            }
        }
    }

    void __cdecl Net_End_Human_Diplomacy() {
        net.diplomacy_target_id = -1;
        net.sync_state = NET_SYNC_STATE_READY;
    }

    bool __cdecl Net_Human_Rejected_Diplomacy() {
        bool result = false;

        if (net.sync_state == NET_SYNC_STATE_HUMAN_DIPLOMACY_REJECTED) {
            net.sync_state = NET_SYNC_STATE_READY;
            result = true;
        }

        return result;
    }

    bool __cdecl Allowed_To_Cancel_Human_Diplomacy() {
        return net.sync_state == NET_SYNC_STATE_HUMAN_DIPLOMACY_REQUESTED;
    }

    int8_t __cdecl Human_Hit_Next_Turn(int16_t player_idx) {
        return net.players_hit_next_turn[player_idx];
    }

    int8_t __cdecl Human_Diplomat_Says_Wait() {
        int8_t wait_flag = net.human_diplomat_wait_flag;
        net.human_diplomat_wait_flag = 0;
        return wait_flag;
    }

    int8_t __cdecl Net_May_Begin_Human_Diplomacy() {
        switch (net.sync_state) {
            case NET_SYNC_STATE_READY:
            case NET_SYNC_STATE_HUMAN_DIPLOMACY_REQUESTED:
            case NET_SYNC_STATE_HUMAN_DIPLOMACY_REJECTED:
            case NET_SYNC_STATE_HUMAN_DIPLOMACY_CONNECTING:
            case NET_SYNC_STATE_HUMAN_DIPLOMACY_BLOCKED:
                return 0;
            case NET_SYNC_STATE_HUMAN_DIPLOMACY_READY:
                return 1;
            default:
                // The binary returns an uninitialized stack byte here; use a stable "not allowed" fallback.
                return 0;
        }
    }

    bool __cdecl Net_Cancel_AI_Diplomacy_Confirmed() {
        return net.sync_state == NET_SYNC_STATE_READY;
    }

    void __cdecl Net_End_AI_Diplomacy() {
        if (MOX::game_type >= 2 && MOX::game_type <= 3) {
            s_net_pack_end_ai_diplomacy msg{};

            int32_t target_player_id = net.diplomacy_target_id;
            msg.target_player_id = target_player_id;
            msg.target_player = NETPROTO::Encode_Player(MOX::player[target_player_id]);

            int32_t player_num = MOX::PLAYER_NUM;
            msg.player_num = player_num;
            msg.player = NETPROTO::Encode_Player(MOX::player[player_num]);

            NETPROTO::Write_Star_Surrender(msg.surrender_to, game::current_state, target_player_id);

            net.diplomacy_target_id = -1;
            net.sync_state = NET_SYNC_STATE_READY;

            if (net.host_election_winner_id != -1) {
                Mox_Send_Message(net.host_election_winner_id, NET_PACKET_AI_DIPLOMACY_END_STATE, &msg, sizeof(msg));
            }
        }
    }

    void __cdecl Net_Cancel_AI_Diplomacy_Request() {
        if (MOX::game_type >= 2 && MOX::game_type <= 3) {
            net.diplomacy_target_id = -1;
            net.sync_state = NET_SYNC_STATE_AI_DIPLOMACY_CANCEL_PENDING;
            if (net.host_election_winner_id != -1) {
                Mox_Send_Message(net.host_election_winner_id, NET_PACKET_AI_DIPLOMACY_CANCEL_REQUEST, &net.diplomacy_target_id, sizeof(net.diplomacy_target_id));
            }
        }
    }

    bool __cdecl No_AI_Diplomacy_Allowed() {
        if (net.host_election_winner_id == -1) {
            net.sync_state = NET_SYNC_STATE_READY;
            return true;
        }
        return false;
    }

    void __cdecl Net_Request_AI_Diplomacy(uint8_t player_idx) {
        net.sync_state = NET_SYNC_STATE_AI_DIPLOMACY_REQUESTED;
        net.diplomacy_target_id = player_idx;

        if (net.host_election_winner_id != -1) {
            Mox_Send_Message(net.host_election_winner_id, NET_PACKET_AI_DIPLOMACY_REQUEST, &player_idx, sizeof(player_idx));
        }
    }

    void __cdecl Receive_Message(int16_t net_id, uint16_t packet_id, uint32_t len, uint8_t* packet) {
        int32_t player_id = Net_Id_To_Player_Id(net_id & 0xFFFF);
        Net_Debug_Log(
            "recv packet=%u bytes=%u sender_net_id=%d sender_player=%d local_net_id=%d sync=%u",
            (unsigned int)packet_id,
            (unsigned int)len,
            (int)net_id,
            (int)player_id,
            (int)net.net_host_id,
            (unsigned int)net.sync_state);
        in_receive_message = 1;

        if (packet_id == NET_PACKET_TACTICAL_COMBAT_START || packet_id == NET_PACKET_TACTICAL_COMBAT_END) {
            Net_Debug_Log(
                "tactical handshake recv packet=%u sender_net_id=%d sender_player=%d local_player=%d state_before=%u",
                (unsigned int)packet_id,
                (int)net_id,
                (int)player_id,
                (int)MOX::PLAYER_NUM,
                (unsigned int)net.tactical_combat_sync_state);
        }

        if (packet_id >= DIP_NET_PACKET_MESSAGE) {
            DIP_SCRN::Diplomacy_Receive_Message(net_id, packet_id, len, reinterpret_cast<s_dipscrn_net_message*>(packet));
        } else if (packet_id >= TACTICAL_NET_PACKET_ACTION) {
            CMBTMOV1::Tactical_Combat_Receive_Message(net_id, packet_id, len, reinterpret_cast<s_net_tactical_combat_message*>(packet));
        } else {
            MAINSCR::Main_Receive_Message(player_id, net_id, packet_id, len, packet);
        }

        in_receive_message = 0;
    }

    void __cdecl Russ_Change_Relations(int16_t change, int16_t attacker, int16_t defender, int16_t reason, int16_t star_idx, int16_t incident_value) {
        if (MOX::game_type != 2 && MOX::game_type != 3) {
            diplomac::Change_Relations(change, attacker, defender, reason, star_idx, incident_value);
        } else {
            uint8_t bytes[CHANGE_RELATIONS_BYTES];
            SERIAL::Writer_ writer(bytes, sizeof(bytes), "change relations overflow");
            writer.Write_Int16(change);
            writer.Write_Int16(attacker);
            writer.Write_Int16(defender);
            writer.Write_Int16(reason);
            SERIAL::Write_Star_Destination(writer, game::Destination_At_Row(game::current_state, star_idx));
            writer.Write_Int16(incident_value);
            Mox_Send_Message(net.local_net_id, NET_PACKET_CHANGE_RELATIONS, bytes, writer.Length());
        }
    }

    uint8_t Humans_Requesting_Diplomacy() {
        return net.players_active_mask;
    }

    int32_t __cdecl Human_Requesting_Diplomacy() {
        int32_t target_player_idx = -1;
        int32_t ready_count = 0;

        if (net.human_diplomacy_request_mask != 0) {
            for (int32_t i = 0; i < (int32_t)MOX::NUM_PLAYERS; ++i) {
                uint8_t ready_mask = net.human_diplomacy_request_mask;
                if (((uint8_t)(ready_mask >> ((uint8_t)i & 0x1F)) & 1) != 0) {
                    ++ready_count;
                    if (game_random::Random(ready_count) == 1) {
                        target_player_idx = i;
                    }
                }
            }

            uint8_t mask = (uint8_t) ~((uint8_t)(1u << ((uint8_t)target_player_idx & 0x1F)));
            net.human_diplomacy_request_mask &= mask;
        }

        return target_player_idx;
    }

    int32_t __cdecl Mox_Send_Message(int16_t recipient_net_id, uint16_t msg_type, void* packet, uint32_t packet_len) {
        uint8_t dummy_packet;
        uint8_t skip_send;
        int32_t result = 1;
        Net_Debug_Log(
            "send packet=%u bytes=%u recipient_net_id=%d local_net_id=%d sync=%u",
            (unsigned int)msg_type,
            (unsigned int)packet_len,
            (int)recipient_net_id,
            (int)net.net_host_id,
            (unsigned int)net.sync_state);

        if (msg_type == NET_PACKET_TACTICAL_COMBAT_START || msg_type == NET_PACKET_TACTICAL_COMBAT_END) {
            Net_Debug_Log(
                "tactical handshake send packet=%u recipient_net_id=%d local_player=%d state=%u turn=%d",
                (unsigned int)msg_type,
                (int)recipient_net_id,
                (int)MOX::PLAYER_NUM,
                (unsigned int)net.tactical_combat_sync_state,
                (int)MOX::combat_turn);
        }

        sending_msg = 1;

        if (packet_len == 0) {
            packet_len = 1;
            packet = &dummy_packet;
        }

        Update();

        if (net.net_host_id == recipient_net_id) {
            Net_Debug_Log("send loopback packet=%u", (unsigned int)msg_type);
            Receive_Message(recipient_net_id, msg_type, packet_len, (uint8_t*)packet);
        } else {
            skip_send = 0;
            do {
                Update();
            } while (netcode::Net_Is_Sending() != 0);

            if (recipient_net_id == (int16_t)netadapter::NETADAPTER_BROADCAST_EXCLUDING_SELF) {
                for (int32_t i = 0; i < MOX::NUM_PLAYERS; i++) {
                    int16_t net_id = MOX::player[i].network_player_id;
                    if (net_id != -1 && net.net_host_id != net_id) {
                        result = netcode::Net_Send_Message(
                            net_id, msg_type, packet, packet_len);
                        if (result == 0) {
                            break;
                        }
                    }
                }
                skip_send = 1;
            }

            if (skip_send == 0) {
                result = netcode::Net_Send_Message(recipient_net_id, msg_type, packet, packet_len);
            }
            Net_Debug_Log("send complete packet=%u recipient_net_id=%d skipped=%u result=%d",
                (unsigned int)msg_type,
                (int)recipient_net_id,
                (unsigned int)skip_send,
                (int)result);

            if (result == 0) {
                exitm::Exit_With_message2(net_send_error);
            }
        }

        sending_msg = 0;
        return result;
    }

    /**
     * @brief Refuses a diplomacy request from another player and updates network state.
     * @param player_idx The index of the player whose request is being refused.
     */
    void __cdecl Refuse_Diplomacy_Request(int player_idx) {
        uint8_t bit_mask = 1 << (player_idx & 0x1F);

        // Mark the player as being ignored in our local player data
        MOX::player[MOX::PLAYER_NUM].ignoring |= bit_mask;

        uint8_t clear_mask = ~bit_mask;

        // Remove player from active and ready masks in network state
        NETMOX::net.players_active_mask &= clear_mask;
        NETMOX::net.human_diplomacy_request_mask &= clear_mask;

        // Notify the remote player that the diplomacy request was refused
        NETMOX::Mox_Send_Message(MOX::player[player_idx].network_player_id, NET_PACKET_HUMAN_DIPLOMACY_IGNORED, nullptr, 0);
    }

    void __cdecl Tell_Diplomacy_Requester_To_Wait(int32_t player_idx) {
        Mox_Send_Message(MOX::player[player_idx].network_player_id, NET_PACKET_HUMAN_DIPLOMAT_WAIT, nullptr, 0);
    }

    void __cdecl Accept_Diplomacy_Request(int player_idx) {
        if (!((net.players_active_mask >> player_idx) & 1)) {
            GENDRAW::Message_Box(ambassador_left_before_you_arrived_fmt);
            return;
        }

        net.diplomacy_target_id = (int8_t)player_idx;
        net.diplomacy_initiator_id = (int8_t)player_idx;

        uint8_t mask = ~(1 << player_idx);
        net.players_active_mask &= mask;
        net.human_diplomacy_request_mask &= mask;

        net.sync_state = NET_SYNC_STATE_HUMAN_DIPLOMACY_CONNECTING;
        net.diplomacy_active_flag = 1;
        net.is_diplomacy_initiator = 0;

        auto player_packet = NETPROTO::Encode_Player(MOX::player[MOX::PLAYER_NUM]);
        Mox_Send_Message(
            MOX::player[player_idx].network_player_id,
            NET_PACKET_ACCEPT_HUMAN_DIPLOMACY,
            &player_packet,
            sizeof(player_packet));

        while (net.sync_state == NET_SYNC_STATE_HUMAN_DIPLOMACY_REQUESTED || net.sync_state == NET_SYNC_STATE_HUMAN_DIPLOMACY_CONNECTING) {
            RUSS::Mox_Update();
        }

        if (Net_May_Begin_Human_Diplomacy()) {
            DIP_SCRN::Diplomacy_Screen(player_idx);
            Net_End_Human_Diplomacy();
        } else {
            GENDRAW::Message_Box(ambassador_left_before_you_arrived_fmt);
            Net_Cancel_Human_Diplomacy_Request();
        }
    }

    void __cdecl Update() {
        sound::Sound_Update();

        // Prevent recursive updates if currently processing messages
        if (NETMOX::in_receive_message == 0 && suspended_net_update_depth == 0) {
            netcode::Net_Update();
        }
    }

    bool __cdecl Net_Debug_Enabled() {
        return net_debug_enabled;
    }

    void __cdecl Net_Debug_Log(const char* format, ...) {
        if (!Net_Debug_Enabled()) {
            return;
        }

        char buffer[512];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "netdbg: %s", buffer);
    }

    void __cdecl Russ_Event_Monster_Update(int16_t monster_type, game::id_planet target_planet_id, int16_t player_idx_or_amount) {
        if (MOX::game_type != 2 /* GAME_TYPE_NETWORK_HOST */ &&
            MOX::game_type != 3 /* GAME_TYPE_NETWORK_CLIENT */) {
            EVENTS::Event_Monster_Update(monster_type, target_planet_id, player_idx_or_amount);
            return;
        }

        uint8_t msg[12];
        SERIAL::Writer_ writer(msg, sizeof(msg), "monster update overflow");
        writer.Write_Int32(monster_type);
        writer.Write_Uint32(target_planet_id.id);
        writer.Write_Int32(player_idx_or_amount);
        NETMOX::Mox_Send_Message(net.local_net_id, NET_PACKET_MONSTER_UPDATE, msg, sizeof(msg));
    }

    void __cdecl Send_Pick_Your_Position(int16_t network_player_id) {
        int32_t num_players = MOX::NUM_PLAYERS;
        if (num_players < 0 || num_players > MAX_PLAYERS) {
            return;
        }

        allocate::Push_Block(MOX::screen_seg);

        s_net_pack_pick_your_position* packet =
            (s_net_pack_pick_your_position*)allocate::Allocate_Next_Block(MOX::screen_seg, sizeof(s_net_pack_pick_your_position));

        packet->num_players = MOX::NUM_PLAYERS;

        for (int i = 0; i < num_players; ++i) packet->players[i] = NETPROTO::Encode_Player(MOX::player[i]);

        uint32_t msg_size = (uint32_t)sizeof(packet->num_players) +
            (uint32_t)num_players * (uint32_t)sizeof(packet->players[0]);
        NETMOX::Mox_Send_Message(network_player_id, NET_PACKET_PICK_YOUR_POSITION, packet, msg_size);

        allocate::Pop_Block(MOX::screen_seg);
    }

    static uint32_t Encode_Native_Game_Data_Snapshot(const game::s_game_state& state, void* buffer, uint32_t len) {
        Scoped_Net_Update_Suspension_ suspend_net_updates;
        NETPROTO::Packet_Writer_ writer(buffer, len, "invalid broadcast game data payload");

        // Assemble a packed byte stream from primitive values using the original x86 little-endian layout.
        writer.Write_Uint32(NATIVE_SNAPSHOT_VERSION_TAG);
        writer.Write_Int16(net.host_election_id);
        writer.Write_Uint32(MOX::stardate);
        writer.Write_Uint8(MOX::settings.random_events);
        writer.Write_Uint8(MOX::settings.game_difficulty);
        writer.Write_Uint8(MOX::settings.number_of_players);
        writer.Write_Uint8(MOX::settings.galaxy_size);
        writer.Write_Uint8(MOX::settings.galaxy_age);
        writer.Write_Uint8(MOX::settings.strategic_combat_flag);
        writer.Write_Uint8(MOX::settings.starting_civilization_level);
        writer.Write_Uint8(MOX::settings.xenons_exist);

        RUSS::Mox_Update();

        writer.Write_Int32(game_random::Get_Random_Seed());

        writer.Write_Uint16((uint16_t)MOX::NUM_PLAYERS);

        for (int i = 0; i < MOX::NUM_PLAYERS; ++i) NETPROTO::Write_Player(writer, MOX::player[i]);

        SERIAL::Write_Star_Block(state, writer);
        SERIAL::Write_Star_Payload(writer, state.antares_destination);
        uint32_t size;

        SERIAL::Write_Planet_Block(state, writer);

        SERIAL::Write_Keyed_Colony_Block(state, writer);

        writer.Write_Int16((int16_t)MOX::NUM_SHIPS);

        for (int i = 0; i < MAX_SHIPS; ++i) SERIAL::Write_Ship_Packet(writer, MOX::ship[i]);

        RUSS::Mox_Update();

        SERIAL::Write_Leader_State(state, writer);

        writer.Write_Uint16((uint16_t)REPORT::n_reports);

        for (const auto& entry : REPORT::reports) SERIAL::Write_Report(&writer, &entry);

        for (int i = 0; i < EVENT_COUNT; ++i) SERIAL::Write_Event(writer, state, EVENTS::event_data[i], i);

        size = MOX::NUM_PLAYERS;
        writer.Write_Bytes(MSG_::first_msg, size);

        writer.Write_Bytes(MSG_::max_msgs, size);

        writer.Write_Bytes(MSG_::n_msgs, size);

        for (const auto& entry : MSG_::msgs) SERIAL::Write_Message(&writer, &entry);

        writer.Write_Uint16((uint16_t)diplodef::COUNCIL_FLAG);

        writer.Write_Uint16((uint16_t)diplodef::next_council_meeting_turn);

        writer.Write_Bytes(&MOX::antarans, sizeof(s_antaran));

        writer.Write_Uint8(MOX::NUM_NEBULAS);

        size = MOX::NUM_NEBULAS * sizeof(s_nebula);
        writer.Write_Bytes(MOX::nebula, size);

        writer.Write_Uint32(EVENTS::last_event_year);

        writer.Write_Uint16(EVENTS::event_delay_count);

        writer.Write_Bytes(MOX::bill_savegame, sizeof(MOX::bill_savegame));

        RUSS::Mox_Update();

        return writer.Length();
    }

    uint32_t __cdecl Encode_Broadcast_Game_Data_Snapshot(void* buffer, uint32_t len) {
        if (!orion2re::lua::Network_Lua_Enabled()) {
            return Encode_Native_Game_Data_Snapshot(game::current_state, buffer, len);
        }
        if (buffer == nullptr || len <= O2GD_HEADER_SIZE) {
            exitm::Exit_With_Message("Lua-enabled game data buffer too small");
        }
        std::string diagnostic;
        uint8_t* bytes = static_cast<uint8_t*>(buffer);
        const uint32_t native_size = Encode_Native_Game_Data_Snapshot(
            game::current_state, bytes + O2GD_HEADER_SIZE, len - O2GD_HEADER_SIZE);
        uint32_t lua_state_size = 0;
        uint32_t flags = 0;
        const bool captured = orion2re::lua::Capture_Lua_Network_Payload(
            bytes + O2GD_HEADER_SIZE + native_size,
            len - O2GD_HEADER_SIZE - native_size,
            &lua_state_size,
            &flags,
            &diagnostic);
        if (!captured) {
            exitm::Exit_With_Message(diagnostic.c_str());
        }
        const uint64_t total = static_cast<uint64_t>(O2GD_HEADER_SIZE) +
            native_size + lua_state_size;
        if (total > len || total > NETMOX_MAX_GAME_DATA_PAYLOAD_BYTES) {
            exitm::Exit_With_Message("Lua-enabled game data payload too large");
        }
        SERIAL::Writer_ writer(bytes,
            O2GD_HEADER_SIZE,
            "Lua-enabled game data header overflow");
        writer.Write_Uint32(O2GD_MAGIC);
        writer.Write_Uint16(O2GD_VERSION);
        writer.Write_Uint16(O2GD_HEADER_SIZE);
        writer.Write_Uint32(native_size);
        writer.Write_Uint32(lua_state_size);
        writer.Write_Uint32(flags);
        return static_cast<uint32_t>(total);
    }

    void __cdecl Broadcast_Game_Data_Differential(uint8_t use_diff) {
        Scoped_Net_Update_Suspension_ suspend_net_updates;
        allocate::Push_Block(MOX::screen_seg);
        uint8_t* block = (uint8_t*)allocate::Allocate_Next_Block(MOX::screen_seg, NETMOX_MAX_GAME_DATA_PAYLOAD_BYTES);
        uint8_t* send_ptr;
        int32_t send_size = 0;
        s_net_snapshot_cache* cache;
        uint32_t block_size = Encode_Broadcast_Game_Data_Snapshot(block, NETMOX_MAX_GAME_DATA_PAYLOAD_BYTES);

        Net_Debug_Log(
            "Broadcast_Game_Data_Differential_: player=%d stardate=%u use_diff=%u block_size=%u sync=%u",
            (int)MOX::PLAYER_NUM,
            (unsigned int)MOX::stardate,
            (unsigned int)use_diff,
            (unsigned int)block_size,
            (unsigned int)net.sync_state);

        if (use_diff != 0) {
            cache = Open_Net_Snapshot_Cache(block_size);
            Write_Net_Snapshot_Cache(cache, block, block_size);
            send_ptr = block;
            send_size = block_size;
        } else {
            cache = Open_Net_Snapshot_Cache(0);
            Net_Debug_Log("Broadcast_Game_Data_Differential_: baseline_bytes=%u snapshot_bytes=%u",
                (unsigned int)cache->buffer_size,
                (unsigned int)block_size);
            send_ptr = block + block_size;
            send_size = Make_Difference_Buffer(cache, block, block_size, send_ptr, NETMOX_MAX_GAME_DATA_PAYLOAD_BYTES - block_size);

            cache = Open_Net_Snapshot_Cache(block_size);
            Write_Net_Snapshot_Cache(cache, block, block_size);
        }

        RUSS::Mox_Update();

        if (send_size > 0) {
            Net_Debug_Log(
                "Broadcast_Game_Data_Differential_: sending payload bytes=%d use_diff=%u",
                send_size,
                (unsigned int)use_diff);
            Mox_Send_Message(0x7ffe, NET_PACKET_GAME_DATA_PAYLOAD, send_ptr, (uint32_t)send_size);
        }

        allocate::Pop_Block(MOX::screen_seg);
    }

    static int8_t Decode_Native_Game_Data_Snapshot(
        game::s_game_state& state, void* buf, size_t size) {
        Scoped_Net_Update_Suspension_ suspend_net_updates;
        uint8_t* ptr = (uint8_t*)buf;

        Net_Debug_Log(
            "Decode_Native_Game_Data_Snapshot_: player=%d stardate_before=%u size=%u",
            (int)MOX::PLAYER_NUM,
            (unsigned int)MOX::stardate,
            (unsigned int)size);

        RUSS::Mox_Update();

        NETPROTO::Packet_Reader_ reader(ptr, (uint32_t)size, "invalid broadcast game data payload");
        if (reader.Read_Uint32() != NATIVE_SNAPSHOT_VERSION_TAG) {
            exitm::Exit_With_Message("incompatible broadcast game data version");
        }

        // Interpret the packed byte stream using the original x86 little-endian layout.
        net.host_election_id = reader.Read_Int16();

        MOX::stardate = reader.Read_Uint32();

        MOX::settings.random_events = reader.Read_Uint8();
        MOX::settings.game_difficulty = reader.Read_Uint8();
        MOX::settings.number_of_players = reader.Read_Uint8();
        MOX::settings.galaxy_size = reader.Read_Uint8();
        MOX::settings.galaxy_age = reader.Read_Uint8();
        MOX::settings.strategic_combat_flag = reader.Read_Uint8();
        MOX::settings.starting_civilization_level = reader.Read_Uint8();
        MOX::settings.xenons_exist = reader.Read_Uint8();

        game_random::Set_Random_Seed(reader.Read_Uint32());

        RUSS::Mox_Update();

        MOX::NUM_PLAYERS = reader.Read_Int16();

        if (MOX::NUM_PLAYERS < 0 || MOX::NUM_PLAYERS > MAX_PLAYERS) {
            exitm::Exit_With_Message("invalid broadcast player count");
        }

        for (int i = 0; i < MOX::NUM_PLAYERS; ++i) {
            if (!NETPROTO::Read_Player(reader, MOX::player[i])) exitm::Exit_With_Message("invalid broadcast player record");
        }

        net.num_connected_players = 0;
        for (int16_t i = 0; i < MOX::NUM_PLAYERS; i++) {
            if (MOX::player[i].eliminated != 0) {
                MOX::player[i].network_player_id = -1;
                MOX::player[i].objectives = 0;
            } else {
                if (MOX::player[i].objectives == PLAYER_OBJECTIVE_HUMAN) {
                    net.num_connected_players++;
                }
            }
        }

        if (!SERIAL::Read_Star_Block(state, reader, 8)) {
            exitm::Exit_With_Message("invalid broadcast star roster");
        }
        reader.Require(SERIAL::STAR_BYTES);
        SERIAL::Read_Star_Payload(reader, state.antares_destination, true);

        RUSS::Mox_Update();

        {
            if (!SERIAL::Read_Planet_Block(state, reader, 8)) {
                exitm::Exit_With_Message("invalid broadcast planet records");
            }
        }

        if (!SERIAL::Read_Keyed_Colony_Block(state, reader)) {
            exitm::Exit_With_Message("invalid broadcast colony records");
        }

        MOX::NUM_SHIPS = reader.Read_Int16();

        if (MOX::NUM_SHIPS < 0 || MOX::NUM_SHIPS > MAX_SHIPS) {
            exitm::Exit_With_Message("invalid broadcast ship count");
        }

        reader.Require(MAX_SHIPS * SERIAL::SHIP_PACKET_BYTES);
        for (int i = 0; i < MAX_SHIPS; ++i) {
            const bool decoded = i < MOX::NUM_SHIPS ? SERIAL::Read_Ship_Packet(reader, MOX::ship[i], state)
                                                    : SERIAL::Read_Ship_Packet(reader, MOX::ship[i]);
            if (!decoded) exitm::Exit_With_Message("invalid broadcast ship record");
        }

        if (!SERIAL::Read_Leader_State(state, reader)) {
            exitm::Exit_With_Message("invalid broadcast leader roster");
        }

        REPORT::n_reports = reader.Read_Int16();

        if (REPORT::n_reports < 0 ||
            REPORT::n_reports > static_cast<int16_t>(std::size(REPORT::reports))) {
            exitm::Exit_With_Message("invalid broadcast report count");
        }

        for (auto& entry : REPORT::reports) SERIAL::Read_Report(&reader, &entry, 8, true);
        REPORT::Client_Initialize_Reports();

        for (int i = 0; i < EVENT_COUNT; ++i) SERIAL::Read_Event(reader, state, EVENTS::event_data[i], i, true);
        reader.Read_Bytes(MSG_::first_msg, MOX::NUM_PLAYERS);

        reader.Read_Bytes(MSG_::max_msgs, MOX::NUM_PLAYERS);

        reader.Read_Bytes(MSG_::n_msgs, MOX::NUM_PLAYERS);

        for (auto& entry : MSG_::msgs) SERIAL::Read_Message(&reader, &entry, 8, true);
        RUSS::Mox_Update();

        diplodef::COUNCIL_FLAG = reader.Read_Int16();

        diplodef::next_council_meeting_turn = reader.Read_Int16();

        reader.Read_Bytes(&MOX::antarans, sizeof(s_antaran));

        MOX::NUM_NEBULAS = reader.Read_Uint8();

        if (MOX::NUM_NEBULAS > MAX_NEBULAS) {
            exitm::Exit_With_Message("invalid broadcast nebula count");
        }

        reader.Read_Bytes(MOX::nebula, MOX::NUM_NEBULAS * sizeof(s_nebula));

        for (int16_t i = 0; i < (int16_t)(uint16_t)MOX::NUM_NEBULAS; ++i) {
            if (MOX::nebula[i].type < 0 || MOX::nebula[i].type >= 12) {
                exitm::Exit_With_Message("invalid broadcast nebula type");
            }
        }

        EVENTS::last_event_year = reader.Read_Uint32();

        EVENTS::event_delay_count = reader.Read_Int16();

        reader.Read_Bytes(MOX::bill_savegame, sizeof(MOX::bill_savegame));

        if (reader.Remaining() != 0) {
            exitm::Exit_With_Message("invalid trailing broadcast game data");
        }

        RUSS::Mox_Update();

        Net_Debug_Log(
            "Decode_Broadcast_Game_Data_Differential_: player=%d stardate_after=%u final_size=%u num_players=%d num_stars=%d num_ships=%d",
            (int)MOX::PLAYER_NUM,
            (unsigned int)MOX::stardate,
            (unsigned int)size,
            (int)MOX::NUM_PLAYERS,
            (int)std::ssize(game::current_state.stars),
            (int)MOX::NUM_SHIPS);

        return 1;
    }

    int8_t __cdecl Decode_Broadcast_Game_Data_Differential(
        void* buf, size_t size, uint8_t flag) {
        if (buf == nullptr || size == 0 ||
            size > NETMOX_MAX_GAME_DATA_PAYLOAD_BYTES) {
            exitm::Exit_With_Message("invalid broadcast game data payload");
        }
        Scoped_Net_Update_Suspension_ suspend_net_updates;
        uint8_t* received = static_cast<uint8_t*>(buf);
        uint8_t* full = received;
        size_t full_size = size;
        if (flag == 0) {
            full = received + size;
            s_net_snapshot_cache* cache = Open_Net_Snapshot_Cache(0);
            const int decoded = Decode_Difference_Buffer(cache,
                full,
                static_cast<int>(NETMOX_MAX_GAME_DATA_PAYLOAD_BYTES - size),
                received,
                static_cast<int>(size));
            if (decoded <= 0) {
                exitm::Exit_With_Message("error decoding broadcast");
            }
            full_size = static_cast<size_t>(decoded);
        }

        const bool lua_enabled = orion2re::lua::Network_Lua_Enabled() ||
            orion2re::lua::Has_Pending_Lua_Network_Setup();
        uint8_t* native = full;
        size_t native_size = full_size;
        const uint8_t* lua_state = nullptr;
        size_t lua_state_size = 0;
        uint32_t lua_flags = 0;
        if (lua_enabled) {
            if (full_size < O2GD_HEADER_SIZE) {
                exitm::Exit_With_Message("Lua-enabled game data header truncated");
            }
            SERIAL::Reader_ envelope(full,
                static_cast<uint32_t>(full_size),
                "invalid Lua-enabled game data envelope");
            const uint32_t magic = envelope.Read_Uint32();
            const uint16_t version = envelope.Read_Uint16();
            const uint16_t header_size = envelope.Read_Uint16();
            const uint32_t encoded_native_size = envelope.Read_Uint32();
            const uint32_t encoded_lua_size = envelope.Read_Uint32();
            const uint32_t flags = envelope.Read_Uint32();
            const uint64_t encoded_total = static_cast<uint64_t>(header_size) +
                encoded_native_size + encoded_lua_size;
            if (magic != O2GD_MAGIC || version != O2GD_VERSION ||
                header_size != O2GD_HEADER_SIZE ||
                encoded_total != full_size || encoded_lua_size == 0) {
                exitm::Exit_With_Message("invalid Lua-enabled game data envelope");
            }
            native = full + header_size;
            native_size = encoded_native_size;
            lua_state = native + native_size;
            lua_state_size = encoded_lua_size;
            lua_flags = flags;
        }

        std::vector<uint8_t> prior_native;
        if (lua_enabled) {
            try {
                prior_native.resize(NETMOX_MAX_GAME_DATA_PAYLOAD_BYTES);
            } catch (...) {
                exitm::Exit_With_Message(
                    "unable to allocate game data rollback checkpoint");
            }
            const uint32_t prior_size = Encode_Native_Game_Data_Snapshot(
                game::current_state, prior_native.data(),
                static_cast<uint32_t>(prior_native.size()));
            prior_native.resize(prior_size);
        }

        // Snapshots populate the backing storage, not the guest's local pointers into it.
        game::current = legacy_world::World();
        Decode_Native_Game_Data_Snapshot(game::current_state, native, native_size);
        orion2re::lua::s_lua_network_state_transaction* lua_transaction =
            nullptr;
        if (lua_enabled) {
            std::string diagnostic;
            bool committed = false;
            const bool prepared = orion2re::lua::Prepare_Lua_Network_Payload(
                lua_state,
                static_cast<uint32_t>(lua_state_size),
                lua_flags,
                static_cast<uint32_t>(MOX::NUM_SHIPS),
                &lua_transaction,
                &committed,
                &diagnostic);
            if (!prepared) {
                Decode_Native_Game_Data_Snapshot(
                    game::current_state, prior_native.data(), prior_native.size());
                exitm::Exit_With_Message(diagnostic.c_str());
            }
            if (!committed) {
                orion2re::lua::Commit_Lua_Network_State(&lua_transaction);
            }
        }

        s_net_snapshot_cache* cache = Open_Net_Snapshot_Cache(
            static_cast<uint32_t>(full_size));
        Write_Net_Snapshot_Cache(cache, full, full_size);
        return 1;
    }

    void __cdecl Mox_Net_Update() {
        if (MOX::game_type != 2 && MOX::game_type != 3) {
            return;
        }

        if (NETMOX::is_updating || NETMOX::in_receive_message || suspended_net_update_depth != 0) {
            return;
        }

        NETMOX::is_updating = 1;

        netcode::Net_Update();

        if (!NETMOX::net.is_network_active) {
            NETMOX::is_updating = 0;
            return;
        }

        int current_player_idx;
        if (MOX::NUM_PLAYERS != 0) {
            current_player_idx = NETMOX::next_update_player_counter % MOX::NUM_PLAYERS;
        } else {
            current_player_idx = -1;
        }

        RUSS::Mox_Update();

        NETMOX::next_update_player_counter++;

        if (current_player_idx == -1 || netcode::Net_Is_Sending()) {
            NETMOX::is_updating = 0;
            return;
        }

        bool message_sent = false;

        if ((NETMOX::net.sync_state == NET_SYNC_STATE_READY || NETMOX::net.sync_state == NET_SYNC_STATE_NEXT_TURN_READY) &&
            !NETMOX::net.player_phase_ready_sent_flags[current_player_idx]) {
            NETMOX::net.player_phase_ready_sent_flags[current_player_idx] = 1;
            if (MOX::player[current_player_idx].network_player_id != -1) {
                message_sent = true;
                NETMOX::Mox_Send_Message(MOX::player[current_player_idx].network_player_id, NET_PACKET_PLAYER_PHASE_READY, nullptr, 0);
            }
        }

        if (!message_sent && (NETMOX::net.ai_diplomacy_done_mask & (1 << current_player_idx))) {
            int8_t remote_player_id = NETMOX::net.player_net_info[current_player_idx].diplomacy_target_id;
            NETMOX::net.ai_diplomacy_done_mask &= ~(1 << current_player_idx);
            message_sent = true;
            NETMOX::Mox_Send_Message(MOX::player[current_player_idx].network_player_id, NET_PACKET_AI_DIPLOMACY_DONE, &remote_player_id, sizeof(remote_player_id));
        }

        for (int i = 0; i < MOX::NUM_PLAYERS; ++i) {
            if (NETMOX::net.players_sync_mask[i] & (1 << current_player_idx)) {
                s_NET_MSG_SYNC_PLAYER sync_pkt{};

                sync_pkt.player1_idx = i;
                sync_pkt.player1 = NETPROTO::Encode_Player(MOX::player[i]);

                NETPROTO::Write_Star_Surrender(sync_pkt.surrender_to, game::current_state, i);

                int32_t sync_target_idx = NETMOX::net.diplomacy_sync_mask[i];
                sync_pkt.player2_idx = sync_target_idx;
                sync_pkt.player2 = NETPROTO::Encode_Player(MOX::player[sync_target_idx]);

                if (MOX::PLAYER_NUM != current_player_idx && sync_target_idx != current_player_idx) {
                    if (MOX::player[current_player_idx].objectives == PLAYER_OBJECTIVE_HUMAN &&
                        !message_sent && NETMOX::net.player_race_selected_flags[current_player_idx]) {
                        message_sent = true;
                        NETMOX::Mox_Send_Message(MOX::player[current_player_idx].network_player_id, NET_PACKET_SYNC_PLAYER_STATE, &sync_pkt, sizeof(sync_pkt));
                    }
                }
                NETMOX::net.players_sync_mask[i] &= ~(1 << current_player_idx);
            }
        }

        if (!message_sent) {
            if (NETMOX::net.ai_diplomacy_active_mask & (1 << current_player_idx)) {
                int8_t remote_player_id = NETMOX::net.player_net_info[current_player_idx].diplomacy_target_id;
                if (!(NETMOX::net.players_sync_mask[remote_player_id] & (1 << current_player_idx))) {
                    message_sent = true;
                    NETMOX::net.ai_diplomacy_active_mask &= ~(1 << current_player_idx);
                    NETMOX::Mox_Send_Message(MOX::player[current_player_idx].network_player_id, NET_PACKET_AI_DIPLOMACY_BEGIN, &remote_player_id, sizeof(remote_player_id));
                }
            }
        }

        if (!message_sent) {
            if (NETMOX::net.ai_diplomacy_cancel_mask & (1 << current_player_idx)) {
                int8_t remote_player_id = NETMOX::net.player_net_info[current_player_idx].diplomacy_target_id;
                NETMOX::net.ai_diplomacy_cancel_mask &= ~(1 << current_player_idx);
                message_sent = true;
                NETMOX::Mox_Send_Message(MOX::player[current_player_idx].network_player_id, NET_PACKET_AI_DIPLOMACY_CANCELLED, &remote_player_id, sizeof(remote_player_id));
            }
        }

        if (!message_sent) {
            if (NETMOX::net.human_diplomacy_ignored_mask & (1 << current_player_idx)) {
                message_sent = true;
                NETMOX::net.human_diplomacy_ignored_mask &= ~(1 << current_player_idx);
                NETMOX::Mox_Send_Message(MOX::player[current_player_idx].network_player_id, NET_PACKET_HUMAN_DIPLOMACY_IGNORED, nullptr, 0);
            }
        }

        if (!message_sent) {
            int32_t player_num = MOX::PLAYER_NUM;
            int8_t* player_rank_sync_flags = NETMOX::net.host_election_rank_sync_cache;

            if (current_player_idx != player_num && MOX::player[current_player_idx].objectives == PLAYER_OBJECTIVE_HUMAN) {
                int8_t local_rank = NETMOX::net.host_election_player_rank[player_num];
                if (player_rank_sync_flags[current_player_idx] != local_rank) {
                    message_sent = true;
                    s_NET_MSG_RANK rank_msg;
                    rank_msg.rank = (int32_t)local_rank;
                    rank_msg.hit_next_turn = NETMOX::net.players_hit_next_turn[player_num];
                    player_rank_sync_flags[current_player_idx] = local_rank;
                    NETMOX::Mox_Send_Message(MOX::player[current_player_idx].network_player_id, NET_PACKET_HOST_ELECTION_RANK, &rank_msg, sizeof(rank_msg));
                }
            }
        }

        if (!message_sent) {
            if (NETMOX::net.local_net_id == NETMOX::net.net_host_id &&
                NETMOX::net.sync_state >= NET_SYNC_STATE_COMBAT_READY && NETMOX::net.sync_state < NET_SYNC_STATE_COMBAT_COMPLETE) {
                COMBFIND::Do_Net_Combat_Update(message_sent);
            }
        }

        if (!message_sent) {
            if (NETMOX::net.player_color_selected_flags[current_player_idx]) {
                message_sent = true;
                NETMOX::net.player_color_selected_flags[current_player_idx] = 0;
                s_NET_MSG_COLOR color_msg;
                color_msg.color = MOX::player[current_player_idx].color;
                color_msg.player_idx = (int8_t)current_player_idx;
                NETMOX::Mox_Send_Message((int16_t)netadapter::NETADAPTER_BROADCAST_EXCLUDING_SELF, NET_PACKET_PLAYER_COLOR_SELECTED, &color_msg, sizeof(color_msg));
            }
        }

        if (!message_sent) {
            if (NETMOX::net.player_race_selected_alt_flags[current_player_idx]) {
                message_sent = true;
                NETMOX::net.player_race_selected_alt_flags[current_player_idx] = 0;
                s_NET_MSG_RACE race_msg;
                race_msg.race = MOX::player[current_player_idx].race;
                race_msg.player_idx = (int8_t)current_player_idx;
                NETMOX::Mox_Send_Message((int16_t)netadapter::NETADAPTER_BROADCAST_EXCLUDING_SELF, NET_PACKET_PLAYER_RACE_SELECTED, &race_msg, sizeof(race_msg));
            }
        }

        if (!message_sent) {
            if (NETMOX::net.player_race_deselected_flags[current_player_idx]) {
                message_sent = true;
                NETMOX::net.player_race_deselected_flags[current_player_idx] = 0;
                NETMOX::net.player_race_selection_sync_pending[current_player_idx] = 1;
                uint8_t race = MOX::player[current_player_idx].race;
                NETMOX::Mox_Send_Message((int16_t)netadapter::NETADAPTER_BROADCAST_EXCLUDING_SELF, NET_PACKET_PLAYER_RACE_DESELECTED, &race, sizeof(race));
            }
        }

        if (!message_sent) {
            if (NETMOX::net.player_race_selection_sync_pending[current_player_idx]) {
                message_sent = true;
                NETMOX::net.player_race_selection_sync_pending[current_player_idx] = 0;
                NETMOX::Mox_Send_Message(MOX::player[current_player_idx].network_player_id, NET_PACKET_RACE_SELECTION_SYNCED, nullptr, 0);
            }
        }

        if (!message_sent) {
            if (NETMOX::net.diplomacy_initiator_id != -1) {
                int8_t target_idx = NETMOX::net.diplomacy_target_id;
                NETMOX::net.diplomacy_initiator_id = -1;
                auto packet = NETPROTO::Encode_Player(MOX::player[MOX::PLAYER_NUM]);
                NETMOX::Mox_Send_Message(MOX::player[target_idx].network_player_id, NET_PACKET_HUMAN_DIPLOMACY_READY_DATA, &packet, sizeof(packet));
            }
        }

        NETMOX::is_updating = 0;
    }

    void __cdecl Send_Game_Orders() {
        Scoped_Net_Update_Suspension_ suspend_net_updates;
        allocate::Push_Block(MOX::screen_seg);
        uint8_t* packet = reinterpret_cast<uint8_t*>(allocate::Allocate_Next_Block(MOX::screen_seg, NETMOX_MAX_GAME_DATA_PAYLOAD_BYTES));
        NETPROTO::Packet_Writer_ writer(packet, NETMOX_MAX_GAME_DATA_PAYLOAD_BYTES, "invalid game orders payload");

        Net_Debug_Log(
            "Send_Game_Orders_: player=%d stardate=%u sync=%u",
            (int)MOX::PLAYER_NUM,
            (unsigned int)MOX::stardate,
            (unsigned int)net.sync_state);

        writer.Write_Uint32(MOX::stardate);
        writer.Write_Uint8(static_cast<uint8_t>(MOX::PLAYER_NUM));
        NETPROTO::Write_Player(writer, MOX::player[MOX::PLAYER_NUM]);

        RUSS::Mox_Update();

        s_net_pack_star_order star_orders[MAX_STARS];
        memset(star_orders, 0, sizeof(star_orders));

        int16_t order_index = 0;
        writer.Write_Uint16(static_cast<uint16_t>(game::current_state.stars.size()));
        for (const auto& [id, star] : game::current_state.stars) {
                s_net_pack_star_order* order = &star_orders[order_index++];

                order->flags =
                    (((star.ignore_colony_ships >> MOX::PLAYER_NUM) & 1) << 0) |
                    (((star.ignore_combat_ships >> MOX::PLAYER_NUM) & 1) << 1) |
                    (((star.has_colony >> MOX::PLAYER_NUM) & 1) << 2) |
                    (((star.has_warp_field_interdictor >> MOX::PLAYER_NUM) & 1) << 3) |
                    (((star.has_tachyon >> MOX::PLAYER_NUM) & 1) << 4) |
                    (((star.has_subspace >> MOX::PLAYER_NUM) & 1) << 5) |
                    (((star.has_stargate >> MOX::PLAYER_NUM) & 1) << 6) |
                    (((star.has_jumpgate >> MOX::PLAYER_NUM) & 1) << 7) |
                    (((star.has_artemis_net >> MOX::PLAYER_NUM) & 1) << 8) |
                    (((star.has_dimensional_portal >> MOX::PLAYER_NUM) & 1) << 9);

                order->officer_index = star.officer_index[MOX::PLAYER_NUM];

                if (star.colonize_player == MOX::PLAYER_NUM) {
                    strlcpy(order->name, star.name, sizeof(order->name));
                }

                order->surrender_to = star.surrender_to[MOX::PLAYER_NUM];
                writer.Write_Uint32(id.id);
                writer.Write_Bytes(order, sizeof(*order));
        }

        RUSS::Mox_Update();

        writer.Write_Uint32(game::current_state.next_colony_id);
        for (const auto& [id, colony] : game::current_state.colonies) {
            if (colony.owner == MOX::PLAYER_NUM) {
                assert(colony.id == id);
                SERIAL::Write_Colony(writer, colony);
            }
        }

        writer.Write_Uint32(game::NO_COLONY.id);

        RUSS::Mox_Update();

        writer.Write_Int16((int16_t)MOX::NUM_SHIPS);

        if (MOX::NUM_SHIPS > 0) {
            for (int16_t i = 0; i < (int16_t)MOX::NUM_SHIPS; ++i) {
                if (MOX::ship[i].owner == MOX::PLAYER_NUM && MOX::ship[i].status != 5) {
                    writer.Write_Int16(i);
                    SERIAL::Write_Ship_Packet(writer, MOX::ship[i]);
                }
            }
        }

        writer.Write_Int16(-1);

        RUSS::Mox_Update();

        const auto& state = game::current_state;
        for (int16_t i = 0; i < MAX_LEADERS; ++i) {
            const auto& leader = state.leaders.at(i);
            if (leader.player_index == MOX::PLAYER_NUM ||
                deassigned_leaders[i] != 0) {
                SERIAL::Write_Leader_Update(writer, leader);
            }
        }

        writer.Write_Int16(-1);
        Clear_Leader_Deassignments();

        RUSS::Mox_Update();

        int32_t packet_size = (int32_t)writer.Length();
        net.sync_state = NET_SYNC_STATE_COMBAT_READY;
        net.combat_attacker_id = -1;
        Net_Debug_Log(
            "Send_Game_Orders_: player=%d packet_size=%d new_sync=%u",
            (int)MOX::PLAYER_NUM,
            packet_size,
            (unsigned int)net.sync_state);
        NETMOX::Mox_Send_Message(net.local_net_id, NET_PACKET_GAME_ORDERS_PAYLOAD, packet, (uint32_t)packet_size);

        allocate::Pop_Block(MOX::screen_seg);
    }

    void __cdecl Send_Drop_Out_Info(int player_idx, uint16_t msg_type) {
        uint8_t any_processed;
        uint8_t player_process_flags[MAX_PLAYERS];
        s_net_pack_drop_out_info packet{};

        MOX::player[player_idx].network_player_id = -1;
        RUSS::Make_Player_Into_AI(player_idx);

        packet.player_data = NETPROTO::Encode_Player(MOX::player[player_idx]);

        NETPROTO::Write_Star_Surrender(packet.star_surrender_to, game::current_state, player_idx);

        memset(player_process_flags, 1, MOX::NUM_PLAYERS);
        player_process_flags[MOX::PLAYER_NUM] = 0;

        do {
            any_processed = 0;
            if (MOX::NUM_PLAYERS > 0) {
                for (int i = 0; i < MOX::NUM_PLAYERS; ++i) {
                    if (player_process_flags[i] != 0) {
                        any_processed = 1;
                        if (MOX::player[i].network_player_id == -1) {
                            player_process_flags[i] = 0;
                        } else if (net.player_race_selected_flags[i] != 0) {
                            Mox_Send_Message(MOX::player[i].network_player_id, msg_type, &packet, sizeof(s_net_pack_drop_out_info));
                            do {
                                RUSS::Mox_Update();
                            } while (netcode::Net_Is_Sending() != 0);
                            player_process_flags[i] = 0;
                        }
                    }
                }
            }
        } while (any_processed != 0);
    }

    void __cdecl Host_Send_Player_Disconnected_Info(int player_idx) {
        if (!Is_Connected_Game_Player(player_idx)) {
            return;
        }

        const auto state = net.sync_state;
        if (!MULTPLAY::silent_player_drop && !MOX::player[player_idx].eliminated &&
            MOX::player[player_idx].objectives == PLAYER_OBJECTIVE_HUMAN &&
            (state == NET_SYNC_STATE_READY ||
                (state >= NET_SYNC_STATE_NEXT_TURN_READY && state <= NET_SYNC_STATE_COMBAT_COMPLETE) ||
                (state >= NET_SYNC_STATE_AI_DIPLOMACY_REQUESTED && state <= NET_SYNC_STATE_HUMAN_DIPLOMACY_READY) ||
                state == NET_SYNC_STATE_HUMAN_DIPLOMACY_CONNECTING || state == NET_SYNC_STATE_HUMAN_DIPLOMACY_BLOCKED)) {
            Scoped_Net_Update_Suspension_ suspend_net_updates;
            NETTURN::Confirm_Player_Drop(player_idx);
        }

        int16_t disconnected_network_id = MOX::player[player_idx].network_player_id;
        s_net_pack_player_disconnected_info_ packet{};
        packet.player_idx = (int16_t)player_idx;
        Build_Drop_Out_Info_From_Local_State(player_idx, &packet.drop_out_info);
        Apply_Drop_Out_Bookkeeping(player_idx, disconnected_network_id, true);

        Mox_Send_Message((int16_t)netadapter::NETADAPTER_BROADCAST_EXCLUDING_SELF, NET_PACKET_PLAYER_DISCONNECTED, &packet, (uint16_t)sizeof(packet));
    }

    void __cdecl Handle_Network_Player_Disconnected(int player_idx) {
        if (net.is_network_active == 0 || !Is_Connected_Game_Player(player_idx) || Is_Local_Player_Slot(player_idx)) {
            return;
        }

        int coordinator_idx = Elect_Disconnect_Recovery_Coordinator(player_idx);
        if (coordinator_idx >= 0 && Is_Local_Player_Slot(coordinator_idx)) {
            Host_Send_Player_Disconnected_Info(player_idx);
        }
    }

    int __cdecl Decode_Difference_Buffer(s_net_snapshot_cache* cache, uint8_t* out_buffer, int max_out_len, uint8_t* diff_buffer, int diff_len) {
        if (cache == nullptr || cache->buffer == nullptr || out_buffer == nullptr ||
            diff_buffer == nullptr || max_out_len < 0 || diff_len <= 0) {
            exitm::Exit_With_Message("invalid network snapshot difference");
            return 0;
        }

        int out_size = 0;
        int position = 0;
        while (position < diff_len) {
            const uint8_t token = diff_buffer[position++];
            if (token == 0xFF) {
                if (position == diff_len) {
                    return out_size;
                }
                break;
            }

            const int count = token <= 0xF4 ? token : token - 0xF5;
            if (count > max_out_len - out_size) {
                break;
            }
            if (token <= 0xF4) {
                if (static_cast<uint32_t>(out_size) > cache->buffer_size ||
                    static_cast<uint32_t>(count) > cache->buffer_size - out_size) {
                    break;
                }
                memcpy(out_buffer + out_size, cache->buffer + out_size, count);
            } else {
                if (count > diff_len - position) {
                    break;
                }
                // Literal runs may extend beyond the previous snapshot when Lua state grows.
                memcpy(out_buffer + out_size, diff_buffer + position, count);
                position += count;
            }
            out_size += count;
            RUSS::Mox_Update();
        }
        exitm::Exit_With_Message("invalid network snapshot difference");
        return 0;
    }

    size_t __cdecl fread_net_snapshot_cache(void* ptr, size_t size, size_t count, s_net_snapshot_cache* cache) {
        if (cache->buffer == nullptr) {
            return 0;
        }

        size_t total_size = count * size;
        memcpy(ptr, cache->current_ptr, total_size);

        cache->current_ptr += total_size;

        if (cache->buffer + cache->buffer_size < cache->current_ptr) {
            return 0;
        }

        return count;
    }

    size_t __cdecl fwrite_net_snapshot_cache(const void* ptr, size_t size, size_t count, s_net_snapshot_cache* cache) {
        if (cache->buffer != nullptr) {
            size_t total_size = count * size;

            // BUG: potential overflow if total_size > (buffer + buffer_size - current_ptr)
            memcpy(cache->current_ptr, ptr, total_size);

            uint8_t* new_ptr = cache->current_ptr + total_size;
            cache->current_ptr = new_ptr;

            if (cache->buffer + cache->buffer_size < new_ptr) {
                return 0;
            }

            return count;
        }

        return 0;
    }

    int32_t __cdecl fgetc_net_snapshot_cache(s_net_snapshot_cache* cache) {
        if (cache->buffer != nullptr) {
            if (cache->buffer + cache->buffer_size == cache->current_ptr) {
                return -1;
            }
            uint8_t val = *cache->current_ptr;
            cache->current_ptr++;
            return val;
        }

        return 0;
    }

    int32_t __cdecl Make_Difference_Buffer(s_net_snapshot_cache* cache, uint8_t* buffer, int32_t buffer_size, uint8_t* diff_buffer, int32_t diff_buffer_max_size) {
        if (cache == nullptr || cache->buffer == nullptr || buffer == nullptr ||
            diff_buffer == nullptr || buffer_size < 0 || diff_buffer_max_size <= 0) {
            exitm::Exit_With_Message("invalid network snapshot difference buffers");
            return 0;
        }

        const auto matches = [&](int32_t offset) {
            return static_cast<uint32_t>(offset) < cache->buffer_size &&
                buffer[offset] == cache->buffer[offset];
        };
        int32_t position = 0;
        int32_t out_size = 0;
        int32_t update_bytes = 0;
        // Keep the legacy copy/literal tokens, but terminate at the new snapshot's length.
        while (position < buffer_size) {
            const bool copy = matches(position);
            const int32_t limit = copy ? 0xF4 : 9;
            int32_t count = 1;
            while (count < limit && count < buffer_size - position &&
                matches(position + count) == copy) {
                ++count;
            }
            const int32_t encoded_size = copy ? 1 : count + 1;
            if (encoded_size >= diff_buffer_max_size - out_size) {
                exitm::Exit_With_Message("network snapshot difference buffer too small");
                return 0;
            }
            diff_buffer[out_size++] = static_cast<uint8_t>(copy ? count : 0xF5 + count);
            if (!copy) {
                memcpy(diff_buffer + out_size, buffer + position, count);
                out_size += count;
            }
            position += count;
            update_bytes += count;
            if (update_bytes >= 256) {
                RUSS::Mox_Update();
                update_bytes = 0;
            }
        }

        diff_buffer[out_size++] = 0xFF;
        return out_size;
    }

    void __cdecl Write_Net_Snapshot_Cache(s_net_snapshot_cache* cache, const void* buffer, size_t size) {
        RUSS::Mox_Update();

        if (fseek_net_snapshot_cache(cache, 0, 0) != 0) {
            exitm::Exit_With_message2(net_snapshot_cache_error);
        }

        RUSS::Mox_Update();

        if (fwrite_net_snapshot_cache(buffer, size, 1, cache) != 1) {
            exitm::Exit_With_message2(net_snapshot_cache_error);
        }

        RUSS::Mox_Update();
    }

    void __cdecl Net_Request_Human_Diplomacy(uint8_t target_player_id) {
        net.diplomacy_target_id = (int8_t)target_player_id;

        if (((net.players_active_mask >> (target_player_id & 0x1F)) & 1) != 0) {
            net.sync_state = NET_SYNC_STATE_HUMAN_DIPLOMACY_CONNECTING;
            net.diplomacy_active_flag = 1;
            net.diplomacy_initiator_id = (int8_t)target_player_id;

            uint8_t mask = (uint8_t) ~(1 << (target_player_id & 0x1F));
            net.players_active_mask &= mask;
            net.human_diplomacy_request_mask &= mask;
            net.is_diplomacy_initiator = 0;

            auto player_packet = NETPROTO::Encode_Player(MOX::player[MOX::PLAYER_NUM]);
            Mox_Send_Message(
                MOX::player[target_player_id].network_player_id,
                NET_PACKET_ACCEPT_HUMAN_DIPLOMACY,
                &player_packet,
                sizeof(player_packet));
        } else {
            net.sync_state = NET_SYNC_STATE_HUMAN_DIPLOMACY_REQUESTED;
            net.diplomacy_active_flag = 0;

            if (Human_Hit_Next_Turn(target_player_id) == 0) {
                s_net_pack_req_human_diplomacy packet{};

                packet.player = NETPROTO::Encode_Player(MOX::player[MOX::PLAYER_NUM]);

                NETPROTO::Write_Star_Surrender(packet.surrender_to, game::current_state, MOX::PLAYER_NUM);

                Mox_Send_Message(
                    MOX::player[target_player_id].network_player_id,
                    NET_PACKET_REQUEST_HUMAN_DIPLOMACY,
                    &packet,
                    sizeof(packet));
            }
        }
    }

    int __cdecl fseek_net_snapshot_cache(s_net_snapshot_cache* cache, int offset, int origin) {
        if (cache->buffer == nullptr) {
            return -1;
        }

        if (origin == 0 /* SEEK_SET */) {
            cache->current_ptr = cache->buffer + offset;
            return 0;
        }
        if (origin == 1 /* SEEK_CUR */) {
            cache->current_ptr = cache->current_ptr + offset;
            return 0;
        }

        return -1;
    }

    bool __cdecl Decode_Game_Orders(const void* data, uint32_t len, int16_t expected_player) {
        Scoped_Net_Update_Suspension_ suspend_net_updates;
        NETPROTO::Packet_Reader_ reader(data, len, "invalid game orders payload", true);

        const auto stardate = reader.Read_Uint32();
        const auto player_idx = reader.Read_Uint8();

        Net_Debug_Log(
            "Decode_Game_Orders_: local_player=%d sender_player=%u len=%u stardate=%u",
            (int)MOX::PLAYER_NUM,
            (unsigned int)player_idx,
            (unsigned int)len,
            (unsigned int)MOX::stardate);

        if (reader.Failed() || stardate != MOX::stardate || player_idx != expected_player ||
            player_idx >= MAX_PLAYERS || player_idx >= MOX::NUM_PLAYERS) return false;

        if (!reader.Require(NETPROTO::PLAYER_BYTES + sizeof(uint16_t))) return false;

        s_player player = {};
        if (!NETPROTO::Read_Player(reader, player)) return false;

        const int32_t freight_count = player.settlers_freighted;
        if (freight_count < 0 ||
            freight_count > SETTLER::max_pop_freighted ||
            freight_count > MAX_FREIGHTED_SETTLERS) return false;

        auto& state = game::current_state;
        const auto star_count = reader.Read_Uint16();
        if (star_count != state.stars.size() || !reader.Require(star_count * (sizeof(uint32_t) + sizeof(s_net_pack_star_order)))) return false;
        std::map<game::id_star, std::array<uint8_t, sizeof(s_net_pack_star_order)>> star_updates;
        for (uint16_t i = 0; i < star_count; ++i) {
            const game::id_star id{reader.Read_Uint32()};
            const auto star = state.stars.find(id);
            if (star == state.stars.end()) return false;
            const auto [update, added] = star_updates.try_emplace(id);
            if (!added) return false;
            reader.Read_Bytes(update->second.data(), update->second.size());
            if (star->second.colonize_player == player_idx && !memchr(update->second.data() + 4, 0, sizeof(s_net_pack_star_order::name))) return false;
        }

        const auto next_id = reader.Read_Uint32();
        std::vector<s_colony> colony_orders;
        std::set<game::id_planet> colony_sites;
        auto colony_count = state.colonies.size();
        for (;;) {
            s_colony colony{};
            if (!SERIAL::Read_Colony_Update(reader, colony)) return false;
            const auto id = colony.id;
            if (id == game::NO_COLONY) break;
            if (id.id >= next_id || (!colony_orders.empty() && id <= colony_orders.back().id) || colony.owner != player_idx) return false;
            const auto planet = state.planets.find(colony.planet_id);
            if (planet == state.planets.end() || !colony_sites.insert(colony.planet_id).second ||
                (planet->second.colony_id != game::NO_COLONY && planet->second.colony_id != id)) return false;
            const auto existing = state.colonies.find(id);
            if (existing == state.colonies.end()) {
                if (++colony_count > MAX_COLONIES) return false;
            } else if (existing->second.planet_id != colony.planet_id || existing->second.owner != player_idx) {
                return false;
            }
            for (int p = 0; p < MAX_PLAYERS; ++p) {
                const auto& queue = state.colony_reservations[p];
                if (p != player_idx && std::binary_search(queue.begin(), queue.end(), id)) return false;
            }
            colony_orders.push_back(colony);
        }

        const auto max_ships = reader.Read_Int16();
        if (reader.Failed() || max_ships < 0 || max_ships > MAX_SHIPS) return false;
        std::vector<std::pair<int16_t, s_ship_data>> ship_orders;
        for (;;) {
            const auto id = reader.Read_Int16();
            if (reader.Failed()) return false;
            if (id == -1) break;
            if (id < 0 || id >= max_ships || (!ship_orders.empty() && id <= ship_orders.back().first)) return false;
            s_ship_data ship{};
            if (!SERIAL::Read_Ship_Packet(reader, ship, state)) return false;
            ship_orders.emplace_back(id, ship);
        }

        std::vector<s_leader_data> leader_orders;
        for (;;) {
            s_leader_data leader{};
            if (!SERIAL::Read_Leader_Update(reader, leader)) return false;
            if (leader.id == game::NO_LEADER) break;
            if (!leader_orders.empty() && leader.id.id <= leader_orders.back().id.id) return false;
            leader_orders.push_back(leader);
        }
        if (reader.Remaining() != 0) return false;

        // Nothing above changes the live game. Apply only after all sections validate.
        MOX::player[player_idx] = player;
        RUSS::Mox_Update();

        for (const auto& [id, bytes] : star_updates) {
                const auto* update = bytes.data();
                uint32_t flags;
                memcpy(&flags, update, sizeof(flags));
                s_star_data* star = &state.stars.at(id);

                if (flags & 0x1)
                    star->ignore_colony_ships |= (1 << player_idx);
                else
                    star->ignore_colony_ships &= ~(1 << player_idx);

                if (flags & 0x2)
                    star->ignore_combat_ships |= (1 << player_idx);
                else
                    star->ignore_combat_ships &= ~(1 << player_idx);

                if (flags & 0x4)
                    star->has_colony |= (1 << player_idx);
                else
                    star->has_colony &= ~(1 << player_idx);

                if (flags & 0x8)
                    star->has_warp_field_interdictor |= (1 << player_idx);
                else
                    star->has_warp_field_interdictor &= ~(1 << player_idx);

                if (flags & 0x10)
                    star->has_tachyon |= (1 << player_idx);
                else
                    star->has_tachyon &= ~(1 << player_idx);

                if (flags & 0x20)
                    star->has_subspace |= (1 << player_idx);
                else
                    star->has_subspace &= ~(1 << player_idx);

                if (flags & 0x40)
                    star->has_stargate |= (1 << player_idx);
                else
                    star->has_stargate &= ~(1 << player_idx);

                if (flags & 0x80)
                    star->has_jumpgate |= (1 << player_idx);
                else
                    star->has_jumpgate &= ~(1 << player_idx);

                if (flags & 0x100)
                    star->has_artemis_net |= (1 << player_idx);
                else
                    star->has_artemis_net &= ~(1 << player_idx);

                if (flags & 0x200)
                    star->has_dimensional_portal |= (1 << player_idx);
                else
                    star->has_dimensional_portal &= ~(1 << player_idx);

                star->officer_index[player_idx] = update[0x13];

                if (star->colonize_player == player_idx) {
                    strlcpy(star->name, (const char*)(update + 4), sizeof(star->name));
                }

                star->surrender_to[player_idx] = update[0x17];
        }

        RUSS::Mox_Update();

        state.next_colony_id = std::max(state.next_colony_id, next_id);
        for (const auto& colony : colony_orders) {
            const auto id = colony.id;
            state.colonies.insert_or_assign(id, colony);
            state.planets.at(colony.planet_id).colony_id = id;
            std::erase(state.colony_reservations[player_idx], id);

            RUSS::Mox_Update();
        }

        RUSS::Mox_Update();

        if (MOX::NUM_SHIPS < max_ships) {
            MOX::NUM_SHIPS = max_ships;
        }

        int16_t ship_idx_prev = 0;
        for (const auto& [ship_idx, ship] : ship_orders) {
            for (int16_t i = ship_idx_prev; i < ship_idx; ++i) {
                if (MOX::ship[i].owner == player_idx && MOX::ship[i].status != 5) {
                    MOX::ship[i].officer_index = -1;
                    SHIPS::Kill_Ship(i);
                }
            }

            MOX::ship[ship_idx] = ship;

            ship_idx_prev = ship_idx + 1;
            RUSS::Mox_Update();
        }

        for (int16_t i = ship_idx_prev; i < MOX::NUM_SHIPS; ++i) {
            if (MOX::ship[i].owner == player_idx && MOX::ship[i].status != 5) {
                MOX::ship[i].officer_index = -1;
                SHIPS::Kill_Ship(i);
            }
        }

        RUSS::Mox_Update();

        int leader_idx_prev = 0;
        for (const auto& leader : leader_orders) {
            const auto leader_idx = static_cast<int>(leader.id.id);
            for (int i = leader_idx_prev; i < leader_idx; ++i) {
                if (state.leaders.at(i).player_index == player_idx) {
                    NETMOX::Ditch_Leader(state, i);
                }
            }

            state.leaders.at(leader_idx) = leader;

            leader_idx_prev = leader_idx + 1;
        }

        for (int i = leader_idx_prev; i < MAX_LEADERS; ++i) {
            if (state.leaders.at(i).player_index == player_idx) {
                NETMOX::Ditch_Leader(state, i);
            }
        }

        RUSS::Mox_Update();
        return true;
    }

    s_net_snapshot_cache* __cdecl Open_Net_Snapshot_Cache(uint32_t size) {
        RUSS::Mox_Update();

        if (net_snapshot_cache.buffer != nullptr && size != 0 && size != net_snapshot_cache.buffer_size) {
            free(net_snapshot_cache.buffer);
            net_snapshot_cache.buffer = nullptr;
        }

        if (net_snapshot_cache.buffer == nullptr) {
            if (size == 0) {
                exitm::Exit_With_message2(net_snapshot_cache_error);
                return &net_snapshot_cache;
            }

            net_snapshot_cache.buffer = static_cast<uint8_t*>(malloc(size));
            if (net_snapshot_cache.buffer == nullptr) {
                exitm::Exit_With_message2(net_snapshot_cache_error);
                return &net_snapshot_cache;
            }

            net_snapshot_cache.buffer_size = size;
        }

        net_snapshot_cache.current_ptr = net_snapshot_cache.buffer;
        return &net_snapshot_cache;
    }

    int16_t __cdecl Get_Next_AI_Talker(int32_t target_player_id) {
        uint8_t weights[MAX_PLAYERS] = {0};
        uint8_t max_weight = 0;
        bool already_talking = false;

        if (MOX::NUM_PLAYERS <= 0) {
            return -1;
        }

        const int16_t player_count = std::clamp<int16_t>(MOX::NUM_PLAYERS, 0, MAX_PLAYERS);
        for (int16_t i = 0; i < player_count; ++i) {
            if (NETMOX::net.player_net_info[i].diplomacy_target_id == target_player_id) {
                if (NETMOX::net.player_net_info[i].player_state == NET_PLAYER_STATE_AI_DIPLOMACY_ACTIVE) {
                    already_talking = true;
                } else {
                    // Preserve assembly logic exactly:
                    // TRAIT_CHARISMATIC != 0 => 3, else TRAIT_REPULSIVE == 0 => 2, else 1
                    if (MOX::player[i].traits[TRAIT_CHARISMATIC] != 0) {
                        weights[i] = 3;
                    } else {
                        weights[i] = 1;
                        if (MOX::player[i].traits[TRAIT_REPULSIVE] == 0) {
                            weights[i] = 2;
                        }
                    }

                    if (max_weight < weights[i]) {
                        max_weight = weights[i];
                    }
                }
            }
        }

        if (already_talking) {
            return -1;
        }

        // Filter out candidates that do not have the highest priority
        for (int16_t i = 0; i < player_count; ++i) {
            if (weights[i] < max_weight) {
                weights[i] = 0;
            }
        }

        return RUSS::Get_Weighted_Choice_Char(weights, player_count);
    }

    void __cdecl Decode_Pick_Target_Info(void* packet_data, uint32_t len) {
        NETPROTO::Packet_Reader_ reader(packet_data, len, "invalid combat payload");

        // 1. Read Combat Star ID
        reader.Require(COMBAT_DESTINATION_BYTES + TARGET_LIST_BYTES);
        const auto destination = Read_Combat_Destination(reader);
        if (!destination) return;
        const auto combat_star_id = game::Destination_Row(game::current_state, *destination);

        NETMOX::net.combat_attacker_id = static_cast<int8_t>(MOX::PLAYER_NUM);
        NETMOX::net.combat_destination = *destination;

        // 2. Read Target Info
        s_combfind_target target_info;
        for (auto& colony : target_info.colony_targets) colony = game::id_colony{reader.Read_Uint32()};
        target_info.num_colony_targets = reader.Read_Int16();
        for (int i = 0; i < 15; ++i) target_info.player_targets[i] = reader.Read_Int16();
        target_info.num_player_targets = reader.Read_Int16();
        if (target_info.num_colony_targets < 0 || target_info.num_colony_targets > 5 ||
            target_info.num_player_targets < 0 || target_info.num_player_targets > 15) return;

        RUSS::Mox_Update();

        // If we are not the host, unpack the state data
        if (NETMOX::net.local_net_id != NETMOX::net.net_host_id) {
            // Pass the remaining buffer pointer to Decode_Star_Player_Presence
            reader.Require(k_star_player_presence_payload_bytes);
            COMBFIND::Decode_Star_Player_Presence(&reader, -1);

            // 3. Read Player Technology State
            if (MOX::NUM_PLAYERS > 0) {
                for (int i = 0; i < MOX::NUM_PLAYERS; ++i) {
                    reader.Read_Bytes(MOX::player[i].tech_fields, sizeof(MOX::player[i].tech_fields));
                    reader.Read_Bytes(MOX::player[i].tech_applications, sizeof(MOX::player[i].tech_applications));
                }
            }

            // 4. Read Leaders
            auto& state = game::current_state;
            for (auto& leader : state.leaders) {
                leader.player_index = -1;
            }

            while (true) {
                s_leader_data leader{};
                if (!SERIAL::Read_Leader_Update(reader, leader)) exitm::Exit_With_Message("invalid combat leader record");
                if (leader.id == game::NO_LEADER) break;
                state.leaders.at(leader.id.id) = leader;
                RUSS::Mox_Update();
            }
            RUSS::Mox_Update();

            // 5. Read Star Officer Indexes
            if (MOX::NUM_PLAYERS > 0) {
                for (int i = 0; i < MOX::NUM_PLAYERS; ++i) {
                    game::Destination_Record_At_Row(state, combat_star_id).officer_index[i] = static_cast<char>(reader.Read_Uint8());
                }
            }
            RUSS::Mox_Update();

            // 6. Read Planets and Colonies
            const auto next_colony_id = reader.Read_Uint32();
            state.next_colony_id = std::max(state.next_colony_id, next_colony_id);

            for (int i = 0; i < 5; ++i) {
                const auto planet_idx = game::Destination_Record_At_Row(state, combat_star_id).orbit_planet_ids[i];

                if (planet_idx != game::NO_PLANET) {
                    const game::id_colony new_colony{reader.Read_Uint32()};
                    int8_t p_type = static_cast<int8_t>(reader.Read_Uint8());
                    char p_climate = static_cast<char>(reader.Read_Uint8());
                    auto& planet = state.planets.at(planet_idx);
                    const auto old_colony = planet.colony_id;
                    if (new_colony != game::NO_COLONY) {
                        if (new_colony.id >= next_colony_id) return;
                        const auto existing = state.colonies.find(new_colony);
                        if (existing != state.colonies.end() && existing->second.planet_id != planet_idx) return;
                        if (existing == state.colonies.end() && old_colony == game::NO_COLONY && state.colonies.size() >= MAX_COLONIES) return;
                        s_colony colony{};
                        SERIAL::Read_Colony_With_Id(reader, colony, new_colony);
                        if (colony.planet_id != planet_idx) return;
                        state.colonies.insert_or_assign(colony.id, colony);
                        for (auto& queue : state.colony_reservations) std::erase(queue, new_colony);
                    }
                    if (old_colony != new_colony) state.colonies.erase(old_colony);
                    planet.colony_id = new_colony;
                    planet.planet_type = p_type;
                    planet.climate = p_climate;
                }
            }
            RUSS::Mox_Update();

            // 7. Read Ships
            MOX::NUM_SHIPS = reader.Read_Int16();

            if (MOX::NUM_SHIPS > 0) {
                for (int16_t i = 0; i < MOX::NUM_SHIPS; ++i) {
                    MOX::ship[i].status = 5; // Status 5 = travelling / in combat
                }
            }

            while (true) {
                int16_t ship_id = reader.Read_Int16();

                if (ship_id == -1) {
                    break;
                }

                if (ship_id < 0 || ship_id >= MAX_SHIPS || !SERIAL::Read_Ship_Packet(reader, MOX::ship[ship_id], state))
                    exitm::Exit_With_Message("invalid combat ship record");

                RUSS::Mox_Update();
            }
        }

        // 8. Finalize synchronization state
        for (int i = 0; i < target_info.num_colony_targets; ++i) {
            if (!game::current_state.colonies.contains(target_info.colony_targets[i].get())) return;
        }
        COMBFIND::Copy_Target_Info(&target_info);
        MAINSCR::Update_Ship_Icons_In_Between_Turns();

        NETMOX::net.player_net_info[MOX::PLAYER_NUM].player_state = NET_PLAYER_STATE_PICK_TARGET_INFO;
        NETMOX::net.sync_state = NET_SYNC_STATE_PICKING_COMBAT_TARGET;
    }

    void __cdecl Decode_You_Are_Target_Info(void* packet_data, uint32_t len) {
        NETPROTO::Packet_Reader_ reader(packet_data, len, "invalid combat payload");

        // 1. Read Header
        reader.Require(sizeof(s_packet_0x12));
        const auto destination = Read_Combat_Destination(reader);
        if (!destination) return;
        const auto star_id = game::Destination_Row(game::current_state, *destination);
        int16_t attacker_id = reader.Read_Int16();
        int16_t defender_id = reader.Read_Int16();
        const game::id_colony target_colony_id{reader.Read_Uint32()};

        NETMOX::net.combat_destination = *destination;
        NETMOX::net.combat_attacker_id = static_cast<int8_t>(attacker_id);
        NETMOX::net.combat_defender_id = defender_id;
        NETMOX::net.combat_target_player_id = MOX::PLAYER_NUM;

        // If we are not the host, unpack the state data
        if (NETMOX::net.local_net_id != NETMOX::net.net_host_id) {
            // Pass the remaining buffer pointer
            reader.Require(k_star_player_presence_payload_bytes);
            COMBFIND::Decode_Star_Player_Presence(&reader, -1);

            // 2. Read Player Technology State
            if (attacker_id < MAX_PLAYERS) {
                reader.Read_Bytes(MOX::player[attacker_id].tech_fields, sizeof(MOX::player[attacker_id].tech_fields));
                reader.Read_Bytes(MOX::player[attacker_id].tech_applications, sizeof(MOX::player[attacker_id].tech_applications));
            }

            if (MOX::PLAYER_NUM < MAX_PLAYERS) {
                reader.Read_Bytes(MOX::player[MOX::PLAYER_NUM].tech_fields, sizeof(MOX::player[MOX::PLAYER_NUM].tech_fields));
                reader.Read_Bytes(MOX::player[MOX::PLAYER_NUM].tech_applications, sizeof(MOX::player[MOX::PLAYER_NUM].tech_applications));
            }

            // 3. Read Leaders
            auto& state = game::current_state;
            for (auto& leader : state.leaders) {
                int16_t owner = leader.player_index;
                if (owner == attacker_id || owner == MOX::PLAYER_NUM) {
                    leader.player_index = -1;
                }
            }

            while (true) {
                s_leader_data leader{};
                if (!SERIAL::Read_Leader_Update(reader, leader)) exitm::Exit_With_Message("invalid combat leader record");
                if (leader.id == game::NO_LEADER) break;
                state.leaders.at(leader.id.id) = leader;
            }

            // 4. Read Star Officer Indexes
            if (attacker_id < MAX_PLAYERS) {
                game::Destination_Record_At_Row(state, star_id).officer_index[attacker_id] = static_cast<char>(reader.Read_Uint8());
            }

            if (MOX::PLAYER_NUM < MAX_PLAYERS) {
                game::Destination_Record_At_Row(state, star_id).officer_index[MOX::PLAYER_NUM] = static_cast<char>(reader.Read_Uint8());
            }

            // 5. Read Targeted Colony Data
            if (target_colony_id != game::NO_COLONY) {
                auto& state = game::current_state;
                const auto next_colony_id = reader.Read_Uint32();
                state.next_colony_id = std::max(state.next_colony_id, next_colony_id);

                const game::id_planet planet_idx{reader.Read_Uint32()};

                if (target_colony_id.id >= next_colony_id) return;
                const auto planet = state.planets.find(planet_idx);
                if (planet == state.planets.end()) return;
                const auto old_colony = planet->second.colony_id;
                const auto existing = state.colonies.find(target_colony_id);
                if (existing != state.colonies.end() && existing->second.planet_id != planet_idx) return;
                if (existing == state.colonies.end() && old_colony == game::NO_COLONY && state.colonies.size() >= MAX_COLONIES) return;

                int8_t p_type = static_cast<int8_t>(reader.Read_Uint8());

                char p_climate = static_cast<char>(reader.Read_Uint8());

                s_colony colony{};
                SERIAL::Read_Colony_With_Id(reader, colony, target_colony_id);
                if (colony.planet_id != planet_idx) return;
                if (old_colony != target_colony_id) state.colonies.erase(old_colony);
                state.colonies.insert_or_assign(colony.id, colony);
                for (auto& queue : state.colony_reservations) std::erase(queue, target_colony_id);
                planet->second.colony_id = target_colony_id;
                planet->second.planet_type = p_type;
                planet->second.climate = p_climate;
            }

            // 6. Read Ships
            MOX::NUM_SHIPS = reader.Read_Int16();

            if (MOX::NUM_SHIPS > 0) {
                for (int16_t i = 0; i < MOX::NUM_SHIPS; ++i) {
                    MOX::ship[i].status = 5; // Status 5 = travelling / in combat
                }
            }

            while (true) {
                int16_t ship_id = reader.Read_Int16();

                if (ship_id == -1) {
                    break;
                }

                if (ship_id < 0 || ship_id >= MAX_SHIPS || !SERIAL::Read_Ship_Packet(reader, MOX::ship[ship_id], state))
                    exitm::Exit_With_Message("invalid combat ship record");
            }
        }

        // 7. Finalize synchronization state
        const auto& colonies = game::current_state.colonies;
        const auto target_colony = colonies.find(target_colony_id);
        NETMOX::net.combat_planet_id = target_colony == colonies.end() ? game::NO_PLANET : target_colony->second.planet_id;
        NETMOX::net.sync_state = NET_SYNC_STATE_COMBAT_RESOLUTION_DISPATCH;
    }

    void __cdecl Decode_Pick_Your_Position(s_net_pack_pick_your_position* packet, uint32_t len) {
        if (packet == nullptr || len < sizeof(packet->num_players)) {
            return;
        }

        int32_t num_players = packet->num_players;
        uint32_t expected_len = (uint32_t)sizeof(packet->num_players) +
            (uint32_t)num_players * (uint32_t)sizeof(packet->players[0]);
        if (num_players < 0 || num_players > MAX_PLAYERS || len != expected_len) {
            return;
        }

        std::array<s_player, MAX_PLAYERS> players;
        for (int i = 0; i < num_players; ++i) {
            if (!NETPROTO::Decode_Player(&packet->players[i], sizeof(packet->players[i]), players[i])) return;
        }
        MOX::NUM_PLAYERS = (int16_t)num_players;
        std::copy_n(players.begin(), num_players, MOX::player);
    }

    void __cdecl Decode_Battle_Results(void* packet_data, uint32_t len) {
        NETPROTO::Packet_Reader_ reader(packet_data, len, "invalid combat payload");

        // 1. Read Header
        reader.Require(sizeof(int16_t) * 4);
        int16_t reporting_player_id = reader.Read_Int16();
        int16_t attacker_id = reader.Read_Int16();
        int16_t defender_id = reader.Read_Int16();
        int16_t combat_status = reader.Read_Int16();

        NETMOX::net.player_net_info[attacker_id].combat_result = (int8_t)combat_status;
        NETMOX::net.player_net_info[defender_id].combat_result = (int8_t)combat_status;

        RUSS::Mox_Update();

        // 2. Filter: Only unpack if we aren't the reporter
        if (MOX::PLAYER_NUM != reporting_player_id) {
            bool skip_unpack = false;
            // Do not unpack if the reporter is the defender, the attacker is a valid player,
            // and the attacker is a human player (objective == 100 / 'd')
            if (defender_id == reporting_player_id && attacker_id < MAX_PLAYERS) {
                if (MOX::player[attacker_id].objectives == PLAYER_OBJECTIVE_HUMAN) {
                    skip_unpack = true;
                }
            }

            if (!skip_unpack) {
                reader.Require(COMBAT_DESTINATION_BYTES + k_star_player_presence_payload_bytes + sizeof(int16_t) * 2);
                const auto destination = Read_Combat_Destination(reader);
                if (!destination) return;
                const auto combat_star_id = game::Destination_Row(game::current_state, *destination);

                COMBFIND::Decode_Star_Player_Presence(&reader, combat_star_id);

                reader.Skip(sizeof(int16_t));

                const game::id_planet combat_planet_idx{reader.Read_Uint32()};

                RUSS::Mox_Update();

                // 3. Unpack Tech Applications and Officer Indexes
                if (attacker_id < MAX_PLAYERS) {
                    reader.Read_Bytes(MOX::player[attacker_id].tech_applications, sizeof(MOX::player[attacker_id].tech_applications));
                    game::Destination_Record_At_Row(game::current_state, combat_star_id).officer_index[attacker_id] = static_cast<char>(reader.Read_Uint8());
                }

                if (defender_id < MAX_PLAYERS) {
                    reader.Read_Bytes(MOX::player[defender_id].tech_applications, sizeof(MOX::player[defender_id].tech_applications));
                    game::Destination_Record_At_Row(game::current_state, combat_star_id).officer_index[defender_id] = static_cast<char>(reader.Read_Uint8());
                }

                // 4. Unpack Surviving Leaders
                auto& state = game::current_state;
                while (true) {
                    s_leader_data leader{};
                    if (!SERIAL::Read_Leader_Update(reader, leader)) exitm::Exit_With_Message("invalid combat leader record");
                    if (leader.id == game::NO_LEADER) break;
                    state.leaders.at(leader.id.id) = leader;
                }
                RUSS::Mox_Update();

                // 5. Unpack Colony Fate
                if (combat_planet_idx != game::NO_PLANET) {
                    auto& state = game::current_state;
                    auto& planet = state.planets.at(combat_planet_idx);
                    const auto old_colony = planet.colony_id;
                    const auto next_colony_id = reader.Read_Uint32();
                    const game::id_colony new_colony{reader.Read_Uint32()};
                    int8_t p_type = static_cast<int8_t>(reader.Read_Uint8());
                    char p_climate = static_cast<char>(reader.Read_Uint8());
                    s_colony colony{};
                    if (new_colony != game::NO_COLONY) {
                        if (new_colony.id >= next_colony_id) return;
                        const auto existing = state.colonies.find(new_colony);
                        if (existing != state.colonies.end() && existing->second.planet_id != combat_planet_idx) return;
                        if (existing == state.colonies.end() && old_colony == game::NO_COLONY && state.colonies.size() >= MAX_COLONIES) return;
                        SERIAL::Read_Colony_With_Id(reader, colony, new_colony);
                        if (colony.planet_id != combat_planet_idx) return;
                    }

                    state.next_colony_id = std::max(state.next_colony_id, next_colony_id);
                    planet.planet_type = p_type;
                    planet.climate = p_climate;

                    if (new_colony == game::NO_COLONY) {
                        if (old_colony != game::NO_COLONY) {
                            BOMB::Destroy_Colony(state, old_colony, 1);
                        }
                    } else {
                        if (old_colony != new_colony) state.colonies.erase(old_colony);
                        state.colonies.insert_or_assign(colony.id, colony);
                        for (auto& queue : state.colony_reservations) std::erase(queue, new_colony);
                        planet.colony_id = new_colony;
                        COLCALC::Compute_Star_Colony_Stuff(&game::current_state.stars.at(game::Star_Key_At_Row(game::current_state, combat_star_id)));
                    }
                }
                RUSS::Mox_Update();

                // 6. Unpack Ships and Process Casualties
                MOX::NUM_SHIPS = reader.Read_Int16();

                int16_t current_ship_idx = 0;

                while (true) {
                    int16_t surviving_ship_id = reader.Read_Int16();

                    if (surviving_ship_id == -1) {
                        RUSS::Mox_Update();
                        break;
                    }

                    // Kill any ships that were skipped in the ID sequence (meaning they died)
                    while (current_ship_idx < surviving_ship_id) {
                        s_ship_data* ship = &MOX::ship[current_ship_idx];
                        if ((ship->owner == attacker_id || ship->owner == defender_id) &&
                            ship->status != 5 && game::Ship_Orbit_Row(game::current_state, ship->position) == combat_star_id) {
                            ship->officer_index = -1;
                            SHIPS::Kill_Ship(current_ship_idx);
                        }
                        current_ship_idx++;
                    }

                    // Update the surviving ship
                    if (surviving_ship_id < 0 || surviving_ship_id >= MAX_SHIPS ||
                        !SERIAL::Read_Ship_Packet(reader, MOX::ship[surviving_ship_id], game::current_state))
                        exitm::Exit_With_Message("invalid combat ship record");

                    current_ship_idx++; // Advance past the surviving ship we just updated
                    RUSS::Mox_Update();
                }

                // Sweep any remaining ships at the end of the array that died
                while (current_ship_idx < MOX::NUM_SHIPS) {
                    s_ship_data* ship = &MOX::ship[current_ship_idx];
                    if ((ship->owner == attacker_id || ship->owner == defender_id) &&
                        ship->status != 5 && game::Ship_Orbit_Row(game::current_state, ship->position) == combat_star_id) {
                        ship->officer_index = -1;
                        SHIPS::Kill_Ship(current_ship_idx);
                    }
                    current_ship_idx++;
                }
            }
        }

        MAINSCR::Update_Ship_Icons_In_Between_Turns();
        NETMOX::net.player_net_info[reporting_player_id].player_state = NET_PLAYER_STATE_HUMAN_DIPLOMACY_ACTIVE;
    }

    void __cdecl Decode_Dragon_Info(const void* payload, uint32_t len) {
        if (payload == nullptr || len != sizeof(uint32_t)) return;
        SERIAL::Reader_ reader(payload, len, "invalid dragon target");
        const auto& state = game::current_state;
        const auto colony = state.colonies.find(game::id_colony{reader.Read_Uint32()});
        if (colony == state.colonies.end()) return;
        const auto planet_idx = colony->second.planet_id;

        net.combat_planet_id = planet_idx;
        net.combat_destination = {game::e_star_destination::star, game::current_state.planets.at(planet_idx).star_id};
    }

    void __cdecl Send_Dragon_Info(game::id_colony colony_id) {
        std::array<uint8_t, sizeof(uint32_t)> packet{};
        SERIAL::Writer_ writer(packet.data(), packet.size(), "invalid dragon target");
        writer.Write_Uint32(colony_id.id);

        int8_t owner = game::current_state.colonies.at(colony_id).owner;
        int16_t network_player_id = MOX::player[owner].network_player_id;

        Mox_Send_Message(network_player_id, NET_PACKET_DRAGON_INFO, packet.data(), writer.Length());
    }

    void __cdecl Decode_Drop_Out_Info(int32_t player_idx, s_net_pack_drop_out_info* drop_out_info) {
        Apply_Drop_Out_Info(player_idx, drop_out_info, false);
    }

    void __cdecl Decode_Player_Disconnected_Info(void* data, uint32_t len) {
        if (data == nullptr || len != sizeof(s_net_pack_player_disconnected_info_)) {
            return;
        }

        s_net_pack_player_disconnected_info_* packet = (s_net_pack_player_disconnected_info_*)data;
        Apply_Drop_Out_Info(packet->player_idx, &packet->drop_out_info, true);
    }

    void __cdecl Send_Battle_Results(int16_t star_id, int16_t player_id, int16_t target_player_id, int16_t combat_defender_id, game::id_planet combat_planet_id, int16_t combat_result) {
        int16_t player_num = MOX::PLAYER_NUM;
        bool include_remote_state =
            net.local_net_id != net.net_host_id &&
            (player_num != target_player_id || player_id >= MAX_PLAYERS || MOX::player[player_id].objectives != PLAYER_OBJECTIVE_HUMAN);

        allocate::Push_Block(MOX::screen_seg);
        uint32_t alloc_size = include_remote_state ? NETMOX_MAX_GAME_DATA_PAYLOAD_BYTES : sizeof(int16_t) * 4;
        uint8_t* packet = (uint8_t*)allocate::Allocate_Next_Block(MOX::screen_seg, alloc_size);
        NETPROTO::Packet_Writer_ writer(packet, alloc_size, "invalid combat payload");

        writer.Write_Int16(player_num);
        writer.Write_Int16(player_id);
        writer.Write_Int16(target_player_id);
        writer.Write_Int16(combat_result);

        if (include_remote_state) {
            Write_Combat_Destination(writer, star_id);

            writer.Require(k_star_player_presence_payload_bytes);
            COMBFIND::Send_Star_Player_Presence(&writer, -1);

            writer.Write_Int16(combat_defender_id);
            writer.Write_Uint32(combat_planet_id.id);

            RUSS::Mox_Update();

            if (player_id < MAX_PLAYERS) {
                writer.Write_Bytes(MOX::player[player_id].tech_applications, sizeof(MOX::player[player_id].tech_applications));
                writer.Write_Uint8((uint8_t)game::Destination_Record_At_Row(game::current_state, star_id).officer_index[player_id]);
            }

            if (target_player_id < MAX_PLAYERS) {
                writer.Write_Bytes(MOX::player[target_player_id].tech_applications, sizeof(MOX::player[target_player_id].tech_applications));
                writer.Write_Uint8((uint8_t)game::Destination_Record_At_Row(game::current_state, star_id).officer_index[target_player_id]);
            }

            RUSS::Mox_Update();

            const auto& state = game::current_state;
            for (int16_t i = 0; i < MAX_LEADERS; i++) {
                const auto& leader = state.leaders.at(i);
                if (leader.player_index == player_id || leader.player_index == target_player_id) {
                    SERIAL::Write_Leader_Update(writer, leader);
                }
            }

            writer.Write_Int16(-1);

            RUSS::Mox_Update();

            if (combat_planet_id != game::NO_PLANET) {
                const auto& planet = game::current_state.planets.at(combat_planet_id);
                writer.Write_Uint32(game::current_state.next_colony_id);
                writer.Write_Uint32(planet.colony_id.id);
                writer.Write_Int8(planet.planet_type);
                writer.Write_Char(planet.climate);

                if (planet.colony_id != game::NO_COLONY) {
                    SERIAL::Write_Colony_Fields(writer, game::current_state.colonies.at(planet.colony_id));
                }
            }

            RUSS::Mox_Update();

            writer.Write_Int16((int16_t)MOX::NUM_SHIPS);

            if (MOX::NUM_SHIPS > 0) {
                for (int16_t i = 0; i < (int16_t)MOX::NUM_SHIPS; i++) {
                    if (game::Ship_Orbit_Row(game::current_state, MOX::ship[i].position) == star_id && MOX::ship[i].status != 5) {
                        if (MOX::ship[i].owner == player_id || MOX::ship[i].owner == target_player_id) {
                            writer.Write_Int16(i);
                            SERIAL::Write_Ship_Packet(writer, MOX::ship[i]);
                        }
                    }
                }
            }

            writer.Write_Int16(-1);

            RUSS::Mox_Update();
        }

        int32_t size = (int32_t)writer.Length();
        NETMOX::Mox_Send_Message(net.local_net_id, NET_PACKET_BATTLE_RESULTS, packet, (uint32_t)size);

        allocate::Pop_Block(MOX::screen_seg);
    }

    void __cdecl Send_You_Are_Target_Info(
        int16_t target_player_idx,
        int16_t other_player_idx,
        int16_t defender_id,
        game::id_colony colony_id,
        int16_t star_idx) {
        allocate::Push_Block(MOX::screen_seg);

        uint32_t alloc_size =
            (target_player_idx == MOX::PLAYER_NUM) ? sizeof(s_packet_0x12) : NETMOX_MAX_GAME_DATA_PAYLOAD_BYTES;
        uint8_t* packet = (uint8_t*)allocate::Allocate_Next_Block(MOX::screen_seg, alloc_size);
        NETPROTO::Packet_Writer_ writer(packet, alloc_size, "invalid combat payload");

        Write_Combat_Destination(writer, star_idx);
        writer.Write_Int16(other_player_idx);
        writer.Write_Int16(defender_id);
        writer.Write_Uint32(colony_id.id);

        if (MOX::player[target_player_idx].network_player_id != net.local_net_id) {
            writer.Require(k_star_player_presence_payload_bytes);
            COMBFIND::Send_Star_Player_Presence(&writer, star_idx);
            RUSS::Mox_Update();

            if (other_player_idx < MAX_PLAYERS) {
                writer.Write_Bytes(MOX::player[other_player_idx].tech_fields, sizeof(MOX::player[other_player_idx].tech_fields));
                writer.Write_Bytes(MOX::player[other_player_idx].tech_applications, sizeof(MOX::player[other_player_idx].tech_applications));
            }

            if (target_player_idx < MAX_PLAYERS) {
                writer.Write_Bytes(MOX::player[target_player_idx].tech_fields, sizeof(MOX::player[target_player_idx].tech_fields));
                writer.Write_Bytes(MOX::player[target_player_idx].tech_applications, sizeof(MOX::player[target_player_idx].tech_applications));
            }

            RUSS::Mox_Update();

            const auto& state = game::current_state;
            for (int16_t i = 0; i < MAX_LEADERS; ++i) {
                const auto& leader = state.leaders.at(i);
                if (leader.player_index == other_player_idx || leader.player_index == target_player_idx) {
                    SERIAL::Write_Leader_Update(writer, leader);
                }
            }

            writer.Write_Int16(-1);

            if (other_player_idx < MAX_PLAYERS) {
                writer.Write_Uint8((uint8_t)game::Destination_Record_At_Row(state, star_idx).officer_index[other_player_idx]);
            }

            if (target_player_idx < MAX_PLAYERS) {
                writer.Write_Uint8((uint8_t)game::Destination_Record_At_Row(state, star_idx).officer_index[target_player_idx]);
            }

            RUSS::Mox_Update();

            if (colony_id != game::NO_COLONY) {
                const auto& colony = game::current_state.colonies.at(colony_id);
                const auto planet_idx = colony.planet_id;

                writer.Write_Uint32(game::current_state.next_colony_id);
                writer.Write_Uint32(planet_idx.id);
                writer.Write_Int8(game::current_state.planets.at(planet_idx).planet_type);
                writer.Write_Char(game::current_state.planets.at(planet_idx).climate);
                SERIAL::Write_Colony_Fields(writer, colony);
            }

            RUSS::Mox_Update();

            writer.Write_Int16((int16_t)MOX::NUM_SHIPS);

            if (MOX::NUM_SHIPS > 0) {
                for (int16_t i = 0; i < (int16_t)MOX::NUM_SHIPS; ++i) {
                    if (game::Ship_Orbit_Row(game::current_state, MOX::ship[i].position) == star_idx && MOX::ship[i].status != 5) {
                        if (MOX::ship[i].owner == other_player_idx || MOX::ship[i].owner == target_player_idx) {
                            writer.Write_Int16(i);
                            SERIAL::Write_Ship_Packet(writer, MOX::ship[i]);
                        }
                    }
                }
            }

            writer.Write_Int16(-1);

            RUSS::Mox_Update();
        }

        int32_t msg_len = (int32_t)writer.Length();
        NETMOX::Mox_Send_Message(MOX::player[target_player_idx].network_player_id, NET_PACKET_YOU_ARE_TARGET_INFO, packet, (uint32_t)msg_len);

        allocate::Pop_Block(MOX::screen_seg);
    }

    void __cdecl Send_Pick_Target_Info(int16_t player_idx, int16_t star_idx, s_combfind_target* target_info) {
        allocate::Push_Block(MOX::screen_seg);

        uint32_t alloc_size =
            (player_idx == MOX::PLAYER_NUM) ? (COMBAT_DESTINATION_BYTES + TARGET_LIST_BYTES) : NETMOX_MAX_GAME_DATA_PAYLOAD_BYTES;
        uint8_t* packet = static_cast<uint8_t*>(allocate::Allocate_Next_Block(MOX::screen_seg, alloc_size));
        NETPROTO::Packet_Writer_ writer(packet, alloc_size, "invalid combat payload");

        Write_Combat_Destination(writer, star_idx);
        for (const auto& colony : target_info->colony_targets) writer.Write_Uint32(colony.get().id);
        writer.Write_Int16(target_info->num_colony_targets);
        for (int i = 0; i < 15; ++i) writer.Write_Int16(target_info->player_targets[i]);
        writer.Write_Int16(target_info->num_player_targets);

        if (MOX::player[player_idx].network_player_id != net.local_net_id) {
            writer.Require(k_star_player_presence_payload_bytes);
            COMBFIND::Send_Star_Player_Presence(&writer, star_idx);

            if (MOX::NUM_PLAYERS > 0) {
                for (int16_t i = 0; i < MOX::NUM_PLAYERS; i++) {
                    writer.Write_Bytes(MOX::player[i].tech_fields, sizeof(MOX::player[i].tech_fields));
                    writer.Write_Bytes(MOX::player[i].tech_applications, sizeof(MOX::player[i].tech_applications));
                }
            }

            RUSS::Mox_Update();

            const auto& state = game::current_state;
            for (int16_t i = 0; i < MAX_LEADERS; i++) {
                SERIAL::Write_Leader_Update(writer, state.leaders.at(i));
                RUSS::Mox_Update();
            }

            writer.Write_Int16(-1);

            if (MOX::NUM_PLAYERS > 0) {
                for (int16_t i = 0; i < MOX::NUM_PLAYERS; i++) {
                    writer.Write_Uint8((uint8_t)game::Destination_Record_At_Row(state, star_idx).officer_index[i]);
                }
            }

            RUSS::Mox_Update();

            writer.Write_Uint32(game::current_state.next_colony_id);

            for (int i = 0; i < 5; i++) {
                const auto planet_idx = game::Destination_Record_At_Row(state, star_idx).orbit_planet_ids[i];
                if (planet_idx != game::NO_PLANET) {
                    const auto& planet = game::current_state.planets.at(planet_idx);
                    writer.Write_Uint32(planet.colony_id.id);
                    writer.Write_Int8(planet.planet_type);
                    writer.Write_Char(planet.climate);

                    if (planet.colony_id != game::NO_COLONY) {
                        SERIAL::Write_Colony_Fields(writer, game::current_state.colonies.at(planet.colony_id));
                    }
                }
            }

            RUSS::Mox_Update();

            writer.Write_Int16((int16_t)MOX::NUM_SHIPS);

            if (MOX::NUM_SHIPS > 0) {
                for (int16_t i = 0; i < (int16_t)MOX::NUM_SHIPS; i++) {
                    if (game::Ship_Orbit_Row(game::current_state, MOX::ship[i].position) == star_idx && MOX::ship[i].status != 5) {
                        writer.Write_Int16(i);
                        SERIAL::Write_Ship_Packet(writer, MOX::ship[i]);
                        RUSS::Mox_Update();
                    }
                }
            }

            writer.Write_Int16(-1);
        }

        RUSS::Mox_Update();

        int32_t diff = static_cast<int32_t>(writer.Length());
        NETMOX::Mox_Send_Message(MOX::player[player_idx].network_player_id, NET_PACKET_PICK_TARGET_INFO, packet, (uint32_t)diff);

        allocate::Pop_Block(MOX::screen_seg);
    }
} // namespace NETMOX
