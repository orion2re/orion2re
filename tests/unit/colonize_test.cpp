#include "pch.h"

#include "doctest/doctest.h"
#include "game/colonize.h"

// Game records are packed, so their members may be misaligned. Assertions
// on them use double parentheses so doctest compares the values directly
// instead of binding references to the packed members.

namespace TEST_COLONIZE {
    void Reset_();
}

DOCTEST_TEST_CASE("same-owner outpost ship does not suppress colony ship priority") {
    TEST_COLONIZE::Reset_();

    MOX::star[0].planet_index[0] = 0;
    MOX::planet[0].star_index = 0;
    MOX::planet[0].planet_type = PLANET_TYPE_PLANET;
    MOX::planet[0].colony_index = 0;
    MOX::colony[0].owner = 0;
    MOX::colony[0].planet = 0;
    MOX::colony[0].outpost_flag = 1;

    MOX::ship[0].owner = 0;
    MOX::ship[0].location = 0;
    MOX::ship[0].d.ship_type = SHIP_TYPE_OUTPOST;
    MOX::ship[1].owner = 0;
    MOX::ship[1].location = 0;
    MOX::ship[1].d.ship_type = SHIP_TYPE_COLONY;

    COLONIZE::Find_Colonize_Players();

    DOCTEST_CHECK((MOX::star[0].colonize_player == 0));
}

DOCTEST_TEST_CASE("same-owner outpost ship does not suppress colony base priority") {
    TEST_COLONIZE::Reset_();

    MOX::NUM_COLONIES = 2;
    MOX::NUM_SHIPS = 1;

    MOX::star[0].planet_index[0] = 0;
    MOX::star[0].planet_index[1] = 1;

    MOX::planet[0].star_index = 0;
    MOX::planet[0].planet_type = PLANET_TYPE_PLANET;
    MOX::planet[0].colony_index = 0;
    MOX::colony[0].owner = 0;
    MOX::colony[0].planet = 0;
    MOX::colony[0].buildings[BUILDING_COLONY_BASE] = 1;

    MOX::planet[1].star_index = 0;
    MOX::planet[1].planet_type = PLANET_TYPE_PLANET;
    MOX::planet[1].colony_index = 1;
    MOX::colony[1].owner = 0;
    MOX::colony[1].planet = 1;
    MOX::colony[1].outpost_flag = 1;

    MOX::ship[0].owner = 0;
    MOX::ship[0].location = 0;
    MOX::ship[0].d.ship_type = SHIP_TYPE_OUTPOST;

    TECHDATA::buildings[BUILDING_COLONY_BASE].cost = 100;

    COLONIZE::Find_Colonize_Players();

    DOCTEST_CHECK((MOX::star[0].colonize_player == 0));
    DOCTEST_CHECK((MOX::colony[0].buildings[BUILDING_COLONY_BASE] == 1));
    DOCTEST_CHECK((MOX::player[0].bc == 0));
}

DOCTEST_TEST_CASE("restoring colony reservations preserves saved claims and does not process colony bases") {
    TEST_COLONIZE::Reset_();
    MOX::NUM_PLAYERS = 2;
    MOX::NUM_COLONIES = 2;
    MOX::NUM_SHIPS = 0;
    for (int16_t i = 0; i < MAX_COLONIES; ++i) {
        MOX::colony[i].allocated_to = -1;
    }

    MOX::star[0].colonize_player = 1;
    MOX::star[0].planet_index[0] = 0;
    MOX::star[0].planet_index[1] = 1;
    MOX::star[0].planet_index[2] = 2;
    MOX::planet[0].colony_index = -1;
    MOX::planet[1].colony_index = -1;
    MOX::planet[2].colony_index = 1;
    MOX::colony[0].planet = -1;
    MOX::colony[0].allocated_to = 1;
    MOX::colony[1].planet = 2;
    MOX::colony[1].buildings[BUILDING_COLONY_BASE] = 1;
    MOX::colony[2].allocated_to = 0;
    TECHDATA::buildings[BUILDING_COLONY_BASE].cost = 100;

    COLONIZE::Restore_Colony_Slot_Reservations();

    DOCTEST_CHECK((MOX::star[0].colonize_player == 1));
    DOCTEST_CHECK((MOX::colony[0].allocated_to == 1));
    DOCTEST_CHECK((MOX::colony[1].buildings[BUILDING_COLONY_BASE] == 1));
    DOCTEST_CHECK((MOX::colony[2].allocated_to == 0));
    DOCTEST_CHECK((MOX::colony[3].allocated_to == 1));
    DOCTEST_CHECK((MOX::colony[4].allocated_to == -1));
    DOCTEST_CHECK((MOX::player[0].bc == 0));
}

DOCTEST_TEST_CASE("outpost on non-planet body uses barren climate") {
    TEST_COLONIZE::Reset_();

    MOX::planet[0].star_index = 0;
    MOX::planet[0].planet_type = PLANET_TYPE_ASTEROID;
    MOX::planet[0].climate = PLANET_CLIMATE_GAIA;

    COLONIZE::Make_New_Outpost(0, 0);

    DOCTEST_CHECK((MOX::colony[0].climate == PLANET_CLIMATE_BARREN));
}

DOCTEST_TEST_CASE("outpost on planet preserves planet climate") {
    TEST_COLONIZE::Reset_();

    MOX::planet[0].star_index = 0;
    MOX::planet[0].planet_type = PLANET_TYPE_PLANET;
    MOX::planet[0].climate = PLANET_CLIMATE_OCEAN;

    COLONIZE::Make_New_Outpost(0, 0);

    DOCTEST_CHECK((MOX::colony[0].climate == PLANET_CLIMATE_OCEAN));
}

DOCTEST_TEST_CASE("first colony after losing the homeworld becomes the new home") {
    TEST_COLONIZE::Reset_();
    MOX::player[0].home_planet_id = game::NO_PLANET;
    MOX::planet[0].star_index = 0;

    COLONIZE::Make_New_Colony(0, 0);

    DOCTEST_CHECK(MOX::player[0].home_planet_id.get() == game::id_planet{0});
}

DOCTEST_TEST_CASE("new outpost does not become the homeworld") {
    TEST_COLONIZE::Reset_();
    MOX::player[0].home_planet_id = game::NO_PLANET;
    MOX::planet[0].star_index = 0;

    COLONIZE::Make_New_Outpost(0, 0);

    DOCTEST_CHECK(MOX::player[0].home_planet_id.get() == game::NO_PLANET);
}

DOCTEST_TEST_CASE("first colony population uses the configured worker rule") {
    TEST_COLONIZE::Reset_();

    MOX::planet[0].star_index = 0;
    MOX::planet[0].food_per_farmer = 2;

    COLONIZE::Make_New_Colony(0, 0);

    DOCTEST_CHECK((POP::Get_Prof(MOX::colony[0].pop[0]) == POP::PROF_FARMER));

    TEST_COLONIZE::Reset_();
    MOX::planet[0].star_index = 0;
    MOX::planet[0].food_per_farmer = 2;
    COLONIZE::first_pop_to_production = 1;

    COLONIZE::Make_New_Colony(0, 0);

    DOCTEST_CHECK((POP::Get_Prof(MOX::colony[0].pop[0]) == POP::PROF_WORKER));
}

DOCTEST_TEST_CASE("new colony populations use semantic identity and assignment fields") {
    TEST_COLONIZE::Reset_();
    constexpr int16_t player_idx = 3;
    MOX::colony[0].allocated_to = player_idx;
    MOX::planet[0].star_index = 0;
    MOX::planet[0].food_per_farmer = 2;
    MOX::planet[0].planet_special = SYSTEM_SPECIAL_NATIVES;

    COLONIZE::Make_New_Colony(0, player_idx);

    DOCTEST_CHECK((POP::Get_Race(MOX::colony[0].pop[0]) == player_idx));
    DOCTEST_CHECK((POP::Get_Original_Owner(MOX::colony[0].pop[0]) == player_idx));
    DOCTEST_CHECK((POP::Is_Assigned(MOX::colony[0].pop[0])));
    DOCTEST_CHECK((POP::Get_Prof(MOX::colony[0].pop[0]) == POP::PROF_FARMER));
    for (int index = 1; index < 4; ++index) {
        DOCTEST_CHECK((POP::Get_Race(MOX::colony[0].pop[index]) == POP::RACE_NATIVE));
        DOCTEST_CHECK((POP::Get_Original_Owner(MOX::colony[0].pop[index]) == player_idx));
        DOCTEST_CHECK((POP::Is_Assigned(MOX::colony[0].pop[index])));
        DOCTEST_CHECK((POP::Get_Prof(MOX::colony[0].pop[index]) == POP::PROF_FARMER));
    }
}