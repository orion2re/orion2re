#include "pch.h"

#include "doctest/doctest.h"
#include "game/bill.h"

DOCTEST_TEST_CASE("population history includes freighted population") {
    struct s_history_fixture {
        s_player player = {};
        s_player* saved_players = MOX::player;
        int16_t saved_num_players = MOX::NUM_PLAYERS;
        int16_t saved_num_ships = MOX::NUM_SHIPS;
        int16_t saved_num_colonies = MOX::NUM_COLONIES;
        int16_t saved_history[6];

        s_history_fixture() {
            memcpy(saved_history, MOX::bill_savegame, sizeof(saved_history));
            MOX::player = &player;
            MOX::NUM_PLAYERS = 1;
            MOX::NUM_SHIPS = 0;
            MOX::NUM_COLONIES = 0;
            const int16_t history[6] = {2, 7, 1, 1, 1, 1};
            memcpy(MOX::bill_savegame, history, sizeof(history));
        }

        ~s_history_fixture() {
            MOX::player = saved_players;
            MOX::NUM_PLAYERS = saved_num_players;
            MOX::NUM_SHIPS = saved_num_ships;
            MOX::NUM_COLONIES = saved_num_colonies;
            memcpy(MOX::bill_savegame, saved_history, sizeof(saved_history));
        }
    } fixture;
    MOX::player[0].total_pop = 12;
    MOX::player[0].settlers_freighted = 3;

    BILL::Record_History();

    DOCTEST_CHECK(MOX::player[0].population_history[7] == 15);
    DOCTEST_CHECK(MOX::bill_savegame[1] == 8);
}