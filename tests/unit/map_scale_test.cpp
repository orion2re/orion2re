#include "pch.h"

#include "doctest/doctest.h"

DOCTEST_TEST_SUITE("map scale") {
    DOCTEST_TEST_CASE("map offsets remain at zero when the galaxy fits the viewport") {
        for (int32_t offset : {-100, 0, 150, 1800}) {
            DOCTEST_CHECK(MAP_SCALE::Clamp_Map_Offset_(offset, 1800, 1821) == 0);
            DOCTEST_CHECK(MAP_SCALE::Clamp_Map_Offset_(offset, 1350, 1440) == 0);
            DOCTEST_CHECK(MAP_SCALE::Clamp_Map_Offset_(offset, 1518, 1518) == 0);
            DOCTEST_CHECK(MAP_SCALE::Clamp_Map_Offset_(offset, 1200, 1200) == 0);
        }
    }

    DOCTEST_TEST_CASE("zoomed-in map offsets retain their position within actual bounds") {
        DOCTEST_CHECK(MAP_SCALE::Clamp_Map_Offset_(-1, 1800, 1518) == 0);
        DOCTEST_CHECK(MAP_SCALE::Clamp_Map_Offset_(141, 1800, 1518) == 141);
        DOCTEST_CHECK(MAP_SCALE::Clamp_Map_Offset_(282, 1800, 1518) == 282);
        DOCTEST_CHECK(MAP_SCALE::Clamp_Map_Offset_(283, 1800, 1518) == 282);
        DOCTEST_CHECK(MAP_SCALE::Clamp_Map_Offset_(75, 1350, 1200) == 75);
        DOCTEST_CHECK(MAP_SCALE::Clamp_Map_Offset_(151, 1350, 1200) == 150);
    }

    DOCTEST_TEST_CASE("zoom-out bounds keep the Maximum overview at the origin in both modes") {
        for (uint8_t mode : {MAP_SCALE::MAXIMUM_GALAXY_ZOOM_CLASSIC, MAP_SCALE::MAXIMUM_GALAXY_ZOOM_HALF}) {
            const int16_t scale = MAP_SCALE::Extended_Scale_For_Zoom_Level_(36, 3, 3, mode);
            DOCTEST_CHECK(scale == 36);
            for (int32_t offset : {-100, 0, 150, 1800}) {
                DOCTEST_CHECK(MAP_SCALE::Clamp_Map_Offset_(offset, 1800, 506 * scale / 10) == 0);
                DOCTEST_CHECK(MAP_SCALE::Clamp_Map_Offset_(offset, 1350, 400 * scale / 10) == 0);
            }
        }
    }

DOCTEST_TEST_CASE("extended scaling starts above the original star capacity") {
    DOCTEST_CHECK_FALSE(MAP_SCALE::Uses_Extended_Scaling_(72, 72));
    DOCTEST_CHECK(MAP_SCALE::Uses_Extended_Scaling_(73, 72));
}

DOCTEST_TEST_CASE("Cluster retains its identity while using Huge classic rules") {
    DOCTEST_CHECK(GALAXY_SIZE_CLUSTER == 5);
    DOCTEST_CHECK(Galaxy_Size_For_Classic_Rules_(GALAXY_SIZE_CLUSTER) == GALAXY_SIZE_HUGE);
    DOCTEST_CHECK(Galaxy_Size_For_Classic_Rules_(GALAXY_SIZE_MAXIMUM) == GALAXY_SIZE_MAXIMUM);
}

DOCTEST_TEST_CASE("galaxy size classification uses configured classic star counts") {
    const int16_t previous_star_count[GALAXY_SIZE_COUNT] = {
        MAPGEN::star_count_by_galaxy_size[0],
        MAPGEN::star_count_by_galaxy_size[1],
        MAPGEN::star_count_by_galaxy_size[2],
        MAPGEN::star_count_by_galaxy_size[3],
        MAPGEN::star_count_by_galaxy_size[4],
        MAPGEN::star_count_by_galaxy_size[5],
    };
    const int16_t previous_num_stars = MOX::NUM_STARS;
    const int8_t previous_galaxy_size = MOX::settings.galaxy_size;
    MAPGEN::star_count_by_galaxy_size[GALAXY_SIZE_SMALL] = 4;
    MAPGEN::star_count_by_galaxy_size[GALAXY_SIZE_MEDIUM] = 8;
    MAPGEN::star_count_by_galaxy_size[GALAXY_SIZE_LARGE] = 16;
    MAPGEN::star_count_by_galaxy_size[GALAXY_SIZE_HUGE] = 32;

    struct s_classification_case {
        int16_t star_count;
        int16_t expected_size;
    };
    const s_classification_case cases[] = {
        {3, GALAXY_SIZE_SMALL},
        {4, GALAXY_SIZE_SMALL},
        {8, GALAXY_SIZE_MEDIUM},
        {16, GALAXY_SIZE_LARGE},
        {32, GALAXY_SIZE_HUGE},
        {71, GALAXY_SIZE_HUGE},
        {72, GALAXY_SIZE_HUGE},
    };
    for (const s_classification_case& test_case : cases) {
        MOX::NUM_STARS = test_case.star_count;
        DOCTEST_CHECK(HAROLD::Galaxy_Size_From_N_Stars() == test_case.expected_size);
    }

    MOX::NUM_STARS = 73;
    MOX::settings.galaxy_size = GALAXY_SIZE_MAXIMUM;
    DOCTEST_CHECK(HAROLD::Galaxy_Size_From_N_Stars() == GALAXY_SIZE_MAXIMUM);
    MOX::settings.galaxy_size = GALAXY_SIZE_CLUSTER;
    DOCTEST_CHECK(HAROLD::Galaxy_Size_From_N_Stars() == GALAXY_SIZE_CLUSTER);

    for (int size = 0; size < GALAXY_SIZE_COUNT; ++size) {
        MAPGEN::star_count_by_galaxy_size[size] = previous_star_count[size];
    }
    MOX::NUM_STARS = previous_num_stars;
    MOX::settings.galaxy_size = previous_galaxy_size;
}

DOCTEST_TEST_CASE("maximum zoom halves each visible map dimension by default") {
    DOCTEST_CHECK(MAP_SCALE::maximum_galaxy_zoom_mode == MAP_SCALE::MAXIMUM_GALAXY_ZOOM_HALF);

    DOCTEST_CHECK(MAP_SCALE::Extended_Scale_For_Zoom_Level_(100, 3, 0, MAP_SCALE::MAXIMUM_GALAXY_ZOOM_HALF) == 13);
    DOCTEST_CHECK(MAP_SCALE::Extended_Scale_For_Zoom_Level_(100, 3, 1, MAP_SCALE::MAXIMUM_GALAXY_ZOOM_HALF) == 25);
    DOCTEST_CHECK(MAP_SCALE::Extended_Scale_For_Zoom_Level_(100, 3, 2, MAP_SCALE::MAXIMUM_GALAXY_ZOOM_HALF) == 50);
    DOCTEST_CHECK(MAP_SCALE::Extended_Scale_For_Zoom_Level_(100, 3, 3, MAP_SCALE::MAXIMUM_GALAXY_ZOOM_HALF) == 100);

    DOCTEST_CHECK(MAP_SCALE::Extended_Zoom_Level_For_Scale_(24, 100, 3, MAP_SCALE::MAXIMUM_GALAXY_ZOOM_HALF) == 0);
    DOCTEST_CHECK(MAP_SCALE::Extended_Zoom_Level_For_Scale_(25, 100, 3, MAP_SCALE::MAXIMUM_GALAXY_ZOOM_HALF) == 1);
    DOCTEST_CHECK(MAP_SCALE::Extended_Zoom_Level_For_Scale_(50, 100, 3, MAP_SCALE::MAXIMUM_GALAXY_ZOOM_HALF) == 2);
    DOCTEST_CHECK(MAP_SCALE::Extended_Zoom_Level_For_Scale_(100, 100, 3, MAP_SCALE::MAXIMUM_GALAXY_ZOOM_HALF) == 3);
}

DOCTEST_TEST_CASE("maximum zoom can preserve the three wider classic zoom scales") {
    DOCTEST_CHECK(MAP_SCALE::Extended_Scale_For_Zoom_Level_(100, 3, 0, MAP_SCALE::MAXIMUM_GALAXY_ZOOM_CLASSIC) == 15);
    DOCTEST_CHECK(MAP_SCALE::Extended_Scale_For_Zoom_Level_(100, 3, 1, MAP_SCALE::MAXIMUM_GALAXY_ZOOM_CLASSIC) == 20);
    DOCTEST_CHECK(MAP_SCALE::Extended_Scale_For_Zoom_Level_(100, 3, 2, MAP_SCALE::MAXIMUM_GALAXY_ZOOM_CLASSIC) == 30);
    DOCTEST_CHECK(MAP_SCALE::Extended_Scale_For_Zoom_Level_(100, 3, 3, MAP_SCALE::MAXIMUM_GALAXY_ZOOM_CLASSIC) == 100);

    DOCTEST_CHECK(MAP_SCALE::Extended_Zoom_Level_For_Scale_(19, 100, 3, MAP_SCALE::MAXIMUM_GALAXY_ZOOM_CLASSIC) == 0);
    DOCTEST_CHECK(MAP_SCALE::Extended_Zoom_Level_For_Scale_(20, 100, 3, MAP_SCALE::MAXIMUM_GALAXY_ZOOM_CLASSIC) == 1);
    DOCTEST_CHECK(MAP_SCALE::Extended_Zoom_Level_For_Scale_(30, 100, 3, MAP_SCALE::MAXIMUM_GALAXY_ZOOM_CLASSIC) == 2);
    DOCTEST_CHECK(MAP_SCALE::Extended_Zoom_Level_For_Scale_(100, 100, 3, MAP_SCALE::MAXIMUM_GALAXY_ZOOM_CLASSIC) == 3);

    DOCTEST_CHECK(MAP_SCALE::Classic_Zoom_Level_For_Scale_(15, 3) == 1);
    DOCTEST_CHECK(MAP_SCALE::Classic_Zoom_Level_For_Scale_(20, 3) == 2);
    DOCTEST_CHECK(MAP_SCALE::Classic_Zoom_Level_For_Scale_(30, 3) == 3);
    DOCTEST_CHECK(MAP_SCALE::Classic_Zoom_Level_For_Scale_(100, 3) == 3);
}

DOCTEST_TEST_CASE("star scaling preserves legacy galaxies and close zoom levels") {
    DOCTEST_CHECK(MAP_SCALE::Star_Scale_Percent_(72, 72, 100) == 100);
    DOCTEST_CHECK(MAP_SCALE::Star_Scale_Percent_(73, 72, 30) == 100);
    DOCTEST_CHECK(MAP_SCALE::Star_Scale_Percent_(73, 72, 60) == 50);
    DOCTEST_CHECK(MAP_SCALE::Star_Scale_Percent_(73, 72, 100) == 30);

    DOCTEST_CHECK(MAP_SCALE::Scale_Star_Dimension_(33, 100) == 33);
    DOCTEST_CHECK(MAP_SCALE::Scale_Star_Dimension_(2, 100) == 2);
    DOCTEST_CHECK(MAP_SCALE::Scale_Star_Dimension_(33, 50) == 16);
    DOCTEST_CHECK(MAP_SCALE::Scale_Star_Dimension_(8, 30) == 3);
}

DOCTEST_TEST_CASE("maximum extended zoom enables the overview and delays black hole animation") {
    DOCTEST_CHECK_FALSE(MAP_SCALE::Is_Extended_Max_Map_View_(72, 72, 100, 100));
    DOCTEST_CHECK_FALSE(MAP_SCALE::Is_Extended_Max_Map_View_(73, 72, 50, 100));
    DOCTEST_CHECK(MAP_SCALE::Is_Extended_Max_Map_View_(73, 72, 100, 100));

    DOCTEST_CHECK_FALSE(MAP_SCALE::Use_Main_Map_Overview_Markers_(false, true, 99, 72, 54, 54));
    DOCTEST_CHECK_FALSE(MAP_SCALE::Use_Main_Map_Overview_Markers_(true, false, 99, 72, 54, 54));
    DOCTEST_CHECK_FALSE(MAP_SCALE::Use_Main_Map_Overview_Markers_(true, true, 99, 72, 30, 54));
    DOCTEST_CHECK(MAP_SCALE::Use_Main_Map_Overview_Markers_(true, true, 99, 72, 54, 54));

    DOCTEST_CHECK(MAP_SCALE::Advance_Black_Hole_Animation_(100));
    DOCTEST_CHECK_FALSE(MAP_SCALE::Advance_Black_Hole_Animation_(99));
}

DOCTEST_TEST_CASE("overview star markers distinguish ownership and exploration") {
    DOCTEST_CHECK(MAP_SCALE::Overview_Star_Color_Index_(false, false, 2, 8, 6) == 6);
    DOCTEST_CHECK(MAP_SCALE::Overview_Star_Color_Index_(false, false, -1, 8, 0) == 8);
    DOCTEST_CHECK(MAP_SCALE::Overview_Star_Color_Index_(false, false, -2, 8, 0) == 8);
    DOCTEST_CHECK(MAP_SCALE::Overview_Star_Color_Index_(true, false, -1, 8, 0) == 10);
    DOCTEST_CHECK(MAP_SCALE::Overview_Star_Color_Index_(true, true, -1, 8, 0) == 9);

    DOCTEST_CHECK_FALSE(MAP_SCALE::Uses_Visited_Neutral_Overview_Color_(false, -1, 8, false));
    DOCTEST_CHECK(MAP_SCALE::Uses_Visited_Neutral_Overview_Color_(false, -1, 8, true));
    DOCTEST_CHECK(MAP_SCALE::Uses_Visited_Neutral_Overview_Color_(false, -2, 8, false));
    DOCTEST_CHECK_FALSE(MAP_SCALE::Uses_Visited_Neutral_Overview_Color_(false, 2, 8, true));
    DOCTEST_CHECK_FALSE(MAP_SCALE::Uses_Visited_Neutral_Overview_Color_(true, -1, 8, true));

    DOCTEST_CHECK_FALSE(MAP_SCALE::Uses_Visited_Uncolonized_Overview_Marker_(false, false, -1, true));
    DOCTEST_CHECK_FALSE(MAP_SCALE::Uses_Visited_Uncolonized_Overview_Marker_(true, false, -1, false));
    DOCTEST_CHECK(MAP_SCALE::Uses_Visited_Uncolonized_Overview_Marker_(true, false, -1, true));
    DOCTEST_CHECK_FALSE(MAP_SCALE::Uses_Visited_Uncolonized_Overview_Marker_(true, false, -2, true));
    DOCTEST_CHECK_FALSE(MAP_SCALE::Uses_Visited_Uncolonized_Overview_Marker_(true, false, 2, true));
    DOCTEST_CHECK_FALSE(MAP_SCALE::Uses_Visited_Uncolonized_Overview_Marker_(true, true, -1, true));
}

DOCTEST_TEST_CASE("maximum zoom influence overlay can replace thorium range") {
    DOCTEST_CHECK(MAP_SCALE::Influence_Overlay_Range_(255, 12, true) == 12);
    DOCTEST_CHECK(MAP_SCALE::Influence_Overlay_Range_(9, 12, true) == 9);
    DOCTEST_CHECK(MAP_SCALE::Influence_Overlay_Range_(255, 12, false) == 255);
    DOCTEST_CHECK(MAP_SCALE::Influence_Overlay_Range_(9, 12, false) == 9);

    DOCTEST_CHECK(MAP_SCALE::Influence_Overlay_Map_Radius_(0) == 15);
    DOCTEST_CHECK(MAP_SCALE::Influence_Overlay_Map_Radius_(1) == 30);
    DOCTEST_CHECK(MAP_SCALE::Influence_Overlay_Map_Radius_(12) == 360);

    DOCTEST_CHECK(MAP_SCALE::Influence_Overlay_Screen_Radius_(15, 100, 6, true) == 4);
    DOCTEST_CHECK(MAP_SCALE::Influence_Overlay_Screen_Radius_(30, 100, 6, false) == 3);
    DOCTEST_CHECK(MAP_SCALE::Influence_Overlay_Screen_Radius_(360, 100, 6, false) == 36);
}

}