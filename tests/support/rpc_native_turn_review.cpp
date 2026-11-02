#include "pch.h"
#include "rpc_native_turn_review.h"
#include "rpc/rpc_game.h"
#include "rpc/game_rpc.h"
#include "rpc/players_rpc.h"
#include "rpc/turn_review_rpc.h"
#include "game/game_events.h"

#include <chrono>
#include <cstdlib>
#include <mutex>
#include <thread>

namespace test_native_turn_review {
    namespace {
        s_player players[MAX_PLAYERS];
        s_ship_data ships[2];
        s_leader_data leaders[MAX_LEADERS];
        s_star_data stars[3];
        s_planet_data planets[2];
        s_colony colonies[1];
        std::vector<char> strings;
        rpc::s_game_store* store;
        rpc::s_map_observation* map;
        uint64_t subscription;
        bool active = false;
        bool output_pending = false;

        void Observe(game_events::e_event event, void*) {
            if (active && event == game_events::MESSAGES_REPLACING && game_events::Message_Recipient() == map->viewer) rpc::Expire_Game_Messages(store);
        }

        bool Read() {
            rpc::s_map_observation fresh;
            if (!(map->ships_captured ? game_rpc::Read_Inspection_Observation(&fresh) : game_rpc::Read_Map_Observation(&fresh))) return false;
            if (!map->ships_captured) {
                fresh.ships_captured = fresh.inspection_captured = fresh.owned_ambient = false;
                fresh.ships.clear();
                fresh.fleets.clear();
                fresh.designs.clear();
                fresh.owned_authority.clear();
            }
            *map = std::move(fresh);
            return rpc::Capture_Map(store, map);
        }
    } // namespace

    void Seed() {
        output_pending = false;
        memset(players, 0, sizeof(players));
        memset(ships, 0, sizeof(ships));
        memset(leaders, 0, sizeof(leaders));
        memset(stars, 0, sizeof(stars));
        memset(planets, 0, sizeof(planets));
        memset(colonies, 0, sizeof(colonies));
        MOX::player = players;
        MOX::ship = ships;
        MOX::leaders = leaders;
        MOX::star = stars;
        MOX::planet = planets;
        MOX::colony = colonies;
        MOX::PLAYER_NUM = 0;
        MOX::NUM_PLAYERS = 4;
        MOX::NUM_STARS = 3;
        MOX::NUM_SHIPS = 2;
        MOX::NUM_COLONIES = 1;
        MOX::NUM_NEBULAS = 0;
        MOX::planet_count = 2;
        MOX::stardate = 35000;
        RUSS::all_ai_game = 0;
        std::fill(std::begin(MOX::first_field_in_group), std::end(MOX::first_field_in_group), 0);
        for (auto& leader : leaders) {
            leader.player_index = -1;
            leader.status = -1;
        }
        for (int16_t player = 0; player < 4; ++player) {
            players[player].objectives = PLAYER_OBJECTIVE_HUMAN;
            players[player].color = (uint8_t)player;
            players[player].officer_for_hire = -1;
            players[player].history_btns = 15;
            snprintf(players[player].name, sizeof(players[player].name), "Empire %i", player);
            snprintf(players[player].race_name, sizeof(players[player].race_name), "Race %i", player);
            players[player].total_pop = 10 + player;
            players[player].total_known_tech_cost = 40 + player;
        }
        players[0].contact[1] = 1;
        players[3].eliminated = 1;
        for (auto& star : stars) {
            for (int orbit = 0; orbit < 5; ++orbit) star.planet_index[orbit] = -1;
            star.wormhole_star_id = -1;
        }
        strlcpy(stars[0].name, "Home", sizeof(stars[0].name));
        stars[0].planet_index[0] = 0;
        stars[0].visited = 1;
        stars[1].planet_index[0] = 1;
        planets[0].planet_type = PLANET_TYPE_PLANET;
        planets[0].climate = PLANET_CLIMATE_TERRAN;
        planets[0].colony_index = 0;
        planets[1].star_index = 1;
        planets[1].planet_type = PLANET_TYPE_PLANET;
        planets[1].colony_index = -1;
        colonies[0].owner = 0;
        colonies[0].buildings[BUILDING_MARINE_BARRACKS] = 1;
        TECHDATA::buildings[BUILDING_MARINE_BARRACKS].cost = 30;
        TECHDATA::buildings[BUILDING_MARINE_BARRACKS].name = (char*)"Native building";
        TECHDATA::technology_applications[1].name = (char*)"Native technology A";
        TECHDATA::technology_applications[2].name = (char*)"Native technology B";
        TECHDATA::hull_data[0].name = (char*)"Native hull";
        MOX::roman_literals[1] = (char*)"I";
        MOX::h_strings.assign({'P', 'r', 'i', 'm', 'e', '\0'});
        MOX::h_string_ix[0xa0] = 0;
        ESTRINGS::strings[45] = (char*)"%s %s";
        ESTRINGS::strings[0x1bd] = (char*)"Refit";
        strlcpy(ships[0].d.name, "Native ship", sizeof(ships[0].d.name));
        strlcpy(leaders[0].name, "Native leader", sizeof(leaders[0].name));
        // clang-format off
        const char* text[] = {
            "Native", "%s and %s allied", "%s and %s declared war", "Balance %i, change %i",
            "Lost %s %s, cost %i at %s", "Scrapped %s %s, cost %i at %s for %i", "Scrapped %s %s, cost %i in transit",
            "colony", "outpost", "Lost %s %s: %s", "; leader %s",
            "Scrapped %s at %s for %i", "%s built %s", "%s built ship %s", "%s refitted %s",
            "%s built spy", "%s built farmer", "%s built worker", "%s built scientist", "%s built freighters",
            "; next %s", "; automatic %s", "%s removed at %s", "%s assimilated at %s",
            "%s lost %u food", "%s lost %u industry", "%s lost %u food and %u industry",
            "%u settler at %s", "%u settlers at %s", "; blockaded", "; %u excess settler", "; %u excess settlers",
            "Plague at %s", "Boom at %s", "Comet at %s", "%u rebels at %s won, lost %u", "%u rebels at %s lost",
            "%u rebel of %s at %s won, lost %u", "%u rebels of %s at %s won, lost %u",
            "%u rebel of %s at %s lost", "%u rebels of %s at %s lost", "%s has no room", "%s android removed %s",
            "Flux continues", "Beast hungry", "Beast ate %s at %s", "Beast ate %s ship",
            "Lost spy at %s", "Lost defending spy", "Lost %i defending spies", "Artemis at %s", "Artemis leader at %s",
            "%u retreating ship at %s", "%u retreating ships at %s", "defensive spy", "spy against %s",
            "Leader %s lost to %s, %s", "Assassinated %s", "Anomaly at %s",
            "%s and %s at war", "%s and %s at peace", "%s and %s allied", "%s and %s nonaggression", "%s and %s peace ended",
            "Eel appeared at %s", "Eel vanished at %s", "Found %s at %s", "April notice", "Dismissed %s",
            "reason A", "reason B", "reason C", "; %u leaders", "; rescued %s", "; lost %s", "; missing %s",
            "reason A", "reason B", "reason C", "reason D"
        };
        // clang-format on
        strings.clear();
        for (const char* value : text) strings.insert(strings.end(), value, value + strlen(value) + 1);
        MSG_::Load_Msg_Strings(strings.data());
        strlcpy(MSG_::artifacts_and, "and", sizeof(MSG_::artifacts_and));
        BILL::Bill_Init();
        INFO::history_graph_smoothing = false;
        REPORT::sort_serious_turn_summary_first = false;
        MSG_::Allocate_Msg_Slots();
        for (int sample = 0; sample < 3; ++sample) {
            players[0].total_pop += 10;
            BILL::Record_History();
            ++MOX::stardate;
        }
    }

    void Add_Variants() {
        for (int16_t subtype = 0; subtype < MSG_COLONY_SUBTYPE_COUNT; ++subtype) {
            s_MSG message = {};
            message.msg_type = MSG_TYPE_COLONY;
            message.u.colony.subtype = (MSG_COLONY_SUBTYPE)subtype;
            if (subtype == MSG_COLONY_SUBTYPE_BUILDING_SCRAPPED) message.u.colony.c.scrap.building_id = BUILDING_MARINE_BARRACKS;
            if (subtype == MSG_COLONY_SUBTYPE_PRODUCTION_COMPLETED) {
                message.u.colony.c.build.build_id = BUILDING_MARINE_BARRACKS;
                message.u.colony.c.build.product_id = COLONY_PRODUCTION_NONE;
            }
            if (subtype == MSG_COLONY_SUBTYPE_SETTLERS_ARRIVED) message.u.colony.c.settlers.settlers = 1;
            if (subtype == MSG_COLONY_SUBTYPE_REBELLION) message.u.colony.c.rebels.rebels = 1;
            MSG_::Add_Msg(message, 0);
        }
        for (int16_t type = 1; type <= MSG_TYPE_SCRAP_LEADER; ++type) {
            if (type == MSG_TYPE_UNKNOWN_0x3) continue;
            s_MSG message = {};
            message.msg_type = (int8_t)type;
            if (type == MSG_TYPE_DECLARE_ALLIANCE || type == MSG_TYPE_DECLARE_WAR) message.u.players.player_b_idx = 1;
            if (type == MSG_TYPE_ARTIFACTS) {
                message.u.artifacts.tech1 = 1;
                message.u.artifacts.tech2 = 2;
            }
            if (type == MSG_TYPE_FAILED_RETREAT) message.u.retreat.ships_lost = 2;
            if (type == MSG_TYPE_DIPLOMACY) message.u.diplomacy.plr_b = 1;
            MSG_::Add_Msg(message, 0);
        }
    }

    void Initialize(rpc::s_game_store* target, rpc::s_map_observation* ambient) {
        store = target;
        map = ambient;
        subscription = game_events::Subscribe(Observe, nullptr);
    }

    bool Command(const std::string& command) {
        if (!command.starts_with("review-")) return false;
        if (command == "review-native") {
            active = true;
            Seed();
            Add_Variants();
        } else if (!active)
            return false;
        else if (command == "review-hidden") {
            players[2].population_history[0] = (int8_t)250;
            players[2].tech_history[1] = (int8_t)250;
            s_MSG message = {};
            message.msg_type = MSG_TYPE_APRIL1;
            MSG_::Add_Msg(message, 2);
        } else if (command == "review-message") {
            if (output_pending) {
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
                while (std::chrono::steady_clock::now() < deadline) {
                    bool drained;
                    {
                        std::lock_guard lock(store->bridge->mutex);
                        drained = store->bridge->ready.empty();
                    }
                    if (drained) break;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                {
                    std::lock_guard lock(store->bridge->mutex);
                    if (!store->bridge->ready.empty() || store->bridge->deliveries.empty()) std::abort();
                }
                output_pending = false;
            }
            s_MSG message = {};
            message.msg_type = MSG_TYPE_FINANCIAL;
            message.priority = 1;
            message.u.financial.a = 99;
            MSG_::Add_Msg(message, 0);
        } else if (command == "review-sort")
            REPORT::sort_serious_turn_summary_first = true;
        else if (command == "review-smooth")
            INFO::history_graph_smoothing = true;
        else if (command == "review-large") {
            for (int sample = 0; sample < 350; ++sample) {
                BILL::Record_History();
                ++MOX::stardate;
            }
        } else if (command == "review-output") {
            std::vector<rpc::s_client_identity> clients;
            rpc::Get_Authorized_Clients(store->bridge, &clients);
            if (clients.size() != 1) return false;
            rpc::s_request request;
            request.id_json = "99";
            request.method = "game.getState";
            request.params_json = "{\"include\":[\"history\",\"messages\"]}";
            for (int index = 0; index < 4; ++index) {
                if (rpc::Submit_Work(store->bridge, clients[0].client, clients[0].epoch, &request, UINT64_MAX) != 0) return false;
                rpc::s_work work;
                if (!rpc::Take_Work(store->bridge, 0, &work)) return false;
                uint64_t lease = 0;
                auto response = rpc::Game_Query(store, &request, &lease);
                // Valid JSON whitespace forces backpressure without changing the native result.
                if (response.size() < 900000) response.resize(900000, ' ');
                if (lease == 0 || !rpc::Complete_Work(store->bridge, &work, &response, lease)) return false;
            }
            output_pending = true;
            return true;
        } else if (command == "review-advance") {
            rpc::Pause_Game_Queries(store);
            MSG_::Allocate_Msg_Slots();
            BILL::Record_History();
            ++MOX::stardate;
            rpc::Resume_Game_Queries(store);
        } else if (command == "review-map-only")
            map->ships_captured = false;
        else if (command == "review-ships")
            map->ships_captured = true;
        else if (command == "review-contact")
            players[0].contact[2] = 1;
        else if (command == "review-lost")
            players[0].contact[1] = 0;
        else if (command == "review-eliminated")
            players[1].eliminated = 1;
        else if (command == "review-switch") {
            MOX::PLAYER_NUM = 2;
            map->research = {};
            map->leaders.clear();
            map->colonies.clear();
        } else if (command == "review-reset") {
            rpc::Begin_Game_Reset(store);
            rpc::Commit_Game_Reset(store);
        } else
            return false;
        return Read();
    }

    void Shutdown() {
        game_events::Unsubscribe(subscription);
        active = false;
        store = nullptr;
        map = nullptr;
    }
} // namespace test_native_turn_review
