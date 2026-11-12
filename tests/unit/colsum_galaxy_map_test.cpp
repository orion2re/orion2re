#include "pch.h"

#include "doctest/doctest.h"
#include "game/colony.h"
#include "game/colsum.h"
#include "game/colxport.h"

namespace {
    // Regression test for the hard hang in Check_Galaxy_Map_Fields: with
    // fewer than 10 colonies, list[first + 9] is -1 for every first, while
    // Decrement_First() is a no-op below 10 colonies, so the scroll-back
    // loop never terminated and the game froze when a colonized star was
    // clicked on the colony summary screen. The call below must return;
    // before the fix it hung forever.
    struct Galaxy_Map_Fixture {
        static constexpr int16_t kStarField = 7;

        s_star_data stars[1] = {};
        s_planet_data planets[12] = {};
        s_colony colonies[12] = {};
        s_player players[MAX_PLAYERS] = {};
        int16_t colony_list[MAX_COLONIES] = {};
        s_animation_header anim_headers[21] = {};
        std::array<int16_t, MAX_STARS> saved_star_fields = {};

        s_star_data* saved_star = MOX::star;
        s_planet_data* saved_planet = MOX::planet;
        s_colony* saved_colony = MOX::colony;
        s_player* saved_player = MOX::player;
        int16_t saved_num_stars = MOX::NUM_STARS;
        int16_t saved_num_colonies = MOX::NUM_COLONIES;
        int16_t saved_player_num = MOX::PLAYER_NUM;
        int16_t* saved_colony_list_ptr = COLXPORT::g_colony_list_ptr;
        int16_t saved_first = COLSUM::first;
        int16_t saved_slider = COLSUM::slider_bar_position;
        uint8_t saved_full_draw = COLONY::full_draw;
        s_animation_header* saved_anims[21] = {};

        Galaxy_Map_Fixture() {
            std::fill_n(colony_list, MAX_COLONIES, static_cast<int16_t>(-1));
            std::copy_n(MOX::galaxy_map_star_field, MAX_STARS, saved_star_fields.begin());
            std::copy_n(COLSUM::anims, 21, saved_anims);

            // Add_Fields() draws button fields from these headers, so they
            // must be valid; Get_Width()/Get_Full_Store_Flag() dereference
            // them.
            for (s_animation_header& header : anim_headers) {
                header.width = 10;
                header.height = 10;
                header.flags = 0;
            }
            for (int i = 0; i < 21; ++i) {
                COLSUM::anims[i] = &anim_headers[i];
            }

            MOX::star = stars;
            MOX::planet = planets;
            MOX::colony = colonies;
            MOX::player = players;
            MOX::PLAYER_NUM = 0;
            MOX::NUM_STARS = 1;
            MOX::galaxy_map_star_field[0] = kStarField;
            COLXPORT::g_colony_list_ptr = colony_list;

            for (s_planet_data& planet : planets) {
                planet.colony_index = -1;
                planet.star_index = 0;
            }
            for (s_colony& colony : colonies) {
                colony.owner = 0;
                colony.outpost_flag = 0;
                colony.n_pops = 0;
            }
        }

        ~Galaxy_Map_Fixture() {
            MOX::star = saved_star;
            MOX::planet = saved_planet;
            MOX::colony = saved_colony;
            MOX::player = saved_player;
            MOX::NUM_STARS = saved_num_stars;
            MOX::NUM_COLONIES = saved_num_colonies;
            MOX::PLAYER_NUM = saved_player_num;
            COLXPORT::g_colony_list_ptr = saved_colony_list_ptr;
            COLSUM::first = saved_first;
            COLSUM::slider_bar_position = saved_slider;
            COLONY::full_draw = saved_full_draw;
            std::copy_n(saved_anims, 21, COLSUM::anims);
            std::copy_n(saved_star_fields.begin(), MAX_STARS, MOX::galaxy_map_star_field);
        }

        // Set up n_colonies colonies owned by the player, listed in order,
        // and point star 0 at clicked_colony's planet.
        void Set_Up_Colonies(int16_t n_colonies, int16_t clicked_colony) {
            MOX::NUM_COLONIES = n_colonies;
            for (int16_t i = 0; i < n_colonies; ++i) {
                colony_list[i] = i;
                colonies[i].planet = i;
                planets[i].colony_index = i;
            }
            stars[0].last_planet_selected[0] = static_cast<int8_t>(clicked_colony);
            stars[0].planet_index[0] = clicked_colony;
        }
    };
} // namespace

DOCTEST_TEST_CASE_FIXTURE(Galaxy_Map_Fixture, "clicking a colonized star with fewer than 10 colonies returns") {
    Set_Up_Colonies(3, 2);

    int16_t clicked_field = Galaxy_Map_Fixture::kStarField;
    int16_t hovered_field = -7;
    COLSUM::Check_Galaxy_Map_Fields(&clicked_field, &hovered_field);

    // Fewer than 10 colonies fit on one page, so the list starts at 0.
    DOCTEST_CHECK(COLSUM::first == 0);
    DOCTEST_CHECK(COLSUM::slider_bar_position == -1);
    DOCTEST_CHECK(COLSUM::list_col[0] == 0);
    DOCTEST_CHECK(COLSUM::list_col[1] == 1);
    DOCTEST_CHECK(COLSUM::list_col[2] == 2);
    DOCTEST_CHECK(COLSUM::list_col[3] == -1);
}

DOCTEST_TEST_CASE_FIXTURE(Galaxy_Map_Fixture, "clicking a colonized star with 12 colonies scrolls it into view") {
    Set_Up_Colonies(12, 11);

    int16_t clicked_field = Galaxy_Map_Fixture::kStarField;
    int16_t hovered_field = -7;
    COLSUM::Check_Galaxy_Map_Fields(&clicked_field, &hovered_field);

    // The clicked colony is the last one, so the list scrolls back until
    // its row is the tenth visible row.
    DOCTEST_CHECK(COLSUM::first == 2);
    DOCTEST_CHECK(COLSUM::slider_bar_position == 0);
    DOCTEST_CHECK(COLSUM::list_col[0] == 2);
    DOCTEST_CHECK(COLSUM::list_col[9] == 11);
}
