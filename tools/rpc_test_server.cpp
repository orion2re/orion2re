#include <iostream>
#include <limits>
#include <string>
#include <atomic>
#include <chrono>
#include <deque>
#include <mutex>
#include <thread>

#include "rpc/rpc_service.h"
#include "rpc/rpc_game.h"

int main() {
    rpc::s_service service;
    rpc::s_service_config config;
    config.port = 0;
    config.key = "rpc-test-key-not-a-real-secret";
    rpc::s_bridge bridge;
    config.bridge = &bridge;
    std::string diagnostic;
    if (!rpc::Start_Service(&service, &config, &diagnostic)) {
        std::cerr << diagnostic << '\n';
        return 1;
    }
    std::cout << rpc::Service_Port(&service) << std::endl;
    std::atomic<bool> running = true;
    std::mutex commands_mutex;
    std::deque<std::string> commands;
    std::thread game([&] {
        rpc::s_game_store store;
        rpc::Initialize_Game_Store(&store, &bridge);
        rpc::s_map_observation map;
        map.viewer = 0;
        map.turn = 35000;
        map.systems = {{10, 20, true, true, "Home"}, {30, 40, false, false, "hidden"}, {50, 60, true, true, "Other"}};
        map.nebulas = {{70, 80}};
        while (running) {
            std::string command;
            {
                std::lock_guard lock(commands_mutex);
                if (!commands.empty()) {
                    command = std::move(commands.front());
                    commands.pop_front();
                }
            }
            if (command == "world") {
                rpc::Begin_Game_Reset(&store);
                rpc::Commit_Game_Reset(&store);
                rpc::Capture_Map(&store, &map);
            } else if (command == "switch") {
                rpc::Revoke_Game_View(&store);
                map.viewer = 1 - map.viewer;
                rpc::Capture_Map(&store, &map);
            } else if (command == "busy")
                rpc::Pause_Game_Queries(&store);
            else if (command == "resume")
                rpc::Resume_Game_Queries(&store);
            else if (command == "hidden") {
                map.systems[1].name = "changed hidden world";
                if (!map.ships.empty()) map.ships.back().name = "changed hidden ship name";
                rpc::Capture_Map(&store, &map);
            } else if (command == "ships") {
                map.ships_captured = true;
                map.ships = {{0, 0, true, "Own ship"}, {30, 0, false, "hidden enemy name"}, {60, 0, false, "secret"}};
                rpc::Capture_Map(&store, &map);
            } else if (command == "ship-move") {
                if (!map.ships.empty()) ++map.ships[0].x;
                map.ships_captured = true;
                rpc::Capture_Map(&store, &map);
            } else if (command == "map-only") {
                map.ships_captured = false;
                map.systems[0].name = "Later map";
                rpc::Capture_Map(&store, &map);
            } else if (command == "advance") {
                ++map.turn;
                rpc::Capture_Map(&store, &map);
            } else if (command == "burst") {
                for (int index = 0; index < 100; ++index) {
                    map.systems[0].name = "Home-" + std::to_string(index);
                    rpc::Capture_Map(&store, &map);
                    rpc::Pump_Game_Queries(&store);
                }
            } else if (command == "revoke")
                rpc::Revoke_Bridge(&bridge);
            else if (command == "publish" || command == "publish-large" || command == "overflow") {
                rpc::s_request request;
                request.id_json = "99";
                request.method = "fixture";
                request.params_json = "{}";
                if (rpc::Submit_Work(&bridge, 1, 1, &request, std::numeric_limits<uint64_t>::max()) == 0) {
                    rpc::s_work work;
                    if (rpc::Take_Work(&bridge, 0, &work)) {
                        const size_t size = command == "publish-large" ? 524288 : 32768;
                        const std::string result = command != "overflow" ? rpc::Result_Response(&request, "{\"fixture\":" + rpc::Json_String(std::string(size, 'x')) + "}") : std::string(1048577, 'x');
                        rpc::Complete_Work(&bridge, &work, &result);
                    }
                }
            }
            if (!command.empty()) std::cout << "done" << std::endl;
            rpc::Pump_Game_Queries(&store);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        rpc::Shutdown_Game_Store(&store);
    });
    std::string command;
    while (std::getline(std::cin, command) && !command.empty() && command != "quit") {
        std::lock_guard lock(commands_mutex);
        if (commands.size() < 16) commands.push_back(command);
    }
    running = false;
    game.join();
    rpc::Stop_Service(&service);
    return 0;
}
