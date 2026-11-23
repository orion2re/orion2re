#include "pch.h"

#include "doctest/doctest.h"
#include "../fixtures/game_state.h"

namespace {
    struct s_leader_text_fixture {

        game::s_game_state saved_state = std::move(game::current_state);

        decltype(MOX::government_labels) saved_labels;
        decltype(TECHDATA::technology_applications) saved_applications;
        decltype(ESTRINGS::strings) saved_strings;
        decltype(OFFICER::progression) saved_progression = OFFICER::progression;
        char federation[11] = "Federation";
        char empty[1] = "";
        char string_format[3] = "%s";

        s_leader_text_fixture() {
            memcpy(saved_labels, MOX::government_labels, sizeof(saved_labels));
            memcpy(saved_applications, TECHDATA::technology_applications, sizeof(saved_applications));
            memcpy(saved_strings, ESTRINGS::strings, sizeof(saved_strings));
            game::current_state = game::s_game_state{};

            memset(MOX::government_labels, 0, sizeof(MOX::government_labels));
            memset(TECHDATA::technology_applications, 0, sizeof(TECHDATA::technology_applications));
            game::current_state.players[0].traits[TRAIT_CURRENT_GOVERNMENT] = GOVERNMENT_DEMOCRACY;
            MOX::government_labels[GOVERNMENT_FEDERATION] = federation;
            for (auto& text : ESTRINGS::strings) text = empty;
            ESTRINGS::strings[207] = string_format;
        }

        ~s_leader_text_fixture() {
            game::current_state = std::move(saved_state);

            memcpy(MOX::government_labels, saved_labels, sizeof(saved_labels));
            memcpy(TECHDATA::technology_applications, saved_applications, sizeof(saved_applications));
            memcpy(ESTRINGS::strings, saved_strings, sizeof(saved_strings));
            OFFICER::progression = saved_progression;
        }
    };
} // namespace

DOCTEST_TEST_CASE("leader technology text uses the configured government leader") {
    s_leader_text_fixture fixture;
    OFFICER::progression.special_leaders.advanced_government_leader_id = 46;
    char output[80];

    MAINPUPS::Leader_Technology_String(0, 46, output, sizeof(output));
    DOCTEST_CHECK(strcmp(output, "Federation") == 0);

    MAINPUPS::Leader_Technology_String(0, 47, output, sizeof(output));
    DOCTEST_CHECK(output[0] == '\0');
}

DOCTEST_TEST_CASE("captain ETA follows the original ship key across roster changes") {
    s_leader_text_fixture fixture;
    auto& state = game::current_state;
    state.ship_count = 2;
    state.next_ship_id = 90003;
    state.ship_records.resize(2);
    const game::id_ship source{90000};
    state.ship_records.Replace_Row(0, s_ship_data{.x = 40, .y = 12, .id = source});
    state.ship_records.Replace_Row(1, s_ship_data{.x = 80, .y = 16, .id = game::id_ship{90001}});
    s_prev_leader_data previous{};
    previous.status = 1;
    previous.eta = 2;
    previous.ship_location = source;
    test::s_saved_value saved_snapshot{OFFICER::prev_leaders};
    OFFICER::prev_leaders = &previous;
    const s_ship_data target{.x = 40, .y = 12};

    state.ship_records.Swap_Rows(0, 1);
    DOCTEST_CHECK(OFFICER::Get_Ship_Leader_ETA(state, state.players[0], 0, target) == 2);
    state.ship_records.Set_Key_At_Row(1, game::id_ship{90002});
    DOCTEST_CHECK(OFFICER::Get_Ship_Leader_ETA(state, state.players[0], 0, target) == 5);
}
