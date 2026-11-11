#include "pch.h"

#include "game/coldraw.h"
#include "doctest/doctest.h"

namespace TEST_COLDRAW {
    void Reset_();
    int Short_Anim_Calls_();
    int Draw_Calls_();
    int Last_Frame_();
}

DOCTEST_TEST_CASE("food shortage animations are shown with no production or imports") {
    TEST_COLDRAW::Reset_();

    MOX::colony[0].production[ECON_FOOD] = 0;
    MOX::colony[0].imports[ECON_FOOD] = 0;
    MOX::colony[0].maintenance[ECON_FOOD] = 3;

    COLDRAW::Draw_Colony_Prod_Both(0, ECON_FOOD, 0, 20, 300, 30, 0);

    DOCTEST_CHECK(TEST_COLDRAW::Short_Anim_Calls_() == 3);
    DOCTEST_CHECK(TEST_COLDRAW::Draw_Calls_() == 3);
}

DOCTEST_TEST_CASE("production display wrappers select the normal and miniature frames") {
    for (int frame = 0; frame < 2; ++frame) {
        TEST_COLDRAW::Reset_();
        MOX::colony[0].maintenance[ECON_FOOD] = 3;
        if (frame == 0) {
            COLDRAW::Draw_Colony_Prod(0, ECON_FOOD, 0, 20, 300, 30);
        } else {
            COLDRAW::Draw_Colony_Wee_Prod(0, ECON_FOOD, 0, 20, 300, 30);
        }
        DOCTEST_CHECK(TEST_COLDRAW::Last_Frame_() == frame);
        DOCTEST_CHECK(TEST_COLDRAW::Short_Anim_Calls_() == 3);
        DOCTEST_CHECK(TEST_COLDRAW::Draw_Calls_() == 3);
    }
}

DOCTEST_TEST_CASE("production icon spacing handles empty and crowded rows") {
    TEST_COLDRAW::Reset_();
    COLDRAW::Calculate_Squish_Step(0, 300, 0, 30);
    DOCTEST_CHECK(COLONY::step_squish == 0);
    COLDRAW::Calculate_Squish_Step(0, 300, 1, 30);
    DOCTEST_CHECK(COLONY::step_squish == 0);
    COLDRAW::Calculate_Squish_Step(0, 300, 1000, 30);
    DOCTEST_CHECK(COLONY::step_squish == 29);
}