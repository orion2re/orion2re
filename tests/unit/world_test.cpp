#include "pch.h"

#include "doctest/doctest.h"
#include "game/world.h"
#include "game/settler_serialization.h"
#include "game/serialization.h"
#include "game/planet_serialization.h"
#include "game/colony_serialization.h"
#include "game/star_serialization.h"

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

DOCTEST_TEST_CASE("world colonies retain their identities after edits and never reuse deleted IDs") {
    game::s_world_state world;
    game::s_world_state other;
    s_colony outpost{};
    outpost.outpost_flag = 1;
    outpost.planet = game::id_planet{4000000000u};
    const auto first = game::Create_Colony(&world, s_colony{});
    const auto second = game::Create_Colony(&world, outpost);
    DOCTEST_REQUIRE(first.has_value());
    DOCTEST_REQUIRE(second.has_value());
    DOCTEST_CHECK(*first == game::id_colony{0});
    auto* promoted = game::Find_Colony(&world, *second);
    DOCTEST_REQUIRE(promoted != nullptr);
    promoted->outpost_flag = 0;
    promoted->owner = 2;

    DOCTEST_REQUIRE(game::Erase_Colony(&world, *first));
    const auto third = game::Create_Colony(&world, s_colony{});
    DOCTEST_REQUIRE(third.has_value());
    DOCTEST_CHECK(*third > *second);
    DOCTEST_CHECK(game::Find_Colony(&world, *first) == nullptr);
    DOCTEST_CHECK(game::Find_Colony(&world, *second) == promoted);
    DOCTEST_CHECK(game::Colony(&world, *second).owner == 2);
    DOCTEST_CHECK(game::Colony(&world, *second).planet == outpost.planet);
    DOCTEST_CHECK(game::Colony_Count(&world) == 2);
    DOCTEST_CHECK(game::Find_Colony(&other, *second) == nullptr);
}

DOCTEST_TEST_CASE("colony allocation bounds live count separately from ID exhaustion") {
    game::s_world_state world;
    world.next_colony_id = 1000000;
    for (int i = 0; i < MAX_COLONIES; ++i) {
        DOCTEST_REQUIRE(game::Create_Colony(&world, s_colony{}).has_value());
    }
    DOCTEST_CHECK_FALSE(game::Create_Colony(&world, s_colony{}).has_value());
    DOCTEST_REQUIRE(game::Erase_Colony(&world, game::id_colony{1000000}));
    world.next_colony_id = UINT32_MAX - uint64_t{1};
    const auto last = game::Create_Colony(&world, s_colony{});
    DOCTEST_REQUIRE(last.has_value());
    DOCTEST_CHECK(last->id == UINT32_MAX - 1);
    DOCTEST_CHECK(game::Erase_Colony(&world, *last));
    DOCTEST_CHECK_FALSE(game::Create_Colony(&world, s_colony{}).has_value());
}

DOCTEST_TEST_CASE("colony snapshot preserves explicit IDs and the allocator after deletion") {
    game::s_world_state source;
    source.next_colony_id = 3000000000u;
    s_colony colony{};
    colony.owner = 1;
    colony.planet = game::id_planet{1000000};
    const auto live = game::Create_Colony(&source, colony);
    const auto removed = game::Create_Colony(&source, colony);
    DOCTEST_REQUIRE(live.has_value());
    DOCTEST_REQUIRE(removed.has_value());
    DOCTEST_REQUIRE(game::Erase_Colony(&source, *removed));

    std::array<uint8_t, 1024> buffer{};
    SERIAL::Writer_ writer(buffer.data(), buffer.size(), "colony snapshot test output");
    SAVEGAME::Write_Colonies(&source, &writer);
    SERIAL::Reader_ reader(buffer.data(), writer.Length(), "colony snapshot test input", true);
    game::s_world_state loaded;
    DOCTEST_REQUIRE(SAVEGAME::Read_Colonies(&loaded, &reader));
    DOCTEST_CHECK(reader.Remaining() == 0);
    DOCTEST_CHECK(game::Colony_Count(&loaded) == 1);
    DOCTEST_REQUIRE(game::Find_Colony(&loaded, *live) != nullptr);
    DOCTEST_CHECK(game::Colony(&loaded, *live).owner == 1);
    DOCTEST_CHECK(game::Colony(&loaded, *live).planet == colony.planet);
    DOCTEST_CHECK(game::Find_Colony(&loaded, *removed) == nullptr);
    const auto next = game::Create_Colony(&loaded, colony);
    DOCTEST_REQUIRE(next.has_value());
    DOCTEST_CHECK(*next > *removed);

    SERIAL::Reader_ truncated(buffer.data(), writer.Length() - 1, "truncated colony snapshot", true);
    DOCTEST_CHECK_FALSE(SAVEGAME::Read_Colonies(&loaded, &truncated));
    DOCTEST_CHECK(game::Colony_Count(&loaded) == 2);
    DOCTEST_CHECK(loaded.next_colony_id == uint64_t{next->id} + 1);
}

DOCTEST_TEST_CASE("historical colony import preserves slot IDs over empty and reserved records") {
    constexpr size_t record_size = 362; // v7 colony wire record.
    std::array<uint8_t, 2 + 5 * record_size> bytes{};
    bytes[0] = 5;
    bytes[2] = 1; // Slot 0 belongs to player 1, on planet 0.
    bytes[2 + record_size + 1] = 3; // Reserved but unused slot 1.
    bytes[2 + 2 * record_size] = 2; // Slot 2 belongs to player 2.
    bytes[2 + 2 * record_size + 2] = 17; // Little-endian legacy planet reference.
    bytes[2 + 3 * record_size] = 0xff; // Destroyed slot 3.
    // Slot 4 is zeroed; it still contributes to the allocator high-water mark.
    SERIAL::Reader_ reader(bytes.data(), bytes.size(), "historical colonies test", true);
    game::s_world_state world;
    DOCTEST_REQUIRE(SAVEGAME::Read_Legacy_Colonies(&world, &reader, 7));
    DOCTEST_CHECK(reader.Remaining() == 0);
    DOCTEST_CHECK(game::Colony_Count(&world) == 2);
    DOCTEST_REQUIRE(game::Find_Colony(&world, game::id_colony{0}) != nullptr);
    DOCTEST_CHECK(game::Colony(&world, game::id_colony{0}).owner == 1);
    DOCTEST_CHECK(game::Find_Colony(&world, game::id_colony{1}) == nullptr);
    DOCTEST_REQUIRE(game::Find_Colony(&world, game::id_colony{2}) != nullptr);
    DOCTEST_CHECK(game::Colony(&world, game::id_colony{2}).planet == game::id_planet{17});
    const auto created = game::Create_Colony(&world, s_colony{});
    DOCTEST_REQUIRE(created.has_value());
    DOCTEST_CHECK(*created == game::id_colony{5});

    SERIAL::Reader_ truncated(bytes.data(), bytes.size() - 1, "truncated historical colonies test", true);
    DOCTEST_CHECK_FALSE(SAVEGAME::Read_Legacy_Colonies(&world, &truncated, 7));
    DOCTEST_CHECK(game::Colony_Count(&world) == 3);
    DOCTEST_CHECK(world.next_colony_id == 6);
}

DOCTEST_TEST_CASE("reusing a colony processing position never reuses the historical colony ID") {
    game::s_world_state world;
    const game::s_colony_slots slots(&world);
    DOCTEST_REQUIRE(slots.Activate(0));
    DOCTEST_REQUIRE(slots.Activate(2));
    const auto old = slots.Id_At(0);
    const auto survivor = slots.Id_At(2);
    slots[2].owner = 2;
    slots[2].planet = game::id_planet{17};
    DOCTEST_CHECK(&slots[2] == game::Find_Colony(&world, survivor));
    slots.Retire(0);
    DOCTEST_CHECK(game::Find_Colony(&world, old) == nullptr);
    DOCTEST_REQUIRE(slots.Activate(0));
    const auto replacement = slots.Id_At(0);
    DOCTEST_CHECK(replacement > survivor);
    slots[0].owner = 1;
    slots[0].planet = game::id_planet{19};
    slots[1].allocated_to = 3;

    std::array<uint8_t, 2048> bytes{};
    SERIAL::Writer_ writer(bytes.data(), bytes.size(), "ordered colonies test");
    SAVEGAME::Write_Ordered_Colonies(slots, &writer, 3);
    game::s_world_state loaded;
    const game::s_colony_slots restored(&loaded);
    SERIAL::Reader_ reader(bytes.data(), writer.Length(), "ordered colonies test", true);
    int16_t extent = 0;
    DOCTEST_REQUIRE(SAVEGAME::Read_Ordered_Colonies(restored, &reader, 8, &extent));
    DOCTEST_CHECK(extent == 3);
    DOCTEST_CHECK(game::Colony_Count(&loaded) == 2);
    DOCTEST_CHECK(restored.Id_At(0) == replacement);
    DOCTEST_CHECK(restored.Id_At(1) == game::NO_COLONY);
    DOCTEST_CHECK(restored[1].allocated_to == 3);
    DOCTEST_CHECK(restored.Id_At(2) == survivor);
    DOCTEST_CHECK(restored[2].planet == game::id_planet{17});
    DOCTEST_CHECK(loaded.next_colony_id == world.next_colony_id);
}

DOCTEST_TEST_CASE("star IDs survive a position reuse and preserve legacy save order") {
    game::s_world_state source;
    int16_t extent = 3;
    const game::s_star_slots positions(&source, &extent);
    DOCTEST_REQUIRE(positions.Activate(0));
    DOCTEST_REQUIRE(positions.Activate(1));
    DOCTEST_REQUIRE(positions.Activate(2));
    const auto retired = positions.Id_At(0);
    const auto survivor = positions.Id_At(2);
    source.next_star_id = 2000000;
    positions.Retire(0);
    positions.Retire(1);
    positions[1].spectral_class = -1;
    DOCTEST_REQUIRE(positions.Activate(0));
    const auto replacement = positions.Id_At(0);
    DOCTEST_CHECK(replacement == game::id_star{2000000});
    DOCTEST_CHECK(game::Find_Star(&source, retired) == nullptr);
    DOCTEST_CHECK(positions.Id_At(2) == survivor);
    DOCTEST_CHECK(positions.Id_At(extent) == game::NO_STAR);
    DOCTEST_CHECK(game::Star_Count(&source) == 2);
    DOCTEST_CHECK(&positions[0] == &game::Star(&source, replacement));
    positions[0].x = 123;
    positions[2].x = 234;
    positions[0].wormhole_star_id = survivor;
    positions[2].wormhole_star_id = replacement;
    positions[2].relocate_ship_to[1] = replacement;
    positions[extent].x = 345;

    std::array<uint8_t, 2048> bytes{};
    SERIAL::Writer_ writer(bytes.data(), bytes.size(), "ordered stars test");
    SAVEGAME::Write_Ordered_Stars(positions, &writer, extent, true);
    game::s_world_state loaded;
    int16_t loaded_extent = 0;
    const game::s_star_slots restored(&loaded, &loaded_extent);
    SERIAL::Reader_ reader(bytes.data(), writer.Length(), "ordered stars test", true);
    DOCTEST_REQUIRE(SAVEGAME::Read_Ordered_Stars(restored, &reader, 8, &loaded_extent, true));
    DOCTEST_CHECK(reader.Remaining() == 0);
    DOCTEST_CHECK(loaded_extent == 3);
    DOCTEST_CHECK(restored.Id_At(0) == replacement);
    DOCTEST_CHECK(restored.Id_At(1) == game::NO_STAR);
    DOCTEST_CHECK(restored.Id_At(2) == survivor);
    DOCTEST_CHECK(restored[0].x == 123);
    DOCTEST_CHECK(restored[1].spectral_class == -1);
    DOCTEST_CHECK(restored[2].x == 234);
    DOCTEST_CHECK(restored[0].wormhole_star_id == survivor);
    DOCTEST_CHECK(restored[2].wormhole_star_id == replacement);
    DOCTEST_CHECK(restored[2].relocate_ship_to[1] == replacement);
    DOCTEST_CHECK(restored[3].x == 345);
    DOCTEST_CHECK(restored.Id_At(3) == game::NO_STAR);
    DOCTEST_CHECK(loaded.next_star_id == 2000001);

    SERIAL::Reader_ truncated(bytes.data(), writer.Length() - 1, "truncated stars", true);
    DOCTEST_CHECK_FALSE(SAVEGAME::Read_Ordered_Stars(restored, &truncated, 8, &loaded_extent, true));
    DOCTEST_CHECK(restored.Id_At(0) == replacement);
    DOCTEST_CHECK(restored[3].x == 345);
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
