#include "pch.h"

#include "game/coldraw.h"
#include "game/colony_read.h"
#include "doctest/doctest.h"

namespace TEST_COLDRAW {
    void Reset_();
    int Short_Anim_Calls_();
    int Draw_Calls_();
}

DOCTEST_TEST_CASE("shared resource groups retain native shortage and signed byte import branches") {
    TEST_COLDRAW::Reset_();
    auto& colony = MOX::colony[0];
    colony.production[ECON_FOOD] = 0;
    colony.maintenance[ECON_FOOD] = 3;
    colony.imports[ECON_FOOD] = 0;
    colony_read::s_resource_row row;
    colony_read::Resource_Row(&colony, ECON_FOOD, &row);
    DOCTEST_CHECK(row.shortage_units == 3);
    COLDRAW::Draw_Colony_Prod_Both(0, ECON_FOOD, 0, 20, 300, 30, 0);
    DOCTEST_CHECK(TEST_COLDRAW::Short_Anim_Calls_() == row.shortage_units);
    colony.production[ECON_FOOD] = 140;
    colony.imports[ECON_FOOD] = 128;
    colony_read::Resource_Row(&colony, ECON_FOOD, &row);
    DOCTEST_CHECK(row.net_tens == 1);
    DOCTEST_CHECK(row.net_units == 2);
    colony.production[ECON_INDUSTRY] = 1;
    colony.maintenance[ECON_INDUSTRY] = 2;
    colony_read::Resource_Row(&colony, ECON_INDUSTRY, &row);
    DOCTEST_CHECK(row.net_units == 0);
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