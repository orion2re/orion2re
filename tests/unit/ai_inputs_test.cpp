#include "pch.h"

#include "doctest/doctest.h"
#include "game/ai_legacy.h"
#include "game/fleet_movement.h"

namespace {
    s_ship_data legacy_ships[2];
    s_star_data legacy_stars[2];
    std::vector<int16_t> queried_ships;
    std::vector<int16_t> queried_locations;
    int16_t queried_destination;
    bool queried;
} // namespace

namespace MOX {
    s_ship_data* ship = legacy_ships;
    s_star_data* star = legacy_stars;
    int16_t first_field_in_group[10] = {0, 18, 55, 57, 29, 7, 22, 28, 10, 74};
} // namespace MOX

namespace SHIPMOVE {
    void __cdecl Ships_Try_To_Move_To(int16_t* ships, int16_t destination, s_ship_move_info* result) {
        queried = true;
        queried_ships.clear();
        queried_locations.clear();
        queried_destination = destination;
        for (int32_t i = 0; ships[i] != -1; ++i) {
            queried_ships.push_back(ships[i]);
            queried_locations.push_back((int16_t)MOX::ship[ships[i]].location);
        }
        *result = {};
        result->moving = 1;
        result->turns_left = 3;
    }

    void __cdecl Make_Ships_Move_To(int16_t* ships, int16_t destination, s_ship_move_info* result) {
        Ships_Try_To_Move_To(ships, destination, result);
    }
} // namespace SHIPMOVE

namespace GEO {
    int32_t __cdecl Parsecs_Between_Stars(int16_t source, int16_t destination) {
        return source == destination ? 0 : 1;
    }
} // namespace GEO

namespace struct_ {
    int16_t __cdecl Test_Bit_Field(void* bits, uint16_t index) {
        return (static_cast<const uint8_t*>(bits)[index / 8] >> (index % 8)) & 1;
    }
} // namespace struct_

namespace INITSHIP {
    int16_t __cdecl Best_Armor(int16_t) {
        DOCTEST_FAIL("Unexpected legacy combat calculation");
        return 0;
    }
    int16_t __cdecl Best_Computer(int16_t) {
        DOCTEST_FAIL("Unexpected legacy combat calculation");
        return 0;
    }
    int16_t __cdecl Best_Warp_Drive(int16_t) {
        DOCTEST_FAIL("Unexpected legacy combat calculation");
        return 0;
    }
    int16_t __cdecl Best_Bomb_Weapon(int16_t) {
        DOCTEST_FAIL("Unexpected legacy combat calculation");
        return 0;
    }
    int16_t __cdecl Best_Ship_Shield(int16_t) {
        DOCTEST_FAIL("Unexpected legacy combat calculation");
        return 0;
    }
    int16_t __cdecl Best_Point_Defense_Weapon(int16_t) {
        DOCTEST_FAIL("Unexpected legacy combat calculation");
        return 0;
    }
    int16_t __cdecl Get_Ship_Structure(int16_t) {
        DOCTEST_FAIL("Unexpected legacy combat calculation");
        return 0;
    }
    int16_t __cdecl Get_Ship_Armor_Hits(int16_t) {
        DOCTEST_FAIL("Unexpected legacy combat calculation");
        return 0;
    }
    void __cdecl Update_Strategic_Ship(int16_t) {
        DOCTEST_FAIL("Unexpected legacy combat calculation");
    }
    void __cdecl Get_Ship_Combat_Bonuses(int16_t, int16_t*, int16_t*, int16_t*) {
        DOCTEST_FAIL("Unexpected legacy combat calculation");
    }
} // namespace INITSHIP

DOCTEST_TEST_CASE("technology defaults bind to the test-owned starting fields") {
    DOCTEST_CHECK(&TECHDATA::starting_techfield.biology == &MOX::first_field_in_group[1]);
    DOCTEST_CHECK(&TECHDATA::starting_techfield.xenon == &MOX::first_field_in_group[9]);
}

DOCTEST_TEST_CASE("AI turn reset does not own or reset world inputs") {
    int16_t player_count = 1;
    uint32_t seed = 42;
    game_world::s_view world;
    AIDATA::s_state first;
    AIDATA::s_state second;
    world.player_count = &player_count;
    world.random = {&seed, game_random::Random_From_Seed};
    first.players[0].build.transports.current = 3;
    first.ships[UINT32_MAX].refit_desirability = 9;

    AIDATA::Begin_Turn(&first, 42);
    DOCTEST_CHECK(first.ships.empty());
    DOCTEST_CHECK(first.players[0].build.transports.current == 0);
    DOCTEST_CHECK(world.random.seed == &seed);
    player_count = 2;
    DOCTEST_CHECK(world.Player_Count() == 2);
    DOCTEST_CHECK_FALSE(second.initialized);
}

DOCTEST_TEST_CASE("AI random sources preserve classic draws without selecting a global stream") {
    game_random::random_seed = 0xdeadbeef;
    uint32_t seed = 0;
    const game_random::s_source source{&seed, game_random::Random_From_Seed};
    const uint32_t expected[] = {1, 828, 653, 838, 54, 759};
    DOCTEST_CHECK(source.Next(0) == 1);
    DOCTEST_CHECK(seed == 0);
    for (uint32_t value : expected) DOCTEST_CHECK(source.Next(1000) == value);
    DOCTEST_CHECK(game_random::random_seed == 0xdeadbeef);

    uint32_t other_seed = 0;
    const game_random::s_source other{&other_seed, game_random::Random_From_Seed};
    DOCTEST_CHECK(other.Next(1000) == 1);
    DOCTEST_CHECK(seed == 3256818826u);
}

DOCTEST_TEST_CASE("source-driven weighted choices have explicit absence and fixed-width positions") {
    uint32_t seed = 0;
    const game_random::s_source random{&seed, game_random::Random_From_Seed};
    const uint16_t zero[] = {0, 0};
    DOCTEST_CHECK_FALSE(RUSS::Choose_Weighted_Index_U16(&random, zero, 2));
    DOCTEST_CHECK(seed == 0);

    const uint16_t weights[] = {0, 5, 0, 7};
    DOCTEST_CHECK(RUSS::Choose_Weighted_Index_U16(&random, weights, 4) == 1);
    DOCTEST_CHECK(RUSS::Choose_Weighted_Index_U16(&random, weights, 4) == 3);

    std::vector<uint16_t> large(40000);
    large.back() = 1;
    DOCTEST_CHECK(RUSS::Choose_Weighted_Index_U16(&random, large.data(), 40000) == 39999);
}

DOCTEST_TEST_CASE("32-bit ID shuffling keeps the legacy permutation and RNG consumption") {
    uint32_t first_seed = 42;
    uint32_t second_seed = 42;
    const game_random::s_source first{&first_seed, game_random::Random_From_Seed};
    const game_random::s_source second{&second_seed, game_random::Random_From_Seed};
    int16_t old_ids[] = {0, 1, 2, 3, 4, 5};
    uint32_t ids[] = {0, 1, 2, 3, 4, 5};
    const uint32_t expected[] = {4, 0, 1, 5, 3, 2};
    RUSS::Shuffle_Sint_With_Source(&first, old_ids, 6);
    RUSS::Shuffle_Uint(&second, ids, 6);
    for (uint32_t i = 0; i < 6; ++i) {
        DOCTEST_CHECK(ids[i] == expected[i]);
        DOCTEST_CHECK(ids[i] == (uint32_t)old_ids[i]);
    }
    DOCTEST_CHECK(first_seed == second_seed);
    DOCTEST_CHECK(second_seed == 2219150180u);
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
    s_player players[3] = {};
    s_colony colonies[3] = {};
    s_planet_data planets[3] = {};
    s_star_data stars[1] = {};
    int16_t player_count = 3;
    int16_t colony_count = 3;
    int16_t star_count = 1;
    stars[0].has_colony = 7;
    stars[0].wormhole_star_id = -1;
    for (uint32_t i = 0; i < 3; ++i) {
        players[i].ship_range = 6;
        colonies[i].owner = (int8_t)i;
        colonies[i].planet = (int16_t)i;
    }
    players[0].treaty[1] = 1;
    players[0].treaty[2] = 4;
    AIDATA::s_state ai;
    game_world::s_view world{
        .players = players,
        .stars = stars,
        .colonies = colonies,
        .planets = planets,
        .player_count = &player_count,
        .star_count = &star_count,
        .colony_count = &colony_count,
    };
    AIDATA::Compute_Star_Player_Range_Info(&ai, &world);
    const auto& assessment = ai.stars.at(0).neighborhood.per_player[0];
    DOCTEST_CHECK(assessment.treaty_partners_in_range == 1);
    DOCTEST_CHECK(assessment.hostile_players_in_range == 1);
    DOCTEST_CHECK(assessment.neutral_players_in_range == 0);
    DOCTEST_CHECK(assessment.covering_colony_count == 1);
}

DOCTEST_TEST_CASE("legacy ship-list encoding accepts zero, rejects narrowing, and owns its terminator") {
    const uint32_t ids[] = {0, INT16_MAX};
    auto encoded = AI_LEGACY::Ship_List(ids);
    DOCTEST_REQUIRE(encoded);
    DOCTEST_CHECK(*encoded == std::vector<int16_t>{0, INT16_MAX, -1});
    const uint32_t invalid[] = {UINT32_MAX};
    DOCTEST_CHECK_FALSE(AI_LEGACY::Ship_List(invalid));
    auto empty = AI_LEGACY::Ship_List({});
    DOCTEST_REQUIRE(empty);
    DOCTEST_CHECK(*empty == std::vector<int16_t>{-1});
}

DOCTEST_TEST_CASE("AI strength lookup does not narrow a ship identity to a legacy index") {
    AIDATA::s_state ai;
    constexpr uint32_t ship_id = 0x12345678u;
    ai.ships[ship_id].strengths[1] = 123;
    ai.ships[ship_id].strengths_in_nebula[1] = 277;
    ai.ships[0].strengths[1] = 7;
    s_star_data stars[1] = {};
    int16_t player_count = 2;
    const game_world::s_view world{.stars = stars, .player_count = &player_count};
    DOCTEST_CHECK(AIMOVE::Ship_Min_Strength_Versus_Players_At_Star(&ai, &world, ship_id, 2, 0) == 123);
    stars[0].in_nebula = 1;
    DOCTEST_CHECK(AIMOVE::Ship_Min_Strength_Versus_Players_At_Star(&ai, &world, ship_id, 2, 0) == 277);
    DOCTEST_CHECK(ai.ships.size() == 2);
}

DOCTEST_TEST_CASE("read-only bit fields have a distinct const-correct interface") {
    const uint8_t bits[] = {1, 0x80};
    DOCTEST_CHECK(struct_::Read_Bit_Field(bits, 0) == 1);
    DOCTEST_CHECK(struct_::Read_Bit_Field(bits, 1) == 0);
    DOCTEST_CHECK(struct_::Read_Bit_Field(bits, 15) == 1);
}

DOCTEST_TEST_CASE("legacy movement queries restore positions including repeated ship IDs") {
    memset(legacy_ships, 0, sizeof(legacy_ships));
    legacy_ships[0].x = legacy_ships[1].x = 10;
    legacy_ships[0].y = legacy_ships[1].y = 20;
    legacy_stars[1].x = 100;
    legacy_stars[1].y = 200;
    const uint32_t ids[] = {0, 0, 1};
    s_ship_move_info result{};
    SHIPMOVE::Evaluate_Fleet_Move(ids, 0, &result, 1);
    DOCTEST_CHECK(queried_ships == std::vector<int16_t>{0, 0, 1});
    DOCTEST_CHECK(queried_locations == std::vector<int16_t>{1, 1, 1});
    DOCTEST_CHECK(queried_destination == 0);
    for (const auto& ship : legacy_ships) {
        DOCTEST_CHECK((int16_t)ship.location == 0);
        DOCTEST_CHECK((int16_t)ship.x == 10);
        DOCTEST_CHECK((int16_t)ship.y == 20);
    }
    queried = false;
    const uint32_t invalid[] = {UINT32_MAX};
    SHIPMOVE::Evaluate_Fleet_Move(invalid, 0, &result);
    DOCTEST_CHECK_FALSE(queried);
    DOCTEST_CHECK(result.moving == 0);
    queried = false;
    SHIPMOVE::Evaluate_Fleet_Move({}, 0, &result);
    DOCTEST_CHECK_FALSE(queried);
}

DOCTEST_TEST_CASE("movement planning rejects destinations unsupported by the legacy engine") {
    s_ship_data ships[1] = {};
    game_world::s_view world;
    world.ships = ships;
    AIMOVE::s_ai_movement_context context{0, nullptr, nullptr, &world};
    const uint32_t ids[] = {0};
    queried = false;
    auto route = AIMOVE::Move_Ships_With_Possible_Intermediate(&context, ids, UINT32_MAX);
    DOCTEST_CHECK_FALSE(route);
    DOCTEST_CHECK_FALSE(queried);
}
