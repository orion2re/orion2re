#include "rpc_game.h"

#include <algorithm>
#include <limits>
#include <tuple>
#include <utility>

#include "yyjson.h"

namespace rpc {
    namespace {
        bool Default_Id(std::string* id, void* context) {
            (void)context;
            return New_Opaque_Id(id);
        }

        void Clear_View(s_game_store* store) {
            store->viewer = -1;
            store->revision = 0;
            store->perspective_id.clear();
            store->system_ids.clear();
            store->nebula_ids.clear();
            store->player_handles.clear();
            store->snapshots.clear();
            store->cursors.clear();
            store->subscriptions.clear();
            store->turn_observers.clear();
            store->capture_requested = false;
        }

        bool Same_Map(const s_map_observation* left, const s_map_observation* right) {
            if (left->viewer != right->viewer || left->turn != right->turn ||
                left->systems.size() != right->systems.size() || left->nebulas.size() != right->nebulas.size()) {
                return false;
            }
            for (size_t index = 0; index < left->systems.size(); ++index) {
                const auto& a = left->systems[index];
                const auto& b = right->systems[index];
                if (a.x != b.x || a.y != b.y || a.visited != b.visited ||
                    a.name_available != b.name_available || a.name != b.name) {
                    return false;
                }
            }
            for (size_t index = 0; index < left->nebulas.size(); ++index) {
                if (left->nebulas[index].x != right->nebulas[index].x || left->nebulas[index].y != right->nebulas[index].y) {
                    return false;
                }
            }
            return true;
        }

        bool Same_Ships(const s_map_observation* left, const s_map_observation* right) {
            if (left->ships_captured != right->ships_captured || left->ships.size() != right->ships.size()) return false;
            for (size_t index = 0; index < left->ships.size(); ++index) {
                const auto& a = left->ships[index];
                const auto& b = right->ships[index];
                if (std::tie(a.x, a.y, a.owned, a.name, a.owner_id, a.destination_available, a.destination_id, a.eta_available, a.eta) !=
                    std::tie(b.x, b.y, b.owned, b.name, b.owner_id, b.destination_available, b.destination_id, b.eta_available, b.eta)) return false;
            }
            return true;
        }

        bool Same_Players(const s_map_observation* left, const s_map_observation* right) {
            if (left->players_captured != right->players_captured || left->players.size() != right->players.size()) return false;
            for (size_t index = 0; index < left->players.size(); ++index) {
                const auto& a = left->players[index];
                const auto& b = right->players[index];
                if (std::tie(a.local, a.contacted, a.eliminated, a.color, a.name, a.treasury, a.hud.income, a.hud.food_surplus, a.hud.freighters, a.hud.freighters_available, a.hud.command_points, a.hud.command_points_available, a.at_war_available, a.at_war, a.research_treaty_available, a.research_treaty) !=
                    std::tie(b.local, b.contacted, b.eliminated, b.color, b.name, b.treasury, b.hud.income, b.hud.food_surplus, b.hud.freighters, b.hud.freighters_available, b.hud.command_points, b.hud.command_points_available, b.at_war_available, b.at_war, b.research_treaty_available, b.research_treaty)) return false;
            }
            return true;
        }

        bool Same_Colonies(const s_map_observation* left, const s_map_observation* right) {
            if (left->colonies_captured != right->colonies_captured || left->colonies.size() != right->colonies.size()) return false;
            for (size_t index = 0; index < left->colonies.size(); ++index) {
                const auto& a = left->colonies[index];
                const auto& b = right->colonies[index];
                if (std::tie(a.system_id, a.orbit, a.population, a.owner_id, a.infantry, a.armor) != std::tie(b.system_id, b.orbit, b.population, b.owner_id, b.infantry, b.armor)) return false;
            }
            return true;
        }

        const s_map_snapshot* Select_Snapshot(const s_game_store* store, bool ships, bool players = false, bool colonies = false) {
            for (auto snapshot = store->snapshots.rbegin(); snapshot != store->snapshots.rend(); ++snapshot) {
                if ((!ships || snapshot->map.ships_captured) && (!players || snapshot->map.players_captured) &&
                    (!colonies || snapshot->map.colonies_captured)) return &*snapshot;
            }
            return nullptr;
        }

        std::string Session(const s_game_store* store, const s_map_snapshot* selected = nullptr) {
            if (store->snapshots.empty()) {
                return store->loaded || store->unavailable ? "{\"phase\":\"unavailable\"}" : "{\"phase\":\"menu\"}";
            }
            const s_map_snapshot& snapshot = selected != nullptr ? *selected : store->snapshots.back();
            return "{\"gameId\":" + Json_String(store->game_id) + ",\"perspectiveId\":" + Json_String(store->perspective_id) +
                ",\"revision\":" + Json_String(std::to_string(snapshot.revision)) + ",\"turn\":" + std::to_string(snapshot.map.turn) +
                ",\"phase\":" + Json_String(store->busy_depth != 0 ? "resolving" : "committed") + "}";
        }

        std::string Stamp(const s_game_store* store, const s_map_snapshot* snapshot) {
            return "{\"gameId\":" + Json_String(store->game_id) + ",\"perspectiveId\":" + Json_String(store->perspective_id) +
                ",\"snapshotId\":" + Json_String(snapshot->id) + ",\"revision\":" + Json_String(std::to_string(snapshot->revision)) +
                ",\"turn\":" + std::to_string(snapshot->map.turn) + ",\"captureMode\":" +
                Json_String(snapshot->map.ships_captured ? "last-native-ship-refresh" : "last-coherent-capture") + "}";
        }

        std::string System(const s_game_store* store, const s_map_snapshot* snapshot, size_t index) {
            const s_system_observation& item = snapshot->map.systems[index];
            std::string result = "{\"id\":" + Json_String(store->system_ids[index]) + ",\"x\":" + std::to_string(item.x) +
                ",\"y\":" + std::to_string(item.y) + ",\"visited\":" + (item.visited ? "true" : "false") + ",\"name\":";
            result += item.name_available ? "{\"availability\":\"current\",\"value\":" + Json_String(item.name) + "}" : "{\"availability\":\"unaudited\"}";
            return result + ",\"owner\":{\"availability\":\"unaudited\"}}";
        }

        std::string Nebula(const s_game_store* store, const s_map_snapshot* snapshot, size_t index) {
            const s_nebula_observation& item = snapshot->map.nebulas[index];
            return "{\"id\":" + Json_String(store->nebula_ids[index]) + ",\"x\":" + std::to_string(item.x) + ",\"y\":" + std::to_string(item.y) + "}";
        }

        std::string Ship_Knowledge(e_ship_knowledge available, const std::string& value) {
            switch (available) {
                case SHIP_CURRENT: return "{\"availability\":\"current\",\"value\":" + value + "}";
                case SHIP_KNOWN_EMPTY: return "{\"availability\":\"known-empty\"}";
                case SHIP_UNKNOWN: return "{\"availability\":\"unknown\"}";
                case SHIP_CONTEXT_REQUIRED: return "{\"availability\":\"context-required\"}";
                default: return "{\"availability\":\"unaudited\"}";
            }
        }

        std::string Ship(const s_map_snapshot* snapshot, size_t index) {
            const s_ship_observation& item = snapshot->map.ships[index];
            const std::string name = item.owned ? "{\"availability\":\"current\",\"value\":" + Json_String(item.name) + "}" : "{\"availability\":\"unaudited\"}";
            const std::string owner = item.owner_id.empty() ? "{\"availability\":\"unaudited\"}" : "{\"availability\":\"current\",\"value\":" + Json_String(item.owner_id) + "}";
            return "{\"id\":" + Json_String(snapshot->ship_ids[index]) + ",\"x\":" + std::to_string(item.x) +
                ",\"y\":" + std::to_string(item.y) + ",\"owned\":" + (item.owned ? "true" : "false") + ",\"name\":" + name +
                ",\"owner\":" + owner + ",\"destination\":" + Ship_Knowledge(item.destination_available, Json_String(item.destination_id)) +
                ",\"eta\":" + Ship_Knowledge(item.eta_available, std::to_string(item.eta)) + "}";
        }

        std::string Player(const s_map_snapshot* snapshot, size_t index) {
            const auto& item = snapshot->map.players[index];
            std::string result = "{\"id\":" + Json_String(snapshot->player_ids[index]) + ",\"local\":" + (item.local ? "true" : "false") +
                ",\"contacted\":" + (item.contacted ? "true" : "false") + ",\"eliminated\":" + (item.eliminated ? "true" : "false") +
                ",\"color\":" + std::to_string(item.color) + ",\"name\":{\"availability\":\"current\",\"value\":" + Json_String(item.name) + "},\"atWar\":";
            result += item.at_war_available ? std::string("{\"availability\":\"current\",\"value\":") + (item.at_war ? "true}" : "false}") : "{\"availability\":\"unknown\"}";
            result += ",\"researchTreaty\":";
            result += item.research_treaty_available ? std::string("{\"availability\":\"current\",\"value\":") + (item.research_treaty ? "true}" : "false}") : "{\"availability\":\"unknown\"}";
            result += ",\"treasury\":";
            if (!item.local) return result + "{\"availability\":\"unaudited\"},\"hud\":{\"availability\":\"unaudited\"}}";
            return result + "{\"availability\":\"current\",\"value\":" + std::to_string(item.treasury) +
                "},\"hud\":{\"availability\":\"current\",\"value\":{\"income\":" + std::to_string(item.hud.income) +
                ",\"foodSurplus\":" + std::to_string(item.hud.food_surplus) + ",\"freighters\":" + std::to_string(item.hud.freighters) +
                ",\"freightersAvailable\":" + std::to_string(item.hud.freighters_available) + ",\"commandPoints\":" + std::to_string(item.hud.command_points) +
                ",\"commandPointsAvailable\":" + std::to_string(item.hud.command_points_available) + "}}}";
        }

        std::string Colony(const s_map_snapshot* snapshot, size_t index) {
            const auto& item = snapshot->map.colonies[index];
            return "{\"id\":" + Json_String(snapshot->colony_ids[index]) + ",\"systemId\":" + Json_String(item.system_id) +
                ",\"orbit\":" + std::to_string(item.orbit) + ",\"planetId\":{\"availability\":\"unaudited\"},\"owner\":{\"availability\":\"current\",\"value\":" +
                Json_String(item.owner_id) + "},\"population\":{\"availability\":\"current\",\"value\":" + std::to_string(item.population) +
                "},\"troops\":{\"availability\":\"current\",\"value\":{\"infantry\":" + std::to_string(item.infantry) +
                ",\"armor\":" + std::to_string(item.armor) + "}},\"production\":{\"availability\":\"unaudited\"},\"buyCost\":{\"availability\":\"unaudited\"}}";
        }

        size_t Collection_Size(const s_map_snapshot* snapshot, const std::string& topic) {
            if (topic == "colonies") return snapshot->map.colonies.size();
            if (topic == "ships") return snapshot->map.ships.size();
            if (topic == "players") return snapshot->map.players.size();
            return topic == "systems" ? snapshot->map.systems.size() : snapshot->map.nebulas.size();
        }

        std::string Collection(const s_game_store* store, const s_map_snapshot* snapshot, const std::string& topic, size_t offset, size_t limit) {
            const size_t count = Collection_Size(snapshot, topic);
            std::string result = "[";
            for (size_t index = offset; index < count && index - offset < limit; ++index) {
                if (index != offset) result += ',';
                if (topic == "colonies")
                    result += Colony(snapshot, index);
                else if (topic == "ships")
                    result += Ship(snapshot, index);
                else if (topic == "players")
                    result += Player(snapshot, index);
                else if (topic == "systems")
                    result += System(store, snapshot, index);
                else
                    result += Nebula(store, snapshot, index);
            }
            return result + "]";
        }

        std::string Text(yyjson_val* object, const char* key) {
            yyjson_val* value = yyjson_obj_get(object, key);
            return yyjson_is_str(value) ? std::string(yyjson_get_str(value), yyjson_get_len(value)) : std::string();
        }

        std::string Topic_Projection(const s_game_store* store, const s_map_snapshot* snapshot, const std::vector<std::string>* topics) {
            std::string result;
            for (const std::string& topic : *topics) {
                result += ',' + Json_String(topic) + ':';
                if (topic == "session")
                    result += Session(store, snapshot);
                else
                    result += Collection(store, snapshot, topic, 0, 10000);
            }
            return result;
        }

        std::string Topics_Json(const std::vector<std::string>* topics) {
            std::string result = "[";
            for (const std::string& topic : *topics) {
                if (result.size() != 1) result += ',';
                result += Json_String(topic);
            }
            return result + ']';
        }

        void Prune_Subscriptions(s_game_store* store) {
            std::erase_if(store->subscriptions, [store](const s_state_subscription& item) {
                return !Client_Is_Current(store->bridge, item.client, item.epoch);
            });
        }

        std::string Subscription_Query(s_game_store* store, const s_work* work) {
            const s_request* request = &work->request;
            if (!Validate_Params(&store->protocol, request)) return Error_Response(request, RPC_INVALID_PARAMS);
            yyjson_doc* doc = yyjson_read(request->params_json.data(), request->params_json.size(), YYJSON_READ_NOFLAG);
            if (doc == nullptr) return Error_Response(request, RPC_INVALID_PARAMS);
            yyjson_val* params = yyjson_doc_get_root(doc);
            if (request->method == "rpc.unsubscribe") {
                const std::string id = Text(params, "subscriptionId");
                const size_t removed = std::erase_if(store->subscriptions, [&](const s_state_subscription& item) {
                    return item.client == work->client && item.epoch == work->epoch && item.id == id;
                });
                yyjson_doc_free(doc);
                return Result_Response(request, removed != 0 ? "{\"removed\":true}" : "{\"removed\":false}");
            }
            s_state_subscription subscription;
            subscription.client = work->client;
            subscription.epoch = work->epoch;
            size_t index, count;
            yyjson_val* topic;
            yyjson_arr_foreach(yyjson_obj_get(params, "topics"), index, count, topic) {
                subscription.topics.emplace_back(yyjson_get_str(topic), yyjson_get_len(topic));
            }
            yyjson_arr_foreach(yyjson_obj_get(params, "events"), index, count, topic) {
                const std::string event(yyjson_get_str(topic), yyjson_get_len(topic));
                subscription.state_changes |= event == "state.changed";
            }
            yyjson_doc_free(doc);
            if (store->busy_depth != 0 || !store->loaded || store->snapshots.empty()) return Error_Response(request, RPC_BUSY);
            const bool ships = std::find(subscription.topics.begin(), subscription.topics.end(), "ships") != subscription.topics.end();
            const bool players = std::find(subscription.topics.begin(), subscription.topics.end(), "players") != subscription.topics.end();
            const bool colonies = std::find(subscription.topics.begin(), subscription.topics.end(), "colonies") != subscription.topics.end();
            const auto* snapshot = Select_Snapshot(store, ships, players, colonies);
            if (snapshot == nullptr) return Error_Response(request, RPC_BUSY);
            const size_t client_count = std::count_if(store->subscriptions.begin(), store->subscriptions.end(), [work](const s_state_subscription& item) { return item.client == work->client; });
            if (store->subscriptions.size() >= 64 || client_count >= 16) {
                return Error_Response(request, RPC_LIMIT_EXCEEDED);
            }
            if (!store->new_id(&subscription.id, store->id_context)) return Error_Response(request, RPC_INTERNAL_ERROR);
            subscription.projection = Topic_Projection(store, snapshot, &subscription.topics);
            subscription.revision = store->revision;
            const std::string result = "{\"subscriptionId\":" + Json_String(subscription.id) +
                ",\"cursor\":\"0\",\"baseline\":{\"stamp\":" + Stamp(store, snapshot) + subscription.projection + "}}";
            const std::string response = Result_Response(request, result);
            if (response.size() > 1048576) return Error_Response(request, RPC_LIMIT_EXCEEDED);
            store->subscriptions.push_back(std::move(subscription));
            return response;
        }

        void Publish_State_Changes(s_game_store* store) {
            if (store->busy_depth != 0 || !store->loaded || store->snapshots.empty()) return;
            for (s_state_subscription& subscription : store->subscriptions) {
                if (subscription.revision == store->revision) continue;
                subscription.revision = store->revision;
                if (!subscription.state_changes) continue;
                const bool ships = std::find(subscription.topics.begin(), subscription.topics.end(), "ships") != subscription.topics.end();
                const bool players = std::find(subscription.topics.begin(), subscription.topics.end(), "players") != subscription.topics.end();
                const bool colonies = std::find(subscription.topics.begin(), subscription.topics.end(), "colonies") != subscription.topics.end();
                const auto* snapshot = Select_Snapshot(store, ships, players, colonies);
                if (snapshot == nullptr) {
                    Cancel_Client(store->bridge, subscription.client);
                    continue;
                }
                std::string projection = Topic_Projection(store, snapshot, &subscription.topics);
                if (projection.size() > 1048576) {
                    Cancel_Client(store->bridge, subscription.client);
                    continue;
                }
                if (projection == subscription.projection) continue;
                if (subscription.cursor == std::numeric_limits<uint64_t>::max()) {
                    Cancel_Client(store->bridge, subscription.client);
                    continue;
                }
                const std::string json = "{\"jsonrpc\":\"2.0\",\"method\":\"state.changed\",\"params\":{\"subscriptionId\":" +
                    Json_String(subscription.id) + ",\"cursor\":" + Json_String(std::to_string(++subscription.cursor)) +
                    ",\"topics\":" + Topics_Json(&subscription.topics) + ",\"stamp\":" + Stamp(store, snapshot) + "}}";
                if (Publish_Event(store->bridge, subscription.client, subscription.epoch, &json, &subscription.id)) subscription.projection = std::move(projection);
            }
        }

        void Sync_Turn_Observers(s_game_store* store) {
            std::vector<s_client_identity> clients;
            Get_Authorized_Clients(store->bridge, &clients);
            std::erase_if(store->turn_observers, [&clients](const s_turn_observer& observer) {
                return std::none_of(clients.begin(), clients.end(), [&observer](const s_client_identity& identity) {
                    return identity.client == observer.identity.client && identity.epoch == observer.identity.epoch;
                });
            });
            if (!store->loaded || store->snapshots.empty()) return;
            const int32_t turn = store->snapshots.back().map.turn;
            for (const s_client_identity& identity : clients) {
                auto observer = std::find_if(store->turn_observers.begin(), store->turn_observers.end(), [&identity](const s_turn_observer& item) {
                    return item.identity.client == identity.client && item.identity.epoch == identity.epoch;
                });
                if (observer == store->turn_observers.end()) {
                    store->turn_observers.push_back({identity, turn});
                }
            }
        }

        void Publish_Turn_Changes(s_game_store* store) {
            Sync_Turn_Observers(store);
            if (store->busy_depth != 0 || !store->loaded || store->snapshots.empty()) return;
            const int32_t turn = store->snapshots.back().map.turn;
            for (s_turn_observer& observer : store->turn_observers) {
                if (turn <= observer.turn) continue;
                const std::string json = "{\"jsonrpc\":\"2.0\",\"method\":\"game.turnAdvanced\",\"params\":{\"gameId\":" +
                    Json_String(store->game_id) + ",\"turn\":" + std::to_string(turn) + ",\"revision\":" +
                    Json_String(std::to_string(store->revision)) + "}}";
                if (Publish_Event(store->bridge, observer.identity.client, observer.identity.epoch, &json)) observer.turn = turn;
            }
        }

        std::string Query(s_game_store* store, const s_request* request, yyjson_val* params, int32_t* error) {
            if (request->method == "game.getSession") {
                return Session(store);
            }
            if (store->busy_depth != 0 || store->snapshots.empty() || !store->loaded) {
                *error = RPC_BUSY;
                return {};
            }
            bool ships = request->method == "game.listShips" || request->method == "game.getShip";
            bool players = request->method == "game.listPlayers" || request->method == "game.getPlayer";
            bool colonies = request->method == "game.listColonies" || request->method == "game.getColony";
            if (request->method == "game.getState") {
                size_t index, count;
                yyjson_val* topic;
                yyjson_arr_foreach(yyjson_obj_get(params, "include"), index, count, topic) {
                    ships |= std::string(yyjson_get_str(topic), yyjson_get_len(topic)) == "ships";
                    players |= std::string(yyjson_get_str(topic), yyjson_get_len(topic)) == "players";
                    colonies |= std::string(yyjson_get_str(topic), yyjson_get_len(topic)) == "colonies";
                }
            }
            const s_map_snapshot* snapshot = Select_Snapshot(store, ships, players, colonies);
            if (snapshot == nullptr) {
                *error = Text(params, "cursor").empty() ? RPC_BUSY : RPC_OBJECT_UNAVAILABLE;
                return {};
            }
            if (request->method == "game.getState") {
                std::string result = "{\"stamp\":" + Stamp(store, snapshot);
                yyjson_val* include = yyjson_obj_get(params, "include");
                size_t index, count;
                yyjson_val* topic;
                yyjson_arr_foreach(include, index, count, topic) {
                    const std::string name(yyjson_get_str(topic), yyjson_get_len(topic));
                    if (name == "session")
                        result += ",\"session\":" + Session(store, snapshot);
                    else if (name == "systems")
                        result += ",\"systems\":" + Collection(store, snapshot, name, 0, 1024);
                    else if (name == "nebulas")
                        result += ",\"nebulas\":" + Collection(store, snapshot, name, 0, 57);
                    else if (name == "ships")
                        result += ",\"ships\":" + Collection(store, snapshot, name, 0, 10000);
                    else if (name == "players")
                        result += ",\"players\":" + Collection(store, snapshot, name, 0, 8);
                    else if (name == "colonies")
                        result += ",\"colonies\":" + Collection(store, snapshot, name, 0, 5120);
                    else {
                        *error = RPC_INVALID_PARAMS;
                        return {};
                    }
                }
                return result + "}";
            }
            if (request->method == "game.getSystem" || request->method == "game.getShip" || request->method == "game.getPlayer" || request->method == "game.getColony") {
                const std::string id = Text(params, "id");
                const auto& ids = colonies ? snapshot->colony_ids : (players ? snapshot->player_ids : (ships ? snapshot->ship_ids : store->system_ids));
                for (size_t index = 0; index < ids.size(); ++index) {
                    if (ids[index] == id) {
                        return "{\"stamp\":" + Stamp(store, snapshot) + ",\"item\":" + (colonies ? Colony(snapshot, index) : (players ? Player(snapshot, index) : (ships ? Ship(snapshot, index) : System(store, snapshot, index)))) + "}";
                    }
                }
                *error = RPC_OBJECT_UNAVAILABLE;
                return {};
            }
            if (request->method != "game.listSystems" && request->method != "game.listNebulas" && request->method != "game.listShips" && request->method != "game.listPlayers" && request->method != "game.listColonies") {
                *error = RPC_METHOD_NOT_FOUND;
                return {};
            }
            const std::string topic = colonies ? "colonies" : (players ? "players" : (ships ? "ships" : (request->method == "game.listSystems" ? "systems" : "nebulas")));
            size_t offset = 0;
            yyjson_val* requested_limit = yyjson_obj_get(params, "limit");
            size_t limit = requested_limit != nullptr ? (size_t)yyjson_get_uint(requested_limit) : 64;
            const std::string cursor_id = Text(params, "cursor");
            if (!cursor_id.empty()) {
                const auto cursor = std::find_if(store->cursors.begin(), store->cursors.end(), [&cursor_id](const s_map_cursor& item) { return item.id == cursor_id; });
                if (cursor == store->cursors.end() || cursor->method != request->method ||
                    (requested_limit != nullptr && limit != cursor->limit)) {
                    *error = RPC_OBJECT_UNAVAILABLE;
                    return {};
                }
                const auto pinned = std::find_if(store->snapshots.begin(), store->snapshots.end(), [&cursor](const s_map_snapshot& item) { return item.id == cursor->snapshot; });
                if (pinned == store->snapshots.end()) {
                    *error = RPC_OBJECT_UNAVAILABLE;
                    return {};
                }
                snapshot = &*pinned;
                offset = cursor->offset;
                limit = cursor->limit;
            }
            const size_t total = Collection_Size(snapshot, topic);
            std::string result = "{\"stamp\":" + Stamp(store, snapshot) + ",\"items\":" + Collection(store, snapshot, topic, offset, limit);
            if (offset + limit < total) {
                auto next = std::find_if(store->cursors.begin(), store->cursors.end(), [&](const s_map_cursor& item) {
                    return item.snapshot == snapshot->id && item.method == request->method && item.offset == offset + limit && item.limit == limit;
                });
                if (next == store->cursors.end()) {
                    if (store->cursors.size() >= 64) {
                        *error = RPC_LIMIT_EXCEEDED;
                        return {};
                    }
                    std::string id;
                    if (!store->new_id(&id, store->id_context)) {
                        *error = RPC_INTERNAL_ERROR;
                        return {};
                    }
                    store->cursors.push_back({std::move(id), snapshot->id, request->method, offset + limit, limit});
                    next = std::prev(store->cursors.end());
                }
                result += ",\"nextCursor\":" + Json_String(next->id);
            }
            return result + "}";
        }
    } // namespace

    bool Initialize_Game_Store(s_game_store* store, s_bridge* bridge, t_new_id new_id, void* context) {
        if (store == nullptr || bridge == nullptr || !Initialize_Protocol(&store->protocol)) return false;
        store->bridge = bridge;
        store->new_id = new_id != nullptr ? new_id : Default_Id;
        store->id_context = context;
        return true;
    }

    void Shutdown_Game_Store(s_game_store* store) {
        Shutdown_Protocol(&store->protocol);
        Clear_View(store);
        store->game_id.clear();
        store->loaded = false;
        store->busy_depth = 0;
        store->unavailable = false;
        store->bridge = nullptr;
    }

    void Revoke_Game_View(s_game_store* store) {
        Clear_View(store);
        if (store->bridge != nullptr) Revoke_Bridge(store->bridge);
    }

    void Begin_Game_Reset(s_game_store* store) {
        Revoke_Game_View(store);
        store->game_id.clear();
        store->loaded = false;
        store->unavailable = true;
    }

    bool Commit_Game_Reset(s_game_store* store) {
        if (!store->new_id(&store->game_id, store->id_context)) return false;
        store->loaded = true;
        store->unavailable = false;
        return true;
    }

    void End_Game(s_game_store* store) {
        if (store->loaded || !store->game_id.empty()) Revoke_Game_View(store);
        store->game_id.clear();
        store->loaded = false;
        store->unavailable = false;
    }

    void Pause_Game_Queries(s_game_store* store) {
        ++store->busy_depth;
    }

    void Resume_Game_Queries(s_game_store* store) {
        if (store->busy_depth != 0) --store->busy_depth;
    }

    bool Capture_Map(s_game_store* store, const s_map_observation* observation) {
        if (store == nullptr || observation == nullptr || !store->loaded || store->busy_depth != 0 ||
            observation->viewer < 0 || observation->viewer >= 8 || observation->turn < 0 ||
            observation->systems.size() > 1024 || observation->nebulas.size() > 57 || observation->ships.size() > 10000 || observation->players.size() > 8 || observation->colonies.size() > 5120) return false;
        // Bind newly authorized default observers to the old coherent turn before replacing it.
        Sync_Turn_Observers(store);
        s_map_observation projected = *observation;
        for (s_system_observation& system : projected.systems) {
            system.name_available &= system.visited;
            if (!system.name_available) system.name.clear();
            if (system.name.size() > 4096) return false;
        }
        if (!projected.players_captured) projected.players.clear();
        std::erase_if(projected.players, [](const s_player_observation& player) { return !player.visible; });
        bool slots[8] = {};
        bool has_local = false;
        for (s_player_observation& player : projected.players) {
            if (player.slot < 0 || player.slot >= 8 || slots[player.slot] || player.name.size() > 60 || player.color >= 8) return false;
            slots[player.slot] = true;
            player.local = player.slot == projected.viewer;
            if (player.eliminated) player.contacted = false;
            if (player.local) {
                if (player.eliminated) return false;
                player.contacted = false;
                has_local = true;
            } else {
                player.treasury = 0;
                player.hud = {};
            }
            player.at_war_available &= player.contacted && !player.local && !player.eliminated;
            if (!player.at_war_available) player.at_war = false;
            player.research_treaty_available &= player.contacted && !player.local && !player.eliminated;
            if (!player.research_treaty_available) player.research_treaty = false;
            for (int32_t value : {player.hud.income, player.hud.food_surplus, player.hud.freighters, player.hud.freighters_available, player.hud.command_points, player.hud.command_points_available}) {
                if (value < -32768 || value > 32767) return false;
            }
        }
        if (projected.players_captured && !has_local) return false;
        if (!projected.colonies_captured) projected.colonies.clear();
        if (projected.colonies_captured && (!projected.players_captured || !has_local)) return false;
        std::erase_if(projected.colonies, [&projected](const s_colony_observation& colony) { return colony.owner_slot != projected.viewer; });
        bool occupied[1024][5] = {};
        for (auto& colony : projected.colonies) {
            if (colony.system < 0 || (size_t)colony.system >= projected.systems.size() || colony.orbit < 1 || colony.orbit > 5 ||
                colony.population < 0 || colony.population > 42 || colony.infantry < 0 || colony.infantry > 32767 ||
                colony.armor < 0 || colony.armor > 32767 || occupied[colony.system][colony.orbit - 1]) return false;
            occupied[colony.system][colony.orbit - 1] = true;
            colony.system_id.clear();
            colony.owner_id.clear();
        }
        std::sort(projected.players.begin(), projected.players.end(), [](const s_player_observation& left, const s_player_observation& right) {
            if (left.local != right.local) return left.local;
            return std::tie(left.name, left.color, left.contacted, left.eliminated, left.at_war_available, left.at_war, left.research_treaty_available, left.research_treaty) < std::tie(right.name, right.color, right.contacted, right.eliminated, right.at_war_available, right.at_war, right.research_treaty_available, right.research_treaty);
        });
        if (!projected.ships_captured) projected.ships.clear();
        for (s_ship_observation& ship : projected.ships) {
            if (!ship.owned) ship.name.clear();
            if (ship.name.size() > 48) return false;
            for (e_ship_knowledge available : {ship.destination_available, ship.eta_available}) {
                if (available < SHIP_UNAUDITED || available > SHIP_CONTEXT_REQUIRED) return false;
            }
            if ((ship.destination_available == SHIP_KNOWN_EMPTY || ship.eta_available == SHIP_KNOWN_EMPTY) &&
                (!ship.owned || ship.destination_available != SHIP_KNOWN_EMPTY || ship.eta_available != SHIP_KNOWN_EMPTY)) return false;
            if (ship.destination_available != SHIP_CURRENT)
                ship.destination_system = -1;
            else if (ship.destination_system < 0 || (size_t)ship.destination_system >= projected.systems.size())
                return false;
            ship.destination_id.clear();
            if (ship.eta_available != SHIP_CURRENT)
                ship.eta = 0;
            else if (ship.eta < 0 || ship.eta > 255 || ship.destination_available != SHIP_CURRENT)
                return false;
        }
        store->capture_requested = false;
        if (!store->snapshots.empty() && (projected.systems.size() != store->snapshots.back().map.systems.size() || projected.nebulas.size() != store->snapshots.back().map.nebulas.size())) {
            Revoke_Game_View(store);
        }
        if (store->viewer != projected.viewer) {
            Revoke_Game_View(store);
            if (!store->new_id(&store->perspective_id, store->id_context)) return false;
            store->viewer = projected.viewer;
        }
        auto Extend = [store](std::vector<std::string>* ids, size_t count) {
            while (ids->size() < count) {
                std::string id;
                if (!store->new_id(&id, store->id_context)) return false;
                ids->push_back(std::move(id));
            }
            return true;
        };
        if (!Extend(&store->system_ids, projected.systems.size()) || !Extend(&store->nebula_ids, projected.nebulas.size())) return false;
        std::vector<std::string> player_ids;
        for (const auto& player : projected.players) {
            auto handle = std::find_if(store->player_handles.begin(), store->player_handles.end(), [&player](const s_player_handle& item) { return item.slot == player.slot; });
            if (handle == store->player_handles.end()) {
                std::string id;
                if (!store->new_id(&id, store->id_context)) return false;
                store->player_handles.push_back({player.slot, std::move(id)});
                handle = std::prev(store->player_handles.end());
            }
            player_ids.push_back(handle->id);
        }
        for (auto& ship : projected.ships) {
            ship.owner_id.clear();
            const int32_t owner_slot = ship.owned ? projected.viewer : ship.owner_slot;
            const auto owner = std::find_if(projected.players.begin(), projected.players.end(), [owner_slot](const s_player_observation& player) { return player.slot == owner_slot; });
            if (owner != projected.players.end() && owner->local == ship.owned &&
                (ship.owned || std::count_if(projected.players.begin(), projected.players.end(), [&owner](const s_player_observation& player) { return player.color == owner->color; }) == 1)) {
                ship.owner_id = player_ids[(size_t)(owner - projected.players.begin())];
            }
            ship.owner_slot = -1;
            if (ship.destination_available == SHIP_CURRENT) ship.destination_id = store->system_ids[(size_t)ship.destination_system];
            ship.destination_system = -1;
        }
        for (auto& colony : projected.colonies) {
            colony.system_id = store->system_ids[(size_t)colony.system];
            const auto local = std::find_if(projected.players.begin(), projected.players.end(), [](const s_player_observation& player) { return player.local; });
            colony.owner_id = player_ids[(size_t)(local - projected.players.begin())];
            colony.system = colony.owner_slot = -1;
        }
        std::sort(projected.colonies.begin(), projected.colonies.end(), [](const s_colony_observation& left, const s_colony_observation& right) {
            return std::tie(left.system_id, left.orbit, left.population, left.owner_id, left.infantry, left.armor) < std::tie(right.system_id, right.orbit, right.population, right.owner_id, right.infantry, right.armor);
        });
        std::sort(projected.ships.begin(), projected.ships.end(), [](const s_ship_observation& left, const s_ship_observation& right) {
            return std::tie(left.x, left.y, left.owned, left.name, left.owner_id, left.destination_available, left.destination_id, left.eta_available, left.eta) <
                std::tie(right.x, right.y, right.owned, right.name, right.owner_id, right.destination_available, right.destination_id, right.eta_available, right.eta);
        });
        if (!store->snapshots.empty() && Same_Map(&projected, &store->snapshots.back().map) &&
            (!projected.ships_captured || Same_Ships(&projected, &store->snapshots.back().map)) &&
            (!projected.players_captured || (Same_Players(&projected, &store->snapshots.back().map) && player_ids == store->snapshots.back().player_ids)) &&
            (!projected.colonies_captured || Same_Colonies(&projected, &store->snapshots.back().map))) return true;
        if (store->revision == std::numeric_limits<uint64_t>::max()) return false;
        s_map_snapshot snapshot;
        if (!store->new_id(&snapshot.id, store->id_context)) return false;
        snapshot.map = std::move(projected);
        snapshot.player_ids = std::move(player_ids);
        const auto* previous_ships = Select_Snapshot(store, true);
        if (snapshot.map.ships_captured && previous_ships != nullptr && Same_Ships(&snapshot.map, &previous_ships->map)) {
            snapshot.ship_ids = previous_ships->ship_ids;
        }
        if (!Extend(&snapshot.ship_ids, snapshot.map.ships.size())) return false;
        const auto* previous_colonies = Select_Snapshot(store, false, false, true);
        if (snapshot.map.colonies_captured && previous_colonies != nullptr && Same_Colonies(&snapshot.map, &previous_colonies->map)) {
            snapshot.colony_ids = previous_colonies->colony_ids;
        }
        if (!Extend(&snapshot.colony_ids, snapshot.map.colonies.size())) return false;
        snapshot.revision = ++store->revision;
        store->snapshots.push_back(std::move(snapshot));
        if (store->snapshots.size() > 4) store->snapshots.pop_front();
        std::erase_if(store->cursors, [store](const s_map_cursor& cursor) {
            return std::none_of(store->snapshots.begin(), store->snapshots.end(), [&cursor](const s_map_snapshot& item) { return item.id == cursor.snapshot; });
        });
        return true;
    }

    std::string Game_Query(s_game_store* store, const s_request* request) {
        if (!Validate_Params(&store->protocol, request)) return Error_Response(request, RPC_INVALID_PARAMS);
        yyjson_doc* params = yyjson_read(request->params_json.data(), request->params_json.size(), YYJSON_READ_NOFLAG);
        if (params == nullptr) return Error_Response(request, RPC_INVALID_PARAMS);
        int32_t error = 0;
        const std::string result = Query(store, request, yyjson_doc_get_root(params), &error);
        yyjson_doc_free(params);
        if (error != 0) return Error_Response(request, error);
        const std::string response = Result_Response(request, result);
        return response.size() > 1048576 ? Error_Response(request, RPC_LIMIT_EXCEEDED) : response;
    }

    void Pump_Game_Queries(s_game_store* store) {
        if (store == nullptr || store->bridge == nullptr) return;
        Prune_Subscriptions(store);
        const uint64_t started = Monotonic_Us();
        for (uint32_t count = 0; count < Protocol_Limits()->commands_per_game_poll; ++count) {
            if (Monotonic_Us() - started >= Protocol_Limits()->game_poll_budget_us) break;
            s_work work;
            if (!Take_Work(store->bridge, Monotonic_Ms(), &work)) break;
            if (work.request.method != "rpc.unsubscribe") store->capture_requested = true;
            const size_t previous_subscriptions = store->subscriptions.size();
            std::string response = work.request.method == "rpc.subscribe" || work.request.method == "rpc.unsubscribe" ? Subscription_Query(store, &work) : Game_Query(store, &work.request);
            const bool expired = Monotonic_Ms() >= work.deadline_ms;
            if (expired) response = Error_Response(&work.request, RPC_REQUEST_EXPIRED);
            const bool completed = Complete_Work(store->bridge, &work, &response);
            if ((expired || !completed) && store->subscriptions.size() > previous_subscriptions) store->subscriptions.pop_back();
        }
        Publish_Turn_Changes(store);
        Publish_State_Changes(store);
    }

    bool Needs_Game_Capture(s_game_store* store) {
        if (store == nullptr || store->bridge == nullptr) return false;
        Prune_Subscriptions(store);
        return store->capture_requested || Has_Pending_Work(store->bridge) ||
            std::any_of(store->subscriptions.begin(), store->subscriptions.end(), [](const s_state_subscription& item) { return item.state_changes; });
    }
} // namespace rpc
