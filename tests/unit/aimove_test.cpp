#include "pch.h"

#include "doctest/doctest.h"
#include "game/aimove.h"

namespace TEST_AIMOVE {
    const AIDATA::s_world_view* World_();
    void Reset_();
    int16_t Spawn_Count_();
    uint16_t Spawned_Eel_Timer_();
    void Set_Wandering_Eel_Count_(int16_t count);
    void Set_Npc_(int8_t owner, uint8_t strategic_speed);
    int Move_Attempt_Count_();
}

DOCTEST_TEST_CASE("wandering Eels reproduce at the configured interval") {
    TEST_AIMOVE::Reset_();
    AIMOVE::rules.eel_spawn_frequency = 3;

    AIMOVE::Move_NPCs(TEST_AIMOVE::World_());
    DOCTEST_CHECK((MOX::ship[0].d.date_of_design == 1));
    DOCTEST_CHECK(TEST_AIMOVE::Spawn_Count_() == 0);

    AIMOVE::Move_NPCs(TEST_AIMOVE::World_());
    DOCTEST_CHECK((MOX::ship[0].d.date_of_design == 2));
    DOCTEST_CHECK(TEST_AIMOVE::Spawn_Count_() == 0);

    AIMOVE::Move_NPCs(TEST_AIMOVE::World_());
    DOCTEST_CHECK((MOX::ship[0].d.date_of_design == 0));
    DOCTEST_CHECK(TEST_AIMOVE::Spawn_Count_() == 1);
    DOCTEST_CHECK(TEST_AIMOVE::Spawned_Eel_Timer_() == 0);
}

DOCTEST_TEST_CASE("wandering Eels respect the configured population cap") {
    TEST_AIMOVE::Reset_();
    TEST_AIMOVE::Set_Wandering_Eel_Count_(4);
    AIMOVE::rules.eel_spawn_frequency = 1;
    AIMOVE::rules.max_eels = 4;

    AIMOVE::Move_NPCs(TEST_AIMOVE::World_());
    DOCTEST_CHECK(TEST_AIMOVE::Spawn_Count_() == 0);

    AIMOVE::rules.max_eels = 5;
    AIMOVE::Move_NPCs(TEST_AIMOVE::World_());
    DOCTEST_CHECK(TEST_AIMOVE::Spawn_Count_() == 1);
}

DOCTEST_TEST_CASE("zero design speed excludes retained monsters from NPC movement") {
    TEST_AIMOVE::Reset_();
    TEST_AIMOVE::Set_Npc_(NONPLAYER_SHIP_TYPE_CRYSTAL, 0);

    AIMOVE::Move_NPCs(TEST_AIMOVE::World_());

    DOCTEST_CHECK(TEST_AIMOVE::Move_Attempt_Count_() == 0);

    TEST_AIMOVE::Reset_();
    TEST_AIMOVE::Set_Npc_(NONPLAYER_SHIP_TYPE_CRYSTAL, 4);

    AIMOVE::Move_NPCs(TEST_AIMOVE::World_());

    DOCTEST_CHECK(TEST_AIMOVE::Move_Attempt_Count_() == 1);

    TEST_AIMOVE::Reset_();
    TEST_AIMOVE::Set_Npc_(NONPLAYER_SHIP_TYPE_CRYSTAL, UINT8_MAX);

    AIMOVE::Move_NPCs(TEST_AIMOVE::World_());

    DOCTEST_CHECK(TEST_AIMOVE::Move_Attempt_Count_() == 1);
}