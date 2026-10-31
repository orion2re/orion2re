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
        map.players_captured = true;
        rpc::s_player_observation own;
        own.slot = 0;
        own.visible = true;
        own.name = "Local empire";
        own.treasury = 50;
        own.hud = {-3, -4, 10, -2, 7, -1};
        rpc::s_player_observation contact;
        contact.slot = 1;
        contact.visible = true;
        contact.contacted = true;
        contact.color = 1;
        contact.name = "Known empire";
        contact.treasury = 1234567;
        rpc::s_player_observation hidden;
        hidden.slot = 2;
        hidden.name = "Hidden empire";
        hidden.color = 2;
        map.players = {own, contact, hidden};
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
                if (map.ships.size() >= 3) {
                    map.ships.back().destination_system = 99999;
                    map.ships.back().destination_id = "hidden destination";
                    map.ships.back().eta = -99999;
                }
                ++map.players[1].treasury;
                map.players[2].name = "different hidden empire";
                map.players[2].at_war_available = true;
                map.players[2].at_war = !map.players[2].at_war;
                if (map.colonies.size() >= 3) {
                    map.colonies.back().system = -99999;
                    map.colonies.back().population = -99999;
                }
                rpc::Capture_Map(&store, &map);
            } else if (command == "player-hud") {
                --map.players[0].treasury;
                --map.players[0].hud.income;
                rpc::Capture_Map(&store, &map);
            } else if (command == "contact") {
                map.players[2].visible = true;
                map.players[2].contacted = true;
                map.players[2].name = "New contact";
                rpc::Capture_Map(&store, &map);
            } else if (command == "player-forget") {
                map.players[1].visible = false;
                rpc::Capture_Map(&store, &map);
            } else if (command == "ships") {
                map.ships_captured = true;
                map.ships = {{0, 0, true, "Own ship"}, {30, 0, false, "hidden enemy name"}, {60, 0, false, "secret"}};
                rpc::Capture_Map(&store, &map);
            } else if (command == "ship-move") {
                if (!map.ships.empty()) ++map.ships[0].x;
                map.ships_captured = true;
                rpc::Capture_Map(&store, &map);
            } else if (command == "ship-owners") {
                if (map.ships.size() >= 3) {
                    map.ships[0].owner_slot = 0;
                    map.ships[1].owner_slot = 1;
                    map.ships[2].owner_slot = 2;
                }
                map.ships_captured = true;
                rpc::Capture_Map(&store, &map);
            } else if (command == "ship-routes") {
                if (map.ships.size() >= 3) {
                    map.ships[0].destination_available = rpc::SHIP_CURRENT;
                    map.ships[0].destination_system = 1;
                    map.ships[0].eta_available = rpc::SHIP_CURRENT;
                    map.ships[0].eta = 0;
                    map.ships[1].owned = true;
                    map.ships[1].name = "Stationary";
                    map.ships[1].destination_available = map.ships[1].eta_available = rpc::SHIP_KNOWN_EMPTY;
                    map.ships[2].destination_available = map.ships[2].eta_available = rpc::SHIP_CONTEXT_REQUIRED;
                }
                map.ships_captured = true;
                rpc::Capture_Map(&store, &map);
            } else if (command == "colonies") {
                map.colonies_captured = true;
                map.colonies = {{0, 0, 1, 0}, {0, 0, 5, 42}, {1, 99999, -1, 99999}};
                rpc::Capture_Map(&store, &map);
            } else if (command == "war") {
                map.players[1].at_war_available = true;
                map.players[1].at_war = !map.players[1].at_war;
                rpc::Capture_Map(&store, &map);
            } else if (command == "threats") {
                if (map.ships.size() >= 3) {
                    map.ships[1].owned = false;
                    map.ships[1].owner_slot = 1;
                    map.ships[1].destination_available = rpc::SHIP_CURRENT;
                    map.ships[1].destination_system = 0;
                    map.ships[1].eta_available = rpc::SHIP_CURRENT;
                    map.ships[1].eta = 2;
                    map.players[1].at_war_available = map.players[1].at_war = true;
                }
                map.ships_captured = true;
                rpc::Capture_Map(&store, &map);
            } else if (command == "colony-pop") {
                if (!map.colonies.empty()) ++map.colonies[0].population;
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
