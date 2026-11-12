#include "pch.h"

#include <cstdlib>
#include <cstring>
#include <stdexcept>

#include "doctest/doctest.h"

#include "game/allocate.h"
#include "game/mainscr.h"
#include "game/netmox.h"
#include "game/netturn.h"

namespace MULTPLAY {
    extern void (*g_chat_box_input_loop_hook)();
}

namespace TEST_LUA_NETWORK {
    void Set_Network_Lua_Enabled_(bool enabled);
    extern bool authority_allowed;
    extern int authority_calls;
    extern int32_t authority_host, authority_local;
}

namespace {
    struct Network_Flags_Fixture_ {
        s_network saved_network = NETMOX::net;
        s_player* saved_players = MOX::player;
        int16_t saved_player_count = MOX::NUM_PLAYERS;
        int16_t saved_player_num = MOX::PLAYER_NUM;
        decltype(MOX::game_type) saved_game_type = MOX::game_type;
        uint8_t saved_update_counter = NETMOX::next_update_player_counter;
        s_player players[MAX_PLAYERS]{};

        Network_Flags_Fixture_() {
            NETMOX::net = {};
            NETMOX::net.is_network_active = 1;
            NETMOX::net.net_host_id = 200;
            NETMOX::net.diplomacy_initiator_id = -1;
            MOX::player = players;
            MOX::NUM_PLAYERS = MAX_PLAYERS;
            MOX::PLAYER_NUM = 0;
            MOX::game_type = 2;
            NETMOX::next_update_player_counter = 0;
            for (int i = 0; i < MAX_PLAYERS; ++i) {
                players[i].network_player_id = 100 + i;
            }
        }

        ~Network_Flags_Fixture_() {
            NETMOX::net = saved_network;
            MOX::player = saved_players;
            MOX::NUM_PLAYERS = saved_player_count;
            MOX::PLAYER_NUM = saved_player_num;
            MOX::game_type = saved_game_type;
            NETMOX::next_update_player_counter = saved_update_counter;
        }
    };

#ifdef TEST_REAL_MAIN_RECEIVE
    DOCTEST_TEST_CASE_FIXTURE(Network_Flags_Fixture_, "network turns reject Lua authority before host or client execution") {
        struct s_authority_scope {
            bool allowed = TEST_LUA_NETWORK::authority_allowed;
            int calls = TEST_LUA_NETWORK::authority_calls;
            int32_t host = TEST_LUA_NETWORK::authority_host;
            int32_t local = TEST_LUA_NETWORK::authority_local;
            s_net_chat* chat = CHAT::chat_info;
            void (*hook)() = MULTPLAY::g_chat_box_input_loop_hook;
            ~s_authority_scope() {
                TEST_LUA_NETWORK::authority_allowed = allowed;
                TEST_LUA_NETWORK::authority_calls = calls;
                TEST_LUA_NETWORK::authority_host = host;
                TEST_LUA_NETWORK::authority_local = local;
                CHAT::chat_info = chat;
                MULTPLAY::g_chat_box_input_loop_hook = hook;
            }
        } scope;
        MOX::NUM_PLAYERS = 1;
        players[0].objectives = PLAYER_OBJECTIVE_HUMAN;
        players[0].network_player_id = 44;
        players[0].surrender_to = -1;
        NETMOX::net.net_host_id = 99;
        MULTPLAY::g_chat_box_input_loop_hook = nullptr;
        TEST_LUA_NETWORK::authority_allowed = false;
        TEST_LUA_NETWORK::authority_calls = 0;
        DOCTEST_CHECK_THROWS_AS(NETTURN::Net_Next_Turn(), std::runtime_error);
        DOCTEST_CHECK(TEST_LUA_NETWORK::authority_calls == 1);
        DOCTEST_CHECK(TEST_LUA_NETWORK::authority_host == 44);
        DOCTEST_CHECK(TEST_LUA_NETWORK::authority_local == 99);
        DOCTEST_CHECK(NETMOX::net.sync_state == NET_SYNC_STATE_NEXT_TURN_READY);
    }
#endif

    void Client_Next_Turn_Payload_Arrives_Before_Final_Wait_() {
        static int callback_count = 0;
        ++callback_count;

        if (callback_count == 1) {
            NETMOX::net.sync_state = NET_SYNC_STATE_RECEIVING_GAME_ORDERS;
            return;
        }

        if (callback_count == 2) {
            static uint8_t payload = 0x5A;
            NETMOX::net.network_data_ptr = &payload;
            NETMOX::net.total_bytes_received = 1;
            NETMOX::net.sync_state = NET_SYNC_STATE_WAITING_FOR_GAME_DATA;
            MOX::player[MOX::PLAYER_NUM].eliminated = 1;
            MULTPLAY::g_chat_box_input_loop_hook = nullptr;
        }
    }
}

DOCTEST_TEST_SUITE("netturn host selection") {

DOCTEST_TEST_CASE("election reset clears all order flags without clearing diplomacy requests") {
    Network_Flags_Fixture_ fixture;
    NETMOX::net.human_diplomacy_request_mask = 0xA5;
    NETMOX::net.field130_0x1a4 = 0x5A;
    std::memset(NETMOX::net.host_orders_received_flags, 1, sizeof(NETMOX::net.host_orders_received_flags));

    NETTURN::Net_Init_Host_Election();

    DOCTEST_CHECK(NETMOX::net.human_diplomacy_request_mask == 0xA5);
    DOCTEST_CHECK(NETMOX::net.field130_0x1a4 == 0x5A);
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        DOCTEST_CHECK(NETMOX::net.host_orders_received_flags[i] == 0);
    }
}

DOCTEST_TEST_CASE("diplomacy request selection leaves every player order flag untouched") {
    Network_Flags_Fixture_ fixture;
    NETMOX::net.human_diplomacy_request_mask = 1u << (MAX_PLAYERS - 1);
    std::memset(NETMOX::net.host_orders_received_flags, 1, sizeof(NETMOX::net.host_orders_received_flags));

    DOCTEST_CHECK(NETMOX::Human_Requesting_Diplomacy() == MAX_PLAYERS - 1);
    DOCTEST_CHECK(NETMOX::net.human_diplomacy_request_mask == 0);
    DOCTEST_CHECK(NETMOX::Human_Requesting_Diplomacy() == -1);
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        DOCTEST_CHECK(NETMOX::net.host_orders_received_flags[i] == 1);
    }
}

DOCTEST_TEST_CASE("phase ready flags cover all player slots without changing adjacent state") {
    Network_Flags_Fixture_ fixture;
    NETMOX::net.sync_state = NET_SYNC_STATE_READY;
    NETMOX::net.player_race_selected_alt_flags[0] = 0x5A;

    for (int i = 0; i < MAX_PLAYERS; ++i) {
        NETMOX::Mox_Net_Update();
        for (int j = 0; j < MAX_PLAYERS; ++j) {
            DOCTEST_CHECK(NETMOX::net.player_phase_ready_sent_flags[j] == (j <= i ? 1 : 0));
        }
    }
    DOCTEST_CHECK(NETMOX::net.is_network_active == 1);
    DOCTEST_CHECK(NETMOX::net.player_race_selected_alt_flags[0] == 0x5A);
}

DOCTEST_TEST_CASE("race deselection sync tracks and clears each player independently") {
    Network_Flags_Fixture_ fixture;
    NETMOX::net.sync_state = NET_SYNC_STATE_IDLE;
    NETMOX::net.race_selection_changed = 0x5A;
    std::memset(NETMOX::net.player_race_deselected_flags, 1, sizeof(NETMOX::net.player_race_deselected_flags));

    for (int i = 0; i < MAX_PLAYERS; ++i) {
        NETMOX::Mox_Net_Update();
        DOCTEST_CHECK(NETMOX::net.player_race_deselected_flags[i] == 0);
        for (int j = 0; j < MAX_PLAYERS; ++j) {
            DOCTEST_CHECK(NETMOX::net.player_race_selection_sync_pending[j] == (j <= i ? 1 : 0));
        }
    }
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        NETMOX::Mox_Net_Update();
        for (int j = 0; j < MAX_PLAYERS; ++j) {
            DOCTEST_CHECK(NETMOX::net.player_race_selection_sync_pending[j] == (j <= i ? 0 : 1));
        }
    }
    DOCTEST_CHECK(NETMOX::net.race_selection_changed == 0x5A);
}

DOCTEST_TEST_CASE("selects the lowest active player slot") {
    s_player players[3]{};
    players[0].objectives = PLAYER_OBJECTIVE_DIPLOMATIC;
    players[0].network_player_id = 10;
    players[1].objectives = PLAYER_OBJECTIVE_HUMAN;
    players[1].network_player_id = 21;
    players[2].objectives = PLAYER_OBJECTIVE_HUMAN;
    players[2].network_player_id = 22;

    DOCTEST_CHECK(NETTURN::Determine_Lowest_Active_Host_Player(players, 3) == 1);
}

DOCTEST_TEST_CASE("skips active slots without a valid network id") {
    s_player players[3]{};
    players[0].objectives = PLAYER_OBJECTIVE_HUMAN;
    players[0].network_player_id = -1;
    players[1].objectives = PLAYER_OBJECTIVE_HUMAN;
    players[1].network_player_id = -1;
    players[2].objectives = PLAYER_OBJECTIVE_HUMAN;
    players[2].network_player_id = 42;

    DOCTEST_CHECK(NETTURN::Determine_Lowest_Active_Host_Player(players, 3) == 2);
}

DOCTEST_TEST_CASE("ignores inactive or eliminated slots even if they have network ids") {
    s_player players[3]{};
    players[0].objectives = 12;
    players[0].network_player_id = 7;
    players[1].objectives = 99;
    players[1].network_player_id = 8;
    players[2].objectives = PLAYER_OBJECTIVE_HUMAN;
    players[2].network_player_id = 9;

    DOCTEST_CHECK(NETTURN::Determine_Lowest_Active_Host_Player(players, 3) == 2);
}

DOCTEST_TEST_CASE("skips eliminated active players") {
    s_player players[3]{};
    players[0].objectives = PLAYER_OBJECTIVE_HUMAN;
    players[0].network_player_id = 15;
    players[0].eliminated = 1;
    players[1].objectives = PLAYER_OBJECTIVE_HUMAN;
    players[1].network_player_id = 27;
    players[1].eliminated = 0;
    players[2].objectives = PLAYER_OBJECTIVE_HUMAN;
    players[2].network_player_id = 39;
    players[2].eliminated = 0;

    DOCTEST_CHECK(NETTURN::Determine_Lowest_Active_Host_Player(players, 3) == 1);
}

DOCTEST_TEST_CASE("returns no host when no active player has a valid network id") {
    s_player players[3]{};
    players[0].objectives = PLAYER_OBJECTIVE_DIPLOMATIC;
    players[0].network_player_id = -1;
    players[1].objectives = PLAYER_OBJECTIVE_HUMAN;
    players[1].network_player_id = -1;
    players[2].objectives = PLAYER_OBJECTIVE_DIPLOMATIC;
    players[2].network_player_id = 12;

    DOCTEST_CHECK(NETTURN::Determine_Lowest_Active_Host_Player(players, 3) == -1);
}

DOCTEST_TEST_CASE("returns no host for empty input") {
    DOCTEST_CHECK(NETTURN::Determine_Lowest_Active_Host_Player(static_cast<const s_player*>(nullptr), 0) == -1);
}

DOCTEST_TEST_CASE("client next turn preserves a payload that arrives before the final wait loop") {
    s_player players[2]{};
    s_leader_data leaders[MAX_LEADERS]{};
    players[0].network_player_id = 176;
    players[0].objectives = PLAYER_OBJECTIVE_HUMAN;
    players[1].network_player_id = 174;
    players[1].objectives = PLAYER_OBJECTIVE_HUMAN;

    MOX::player = players;
    MOX::PLAYER_NUM = 1;
    MOX::NUM_PLAYERS = 2;
    MOX::NUM_STARS = 0;
    MOX::leaders = leaders;
    MOX::game_type = 3;
    MOX::stardate = 35155;
    MOX::screen_seg = static_cast<s_animation_header*>(allocate::Allocate_Space(
        NETMOX::NETMOX_MAX_GAME_DATA_PAYLOAD_BYTES + sizeof(s_alloc_header)));
    NETTURN::waiting_message_fmt = const_cast<char*>("");

    NETMOX::net = {};
    NETMOX::net.local_net_id = 174;
    NETMOX::net.net_host_id = 176;
    NETMOX::net.sync_state = NET_SYNC_STATE_NEXT_TURN_READY;
    NETMOX::net.num_connected_players = 2;

    MULTPLAY::g_chat_box_input_loop_hook = &Client_Next_Turn_Payload_Arrives_Before_Final_Wait_;
    NETTURN::Client_Next_Turn();

    DOCTEST_CHECK(MULTPLAY::g_chat_box_input_loop_hook == nullptr);
    DOCTEST_CHECK((NETMOX::net.total_bytes_received == 1));

    std::free((s_alloc_header*)MOX::screen_seg - 1);
    MOX::screen_seg = nullptr;
}

DOCTEST_TEST_CASE("Lua game data accepts only the elected sender with full decode workspace") {
    const uint8_t payload[] = {0x4F, 0x32, 0x47, 0x44, 0x5A};
    uint8_t retained_payload = 0xA5;
    MOX::screen_seg = static_cast<s_animation_header*>(
        allocate::Allocate_Space(
            NETMOX::NETMOX_MAX_GAME_DATA_PAYLOAD_BYTES +
            sizeof(s_alloc_header)));
    allocate::mark_stack_pos[0] = 0;
    NETMOX::net = {};
    NETMOX::net.host_election_id = 176;
    NETMOX::net.host_election_winner_id = 91;
    NETMOX::net.network_data_ptr = &retained_payload;
    NETMOX::net.total_bytes_received = 1;
    TEST_LUA_NETWORK::Set_Network_Lua_Enabled_(true);

    MAINSCR::Main_Receive_Message(
        1, 174, NET_PACKET_GAME_DATA_PAYLOAD, sizeof(payload), const_cast<uint8_t*>(payload));

    DOCTEST_CHECK((NETMOX::net.network_data_ptr == &retained_payload));
    DOCTEST_CHECK((NETMOX::net.total_bytes_received == 1));
    DOCTEST_CHECK(NETMOX::net.host_election_winner_id == 91);
    DOCTEST_CHECK(allocate::mark_stack_pos[0] == 0);

    MAINSCR::Main_Receive_Message(
        1, 176, NET_PACKET_GAME_DATA_PAYLOAD, sizeof(payload), const_cast<uint8_t*>(payload));

    DOCTEST_REQUIRE((NETMOX::net.network_data_ptr != nullptr));
    DOCTEST_CHECK(std::memcmp(
                      NETMOX::net.network_data_ptr, payload, sizeof(payload)) ==
        0);
    DOCTEST_CHECK((NETMOX::net.total_bytes_received == sizeof(payload)));
    DOCTEST_CHECK(NETMOX::net.host_election_winner_id == 176);
    DOCTEST_CHECK(allocate::Get_Max_Block_Space(
                      NETMOX::net.network_data_ptr) ==
        NETMOX::NETMOX_MAX_GAME_DATA_PAYLOAD_BYTES);
    DOCTEST_CHECK(allocate::mark_stack_pos[0] == 1);

    allocate::Pop_Block(MOX::screen_seg);
    std::free((s_alloc_header*)MOX::screen_seg - 1);
    MOX::screen_seg = nullptr;
    NETMOX::net.network_data_ptr = nullptr;
    TEST_LUA_NETWORK::Set_Network_Lua_Enabled_(false);
}
}

DOCTEST_TEST_SUITE("netmox disconnect recovery") {

DOCTEST_TEST_CASE("client handles host disconnect when local player becomes recovery coordinator") {
    s_player players[3]{};

    players[0].network_player_id = 202;
    players[0].objectives = PLAYER_OBJECTIVE_HUMAN;
    players[0].eliminated = 0;

    players[1].network_player_id = 101;
    players[1].objectives = PLAYER_OBJECTIVE_HUMAN;
    players[1].eliminated = 0;

    players[2].network_player_id = 303;
    players[2].objectives = PLAYER_OBJECTIVE_HUMAN;
    players[2].eliminated = 0;

    MOX::player = players;
    MOX::PLAYER_NUM = 0;
    MOX::NUM_PLAYERS = 3;
    MOX::NUM_STARS = 0;

    NETMOX::net = {};
    NETMOX::net.is_network_active = 1;
    NETMOX::net.net_host_id = 101;
    NETMOX::net.local_net_id = 202;
    NETMOX::net.host_election_id = 101;
    NETMOX::net.host_election_winner_id = 101;
    NETMOX::net.num_connected_players = 3;

    NETMOX::Handle_Network_Player_Disconnected(1);

    DOCTEST_CHECK((MOX::player[1].network_player_id == -1));
    DOCTEST_CHECK(NETMOX::net.num_connected_players == 2);
    DOCTEST_CHECK(NETMOX::net.players_hit_next_turn[1] == 1);
    DOCTEST_CHECK(NETMOX::net.host_election_id == 202);
    DOCTEST_CHECK(NETMOX::net.host_election_winner_id == 202);
    DOCTEST_CHECK(NETMOX::net.local_net_id == 202);
}

DOCTEST_TEST_CASE("local player disconnect is ignored") {
    s_player players[2]{};

    players[0].network_player_id = 202;
    players[0].objectives = PLAYER_OBJECTIVE_HUMAN;
    players[1].network_player_id = 101;
    players[1].objectives = PLAYER_OBJECTIVE_HUMAN;

    MOX::player = players;
    MOX::PLAYER_NUM = 0;
    MOX::NUM_PLAYERS = 2;
    MOX::NUM_STARS = 0;

    NETMOX::net = {};
    NETMOX::net.is_network_active = 1;
    NETMOX::net.net_host_id = 101;
    NETMOX::net.local_net_id = 202;
    NETMOX::net.num_connected_players = 2;

    NETMOX::Handle_Network_Player_Disconnected(0);

    DOCTEST_CHECK((MOX::player[0].network_player_id == 202));
    DOCTEST_CHECK(NETMOX::net.num_connected_players == 2);
}

}