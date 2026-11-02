#include "rpc_game.h"
#include "rpc_json.h"

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
            TOPIC_LEADERS = 1 << 8,
            TOPIC_RELATIONS = 1 << 9,
            TOPIC_INTELLIGENCE = 1 << 10,
            TOPIC_HISTORY = 1 << 11,
            TOPIC_MESSAGES = 1 << 12,
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
            {TOPIC_LEADERS, "leaders", "game.listLeaders", "game.getLeader"},
            {TOPIC_RELATIONS, "relations", nullptr, "game.getRelations"},
            {TOPIC_INTELLIGENCE, "intelligence", nullptr, "game.getIntelligence"},
            {TOPIC_HISTORY, "history", nullptr, "game.getHistory"},
            {TOPIC_MESSAGES, "messages", "game.listMessages", "game.getMessage"},
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
                if (std::tie(a.system_id, a.orbit, a.population, a.owner_id, a.infantry, a.armor, a.planet_id) != std::tie(b.system_id, b.orbit, b.population, b.owner_id, b.infantry, b.armor, b.planet_id) || a.operations != b.operations) return false;
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

        auto Leader_Data(const s_leader_observation& item) {
            return std::tie(item.disclosure, item.role, item.name, item.title, item.rank, item.xp_available, item.xp, item.status, item.assignment_available, item.assignment_kind, item.assignment_name, item.assignment_id, item.eta_available, item.eta, item.hire_cost_available, item.hire_cost, item.upkeep, item.offer_turns_available, item.offer_turns, item.benefits_available, item.benefits, item.partial, item.skills_available, item.bonus_available, item.bonus);
        }

        bool Same_Leaders(const s_map_observation* left, const s_map_observation* right) {
            if (left->leaders_captured != right->leaders_captured || left->leaders.size() != right->leaders.size()) return false;
            for (size_t index = 0; index < left->leaders.size(); ++index) {
                const auto& a = left->leaders[index];
                const auto& b = right->leaders[index];
                if (Leader_Data(a) != Leader_Data(b) || a.skills.size() != b.skills.size()) return false;
                for (size_t skill = 0; skill < a.skills.size(); ++skill) {
                    if (std::tie(a.skills[skill].name, a.skills[skill].value) != std::tie(b.skills[skill].name, b.skills[skill].value)) return false;
                }
            }
            return true;
        }

        bool Authorized_Snapshot(const s_game_store* store, const s_map_snapshot* snapshot) {
            return snapshot->map.context_epoch == 0 ||
                (store->context_active && snapshot->map.context_epoch == Bridge_Context_Epoch(store->bridge));
        }

        bool Same_Relations(const s_map_observation* left, const s_map_observation* right) {
            if (left->relations_captured != right->relations_captured || left->relations.size() != right->relations.size()) return false;
            const auto& a = left->intelligence;
            const auto& b = right->intelligence;
            if (std::tie(a.available, a.agents, a.bonuses_available, a.spy_bonus, a.agent_bonus) != std::tie(b.available, b.agents, b.bonuses_available, b.spy_bonus, b.agent_bonus)) return false;
            for (size_t index = 0; index < left->relations.size(); ++index) {
                const auto& x = left->relations[index];
                const auto& y = right->relations[index];
                if (x.relationship_offset != y.relationship_offset) return false;
                if (std::tie(x.player_id, x.available, x.relationship, x.treaty, x.ignoring, x.research, x.research_level, x.trade, x.trade_level, x.tribute_percent, x.giving, x.assignment_available, x.spies, x.mission, x.incoming_available, x.incoming, x.report_available, x.personality, x.objective, x.objective_empty) !=
                        std::tie(y.player_id, y.available, y.relationship, y.treaty, y.ignoring, y.research, y.research_level, y.trade, y.trade_level, y.tribute_percent, y.giving, y.assignment_available, y.spies, y.mission, y.incoming_available, y.incoming, y.report_available, y.personality, y.objective, y.objective_empty) ||
                    x.report.size() != y.report.size()) return false;
                for (size_t row = 0; row < x.report.size(); ++row) {
                    if (std::tie(x.report[row].player_id, x.report[row].name, x.report[row].alliance) != std::tie(y.report[row].player_id, y.report[row].name, y.report[row].alliance)) return false;
                }
            }
            return true;
        }

        bool Same_History(const s_map_observation* left, const s_map_observation* right) {
            if (left->history_captured != right->history_captured) return false;
            const auto& a = left->history;
            const auto& b = right->history;
            if (a.x_step != b.x_step || a.smoothing != b.smoothing || a.labels != b.labels || a.charts.size() != b.charts.size()) return false;
            for (size_t chart = 0; chart < a.charts.size(); ++chart) {
                const auto& x = a.charts[chart];
                const auto& y = b.charts[chart];
                if (x.series != y.series || x.top_scale != y.top_scale || x.curves.size() != y.curves.size()) return false;
                for (size_t row = 0; row < x.curves.size(); ++row) {
                    if (x.curves[row].player_id != y.curves[row].player_id || x.curves[row].y != y.curves[row].y) return false;
                }
            }
            return true;
        }

        bool Same_Messages(const s_map_observation* left, const s_map_observation* right) {
            if (left->messages_captured != right->messages_captured || left->messages.size() != right->messages.size()) return false;
            for (size_t row = 0; row < left->messages.size(); ++row) {
                const auto& a = left->messages[row];
                const auto& b = right->messages[row];
                if (std::tie(a.text_available, a.text, a.serious, a.colony_id) != std::tie(b.text_available, b.text, b.serious, b.colony_id)) return false;
            }
            return true;
        }

        const s_map_snapshot* Select_Snapshot(const s_game_store* store, uint32_t topics) {
            for (auto snapshot = store->snapshots.rbegin(); snapshot != store->snapshots.rend(); ++snapshot) {
                if (snapshot->map.leaders_only && (topics & ~(TOPIC_SESSION | TOPIC_LEADERS))) continue;
                if (store->busy_depth != 0 && (!snapshot->map.leaders_only || !snapshot->map.leader_presentation)) continue;
                if ((topics & (TOPIC_SHIPS | TOPIC_LEADERS | TOPIC_RELATIONS)) && !Authorized_Snapshot(store, &*snapshot)) continue;
                if ((!(topics & TOPIC_SHIPS) || snapshot->map.ships_captured) &&
                    (!(topics & TOPIC_PLAYERS) || snapshot->map.players_captured) &&
                    (!(topics & TOPIC_COLONIES) || snapshot->map.colonies_captured) &&
                    (!(topics & TOPIC_PLANETS) || snapshot->map.planets_captured) &&
                    (!(topics & TOPIC_RESEARCH) || snapshot->map.research_captured) &&
                    (!(topics & TOPIC_LEADERS) || snapshot->map.leaders_captured) &&
                    (!(topics & (TOPIC_RELATIONS | TOPIC_INTELLIGENCE)) || snapshot->map.relations_captured) &&
                    (!(topics & TOPIC_HISTORY) || snapshot->map.history_captured) &&
                    (!(topics & TOPIC_MESSAGES) || snapshot->map.messages_captured)) return &*snapshot;
            }
            return nullptr;
        }

        yyjson_mut_val* Session(s_json_builder* json, const s_game_store* store, const s_map_snapshot* selected = nullptr) {
            if (store->snapshots.empty()) {
                return Json_Object(json, {{"phase", Json_String_Value(json, store->loaded || store->unavailable ? "unavailable" : "menu")}});
            }
            const s_map_snapshot& snapshot = selected != nullptr ? *selected : store->snapshots.back();
            return Json_Object(json, {
                {"gameId", Json_String_Value(json, store->game_id)},
                {"perspectiveId", Json_String_Value(json, store->perspective_id)},
                {"revision", Json_String_Value(json, std::to_string(snapshot.revision))},
                {"turn", Json_Integer(json, snapshot.map.turn)},
                {"phase", Json_String_Value(json, store->busy_depth != 0 ? "resolving" : "committed")},
            });
        }

        yyjson_mut_val* Stamp(s_json_builder* json, const s_game_store* store, const s_map_snapshot* snapshot) {
            return Json_Object(json, {
                {"gameId", Json_String_Value(json, store->game_id)},
                {"perspectiveId", Json_String_Value(json, store->perspective_id)},
                {"snapshotId", Json_String_Value(json, snapshot->id)},
                {"revision", Json_String_Value(json, std::to_string(snapshot->revision))},
                {"turn", Json_Integer(json, snapshot->map.turn)},
                {"captureMode", Json_String_Value(json, snapshot->map.relations_presentation ? "native-relations-presentation" : (snapshot->map.leader_presentation ? "native-leader-presentation" : (snapshot->map.context_epoch != 0 ? "last-native-fleet-box" : (snapshot->map.ships_captured ? "last-native-ship-refresh" : "last-coherent-capture"))))},
            });
        }

        yyjson_mut_val* Exploration_Knowledge(s_json_builder* json, e_exploration_knowledge available, yyjson_mut_val* value = nullptr) {
            const char* availability;
            switch (available) {
                case EXPLORATION_CURRENT: availability = "current"; break;
                case EXPLORATION_EMPTY: availability = "known-empty"; break;
                case EXPLORATION_UNAUDITED: availability = "unaudited"; break;
                default: availability = "unknown"; break;
            }
            auto* result = Json_Object(json, {{"availability", Json_String_Value(json, availability)}});
            if (available == EXPLORATION_CURRENT) Json_Add(json, result, "value", value);
            return result;
        }

        yyjson_mut_val* Id_Knowledge(s_json_builder* json, const std::string& id) {
            return Exploration_Knowledge(json, id.empty() ? EXPLORATION_UNKNOWN : EXPLORATION_CURRENT, Json_String_Value(json, id));
        }

        yyjson_mut_val* System(s_json_builder* json, const s_game_store* store, const s_map_snapshot* snapshot, size_t index) {
            const s_system_observation& item = snapshot->map.systems[index];
            auto* planets = Json_Array(json);
            for (const auto& id : item.planet_ids) Json_Append(json, planets, Json_String_Value(json, id));
            return Json_Object(json, {
                {"id", Json_String_Value(json, store->system_ids[index])},
                {"x", Json_Integer(json, item.x)},
                {"y", Json_Integer(json, item.y)},
                {"visited", Json_Boolean(json, item.visited)},
                {"name", Exploration_Knowledge(json, item.name_available ? EXPLORATION_CURRENT : EXPLORATION_UNKNOWN, Json_String_Value(json, item.name))},
                {"spectralClass", Json_Integer(json, item.spectral_class)},
                {"size", Json_Integer(json, item.size)},
                {"owner", Id_Knowledge(json, item.owner_id)},
                {"wormhole", Exploration_Knowledge(json, item.wormhole_available, Json_String_Value(json, item.wormhole_id))},
                {"inExtendedRange", Exploration_Knowledge(json, item.explored ? EXPLORATION_CURRENT : EXPLORATION_UNKNOWN, Json_Boolean(json, item.in_extended_range))},
                {"planetIds", Exploration_Knowledge(json, item.explored ? EXPLORATION_CURRENT : EXPLORATION_UNKNOWN, planets)},
            });
        }

        yyjson_mut_val* Nebula(s_json_builder* json, const s_game_store* store, const s_map_snapshot* snapshot, size_t index) {
            const s_nebula_observation& item = snapshot->map.nebulas[index];
            return Json_Object(json, {
                {"id", Json_String_Value(json, store->nebula_ids[index])},
                {"x", Json_Integer(json, item.x)},
                {"y", Json_Integer(json, item.y)},
                {"variant", Json_Integer(json, item.variant)},
            });
        }

        yyjson_mut_val* Planet(s_json_builder* json, const s_map_snapshot* snapshot, size_t index) {
            const auto& item = snapshot->map.planets[index];
            const auto details = item.kind == 3 ? EXPLORATION_CURRENT : EXPLORATION_EMPTY;
            const char* kind = item.kind == 1 ? "asteroid" : (item.kind == 2 ? "gas-giant" : "planet");
            auto* result = Json_Object(json, {
                {"id", Json_String_Value(json, snapshot->planet_ids[index])},
                {"systemId", Json_String_Value(json, item.system_id)},
                {"orbit", Json_Integer(json, item.orbit)},
                {"kind", Exploration_Knowledge(json, EXPLORATION_CURRENT, Json_String_Value(json, kind))},
                {"name", Exploration_Knowledge(json, details, Json_String_Value(json, item.name))},
            });
            const std::pair<const char*, int32_t> fields[] = {{"size", item.size}, {"gravity", item.gravity}, {"climate", item.climate}, {"minerals", item.minerals}, {"foodPerFarmer2", item.food2}, {"mineralsPerMine", item.industry}, {"gravityPenalty", item.gravity_penalty}};
            for (const auto& field : fields) Json_Add(json, result, field.first, Exploration_Knowledge(json, details, Json_Integer(json, field.second)));
            Json_Add(json, result, "special", Exploration_Knowledge(json, item.kind == 3 && item.special != 0 ? EXPLORATION_CURRENT : EXPLORATION_EMPTY, Json_Integer(json, item.special)));
            Json_Add(json, result, "hostileEnvironment", Exploration_Knowledge(json, details, Json_Boolean(json, item.food2 == 0)));
            Json_Add(json, result, "maxPopulation", Exploration_Knowledge(json, item.kind != 3 ? EXPLORATION_EMPTY : (item.max_population_available ? EXPLORATION_CURRENT : EXPLORATION_UNAUDITED), Json_Integer(json, item.max_population)));
            Json_Add(json, result, "colony", Id_Knowledge(json, item.colony_id));
            return result;
        }

        yyjson_mut_val* Ship_Knowledge(s_json_builder* json, e_ship_knowledge available, yyjson_mut_val* value) {
            const char* availability;
            switch (available) {
                case SHIP_CURRENT: availability = "current"; break;
                case SHIP_KNOWN_EMPTY: availability = "known-empty"; break;
                case SHIP_UNKNOWN: availability = "unknown"; break;
                case SHIP_CONTEXT_REQUIRED: availability = "context-required"; break;
                default: availability = "unaudited"; break;
            }
            auto* result = Json_Object(json, {{"availability", Json_String_Value(json, availability)}});
            if (available == SHIP_CURRENT) Json_Add(json, result, "value", value);
            return result;
        }

        yyjson_mut_val* Ship(s_json_builder* json, const s_map_snapshot* snapshot, size_t index) {
            const s_ship_observation& item = snapshot->map.ships[index];
            return Json_Object(json, {
                {"id", Json_String_Value(json, snapshot->ship_ids[index])},
                {"x", Json_Integer(json, item.x)},
                {"y", Json_Integer(json, item.y)},
                {"owned", Json_Boolean(json, item.owned)},
                {"name", Exploration_Knowledge(json, item.owned ? EXPLORATION_CURRENT : EXPLORATION_UNAUDITED, Json_String_Value(json, item.name))},
                {"owner", Exploration_Knowledge(json, item.owner_id.empty() ? EXPLORATION_UNAUDITED : EXPLORATION_CURRENT, Json_String_Value(json, item.owner_id))},
                {"destination", Ship_Knowledge(json, item.destination_available, Json_String_Value(json, item.destination_id))},
                {"eta", Ship_Knowledge(json, item.eta_available, Json_Integer(json, item.eta))},
            });
        }

        yyjson_mut_val* Player(s_json_builder* json, const s_map_snapshot* snapshot, size_t index) {
            const auto& item = snapshot->map.players[index];
            auto* result = Json_Object(json, {
                {"id", Json_String_Value(json, snapshot->player_ids[index])},
                {"local", Json_Boolean(json, item.local)},
                {"contacted", Json_Boolean(json, item.contacted)},
                {"eliminated", Json_Boolean(json, item.eliminated)},
                {"color", Json_Integer(json, item.color)},
                {"name", Exploration_Knowledge(json, EXPLORATION_CURRENT, Json_String_Value(json, item.name))},
                {"atWar", Exploration_Knowledge(json, item.at_war_available ? EXPLORATION_CURRENT : EXPLORATION_UNKNOWN, Json_Boolean(json, item.at_war))},
                {"researchTreaty", Exploration_Knowledge(json, item.research_treaty_available ? EXPLORATION_CURRENT : EXPLORATION_UNKNOWN, Json_Boolean(json, item.research_treaty))},
                {"tradeTreaty", Exploration_Knowledge(json, item.trade_treaty_available ? EXPLORATION_CURRENT : EXPLORATION_UNKNOWN, Json_Boolean(json, item.trade_treaty))},
            });
            const auto private_data = item.local ? EXPLORATION_CURRENT : EXPLORATION_UNAUDITED;
            Json_Add(json, result, "treasury", Exploration_Knowledge(json, private_data, Json_Integer(json, item.treasury)));
            yyjson_mut_val* hud = nullptr;
            if (item.local) {
                hud = Json_Object(json, {
                    {"income", Json_Integer(json, item.hud.income)},
                    {"foodSurplus", Json_Integer(json, item.hud.food_surplus)},
                    {"freighters", Json_Integer(json, item.hud.freighters)},
                    {"freightersAvailable", Json_Integer(json, item.hud.freighters_available)},
                    {"commandPoints", Json_Integer(json, item.hud.command_points)},
                    {"commandPointsAvailable", Json_Integer(json, item.hud.command_points_available)},
                });
            }
            Json_Add(json, result, "hud", Exploration_Knowledge(json, private_data, hud));
            return result;
        }

        yyjson_mut_val* Colony_Operations(s_json_builder* json, const s_colony_operations& item) {
            if (!item.available) return Exploration_Knowledge(json, EXPLORATION_UNKNOWN);
            auto* result = Json_Object(json, {
                {"populationText", Json_String_Value(json, item.population_text)},
                {"heading", Exploration_Knowledge(json, item.heading_available ? EXPLORATION_CURRENT : EXPLORATION_UNKNOWN, Json_String_Value(json, item.heading))},
                {"autobuild", Json_String_Value(json, item.autobuild)},
                {"autobuildActive", Json_Boolean(json, item.autobuild_active)},
                {"farmCapacity", Exploration_Knowledge(json, item.farm_capacity < 0 ? EXPLORATION_EMPTY : EXPLORATION_CURRENT, Json_Integer(json, item.farm_capacity))},
                {"blockaded", Json_Boolean(json, item.blockaded)},
                {"event", Exploration_Knowledge(json, item.event_present ? EXPLORATION_CURRENT : EXPLORATION_EMPTY, Json_String_Value(json, item.event))},
                {"status", Exploration_Knowledge(json, item.status_present ? EXPLORATION_CURRENT : EXPLORATION_EMPTY, Json_String_Value(json, item.status))},
                {"inboundPopulation", Json_Integer(json, item.inbound)},
                {"infantryLabel", Json_String_Value(json, item.infantry_name)},
                {"armorLabel", Json_String_Value(json, item.armor_name)},
                {"morale", Json_Object(json, {
                    {"suppressed", Json_Boolean(json, item.morale_suppressed)},
                    {"icons", Json_Integer(json, item.morale_icons)},
                })},
            });
            auto* queue = Json_Array(json);
            for (const auto& product : item.queue) {
                auto* value = product.available ? Json_Object(json, {
                    {"kind", Json_String_Value(json, product.kind)},
                    {"label", Json_String_Value(json, product.label)},
                }) : nullptr;
                Json_Append(json, queue, Exploration_Knowledge(json, product.available ? EXPLORATION_CURRENT : EXPLORATION_UNKNOWN, value));
            }
            Json_Add(json, result, "queue", queue);
            auto* workforce = Json_Array(json);
            for (const auto& worker : item.workforce) {
                Json_Append(json, workforce, Json_Object(json, {
                    {"label", Json_String_Value(json, worker.label)},
                    {"species", Json_String_Value(json, worker.species)},
                    {"job", Json_String_Value(json, worker.job)},
                    {"state", Json_String_Value(json, worker.state)},
                    {"assigned", Json_Boolean(json, worker.assigned)},
                    {"conquered", Json_Boolean(json, worker.conquered)},
                    {"player", Id_Knowledge(json, worker.player_id)},
                }));
            }
            Json_Add(json, result, "workforce", workforce);
            auto* structures = Json_Array(json);
            for (const auto& building : item.structures) {
                Json_Append(json, structures, Json_Object(json, {
                    {"label", Json_String_Value(json, building.name)},
                    {"satellite", Json_Boolean(json, building.satellite)},
                }));
            }
            Json_Add(json, result, "structures", structures);
            auto* resources = Json_Array(json);
            for (const auto& row : item.resources) {
                Json_Append(json, resources, Json_Object(json, {
                    {"resource", Json_String_Value(json, row.resource)},
                    {"local", Json_Integer(json, row.local)},
                    {"exchanged", Json_Integer(json, row.exchanged)},
                    {"secondary", Json_Integer(json, row.secondary)},
                    {"shortage", Json_Integer(json, row.shortage)},
                    {"exporting", Json_Boolean(json, row.exporting)},
                }));
            }
            Json_Add(json, result, "resources", resources);
            yyjson_mut_val* garrison = nullptr;
            if (item.garrison_available) {
                garrison = Json_Array(json);
                for (const auto& unit : item.garrison) {
                    Json_Append(json, garrison, Json_Object(json, {
                        {"label", Json_String_Value(json, unit.label)},
                        {"count", Json_Integer(json, unit.count)},
                        {"strength", Json_Integer(json, unit.strength)},
                        {"hp", Json_Integer(json, unit.hp)},
                    }));
                }
            }
            Json_Add(json, result, "garrison", Exploration_Knowledge(json, item.garrison_available ? EXPLORATION_CURRENT : EXPLORATION_UNKNOWN, garrison));
            yyjson_mut_val* production = nullptr;
            if (item.production_available) {
                production = Json_Object(json, {
                    {"cost", Json_Integer(json, item.cost)},
                    {"stored", Json_Integer(json, item.stored)},
                    {"bought", Json_Integer(json, item.bought)},
                    {"turns", Exploration_Knowledge(json, item.turns > 0 && item.turns < 10000 ? EXPLORATION_CURRENT : EXPLORATION_UNKNOWN, Json_Integer(json, item.turns))},
                    {"buyCost", Json_Integer(json, item.buy_cost)},
                    {"canBuy", Json_Boolean(json, item.can_buy)},
                });
            }
            Json_Add(json, result, "currentProduction", Exploration_Knowledge(json, item.production_available ? EXPLORATION_CURRENT : EXPLORATION_UNKNOWN, production));
            return Exploration_Knowledge(json, EXPLORATION_CURRENT, result);
        }

        yyjson_mut_val* Colony(s_json_builder* json, const s_map_snapshot* snapshot, size_t index) {
            const auto& item = snapshot->map.colonies[index];
            const bool production = item.operations.available && !item.operations.queue.empty() && item.operations.queue[0].available;
            return Json_Object(json, {
                {"id", Json_String_Value(json, snapshot->colony_ids[index])},
                {"systemId", Json_String_Value(json, item.system_id)},
                {"orbit", Json_Integer(json, item.orbit)},
                {"planetId", Id_Knowledge(json, item.planet_id)},
                {"owner", Exploration_Knowledge(json, EXPLORATION_CURRENT, Json_String_Value(json, item.owner_id))},
                {"population", Exploration_Knowledge(json, EXPLORATION_CURRENT, Json_Integer(json, item.population))},
                {"troops", Exploration_Knowledge(json, EXPLORATION_CURRENT, Json_Object(json, {
                    {"infantry", Json_Integer(json, item.infantry)},
                    {"armor", Json_Integer(json, item.armor)},
                }))},
                {"production", Exploration_Knowledge(json, production ? EXPLORATION_CURRENT : EXPLORATION_UNKNOWN,
                    production ? Json_String_Value(json, item.operations.queue[0].label) : nullptr)},
                {"buyCost", Exploration_Knowledge(json, item.operations.available && item.operations.production_available ? EXPLORATION_CURRENT : EXPLORATION_UNKNOWN, Json_Integer(json, item.operations.buy_cost))},
                {"operations", Colony_Operations(json, item.operations)},
            });
        }

        yyjson_mut_val* Research_Id(s_json_builder* json, const s_map_snapshot* snapshot, int32_t index) {
            return index < 0 ? Exploration_Knowledge(json, EXPLORATION_EMPTY) : Exploration_Knowledge(json, EXPLORATION_CURRENT, Json_String_Value(json, snapshot->technology_ids[(size_t)index]));
        }

        yyjson_mut_val* Research_Cost(s_json_builder* json, const s_technology_observation* technology) {
            return Exploration_Knowledge(json, technology == nullptr ? EXPLORATION_EMPTY : (technology->cost_available ? EXPLORATION_CURRENT : EXPLORATION_UNAUDITED),
                technology != nullptr ? Json_Integer(json, technology->cost) : nullptr);
        }

        yyjson_mut_val* Research_Option(s_json_builder* json, const s_map_snapshot* snapshot, size_t index) {
            const auto& option = snapshot->map.research.options[index];
            const auto& technology = snapshot->map.research.technologies[(size_t)(option.application >= 0 ? option.application : option.field)];
            return Json_Object(json, {
                {"id", Json_String_Value(json, snapshot->research_option_ids[index])},
                {"field", Json_String_Value(json, snapshot->technology_ids[(size_t)option.field])},
                {"application", Research_Id(json, snapshot, option.application)},
                {"label", Json_String_Value(json, technology.name)},
                {"remainingCost", Json_Integer(json, option.remaining)},
            });
        }

        yyjson_mut_val* Research(s_json_builder* json, const s_game_store* store, const s_map_snapshot* snapshot, bool stamped = true) {
            const auto& research = snapshot->map.research;
            auto* result = Json_Object(json);
            if (stamped) Json_Add(json, result, "stamp", Stamp(json, store, snapshot));
            Json_Add(json, result, "mode", Json_String_Value(json, research.mode));
            Json_Add(json, result, "project", Research_Id(json, snapshot, research.project));
            Json_Add(json, result, "application", Research_Id(json, snapshot, research.application));
            Json_Add(json, result, "progress", Exploration_Knowledge(json, EXPLORATION_UNAUDITED));
            Json_Add(json, result, "cost", Research_Cost(json, research.project >= 0 ? &research.technologies[(size_t)research.project] : nullptr));
            auto* options = Json_Array(json);
            for (size_t index = 0; index < research.options.size(); ++index) {
                Json_Append(json, options, Research_Option(json, snapshot, index));
            }
            Json_Add(json, result, "options", options);
            return result;
        }

        yyjson_mut_val* Technology(s_json_builder* json, const s_game_store* store, const s_map_snapshot* snapshot, size_t index) {
            const auto& technology = snapshot->map.research.technologies[index];
            return Json_Object(json, {
                {"stamp", Stamp(json, store, snapshot)},
                {"id", Json_String_Value(json, snapshot->technology_ids[index])},
                {"kind", Json_String_Value(json, technology.application ? "application" : "field")},
                {"name", Exploration_Knowledge(json, EXPLORATION_CURRENT, Json_String_Value(json, technology.name))},
                {"description", Exploration_Knowledge(json, EXPLORATION_UNAUDITED)},
                {"cost", Research_Cost(json, &technology)},
            });
        }

        yyjson_mut_val* Leader(s_json_builder* json, const s_map_snapshot* snapshot, size_t index) {
            const auto& item = snapshot->map.leaders[index];
            auto* skills = Json_Array(json);
            for (const auto& skill : item.skills) {
                Json_Append(json, skills, Json_Object(json, {
                    {"name", Json_String_Value(json, skill.name)},
                    {"displayValue", Json_String_Value(json, skill.value)},
                }));
            }
            if (item.partial) {
                return Json_Object(json, {
                    {"id", Json_String_Value(json, snapshot->leader_ids[index])},
                    {"disclosure", Json_String_Value(json, item.disclosure)},
                    {"name", Json_String_Value(json, item.name)},
                    {"role", Exploration_Knowledge(json, item.role.empty() ? EXPLORATION_UNKNOWN : EXPLORATION_CURRENT, Json_String_Value(json, item.role))},
                    {"title", Exploration_Knowledge(json, item.disclosure != "leader-result" ? EXPLORATION_UNKNOWN : EXPLORATION_CURRENT, Json_String_Value(json, item.title))},
                    {"rank", Exploration_Knowledge(json, item.disclosure != "leader-result" ? EXPLORATION_UNKNOWN : EXPLORATION_CURRENT, Json_String_Value(json, item.rank))},
                    {"experience", Exploration_Knowledge(json, EXPLORATION_UNKNOWN)},
                    {"abilities", Exploration_Knowledge(json, item.skills_available ? EXPLORATION_CURRENT : EXPLORATION_UNKNOWN, skills)},
                    {"status", Exploration_Knowledge(json, EXPLORATION_UNKNOWN)},
                    {"assignment", Exploration_Knowledge(json, EXPLORATION_UNKNOWN)},
                    {"eta", Exploration_Knowledge(json, item.eta_available, Json_Integer(json, item.eta))},
                    {"hireCost", Exploration_Knowledge(json, EXPLORATION_UNKNOWN)},
                    {"upkeep", Exploration_Knowledge(json, EXPLORATION_UNKNOWN)},
                    {"offerRemainingTurns", Exploration_Knowledge(json, EXPLORATION_UNKNOWN)},
                    {"offerBenefits", Exploration_Knowledge(json, EXPLORATION_UNKNOWN)},
                    {"contextBonus", Exploration_Knowledge(json, item.bonus_available ? EXPLORATION_CURRENT : EXPLORATION_UNKNOWN, Json_Integer(json, item.bonus))},
                    {"description", Exploration_Knowledge(json, EXPLORATION_UNAUDITED)},
                });
            }
            yyjson_mut_val* assignment = nullptr;
            if (item.assignment_available == EXPLORATION_CURRENT) {
                assignment = Json_Object(json, {
                    {"kind", Json_String_Value(json, item.assignment_kind)},
                    {"name", Json_String_Value(json, item.assignment_name)},
                    {"target", Id_Knowledge(json, item.assignment_id)},
                });
            }
            auto* result = Json_Object(json, {
                {"id", Json_String_Value(json, snapshot->leader_ids[index])},
                {"disclosure", Json_String_Value(json, item.disclosure)},
                {"role", Json_String_Value(json, item.role)},
                {"name", Json_String_Value(json, item.name)},
                {"title", Json_String_Value(json, item.title)},
                {"rank", Json_String_Value(json, item.rank)},
                {"experience", Exploration_Knowledge(json, item.xp_available ? EXPLORATION_CURRENT : EXPLORATION_UNAUDITED, Json_Integer(json, item.xp))},
                {"abilities", skills},
                {"status", Json_String_Value(json, item.status)},
                {"assignment", Exploration_Knowledge(json, item.assignment_available, assignment)},
                {"eta", Exploration_Knowledge(json, item.eta_available, Json_Integer(json, item.eta))},
                {"hireCost", Exploration_Knowledge(json, item.hire_cost_available ? EXPLORATION_CURRENT : EXPLORATION_UNAUDITED, Json_Integer(json, item.hire_cost))},
                {"upkeep", Json_Integer(json, item.upkeep)},
                {"offerRemainingTurns", Exploration_Knowledge(json, item.offer_turns_available ? EXPLORATION_CURRENT : EXPLORATION_UNAUDITED, Json_Integer(json, item.offer_turns))},
                {"offerBenefits", Exploration_Knowledge(json, item.benefits_available ? EXPLORATION_CURRENT : EXPLORATION_UNAUDITED, Json_String_Value(json, item.benefits))},
            });
            if (item.bonus_available) Json_Add(json, result, "contextBonus", Exploration_Knowledge(json, EXPLORATION_CURRENT, Json_Integer(json, item.bonus)));
            Json_Add(json, result, "description", Exploration_Knowledge(json, EXPLORATION_UNAUDITED));
            return result;
        }

        yyjson_mut_val* Relations(s_json_builder* json, const s_map_snapshot* snapshot) {
            auto* result = Json_Array(json);
            for (const auto& item : snapshot->map.relations) {
                const auto known = item.available ? EXPLORATION_CURRENT : EXPLORATION_UNKNOWN;
                auto Agreement = [&](bool present, int32_t level, bool research) {
                    return Exploration_Knowledge(json, !item.available ? EXPLORATION_UNKNOWN : (present ? EXPLORATION_CURRENT : EXPLORATION_EMPTY),
                        Json_Object(json, {
                            {"level", Json_Integer(json, level)},
                            {"unit", Json_String_Value(json, research && level >= 0 ? "rp-per-turn" : "bc-per-turn")},
                        }));
                };
                auto* tribute = Json_Object(json, {
                    {"direction", Json_String_Value(json, item.giving ? "giving" : "receiving")},
                    {"percent", Json_Integer(json, item.tribute_percent)},
                });
                auto* entries = Json_Array(json);
                for (const auto& entry : item.report) {
                    Json_Append(json, entries, Json_Object(json, {
                        {"player", Id_Knowledge(json, entry.player_id)},
                        {"name", Json_String_Value(json, entry.name)},
                        {"kind", Json_String_Value(json, entry.alliance ? "alliance" : "war")},
                    }));
                }
                Json_Append(json, result, Json_Object(json, {
                    {"player", Json_String_Value(json, item.player_id)},
                    {"relationship", Exploration_Knowledge(json, item.available && !item.relationship.empty() ? EXPLORATION_CURRENT : EXPLORATION_UNKNOWN, Json_String_Value(json, item.relationship))},
                    {"relationshipIndicatorOffset", Exploration_Knowledge(json, known, Json_Integer(json, item.relationship_offset))},
                    {"treaty", Exploration_Knowledge(json, item.available && !item.treaty.empty() ? EXPLORATION_CURRENT : EXPLORATION_UNKNOWN, Json_String_Value(json, item.treaty))},
                    {"ignoring", Exploration_Knowledge(json, known, Json_Boolean(json, item.ignoring))},
                    {"researchAgreement", Agreement(item.research, item.research_level, true)},
                    {"tradeAgreement", Agreement(item.trade, item.trade_level, false)},
                    {"tribute", Exploration_Knowledge(json, !item.available ? EXPLORATION_UNKNOWN : (item.tribute_percent != 0 ? EXPLORATION_CURRENT : EXPLORATION_EMPTY), tribute)},
                    {"incomingSpies", Exploration_Knowledge(json, item.incoming_available ? EXPLORATION_CURRENT : EXPLORATION_UNKNOWN, Json_Integer(json, item.incoming))},
                    {"report", Exploration_Knowledge(json, item.report_available ? EXPLORATION_CURRENT : EXPLORATION_UNKNOWN,
                        Json_Object(json, {
                            {"personality", Exploration_Knowledge(json, item.personality.empty() ? EXPLORATION_UNKNOWN : EXPLORATION_CURRENT, Json_String_Value(json, item.personality))},
                            {"objective", Exploration_Knowledge(json, item.objective_empty ? EXPLORATION_EMPTY : (item.objective.empty() ? EXPLORATION_UNKNOWN : EXPLORATION_CURRENT), Json_String_Value(json, item.objective))},
                            {"treaties", entries},
                        }))},
                }));
            }
            return result;
        }

        yyjson_mut_val* Intelligence(s_json_builder* json, const s_game_store* store, const s_map_snapshot* snapshot, bool stamped = true) {
            const auto& item = snapshot->map.intelligence;
            auto* result = Json_Object(json);
            if (stamped) Json_Add(json, result, "stamp", Stamp(json, store, snapshot));
            Json_Add(json, result, "agents", Exploration_Knowledge(json, item.available ? EXPLORATION_CURRENT : EXPLORATION_UNKNOWN, Json_Integer(json, item.agents)));
            Json_Add(json, result, "spyBonusPercent", Exploration_Knowledge(json, item.bonuses_available ? EXPLORATION_CURRENT : EXPLORATION_UNKNOWN, Json_Integer(json, item.spy_bonus)));
            Json_Add(json, result, "agentBonusPercent", Exploration_Knowledge(json, item.bonuses_available ? EXPLORATION_CURRENT : EXPLORATION_UNKNOWN, Json_Integer(json, item.agent_bonus)));
            auto* allocations = Json_Array(json);
            for (const auto& assignment : snapshot->map.relations) {
                const char* mission = assignment.mission == 0 ? "espionage" : (assignment.mission == 1 ? "sabotage" : "hide");
                Json_Append(json, allocations, Json_Object(json, {
                    {"player", Json_String_Value(json, assignment.player_id)},
                    {"spies", Exploration_Knowledge(json, assignment.assignment_available ? EXPLORATION_CURRENT : EXPLORATION_UNKNOWN, Json_Integer(json, assignment.spies))},
                    {"mission", Exploration_Knowledge(json, assignment.assignment_available && assignment.mission >= 0 ? EXPLORATION_CURRENT : EXPLORATION_UNKNOWN, Json_String_Value(json, mission))},
                }));
            }
            Json_Add(json, result, "allocations", allocations);
            return result;
        }

        yyjson_mut_val* History(s_json_builder* json, const s_game_store* store, const s_map_snapshot* snapshot, bool stamped = true) {
            const auto& history = snapshot->map.history;
            auto* result = Json_Object(json);
            if (stamped) Json_Add(json, result, "stamp", Stamp(json, store, snapshot));
            Json_Add(json, result, "smoothing", Json_Boolean(json, history.smoothing));
            Json_Add(json, result, "xStep", Json_Integer(json, history.x_step));
            auto* labels = Json_Array(json);
            for (const auto& label : history.labels) Json_Append(json, labels, Json_String_Value(json, label));
            Json_Add(json, result, "dateLabels", labels);
            auto* charts = Json_Array(json);
            for (size_t index = 0; index < history.charts.size(); ++index) {
                const auto& chart = history.charts[index];
                auto* series = Json_Array(json);
                const char* names[] = {"population", "production", "fleet", "technology"};
                for (int bit = 0; bit < 4; ++bit) {
                    if (!(chart.series & (1 << bit))) continue;
                    Json_Append(json, series, Json_String_Value(json, names[bit]));
                }
                auto* curves = Json_Array(json);
                for (const auto& curve : chart.curves) {
                    auto* points = Json_Array(json);
                    for (size_t point = 0; point < curve.y.size(); ++point) {
                        Json_Append(json, points, Json_Object(json, {
                            {"x", Json_Unsigned(json, 238 + point * history.x_step)},
                            {"y", Json_Integer(json, curve.y[point])},
                        }));
                    }
                    Json_Append(json, curves, Json_Object(json, {
                        {"player", Json_String_Value(json, curve.player_id)},
                        {"points", points},
                    }));
                }
                Json_Append(json, charts, Json_Object(json, {
                    {"selection", Json_Boolean(json, index == 4)},
                    {"series", series},
                    {"topScale", Json_Integer(json, chart.top_scale)},
                    {"curves", curves},
                }));
            }
            Json_Add(json, result, "charts", charts);
            return result;
        }

        yyjson_mut_val* Message(s_json_builder* json, const s_map_snapshot* snapshot, size_t index) {
            const auto& message = snapshot->map.messages[index];
            return Json_Object(json, {
                {"id", Json_String_Value(json, snapshot->message_ids[index])},
                {"text", Exploration_Knowledge(json, message.text_available ? EXPLORATION_CURRENT : EXPLORATION_UNKNOWN, Json_String_Value(json, message.text))},
                {"serious", Json_Boolean(json, message.serious)},
                {"colony", Id_Knowledge(json, message.colony_id)},
            });
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
                case TOPIC_LEADERS: return &snapshot->leader_ids;
                case TOPIC_MESSAGES: return &snapshot->message_ids;
                default: return nullptr;
            }
        }

        yyjson_mut_val* Collection_Item(s_json_builder* json, const s_game_store* store, const s_map_snapshot* snapshot, e_topic topic, size_t index) {
            switch (topic) {
                case TOPIC_SYSTEMS: return System(json, store, snapshot, index);
                case TOPIC_NEBULAS: return Nebula(json, store, snapshot, index);
                case TOPIC_SHIPS: return Ship(json, snapshot, index);
                case TOPIC_PLAYERS: return Player(json, snapshot, index);
                case TOPIC_COLONIES: return Colony(json, snapshot, index);
                case TOPIC_PLANETS: return Planet(json, snapshot, index);
                case TOPIC_RESEARCH: return Research_Option(json, snapshot, index);
                case TOPIC_LEADERS: return Leader(json, snapshot, index);
                case TOPIC_MESSAGES: return Message(json, snapshot, index);
                default: json->failed = true; return nullptr;
            }
        }

        yyjson_mut_val* Collection(s_json_builder* json, const s_game_store* store, const s_map_snapshot* snapshot, e_topic topic, size_t offset, size_t limit) {
            const auto* ids = Collection_Ids(store, snapshot, topic);
            const size_t count = ids != nullptr ? ids->size() : 0;
            auto* result = Json_Array(json);
            for (size_t index = offset; index < count && index - offset < limit; ++index) {
                Json_Append(json, result, Collection_Item(json, store, snapshot, topic, index));
            }
            return result;
        }

        std::string Text(yyjson_val* object, const char* key) {
            yyjson_val* value = yyjson_obj_get(object, key);
            return yyjson_is_str(value) ? std::string(yyjson_get_str(value), yyjson_get_len(value)) : std::string();
        }

        void Topic_Projection(s_json_builder* json, yyjson_mut_val* result, const s_game_store* store, const s_map_snapshot* snapshot, const std::vector<std::string>* topics, bool stamped = false) {
            for (const std::string& topic : *topics) {
                const e_topic selected = Find_Topic(topic);
                yyjson_mut_val* value;
                if (selected == TOPIC_SESSION)
                    value = Session(json, store, snapshot);
                else if (selected == TOPIC_RESEARCH)
                    value = Research(json, store, snapshot, stamped);
                else if (selected == TOPIC_RELATIONS)
                    value = Relations(json, snapshot);
                else if (selected == TOPIC_INTELLIGENCE)
                    value = Intelligence(json, store, snapshot, stamped);
                else if (selected == TOPIC_HISTORY)
                    value = History(json, store, snapshot, stamped);
                else
                    value = Collection(json, store, snapshot, selected, 0, 10000);
                Json_Add(json, result, topic, value);
            }
        }

        std::string State_Projection(const s_game_store* store, const s_map_snapshot* snapshot, const std::vector<std::string>* topics) {
            s_json_builder json;
            Initialize_Json(&json);
            auto* root = Json_Object(&json);
            Topic_Projection(&json, root, store, snapshot, topics);
            std::string result;
            Write_Json(&json, root, &result);
            Shutdown_Json(&json);
            return result;
        }

        yyjson_mut_val* Topics_Json(s_json_builder* json, const std::vector<std::string>* topics) {
            auto* result = Json_Array(json);
            for (const std::string& topic : *topics) {
                Json_Append(json, result, Json_String_Value(json, topic));
            }
            return result;
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
                s_json_builder json;
                Initialize_Json(&json);
                const std::string response = Result_Response(&json, request, Json_Object(&json, {{"removed", Json_Boolean(&json, removed != 0)}}));
                Shutdown_Json(&json);
                return response;
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
            if (!store->loaded || store->snapshots.empty()) return Error_Response(request, RPC_BUSY);
            const uint32_t topics = Topic_Mask(&subscription.topics);
            const auto* snapshot = Select_Snapshot(store, topics);
            if (snapshot == nullptr) return Error_Response(request, RPC_BUSY);
            if (topics & (TOPIC_SHIPS | TOPIC_LEADERS | TOPIC_RELATIONS)) *context_epoch = snapshot->map.context_epoch;
            if (topics & (TOPIC_MESSAGES | TOPIC_COLONIES | TOPIC_PLANETS)) {
                *context_epoch = Bridge_Context_Epoch(store->bridge);
                store->context_active = true;
            }
            if (*context_epoch != 0) subscription.baseline_ticket = work->ticket;
            const size_t client_count = std::count_if(store->subscriptions.begin(), store->subscriptions.end(), [work](const s_state_subscription& item) { return item.client == work->client; });
            if (store->subscriptions.size() >= 64 || client_count >= 16) {
                return Error_Response(request, RPC_LIMIT_EXCEEDED);
            }
            if (!store->new_id(&subscription.id, store->id_context)) return Error_Response(request, RPC_INTERNAL_ERROR);
            subscription.projection = State_Projection(store, snapshot, &subscription.topics);
            if (subscription.projection.empty()) return Error_Response(request, RPC_INTERNAL_ERROR);
            subscription.revision = store->revision;
            s_json_builder json;
            Initialize_Json(&json);
            auto* baseline = Json_Object(&json, {{"stamp", Stamp(&json, store, snapshot)}});
            Topic_Projection(&json, baseline, store, snapshot, &subscription.topics, true);
            const std::string response = Result_Response(&json, request, Json_Object(&json, {
                {"subscriptionId", Json_String_Value(&json, subscription.id)},
                {"cursor", Json_String_Value(&json, "0")},
                {"baseline", baseline},
            }));
            const bool failed = json.failed;
            Shutdown_Json(&json);
            if (failed) {
                *context_epoch = 0;
                return response;
            }
            if (response.size() > 1048576) return Error_Response(request, RPC_LIMIT_EXCEEDED);
            store->subscriptions.push_back(std::move(subscription));
            return response;
        }

        void Publish_State_Changes(s_game_store* store) {
            if (!store->loaded || store->snapshots.empty()) return;
            for (s_state_subscription& subscription : store->subscriptions) {
                if (subscription.revision == store->revision) continue;
                subscription.revision = store->revision;
                if (!subscription.state_changes) continue;
                const uint32_t topics = Topic_Mask(&subscription.topics);
                const auto* snapshot = Select_Snapshot(store, topics);
                if (snapshot == nullptr) {
                    if (store->busy_depth != 0) {
                        subscription.revision = 0;
                        continue;
                    }
                    Cancel_Client(store->bridge, subscription.client);
                    continue;
                }
                std::string projection = State_Projection(store, snapshot, &subscription.topics);
                if (projection.empty() || projection.size() > 1048576) {
                    Cancel_Client(store->bridge, subscription.client);
                    continue;
                }
                if (projection == subscription.projection) continue;
                if (subscription.cursor == std::numeric_limits<uint64_t>::max()) {
                    Cancel_Client(store->bridge, subscription.client);
                    continue;
                }
                s_json_builder json;
                Initialize_Json(&json);
                auto* event = Json_Notification(&json, "state.changed", Json_Object(&json, {
                    {"subscriptionId", Json_String_Value(&json, subscription.id)},
                    {"cursor", Json_String_Value(&json, std::to_string(++subscription.cursor))},
                    {"topics", Topics_Json(&json, &subscription.topics)},
                    {"stamp", Stamp(&json, store, snapshot)},
                }));
                std::string encoded;
                const bool written = Write_Json(&json, event, &encoded);
                Shutdown_Json(&json);
                if (!written) {
                    Cancel_Client(store->bridge, subscription.client);
                    continue;
                }
                uint64_t lease = (topics & (TOPIC_SHIPS | TOPIC_LEADERS | TOPIC_RELATIONS)) ? snapshot->map.context_epoch : 0;
                if (topics & (TOPIC_MESSAGES | TOPIC_COLONIES | TOPIC_PLANETS)) {
                    lease = Bridge_Context_Epoch(store->bridge);
                    store->context_active = true;
                }
                if (Publish_Event(store->bridge, subscription.client, subscription.epoch, &encoded, &subscription.id, lease)) subscription.projection = std::move(projection);
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
                s_json_builder json;
                Initialize_Json(&json);
                auto* event = Json_Notification(&json, "game.turnAdvanced", Json_Object(&json, {
                    {"gameId", Json_String_Value(&json, store->game_id)},
                    {"turn", Json_Integer(&json, turn)},
                    {"revision", Json_String_Value(&json, std::to_string(store->revision))},
                }));
                std::string encoded;
                const bool written = Write_Json(&json, event, &encoded);
                Shutdown_Json(&json);
                if (!written) {
                    Cancel_Client(store->bridge, observer.identity.client);
                    continue;
                }
                if (Publish_Event(store->bridge, observer.identity.client, observer.identity.epoch, &encoded)) observer.turn = turn;
            }
        }

        yyjson_mut_val* Query(s_json_builder* json, s_game_store* store, const s_request* request, yyjson_val* params, int32_t* error, uint64_t* context_epoch) {
            if (request->method == "game.getSession") {
                return Session(json, store);
            }
            if (store->snapshots.empty() || !store->loaded) {
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
            if (topics & (TOPIC_SHIPS | TOPIC_LEADERS | TOPIC_RELATIONS)) *context_epoch = snapshot->map.context_epoch;
            if (topics & (TOPIC_MESSAGES | TOPIC_COLONIES | TOPIC_PLANETS)) {
                *context_epoch = Bridge_Context_Epoch(store->bridge);
                store->context_active = true;
            }
            if (state) {
                auto* result = Json_Object(json, {{"stamp", Stamp(json, store, snapshot)}});
                Topic_Projection(json, result, store, snapshot, &selected, true);
                return result;
            }
            if (request->method == "game.getResearch") return Research(json, store, snapshot);
            if (request->method == "game.getRelations") return Json_Object(json, {
                {"stamp", Stamp(json, store, snapshot)},
                {"items", Relations(json, snapshot)},
            });
            if (request->method == "game.getIntelligence") return Intelligence(json, store, snapshot);
            if (request->method == "game.getHistory") return History(json, store, snapshot);
            if (request->method == "game.getTechnology") {
                const std::string id = Text(params, "id");
                for (size_t index = 0; index < snapshot->technology_ids.size(); ++index) {
                    if (snapshot->technology_ids[index] == id) return Technology(json, store, snapshot, index);
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
                        return Json_Object(json, {
                            {"stamp", Stamp(json, store, snapshot)},
                            {"item", Collection_Item(json, store, snapshot, method->topic, index)},
                        });
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
                if (pinned == store->snapshots.end() || ((topics & TOPIC_MESSAGES) && !pinned->map.messages_captured) || ((topics & TOPIC_COLONIES) && !pinned->map.colonies_captured) || ((topics & (TOPIC_SHIPS | TOPIC_LEADERS)) && !Authorized_Snapshot(store, &*pinned))) {
                    *error = RPC_OBJECT_UNAVAILABLE;
                    return {};
                }
                snapshot = &*pinned;
                if (topics & (TOPIC_SHIPS | TOPIC_LEADERS)) *context_epoch = snapshot->map.context_epoch;
                offset = cursor->offset;
                limit = cursor->limit;
            }
            const size_t total = Collection_Ids(store, snapshot, method->topic)->size();
            auto* result = Json_Object(json, {
                {"stamp", Stamp(json, store, snapshot)},
                {"items", Collection(json, store, snapshot, method->topic, offset, limit)},
            });
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
                Json_Add(json, result, "nextCursor", Json_String_Value(json, next->id));
            }
            return result;
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
            return snapshot == store->snapshots.end() || ((cursor.method == "game.listShips" || cursor.method == "game.listLeaders") && !Authorized_Snapshot(store, &*snapshot));
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
        Expire_Game_Messages(store);
    }

    void Expire_Game_Messages(s_game_store* store) {
        if (store == nullptr) return;
        const bool retained = std::any_of(store->snapshots.begin(), store->snapshots.end(), [](const s_map_snapshot& snapshot) { return snapshot.map.messages_captured; });
        if (!retained) return;
        for (const auto& subscription : store->subscriptions) {
            if (Context_Response_Pending(store->bridge, subscription.client, subscription.baseline_ticket)) Cancel_Client(store->bridge, subscription.client);
        }
        Revoke_Bridge_Context(store->bridge);
        store->context_active = false;
        for (auto& snapshot : store->snapshots) {
            snapshot.map.messages_captured = false;
            snapshot.map.messages.clear();
            snapshot.message_ids.clear();
        }
        std::erase_if(store->cursors, [](const s_map_cursor& cursor) { return cursor.method == "game.listMessages"; });
        store->capture_requested = true;
        for (auto& subscription : store->subscriptions) subscription.revision = 0;
    }

    void Expire_Game_Colonies(s_game_store* store) {
        if (store == nullptr || std::none_of(store->snapshots.begin(), store->snapshots.end(), [](const auto& snapshot) { return snapshot.map.colonies_captured; })) return;
        for (const auto& subscription : store->subscriptions)
            if (Context_Response_Pending(store->bridge, subscription.client, subscription.baseline_ticket)) Cancel_Client(store->bridge, subscription.client);
        Revoke_Bridge_Context(store->bridge);
        store->context_active = false;
        for (auto& snapshot : store->snapshots) {
            snapshot.map.colonies_captured = false;
            snapshot.map.colonies.clear();
            snapshot.colony_ids.clear();
            for (auto& planet : snapshot.map.planets) {
                if (!planet.colony_id.empty()) {
                    planet.max_population_available = false;
                    planet.max_population = 0;
                }
                planet.colony_id.clear();
            }
            for (auto& message : snapshot.map.messages) message.colony_id.clear();
        }
        std::erase_if(store->cursors, [](const auto& cursor) { return cursor.method == "game.listColonies" || cursor.method == "game.listPlanets"; });
        store->capture_requested = true;
        for (auto& subscription : store->subscriptions) subscription.revision = 0;
    }

    void Resume_Game_Queries(s_game_store* store) {
        if (store->busy_depth != 0) --store->busy_depth;
    }

    bool Capture_Map(s_game_store* store, const s_map_observation* observation) {
        if (store == nullptr || observation == nullptr || !store->loaded || (store->busy_depth != 0 && !observation->leaders_only) ||
            observation->viewer < 0 || observation->viewer >= 8 || observation->turn < 0 ||
            observation->systems.size() > 1024 || observation->nebulas.size() > 57 || observation->ships.size() > 10000 || observation->players.size() > 8 || observation->colonies.size() > 5120) return false;
        if (observation->leaders_only && (!observation->leader_presentation || observation->viewer != store->viewer || !observation->systems.empty() || !observation->nebulas.empty() || observation->ships_captured || observation->players_captured || observation->colonies_captured || observation->planets_captured || observation->research_captured)) return false;
        if (observation->leader_presentation && (observation->context_epoch == 0 || !observation->leaders_captured)) return false;
        if (observation->context_epoch != 0 && ((!observation->ships_captured && !observation->leader_presentation && !observation->relations_presentation) || observation->context_epoch != Bridge_Context_Epoch(store->bridge))) return false;
        if (observation->relations_presentation && (!observation->relations_captured || observation->context_epoch == 0 || observation->leaders_only)) return false;
        // Bind newly authorized default observers to the old coherent turn before replacing it.
        Sync_Turn_Observers(store);
        s_map_observation projected;
        projected.viewer = observation->viewer;
        projected.turn = observation->turn;
        projected.systems = observation->systems;
        projected.nebulas = observation->nebulas;
        projected.ships_captured = observation->ships_captured;
        projected.ships = observation->ships;
        projected.players_captured = observation->players_captured;
        projected.players = observation->players;
        projected.colonies_captured = observation->colonies_captured;
        if (observation->colonies_captured)
            for (const auto& colony : observation->colonies)
                if (colony.owner_slot == observation->viewer) projected.colonies.push_back(colony);
        projected.context_epoch = observation->context_epoch;
        projected.leader_presentation = observation->leader_presentation;
        projected.leaders_only = observation->leaders_only;
        projected.planets_captured = observation->planets_captured;
        projected.planets = observation->planets;
        projected.research_captured = observation->research_captured;
        projected.research = observation->research;
        projected.leaders_captured = observation->leaders_captured;
        projected.relations_captured = observation->relations_captured;
        projected.relations_presentation = observation->relations_presentation;
        if (projected.relations_captured) {
            projected.relations = observation->relations;
            projected.intelligence = observation->intelligence;
        }
        if (projected.leaders_captured) {
            for (const auto& leader : observation->leaders) {
                if (leader.visible) projected.leaders.push_back(leader);
            }
        }
        if (!projected.leaders_captured) projected.leaders.clear();
        std::erase_if(projected.leaders, [](const s_leader_observation& leader) { return !leader.visible; });
        if (projected.leaders.size() > 67) return false;
        bool leader_slots[67] = {};
        for (auto& leader : projected.leaders) {
            if (leader.partial) {
                const bool result = leader.disclosure == "leader-result";
                const bool ground = leader.disclosure == "ground-summary" || leader.disclosure == "troop-summary";
                if (!projected.leader_presentation || (!result && !ground && leader.disclosure != "message-name")) return false;
                leader.xp_available = leader.hire_cost_available = leader.offer_turns_available = leader.benefits_available = false;
                leader.upkeep = 0;
                leader.assignment_available = EXPLORATION_UNKNOWN;
                if (!result || leader.eta_available != EXPLORATION_CURRENT || leader.eta <= 0) leader.eta_available = EXPLORATION_UNKNOWN;
                leader.status = "unavailable";
                if (!result) {
                    leader.role.clear();
                    leader.title.clear();
                    leader.rank.clear();
                    leader.skills_available = false;
                }
                if (!leader.skills_available) leader.skills.clear();
                leader.bonus_available &= ground;
            }
            if (!leader.bonus_available) leader.bonus = 0;
            if (leader.slot < 0 || leader.slot >= 67 || leader_slots[leader.slot] ||
                (!leader.partial && leader.disclosure != "owned" && leader.disclosure != "pending-offer" && leader.disclosure != "hiring-pool") ||
                ((!leader.partial || leader.disclosure == "leader-result") && leader.role != "ship-officer" && leader.role != "colony-leader")) return false;
            leader_slots[leader.slot] = true;
            if (!leader.xp_available) leader.xp = 0;
            if (!leader.hire_cost_available) leader.hire_cost = 0;
            if (!leader.offer_turns_available) leader.offer_turns = 0;
            if (!leader.benefits_available) leader.benefits.clear();
            if (leader.eta_available != EXPLORATION_CURRENT) leader.eta = 0;
            if (leader.assignment_available != EXPLORATION_CURRENT) {
                leader.assignment_kind.clear();
                leader.assignment_name.clear();
                leader.assignment_slot = -1;
            }
            leader.assignment_id.clear();
            if (leader.name.size() > (leader.partial ? 1536 : 42) || leader.title.size() > 57 || leader.rank.size() > 384 || leader.benefits.size() > 3072 ||
                leader.xp < 0 || leader.xp > 32767 || leader.skills.size() > 54 || leader.upkeep < 0 || leader.upkeep > 32767 ||
                leader.hire_cost < 0 || leader.offer_turns < 0 || leader.offer_turns > 30 || leader.eta < 0 || leader.eta > 127 ||
                leader.assignment_name.size() > 80 || leader.assignment_available < EXPLORATION_UNKNOWN || leader.assignment_available > EXPLORATION_EMPTY ||
                leader.eta_available < EXPLORATION_UNKNOWN || leader.eta_available > EXPLORATION_EMPTY ||
                (leader.status != "unassigned" && leader.status != "assigned" && leader.status != "travelling" && leader.status != "limbo" && leader.status != "available-for-hire" && leader.status != "unavailable")) return false;
            for (const auto& skill : leader.skills) {
                if (skill.name.size() > 384 || skill.value.size() > 237) return false;
            }
            if (leader.assignment_available == EXPLORATION_CURRENT &&
                (leader.assignment_slot < 0 || (leader.assignment_kind != "system" && leader.assignment_kind != "ship") ||
                    (leader.assignment_kind == "system" && (size_t)leader.assignment_slot >= projected.systems.size()) ||
                    (leader.assignment_kind == "ship" && leader.assignment_slot >= 10000))) return false;
        }
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
        projected.history_captured = observation->history_captured;
        if (projected.history_captured) {
            if (!projected.players_captured || observation->history.charts.size() != 5 || observation->history.labels.size() != 8) return false;
            projected.history.x_step = observation->history.x_step;
            projected.history.smoothing = observation->history.smoothing;
            projected.history.labels = observation->history.labels;
            if (projected.history.x_step != 1 && projected.history.x_step != 2 && projected.history.x_step != 5 && projected.history.x_step != 10) return false;
            for (const auto& label : projected.history.labels)
                if (label.size() > 45) return false;
            for (const auto& source : observation->history.charts) {
                s_history_chart chart;
                chart.series = source.series;
                chart.top_scale = source.top_scale;
                if (chart.series == 0 || chart.series > 15 || chart.top_scale < 25) return false;
                bool included[8] = {};
                for (const auto& curve : source.curves) {
                    if (std::none_of(projected.players.begin(), projected.players.end(), [&curve](const auto& player) { return player.slot == curve.slot; })) continue;
                    if (included[curve.slot] || curve.y.size() > 350) return false;
                    included[curve.slot] = true;
                    for (int32_t y : curve.y) {
                        if (y < -32768 || y > 32767) return false;
                    }
                    chart.curves.push_back(curve);
                }
                projected.history.charts.push_back(std::move(chart));
            }
        }
        projected.messages_captured = observation->messages_captured;
        if (projected.messages_captured) {
            if (observation->messages.size() > 100) return false;
            projected.messages = observation->messages;
            for (auto& message : projected.messages) {
                if (!message.text_available) message.text.clear();
                if (message.text.size() > 765) return false;
                message.colony_id.clear();
            }
        }
        if (projected.relations_captured && !projected.players_captured) return false;
        std::erase_if(projected.relations, [&projected](const s_relation_observation& relation) {
            return std::none_of(projected.players.begin(), projected.players.end(), [&relation](const s_player_observation& player) { return !player.local && player.slot == relation.slot; });
        });
        bool relation_slots[8] = {};
        for (auto& relation : projected.relations) {
            const auto player = std::find_if(projected.players.begin(), projected.players.end(), [&relation](const s_player_observation& item) { return item.slot == relation.slot; });
            if (!player->contacted || player->eliminated) {
                const int32_t slot = relation.slot;
                relation = {};
                relation.slot = slot;
            }
            if (relation_slots[relation.slot]) return false;
            relation_slots[relation.slot] = true;
            if (!relation.available) {
                relation.relationship.clear();
                relation.relationship_offset = 0;
                relation.treaty.clear();
                relation.ignoring = relation.research = relation.trade = relation.giving = false;
                relation.tribute_percent = 0;
            }
            if (!relation.research) relation.research_level = 0;
            if (!relation.trade) relation.trade_level = 0;
            if (relation.tribute_percent == 0) relation.giving = false;
            if (!relation.assignment_available) {
                relation.spies = 0;
                relation.mission = -1;
            }
            if (!projected.relations_presentation || !relation.available) relation.incoming_available = relation.report_available = false;
            if (!relation.incoming_available) relation.incoming = 0;
            if (!relation.report_available) {
                relation.personality.clear();
                relation.objective.clear();
                relation.objective_empty = false;
                relation.report.clear();
            }
            relation.player_id.clear();
            if (relation.objective_empty) relation.objective.clear();
            if (relation.relationship.size() > 42 || relation.relationship_offset < -6 || relation.relationship_offset > 68 || relation.treaty.size() > 597 || relation.personality.size() > 381 || relation.objective.size() > 381 ||
                relation.research_level < -32768 || relation.research_level > 32767 || relation.trade_level < -32768 || relation.trade_level > 32767 ||
                (relation.tribute_percent != 0 && relation.tribute_percent != 5 && relation.tribute_percent != 10) || relation.spies < 0 || relation.spies > 63 ||
                relation.mission < -1 || relation.mission > 2 || relation.incoming < 0 || relation.incoming > 63 || relation.report.size() > 7) return false;
            for (auto& entry : relation.report) {
                entry.player_id.clear();
                if (entry.name.size() > 381) return false;
            }
        }
        auto& intelligence = projected.intelligence;
        if (!intelligence.available) intelligence = {};
        if (!intelligence.bonuses_available) intelligence.spy_bonus = intelligence.agent_bonus = 0;
        if (intelligence.agents < 0 || intelligence.agents > 63 || intelligence.spy_bonus < -32768 || intelligence.spy_bonus > 32767 || intelligence.agent_bonus < -32768 || intelligence.agent_bonus > 32767) return false;
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
            auto& operations = colony.operations;
            if (!operations.available) operations = {};
            if (!operations.heading_available) operations.heading.clear();
            if (!operations.event_present) operations.event.clear();
            if (!operations.status_present) operations.status.clear();
            if (operations.morale_suppressed) operations.morale_icons = 0;
            if (!operations.production_available) {
                operations.cost = operations.stored = operations.bought = operations.turns = operations.buy_cost = 0;
                operations.can_buy = false;
            }
            if (operations.turns < 1 || operations.turns > 9999) operations.turns = 0;
            if (operations.available && (operations.queue.size() != 7 || operations.resources.size() != 4)) return false;
            if (!operations.garrison_available) operations.garrison.clear();
            if (operations.garrison_available && operations.garrison.size() != 3) return false;
            for (const auto& unit : operations.garrison)
                if (unit.label.size() > 4096 || unit.count < 0 || unit.count > 32767 || unit.strength < -32768 || unit.strength > 32767 || unit.hp < 0 || unit.hp > 255) return false;
            if (operations.workforce.size() > 42 || operations.structures.size() > 48 || operations.population_text.size() > 4096 || operations.heading.size() > 4096 ||
                operations.autobuild.size() > 4096 || operations.event.size() > 4096 || operations.status.size() > 4096 || operations.infantry_name.size() > 4096 || operations.armor_name.size() > 4096 ||
                operations.morale_icons < -20 || operations.morale_icons > 20 || operations.inbound < 0 || operations.farm_capacity < -1 || operations.farm_capacity > 254) return false;
            for (auto& product : operations.queue) {
                if (!product.available) product = {};
                if (product.label.size() > 4096) return false;
            }
            for (auto& worker : operations.workforce) {
                worker.player_id.clear();
                if (worker.label.size() > 4096 || worker.species.size() > 4096 ||
                    (worker.job != "farmer" && worker.job != "worker" && worker.job != "scientist") ||
                    (worker.state != "colonist" && worker.state != "native" && worker.state != "android")) return false;
            }
            for (const auto& structure : operations.structures)
                if (structure.name.size() > 4096) return false;
            for (const auto& resource : operations.resources)
                if (resource.local < 0 || resource.local > 65535 || resource.exchanged < 0 || resource.exchanged > 65535 ||
                    resource.secondary < 0 || resource.secondary > 65535 || resource.shortage < 0 || resource.shortage > 65535) return false;
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
            const auto previous_full = std::find_if(store->snapshots.rbegin(), store->snapshots.rend(), [](const s_map_snapshot& item) { return !item.map.leaders_only; });
            const auto& previous = previous_full != store->snapshots.rend() ? previous_full->map : projected;
            bool lost = false;
            if ((projected.relations_captured || projected.history_captured || projected.messages_captured) && projected.players_captured && previous.players_captured) {
                for (const auto& player : previous.players) {
                    const auto current = std::find_if(projected.players.begin(), projected.players.end(), [&player](const s_player_observation& item) { return item.slot == player.slot; });
                    lost |= current == projected.players.end() || current->name != player.name || current->color != player.color ||
                        (player.contacted && !current->contacted) || (player.eliminated != current->eliminated);
                }
            }
            if (projected.leaders_captured && !projected.leaders_only) {
                const auto old = std::find_if(store->snapshots.rbegin(), store->snapshots.rend(), [](const s_map_snapshot& item) { return item.map.leaders_captured && item.map.context_epoch == 0; });
                if (old != store->snapshots.rend()) {
                    for (const auto& leader : old->map.leaders) {
                        lost |= std::none_of(projected.leaders.begin(), projected.leaders.end(), [&leader](const s_leader_observation& current) {
                            return current.slot == leader.slot && current.name == leader.name && current.role == leader.role;
                        });
                    }
                }
            }
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
            bool colony_lost = false;
            if (projected.colonies_captured) {
                for (const auto& previous_colony : previous.colonies) {
                    colony_lost |= std::none_of(projected.colonies.begin(), projected.colonies.end(), [&previous_colony, store](const auto& current) {
                        return current.system >= 0 && (size_t)current.system < store->system_ids.size() &&
                            store->system_ids[current.system] == previous_colony.system_id && current.orbit == previous_colony.orbit;
                    });
                }
            }
            if (lost) Revoke_Game_View(store);
            else if (colony_lost)
                Expire_Game_Colonies(store);
        }
        const auto* ambient_map = Select_Snapshot(store, TOPIC_SYSTEMS);
        if (!projected.leaders_only && ambient_map != nullptr && (projected.systems.size() != ambient_map->map.systems.size() || projected.nebulas.size() != ambient_map->map.nebulas.size())) {
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
        for (auto& relation : projected.relations) {
            const auto player = std::find_if(projected.players.begin(), projected.players.end(), [&relation](const s_player_observation& item) { return item.slot == relation.slot; });
            relation.player_id = player_ids[(size_t)(player - projected.players.begin())];
            relation.slot = -1;
            for (auto& entry : relation.report) {
                const auto target = std::find_if(projected.players.begin(), projected.players.end(), [&entry](const s_player_observation& item) { return item.slot == entry.slot; });
                if (target != projected.players.end()) entry.player_id = player_ids[(size_t)(target - projected.players.begin())];
                entry.slot = -1;
            }
            std::sort(relation.report.begin(), relation.report.end(), [](const auto& a, const auto& b) { return std::tie(a.name, a.alliance, a.player_id) < std::tie(b.name, b.alliance, b.player_id); });
        }
        std::sort(projected.relations.begin(), projected.relations.end(), [](const auto& a, const auto& b) { return a.player_id < b.player_id; });
        for (auto& chart : projected.history.charts) {
            for (auto& curve : chart.curves) {
                const auto player = std::find_if(projected.players.begin(), projected.players.end(), [&curve](const auto& item) { return item.slot == curve.slot; });
                curve.player_id = player_ids[(size_t)(player - projected.players.begin())];
                curve.slot = -1;
            }
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
            for (auto& worker : colony.operations.workforce) {
                const auto player = std::find_if(projected.players.begin(), projected.players.end(), [&worker](const auto& item) { return item.slot == worker.player_slot; });
                if (player != projected.players.end()) worker.player_id = player_ids[(size_t)(player - projected.players.begin())];
                worker.player_slot = -1;
            }
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
        for (auto& message : projected.messages) {
            const auto colony = std::find_if(projected.colonies.begin(), projected.colonies.end(), [&message, store](const auto& item) {
                return message.colony_system >= 0 && (size_t)message.colony_system < store->system_ids.size() && item.system_id == store->system_ids[message.colony_system] && item.orbit == message.colony_orbit;
            });
            if (colony != projected.colonies.end()) message.colony_id = colony_ids[(size_t)(colony - projected.colonies.begin())];
            message.colony_system = -1;
            message.colony_orbit = 0;
        }
        for (auto& planet : projected.planets) {
            const auto colony = std::find_if(projected.colonies.begin(), projected.colonies.end(), [&planet](const s_colony_observation& item) { return item.system_id == planet.system_id && item.orbit == planet.orbit; });
            if (colony != projected.colonies.end()) planet.colony_id = colony_ids[(size_t)(colony - projected.colonies.begin())];
            planet.system = -1;
        }
        std::vector<std::string> ship_ids;
        const s_map_snapshot* previous_ships = Select_Snapshot(store, TOPIC_SHIPS);
        if (projected.context_epoch != 0) {
            for (auto item = store->snapshots.rbegin(); item != store->snapshots.rend(); ++item) {
                if (item->map.context_epoch != 0 && Same_Ships(&projected, &item->map)) {
                    previous_ships = &*item;
                    break;
                }
            }
        }
        if (projected.ships_captured && previous_ships != nullptr && Same_Ships(&projected, &previous_ships->map)) ship_ids = previous_ships->ship_ids;
        if (!Extend(&ship_ids, projected.ships.size())) return false;
        for (auto& leader : projected.leaders) {
            if (leader.assignment_available == EXPLORATION_CURRENT) {
                if (leader.assignment_kind == "system") {
                    leader.assignment_id = store->system_ids[leader.assignment_slot];
                } else {
                    const auto ship = std::find_if(projected.ships.begin(), projected.ships.end(), [&leader](const s_ship_observation& item) {
                        return item.owned && item.slot == leader.assignment_slot;
                    });
                    if (ship != projected.ships.end()) leader.assignment_id = ship_ids[(size_t)(ship - projected.ships.begin())];
                }
            }
            leader.assignment_slot = -1;
        }
        for (auto& ship : projected.ships) ship.slot = -1;
        std::sort(projected.leaders.begin(), projected.leaders.end(), [](const s_leader_observation& a, const s_leader_observation& b) {
            if (Leader_Data(a) != Leader_Data(b)) return Leader_Data(a) < Leader_Data(b);
            return std::lexicographical_compare(a.skills.begin(), a.skills.end(), b.skills.begin(), b.skills.end(), [](const s_leader_skill& x, const s_leader_skill& y) {
                return std::tie(x.name, x.value) < std::tie(y.name, y.value);
            });
        });
        const auto* old_presentation = Select_Snapshot(store, TOPIC_LEADERS);
        if (projected.leader_presentation && old_presentation != nullptr && old_presentation->map.leader_presentation && !Same_Leaders(&projected, &old_presentation->map)) {
            Revoke_Game_Context(store);
            projected.context_epoch = Bridge_Context_Epoch(store->bridge);
        }
        const auto* old_relations = Select_Snapshot(store, TOPIC_RELATIONS);
        if (projected.relations_presentation && old_relations != nullptr && old_relations->map.relations_presentation && !Same_Relations(&projected, &old_relations->map)) {
            Revoke_Game_Context(store);
            projected.context_epoch = Bridge_Context_Epoch(store->bridge);
        }
        if (projected.context_epoch == 0 && !store->snapshots.empty() && store->snapshots.back().map.context_epoch != 0) {
            const auto ambient = std::find_if(store->snapshots.rbegin(), store->snapshots.rend(), [&projected](const s_map_snapshot& item) {
                return item.map.context_epoch == 0 && !item.map.leaders_only &&
                    (!projected.ships_captured || item.map.ships_captured) && (!projected.players_captured || item.map.players_captured) &&
                    (!projected.colonies_captured || item.map.colonies_captured) && (!projected.planets_captured || item.map.planets_captured) &&
                    (!projected.research_captured || item.map.research_captured) && (!projected.leaders_captured || item.map.leaders_captured) &&
                    (!projected.relations_captured || item.map.relations_captured) &&
                    (!projected.history_captured || item.map.history_captured) && (!projected.messages_captured || item.map.messages_captured);
            });
            if (ambient != store->snapshots.rend() && Same_Map(&projected, &ambient->map) && (!projected.ships_captured || Same_Ships(&projected, &ambient->map)) &&
                (!projected.players_captured || (Same_Players(&projected, &ambient->map) && player_ids == ambient->player_ids)) &&
                (!projected.colonies_captured || Same_Colonies(&projected, &ambient->map)) && (!projected.planets_captured || Same_Planets(&projected, &ambient->map)) &&
                (!projected.research_captured || Same_Research(&projected, &ambient->map)) &&
                (!projected.leaders_captured || Same_Leaders(&projected, &ambient->map)) && Same_Relations(&projected, &ambient->map) &&
                Same_History(&projected, &ambient->map) && Same_Messages(&projected, &ambient->map)) return true;
        }
        const bool equal = !store->snapshots.empty() && projected.leaders_only == store->snapshots.back().map.leaders_only && Same_Map(&projected, &store->snapshots.back().map) &&
            (!projected.ships_captured || Same_Ships(&projected, &store->snapshots.back().map)) &&
            (!projected.players_captured || (Same_Players(&projected, &store->snapshots.back().map) && player_ids == store->snapshots.back().player_ids)) &&
            (!projected.colonies_captured || Same_Colonies(&projected, &store->snapshots.back().map)) &&
            (!projected.planets_captured || Same_Planets(&projected, &store->snapshots.back().map)) &&
            (!projected.research_captured || Same_Research(&projected, &store->snapshots.back().map)) &&
            (!projected.leaders_captured || Same_Leaders(&projected, &store->snapshots.back().map)) && Same_Relations(&projected, &store->snapshots.back().map) &&
            Same_History(&projected, &store->snapshots.back().map) && Same_Messages(&projected, &store->snapshots.back().map);
        if (projected.context_epoch != 0) store->context_active = true;
        if (equal && (projected.context_epoch == store->snapshots.back().map.context_epoch || (!projected.ships_captured && !projected.leader_presentation && !projected.relations_presentation))) return true;
        if (store->revision == std::numeric_limits<uint64_t>::max()) return false;
        s_map_snapshot snapshot;
        if (!store->new_id(&snapshot.id, store->id_context)) return false;
        snapshot.map = std::move(projected);
        snapshot.player_ids = std::move(player_ids);
        snapshot.planet_ids = std::move(planet_ids);
        snapshot.colony_ids = std::move(colony_ids);
        snapshot.ship_ids = std::move(ship_ids);
        const auto* previous_messages = Select_Snapshot(store, TOPIC_MESSAGES);
        if (snapshot.map.messages_captured && previous_messages != nullptr && Same_Messages(&snapshot.map, &previous_messages->map)) snapshot.message_ids = previous_messages->message_ids;
        if (!Extend(&snapshot.message_ids, snapshot.map.messages.size())) return false;
        const auto* previous_leaders = Select_Snapshot(store, TOPIC_LEADERS);
        if (snapshot.map.leaders_captured && previous_leaders != nullptr && Same_Leaders(&snapshot.map, &previous_leaders->map)) snapshot.leader_ids = previous_leaders->leader_ids;
        if (!Extend(&snapshot.leader_ids, snapshot.map.leaders.size())) return false;
        const auto* previous_research = Select_Snapshot(store, TOPIC_RESEARCH);
        if (snapshot.map.research_captured && previous_research != nullptr && Same_Research(&snapshot.map, &previous_research->map)) {
            snapshot.technology_ids = previous_research->technology_ids;
            snapshot.research_option_ids = previous_research->research_option_ids;
        }
        if (!Extend(&snapshot.technology_ids, snapshot.map.research.technologies.size()) ||
            !Extend(&snapshot.research_option_ids, snapshot.map.research.options.size())) return false;
        snapshot.revision = equal ? store->revision : ++store->revision;
        store->snapshots.push_back(std::move(snapshot));
        for (auto& subscription : store->subscriptions) subscription.revision = 0;
        if (store->snapshots.size() > 4) {
            const bool has_context = std::any_of(store->snapshots.begin(), store->snapshots.end(), [](const s_map_snapshot& item) { return item.map.context_epoch != 0; });
            auto ambient = has_context ? std::find_if(store->snapshots.rbegin(), store->snapshots.rend(), [](const s_map_snapshot& item) { return item.map.ships_captured && item.map.context_epoch == 0; }) : store->snapshots.rend();
            if (has_context && ambient == store->snapshots.rend()) ambient = std::find_if(store->snapshots.rbegin(), store->snapshots.rend(), [](const s_map_snapshot& item) { return !item.map.leaders_only && item.map.context_epoch == 0; });
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
        s_json_builder json;
        Initialize_Json(&json);
        auto* result = Query(&json, store, request, yyjson_doc_get_root(params), &error, &captured_context);
        yyjson_doc_free(params);
        const std::string response = error != 0 ? Error_Response(request, error) : Result_Response(&json, request, result);
        const bool failed = json.failed;
        Shutdown_Json(&json);
        if (context_epoch != nullptr && error == 0 && !failed && response.size() <= 1048576) *context_epoch = captured_context;
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
