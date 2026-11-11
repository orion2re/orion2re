#include "pch.h"

#include <cstring>

#include "doctest/doctest.h"
#include "game/racesel.h"

namespace TEST_RACESEL_DIGEST {
    void Reset_();
    const char* Captured_Paragraph_();
}

DOCTEST_TEST_CASE("race selection digest retains every configured trait") {
    struct s_selection_scope {
        uint8_t* saved = RACESEL::race_specials[STOCK_RACE_ALKARI];
        int16_t saved_custom = RACESEL::custom_flag;
        int16_t saved_displayed = RACESEL::displayed_race;
        int16_t saved_old = RACESEL::old_race;
        decltype(RACESEL::race_flag) saved_flags;
        decltype(RACESEL::race_button_status) saved_buttons;

        s_selection_scope() {
            memcpy(saved_flags, RACESEL::race_flag, sizeof(saved_flags));
            memcpy(saved_buttons, RACESEL::race_button_status, sizeof(saved_buttons));
        }

        ~s_selection_scope() {
            RACESEL::race_specials[STOCK_RACE_ALKARI] = saved;
            RACESEL::custom_flag = saved_custom;
            RACESEL::displayed_race = saved_displayed;
            RACESEL::old_race = saved_old;
            memcpy(RACESEL::race_flag, saved_flags, sizeof(saved_flags));
            memcpy(RACESEL::race_button_status, saved_buttons, sizeof(saved_buttons));
        }
    };
    RACESEL::s_stock_race configured_race = {
        .government = GOVERNMENT_UNIFICATION,
        .growth = 1,
        .farming = 1,
        .industry = 1,
        .science = 1,
        .money = 1,
        .defense = 1,
        .attack = 1,
        .ground = 1,
        .spying = 1,
        .lowg_world = 1,
        .highg_world = 1,
        .aquatic = 1,
        .subterranean = 1,
        .large_hw = 1,
        .hw_richness = -1,
        .arti_world = 1,
        .cybernetic = 1,
        .lithovore = 1,
        .repulsive = 1,
        .charismatic = 1,
        .uncreative = 1,
        .creative = 1,
        .tolerant = 1,
        .fantastic_traders = 1,
        .telepathic = 1,
        .lucky = 1,
        .omniscient = 1,
        .stealthy_ships = 1,
        .trans_dimensional = 1,
        .warlord = 1,
    };
    s_selection_scope selection_scope;

    TEST_RACESEL_DIGEST::Reset_();
    memset(RACESEL::race_flag, 0, sizeof(RACESEL::race_flag));
    RACESEL::custom_flag = 0;
    RACESEL::displayed_race = STOCK_RACE_ALKARI;
    RACESEL::old_race = STOCK_RACE_ALKARI;
    RACESEL::race_specials[STOCK_RACE_ALKARI] =
        reinterpret_cast<uint8_t*>(&configured_race);

    RACESEL::Draw_Race_Selection_Screen();

    const char* paragraph = TEST_RACESEL_DIGEST::Captured_Paragraph_();
    DOCTEST_CHECK(std::strlen(paragraph) > 200);
    DOCTEST_CHECK(std::strstr(paragraph, "Poor Home World") != nullptr);
    DOCTEST_CHECK(
        std::strstr(
            paragraph,
            "Stealthy Ships, Trans-Dimensional, Warlord, and Unification"
        ) != nullptr
    );
}

DOCTEST_TEST_CASE("race digest separators distinguish the first middle and last traits") {
    TEST_RACESEL_DIGEST::Reset_();
    char output[64] = {};
    int16_t count = 0;
    RACESEL::Message_Build(&count, 3, output, sizeof(output));
    DOCTEST_CHECK(count == 1);
    DOCTEST_CHECK(output[0] == '\0');
    strcpy(output, "First");
    RACESEL::Message_Build(&count, 3, output, sizeof(output));
    DOCTEST_CHECK(strcmp(output, "First, ") == 0);
    strcat(output, "Second");
    RACESEL::Message_Build(&count, 3, output, sizeof(output));
    DOCTEST_CHECK(count == 3);
    DOCTEST_CHECK(strcmp(output, "First, Second, and ") == 0);

    char tiny[1] = {};
    count = 1;
    RACESEL::Message_Build(&count, 2, tiny, sizeof(tiny));
    DOCTEST_CHECK(tiny[0] == '\0');
}