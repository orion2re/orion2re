#include "pch.h"
#include <memory>
#include "../fixtures/ship_rows.h"

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

    MAINPUPS::Leader_Technology_String(game::id_player{0}, 46, output, sizeof(output));
    DOCTEST_CHECK(strcmp(output, "Federation") == 0);

    MAINPUPS::Leader_Technology_String(game::id_player{0}, 47, output, sizeof(output));
    DOCTEST_CHECK(output[0] == '\0');
}

DOCTEST_TEST_CASE("defense popup colony lookup uses star and player keys and includes outposts") {
    s_leader_text_fixture fixture;
    auto& state = game::current_state;
    constexpr game::id_star star{90000};
    constexpr game::id_planet planet{80000};
    constexpr game::id_colony colony{70000};
    constexpr game::id_player player{1};
    state.stars[star].orbit_planet_ids[0] = planet;
    state.planets[planet].colony_id = colony;
    state.colonies[colony].owner = player;
    state.colonies[colony].outpost_flag = 1;

    DOCTEST_CHECK(MAINPUPS::Player_Has_Colony_Or_Outpost_At_Star(player, star) == 1);
    DOCTEST_CHECK(MAINPUPS::Player_Has_Colony_Or_Outpost_At_Star(game::id_player{0}, star) == 0);
    DOCTEST_CHECK(MAINPUPS::Player_Has_Colony_Or_Outpost_At_Star(game::NO_PLAYER, star) == 0);
    DOCTEST_CHECK(MAINPUPS::Player_Has_Colony_Or_Outpost_At_Star(player, game::NO_STAR) == 0);
}

DOCTEST_TEST_CASE("diplomacy system scan uses the supplied state and preserves the NPC owner fallback") {
    auto state = std::make_unique<game::s_game_state>();
    constexpr game::id_star system{90000};
    constexpr game::id_player giver{1};
    constexpr game::id_player other{2};
    constexpr game::owner_slot guardian{9};
    constexpr game::id_planet first{80003}, second{80001}, foreign{80002};
    constexpr game::id_colony first_colony{70001}, second_colony{70002}, foreign_colony{70003};
    auto& star = state->stars[system];
    star.owner = guardian;
    star.has_colony.set(giver);
    star.orbit_planet_ids[0] = first;
    star.orbit_planet_ids[2] = foreign;
    star.orbit_planet_ids[4] = second;
    state->planets[first].colony_id = first_colony;
    state->planets[second].colony_id = second_colony;
    state->planets[foreign].colony_id = foreign_colony;
    state->colonies[first_colony].owner = giver;
    state->colonies[second_colony].owner = giver;
    state->colonies[second_colony].buildings[BUILDING_CAPITOL] = 1;
    state->colonies[foreign_colony].owner = other;

    game::id_planet capitol;
    int16_t count = 0;
    game::id_planet planets[5]{};
    DOCTEST_CHECK(DIP_SCRN::Diplomacy_System_Scan(*state, system, giver, &capitol, &count, planets) == factions::From_Player(giver));
    DOCTEST_REQUIRE(count == 2);
    DOCTEST_CHECK(planets[0] == first);
    DOCTEST_CHECK(planets[1] == second);
    DOCTEST_CHECK(capitol == second);

    DOCTEST_CHECK(DIP_SCRN::Diplomacy_System_Scan(*state, system, game::id_player{0}, &capitol, &count, planets) == guardian);
    DOCTEST_CHECK(count == 0);
    DOCTEST_CHECK(capitol == game::NO_PLANET);
    DOCTEST_CHECK(DIP_SCRN::Diplomacy_System_Scan(*state, game::NO_STAR, giver, &capitol, &count, planets) == game::NO_OWNER);
}

DOCTEST_TEST_CASE("captain ETA follows the original ship key across roster changes") {
    s_leader_text_fixture fixture;
    auto& state = game::current_state;
    ship_slots::Resize(state, 2);
    state.next_ship_id = 90003;
    test_ships::Rows(state).resize(2);
    const game::id_ship source{90000};
    test_ships::Rows(state).Replace_Row(0, s_ship_data{.x = 40, .y = 12, .id = source});
    test_ships::Rows(state).Replace_Row(1, s_ship_data{.x = 80, .y = 16, .id = game::id_ship{90001}});
    s_prev_leader_data previous{};
    previous.status = 1;
    previous.eta = 2;
    previous.ship_location = source;
    test::s_saved_value saved_snapshot{OFFICER::prev_leaders};
    OFFICER::prev_leaders = &previous;
    const s_ship_data target{.x = 40, .y = 12};

    test_ships::Rows(state).Swap_Rows(0, 1);
    DOCTEST_CHECK(OFFICER::Get_Ship_Leader_ETA(state, state.players[0], 0, target) == 2);
    test_ships::Rows(state).Set_Key_At_Row(1, game::id_ship{90002});
    DOCTEST_CHECK(OFFICER::Get_Ship_Leader_ETA(state, state.players[0], 0, target) == 5);
}
