#include "pch.h"

#include "doctest/doctest.h"
#include "game/world.h"

DOCTEST_TEST_CASE("worlds own independent leader records") {
    game::s_world_state first;
    game::s_world_state second;
    DOCTEST_CHECK(first.leaders.size() == MAX_LEADERS);
    DOCTEST_CHECK(first.players.empty());
    DOCTEST_CHECK(first.ships.empty());
    DOCTEST_CHECK(first.stars.empty());
    DOCTEST_CHECK(first.planets.empty());
    DOCTEST_CHECK(first.colonies.empty());
    DOCTEST_CHECK(first.nebulas.empty());

    game::Leader(&first, 0).xp = 123;
    DOCTEST_CHECK(game::Leader(&second, 0).xp == 0);
    game::s_world_state copy = first;
    game::Leader(&copy, 0).xp = 456;
    DOCTEST_CHECK(game::Leader(&first, 0).xp == 123);
}

DOCTEST_TEST_CASE("leader maps support sparse IDs without inserting on lookup") {
    game::s_world_state world;
    world.leaders.clear();
    world.leaders.emplace(game::id_leader{1000}, s_leader_data{});
    world.leaders.emplace(game::id_leader{1000000}, s_leader_data{});
    game::Leader(&world, 1000).xp = 42;
    const auto* record = game::Find_Leader(&world, game::id_leader{1000});
    DOCTEST_REQUIRE(record != nullptr);
    DOCTEST_CHECK(record->xp == 42);
    DOCTEST_CHECK(game::Find_Leader(&world, game::id_leader{999}) == nullptr);
    DOCTEST_CHECK(world.leaders.size() == 2);
    DOCTEST_CHECK(world.leaders.begin()->first == game::id_leader{1000});
    world.leaders.erase(game::id_leader{1000});
    DOCTEST_CHECK(game::Find_Leader(&world, game::id_leader{1000}) == nullptr);
    DOCTEST_CHECK(world.leaders.size() == 1);
}

DOCTEST_TEST_CASE("legacy leader records copy individually between map and wire storage") {
    game::s_world_state world;
    std::vector<s_leader_data> records(MAX_LEADERS);
    for (uint32_t id = 0; id < records.size(); ++id) {
        records[id].xp = static_cast<int16_t>(id * 3);
        records[id].location = static_cast<int16_t>(id);
    }
    game::Import_Leaders(&world, records);
    const auto exported = game::Export_Leaders(&world);
    DOCTEST_REQUIRE(exported.size() == records.size());
    DOCTEST_CHECK(memcmp(exported.data(), records.data(), records.size() * sizeof(s_leader_data)) == 0);
    records[0].xp = -1;
    DOCTEST_CHECK(game::Leader(&world, 0).xp == 0);
}
