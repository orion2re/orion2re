#include "pch.h"

#include "doctest/doctest.h"
#include "game/game_events.h"

#include <array>
#include <cstring>
#include <vector>

namespace netcode {
    extern void (*g_net_update_hook)();
    extern void (*g_net_send_hook)(int32_t, int16_t, void*, uint32_t);
}

namespace TEST_LUA_NETWORK {
    void Set_Network_Lua_Enabled_(bool enabled);
    void Set_Queued_Load_State_(bool queued);
    int Prepared_Payload_Count_();
}

namespace {
    int g_net_update_hook_invocations = 0;
    std::vector<uint8_t> g_game_orders_payload;
    std::vector<uint8_t> g_pick_position_payload;

    void Snapshot_Encode_Mutation_Hook_() {
        ++g_net_update_hook_invocations;
        MOX::player[1].bc += 999;
        netcode::g_net_update_hook = nullptr;
    }

    void Capture_Game_Orders_(int32_t, int16_t packet_id, void* packet, uint32_t packet_len) {
        if (packet_id == NET_PACKET_GAME_ORDERS_PAYLOAD) {
            uint8_t* bytes = static_cast<uint8_t*>(packet);
            g_game_orders_payload.assign(bytes, bytes + packet_len);
        }
    }

    void Capture_Pick_Position_(int32_t, int16_t packet_id, void* packet, uint32_t packet_len) {
        if (packet_id == NET_PACKET_PICK_YOUR_POSITION) {
            uint8_t* bytes = (uint8_t*)packet;
            g_pick_position_payload.assign(bytes, bytes + packet_len);
        }
    }

    constexpr uint32_t Broadcast_Game_Data_Snapshot_Max_Bytes_() {
        return
            sizeof(NETMOX::net.host_election_id) +
            sizeof(MOX::stardate) +
            sizeof(MOX::settings.random_events) +
            sizeof(MOX::settings.game_difficulty) +
            sizeof(MOX::settings.number_of_players) +
            sizeof(MOX::settings.galaxy_size) +
            sizeof(MOX::settings.galaxy_age) +
            sizeof(MOX::settings.strategic_combat_flag) +
            sizeof(MOX::settings.starting_civilization_level) +
            sizeof(MOX::settings.xenons_exist) +
            sizeof(uint32_t) +
            sizeof(uint16_t) +
            MAX_PLAYERS * sizeof(s_player) +
            sizeof(uint16_t) +
            (MAX_STARS + 1) * sizeof(s_star_data) +
            sizeof(uint16_t) +
            MAX_PLANETS * sizeof(s_planet_data) +
            sizeof(uint16_t) +
            MAX_COLONIES * sizeof(s_colony) +
            sizeof(int16_t) +
            MAX_SHIPS * sizeof(s_ship_data) +
            MAX_LEADERS * sizeof(s_leader_data) +
            sizeof(uint16_t) +
            sizeof(REPORT::reports) +
            sizeof(EVENTS::event_data) +
            sizeof(MSG_::first_msg) +
            sizeof(MSG_::max_msgs) +
            sizeof(MSG_::n_msgs) +
            sizeof(MSG_::msgs) +
            sizeof(diplodef::COUNCIL_FLAG) +
            sizeof(diplodef::next_council_meeting_turn) +
            sizeof(MOX::antarans) +
            sizeof(MOX::NUM_NEBULAS) +
            MAX_NEBULAS * sizeof(s_nebula) +
            sizeof(EVENTS::last_event_year) +
            sizeof(EVENTS::event_delay_count) +
            sizeof(MOX::bill_savegame);
    }

    struct Scoped_Netmox_Roundtrip_State_ {
        s_network saved_net = NETMOX::net;
        uint32_t saved_stardate = MOX::stardate;
        s_settings saved_settings = MOX::settings;
        int8_t saved_game_type = MOX::game_type;
        int16_t saved_player_num = MOX::PLAYER_NUM;
        int16_t saved_num_players = MOX::NUM_PLAYERS;
        int16_t saved_num_stars = MOX::NUM_STARS;
        int16_t saved_planet_count = MOX::planet_count;
        int16_t saved_num_colonies = MOX::NUM_COLONIES;
        int16_t saved_num_ships = MOX::NUM_SHIPS;
        uint8_t saved_num_nebulas = MOX::NUM_NEBULAS;
        uint16_t saved_max_pop_freighted = SETTLER::max_pop_freighted;
        s_antaran saved_antarans = MOX::antarans;
        uint32_t saved_random_seed = game_random::Get_Random_Seed();
        int16_t saved_report_count = REPORT::n_reports;
        std::array<int16_t, 6> saved_bill_savegame = {};
        std::array<s_report, 50> saved_reports = {};
        std::array<s_event_data, 36> saved_event_data = {};
        std::array<uint8_t, MAX_PLAYERS> saved_first_msg = {};
        std::array<uint8_t, MAX_PLAYERS> saved_max_msgs = {};
        std::array<uint8_t, MAX_PLAYERS> saved_n_msgs = {};
        std::array<s_MSG, 100> saved_msgs = {};
        s_player* saved_player_ptr = MOX::player;
        s_star_data* saved_star_ptr = MOX::star;
        s_planet_data* saved_planet_ptr = MOX::planet;
        s_colony* saved_colony_ptr = MOX::colony;
        s_ship_data* saved_ship_ptr = MOX::ship;
        s_leader_data* saved_leader_ptr = MOX::leaders;
        s_nebula* saved_nebula_ptr = MOX::nebula;
        s_animation_header* saved_screen_seg = MOX::screen_seg;
        void (*saved_net_send_hook)(int32_t, int16_t, void*, uint32_t) = netcode::g_net_send_hook;
        uint8_t* saved_snapshot_cache_buffer = NETMOX::net_snapshot_cache.buffer;
        uint8_t* saved_snapshot_cache_current_ptr = NETMOX::net_snapshot_cache.current_ptr;
        uint32_t saved_snapshot_cache_size = NETMOX::net_snapshot_cache.buffer_size;

        std::vector<s_player> players = std::vector<s_player>(MAX_PLAYERS);
        std::vector<s_star_data> stars = std::vector<s_star_data>(MAX_STARS + 1);
        std::vector<s_planet_data> planets = std::vector<s_planet_data>(MAX_PLANETS);
        std::vector<s_colony> colonies = std::vector<s_colony>(MAX_COLONIES);
        std::vector<s_ship_data> ships = std::vector<s_ship_data>(MAX_SHIPS);
        std::vector<s_leader_data> leaders = std::vector<s_leader_data>(MAX_LEADERS);
        std::vector<s_nebula> nebulas = std::vector<s_nebula>(MAX_NEBULAS);

        Scoped_Netmox_Roundtrip_State_() {
            std::memcpy(saved_bill_savegame.data(), MOX::bill_savegame, sizeof(MOX::bill_savegame));
            std::memcpy(saved_reports.data(), REPORT::reports, sizeof(REPORT::reports));
            std::memcpy(saved_event_data.data(), EVENTS::event_data, sizeof(EVENTS::event_data));
            std::memcpy(saved_first_msg.data(), MSG_::first_msg, sizeof(MSG_::first_msg));
            std::memcpy(saved_max_msgs.data(), MSG_::max_msgs, sizeof(MSG_::max_msgs));
            std::memcpy(saved_n_msgs.data(), MSG_::n_msgs, sizeof(MSG_::n_msgs));
            std::memcpy(saved_msgs.data(), MSG_::msgs, sizeof(MSG_::msgs));

            MOX::player = players.data();
            MOX::star = stars.data();
            MOX::planet = planets.data();
            MOX::colony = colonies.data();
            MOX::ship = ships.data();
            MOX::leaders = leaders.data();
            MOX::nebula = nebulas.data();
        }

        ~Scoped_Netmox_Roundtrip_State_() {
            TEST_LUA_NETWORK::Set_Network_Lua_Enabled_(false);
            TEST_LUA_NETWORK::Set_Queued_Load_State_(false);
            if (MOX::screen_seg != saved_screen_seg) {
                std::free((s_alloc_header*)MOX::screen_seg - 1);
            }

            MOX::player = saved_player_ptr;
            MOX::star = saved_star_ptr;
            MOX::planet = saved_planet_ptr;
            MOX::colony = saved_colony_ptr;
            MOX::ship = saved_ship_ptr;
            MOX::leaders = saved_leader_ptr;
            MOX::nebula = saved_nebula_ptr;

            NETMOX::net = saved_net;
            MOX::stardate = saved_stardate;
            MOX::settings = saved_settings;
            MOX::game_type = saved_game_type;
            MOX::PLAYER_NUM = saved_player_num;
            MOX::NUM_PLAYERS = saved_num_players;
            MOX::NUM_STARS = saved_num_stars;
            MOX::planet_count = saved_planet_count;
            MOX::NUM_COLONIES = saved_num_colonies;
            MOX::NUM_SHIPS = saved_num_ships;
            MOX::NUM_NEBULAS = saved_num_nebulas;
            SETTLER::max_pop_freighted = saved_max_pop_freighted;
            MOX::antarans = saved_antarans;
            MOX::screen_seg = saved_screen_seg;
            netcode::g_net_send_hook = saved_net_send_hook;
            game_random::Set_Random_Seed(saved_random_seed);
            REPORT::n_reports = saved_report_count;
            std::memcpy(MOX::bill_savegame, saved_bill_savegame.data(), sizeof(MOX::bill_savegame));
            std::memcpy(REPORT::reports, saved_reports.data(), sizeof(REPORT::reports));
            std::memcpy(EVENTS::event_data, saved_event_data.data(), sizeof(EVENTS::event_data));
            std::memcpy(MSG_::first_msg, saved_first_msg.data(), sizeof(MSG_::first_msg));
            std::memcpy(MSG_::max_msgs, saved_max_msgs.data(), sizeof(MSG_::max_msgs));
            std::memcpy(MSG_::n_msgs, saved_n_msgs.data(), sizeof(MSG_::n_msgs));
            std::memcpy(MSG_::msgs, saved_msgs.data(), sizeof(MSG_::msgs));

            if (NETMOX::net_snapshot_cache.buffer != saved_snapshot_cache_buffer) {
                std::free(NETMOX::net_snapshot_cache.buffer);
            }
            NETMOX::net_snapshot_cache.buffer = saved_snapshot_cache_buffer;
            NETMOX::net_snapshot_cache.current_ptr = saved_snapshot_cache_current_ptr;
            NETMOX::net_snapshot_cache.buffer_size = saved_snapshot_cache_size;
        }
    };

    void Fill_Netmox_Roundtrip_State_() {
        MOX::PLAYER_NUM = 1;
        MOX::NUM_PLAYERS = 3;
        MOX::NUM_STARS = 4;
        MOX::planet_count = 6;
        MOX::NUM_COLONIES = 5;
        MOX::NUM_SHIPS = 7;
        MOX::NUM_NEBULAS = 2;
        MOX::stardate = 35123;
        MOX::settings.random_events = 1;
        MOX::settings.game_difficulty = 3;
        MOX::settings.number_of_players = 3;
        MOX::settings.galaxy_size = 2;
        MOX::settings.galaxy_age = 1;
        MOX::settings.strategic_combat_flag = 1;
        MOX::settings.starting_civilization_level = 4;
        MOX::settings.xenons_exist = 1;
        NETMOX::net.host_election_id = 77;
        diplodef::COUNCIL_FLAG = 1;
        diplodef::next_council_meeting_turn = 42;
        EVENTS::last_event_year = 123456u;
        EVENTS::event_delay_count = 9;
        REPORT::n_reports = 3;
        game_random::Set_Random_Seed(0x13572468u);

        std::memset(MOX::player, 0, MAX_PLAYERS * sizeof(s_player));
        std::memset(MOX::star, 0, (MAX_STARS + 1) * sizeof(s_star_data));
        std::memset(MOX::planet, 0, MAX_PLANETS * sizeof(s_planet_data));
        std::memset(MOX::colony, 0, MAX_COLONIES * sizeof(s_colony));
        std::memset(MOX::ship, 0, MAX_SHIPS * sizeof(s_ship_data));
        std::memset(MOX::leaders, 0, MAX_LEADERS * sizeof(s_leader_data));
        std::memset(MOX::nebula, 0, MAX_NEBULAS * sizeof(s_nebula));
        std::memset(REPORT::reports, 0, sizeof(REPORT::reports));
        std::memset(EVENTS::event_data, 0, sizeof(EVENTS::event_data));
        std::memset(MSG_::first_msg, 0, sizeof(MSG_::first_msg));
        std::memset(MSG_::max_msgs, 0, sizeof(MSG_::max_msgs));
        std::memset(MSG_::n_msgs, 0, sizeof(MSG_::n_msgs));
        std::memset(MSG_::msgs, 0, sizeof(MSG_::msgs));
        std::memset(MOX::bill_savegame, 0, sizeof(MOX::bill_savegame));
        std::memset(&MOX::antarans, 0, sizeof(MOX::antarans));

        for (int i = 0; i < MOX::NUM_PLAYERS; ++i) {
            MOX::player[i].network_player_id = (int16_t)(100 + i);
            MOX::player[i].objectives = (i == 2) ? PLAYER_OBJECTIVE_MILITARIST : PLAYER_OBJECTIVE_HUMAN;
            MOX::player[i].race = (uint8_t)(i + 3);
            MOX::player[i].color = (uint8_t)(i + 1);
            MOX::player[i].home_planet_id = (int16_t)i;
            MOX::player[i].ship_range = (int16_t)(30 + i);
            MOX::player[i].bc = 1000 + i * 77;
            MOX::player[i].tech_applications[0] = (uint8_t)(11 + i);
            MOX::player[i].tech_applications[17] = (uint8_t)(22 + i);
            MOX::player[i].name[0] = (char)('A' + i);
            MOX::player[i].race_name[0] = (char)('K' + i);
        }

        for (int i = 0; i <= MOX::NUM_STARS; ++i) {
            MOX::star[i].x = (int16_t)(10 + i * 3);
            MOX::star[i].y = (int16_t)(20 + i * 5);
            MOX::star[i].owner = (int8_t)(i % MOX::NUM_PLAYERS);
            MOX::star[i].surrender_to[0] = (int8_t)(i + 1);
            MOX::star[i].surrender_to[1] = (int8_t)(i + 2);
            MOX::star[i].officer_index[0] = (int8_t)(30 + i);
        }

        for (int i = 0; i < MOX::planet_count; ++i) {
            MOX::planet[i].star_index = (int8_t)(i % MOX::NUM_STARS);
            MOX::planet[i].orbit = (int8_t)(i % 5);
            MOX::planet[i].colony_index = (int16_t)((i < MOX::NUM_COLONIES) ? i : -1);
            MOX::planet[i].planet_type = (int8_t)(i + 1);
            MOX::planet[i].climate = (char)(i + 2);
        }

        for (int i = 0; i < MOX::NUM_COLONIES; ++i) {
            MOX::colony[i].owner = (int8_t)(i % MOX::NUM_PLAYERS);
            MOX::colony[i].planet = (int16_t)i;
            MOX::colony[i].morale = (int8_t)(50 + i);
            MOX::colony[i].pollution = (int16_t)(i * 2);
            MOX::colony[i].pop_growth[0] = (int16_t)(100 + i);
        }

        for (int i = 0; i < MOX::NUM_SHIPS; ++i) {
            MOX::ship[i].owner = (int8_t)(i % MOX::NUM_PLAYERS);
            MOX::ship[i].location = (int16_t)(i % MOX::NUM_STARS);
            MOX::ship[i].status = 0;
            MOX::ship[i].turns_left = (int8_t)(i + 1);
            MOX::ship[i].d.ship_type = (int8_t)(i % 3);
        }

        for (int i = 0; i < MAX_LEADERS; ++i) {
            MOX::leaders[i].player_index = (int8_t)(i % MOX::NUM_PLAYERS);
            MOX::leaders[i].location = (int8_t)(i % MOX::NUM_STARS);
            MOX::leaders[i].eta = (int8_t)(i + 3);
        }

        for (int i = 0; i < REPORT::n_reports; ++i) {
            REPORT::reports[i].type = (uint8_t)(i + 1);
            REPORT::reports[i].player_idx = (int16_t)i;
            REPORT::reports[i].action = (uint8_t)(7 + i);
            REPORT::reports[i].d.steal.tech_app_id = (int16_t)(100 + i);
            for (size_t j = 0; j < sizeof(REPORT::reports[i].reserved); ++j) {
                REPORT::reports[i].reserved[j] = (uint8_t)(10 + i + j);
            }
        }

        for (int i = 0; i < 36; ++i) {
            EVENTS::event_data[i].player_idx = (int8_t)(i % MOX::NUM_PLAYERS);
            EVENTS::event_data[i].status = (int8_t)(i % 7);
            EVENTS::event_data[i].turn_count = (int16_t)(i * 3);
            EVENTS::event_data[i].target_id = (int16_t)(1000 + i);
            EVENTS::event_data[i].amount = (int16_t)(2000 + i);
            EVENTS::event_data[i].max_amount = (int16_t)(3000 + i);
        }

        for (int i = 0; i < MOX::NUM_PLAYERS; ++i) {
            MSG_::first_msg[i] = (uint8_t)(i * 2);
            MSG_::max_msgs[i] = (uint8_t)(10 + i);
            MSG_::n_msgs[i] = (uint8_t)(3 + i);
        }

        for (int i = 0; i < 100; ++i) {
            MSG_::msgs[i].msg_type = (int8_t)(i % 9);
            MSG_::msgs[i].priority = (uint8_t)(i % 5);
            MSG_::msgs[i].u.financial.a = (int16_t)(500 + i);
            MSG_::msgs[i].u.financial.b = (int16_t)(700 + i);
        }

        for (int i = 0; i < MOX::NUM_NEBULAS; ++i) {
            MOX::nebula[i].x = (int16_t)(5 + i);
            MOX::nebula[i].y = (int16_t)(7 + i);
            MOX::nebula[i].type = (int8_t)(9 + i);
        }

        for (int i = 0; i < 6; ++i) {
            MOX::bill_savegame[i] = (int16_t)(600 + i);
        }

        MOX::antarans.attack_notified_mask = 3;
        MOX::antarans.attack_pressure = 4;
        MOX::antarans.resource_offensive = 5;
        MOX::antarans.resource_defensive = 6;
        MOX::antarans.invasion_stardate = 777;
        MOX::antarans.offensive_titan_built = 1;
        MOX::antarans.offensive_limit_exhausted_mask = 0x05;
    }
}

DOCTEST_TEST_SUITE("netmox packet limits") {

DOCTEST_TEST_CASE("large combat payload allocations stay within the shared limit") {
    constexpr uint32_t max_network_payload_bytes = NETMOX::NETMOX_MAX_GAME_DATA_PAYLOAD_BYTES;
    constexpr uint32_t game_orders_payload_bytes =
        sizeof(uint32_t) +
        sizeof(uint8_t) +
        sizeof(s_player) +
        MAX_STARS * sizeof(s_net_pack_star_order) +
        sizeof(int16_t) +
        MAX_COLONIES * (sizeof(int16_t) + sizeof(s_colony)) +
        sizeof(int16_t) +
        sizeof(int16_t) +
        MAX_SHIPS * (sizeof(int16_t) + sizeof(s_ship_data)) +
        sizeof(int16_t) +
        MAX_LEADERS * (sizeof(int16_t) + sizeof(s_leader_data)) +
        sizeof(int16_t);

    constexpr uint32_t battle_results_remote_payload_bytes =
        sizeof(int16_t) * 4 +
        sizeof(int16_t) +
        MAX_PLAYERS * sizeof(s_combfind_star_has_player) +
        sizeof(uint8_t) +
        sizeof(int16_t) * 2 +
        2 * (sizeof(MOX::player[0].tech_applications) + sizeof(uint8_t)) +
        MAX_LEADERS * (sizeof(int16_t) + sizeof(s_leader_data)) +
        sizeof(int16_t) +
        (sizeof(int16_t) * 2 + sizeof(uint8_t) * 2 + sizeof(s_colony)) +
        sizeof(int16_t) +
        MAX_SHIPS * (sizeof(int16_t) + sizeof(s_ship_data)) +
        sizeof(int16_t);

    constexpr uint32_t you_are_target_remote_payload_bytes =
        sizeof(NETMOX::s_packet_0x12) +
        MAX_PLAYERS * sizeof(s_combfind_star_has_player) +
        sizeof(uint8_t) +
        2 * (sizeof(MOX::player[0].tech_fields) + sizeof(MOX::player[0].tech_applications)) +
        MAX_LEADERS * (sizeof(int16_t) + sizeof(s_leader_data)) +
        sizeof(int16_t) +
        2 * sizeof(uint8_t) +
        (sizeof(int16_t) * 3 + sizeof(uint8_t) * 2 + sizeof(s_colony)) +
        sizeof(int16_t) +
        MAX_SHIPS * (sizeof(int16_t) + sizeof(s_ship_data)) +
        sizeof(int16_t);

    constexpr uint32_t pick_target_remote_payload_bytes =
        sizeof(int16_t) +
        sizeof(s_combfind_target) +
        MAX_PLAYERS * sizeof(s_combfind_star_has_player) +
        sizeof(uint8_t) +
        MAX_PLAYERS * (sizeof(MOX::player[0].tech_fields) + sizeof(MOX::player[0].tech_applications)) +
        MAX_LEADERS * (sizeof(int16_t) + sizeof(s_leader_data)) +
        sizeof(int16_t) +
        MAX_PLAYERS * sizeof(uint8_t) +
        sizeof(int16_t) +
        5 * (sizeof(int16_t) * 2 + sizeof(uint8_t) * 2 + sizeof(s_colony)) +
        sizeof(int16_t) +
        MAX_SHIPS * (sizeof(int16_t) + sizeof(s_ship_data)) +
        sizeof(int16_t);

    DOCTEST_CHECK(game_orders_payload_bytes <= max_network_payload_bytes);
    DOCTEST_CHECK(battle_results_remote_payload_bytes <= max_network_payload_bytes);
    DOCTEST_CHECK(you_are_target_remote_payload_bytes <= max_network_payload_bytes);
    DOCTEST_CHECK(pick_target_remote_payload_bytes <= max_network_payload_bytes);
}

DOCTEST_TEST_CASE("broadcast differential layout still matches legacy fixed-size arrays") {
    DOCTEST_CHECK(sizeof(REPORT::reports) == 50 * sizeof(s_report));
    DOCTEST_CHECK(sizeof(EVENTS::event_data) == 36 * sizeof(s_event_data));
    DOCTEST_CHECK(sizeof(MSG_::msgs) == 100 * sizeof(s_MSG));
    DOCTEST_CHECK(sizeof(MSG_::first_msg) == MAX_PLAYERS * sizeof(MSG_::first_msg[0]));
    DOCTEST_CHECK(sizeof(MSG_::max_msgs) == MAX_PLAYERS * sizeof(MSG_::max_msgs[0]));
    DOCTEST_CHECK(sizeof(MSG_::n_msgs) == MAX_PLAYERS * sizeof(MSG_::n_msgs[0]));
    DOCTEST_CHECK(sizeof(MOX::bill_savegame) == 6 * sizeof(MOX::bill_savegame[0]));
}

DOCTEST_TEST_CASE("broadcast snapshot maximum fits the shared network payload limit") {
    constexpr uint32_t broadcast_snapshot_payload_bytes = Broadcast_Game_Data_Snapshot_Max_Bytes_();

    DOCTEST_CHECK(broadcast_snapshot_payload_bytes <= NETMOX::NETMOX_MAX_GAME_DATA_PAYLOAD_BYTES);
}

DOCTEST_TEST_CASE("differential decode requires full shared payload workspace on the receive side") {
    constexpr uint32_t worst_case_received_diff_bytes = Broadcast_Game_Data_Snapshot_Max_Bytes_() + 1;
    constexpr uint32_t required_decode_workspace_bytes = NETMOX::NETMOX_MAX_GAME_DATA_PAYLOAD_BYTES;

    DOCTEST_CHECK(required_decode_workspace_bytes >= worst_case_received_diff_bytes);
    DOCTEST_CHECK(Broadcast_Game_Data_Snapshot_Max_Bytes_() +
                  worst_case_received_diff_bytes <= required_decode_workspace_bytes);
}

DOCTEST_TEST_CASE("network packet structs stay byte packed and size stable") {
    DOCTEST_CHECK(alignof(s_net_pack_combat_target_chosen) == 1);
    DOCTEST_CHECK(sizeof(s_net_pack_combat_target_chosen) == 6);
    DOCTEST_CHECK(offsetof(s_net_pack_combat_target_chosen, combat_defender_id) == 0);
    DOCTEST_CHECK(offsetof(s_net_pack_combat_target_chosen, combat_planet_id) == 2);
    DOCTEST_CHECK(offsetof(s_net_pack_combat_target_chosen, combat_target_player_id) == 4);

    DOCTEST_CHECK(alignof(s_net_pack_change_relation) == 1);
    DOCTEST_CHECK(sizeof(s_net_pack_change_relation) == 12);
    DOCTEST_CHECK(offsetof(s_net_pack_change_relation, incident_value) == 10);

    DOCTEST_CHECK(alignof(s_net_pack_monster_update) == 1);
    DOCTEST_CHECK(sizeof(s_net_pack_monster_update) == 10);
    DOCTEST_CHECK(offsetof(s_net_pack_monster_update, monster_type) == 0);
    DOCTEST_CHECK(offsetof(s_net_pack_monster_update, target_planet_id) == 4);
    DOCTEST_CHECK(offsetof(s_net_pack_monster_update, player_idx_or_amount) == 6);

    DOCTEST_CHECK(alignof(NETMOX::s_packet_dragon_info) == 1);
    DOCTEST_CHECK(sizeof(NETMOX::s_packet_dragon_info) == sizeof(int16_t));

    DOCTEST_CHECK(alignof(NETMOX::s_packet_0x12) == 1);
    DOCTEST_CHECK(sizeof(NETMOX::s_packet_0x12) ==
                  sizeof(int16_t) * 4);

    DOCTEST_CHECK(alignof(NETMOX::s_NET_MSG_RANK) == 1);
    DOCTEST_CHECK(sizeof(NETMOX::s_NET_MSG_RANK) ==
                  sizeof(int32_t) + sizeof(int8_t));

    DOCTEST_CHECK(alignof(NETMOX::s_NET_MSG_COLOR) == 1);
    DOCTEST_CHECK(sizeof(NETMOX::s_NET_MSG_COLOR) ==
                  sizeof(int8_t) + sizeof(int8_t));

    DOCTEST_CHECK(alignof(NETMOX::s_NET_MSG_RACE) == 1);
    DOCTEST_CHECK(sizeof(NETMOX::s_NET_MSG_RACE) ==
                  sizeof(uint8_t) + sizeof(int8_t));

    DOCTEST_CHECK(alignof(NETMOX::s_NET_MSG_SYNC_PLAYER) == 1);
    DOCTEST_CHECK(sizeof(NETMOX::s_NET_MSG_SYNC_PLAYER) ==
                  sizeof(int32_t) + sizeof(s_player) + MAX_STARS * sizeof(int8_t) + sizeof(int32_t) + sizeof(s_player));

    DOCTEST_CHECK(alignof(s_net_pack_drop_out_info) == 1);
    DOCTEST_CHECK(sizeof(s_net_pack_drop_out_info) ==
                  sizeof(s_player) + MAX_STARS * sizeof(int8_t));

    DOCTEST_CHECK(alignof(s_net_pack_end_ai_diplomacy) == 1);
    DOCTEST_CHECK(sizeof(s_net_pack_end_ai_diplomacy) ==
                  sizeof(int32_t) + sizeof(s_player) + MAX_STARS * sizeof(int8_t) + sizeof(int32_t) + sizeof(s_player));
}

DOCTEST_TEST_CASE("pick-position packet preserves expanded freight storage") {
    Scoped_Netmox_Roundtrip_State_ scope;
    Fill_Netmox_Roundtrip_State_();
    MOX::NUM_PLAYERS = MAX_PLAYERS;
    MOX::player[MAX_PLAYERS - 1].settlers[MAX_FREIGHTED_SETTLERS - 1].val =
            0x123456789ABCDEF0ull;
    MOX::screen_seg = (s_animation_header*)allocate::Allocate_Space(
            NETMOX::NETMOX_MAX_GAME_DATA_PAYLOAD_BYTES + sizeof(s_alloc_header));

    g_pick_position_payload.clear();
    netcode::g_net_send_hook = &Capture_Pick_Position_;
    NETMOX::Send_Pick_Your_Position(123);
    netcode::g_net_send_hook = nullptr;

    constexpr uint32_t expected_size =
            sizeof(int16_t) + MAX_PLAYERS * sizeof(s_player);
    DOCTEST_CHECK(expected_size > 65535u);
    DOCTEST_REQUIRE(g_pick_position_payload.size() == expected_size);

    memset(MOX::player, 0, MAX_PLAYERS * sizeof(s_player));
    NETMOX::Decode_Pick_Your_Position(
            (s_net_pack_pick_your_position*)g_pick_position_payload.data(),
            (uint32_t)g_pick_position_payload.size());
    DOCTEST_CHECK((
        MOX::player[MAX_PLAYERS - 1].settlers[MAX_FREIGHTED_SETTLERS - 1].val ==
        0x123456789ABCDEF0ull));

    MOX::NUM_PLAYERS = 3;
    NETMOX::Decode_Pick_Your_Position(
            (s_net_pack_pick_your_position*)g_pick_position_payload.data(),
            (uint32_t)g_pick_position_payload.size() - 1);
    DOCTEST_CHECK(MOX::NUM_PLAYERS == 3);

    s_net_pack_pick_your_position* packet =
            (s_net_pack_pick_your_position*)g_pick_position_payload.data();
    packet->num_players = MAX_PLAYERS + 1;
    NETMOX::Decode_Pick_Your_Position(
            packet, (uint32_t)g_pick_position_payload.size());
    DOCTEST_CHECK(MOX::NUM_PLAYERS == 3);
}

DOCTEST_TEST_CASE("broadcast snapshot encode and decode round trip serialized game state") {
    Scoped_Netmox_Roundtrip_State_ scope;
    Fill_Netmox_Roundtrip_State_();

    const s_network expected_net = NETMOX::net;
    const uint32_t expected_stardate = MOX::stardate;
    const s_settings expected_settings = MOX::settings;
    const uint32_t expected_random_seed = game_random::Get_Random_Seed();
    const int16_t expected_num_players = MOX::NUM_PLAYERS;
    const int16_t expected_num_stars = MOX::NUM_STARS;
    const int16_t expected_planet_count = MOX::planet_count;
    const int16_t expected_num_colonies = MOX::NUM_COLONIES;
    const int16_t expected_num_ships = MOX::NUM_SHIPS;
    const uint8_t expected_num_nebulas = MOX::NUM_NEBULAS;
    const int16_t expected_report_count = REPORT::n_reports;
    std::vector<s_player> expected_players(MOX::player, MOX::player + MAX_PLAYERS);
    std::vector<s_star_data> expected_stars(MOX::star, MOX::star + MAX_STARS + 1);
    std::vector<s_planet_data> expected_planets(MOX::planet, MOX::planet + MAX_PLANETS);
    std::vector<s_colony> expected_colonies(MOX::colony, MOX::colony + MAX_COLONIES);
    std::vector<s_ship_data> expected_ships(MOX::ship, MOX::ship + MAX_SHIPS);
    std::vector<s_leader_data> expected_leaders(MOX::leaders, MOX::leaders + MAX_LEADERS);
    std::array<s_report, 50> expected_reports = {};
    std::array<s_event_data, 36> expected_events = {};
    std::array<uint8_t, MAX_PLAYERS> expected_first_msg = {};
    std::array<uint8_t, MAX_PLAYERS> expected_max_msgs = {};
    std::array<uint8_t, MAX_PLAYERS> expected_n_msgs = {};
    std::array<s_MSG, 100> expected_msgs = {};
    std::array<int16_t, 6> expected_bill = {};
    std::vector<s_nebula> expected_nebulas(MOX::nebula, MOX::nebula + MAX_NEBULAS);
    const s_antaran expected_antarans = MOX::antarans;

    std::memcpy(expected_reports.data(), REPORT::reports, sizeof(REPORT::reports));
    std::memcpy(expected_events.data(), EVENTS::event_data, sizeof(EVENTS::event_data));
    std::memcpy(expected_first_msg.data(), MSG_::first_msg, sizeof(MSG_::first_msg));
    std::memcpy(expected_max_msgs.data(), MSG_::max_msgs, sizeof(MSG_::max_msgs));
    std::memcpy(expected_n_msgs.data(), MSG_::n_msgs, sizeof(MSG_::n_msgs));
    std::memcpy(expected_msgs.data(), MSG_::msgs, sizeof(MSG_::msgs));
    std::memcpy(expected_bill.data(), MOX::bill_savegame, sizeof(MOX::bill_savegame));

    std::vector<uint8_t> payload(Broadcast_Game_Data_Snapshot_Max_Bytes_());
    const uint32_t payload_len = NETMOX::Encode_Broadcast_Game_Data_Snapshot(payload.data(), (uint32_t)payload.size());

    std::memset(&NETMOX::net, 0, sizeof(NETMOX::net));
    MOX::stardate = 0;
    std::memset(&MOX::settings, 0, sizeof(MOX::settings));
    MOX::NUM_PLAYERS = 0;
    MOX::NUM_STARS = 0;
    MOX::planet_count = 0;
    MOX::NUM_COLONIES = 0;
    MOX::NUM_SHIPS = 0;
    MOX::NUM_NEBULAS = 0;
    REPORT::n_reports = 0;
    std::memset(MOX::player, 0, MAX_PLAYERS * sizeof(s_player));
    std::memset(MOX::star, 0, (MAX_STARS + 1) * sizeof(s_star_data));
    std::memset(MOX::planet, 0, MAX_PLANETS * sizeof(s_planet_data));
    std::memset(MOX::colony, 0, MAX_COLONIES * sizeof(s_colony));
    std::memset(MOX::ship, 0, MAX_SHIPS * sizeof(s_ship_data));
    std::memset(MOX::leaders, 0, MAX_LEADERS * sizeof(s_leader_data));
    std::memset(REPORT::reports, 0, sizeof(REPORT::reports));
    std::memset(EVENTS::event_data, 0, sizeof(EVENTS::event_data));
    std::memset(MSG_::first_msg, 0, sizeof(MSG_::first_msg));
    std::memset(MSG_::max_msgs, 0, sizeof(MSG_::max_msgs));
    std::memset(MSG_::n_msgs, 0, sizeof(MSG_::n_msgs));
    std::memset(MSG_::msgs, 0, sizeof(MSG_::msgs));
    std::memset(&MOX::antarans, 0, sizeof(MOX::antarans));
    std::memset(MOX::nebula, 0, MAX_NEBULAS * sizeof(s_nebula));
    std::memset(MOX::bill_savegame, 0, sizeof(MOX::bill_savegame));
    game_random::Set_Random_Seed(0);

    DOCTEST_REQUIRE(NETMOX::Decode_Broadcast_Game_Data_Differential(payload.data(), payload_len, 1) == 1);

    DOCTEST_CHECK(NETMOX::net.host_election_id == expected_net.host_election_id);
    DOCTEST_CHECK(MOX::stardate == expected_stardate);
    DOCTEST_CHECK(std::memcmp(&MOX::settings, &expected_settings, sizeof(expected_settings)) == 0);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == expected_random_seed);
    DOCTEST_CHECK(MOX::NUM_PLAYERS == expected_num_players);
    DOCTEST_CHECK(MOX::NUM_STARS == expected_num_stars);
    DOCTEST_CHECK(MOX::planet_count == expected_planet_count);
    DOCTEST_CHECK(MOX::NUM_COLONIES == expected_num_colonies);
    DOCTEST_CHECK(MOX::NUM_SHIPS == expected_num_ships);
    DOCTEST_CHECK(MOX::NUM_NEBULAS == expected_num_nebulas);
    DOCTEST_CHECK(REPORT::n_reports == expected_report_count);
    DOCTEST_CHECK(std::memcmp(MOX::player, expected_players.data(), MAX_PLAYERS * sizeof(s_player)) == 0);
    DOCTEST_CHECK(std::memcmp(MOX::star, expected_stars.data(), (MAX_STARS + 1) * sizeof(s_star_data)) == 0);
    DOCTEST_CHECK(std::memcmp(MOX::planet, expected_planets.data(), MAX_PLANETS * sizeof(s_planet_data)) == 0);
    DOCTEST_CHECK(std::memcmp(MOX::colony, expected_colonies.data(), MAX_COLONIES * sizeof(s_colony)) == 0);
    DOCTEST_CHECK(std::memcmp(MOX::ship, expected_ships.data(), MAX_SHIPS * sizeof(s_ship_data)) == 0);
    DOCTEST_CHECK(std::memcmp(MOX::leaders, expected_leaders.data(), MAX_LEADERS * sizeof(s_leader_data)) == 0);
    DOCTEST_CHECK(std::memcmp(REPORT::reports, expected_reports.data(), sizeof(REPORT::reports)) == 0);
    DOCTEST_CHECK(std::memcmp(EVENTS::event_data, expected_events.data(), sizeof(EVENTS::event_data)) == 0);
    DOCTEST_CHECK(std::memcmp(MSG_::first_msg, expected_first_msg.data(), sizeof(MSG_::first_msg)) == 0);
    DOCTEST_CHECK(std::memcmp(MSG_::max_msgs, expected_max_msgs.data(), sizeof(MSG_::max_msgs)) == 0);
    DOCTEST_CHECK(std::memcmp(MSG_::n_msgs, expected_n_msgs.data(), sizeof(MSG_::n_msgs)) == 0);
    DOCTEST_CHECK(std::memcmp(MSG_::msgs, expected_msgs.data(), sizeof(MSG_::msgs)) == 0);
    DOCTEST_CHECK(std::memcmp(&MOX::antarans, &expected_antarans, sizeof(expected_antarans)) == 0);
    DOCTEST_CHECK(std::memcmp(MOX::nebula, expected_nebulas.data(), MAX_NEBULAS * sizeof(s_nebula)) == 0);
    DOCTEST_CHECK(std::memcmp(MOX::bill_savegame, expected_bill.data(), sizeof(MOX::bill_savegame)) == 0);

    std::vector<uint8_t> payload_roundtrip(Broadcast_Game_Data_Snapshot_Max_Bytes_());
    const uint32_t payload_roundtrip_len = NETMOX::Encode_Broadcast_Game_Data_Snapshot(payload_roundtrip.data(), (uint32_t)payload_roundtrip.size());
    DOCTEST_CHECK(payload_roundtrip_len == payload_len);
    DOCTEST_CHECK(std::memcmp(payload_roundtrip.data(), payload.data(), payload_len) == 0);
}

DOCTEST_TEST_CASE("loaded Lua state extends the current native snapshot") {
    Scoped_Netmox_Roundtrip_State_ scope;
    Fill_Netmox_Roundtrip_State_();

    NETMOX::net.host_election_id = 306;
    MOX::player[0].network_player_id = 306;
    MOX::player[1].network_player_id = 308;
    MOX::player[2].network_player_id = -1;
    MOX::player[2].objectives = PLAYER_OBJECTIVE_ECOLOGIST;
    MOX::player[1].bc += 999;
    MOX::stardate = 35200;
    const int32_t expected_player_bc = MOX::player[1].bc;
    TEST_LUA_NETWORK::Set_Network_Lua_Enabled_(true);
    TEST_LUA_NETWORK::Set_Queued_Load_State_(true);

    std::vector<uint8_t> payload(NETMOX::NETMOX_MAX_GAME_DATA_PAYLOAD_BYTES);
    const uint32_t payload_len = NETMOX::Encode_Broadcast_Game_Data_Snapshot(
        payload.data(), static_cast<uint32_t>(payload.size()));
    SERIAL::Reader_ envelope(
        payload.data(), payload_len, "invalid test Lua network envelope");
    envelope.Skip(sizeof(uint32_t) + sizeof(uint16_t) * 2);
    const uint32_t native_size = envelope.Read_Uint32();
    envelope.Skip(sizeof(uint32_t) * 2);

    SERIAL::Reader_ native(
        envelope.Ptr(), native_size, "invalid test native network snapshot");
    DOCTEST_CHECK(native.Read_Int16() == 306);
    DOCTEST_CHECK(native.Read_Uint32() == 35200);
    native.Skip(sizeof(MOX::settings.random_events) +
        sizeof(MOX::settings.game_difficulty) +
        sizeof(MOX::settings.number_of_players) +
        sizeof(MOX::settings.galaxy_size) +
        sizeof(MOX::settings.galaxy_age) +
        sizeof(MOX::settings.strategic_combat_flag) +
        sizeof(MOX::settings.starting_civilization_level) +
        sizeof(MOX::settings.xenons_exist) +
        sizeof(uint32_t));
    DOCTEST_REQUIRE(native.Read_Int16() == MOX::NUM_PLAYERS);
    std::array<s_player, 3> transferred_players = {};
    native.Read_Bytes(transferred_players.data(), sizeof(transferred_players));

    DOCTEST_CHECK((transferred_players[0].network_player_id == 306));
    DOCTEST_CHECK((transferred_players[1].network_player_id == 308));
    DOCTEST_CHECK((transferred_players[2].network_player_id == -1));
    DOCTEST_CHECK(transferred_players[2].objectives == PLAYER_OBJECTIVE_ECOLOGIST);
    DOCTEST_CHECK(transferred_players[1].bc == expected_player_bc);
    DOCTEST_CHECK(envelope.Remaining() == native_size + 1);
    envelope.Skip(native_size);
    DOCTEST_CHECK(envelope.Read_Uint8() == 0xA5);

    MOX::stardate = 0;
    MOX::player[0].network_player_id = -1;
    MOX::player[1].bc = 0;
    DOCTEST_REQUIRE(NETMOX::Decode_Broadcast_Game_Data_Differential(
        payload.data(), payload_len, 1) == 1);
    DOCTEST_CHECK(MOX::stardate == 35200);
    DOCTEST_CHECK((MOX::player[0].network_player_id == 306));
    DOCTEST_CHECK((MOX::player[1].bc == expected_player_bc));
    DOCTEST_CHECK(TEST_LUA_NETWORK::Prepared_Payload_Count_() == 1);
}

DOCTEST_TEST_CASE("broadcast differential decode round trips against cached prior snapshot") {
    Scoped_Netmox_Roundtrip_State_ scope;
    Fill_Netmox_Roundtrip_State_();

    std::vector<uint8_t> base_payload(Broadcast_Game_Data_Snapshot_Max_Bytes_());
    const uint32_t base_payload_len = NETMOX::Encode_Broadcast_Game_Data_Snapshot(base_payload.data(), (uint32_t)base_payload.size());

    s_net_snapshot_cache* cache = NETMOX::Open_Net_Snapshot_Cache(base_payload_len);
    NETMOX::Write_Net_Snapshot_Cache(cache, base_payload.data(), base_payload_len);

    MOX::stardate += 19;
    MOX::settings.galaxy_age = 3;
    MOX::player[1].bc += 321;
    MOX::player[1].tech_applications[3] ^= 0x5a;
    MOX::star[2].owner = 2;
    MOX::star[2].surrender_to[1] = 7;
    MOX::planet[3].planet_type = 9;
    MOX::colony[1].morale = 88;
    MOX::ship[3].turns_left = 12;
    MOX::leaders[4].eta = 21;
    REPORT::n_reports = 2;
    REPORT::reports[0].type = 9;
    EVENTS::event_data[5].status = 6;
    MSG_::first_msg[1] = 44;
    MSG_::msgs[7].priority = 4;
    diplodef::COUNCIL_FLAG = 0;
    diplodef::next_council_meeting_turn = 99;
    MOX::antarans.attack_pressure = 8;
    MOX::nebula[0].x = 44;
    EVENTS::last_event_year = 654321u;
    EVENTS::event_delay_count = 4;
    MOX::bill_savegame[1] = 777;

    DOCTEST_SUBCASE("unchanged snapshot length") {}
    DOCTEST_SUBCASE("snapshot grows") {
        MOX::NUM_NEBULAS = 3;
    }
    DOCTEST_SUBCASE("snapshot shrinks") {
        MOX::NUM_NEBULAS = 1;
        MOX::nebula[1] = {};
    }

    const s_network expected_net = NETMOX::net;
    const uint32_t expected_stardate = MOX::stardate;
    const s_settings expected_settings = MOX::settings;
    const uint32_t expected_random_seed = game_random::Get_Random_Seed();
    const int16_t expected_num_players = MOX::NUM_PLAYERS;
    const int16_t expected_num_stars = MOX::NUM_STARS;
    const int16_t expected_planet_count = MOX::planet_count;
    const int16_t expected_num_colonies = MOX::NUM_COLONIES;
    const int16_t expected_num_ships = MOX::NUM_SHIPS;
    const uint8_t expected_num_nebulas = MOX::NUM_NEBULAS;
    const int16_t expected_report_count = REPORT::n_reports;
    std::vector<s_player> expected_players(MOX::player, MOX::player + MAX_PLAYERS);
    std::vector<s_star_data> expected_stars(MOX::star, MOX::star + MAX_STARS + 1);
    std::vector<s_planet_data> expected_planets(MOX::planet, MOX::planet + MAX_PLANETS);
    std::vector<s_colony> expected_colonies(MOX::colony, MOX::colony + MAX_COLONIES);
    std::vector<s_ship_data> expected_ships(MOX::ship, MOX::ship + MAX_SHIPS);
    std::vector<s_leader_data> expected_leaders(MOX::leaders, MOX::leaders + MAX_LEADERS);
    std::array<s_report, 50> expected_reports = {};
    std::array<s_event_data, 36> expected_events = {};
    std::array<uint8_t, MAX_PLAYERS> expected_first_msg = {};
    std::array<uint8_t, MAX_PLAYERS> expected_max_msgs = {};
    std::array<uint8_t, MAX_PLAYERS> expected_n_msgs = {};
    std::array<s_MSG, 100> expected_msgs = {};
    std::array<int16_t, 6> expected_bill = {};
    std::vector<s_nebula> expected_nebulas(MOX::nebula, MOX::nebula + MAX_NEBULAS);
    const s_antaran expected_antarans = MOX::antarans;

    std::memcpy(expected_reports.data(), REPORT::reports, sizeof(REPORT::reports));
    std::memcpy(expected_events.data(), EVENTS::event_data, sizeof(EVENTS::event_data));
    std::memcpy(expected_first_msg.data(), MSG_::first_msg, sizeof(MSG_::first_msg));
    std::memcpy(expected_max_msgs.data(), MSG_::max_msgs, sizeof(MSG_::max_msgs));
    std::memcpy(expected_n_msgs.data(), MSG_::n_msgs, sizeof(MSG_::n_msgs));
    std::memcpy(expected_msgs.data(), MSG_::msgs, sizeof(MSG_::msgs));
    std::memcpy(expected_bill.data(), MOX::bill_savegame, sizeof(MOX::bill_savegame));

    std::vector<uint8_t> next_payload(Broadcast_Game_Data_Snapshot_Max_Bytes_());
    const uint32_t next_payload_len = NETMOX::Encode_Broadcast_Game_Data_Snapshot(next_payload.data(), (uint32_t)next_payload.size());

    std::vector<uint8_t> diff_payload(NETMOX::NETMOX_MAX_GAME_DATA_PAYLOAD_BYTES - next_payload_len);
    const int32_t diff_payload_len = NETMOX::Make_Difference_Buffer(
        cache,
        next_payload.data(),
        (int32_t)next_payload_len,
        diff_payload.data(),
        (int32_t)diff_payload.size());

    std::memset(&NETMOX::net, 0, sizeof(NETMOX::net));
    MOX::stardate = 0;
    std::memset(&MOX::settings, 0, sizeof(MOX::settings));
    MOX::NUM_PLAYERS = 0;
    MOX::NUM_STARS = 0;
    MOX::planet_count = 0;
    MOX::NUM_COLONIES = 0;
    MOX::NUM_SHIPS = 0;
    MOX::NUM_NEBULAS = 0;
    REPORT::n_reports = 0;
    std::memset(MOX::player, 0, MAX_PLAYERS * sizeof(s_player));
    std::memset(MOX::star, 0, (MAX_STARS + 1) * sizeof(s_star_data));
    std::memset(MOX::planet, 0, MAX_PLANETS * sizeof(s_planet_data));
    std::memset(MOX::colony, 0, MAX_COLONIES * sizeof(s_colony));
    std::memset(MOX::ship, 0, MAX_SHIPS * sizeof(s_ship_data));
    std::memset(MOX::leaders, 0, MAX_LEADERS * sizeof(s_leader_data));
    std::memset(REPORT::reports, 0, sizeof(REPORT::reports));
    std::memset(EVENTS::event_data, 0, sizeof(EVENTS::event_data));
    std::memset(MSG_::first_msg, 0, sizeof(MSG_::first_msg));
    std::memset(MSG_::max_msgs, 0, sizeof(MSG_::max_msgs));
    std::memset(MSG_::n_msgs, 0, sizeof(MSG_::n_msgs));
    std::memset(MSG_::msgs, 0, sizeof(MSG_::msgs));
    std::memset(&MOX::antarans, 0, sizeof(MOX::antarans));
    std::memset(MOX::nebula, 0, MAX_NEBULAS * sizeof(s_nebula));
    std::memset(MOX::bill_savegame, 0, sizeof(MOX::bill_savegame));
    game_random::Set_Random_Seed(0);

    struct s_observed_update {
        std::vector<game_events::e_event> events;
        std::vector<uint32_t> turns;
        uint64_t token = 0;
        ~s_observed_update() {
            game_events::Unsubscribe(token);
        }
    } observed;
    observed.token = game_events::Subscribe([](game_events::e_event event, void* context) {
        if (event != game_events::WORLD_UPDATE_BEGIN && event != game_events::WORLD_UPDATE_COMMITTED && event != game_events::WORLD_UPDATE_FAILED) return;
        auto* update = (s_observed_update*)context;
        update->events.push_back(event);
        update->turns.push_back(MOX::stardate);
    }, &observed);
    DOCTEST_REQUIRE(observed.token != 0);
    DOCTEST_REQUIRE(NETMOX::Decode_Broadcast_Game_Data_Differential(diff_payload.data(), (size_t)diff_payload_len, 0) == 1);
    const std::vector<game_events::e_event> expected_update_events = {game_events::WORLD_UPDATE_BEGIN, game_events::WORLD_UPDATE_COMMITTED};
    DOCTEST_CHECK(observed.events == expected_update_events);
    DOCTEST_REQUIRE(observed.turns.size() == 2);
    DOCTEST_CHECK(observed.turns[0] == 0);
    DOCTEST_CHECK(observed.turns[1] == expected_stardate);

    DOCTEST_CHECK(NETMOX::net.host_election_id == expected_net.host_election_id);
    DOCTEST_CHECK(MOX::stardate == expected_stardate);
    DOCTEST_CHECK(std::memcmp(&MOX::settings, &expected_settings, sizeof(expected_settings)) == 0);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == expected_random_seed);
    DOCTEST_CHECK(MOX::NUM_PLAYERS == expected_num_players);
    DOCTEST_CHECK(MOX::NUM_STARS == expected_num_stars);
    DOCTEST_CHECK(MOX::planet_count == expected_planet_count);
    DOCTEST_CHECK(MOX::NUM_COLONIES == expected_num_colonies);
    DOCTEST_CHECK(MOX::NUM_SHIPS == expected_num_ships);
    DOCTEST_CHECK(MOX::NUM_NEBULAS == expected_num_nebulas);
    DOCTEST_CHECK(REPORT::n_reports == expected_report_count);
    DOCTEST_CHECK(std::memcmp(MOX::player, expected_players.data(), MAX_PLAYERS * sizeof(s_player)) == 0);
    DOCTEST_CHECK(std::memcmp(MOX::star, expected_stars.data(), (MAX_STARS + 1) * sizeof(s_star_data)) == 0);
    DOCTEST_CHECK(std::memcmp(MOX::planet, expected_planets.data(), MAX_PLANETS * sizeof(s_planet_data)) == 0);
    DOCTEST_CHECK(std::memcmp(MOX::colony, expected_colonies.data(), MAX_COLONIES * sizeof(s_colony)) == 0);
    DOCTEST_CHECK(std::memcmp(MOX::ship, expected_ships.data(), MAX_SHIPS * sizeof(s_ship_data)) == 0);
    DOCTEST_CHECK(std::memcmp(MOX::leaders, expected_leaders.data(), MAX_LEADERS * sizeof(s_leader_data)) == 0);
    DOCTEST_CHECK(std::memcmp(REPORT::reports, expected_reports.data(), sizeof(REPORT::reports)) == 0);
    DOCTEST_CHECK(std::memcmp(EVENTS::event_data, expected_events.data(), sizeof(EVENTS::event_data)) == 0);
    DOCTEST_CHECK(std::memcmp(MSG_::first_msg, expected_first_msg.data(), sizeof(MSG_::first_msg)) == 0);
    DOCTEST_CHECK(std::memcmp(MSG_::max_msgs, expected_max_msgs.data(), sizeof(MSG_::max_msgs)) == 0);
    DOCTEST_CHECK(std::memcmp(MSG_::n_msgs, expected_n_msgs.data(), sizeof(MSG_::n_msgs)) == 0);
    DOCTEST_CHECK(std::memcmp(MSG_::msgs, expected_msgs.data(), sizeof(MSG_::msgs)) == 0);
    DOCTEST_CHECK(std::memcmp(&MOX::antarans, &expected_antarans, sizeof(expected_antarans)) == 0);
    DOCTEST_CHECK(std::memcmp(MOX::nebula, expected_nebulas.data(), MAX_NEBULAS * sizeof(s_nebula)) == 0);
    DOCTEST_CHECK(std::memcmp(MOX::bill_savegame, expected_bill.data(), sizeof(MOX::bill_savegame)) == 0);

    std::vector<uint8_t> payload_roundtrip(Broadcast_Game_Data_Snapshot_Max_Bytes_());
    const uint32_t payload_roundtrip_len = NETMOX::Encode_Broadcast_Game_Data_Snapshot(payload_roundtrip.data(), (uint32_t)payload_roundtrip.size());
    DOCTEST_CHECK(payload_roundtrip_len == next_payload_len);
    DOCTEST_CHECK(std::memcmp(payload_roundtrip.data(), next_payload.data(), next_payload_len) == 0);
}

DOCTEST_TEST_CASE("snapshot differences round trip growing and shrinking state") {
    const std::array<int, 11> lengths = {1, 2, 8, 9, 10, 243, 244, 245, 488, 489, 1024};
    for (int base_size : lengths) {
        for (int next_size : lengths) {
            DOCTEST_CAPTURE(base_size);
            DOCTEST_CAPTURE(next_size);
            std::vector<uint8_t> baseline(base_size, 0x42);
            s_net_snapshot_cache cache = {};
            cache.buffer = baseline.data();
            cache.current_ptr = baseline.data();
            cache.buffer_size = static_cast<uint32_t>(baseline.size());

            for (int pattern = 0; pattern < 3; ++pattern) {
                DOCTEST_CAPTURE(pattern);
                std::vector<uint8_t> next(next_size, 0x42);
                for (int i = 0; i < next_size; ++i) {
                    if (pattern == 1 || (pattern == 2 && i % 3 == 0)) {
                        next[i] = 0x73;
                    }
                }
                std::vector<uint8_t> diff(next_size * 2 + 1);
                const int diff_size = NETMOX::Make_Difference_Buffer(
                    &cache, next.data(), next_size, diff.data(), static_cast<int>(diff.size()));
                DOCTEST_REQUIRE(diff_size > 0);
                DOCTEST_REQUIRE(diff_size <= static_cast<int>(diff.size()));
                DOCTEST_CHECK(diff[diff_size - 1] == 0xFF);

                std::vector<uint8_t> decoded(next_size);
                DOCTEST_REQUIRE(NETMOX::Decode_Difference_Buffer(&cache, decoded.data(), next_size, diff.data(), diff_size) == next_size);
                DOCTEST_CHECK(decoded == next);
                DOCTEST_CHECK(baseline == std::vector<uint8_t>(base_size, 0x42));
            }
        }
    }
}

DOCTEST_TEST_CASE("snapshot difference decoder accepts legacy copy and literal runs") {
    std::array<uint8_t, 6> baseline = {1, 2, 3, 4, 5, 6};
    s_net_snapshot_cache cache = {};
    cache.buffer = baseline.data();
    cache.current_ptr = baseline.data();
    cache.buffer_size = static_cast<uint32_t>(baseline.size());
    std::array<uint8_t, 6> diff = {2, 0xF7, 7, 8, 2, 0xFF};
    std::array<uint8_t, 6> decoded = {};
    const std::array<uint8_t, 6> expected = {1, 2, 7, 8, 5, 6};

    DOCTEST_REQUIRE(NETMOX::Decode_Difference_Buffer(&cache, decoded.data(), static_cast<int>(decoded.size()), diff.data(), static_cast<int>(diff.size())) == static_cast<int>(expected.size()));
    DOCTEST_CHECK(decoded == expected);
}

DOCTEST_TEST_CASE("broadcast snapshot encode suppresses live network updates while serializing") {
    Scoped_Netmox_Roundtrip_State_ scope;
    Fill_Netmox_Roundtrip_State_();

    MOX::game_type = 3;
    NETMOX::net.is_network_active = 1;
    NETMOX::net.sync_state = NET_SYNC_STATE_READY;
    g_net_update_hook_invocations = 0;
    netcode::g_net_update_hook = &Snapshot_Encode_Mutation_Hook_;

    std::vector<uint8_t> payload(Broadcast_Game_Data_Snapshot_Max_Bytes_());
    const int32_t expected_bc = MOX::player[1].bc;
    const uint32_t payload_len = NETMOX::Encode_Broadcast_Game_Data_Snapshot(payload.data(), (uint32_t)payload.size());

    DOCTEST_CHECK(payload_len > 0);
    DOCTEST_CHECK(g_net_update_hook_invocations == 0);
    DOCTEST_CHECK(MOX::player[1].bc == expected_bc);

    netcode::g_net_update_hook = nullptr;
}

DOCTEST_TEST_CASE("game orders preserve expanded freight storage") {
    Scoped_Netmox_Roundtrip_State_ scope;
    Fill_Netmox_Roundtrip_State_();

    MOX::game_type = 3;
    MOX::PLAYER_NUM = 1;
    MOX::NUM_STARS = 0;
    MOX::NUM_COLONIES = 0;
    MOX::NUM_SHIPS = 0;
    MOX::screen_seg = static_cast<s_animation_header*>(allocate::Allocate_Space(
        NETMOX::NETMOX_MAX_GAME_DATA_PAYLOAD_BYTES + sizeof(s_alloc_header)));
    NETMOX::net.is_network_active = 1;
    NETMOX::net.net_host_id = 101;
    NETMOX::net.local_net_id = 202;

    SETTLER::max_pop_freighted = MAX_FREIGHTED_SETTLERS;
    s_player* player = &MOX::player[MOX::PLAYER_NUM];
    player->settlers_freighted = MAX_FREIGHTED_SETTLERS;
    player->settlers[MAX_FREIGHTED_SETTLERS - 1].val = 0x123456789ABCDEF0ull;

    g_game_orders_payload.clear();
    netcode::g_net_send_hook = &Capture_Game_Orders_;
    NETMOX::Send_Game_Orders();
    netcode::g_net_send_hook = nullptr;

    DOCTEST_REQUIRE(!g_game_orders_payload.empty());

    player->settlers_freighted = 0;
    std::memset(player->settlers, 0, sizeof(player->settlers));

    NETMOX::Decode_Game_Orders(
        g_game_orders_payload.data(),
        static_cast<uint32_t>(g_game_orders_payload.size()));

    DOCTEST_CHECK(player->settlers_freighted == MAX_FREIGHTED_SETTLERS);
    DOCTEST_CHECK(
        player->settlers[MAX_FREIGHTED_SETTLERS - 1].val ==
        0x123456789ABCDEF0ull);
}

DOCTEST_TEST_CASE("game orders preserve the advanced XP of a deassigned leader") {
    Scoped_Netmox_Roundtrip_State_ scope;
    Fill_Netmox_Roundtrip_State_();

    MOX::game_type = 3;
    MOX::PLAYER_NUM = 1;
    MOX::NUM_STARS = 0;
    MOX::NUM_COLONIES = 0;
    MOX::NUM_SHIPS = 0;
    MOX::screen_seg = static_cast<s_animation_header*>(allocate::Allocate_Space(
        NETMOX::NETMOX_MAX_GAME_DATA_PAYLOAD_BYTES + sizeof(s_alloc_header)));
    NETMOX::net.is_network_active = 1;
    NETMOX::net.net_host_id = 101;
    NETMOX::net.local_net_id = 202;

    constexpr int16_t leader_idx = 4;
    MOX::leaders[leader_idx].player_index = 1;
    MOX::leaders[leader_idx].status = 0;
    MOX::leaders[leader_idx].xp = 59;

    NETMOX::Mark_Leader_Deassigned(leader_idx);
    MOX::leaders[leader_idx].xp = 60;
    NETMOX::Ditch_Leader(leader_idx);

    g_game_orders_payload.clear();
    netcode::g_net_send_hook = &Capture_Game_Orders_;
    NETMOX::Send_Game_Orders();
    netcode::g_net_send_hook = nullptr;

    DOCTEST_REQUIRE(!g_game_orders_payload.empty());

    MOX::leaders[leader_idx].player_index = 1;
    MOX::leaders[leader_idx].status = 0;
    MOX::leaders[leader_idx].xp = 59;

    NETMOX::Decode_Game_Orders(
        g_game_orders_payload.data(),
        static_cast<uint32_t>(g_game_orders_payload.size()));

    DOCTEST_CHECK(MOX::leaders[leader_idx].xp == 60);
    DOCTEST_CHECK(MOX::leaders[leader_idx].player_index == -1);
    DOCTEST_CHECK(MOX::leaders[leader_idx].status == -1);
}

}