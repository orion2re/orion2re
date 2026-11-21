#include "pch.h"
#include "../fixtures/colony_roster.h"
#include "../fixtures/planet_roster.h"

#include "doctest/doctest.h"

namespace {
    struct s_colony_navigation_fixture {
        test_colonies::Slots<32> colonies;
        s_star_data stars[1] = {};
        s_player players[MAX_PLAYERS] = {};
        game::id_colony colony_list[MAX_COLONIES];
        std::vector<s_field> field_storage = std::vector<s_field>(250);
        s_animation_header animation = {};
        game::s_game_state saved_planets_saved_roster = game::current_state;
        s_star_data* saved_stars = MOX::star;
        s_player* saved_players = MOX::player;
        int16_t saved_star_count = MOX::NUM_STARS;
        int16_t saved_star_field = MOX::galaxy_map_star_field[0];
        game::id_colony* saved_list = COLXPORT::g_colony_list_ptr;
        s_field* saved_fields = fields::fields;
        decltype(COLSUM::anims) saved_anims;
        char minus_hotkey[2] = "-";
        char plus_hotkey[2] = "+";
        char empty_string[1] = "";
        char* saved_minus_hotkey = ESTRINGS::strings[11];
        char* saved_plus_hotkey = ESTRINGS::strings[137];
        char* saved_empty_string = ESTRINGS::strings[12];

        s_colony_navigation_fixture() {
            colonies.Reset();
            test_planets::Reset(32);
            MOX::star = stars;
            MOX::player = players;
            MOX::NUM_STARS = 1;
            MOX::PLAYER_NUM = 0;
            COLXPORT::cheezy_hack_col_count_state = 0;
            std::fill_n(colony_list, MAX_COLONIES, game::NO_COLONY);
            for (int16_t index = 0; index < 32; ++index) {
                colony_list[index] = game::id_colony{static_cast<uint32_t>(index)};
                colonies[index].planet_id = SERIAL::Decode_Legacy_Planet_Reference(index);
                colonies[index].producing[0] = COLONY_PRODUCTION_NONE;
                test_planets::Record(index).colony_id = game::id_colony{static_cast<uint32_t>(index)};
            }
            COLXPORT::g_colony_list_ptr = colony_list;
            fields::fields = field_storage.data();
            fields::max_fields_count = static_cast<int32_t>(field_storage.size());
            fields::fields_count = 1;
            fields::active_input_field_number = -1;
            fields::input_field_active = 0;
            fields::multi_hotkey_active_field = 0;
            memcpy(saved_anims, COLSUM::anims, sizeof(saved_anims));
            std::fill_n(COLSUM::anims, 21, &animation);
            ESTRINGS::strings[11] = minus_hotkey;
            ESTRINGS::strings[137] = plus_hotkey;
            ESTRINGS::strings[12] = empty_string;
            COLSUM::Clear_List_Col_Array();
            COLSUM::Add_Fields();
            COLSUM::first = 5;
            COLSUM::slider_bar_position = 17;
            COLONY::full_draw = 0;
        }

        ~s_colony_navigation_fixture() {
            game::current_state = std::move(saved_planets_saved_roster);
            MOX::star = saved_stars;
            MOX::player = saved_players;
            MOX::NUM_STARS = saved_star_count;
            MOX::galaxy_map_star_field[0] = saved_star_field;
            COLXPORT::g_colony_list_ptr = saved_list;
            fields::fields = saved_fields;
            memcpy(COLSUM::anims, saved_anims, sizeof(saved_anims));
            ESTRINGS::strings[11] = saved_minus_hotkey;
            ESTRINGS::strings[137] = saved_plus_hotkey;
            ESTRINGS::strings[12] = saved_empty_string;
        }

        void Set_Count(int16_t count) {
            std::erase_if(game::current_state.colonies, [count](const auto& entry) { return entry.first.id >= static_cast<uint32_t>(count); });
            for (int i = 0; i < count; ++i) {
                auto& colony = test_colonies::Record(i);
                colony.planet_id = game::id_planet{static_cast<uint32_t>(i)};
                colony.producing[0] = COLONY_PRODUCTION_NONE;
            }
        }

        void Input(int16_t input) {
            int16_t scanned_field = 0;
            DOCTEST_REQUIRE(COLSUM::Evaluate_Input(&input, &scanned_field));
        }
    };
} // namespace

DOCTEST_TEST_CASE_FIXTURE(s_colony_navigation_fixture, "Colonies list scroll stays at the retained row on re-entry") {
    Set_Count(21);
    DOCTEST_REQUIRE(COLXPORT::N_Colonies(MOX::PLAYER_NUM) == 21);
    for (int16_t row : {0, 5, 11}) {
        COLSUM::first = row;
        COLSUM::slider_bar_position = 11 - row;
        for (int repeat = 0; repeat < 2; ++repeat) {
            COLXPORT::Build_Global_Colony_List();
            COLSUM::Restore_List_Scroll();
            COLSUM::Update_Col_List();
            COLSUM::Update_First(10, &COLSUM::first, &COLSUM::slider_bar_position);
            DOCTEST_CHECK(COLSUM::first == row);
            DOCTEST_CHECK(COLSUM::slider_bar_position == 11 - row);
            DOCTEST_CHECK(COLSUM::list_col[0] == game::id_colony{static_cast<uint32_t>(row)});
            DOCTEST_CHECK(COLSUM::list_col[9] == game::id_colony{static_cast<uint32_t>(row + 9)});
        }
    }
}

DOCTEST_TEST_CASE_FIXTURE(s_colony_navigation_fixture, "Colonies list initially opens at the top") {
    COLSUM::first = 0;
    COLSUM::slider_bar_position = 0;
    COLSUM::Restore_List_Scroll();
    COLSUM::Update_First(10, &COLSUM::first, &COLSUM::slider_bar_position);
    DOCTEST_CHECK(COLSUM::first == 0);
    DOCTEST_CHECK(COLSUM::slider_bar_position == 22);
}

DOCTEST_TEST_CASE_FIXTURE(s_colony_navigation_fixture, "Colonies list retains or clamps the row when colony count changes") {
    COLSUM::first = 11;
    for (int16_t count : {32, 21, 15, 20}) {
        Set_Count(count);
        COLSUM::Restore_List_Scroll();
        COLXPORT::Build_Global_Colony_List();
        COLSUM::Update_Col_List();
        COLSUM::Update_First(10, &COLSUM::first, &COLSUM::slider_bar_position);
        const int16_t expected_row = count >= 21 ? 11 : 5;
        DOCTEST_CHECK(COLSUM::first == expected_row);
        DOCTEST_CHECK(COLSUM::slider_bar_position == count - expected_row - 10);
        DOCTEST_CHECK(COLSUM::list_col[0] == game::id_colony{static_cast<uint32_t>(expected_row)});
    }
}

DOCTEST_TEST_CASE_FIXTURE(s_colony_navigation_fixture, "Colonies list scroll initializes short and exact-page lists") {
    for (int16_t count : {0, 1, 5, 9, 10}) {
        DOCTEST_CAPTURE(count);
        Set_Count(count);
        DOCTEST_REQUIRE(COLXPORT::N_Colonies(MOX::PLAYER_NUM) == count);
        COLSUM::first = 11;
        COLSUM::slider_bar_position = 0;

        COLSUM::Restore_List_Scroll();
        DOCTEST_CHECK(COLSUM::first == 0);
        DOCTEST_CHECK(COLSUM::slider_bar_position == (count < 10 ? -1 : 0));

        COLSUM::Update_First(10, &COLSUM::first, &COLSUM::slider_bar_position);
        DOCTEST_CHECK(COLSUM::first == 0);
        DOCTEST_CHECK(COLSUM::slider_bar_position == (count < 10 ? -1 : 0));
    }
}

DOCTEST_TEST_CASE_FIXTURE(s_colony_navigation_fixture, "Colonies sorting still resets the scroll to the top") {
    const int16_t saved_sort_index = COLSUM::g_sort_index;
    for (int16_t sort_index = 0; sort_index < 8; ++sort_index) {
        COLSUM::first = 5;
        COLSUM::slider_bar_position = 17;
        COLSUM::g_sort_index = sort_index;
        int16_t sort_field = -1;
        if (sort_index == 7) {
            for (int16_t index = 1; index < fields::fields_count; ++index) {
                if (fields::fields[index].hotkey == 't') {
                    sort_field = index;
                    break;
                }
            }
        } else {
            sort_field = COLSUM::x_fields[5 + sort_index];
        }
        DOCTEST_REQUIRE(sort_field != -1);
        Input(sort_field);
        DOCTEST_CHECK(COLSUM::first == 0);
        DOCTEST_CHECK(COLSUM::slider_bar_position == 22);
        DOCTEST_CHECK(COLSUM::list_col[0] == colony_list[0]);
    }
    COLSUM::g_sort_index = saved_sort_index;
}

DOCTEST_TEST_CASE_FIXTURE(s_colony_navigation_fixture, "Colonies binds minus to up and plus to down") {
    DOCTEST_CHECK(fields::fields[COLSUM::x_fields[1]].hotkey == '-');
    DOCTEST_CHECK(fields::fields[COLSUM::x_fields[2]].hotkey == '+');

    Input(COLSUM::x_fields[2]);
    DOCTEST_CHECK(COLSUM::first == 6);
    DOCTEST_CHECK(COLSUM::slider_bar_position == 16);
    DOCTEST_CHECK(COLONY::full_draw == 1);
    Input(COLSUM::x_fields[1]);
    DOCTEST_CHECK(COLSUM::first == 5);
    DOCTEST_CHECK(COLSUM::slider_bar_position == 17);
}

DOCTEST_TEST_CASE_FIXTURE(s_colony_navigation_fixture, "Colonies Home and End reach the first and last full pages") {
    for (int repeat = 0; repeat < 2; ++repeat) {
        COLONY::scanned_pop = 3;
        Input(fields::INPUT_END);
        DOCTEST_CHECK(COLSUM::first == 22);
        DOCTEST_CHECK(COLSUM::slider_bar_position == 0);
        DOCTEST_CHECK(COLONY::scanned_pop == -1);
        DOCTEST_CHECK(COLONY::full_draw == 1);
    }
    Input(COLSUM::x_fields[1]);
    DOCTEST_CHECK(COLSUM::first == 21);
    Input(fields::INPUT_HOME);
    Input(fields::INPUT_HOME);
    Input(COLSUM::x_fields[1]);
    DOCTEST_CHECK(COLSUM::first == 0);
    DOCTEST_CHECK(COLSUM::slider_bar_position == 22);
    Input(COLSUM::x_fields[2]);
    DOCTEST_CHECK(COLSUM::first == 1);
}

DOCTEST_TEST_CASE_FIXTURE(s_colony_navigation_fixture, "Colonies Home and End keep short lists at the top") {
    const int16_t counts[] = {0, 1, 9, 10};
    for (int16_t count : counts) {
        Set_Count(count);
        Input(fields::INPUT_END);
        DOCTEST_CHECK(COLSUM::first == 0);
        DOCTEST_CHECK(COLSUM::slider_bar_position == 0);
        Input(fields::INPUT_HOME);
        DOCTEST_CHECK(COLSUM::first == 0);
    }
}

DOCTEST_TEST_CASE_FIXTURE(s_colony_navigation_fixture, "Colonies map clicks clamp short and full pages") {
    for (int16_t count : {1, 3, 9, 10, 12, 32}) {
        Set_Count(count);
        for (int i = 0; i < 32; ++i) colony_list[i] = i < count ? game::id_colony{static_cast<uint32_t>(i)} : game::NO_COLONY;
        stars[0].last_planet_selected[0] = 0;
        stars[0].orbit_planet_ids[0] = game::id_planet{static_cast<uint32_t>(count - 1)};
        MOX::galaxy_map_star_field[0] = 7;
        int16_t clicked = 7;
        int16_t hovered = -1;
        COLSUM::Check_Galaxy_Map_Fields(&clicked, &hovered);
        DOCTEST_CHECK(COLSUM::first == std::max(0, count - 10));
        DOCTEST_CHECK(COLSUM::slider_bar_position == (count < 10 ? -1 : 0));
        DOCTEST_CHECK(COLSUM::list_col[std::min(9, count - 1)] == game::id_colony{static_cast<uint32_t>(count - 1)});
    }
}

DOCTEST_TEST_CASE("weapon list Home and End keep row and thumb positions synchronized") {
    const int16_t counts[] = {0, 1, 9, 10, 11, 23, 40};
    for (int16_t count : counts) {
        s_scroll_bar scroll_bar = {};
        scroll_bar.total_rows = count;
        scroll_bar.visible_rows = 10;
        scroll_bar.track_len_px = 120;
        scroll_bar.row_step = 1;
        scroll_bar.first_visible_row = 5;

        DOCTEST_CHECK_FALSE(SCROLL::Handle_List_Navigation(&scroll_bar, 0));
        DOCTEST_CHECK((scroll_bar.first_visible_row == 5));

        for (int repeat = 0; repeat < 2; ++repeat) {
            DOCTEST_REQUIRE(SCROLL::Handle_List_Navigation(&scroll_bar, fields::INPUT_END));
            DOCTEST_CHECK((scroll_bar.first_visible_row == std::max(0, count - 10)));
            DOCTEST_CHECK((scroll_bar.last_thumb_center_y == scroll_bar.thumb_center_y));
            DOCTEST_CHECK((scroll_bar.thumb_center_y_copy == scroll_bar.thumb_center_y));
            DOCTEST_CHECK(SCROLL::Set_Vertical_Scroll_Bar_Maxima(&scroll_bar) == 0);
        }
        if (count > 10) {
            SCROLL::Scroll_Up_One_Item(&scroll_bar);
            DOCTEST_CHECK((scroll_bar.first_visible_row == count - 11));
        }

        DOCTEST_REQUIRE(SCROLL::Handle_List_Navigation(&scroll_bar, fields::INPUT_HOME));
        DOCTEST_CHECK((scroll_bar.first_visible_row == 0));
        DOCTEST_CHECK((scroll_bar.thumb_center_y == 120 - scroll_bar.thumb_height_px / 2));
        DOCTEST_CHECK((scroll_bar.last_thumb_center_y == scroll_bar.thumb_center_y));
        DOCTEST_CHECK((scroll_bar.thumb_center_y_copy == scroll_bar.thumb_center_y));
        DOCTEST_CHECK(SCROLL::Set_Vertical_Scroll_Bar_Maxima(&scroll_bar) == 0);
        if (count > 10) {
            SCROLL::Scroll_Down_One_Item(&scroll_bar);
            DOCTEST_CHECK((scroll_bar.first_visible_row == 1));
        }
    }
}
