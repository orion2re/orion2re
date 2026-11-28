#include "pch.h"

#include "doctest/doctest.h"
#include "game/game_wire_version.h"
#include "lua/lua_network.h"
#include "network/router_service.h"
#include "../fixtures/game_state.h"

#include <future>
#include <thread>

namespace {
    using namespace network::router;

    struct Test_Router {
        s_router_service service;
        std::thread worker;

        Test_Router() {
            s_router_config config;
            Router_Copy_String(config.listen_host, sizeof(config.listen_host), "127.0.0.1");
            config.tcp_port = 0;
            config.enable_discovery = false;
            config.enable_signal_handlers = false;
            Router_Service_Init(&service, &config);
            worker = std::thread([this] { Router_Service_Run(&service); });
        }

        ~Test_Router() { Stop(); }

        void Stop() {
            if (worker.joinable()) {
                Router_Service_Request_Stop(&service);
                worker.join();
            }
        }
    };

    template <typename Predicate>
    bool Wait_For(Predicate ready) {
        const uint64_t start = SDL_GetTicks();
        do {
            if (ready()) return true;
            SDL_Delay(1);
        } while (SDL_GetTicks() - start < 2000);
        return false;
    }

    void Join_Test_Session(Test_Router& router, netadapter::s_adapter* host, int max_players = 2) {
        DOCTEST_REQUIRE(Wait_For([&] { return Router_Service_Bound_Tcp_Port(&router.service) != 0; }));
        const uint16_t port = Router_Service_Bound_Tcp_Port(&router.service);
        netadapter::s_config host_config;
        host_config.tcp_port = port;
        Router_Copy_String(host_config.build_string, sizeof(host_config.build_string), NETPROTO::GAME_WIRE_VERSION);
        netadapter::Init(host, &host_config);
        DOCTEST_REQUIRE(netadapter::Connect(host));
        DOCTEST_REQUIRE(Wait_For([&] {
            netadapter::Pump(host);
            return netadapter::Local_Network_Id(host) != 0;
        }));
        DOCTEST_REQUIRE(netadapter::Create_Game(host, "Disconnect test", max_players, "Host"));
        DOCTEST_REQUIRE(Wait_For([&] {
            netadapter::Pump(host);
            return netadapter::Session_Id(host) != 0;
        }));

        snprintf(MULTPLAY::router_endpoint, sizeof(MULTPLAY::router_endpoint), "127.0.0.1:%u", (unsigned)port);
        DOCTEST_REQUIRE(netcode::Net_Init(1, nullptr, nullptr));

        // Start at the admission decision; this test is about an already joined client.
        const orion2re::lua::s_lua_network_digest digest{};
        orion2re::lua::Set_Lua_Network_Offer_Context(1, &digest);
        uint8_t decision[1100];
        uint32_t decision_size;
        DOCTEST_REQUIRE(orion2re::lua::Write_Lua_Network_Setup_Decision(decision, sizeof(decision), true, {}, &decision_size));
        const uint32_t session_id = netadapter::Session_Id(host);
        auto admission = std::async(std::launch::async, [&] {
            return Wait_For([&] {
                netadapter::Pump(host);
                netadapter::s_event event;
                while (netadapter::Poll_Event(host, &event)) {
                    if (event.kind == netadapter::EVENT_MEMBER_JOINED) {
                        return netadapter::Send_Game_Message(host, event.network_id,
                            NET_PACKET_LUA_SETUP_DECISION, decision, decision_size);
                    }
                }
                return false;
            });
        });
        const bool joined = netcode::Net_Join_Game(session_id, "", "Client") != 0;
        DOCTEST_REQUIRE(admission.get());
        DOCTEST_REQUIRE(joined);
    }

    void Join_Additional_Player(Test_Router& router, netadapter::s_adapter* peer, uint32_t session_id) {
        netadapter::s_config config;
        config.tcp_port = Router_Service_Bound_Tcp_Port(&router.service);
        Router_Copy_String(config.build_string, sizeof(config.build_string), NETPROTO::GAME_WIRE_VERSION);
        netadapter::Init(peer, &config);
        DOCTEST_REQUIRE(netadapter::Connect(peer));
        DOCTEST_REQUIRE(Wait_For([&] {
            netadapter::Pump(peer);
            return netadapter::Local_Network_Id(peer) != 0;
        }));
        DOCTEST_REQUIRE(netadapter::Refresh_Game_List(peer));
        DOCTEST_REQUIRE(Wait_For([&] {
            netadapter::Pump(peer);
            return netadapter::Game_Session_Id(peer, 0) == session_id;
        }));
        DOCTEST_REQUIRE(netadapter::Join_Game(peer, session_id, "Peer"));
        DOCTEST_REQUIRE(Wait_For([&] {
            netadapter::Pump(peer);
            return netadapter::Session_Id(peer) == session_id;
        }));
    }

    bool Poll_Game_Ended(netcode::e_session_end_reason expected_reason) {
        try {
            netcode::Net_Update();
            return false;
        } catch (const netcode::session_lost& ended) {
            DOCTEST_CHECK(ended.reason == expected_reason);
            return true;
        }
    }
}

DOCTEST_TEST_CASE("an established client reports router loss and can join another session") {
    struct Client_Cleanup {
        ~Client_Cleanup() { netcode::Net_Uninit(); }
    } cleanup;

    Test_Router router;
    netadapter::s_adapter host;
    Join_Test_Session(router, &host);
    router.Stop();
    DOCTEST_CHECK_THROWS_AS(Wait_For([] {
        netcode::Net_Update();
        return false;
    }), netcode::session_lost);
    netcode::Net_Uninit();
    DOCTEST_CHECK(netcode::inComms == 0);

    Test_Router next_router;
    netadapter::s_adapter next_host;
    Join_Test_Session(next_router, &next_host);
    const uint32_t owner = netadapter::Local_Network_Id(&next_host);
    DOCTEST_CHECK(netcode::Net_Send_Message(owner, NET_PACKET_CHAT, nullptr, 0) != 0);
    netcode::Net_Uninit();
    DOCTEST_CHECK_NOTHROW(netcode::Net_Update());
}

DOCTEST_TEST_CASE("a saved match ends for remaining players only on notice from the recovery host") {
    struct Client_Cleanup {
        ~Client_Cleanup() { netcode::Net_Uninit(); }
    } cleanup;
    test::s_game_state world;
    test::s_saved_value player_num(MOX::local_player);
    test::s_saved_value game_type(MOX::game_type);
    test::s_saved_value network(NETMOX::net);

    Test_Router router;
    netadapter::s_adapter owner, peer_a, peer_b;
    Join_Test_Session(router, &owner, 4);
    const uint32_t session_id = netadapter::Session_Id(&owner);
    Join_Additional_Player(router, &peer_a, session_id);
    Join_Additional_Player(router, &peer_b, session_id);
    const uint32_t owner_id = netadapter::Local_Network_Id(&owner);
    const uint32_t peer_a_id = netadapter::Local_Network_Id(&peer_a);
    const uint32_t peer_b_id = netadapter::Local_Network_Id(&peer_b);
    const uint32_t local_id = netcode::Net_Host_ID();
    DOCTEST_REQUIRE(Wait_For([&] {
        netcode::Net_Update();
        netcode::Net_Accept_Member(peer_a_id);
        netcode::Net_Accept_Member(peer_b_id);
        return netcode::Net_Get_Users(nullptr) == 4;
    }));

    auto& players = game::current_state.players;
    game::current_state.player_count = 4;
    MOX::game_type = 2;
    NETMOX::net = {};
    NETMOX::net.is_network_active = 1;
    for (int i = 0; i < game::current_state.player_count; ++i) {
        players[i].objectives = PLAYER_OBJECTIVE_HUMAN;
    }
    players[0].network_player_id = static_cast<int16_t>(owner_id);
    players[3].network_player_id = static_cast<int16_t>(peer_b_id);

    DOCTEST_SUBCASE("the coordinator sends to both remaining peers and leaves after routing") {
        MOX::local_player = game::id_player{1};
        players[1].network_player_id = static_cast<int16_t>(local_id);
        players[2].network_player_id = static_cast<int16_t>(peer_a_id);
        // Observe the loss before entering the in-game handler that would show the prompt.
        NETMOX::net.is_network_active = 0;
        netadapter::Shutdown(&owner);
        DOCTEST_REQUIRE(Wait_For([] { return netcode::Net_Session_Owner_ID() == 0; }));
        NETMOX::net.is_network_active = 1;

        netcode::Net_End_Game_After_Save(game::id_player{0});
        netcode::Net_Uninit();
        for (auto* peer : {&peer_a, &peer_b}) {
            netadapter::s_game_packet packet;
            DOCTEST_REQUIRE(Wait_For([&] {
                netadapter::Pump(peer);
                return !netadapter::Is_Session_Member(peer, local_id) && netadapter::Poll_Game_Packet(peer, &packet);
            }));
            DOCTEST_CHECK(packet.sender_network_id == local_id);
            DOCTEST_CHECK(packet.message_type == NET_PACKET_GAME_ENDED);
            DOCTEST_REQUIRE(packet.payload.size() == sizeof(uint16_t));
            SERIAL::Reader_ reader(packet.payload.data(), sizeof(uint16_t), "Invalid test notice");
            DOCTEST_CHECK(reader.Read_Uint16() == owner_id);
        }
    }

    DOCTEST_SUBCASE("clients ignore wrong senders and honor a notice delivered with the coordinator's leave") {
        MOX::local_player = game::id_player{2};
        players[1].network_player_id = static_cast<int16_t>(peer_a_id);
        players[2].network_player_id = static_cast<int16_t>(local_id);
        uint8_t payload[sizeof(uint16_t)];
        SERIAL::Writer_ writer(payload, sizeof(payload), "Invalid test notice size");
        writer.Write_Uint16(static_cast<uint16_t>(owner_id));
        const auto send_notice_and_marker = [&](netadapter::s_adapter* peer) {
            DOCTEST_REQUIRE(netadapter::Send_Game_Message(peer, local_id, NET_PACKET_GAME_ENDED, payload, sizeof(payload)));
            DOCTEST_REQUIRE(netadapter::Send_Game_Message(peer, local_id, NET_PACKET_CHAT, nullptr, 0));
        };
        // Use a marker on the same TCP connection to prove the rejected notice has been processed.
        static bool received_marker;
        netcode::recv_fn = [](int16_t, uint16_t type, uint32_t, uint8_t*) {
            if (type == NET_PACKET_CHAT) received_marker = true;
        };
        received_marker = false;
        send_notice_and_marker(&peer_a); // Cannot claim the still-connected owner has dropped.
        DOCTEST_REQUIRE(Wait_For([] { netcode::Net_Update(); return received_marker; }));

        netadapter::Shutdown(&owner);
        DOCTEST_REQUIRE(Wait_For([&] {
            netadapter::Pump(&peer_a);
            return !netadapter::Is_Session_Member(&peer_a, owner_id);
        }));
        received_marker = false;
        send_notice_and_marker(&peer_b); // Not the elected coordinator for this drop.
        DOCTEST_REQUIRE(Wait_For([] { netcode::Net_Update(); return received_marker; }));

        DOCTEST_REQUIRE(netadapter::Send_Game_Message(&peer_a, local_id, NET_PACKET_GAME_ENDED, payload, sizeof(payload)));
        DOCTEST_REQUIRE(netadapter::Leave_Game(&peer_a));
        DOCTEST_REQUIRE(Wait_For([&] {
            netadapter::Pump(&peer_a);
            return netadapter::Session_Id(&peer_a) == 0;
        }));
        DOCTEST_CHECK(Wait_For([] { return Poll_Game_Ended(netcode::e_session_end_reason::host_saved_and_ended); }));
    }
}
