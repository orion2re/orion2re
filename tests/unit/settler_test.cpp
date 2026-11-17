#include "../fixtures/planet_state.h"
#include "pch.h"

#include "doctest/doctest.h"
#include "game/settler.h"

namespace TEST_SETTLER {
    void Reset_();
    void Set_Pop_Limit_(int32_t race_idx, int16_t pop_limit);
}

static void Add_Resident_Pops_(s_colony* colony, int32_t race_idx, int32_t pop_count) {
    for (int32_t i = 0; i < pop_count; ++i) {
        POP::Set_Race(&colony->pop[(int32_t)(uint8_t)colony->n_pops], (uint32_t)race_idx);
        colony->n_pops++;
    }
}

static void Add_Inbound_Settler_(s_player* player, int32_t race_idx, int16_t dest_planet) {
    SETTLER::value_type settler_data = 0;
    SETTLER::Set_Race(&settler_data, (uint32_t)race_idx);
    SETTLER::Set_Dest_Planet(&settler_data, dest_planet);
    player->settlers[player->settlers_freighted].val = settler_data;
    player->settlers_freighted++;
}

static void Prepare_Interstar_Move_() {
    TEST_SETTLER::Set_Pop_Limit_(0, 42);

    MOX::colony[0].owner = 0;
    MOX::colony[0].planet = 0;
    MOX::colony[0].n_pops = 2;
    MOX::colony[1].owner = 0;
    MOX::colony[1].planet = 1;
    test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).star_index = 0;
    test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(1))).star_index = 1;
    MOX::player[0].n_freighters = 10000;
}

DOCTEST_TEST_SUITE("settler population limits") {

DOCTEST_TEST_CASE("inbound settlers count only when scheduling another settler") {
    TEST_SETTLER::Reset_();
    TEST_SETTLER::Set_Pop_Limit_(0, 5);

    s_colony* colony = &MOX::colony[0];
    colony->owner = 0;
    colony->planet = 0;
    Add_Resident_Pops_(colony, 0, 4);
    Add_Inbound_Settler_(&MOX::player[0], 0, colony->planet);

    DOCTEST_CHECK(SETTLER::Room_For_Another(&::game::world, colony, 0, 0));
    DOCTEST_CHECK_FALSE(SETTLER::Room_For_Another(&::game::world, colony, 0, 1));
}

DOCTEST_TEST_CASE("settlers heading elsewhere do not consume destination capacity") {
    TEST_SETTLER::Reset_();
    TEST_SETTLER::Set_Pop_Limit_(0, 5);

    s_colony* colony = &MOX::colony[0];
    colony->owner = 0;
    colony->planet = 0;
    Add_Resident_Pops_(colony, 0, 4);
    Add_Inbound_Settler_(&MOX::player[0], 0, 1);

    DOCTEST_CHECK(SETTLER::Room_For_Another(&::game::world, colony, 0, 1));
}

DOCTEST_TEST_CASE("higher mixed-race limit blocks an additional lower-limit settler") {
    TEST_SETTLER::Reset_();
    TEST_SETTLER::Set_Pop_Limit_(0, 5);
    TEST_SETTLER::Set_Pop_Limit_(1, 10);

    s_colony* colony = &MOX::colony[0];
    colony->owner = 0;
    colony->planet = 0;
    Add_Resident_Pops_(colony, 0, 4);
    Add_Resident_Pops_(colony, 1, 6);

    DOCTEST_CHECK_FALSE(SETTLER::Room_For_Another(&::game::world, colony, 0, 1));
}

DOCTEST_TEST_CASE("absolute colony population limit remains 42") {
    TEST_SETTLER::Reset_();
    TEST_SETTLER::Set_Pop_Limit_(0, 50);

    s_colony* colony = &MOX::colony[0];
    colony->owner = 0;
    colony->planet = 0;
    Add_Resident_Pops_(colony, 0, 42);

    DOCTEST_CHECK_FALSE(SETTLER::Room_For_Another(&::game::world, colony, 0, 0));
}

DOCTEST_TEST_CASE("configured freight capacity may exceed the classic limit") {
    TEST_SETTLER::Reset_();
    Prepare_Interstar_Move_();
    SETTLER::max_pop_freighted = 26;
    MOX::player[0].settlers_freighted = 25;

    s_settler_settle_result result = {};
    SETTLER::Pop_Tries_To_Settle(&::game::world, 
            &MOX::colony[0], 0, &MOX::colony[1], &result);

    DOCTEST_CHECK(result.can_settle == 1);
    DOCTEST_CHECK(result.max_settlers_in_transit == 0);
}

DOCTEST_TEST_CASE("freight limit rejects counts at or above the configured maximum") {
    const uint16_t limits[] = {0, 2, 2};
    const int16_t counts[] = {0, 2, 3};

    for (int i = 0; i < 3; ++i) {
        TEST_SETTLER::Reset_();
        Prepare_Interstar_Move_();
        SETTLER::max_pop_freighted = limits[i];
        MOX::player[0].settlers_freighted = counts[i];

        s_settler_settle_result result = {};
        SETTLER::Pop_Tries_To_Settle(&::game::world, 
                &MOX::colony[0], 0, &MOX::colony[1], &result);

        DOCTEST_CHECK(result.can_settle == 0);
        DOCTEST_CHECK(result.max_settlers_in_transit == 1);
    }
}

DOCTEST_TEST_CASE("last population transfer follows the configured policy") {
    TEST_SETTLER::Reset_();
    TEST_SETTLER::Set_Pop_Limit_(0, 42);

    s_colony* source = &MOX::colony[0];
    source->owner = 0;
    source->planet = 0;
    source->n_pops = 1;
    POP::Set_Race(&source->pop[0], 0);
    s_colony* destination = &MOX::colony[1];
    destination->owner = 0;
    destination->planet = 1;
    test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).star_index = 0;
    test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(1))).star_index = 0;

    s_settler_settle_result result = {};
    SETTLER::Pop_Tries_To_Settle(&::game::world, source, 0, destination, &result);
    DOCTEST_CHECK(result.only_one_pop_left == 1);
    DOCTEST_CHECK(result.can_settle == 0);

    SETTLER::allow_last_population_transfer = true;
    SETTLER::Pop_Tries_To_Settle(&::game::world, source, 0, destination, &result);
    DOCTEST_REQUIRE(result.can_settle == 1);
    DOCTEST_CHECK(result.only_one_pop_left == 0);

    SETTLER::Settle_Pop(&::game::world, source, 0, destination);
    DOCTEST_CHECK(source->n_pops == 0);
    DOCTEST_CHECK(destination->n_pops == 1);

    SETTLER::allow_last_population_transfer = false;
}

}