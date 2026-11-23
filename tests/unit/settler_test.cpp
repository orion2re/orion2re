#include "pch.h"
#include "../fixtures/colony_roster.h"
#include "../fixtures/planet_roster.h"

#include "doctest/doctest.h"
#include "game/settler.h"
#include "game/legacy_ids.h"

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
    player->settlers[player->settlers_freighted] = {
        .destination = game::id_planet{static_cast<uint32_t>(dest_planet)},
        .race = static_cast<uint8_t>(race_idx),
    };
    player->settlers_freighted++;
}

static void Prepare_Interstar_Move_() {
    TEST_SETTLER::Set_Pop_Limit_(0, 42);

    test_colonies::Record(0).owner = legacy_world::Decode_Player_Slot(0);
    test_colonies::Record(0).planet_id = game::id_planet{0};
    test_colonies::Record(0).n_pops = 2;
    test_colonies::Record(1).owner = legacy_world::Decode_Player_Slot(0);
    test_colonies::Record(1).planet_id = game::id_planet{1};
    test_planets::Record(0).star_id = game::id_star{static_cast<uint32_t>(0)};
    test_planets::Record(1).star_id = game::id_star{static_cast<uint32_t>(1)};
    game::current_state.players[0].n_freighters = 10000;
}

DOCTEST_TEST_SUITE("settler population limits") {

DOCTEST_TEST_CASE("inbound settlers count only when scheduling another settler") {
    TEST_SETTLER::Reset_();
    TEST_SETTLER::Set_Pop_Limit_(0, 5);

    s_colony* colony = &test_colonies::Record(0);
    colony->owner = legacy_world::Decode_Player_Slot(0);
    colony->planet_id = game::id_planet{0};
    Add_Resident_Pops_(colony, 0, 4);
    Add_Inbound_Settler_(&game::current_state.players[0], 0, SERIAL::Encode_Legacy_Planet_Reference(colony->planet_id));

    DOCTEST_CHECK(SETTLER::Room_For_Another(colony, 0, 0));
    DOCTEST_CHECK_FALSE(SETTLER::Room_For_Another(colony, 0, 1));
}

DOCTEST_TEST_CASE("settlers heading elsewhere do not consume destination capacity") {
    TEST_SETTLER::Reset_();
    TEST_SETTLER::Set_Pop_Limit_(0, 5);

    s_colony* colony = &test_colonies::Record(0);
    colony->owner = legacy_world::Decode_Player_Slot(0);
    colony->planet_id = game::id_planet{0};
    Add_Resident_Pops_(colony, 0, 4);
    Add_Inbound_Settler_(&game::current_state.players[0], 0, 1);

    DOCTEST_CHECK(SETTLER::Room_For_Another(colony, 0, 1));
}

DOCTEST_TEST_CASE("arrivals resolve existing planets and discard missing destinations") {
    TEST_SETTLER::Reset_();
    TEST_SETTLER::Set_Pop_Limit_(0, 5);
    auto& player = game::current_state.players[0];
    test_colonies::Record(0).owner = legacy_world::Decode_Player_Slot(0);
    test_planets::Record(0).colony_id = game::id_colony{0};
    Add_Inbound_Settler_(&player, 0, static_cast<int16_t>(game::current_state.planets.size()));
    Add_Inbound_Settler_(&player, 0, 0);
    player.settlers[0].eta = 1;
    player.settlers[1].eta = 1;

    SETTLER::Move_Settlers();

    DOCTEST_CHECK(player.settlers_freighted == 0);
    DOCTEST_CHECK(test_colonies::Record(0).n_pops == 1);
}

DOCTEST_TEST_CASE("higher mixed-race limit blocks an additional lower-limit settler") {
    TEST_SETTLER::Reset_();
    TEST_SETTLER::Set_Pop_Limit_(0, 5);
    TEST_SETTLER::Set_Pop_Limit_(1, 10);

    s_colony* colony = &test_colonies::Record(0);
    colony->owner = legacy_world::Decode_Player_Slot(0);
    colony->planet_id = game::id_planet{0};
    Add_Resident_Pops_(colony, 0, 4);
    Add_Resident_Pops_(colony, 1, 6);

    DOCTEST_CHECK_FALSE(SETTLER::Room_For_Another(colony, 0, 1));
}

DOCTEST_TEST_CASE("absolute colony population limit remains 42") {
    TEST_SETTLER::Reset_();
    TEST_SETTLER::Set_Pop_Limit_(0, 50);

    s_colony* colony = &test_colonies::Record(0);
    colony->owner = legacy_world::Decode_Player_Slot(0);
    colony->planet_id = game::id_planet{0};
    Add_Resident_Pops_(colony, 0, 42);

    DOCTEST_CHECK_FALSE(SETTLER::Room_For_Another(colony, 0, 0));
}

DOCTEST_TEST_CASE("configured freight capacity may exceed the classic limit") {
    TEST_SETTLER::Reset_();
    Prepare_Interstar_Move_();
    SETTLER::max_pop_freighted = 26;
    game::current_state.players[0].settlers_freighted = 25;

    s_settler_settle_result result = {};
    SETTLER::Pop_Tries_To_Settle(
            game::current_state, game::id_colony{0}, 0, game::id_colony{1}, &result);

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
        game::current_state.players[0].settlers_freighted = counts[i];

        s_settler_settle_result result = {};
        SETTLER::Pop_Tries_To_Settle(
                game::current_state, game::id_colony{0}, 0, game::id_colony{1}, &result);

        DOCTEST_CHECK(result.can_settle == 0);
        DOCTEST_CHECK(result.max_settlers_in_transit == 1);
    }
}

DOCTEST_TEST_CASE("last population transfer follows the configured policy") {
    TEST_SETTLER::Reset_();
    TEST_SETTLER::Set_Pop_Limit_(0, 42);

    s_colony* source = &test_colonies::Record(0);
    source->owner = legacy_world::Decode_Player_Slot(0);
    source->planet_id = game::id_planet{0};
    source->n_pops = 1;
    POP::Set_Race(&source->pop[0], 0);
    s_colony* destination = &test_colonies::Record(1);
    destination->owner = legacy_world::Decode_Player_Slot(0);
    destination->planet_id = game::id_planet{1};
    test_planets::Record(0).star_id = game::id_star{static_cast<uint32_t>(0)};
    test_planets::Record(1).star_id = game::id_star{static_cast<uint32_t>(0)};

    s_settler_settle_result result = {};
    SETTLER::Pop_Tries_To_Settle(game::current_state, game::id_colony{0}, 0, game::id_colony{1}, &result);
    DOCTEST_CHECK(result.only_one_pop_left == 1);
    DOCTEST_CHECK(result.can_settle == 0);

    SETTLER::allow_last_population_transfer = true;
    SETTLER::Pop_Tries_To_Settle(game::current_state, game::id_colony{0}, 0, game::id_colony{1}, &result);
    DOCTEST_REQUIRE(result.can_settle == 1);
    DOCTEST_CHECK(result.only_one_pop_left == 0);

    SETTLER::Settle_Pop(game::id_colony{0}, 0, game::id_colony{1});
    DOCTEST_CHECK(source->n_pops == 0);
    DOCTEST_CHECK(destination->n_pops == 1);

    SETTLER::allow_last_population_transfer = false;
}

}
