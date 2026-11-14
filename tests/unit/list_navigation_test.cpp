#include "pch.h"

#include "doctest/doctest.h"

namespace {
    struct s_colony_navigation_fixture {
        s_colony colonies[32] = {};
        int16_t colony_list[MAX_COLONIES];
        std::vector<s_field> field_storage = std::vector<s_field>(250);
        s_animation_header animation = {};
        s_colony* saved_colonies = MOX::colony;
        int16_t* saved_list = COLXPORT::g_colony_list_ptr;
        s_field* saved_fields = fields::fields;
        decltype(COLSUM::anims) saved_anims;
        char minus_hotkey[2] = "-";
        char plus_hotkey[2] = "+";
        char* saved_minus_hotkey = ESTRINGS::strings[11];
        char* saved_plus_hotkey = ESTRINGS::strings[137];

        s_colony_navigation_fixture() {
            MOX::colony = colonies;
            MOX::NUM_COLONIES = 32;
            MOX::PLAYER_NUM = 0;
            COLXPORT::cheezy_hack_col_count_state = 0;
            std::fill_n(colony_list, MAX_COLONIES, static_cast<int16_t>(-1));
            for (int16_t index = 0; index < 32; ++index) {
                colony_list[index] = index;
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
            COLSUM::Clear_List_Col_Array();
            COLSUM::Add_Fields();
            COLSUM::first = 5;
            COLSUM::slider_bar_position = 17;
            COLONY::full_draw = 0;
        }

        ~s_colony_navigation_fixture() {
            MOX::colony = saved_colonies;
            COLXPORT::g_colony_list_ptr = saved_list;
            fields::fields = saved_fields;
            memcpy(COLSUM::anims, saved_anims, sizeof(saved_anims));
            ESTRINGS::strings[11] = saved_minus_hotkey;
            ESTRINGS::strings[137] = saved_plus_hotkey;
        }

        void Input(int16_t input) {
            int16_t scanned_field = 0;
            DOCTEST_REQUIRE(COLSUM::Evaluate_Input(&input, &scanned_field));
        }
    };
} // namespace

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
        MOX::NUM_COLONIES = count;
        Input(fields::INPUT_END);
        DOCTEST_CHECK(COLSUM::first == 0);
        DOCTEST_CHECK(COLSUM::slider_bar_position == 0);
        Input(fields::INPUT_HOME);
        DOCTEST_CHECK(COLSUM::first == 0);
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
