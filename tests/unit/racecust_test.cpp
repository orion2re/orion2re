#include "pch.h"

#include "doctest/doctest.h"
#include "game/racecust.h"

namespace TEST_RACECUST {
    void Reset_();
    void Set_Random_Result_(uint32_t result);
    uint32_t Random_Max_();
    void Set_Race_And_Difficulty_(uint8_t race, uint8_t difficulty);
}

DOCTEST_TEST_CASE("AI race customization selects from the configured variant list") {
    TEST_RACECUST::Reset_();
    TEST_RACECUST::Set_Random_Result_(1);
    RACECUST::ai_race_variants[0] = 4;
    RACECUST::ai_race_variants[1] = 5;
    RACECUST::ai_race_variants[2] = 0;

    RACECUST::Race_Customization(game::current_state.players[0]);

    DOCTEST_CHECK(TEST_RACECUST::Random_Max_() == 2);
    DOCTEST_CHECK(game::current_state.players[0].traits[TRAIT_CYBERNETIC] == 1);
}

DOCTEST_TEST_CASE("AI race customization can reach a noncontiguous fifth variant") {
    TEST_RACECUST::Reset_();
    TEST_RACECUST::Set_Random_Result_(4);
    const uint8_t variants[RACECUST::AI_RACE_VARIANT_LIST_SIZE] = {1, 2, 3, 5};
    memcpy(RACECUST::ai_race_variants, variants, sizeof(variants));

    RACECUST::Race_Customization(game::current_state.players[0]);

    DOCTEST_CHECK(TEST_RACECUST::Random_Max_() == 4);
    DOCTEST_CHECK(game::current_state.players[0].traits[TRAIT_PRODUCTION_BONUS] == 2);
}

DOCTEST_TEST_CASE("corrected AI race defaults apply through runtime row selection") {
    DOCTEST_CHECK(RACECUST::variants.customization[STOCK_RACE_ALKARI][0][4]
                      .count == 3);
    DOCTEST_CHECK(RACECUST::variants.customization[STOCK_RACE_ALKARI][0][4]
                      .items[3].trait_idx == 0);
    DOCTEST_CHECK(RACECUST::variants.customization[STOCK_RACE_ALKARI][0][4]
                      .items[3].trait_val == 0);
    DOCTEST_CHECK(RACECUST::variants.evolution[STOCK_RACE_BULRATHI][1][0]
                      .count == 1);
    DOCTEST_CHECK(RACECUST::variants.evolution[STOCK_RACE_BULRATHI][1][0]
                      .items[0].trait_idx ==
                  TRAIT_SHIP_DEFENSE);
    DOCTEST_CHECK(RACECUST::variants.evolution[STOCK_RACE_BULRATHI][1][0]
                      .items[0].trait_val == 3);
    DOCTEST_CHECK(RACECUST::variants.evolution[STOCK_RACE_BULRATHI][1][0]
                      .items[1].trait_idx == 0);
    DOCTEST_CHECK(RACECUST::variants.evolution[STOCK_RACE_BULRATHI][1][0]
                      .items[1].trait_val == 0);
    DOCTEST_CHECK(RACECUST::variants.evolution[STOCK_RACE_ELERIAN][0][3]
                      .items[0].trait_val == 3);
    DOCTEST_CHECK(RACECUST::variants.evolution[STOCK_RACE_ELERIAN][0][4]
                      .items[0].trait_val == 3);
    DOCTEST_CHECK(RACECUST::variants.evolution[STOCK_RACE_ELERIAN][1][2]
                      .items[0].trait_val == 3);

    struct s_variant_case {
        uint8_t race;
        uint8_t difficulty;
        uint8_t variant;
    };
    const s_variant_case elerian_cases[] = {
        {STOCK_RACE_ELERIAN, GAME_DIFFICULTY_HARD, 4},
        {STOCK_RACE_ELERIAN, GAME_DIFFICULTY_HARD, 5},
        {STOCK_RACE_ELERIAN, GAME_DIFFICULTY_IMPOSSIBLE, 3},
    };

    TEST_RACECUST::Reset_();
    TEST_RACECUST::Set_Race_And_Difficulty_(
        STOCK_RACE_ALKARI, GAME_DIFFICULTY_HARD);
    RACECUST::ai_race_variants[0] = 5;
    RACECUST::ai_race_variants[1] = 0;
    RACECUST::Race_Customization(game::current_state.players[0]);
    DOCTEST_CHECK(game::current_state.players[0].traits[TRAIT_CURRENT_GOVERNMENT] ==
                  GOVERNMENT_FEUDAL);
    DOCTEST_CHECK(game::current_state.players[0].traits[TRAIT_SHIP_ATTACK] == 2);
    DOCTEST_CHECK(game::current_state.players[0].traits[TRAIT_PRODUCTION_BONUS] == 2);

    TEST_RACECUST::Reset_();
    TEST_RACECUST::Set_Race_And_Difficulty_(
        STOCK_RACE_BULRATHI, GAME_DIFFICULTY_IMPOSSIBLE);
    RACECUST::ai_race_variants[0] = 1;
    RACECUST::ai_race_variants[1] = 0;
    RACECUST::Race_Customization(game::current_state.players[0]);
    DOCTEST_CHECK(game::current_state.players[0].evolutionary_upgrade_traits[
                      TRAIT_SHIP_DEFENSE] == 3);
    DOCTEST_CHECK(game::current_state.players[0].evolutionary_upgrade_traits[
                      TRAIT_CURRENT_GOVERNMENT] == 0);

    for (const s_variant_case& test_case : elerian_cases) {
        TEST_RACECUST::Reset_();
        TEST_RACECUST::Set_Race_And_Difficulty_(
            test_case.race, test_case.difficulty);
        RACECUST::ai_race_variants[0] = test_case.variant;
        RACECUST::ai_race_variants[1] = 0;
        RACECUST::Race_Customization(game::current_state.players[0]);
        DOCTEST_CHECK(game::current_state.players[0].evolutionary_upgrade_traits[
                          TRAIT_SHIP_DEFENSE] == 3);
    }
}
