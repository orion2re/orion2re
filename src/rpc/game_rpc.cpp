#include "pch.h"

#include "rpc/game_rpc.h"
#include "game/game_events.h"

#include "config/text_encoding.h"
#include "rpc/rpc_game.h"
#include "rpc/rpc_service.h"
#include "rpc/rpc_settings.h"
#include "rpc/players_rpc.h"
#include "rpc/colony_rpc.h"
#include "rpc/exploration_rpc.h"
#include "rpc/tech_rpc.h"
#include "rpc/officer_rpc.h"
#include "rpc/relations_rpc.h"
#include "rpc/turn_review_rpc.h"
#include "lua/lua_game_ship_identity.h"

#include <algorithm>
#include <tuple>

namespace game_rpc {
    namespace {
        rpc::s_bridge bridge;
        rpc::s_game_store store;
        rpc::s_service service;
        s_fleet_eta_adapter fleet_eta;
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
            MOX::NUM_PLAYERS > MAX_PLAYERS || MOX::planet_count < 0 || MOX::planet_count > MAX_PLANETS ||
            MOX::NUM_COLONIES < 0 || MOX::NUM_COLONIES > MAX_COLONIES ||
            (MOX::planet_count != 0 && MOX::planet == nullptr) || (MOX::NUM_COLONIES != 0 && MOX::colony == nullptr) ||
            MOX::player == nullptr || MOX::star == nullptr || (MOX::NUM_NEBULAS != 0 && MOX::nebula == nullptr) ||
            MOX::player[viewer].objectives != PLAYER_OBJECTIVE_HUMAN || MOX::player[viewer].eliminated != 0 ||
            MOX::NUM_STARS < 0 || MOX::NUM_STARS > MAX_STARS || MOX::NUM_NEBULAS > MAX_NEBULAS) {
            return false;
        }
        observation->viewer = viewer;
        observation->turn = MOX::stardate;
        return exploration_rpc::Read_Exploration(observation) && players_rpc::Read_Players(observation) &&
            colony_rpc::Read_Colonies(observation) && tech_rpc::Read_Research(observation) && officer_rpc::Read_Leaders(observation) && relations_rpc::Read_Relations(observation) && turn_review_rpc::Read_Turn_Review(observation);
    }

    bool Capture_Leader_View(rpc::s_game_store* target, const rpc::s_map_observation* ambient, const game_events::s_leader_view* view) {
        if (target == nullptr || !target->loaded || target->viewer < 0 || target->snapshots.empty() || view == nullptr) return false;
        if (view->count < 0 || view->count > 2 || (view->count != 0 && view->leaders == nullptr)) return false;
        bool disclosed = false;
        for (int16_t index = 0; index < view->count; ++index) disclosed |= view->leaders[index].viewer == target->viewer;
        if (!disclosed) {
            rpc::Revoke_Game_Context(target);
            return view->count >= 0 && view->count <= 2;
        }
        rpc::s_map_observation observation;
        if (target->busy_depth == 0 && ambient != nullptr)
            observation = *ambient;
        else {
            observation.viewer = target->viewer;
            observation.turn = target->snapshots.back().map.turn;
            observation.leaders_only = true;
        }
        if (!officer_rpc::Read_Disclosures(view, &observation)) return false;
        observation.leader_presentation = true;
        observation.context_epoch = rpc::Bridge_Context_Epoch(target->bridge);
        return rpc::Capture_Map(target, &observation);
    }

    bool Capture_Relations_View(rpc::s_game_store* target, const rpc::s_map_observation* ambient, const game_events::s_relations_view* view) {
        if (target == nullptr || ambient == nullptr || !target->loaded || target->busy_depth != 0 || ambient->viewer != target->viewer) return false;
        auto observation = *ambient;
        if (!relations_rpc::Read_Disclosures(view, &observation)) return false;
        if (!rpc::Capture_Map(target, ambient)) return false;
        if (!observation.relations_presentation) {
            rpc::Revoke_Game_Context(target);
            return true;
        }
        observation.context_epoch = rpc::Bridge_Context_Epoch(target->bridge);
        return rpc::Capture_Map(target, &observation);
    }

    static rpc::s_bridge* Initialize() {
        if (!active) {
            active = rpc::Initialize_Game_Store(&store, &bridge);
            if (active && !Initialize_Fleet_Eta_Adapter(&fleet_eta, &store)) {
                rpc::Shutdown_Game_Store(&store);
                active = false;
            }
        }
        return active ? &bridge : nullptr;
    }

    static bool Ambient_Foreign_Route(int16_t destination) {
        if (destination < 0 || destination >= MOX::NUM_STARS) return false;
        if (MOX::settings.enemy_moves != 0 || HAROLD::Player_Has_Colony(MOX::PLAYER_NUM, destination)) return true;
        // The incoming predicate's remaining branch. Validate links before the
        // native outpost read; its unused player argument intentionally stays so.
        for (int orbit = 0; orbit < 5; ++orbit) {
            const int16_t planet = MOX::star[destination].planet_index[orbit];
            if (planet < -1 || planet >= MAX_PLANETS || (planet >= 0 && MOX::planet == nullptr)) return false;
            if (planet >= 0) {
                const int16_t colony = MOX::planet[planet].colony_index;
                if (colony < -1 || colony >= MAX_COLONIES || (colony >= 0 && MOX::colony == nullptr)) return false;
            }
        }
        return HAROLD::Player_Has_Outpost(MOX::PLAYER_NUM, destination);
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
        if (!owned && !Ambient_Foreign_Route(destination)) return;
        route->destination_available = rpc::SHIP_CURRENT;
        route->destination_system = destination;
        route->eta_available = rpc::SHIP_CONTEXT_REQUIRED;
        if (ship->location < SHIP_LOCATION_WORMHOLE_OFFSET) {
            route->eta_available = rpc::SHIP_CURRENT;
            route->eta = (uint8_t)ship->turns_left;
        }
    }

    static bool Read_Ship_Refresh(rpc::s_map_observation* observation, std::vector<s_ship_association>* associations) {
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
                item.slot = slot;
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
                if (associations != nullptr) {
                    s_ship_association association;
                    association.node = node;
                    association.head = head;
                    association.slot = slot;
                    association.ship = captured.ships.back();
                    association.owner = ship->owner;
                    association.location = ship->location;
                    association.status = ship->status;
                    association.generation = orion2re::lua::game::Ship_Generation(slot);
                    associations->push_back(std::move(association));
                }
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

    bool Read_Refreshed_Ship_Observation(rpc::s_map_observation* observation) {
        return Read_Ship_Refresh(observation, nullptr);
    }

    static void Revoke_Fleet_Eta(s_fleet_eta_adapter* adapter) {
        rpc::Revoke_Game_Context(adapter->store);
        adapter->authorized_head = -1;
        adapter->pending_head = -1;
        adapter->pending = {};
        adapter->contextual_destination = false;
        adapter->route_node = adapter->route_ship = adapter->route_destination = -1;
    }

    static void Observe_Ship_Destination(const game_events::s_ship_destination_line* line, void* context) {
        auto* adapter = (s_fleet_eta_adapter*)context;
        if (adapter->failed || !adapter->initialized || !adapter->presenting || !adapter->frame || !line->box_selected) return;
        adapter->route_node = line->node;
        adapter->route_ship = line->ship;
        adapter->route_destination = line->destination;
    }

    static void Observe_Fleet_Info(const game_events::s_fleet_box_info* info, void* context) {
        auto* adapter = (s_fleet_eta_adapter*)context;
        try {
            if (adapter->failed || !adapter->initialized || !adapter->presenting || !adapter->printing ||
                !adapter->store->loaded || adapter->store->busy_depth != 0 || !info->box_open ||
                info->member_count <= 0 || info->member_count > MAX_SHIPS || info->members == nullptr ||
                info->selected_count < 0 || info->selected_count > MAX_SHIPS ||
                (info->selected_count != 0 && info->selected == nullptr)) return;
            const auto head = std::find_if(adapter->associations.begin(), adapter->associations.end(), [info](const s_ship_association& item) {
                return item.node == info->node && item.slot == info->ship && item.head == info->box_head;
            });
            if (head == adapter->associations.end() || head->node != head->head ||
                head->location < SHIP_LOCATION_MOVING_OFFSET || head->location >= SHIP_LOCATION_LIMIT ||
                head->ship.eta_available != rpc::SHIP_CONTEXT_REQUIRED ||
                info->members[0].node != head->node || info->members[0].ship != head->slot) return;
            const bool contextual = head->ship.destination_available != rpc::SHIP_CURRENT;
            const int16_t destination = contextual ? adapter->route_destination : head->ship.destination_system;
            if (destination < 0 || destination >= MOX::NUM_STARS ||
                (info->star != -1 && info->star != destination)) return;
            if (contextual) {
                // Native departure uses the upper encoding; ordinary transit uses
                // the lower encoding and status 1. Do not admit mixed states.
                const int8_t status = head->location < SHIP_LOCATION_WORMHOLE_OFFSET ? 1 : 2;
                if (head->ship.owned || head->status != status || !adapter->frame ||
                    adapter->route_node != head->node || adapter->route_ship != head->slot ||
                    HAROLD::Absolute_Location(head->location) != destination) return;
            } else if (head->location < SHIP_LOCATION_WORMHOLE_OFFSET ||
                (!head->ship.owned && !Ambient_Foreign_Route(destination)))
                return;
            std::vector<bool> members(MAX_SHIPS);
            std::vector<int32_t> associations(MAX_SHIPS, -1);
            for (size_t index = 0; index < adapter->associations.size(); ++index) associations[adapter->associations[index].node] = (int32_t)index;
            for (int index = 0; index < info->member_count; ++index) {
                const auto& ref = info->members[index];
                if (ref.node < 0 || ref.node >= MAX_SHIPS || members[ref.node]) return;
                members[ref.node] = true;
                const int32_t index_in_refresh = associations[ref.node];
                if (index_in_refresh < 0 || adapter->associations[index_in_refresh].slot != ref.ship ||
                    adapter->associations[index_in_refresh].head != head->head) return;
            }
            if (std::count_if(adapter->associations.begin(), adapter->associations.end(), [&head](const s_ship_association& item) { return item.head == head->head; }) != info->member_count) return;
            for (int index = 0; index < info->selected_count; ++index) {
                const auto& ref = info->selected[index];
                if (ref.node < 0 || ref.node >= MAX_SHIPS || !members[ref.node] ||
                    adapter->associations[associations[ref.node]].slot != ref.ship) return;
            }
            rpc::s_map_observation captured;
            if (!Read_Map_Observation(&captured) || captured.viewer != adapter->store->viewer) return;
            for (const auto& item : adapter->associations) {
                if (!item.valid) return;
                if (item.ship.owned || members[item.node]) {
                    if (item.slot < 0 || item.slot >= MOX::NUM_SHIPS || MOX::ship == nullptr) return;
                    const auto& ship = MOX::ship[item.slot];
                    if (item.generation == 0 || item.generation != orion2re::lua::game::Ship_Generation(item.slot) ||
                        ship.owner != item.owner || ship.status != item.status || ship.location != item.location ||
                        ship.x != item.ship.x || ship.y != item.ship.y) return;
                }
                if (item.ship.owned) {
                    const auto& ship = MOX::ship[item.slot];
                    const char* end = (const char*)memchr(ship.d.name, 0, sizeof(ship.d.name));
                    std::string name;
                    if (end == nullptr) return;
                    config::Decode_Cp850_To_Utf8(ship.d.name, (size_t)(end - ship.d.name), &name);
                    if (name != item.ship.name) return;
                }
                captured.ships.push_back(item.ship);
                if (item.node == head->node) {
                    captured.ships.back().destination_available = rpc::SHIP_CURRENT;
                    captured.ships.back().destination_system = destination;
                    captured.ships.back().eta_available = rpc::SHIP_CURRENT;
                    captured.ships.back().eta = info->turns_left;
                }
            }
            captured.ships_captured = true;
            adapter->pending = std::move(captured);
            adapter->pending_head = head->head;
            adapter->pending_contextual_destination = contextual;
        } catch (...) {
            adapter->failed = true;
            Revoke_Fleet_Eta(adapter);
        }
    }

    static void Observe_Ship_Identity(int16_t slot, void* context) {
        auto* adapter = (s_fleet_eta_adapter*)context;
        for (auto& item : adapter->associations) {
            if (item.slot != slot) continue;
            item.valid = false;
            Revoke_Fleet_Eta(adapter);
        }
    }

    static void Observe_Fleet_Event(game_events::e_event event, void* context) {
        auto* adapter = (s_fleet_eta_adapter*)context;
        try {
            switch (event) {
                case game_events::FLEET_BOX_VIEW_ENTERED:
                case game_events::FLEET_BOX_VIEW_LEAVING:
                    adapter->owner_present = event == game_events::FLEET_BOX_VIEW_ENTERED;
                    adapter->initialized = adapter->presenting = false;
                    adapter->associations.clear();
                    Revoke_Fleet_Eta(adapter);
                    break;
                case game_events::FLEET_BOX_VIEW_INITIALIZED: adapter->initialized = adapter->owner_present; break;
                case game_events::MAIN_VIEW_INPUT_ENTERED:
                case game_events::MAIN_VIEW_DRAW_ENTERED: adapter->presenting = adapter->initialized; break;
                case game_events::MAIN_VIEW_INPUT_LEAVING:
                case game_events::MAIN_VIEW_DRAW_LEAVING: adapter->presenting = false; break;
                case game_events::MAIN_SCREEN_FRAME_ENTERED:
                    adapter->frame = adapter->presenting;
                    adapter->frame_accepted = false;
                    adapter->route_node = adapter->route_ship = adapter->route_destination = -1;
                    break;
                case game_events::MAIN_SCREEN_FRAME_LEAVING:
                    if (adapter->contextual_destination && !adapter->frame_accepted) Revoke_Fleet_Eta(adapter);
                    adapter->frame = false;
                    adapter->route_node = adapter->route_ship = adapter->route_destination = -1;
                    break;
                case game_events::MAIN_VIEW_INPUT_INTERRUPTED:
                    adapter->presenting = false;
                    Revoke_Fleet_Eta(adapter);
                    break;
                case game_events::FLEET_BOX_PRINT_ENTERED:
                    adapter->printing = true;
                    adapter->pending = {};
                    adapter->pending_head = -1;
                    break;
                case game_events::FLEET_BOX_PRINT_LEAVING:
                    adapter->printing = false;
                    if (!adapter->pending.ships_captured) {
                        Revoke_Fleet_Eta(adapter);
                        break;
                    }
                    if (adapter->authorized_head != -1 && adapter->authorized_head != adapter->pending_head) rpc::Revoke_Game_Context(adapter->store);
                    adapter->pending.context_epoch = rpc::Bridge_Context_Epoch(adapter->store->bridge);
                    if (!rpc::Capture_Map(adapter->store, &adapter->pending)) {
                        Revoke_Fleet_Eta(adapter);
                        break;
                    }
                    adapter->authorized_head = adapter->pending_head;
                    adapter->contextual_destination = adapter->pending_contextual_destination;
                    adapter->frame_accepted = true;
                    adapter->pending = {};
                    break;
                case game_events::FLEET_BOX_INFO_CLEARED:
                    if (!adapter->printing) Revoke_Fleet_Eta(adapter);
                    break;
                case game_events::SHIP_ICONS_REBUILD_BEGIN:
                case game_events::SHIP_IDENTITIES_RESETTING:
                case game_events::WORLD_REPLACE_BEGIN:
                case game_events::WORLD_UPDATE_BEGIN:
                case game_events::RESOLUTION_BEGIN:
                case game_events::PERSPECTIVE_SELECTION_BEGIN:
                case game_events::MENU_ENTERED:
                    adapter->associations.clear();
                    Revoke_Fleet_Eta(adapter);
                    break;
                case game_events::SHIP_ICONS_REFRESHED: {
                    adapter->associations.clear();
                    if (!adapter->store->loaded || adapter->store->busy_depth != 0 || adapter->failed) break;
                    rpc::s_map_observation observation;
                    if (!Read_Ship_Refresh(&observation, &adapter->associations) || !rpc::Capture_Map(adapter->store, &observation)) {
                        adapter->associations.clear();
                        rpc::Begin_Game_Reset(adapter->store);
                    }
                    break;
                }
                default: break;
            }
        } catch (...) {
            adapter->failed = true;
            adapter->associations.clear();
            Revoke_Fleet_Eta(adapter);
        }
    }

    bool Initialize_Fleet_Eta_Adapter(s_fleet_eta_adapter* adapter, rpc::s_game_store* store) {
        if (adapter == nullptr || store == nullptr || adapter->store != nullptr) return false;
        adapter->store = store;
        adapter->events = game_events::Subscribe(Observe_Fleet_Event, adapter);
        adapter->info = game_events::Subscribe_Fleet_Box_Info(Observe_Fleet_Info, adapter);
        adapter->identity = game_events::Subscribe_Ship_Identity(Observe_Ship_Identity, adapter);
        adapter->destination = game_events::Subscribe_Ship_Destination(Observe_Ship_Destination, adapter);
        if (adapter->events != 0 && adapter->info != 0 && adapter->identity != 0 && adapter->destination != 0) return true;
        Shutdown_Fleet_Eta_Adapter(adapter);
        return false;
    }

    void Shutdown_Fleet_Eta_Adapter(s_fleet_eta_adapter* adapter) {
        if (adapter == nullptr) return;
        game_events::Unsubscribe(adapter->events);
        game_events::Unsubscribe(adapter->info);
        game_events::Unsubscribe(adapter->identity);
        game_events::Unsubscribe(adapter->destination);
        if (adapter->store != nullptr) Revoke_Fleet_Eta(adapter);
        *adapter = {};
    }

    static void Shutdown() {
        Shutdown_Fleet_Eta_Adapter(&fleet_eta);
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
            case game_events::MESSAGES_REPLACING:
                if (game_events::Message_Recipient() == store.viewer) rpc::Expire_Game_Messages(&store);
                break;
            case game_events::RELATIONS_VIEW_CLEARED:
                rpc::Revoke_Game_Context(&store);
                break;
            case game_events::RELATIONS_VIEW_READY: {
                rpc::s_map_observation observation;
                if (store.busy_depth == 0 && Read_Map_Observation(&observation)) Capture_Relations_View(&store, &observation, game_events::Published_Relations_View());
                break;
            }
            case game_events::LEADER_VIEW_CLEARED:
                rpc::Revoke_Game_Context(&store);
                break;
            case game_events::LEADER_VIEW_READY: {
                rpc::s_map_observation observation;
                const bool ambient = store.busy_depth == 0 && Read_Map_Observation(&observation);
                if (!Capture_Leader_View(&store, ambient ? &observation : nullptr, game_events::Published_Leader_View())) rpc::Revoke_Game_Context(&store);
                break;
            }
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
