#include "pch.h"
#include "../fixtures/ship_rows.h"

#include <memory>

static_assert(game::player_bitmap::From_Byte(0x81).test(game::id_player{7}));
static_assert(game::player_bitmap::From_Byte(0x81).To_Byte() == 0x81);
static_assert(game::player_bitmap::Only(game::NO_PLAYER).none());
static_assert(game::player_bitmap::Only(game::id_player{65536}).none());
#include "../fixtures/star_roster.h"

#include "doctest/doctest.h"
#include "game/legacy_ids.h"
#include "game/ship_ids.h"
#include "../fixtures/game_state.h"
#include "../fixtures/planet_roster.h"

DOCTEST_TEST_CASE("ground-tech assessment uses the supplied player's researched equipment") {
    test::s_game_state fixture;
    auto& live = game::current_state;
    live.player_count = 1;
    auto world = std::make_unique<game::s_game_state>(live);
    live.players[0].tech_applications[TECH_APP_POWERED_ARMOR] = TECH_RESEARCH_STATUS_RESEARCHED;
    world->players[0].tech_applications[TECH_APP_BATTLEOIDS] = TECH_RESEARCH_STATUS_RESEARCHED;
    AIDATA::s_state ai{};
    aitech::Compute_AI_Tech_Info(&ai, &live, GAME_DIFFICULTY_AVERAGE, 0);
    DOCTEST_CHECK(ai.players[0].technology.ground_tech_score == 8);
    aitech::Compute_AI_Tech_Info(&ai, world.get(), GAME_DIFFICULTY_AVERAGE, 0);
    DOCTEST_CHECK(ai.players[0].technology.ground_tech_score == 5);
    DOCTEST_CHECK(live.players[0].tech_applications[TECH_APP_BATTLEOIDS] != TECH_RESEARCH_STATUS_RESEARCHED);
}

DOCTEST_TEST_CASE("home climate and planet selection use the supplied orbits and spectral class") {
    test::s_game_state fixture;
    auto& live = game::current_state;
    const auto star = game::Create_Star(live);
    DOCTEST_REQUIRE(star);
    const auto live_planet = game::Create_Planet(live, {.star_id = *star});
    DOCTEST_REQUIRE(live_planet);
    live.stars.at(*star).orbit_planet_ids[0] = *live_planet;
    live.stars.at(*star).spectral_class = STAR_CLASS_F;

    auto detached = std::make_unique<game::s_game_state>(live);
    const auto planet = game::Create_Planet(*detached, {.star_id = *star, .planet_type = PLANET_TYPE_PLANET});
    DOCTEST_REQUIRE(planet);
    auto& system = detached->stars.at(*star);
    system.orbit_planet_ids[0] = *planet;
    system.spectral_class = STAR_CLASS_BLACK_HOLE;
    auto& home = detached->planets.at(*planet);
    home.climate = PLANET_CLIMATE_TERRAN;
    DOCTEST_CHECK(HOMEGEN::Random_Planet(*detached, *star) == *planet);
    DOCTEST_CHECK(HOMEGEN::Enforce_Climate(*detached, *star, PLANET_CLIMATE_TERRAN) == *planet);
    DOCTEST_CHECK(home.food_per_farmer == 0);

    test::s_saved_array saved_weights{MOX::climate_roll_table[PLANET_CLIMATE_TERRAN]};
    std::ranges::fill(MOX::climate_roll_table[PLANET_CLIMATE_TERRAN], 1);
    const auto original_star = live.stars.at(*star);
    const auto original_planet = live.planets.at(*live_planet);
    home.climate = PLANET_CLIMATE_BARREN;
    DOCTEST_CHECK(HOMEGEN::Enforce_Climate(*detached, *star, PLANET_CLIMATE_TERRAN) == *planet);
    DOCTEST_CHECK(home.climate == PLANET_CLIMATE_TERRAN);
    DOCTEST_CHECK(system.spectral_class < STAR_CLASS_BLACK_HOLE);
    DOCTEST_CHECK(home.food_per_farmer == MOX::food_per_farmer_table[PLANET_CLIMATE_TERRAN]);
    DOCTEST_CHECK(live.stars.at(*star) == original_star);
    DOCTEST_CHECK(live.planets.at(*live_planet) == original_planet);
}

DOCTEST_TEST_CASE("colony AI classifies, sorts and recalculates the supplied colonies") {
    test::s_game_state fixture;
    auto& live = game::current_state;
    live.player_count = 1;
    const game::id_player owner{0};
    live.players[0].objectives = PLAYER_OBJECTIVE_INDUSTRIALIST;
    live.players[0].do_not_research = 1;
    live.players[0].traits[TRAIT_CURRENT_GOVERNMENT] = GOVERNMENT_UNIFICATION;
    std::array<game::id_star, 2> stars;
    std::array<game::id_planet, 2> planets;
    std::array<game::id_colony, 2> colonies;
    for (int i = 0; i < 2; ++i) {
        const auto star = game::Create_Star(live);
        DOCTEST_REQUIRE(star);
        stars[i] = *star;
        const auto planet = game::Create_Planet(live, {.star_id = *star, .gravity_class = PLANET_GRAVITY_NORMAL_G});
        DOCTEST_REQUIRE(planet);
        planets[i] = *planet;
        const auto id = game::Create_Colony(live, {.owner = 0, .planet_id = *planet, .n_pops = 3});
        DOCTEST_REQUIRE(id);
        colonies[i] = *id;
        live.planets.at(*planet).colony_id = *id;
        live.planets.at(*planet).food_per_farmer = 2;
        live.stars.at(*star).orbit_planet_ids[0] = *planet;
        auto& colony = live.colonies.at(*id);
        std::ranges::fill(colony.producing, COLONY_PRODUCTION_NONE);
        colony.producing[0] = BUILDING_AUTOMATED_FACTORY;
        colony.bought_outright = 1;
        colony.food2_per_farmer = 2;
    }
    live.stars.at(stars[0]).blockaded.set(owner);
    auto world = std::make_unique<game::s_game_state>(live);
    world->stars.at(stars[0]).blockaded.reset(owner);
    world->stars.at(stars[1]).blockaded.set(owner);
    world->planets.at(planets[0]).food_per_farmer = 16;
    world->planets.at(planets[1]).food_per_farmer = 20;
    const auto old_colonies = live.colonies;
    const auto old_player = live.players[0];

    AIDATA::s_state ai{};
    aidudes::All_Colony_AI(&ai, world.get(), GAME_DIFFICULTY_TUTOR, 0, 35000);
    const auto& work = ai.players[0].colony_work;
    DOCTEST_CHECK(work.complete);
    DOCTEST_REQUIRE(work.colonies.size() == 2);
    DOCTEST_CHECK(work.blockaded_count == 1);
    DOCTEST_CHECK(work.colonies[0].colony_id == colonies[1]);
    DOCTEST_CHECK(work.colonies[1].colony_id == colonies[0]);
    DOCTEST_CHECK(work.colonies[1].n_workers >= 1);
    DOCTEST_CHECK(world->colonies.at(colonies[0]).food2_per_farmer == 16);
    DOCTEST_CHECK(world->colonies.at(colonies[1]).food2_per_farmer == 20);
    DOCTEST_CHECK(live.colonies == old_colonies);
    DOCTEST_CHECK(live.players[0] == old_player);
}

DOCTEST_TEST_CASE("detached empire production recalculates the world whose order changed") {
    test::s_game_state fixture;
    auto& live = game::current_state;
    live.player_count = 1;
    const auto star = game::Create_Star(live);
    DOCTEST_REQUIRE(star);
    const auto planet = game::Create_Planet(live, {.star_id = *star});
    DOCTEST_REQUIRE(planet);
    const auto colony = game::Create_Colony(live, {.owner = 0, .planet_id = *planet, .n_pops = 1});
    DOCTEST_REQUIRE(colony);
    live.planets.at(*planet).food_per_farmer = 2;
    live.colonies.at(*colony).food2_per_farmer = 22;
    auto world = std::make_unique<game::s_game_state>(live);
    auto& draft = world->colonies.at(*colony);
    world->planets.at(*planet).food_per_farmer = 8;
    std::ranges::fill(draft.producing, COLONY_PRODUCTION_NONE);
    test::s_saved_value saved_rules{AIBUILD::rules};
    AIBUILD::rules.strategic.expansion.enabled = false;
    AIBUILD::rules.strategic.emergency_defense.enabled = false;
    AIBUILD::rules.strategic.economic_emergency.enabled = false;
    DOCTEST_SUBCASE("trade goods") {
        draft.producing[0] = COLONY_PRODUCTION_TRADE_GOODS;
    }
    DOCTEST_SUBCASE("economic emergency") {
        world->players[0].surplus_bc = -10;
        world->players[0].total_maintenance = 10;
        draft.production[ECON_INDUSTRY] = 5;
        AIBUILD::rules.strategic.economic_emergency.enabled = true;
        AIBUILD::rules.strategic.economic_emergency.product = COLONY_PRODUCTION_TRADE_GOODS;
    }
    const auto live_colony = live.colonies.at(*colony);
    const auto live_player = live.players[0];
    AIDATA::s_state ai{};
    aidudes::Assign_Empire_Building(&ai, world.get(), 0, 0, game::id_player{0});
    DOCTEST_CHECK(draft.food2_per_farmer == 8);
    DOCTEST_CHECK(live.colonies.at(*colony) == live_colony);
    DOCTEST_CHECK(live.players[0] == live_player);
}

DOCTEST_TEST_CASE("detached empire production queues a new hull without allocating in the live roster") {
    test::s_game_state fixture;
    auto& live = game::current_state;
    live.player_count = 1;
    const auto star = game::Create_Star(live);
    DOCTEST_REQUIRE(star);
    const auto planet = game::Create_Planet(live, {.star_id = *star});
    DOCTEST_REQUIRE(planet);
    const auto colony = game::Create_Colony(live, {.owner = 0, .planet_id = *planet, .n_pops = 1});
    DOCTEST_REQUIRE(colony);
    auto world = std::make_unique<game::s_game_state>(live);
    world->next_ship_id = 90000;
    auto& player = world->players[0];
    player.ship_speed = player.ship_range = 5;
    player.ship_designs[SHIP_SIZE_SMALL].size = SHIP_SIZE_SMALL;
    player.ship_designs[SHIP_SIZE_SMALL].ship_type = SHIP_TYPE_COMBAT;
    strlcpy(player.ship_designs[SHIP_SIZE_SMALL].name, "Candidate", sizeof(player.ship_designs[SHIP_SIZE_SMALL].name));
    player.ship_designs[SHIP_SIZE_MEDIUM].cost = INT16_MAX;
    auto& order = world->colonies.at(*colony);
    std::ranges::fill(order.producing, COLONY_PRODUCTION_NONE);
    order.production[ECON_INDUSTRY] = 20;
    test::s_saved_value saved_rules{AIBUILD::rules};
    AIBUILD::rules.strategic.expansion.enabled = false;
    AIBUILD::rules.strategic.emergency_defense.enabled = false;
    AIBUILD::rules.strategic.economic_emergency.enabled = false;
    for (auto& choice : AIBUILD::rules.strategic.allocation.candidates) choice.enabled = false;
    auto& combat = AIBUILD::rules.strategic.allocation.candidates[AIBUILD::ALLOCATION_COMBAT_SHIPS];
    combat = {.enabled = true};
    combat.weight.base = 100;
    combat.weight.clamp.maximum = 100;
    AIDATA::s_state ai{};
    ai.players[0].build.combat_strength.additional_wanted = 10;

    char name_format[] = "%s %d";
    test::s_saved_value saved_format{ESTRINGS::strings[0x2a]};
    ESTRINGS::strings[0x2a] = name_format;
    DOCTEST_SUBCASE("matching live colony key") {}
    DOCTEST_SUBCASE("missing live colony key") { live.colonies.clear(); }

    const auto live_player = live.players[0];
    const auto live_next_id = live.next_ship_id;
    aidudes::Assign_Empire_Building(&ai, world.get(), 0, 1, game::id_player{0});
    const auto key = game::Queued_Ship_Key(order.producing[0]);
    DOCTEST_REQUIRE(key == game::id_ship{90000});
    DOCTEST_REQUIRE(world->ships.contains(key));
    DOCTEST_CHECK(world->ships.at(key).status == 6);
    DOCTEST_CHECK(world->ships.at(key).position.star_id == *star);
    DOCTEST_CHECK(static_cast<int>(player.ships_built[SHIP_SIZE_SMALL]) == 1);
    DOCTEST_CHECK(world->ship_generations[0] == 1);
    DOCTEST_CHECK(live.ships.empty());
    DOCTEST_CHECK(live.ship_slots.empty());
    DOCTEST_CHECK(live.ship_generations[0] == 0);
    DOCTEST_CHECK(live.next_ship_id == live_next_id);
    DOCTEST_CHECK(live.players[0] == live_player);
}

DOCTEST_TEST_CASE("blockaded colonists farm according to their own planet's food output") {
    test::s_game_state fixture;
    auto& live = game::current_state;
    live.player_count = 1;
    live.players[0].objectives = PLAYER_OBJECTIVE_HUMAN;
    live.players[0].traits[TRAIT_CURRENT_GOVERNMENT] = GOVERNMENT_UNIFICATION;
    auto& government = COLCALC::government_bonuses[GOVERNMENT_UNIFICATION];
    test::s_saved_value saved_government{government};
    government.food = 0;
    const auto star = game::Create_Star(live);
    DOCTEST_REQUIRE(star);
    const auto planet = game::Create_Planet(live, {.star_id = *star, .gravity_class = PLANET_GRAVITY_NORMAL_G});
    DOCTEST_REQUIRE(planet);
    live.planets.at(*planet).food_per_farmer = 2;
    live.stars.at(*star).blockaded.set(game::id_player{0});
    const auto colony = game::Create_Colony(live, {.owner = 0, .planet_id = *planet, .n_pops = 3});
    DOCTEST_REQUIRE(colony);
    live.colonies.at(*colony).food2_per_farmer = 2;
    const auto original = live.colonies.at(*colony);
    auto rich_world = std::make_unique<game::s_game_state>(live);
    auto poor_world = std::make_unique<game::s_game_state>(live);
    rich_world->planets.at(*planet).food_per_farmer = 16;
    AIDATA::s_colony_assignment rich{.colony_id = *colony}, poor{.colony_id = *colony};
    aidudes::Do_Blockaded_Colony(rich_world.get(), &rich);
    aidudes::Do_Blockaded_Colony(poor_world.get(), &poor);
    DOCTEST_CHECK(rich_world->colonies.at(*colony).food2_per_farmer == 16);
    DOCTEST_CHECK(poor_world->colonies.at(*colony).food2_per_farmer == 2);
    DOCTEST_CHECK(rich.n_farmers < poor.n_farmers);
    DOCTEST_CHECK(live.colonies.at(*colony) == original);
}

DOCTEST_TEST_CASE("Dragon popup captures a star key at selection before roster order changes") {
    test::s_game_state fixture;
    auto& world = game::current_state;
    world.next_star_id = 90000;
    const auto origin = game::Create_Star(world);
    const auto target = game::Create_Star(world);
    DOCTEST_REQUIRE(origin);
    DOCTEST_REQUIRE(target);
    test::s_saved_array saved_map_fields{MOX::galaxy_map_star_field};
    test::s_saved_array saved_popup_fields{MAINPUPS::mp_fields};
    test::s_saved_value saved_mode{MAINPUPS::field_mode};
    MAINPUPS::mp_fields[2] = 4;
    MAINPUPS::mp_fields[25] = 5;
    MAINPUPS::field_mode = 1;
    MOX::galaxy_map_star_field[0] = 6;
    MOX::galaxy_map_star_field[1] = 7;
    int16_t accepted = 0;
    const auto selected = MAINPUPS::Evaluate_Dragon_New_Star_Input(7, -1, &accepted, *origin);
    DOCTEST_REQUIRE(accepted == 1);
    DOCTEST_CHECK(selected == *target);
    std::swap(world.star_order[0], world.star_order[1]);
    DOCTEST_CHECK(game::Star_Key_At_Row(world, 1) == *origin);
    DOCTEST_CHECK(selected == game::Star_Key_At_Row(world, 0));
    accepted = 0;
    DOCTEST_CHECK(MAINPUPS::Evaluate_Dragon_New_Star_Input(4, -1, &accepted, *origin) == game::NO_STAR);
    DOCTEST_CHECK(accepted == 1);
}

DOCTEST_TEST_CASE("detached ship prices use the priced hull without a live editor") {
    test::s_game_state fixture;
    auto world = std::make_unique<game::s_game_state>();
    world->player_count = 1;
    const auto player = world->players[0].id;
    world->players[0].traits[TRAIT_CURRENT_GOVERNMENT] = -1;
    auto& small = TECHDATA::hull_data[SHIP_SIZE_SMALL];
    auto& shield = TECHDATA::shields[SHIELD_CLASS_I_SHIELD];
    auto& computer = TECHDATA::computers[COMPUTER_ELECTRONIC_COMPUTER];
    auto& drive = TECHDATA::drives[DRIVE_NO_DRIVE];
    auto& armor = TECHDATA::armor[ARMOR_NO_ARMOR];
    test::s_saved_value saved_small{small};
    test::s_saved_value saved_shield{shield};
    test::s_saved_value saved_computer{computer};
    test::s_saved_value saved_drive{drive};
    test::s_saved_value saved_armor{armor};
    small.size = 100;
    small.cost = 100;
    drive.cost_per_ship_type[SHIP_SIZE_SMALL] = 0;
    armor.cost_percent = 0;
    shield.tech_app_id = computer.tech_app_id = TECH_APP_NO_TECH;
    shield.cost_per_ship_type[SHIP_SIZE_SMALL] = 12;
    shield.cost_per_ship_type[SHIP_SIZE_LARGE] = 90;
    computer.cost_by_ship_type[SHIP_SIZE_SMALL] = 9;
    computer.cost_by_ship_type[SHIP_SIZE_LARGE] = 60;
    s_ship_design design{};
    design.size = SHIP_SIZE_SMALL;
    design.shield_type = SHIELD_CLASS_I_SHIELD;
    design.computer_type = COMPUTER_ELECTRONIC_COMPUTER;
    MOX::design->ship_size = SHIP_SIZE_LARGE;

    const auto price = DESIGN::Calculate_Design(*world, player, &design, nullptr);
    DOCTEST_REQUIRE(price == 121);
    DOCTEST_CHECK(static_cast<int>(MOX::design->ship_size) == SHIP_SIZE_LARGE);
    MOX::design = nullptr;
    DOCTEST_CHECK(DESIGN::Calculate_Design(*world, player, &design, nullptr) == price);
    DOCTEST_CHECK(MOX::design == nullptr);
}

DOCTEST_TEST_CASE("attack forecast buys queued reinforcements at the supplied ship's price") {
    test::s_game_state fixture;
    auto world = std::make_unique<game::s_game_state>();
    world->player_count = 2;
    const auto star = game::Create_Star(*world);
    DOCTEST_REQUIRE(star);
    const auto planet = game::Create_Planet(*world, {.star_id = *star});
    DOCTEST_REQUIRE(planet);
    const auto colony = game::Create_Colony(*world, {.owner = 1, .planet_id = *planet});
    DOCTEST_REQUIRE(colony);
    constexpr game::id_ship attacker{70000}, queued{70001};
    s_ship_data attacking{};
    attacking.id = attacker;
    attacking.owner = game::owner_slot{0};
    world->ships.emplace(attacker, attacking);
    s_ship_data reinforcement{};
    reinforcement.id = queued;
    reinforcement.owner = game::owner_slot{1};
    reinforcement.d.ship_type = SHIP_TYPE_COMBAT;
    reinforcement.d.cost = 100;
    reinforcement.d.ship_weapon[0] = {.type = WEAPON_LASER_CANNON, .count = 1, .current_count = 1, .specials = WEAPON_MOD_FLAG_SHIELD_PIERCING};
    auto& weapon = TECHDATA::weapons[WEAPON_LASER_CANNON];
    test::s_saved_value saved_weapon{weapon};
    weapon.max_damage = 100;
    weapon.type = WEAPON_TYPE_BEAM;
    world->ships.emplace(queued, reinforcement);
    reinforcement.d.cost = 10;
    game::current_state.ships.emplace(queued, reinforcement);
    world->colonies.at(*colony).producing[0] = game::Queued_Ship_Product(queued);
    const auto cheap = COLCALC::Production_Cost_To_Buy(10, 0);
    const auto expensive = COLCALC::Production_Cost_To_Buy(100, 0);
    DOCTEST_REQUIRE(cheap > 0);
    DOCTEST_REQUIRE(cheap < expensive);
    AIDATA::s_state ai{};
    AIDATA::s_movement_workspace work{};
    AIMOVE::s_ai_movement_context ctx{game::id_player{0}, &world->players[0], &work, world.get(), 0, 0};
    ai.ships[attacker].strengths[1] = 8;
    ai.stars[*star].possible_strengths[1][0][1] = 1;
    DOCTEST_REQUIRE(AIPOWER::Ship_Strength_Vs_Ships(world.get(), 0, queued, game::owner_slot{0}) > 6);
    const auto would_flee = [&](int32_t budget) {
        world->players[1].bc = budget;
        int8_t attack_type = 0;
        uint8_t flee = 0;
        AIMOVE::Compute_Attack_Worthiness(&ai, &ctx, *colony, std::span{&attacker, 1}, 1, &attack_type, &flee, nullptr, 0);
        return flee != 0;
    };
    DOCTEST_CHECK_FALSE(would_flee(cheap));
    DOCTEST_CHECK(would_flee(expensive));
    game::current_state.ships.erase(queued);
    DOCTEST_CHECK_FALSE(would_flee(cheap));
}

DOCTEST_TEST_CASE("detached leader tech grants refresh designs, fleet range, and colony production") {
    test::s_game_state fixture;
    auto& fuel = TECHDATA::fuel[FUEL_COUNT - 1];
    test::s_saved_value saved_fuel{fuel};
    fuel.tech_app_id = TECH_APP_DEUTERIUM_FUEL_CELLS;
    fuel.ship_range = 9;
    auto& armor = TECHDATA::armor[ARMOR_TRITANIUM_ARMOR];
    test::s_saved_value saved_armor{armor};
    armor.tech_app_id = TECH_APP_TRITANIUM_ARMOR;
    test::s_saved_value hull_space{TECHDATA::hull_data[SHIP_SIZE_SMALL].size};
    TECHDATA::hull_data[SHIP_SIZE_SMALL].size = 100;
    test::s_saved_value microlite{COLCALC::production_worker_bonus.microlite_construction};
    COLCALC::production_worker_bonus.microlite_construction = 2;
    auto world = std::make_unique<game::s_game_state>();
    world->player_count = 1;
    auto& player = world->players[0];
    player.objectives = PLAYER_OBJECTIVE_HUMAN;
    player.ship_range = -1;
    const auto star = game::Create_Star(*world);
    DOCTEST_REQUIRE(star);
    const auto planet = game::Create_Planet(*world, {.star_id = *star});
    DOCTEST_REQUIRE(planet);
    const auto colony_id = game::Create_Colony(*world, {.owner = 0, .planet_id = *planet, .n_pops = 1});
    DOCTEST_REQUIRE(colony_id);
    auto& colony = world->colonies.at(*colony_id);
    colony.pop[0].profession = POP::PROF_WORKER;
    test_ships::Rows(*world).Set_Key_At_Row(0, game::id_ship{70000});
    auto& ship = world->ships.at(game::id_ship{70000});
    ship.owner = factions::From_Player(player.id);
    ship.status = 6;
    ship.d.armor_type = ARMOR_TITANIUM_ARMOR;
    game::current_state.player_count = 1;
    game::current_state.players[0].ship_range = 2;
    const auto live = game::current_state.players[0];

    COLCALC::Player_Gets_Tech_App(*world, &player, fuel.tech_app_id);
    DOCTEST_CHECK(player.tech_applications[fuel.tech_app_id] == TECH_RESEARCH_STATUS_RESEARCHED);
    DOCTEST_CHECK(player.ship_range == fuel.ship_range);
    const auto industry = colony.industry_per_worker;
    COLCALC::Player_Gets_Tech_App(*world, &player, TECH_APP_MICROLITE_CONSTRUCTION);
    DOCTEST_CHECK(colony.industry_per_worker == industry + 2);
    DOCTEST_CHECK(player.industry_produced == colony.production[ECON_INDUSTRY]);
    COLCALC::Player_Gets_Tech_App(*world, &player, TECH_APP_TRITANIUM_ARMOR);
    DOCTEST_CHECK(player.ship_designs[0].armor_type == ARMOR_TRITANIUM_ARMOR);
    DOCTEST_CHECK(ship.d.armor_type == ARMOR_TRITANIUM_ARMOR);
    DOCTEST_CHECK(game::current_state.players[0] == live);
}

DOCTEST_TEST_CASE("AI hiring and rescue change only the supplied roster") {
    test::s_game_state fixture;
    auto storage = std::make_unique<game::s_game_state>();
    auto& world = *storage;
    world.player_count = 1;
    world.players[0].objectives = PLAYER_OBJECTIVE_MILITARIST;
    world.players[0].bc = 1000;
    world.players[0].officer_for_hire = game::id_leader{0};
    world.leaders[0].id = game::id_leader{0};
    world.leaders[0].type = LEADER_TYPE_SHIP;
    world.leaders[0].special_skills = LEADER_SHIP_SKILL_HELMSMAN;
    world.leaders[0].skill_value = 10;
    game::current_state.leaders[0].skill_value = 1000;
    const auto rescue = game::Create_Star(world, {.system_special = 80});
    DOCTEST_REQUIRE(rescue);
    world.leaders[1].id = game::id_leader{1};
    world.leaders[1].type = LEADER_TYPE_SHIP;
    world.leaders[1].status = 5;
    world.leaders[1].Set_Star_Location(*rescue);

    const auto expected_cost = OFFICER::Officer_Cost(world, game::id_player{0}, world.leaders[0].id);
    DOCTEST_REQUIRE(expected_cost > 0);
    AIDATA::s_state ai{};
    aileader::Do_AI_Leaders(&ai, &world);
    DOCTEST_CHECK(world.players[0].bc == 1000 - expected_cost);
    DOCTEST_CHECK(world.players[0].officer_for_hire == game::NO_LEADER);
    DOCTEST_CHECK(world.leaders[0].employer == game::id_player{0});
    DOCTEST_CHECK(world.leaders[0].status == 0);
    DOCTEST_CHECK(world.leaders[1].employer == game::id_player{0});
    DOCTEST_CHECK(world.leaders[1].star_location == game::NO_STAR);
    DOCTEST_CHECK(game::current_state.leaders[0].employer == game::NO_PLAYER);
    DOCTEST_CHECK(game::current_state.leaders[1].employer == game::NO_PLAYER);
}

DOCTEST_TEST_CASE("destroying a detached colony clears its own officer and system flags") {
    test::s_game_state fixture;
    auto storage = std::make_unique<game::s_game_state>();
    auto& world = *storage;
    world.player_count = 1;
    const auto star = game::Create_Star(world);
    DOCTEST_REQUIRE(star);
    const auto planet = game::Create_Planet(world, {.star_id = *star});
    DOCTEST_REQUIRE(planet);
    const auto colony = game::Create_Colony(world, {.owner = 0, .planet_id = *planet});
    DOCTEST_REQUIRE(colony);
    world.stars.at(*star).orbit_planet_ids[0] = *planet;
    world.stars.at(*star).officer_id[0] = game::id_leader{0};
    world.stars.at(*star).has_colony.set(game::id_player{0});
    world.leaders[0].id = game::id_leader{0};
    world.leaders[0].employer = game::id_player{0};
    world.leaders[0].type = LEADER_TYPE_COLONY;
    world.leaders[0].status = 1;
    world.leaders[0].Set_Star_Location(*star);
    world.colonies.at(*colony).bought_outright = 20;
    const auto live_cash = game::current_state.players[0].bc;

    BOMB::Destroy_Colony(world, *colony, -1);
    DOCTEST_CHECK_FALSE(world.colonies.contains(*colony));
    DOCTEST_CHECK(world.planets.at(*planet).colony_id == game::NO_COLONY);
    DOCTEST_CHECK_FALSE(world.stars.at(*star).has_colony.any());
    DOCTEST_CHECK(world.stars.at(*star).officer_id[0] == game::NO_LEADER);
    DOCTEST_CHECK(world.leaders[0].status == -2);
    DOCTEST_CHECK(world.players[0].bc > 0);
    DOCTEST_CHECK(game::current_state.players[0].bc == live_cash);
    DOCTEST_CHECK(game::current_state.leaders[0].status == -1);
}

using game::id_ship;
using legacy_world::Ship_Id;
using legacy_world::Star_Id;
using legacy_world::Player_Id;

DOCTEST_TEST_CASE("AI ship order uses the supplied roster rows for orbiting ships") {
    auto state_storage = std::make_unique<game::s_game_state>();
    auto& state = *state_storage;
    DOCTEST_REQUIRE(game::Create_Star(state));
    const auto second = game::Create_Star(state);
    state.next_star_id = 90000;
    const auto first = game::Replace_Draft_Star(state, 0);
    DOCTEST_REQUIRE(first);
    DOCTEST_REQUIRE(second);

    s_ship_data ships[4]{};
    const game::id_ship keys[] = {{90003}, {90001}, {90000}, {90002}};
    for (int row = 0; row < 4; ++row) ships[row].id = keys[row];
    ships[0].position.Set({game::e_ship_location::at_star, *first});
    ships[1].position.Set({game::e_ship_location::at_star, *second});
    ships[2].position.Set({game::e_ship_location::at_star, game::NO_STAR, game::e_ship_target::antares});
    ships[3].position.Set({game::e_ship_location::in_flight, *first});
    test_ships::Rows(state).assign(std::begin(ships), std::end(ships));
    ship_slots::Resize(state, 4);
    state.next_ship_id = 90004;
    auto& world = state;

    DOCTEST_CHECK(AIDATA::Compare_Combat_Ships(world, keys[0], keys[1]) < 0);
    DOCTEST_CHECK(AIDATA::Compare_Combat_Ships(world, keys[1], keys[2]) < 0);
    DOCTEST_CHECK(AIDATA::Compare_Combat_Ships(world, keys[2], keys[3]) < 0);
    test_ships::Rows(state)[1].owner = factions::From_Legacy(1);
    DOCTEST_CHECK(AIDATA::Compare_Combat_Ships(world, keys[1], keys[3]) > 0);
}

DOCTEST_TEST_CASE("advanced start ranking keeps planet identities attached to scores") {
    s_adv_civ_player_planet candidates[] = {
        {.worth = 20, .planet_id = game::id_planet{90001}},
        {.worth = 5, .planet_id = game::id_planet{12}},
        {.worth = 30, .planet_id = game::id_planet{70000}},
    };
    Legacy_Qsort(candidates, std::size(candidates), sizeof(candidates[0]), ADVCIV::qsort_planet_worth);
    DOCTEST_CHECK(candidates[0].planet_id == game::id_planet{70000});
    DOCTEST_CHECK(candidates[1].planet_id == game::id_planet{90001});
    DOCTEST_CHECK(candidates[2].planet_id == game::id_planet{12});
}

DOCTEST_TEST_CASE("advanced start candidates resolve sparse orbit IDs in orbit order") {
    auto state_storage = std::make_unique<game::s_game_state>();
    auto& state = *state_storage;
    state.player_count = 1;
    DOCTEST_REQUIRE(game::Create_Star(state));
    state.next_star_id = 90000;
    const auto system = *game::Create_Star(state);
    const s_planet_data unoccupied{.star_id = system, .planet_type = PLANET_TYPE_PLANET};
    state.planets.emplace(game::id_planet{17}, unoccupied);
    state.planets.emplace(game::id_planet{70000}, unoccupied);
    auto& star = state.stars.at(system);
    star.orbit_planet_ids[0] = game::id_planet{70000};
    star.orbit_planet_ids[1] = game::id_planet{18}; // A missing key must not resolve to another record.
    star.orbit_planet_ids[2] = game::id_planet{17};

    s_adv_civ_player candidates{};
    candidates.stars[1].star_slot = 1;
    auto* previous = ADVCIV::adv_civ_player;
    ADVCIV::adv_civ_player = &candidates;
    ADVCIV::Build_Adv_Civ_Planet_List(state);
    ADVCIV::adv_civ_player = previous;

    DOCTEST_REQUIRE(candidates.candidate_planet_count == 2);
    DOCTEST_CHECK(candidates.planets[0].planet_id == game::id_planet{70000});
    DOCTEST_CHECK(candidates.planets[1].planet_id == game::id_planet{17});
}

DOCTEST_TEST_CASE("advanced start reservations follow star IDs rather than slots or live colony flags") {
    auto state_storage = std::make_unique<game::s_game_state>();
    auto& state = *state_storage;
    const auto empty = *game::Create_Star(state);
    state.next_star_id = 90000;
    const auto reserved = *game::Create_Star(state);
    const game::id_player owner{1};
    const ADVCIV::planned_colony_owners owners{{reserved, owner}};

    DOCTEST_CHECK_FALSE(state.stars.at(reserved).has_colony.test(owner));
    DOCTEST_CHECK(game::Star_Slot(state, reserved) == 1);
    DOCTEST_CHECK(ADVCIV::Planned_Colony_Owner(owners, reserved) == owner);

    std::swap(state.star_order[0], state.star_order[1]);
    state.star_order_sorted = false;
    DOCTEST_CHECK(game::Star_Slot(state, reserved) == 0);
    DOCTEST_CHECK(game::Star_Key_At_Slot(state, 1) == empty);
    DOCTEST_CHECK(ADVCIV::Planned_Colony_Owner(owners, reserved) == owner);
    DOCTEST_CHECK(ADVCIV::Planned_Colony_Owner(owners, empty) == game::NO_PLAYER);
    DOCTEST_CHECK(ADVCIV::Planned_Colony_Owner(owners, game::NO_STAR) == game::NO_PLAYER);

    // The scoring path also consults the plan, not the live system bitmap.
    s_adv_civ_player candidates{};
    for (auto& candidate : candidates.stars) candidate.star_slot = -1;
    std::array<s_adv_civ_player, 2> player_scratch{};
    player_scratch[owner.id] = candidates;
    test::s_saved_value saved_players(ADVCIV::adv_civ_player);
    ADVCIV::adv_civ_player = player_scratch.data();
    s_planet_data planet{.star_id = reserved, .planet_type = PLANET_TYPE_PLANET};
    auto& player = state.players[owner.id];
    int32_t worth = 0;
    int8_t has_colony = 0;
    int16_t proximity_bonus = 0;
    ADVCIV::Get_Adv_Civ_Planet_Worthiness(state, owners, planet, player, &worth, &has_colony, &proximity_bonus);
    DOCTEST_CHECK(has_colony == 1);
    ADVCIV::Get_Adv_Civ_Planet_Worthiness(state, {}, planet, player, &worth, &has_colony, &proximity_bonus);
    DOCTEST_CHECK(has_colony == 0);
}

DOCTEST_TEST_CASE("nested game-state fixtures restore the outer roster") {
    test::s_game_state outer;
    outer.ships.resize(2);
    ship_slots::Resize(game::current_state, 2);
    game::current_state.player_count = 3;
    {
        test::s_game_state inner;
        DOCTEST_CHECK(game::current_state.ship_slots.size() == 0);
        DOCTEST_CHECK(game::current_state.player_count == 0);
    }
    DOCTEST_CHECK(game::current_state.ship_slots.size() == 2);
    DOCTEST_CHECK(game::current_state.player_count == 3);
}

DOCTEST_TEST_CASE("technology defaults bind to canonical starting fields") {
    test::s_saved_array fields{MOX::first_field_in_group};
    DOCTEST_CHECK(&TECHDATA::starting_techfield.biology == &MOX::first_field_in_group[1]);
    DOCTEST_CHECK(&TECHDATA::starting_techfield.xenon == &MOX::first_field_in_group[9]);
    MOX::first_field_in_group[1] = 17;
    MOX::first_field_in_group[9] = 75;
    DOCTEST_CHECK(TECHDATA::starting_techfield.biology == 17);
    DOCTEST_CHECK(TECHDATA::starting_techfield.xenon == 75);
}

DOCTEST_TEST_CASE("refit desirability uses the supplied owner and assessment") {
    s_player owner{};
    s_ship_data ship{};
    AIDATA::s_technology_assessment assessment;
    assessment.best_armor = 2;
    owner.ship_designs[SHIP_SIZE_SMALL] = ship.d;
    DOCTEST_CHECK(AIDATA::Refit_Desirability(&owner, &assessment, &ship) == 0);
    owner.ship_designs[SHIP_SIZE_SMALL].cost = 1;
    DOCTEST_CHECK(AIDATA::Refit_Desirability(&owner, &assessment, &ship) > 0);
}

DOCTEST_TEST_CASE("AI strength and technology leaves read supplied records instead of world globals") {
    s_player players[3] = {};
    players[1].objectives = PLAYER_OBJECTIVE_HUMAN;
    players[2].objectives = PLAYER_OBJECTIVE_MILITARIST;
    AIDATA::s_ship_assessment ship_assessment;
    ship_assessment.strengths[1] = 10;
    ship_assessment.strengths[2] = 40;
    AIDATA::s_empire_assessment empire;
    empire.reachable_player_count = 2;
    empire.reachable_players[1] = empire.reachable_players[2] = true;
    s_ship_data ship{};
    AIBUILD::s_opponent_view opponents{players, 3, 1};
    DOCTEST_CHECK(AIBUILD::Mean_Ship_Strength(&opponents, &ship_assessment, &empire, &ship) == 25);
    opponents.human_opponent_bias = 3;
    DOCTEST_CHECK(AIBUILD::Mean_Ship_Strength(&opponents, &ship_assessment, &empire, &ship) == 17);

    s_tech_armor_data armor[ARMOR_COUNT] = {};
    armor[ARMOR_TRITANIUM_ARMOR].tech_app_id = TECH_APP_TRITANIUM_ARMOR;
    players[0].tech_applications[TECH_APP_TRITANIUM_ARMOR] = TECH_RESEARCH_STATUS_RESEARCHED;
    DOCTEST_CHECK(aitech::Player_Best_Armor(armor, &players[0]) == ARMOR_TRITANIUM_ARMOR);

    s_tech_weapon_mod_data modifications[WEAPON_MOD_COUNT] = {};
    modifications[WEAPON_MOD_HEAVY_MOUNT].space = 20;
    DOCTEST_CHECK(AIPOWER::Modified_Weapon_Strength(modifications, 100, WEAPON_MOD_FLAG_HEAVY_MOUNT) == 120);
}

DOCTEST_TEST_CASE("star assessments name treaty partners and hostile players by actual treaty state") {
    test::s_game_state game_state;
    auto& players = game::current_state.players;
    s_colony colonies[3] = {};
    test_planets::Resize_Dense(3);
    s_star_data stars[1] = {};
    game::current_state.player_count = 3;
    int16_t colony_count = 3;
    int16_t star_count = 1;
    stars[0].has_colony = game::player_bitmap::From_Byte(7);
    stars[0].wormhole_star_id = game::NO_STAR;
    test_stars::Set(0, stars[0]);
    for (uint32_t i = 0; i < 3; ++i) {
        players[i].ship_range = 6;
        colonies[i].owner = legacy_world::Decode_Player_Slot((int8_t)i);
        colonies[i].planet_id = SERIAL::Decode_Legacy_Planet_Reference((int16_t)i);
        game::current_state.colonies.insert_or_assign(game::id_colony{static_cast<uint32_t>(i)}, colonies[i]);
    }
    players[0].treaty[1] = 1;
    players[0].treaty[2] = 4;
    AIDATA::s_state ai;
    auto& world = game::current_state;
    AIDATA::Compute_Star_Player_Range_Info(&ai, &world);
    const auto& assessment = ai.stars.at(Star_Id(0)).neighborhood.per_player[0];
    DOCTEST_CHECK(assessment.treaty_partners_in_range == 1);
    DOCTEST_CHECK(assessment.hostile_players_in_range == 1);
    DOCTEST_CHECK(assessment.neutral_players_in_range == 0);
    DOCTEST_CHECK(assessment.covering_colony_count == 1);
}

DOCTEST_TEST_CASE("AI strength lookup does not narrow a ship identity to a legacy index") {
    AIDATA::s_state ai;
    constexpr id_ship ship_id = Ship_Id(0x12345678u);
    ai.ships[ship_id].strengths[1] = 123;
    ai.ships[ship_id].strengths_in_nebula[1] = 277;
    ai.ships[Ship_Id(0)].strengths[1] = 7;
    s_star_data stars[1] = {};
    game::current_state.player_count = 2;
    test_stars::Set(0, stars[0]);
    auto& world = game::current_state;
    const auto faction = game::owner_bitmap{}.set(1);
    DOCTEST_CHECK(AIMOVE::Ship_Min_Strength_Versus_Players_At_Star(&ai, &world, 0, ship_id, faction, Star_Id(0)) == 123);
    test_stars::Record(0).in_nebula = 1;
    DOCTEST_CHECK(AIMOVE::Ship_Min_Strength_Versus_Players_At_Star(&ai, &world, 0, ship_id, faction, Star_Id(0)) == 277);
    DOCTEST_CHECK(ai.ships.size() == 2);
}

DOCTEST_TEST_CASE("range assessments use the supplied star keys, parents, and coordinates") {
    test::s_game_state fixture;
    auto state_storage = std::make_unique<game::s_game_state>();
    auto& state = *state_storage;
    state.next_star_id = 90000;
    const auto home = *game::Create_Star(state, {.x = 0, .has_colony = game::player_bitmap::From_Byte(1)});
    const auto far = *game::Create_Star(state, {.x = 300});
    const auto via = *game::Create_Star(state, {.x = 330});
    state.stars.at(home).wormhole_star_id = via;
    const auto planet = *game::Create_Planet(state, {.star_id = home});
    DOCTEST_REQUIRE(game::Create_Colony(state, {.owner = 0, .planet_id = planet}));

    test_stars::Set(far.id, {.x = 0});
    test_stars::Set(home.id, {.x = 0});
    test_stars::Set(via.id, {.x = 0});
    game::current_state.star_order = {far, home, via};
    game::current_state.star_order_sorted = false;

    auto& player = state.players[0];
    player.ship_range = 1;
    state.player_count = 1;
    auto& world = state;
    AIDATA::s_state ai;
    AIDATA::Compute_Star_Player_Range_Info(&ai, &world);
    DOCTEST_CHECK(ai.stars.at(home).neighborhood.per_player[0].covering_colony_count == 1);
    DOCTEST_CHECK(ai.stars.at(far).neighborhood.per_player[0].covering_colony_count == 1);

    int8_t normal[MAX_PLAYERS], extended[MAX_PLAYERS];
    AIDATA::Find_Players_In_Range(&world, far, normal, extended);
    DOCTEST_CHECK(normal[0] == 0);
    DOCTEST_CHECK(extended[0] == 0);
    DOCTEST_CHECK_FALSE(AIDATA::Star_In_Range_Of_Given_Star(&world, home, far, 1));
}

DOCTEST_TEST_CASE("AI staging and officer assignment ignore the singleton roster and player array") {
    test::s_game_state fixture;
    test_stars::Reset(2);
    auto state_storage = std::make_unique<game::s_game_state>();
    auto& state = *state_storage;
    state.next_star_id = 90000;
    const auto home = *game::Create_Star(state, {.has_colony = game::player_bitmap::From_Byte(1)});
    const auto other = *game::Create_Star(state, {.x = 30, .has_colony = game::player_bitmap::From_Byte(1)});
    for (auto& [key, star] : state.stars) star.officer_id[0] = game::NO_LEADER;
    const auto planet = *game::Create_Planet(state, {.star_id = home});
    DOCTEST_REQUIRE(game::Create_Colony(state, {.owner = 0, .planet_id = planet, .n_pops = 4}));
    auto& player = state.players[0];
    player.home_planet_id = planet;
    state.player_count = 1;
    auto& world = state;
    AIDATA::s_state ai;
    ai.stars[home].stage_weights[0] = 5;
    ai.stars[other].stage_weights[0] = 10;
    AISTAGE::Compute_Defensive_Stage_Points(&ai, &world, Player_Id(0));
    DOCTEST_CHECK(player.offensive_stagepoint == other);
    std::reverse(state.star_order.begin(), state.star_order.end());
    state.star_order_sorted = false;
    DOCTEST_CHECK(player.offensive_stagepoint == other);
    DOCTEST_CHECK(state.stars.at(other).is_stagepoint.test(game::id_player{0}));
    DOCTEST_CHECK_FALSE(ai.stars.contains(Star_Id(0)));

    auto& leader = state.leaders[0];
    leader.type = LEADER_TYPE_COLONY;
    leader.status = 0;
    leader.special_skills = LEADER_COLONY_SKILL_FINANCIAL_LEADER;
    aileader::Assign_Star_Officer(&world, &ai, Player_Id(0), game::id_leader{0});
    DOCTEST_CHECK(leader.star_location == home);
    DOCTEST_CHECK(leader.eta == 0);
    DOCTEST_CHECK(state.stars.at(home).officer_id[0] == game::id_leader{0});
    DOCTEST_CHECK(state.stars.at(other).officer_id[0] == game::NO_LEADER);
}

DOCTEST_TEST_CASE("colony proximity scores do not depend on star rows") {
    test::s_game_state fixture;
    auto state_storage = std::make_unique<game::s_game_state>();
    auto& state = *state_storage;
    state.next_star_id = 90000;
    const auto home = *game::Create_Star(state, {.x = 0, .has_colony = game::player_bitmap::From_Byte(1)});
    const auto near = *game::Create_Star(state, {.x = 60, .has_colony = game::player_bitmap::From_Byte(2)});
    const auto distant = *game::Create_Star(state, {.x = 240, .has_colony = game::player_bitmap::From_Byte(2)});
    const auto hole = *game::Create_Star(state, {.x = 30, .spectral_class = STAR_CLASS_BLACK_HOLE});
    const auto planet = *game::Create_Planet(state, {.star_id = home});
    s_colony colony{.owner = 0, .planet_id = planet};
    auto& players = state.players;
    players[0].ship_range = 2;
    s_settings settings{};
    settings.game_difficulty = GAME_DIFFICULTY_AVERAGE;
    state.player_count = 2;
    auto& world = state;
    AIDATA::s_state ai;
    ai.stars[home].neighborhood.per_player[0].covering_colony_count = 1;
    ai.stars[near].neighborhood.per_player[0].covering_colony_count = 2;
    ai.stars[distant].neighborhood.per_player[0].covering_colony_count = 1;
    ai.stars[hole].neighborhood.per_player[0].covering_colony_count = 1;
    const auto check_scores = [&] {
        DOCTEST_CHECK(AIDATA::Proximity_Worth_To_Player(&ai, &world, settings.game_difficulty, &colony, &players[0]) == 140);
        DOCTEST_CHECK(AIDATA::Proximity_Worth_To_Player(&ai, &world, settings.game_difficulty, &colony, &players[1]) == 250);
    };
    check_scores();
    state.star_order = {hole, distant, near, home};
    state.star_order_sorted = false;
    check_scores();
}
