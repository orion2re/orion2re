#include "pch.h"

#include "doctest/doctest.h"
#include "game/world.h"
#include "game/settler_serialization.h"
#include "game/serialization.h"
#include "game/planet_serialization.h"

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

DOCTEST_TEST_CASE("planet identities survive mutation and deletion never reuses an ID") {
    game::s_world_state world;
    const auto first = game::Create_Planet(&world, s_planet_data{});
    const auto second = game::Create_Planet(&world, s_planet_data{});
    DOCTEST_REQUIRE(first.has_value());
    DOCTEST_REQUIRE(second.has_value());
    game::Planet(&world, *second).size = PLANET_SIZE_HUGE;
    DOCTEST_CHECK(game::Erase_Planet(&world, *first));
    const auto third = game::Create_Planet(&world, s_planet_data{});
    DOCTEST_REQUIRE(third.has_value());
    DOCTEST_CHECK(*third > *second);
    DOCTEST_CHECK(game::Find_Planet(&world, *first) == nullptr);
    DOCTEST_CHECK(game::Planet(&world, *second).size == PLANET_SIZE_HUGE);
    DOCTEST_CHECK(game::Planet_Count(&world) == 2);
}

DOCTEST_TEST_CASE("planet allocation supports sparse full-width identities and detects exhaustion") {
    game::s_world_state world;
    const game::id_planet imported{4000000000u};
    DOCTEST_REQUIRE(game::Insert_Planet(&world, imported, s_planet_data{}));
    const auto created = game::Create_Planet(&world, s_planet_data{});
    DOCTEST_REQUIRE(created.has_value());
    DOCTEST_CHECK(created->id == imported.id + 1);
    world.next_planet_id = UINT32_MAX - uint64_t{1};
    const auto last = game::Create_Planet(&world, s_planet_data{});
    DOCTEST_REQUIRE(last.has_value());
    DOCTEST_CHECK(last->id == UINT32_MAX - 1);
    DOCTEST_CHECK(game::Erase_Planet(&world, *last));
    DOCTEST_CHECK_FALSE(game::Create_Planet(&world, s_planet_data{}).has_value());
    DOCTEST_CHECK_FALSE(game::Insert_Planet(&world, game::NO_PLANET, s_planet_data{}));
}

DOCTEST_TEST_CASE("settler serialization preserves typed destinations without native bitpacking") {
    game::s_world_state source;
    const game::id_player player{0};
    source.settlers[player] = {
        {game::id_colony{3}, game::id_planet{3000000000u}, 8, 12, POP::PROF_SCIENTIST},
        {std::nullopt, game::id_planet{1000000}, 1, 2, POP::PROF_WORKER},
    };
    std::array<uint8_t, 256> buffer{};
    SERIAL::Writer_ writer(buffer.data(), buffer.size(), "settler test output");
    SAVEGAME::Write_Settlers(&source, &writer, player);
    SERIAL::Reader_ reader(buffer.data(), writer.Length(), "settler test input", true);
    game::s_world_state destination;
    DOCTEST_REQUIRE(SAVEGAME::Read_Settlers(&destination, &reader, player));
    DOCTEST_REQUIRE(destination.settlers[player].size() == 2);
    DOCTEST_CHECK(destination.settlers[player][0].destination_planet == game::id_planet{3000000000u});
    DOCTEST_CHECK(destination.settlers[player][0].source_colony == game::id_colony{3});
    DOCTEST_CHECK(destination.settlers[player][0].eta == 12);
    DOCTEST_CHECK_FALSE(destination.settlers[player][1].source_colony.has_value());
    DOCTEST_CHECK(reader.Remaining() == 0);
}

DOCTEST_TEST_CASE("historical settler words decode only at the compatibility boundary") {
    const uint32_t original = 3u | (201u << 8) | (8u << 16) | (12u << 20) | (2u << 24);
    const auto old = SAVEGAME::Decode_Legacy_Settler(original, true);
    DOCTEST_CHECK(old.source_colony == game::id_colony{3});
    DOCTEST_CHECK(old.destination_planet == game::id_planet{201});
    DOCTEST_CHECK(old.race == 8);
    DOCTEST_CHECK(old.eta == 12);
    DOCTEST_CHECK(old.profession == POP::PROF_SCIENTIST);
    const uint64_t expanded = UINT64_C(0xFFFF) | (UINT64_C(5119) << 16) |
        (UINT64_C(8) << 32) | (UINT64_C(12) << 36) | (UINT64_C(2) << 40);
    const auto newer = SAVEGAME::Decode_Legacy_Settler(expanded, false);
    DOCTEST_CHECK_FALSE(newer.source_colony.has_value());
    DOCTEST_CHECK(newer.destination_planet == game::id_planet{5119});
}

DOCTEST_TEST_CASE("planet snapshots retain explicit IDs and the deleted-ID high-water mark") {
    game::s_world_state source;
    DOCTEST_REQUIRE(game::Insert_Planet(&source, game::id_planet{1000000}, s_planet_data{}));
    DOCTEST_REQUIRE(game::Insert_Planet(&source, game::id_planet{4000000000u}, s_planet_data{}));
    DOCTEST_REQUIRE(game::Erase_Planet(&source, game::id_planet{4000000000u}));
    std::array<uint8_t, 256> bytes{};
    SERIAL::Writer_ writer(bytes.data(), bytes.size(), "planet snapshot test");
    SAVEGAME::Write_Planets(&source, &writer);
    game::s_world_state loaded;
    SERIAL::Reader_ reader(bytes.data(), writer.Length(), "planet snapshot test", true);
    DOCTEST_REQUIRE(SAVEGAME::Read_Planets(&loaded, &reader));
    DOCTEST_CHECK(loaded.planets.size() == 1);
    DOCTEST_CHECK(game::Find_Planet(&loaded, game::id_planet{1000000}) != nullptr);
    const auto created = game::Create_Planet(&loaded, s_planet_data{});
    DOCTEST_REQUIRE(created.has_value());
    DOCTEST_CHECK(created->id == 4000000001u);

    const auto before = loaded.planets.size();
    SERIAL::Reader_ truncated(bytes.data(), writer.Length() - 1, "truncated planet snapshot", true);
    DOCTEST_CHECK_FALSE(SAVEGAME::Read_Planets(&loaded, &truncated));
    DOCTEST_CHECK(loaded.planets.size() == before);
    DOCTEST_CHECK(loaded.next_planet_id == 4000000002u);
}
