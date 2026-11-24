#include "pch.h"

#include <chrono>

#include "doctest/doctest.h"
#include "game/event_serialization.h"
#include "game/leader_serialization.h"
#include "game/notice_serialization.h"
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
            SERIAL::Write_Event(writer, *roster, event, type, wide);
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
        bytes.push_back(0xa5);
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

    v8.back() ^= 1;
    DOCTEST_REQUIRE(turn_oracle::Write_File(actual, v8, &error));
    DOCTEST_CHECK_FALSE(turn_oracle::Compare_Files(canonical, actual, &error));
    DOCTEST_CHECK(error.find("first difference at byte") != std::string::npos);

    v7.resize(605);
    DOCTEST_REQUIRE(turn_oracle::Write_File(canonical, v7, &error));
    DOCTEST_CHECK_FALSE(turn_oracle::Compare_Files(canonical, actual, &error));
    DOCTEST_CHECK(error.find("invalid save prefix, planet section, or leader section") != std::string::npos);
}
