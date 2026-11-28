#include "pch.h"

#include <chrono>

#include "doctest/doctest.h"
#include "game/event_serialization.h"
#include "game/leader_serialization.h"
#include "game/notice_serialization.h"
#include "game/save_sections.h"
#include "game/settler_serialization.h"
#include "game/turn_oracle_history.h"

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

    size_t Append_Lua_Extension(std::vector<uint8_t>& save, std::string_view hash,
        std::string_view prototype = "test prototype") {
        constexpr std::string_view lua_version = "5.5.1";
        const std::string lua_sources(64, 'a');
        constexpr std::array<uint8_t, 24> config{'O', 'C', 'F', 'G', 1, 0, 24, 0};
        const uint32_t snapshot_bytes = 14 * sizeof(uint32_t) + lua_version.size() +
            prototype.size() + hash.size() + lua_sources.size();
        const uint32_t payload_bytes = config.size() + snapshot_bytes;
        const size_t start = save.size();
        save.resize(start + SAVEGAME::Extension_Size(payload_bytes, 2));
        SERIAL::Writer_ writer(save.data() + start, save.size() - start, "oracle test extension overflow");
        SAVEGAME::Write_Extension_Header(&writer, payload_bytes, 2);
        SAVEGAME::Write_Section_Header(&writer, SAVEGAME::SECTION_TYPE_CONFIG,
            SAVEGAME::SECTION_FORMAT_VERSION, SAVEGAME::SECTION_FLAG_REQUIRED, config.size());
        writer.Write_Bytes(config.data(), config.size());
        SAVEGAME::Write_Section_Header(&writer, SAVEGAME::SECTION_TYPE_LUA,
            SAVEGAME::SECTION_FORMAT_VERSION, SAVEGAME::SECTION_FLAG_REQUIRED, snapshot_bytes);
        writer.Write_Uint32(snapshot_bytes - sizeof(uint32_t));
        writer.Write_Uint32(0x554c324f); // O2LU
        writer.Write_Uint32(6); // Snapshot version.
        writer.Write_Uint32(3); // Registry fingerprint version.
        const auto string = [&](std::string_view value) {
            writer.Write_Uint32(value.size());
            writer.Write_Bytes(value.data(), value.size());
        };
        string(lua_version);
        writer.Write_Uint32(4); // Profile version.
        writer.Write_Uint32(0); // Packages.
        writer.Write_Uint32(0); // Lua runtime packages.
        string(prototype);
        const auto hash_offset = start + writer.Length() + sizeof(uint32_t);
        string(hash);
        string(lua_sources);
        writer.Write_Uint32(0); // Trait assignments.
        writer.Write_Uint32(0); // Ship generations (empty roster).
        writer.Write_Uint32(0); // Lua storage packages.
        DOCTEST_REQUIRE(start + writer.Length() == save.size());
        return hash_offset;
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

DOCTEST_TEST_CASE("oracle compares pre-v8 history with the fixed v8 format") {
    s_history_fixture fixture;
    const auto canonical = turn_oracle::Turn_Path(fixture.directory, 0);
    const auto actual = turn_oracle::Turn_Path(fixture.directory, 1);
    std::string error;

    const auto roster = std::make_unique<game::s_game_state>();
    const auto make_save = [&](bool wide) {
        std::vector<uint8_t> bytes(605, 0);
        bytes[0] = 0xe1;
        bytes[46] = wide ? 8 : 7;
        const auto zeroes = [&](size_t count) { bytes.insert(bytes.end(), count, 0); };
        zeroes(wide ? 2 + 4 + 2 + 4 : 2); // Empty colony roster, reservations, and queue keys.
        zeroes(wide ? 6 : 2); // Empty planet roster.
        zeroes(wide ? 6 : 2); // Empty star roster.
        for (uint8_t slot = 0; slot < MAX_LEADERS; ++slot) {
            if (wide) bytes.insert(bytes.end(), {slot, 0, 0, 0});
            zeroes(SERIAL::LEGACY_LEADER_BYTES);
            if (wide) bytes.insert(bytes.end(), 4, 0xff);
        }
        constexpr auto wide_player_bytes = 0xf0e + ORIGINAL_MAX_FREIGHTED_SETTLERS * (SERIAL::SETTLER_BYTES - 8) + 20;
        zeroes(2 + MAX_PLAYERS * (wide ? wide_player_bytes : 0xf0e));
        zeroes(wide ? 6 : 2); // Empty ship roster.
        bytes.insert(bytes.end(), {0, 0, 0xff, 0xff}); // Closed system box.
        zeroes(164 + 12 - 4);
        zeroes(2 + MAX_REPORTS * (wide ? SERIAL::REPORT_BYTES : SERIAL::LEGACY_REPORT_BYTES) + 1 + MAX_PLAYERS + 128);
        for (int type = 0; type < EVENT_COUNT; ++type) {
            std::array<uint8_t, SERIAL::EVENT_BYTES> record{};
            SERIAL::Writer_ writer(record.data(), record.size(), "test event overflow");
            s_event_data event{};
            if (SERIAL::Event_Targets_Star(type) || SERIAL::Event_Targets_Colony(type)) event.target_id = -1;
            if (type == EVENT_UNSTABLE_WORMHOLE) event.amount = -1;
            if (wide) SERIAL::Write_Event(writer, *roster, event, type);
            else SERIAL::Write_Legacy_Event(writer, *roster, event, type);
            bytes.insert(bytes.end(), record.begin(), record.begin() + writer.Length());
        }
        for (int i = 0; i < MAX_MESSAGES; ++i) {
            bytes.push_back(0x7f); // Notice with no target.
            zeroes((wide ? SERIAL::MESSAGE_BYTES : SERIAL::LEGACY_MESSAGE_BYTES) - 1);
        }
        constexpr size_t antaran_bytes = 12 + 6 * HULL_CLASS_COUNT;
        zeroes(3 * MAX_PLAYERS + 2 + 4 + 6 + 6 + antaran_bytes + 6);
        if (wide) bytes.insert(bytes.end(), 4, 0xff); // No system box star.
        zeroes(1000 - (wide ? 4 : 0) + 2);
        zeroes(MAX_PLAYERS * (MAX_FREIGHTED_SETTLERS - ORIGINAL_MAX_FREIGHTED_SETTLERS) * (wide ? SERIAL::SETTLER_BYTES : 8));
        return bytes;
    };
    auto v7 = make_save(false);
    auto v8 = make_save(true);

    DOCTEST_REQUIRE(turn_oracle::Write_File(canonical, v7, &error));
    DOCTEST_REQUIRE(turn_oracle::Write_File(actual, v8, &error));
    const bool forward = turn_oracle::Compare_Files(canonical, actual, &error);
    DOCTEST_CHECK_MESSAGE(forward, error);
    const bool reverse = turn_oracle::Compare_Files(actual, canonical, &error);
    DOCTEST_CHECK_MESSAGE(reverse, error);

    DOCTEST_SUBCASE("verified fingerprint pair only in the Lua config field") {
        constexpr std::string_view old_hash = "v2:749ecf84cbd3e18efd2463e5a4a38ebaa92803fe0ec8d725f7992280809fd410";
        constexpr std::string_view new_hash = "v2:123f0f92c4c3ef6d631284a57d7ff801ee151722e14ceb5628cea8d213e540f9";
        Append_Lua_Extension(v7, old_hash);
        const auto new_hash_offset = Append_Lua_Extension(v8, new_hash);
        const auto compare = [&] {
            DOCTEST_REQUIRE(turn_oracle::Write_File(canonical, v7, &error));
            DOCTEST_REQUIRE(turn_oracle::Write_File(actual, v8, &error));
            return turn_oracle::Compare_Files(canonical, actual, &error);
        };
        DOCTEST_CHECK_MESSAGE(compare(), error);
        DOCTEST_CHECK_MESSAGE(turn_oracle::Compare_Files(actual, canonical, &error), error);
        std::vector<uint8_t> original;
        DOCTEST_REQUIRE(turn_oracle::Read_File(canonical, &original, &error));
        DOCTEST_CHECK(original == v7);

        DOCTEST_SUBCASE("accept the verified local-schema change without hiding seed changes") {
            constexpr std::string_view local_schema_hash = "v2:cd2b12d94b7db38586fec595fc31d011e795a3682596d3b04ba8d1e4bf345e38";
            std::copy(local_schema_hash.begin(), local_schema_hash.end(), v8.begin() + new_hash_offset);
            DOCTEST_CHECK_MESSAGE(compare(), error);
            DOCTEST_CHECK_MESSAGE(turn_oracle::Compare_Files(actual, canonical, &error), error);
            v8[601] ^= 1; // RNG seed.
            DOCTEST_CHECK_FALSE(compare());
        }

        DOCTEST_SUBCASE("reject a different synchronized configuration hash") {
            v8[new_hash_offset + 3] = 'f';
            DOCTEST_CHECK_FALSE(compare());
        }
        DOCTEST_SUBCASE("reject gameplay and Lua source differences even with the verified pair") {
            v8[42] ^= 1; // Stardate.
            DOCTEST_CHECK_FALSE(compare());
            v8[42] ^= 1;
            v8[new_hash_offset + new_hash.size() + sizeof(uint32_t)] ^= 1;
            DOCTEST_CHECK_FALSE(compare());
        }
        DOCTEST_SUBCASE("reject the hashes in a different Lua field") {
            v7 = make_save(false);
            v8 = make_save(true);
            Append_Lua_Extension(v7, new_hash, old_hash);
            Append_Lua_Extension(v8, new_hash, new_hash);
            DOCTEST_CHECK_FALSE(compare());
        }
        DOCTEST_SUBCASE("reject the pair in the wrong version direction") {
            v7 = make_save(false);
            v8 = make_save(true);
            Append_Lua_Extension(v7, new_hash);
            Append_Lua_Extension(v8, old_hash);
            DOCTEST_CHECK_FALSE(compare());
        }
        DOCTEST_SUBCASE("reject matching damage to the Lua frame rather than ignoring the fingerprint") {
            // Truncate identically and retain the length prefixes: the fingerprint is no longer in a valid extension.
            v7.pop_back();
            v8.pop_back();
            DOCTEST_CHECK_FALSE(compare());
        }
        DOCTEST_SUBCASE("no exception for two current-format saves") {
            v7 = v8;
            std::copy(old_hash.begin(), old_hash.end(), v7.begin() + new_hash_offset);
            DOCTEST_CHECK_FALSE(compare());
        }
        return;
    }

    v8.push_back(0xa4);
    DOCTEST_REQUIRE(turn_oracle::Write_File(actual, v8, &error));
    DOCTEST_CHECK_FALSE(turn_oracle::Compare_Files(canonical, actual, &error));
    DOCTEST_CHECK(error.find("first difference at byte") != std::string::npos);

    v7.resize(605);
    DOCTEST_REQUIRE(turn_oracle::Write_File(canonical, v7, &error));
    DOCTEST_CHECK_FALSE(turn_oracle::Compare_Files(canonical, actual, &error));
    DOCTEST_CHECK(error.find("invalid save prefix, planet section, or leader section") != std::string::npos);
}
