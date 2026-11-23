#include "pch.h"
#include "../fixtures/star_roster.h"

#include "doctest/doctest.h"
#include "game/legacy_world.h"
#include "../fixtures/game_state.h"
#include "../fixtures/planet_roster.h"

using game::id_ship;
using legacy_world::Ship_Id;
using legacy_world::Star_Id;
using legacy_world::Player_Id;

DOCTEST_TEST_CASE("AI ship order uses the supplied roster rows for orbiting ships") {
    game::s_game_state state;
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
    state.ship_records.assign(std::begin(ships), std::end(ships));
    state.ship_count = 4;
    state.next_ship_id = 90004;
    const game::s_view world{.state = &state};

    DOCTEST_CHECK(AIDATA::Compare_Combat_Ships(world, keys[0], keys[1]) < 0);
    DOCTEST_CHECK(AIDATA::Compare_Combat_Ships(world, keys[1], keys[2]) < 0);
    DOCTEST_CHECK(AIDATA::Compare_Combat_Ships(world, keys[2], keys[3]) < 0);
    state.ship_records[1].owner = 1;
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
    test::s_game_state fixture;
    MOX::NUM_PLAYERS = 1;
    game::s_game_state state;
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
    candidates.stars[1].star_idx = 1;
    auto* previous = ADVCIV::adv_civ_player;
    ADVCIV::adv_civ_player = &candidates;
    ADVCIV::Build_Adv_Civ_Planet_List(state);
    ADVCIV::adv_civ_player = previous;

    DOCTEST_REQUIRE(candidates.candidate_planet_count == 2);
    DOCTEST_CHECK(candidates.planets[0].planet_id == game::id_planet{70000});
    DOCTEST_CHECK(candidates.planets[1].planet_id == game::id_planet{17});
}

DOCTEST_TEST_CASE("game-state fixtures bind borrowed views and restore nested bindings") {
    const game::s_view previous = game::current;
    s_player* previous_players = MOX::player;
    const int16_t previous_ship_count = game::current_state.ship_count;
    {
        test::s_game_state outer;
        DOCTEST_CHECK(game::current.players == outer.players.data());
        DOCTEST_CHECK(&game::Ship_Rows(game::current) == &outer.ships);
        DOCTEST_CHECK(game::current.state == &game::current_state);
        DOCTEST_CHECK(game::current.state == &game::current_state);
        DOCTEST_CHECK(game::current.settings == &MOX::settings);
        game::current_state.ship_count = 2;
        DOCTEST_CHECK(game::current.Ship_Count() == 2);
        {
            test::s_game_state inner;
            DOCTEST_CHECK(game::current.players == inner.players.data());
            DOCTEST_CHECK(&game::Ship_Rows(game::current) == &inner.ships);
            DOCTEST_CHECK(game::current.Ship_Count() == 0);
        }
        DOCTEST_CHECK(MOX::player == outer.players.data());
        DOCTEST_CHECK(game::current.players == outer.players.data());
        DOCTEST_CHECK(&game::Ship_Rows(game::current) == &outer.ships);
        DOCTEST_CHECK(game::current.Ship_Count() == 2);
    }
    DOCTEST_CHECK(MOX::player == previous_players);
    DOCTEST_CHECK(game::current_state.ship_count == previous_ship_count);
    DOCTEST_CHECK(game::current.players == previous.players);
    DOCTEST_CHECK(game::current.state == previous.state);
    DOCTEST_CHECK(game::current.settings == previous.settings);
    DOCTEST_CHECK(game::current.player_count == previous.player_count);
    DOCTEST_CHECK(game::current.Star_Count() == previous.Star_Count());
    DOCTEST_CHECK(game::current.stardate == previous.stardate);
    DOCTEST_CHECK(game::current.combat_x == previous.combat_x);
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
    s_player players[3] = {};
    s_colony colonies[3] = {};
    test_planets::Resize_Dense(3);
    s_star_data stars[1] = {};
    int16_t player_count = 3;
    int16_t colony_count = 3;
    int16_t star_count = 1;
    stars[0].has_colony = 7;
    stars[0].wormhole_star_id = game::NO_STAR;
    test_stars::Set(0, stars[0]);
    for (uint32_t i = 0; i < 3; ++i) {
        players[i].ship_range = 6;
        colonies[i].owner = (int8_t)i;
        colonies[i].planet_id = SERIAL::Decode_Legacy_Planet_Reference((int16_t)i);
        game::current_state.colonies.insert_or_assign(game::id_colony{static_cast<uint32_t>(i)}, colonies[i]);
    }
    players[0].treaty[1] = 1;
    players[0].treaty[2] = 4;
    AIDATA::s_state ai;
    game::s_view world{
        .players = players,
        .state = &game::current_state,
        .player_count = &player_count,

    };
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
    int16_t player_count = 2;
    test_stars::Set(0, stars[0]);
    const game::s_view world{.state = &game::current_state, .player_count = &player_count};
    DOCTEST_CHECK(AIMOVE::Ship_Min_Strength_Versus_Players_At_Star(&ai, &world, ship_id, 2, Star_Id(0)) == 123);
    test_stars::Record(0).in_nebula = 1;
    DOCTEST_CHECK(AIMOVE::Ship_Min_Strength_Versus_Players_At_Star(&ai, &world, ship_id, 2, Star_Id(0)) == 277);
    DOCTEST_CHECK(ai.ships.size() == 2);
}

DOCTEST_TEST_CASE("range assessments use the supplied star keys, parents, and coordinates") {
    test::s_game_state fixture;
    game::s_game_state state;
    state.next_star_id = 90000;
    const auto home = *game::Create_Star(state, {.x = 0, .has_colony = 1});
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

    s_player player{};
    player.ship_range = 1;
    const int16_t player_count = 1;
    const game::s_view world{.players = &player, .state = &state, .player_count = &player_count};
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
    game::s_game_state state;
    state.next_star_id = 90000;
    const auto home = *game::Create_Star(state, {.has_colony = 1});
    const auto other = *game::Create_Star(state, {.x = 30, .has_colony = 1});
    for (auto& [key, star] : state.stars) star.officer_index[0] = -1;
    const auto planet = *game::Create_Planet(state, {.star_id = home});
    DOCTEST_REQUIRE(game::Create_Colony(state, {.owner = 0, .planet_id = planet, .n_pops = 4}));
    s_player player{};
    player.home_planet_id = planet;
    const int16_t count = 1;
    const game::s_view world{.players = &player, .state = &state, .player_count = &count};
    AIDATA::s_state ai;
    ai.stars[home].stage_weights[0] = 5;
    ai.stars[other].stage_weights[0] = 10;
    AISTAGE::Compute_Defensive_Stage_Points(&ai, &world, Player_Id(0));
    DOCTEST_CHECK((player.offensive_stagepoint == game::Star_Row(state, other)));
    DOCTEST_CHECK((state.stars.at(other).is_stagepoint & 1) != 0);
    DOCTEST_CHECK_FALSE(ai.stars.contains(Star_Id(0)));

    auto& leader = state.leaders[0];
    leader.type = LEADER_TYPE_COLONY;
    leader.status = 0;
    leader.special_skills = LEADER_COLONY_SKILL_FINANCIAL_LEADER;
    aileader::Assign_Star_Officer(&world, &ai, Player_Id(0), 0);
    DOCTEST_CHECK(leader.star_location == home);
    DOCTEST_CHECK(leader.eta == 0);
    DOCTEST_CHECK(state.stars.at(home).officer_index[0] == 0);
    DOCTEST_CHECK(state.stars.at(other).officer_index[0] == -1);
}

DOCTEST_TEST_CASE("colony proximity scores do not depend on star rows") {
    test::s_game_state fixture;
    game::s_game_state state;
    state.next_star_id = 90000;
    const auto home = *game::Create_Star(state, {.x = 0, .has_colony = 1});
    const auto near = *game::Create_Star(state, {.x = 60, .has_colony = 2});
    const auto distant = *game::Create_Star(state, {.x = 240, .has_colony = 2});
    const auto hole = *game::Create_Star(state, {.x = 30, .spectral_class = STAR_CLASS_BLACK_HOLE});
    const auto planet = *game::Create_Planet(state, {.star_id = home});
    s_colony colony{.owner = 0, .planet_id = planet};
    s_player players[2]{};
    players[0].ship_range = 2;
    s_settings settings{};
    settings.game_difficulty = GAME_DIFFICULTY_AVERAGE;
    const int16_t player_count = 2;
    const game::s_view world{.players = players, .state = &state, .player_count = &player_count, .settings = &settings};
    AIDATA::s_state ai;
    ai.stars[home].neighborhood.per_player[0].covering_colony_count = 1;
    ai.stars[near].neighborhood.per_player[0].covering_colony_count = 2;
    ai.stars[distant].neighborhood.per_player[0].covering_colony_count = 1;
    ai.stars[hole].neighborhood.per_player[0].covering_colony_count = 1;
    const auto check_scores = [&] {
        DOCTEST_CHECK(AIDATA::Proximity_Worth_To_Player(&ai, &world, &colony, &players[0]) == 140);
        DOCTEST_CHECK(AIDATA::Proximity_Worth_To_Player(&ai, &world, &colony, &players[1]) == 250);
    };
    check_scores();
    state.star_order = {hole, distant, near, home};
    state.star_order_sorted = false;
    check_scores();
}
