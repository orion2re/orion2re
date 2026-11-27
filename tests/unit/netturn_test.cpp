#include "pch.h"
#include "../fixtures/star_roster.h"

#include <cstdlib>
#include <cstring>
#include <ostream>
#include <stdexcept>
#include <string_view>

#include "doctest/doctest.h"

#include "game/allocate.h"
#include "game/game_wire_version.h"
#include "game/mainscr.h"
#include "game/netmox.h"
#include "game/netturn.h"
#include "../fixtures/game_state.h"
#include "game/player_network_serialization.h"

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
    extern int (*confirm_hook)(const char*);
    extern void (*turn_hook)();
    extern std::string prompt;
    extern int16_t saved_slot;
    extern int ended_for_player;
    extern uint8_t saved_objectives[MAX_PLAYERS];
    extern int32_t saved_stardate;
    extern bool save_succeeds;
    extern int uninit_calls;
}

namespace {
    struct Network_Flags_Fixture_ {
        s_network saved_network = NETMOX::net;
        decltype(game::current_state.players) saved_players = game::current_state.players;
        int16_t saved_player_count = game::current_state.player_count;
        const game::id_player saved_player_num = MOX::local_player;
        decltype(MOX::game_type) saved_game_type = MOX::game_type;
        uint8_t saved_update_counter = NETMOX::next_update_player_counter;
        std::array<s_player, MAX_PLAYERS>& players = game::current_state.players;

        Network_Flags_Fixture_() {
            NETMOX::net = {};
            NETMOX::net.is_network_active = 1;
            NETMOX::net.net_host_id = 200;
            NETMOX::net.diplomacy_initiator_id = game::NO_PLAYER;
            players = ::player_roster::Make_Roster();
            game::current_state.player_count = MAX_PLAYERS;
            MOX::local_player = game::id_player{0};
            MOX::game_type = 2;
            NETMOX::next_update_player_counter = 0;
            for (int i = 0; i < MAX_PLAYERS; ++i) {
                players[i].network_player_id = 100 + i;
            }
        }

        ~Network_Flags_Fixture_() {
            NETMOX::net = saved_network;
            game::current_state.players = saved_players;
            game::current_state.player_count = saved_player_count;
            MOX::local_player = saved_player_num;
            MOX::game_type = saved_game_type;
            NETMOX::next_update_player_counter = saved_update_counter;
        }
    };

#ifdef TEST_REAL_MAIN_RECEIVE
    DOCTEST_TEST_CASE_FIXTURE(Network_Flags_Fixture_, "turn readiness requires a roster participant") {
        game::current_state.player_count = 2;
        MAINSCR::Main_Receive_Message(game::NO_PLAYER, 200, NET_PACKET_TURN_END_REQUEST, 0, nullptr);
        MAINSCR::Main_Receive_Message(game::id_player{2}, 102, NET_PACKET_TURN_END_REQUEST, 0, nullptr);
        DOCTEST_CHECK(NETMOX::net.player_is_active_flags[2] == 0);
        MAINSCR::Main_Receive_Message(game::id_player{1}, 101, NET_PACKET_TURN_END_REQUEST, 0, nullptr);
        DOCTEST_CHECK(NETMOX::net.player_is_active_flags[1] == 1);
    }

    DOCTEST_TEST_CASE_FIXTURE(Network_Flags_Fixture_, "host does not count a rejected orders packet") {
        const auto saved_screen = MOX::screen_seg;
        const auto saved_hook = MULTPLAY::g_chat_box_input_loop_hook;
        MOX::screen_seg = static_cast<s_animation_header*>(allocate::Allocate_Space(1024));
        game::current_state.player_count = 2;
        players[0].objectives = PLAYER_OBJECTIVE_HUMAN;
        players[1].objectives = PLAYER_OBJECTIVE_HUMAN;
        NETMOX::net.local_net_id = NETMOX::net.net_host_id = players[0].network_player_id;
        NETMOX::net.host_active_player_id = game::NO_PLAYER;
        NETMOX::net.players_hit_next_turn[1] = 1;
        NETMOX::net.player_is_active_flags[1] = 1;
        MULTPLAY::g_chat_box_input_loop_hook = [] {
            if (NETMOX::net.sync_state != NET_SYNC_STATE_HOST_RECEIVING_PLAYER_DATA) return;
            uint8_t truncated = 0;
            MAINSCR::Main_Receive_Message(game::id_player{0}, 100, NET_PACKET_GAME_ORDERS_PAYLOAD, 1, &truncated);
            DOCTEST_CHECK((NETMOX::net.network_data_ptr == nullptr));
            MAINSCR::Main_Receive_Message(game::id_player{1}, 101, NET_PACKET_GAME_ORDERS_PAYLOAD, 1, &truncated);
            DOCTEST_CHECK(NETMOX::net.host_sync_counter == 1);
        };

        DOCTEST_CHECK_THROWS_WITH(NETTURN::Host_Next_Turn(), "Invalid multiplayer game orders; turn not advanced.");
        DOCTEST_CHECK(NETMOX::net.host_sync_counter == 1);
        DOCTEST_CHECK(NETMOX::net.host_orders_received_flags[1] == 0);
        DOCTEST_CHECK((NETMOX::net.network_data_ptr == nullptr));
        DOCTEST_CHECK((NETMOX::net.total_bytes_received == 0));
        std::free(reinterpret_cast<s_alloc_header*>(MOX::screen_seg) - 1);
        MOX::screen_seg = saved_screen;
        MULTPLAY::g_chat_box_input_loop_hook = saved_hook;
    }

    DOCTEST_TEST_CASE_FIXTURE(Network_Flags_Fixture_, "new and loaded games check the wire version before applying setup") {
        const auto check = [](auto& packet, uint16_t kind) {
            DOCTEST_CAPTURE(kind);
            const auto receive = [&] {
                MAINSCR::Main_Receive_Message(game::id_player{1}, 101, kind, sizeof(packet), &packet);
            };

            std::strcpy(packet.version_string, "Aug 15 2026"); // Legacy build-date token.
            DOCTEST_CHECK_THROWS_WITH(receive(), "Incompatible multiplayer protocol version");

            std::strcpy(packet.version_string, "orion2re-keyed/6"); // Interim refactor version.
            DOCTEST_CHECK_THROWS_WITH(receive(), "Incompatible multiplayer protocol version");

            NETPROTO::Write_Setup_Version(packet.version_string);
            std::memset(packet.version_string + sizeof(NETPROTO::GAME_WIRE_VERSION) - 1,
                'x', sizeof(packet.version_string) - sizeof(NETPROTO::GAME_WIRE_VERSION) + 1);
            DOCTEST_CHECK_THROWS_WITH(receive(), "Incompatible multiplayer protocol version");

            NETPROTO::Write_Setup_Version(packet.version_string);
            DOCTEST_CHECK(std::string_view{packet.version_string} == "orion2re-keyed/1");
            // A matching version reaches configuration validation. No config follows this prefix.
            DOCTEST_CHECK_THROWS_WITH(receive(), "network configuration packet missing");
        };

        s_net_pack_start_game start{};
        s_load_net_screen_packet load{};
        check(start, NET_PACKET_START_GAME);
        check(load, NET_PACKET_LOAD_GAME_START);
    }

    DOCTEST_TEST_CASE_FIXTURE(Network_Flags_Fixture_, "invalid player setup fails instead of leaving the host waiting") {
        NETMOX::net.sync_state = NET_SYNC_STATE_HOST_WAITING_FOR_PLAYER_SETUP;
        s_player player{};
        player.network_player_id = 101;

        // Older builds sent an untagged player record.
        DOCTEST_CHECK_THROWS_WITH(MAINSCR::Main_Receive_Message(
            game::id_player{1}, 101, NET_PACKET_PLAYER_SETUP, sizeof(player), &player),
            "Invalid multiplayer player setup packet. Make sure all players use compatible game versions.");
        DOCTEST_CHECK(NETMOX::net.player_connected_flags[1] == 0);

        auto packet = NETPROTO::Encode_Player(player);
        MAINSCR::Main_Receive_Message(game::id_player{1}, 101, NET_PACKET_PLAYER_SETUP, sizeof(packet), &packet);
        DOCTEST_CHECK(NETMOX::net.player_connected_flags[1] == 1);
    }

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
        game::current_state.player_count = 1;
        players[0].objectives = PLAYER_OBJECTIVE_HUMAN;
        players[0].network_player_id = 44;
        players[0].surrender_to = legacy_world::Decode_Player_Slot(-1);
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
            game::current_state.players.at(MOX::local_player.id).eliminated = 1;
            MULTPLAY::g_chat_box_input_loop_hook = nullptr;
        }
    }
}

DOCTEST_TEST_SUITE("netturn host selection") {

DOCTEST_TEST_CASE("election reset clears all order flags without clearing diplomacy requests") {
    Network_Flags_Fixture_ fixture;
    NETMOX::net.human_diplomacy_request_mask = game::player_bitmap::From_Byte(0xA5);
    NETMOX::net.field130_0x1a4 = 0x5A;
    std::memset(NETMOX::net.host_orders_received_flags, 1, sizeof(NETMOX::net.host_orders_received_flags));

    NETTURN::Net_Init_Host_Election();

    DOCTEST_CHECK(NETMOX::net.human_diplomacy_request_mask == game::player_bitmap::From_Byte(0xA5));
    DOCTEST_CHECK(NETMOX::net.field130_0x1a4 == 0x5A);
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        DOCTEST_CHECK(NETMOX::net.host_orders_received_flags[i] == 0);
    }
}

DOCTEST_TEST_CASE("diplomacy request selection leaves every player order flag untouched") {
    Network_Flags_Fixture_ fixture;
    NETMOX::net.human_diplomacy_request_mask = game::player_bitmap::Only_Slot(MAX_PLAYERS - 1);
    std::memset(NETMOX::net.host_orders_received_flags, 1, sizeof(NETMOX::net.host_orders_received_flags));

    DOCTEST_CHECK(NETMOX::Human_Requesting_Diplomacy() == game::id_player{MAX_PLAYERS - 1});
    DOCTEST_CHECK(NETMOX::net.human_diplomacy_request_mask.none());
    DOCTEST_CHECK(NETMOX::Human_Requesting_Diplomacy() == game::NO_PLAYER);
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

    DOCTEST_CHECK(NETTURN::Determine_Lowest_Active_Host_Player(players, 3) == &players[1]);
}

DOCTEST_TEST_CASE("skips active slots without a valid network id") {
    s_player players[3]{};
    players[0].objectives = PLAYER_OBJECTIVE_HUMAN;
    players[0].network_player_id = -1;
    players[1].objectives = PLAYER_OBJECTIVE_HUMAN;
    players[1].network_player_id = -1;
    players[2].objectives = PLAYER_OBJECTIVE_HUMAN;
    players[2].network_player_id = 42;

    DOCTEST_CHECK(NETTURN::Determine_Lowest_Active_Host_Player(players, 3) == &players[2]);
}

DOCTEST_TEST_CASE("ignores inactive or eliminated slots even if they have network ids") {
    s_player players[3]{};
    players[0].objectives = 12;
    players[0].network_player_id = 7;
    players[1].objectives = 99;
    players[1].network_player_id = 8;
    players[2].objectives = PLAYER_OBJECTIVE_HUMAN;
    players[2].network_player_id = 9;

    DOCTEST_CHECK(NETTURN::Determine_Lowest_Active_Host_Player(players, 3) == &players[2]);
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

    DOCTEST_CHECK(NETTURN::Determine_Lowest_Active_Host_Player(players, 3) == &players[1]);
}

DOCTEST_TEST_CASE("returns no host when no active player has a valid network id") {
    s_player players[3]{};
    players[0].objectives = PLAYER_OBJECTIVE_DIPLOMATIC;
    players[0].network_player_id = -1;
    players[1].objectives = PLAYER_OBJECTIVE_HUMAN;
    players[1].network_player_id = -1;
    players[2].objectives = PLAYER_OBJECTIVE_DIPLOMATIC;
    players[2].network_player_id = 12;

    DOCTEST_CHECK(NETTURN::Determine_Lowest_Active_Host_Player(players, 3) == nullptr);
}

DOCTEST_TEST_CASE("returns no host for empty input") {
    DOCTEST_CHECK(NETTURN::Determine_Lowest_Active_Host_Player(nullptr, 0) == nullptr);
}

DOCTEST_TEST_CASE_FIXTURE(Network_Flags_Fixture_, "No during resolution saves the checkpoint with the human empire") {
    using namespace TEST_NETTURN_DROP;
    using test::s_saved_value;
    s_saved_value silent(MULTPLAY::silent_player_drop);
    s_saved_value confirm(confirm_hook);
    s_saved_value simulate(turn_hook);
    s_saved_value save_ok(save_succeeds);
    s_saved_value stardate(MOX::stardate);
    s_saved_value chat_hook(MULTPLAY::g_chat_box_input_loop_hook);
    MULTPLAY::silent_player_drop = false;
    save_succeeds = true;
    game::current_state.player_count = 2;
    players[0].objectives = PLAYER_OBJECTIVE_HUMAN;
    players[1].objectives = PLAYER_OBJECTIVE_HUMAN;
    NETMOX::net.num_connected_players = 2;
    NETMOX::net.local_net_id = NETMOX::net.net_host_id = players[0].network_player_id;
    NETMOX::net.host_active_player_id = game::NO_PLAYER;
    MULTPLAY::g_chat_box_input_loop_hook = [] { NETMOX::net.sync_state = NET_SYNC_STATE_HOST_BEGIN_TURN; };
    confirm_hook = [](const char*) { return 0; };
    turn_hook = [] {
        const auto& combat = NETMOX::net.player_net_info[0];
        DOCTEST_CHECK(combat.combat_destination == game::s_star_destination{});
        DOCTEST_CHECK(combat.combat_planet_id.get() == game::NO_PLANET);
        DOCTEST_CHECK(combat.combat_defender_id.get() == game::NO_OWNER);
        DOCTEST_CHECK(combat.combat_target_player_id.get() == game::NO_OWNER);
        DOCTEST_CHECK(combat.diplomacy_target_id.get() == game::NO_PLAYER);
        ++MOX::stardate;
        NETMOX::Handle_Network_Player_Disconnected(game::id_player{1});
    };
    DOCTEST_CHECK_THROWS_AS(NETTURN::Host_Next_Turn(), netcode::session_lost);
    DOCTEST_CHECK(saved_stardate == stardate.saved);
    DOCTEST_CHECK(saved_objectives[1] == PLAYER_OBJECTIVE_HUMAN);
    DOCTEST_CHECK(players[1].objectives == PLAYER_OBJECTIVE_HUMAN);
}

DOCTEST_TEST_CASE("client next turn preserves a payload that arrives before the final wait loop") {
    s_player players[2]{};
    players[0].network_player_id = 176;
    players[0].objectives = PLAYER_OBJECTIVE_HUMAN;
    players[1].network_player_id = 174;
    players[1].objectives = PLAYER_OBJECTIVE_HUMAN;

    for (uint32_t i = 0; i < std::size(players); ++i) {
        game::current_state.players[i] = players[i];
        game::current_state.players[i].id = game::id_player{i};
    }
    MOX::local_player = game::id_player{1};
    game::current_state.player_count = 2;
    test_stars::Resize(0);
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
        game::id_player{1}, 174, NET_PACKET_GAME_DATA_PAYLOAD, sizeof(payload), const_cast<uint8_t*>(payload));

    DOCTEST_CHECK((NETMOX::net.network_data_ptr == &retained_payload));
    DOCTEST_CHECK((NETMOX::net.total_bytes_received == 1));
    DOCTEST_CHECK((NETMOX::net.host_election_winner_id == 91));
    DOCTEST_CHECK(allocate::mark_stack_pos[0] == 0);

    MAINSCR::Main_Receive_Message(
        game::id_player{1}, 176, NET_PACKET_GAME_DATA_PAYLOAD, sizeof(payload), const_cast<uint8_t*>(payload));

    DOCTEST_REQUIRE((NETMOX::net.network_data_ptr != nullptr));
    DOCTEST_CHECK(std::memcmp(
                      NETMOX::net.network_data_ptr, payload, sizeof(payload)) ==
        0);
    DOCTEST_CHECK((NETMOX::net.total_bytes_received == sizeof(payload)));
    DOCTEST_CHECK((NETMOX::net.host_election_winner_id == 176));
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
    test::s_saved_value silent(MULTPLAY::silent_player_drop);
    test::s_saved_value confirm(confirm_hook);
    test::s_saved_value save_ok(save_succeeds);

    game::current_state.player_count = 2;
    NETMOX::net.net_host_id = players[0].network_player_id;
    NETMOX::net.sync_state = NET_SYNC_STATE_READY;
    NETMOX::net.num_connected_players = 2;
    players[1].objectives = PLAYER_OBJECTIVE_HUMAN;
    strlcpy(players[1].name, "Orion", sizeof(players[1].name));
    prompt.clear();
    saved_slot = -1;
    ended_for_player = -1;
    save_succeeds = true;
    MULTPLAY::silent_player_drop = false;

    bool leaves = false;
    bool saves = false;
    bool shows_prompt = true;
    bool takes_over = true;
    confirm_hook = [](const char*) -> int {
        DOCTEST_CHECK(game::current_state.players[1].objectives == PLAYER_OBJECTIVE_HUMAN);
        return 1;
    };
    DOCTEST_SUBCASE("Yes continues") {}
    DOCTEST_SUBCASE("No autosaves and leaves") {
        confirm_hook = [](const char*) -> int {
            DOCTEST_CHECK(game::current_state.players[1].objectives == PLAYER_OBJECTIVE_HUMAN);
            return 0;
        };
        leaves = saves = true;
        takes_over = false;
    }
    DOCTEST_SUBCASE("a failed autosave does not leave") {
        save_succeeds = false;
        saves = true;
        confirm_hook = [](const char* message) -> int {
            DOCTEST_CHECK(game::current_state.players[1].objectives == PLAYER_OBJECTIVE_HUMAN);
            if (saved_slot == 9) {
                DOCTEST_CHECK(std::strstr(message, "Autosave failed") != nullptr);
                return 1;
            }
            return 0;
        };
    }
    DOCTEST_SUBCASE("silent mode skips the prompt") {
        MULTPLAY::silent_player_drop = true;
        shows_prompt = false;
    }
    DOCTEST_SUBCASE("a non-host does not prompt") {
        game::current_state.player_count = 3;
        MOX::local_player = game::id_player{2};
        NETMOX::net.net_host_id = players[2].network_player_id;
        players[0].objectives = PLAYER_OBJECTIVE_HUMAN;
        shows_prompt = false;
        takes_over = false;
    }

    bool saved_and_left = false;
    try {
        NETMOX::Handle_Network_Player_Disconnected(game::id_player{1});
    } catch (const netcode::session_lost& ended) {
        DOCTEST_CHECK(ended.reason == netcode::e_session_end_reason::local_save);
        saved_and_left = true;
    }
    DOCTEST_CHECK(saved_and_left == leaves);
    DOCTEST_CHECK((!prompt.empty()) == shows_prompt);
    if (shows_prompt) {
        DOCTEST_CHECK(prompt.find("Player Orion has disconnected") != std::string::npos);
    }
    DOCTEST_CHECK(saved_slot == (saves ? 9 : -1));
    DOCTEST_CHECK(ended_for_player == (leaves ? 1 : -1));
    if (saves) {
        DOCTEST_CHECK(saved_objectives[1] == PLAYER_OBJECTIVE_HUMAN);
    }
    DOCTEST_CHECK(players[1].objectives == (takes_over ? PLAYER_OBJECTIVE_DIPLOMATIC : PLAYER_OBJECTIVE_HUMAN));
    DOCTEST_CHECK(static_cast<int>(players[1].network_player_id) == (takes_over ? -1 : 101));
}

DOCTEST_TEST_CASE_FIXTURE(Network_Flags_Fixture_, "dropping the player currently supplying orders releases the host wait") {
    game::current_state.player_count = 3;
    NETMOX::net.local_net_id = NETMOX::net.net_host_id = players[0].network_player_id;
    NETMOX::net.num_connected_players = 3;
    NETMOX::net.host_sync_counter = 1;
    NETMOX::net.host_active_player_id = game::id_player{1};
    NETMOX::net.sync_state = NET_SYNC_STATE_HOST_RECEIVING_PLAYER_DATA;

    NETMOX::Handle_Network_Player_Disconnected(game::id_player{1});

    DOCTEST_CHECK(NETMOX::net.sync_state == NET_SYNC_STATE_HOST_WAITING_FOR_ORDERS);
    DOCTEST_CHECK(NETMOX::net.host_active_player_id.get() == game::NO_PLAYER);
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

    for (uint32_t i = 0; i < std::size(players); ++i) {
        game::current_state.players[i] = players[i];
        game::current_state.players[i].id = game::id_player{i};
    }
    MOX::local_player = game::id_player{0};
    game::current_state.player_count = 3;
    test_stars::Resize(0);

    NETMOX::net = {};
    NETMOX::net.is_network_active = 1;
    NETMOX::net.net_host_id = 101;
    NETMOX::net.local_net_id = 202;
    NETMOX::net.host_election_id = 101;
    NETMOX::net.host_election_winner_id = 101;
    NETMOX::net.num_connected_players = 3;

    NETMOX::Handle_Network_Player_Disconnected(game::id_player{1});

    DOCTEST_CHECK((game::current_state.players[1].network_player_id == -1));
    DOCTEST_CHECK(NETMOX::net.num_connected_players == 2);
    DOCTEST_CHECK(NETMOX::net.players_hit_next_turn[1] == 1);
    DOCTEST_CHECK(NETMOX::net.host_election_id == 202);
    DOCTEST_CHECK((NETMOX::net.host_election_winner_id == 202));
    DOCTEST_CHECK(NETMOX::net.local_net_id == 202);
}

DOCTEST_TEST_CASE("local player disconnect is ignored") {
    s_player players[2]{};

    players[0].network_player_id = 202;
    players[0].objectives = PLAYER_OBJECTIVE_HUMAN;
    players[1].network_player_id = 101;
    players[1].objectives = PLAYER_OBJECTIVE_HUMAN;

    for (uint32_t i = 0; i < std::size(players); ++i) {
        game::current_state.players[i] = players[i];
        game::current_state.players[i].id = game::id_player{i};
    }
    MOX::local_player = game::id_player{0};
    game::current_state.player_count = 2;
    test_stars::Resize(0);

    NETMOX::net = {};
    NETMOX::net.is_network_active = 1;
    NETMOX::net.net_host_id = 101;
    NETMOX::net.local_net_id = 202;
    NETMOX::net.num_connected_players = 2;

    NETMOX::Handle_Network_Player_Disconnected(game::id_player{0});

    DOCTEST_CHECK((game::current_state.players[0].network_player_id == 202));
    DOCTEST_CHECK(NETMOX::net.num_connected_players == 2);
}

}
