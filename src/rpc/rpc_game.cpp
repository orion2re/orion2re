#include "rpc_game.h"

#include <algorithm>
#include <limits>
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

        std::string Session(const s_game_store* store) {
            if (store->snapshots.empty()) {
                return store->loaded || store->unavailable ? "{\"phase\":\"unavailable\"}" : "{\"phase\":\"menu\"}";
            }
            const s_map_snapshot& snapshot = store->snapshots.back();
            return "{\"gameId\":" + Json_String(store->game_id) + ",\"perspectiveId\":" + Json_String(store->perspective_id) +
                ",\"revision\":" + Json_String(std::to_string(snapshot.revision)) + ",\"turn\":" + std::to_string(snapshot.map.turn) +
                ",\"phase\":" + Json_String(store->busy_depth != 0 ? "resolving" : "committed") + "}";
        }

        std::string Stamp(const s_game_store* store, const s_map_snapshot* snapshot) {
            return "{\"gameId\":" + Json_String(store->game_id) + ",\"perspectiveId\":" + Json_String(store->perspective_id) +
                ",\"snapshotId\":" + Json_String(snapshot->id) + ",\"revision\":" + Json_String(std::to_string(snapshot->revision)) +
                ",\"turn\":" + std::to_string(snapshot->map.turn) + ",\"captureMode\":\"last-coherent-capture\"}";
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

        std::string Collection(const s_game_store* store, const s_map_snapshot* snapshot, bool systems, size_t offset, size_t limit) {
            const size_t count = systems ? snapshot->map.systems.size() : snapshot->map.nebulas.size();
            std::string result = "[";
            for (size_t index = offset; index < count && index - offset < limit; ++index) {
                if (index != offset) result += ',';
                result += systems ? System(store, snapshot, index) : Nebula(store, snapshot, index);
            }
            return result + "]";
        }

        std::string Text(yyjson_val* object, const char* key) {
            yyjson_val* value = yyjson_obj_get(object, key);
            return yyjson_is_str(value) ? std::string(yyjson_get_str(value), yyjson_get_len(value)) : std::string();
        }

        std::string Topic_Projection(const s_game_store* store, const std::vector<std::string>* topics) {
            const auto* snapshot = &store->snapshots.back();
            std::string result;
            for (const std::string& topic : *topics) {
                result += ',' + Json_String(topic) + ':';
                if (topic == "session")
                    result += Session(store);
                else
                    result += Collection(store, snapshot, topic == "systems", 0, topic == "systems" ? 1024 : 57);
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
            const size_t client_count = std::count_if(store->subscriptions.begin(), store->subscriptions.end(), [work](const s_state_subscription& item) { return item.client == work->client; });
            if (store->subscriptions.size() >= 64 || client_count >= 16) {
                return Error_Response(request, RPC_LIMIT_EXCEEDED);
            }
            if (!store->new_id(&subscription.id, store->id_context)) return Error_Response(request, RPC_INTERNAL_ERROR);
            subscription.projection = Topic_Projection(store, &subscription.topics);
            subscription.revision = store->revision;
            const std::string result = "{\"subscriptionId\":" + Json_String(subscription.id) +
                ",\"cursor\":\"0\",\"baseline\":{\"stamp\":" + Stamp(store, &store->snapshots.back()) + subscription.projection + "}}";
            store->subscriptions.push_back(std::move(subscription));
            return Result_Response(request, result);
        }

        void Publish_State_Changes(s_game_store* store) {
            if (store->busy_depth != 0 || !store->loaded || store->snapshots.empty()) return;
            for (s_state_subscription& subscription : store->subscriptions) {
                if (subscription.revision == store->revision) continue;
                subscription.revision = store->revision;
                if (!subscription.state_changes) continue;
                std::string projection = Topic_Projection(store, &subscription.topics);
                if (projection == subscription.projection) continue;
                if (subscription.cursor == std::numeric_limits<uint64_t>::max()) {
                    Cancel_Client(store->bridge, subscription.client);
                    continue;
                }
                const std::string json = "{\"jsonrpc\":\"2.0\",\"method\":\"state.changed\",\"params\":{\"subscriptionId\":" +
                    Json_String(subscription.id) + ",\"cursor\":" + Json_String(std::to_string(++subscription.cursor)) +
                    ",\"topics\":" + Topics_Json(&subscription.topics) + ",\"stamp\":" + Stamp(store, &store->snapshots.back()) + "}}";
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
            const s_map_snapshot* snapshot = &store->snapshots.back();
            if (request->method == "game.getState") {
                std::string result = "{\"stamp\":" + Stamp(store, snapshot);
                yyjson_val* include = yyjson_obj_get(params, "include");
                size_t index, count;
                yyjson_val* topic;
                yyjson_arr_foreach(include, index, count, topic) {
                    const std::string name(yyjson_get_str(topic), yyjson_get_len(topic));
                    if (name == "session")
                        result += ",\"session\":" + Session(store);
                    else if (name == "systems")
                        result += ",\"systems\":" + Collection(store, snapshot, true, 0, 1024);
                    else if (name == "nebulas")
                        result += ",\"nebulas\":" + Collection(store, snapshot, false, 0, 57);
                    else {
                        *error = RPC_INVALID_PARAMS;
                        return {};
                    }
                }
                return result + "}";
            }
            if (request->method == "game.getSystem") {
                const std::string id = Text(params, "id");
                for (size_t index = 0; index < snapshot->map.systems.size(); ++index) {
                    if (store->system_ids[index] == id) {
                        return "{\"stamp\":" + Stamp(store, snapshot) + ",\"item\":" + System(store, snapshot, index) + "}";
                    }
                }
                *error = RPC_OBJECT_UNAVAILABLE;
                return {};
            }
            if (request->method != "game.listSystems" && request->method != "game.listNebulas") {
                *error = RPC_METHOD_NOT_FOUND;
                return {};
            }
            const bool systems = request->method == "game.listSystems";
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
            const size_t total = systems ? snapshot->map.systems.size() : snapshot->map.nebulas.size();
            std::string result = "{\"stamp\":" + Stamp(store, snapshot) + ",\"items\":" + Collection(store, snapshot, systems, offset, limit);
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
            observation->systems.size() > 1024 || observation->nebulas.size() > 57) return false;
        // Bind newly authorized default observers to the old coherent turn before replacing it.
        Sync_Turn_Observers(store);
        s_map_observation projected = *observation;
        for (s_system_observation& system : projected.systems) {
            system.name_available &= system.visited;
            if (!system.name_available) system.name.clear();
            if (system.name.size() > 4096) return false;
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
        if (!store->snapshots.empty() && Same_Map(&projected, &store->snapshots.back().map)) return true;
        if (store->revision == std::numeric_limits<uint64_t>::max()) return false;
        auto Extend = [store](std::vector<std::string>* ids, size_t count) {
            while (ids->size() < count) {
                std::string id;
                if (!store->new_id(&id, store->id_context)) return false;
                ids->push_back(std::move(id));
            }
            return true;
        };
        if (!Extend(&store->system_ids, projected.systems.size()) || !Extend(&store->nebula_ids, projected.nebulas.size())) return false;
        s_map_snapshot snapshot;
        if (!store->new_id(&snapshot.id, store->id_context)) return false;
        snapshot.map = std::move(projected);
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
        return error != 0 ? Error_Response(request, error) : Result_Response(request, result);
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
