#include "pch.h"
#include "rpc_native_colonies.h"
#include "rpc_native_turn_review.h"
#include "rpc_native_ships.h"
#include "rpc/game_rpc.h"
#include "rpc/rpc_game.h"
#include "game/colony_read.h"
#include "game/game_events.h"
#include "game/build_queue.h"

#include <cstring>
#include <cstdlib>
#include <chrono>
#include <iterator>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace test_native_colonies {
    namespace {
        s_colony colonies[15];
        s_planet_data planets[15];
        s_star_data stars[3];
        rpc::s_game_store* store;
        rpc::s_map_observation* map;
        uint64_t subscription;
        bool active = false;
        bool ships_captured = false;
        bool strategic = false;
        std::unique_ptr<game_events::s_settlement_view_scope> settlement_owner;
        char long_label[450];
        bool output_pending = false;

        void Wait_For_Output() {
            if (!output_pending) return;
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

        bool Read() {
            if (game_events::Colony_Edits_Active() || store->busy_depth != 0) return true;
            rpc::s_map_observation fresh;
            if (!(strategic ? game_rpc::Read_Refreshed_Ship_Observation(&fresh) : (ships_captured ? game_rpc::Read_Inspection_Observation(&fresh) : game_rpc::Read_Map_Observation(&fresh)))) return false;
            // No ship-derived DTOs from a previous fixture/world are carried forward.
            fresh.ships_captured = strategic || ships_captured;
            if (!strategic && !ships_captured) {
                fresh.ships.clear();
                fresh.inspection_captured = false;
                fresh.owned_ambient = false;
                fresh.fleets.clear();
                fresh.designs.clear();
                fresh.owned_authority.clear();
            }
            *map = std::move(fresh);
            return rpc::Capture_Map(store, map);
        }

        void Observe(game_events::e_event event, void*) {
            if (!active) return;
            if (event == game_events::COLONY_REPLACING && game_events::Replaced_Colony_Owner() == store->viewer) rpc::Expire_Game_Colonies(store);
            if (event == game_events::COLONY_REPLACING && strategic) {
                const int16_t planet = game_events::Replaced_Colony_Planet();
                if (planet >= 0 && planet < MOX::planet_count)
                    rpc::Expire_Game_Settlement(store, MOX::planet[planet].star_index, MOX::planet[planet].orbit + 1);
            }
            if (event == game_events::STRATEGIC_LABELS_REPLACING) rpc::Expire_Game_Colonies(store);
            if (event == game_events::SETTLEMENT_VIEW_CLEARED && strategic) game_rpc::Revoke_Settlement_View(store);
            if (event == game_events::SETTLEMENT_VIEW_READY && strategic) {
                rpc::s_map_observation fresh;
                if (!game_rpc::Read_Refreshed_Ship_Observation(&fresh) || !game_rpc::Capture_Settlement_View(store, &fresh, game_events::Published_Settlement_View())) rpc::Revoke_Game_Context(store);
            }
            if (event == game_events::MESSAGES_REPLACING && game_events::Message_Recipient() == store->viewer) rpc::Expire_Game_Messages(store);
        }

        void Seed() {
            output_pending = false;
            test_native_turn_review::Seed();
            EVENTS::Init_Events();
            memset(colonies, 0, sizeof(colonies));
            memset(planets, 0, sizeof(planets));
            memset(stars, 0, sizeof(stars));
            MOX::colony = colonies;
            MOX::planet = planets;
            MOX::star = stars;
            MOX::NUM_COLONIES = 5;
            MOX::planet_count = 15;
            for (auto& star : stars) {
                for (int orbit = 0; orbit < 5; ++orbit) star.planet_index[orbit] = -1;
                star.wormhole_star_id = -1;
                star.visited = 1;
                for (int player = 0; player < MAX_PLAYERS; ++player) star.officer_index[player] = -1;
                strlcpy(star.name, "Operations", sizeof(star.name));
            }
            for (int i = 0; i < 15; ++i) {
                planets[i].planet_type = PLANET_TYPE_PLANET;
                planets[i].climate = PLANET_CLIMATE_TERRAN;
                planets[i].star_index = i / 5;
                planets[i].orbit = i % 5;
                planets[i].colony_index = i < 5 ? i : -1;
                stars[i / 5].planet_index[i % 5] = i;
                colonies[i].planet = i;
                colonies[i].occupation_policy = 4;
                colonies[i].specialty = -1;
                colonies[i].n_pops = 3;
                colonies[i].max_farms = 42;
                colonies[i].production[ECON_FOOD] = 2;
                colonies[i].production[ECON_INDUSTRY] = 14;
                colonies[i].production[ECON_RESEARCH] = 7 + i;
                colonies[i].production[ECON_BC] = 8;
                colonies[i].maintenance[ECON_FOOD] = 3;
                colonies[i].maintenance[ECON_INDUSTRY] = 2;
                colonies[i].pollution = 4;
                colonies[i].morale = -5;
                colonies[i].production_spent = 9;
                colonies[i].military[0] = 2;
                colonies[i].military[1] = 1;
                colonies[i].pop_roundoff[0] = 250;
                colonies[i].pop_growth[0] = -125;
                for (int pop = 0; pop < 3; ++pop) {
                    POP::Set_Race(&colonies[i].pop[pop], pop == 2 ? 8 : pop);
                    POP::Set_Prof(&colonies[i].pop[pop], (POP::e_prof)pop);
                    POP::Set_Assigned(&colonies[i].pop[pop], true);
                }
                colonies[i].buildings[BUILDING_MARINE_BARRACKS] = 1;
                colonies[i].buildings[BUILDING_STAR_BASE] = 1;
                colonies[i].buildings[BUILDING_STAR_FORTRESS] = 1;
                for (int slot = 0; slot < 7; ++slot) colonies[i].producing[slot] = COLONY_PRODUCTION_NONE;
                colonies[i].producing[0] = BUILDING_MARINE_BARRACKS;
                colonies[i].producing[1] = COLONY_PRODUCTION_HOUSING;
                colonies[i].producing[2] = COLONY_PRODUCTION_REPEAT;
                colonies[i].producing[3] = COLBLDG::Colony_Production_From_Ship_Index(0);
                colonies[i].producing[4] = COLBLDG::Colony_Production_From_Ship_Design_Index(0);
            }
            colonies[3].owner = 2;
            colonies[4].outpost_flag = 1;
            colonies[1].auto_building = BUILD_QUEUE::Encode_Autobuild_Preset(3);
            colonies[2].producing[0] = COLONY_PRODUCTION_TRADE_GOODS;
            colonies[1].specialty = 1;
            colonies[2].occupation_policy = 0;
            POP::Set_Race(&colonies[2].pop[1], 2);
            POP::Set_Conquered(&colonies[2].pop[1], true);
            BUILD_QUEUE::autobuild_settings.enabled = 1;
            TECHDATA::buildings[BUILDING_STAR_BASE].name = (char*)"Star Base";
            TECHDATA::buildings[BUILDING_STAR_BASE].group = 7;
            TECHDATA::buildings[BUILDING_STAR_FORTRESS].name = (char*)"Star Fortress";
            TECHDATA::buildings[BUILDING_STAR_FORTRESS].group = 7;
            TECHDATA::units[1].name = (char*)"Marines";
            TECHDATA::units[2].name = (char*)"Armor";
            ESTRINGS::strings[ESTR_POP_D03D_K_DK] = (char*)"Pop %d.%03d M (%+d k/turn)";
            ESTRINGS::strings[ESTR_SCOLONY_OF_S] = (char*)"%sColony of %s";
            ESTRINGS::strings[ESTR_ANNIHILATING_S] = (char*)"Annihilating %s";
            ESTRINGS::strings[ESTR_AGRICULTURAL] = (char*)"Agricultural ";
            ESTRINGS::strings[ESTR_INDUSTRIAL] = (char*)"Industrial ";
            ESTRINGS::strings[ESTR_RESEARCH] = (char*)"Research ";
            MOX::roman_literals[2] = (char*)"II";
            MOX::roman_literals[3] = (char*)"III";
            MOX::roman_literals[4] = (char*)"IV";
            MOX::roman_literals[5] = (char*)"V";
            ESTRINGS::strings[12] = (char*)"";
            ESTRINGS::strings[76] = (char*)"%s%s %s %s";
            ESTRINGS::strings[ESTR_CONQUERED] = (char*)"Conquered ";
            ESTRINGS::strings[ESTR_FARMER] = (char*)"Farmer";
            ESTRINGS::strings[ESTR_WORKER] = (char*)"Worker";
            ESTRINGS::strings[ESTR_SCIENTIST] = (char*)"Scientist";
            ESTRINGS::strings[0xb3] = (char*)"Android";
            ESTRINGS::strings[0x255] = (char*)"Native";
            ESTRINGS::strings[0x142] = (char*)"Housing";
            ESTRINGS::strings[0x21d] = (char*)"Trade Goods";
            ESTRINGS::strings[0x88] = (char*)"Repeat";
            ESTRINGS::strings[0xbd] = (char*)"Auto Build";
            ESTRINGS::strings[0xce] = (char*)"Population Boom";
            ESTRINGS::strings[ESTR_POP_BOOM] = (char*)"Population Boom";
            ESTRINGS::strings[ESTR_PLAGUE] = (char*)"Plague";
            ESTRINGS::strings[ESTR_BLOCKADED1] = (char*)"Blockaded";
            ESTRINGS::strings[0x1f8] = (char*)"Tanks";
            ESTRINGS::strings[0xc3] = (char*)"Battloids";
            ESTRINGS::strings[0x14d] = (char*)"Marines";
            ESTRINGS::strings[0x1ac] = (char*)"Powered Infantry";
            ESTRINGS::strings[0x174] = (char*)"Militia";
            ESTRINGS::strings[0x12d] = (char*)"Freighters";
            ESTRINGS::strings[0x1ec] = (char*)"Spy";
            ESTRINGS::strings[0xb0] = (char*)"Android Farmer";
            ESTRINGS::strings[0xb2] = (char*)"Android Worker";
            ESTRINGS::strings[0xb1] = (char*)"Android Scientist";
            ESTRINGS::strings[0xe6] = (char*)"Colony Ship";
            ESTRINGS::strings[0x195] = (char*)"Outpost Ship";
            ESTRINGS::strings[0x21e] = (char*)"Transport Ship";
            ESTRINGS::strings[0x10f] = (char*)"Doom Star";
            ESTRINGS::strings[0xfa] = (char*)"Custom Design";
            MOX::ship[0].owner = 0;
            MOX::ship[0].status = 4;
            MOX::ship[0].d.cost = 55;
            MOX::ship[1].owner = -1;
            MOX::ship[1].status = 5;
            strlcpy(MOX::player[0].ship_designs[0].name, "Own design", sizeof(MOX::player[0].ship_designs[0].name));
            MOX::player[0].bc = 1000;
            MOX::player[0].settlers_freighted = 1;
            SETTLER::Set_Dest_Planet(&MOX::player[0].settlers[0].val, 1);
            EVENTS::event_data[EVENT_POPULATION_BOOM].status = 2;
            EVENTS::event_data[EVENT_POPULATION_BOOM].target_id = 1;
            test_native_turn_review::Add_Variants();
        }
    } // namespace

    void Seed_World() {
        Seed();
    }

    void Initialize(rpc::s_game_store* target, rpc::s_map_observation* observation) {
        store = target;
        map = observation;
        subscription = game_events::Subscribe(Observe, nullptr);
    }

    bool Command(const std::string& command) {
        if (command.starts_with("settlement-")) {
            if (command == "settlement-native") {
                active = strategic = true;
                settlement_owner.reset();
                if (!test_native_ships::Command("fleet-native")) return false;
                for (int i = 0; i < PLANET_SIZE_COUNT; ++i) MOX::planet_size_string[i] = (char*)"Medium";
                for (int i = 0; i < PLANET_CLIMATE_COUNT; ++i) MOX::planet_climate_string[i] = (char*)"Terran";
                MOX::NUM_COLONIES = 9;
                for (int i = 5; i < 9; ++i) {
                    colonies[i].owner = 1;
                    colonies[i].climate = PLANET_CLIMATE_TERRAN;
                    planets[i].colony_index = i;
                }
                colonies[6].outpost_flag = 1;
                colonies[7].owner = 2;
                colonies[8].owner = 2;
                MOX::star[1].has_colony = 0;
                for (int ship : {2, 3}) MOX::ship[ship].location = 0;
                MOX::star[2].visited = 0;
                MOX::star[1].planet_index[3] = -1;
                MOX::star[2].planet_index[0] = 8;
                planets[8].star_index = 2;
                planets[8].orbit = 0;
                colonies[5].n_pops = 7;
                colonies[5].buildings[BUILDING_MISSILE_BASE] = 1;
                TECHDATA::buildings[BUILDING_MISSILE_BASE].name = (char*)"Missile Base";
                const char* population_format = "Population %u / %d";
                MOX::h_string_ix[0x181] = MOX::h_strings.size();
                MOX::h_strings.insert(MOX::h_strings.end(), population_format, population_format + strlen(population_format) + 1);
                if (!test_native_ships::Command("fleet-refresh")) return false;
            } else if (!strategic)
                return false;
            else if (command == "settlement-hidden") {
                for (int i : {3, 5, 6, 7, 8}) {
                    colonies[i].military[0] = -1;
                    colonies[i].military[1] = -1;
                    colonies[i].pop[0] ^= 0x100;
                    colonies[i].producing[0] = -32768;
                    colonies[i].production_spent++;
                    colonies[i].production[ECON_RESEARCH]++;
                    colonies[i].buildings[BUILDING_RESEARCH_LAB] ^= 1;
                }
                MOX::player[1].bc = -999999;
            } else if (command == "settlement-change")
                colonies[5].n_pops++;
            else if (command == "settlement-present" || command == "settlement-other" || command == "settlement-suppress") {
                if (!settlement_owner) settlement_owner = std::make_unique<game_events::s_settlement_view_scope>(MOX::PLAYER_NUM);
                settlement_owner->Initialized();
                game_events::s_settlement_frame frame;
                if (command == "settlement-suppress") return true;
                const int16_t planet = command == "settlement-other" ? 7 : 5;
                char text[80];
                colony_read::Strategic_Population_Text(&colonies[planet], COLCALC::Planet_Max_Population_For_Player(&planets[planet], MOX::PLAYER_NUM), text, sizeof(text));
                game_events::Observe_Settlement_Text(planet, "system-detail", text, true);
                return true;
            } else if (command == "settlement-unclassified") {
                if (!settlement_owner) settlement_owner = std::make_unique<game_events::s_settlement_view_scope>(MOX::PLAYER_NUM);
                settlement_owner->Initialized();
                ESTRINGS::strings[8] = (char*)"%d / %d";
                ESTRINGS::strings[0x29] = (char*)"%s - %s\n%s%s";
                ESTRINGS::strings[0x169] = (char*)"Low G";
                ESTRINGS::strings[0x222] = (char*)"Ultra Poor";
                game_events::s_settlement_frame frame;
                for (int16_t planet = 5; planet < 8; ++planet) {
                    char text[500];
                    COLSYSDI::Planet_Summary_String(text, sizeof(text), planet, -1);
                    game_events::Observe_Settlement_Text(planet, "colony-system-summary", text, false, colony_read::Strategic_Owner_Disclosed(ESTRINGS::E_Strings(0x29), 1, sizeof(text)));
                }
                return true;
            } else if (command == "settlement-interrupt") {
                game_events::s_settlement_interruption_scope child;
                return true;
            } else if (command == "settlement-exit") {
                settlement_owner.reset();
                return true;
            } else if (command == "settlement-large") {
                memset(long_label, 0xb0, sizeof(long_label) - 1);
                ESTRINGS::strings[76] = long_label;
                TECHDATA::buildings[BUILDING_MARINE_BARRACKS].name = long_label;
                colonies[8].owner = -1;
                MOX::NUM_COLONIES = 15;
                for (int i = 9; i < 15; ++i) {
                    const int16_t planet = i;
                    colonies[i] = colonies[0];
                    colonies[i].planet = planet;
                    planets[planet].colony_index = i;
                    MOX::star[planets[planet].star_index].planet_index[planets[planet].orbit] = planet;
                }
                for (int i : {0, 1, 2, 9, 10, 11, 12, 13, 14}) {
                    colonies[i].n_pops = 42;
                    for (int pop = 3; pop < 42; ++pop) colonies[i].pop[pop] = colonies[i].pop[0];
                    for (int slot = 0; slot < 7; ++slot) colonies[i].producing[slot] = BUILDING_MARINE_BARRACKS;
                }
            } else if (command == "settlement-output") {
                return Command("colony-output");
            } else if (command == "settlement-loss") {
                MOX::star[1].visited = 0;
            } else if (command == "settlement-known") {
                MOX::star[1].visited = 1;
            } else if (command == "settlement-reset") {
                return test_native_ships::Command("fleet-reset");
            } else if (command == "settlement-switch") {
                return test_native_ships::Command("fleet-switch");
            } else if (command == "settlement-destroy") {
                Wait_For_Output();
                game_events::Publish_Colony_Replacing(colonies[5].owner, 5);
                colonies[5].owner = -1;
                planets[5].colony_index = -1;
            } else if (command == "settlement-revoke-only") {
                game_events::Publish_Colony_Replacing(colonies[5].owner, 5);
                return true;
            } else if (command == "settlement-reuse") {
                INITGAME::Init_Colony(5, 5, 1);
                colonies[5].n_pops = 2;
            } else if (command == "settlement-hidden-replace") {
                game_events::Publish_Colony_Replacing(colonies[8].owner, 8);
                colonies[8].military[0]++;
            } else if (command == "settlement-hidden-init") {
                MOX::NUM_COLONIES = 10;
                colonies[9].owner = -1;
                colonies[9].planet = 0; // An inactive slot's stale location is not replacement authority.
                INITGAME::Init_Colony(9, 11, 2);
            } else if (command == "settlement-owner") {
                game_events::Publish_Colony_Replacing(colonies[5].owner, 5);
                colonies[5].owner = 0;
                colonies[5].military[0] = 2;
                colonies[5].military[1] = 1;
                colonies[5].producing[0] = COLONY_PRODUCTION_NONE;
            } else if (command == "settlement-unavailable") {
                rpc::Expire_Game_Colonies(store);
                return true;
            } else if (command == "settlement-empty") {
                rpc::Expire_Game_Colonies(store);
                for (int i = 0; i < MOX::NUM_COLONIES; ++i) {
                    colonies[i].owner = -1;
                    planets[i].colony_index = -1;
                }
                for (int i = 0; i < MOX::NUM_STARS; ++i) MOX::star[i].has_colony = 0;
            } else if (command != "settlement-refresh")
                return false;
            if (!Read()) std::abort();
            return true;
        }
        if (!command.starts_with("colony-")) return false;
        if (command == "colony-native") {
            strategic = false;
            active = true;
            ships_captured = false;
            Seed();
        } else if (!active)
            return false;
        else if (command == "colony-hidden") {
            colonies[3].military[0] = -1;
            colonies[3].producing[0] = -32768;
            MOX::player[2].bc = -999999;
        } else if (command == "colony-change") {
            colonies[1].production_spent += 5;
            colonies[1].production[ECON_RESEARCH] += 1;
        } else if (command == "colony-staged") {
            game_events::Set_Colony_Population_Staged(true);
            POP::Set_Assigned(&colonies[0].pop[0], false);
        } else if (command == "colony-restored") {
            POP::Set_Assigned(&colonies[0].pop[0], true);
            game_events::Set_Colony_Population_Staged(false);
        } else if (command == "colony-lose") {
            Wait_For_Output();
            game_events::Publish_Colony_Replacing(0);
            colonies[0].owner = 1;
        } else if (command == "colony-replace") {
            INITGAME::Init_Colony(0, 0, 0);
        } else if (command == "colony-unobserved-loss") {
            colonies[0].owner = 1;
        } else if (command == "colony-ships")
            ships_captured = true;
        else if (command == "colony-map-only")
            ships_captured = false;
        else if (command == "colony-ship")
            MOX::ship[0].status = 6;
        else if (command == "colony-bought")
            colonies[0].bought_outright = 21;
        else if (command == "colony-blockade")
            stars[0].blockaded = 1;
        else if (command == "colony-stalled")
            colonies[0].production[ECON_INDUSTRY] = 0;
        else if (command == "colony-militia") {
            colonies[0].n_pops = 8;
            for (int i = 3; i < 8; ++i) colonies[0].pop[i] = colonies[0].pop[0];
        } else if (command == "colony-powered") {
            MOX::player[0].tech_applications[TECH_APP_BATTLEOIDS] = TECH_RESEARCH_STATUS_RESEARCHED;
            MOX::player[0].tech_applications[TECH_APP_POWERED_ARMOR] = TECH_RESEARCH_STATUS_RESEARCHED;
        } else if (command == "colony-unification")
            MOX::player[0].traits[TRAIT_CURRENT_GOVERNMENT] = GOVERNMENT_UNIFICATION;
        else if (command == "colony-reset") {
            rpc::Begin_Game_Reset(store);
            rpc::Commit_Game_Reset(store);
        } else if (command == "colony-switch") {
            rpc::Revoke_Game_View(store);
            MOX::PLAYER_NUM = 1;
        } else if (command == "colony-advance") {
            rpc::Pause_Game_Queries(store);
            ++MOX::stardate;
            return true;
        } else if (command == "colony-commit")
            rpc::Resume_Game_Queries(store);
        else if (command.starts_with("colony-product-")) {
            const int16_t products[] = {COLONY_PRODUCTION_NONE, COLONY_PRODUCTION_HOUSING, COLONY_PRODUCTION_TRADE_GOODS, COLONY_PRODUCTION_FREIGHTERS, COLONY_PRODUCTION_SPY, COLONY_PRODUCTION_FARMER, COLONY_PRODUCTION_WORKER, COLONY_PRODUCTION_SCIENTIST, COLONY_PRODUCTION_COLONY_SHIP, COLONY_PRODUCTION_OUTPOST_SHIP, COLONY_PRODUCTION_TRANSPORT_SHIP, -100, -50, 49, -32768, COLONY_PRODUCTION_DOOM_STAR, COLONY_PRODUCTION_CUSTOM_DESIGN, COLONY_PRODUCTION_SEPARATOR};
            const int index = atoi(command.c_str() + strlen("colony-product-"));
            if (index < 0 || (size_t)index >= std::size(products)) return false;
            colonies[0].producing[0] = products[index];
        } else if (command == "colony-large") {
            MOX::NUM_COLONIES = 12;
            memset(long_label, 0xb0, sizeof(long_label) - 1);
            ESTRINGS::strings[76] = long_label;
            TECHDATA::buildings[BUILDING_MARINE_BARRACKS].name = long_label;
            for (int i = 0; i < 15; ++i) {
                colonies[i].owner = 0;
                colonies[i].outpost_flag = 0;
                planets[i].colony_index = i < MOX::NUM_COLONIES ? i : -1;
                colonies[i].n_pops = 42;
                for (int pop = 3; pop < 42; ++pop) colonies[i].pop[pop] = colonies[i].pop[0];
                for (int slot = 0; slot < 7; ++slot) colonies[i].producing[slot] = BUILDING_MARINE_BARRACKS;
            }
        } else if (command == "colony-output") {
            std::vector<rpc::s_client_identity> clients;
            rpc::Get_Authorized_Clients(store->bridge, &clients);
            if (clients.size() != 1) return false;
            rpc::s_request request;
            request.id_json = "99";
            request.method = "game.getState";
            request.params_json = "{\"include\":[\"colonies\"]}";
            for (int i = 0; i < 4; ++i) {
                if (rpc::Submit_Work(store->bridge, clients[0].client, clients[0].epoch, &request, UINT64_MAX) != 0) return false;
                rpc::s_work work;
                if (!rpc::Take_Work(store->bridge, 0, &work)) return false;
                uint64_t lease = 0;
                const auto response = rpc::Game_Query(store, &request, &lease);
                if (response.size() < 650000 || response.size() >= 1048576) std::abort();
                if (lease == 0 || !rpc::Complete_Work(store->bridge, &work, &response, lease)) return false;
            }
            output_pending = true;
            return true;
        } else
            return false;
        if (!Read()) std::abort();
        return true;
    }

    void Shutdown() {
        settlement_owner.reset();
        game_events::Unsubscribe(subscription);
        active = false;
    }
} // namespace test_native_colonies
