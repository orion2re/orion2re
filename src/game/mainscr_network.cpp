#include "pch.h"
#include "player_roster.h"
#include "battle_results_serialization.h"
#include "star_surrender_serialization.h"
#include "game_wire_version.h"
#include "player_network_serialization.h"

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
        for (int16_t i = 0; i < std::ssize(game::current_state.star_order); i++) {
            const auto star_id = game::current_state.star_order[i];
            const auto player = HAROLD::Star_Owner(i, MOX::local_player);
            game::current_state.stars.at(star_id).owner = factions::From_Player(player);
        }
    }

    void __cdecl Update_Ship_Icons_In_Between_Turns() {
        Update_Star_Owners();
        SHIPS::Build_Ship_Icons();
        SHIPS::Set_Ship_Icon_XYs();
    }

    void __cdecl Main_Receive_Message(game::id_player sender_key, int16_t sender_net_id_or_host_id, uint16_t msg_type, uint32_t payload_len, void* payload) {
        const auto from_player_id = sender_key.id;
        // Setup and transport messages can arrive before the roster maps the peer.
        // Only routes that address the sender's roster slot require membership.
        switch (msg_type) {
            case NET_PACKET_PLAYER_SETUP:
            case NET_PACKET_HOST_ELECTION_RANK:
            case NET_PACKET_AI_DIPLOMACY_REQUEST:
            case NET_PACKET_AI_DIPLOMACY_CANCEL_REQUEST:
            case NET_PACKET_AI_DIPLOMACY_END_STATE:
            case NET_PACKET_TURN_END_REQUEST:
            case NET_PACKET_COMBAT_TARGET_CHOSEN:
            case NET_PACKET_COMBAT_RESULTS_ACK:
            case NET_PACKET_REQUEST_HUMAN_DIPLOMACY:
            case NET_PACKET_ACCEPT_HUMAN_DIPLOMACY:
            case NET_PACKET_CANCEL_HUMAN_DIPLOMACY:
            case NET_PACKET_DRAGON_NO_BRIBE:
            case NET_PACKET_DRAGON_BRIBE_DESTINATION:
            case NET_PACKET_CHAT:
            case NET_PACKET_REQUEST_PLAYER_COLOR:
            case NET_PACKET_PLAYER_PHASE_READY:
            case NET_PACKET_REQUEST_PLAYER_RACE:
            case NET_PACKET_PLAYER_RACE_DESELECT_REQUEST:
            case NET_PACKET_HUMAN_DIPLOMACY_READY_DATA:
            case NET_PACKET_DECLARE_WAR:
                if (!player_roster::Contains(game::current_state, sender_key)) return;
                break;
        }
        switch (msg_type) {
            case NET_PACKET_START_GAME: {
                if (payload == nullptr || payload_len < sizeof(s_net_pack_start_game)) {
                    exitm::Exit_With_Message("network start packet truncated");
                }
                s_net_pack_start_game pack;
                memcpy(&pack, payload, sizeof(pack));
                if (!NETPROTO::Valid_Setup_Version(pack.version_string)) {
                    exitm::Exit_With_Message("Incompatible multiplayer protocol version");
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

                game::current_state.player_count = pack.number_of_players;
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
                MOX::local_player = game::NO_PLAYER;

                if (game::current_state.player_count > 0) {
                    for (int i = 0; i < game::current_state.player_count; ++i) {
                        game::current_state.players[i].network_player_id = pack.net_ids[i];
                        netcode::Net_Accept_Member(pack.net_ids[i]);
                        if (NETMOX::net.net_host_id == pack.net_ids[i]) {
                            MOX::local_player = legacy_world::Decode_Player_Slot(i);
                        }
                    }
                }

                if (MOX::Legacy_Local_Player_Slot() != -1) {
                    NETMOX::net.sync_state = NET_SYNC_STATE_CLIENT_SELECT_RACE;
                } else {
                    NETMOX::net.sync_state = NET_SYNC_STATE_CLIENT_NO_AVAILABLE_SLOT;
                }
                break;
            }
            case NET_PACKET_PLAYER_SETUP: {
                if (NETMOX::net.sync_state != NET_SYNC_STATE_HOST_WAITING_FOR_PLAYER_SETUP ||
                    from_player_id >= MAX_PLAYERS || std::cmp_greater_equal(from_player_id, game::current_state.player_count)) break;
                if (!NETPROTO::Decode_Player(payload, payload_len, game::current_state.players[from_player_id])) {
                    exitm::Exit_With_Message("Invalid multiplayer player setup packet. Make sure all players use compatible game versions.");
                }
                NETMOX::net.player_connected_flags[from_player_id] = 1;

                if (game::current_state.player_count > 0) {
                    for (int i = 0; i < game::current_state.player_count; ++i) {
                        if (from_player_id != i && game::current_state.players[i].color == game::current_state.players[from_player_id].color) {
                            game::current_state.players[from_player_id].color = 8;
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
                if (NETMOX::net.sync_state != NET_SYNC_STATE_HOST_RECEIVING_PLAYER_DATA ||
                    !player_roster::Contains(game::current_state, sender_key) ||
                    sender_key != NETMOX::net.host_active_player_id.get()) break;
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
                NETMOX::net.sync_state = NET_SYNC_STATE_HOST_WAITING_FOR_ORDERS;
                break;
            }
            case NET_PACKET_HOST_ELECTION_RANK: {
                int8_t rank = *(int8_t*)payload;
                int8_t hit_next = *(int8_t*)((uint8_t*)payload + 4);
                NETMOX::net.host_election_player_rank[from_player_id] = rank;
                NETMOX::net.players_hit_next_turn[from_player_id] = hit_next;

                int8_t cur_rank = NETMOX::net.host_election_player_rank[MOX::Legacy_Local_Player_Slot()];
                if (cur_rank == -1 || rank < cur_rank) {
                    NETMOX::net.host_election_player_rank[MOX::Legacy_Local_Player_Slot()] = rank;
                }
                break;
            }
            case NET_PACKET_AI_DIPLOMACY_REQUEST: {
                if (!payload || payload_len != sizeof(int8_t)) break;
                int8_t remote_id = *(int8_t*)payload;
                const auto target = legacy_world::Decode_Player_Slot(remote_id);
                if (!player_roster::Contains(game::current_state, target)) break;
                NETMOX::net.player_net_info[from_player_id].diplomacy_target_id = target;
                NETMOX::net.player_net_info[from_player_id].player_state = NET_PLAYER_STATE_AI_DIPLOMACY_REQUESTED;

                const auto next_talker = NETMOX::Get_Next_AI_Talker(target);
                if (next_talker == game::NO_PLAYER) {
                    NETMOX::net.ai_diplomacy_done_mask.set_slot(from_player_id & 0x1F);
                } else {
                    NETMOX::net.ai_diplomacy_active_mask.set_slot(from_player_id & 0x1F);
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
                    NETMOX::net.ai_diplomacy_active_mask.reset_slot(from_player_id & 0x1F);
                    const auto next_talker = NETMOX::Get_Next_AI_Talker(NETMOX::net.player_net_info[from_player_id].diplomacy_target_id.get());
                    if (next_talker != game::NO_PLAYER) {
                        NETMOX::net.ai_diplomacy_active_mask.set(next_talker);
                        NETMOX::net.player_net_info[next_talker.id].player_state = NET_PLAYER_STATE_AI_DIPLOMACY_ACTIVE;
                    }
                }
                NETMOX::net.player_net_info[from_player_id].diplomacy_target_id = game::NO_PLAYER;
                NETMOX::net.player_net_info[from_player_id].player_state = NET_PLAYER_STATE_READY;
                NETMOX::net.ai_diplomacy_cancel_mask.set_slot(from_player_id & 0x1F);
                break;
            }
            case NET_PACKET_AI_DIPLOMACY_CANCELLED: {
                NETMOX::net.diplomacy_target_id = game::NO_PLAYER;
                NETMOX::net.sync_state = NET_SYNC_STATE_READY;
                break;
            }
            case NET_PACKET_AI_DIPLOMACY_END_STATE: {
                if (payload_len != sizeof(s_net_pack_end_ai_diplomacy)) break;
                s_net_pack_end_ai_diplomacy* pack = (s_net_pack_end_ai_diplomacy*)payload;
                const auto remote = NETMOX::net.player_net_info[from_player_id].diplomacy_target_id.get();
                const auto remote_id = legacy_world::Encode_Player_Slot(remote);

                s_player target, sender;
                NETPROTO::star_surrender_updates surrender;
                if (remote_id < 0 || remote_id >= game::current_state.player_count ||
                    !NETPROTO::Decode_Player(&pack->target_player, sizeof(pack->target_player), target) ||
                    !NETPROTO::Decode_Player(&pack->player, sizeof(pack->player), sender) ||
                    !NETPROTO::Read_Star_Surrender(pack->surrender_to, game::current_state, surrender)) break;
                player_roster::Install(game::current_state, remote_id, target);
                player_roster::Install(game::current_state, from_player_id, sender);

                NETPROTO::Apply_Star_Surrender(game::current_state, remote, surrender);

                NETMOX::net.player_net_info[from_player_id].diplomacy_target_id = game::NO_PLAYER;
                NETMOX::net.player_net_info[from_player_id].player_state = NET_PLAYER_STATE_READY;
                NETMOX::net.players_sync_mask[remote_id] = game::player_bitmap::All();
                NETMOX::net.diplomacy_sync_target[remote_id] = sender_key;

                const auto next_talker = NETMOX::Get_Next_AI_Talker(remote);
                if (next_talker != game::NO_PLAYER) {
                    NETMOX::net.ai_diplomacy_active_mask.set(next_talker);
                    NETMOX::net.player_net_info[next_talker.id].player_state = NET_PLAYER_STATE_AI_DIPLOMACY_ACTIVE;
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
                    (int)MOX::Legacy_Local_Player_Slot(),
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
                if (payload_len != 8 || from_player_id < 0 || from_player_id >= MAX_PLAYERS) break;
                SERIAL::Reader_ reader(payload, payload_len, "combat target underflow", true);
                auto& info = NETMOX::net.player_net_info[from_player_id];
                info.combat_defender_id = game::owner_slot{reader.Read_Int16()};
                info.combat_planet_id = game::id_planet{reader.Read_Uint32()};
                info.combat_target_player_id = game::owner_slot{reader.Read_Int16()};
                info.player_state = NET_PLAYER_STATE_COMBAT_TARGET_CHOSEN;
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
                } else if (factions::Legacy_Code(NETMOX::net.combat_target_player_id.get()) < MAX_PLAYERS && game::current_state.players[factions::Legacy_Code(NETMOX::net.combat_target_player_id.get())].objectives == PLAYER_OBJECTIVE_HUMAN) {
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
                if (payload_len != SERIAL::BATTLE_RESULTS_BYTES || !NETMOX::net.combat_results_data) break;
                SERIAL::Reader_ reader(payload, payload_len, "battle results underflow", true);
                SERIAL::Read_Battle_Results(reader, *NETMOX::net.combat_results_data);
                if (msg_type == NET_PACKET_STRATEGIC_BATTLE_RESULTS) {
                    NETMOX::net.sync_state = NET_SYNC_STATE_COMBAT_RESULTS_DISPLAY;
                } else {
                    NETMOX::net.sync_state = NET_SYNC_STATE_STRATEGIC_RESULTS_RECEIVED;
                }
                break;
            }
            case NET_PACKET_COMBAT_RESULTS_ACK: {
                NETMOX::net.player_net_info[from_player_id].player_state = NET_PLAYER_STATE_COMBAT_READY;
                NETMOX::net.player_net_info[from_player_id].combat_attacker_id = game::NO_OWNER;
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
                    (int)MOX::Legacy_Local_Player_Slot());
                NETMOX::net.sync_state = NET_SYNC_STATE_WAITING_FOR_GAME_DATA;
                break;
            }
            case NET_PACKET_REQUEST_HUMAN_DIPLOMACY: {
                if (payload_len != sizeof(s_net_pack_req_human_diplomacy)) break;
                s_net_pack_req_human_diplomacy* pack = (s_net_pack_req_human_diplomacy*)payload;
                if (NETMOX::net.players_hit_next_turn[MOX::Legacy_Local_Player_Slot()] == 0) {
                    NETPROTO::star_surrender_updates surrender;
                    if (!NETPROTO::Read_Star_Surrender(pack->surrender_to, game::current_state, surrender) ||
                        !NETPROTO::Decode_Player(&pack->player, sizeof(pack->player), game::current_state.players[from_player_id])) break;
                    NETPROTO::Apply_Star_Surrender(game::current_state, sender_key, surrender);

                    if (NETMOX::net.sync_state == NET_SYNC_STATE_HUMAN_DIPLOMACY_REQUESTED && NETMOX::net.diplomacy_target_id.get() == sender_key) {
                        const auto mask = ~game::player_bitmap::Only_Slot(from_player_id & 0x1F);
                        NETMOX::net.sync_state = NET_SYNC_STATE_HUMAN_DIPLOMACY_CONNECTING;
                        NETMOX::net.diplomacy_initiator_id = sender_key;
                        NETMOX::net.players_active_mask &= mask;
                        NETMOX::net.human_diplomacy_request_mask &= mask;
                        NETMOX::net.is_diplomacy_initiator = MOX::local_player.id < sender_key.id;
                    } else if (game::current_state.players.at(MOX::local_player.id).ignoring.test_slot(from_player_id)) {
                        NETMOX::net.human_diplomacy_ignored_mask.set_slot(from_player_id & 0x1F);
                    } else {
                        const auto mask = game::player_bitmap::Only_Slot(from_player_id & 0x1F);
                        NETMOX::net.players_active_mask |= mask;
                        NETMOX::net.human_diplomacy_request_mask |= mask;
                    }
                }
                break;
            }
            case NET_PACKET_ACCEPT_HUMAN_DIPLOMACY: {
                if (!NETPROTO::Decode_Player(payload, payload_len, game::current_state.players[from_player_id])) break;

                if (NETMOX::net.sync_state == NET_SYNC_STATE_HUMAN_DIPLOMACY_REQUESTED && NETMOX::net.diplomacy_target_id.get() == sender_key) {
                    const auto mask = ~game::player_bitmap::Only_Slot(from_player_id & 0x1F);
                    NETMOX::net.sync_state = NET_SYNC_STATE_HUMAN_DIPLOMACY_CONNECTING;
                    NETMOX::net.diplomacy_initiator_id = sender_key;
                    NETMOX::net.players_active_mask &= mask;
                    NETMOX::net.human_diplomacy_request_mask &= mask;
                    NETMOX::net.is_diplomacy_initiator = 1;
                } else if (game::current_state.players.at(MOX::local_player.id).ignoring.test_slot(from_player_id)) {
                    NETMOX::net.human_diplomacy_ignored_mask.set_slot(from_player_id & 0x1F);
                } else {
                    const auto mask = game::player_bitmap::Only_Slot(from_player_id & 0x1F);
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
                const auto mask = ~game::player_bitmap::Only_Slot(from_player_id & 0x1F);
                NETMOX::net.players_active_mask &= mask;
                NETMOX::net.human_diplomacy_request_mask &= mask;

                if (NETMOX::net.sync_state == NET_SYNC_STATE_HUMAN_DIPLOMACY_READY && NETMOX::net.diplomacy_target_id.get() == sender_key) {
                    NETMOX::net.sync_state = NET_SYNC_STATE_READY;
                    NETMOX::net.diplomacy_target_id = game::NO_PLAYER;
                    NETMOX::net.diplomacy_active_flag = 0;
                }
                if (NETMOX::net.sync_state == NET_SYNC_STATE_HUMAN_DIPLOMACY_CONNECTING && NETMOX::net.diplomacy_target_id.get() == sender_key) {
                    if (NETMOX::net.diplomacy_active_flag == 0) {
                        NETMOX::net.sync_state = NET_SYNC_STATE_HUMAN_DIPLOMACY_REQUESTED;
                    } else {
                        NETMOX::net.sync_state = NET_SYNC_STATE_READY;
                    }
                    NETMOX::net.diplomacy_active_flag = 0;
                    NETMOX::net.diplomacy_initiator_id = game::NO_PLAYER;
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
                game::current_state.players[target_id].network_player_id = sender_net_id_or_host_id;
                NETMOX::net.joining_net_id = -1;
                break;
            }
            case NET_PACKET_SYNC_PLAYER_STATE: {
                if (payload_len != sizeof(s_net_pack_end_ai_diplomacy)) break;
                s_net_pack_end_ai_diplomacy* pack = (s_net_pack_end_ai_diplomacy*)payload;
                int target_id = pack->target_player_id;

                s_player target, sender;
                NETPROTO::star_surrender_updates surrender;
                if (target_id < 0 || target_id >= game::current_state.player_count ||
                    pack->player_num < 0 || pack->player_num >= game::current_state.player_count ||
                    !NETPROTO::Decode_Player(&pack->target_player, sizeof(pack->target_player), target) ||
                    !NETPROTO::Decode_Player(&pack->player, sizeof(pack->player), sender) ||
                    !NETPROTO::Read_Star_Surrender(pack->surrender_to, game::current_state, surrender)) break;
                player_roster::Install(game::current_state, target_id, target);
                player_roster::Install(game::current_state, pack->player_num, sender);

                NETPROTO::Apply_Star_Surrender(game::current_state, game::current_state.players[target_id].id, surrender);
                break;
            }
            case NET_PACKET_DRAGON_INFO: {
                NETMOX::net.sync_state = NET_SYNC_STATE_DRAGON_BRIBE_PROMPT;
                NETMOX::Decode_Dragon_Info(payload, payload_len);
                break;
            }
            case NET_PACKET_DRAGON_NO_BRIBE: {
                NETMOX::net.player_net_info[from_player_id].player_state = NET_PLAYER_STATE_HUMAN_DIPLOMACY_IGNORED;
                break;
            }
            case NET_PACKET_DRAGON_BRIBE_DESTINATION: {
                if (payload_len != sizeof(uint32_t) || from_player_id >= MAX_PLAYERS) {
                    break;
                }
                SERIAL::Reader_ reader(payload, payload_len, "invalid Dragon bribe destination");
                const game::id_star destination{reader.Read_Uint32()};
                if (!game::current_state.stars.contains(destination)) break;
                NETMOX::net.dragon_bribe_destination_star_id = destination;
                NETMOX::net.player_net_info[from_player_id].player_state = NET_PLAYER_STATE_DRAGON_BRIBE_DESTINATION;
                break;
            }
            case NET_PACKET_HUMAN_DIPLOMAT_WAIT: {
                if (NETMOX::net.sync_state == NET_SYNC_STATE_HUMAN_DIPLOMACY_REQUESTED && NETMOX::net.diplomacy_target_id.get() == sender_key) {
                    NETMOX::net.human_diplomat_wait_flag = 1;
                }
                break;
            }
            case NET_PACKET_CHAT: {
                if (CHAT::chat_info != nullptr) {
                    CHAT::Receive_Chat_Msg(sender_key, (char*)payload);
                }
                break;
            }
            case NET_PACKET_LOAD_GAME_START: {
                if (payload == nullptr || payload_len < sizeof(s_load_net_screen_packet)) {
                    exitm::Exit_With_Message("network load packet truncated");
                }
                s_load_net_screen_packet pack;
                memcpy(&pack, payload, sizeof(pack));
                if (!NETPROTO::Valid_Setup_Version(pack.version_string)) {
                    exitm::Exit_With_Message("Incompatible multiplayer protocol version");
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
                    game::current_state.players[cur_players].network_player_id =
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
                game::current_state.players[player_idx].color = color_idx;
                break;
            }
            case NET_PACKET_REQUEST_PLAYER_COLOR: {
                uint8_t color_idx = *(uint8_t*)payload;
                if (RACESEL::color_flag[color_idx] == 0) {
                    RACESEL::color_flag[color_idx] = 1;
                    game::current_state.players[from_player_id].color = color_idx;
                    NETMOX::net.player_color_selected_flags[from_player_id] = 1;
                }
                break;
            }
            case NET_PACKET_GNN_CHAT: {
                if (CHAT::chat_info != nullptr) {
                    CHAT::Receive_Chat_Msg(game::NO_PLAYER, (char*)payload);
                }
                break;
            }
            case NET_PACKET_PLAYER_PHASE_READY: {
                NETMOX::net.player_race_selected_flags[from_player_id] = 1;
                break;
            }
            case NET_PACKET_PLAYER_ABDICATED:
            case NET_PACKET_PLAYER_SURRENDERED: {
                if (payload_len != sizeof(s_net_pack_drop_out_info)) break;
                NETMOX::Decode_Drop_Out_Info(sender_key, (s_net_pack_drop_out_info*)payload);
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
                game::current_state.players[player_idx].race = race_idx;
                break;
            }
            case NET_PACKET_REQUEST_PLAYER_RACE: {
                uint8_t race_idx = *(uint8_t*)payload;
                if (RACESEL::race_flag[race_idx] == 0) {
                    RACESEL::race_flag[race_idx] = 1;
                    game::current_state.players[from_player_id].race = race_idx;
                    NETMOX::net.player_race_selected_alt_flags[from_player_id] = 1;
                }
                break;
            }
            case NET_PACKET_FORCE_PLAYER_ELIMINATED: {
                game::current_state.players.at(MOX::local_player.id).eliminated = 1;
                break;
            }
            case NET_PACKET_MONSTER_UPDATE: {
                if (payload_len != 12) {
                    break;
                }
                SERIAL::Reader_ reader(static_cast<const uint8_t*>(payload), payload_len, "monster update underflow", true);
                const auto monster = reader.Read_Int32();
                const game::id_planet target{reader.Read_Uint32()};
                const auto amount = reader.Read_Int32();
                if (std::in_range<int16_t>(monster) && std::in_range<int16_t>(amount)) {
                    EVENTS::Event_Monster_Update(factions::From_Legacy(monster), target, factions::From_Legacy(amount));
                }
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
                if (payload_len != NETMOX::CHANGE_RELATIONS_BYTES) break;
                SERIAL::Reader_ reader(payload, payload_len, "change relations underflow");
                const auto change = reader.Read_Int16();
                const auto attacker = reader.Read_Int16();
                const auto defender = reader.Read_Int16();
                const auto reason = reader.Read_Int16();
                const auto destination = SERIAL::Read_Star_Destination(reader);
                const auto incident = reader.Read_Int16();
                if (destination.kind == game::e_star_destination::star && game::Destination_Row(game::current_state, destination) < 0) break;
                diplomac::Change_Relations(change, legacy_world::Decode_Player_Slot(attacker), legacy_world::Decode_Player_Slot(defender), reason, game::Destination_Row(game::current_state, destination), incident);
                break;
            }
            case NET_PACKET_HUMAN_DIPLOMACY_READY_DATA: {
                if (!NETPROTO::Decode_Player(payload, payload_len, game::current_state.players[from_player_id])) break;
                if (NETMOX::net.sync_state == NET_SYNC_STATE_HUMAN_DIPLOMACY_CONNECTING && NETMOX::net.diplomacy_target_id.get() == sender_key) {
                    const auto mask = ~game::player_bitmap::Only_Slot(from_player_id & 0x1F);
                    NETMOX::net.sync_state = NET_SYNC_STATE_HUMAN_DIPLOMACY_READY;
                    NETMOX::net.players_active_mask &= mask;
                    NETMOX::net.human_diplomacy_request_mask &= mask;
                }
                break;
            }
            case NET_PACKET_DECLARE_WAR: {
                if (!payload || payload_len != sizeof(int32_t)) break;
                int32_t target_player_id;
                memcpy(&target_player_id, payload, sizeof(target_player_id));
                diplomac::Declare_War(sender_key, legacy_world::Decode_Player_Slot(target_player_id));
                break;
            }
        }
    }
} // namespace MAINSCR
