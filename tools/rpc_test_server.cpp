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
        bool polling = true;
        size_t eta_ship = 0;
        bool contextual_route = false;
        while (running) {
            std::string command;
            std::string acknowledgment = "done";
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
                map.research = {};
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
                map.players[2].research_treaty_available = true;
                map.players[2].research_treaty = !map.players[2].research_treaty;
                map.players[2].trade_treaty_available = true;
                map.players[2].trade_treaty = !map.players[2].trade_treaty;
                if (map.colonies.size() >= 3) {
                    map.colonies.back().system = -99999;
                    map.colonies.back().population = -99999;
                    map.colonies.back().infantry = -99999;
                    map.colonies.back().armor = 99999;
                }
                rpc::Capture_Map(&store, &map);
            } else if (command == "research") {
                map.research_captured = true;
                map.research.mode = "researching";
                map.research.project = 0;
                map.research.application = 1;
                map.research.technologies = {{false, "Research field", true, 500}, {true, "Offered application A", true, 500}, {true, "Offered application B", true, 500}};
                map.research.options = {{0, 1, 450}, {0, 2, 450}, {0, -1, 450}};
                rpc::Capture_Map(&store, &map);
            } else if (command == "research-change") {
                if (!map.research.options.empty()) {
                    map.research.application = 2;
                    for (auto& option : map.research.options) option.remaining = 300;
                }
                if (!map.ships.empty()) map.ships[0].x = 123;
                rpc::Capture_Map(&store, &map);
            } else if (command == "research-clear") {
                map.research = {};
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
            } else if (command == "eta-ambient" || command == "eta-context" || command == "eta-foreign-ambient" || command == "eta-foreign-context" || command == "route-ambient" || command == "route-context" || command == "ordinary-departing-ambient" || command == "ordinary-departing-context" || command == "ordinary-transit-ambient" || command == "ordinary-transit-context") {
                contextual_route = command.starts_with("route-") || command.starts_with("ordinary-");
                eta_ship = contextual_route || command.find("foreign") != std::string::npos ? 1 : 0;
                const bool contextual = command.ends_with("-context");
                map.ships_captured = true;
                if (map.ships.empty()) map.ships = {{0, 0, true, "Own"}, {30, 0, false, ""}, {60, 0, false, ""}};
                map.ships[eta_ship].destination_available = contextual_route && !contextual ? rpc::SHIP_CONTEXT_REQUIRED : rpc::SHIP_CURRENT;
                map.ships[eta_ship].destination_system = 1;
                map.ships[eta_ship].eta_available = contextual ? rpc::SHIP_CURRENT : rpc::SHIP_CONTEXT_REQUIRED;
                map.ships[eta_ship].eta = command.starts_with("ordinary-transit-") ? 255 : 0;
                if (contextual_route) {
                    map.ships[0].destination_available = map.ships[0].eta_available = rpc::SHIP_CURRENT;
                    map.ships[0].destination_system = 0;
                    map.ships[0].eta = 255;
                }
                map.context_epoch = contextual ? rpc::Bridge_Context_Epoch(&bridge) : 0;
                rpc::Capture_Map(&store, &map);
            } else if (command == "eta-revoke") {
                rpc::Revoke_Game_Context(&store);
                map.context_epoch = 0;
                if (map.ships.size() > eta_ship) map.ships[eta_ship].eta_available = rpc::SHIP_CONTEXT_REQUIRED;
                if (contextual_route && map.ships.size() > eta_ship) map.ships[eta_ship].destination_available = rpc::SHIP_CONTEXT_REQUIRED;
            } else if (command == "colonies") {
                map.colonies_captured = true;
                map.colonies = {{0, 0, 1, 0}, {0, 0, 5, 42}, {1, 99999, -1, 99999}};
                map.colonies[0].armor = 32767;
                map.colonies[1].infantry = 32767;
                rpc::Capture_Map(&store, &map);
            } else if (command == "exploration" || command == "exploration-large") {
                map.planets_captured = true;
                map.planets.clear();
                const size_t systems = command == "exploration-large" ? 400 : map.systems.size();
                if (systems != map.systems.size()) map.systems.resize(systems, {100, 100, true, true, "Known"});
                for (size_t system = 0; system < systems; ++system) {
                    if (system == 1 && command == "exploration") continue;
                    map.systems[system].explored = true;
                    map.systems[system].wormhole_available = rpc::EXPLORATION_EMPTY;
                    for (int orbit : {1, 5}) {
                        rpc::s_planet_observation planet;
                        planet.visible = true;
                        planet.system = (int32_t)system;
                        planet.orbit = orbit;
                        planet.kind = 3;
                        planet.name = "Known planet " + std::to_string(system) + "-" + std::to_string(orbit);
                        planet.climate = 8;
                        planet.food2 = 5;
                        planet.max_population_available = system == 0;
                        planet.max_population = 42;
                        map.planets.push_back(std::move(planet));
                    }
                }
                map.systems[0].owner_slot = 0;
                map.systems[0].wormhole_available = rpc::EXPLORATION_CURRENT;
                map.systems[0].wormhole_system = 1;
                map.nebulas[0].variant = 11;
                if (command == "exploration-large") {
                    map.systems[1].visited = false;
                    map.systems[1].name_known = true;
                }
                rpc::Capture_Map(&store, &map);
            } else if (command == "exploration-change") {
                if (!map.planets.empty()) map.planets[0].climate = 9;
                rpc::Capture_Map(&store, &map);
            } else if (command == "exploration-hidden") {
                rpc::s_planet_observation hidden;
                hidden.system = -99999;
                hidden.climate = -99999;
                hidden.name = "hidden planet";
                map.planets.push_back(hidden);
                map.systems[1].in_extended_range = true;
                map.systems[1].name = "hidden name";
                rpc::Capture_Map(&store, &map);
            } else if (command == "exploration-lore") {
                map.systems[1].explored = true;
                map.systems[1].name_available = map.systems[1].name_known = true;
                map.systems[1].name = "(Lore)";
                rpc::Capture_Map(&store, &map);
            } else if (command == "exploration-forget") {
                map.systems[1].explored = false;
                map.systems[1].name_available = map.systems[1].name_known = false;
                rpc::Capture_Map(&store, &map);
            } else if (command == "publish-exploration") {
                rpc::s_request request;
                request.id_json = "99";
                request.method = "game.getState";
                request.params_json = "{\"include\":[\"session\",\"systems\",\"planets\",\"nebulas\",\"players\",\"colonies\",\"ships\"]}";
                std::vector<rpc::s_client_identity> clients;
                rpc::Get_Authorized_Clients(&bridge, &clients);
                if (clients.size() != 1)
                    acknowledgment = "missing-exploration-client";
                else {
                    for (int index = 0; index < 4; ++index) {
                        rpc::Submit_Work(&bridge, clients[0].client, clients[0].epoch, &request, UINT64_MAX);
                    }
                    rpc::Pump_Game_Queries(&store);
                }
            } else if (command == "revoke-exploration-output") {
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
                bool drained = false;
                while (std::chrono::steady_clock::now() < deadline) {
                    {
                        std::lock_guard lock(bridge.mutex);
                        drained = bridge.ready.empty();
                    }
                    if (drained) break;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                {
                    std::lock_guard lock(bridge.mutex);
                    if (!drained || bridge.deliveries.empty()) acknowledgment = "no-service-owned-exploration-output";
                }
                rpc::Pause_Game_Queries(&store);
                polling = false;
            } else if (command == "war") {
                map.players[1].at_war_available = true;
                map.players[1].at_war = !map.players[1].at_war;
                rpc::Capture_Map(&store, &map);
            } else if (command == "research-treaty") {
                map.players[1].research_treaty_available = true;
                map.players[1].research_treaty = !map.players[1].research_treaty;
                rpc::Capture_Map(&store, &map);
            } else if (command == "trade-treaty") {
                map.players[1].trade_treaty_available = true;
                map.players[1].trade_treaty = !map.players[1].trade_treaty;
                rpc::Capture_Map(&store, &map);
            } else if (command == "player-uncontact") {
                map.players[1].contacted = false;
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
            } else if (command == "colony-troops") {
                if (!map.colonies.empty()) ++map.colonies[0].infantry;
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
            else if (command == "publish-foreign-context-large") {
                if (map.ships.size() == 3) {
                    const auto foreign = map.ships[1];
                    map.ships.assign(3500, foreign);
                    rpc::Capture_Map(&store, &map);
                }
                rpc::s_request request;
                request.id_json = "99";
                request.method = "game.getState";
                request.params_json = "{\"include\":[\"session\",\"systems\",\"nebulas\",\"players\",\"colonies\",\"ships\"]}";
                std::vector<rpc::s_client_identity> clients;
                rpc::Get_Authorized_Clients(&bridge, &clients);
                if (clients.size() == 1 && rpc::Submit_Work(&bridge, clients[0].client, clients[0].epoch, &request, UINT64_MAX) == 0) {
                    rpc::s_work work;
                    if (rpc::Take_Work(&bridge, 0, &work)) {
                        uint64_t context = 0;
                        const auto result = rpc::Game_Query(&store, &request, &context);
                        if (context == 0 || result.size() < 900000 || result.size() >= 1048576) acknowledgment = "invalid-large-context-state";
                        rpc::Complete_Work(&bridge, &work, &result, context);
                    }
                } else
                    acknowledgment = "missing-context-client";
            } else if (command == "revoke-context-output") {
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
                while (std::chrono::steady_clock::now() < deadline) {
                    bool drained;
                    {
                        std::lock_guard lock(bridge.mutex);
                        drained = bridge.ready.empty();
                    }
                    if (drained) break;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                {
                    std::lock_guard lock(bridge.mutex);
                    if (!bridge.ready.empty() || bridge.deliveries.empty()) acknowledgment = "no-service-owned-pending-output";
                }
                rpc::Revoke_Bridge_Context(&bridge);
                polling = false;
            } else if (command == "publish" || command == "publish-large" || command == "publish-context-large" || command == "overflow") {
                rpc::s_request request;
                request.id_json = "99";
                request.method = "fixture";
                request.params_json = "{}";
                if (rpc::Submit_Work(&bridge, 1, 1, &request, std::numeric_limits<uint64_t>::max()) == 0) {
                    rpc::s_work work;
                    if (rpc::Take_Work(&bridge, 0, &work)) {
                        const size_t size = command == "publish-context-large" ? 1048000 : (command == "publish-large" ? 524288 : 32768);
                        const std::string result = command != "overflow" ? rpc::Result_Response(&request, "{\"fixture\":" + rpc::Json_String(std::string(size, 'x')) + "}") : std::string(1048577, 'x');
                        rpc::Complete_Work(&bridge, &work, &result, command == "publish-context-large" ? rpc::Bridge_Context_Epoch(&bridge) : 0);
                    }
                }
            }
            if (!command.empty()) std::cout << acknowledgment << std::endl;
            if (polling) rpc::Pump_Game_Queries(&store);
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
