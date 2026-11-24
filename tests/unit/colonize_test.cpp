#include "pch.h"
#include "../fixtures/ship_rows.h"
#include "game/legacy_ship_location_codec.h"
#include "../fixtures/star_roster.h"
#include "../fixtures/colony_roster.h"
#include "../fixtures/planet_roster.h"

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

    test_stars::Record(0).orbit_planet_ids[0] = game::id_planet{0};
    test_planets::Record(0).star_id = game::id_star{static_cast<uint32_t>(0)};
    test_planets::Record(0).planet_type = PLANET_TYPE_PLANET;
    test_planets::Record(0).colony_id = game::id_colony{0};
    test_colonies::Record(0).owner = legacy_world::Decode_Player_Slot(0);
    test_colonies::Record(0).planet_id = game::id_planet{0};
    test_colonies::Record(0).outpost_flag = 1;

    test_ships::Rows(game::current_state)[0].owner = factions::From_Legacy(0);
    test_ships::Rows(game::current_state)[0].position.Set(legacy_world::Location_From_Slot(game::e_ship_location::at_star, 0, -1));
    test_ships::Rows(game::current_state)[0].d.ship_type = SHIP_TYPE_OUTPOST;
    test_ships::Rows(game::current_state)[1].owner = factions::From_Legacy(0);
    test_ships::Rows(game::current_state)[1].position.Set(legacy_world::Location_From_Slot(game::e_ship_location::at_star, 0, -1));
    test_ships::Rows(game::current_state)[1].d.ship_type = SHIP_TYPE_COLONY;

    COLONIZE::Find_Colonize_Players();

    DOCTEST_CHECK((factions::Legacy_Code(test_stars::Record(0).colonize_player) == 0));
}

DOCTEST_TEST_CASE("same-owner outpost ship does not suppress colony base priority") {
    TEST_COLONIZE::Reset_();

    ship_slots::Resize(game::current_state, 1);

    test_stars::Record(0).orbit_planet_ids[0] = game::id_planet{0};
    test_stars::Record(0).orbit_planet_ids[1] = game::id_planet{1};

    test_planets::Record(0).star_id = game::id_star{static_cast<uint32_t>(0)};
    test_planets::Record(0).planet_type = PLANET_TYPE_PLANET;
    test_planets::Record(0).colony_id = game::id_colony{0};
    test_colonies::Record(0).owner = legacy_world::Decode_Player_Slot(0);
    test_colonies::Record(0).planet_id = game::id_planet{0};
    test_colonies::Record(0).buildings[BUILDING_COLONY_BASE] = 1;

    test_planets::Record(1).star_id = game::id_star{static_cast<uint32_t>(0)};
    test_planets::Record(1).planet_type = PLANET_TYPE_PLANET;
    test_planets::Record(1).colony_id = game::id_colony{1};
    test_colonies::Record(1).owner = legacy_world::Decode_Player_Slot(0);
    test_colonies::Record(1).planet_id = game::id_planet{1};
    test_colonies::Record(1).outpost_flag = 1;

    test_ships::Rows(game::current_state)[0].owner = factions::From_Legacy(0);
    test_ships::Rows(game::current_state)[0].position.Set(legacy_world::Location_From_Slot(game::e_ship_location::at_star, 0, -1));
    test_ships::Rows(game::current_state)[0].d.ship_type = SHIP_TYPE_OUTPOST;

    TECHDATA::buildings[BUILDING_COLONY_BASE].cost = 100;

    COLONIZE::Find_Colonize_Players();

    DOCTEST_CHECK((factions::Legacy_Code(test_stars::Record(0).colonize_player) == 0));
    DOCTEST_CHECK((test_colonies::Record(0).buildings[BUILDING_COLONY_BASE] == 1));
    DOCTEST_CHECK((game::current_state.players[0].bc == 0));
}

DOCTEST_TEST_CASE("outpost on non-planet body uses barren climate") {
    TEST_COLONIZE::Reset_();

    test_planets::Record(0).star_id = game::id_star{static_cast<uint32_t>(0)};
    test_planets::Record(0).planet_type = PLANET_TYPE_ASTEROID;
    test_planets::Record(0).climate = PLANET_CLIMATE_GAIA;

    COLONIZE::Make_New_Outpost(game::id_planet{0}, game::id_player{0});

    DOCTEST_CHECK((test_colonies::Record(0).climate == PLANET_CLIMATE_BARREN));
}

DOCTEST_TEST_CASE("outpost on planet preserves planet climate") {
    TEST_COLONIZE::Reset_();

    test_planets::Record(0).star_id = game::id_star{static_cast<uint32_t>(0)};
    test_planets::Record(0).planet_type = PLANET_TYPE_PLANET;
    test_planets::Record(0).climate = PLANET_CLIMATE_OCEAN;

    COLONIZE::Make_New_Outpost(game::id_planet{0}, game::id_player{0});

    DOCTEST_CHECK((test_colonies::Record(0).climate == PLANET_CLIMATE_OCEAN));
}

DOCTEST_TEST_CASE("outpost founding consumes a reserved key below the high-water mark") {
    TEST_COLONIZE::Reset_();
    auto& state = game::current_state;
    constexpr game::id_planet site{0};
    constexpr game::id_colony reserved{79998};
    test_planets::Record(site.id).star_id = game::id_star{static_cast<uint32_t>(0)};
    state.next_colony_id = 80001;
    state.colony_reservations[0] = {reserved, game::id_colony{80002}};

    DOCTEST_REQUIRE(COLONIZE::Make_New_Outpost(site, game::id_player{0}));
    DOCTEST_CHECK(state.planets.at(site).colony_id == reserved);
    DOCTEST_CHECK(state.colonies.at(reserved).outpost_flag == 1);
    DOCTEST_CHECK(state.next_colony_id == 80001);
    DOCTEST_REQUIRE(state.colony_reservations[0].size() == 1);
    DOCTEST_CHECK(state.colony_reservations[0].front() == game::id_colony{80002});
}

DOCTEST_TEST_CASE("founding consumes the chosen ship key but not its row replacement") {
    for (const bool outpost : {false, true}) {
        TEST_COLONIZE::Reset_();
        auto& state = game::current_state;
        constexpr game::id_ship chosen{90000}, other{90001}, replacement{90002};
        constexpr game::id_star source{0};
        constexpr game::id_planet site{0};
        constexpr game::id_colony reserved{70000};
        state.next_ship_id = 90003;
        state.next_colony_id = 70001;
        state.colony_reservations[0] = {reserved};
        test_planets::Record(site.id).star_id = source;
        test_planets::Record(site.id).planet_type = PLANET_TYPE_PLANET;
        test_planets::Record(site.id).colony_id = game::NO_COLONY;

        s_ship_data ship{};
        ship.id = other;
        ship.position.Set({game::e_ship_location::at_star, source});
        ship.d.ship_type = outpost ? SHIP_TYPE_OUTPOST : SHIP_TYPE_COLONY;
        test_ships::Rows(state).Replace_Row(1, ship);
        ship.id = replacement;
        test_ships::Rows(state).Replace_Row(0, ship);

        DOCTEST_CHECK_FALSE(COLONIZE::Found_From_Ship(chosen, source, site, game::id_player{0}, outpost));
        DOCTEST_CHECK(state.planets.at(site).colony_id == game::NO_COLONY);
        DOCTEST_CHECK(state.colony_reservations[0] == std::vector{reserved});
        DOCTEST_CHECK((test_ships::Rows(state)[0].status == 0));

        test_ships::Rows(state).Set_Key_At_Row(0, chosen);
        test_ships::Rows(state).Swap_Rows(0, 1);
        DOCTEST_REQUIRE(COLONIZE::Found_From_Ship(chosen, source, site, game::id_player{0}, outpost));
        DOCTEST_CHECK(state.planets.at(site).colony_id == reserved);
        DOCTEST_CHECK((state.colonies.at(reserved).outpost_flag == outpost));
        DOCTEST_CHECK((test_ships::Rows(state)[0].status == 0));
        DOCTEST_CHECK((test_ships::Rows(state)[1].status == 5));
    }
}

DOCTEST_TEST_CASE("first colony after losing the homeworld becomes the new home") {
    TEST_COLONIZE::Reset_();
    game::current_state.players[0].home_planet_id = game::NO_PLANET;
    test_planets::Record(0).star_id = game::id_star{static_cast<uint32_t>(0)};

    COLONIZE::Make_New_Colony(game::id_planet{0}, game::id_player{0});

    DOCTEST_CHECK(game::current_state.players[0].home_planet_id == game::id_planet{0});
}

DOCTEST_TEST_CASE("new outpost does not become the homeworld") {
    TEST_COLONIZE::Reset_();
    game::current_state.players[0].home_planet_id = game::NO_PLANET;
    test_planets::Record(0).star_id = game::id_star{static_cast<uint32_t>(0)};

    COLONIZE::Make_New_Outpost(game::id_planet{0}, game::id_player{0});

    DOCTEST_CHECK(game::current_state.players[0].home_planet_id == game::NO_PLANET);
}

DOCTEST_TEST_CASE("orbit queries and colony creation use a wide planet ID") {
    TEST_COLONIZE::Reset_();
    constexpr game::id_planet id{70000};
    test_planets::Set(id.id, {.colony_id = game::NO_COLONY, .star_id = game::id_star{static_cast<uint32_t>(0)}, .planet_type = PLANET_TYPE_PLANET});
    auto& star = test_stars::Record(0);
    star.orbit_planet_ids[0] = id;
    game::current_state.players[0].home_planet_id = game::NO_PLANET;

    DOCTEST_CHECK(COLONIZE::Star_N_Colonizable_Planets_For_Player(&star, game::id_player{0}) == 1);
    DOCTEST_CHECK(COLONIZE::Star_N_Outpostable_Planets(&star) == 1);
    COLONIZE::Make_New_Colony(id, game::id_player{0});
    DOCTEST_CHECK(test_colonies::Record(0).planet_id == id);
    DOCTEST_CHECK(game::current_state.players[0].home_planet_id == id);
    DOCTEST_CHECK(COLONIZE::Star_N_Uncolonized_Planets(&star) == 0);
}

DOCTEST_TEST_CASE("first colony population uses the configured worker rule") {
    TEST_COLONIZE::Reset_();

    test_planets::Record(0).star_id = game::id_star{static_cast<uint32_t>(0)};
    test_planets::Record(0).food_per_farmer = 2;

    COLONIZE::Make_New_Colony(game::id_planet{0}, game::id_player{0});

    DOCTEST_CHECK((POP::Get_Prof(test_colonies::Record(0).pop[0]) == POP::PROF_FARMER));

    TEST_COLONIZE::Reset_();
    test_planets::Record(0).star_id = game::id_star{static_cast<uint32_t>(0)};
    test_planets::Record(0).food_per_farmer = 2;
    COLONIZE::first_pop_to_production = 1;

    COLONIZE::Make_New_Colony(game::id_planet{0}, game::id_player{0});

    DOCTEST_CHECK((POP::Get_Prof(test_colonies::Record(0).pop[0]) == POP::PROF_WORKER));
}

DOCTEST_TEST_CASE("new colony populations use semantic identity and assignment fields") {
    TEST_COLONIZE::Reset_();
    constexpr int16_t player_idx = 3;
    test_colonies::Record(0).allocated_to = legacy_world::Decode_Player_Slot(player_idx);
    test_planets::Record(0).star_id = game::id_star{static_cast<uint32_t>(0)};
    test_planets::Record(0).food_per_farmer = 2;
    test_planets::Record(0).planet_special = SYSTEM_SPECIAL_NATIVES;

    COLONIZE::Make_New_Colony(game::id_planet{0}, legacy_world::Decode_Player_Slot(player_idx));

    DOCTEST_CHECK((POP::Get_Race(test_colonies::Record(0).pop[0]) == player_idx));
    DOCTEST_CHECK((POP::Get_Original_Owner(test_colonies::Record(0).pop[0]) == player_idx));
    DOCTEST_CHECK((POP::Is_Assigned(test_colonies::Record(0).pop[0])));
    DOCTEST_CHECK((POP::Get_Prof(test_colonies::Record(0).pop[0]) == POP::PROF_FARMER));
    for (int index = 1; index < 4; ++index) {
        DOCTEST_CHECK((POP::Get_Race(test_colonies::Record(0).pop[index]) == POP::RACE_NATIVE));
        DOCTEST_CHECK((POP::Get_Original_Owner(test_colonies::Record(0).pop[index]) == player_idx));
        DOCTEST_CHECK((POP::Is_Assigned(test_colonies::Record(0).pop[index])));
        DOCTEST_CHECK((POP::Get_Prof(test_colonies::Record(0).pop[index]) == POP::PROF_FARMER));
    }
}
