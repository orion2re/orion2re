#include "pch.h"

#include "doctest/doctest.h"

namespace TEST_AIDUDES {
    extern bool colony_researching;
    const game_world::s_view* World_();
    AIDATA::s_state* State_();
    aidudes::s_ai_colony_context Context_();
    void Reset_();
    void Reset_Maintenance_Scrap_(bool add_replacement);
    void Reset_Maintenance_Scrap_With_Incomparable_Replacements_();
    void Reset_Doom_Star_Update_();
    void Reset_Doom_Star_Refit_();
    int Pre_Import_Calls_();
    int8_t Status_During_Recalculation_();
    int16_t Pooled_Officer_();
    bool Doom_Star_Strategic_Mode_();
    int Doom_Star_Load_Count_();
} // namespace TEST_AIDUDES

DOCTEST_TEST_CASE("blockaded event-research colonies assign non-farmers to science") {
    TEST_AIDUDES::Reset_();
    TEST_AIDUDES::colony_researching = true;
    MOX::colony[0].food2_per_farmer = 0;
    s_aidudes_colony_info info = {};
    aidudes::Do_Blockaded_Colony(&info);
    DOCTEST_CHECK(info.cur_pop_idx == 1);
    DOCTEST_CHECK(POP::Get_Prof(MOX::colony[0].pop[0]) == POP::PROF_SCIENTIST);
    DOCTEST_CHECK(POP::Is_Assigned(MOX::colony[0].pop[0]));
    TEST_AIDUDES::Reset_();
}

DOCTEST_TEST_CASE("blockaded colony stops assigning farmers when recalculation disables farming") {
    TEST_AIDUDES::Reset_();

    AIDATA::s_colony_assignment colony_info = {};
    colony_info.colony_id = 0;

    aidudes::Do_Blockaded_Colony(TEST_AIDUDES::World_(), &colony_info);

    DOCTEST_CHECK(TEST_AIDUDES::Pre_Import_Calls_() == 2);
    DOCTEST_CHECK(colony_info.cur_pop_idx == 1);
    DOCTEST_CHECK(POP::Is_Worker(MOX::colony[0].pop[0]));
    DOCTEST_CHECK(POP::Is_Assigned(MOX::colony[0].pop[0]));
}

DOCTEST_TEST_CASE("maintenance marks a ship scrapped before recalculation and reassigns its officer") {
    TEST_AIDUDES::Reset_Maintenance_Scrap_(true);

    maintain::Kill_Ships(&MOX::player[0], false, false, SHIP_TYPE_COMBAT);

    DOCTEST_CHECK(TEST_AIDUDES::Status_During_Recalculation_() == 7);
    DOCTEST_CHECK(MOX::ship[0].status == 7);
    DOCTEST_CHECK(MOX::ship[0].officer_index == -1);
    DOCTEST_CHECK((MOX::ship[1].officer_index == 0));
    DOCTEST_CHECK((MOX::leaders[0].location == 1));
    DOCTEST_CHECK(TEST_AIDUDES::Pooled_Officer_() == -1);
}

DOCTEST_TEST_CASE("maintenance considers officer replacements in descending ship order") {
    TEST_AIDUDES::Reset_Maintenance_Scrap_With_Incomparable_Replacements_();

    maintain::Kill_Ships(&MOX::player[0], false, false, SHIP_TYPE_COMBAT);

    DOCTEST_CHECK(TEST_AIDUDES::Status_During_Recalculation_() == 7);
    DOCTEST_CHECK(MOX::ship[0].officer_index == -1);
    DOCTEST_CHECK((MOX::ship[1].officer_index == -1));
    DOCTEST_CHECK((MOX::ship[2].officer_index == 0));
    DOCTEST_CHECK((MOX::leaders[0].location == 2));
    DOCTEST_CHECK(TEST_AIDUDES::Pooled_Officer_() == -1);
}

DOCTEST_TEST_CASE("maintenance pools an officer when the scrapped ship has no replacement") {
    TEST_AIDUDES::Reset_Maintenance_Scrap_(false);

    maintain::Kill_Ships(&MOX::player[0], false, false, SHIP_TYPE_COMBAT);

    DOCTEST_CHECK(TEST_AIDUDES::Status_During_Recalculation_() == 7);
    DOCTEST_CHECK(MOX::ship[0].officer_index == -1);
    DOCTEST_CHECK((MOX::leaders[0].location == -1));
    DOCTEST_CHECK(TEST_AIDUDES::Pooled_Officer_() == 0);
}

DOCTEST_TEST_CASE("ship scrapping uses the configured refund percentage") {
    TEST_AIDUDES::Reset_Maintenance_Scrap_(false);

    maintain::ship_scrap_refund_percent = 25;
    DOCTEST_CHECK(maintain::Ship_Scrap_Value(&MOX::ship[0]) == 10);

    maintain::ship_scrap_refund_percent = 50;
    DOCTEST_CHECK(maintain::Ship_Scrap_Value(&MOX::ship[0]) == 20);

    maintain::ship_scrap_refund_percent = 25;
}

DOCTEST_TEST_CASE("AI queued Doom Star updates use strategic designs") {
    TEST_AIDUDES::Reset_Doom_Star_Update_();

    auto context = TEST_AIDUDES::Context_();
    aidudes::Keep_Ship_Designs_Up_To_Date(&context);

    DOCTEST_CHECK(TEST_AIDUDES::Doom_Star_Load_Count_() == 1);
    DOCTEST_CHECK(TEST_AIDUDES::Doom_Star_Strategic_Mode_());
    DOCTEST_CHECK(MOX::ship[0].d.ship_type == SHIP_TYPE_COMBAT);
    DOCTEST_CHECK(MOX::ship[0].d.size == SHIP_SIZE_DOOM_STAR);
}

DOCTEST_TEST_CASE("AI Doom Star upgrade refits use strategic designs") {
    TEST_AIDUDES::Reset_Doom_Star_Refit_();

    auto context = TEST_AIDUDES::Context_();
    aidudes::Do_Ship_Upgrade_Cheats(TEST_AIDUDES::State_(), &context);

    DOCTEST_CHECK(TEST_AIDUDES::Doom_Star_Load_Count_() == 1);
    DOCTEST_CHECK(TEST_AIDUDES::Doom_Star_Strategic_Mode_());
    DOCTEST_CHECK(MOX::ship[0].d.ship_type == SHIP_TYPE_COMBAT);
    DOCTEST_CHECK(MOX::ship[0].d.size == SHIP_SIZE_DOOM_STAR);
}