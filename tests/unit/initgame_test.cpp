#include "pch.h"

#include "doctest/doctest.h"
#include "game/initgame.h"
#include "game/newgame.h"
#include "game/newgame_options.h"
#include "game/random.h"

namespace TEST_NEWGAME {
    void Reset_Player_Setup_Order_();
    int Technology_Setup_Order_();
    int Player_Technology_Order_();
    int Player_Setup_Order_();
    int Ship_Design_Order_();
} // namespace TEST_NEWGAME

DOCTEST_TEST_CASE("fixed new-game player count is unchanged") {
    s_player players[MAX_PLAYERS] = {};
    game_random::Set_Random_Seed(0x12345678);
    const uint32_t seed = game_random::Get_Random_Seed();

    for (int16_t count = 2; count <= MAX_PLAYERS; ++count) {
        DOCTEST_CHECK(INITGAME::Resolve_New_Game_Player_Count(count, players) == count);
        DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
    }
}

DOCTEST_TEST_CASE("invalid setup counts use a fixed fallback without resolving Random") {
    const uint32_t prior_seed = game_random::Get_Random_Seed();
    constexpr uint32_t seed = 0x12345678;
    game_random::Set_Random_Seed(seed);
    const int invalid_fallbacks[] = {INT16_MIN, -1, 0, 1, NEW_GAME_RANDOM_PLAYER_COUNT, INT16_MAX};
    for (int count = INT8_MIN; count <= INT8_MAX; ++count) {
        DOCTEST_CAPTURE(count);
        const bool valid = (count >= 2 && count <= MAX_PLAYERS) ||
            count == NEW_GAME_RANDOM_PLAYER_COUNT;
        DOCTEST_CHECK(INITGAME::Normalize_New_Game_Player_Count(count, 6) == (valid ? count : 6));
        for (int fallback : invalid_fallbacks) {
            DOCTEST_CHECK(INITGAME::Normalize_New_Game_Player_Count(count, fallback) == (valid ? count : 5));
        }
        DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
    }
    for (int16_t fallback = 2; fallback <= MAX_PLAYERS; ++fallback) {
        DOCTEST_CHECK(INITGAME::Normalize_New_Game_Player_Count(0, fallback) == fallback);
    }
    game_random::Set_Random_Seed(prior_seed);
}

DOCTEST_TEST_CASE("new-game player selector safely round-trips every stored count byte") {
    const s_settings prior = MOX::settings;
    const uint32_t prior_seed = game_random::Get_Random_Seed();
    constexpr uint32_t seed = 0x12345678;
    game_random::Set_Random_Seed(seed);
    for (int count = INT8_MIN; count <= INT8_MAX; ++count) {
        DOCTEST_CAPTURE(count);
        MOX::settings.number_of_players = (int8_t)count;
        const bool valid = (count >= 2 && count <= MAX_PLAYERS) ||
            count == NEW_GAME_RANDOM_PLAYER_COUNT;
        const int expected = valid ? count : 5;
        NEWGAME::Globals_To_Temps();
        DOCTEST_CHECK(NEWGAME::Random_Player_Count_Selected() ==
            (expected == NEW_GAME_RANDOM_PLAYER_COUNT));
        DOCTEST_CHECK(NEWGAME::opp_button_variable >= 0);
        DOCTEST_CHECK(NEWGAME::opp_button_variable < MAX_PLAYERS);
        DOCTEST_CHECK(NEWGAME::opp_button_variable ==
            (expected == NEW_GAME_RANDOM_PLAYER_COUNT ? MAX_PLAYERS - 1 : expected - 2));
        NEWGAME::Temps_To_Globals();
        DOCTEST_CHECK(NEWGAME::Random_Player_Count_Selected() ==
            (expected == NEW_GAME_RANDOM_PLAYER_COUNT));
        DOCTEST_CHECK(MOX::settings.number_of_players == expected);
        DOCTEST_CHECK(NEWGAME::Player_Count_For_Initialization() == expected);
        DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
    }
    MOX::settings = prior;
    NEWGAME::Globals_To_Temps();
    game_random::Set_Random_Seed(prior_seed);
}

DOCTEST_TEST_CASE("cached Random selection is independent of the persisted player count") {
    struct s_options_scope {
        s_settings saved_settings = MOX::settings;
        bool saved_random = NEWGAME::Random_Player_Count_Selected();
        uint16_t saved_combat = NEWGAME::combat_button_variable;
        int16_t saved_selectors[7] = {
            NEWGAME::opp_button_variable, NEWGAME::diff_button_variable,
            NEWGAME::xenon_attacks_button_variable, NEWGAME::civ_button_variable,
            NEWGAME::gal_size_button_variable, NEWGAME::random_events_button_variable,
            NEWGAME::gal_age_button_variable,
        };

        ~s_options_scope() {
            // The cached choice can differ from the persisted count after setup.
            NEWGAME::opp_button_variable = saved_random ? NEWGAME::NEW_GAME_PLAYER_COUNT_RANDOM : 0;
            NEWGAME::Temps_To_Globals();
            NEWGAME::opp_button_variable = saved_selectors[0];
            NEWGAME::diff_button_variable = saved_selectors[1];
            NEWGAME::xenon_attacks_button_variable = saved_selectors[2];
            NEWGAME::civ_button_variable = saved_selectors[3];
            NEWGAME::gal_size_button_variable = saved_selectors[4];
            NEWGAME::random_events_button_variable = saved_selectors[5];
            NEWGAME::gal_age_button_variable = saved_selectors[6];
            NEWGAME::combat_button_variable = saved_combat;
            MOX::settings = saved_settings;
        }
    } scope;

    MOX::settings.number_of_players = NEW_GAME_RANDOM_PLAYER_COUNT;
    NEWGAME::Globals_To_Temps();
    MOX::settings.number_of_players = MAX_PLAYERS;
    DOCTEST_CHECK(NEWGAME::Random_Player_Count_Selected());
    DOCTEST_CHECK(NEWGAME::Player_Count_For_Initialization() == NEW_GAME_RANDOM_PLAYER_COUNT);

    NEWGAME::Globals_To_Temps();
    MOX::settings.number_of_players = NEW_GAME_RANDOM_PLAYER_COUNT;
    DOCTEST_CHECK_FALSE(NEWGAME::Random_Player_Count_Selected());
    DOCTEST_CHECK(NEWGAME::Player_Count_For_Initialization() == NEW_GAME_RANDOM_PLAYER_COUNT);
}

DOCTEST_TEST_CASE("Random handles a zero bound without advancing the seed") {
    constexpr uint32_t seed = 0x12345678;
    game_random::Set_Random_Seed(seed);

    DOCTEST_CHECK(game_random::Random(0) == 1);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
}

DOCTEST_TEST_CASE("single-player Random chooses from two through eight players") {
    s_player players[MAX_PLAYERS] = {};
    players[0].objectives = PLAYER_OBJECTIVE_HUMAN;

    constexpr uint32_t seed = 0x13572468;
    game_random::Set_Random_Seed(seed);
    const int16_t expected = 2 + (int16_t)game_random::Random(7) - 1;

    game_random::Set_Random_Seed(seed);
    DOCTEST_CHECK(
        INITGAME::Resolve_New_Game_Player_Count(
            NEW_GAME_RANDOM_PLAYER_COUNT, players) == expected);
}

DOCTEST_TEST_CASE("Random preserves existing human player slots") {
    s_player players[MAX_PLAYERS] = {};
    for (int16_t player_idx = 0; player_idx < 3; ++player_idx) {
        players[player_idx].objectives = PLAYER_OBJECTIVE_HUMAN;
    }

    constexpr uint32_t seed = 0x24681357;
    game_random::Set_Random_Seed(seed);
    const int16_t expected = 3 + (int16_t)game_random::Random(6) - 1;

    game_random::Set_Random_Seed(seed);
    DOCTEST_CHECK(
        INITGAME::Resolve_New_Game_Player_Count(
            NEW_GAME_RANDOM_PLAYER_COUNT, players) == expected);
}

DOCTEST_TEST_CASE("Random remains bounded when seven human slots are occupied") {
    s_player players[MAX_PLAYERS] = {};
    for (int16_t player_idx = 0; player_idx < MAX_PLAYERS - 1; ++player_idx) {
        players[player_idx].objectives = PLAYER_OBJECTIVE_HUMAN;
    }

    DOCTEST_CHECK(
        INITGAME::Resolve_New_Game_Player_Count(
            NEW_GAME_RANDOM_PLAYER_COUNT, players) == MAX_PLAYERS);
}

DOCTEST_TEST_CASE("new-game galaxy selector omits Cluster and converts legacy settings") {
    struct s_case {
        uint8_t setting;
        int16_t button;
    };
    const s_case settings[] = {
        {GALAXY_SIZE_SMALL, 0},
        {GALAXY_SIZE_MEDIUM, 1},
        {GALAXY_SIZE_LARGE, 2},
        {GALAXY_SIZE_HUGE, 3},
        {GALAXY_SIZE_MAXIMUM, 4},
        {GALAXY_SIZE_CLUSTER, 3},
    };

    for (const s_case& test_case : settings) {
        MOX::settings.galaxy_size = test_case.setting;
        NEWGAME::Globals_To_Temps();
        DOCTEST_CHECK(NEWGAME::gal_size_button_variable == test_case.button);
    }

    for (int16_t button = 0; button < 5; ++button) {
        NEWGAME::gal_size_button_variable = button;
        NEWGAME::Temps_To_Globals();
        DOCTEST_CHECK(MOX::settings.galaxy_size == (uint8_t)button);
    }
}

DOCTEST_TEST_CASE("player initialization assigns the configured signed-safe treasury") {
    const INITGAME::s_start saved = INITGAME::start;
    const uint32_t money[] = {0, 2147483647};

    for (uint32_t starting_money : money) {
        memset(MOX::player, 0, sizeof(s_player));
        MOX::NUM_PLAYERS = 1;
        MOX::PLAYER_NUM = 0;
        MOX::player[0].objectives = PLAYER_OBJECTIVE_HUMAN;
        INITGAME::start.money = starting_money;

        INITGAME::Init_Players();

        DOCTEST_CHECK((MOX::player[0].bc == (int32_t)starting_money));
    }

    INITGAME::start = saved;
}

DOCTEST_TEST_CASE("player initialization resolves tech-level starting money") {
    const INITGAME::s_start saved_start = INITGAME::start;
    INITGAME::s_tech_start saved_tech_start[4];
    memcpy(saved_tech_start, INITGAME::tech_start, sizeof(saved_tech_start));
    const int8_t saved_level = MOX::settings.starting_civilization_level;

    INITGAME::start.money = 37;
    const int32_t money[4] = {0, 50, 100, 2147483647};
    for (int level = STARTING_CIVILIZATION_LEVEL_PRE_WARP;
         level < STARTING_CIVILIZATION_LEVEL_COUNT;
         ++level) {
        memset(MOX::player, 0, sizeof(s_player));
        MOX::NUM_PLAYERS = 1;
        MOX::PLAYER_NUM = 0;
        MOX::player[0].objectives = PLAYER_OBJECTIVE_HUMAN;
        MOX::settings.starting_civilization_level = (int8_t)level;
        INITGAME::tech_start[level].money = money[level];

        INITGAME::Init_Players();

        DOCTEST_CHECK((MOX::player[0].bc == money[level]));
    }

    INITGAME::tech_start[STARTING_CIVILIZATION_LEVEL_AVERAGE].money = -1;
    const int invalid_levels[] = {
        STARTING_CIVILIZATION_LEVEL_AVERAGE,
        -1,
        STARTING_CIVILIZATION_LEVEL_COUNT,
    };
    for (int level : invalid_levels) {
        memset(MOX::player, 0, sizeof(s_player));
        MOX::NUM_PLAYERS = 1;
        MOX::PLAYER_NUM = 0;
        MOX::player[0].objectives = PLAYER_OBJECTIVE_HUMAN;
        MOX::settings.starting_civilization_level = (int8_t)level;

        INITGAME::Init_Players();

        DOCTEST_CHECK((MOX::player[0].bc == 37));
    }

    INITGAME::start = saved_start;
    memcpy(INITGAME::tech_start, saved_tech_start, sizeof(saved_tech_start));
    MOX::settings.starting_civilization_level = saved_level;
}

DOCTEST_TEST_CASE("technology and player setup preserve new-game lifecycle order") {
    memset(MOX::player, 0, sizeof(s_player));
    MOX::NUM_PLAYERS = 1;
    MOX::PLAYER_NUM = 0;
    MOX::player[0].objectives = PLAYER_OBJECTIVE_HUMAN;
    TEST_NEWGAME::Reset_Player_Setup_Order_();

    INITGAME::Init_Players();

    DOCTEST_CHECK(TEST_NEWGAME::Technology_Setup_Order_() == 1);
    DOCTEST_CHECK(TEST_NEWGAME::Player_Technology_Order_() == 2);
    DOCTEST_CHECK(TEST_NEWGAME::Player_Setup_Order_() == 3);
    DOCTEST_CHECK(TEST_NEWGAME::Ship_Design_Order_() == 4);
}