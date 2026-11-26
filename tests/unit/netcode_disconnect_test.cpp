#include "pch.h"

#include "doctest/doctest.h"
#include "lua/lua_network.h"
#include "network/router_service.h"

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

    void Join_Test_Session(Test_Router& router, netadapter::s_adapter* host) {
        DOCTEST_REQUIRE(Wait_For([&] { return Router_Service_Bound_Tcp_Port(&router.service) != 0; }));
        const uint16_t port = Router_Service_Bound_Tcp_Port(&router.service);
        netadapter::s_config host_config;
        host_config.tcp_port = port;
        Router_Copy_String(host_config.build_string, sizeof(host_config.build_string), GAME_BUILD_DATE);
        netadapter::Init(host, &host_config);
        DOCTEST_REQUIRE(netadapter::Connect(host));
        DOCTEST_REQUIRE(Wait_For([&] {
            netadapter::Pump(host);
            return netadapter::Local_Network_Id(host) != 0;
        }));
        DOCTEST_REQUIRE(netadapter::Create_Game(host, "Disconnect test", 2, "Host"));
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
