#include "pch.h"

#include <chrono>
#include <fstream>

#include "doctest/doctest.h"
#include "game/turn_oracle_history.h"
#include "game/turn_oracle_save.h"
#include "game/record_serialization.h"
#include "game/planet_serialization.h"
#include "game/save_sections.h"
#include "game/savegame.h"

namespace {
    struct s_history_fixture {
        std::filesystem::path directory;
        s_history_fixture() {
            directory = std::filesystem::temp_directory_path() / ("orion2re-history-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            std::filesystem::create_directory(directory);
        }
        ~s_history_fixture() {
            std::filesystem::remove_all(directory);
        }
    };

    struct s_save_changes {
        bool swap_planet_ids = false;
        bool planet_size = false;
        bool colony_target = false;
        bool star_target = false;
        bool player_target = false;
        bool settler_target = false;
        bool ship_target = false;
        bool ship_storage = false;
        bool event_target = false;
        bool random_seed = false;
        bool allocator = false;
        bool shift_colony_slot = false;
        bool colony_population = false;
        bool colony_backlink = false;
        bool colony_event = false;
        bool settler_source = false;
        bool planet_parent = false;
    };

    std::vector<uint8_t> Encoded_Save(uint16_t version, s_save_changes changes = {}) {
        using namespace SAVEGAME::records;
        std::vector<uint8_t> bytes(SAVEGAME::BUFFER_CAPACITY);
        SERIAL::Writer_ writer(bytes.data(), bytes.size(), "oracle fixture overflow");
        const auto zeros = [&writer](size_t count) { Write_Zeroes(&writer, count); };
        const game::id_planet first{changes.swap_planet_ids ? 1u : 0u};
        const game::id_planet second{changes.swap_planet_ids ? 0u : 1u};
        writer.Write_Uint32(SAVEGAME::VERSIONED_HEADER);
        char description[37] = "Turn oracle";
        writer.Write_Bytes(description, sizeof(description));
        writer.Write_Int32(35000);
        writer.Write_Int8(0);
        writer.Write_Uint16(version);
        zeros(0x229);
        writer.Write_Uint32(changes.random_seed ? 43 : 42);
        const int16_t colony_slot = changes.shift_colony_slot ? 1 : 0;
        const game::id_colony colony_id{changes.shift_colony_slot ? 1000000u : 0u};
        writer.Write_Uint16(colony_slot + 1);
        if (version == 7) {
            // Version-7 colony records are 362 wire bytes, regardless of native layout.
            writer.Write_Int8(1); // Owner; distinguishes the live record from a zeroed slot.
            zeros(361);
        } else {
            writer.Write_Uint64(uint64_t{colony_id.id} + 1);
            if (changes.shift_colony_slot) {
                s_colony hole{};
                hole.owner = -1;
                hole.planet = game::NO_PLANET;
                writer.Write_Uint32(game::NO_COLONY.id);
                Write_Colony(&writer, &hole);
            }
            s_colony colony{};
            colony.owner = 1;
            colony.planet = changes.colony_target ? second : first;
            if (changes.colony_population) colony.n_pops = 1;
            writer.Write_Uint32(colony_id.id);
            Write_Colony(&writer, &colony);
        }
        game::s_world_state world;
        s_planet_data a{};
        a.colony_index = changes.colony_backlink ? -1 : colony_slot;
        a.parent_star = game::id_star{0};
        a.size = PLANET_SIZE_MEDIUM;
        s_planet_data b = a;
        b.colony_index = -1;
        b.orbit = 1;
        if (changes.planet_parent) b.parent_star = game::id_star{1};
        if (changes.planet_size) b.size = PLANET_SIZE_HUGE;
        DOCTEST_REQUIRE(game::Insert_Planet(&world, first, a));
        DOCTEST_REQUIRE(game::Insert_Planet(&world, second, b));
        world.next_planet_id = changes.allocator ? 3 : 2;
        if (version == 7) {
            writer.Write_Uint16(2);
            for (const auto& planet : {a, b}) {
                writer.Write_Int16(planet.colony_index);
                writer.Write_Int16(0); // Historical parent position.
                writer.Write_Int8(planet.orbit);
                writer.Write_Int8(0); // Body type.
                writer.Write_Int8(planet.size);
                zeros(11);
            }
        } else SAVEGAME::Write_Planets(&world, &writer);
        writer.Write_Uint16(1);
        if (version == 7) {
            std::vector<uint8_t> star(106 + BITMAP(MAX_STARS));
            const size_t ids_offset = 67 + BITMAP(MAX_STARS);
            const size_t wormhole_offset = 32 + BITMAP(MAX_STARS);
            star[wormhole_offset] = star[wormhole_offset + 1] = 0xff;
            std::fill_n(star.begin() + ids_offset + 10, MAX_PLAYERS * 2, 0xff);
            for (size_t orbit = 0; orbit < 5; ++orbit) {
                const uint16_t id = orbit < 2 ? static_cast<uint16_t>(orbit) : UINT16_MAX;
                star[ids_offset + orbit * 2] = static_cast<uint8_t>(id);
                star[ids_offset + orbit * 2 + 1] = static_cast<uint8_t>(id >> 8);
            }
            writer.Write_Bytes(star.data(), star.size());
        } else {
            writer.Write_Uint64(1); // Next star ID.
            writer.Write_Uint32(0); // The only star keeps its historical ID.
            s_star_data star{};
            std::fill(std::begin(star.planet_index), std::end(star.planet_index), game::NO_PLANET);
            star.planet_index[0] = first;
            star.planet_index[1] = second;
            if (changes.star_target) star.planet_index[0] = second;
            Write_Star(&writer, &star);
        }
        zeros(MAX_LEADERS * 59);
        writer.Write_Uint16(1);
        constexpr uint64_t packed_settler = (UINT64_C(1) << 16) |
            (UINT64_C(2) << 32) | (UINT64_C(7) << 36) | (UINT64_C(1) << 40);
        world.settlers[game::id_player{0}] = std::vector<SETTLER::s_settler>(26,
            {colony_id, second, 2, 7, POP::PROF_WORKER});
        if (changes.settler_target) world.settlers[game::id_player{0}][25].destination_planet = first;
        if (changes.settler_source) world.settlers[game::id_player{0}][25].source_colony.reset();
        for (uint32_t player = 0; player < MAX_PLAYERS; ++player) {
            if (version == 7) {
                // Only the first 25 freight words live in the old player record.
                std::vector<uint8_t> record(3854);
                if (player == 0) {
                    record[64] = 26;
                    for (size_t slot = 0; slot < 25; ++slot) {
                        for (size_t byte = 0; byte < 8; ++byte) record[66 + slot * 8 + byte] = static_cast<uint8_t>(packed_settler >> (byte * 8));
                    }
                }
                writer.Write_Bytes(record.data(), record.size());
            } else {
                s_player record{};
                record.offensive_stagepoint = game::id_star{0}; // Historical fixture is zero-filled.
                record.home_planet_id = first;
                if (changes.player_target && player == 0) record.home_planet_id = second;
                record.sneak_attack_planet = first;
                std::fill(std::begin(record.opportunity_planet_num), std::end(record.opportunity_planet_num), first);
                Write_Player(&world, &writer, &record, game::id_player{player});
            }
        }
        writer.Write_Uint16(1);
        s_ship_data ship{};
        ship.d.ship_type = SHIP_TYPE_COLONY;
        ship.location = {game::e_ship_location::at_star, game::id_star{0}};
        if (version == 7) {
            ship.d.ship_weapon[0].type = 1;
            ship.d.ship_weapon[1].type = 1;
            std::array<uint8_t, 1024> encoded{};
            SERIAL::Writer_ old_ship(encoded.data(), encoded.size(), "oracle ship fixture");
            Write_Ship(&old_ship, &ship);
            DOCTEST_REQUIRE(old_ship.Length() == 134);
            writer.Write_Bytes(encoded.data(), 101); // Design, owner and status.
            writer.Write_Int16(0); // Historical orbit position.
            writer.Write_Bytes(encoded.data() + 107, 26); // Fields after location, before optional target.
        } else {
            ship.auto_colonize_target = changes.ship_target ? first : second;
            if (changes.ship_storage) ship.d.ship_weapon[0].type = 42;
            Write_Ship(&writer, &ship);
        }
        zeros(163); // Movable boxes and map UI scalars.
        writer.Write_Uint8(0); // Nebulas.
        zeros(14); // Bill data and report count.
        for (size_t report = 0; report < MAX_REPORTS; ++report) {
            if (version == 7) zeros(20);
            else { s_report record{}; Write_Report(&writer, &record); }
        }
        zeros(1 + MAX_PLAYERS + BITMAP(MAX_STARS));
        for (int type = 0; type < EVENT_COUNT; ++type) {
            const bool climate = type == EVENT_CLIMATIC_CHANGE;
            const bool plague = type == EVENT_PLAGUE;
            writer.Write_Int8(0);
            writer.Write_Int8(climate ? 1 : plague ? 2 : 0);
            writer.Write_Int8(0);
            writer.Write_Int16(version == 7 && climate ? 1 : plague && !changes.colony_event ? colony_slot : -1);
            writer.Write_Int16(0);
            writer.Write_Int16(-1);
            if (version == 8) writer.Write_Uint32(climate ? (changes.event_target ? first.id : second.id) : game::NO_PLANET.id);
        }
        for (size_t message = 0; message < MAX_MESSAGES; ++message) {
            if (version == 7) zeros(19);
            else { s_MSG record{}; Write_Message(&writer, &record); }
        }
        zeros(1116); // Remaining turn globals and Antaran runtime state.
        if (version == 7) {
            for (size_t player = 0; player < MAX_PLAYERS; ++player) {
                for (size_t slot = 25; slot < MAX_FREIGHTED_SETTLERS; ++slot) writer.Write_Uint64(player == 0 && slot == 25 ? packed_settler : 0);
            }
        }
        if (version == 7) writer.Write_Int16(-1);
        else writer.Write_Uint32(game::NO_STAR.id);
        writer.Write_Int32(0); // One ship's redirect-state tail.
        SAVEGAME::Write_Extension_Header(&writer, 0, 0);
        bytes.resize(writer.Length());
        return bytes;
    }
} // namespace

DOCTEST_TEST_CASE("oracle compares binary saves and reports first differing offset and paths") {
    s_history_fixture fixture;
    const auto canonical = turn_oracle::Turn_Path(fixture.directory, 0);
    const auto actual = turn_oracle::Turn_Path(fixture.directory, 1);
    DOCTEST_CHECK(canonical.filename() == "turn-000000.GAM");
    std::string error;
    DOCTEST_REQUIRE(turn_oracle::Write_File(canonical, {0, 10, 255, 3}, &error));
    DOCTEST_REQUIRE(turn_oracle::Write_File(actual, {0, 10, 255, 3}, &error));
    DOCTEST_CHECK(turn_oracle::Compare_Files(canonical, actual, &error));
    DOCTEST_CHECK(error.empty());
    DOCTEST_REQUIRE(turn_oracle::Write_File(actual, {0, 10, 254, 4}, &error));
    DOCTEST_CHECK_FALSE(turn_oracle::Compare_Files(canonical, actual, &error));
    DOCTEST_CHECK(error.find("byte 2 (0x2)") != std::string::npos);
    DOCTEST_CHECK(error.find("canonical=0xFF actual=0xFE") != std::string::npos);
    DOCTEST_CHECK(error.find(canonical.string()) != std::string::npos);
    DOCTEST_CHECK(error.find(actual.string()) != std::string::npos);
}

DOCTEST_TEST_CASE("oracle detects truncated or longer saves and missing history") {
    s_history_fixture fixture;
    const auto canonical = turn_oracle::Turn_Path(fixture.directory, 0);
    const auto actual = turn_oracle::Turn_Path(fixture.directory, 1);
    std::string error;
    DOCTEST_REQUIRE(turn_oracle::Write_File(canonical, {1, 2, 3}, &error));
    DOCTEST_REQUIRE(turn_oracle::Write_File(actual, {1, 2}, &error));
    DOCTEST_CHECK_FALSE(turn_oracle::Compare_Files(canonical, actual, &error));
    DOCTEST_CHECK(error.find("canonical=0x03 actual=EOF") != std::string::npos);
    DOCTEST_CHECK(error.find("sizes: canonical=3 actual=2") != std::string::npos);
    DOCTEST_CHECK_FALSE(turn_oracle::Compare_Files(actual, canonical, &error));
    DOCTEST_CHECK(error.find("canonical=EOF actual=0x03") != std::string::npos);
    DOCTEST_REQUIRE(turn_oracle::Write_File(actual, {}, &error));
    DOCTEST_CHECK_FALSE(turn_oracle::Compare_Files(canonical, actual, &error));
    DOCTEST_CHECK(error.find("byte 0 (0x0): canonical=0x01 actual=EOF") != std::string::npos);
    std::filesystem::remove(canonical);
    DOCTEST_CHECK_FALSE(turn_oracle::Compare_Files(canonical, actual, &error));
    DOCTEST_CHECK(error.find("cannot read save") != std::string::npos);
    DOCTEST_CHECK_FALSE(turn_oracle::Write_File(fixture.directory / "missing" / "save.GAM", {1}, &error));
}

DOCTEST_TEST_CASE("oracle compares version 7 positional planets with unchanged version 8 IDs") {
    s_history_fixture fixture;
    const auto canonical = fixture.directory / "old.GAM";
    const auto actual = fixture.directory / "new.GAM";
    std::string error;
    DOCTEST_REQUIRE(turn_oracle::Write_File(canonical, Encoded_Save(7), &error));
    DOCTEST_REQUIRE(turn_oracle::Write_File(actual, Encoded_Save(8), &error));
    DOCTEST_CAPTURE(error);
    DOCTEST_CHECK(turn_oracle::Compare_Save_Files(canonical, actual, &error));
    DOCTEST_CHECK(turn_oracle::Compare_Save_Files(actual, canonical, &error));
    DOCTEST_CHECK(error.empty());
}

DOCTEST_TEST_CASE("oracle rejects changed planet data, references, IDs, counters, and random state") {
    s_history_fixture fixture;
    const auto canonical = fixture.directory / "old.GAM";
    const auto actual = fixture.directory / "new.GAM";
    std::string error;
    DOCTEST_REQUIRE(turn_oracle::Write_File(canonical, Encoded_Save(7), &error));
    const std::vector<std::pair<s_save_changes, std::string>> cases = {
        {{.planet_size = true}, "planet[1].size"},
        {{.colony_target = true}, "colony on planet[0]"},
        {{.star_target = true}, "star[0]"},
        {{.player_target = true}, "player[0]"},
        {{.settler_target = true}, "player[0]"},
        {{.ship_target = true}, "ship[0]"},
        {{.event_target = true}, "event["},
        {{.random_seed = true}, "settings and random seed"},
        {{.allocator = true}, "planet count and next ID"},
        {{.swap_planet_ids = true}, "colony on planet[0]"},
        {{.planet_parent = true}, "planet[1].parent_star"},
    };
    for (const auto& [changes, section] : cases) {
        DOCTEST_REQUIRE(turn_oracle::Write_File(actual, Encoded_Save(8, changes), &error));
        DOCTEST_CHECK_FALSE(turn_oracle::Compare_Save_Files(canonical, actual, &error));
        DOCTEST_CHECK(error.find(section) != std::string::npos);
        DOCTEST_CHECK(error.find("old position i") != std::string::npos);
    }
}

DOCTEST_TEST_CASE("cross-version oracle matches colony identities by planet but checks state and links") {
    s_history_fixture fixture;
    const auto canonical = fixture.directory / "old.GAM";
    const auto actual = fixture.directory / "new.GAM";
    std::string error;
    DOCTEST_REQUIRE(turn_oracle::Write_File(canonical, Encoded_Save(7), &error));
    DOCTEST_REQUIRE(turn_oracle::Write_File(actual, Encoded_Save(8, {.shift_colony_slot = true}), &error));
    DOCTEST_REQUIRE(turn_oracle::Compare_Save_Files(canonical, actual, &error));
    const std::pair<s_save_changes, const char*> cases[] = {
        {{.shift_colony_slot = true, .colony_population = true}, "colony on planet[0]"},
        {{.shift_colony_slot = true, .colony_backlink = true}, "planet[0].colony_index"},
        {{.shift_colony_slot = true, .colony_event = true}, "event["},
        {{.shift_colony_slot = true, .settler_source = true}, "player[0]"},
    };
    for (const auto& [changes, section] : cases) {
        DOCTEST_CAPTURE(section);
        DOCTEST_REQUIRE(turn_oracle::Write_File(actual, Encoded_Save(8, changes), &error));
        DOCTEST_CHECK_FALSE(turn_oracle::Compare_Save_Files(canonical, actual, &error));
        DOCTEST_CHECK(error.find(section) != std::string::npos);
    }
    DOCTEST_REQUIRE(turn_oracle::Write_File(canonical, Encoded_Save(8), &error));
    DOCTEST_REQUIRE(turn_oracle::Write_File(actual, Encoded_Save(8, {.shift_colony_slot = true}), &error));
    DOCTEST_CHECK_FALSE(turn_oracle::Compare_Save_Files(canonical, actual, &error));
}

DOCTEST_TEST_CASE("oracle rejects truncated and unsupported save formats") {
    const auto save = Encoded_Save(8);
    std::vector<uint8_t> normalized;
    std::string error;
    for (const size_t size : {size_t{0}, size_t{47}, save.size() / 2, save.size() - 1}) {
        const std::vector<uint8_t> truncated(save.begin(), save.begin() + size);
        DOCTEST_CHECK_FALSE(turn_oracle::Normalize_Save(truncated, &normalized, nullptr, &error));
        DOCTEST_CHECK_FALSE(error.empty());
        DOCTEST_CHECK(normalized.empty());
    }
    auto unknown = save;
    unknown[46] = 9;
    DOCTEST_CHECK_FALSE(turn_oracle::Normalize_Save(unknown, &normalized, nullptr, &error));
    DOCTEST_CHECK(error.find("unsupported") != std::string::npos);
}

DOCTEST_TEST_CASE("same-version oracle comparison remains byte-exact") {
    s_history_fixture fixture;
    const auto canonical = fixture.directory / "one.GAM";
    const auto actual = fixture.directory / "two.GAM";
    const auto save = Encoded_Save(8);
    const auto changed = Encoded_Save(8, {.ship_storage = true});
    std::string error;
    std::vector<uint8_t> normalized;
    DOCTEST_REQUIRE(turn_oracle::Normalize_Save(changed, &normalized, nullptr, &error));
    DOCTEST_CHECK(normalized == changed);
    DOCTEST_REQUIRE(turn_oracle::Write_File(canonical, save, &error));
    DOCTEST_REQUIRE(turn_oracle::Write_File(actual, changed, &error));
    DOCTEST_CHECK_FALSE(turn_oracle::Compare_Save_Files(canonical, actual, &error));
    DOCTEST_CHECK(error.find("same-version") != std::string::npos);
}

DOCTEST_TEST_CASE("oracle rejects duplicate planet IDs and invalid allocator metadata") {
    const auto save = Encoded_Save(8);
    SERIAL::Reader_ reader(save.data(), save.size(), "oracle fixture layout", true);
    reader.Skip(48 + 0x229 + 4);
    const uint16_t colonies = reader.Read_Uint16();
    reader.Skip(8);
    for (uint16_t position = 0; position < colonies; ++position) {
        reader.Skip(4);
        s_colony colony{};
        SAVEGAME::records::Read_Colony(&reader, &colony, 8);
    }
    const size_t metadata = reader.Length();
    DOCTEST_REQUIRE(reader.Read_Uint16() == 2);
    DOCTEST_REQUIRE(reader.Read_Uint64() == 2);
    const uint32_t first = reader.Read_Uint32();
    s_planet_data planet{};
    SAVEGAME::records::Read_Planet(&reader, &planet, 8);
    const size_t second_id = reader.Length();
    std::vector<uint8_t> normalized;
    std::string error;
    auto duplicate = save;
    for (size_t byte = 0; byte < 4; ++byte) duplicate[second_id + byte] = static_cast<uint8_t>(first >> (byte * 8));
    DOCTEST_CHECK_FALSE(turn_oracle::Normalize_Save(duplicate, &normalized, nullptr, &error));
    auto rewound = save;
    rewound[metadata + 2] = 1;
    DOCTEST_CHECK_FALSE(turn_oracle::Normalize_Save(rewound, &normalized, nullptr, &error));
    auto exhausted = save;
    exhausted[metadata + 2 + 4] = 1; // Next ID exceeds the uint32_t domain.
    DOCTEST_CHECK_FALSE(turn_oracle::Normalize_Save(exhausted, &normalized, nullptr, &error));
}

DOCTEST_TEST_CASE("external oracle history retains planet numbering across ordinary turns") {
    const char* supplied = std::getenv("ORION2RE_ORACLE_HISTORY");
    if (supplied == nullptr) return; // Local game histories are not repository fixtures.
    std::ifstream manifest(std::filesystem::path(supplied) / "history.txt");
    std::string header;
    std::getline(manifest, header);
    int32_t turns = 0;
    DOCTEST_REQUIRE(header == "orion2re-turn-oracle 1");
    DOCTEST_REQUIRE(static_cast<bool>(manifest >> turns));
    DOCTEST_REQUIRE(turns >= 1);
    DOCTEST_REQUIRE(turns <= 100000);
    std::vector<std::filesystem::path> saves;
    for (int32_t turn = 0; turn <= turns; ++turn) saves.push_back(turn_oracle::Turn_Path(supplied, turn));
    std::vector<turn_oracle::s_planet_placement> initial;
    uint64_t initial_next = 0;
    bool initialized = false;
    s_history_fixture fixture;
    for (const auto& path : saves) {
        std::vector<uint8_t> bytes;
        std::vector<uint8_t> normalized;
        std::vector<uint8_t> reparsed;
        std::string error;
        turn_oracle::s_save_summary summary;
        DOCTEST_CAPTURE(path.string());
        DOCTEST_CAPTURE(error);
        DOCTEST_REQUIRE(turn_oracle::Read_File(path, &bytes, &error));
        DOCTEST_REQUIRE(turn_oracle::Normalize_Save(bytes, &normalized, &summary, &error));
        if (!initialized) {
            initial = summary.planets;
            initial_next = summary.next_planet_id;
            initialized = true;
        }
        DOCTEST_CHECK(summary.planets == initial);
        DOCTEST_CHECK(summary.next_planet_id == initial_next);
        DOCTEST_REQUIRE(turn_oracle::Normalize_Save(normalized, &reparsed, nullptr, &error));
        DOCTEST_CHECK(normalized == reparsed);
        const auto converted = fixture.directory / path.filename();
        DOCTEST_REQUIRE(turn_oracle::Write_File(converted, normalized, &error));
        DOCTEST_CHECK(turn_oracle::Compare_Save_Files(path, converted, &error));
    }
    DOCTEST_MESSAGE("Verified ", saves.size(), " saved snapshots with ", initial.size(), " stable planet IDs and placements; next ID ", initial_next);
}
