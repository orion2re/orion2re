#include "pch.h"

#include <cstring>

#include "doctest/doctest.h"
#include "game/info.h"
#include "game/list.h"
#include "game/raceopt.h"

namespace {
    struct s_expected_review {
        int16_t review_idx;
        uint16_t entry_count;
        uint32_t hash;
    };

    constexpr s_expected_review kExpectedReviews[] = {
        {0, 59, 0x97EDD57D},
        {1, 52, 0x571D60D9},
        {2, 45, 0x7574C917},
        {3, 79, 0xD2055110},
    };

    uint32_t Review_Hash_(const s_tech_review_achievement* review, uint16_t entry_count) {
        uint32_t hash = 2166136261u;

        for (uint16_t i = 0; i < entry_count; ++i) {
            uint8_t id = review[i].group < TECH_REVIEW_GROUP_COUNT ?
                review[i].group : static_cast<uint8_t>(review[i].application);
            uint8_t flags = review[i].group < TECH_REVIEW_GROUP_COUNT ? 4 :
                review[i].application == TECH_APP_NO_TECH ? 8 : 2;
            hash = (hash ^ id) * 16777619u;
            hash = (hash ^ flags) * 16777619u;
        }

        return hash;
    }

    bool Review_Contains_App_(const s_tech_review_achievement* review, uint16_t entry_count, uint8_t app_id) {
        for (uint16_t i = 0; i < entry_count; ++i) {
            if (review[i].group == TECH_REVIEW_GROUP_NONE && review[i].application == app_id) {
                return true;
            }
        }

        return false;
    }
}

DOCTEST_TEST_CASE("tech review categories use the 1.51 ordering") {
    for (const s_expected_review& expected : kExpectedReviews) {
        const s_tech_review_achievement* review = INFO::review_list[expected.review_idx];

        DOCTEST_CHECK(Review_Hash_(review, expected.entry_count) == expected.hash);
        DOCTEST_CHECK(review[expected.entry_count - 1].application == TECH_APP_NO_TECH);
        DOCTEST_CHECK(review[expected.entry_count - 1].group == TECH_REVIEW_GROUP_NONE);
        DOCTEST_CHECK(INFO::list_sizes[expected.review_idx] == 256 * sizeof(s_list_item));
    }
}

DOCTEST_TEST_CASE("galactic currency exchange appears in general instead of buildings") {
    DOCTEST_CHECK(Review_Contains_App_(
        INFO::review_list[0], INFO::TECH_REVIEW_ACHIEVEMENT_COUNT, TECH_APP_GALACTIC_CURRENCY_EXCHANGE
    ));
    DOCTEST_CHECK_FALSE(Review_Contains_App_(
        INFO::review_list[1], INFO::TECH_REVIEW_COLONY_COUNT, TECH_APP_GALACTIC_CURRENCY_EXCHANGE
    ));
}

DOCTEST_TEST_CASE("tractor beam appears in weapons instead of equipment") {
    DOCTEST_CHECK(Review_Contains_App_(
        INFO::review_list[2], INFO::TECH_REVIEW_WEAPON_COUNT, TECH_APP_TRACTOR_BEAM
    ));
    DOCTEST_CHECK_FALSE(Review_Contains_App_(
        INFO::review_list[3], INFO::TECH_REVIEW_EQUIPMENT_COUNT, TECH_APP_TRACTOR_BEAM
    ));
}

DOCTEST_TEST_CASE("race numeric traits use signed half units and BC labels") {
    char output[16];

    raceopt::Format_Numeric_Trait(
        output, sizeof(output), TRAIT_FOOD_BONUS, -1
    );
    DOCTEST_CHECK(std::strcmp(output, "-0.5") == 0);

    raceopt::Format_Numeric_Trait(
        output, sizeof(output), TRAIT_FOOD_BONUS, 4
    );
    DOCTEST_CHECK(std::strcmp(output, "+2") == 0);

    raceopt::Format_Numeric_Trait(
        output, sizeof(output), TRAIT_TAX_BONUS, 1
    );
    DOCTEST_CHECK(std::strcmp(output, "+0.5 BC") == 0);

    raceopt::Format_Numeric_Trait(
        output, sizeof(output), TRAIT_PRODUCTION_BONUS, -2
    );
    DOCTEST_CHECK(std::strcmp(output, "-2") == 0);
}

DOCTEST_TEST_CASE("race statistics layout shrinks as the trait list grows") {
    int8_t traits[TRAIT_COUNT] = {};

    for (int i = 1; i <= 10; ++i) {
        traits[i] = 1;
    }
    raceopt::s_race_stat_layout layout =
        raceopt::Race_Stat_Layout(traits);
    DOCTEST_CHECK(layout.font_style == 2);
    DOCTEST_CHECK(layout.line_step == 13);

    traits[11] = 1;
    layout = raceopt::Race_Stat_Layout(traits);
    DOCTEST_CHECK(layout.font_style == 1);
    DOCTEST_CHECK(layout.line_step == 11);

    traits[12] = 1;
    traits[13] = 1;
    layout = raceopt::Race_Stat_Layout(traits);
    DOCTEST_CHECK(layout.font_style == 0);
    DOCTEST_CHECK(layout.line_step == 7);
}