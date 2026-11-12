#include "pch.h"

#include "config/config_snapshot.h"
#include "game/serialization.h"
#include "lua/lua_mod_runtime.h"
#include "lua/lua_network.h"

namespace MAINSCR {
    constexpr uint32_t k_max_game_data_payload_bytes = NETMOX::NETMOX_MAX_GAME_DATA_PAYLOAD_BYTES;

    namespace {
        bool Commit_Pending_Lua_Setup(
            int16_t sender_net_id,
            orion2re::lua::e_lua_network_setup_kind expected_kind,
            const void* payload,
            uint32_t payload_len,
            uint32_t prefix_size) {
            if (sender_net_id != static_cast<int16_t>(netcode::Net_Session_Owner_ID()) ||
                !orion2re::lua::Has_Pending_Lua_Network_Setup()) {
                orion2re::lua::Discard_Pending_Lua_Network_Setup();
                return false;
            }
            std::string diagnostic;
            if (!orion2re::lua::Commit_Lua_Network_Setup(expected_kind,
                    payload,
                    payload_len,
                    prefix_size,
                    &diagnostic)) {
                exitm::Exit_With_Message(diagnostic.c_str());
            }
            return true;
        }

        bool Valid_Setup_Version(const char* version, size_t capacity) {
            const void* terminator = memchr(version, '\0', capacity);
            return terminator != nullptr && strcmp(version, GAME_BUILD_DATE) == 0;
        }

        void Apply_Setup_Config(const void* payload, uint32_t payload_len, uint32_t prefix_size) {
            if (payload == nullptr || payload_len <= prefix_size) {
                exitm::Exit_With_Message("network configuration packet missing");
            }
            std::string lua_diagnostic;
            if (!orion2re::lua::Check_Network_Mod_Runtime_Compatibility(
                    orion2re::lua::Get_Active_Mod_Runtimes(),
                    &lua_diagnostic)) {
                exitm::Exit_With_Message(lua_diagnostic.c_str());
            }
            const uint8_t* snapshot = static_cast<const uint8_t*>(payload) + prefix_size;
            const uint32_t snapshot_size = payload_len - prefix_size;
            SERIAL::Reader_ reader(
                snapshot, snapshot_size, "network configuration packet truncated");
            if (!config::Apply_Config_Snapshot(
                    &reader, &config::synchronized_config_snapshot) ||
                reader.Length() != snapshot_size) {
                exitm::Exit_With_Message("incompatible network configuration");
            }
        }
    } // namespace

    void __cdecl Update_Star_Owners() {
        for (int16_t i = 0; i < MOX::NUM_STARS; i++) {
            int16_t owner_id = HAROLD::Star_Owner(i, MOX::PLAYER_NUM);
            MOX::star[i].owner = (int8_t)owner_id;
        }
    }

    void __cdecl Update_Ship_Icons_In_Between_Turns() {
        Update_Star_Owners();
        SHIPS::Build_Ship_Icons();
        SHIPS::Set_Ship_Icon_XYs();
    }

    void __cdecl Main_Receive_Message(uint32_t from_player_id, int16_t sender_net_id_or_host_id, uint16_t msg_type, uint32_t payload_len, void* payload) {
        switch (msg_type) {
            case NET_PACKET_START_GAME: {
                if (payload == nullptr || payload_len < sizeof(s_net_pack_start_game)) {
                    exitm::Exit_With_Message("network start packet truncated");
                }
                s_net_pack_start_game pack;
                memcpy(&pack, payload, sizeof(pack));
                if (!Valid_Setup_Version(pack.version_string,
                        sizeof(pack.version_string))) {
                    exitm::Exit_With_Message("Incompatible network version");
                }
                if (orion2re::lua::Has_Pending_Lua_Network_Setup()) {
                    if (!Commit_Pending_Lua_Setup(sender_net_id_or_host_id,
                            orion2re::lua::LUA_NETWORK_SETUP_NEW_GAME,
                            payload,
                            payload_len,
                            sizeof(pack))) {
                        exitm::Exit_With_Message(
                            "invalid Lua network new-game commit");
                    }
                } else {
                    if (orion2re::lua::Network_Lua_Enabled() ||
                        orion2re::lua::Has_Lua_Network_Setup_Commit(payload,
                            payload_len,
                            sizeof(pack))) {
                        exitm::Exit_With_Message(
                            "Lua network setup offer is missing");
                    }
                    Apply_Setup_Config(payload, payload_len, sizeof(pack));
                }

                MOX::NUM_PLAYERS = pack.number_of_players;
                MOX::settings.random_events = pack.random_events;
                MOX::settings.game_difficulty = pack.game_difficulty;
                MOX::settings.number_of_players = pack.number_of_players;
                MOX::settings.galaxy_size = pack.galaxy_size;
                MOX::settings.galaxy_age = pack.galaxy_age;
                MOX::settings.strategic_combat_flag = pack.strategic_combat_flag;
                MOX::settings.starting_civilization_level =
                    pack.starting_civilization_level;
                MOX::settings.xenons_exist = pack.xenons_exist;

                NETMOX::net.host_election_id = (int16_t)pack.net_host_id;
                NETMOX::net.num_connected_players = (int8_t)pack.num_connected_players;
                MOX::PLAYER_NUM = -1;

                if (MOX::NUM_PLAYERS > 0) {
                    for (int i = 0; i < MOX::NUM_PLAYERS; ++i) {
                        MOX::player[i].network_player_id = pack.net_ids[i];
                        netcode::Net_Accept_Member(pack.net_ids[i]);
                        if (NETMOX::net.net_host_id == pack.net_ids[i]) {
                            MOX::PLAYER_NUM = i;
                        }
                    }
                }

                if (MOX::PLAYER_NUM != -1) {
                    NETMOX::net.sync_state = NET_SYNC_STATE_CLIENT_SELECT_RACE;
                } else {
                    NETMOX::net.sync_state = NET_SYNC_STATE_CLIENT_NO_AVAILABLE_SLOT;
                }
                break;
            }
            case NET_PACKET_PLAYER_SETUP: {
                NETMOX::net.player_connected_flags[from_player_id] = 1;
                memcpy(&MOX::player[from_player_id], payload, (uint16_t)payload_len);

                if (MOX::NUM_PLAYERS > 0) {
                    for (int i = 0; i < MOX::NUM_PLAYERS; ++i) {
                        if (from_player_id != i && MOX::player[i].color == MOX::player[from_player_id].color) {
                            MOX::player[from_player_id].color = 8;
                        }
                    }
                }
                break;
            }
            case NET_PACKET_GAME_DATA_PAYLOAD: {
                if (sender_net_id_or_host_id !=
                    NETMOX::net.host_election_id) {
                    break;
                }
                NETMOX::Net_Debug_Log(
                    "Main_Receive_Message_: GAME_DATA_PAYLOAD from_player=%u sender_net_id=%d bytes=%u sync=%u",
                    (unsigned int)from_player_id,
                    (int)sender_net_id_or_host_id,
                    (unsigned int)payload_len,
                    (unsigned int)NETMOX::net.sync_state);
                allocate::Push_Block(MOX::screen_seg);
                if (payload_len > k_max_game_data_payload_bytes) {
                    exitm::Exit_With_Message("game data payload too large");
                }
                NETMOX::net.network_data_ptr =
                    allocate::Allocate_Next_Block(MOX::screen_seg,
                        k_max_game_data_payload_bytes);
                memcpy(NETMOX::net.network_data_ptr, payload, payload_len);
                NETMOX::net.host_election_winner_id = sender_net_id_or_host_id;
                NETTURN::Net_Init_Host_Election();
                NETMOX::net.total_bytes_received = payload_len;
                break;
            }
            case NET_PACKET_GAME_ORDERS_PAYLOAD: {
                NETMOX::Net_Debug_Log(
                    "Main_Receive_Message_: GAME_ORDERS_PAYLOAD from_player=%u bytes=%u host_sync_counter_before=%d",
                    (unsigned int)from_player_id,
                    (unsigned int)payload_len,
                    (int)NETMOX::net.host_sync_counter);
                if (payload == nullptr || payload_len == 0 || payload_len > k_max_game_data_payload_bytes) {
                    exitm::Exit_With_Message("game orders payload invalid");
                }

                allocate::Push_Block(MOX::screen_seg);
                NETMOX::net.network_data_ptr = allocate::Allocate_Next_Block(MOX::screen_seg, payload_len);
                memcpy(NETMOX::net.network_data_ptr, payload, payload_len);
                NETMOX::net.total_bytes_received = payload_len;
                NETMOX::net.host_sync_counter++;
                if ((uint8_t)NETMOX::net.host_sync_counter == MOX::NUM_PLAYERS) {
                    NETMOX::net.sync_state = NET_SYNC_STATE_HOST_BEGIN_TURN;
                } else {
                    NETMOX::net.sync_state = NET_SYNC_STATE_HOST_WAITING_FOR_ORDERS;
                }
                NETMOX::Net_Debug_Log(
                    "Main_Receive_Message_: GAME_ORDERS_PAYLOAD complete host_sync_counter_after=%d next_sync=%u",
                    (int)NETMOX::net.host_sync_counter,
                    (unsigned int)NETMOX::net.sync_state);
                break;
            }
            case NET_PACKET_HOST_ELECTION_RANK: {
                int8_t rank = *(int8_t*)payload;
                int8_t hit_next = *(int8_t*)((uint8_t*)payload + 4);
                NETMOX::net.host_election_player_rank[from_player_id] = rank;
                NETMOX::net.players_hit_next_turn[from_player_id] = hit_next;

                int8_t cur_rank = NETMOX::net.host_election_player_rank[MOX::PLAYER_NUM];
                if (cur_rank == -1 || rank < cur_rank) {
                    NETMOX::net.host_election_player_rank[MOX::PLAYER_NUM] = rank;
                }
                break;
            }
            case NET_PACKET_AI_DIPLOMACY_REQUEST: {
                int8_t remote_id = *(int8_t*)payload;
                NETMOX::net.player_net_info[from_player_id].diplomacy_target_id = remote_id;
                NETMOX::net.player_net_info[from_player_id].player_state = NET_PLAYER_STATE_AI_DIPLOMACY_REQUESTED;

                int next_talker = NETMOX::Get_Next_AI_Talker(remote_id);
                if (next_talker == -1) {
                    NETMOX::net.ai_diplomacy_done_mask |= (1 << (from_player_id & 0x1F));
                } else {
                    NETMOX::net.ai_diplomacy_active_mask |= (1 << (from_player_id & 0x1F));
                    NETMOX::net.player_net_info[from_player_id].diplomacy_target_id = remote_id;
                    NETMOX::net.player_net_info[from_player_id].player_state = NET_PLAYER_STATE_AI_DIPLOMACY_ACTIVE;
                }
                break;
            }
            case NET_PACKET_AI_DIPLOMACY_BEGIN: {
                if (NETMOX::net.sync_state == NET_SYNC_STATE_AI_DIPLOMACY_REQUESTED || NETMOX::net.sync_state == NET_SYNC_STATE_AI_DIPLOMACY_DONE) {
                    NETMOX::net.sync_state = NET_SYNC_STATE_AI_DIPLOMACY_READY;
                }
                break;
            }
            case NET_PACKET_AI_DIPLOMACY_DONE: {
                if (NETMOX::net.sync_state == NET_SYNC_STATE_AI_DIPLOMACY_REQUESTED) {
                    NETMOX::net.sync_state = NET_SYNC_STATE_AI_DIPLOMACY_DONE;
                }
                break;
            }
            case NET_PACKET_AI_DIPLOMACY_CANCEL_REQUEST: {
                if (NETMOX::net.player_net_info[from_player_id].player_state != NET_PLAYER_STATE_AI_DIPLOMACY_REQUESTED) {
                    NETMOX::net.ai_diplomacy_active_mask &= ~(1 << (from_player_id & 0x1F));
                    int next_talker = NETMOX::Get_Next_AI_Talker(NETMOX::net.player_net_info[from_player_id].diplomacy_target_id);
                    if (next_talker != -1) {
                        NETMOX::net.ai_diplomacy_active_mask |= (1 << (next_talker & 0x1F));
                        NETMOX::net.player_net_info[next_talker].player_state = NET_PLAYER_STATE_AI_DIPLOMACY_ACTIVE;
                    }
                }
                NETMOX::net.player_net_info[from_player_id].diplomacy_target_id = -1;
                NETMOX::net.player_net_info[from_player_id].player_state = NET_PLAYER_STATE_READY;
                NETMOX::net.ai_diplomacy_cancel_mask |= (1 << (from_player_id & 0x1F));
                break;
            }
            case NET_PACKET_AI_DIPLOMACY_CANCELLED: {
                NETMOX::net.diplomacy_target_id = -1;
                NETMOX::net.sync_state = NET_SYNC_STATE_READY;
                break;
            }
            case NET_PACKET_AI_DIPLOMACY_END_STATE: {
                s_net_pack_end_ai_diplomacy* pack = (s_net_pack_end_ai_diplomacy*)payload;
                int8_t remote_id = NETMOX::net.player_net_info[from_player_id].diplomacy_target_id;

                memcpy(&MOX::player[remote_id], &pack->target_player, sizeof(s_player));
                memcpy(&MOX::player[from_player_id], &pack->player, sizeof(s_player));

                for (int16_t i = 0; i < MOX::NUM_STARS; ++i) {
                    MOX::star[i].surrender_to[remote_id] = pack->surrender_to[i];
                }

                NETMOX::net.player_net_info[from_player_id].diplomacy_target_id = -1;
                NETMOX::net.player_net_info[from_player_id].player_state = NET_PLAYER_STATE_READY;
                NETMOX::net.players_sync_mask[remote_id] = 0xFF;
                NETMOX::net.diplomacy_sync_mask[remote_id] = from_player_id;

                int next_talker = NETMOX::Get_Next_AI_Talker(remote_id);
                if (next_talker != -1) {
                    NETMOX::net.ai_diplomacy_active_mask |= (1 << (next_talker & 0x1F));
                    NETMOX::net.player_net_info[next_talker].player_state = NET_PLAYER_STATE_AI_DIPLOMACY_ACTIVE;
                }
                break;
            }
            case NET_PACKET_TURN_END_REQUEST: {
                NETMOX::Net_Debug_Log(
                    "Main_Receive_Message_: TURN_END_REQUEST from_player=%u",
                    (unsigned int)from_player_id);
                NETMOX::net.player_is_active_flags[from_player_id] = 1;
                break;
            }
            case NET_PACKET_SEND_GAME_ORDERS_REQUEST: {
                NETMOX::Net_Debug_Log(
                    "Main_Receive_Message_: SEND_GAME_ORDERS_REQUEST local_player=%d sync_before=%u",
                    (int)MOX::PLAYER_NUM,
                    (unsigned int)NETMOX::net.sync_state);
                NETMOX::net.sync_state = NET_SYNC_STATE_RECEIVING_GAME_ORDERS;
                break;
            }
            case NET_PACKET_COMBAT_SYNC_RESET: {
                memset(
                    &NETMOX::net.player_net_info,
                    0,
                    offsetof(s_network, sync_state) - offsetof(s_network, player_net_info));
                NETMOX::net.sync_state = NET_SYNC_STATE_COMBAT_READY;
                break;
            }
            case NET_PACKET_PICK_TARGET_INFO: {
                NETMOX::net.sync_state = NET_SYNC_STATE_PICKING_COMBAT_TARGET;
                NETMOX::Decode_Pick_Target_Info(payload, payload_len);
                break;
            }
            case NET_PACKET_COMBAT_TARGET_CHOSEN: {
                const s_net_pack_combat_target_chosen* packet = (const s_net_pack_combat_target_chosen*)payload;
                NETMOX::net.player_net_info[from_player_id].combat_defender_id = packet->combat_defender_id;
                NETMOX::net.player_net_info[from_player_id].combat_planet_id = packet->combat_planet_id;
                NETMOX::net.player_net_info[from_player_id].combat_target_player_id = packet->combat_target_player_id;
                NETMOX::net.player_net_info[from_player_id].player_state = NET_PLAYER_STATE_COMBAT_TARGET_CHOSEN;
                break;
            }
            case NET_PACKET_YOU_ARE_TARGET_INFO: {
                NETMOX::Decode_You_Are_Target_Info(payload, payload_len);
                break;
            }
            case NET_PACKET_COMBAT_EXECUTE: {
                NETMOX::net.sync_state = NET_SYNC_STATE_COMBAT_TACTICAL_ATTACKER;
                break;
            }
            case NET_PACKET_COMBAT_RESOLUTION_BEGIN: {
                if (MOX::settings.strategic_combat_flag != 0) {
                    NETMOX::net.sync_state = NET_SYNC_STATE_COMBAT_STRATEGIC_ATTACKER;
                } else if (NETMOX::net.combat_target_player_id < MAX_PLAYERS && MOX::player[NETMOX::net.combat_target_player_id].objectives == PLAYER_OBJECTIVE_HUMAN) {
                    NETMOX::net.sync_state = NET_SYNC_STATE_COMBAT_TACTICAL_ATTACKER;
                } else {
                    NETMOX::net.sync_state = NET_SYNC_STATE_COMBAT_TACTICAL_ATTACKER_VS_NONHUMAN;
                }
                break;
            }
            case NET_PACKET_BATTLE_RESULTS: {
                NETMOX::Decode_Battle_Results(payload, payload_len);
                break;
            }
            case NET_PACKET_STRATEGIC_BATTLE_RESULTS:
            case NET_PACKET_STRATEGIC_RESULTS_READY: {
                memcpy(NETMOX::net.combat_results_data, payload, sizeof(s_battle_results));
                if (msg_type == NET_PACKET_STRATEGIC_BATTLE_RESULTS) {
                    NETMOX::net.sync_state = NET_SYNC_STATE_COMBAT_RESULTS_DISPLAY;
                } else {
                    NETMOX::net.sync_state = NET_SYNC_STATE_STRATEGIC_RESULTS_RECEIVED;
                }
                break;
            }
            case NET_PACKET_COMBAT_RESULTS_ACK: {
                NETMOX::net.player_net_info[from_player_id].player_state = NET_PLAYER_STATE_COMBAT_READY;
                NETMOX::net.player_net_info[from_player_id].combat_attacker_id = -1;
                NETMOX::net.sync_phase_active_flag = 0;
                break;
            }
            case NET_PACKET_STRATEGIC_RESULTS_ACK: {
                NETMOX::net.sync_state = NET_SYNC_STATE_COMBAT_READY;
                break;
            }
            case NET_PACKET_COMBAT_PHASE_COMPLETE: {
                NETMOX::Net_Debug_Log(
                    "Main_Receive_Message_: COMBAT_PHASE_COMPLETE local_player=%d",
                    (int)MOX::PLAYER_NUM);
                NETMOX::net.sync_state = NET_SYNC_STATE_WAITING_FOR_GAME_DATA;
                break;
            }
            case NET_PACKET_REQUEST_HUMAN_DIPLOMACY: {
                s_net_pack_req_human_diplomacy* pack = (s_net_pack_req_human_diplomacy*)payload;
                if (NETMOX::net.players_hit_next_turn[MOX::PLAYER_NUM] == 0) {
                    memcpy(&MOX::player[from_player_id], &pack->player, sizeof(s_player));

                    for (int16_t i = 0; i < MOX::NUM_STARS; ++i) {
                        MOX::star[i].surrender_to[from_player_id] = pack->surrender_to[i];
                    }

                    if (NETMOX::net.sync_state == NET_SYNC_STATE_HUMAN_DIPLOMACY_REQUESTED && NETMOX::net.diplomacy_target_id == from_player_id) {
                        uint8_t mask = ~(1 << (from_player_id & 0x1F));
                        NETMOX::net.sync_state = NET_SYNC_STATE_HUMAN_DIPLOMACY_CONNECTING;
                        NETMOX::net.diplomacy_initiator_id = from_player_id;
                        NETMOX::net.players_active_mask &= mask;
                        NETMOX::net.human_diplomacy_request_mask &= mask;
                        NETMOX::net.is_diplomacy_initiator = (MOX::PLAYER_NUM < NETMOX::net.diplomacy_target_id);
                    } else if ((MOX::player[MOX::PLAYER_NUM].ignoring & (1 << (from_player_id & 0x1F))) != 0) {
                        NETMOX::net.human_diplomacy_ignored_mask |= (1 << (from_player_id & 0x1F));
                    } else {
                        uint8_t mask = (1 << (from_player_id & 0x1F));
                        NETMOX::net.players_active_mask |= mask;
                        NETMOX::net.human_diplomacy_request_mask |= mask;
                    }
                }
                break;
            }
            case NET_PACKET_ACCEPT_HUMAN_DIPLOMACY: {
                memcpy(&MOX::player[from_player_id], payload, sizeof(s_player));

                if (NETMOX::net.sync_state == NET_SYNC_STATE_HUMAN_DIPLOMACY_REQUESTED && NETMOX::net.diplomacy_target_id == from_player_id) {
                    uint8_t mask = ~(1 << (from_player_id & 0x1F));
                    NETMOX::net.sync_state = NET_SYNC_STATE_HUMAN_DIPLOMACY_CONNECTING;
                    NETMOX::net.diplomacy_initiator_id = from_player_id;
                    NETMOX::net.players_active_mask &= mask;
                    NETMOX::net.human_diplomacy_request_mask &= mask;
                    NETMOX::net.is_diplomacy_initiator = 1;
                } else if ((MOX::player[MOX::PLAYER_NUM].ignoring & (1 << (from_player_id & 0x1F))) != 0) {
                    NETMOX::net.human_diplomacy_ignored_mask |= (1 << (from_player_id & 0x1F));
                } else {
                    uint8_t mask = (1 << (from_player_id & 0x1F));
                    NETMOX::net.players_active_mask |= mask;
                    NETMOX::net.human_diplomacy_request_mask |= mask;
                }
                break;
            }
            case NET_PACKET_HUMAN_DIPLOMACY_IGNORED: {
                NETMOX::net.sync_state = NET_SYNC_STATE_HUMAN_DIPLOMACY_REJECTED;
                break;
            }
            case NET_PACKET_CANCEL_HUMAN_DIPLOMACY: {
                uint8_t mask = ~(1 << (from_player_id & 0x1F));
                NETMOX::net.players_active_mask &= mask;
                NETMOX::net.human_diplomacy_request_mask &= mask;

                if (NETMOX::net.sync_state == NET_SYNC_STATE_HUMAN_DIPLOMACY_READY && NETMOX::net.diplomacy_target_id == from_player_id) {
                    NETMOX::net.sync_state = NET_SYNC_STATE_READY;
                    NETMOX::net.diplomacy_target_id = -1;
                    NETMOX::net.diplomacy_active_flag = 0;
                }
                if (NETMOX::net.sync_state == NET_SYNC_STATE_HUMAN_DIPLOMACY_CONNECTING && NETMOX::net.diplomacy_target_id == from_player_id) {
                    if (NETMOX::net.diplomacy_active_flag == 0) {
                        NETMOX::net.sync_state = NET_SYNC_STATE_HUMAN_DIPLOMACY_REQUESTED;
                    } else {
                        NETMOX::net.sync_state = NET_SYNC_STATE_READY;
                    }
                    NETMOX::net.diplomacy_active_flag = 0;
                    NETMOX::net.diplomacy_initiator_id = -1;
                }
                break;
            }
            case NET_PACKET_PICK_YOUR_POSITION: {
                NETMOX::net.sync_state = NET_SYNC_STATE_CLIENT_PICK_POSITION;
                NETMOX::net.host_election_id = sender_net_id_or_host_id;
                NETMOX::Decode_Pick_Your_Position((s_net_pack_pick_your_position*)payload, payload_len);
                break;
            }
            case NET_PACKET_PICK_YOUR_POSITION_RESULT: {
                int32_t target_id;
                memcpy(&target_id, payload, sizeof(target_id));
                MOX::player[target_id].network_player_id = sender_net_id_or_host_id;
                NETMOX::net.joining_net_id = -1;
                break;
            }
            case NET_PACKET_SYNC_PLAYER_STATE: {
                s_net_pack_end_ai_diplomacy* pack = (s_net_pack_end_ai_diplomacy*)payload;
                int target_id = pack->target_player_id;

                memcpy(&MOX::player[target_id], &pack->target_player, sizeof(s_player));
                memcpy(&MOX::player[pack->player_num], &pack->player, sizeof(s_player));

                for (int16_t i = 0; i < MOX::NUM_STARS; ++i) {
                    pack->surrender_to[i] = MOX::star[i].surrender_to[target_id];
                }
                break;
            }
            case NET_PACKET_DRAGON_INFO: {
                NETMOX::net.sync_state = NET_SYNC_STATE_DRAGON_BRIBE_PROMPT;
                NETMOX::Decode_Dragon_Info((int16_t*)payload, (uint16_t)payload_len);
                break;
            }
            case NET_PACKET_DRAGON_NO_BRIBE: {
                NETMOX::net.player_net_info[from_player_id].player_state = NET_PLAYER_STATE_HUMAN_DIPLOMACY_IGNORED;
                break;
            }
            case NET_PACKET_DRAGON_BRIBE_DESTINATION: {
                if (payload_len != sizeof(int16_t)) {
                    break;
                }
                memcpy(&NETMOX::net.dragon_bribe_destination_star_id, payload, sizeof(int16_t));
                NETMOX::net.player_net_info[from_player_id].player_state = NET_PLAYER_STATE_DRAGON_BRIBE_DESTINATION;
                break;
            }
            case NET_PACKET_HUMAN_DIPLOMAT_WAIT: {
                if (NETMOX::net.sync_state == NET_SYNC_STATE_HUMAN_DIPLOMACY_REQUESTED && NETMOX::net.diplomacy_target_id == from_player_id) {
                    NETMOX::net.human_diplomat_wait_flag = 1;
                }
                break;
            }
            case NET_PACKET_CHAT: {
                if (CHAT::chat_info != nullptr) {
                    CHAT::Receive_Chat_Msg(from_player_id, (char*)payload);
                }
                break;
            }
            case NET_PACKET_LOAD_GAME_START: {
                if (payload == nullptr || payload_len < sizeof(s_load_net_screen_packet)) {
                    exitm::Exit_With_Message("network load packet truncated");
                }
                s_load_net_screen_packet pack;
                memcpy(&pack, payload, sizeof(pack));
                if (!Valid_Setup_Version(pack.version_string,
                        sizeof(pack.version_string))) {
                    exitm::Exit_With_Message("Incompatible network version");
                }
                if (orion2re::lua::Has_Pending_Lua_Network_Setup()) {
                    if (!Commit_Pending_Lua_Setup(sender_net_id_or_host_id,
                            orion2re::lua::LUA_NETWORK_SETUP_LOAD_GAME,
                            payload,
                            payload_len,
                            sizeof(pack))) {
                        exitm::Exit_With_Message(
                            "invalid Lua network load-game commit");
                    }
                } else {
                    if (orion2re::lua::Network_Lua_Enabled() ||
                        orion2re::lua::Has_Lua_Network_Setup_Commit(payload,
                            payload_len,
                            sizeof(pack))) {
                        exitm::Exit_With_Message(
                            "Lua network setup offer is missing");
                    }
                    Apply_Setup_Config(payload, payload_len, sizeof(pack));
                }

                int cur_players = pack.number_of_players;
                while (cur_players > 0) {
                    cur_players--;
                    MOX::player[cur_players].network_player_id =
                        pack.network_ids[cur_players];
                    netcode::Net_Accept_Member(
                        pack.network_ids[cur_players]);
                }
                break;
            }
            case NET_PACKET_PLAYER_COLOR_SELECTED: {
                uint8_t color_idx = *(uint8_t*)payload;
                uint8_t player_idx = *(uint8_t*)((uint8_t*)payload + 1);
                RACESEL::color_flag[color_idx] = 1;
                MOX::player[player_idx].color = color_idx;
                break;
            }
            case NET_PACKET_REQUEST_PLAYER_COLOR: {
                uint8_t color_idx = *(uint8_t*)payload;
                if (RACESEL::color_flag[color_idx] == 0) {
                    RACESEL::color_flag[color_idx] = 1;
                    MOX::player[from_player_id].color = color_idx;
                    NETMOX::net.player_color_selected_flags[from_player_id] = 1;
                }
                break;
            }
            case NET_PACKET_GNN_CHAT: {
                if (CHAT::chat_info != nullptr) {
                    CHAT::Receive_Chat_Msg(8, (char*)payload);
                }
                break;
            }
            case NET_PACKET_PLAYER_PHASE_READY: {
                NETMOX::net.player_race_selected_flags[from_player_id] = 1;
                break;
            }
            case NET_PACKET_PLAYER_ABDICATED:
            case NET_PACKET_PLAYER_SURRENDERED: {
                NETMOX::Decode_Drop_Out_Info(from_player_id, (s_net_pack_drop_out_info*)payload);
                break;
            }
            case NET_PACKET_PLAYER_DISCONNECTED: {
                NETMOX::Decode_Player_Disconnected_Info(payload, payload_len);
                break;
            }
            case NET_PACKET_PLAYER_RACE_SELECTED: {
                if (NETMOX::net.host_election_id == NETMOX::net.net_host_id) {
                    exitm::Exit_With_Message(NETMOX::only_1_host_allowed_error);
                }
                uint8_t race_idx = *(uint8_t*)payload;
                uint8_t player_idx = *(uint8_t*)((uint8_t*)payload + 1);
                RACESEL::race_flag[race_idx] = 1;
                MOX::player[player_idx].race = race_idx;
                break;
            }
            case NET_PACKET_REQUEST_PLAYER_RACE: {
                uint8_t race_idx = *(uint8_t*)payload;
                if (RACESEL::race_flag[race_idx] == 0) {
                    RACESEL::race_flag[race_idx] = 1;
                    MOX::player[from_player_id].race = race_idx;
                    NETMOX::net.player_race_selected_alt_flags[from_player_id] = 1;
                }
                break;
            }
            case NET_PACKET_FORCE_PLAYER_ELIMINATED: {
                MOX::player[MOX::PLAYER_NUM].eliminated = 1;
                break;
            }
            case NET_PACKET_MONSTER_UPDATE: {
                if (payload_len != sizeof(s_net_pack_monster_update)) {
                    break;
                }
                s_net_pack_monster_update monster_update;
                memcpy(&monster_update, payload, sizeof(monster_update));
                EVENTS::Event_Monster_Update((int16_t)monster_update.monster_type, monster_update.target_planet_id, (int16_t)monster_update.player_idx_or_amount);
                break;
            }
            case NET_PACKET_PLAYER_RACE_DESELECTED: {
                uint8_t race_idx = *(uint8_t*)payload;
                RACESEL::race_flag[race_idx] = 0;
                break;
            }
            case NET_PACKET_PLAYER_RACE_DESELECT_REQUEST: {
                uint8_t race_idx = *(uint8_t*)payload;
                RACESEL::race_flag[race_idx] = 0;
                NETMOX::net.player_race_deselected_flags[from_player_id] = 1;
                break;
            }
            case NET_PACKET_RACE_SELECTION_SYNCED: {
                NETMOX::net.race_selection_changed = 0;
                break;
            }
            case NET_PACKET_TACTICAL_COMBAT_START: {
                NETMOX::net.tactical_combat_sync_state = NET_TACTICAL_COMBAT_SYNC_STARTED;
                break;
            }
            case NET_PACKET_TACTICAL_COMBAT_END: {
                NETMOX::net.tactical_combat_sync_state = NET_TACTICAL_COMBAT_SYNC_FINISHED;
                break;
            }
            case NET_PACKET_NETWORK_SYNC_STATUS: {
                if (*(int32_t*)payload == 0) {
                    NETMOX::net.network_sync_complete_flag = 0;
                }
                break;
            }
            case NET_PACKET_CHANGE_RELATIONS: {
                int16_t relation_params[6];
                memcpy(relation_params, payload, sizeof(relation_params));
                diplomac::Change_Relations(relation_params[0], relation_params[1], relation_params[2], relation_params[3], relation_params[4], relation_params[5]);
                break;
            }
            case NET_PACKET_HUMAN_DIPLOMACY_READY_DATA: {
                memcpy(&MOX::player[from_player_id], payload, sizeof(s_player));
                if (NETMOX::net.sync_state == NET_SYNC_STATE_HUMAN_DIPLOMACY_CONNECTING && NETMOX::net.diplomacy_target_id == from_player_id) {
                    uint8_t mask = ~(1 << (from_player_id & 0x1F));
                    NETMOX::net.sync_state = NET_SYNC_STATE_HUMAN_DIPLOMACY_READY;
                    NETMOX::net.players_active_mask &= mask;
                    NETMOX::net.human_diplomacy_request_mask &= mask;
                }
                break;
            }
            case NET_PACKET_DECLARE_WAR: {
                int32_t target_player_id;
                memcpy(&target_player_id, payload, sizeof(target_player_id));
                diplomac::Declare_War((int16_t)from_player_id, (int16_t)target_player_id);
                break;
            }
        }
    }
} // namespace MAINSCR
