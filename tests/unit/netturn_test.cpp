#include "pch.h"

#include <cstdlib>
#include <cstring>
#include <stdexcept>

#include "doctest/doctest.h"

#include "game/allocate.h"
#include "game/mainscr.h"
#include "game/netmox.h"
#include "game/netturn.h"
#include "../fixtures/game_state.h"

namespace MULTPLAY {
    extern void (*g_chat_box_input_loop_hook)();
}

namespace TEST_LUA_NETWORK {
    void Set_Network_Lua_Enabled_(bool enabled);
    extern bool authority_allowed;
    extern int authority_calls;
    extern int32_t authority_host, authority_local;
}

namespace TEST_NETTURN_DROP {
    extern int16_t confirmation_result;
    extern std::string prompt;
    extern int16_t saved_slot;
    extern bool save_succeeds;
    extern int uninit_calls;
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

DOCTEST_TEST_CASE_FIXTURE(Network_Flags_Fixture_, "a lone network player only converts to single-player when opted in") {
    using test::s_saved_value;
    s_saved_value auto_change(MULTPLAY::auto_single_player_change);
    s_saved_value uninit_calls(TEST_NETTURN_DROP::uninit_calls);
    s_saved_value report(REPORT::something_interesting_happened[0]);
    s_saved_value screen(MOX::current_screen);
    s_saved_value previous_screen(MOX::previous_screen);
    s_saved_value chat(CHAT::chat_info);
    s_saved_value screen_seg(MOX::screen_seg);
    s_saved_value mark_depth(allocate::mark_stack_pos[0]);
    s_saved_value star(MOX::star);
    s_saved_value colony(MOX::colony);
    s_saved_value ship(MOX::ship);
    s_saved_value leader_data(MOX::leaders);
    s_saved_value stars(MOX::NUM_STARS);
    s_saved_value colony_count(MOX::NUM_COLONIES);
    s_saved_value ship_count(MOX::NUM_SHIPS);
    s_saved_value planets(MOX::planet_count);
    s_saved_value nebulas(MOX::NUM_NEBULAS);
    s_saved_value waiting_message(NETTURN::waiting_message_fmt);
    s_saved_value chat_hook(MULTPLAY::g_chat_box_input_loop_hook);
    s_saved_value cache(NETMOX::net_snapshot_cache);
    s_saved_value world(game::current);
    NETMOX::net_snapshot_cache = {};
    struct Snapshot_Cleanup {
        ~Snapshot_Cleanup() { std::free(NETMOX::net_snapshot_cache.buffer); }
    } cleanup;

    s_star_data sentinel_star{};
    s_leader_data leaders[MAX_LEADERS]{};
    std::vector<s_colony> colonies(MAX_COLONIES);
    std::vector<s_ship_data> ships(MAX_SHIPS);
    constexpr auto payload_capacity = NETMOX::NETMOX_MAX_GAME_DATA_PAYLOAD_BYTES;
    MOX::screen_seg = static_cast<s_animation_header*>(allocate::Allocate_Space(2 * (payload_capacity + sizeof(s_alloc_header))));
    std::unique_ptr<s_alloc_header, decltype(&std::free)> arena((s_alloc_header*)MOX::screen_seg - 1, std::free);
    MOX::star = &sentinel_star;
    MOX::colony = colonies.data();
    MOX::ship = ships.data();
    MOX::leaders = leaders;
    MOX::NUM_STARS = MOX::NUM_COLONIES = MOX::NUM_SHIPS = MOX::planet_count = MOX::NUM_NEBULAS = 0;
    MOX::NUM_PLAYERS = 2;
    players[0].objectives = PLAYER_OBJECTIVE_HUMAN;
    players[0].surrender_to = -1;
    players[1].objectives = PLAYER_OBJECTIVE_DIPLOMATIC;
    players[1].network_player_id = -1;
    NETMOX::net.num_connected_players = 1;
    NETMOX::net.net_host_id = players[0].network_player_id;
    NETMOX::Broadcast_Game_Data_Differential(1);

    bool client = false;
    DOCTEST_SUBCASE("host") {}
    DOCTEST_SUBCASE("client") { client = true; }
    for (bool convert : {false, true}) {
        DOCTEST_CAPTURE(convert);
        MULTPLAY::auto_single_player_change = convert;
        TEST_NETTURN_DROP::uninit_calls = 0;
        MOX::game_type = client ? 3 : 2;
        const auto multiplayer_type = MOX::game_type;
        if (!client) {
            NETTURN::Net_Next_Turn();
            if (!convert) {
                DOCTEST_REQUIRE(MOX::game_type == multiplayer_type);
                NETTURN::Net_Next_Turn();
            }
        } else {
            // Make the client take its normal receive path using a valid no-change delta.
            NETMOX::net.local_net_id = 200;
            NETMOX::net.sync_state = NET_SYNC_STATE_NEXT_TURN_READY;
            char waiting_message[] = "Waiting";
            NETTURN::waiting_message_fmt = waiting_message;
            MULTPLAY::g_chat_box_input_loop_hook = [] {
                NETMOX::net.sync_state = NET_SYNC_STATE_WAITING_FOR_GAME_DATA;
                allocate::Push_Block(MOX::screen_seg);
                auto* delta = static_cast<uint8_t*>(allocate::Allocate_Next_Block(MOX::screen_seg, payload_capacity));
                auto& snapshot = NETMOX::net_snapshot_cache;
                NETMOX::net.total_bytes_received = NETMOX::Make_Difference_Buffer(
                    &snapshot, snapshot.buffer, snapshot.buffer_size, delta, payload_capacity);
                NETMOX::net.network_data_ptr = delta;
                MULTPLAY::g_chat_box_input_loop_hook = nullptr;
            };
            NETTURN::Client_Next_Turn();
        }
        DOCTEST_CHECK(MOX::game_type == (convert ? 0 : multiplayer_type));
        DOCTEST_CHECK(TEST_NETTURN_DROP::uninit_calls == (convert ? 1 : 0));
        DOCTEST_CHECK(NETMOX::net.sync_state == NET_SYNC_STATE_READY);
    }
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

DOCTEST_TEST_CASE_FIXTURE(Network_Flags_Fixture_, "the turn host decides whether to autosave after a player disconnects") {
    using namespace TEST_NETTURN_DROP;
    struct Drop_Test_State {
        bool silent = MULTPLAY::silent_player_drop;
        int16_t screen = MOX::current_screen;
        int16_t return_screen = MOX::return_screen;
        int8_t previous_screen = MOX::previous_screen;
        uint16_t full_draw = MOX::FULL_DRAW_SCREEN_FLAG;
        s_save_game_description description = MOX::save_game_description[9];

        ~Drop_Test_State() {
            MULTPLAY::silent_player_drop = silent;
            MOX::current_screen = screen;
            MOX::return_screen = return_screen;
            MOX::previous_screen = previous_screen;
            MOX::FULL_DRAW_SCREEN_FLAG = full_draw;
            MOX::save_game_description[9] = description;
            NETMOX::Clear_Pending_Player_Drops();
        }
    } state;

    MOX::NUM_PLAYERS = 2;
    MOX::current_screen = SCREEN_MAIN;
    NETMOX::net.net_host_id = players[0].network_player_id;
    NETMOX::net.sync_state = NET_SYNC_STATE_READY;
    NETMOX::net.num_connected_players = 2;
    players[1].objectives = PLAYER_OBJECTIVE_HUMAN;
    strlcpy(players[1].name, "Orion", sizeof(players[1].name));
    confirmation_result = 1;
    prompt.clear();
    saved_slot = -1;
    save_succeeds = true;
    uninit_calls = 0;
    MULTPLAY::silent_player_drop = false;

    bool leaves = false;
    bool saves = false;
    bool shows_prompt = true;
    DOCTEST_SUBCASE("Yes continues") {}
    DOCTEST_SUBCASE("No autosaves and leaves") {
        confirmation_result = 0;
        leaves = saves = true;
    }
    DOCTEST_SUBCASE("a failed autosave does not leave") {
        confirmation_result = 0;
        save_succeeds = false;
        saves = true;
    }
    DOCTEST_SUBCASE("silent mode skips the prompt") {
        MULTPLAY::silent_player_drop = true;
        shows_prompt = false;
    }
    DOCTEST_SUBCASE("a non-host does not prompt") {
        MOX::NUM_PLAYERS = 3;
        MOX::PLAYER_NUM = 2;
        NETMOX::net.net_host_id = players[2].network_player_id;
        players[0].objectives = PLAYER_OBJECTIVE_HUMAN;
        shows_prompt = false;
    }

    NETMOX::Handle_Network_Player_Disconnected(1);
    DOCTEST_CHECK(prompt.empty()); // Never display UI during packet processing.
    DOCTEST_CHECK(NETTURN::Confirm_Player_Drops() == leaves);
    DOCTEST_CHECK((!prompt.empty()) == shows_prompt);
    if (shows_prompt) {
        DOCTEST_CHECK(prompt.find("Player Orion has disconnected") != std::string::npos);
    }
    DOCTEST_CHECK(saved_slot == (saves ? 9 : -1));
    DOCTEST_CHECK(uninit_calls == (leaves ? 1 : 0));
    DOCTEST_CHECK(MOX::current_screen == (leaves ? SCREEN_MAIN_MENU : SCREEN_MAIN));
    DOCTEST_CHECK(NETMOX::Take_Pending_Player_Drop() == -1);
}

DOCTEST_TEST_CASE_FIXTURE(Network_Flags_Fixture_, "dropping the player currently supplying orders releases the host wait") {
    MOX::NUM_PLAYERS = 3;
    NETMOX::net.local_net_id = NETMOX::net.net_host_id = players[0].network_player_id;
    NETMOX::net.num_connected_players = 3;
    NETMOX::net.host_sync_counter = 1;
    NETMOX::net.host_active_player_id = 1;
    NETMOX::net.sync_state = NET_SYNC_STATE_HOST_RECEIVING_PLAYER_DATA;

    NETMOX::Handle_Network_Player_Disconnected(1);

    DOCTEST_CHECK(NETMOX::net.sync_state == NET_SYNC_STATE_HOST_WAITING_FOR_ORDERS);
    DOCTEST_CHECK(NETMOX::net.host_active_player_id == -1);
    DOCTEST_CHECK(NETMOX::net.host_orders_received_flags[1] == 1);
    DOCTEST_CHECK(NETMOX::net.host_sync_counter == 2);
}

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