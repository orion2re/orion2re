#include "pch.h"
#include "../fixtures/colony_roster.h"

#include "doctest/doctest.h"

namespace TEST_TOWN_ZERO {
    void Reset_();
}

DOCTEST_TEST_CASE("zero-population colony defenses are valid bomb targets") {
    TEST_TOWN_ZERO::Reset_();

    s_colony* colony = &test_colonies::Record(0);
    colony->buildings[BUILDING_MISSILE_BASE] = 1;
    colony->buildings[BUILDING_ROBOTIC_FACTORY] = 1;

    s_combat_bomb_result result{.colony_id = game::id_colony{0}};
    result.attacker = factions::From_Legacy(0);

    DOCTEST_REQUIRE(BOMB::Resolve_Bomb_Hit(colony, &result, 1) == 1);
    DOCTEST_CHECK(colony->buildings[BUILDING_MISSILE_BASE] == 0);
    DOCTEST_CHECK(result.buildings_destroyed[BUILDING_MISSILE_BASE] == 1);
    DOCTEST_CHECK(result.colony_wiped_out == 0);
}

DOCTEST_TEST_CASE("bombing the final building destroys a zero-population colony") {
    TEST_TOWN_ZERO::Reset_();

    s_colony* colony = &test_colonies::Record(0);
    colony->buildings[BUILDING_ROBOTIC_FACTORY] = 1;

    s_combat_bomb_result result{.colony_id = game::id_colony{0}};
    result.attacker = factions::From_Legacy(0);

    DOCTEST_REQUIRE(BOMB::Resolve_Bomb_Hit(colony, &result, 1) == 1);
    DOCTEST_CHECK(result.damage_occurred == 1);
    DOCTEST_CHECK(result.buildings_destroyed[BUILDING_ROBOTIC_FACTORY] == 1);
    DOCTEST_CHECK(result.colony_wiped_out == 1);
    DOCTEST_CHECK((game::current_state.players[0].colony_war_losses[0] == 10));
}

DOCTEST_TEST_CASE("bombing records war losses only for player attackers") {
    for (int8_t attacker_idx = -1; attacker_idx <= NONPLAYER_SHIP_TYPE_HYDRA; ++attacker_idx) {
        for (int scenario = 0; scenario < 5; ++scenario) {
            DOCTEST_CAPTURE(attacker_idx);
            DOCTEST_CAPTURE(scenario);
            TEST_TOWN_ZERO::Reset_();

            s_colony* colony = &test_colonies::Record(0);
            s_player& owner = game::current_state.players[0];
            for (int i = 0; i < MAX_PLAYERS; ++i) {
                owner.colony_war_losses[i] = 20;
                owner.ship_war_losses[i] = 30;
                owner.biological_weapons_flag[i] = 1;
            }

            s_combat_bomb_result result{.colony_id = game::id_colony{0}};
            result.attacker = factions::From_Legacy(attacker_idx);
            const bool biological = scenario >= 3;
            const bool final_building = scenario == 2;
            const bool wipeout = scenario == 1 || scenario == 2 || scenario == 4;
            colony->n_pops = final_building ? 0 : (wipeout ? 1 : 2);
            colony->pop[0] = 0;
            colony->pop[1] = 0;
            colony->pop_roundoff[0] = 100;
            if (final_building) {
                colony->buildings[BUILDING_ROBOTIC_FACTORY] = 1;
            }

            if (biological) {
                BOMB::Resolve_Biological_Hit(colony, &result);
            } else {
                DOCTEST_REQUIRE(BOMB::Resolve_Bomb_Hit(colony, &result, 1) == 1);
            }

            DOCTEST_CHECK(result.damage_occurred == 1);
            DOCTEST_CHECK(result.colony_wiped_out == (wipeout ? 1 : 0));
            DOCTEST_CHECK(colony->n_pops == (wipeout ? 0 : 1));
            DOCTEST_CHECK((int)result.population_killed == (final_building ? 0 : 1));
            DOCTEST_CHECK((int)result.bio_pop_killed == (biological ? 1 : 0));
            if (final_building) {
                DOCTEST_CHECK(colony->buildings[BUILDING_ROBOTIC_FACTORY] == 0);
                DOCTEST_CHECK(result.buildings_destroyed[BUILDING_ROBOTIC_FACTORY] == 1);
            }
            for (int i = 0; i < MAX_PLAYERS; ++i) {
                const int expected_loss = i == attacker_idx ? (wipeout ? 10 : 1) : 0;
                DOCTEST_CHECK((int)owner.colony_war_losses[i] == 20 + expected_loss);
                DOCTEST_CHECK((int)owner.ship_war_losses[i] == 30);
                DOCTEST_CHECK(owner.biological_weapons_flag[i] == 1);
            }
        }
    }
}

DOCTEST_TEST_CASE("Capitol bombardment follows the configured vulnerability") {
    TEST_TOWN_ZERO::Reset_();

    s_colony* colony = &test_colonies::Record(0);
    colony->buildings[BUILDING_CAPITOL] = 1;
    s_combat_bomb_result result{.colony_id = game::id_colony{0}};
    result.attacker = factions::From_Legacy(0);

    BOMB::capitol_can_be_bombed = false;
    DOCTEST_CHECK(BOMB::Resolve_Bomb_Hit(colony, &result, 1) == 0);
    DOCTEST_CHECK(colony->buildings[BUILDING_CAPITOL] == 1);

    BOMB::capitol_can_be_bombed = true;
    DOCTEST_REQUIRE(BOMB::Resolve_Bomb_Hit(colony, &result, 1) == 1);
    DOCTEST_CHECK(colony->buildings[BUILDING_CAPITOL] == 0);
    DOCTEST_CHECK(result.buildings_destroyed[BUILDING_CAPITOL] == 1);
    DOCTEST_CHECK(result.colony_wiped_out == 1);

    BOMB::capitol_can_be_bombed = false;
}
