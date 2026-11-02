#include "pch.h"
#include "rpc_native_colonies.h"
#include "rpc_native_turn_review.h"
#include "rpc/game_rpc.h"
#include "rpc/rpc_game.h"
#include "game/colony_read.h"
#include "game/game_events.h"
#include "game/build_queue.h"

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
        char long_label[450];

        bool Read() {
            if (game_events::Colony_Edits_Active() || store->busy_depth != 0) return true;
            rpc::s_map_observation fresh;
            if (!(ships_captured ? game_rpc::Read_Inspection_Observation(&fresh) : game_rpc::Read_Map_Observation(&fresh))) return false;
            // No ship-derived DTOs from a previous fixture/world are carried forward.
            fresh.ships_captured = ships_captured;
            if (!ships_captured) {
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
            if (event == game_events::MESSAGES_REPLACING && game_events::Message_Recipient() == store->viewer) rpc::Expire_Game_Messages(store);
        }

        void Seed() {
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
        if (!command.starts_with("colony-")) return false;
        if (command == "colony-native") {
            active = true;
            ships_captured = false;
            Seed();
        } else if (!active)
            return false;
        else if (command == "colony-hidden") {
            colonies[3].n_pops = 255;
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
            MOX::NUM_COLONIES = 10;
            memset(long_label, 'X', sizeof(long_label) - 1);
            ESTRINGS::strings[76] = long_label;
            for (int i = 0; i < 15; ++i) {
                colonies[i].owner = 0;
                colonies[i].outpost_flag = 0;
                planets[i].colony_index = i < MOX::NUM_COLONIES ? i : -1;
                colonies[i].n_pops = 42;
                for (int pop = 3; pop < 42; ++pop) colonies[i].pop[pop] = colonies[i].pop[0];
            }
        } else if (command == "colony-output") {
            std::vector<rpc::s_client_identity> clients;
            rpc::Get_Authorized_Clients(store->bridge, &clients);
            if (clients.size() != 1) return false;
            rpc::s_request request;
            request.id_json = "99";
            request.method = "game.getState";
            request.params_json = "{\"include\":[\"colonies\"]}";
            for (int i = 0; i < 12; ++i) {
                if (rpc::Submit_Work(store->bridge, clients[0].client, clients[0].epoch, &request, UINT64_MAX) != 0) return false;
                rpc::s_work work;
                if (!rpc::Take_Work(store->bridge, 0, &work)) return false;
                uint64_t lease = 0;
                const auto response = rpc::Game_Query(store, &request, &lease);
                if (lease == 0 || !rpc::Complete_Work(store->bridge, &work, &response, lease)) return false;
            }
            return true;
        } else
            return false;
        if (!Read()) std::abort();
        return true;
    }

    void Shutdown() {
        game_events::Unsubscribe(subscription);
        active = false;
    }
} // namespace test_native_colonies
