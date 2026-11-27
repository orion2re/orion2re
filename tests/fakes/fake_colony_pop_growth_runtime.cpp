#include "pch.h"
#include "../fixtures/ship_rows.h"
#include "../fixtures/star_roster.h"
#include "../fixtures/colony_roster.h"
#include "../fixtures/planet_roster.h"

#include "fake_colony_services.h"
#include "lua/lua_game.h"

namespace {
    test_colonies::Slots<2> colonies;

    auto& players = game::current_state.players;
    test_stars::Slots<2> stars;
    auto ships = []() {
        auto rows = test_ships::Rows(game::current_state);
        rows.resize(2);
        return rows;
    }();
    char race_name[] = "Fixture race";
    int32_t destroy_colony_reason;
    int16_t add_spy_result;
    int16_t deleted_product;
    bool population_boom;
    bool plague;
    int custom_autobuild_calls;
    int16_t legacy_autobuild_product;
    int calc_ship_level_calls;
    int init_ship_designs_calls;
    int16_t population_growth_adjustment;
    int population_growth_calls;
    orion2re::lua::game::s_population_growth_context population_growth_context;
}

namespace TEST_COLONY_POP_GROWTH {
    s_services services;

    void Reset_() {
        services = {};
        test_stars::Resize(0);
        ship_slots::Resize(game::current_state, 1);
        MOX::stardate = 35000;
        memset(REPORT::something_interesting_happened, 0, sizeof(REPORT::something_interesting_happened));
        for (char*& name : MOX::race_names) {
            name = race_name;
        }
        colonies.Reset();
        test_planets::Reset(2);
        players = ::player_roster::Make_Roster();
        stars.Reset();
        ships.assign(2, s_ship_data{});
        game::current_state = game::s_game_state{};
        game::current_state.player_count = 1;
        ships.resize(2);
        memset(TECHDATA::buildings, 0, sizeof(TECHDATA::buildings));
        memset(
            TECHDATA::technology_applications,
            0,
            sizeof(TECHDATA::technology_applications));
        test_planets::Record(0).size = PLANET_SIZE_HUGE;
        colonies[1].buildings[BUILDING_CAPITOL] = 1;
        stars[0].officer_id[0] = game::NO_LEADER;
        TECHDATA::buildings[BUILDING_CAPITOL].tech_app = TECH_APP_CAPITOL;
        destroy_colony_reason = -1;
        add_spy_result = 1;
        deleted_product = COLONY_PRODUCTION_NONE;
        population_boom = false;
        plague = false;
        custom_autobuild_calls = 0;
        legacy_autobuild_product = COLONY_PRODUCTION_NONE;
        calc_ship_level_calls = 0;
        init_ship_designs_calls = 0;
        population_growth_adjustment = 0;
        population_growth_calls = 0;
        population_growth_context = {};
        memset(
            COLCALC::government_bonuses,
            0,
            sizeof(COLCALC::government_bonuses));
        COLCALC::growth_rate_bonus = {
            0,
            25,
            50,
            100,
            -200,
        };
        COLCALC::growth_formula_factor = 2000;
        COLCALC::food_farmer_bonus = {2, 2, 4, 2, 4, 6};
        COLCALC::fungi_climate = COLCALC::FUNGI_CLIMATE_CLASSIC;
        AIDATA::planet_valuation_rules.food = {75, 38};
        AIDATA::planet_valuation_rules.special = {1280, 2560, 640, 1600};
        COLCALC::population_bonus.advanced_city_planning = 5;
        COLCALC::population_bonus.tolerant = 25;
        COLCALC::pop_bonus_biospheres = 2;
        const uint8_t subterranean_defaults[PLANET_SIZE_COUNT] = {
            2, 4, 6, 8, 10,
        };
        memcpy(
            COLCALC::pop_bonus_subterranean,
            subterranean_defaults,
            sizeof(subterranean_defaults));
        COLCALC::weather_control_climate = PLANET_CLIMATE_TOXIC;
        COLCALC::terraform_toxic_building = BUILDING_NO_BUILDING;
        COLCALC::aquatic_treats_tundra_as_swamp = 0;
        memset(COLCALC::ignore_mixed_race_penalty, 0,
               sizeof(COLCALC::ignore_mixed_race_penalty));
        COLCALC::housing_creates_no_farmers = 0;
        COLCALC::new_farmer_food_threshold = 0;
        COLCALC::housing_no_popagg = 0;
        COLCALC::housing_formula_factor = 40;
        COLCALC::planet_extinction = 0;
        COLCALC::zero_colony_lost = 0;
        COLCALC::feudal_affects_freighters = 0;
        COLCALC::freighters_cash_bonus = 0;
        COLCALC::multibuild = 0;
        COLCALC::multibuild_nonbuilding_products_only = 0;
        memset(
            DESIGN::rebuild_ship_designs_after_field,
            0,
            sizeof(DESIGN::rebuild_ship_designs_after_field));
        DESIGN::rebuild_ship_designs_after_field[TECH_FIELD_CHEMISTRY] = true;
        DESIGN::rebuild_ship_designs_after_field[TECH_FIELD_NUCLEAR_FISSION] = true;
        DESIGN::rebuild_ship_designs_after_field[TECH_FIELD_PHYSICS] = true;
        COLCALC::lander_ship_cost.colony_ship = 500;
        COLCALC::lander_ship_cost.outpost_ship = 100;
        COLCALC::lander_ship_cost.transport_ship = 100;
        COLCALC::cloning_center_growth_bonus = 100;
        COLCALC::misc_costs.freighters = 50;
        COLCALC::misc_costs.android_farmer = 50;
        COLCALC::misc_costs.android_worker = 50;
        COLCALC::misc_costs.android_scientist = 50;
        COLCALC::misc_costs.spy = 100;
        COLCALC::misc_costs.terraforming_step = 250;
        memset(MOX::food_per_farmer_table, 0, sizeof(MOX::food_per_farmer_table));
    }

    void Reset_Services_() {
        Reset_();
        static char command_title[] = "Command summary";
        static char industry_title[] = "Industry summary";
        static char research_title[] = "Research summary";
        COLCALC::command_summary_msg = command_title;
        COLCALC::industry_summary_msg = industry_title;
        COLCALC::research_summary_msg = research_title;
        memset(&MOX::settings, 0, sizeof(MOX::settings));
        game::current_state.player_count = 2;
        test_stars::Resize(1);
        ship_slots::Resize(game::current_state, 2);
        for (auto& leader : game::current_state.leaders) {
            leader.employer = game::NO_PLAYER;
            leader.status = -1;
            leader.location = -1;
        }
        for (size_t row = 0; row < ships.size(); ++row) {
            auto& ship = ships[row];
            ship.officer_id = game::NO_LEADER;
        }
        for (auto& player : players) {
            player.home_planet_id = game::NO_PLANET;
            player.last_attacker = game::NO_OWNER;
        }
        for (auto& star : stars) {
            for (auto& link : star.orbit_planet_ids) link = game::NO_PLANET;
            std::ranges::fill(star.officer_id, game::NO_LEADER);
            std::ranges::fill(star.surrender_to, game::star_surrender_order::None());
        }
        for (int i = 0; i < 2; ++i) {
            colonies[i].planet_id = SERIAL::Decode_Legacy_Planet_Reference(i);
            colonies[i].n_pops = 1;
            for (int slot = 0; slot < 7; ++slot) {
                colonies[i].producing[slot] = COLONY_PRODUCTION_NONE;
            }
            test_planets::Record(i).colony_id = game::id_colony{static_cast<uint32_t>(i)};
            test_planets::Record(i).star_id = game::id_star{static_cast<uint32_t>(i)};
            stars[i].orbit_planet_ids[0] = game::id_planet{static_cast<uint32_t>(i)};
            stars[i].has_colony = game::player_bitmap::From_Byte(1);
        }
    }

    int32_t Destroy_Colony_Reason_() {
        return destroy_colony_reason;
    }

    void Set_Add_Spy_Result_(int16_t result) {
        add_spy_result = result;
    }

    int16_t Deleted_Product_() {
        return deleted_product;
    }

    void Set_Population_Boom_(bool enabled) {
        population_boom = enabled;
    }

    void Set_Plague_(bool enabled) {
        plague = enabled;
    }

    int Custom_Autobuild_Calls_() {
        return custom_autobuild_calls;
    }

    void Set_Legacy_Autobuild_Product_(int16_t product) {
        legacy_autobuild_product = product;
    }

    int Calc_Ship_Level_Calls_() {
        return calc_ship_level_calls;
    }

    int Init_Ship_Designs_Calls_() {
        return init_ship_designs_calls;
    }

    void Set_Population_Growth_Adjustment_(int16_t adjustment) {
        population_growth_adjustment = adjustment;
    }

    int Population_Growth_Calls_() {
        return population_growth_calls;
    }

    const orion2re::lua::game::s_population_growth_context&
    Population_Growth_Context_() {
        return population_growth_context;
    }
} // namespace TEST_COLONY_POP_GROWTH

namespace orion2re::lua::game {
    std::int16_t Modify_Population_Growth(
        const s_population_growth_context* context, std::int64_t growth) {
        ++population_growth_calls;
        population_growth_context = *context;
        return static_cast<std::int16_t>(
            growth + population_growth_adjustment);
    }
} // namespace orion2re::lua::game

namespace MOX {
    [[maybe_unused]] const bool initial_player_count = [] { game::current_state.player_count = 1; return true; }();


    int32_t stardate = 35000;
    char* race_names[20];

    s_settings settings = {};
    s_crew_data crew_data[CREW_QUALITY_COUNT] = {};
    uint8_t food_per_farmer_table[PLANET_CLIMATE_COUNT] = {};
}

namespace REPORT {
    uint8_t something_interesting_happened[MAX_PLAYERS];
}

namespace TECHDATA {
    s_tech_building_data buildings[BUILDING_COUNT] = {};
    s_tech_app_data technology_applications[TECH_APP_COUNT] = {};
    s_tech_field_data technology_fields[TECH_FIELD_COUNT] = {};
}

namespace EVENTS {
    void __cdecl Event_Race_Eliminated(game::id_player player_key) {
        int player = legacy_world::Encode_Player_Slot(player_key);
        TEST_COLONY_POP_GROWTH::services.eliminated.push_back(player);
    }

    int16_t __cdecl Event_Check_Space_Anomaly_At_Star(int16_t star) {
        TEST_COLONY_POP_GROWTH::services.anomalies.push_back(star);
        return TEST_COLONY_POP_GROWTH::services.anomaly_result;
    }

    int16_t __cdecl Event_Check_Space_Anomaly(game::id_colony) {
        return 0;
    }

    int16_t __cdecl Event_Check_Colony_Researching(game::id_colony) {
        return 0;
    }

    int16_t __cdecl Event_Check_Population_Boom(game::id_colony) {
        return population_boom;
    }

    int16_t __cdecl Event_Check_Plague(game::id_colony) {
        return plague;
    }
}

namespace OFFICER {
    int16_t Owned_Officer_Level(const game::s_game_state&, game::id_leader id) { return Owned_Officer_Level(id); }
    void __cdecl Deassign_Officer(game::id_leader leader) {
        TEST_COLONY_POP_GROWTH::services.deassigned.push_back(leader_slots::Encode(leader));
        auto& officer = game::current_state.leaders.at(leader.id);
        officer.location = -1;
        officer.status = 0;
        officer.eta = 0;
    }

    void __cdecl Move_Officer_To_Pool(game::id_leader leader) {
        TEST_COLONY_POP_GROWTH::services.pooled.push_back(leader_slots::Encode(leader));
        auto& officer = game::current_state.leaders.at(leader.id);
        officer.location = -1;
        officer.status = 0;
        officer.eta = 0;
    }

    int16_t __cdecl Owned_Officer_Level(game::id_leader) {
        return 0;
    }
}

namespace special {
    uint32_t __cdecl isqrt(uint32_t) {
        return 100;
    }
}

namespace MSG_ {
    void __cdecl Add_Msg(s_MSG message, game::id_player player) {
        TEST_COLONY_POP_GROWTH::services.messages.push_back(message);
        TEST_COLONY_POP_GROWTH::services.message_players.push_back(legacy_world::Encode_Player_Slot(player));
    }
}

namespace RUSS {
    void __cdecl Assert_Settings() {
        ++TEST_COLONY_POP_GROWTH::services.settings_checks;
    }

    void __cdecl Mox_Update() {
        ++TEST_COLONY_POP_GROWTH::services.screen_updates;
    }

    void __cdecl Shuffle_Sint(int16_t*, int16_t) {
    }

    int16_t __cdecl Get_Weighted_Choice_Int(uint32_t*, int16_t) {
        return 1;
    }
}

namespace BOMB {
    void __cdecl Destroy_Colony(game::s_game_state&, game::id_colony, int32_t capturing_player) {
        destroy_colony_reason = capturing_player;
    }
}

namespace ERICNET {
    void __cdecl Add_Building(game::id_colony colony_idx, int16_t building_idx) {
        test_colonies::Record(colony_idx.id).buildings[building_idx] = 1;
    }

    void __cdecl Calc_Ship_Level(s_ship_data&) {
        ++calc_ship_level_calls;
    }

    void __cdecl Remove_Building(game::s_game_state&, game::id_colony, int16_t) {
    }
}

namespace AIBUILD {
    void __cdecl Player_Colony_Autobuild(AIDATA::s_state*, game::s_game_state*, int16_t, game::id_colony, s_colony* colony) {
        if (legacy_autobuild_product != COLONY_PRODUCTION_NONE) {
            colony->producing[0] = legacy_autobuild_product;
        }
    }
}

namespace BILL {
    int16_t __cdecl Get_Their_Spy_Number(game::id_player player, game::id_player target) {
        TEST_COLONY_POP_GROWTH::services.spy_reads.push_back({static_cast<int16_t>(player.id), static_cast<int16_t>(target.id)});
        return TEST_COLONY_POP_GROWTH::services.spies[player.id][target.id];
    }

    void __cdecl Set_Their_Spy_Number(game::id_player player, game::id_player target, int16_t count) {
        TEST_COLONY_POP_GROWTH::services.spy_writes.push_back({static_cast<int16_t>(player.id), static_cast<int16_t>(target.id), count});
        TEST_COLONY_POP_GROWTH::services.spies[player.id][target.id] = count;
    }

    int16_t __cdecl Add_Their_Spy(game::id_player, game::id_player) {
        return add_spy_result;
    }
}

namespace BUILD_QUEUE {
    bool Apply_Custom_Autobuild(int16_t) {
        ++custom_autobuild_calls;
        return false;
    }
}

namespace COLBLDG {
    void __cdecl Clear_Colony_Queue(game::s_game_state&, game::id_colony id) {
        const auto colony = static_cast<int16_t>(id.id);
        TEST_COLONY_POP_GROWTH::services.queues.push_back(colony);
        for (int slot = 0; slot < 7; ++slot) {
            test_colonies::Record(colony).producing[slot] = COLONY_PRODUCTION_NONE;
        }
    }

    game::id_ship Create_Ship(game::id_player player, int16_t design, game::id_colony colony) {
        TEST_COLONY_POP_GROWTH::services.creations.push_back({static_cast<int16_t>(player.id), design, static_cast<int16_t>(colony.id)});
        return game::NO_SHIP;
    }

    void __cdecl Delete_Prod_From_Colony_Queue(game::s_game_state&, game::id_colony, game::colony_product product) {
        deleted_product = production::Code_Or(product);
    }
}

namespace DESIGN {
    int16_t __cdecl Cost_Of_Design(game::id_player, s_ship_design*) {
        return 0;
    }

    void __cdecl Cost_Reduction_For_Govt_Type(
        game::id_player player, int32_t* cost) {
        Apply_Government_Cost_Reduction(game::current_state.players[player.id].traits[TRAIT_CURRENT_GOVERNMENT], cost);
    }

    void __cdecl Apply_Government_Cost_Reduction(int8_t government, int32_t* cost) {
        if ((uint8_t)government >= GOVERNMENT_COUNT) {
            return;
        }

        const s_ratio16& reduction =
            COLCALC::government_bonuses[government].cost_reduction;
        if (reduction.denominator == 0) {
            return;
        }

        *cost -= (int32_t)(
            (int64_t)*cost * reduction.numerator /
            reduction.denominator);
    }
}

namespace INITSHIP {
    void Update_Player_Ship_Designs(game::s_game_state&, game::id_player player, const game::s_tech_grant_context&) { Update_Player_Ship_Designs(player); }
    void __cdecl Update_Player_Ship_Designs(game::id_player player) {
        TEST_COLONY_POP_GROWTH::services.designs.push_back(legacy_world::Encode_Player_Slot(player));
    }

    void __cdecl Init_Ship_Designs(game::id_player) {
        ++init_ship_designs_calls;
    }
}

namespace COLCALC {
    int16_t Colony_Empire_Base_Food2_Produced(const game::s_game_state&, const s_colony* c, game::id_player p, int16_t g, s_colony_job_production* out) { return Colony_Empire_Base_Food2_Produced(c, p, g, out); }
    int16_t Colony_Empire_Base_Industry_Produced(const game::s_game_state&, const s_colony* c, game::id_player p, int16_t g, s_colony_job_production* out) { return Colony_Empire_Base_Industry_Produced(c, p, g, out); }
    int16_t Colony_Empire_Base_Research_Produced(const game::s_game_state&, const s_colony* c, game::id_player p, int16_t g, s_colony_job_production* out) { return Colony_Empire_Base_Research_Produced(c, p, g, out); }
    void Colony_Morale(const game::s_game_state&, s_colony* c, STRBUILD::s_builder* out) { Colony_Morale(c, out); }
    void Colony_Industry_Production(const game::s_game_state&, game::id_colony id, s_colony* c, STRBUILD::s_builder* out) { Colony_Industry_Production(id, c, out); }
    void Colony_Food_Production(const game::s_game_state&, game::id_colony id, s_colony* c, STRBUILD::s_builder* out) { Colony_Food_Production(id, c, out); }
    void Colony_Research_Production(const game::s_game_state&, game::id_colony id, s_colony* c, STRBUILD::s_builder* out) { Colony_Research_Production(id, c, out); }
    void Colony_BC_Production(const game::s_game_state&, game::id_colony id, s_colony* c, STRBUILD::s_builder* out) { Colony_BC_Production(id, c, out); }
    void Pass_Out_Imports(game::s_game_state&, game::id_player p, game::id_colony id) { Pass_Out_Imports(p, id); }
    void Update_Player_Stats(game::s_game_state&, s_player* player) { Update_Player_Stats(player); }
    void __cdecl Twiddle_Initial_Homeworld(game::id_colony colony_id, s_colony*) {
        TEST_COLONY_POP_GROWTH::services.homeworlds.push_back(static_cast<int16_t>(colony_id.id));
    }

    void __cdecl Compute_Player_Maintenance(s_player* player, STRBUILD::s_builder* builder) {
        TEST_COLONY_POP_GROWTH::services.maintenance_summary_player = (int16_t)(player - game::current_state.players.data());
        STRBUILD::Cat(builder, "Maintenance detail");
    }

    void __cdecl Player_Gets_Tech_App(s_player* player, int16_t tech_app_id) {
        player->tech_applications[tech_app_id] = TECH_RESEARCH_STATUS_RESEARCHED;
    }

    int16_t __cdecl Colony_Empire_Base_Food2_Produced(
        const s_colony*, game::id_player, int16_t, s_colony_job_production*) {
        return 8;
    }

    int16_t __cdecl Colony_Empire_Base_Industry_Produced(
        const s_colony*, game::id_player, int16_t, s_colony_job_production*) {
        return 6;
    }

    int16_t __cdecl Colony_Empire_Base_Research_Produced(
        const s_colony*, game::id_player, int16_t, s_colony_job_production*) {
        return 4;
    }

    void __cdecl Colony_Morale(s_colony*, STRBUILD::s_builder*) {
    }

    void __cdecl Colony_Industry_Production(game::id_colony colony_id, s_colony* colony, STRBUILD::s_builder* builder) {
        if (builder != nullptr) {
            TEST_COLONY_POP_GROWTH::services.industry_summary_colony = static_cast<int16_t>(colony_id.id);
            STRBUILD::Cat(builder, "Industry detail");
        }
    }

    void __cdecl Colony_Food_Production(game::id_colony, s_colony*, STRBUILD::s_builder*) {
    }

    void __cdecl Colony_Replicators(s_colony*) {
    }

    void __cdecl Colony_Research_Production(game::id_colony colony_id, s_colony* colony, STRBUILD::s_builder* builder) {
        if (builder != nullptr) {
            TEST_COLONY_POP_GROWTH::services.research_summary_colony = static_cast<int16_t>(colony_id.id);
            STRBUILD::Cat(builder, "Research detail");
        }
    }

    void __cdecl Colony_BC_Production(game::id_colony, s_colony*, STRBUILD::s_builder*) {
    }

    void __cdecl Pass_Out_Imports(game::id_player, game::id_colony) {
    }

    void __cdecl Update_Player_Stats(s_player* player) {
        TEST_COLONY_POP_GROWTH::services.stats.push_back((int16_t)(player - game::current_state.players.data()));
    }
}

namespace COLONY {
    bool __cdecl Colony_Pop_Is_Annihilation_Target(
        const s_colony*, const s_colony_pop&) {
        return false;
    }

    void __cdecl Fix_Colony_Queue(game::id_colony colony_idx) {
        s_colony& colony = test_colonies::Record(colony_idx.id);
        int destination = 0;
        for (int source = 0; source < 7; ++source) {
            if (colony.producing[source] != COLONY_PRODUCTION_NONE) {
                colony.producing[destination++] = colony.producing[source];
            }
        }
        while (destination < 7) {
            colony.producing[destination++] = COLONY_PRODUCTION_NONE;
        }
    }
}

namespace game_random {
    uint32_t __cdecl Random(uint32_t) {
        return 1;
    }
}

namespace GEO {
    std::span<const s_nebula> Session_Nebulas_For(const game::s_game_state&) { return {}; }
    int32_t __cdecl Parsecs_Between_Points(int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
        return (std::abs(x1 - x2) + std::abs(y1 - y2) + 29) / 30;
    }
    int32_t __cdecl Parsecs_Between_Stars(int16_t, int16_t) {
        return 0;
    }
}

namespace SHIPMOVE {
    void Update_Ship_ETAs(game::s_game_state&, game::id_player player_key) {
        int player = legacy_world::Encode_Player_Slot(player_key);
        TEST_COLONY_POP_GROWTH::services.fleet_etas.push_back(player);
    }

    void Update_Ship_ETAs(game::s_game_state& state, game::id_player player, std::span<const s_nebula>) {
        Update_Ship_ETAs(state, player);
    }

    void Update_All_Ship_Ranges(game::s_game_state&) {
        ++TEST_COLONY_POP_GROWTH::services.range_updates;
    }

    bool Star_In_Normal_Range_Of_Player(const game::s_game_state&, const s_star_data& star, game::id_player player) {
        TEST_COLONY_POP_GROWTH::services.normal_ranges.push_back({static_cast<int16_t>(star.id.id), static_cast<int16_t>(player.id)});
        return TEST_COLONY_POP_GROWTH::services.in_normal_range;
    }

    bool Star_In_Range_Of_Player(const game::s_game_state&, const s_star_data& star, game::id_player player, int16_t range) {
        TEST_COLONY_POP_GROWTH::services.ranges.push_back({static_cast<int16_t>(star.id.id), static_cast<int16_t>(player.id), range});
        return TEST_COLONY_POP_GROWTH::services.in_range;
    }
}

namespace spy {
    void __cdecl Bring_Home_Spies_Vs(game::id_player player_key, game::id_player target_key) {
        TEST_COLONY_POP_GROWTH::services.spies_home.push_back({player_key, target_key});
    }
}

namespace SHIPS {
    void Kill_Ship(s_ship_data& ship) {
        TEST_COLONY_POP_GROWTH::services.killed.push_back(test_ships::Rows(game::current_state).Row_Of(ship));
        ship.status = 5;
    }
}

namespace MAINSCR {
    void __cdecl Update_Ship_Icons_In_Between_Turns() {
        ++TEST_COLONY_POP_GROWTH::services.icon_updates;
    }
}

namespace CONTACT {
    void __cdecl Compute_Contacts() {
        ++TEST_COLONY_POP_GROWTH::services.contact_updates;
    }
}

namespace diplomac {
    void __cdecl Change_Relations(int16_t amount, game::id_player player_key, game::id_player target_key, int16_t reason, int16_t star, int16_t extra) {
        int player = legacy_world::Encode_Player_Slot(player_key);
        int target = legacy_world::Encode_Player_Slot(target_key);
        TEST_COLONY_POP_GROWTH::services.relations.push_back({amount, static_cast<int16_t>(player), static_cast<int16_t>(target), reason, star, extra});
    }
}

namespace invasion {
    void __cdecl Enforce_Population_Limits_At_Colony(s_colony* colony) {
        TEST_COLONY_POP_GROWTH::services.population_limits.push_back(static_cast<int16_t>(game::current_state.planets.at(colony->planet_id).colony_id.id));
    }
}

namespace maintain {
    void __cdecl Player_Maintenance(s_player* player) {
        TEST_COLONY_POP_GROWTH::services.maintenance.push_back((int16_t)(player - game::current_state.players.data()));
    }
}

namespace TEXTBOX {
    void __cdecl Text_Box(const char* title, const char* text) {
        TEST_COLONY_POP_GROWTH::services.titles.emplace_back(title);
        TEST_COLONY_POP_GROWTH::services.texts.emplace_back(text);
    }
}
