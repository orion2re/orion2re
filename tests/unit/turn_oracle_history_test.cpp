#include "pch.h"

#include <chrono>

#include "doctest/doctest.h"
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

DOCTEST_TEST_CASE("oracle compares v7 saves with their v8 leader IDs without masking differences") {
    s_history_fixture fixture;
    const auto canonical = turn_oracle::Turn_Path(fixture.directory, 0);
    const auto actual = turn_oracle::Turn_Path(fixture.directory, 1);
    std::string error;

    // A versioned header, settings, seed, and one record in each leading section.
    std::vector<uint8_t> v7(605, 0);
    v7[0] = 0xe1;
    v7[46] = 7;
    for (size_t width : {0x16a, 0x12, 0xea}) {
        v7.insert(v7.end(), {1, 0});
        v7.insert(v7.end(), width, 0xa5);
    }
    auto v8 = v7;
    v8[46] = 8;
    const size_t leader_offset = v8.size();
    for (uint8_t slot = 0; slot < 67; ++slot) {
        v8.insert(v8.end(), {slot, 0, 0, 0});
        v7.insert(v7.end(), 59, slot);
        v8.insert(v8.end(), 59, slot);
    }
    v7.insert(v7.end(), {0x34, 0x12});
    v8.insert(v8.end(), {0x34, 0x12});

    DOCTEST_REQUIRE(turn_oracle::Write_File(canonical, v7, &error));
    DOCTEST_REQUIRE(turn_oracle::Write_File(actual, v8, &error));
    DOCTEST_CHECK(turn_oracle::Compare_Files(canonical, actual, &error));
    DOCTEST_CHECK(turn_oracle::Compare_Files(actual, canonical, &error));

    for (size_t offset : {leader_offset, leader_offset + 4, v8.size() - 1}) {
        v8[offset] ^= 1;
        DOCTEST_REQUIRE(turn_oracle::Write_File(actual, v8, &error));
        DOCTEST_CHECK_FALSE(turn_oracle::Compare_Files(canonical, actual, &error));
        DOCTEST_CHECK(error.find("first difference at byte " + std::to_string(offset)) != std::string::npos);
        v8[offset] ^= 1;
    }

    v7.resize(leader_offset);
    DOCTEST_REQUIRE(turn_oracle::Write_File(canonical, v7, &error));
    DOCTEST_CHECK_FALSE(turn_oracle::Compare_Files(canonical, actual, &error));
    DOCTEST_CHECK(error.find("invalid v7 save prefix or leader section") != std::string::npos);
}
