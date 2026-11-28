#include "pch.h"

#include "doctest/doctest.h"
#include "game/bill.h"

DOCTEST_TEST_CASE("population history includes freighted population") {
    struct s_history_fixture {
        s_player& player = game::current_state.players[0];
        decltype(game::current_state.players) saved_players = game::current_state.players;
        int16_t saved_num_players = game::current_state.player_count;
        int16_t saved_num_ships = static_cast<int16_t>(game::current_state.ship_slots.size());
        int16_t saved_history[6];

        s_history_fixture() {
            memcpy(saved_history, MOX::bill_savegame, sizeof(saved_history));
            game::current_state.players = player_roster::Make_Roster();
            game::current_state.player_count = 1;
            ship_slots::Resize(game::current_state, 0);
            const int16_t history[6] = {2, 7, 1, 1, 1, 1};
            memcpy(MOX::bill_savegame, history, sizeof(history));
        }

        ~s_history_fixture() {
            game::current_state.players = saved_players;
            game::current_state.player_count = saved_num_players;
            ship_slots::Resize(game::current_state, saved_num_ships);
            memcpy(MOX::bill_savegame, saved_history, sizeof(saved_history));
        }
    } fixture;
    game::current_state.players[0].total_pop = 12;
    game::current_state.players[0].settlers_freighted = 3;

    BILL::Record_History();

    DOCTEST_CHECK(game::current_state.players[0].population_history[7] == 15);
    DOCTEST_CHECK(MOX::bill_savegame[1] == 8);
}
