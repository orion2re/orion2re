#include "pch.h"

#include "doctest/doctest.h"
#include "game/legacy_world.h"
#include "../fixtures/game_state.h"

using game::id_ship;
using legacy_world::Ship_Id;
using legacy_world::Star_Id;
using legacy_world::Player_Id;

DOCTEST_TEST_CASE("game-state fixtures bind borrowed views and restore nested bindings") {
    const game::s_view previous = game::current;
    s_player* previous_players = MOX::player;
    const int16_t previous_ship_count = MOX::NUM_SHIPS;
    {
        test::s_game_state outer;
        DOCTEST_CHECK(game::current.players == outer.players.data());
        DOCTEST_CHECK(game::current.ships == outer.ships.data());
        DOCTEST_CHECK(game::current.stars == outer.stars.data());
        DOCTEST_CHECK(game::current.colonies == outer.colonies.data());
        DOCTEST_CHECK(game::current.state == &game::world);
        DOCTEST_CHECK(game::current.state == &game::world);
        DOCTEST_CHECK(game::current.settings == &MOX::settings);
        MOX::NUM_SHIPS = 2;
        DOCTEST_CHECK(game::current.Ship_Count() == 2);
        {
            test::s_game_state inner;
            DOCTEST_CHECK(game::current.players == inner.players.data());
            DOCTEST_CHECK(game::current.ships == inner.ships.data());
            DOCTEST_CHECK(game::current.Ship_Count() == 0);
        }
        DOCTEST_CHECK(MOX::player == outer.players.data());
        DOCTEST_CHECK(game::current.players == outer.players.data());
        DOCTEST_CHECK(game::current.ships == outer.ships.data());
        DOCTEST_CHECK(game::current.Ship_Count() == 2);
    }
    DOCTEST_CHECK(MOX::player == previous_players);
    DOCTEST_CHECK(MOX::NUM_SHIPS == previous_ship_count);
    DOCTEST_CHECK(game::current.players == previous.players);
    DOCTEST_CHECK(game::current.ships == previous.ships);
    DOCTEST_CHECK(game::current.stars == previous.stars);
    DOCTEST_CHECK(game::current.colonies.state == previous.colonies.state);
    DOCTEST_CHECK(game::current.colonies.fixture == previous.colonies.fixture);
    DOCTEST_CHECK(game::current.state == previous.state);
    DOCTEST_CHECK(game::current.state == previous.state);
    DOCTEST_CHECK(game::current.settings == previous.settings);
    DOCTEST_CHECK(game::current.player_count == previous.player_count);
    DOCTEST_CHECK(game::current.ship_count == previous.ship_count);
    DOCTEST_CHECK(game::current.star_count == previous.star_count);
    DOCTEST_CHECK(game::current.colony_count == previous.colony_count);
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
    s_star_data stars[1] = {};
    int16_t player_count = 3;
    int16_t colony_count = 3;
    int16_t star_count = 1;
    stars[0].has_colony = 7;
    stars[0].wormhole_star_id = -1;
    for (uint32_t i = 0; i < 3; ++i) {
        players[i].ship_range = 6;
        colonies[i].owner = (int8_t)i;
        colonies[i].planet = game::id_planet{i};
        game::Create_Planet(&game::world, {});
    }
    players[0].treaty[1] = 1;
    players[0].treaty[2] = 4;
    AIDATA::s_state ai;
    game::s_view world{
        .players = players,
        .stars = stars,
        .colonies = colonies,
        .state = &game::world,
        .player_count = &player_count,
        .star_count = &star_count,
        .colony_count = &colony_count,
    };
    AIDATA::Compute_Star_Player_Range_Info(&::game::world, &ai, &world);
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
    const game::s_view world{.stars = stars, .player_count = &player_count};
    DOCTEST_CHECK(AIMOVE::Ship_Min_Strength_Versus_Players_At_Star(&ai, &world, ship_id, 2, Star_Id(0)) == 123);
    stars[0].in_nebula = 1;
    DOCTEST_CHECK(AIMOVE::Ship_Min_Strength_Versus_Players_At_Star(&ai, &world, ship_id, 2, Star_Id(0)) == 277);
    DOCTEST_CHECK(ai.ships.size() == 2);
}
