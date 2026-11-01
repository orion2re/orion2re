#include "rpc_game.h"

#include <algorithm>
#include <limits>
#include <tuple>
#include <utility>

#include "yyjson.h"

namespace rpc {
    namespace {
        enum e_topic : uint32_t {
            TOPIC_NONE = 0,
            TOPIC_SESSION = 1 << 0,
            TOPIC_SYSTEMS = 1 << 1,
            TOPIC_NEBULAS = 1 << 2,
            TOPIC_SHIPS = 1 << 3,
            TOPIC_PLAYERS = 1 << 4,
            TOPIC_COLONIES = 1 << 5,
            TOPIC_PLANETS = 1 << 6,
            TOPIC_RESEARCH = 1 << 7,
        };

        struct s_topic_methods {
            e_topic topic;
            const char* name;
            const char* list;
            const char* get;
        };

        constexpr s_topic_methods TOPICS[] = {
            {TOPIC_SESSION, "session", nullptr, "game.getSession"},
            {TOPIC_SYSTEMS, "systems", "game.listSystems", "game.getSystem"},
            {TOPIC_NEBULAS, "nebulas", "game.listNebulas", nullptr},
            {TOPIC_SHIPS, "ships", "game.listShips", "game.getShip"},
            {TOPIC_PLAYERS, "players", "game.listPlayers", "game.getPlayer"},
            {TOPIC_COLONIES, "colonies", "game.listColonies", "game.getColony"},
            {TOPIC_PLANETS, "planets", "game.listPlanets", "game.getPlanet"},
            {TOPIC_RESEARCH, "research", "game.listResearchOptions", "game.getTechnology"},
        };

        e_topic Find_Topic(const std::string& name) {
            for (const auto& topic : TOPICS) {
                if (name == topic.name) return topic.topic;
            }
            return TOPIC_NONE;
        }

        const s_topic_methods* Find_Query_Topic(const std::string& method) {
            for (const auto& topic : TOPICS) {
                if (topic.topic == TOPIC_RESEARCH && method == "game.getResearch") return &topic;
                if ((topic.list != nullptr && method == topic.list) ||
                    (topic.get != nullptr && method == topic.get)) return &topic;
            }
            return nullptr;
        }

        uint32_t Topic_Mask(const std::vector<std::string>* topics) {
            uint32_t mask = TOPIC_NONE;
            for (const auto& topic : *topics) mask |= Find_Topic(topic);
            return mask;
        }

        std::vector<std::string> Read_Topics(yyjson_val* array) {
            std::vector<std::string> topics;
            size_t index, count;
            yyjson_val* topic;
            yyjson_arr_foreach(array, index, count, topic) {
                topics.emplace_back(yyjson_get_str(topic), yyjson_get_len(topic));
            }
            return topics;
        }

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
            store->planet_handles.clear();
            store->snapshots.clear();
            store->cursors.clear();
            store->subscriptions.clear();
            store->turn_observers.clear();
            store->capture_requested = false;
            store->context_active = false;
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
                    a.name_available != b.name_available || a.name != b.name ||
                    std::tie(a.spectral_class, a.size, a.explored, a.owner_id, a.wormhole_available, a.wormhole_id, a.in_extended_range, a.planet_ids) !=
                        std::tie(b.spectral_class, b.size, b.explored, b.owner_id, b.wormhole_available, b.wormhole_id, b.in_extended_range, b.planet_ids)) {
                    return false;
                }
            }
            for (size_t index = 0; index < left->nebulas.size(); ++index) {
                if (left->nebulas[index].x != right->nebulas[index].x || left->nebulas[index].y != right->nebulas[index].y || left->nebulas[index].variant != right->nebulas[index].variant) {
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
                if (std::tie(a.local, a.contacted, a.eliminated, a.color, a.name, a.treasury, a.hud.income, a.hud.food_surplus, a.hud.freighters, a.hud.freighters_available, a.hud.command_points, a.hud.command_points_available, a.at_war_available, a.at_war, a.research_treaty_available, a.research_treaty, a.trade_treaty_available, a.trade_treaty) !=
                    std::tie(b.local, b.contacted, b.eliminated, b.color, b.name, b.treasury, b.hud.income, b.hud.food_surplus, b.hud.freighters, b.hud.freighters_available, b.hud.command_points, b.hud.command_points_available, b.at_war_available, b.at_war, b.research_treaty_available, b.research_treaty, b.trade_treaty_available, b.trade_treaty)) return false;
            }
            return true;
        }

        bool Same_Colonies(const s_map_observation* left, const s_map_observation* right) {
            if (left->colonies_captured != right->colonies_captured || left->colonies.size() != right->colonies.size()) return false;
            for (size_t index = 0; index < left->colonies.size(); ++index) {
                const auto& a = left->colonies[index];
                const auto& b = right->colonies[index];
                if (std::tie(a.system_id, a.orbit, a.population, a.owner_id, a.infantry, a.armor, a.planet_id) != std::tie(b.system_id, b.orbit, b.population, b.owner_id, b.infantry, b.armor, b.planet_id)) return false;
            }
            return true;
        }

        bool Same_Planets(const s_map_observation* left, const s_map_observation* right) {
            if (left->planets_captured != right->planets_captured || left->planets.size() != right->planets.size()) return false;
            for (size_t index = 0; index < left->planets.size(); ++index) {
                const auto& a = left->planets[index];
                const auto& b = right->planets[index];
                if (std::tie(a.system_id, a.orbit, a.kind, a.name, a.size, a.gravity, a.climate, a.minerals, a.special, a.food2, a.industry, a.gravity_penalty, a.max_population_available, a.max_population, a.colony_id) !=
                    std::tie(b.system_id, b.orbit, b.kind, b.name, b.size, b.gravity, b.climate, b.minerals, b.special, b.food2, b.industry, b.gravity_penalty, b.max_population_available, b.max_population, b.colony_id)) return false;
            }
            return true;
        }

        bool Same_Research(const s_map_observation* left, const s_map_observation* right) {
            if (left->research_captured != right->research_captured) return false;
            const auto& a = left->research;
            const auto& b = right->research;
            if (std::tie(a.mode, a.project, a.application) != std::tie(b.mode, b.project, b.application) ||
                a.technologies.size() != b.technologies.size() || a.options.size() != b.options.size()) return false;
            for (size_t index = 0; index < a.technologies.size(); ++index) {
                const auto& x = a.technologies[index];
                const auto& y = b.technologies[index];
                if (std::tie(x.application, x.name, x.cost_available, x.cost) != std::tie(y.application, y.name, y.cost_available, y.cost)) return false;
            }
            for (size_t index = 0; index < a.options.size(); ++index) {
                const auto& x = a.options[index];
                const auto& y = b.options[index];
                if (std::tie(x.field, x.application, x.remaining) != std::tie(y.field, y.application, y.remaining)) return false;
            }
            return true;
        }

        bool Authorized_Snapshot(const s_game_store* store, const s_map_snapshot* snapshot) {
            return snapshot->map.context_epoch == 0 ||
                (store->context_active && snapshot->map.context_epoch == Bridge_Context_Epoch(store->bridge));
        }

        const s_map_snapshot* Select_Snapshot(const s_game_store* store, uint32_t topics) {
            for (auto snapshot = store->snapshots.rbegin(); snapshot != store->snapshots.rend(); ++snapshot) {
                if ((topics & TOPIC_SHIPS) && !Authorized_Snapshot(store, &*snapshot)) continue;
                if ((!(topics & TOPIC_SHIPS) || snapshot->map.ships_captured) &&
                    (!(topics & TOPIC_PLAYERS) || snapshot->map.players_captured) &&
                    (!(topics & TOPIC_COLONIES) || snapshot->map.colonies_captured) &&
                    (!(topics & TOPIC_PLANETS) || snapshot->map.planets_captured) &&
                    (!(topics & TOPIC_RESEARCH) || snapshot->map.research_captured)) return &*snapshot;
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
                Json_String(snapshot->map.context_epoch != 0 ? "last-native-fleet-box" : (snapshot->map.ships_captured ? "last-native-ship-refresh" : "last-coherent-capture")) + "}";
        }

        std::string Exploration_Knowledge(e_exploration_knowledge available, const std::string& value = {}) {
            switch (available) {
                case EXPLORATION_CURRENT: return "{\"availability\":\"current\",\"value\":" + value + "}";
                case EXPLORATION_EMPTY: return "{\"availability\":\"known-empty\"}";
                case EXPLORATION_UNAUDITED: return "{\"availability\":\"unaudited\"}";
                default: return "{\"availability\":\"unknown\"}";
            }
        }

        std::string Id_Knowledge(const std::string& id) {
            return Exploration_Knowledge(id.empty() ? EXPLORATION_UNKNOWN : EXPLORATION_CURRENT, Json_String(id));
        }

        std::string System(const s_game_store* store, const s_map_snapshot* snapshot, size_t index) {
            const s_system_observation& item = snapshot->map.systems[index];
            std::string result = "{\"id\":" + Json_String(store->system_ids[index]) + ",\"x\":" + std::to_string(item.x) +
                ",\"y\":" + std::to_string(item.y) + ",\"visited\":" + (item.visited ? "true" : "false") + ",\"name\":";
            result += item.name_available ? "{\"availability\":\"current\",\"value\":" + Json_String(item.name) + "}" : "{\"availability\":\"unknown\"}";
            result += ",\"spectralClass\":" + std::to_string(item.spectral_class) + ",\"size\":" + std::to_string(item.size) +
                ",\"owner\":" + Id_Knowledge(item.owner_id) + ",\"wormhole\":" + Exploration_Knowledge(item.wormhole_available, Json_String(item.wormhole_id)) +
                ",\"inExtendedRange\":" + Exploration_Knowledge(item.explored ? EXPLORATION_CURRENT : EXPLORATION_UNKNOWN, item.in_extended_range ? "true" : "false");
            std::string planets = "[";
            for (const auto& id : item.planet_ids) {
                if (planets.size() != 1) planets += ',';
                planets += Json_String(id);
            }
            return result + ",\"planetIds\":" + Exploration_Knowledge(item.explored ? EXPLORATION_CURRENT : EXPLORATION_UNKNOWN, planets + "]") + "}";
        }

        std::string Nebula(const s_game_store* store, const s_map_snapshot* snapshot, size_t index) {
            const s_nebula_observation& item = snapshot->map.nebulas[index];
            return "{\"id\":" + Json_String(store->nebula_ids[index]) + ",\"x\":" + std::to_string(item.x) + ",\"y\":" + std::to_string(item.y) + ",\"variant\":" + std::to_string(item.variant) + "}";
        }

        std::string Planet(const s_map_snapshot* snapshot, size_t index) {
            const auto& item = snapshot->map.planets[index];
            const auto details = item.kind == 3 ? EXPLORATION_CURRENT : EXPLORATION_EMPTY;
            const char* kind = item.kind == 1 ? "asteroid" : (item.kind == 2 ? "gas-giant" : "planet");
            std::string result = "{\"id\":" + Json_String(snapshot->planet_ids[index]) + ",\"systemId\":" + Json_String(item.system_id) +
                ",\"orbit\":" + std::to_string(item.orbit) + ",\"kind\":" + Exploration_Knowledge(EXPLORATION_CURRENT, Json_String(kind)) +
                ",\"name\":" + Exploration_Knowledge(details, Json_String(item.name));
            const std::pair<const char*, int32_t> fields[] = {{"size", item.size}, {"gravity", item.gravity}, {"climate", item.climate}, {"minerals", item.minerals}, {"foodPerFarmer2", item.food2}, {"mineralsPerMine", item.industry}, {"gravityPenalty", item.gravity_penalty}};
            for (const auto& field : fields) result += ',' + Json_String(field.first) + ':' + Exploration_Knowledge(details, std::to_string(field.second));
            return result + ",\"special\":" + Exploration_Knowledge(item.kind == 3 && item.special != 0 ? EXPLORATION_CURRENT : EXPLORATION_EMPTY, std::to_string(item.special)) +
                ",\"hostileEnvironment\":" + Exploration_Knowledge(details, item.food2 == 0 ? "true" : "false") +
                ",\"maxPopulation\":" + Exploration_Knowledge(item.kind != 3 ? EXPLORATION_EMPTY : (item.max_population_available ? EXPLORATION_CURRENT : EXPLORATION_UNAUDITED), std::to_string(item.max_population)) +
                ",\"colony\":" + Id_Knowledge(item.colony_id) + "}";
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
            result += ",\"tradeTreaty\":";
            result += item.trade_treaty_available ? std::string("{\"availability\":\"current\",\"value\":") + (item.trade_treaty ? "true}" : "false}") : "{\"availability\":\"unknown\"}";
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
                ",\"orbit\":" + std::to_string(item.orbit) + ",\"planetId\":" + Id_Knowledge(item.planet_id) + ",\"owner\":{\"availability\":\"current\",\"value\":" +
                Json_String(item.owner_id) + "},\"population\":{\"availability\":\"current\",\"value\":" + std::to_string(item.population) +
                "},\"troops\":{\"availability\":\"current\",\"value\":{\"infantry\":" + std::to_string(item.infantry) +
                ",\"armor\":" + std::to_string(item.armor) + "}},\"production\":{\"availability\":\"unaudited\"},\"buyCost\":{\"availability\":\"unaudited\"}}";
        }

        std::string Research_Id(const s_map_snapshot* snapshot, int32_t index) {
            return index < 0 ? Exploration_Knowledge(EXPLORATION_EMPTY) : Exploration_Knowledge(EXPLORATION_CURRENT, Json_String(snapshot->technology_ids[(size_t)index]));
        }

        std::string Research_Cost(const s_technology_observation* technology) {
            return Exploration_Knowledge(technology == nullptr ? EXPLORATION_EMPTY : (technology->cost_available ? EXPLORATION_CURRENT : EXPLORATION_UNAUDITED),
                technology != nullptr ? std::to_string(technology->cost) : std::string());
        }

        std::string Research_Option(const s_map_snapshot* snapshot, size_t index) {
            const auto& option = snapshot->map.research.options[index];
            const auto& technology = snapshot->map.research.technologies[(size_t)(option.application >= 0 ? option.application : option.field)];
            return "{\"id\":" + Json_String(snapshot->research_option_ids[index]) +
                ",\"field\":" + Json_String(snapshot->technology_ids[(size_t)option.field]) +
                ",\"application\":" + Research_Id(snapshot, option.application) +
                ",\"label\":" + Json_String(technology.name) + ",\"remainingCost\":" + std::to_string(option.remaining) + "}";
        }

        std::string Research(const s_game_store* store, const s_map_snapshot* snapshot, bool stamped = true) {
            const auto& research = snapshot->map.research;
            std::string result = stamped ? "{\"stamp\":" + Stamp(store, snapshot) + ',' : "{";
            result += "\"mode\":" + Json_String(research.mode) + ",\"project\":" + Research_Id(snapshot, research.project) +
                ",\"application\":" + Research_Id(snapshot, research.application) +
                ",\"progress\":{\"availability\":\"unaudited\"},\"cost\":" + Research_Cost(research.project >= 0 ? &research.technologies[(size_t)research.project] : nullptr) +
                ",\"options\":[";
            for (size_t index = 0; index < research.options.size(); ++index) {
                if (index != 0) result += ',';
                result += Research_Option(snapshot, index);
            }
            return result + "]}";
        }

        std::string Technology(const s_game_store* store, const s_map_snapshot* snapshot, size_t index) {
            const auto& technology = snapshot->map.research.technologies[index];
            return "{\"stamp\":" + Stamp(store, snapshot) + ",\"id\":" + Json_String(snapshot->technology_ids[index]) +
                ",\"kind\":" + Json_String(technology.application ? "application" : "field") +
                ",\"name\":" + Exploration_Knowledge(EXPLORATION_CURRENT, Json_String(technology.name)) +
                ",\"description\":{\"availability\":\"unaudited\"},\"cost\":" + Research_Cost(&technology) + "}";
        }

        const std::vector<std::string>* Collection_Ids(const s_game_store* store, const s_map_snapshot* snapshot, e_topic topic) {
            switch (topic) {
                case TOPIC_SYSTEMS: return &store->system_ids;
                case TOPIC_NEBULAS: return &store->nebula_ids;
                case TOPIC_SHIPS: return &snapshot->ship_ids;
                case TOPIC_PLAYERS: return &snapshot->player_ids;
                case TOPIC_COLONIES: return &snapshot->colony_ids;
                case TOPIC_PLANETS: return &snapshot->planet_ids;
                case TOPIC_RESEARCH: return &snapshot->research_option_ids;
                default: return nullptr;
            }
        }

        std::string Collection_Item(const s_game_store* store, const s_map_snapshot* snapshot, e_topic topic, size_t index) {
            switch (topic) {
                case TOPIC_SYSTEMS: return System(store, snapshot, index);
                case TOPIC_NEBULAS: return Nebula(store, snapshot, index);
                case TOPIC_SHIPS: return Ship(snapshot, index);
                case TOPIC_PLAYERS: return Player(snapshot, index);
                case TOPIC_COLONIES: return Colony(snapshot, index);
                case TOPIC_PLANETS: return Planet(snapshot, index);
                case TOPIC_RESEARCH: return Research_Option(snapshot, index);
                default: return {};
            }
        }

        std::string Collection(const s_game_store* store, const s_map_snapshot* snapshot, e_topic topic, size_t offset, size_t limit) {
            const auto* ids = Collection_Ids(store, snapshot, topic);
            const size_t count = ids != nullptr ? ids->size() : 0;
            std::string result = "[";
            for (size_t index = offset; index < count && index - offset < limit; ++index) {
                if (index != offset) result += ',';
                result += Collection_Item(store, snapshot, topic, index);
            }
            return result + "]";
        }

        std::string Text(yyjson_val* object, const char* key) {
            yyjson_val* value = yyjson_obj_get(object, key);
            return yyjson_is_str(value) ? std::string(yyjson_get_str(value), yyjson_get_len(value)) : std::string();
        }

        std::string Topic_Projection(const s_game_store* store, const s_map_snapshot* snapshot, const std::vector<std::string>* topics, bool stamped = false) {
            std::string result;
            for (const std::string& topic : *topics) {
                result += ',' + Json_String(topic) + ':';
                const e_topic selected = Find_Topic(topic);
                if (selected == TOPIC_SESSION)
                    result += Session(store, snapshot);
                else if (selected == TOPIC_RESEARCH)
                    result += Research(store, snapshot, stamped);
                else
                    result += Collection(store, snapshot, selected, 0, 10000);
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

        std::string Subscription_Query(s_game_store* store, const s_work* work, uint64_t* context_epoch) {
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
            subscription.topics = Read_Topics(yyjson_obj_get(params, "topics"));
            size_t index, count;
            yyjson_val* topic;
            yyjson_arr_foreach(yyjson_obj_get(params, "events"), index, count, topic) {
                const std::string event(yyjson_get_str(topic), yyjson_get_len(topic));
                subscription.state_changes |= event == "state.changed";
            }
            yyjson_doc_free(doc);
            if (store->busy_depth != 0 || !store->loaded || store->snapshots.empty()) return Error_Response(request, RPC_BUSY);
            const uint32_t topics = Topic_Mask(&subscription.topics);
            const auto* snapshot = Select_Snapshot(store, topics);
            if (snapshot == nullptr) return Error_Response(request, RPC_BUSY);
            if (topics & TOPIC_SHIPS) *context_epoch = snapshot->map.context_epoch;
            if (*context_epoch != 0) subscription.baseline_ticket = work->ticket;
            const size_t client_count = std::count_if(store->subscriptions.begin(), store->subscriptions.end(), [work](const s_state_subscription& item) { return item.client == work->client; });
            if (store->subscriptions.size() >= 64 || client_count >= 16) {
                return Error_Response(request, RPC_LIMIT_EXCEEDED);
            }
            if (!store->new_id(&subscription.id, store->id_context)) return Error_Response(request, RPC_INTERNAL_ERROR);
            subscription.projection = Topic_Projection(store, snapshot, &subscription.topics);
            subscription.revision = store->revision;
            const std::string result = "{\"subscriptionId\":" + Json_String(subscription.id) +
                ",\"cursor\":\"0\",\"baseline\":{\"stamp\":" + Stamp(store, snapshot) + Topic_Projection(store, snapshot, &subscription.topics, true) + "}}";
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
                const uint32_t topics = Topic_Mask(&subscription.topics);
                const auto* snapshot = Select_Snapshot(store, topics);
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
                if (Publish_Event(store->bridge, subscription.client, subscription.epoch, &json, &subscription.id, (topics & TOPIC_SHIPS) ? snapshot->map.context_epoch : 0)) subscription.projection = std::move(projection);
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

        std::string Query(s_game_store* store, const s_request* request, yyjson_val* params, int32_t* error, uint64_t* context_epoch) {
            if (request->method == "game.getSession") {
                return Session(store);
            }
            if (store->busy_depth != 0 || store->snapshots.empty() || !store->loaded) {
                *error = RPC_BUSY;
                return {};
            }
            const bool state = request->method == "game.getState";
            const auto* method = Find_Query_Topic(request->method);
            const auto selected = state ? Read_Topics(yyjson_obj_get(params, "include")) : std::vector<std::string>();
            const uint32_t topics = state ? Topic_Mask(&selected) : (method != nullptr ? method->topic : TOPIC_NONE);
            const s_map_snapshot* snapshot = Select_Snapshot(store, topics);
            if (snapshot == nullptr) {
                *error = Text(params, "cursor").empty() ? RPC_BUSY : RPC_OBJECT_UNAVAILABLE;
                return {};
            }
            if (topics & TOPIC_SHIPS) *context_epoch = snapshot->map.context_epoch;
            if (state) {
                return "{\"stamp\":" + Stamp(store, snapshot) + Topic_Projection(store, snapshot, &selected, true) + "}";
            }
            if (request->method == "game.getResearch") return Research(store, snapshot);
            if (request->method == "game.getTechnology") {
                const std::string id = Text(params, "id");
                for (size_t index = 0; index < snapshot->technology_ids.size(); ++index) {
                    if (snapshot->technology_ids[index] == id) return Technology(store, snapshot, index);
                }
                *error = RPC_OBJECT_UNAVAILABLE;
                return {};
            }
            if (method == nullptr) {
                *error = RPC_METHOD_NOT_FOUND;
                return {};
            }
            const auto* ids = Collection_Ids(store, snapshot, method->topic);
            if (method->get != nullptr && request->method == method->get) {
                const std::string id = Text(params, "id");
                for (size_t index = 0; index < ids->size(); ++index) {
                    if ((*ids)[index] == id) {
                        return "{\"stamp\":" + Stamp(store, snapshot) + ",\"item\":" + Collection_Item(store, snapshot, method->topic, index) + "}";
                    }
                }
                *error = RPC_OBJECT_UNAVAILABLE;
                return {};
            }
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
                if (pinned == store->snapshots.end() || ((topics & TOPIC_SHIPS) && !Authorized_Snapshot(store, &*pinned))) {
                    *error = RPC_OBJECT_UNAVAILABLE;
                    return {};
                }
                snapshot = &*pinned;
                if (topics & TOPIC_SHIPS) *context_epoch = snapshot->map.context_epoch;
                offset = cursor->offset;
                limit = cursor->limit;
            }
            const size_t total = Collection_Ids(store, snapshot, method->topic)->size();
            std::string result = "{\"stamp\":" + Stamp(store, snapshot) + ",\"items\":" + Collection(store, snapshot, method->topic, offset, limit);
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
        Revoke_Game_Context(store);
        Clear_View(store);
        if (store->bridge != nullptr) Revoke_Bridge(store->bridge);
    }

    void Revoke_Game_Context(s_game_store* store) {
        if (store == nullptr || !store->context_active) return;
        store->context_active = false;
        for (const auto& subscription : store->subscriptions) {
            if (Context_Response_Pending(store->bridge, subscription.client, subscription.baseline_ticket)) Cancel_Client(store->bridge, subscription.client);
        }
        Revoke_Bridge_Context(store->bridge);
        Prune_Subscriptions(store);
        std::erase_if(store->cursors, [store](const s_map_cursor& cursor) {
            const auto snapshot = std::find_if(store->snapshots.begin(), store->snapshots.end(), [&cursor](const s_map_snapshot& item) { return item.id == cursor.snapshot; });
            return snapshot == store->snapshots.end() || (cursor.method == "game.listShips" && !Authorized_Snapshot(store, &*snapshot));
        });
        for (auto& subscription : store->subscriptions) subscription.revision = 0;
        Publish_State_Changes(store);
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
        if (std::any_of(store->snapshots.begin(), store->snapshots.end(), [](const s_map_snapshot& snapshot) { return snapshot.map.exploration_sensitive; })) Revoke_Game_View(store);
        Revoke_Game_Context(store);
        ++store->busy_depth;
    }

    void Resume_Game_Queries(s_game_store* store) {
        if (store->busy_depth != 0) --store->busy_depth;
    }

    bool Capture_Map(s_game_store* store, const s_map_observation* observation) {
        if (store == nullptr || observation == nullptr || !store->loaded || store->busy_depth != 0 ||
            observation->viewer < 0 || observation->viewer >= 8 || observation->turn < 0 ||
            observation->systems.size() > 1024 || observation->nebulas.size() > 57 || observation->ships.size() > 10000 || observation->players.size() > 8 || observation->colonies.size() > 5120) return false;
        if (observation->context_epoch != 0 && (!observation->ships_captured || observation->context_epoch != Bridge_Context_Epoch(store->bridge))) return false;
        // Bind newly authorized default observers to the old coherent turn before replacing it.
        Sync_Turn_Observers(store);
        s_map_observation projected = *observation;
        if (!projected.research_captured) projected.research = {};
        auto& research = projected.research;
        if (research.technologies.size() > 64 || research.options.size() > 32 ||
            (research.mode != "no-selection" && research.mode != "researching" && research.mode != "hyper" && research.mode != "completed")) return false;
        auto Research_Index = [&research](int32_t index, bool application, bool empty) {
            return (empty && index == -1) || (index >= 0 && (size_t)index < research.technologies.size() && research.technologies[(size_t)index].application == application);
        };
        if (!Research_Index(research.project, false, true) || !Research_Index(research.application, true, true)) return false;
        for (auto& technology : research.technologies) {
            if (!technology.cost_available) technology.cost = 0;
            if (technology.name.size() > 384 || technology.cost < 0) return false;
        }
        for (const auto& option : research.options) {
            if (!Research_Index(option.field, false, false) || !Research_Index(option.application, true, true) || option.remaining < 0) return false;
        }
        projected.exploration_sensitive = false;
        for (s_system_observation& system : projected.systems) {
            system.name_available &= system.visited || system.name_known;
            if (!system.name_available) system.name.clear();
            if (system.name.size() > 4096 || system.spectral_class < 0 || system.spectral_class > 6 || system.size < 0 || system.size > 2) return false;
            if (system.spectral_class == 6) system.size = 0;
            system.owner_id.clear();
            system.wormhole_id.clear();
            system.planet_ids.clear();
            if (system.wormhole_available != EXPLORATION_CURRENT)
                system.wormhole_system = -1;
            else if (system.wormhole_system < 0 || (size_t)system.wormhole_system >= projected.systems.size())
                return false;
            if (system.wormhole_available < EXPLORATION_UNKNOWN || system.wormhole_available > EXPLORATION_EMPTY) return false;
            if (!system.explored) system.in_extended_range = false;
            projected.exploration_sensitive |= (!system.visited && (system.explored || system.name_available || system.wormhole_available != EXPLORATION_UNKNOWN));
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
            player.trade_treaty_available &= player.contacted && !player.local && !player.eliminated;
            if (!player.trade_treaty_available) player.trade_treaty = false;
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
            colony.planet_id.clear();
        }
        if (!projected.planets_captured) projected.planets.clear();
        std::erase_if(projected.planets, [](const s_planet_observation& planet) { return !planet.visible; });
        bool planet_locations[1024][5] = {};
        for (auto& planet : projected.planets) {
            if (planet.system < 0 || (size_t)planet.system >= projected.systems.size() || planet.orbit < 1 || planet.orbit > 5) return false;
            if (!projected.systems[planet.system].explored && !occupied[planet.system][planet.orbit - 1]) {
                planet.visible = false;
                continue;
            }
            if (planet_locations[planet.system][planet.orbit - 1] || planet.kind < 1 || planet.kind > 3) return false;
            planet_locations[planet.system][planet.orbit - 1] = true;
            projected.exploration_sensitive |= !projected.systems[planet.system].explored;
            planet.system_id.clear();
            planet.colony_id.clear();
            if (planet.kind != 3) {
                planet.name.clear();
                planet.size = planet.gravity = planet.climate = planet.minerals = planet.special = planet.food2 = planet.industry = planet.gravity_penalty = 0;
                planet.max_population_available = false;
            } else if (planet.name.size() > 90 || planet.size < 0 || planet.size > 4 || planet.gravity < 0 || planet.gravity > 2 ||
                planet.climate < 0 || planet.climate > 9 || planet.minerals < 0 || planet.minerals > 4 || planet.special < 0 || planet.special > 11 ||
                planet.food2 < 0 || planet.food2 > 255 || planet.industry < 0 || planet.industry > 255 ||
                (planet.gravity_penalty != 0 && planet.gravity_penalty != 25 && planet.gravity_penalty != 50))
                return false;
            if (planet.special == 11) planet.special = 0;
            if (!planet.max_population_available)
                planet.max_population = 0;
            else if (planet.max_population < 0 || planet.max_population > 42)
                return false;
        }
        std::erase_if(projected.planets, [](const s_planet_observation& planet) { return !planet.visible; });
        if (projected.planets.size() > 5120) return false;
        for (const auto& nebula : projected.nebulas) {
            if (nebula.variant < 0 || nebula.variant >= 12) return false;
        }
        std::sort(projected.players.begin(), projected.players.end(), [](const s_player_observation& left, const s_player_observation& right) {
            if (left.local != right.local) return left.local;
            return std::tie(left.name, left.color, left.contacted, left.eliminated, left.at_war_available, left.at_war, left.research_treaty_available, left.research_treaty, left.trade_treaty_available, left.trade_treaty) < std::tie(right.name, right.color, right.contacted, right.eliminated, right.at_war_available, right.at_war, right.research_treaty_available, right.research_treaty, right.trade_treaty_available, right.trade_treaty);
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
        if (!store->snapshots.empty()) {
            const auto& previous = store->snapshots.back().map;
            bool lost = false;
            for (size_t index = 0; index < std::min(previous.systems.size(), projected.systems.size()); ++index) {
                const auto& a = previous.systems[index];
                const auto& b = projected.systems[index];
                const auto owner = std::find_if(projected.players.begin(), projected.players.end(), [&b](const s_player_observation& player) { return player.slot == b.owner_slot; });
                const bool owner_known = owner != projected.players.end() && (owner->local || std::count_if(projected.players.begin(), projected.players.end(), [&owner](const s_player_observation& player) { return player.color == owner->color; }) == 1);
                lost |= (a.explored && !b.explored) || (a.name_available && !b.name_available) ||
                    (a.wormhole_available != EXPLORATION_UNKNOWN && b.wormhole_available == EXPLORATION_UNKNOWN) ||
                    (!a.owner_id.empty() && !owner_known);
            }
            if (projected.planets_captured) {
                for (const auto& planet : previous.planets) {
                    lost |= std::none_of(projected.planets.begin(), projected.planets.end(), [&planet, store](const s_planet_observation& current) {
                        return (size_t)current.system < store->system_ids.size() && store->system_ids[current.system] == planet.system_id && current.orbit == planet.orbit;
                    });
                }
            }
            if (lost) Revoke_Game_View(store);
        }
        if (!store->snapshots.empty() && (projected.systems.size() != store->snapshots.back().map.systems.size() || projected.nebulas.size() != store->snapshots.back().map.nebulas.size())) {
            Revoke_Game_View(store);
        }
        if (store->viewer != projected.viewer) {
            Revoke_Game_View(store);
            if (!store->new_id(&store->perspective_id, store->id_context)) return false;
            store->viewer = projected.viewer;
        }
        if (projected.context_epoch != 0 && projected.context_epoch != Bridge_Context_Epoch(store->bridge)) return false;
        auto Extend = [store](std::vector<std::string>* ids, size_t count) {
            while (ids->size() < count) {
                std::string id;
                if (!store->new_id(&id, store->id_context)) return false;
                ids->push_back(std::move(id));
            }
            return true;
        };
        if (!Extend(&store->system_ids, projected.systems.size()) || !Extend(&store->nebula_ids, projected.nebulas.size())) return false;
        for (auto& planet : projected.planets) planet.system_id = store->system_ids[planet.system];
        std::sort(projected.planets.begin(), projected.planets.end(), [](const s_planet_observation& a, const s_planet_observation& b) {
            return std::tie(a.system_id, a.orbit) < std::tie(b.system_id, b.orbit);
        });
        std::vector<std::string> planet_ids;
        for (auto& planet : projected.planets) {
            auto handle = std::find_if(store->planet_handles.begin(), store->planet_handles.end(), [&planet](const s_planet_handle& item) { return item.system_id == planet.system_id && item.orbit == planet.orbit; });
            if (handle == store->planet_handles.end()) {
                std::string id;
                if (!store->new_id(&id, store->id_context)) return false;
                store->planet_handles.push_back({planet.system_id, planet.orbit, std::move(id)});
                handle = std::prev(store->planet_handles.end());
            }
            planet_ids.push_back(handle->id);
            if (projected.systems[planet.system].explored) projected.systems[planet.system].planet_ids.push_back(handle->id);
        }
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
            const auto planet = std::find_if(projected.planets.begin(), projected.planets.end(), [&colony](const s_planet_observation& item) { return item.system_id == colony.system_id && item.orbit == colony.orbit; });
            if (planet != projected.planets.end()) colony.planet_id = planet_ids[(size_t)(planet - projected.planets.begin())];
            const auto local = std::find_if(projected.players.begin(), projected.players.end(), [](const s_player_observation& player) { return player.local; });
            colony.owner_id = player_ids[(size_t)(local - projected.players.begin())];
            colony.system = colony.owner_slot = -1;
        }
        for (auto& system : projected.systems) {
            const auto owner = std::find_if(projected.players.begin(), projected.players.end(), [&system](const s_player_observation& player) { return player.slot == system.owner_slot; });
            if (owner != projected.players.end() && (owner->local || std::count_if(projected.players.begin(), projected.players.end(), [&owner](const s_player_observation& player) { return player.color == owner->color; }) == 1)) {
                system.owner_id = player_ids[(size_t)(owner - projected.players.begin())];
                projected.exploration_sensitive |= !owner->local;
            }
            if (system.wormhole_available == EXPLORATION_CURRENT) system.wormhole_id = store->system_ids[system.wormhole_system];
            system.owner_slot = system.wormhole_system = -1;
        }
        std::sort(projected.colonies.begin(), projected.colonies.end(), [](const s_colony_observation& left, const s_colony_observation& right) {
            return std::tie(left.system_id, left.orbit, left.population, left.owner_id, left.infantry, left.armor) < std::tie(right.system_id, right.orbit, right.population, right.owner_id, right.infantry, right.armor);
        });
        std::sort(projected.ships.begin(), projected.ships.end(), [](const s_ship_observation& left, const s_ship_observation& right) {
            return std::tie(left.x, left.y, left.owned, left.name, left.owner_id, left.destination_available, left.destination_id, left.eta_available, left.eta) <
                std::tie(right.x, right.y, right.owned, right.name, right.owner_id, right.destination_available, right.destination_id, right.eta_available, right.eta);
        });
        std::vector<std::string> colony_ids;
        const auto* previous_colonies = Select_Snapshot(store, TOPIC_COLONIES);
        if (projected.colonies_captured && previous_colonies != nullptr && Same_Colonies(&projected, &previous_colonies->map)) colony_ids = previous_colonies->colony_ids;
        if (!Extend(&colony_ids, projected.colonies.size())) return false;
        for (auto& planet : projected.planets) {
            const auto colony = std::find_if(projected.colonies.begin(), projected.colonies.end(), [&planet](const s_colony_observation& item) { return item.system_id == planet.system_id && item.orbit == planet.orbit; });
            if (colony != projected.colonies.end()) planet.colony_id = colony_ids[(size_t)(colony - projected.colonies.begin())];
            planet.system = -1;
        }
        if (projected.context_epoch == 0 && projected.ships_captured && !store->snapshots.empty() && !Authorized_Snapshot(store, &store->snapshots.back())) {
            const auto* ambient = Select_Snapshot(store, TOPIC_SHIPS | (projected.players_captured ? TOPIC_PLAYERS : TOPIC_NONE) | (projected.colonies_captured ? TOPIC_COLONIES : TOPIC_NONE));
            if (ambient != nullptr && Same_Map(&projected, &ambient->map) && Same_Ships(&projected, &ambient->map) &&
                (!projected.players_captured || (Same_Players(&projected, &ambient->map) && player_ids == ambient->player_ids)) &&
                (!projected.colonies_captured || Same_Colonies(&projected, &ambient->map)) && (!projected.planets_captured || Same_Planets(&projected, &ambient->map)) &&
                (!projected.research_captured || Same_Research(&projected, &ambient->map))) return true;
        }
        const bool equal = !store->snapshots.empty() && Same_Map(&projected, &store->snapshots.back().map) &&
            (!projected.ships_captured || Same_Ships(&projected, &store->snapshots.back().map)) &&
            (!projected.players_captured || (Same_Players(&projected, &store->snapshots.back().map) && player_ids == store->snapshots.back().player_ids)) &&
            (!projected.colonies_captured || Same_Colonies(&projected, &store->snapshots.back().map)) &&
            (!projected.planets_captured || Same_Planets(&projected, &store->snapshots.back().map)) &&
            (!projected.research_captured || Same_Research(&projected, &store->snapshots.back().map));
        if (projected.context_epoch != 0) store->context_active = true;
        if (equal && (projected.context_epoch == store->snapshots.back().map.context_epoch || !projected.ships_captured)) return true;
        if (store->revision == std::numeric_limits<uint64_t>::max()) return false;
        s_map_snapshot snapshot;
        if (!store->new_id(&snapshot.id, store->id_context)) return false;
        snapshot.map = std::move(projected);
        snapshot.player_ids = std::move(player_ids);
        snapshot.planet_ids = std::move(planet_ids);
        snapshot.colony_ids = std::move(colony_ids);
        const auto* previous_research = Select_Snapshot(store, TOPIC_RESEARCH);
        if (snapshot.map.research_captured && previous_research != nullptr && Same_Research(&snapshot.map, &previous_research->map)) {
            snapshot.technology_ids = previous_research->technology_ids;
            snapshot.research_option_ids = previous_research->research_option_ids;
        }
        if (!Extend(&snapshot.technology_ids, snapshot.map.research.technologies.size()) ||
            !Extend(&snapshot.research_option_ids, snapshot.map.research.options.size())) return false;
        const s_map_snapshot* previous_ships = nullptr;
        for (auto item = store->snapshots.rbegin(); item != store->snapshots.rend(); ++item) {
            if (item->map.ships_captured) {
                previous_ships = &*item;
                break;
            }
        }
        if (snapshot.map.context_epoch != 0) {
            for (auto item = store->snapshots.rbegin(); item != store->snapshots.rend(); ++item) {
                if (item->map.context_epoch != 0 && Same_Ships(&snapshot.map, &item->map)) {
                    previous_ships = &*item;
                    break;
                }
            }
        }
        if (snapshot.map.ships_captured && previous_ships != nullptr && Same_Ships(&snapshot.map, &previous_ships->map)) {
            snapshot.ship_ids = previous_ships->ship_ids;
        }
        if (!Extend(&snapshot.ship_ids, snapshot.map.ships.size())) return false;
        snapshot.revision = equal ? store->revision : ++store->revision;
        store->snapshots.push_back(std::move(snapshot));
        for (auto& subscription : store->subscriptions) subscription.revision = 0;
        if (store->snapshots.size() > 4) {
            const bool has_context = std::any_of(store->snapshots.begin(), store->snapshots.end(), [](const s_map_snapshot& item) { return item.map.context_epoch != 0; });
            auto ambient = has_context ? std::find_if(store->snapshots.rbegin(), store->snapshots.rend(), [](const s_map_snapshot& item) { return item.map.ships_captured && item.map.context_epoch == 0; }) : store->snapshots.rend();
            const std::string protected_id = ambient != store->snapshots.rend() ? ambient->id : std::string();
            auto expired = std::find_if(store->snapshots.begin(), store->snapshots.end(), [&protected_id](const s_map_snapshot& item) { return item.id != protected_id; });
            store->snapshots.erase(expired);
        }
        std::erase_if(store->cursors, [store](const s_map_cursor& cursor) {
            return std::none_of(store->snapshots.begin(), store->snapshots.end(), [&cursor](const s_map_snapshot& item) { return item.id == cursor.snapshot; });
        });
        return true;
    }

    std::string Game_Query(s_game_store* store, const s_request* request, uint64_t* context_epoch) {
        uint64_t captured_context = 0;
        if (context_epoch != nullptr) *context_epoch = 0;
        if (!Validate_Params(&store->protocol, request)) return Error_Response(request, RPC_INVALID_PARAMS);
        yyjson_doc* params = yyjson_read(request->params_json.data(), request->params_json.size(), YYJSON_READ_NOFLAG);
        if (params == nullptr) return Error_Response(request, RPC_INVALID_PARAMS);
        int32_t error = 0;
        const std::string result = Query(store, request, yyjson_doc_get_root(params), &error, &captured_context);
        yyjson_doc_free(params);
        if (error != 0) return Error_Response(request, error);
        const std::string response = Result_Response(request, result);
        if (context_epoch != nullptr && response.size() <= 1048576) *context_epoch = captured_context;
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
            uint64_t context_epoch = 0;
            std::string response = work.request.method == "rpc.subscribe" || work.request.method == "rpc.unsubscribe" ? Subscription_Query(store, &work, &context_epoch) : Game_Query(store, &work.request, &context_epoch);
            const bool expired = Monotonic_Ms() >= work.deadline_ms;
            if (expired) {
                response = Error_Response(&work.request, RPC_REQUEST_EXPIRED);
                context_epoch = 0;
            }
            const bool completed = Complete_Work(store->bridge, &work, &response, context_epoch);
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
