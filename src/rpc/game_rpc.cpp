#include "pch.h"

#include "rpc/game_rpc.h"
#include "game/game_events.h"

#include "config/text_encoding.h"
#include "rpc/rpc_game.h"
#include "rpc/rpc_service.h"
#include "rpc/rpc_settings.h"
#include "rpc/players_rpc.h"
#include "rpc/colony_rpc.h"

#include <algorithm>
#include <tuple>

namespace game_rpc {
    namespace {
        rpc::s_bridge bridge;
        rpc::s_game_store store;
        rpc::s_service service;
        uint64_t subscription = 0;
        bool active = false;
        bool pumping = false;
        uint64_t last_capture_ms = 0;

    } // namespace

    bool Read_Map_Observation(rpc::s_map_observation* observation) {
        if (observation == nullptr) return false;
        *observation = {};
        const int32_t viewer = MOX::PLAYER_NUM;
        if (RUSS::all_ai_game != 0 || viewer < 0 || viewer >= MOX::NUM_PLAYERS || viewer >= MAX_PLAYERS ||
            MOX::player == nullptr || MOX::star == nullptr || (MOX::NUM_NEBULAS != 0 && MOX::nebula == nullptr) ||
            MOX::player[viewer].objectives != PLAYER_OBJECTIVE_HUMAN || MOX::player[viewer].eliminated != 0 ||
            MOX::NUM_STARS < 0 || MOX::NUM_STARS > MAX_STARS || MOX::NUM_NEBULAS > MAX_NEBULAS) {
            return false;
        }
        observation->viewer = viewer;
        observation->turn = MOX::stardate;
        observation->systems.reserve(MOX::NUM_STARS);
        for (int16_t index = 0; index < MOX::NUM_STARS; ++index) {
            const s_star_data* star = &MOX::star[index];
            rpc::s_system_observation item;
            item.x = star->x;
            item.y = star->y;
            item.visited = HAROLD::Player_Has_Visited((int16_t)viewer, index) != 0;
            if (item.visited && star->spectral_class != STAR_CLASS_BLACK_HOLE) {
                if (memchr(star->name, 0, sizeof(star->name)) == nullptr) return false;
                // Validate links before the native read-only outpost-format predicate.
                for (int orbit = 0; orbit < 5; ++orbit) {
                    const int16_t planet = star->planet_index[orbit];
                    if (planet < -1 || planet >= MAX_PLANETS || (planet >= 0 && MOX::planet == nullptr)) return false;
                    if (planet >= 0) {
                        const int16_t colony = MOX::planet[planet].colony_index;
                        if (colony < -1 || colony >= MAX_COLONIES || (colony >= 0 && MOX::colony == nullptr)) return false;
                    }
                }
                char name[sizeof(star->name) + 1] = {};
                memcpy(name, star->name, sizeof(star->name));
                if (HAROLD::Star_Is_Outpost_Star(index)) strings::String_To_Lower(name);
                config::Decode_Cp850_To_Utf8(name, strlen(name), &item.name);
                item.name_available = true;
            }
            observation->systems.push_back(std::move(item));
        }
        observation->nebulas.reserve(MOX::NUM_NEBULAS);
        for (int16_t index = 0; index < MOX::NUM_NEBULAS; ++index) {
            observation->nebulas.push_back({MOX::nebula[index].x, MOX::nebula[index].y});
        }
        return players_rpc::Read_Players(observation) && colony_rpc::Read_Colonies(observation);
    }

    static rpc::s_bridge* Initialize() {
        if (!active) active = rpc::Initialize_Game_Store(&store, &bridge);
        return active ? &bridge : nullptr;
    }

    static void Read_Map_Route(int16_t node, rpc::s_ship_observation* route) {
        const s_ship_data* ship = &MOX::ship[MOX::ship_node[node].ship_idx];
        const bool owned = ship->owner == MOX::PLAYER_NUM;
        route->destination_available = route->eta_available = owned ? rpc::SHIP_UNAUDITED : rpc::SHIP_CONTEXT_REQUIRED;
        if (owned && ship->location >= 0 && ship->location < MOX::NUM_STARS) {
            route->destination_available = route->eta_available = rpc::SHIP_KNOWN_EMPTY;
            return;
        }
        // This subset follows the non-selection branches of Do_Ship_Destination_Lines.
        // No moveable/fleet-box state is assumed initialized at an icon refresh.
        if (!owned && ship->status == 0) return;
        if (ship->location < SHIP_LOCATION_MOVING_OFFSET || ship->location >= SHIP_LOCATION_LIMIT) return;
        const int16_t destination = HAROLD::Absolute_Location(ship->location);
        if (destination < 0 || destination >= MOX::NUM_STARS) {
            if (owned) route->destination_available = route->eta_available = rpc::SHIP_UNAUDITED;
            return; // Includes the special Antares destination, not a map system.
        }
        if (!owned && MOX::settings.enemy_moves == 0) {
            // Validate only links the existing read-only predicate may follow.
            if (!HAROLD::Player_Has_Colony(MOX::PLAYER_NUM, destination)) {
                for (int orbit = 0; orbit < 5; ++orbit) {
                    const int16_t planet = MOX::star[destination].planet_index[orbit];
                    if (planet < -1 || planet >= MAX_PLANETS || (planet >= 0 && MOX::planet == nullptr)) return;
                    if (planet >= 0) {
                        const int16_t colony = MOX::planet[planet].colony_index;
                        if (colony < -1 || colony >= MAX_COLONIES || (colony >= 0 && MOX::colony == nullptr)) return;
                    }
                }
            }
            if (!SHIPS::Enemy_Ship_Heading_Toward_Our_Colony(node)) return;
        }
        route->destination_available = rpc::SHIP_CURRENT;
        route->destination_system = destination;
        route->eta_available = rpc::SHIP_CONTEXT_REQUIRED;
        if (ship->location < SHIP_LOCATION_WORMHOLE_OFFSET) {
            route->eta_available = rpc::SHIP_CURRENT;
            route->eta = (uint8_t)ship->turns_left;
        }
    }

    bool Read_Refreshed_Ship_Observation(rpc::s_map_observation* observation) {
        if (observation == nullptr) return false;
        *observation = {};
        rpc::s_map_observation captured;
        if (!Read_Map_Observation(&captured) || MOX::NUM_SHIPS < 0 || MOX::NUM_SHIPS > MAX_SHIPS ||
            (MOX::NUM_SHIPS != 0 && MOX::ship == nullptr) || MOX::ship_stack_count < 0 || MOX::ship_stack_count > MAX_SHIPS ||
            MOX::ship_icon_count < 0 || MOX::ship_icon_count > SHIP_ICON_COUNT) return false;
        std::vector<bool> heads(MAX_SHIPS);
        std::vector<bool> nodes(MAX_SHIPS);
        std::vector<bool> ships(MAX_SHIPS);
        for (int index = 0; index < MOX::ship_stack_count; ++index) {
            const int16_t head = MOX::ship_stack_start[index];
            if (head < 0 || head >= MAX_SHIPS || heads[head]) return false;
            heads[head] = true;
        }
        for (int index = 0; index < MOX::ship_icon_count; ++index) {
            int16_t node = MOX::ship_icon[index].node_idx;
            if (node < 0 || node >= MAX_SHIPS || !heads[node]) return false;
            const int16_t head = node;
            rpc::s_ship_observation route;
            while (node != -1) {
                if (node < 0 || node >= MAX_SHIPS || nodes[node]) return false;
                nodes[node] = true;
                const s_ship_node* link = &MOX::ship_node[node];
                const int16_t slot = link->ship_idx;
                if (slot < 0 || slot >= MOX::NUM_SHIPS || ships[slot]) return false;
                ships[slot] = true;
                const s_ship_data* ship = &MOX::ship[slot];
                if (ship->status < 0 || ship->status >= 3) return false;
                if (node == head)
                    Read_Map_Route(head, &route);
                else {
                    const s_ship_data* first = &MOX::ship[MOX::ship_node[head].ship_idx];
                    if (ship->x != first->x || ship->y != first->y || ship->owner != first->owner || ship->location != first->location) return false;
                }
                rpc::s_ship_observation item = route;
                // The map prints only the head ship's cached ETA, not every member's.
                if (node != head && item.eta_available == rpc::SHIP_CURRENT) {
                    item.eta_available = rpc::SHIP_CONTEXT_REQUIRED;
                    item.eta = 0;
                }
                item.x = ship->x;
                item.y = ship->y;
                item.owned = ship->owner == captured.viewer;
                if (std::any_of(captured.players.begin(), captured.players.end(), [ship](const rpc::s_player_observation& player) { return player.slot == ship->owner; })) {
                    item.owner_slot = ship->owner;
                }
                if (item.owned) {
                    const char* end = (const char*)memchr(ship->d.name, 0, sizeof(ship->d.name));
                    if (end == nullptr) return false;
                    config::Decode_Cp850_To_Utf8(ship->d.name, (size_t)(end - ship->d.name), &item.name);
                }
                captured.ships.push_back(std::move(item));
                node = link->next_node;
            }
        }
        // Native sorting uses fields outside this allowlist. Never expose that ordering.
        std::sort(captured.ships.begin(), captured.ships.end(), [](const rpc::s_ship_observation& left, const rpc::s_ship_observation& right) {
            return std::tie(left.x, left.y, left.owned, left.name) < std::tie(right.x, right.y, right.owned, right.name);
        });
        captured.ships_captured = true;
        *observation = std::move(captured);
        return true;
    }

    static void Shutdown() {
        rpc::Stop_Service(&service);
        if (active) rpc::Shutdown_Game_Store(&store);
        active = false;
    }

    static void Pump() {
        if (!active || pumping) return;
        pumping = true;
        rpc::Pump_Game_Queries(&store);
        pumping = false;
    }

    static void Capture_Main_Map() {
        if (!active || !store.loaded || store.busy_depth != 0) return;
        if (!store.snapshots.empty() && !rpc::Needs_Game_Capture(&store)) return;
        const uint64_t now = rpc::Monotonic_Ms();
        if (last_capture_ms != 0 && now - last_capture_ms < rpc::Protocol_Limits()->capture_interval_ms) return;
        last_capture_ms = now;
        rpc::s_map_observation observation;
        if (!Read_Map_Observation(&observation) || !rpc::Capture_Map(&store, &observation)) {
            rpc::Begin_Game_Reset(&store);
        }
    }

    static void Menu() {
        if (active) rpc::End_Game(&store);
    }

    static void Capture_Refreshed_Ships() {
        if (!active || !store.loaded || store.busy_depth != 0) return;
        rpc::s_map_observation observation;
        if (!Read_Refreshed_Ship_Observation(&observation) || !rpc::Capture_Map(&store, &observation)) {
            rpc::Begin_Game_Reset(&store);
        }
    }

    static void Revoke_Perspective() {
        if (active) rpc::Revoke_Game_View(&store);
        last_capture_ms = 0;
    }

    static void Handle_Event(game_events::e_event event, void* context) {
        (void)context;
        if (event == game_events::THREAD_STARTED) {
            if (!rpc::enabled) return;
            rpc::s_service_config service_config;
            service_config.bridge = Initialize();
            service_config.port = rpc::port;
            config::Decode_Cp850_To_Utf8(rpc::shared_key, strlen(rpc::shared_key), &service_config.key);
            std::string diagnostic;
            if (service_config.bridge == nullptr || !rpc::Start_Service(&service, &service_config, &diagnostic)) {
                Shutdown();
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "rpc: listener initialization failed");
            } else {
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "rpc: listening on loopback port %u", (unsigned)rpc::Service_Port(&service));
            }
            return;
        }
        if (event == game_events::THREAD_STOPPING) {
            Shutdown();
            return;
        }
        if (!active) return;
        switch (event) {
            case game_events::MENU_ENTERED: Menu(); break;
            case game_events::PERSPECTIVE_SELECTION_BEGIN: Revoke_Perspective(); break;
            case game_events::WORLD_REPLACE_BEGIN:
                rpc::Begin_Game_Reset(&store);
                rpc::Pause_Game_Queries(&store);
                last_capture_ms = 0;
                break;
            case game_events::WORLD_UPDATE_BEGIN:
            case game_events::RESOLUTION_BEGIN: rpc::Pause_Game_Queries(&store); break;
            case game_events::WORLD_REPLACE_COMMITTED:
                if (!rpc::Commit_Game_Reset(&store)) rpc::Begin_Game_Reset(&store);
                rpc::Resume_Game_Queries(&store);
                break;
            case game_events::WORLD_REPLACE_FAILED:
            case game_events::WORLD_UPDATE_FAILED:
                rpc::Begin_Game_Reset(&store);
                rpc::Resume_Game_Queries(&store);
                break;
            case game_events::WORLD_UPDATE_COMMITTED:
            case game_events::RESOLUTION_END:
                rpc::Resume_Game_Queries(&store);
                store.capture_requested = true;
                last_capture_ms = 0;
                break;
            case game_events::MAP_OBSERVATION_POINT: Capture_Main_Map(); break;
            case game_events::SHIP_ICONS_REFRESHED: Capture_Refreshed_Ships(); break;
            case game_events::IDLE_OBSERVATION_POINT: Pump(); break;
            default: break;
        }
    }

    static void Observe(game_events::e_event event, void* context) {
        try {
            Handle_Event(event, context);
        } catch (...) {
            Shutdown();
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "rpc: observer disabled after an internal failure");
        }
    }

    bool Subscribe_To_Game() {
        if (subscription != 0) return false;
        subscription = game_events::Subscribe(Observe, nullptr);
        return subscription != 0;
    }

    void Unsubscribe_From_Game() {
        game_events::Unsubscribe(subscription);
        subscription = 0;
        Shutdown();
    }
} // namespace game_rpc
