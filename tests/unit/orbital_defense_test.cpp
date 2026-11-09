#include "pch.h"

#include <string>
#include <vector>

#include "doctest/doctest.h"

namespace TEST_ORBITAL_DEFENSE {
    const game_world::s_view* World_();
    AIDATA::s_state* State_();
    void Reset_(int16_t preferred, bool large_ships);
    extern std::vector<std::string> printed;
    extern uint16_t station_weights[3];
    extern int choice_calls;
} // namespace TEST_ORBITAL_DEFENSE

namespace {
    const int16_t stations[] = {BUILDING_STAR_BASE, BUILDING_BATTLESTATION, BUILDING_STAR_FORTRESS};
    const char* station_names[] = {"Star Base", "Battlestation", "Star Fortress"};

    void Check_Popup(int16_t colony, const std::vector<std::string>& expected) {
        TEST_ORBITAL_DEFENSE::printed.clear();
        int16_t line = 0, width = 0, height = 0;
        int16_t measured_count = SYS::Print_Special_Building_Data(colony, 0, &line, &width, &height, 10, false);
        DOCTEST_CHECK(TEST_ORBITAL_DEFENSE::printed.empty());
        DOCTEST_CHECK(measured_count == expected.size());
        const int16_t measured_line = line;
        line = 0;
        int16_t drawn_count = SYS::Print_Special_Building_Data(colony, 0, &line, &width, &height, 10, true);
        DOCTEST_CHECK(drawn_count == measured_count);
        DOCTEST_CHECK(line == measured_line);
        DOCTEST_CHECK(TEST_ORBITAL_DEFENSE::printed == expected);
    }
} // namespace

DOCTEST_TEST_CASE("system summaries count linked ship roles and clamp oversized hulls") {
    TEST_ORBITAL_DEFENSE::Reset_(7, true);
    const uint8_t types[] = {SHIP_TYPE_COMBAT, SHIP_TYPE_COLONY, SHIP_TYPE_OUTPOST, SHIP_TYPE_TRANSPORT, SHIP_TYPE_COMBAT};
    for (int16_t i = 0; i < 5; ++i) {
        MOX::ship[i].d.ship_type = types[i];
        MOX::ship_node[i].ship_idx = i;
        MOX::ship_node[i].next_node = i == 4 ? -1 : i + 1;
    }
    MOX::ship[0].d.size = SHIP_SIZE_LARGE;
    MOX::ship[4].d.size = INT8_MAX;
    int16_t total, colonies, outposts, transports;
    int16_t sizes[SHIP_SIZE_DOOM_STAR + 1];
    SYS::N_Ships_By_Type_In_Stack(0, &total, &colonies, &outposts, &transports, sizes);
    DOCTEST_CHECK(total == 5);
    DOCTEST_CHECK(colonies == 1);
    DOCTEST_CHECK(outposts == 1);
    DOCTEST_CHECK(transports == 1);
    DOCTEST_CHECK(sizes[SHIP_SIZE_LARGE] == 1);
    DOCTEST_CHECK(sizes[SHIP_SIZE_DOOM_STAR] == 1);
}

DOCTEST_TEST_CASE("system summary display records sort by vertical position") {
    s_sys_disp rows[3] = {};
    rows[0].y = 20;
    rows[1].y = 4;
    rows[2].y = 12;
    Legacy_Qsort(rows, 3, sizeof(rows[0]), SYS::qsort_sys_disp);
    DOCTEST_CHECK((rows[0].y == 4));
    DOCTEST_CHECK((rows[1].y == 12));
    DOCTEST_CHECK((rows[2].y == 20));
}

DOCTEST_TEST_CASE("AI station orders cannot create doubled defenses in the attack popup") {
    for (int existing = -1; existing < 3; ++existing) {
        for (int candidate = 0; candidate < 3; ++candidate) {
            for (bool large_ships : {false, true}) {
                DOCTEST_CAPTURE(existing);
                DOCTEST_CAPTURE(candidate);
                DOCTEST_CAPTURE(large_ships);
                TEST_ORBITAL_DEFENSE::Reset_(candidate + 6, large_ships);
                if (existing >= 0) ERICNET::Add_Building(0, stations[existing]);

                aidudes::Assign_Empire_Building(TEST_ORBITAL_DEFENSE::State_(), TEST_ORBITAL_DEFENSE::World_(), legacy_world::Player_Id(0));

                DOCTEST_REQUIRE(TEST_ORBITAL_DEFENSE::choice_calls == 1);
                const bool eligible = candidate > existing;
                DOCTEST_CHECK((TEST_ORBITAL_DEFENSE::station_weights[candidate] != 0) == eligible);
                int16_t ordered = MOX::colony[0].producing[0];
                if (eligible) {
                    DOCTEST_CHECK(ordered == stations[candidate]);
                } else {
                    DOCTEST_CHECK(ordered == COLONY_PRODUCTION_AI_UNASSIGNED);
                }
                // Complete the selected station using the same addition routine as production.
                for (int16_t station : stations) {
                    if (ordered == station) ERICNET::Add_Building(0, ordered);
                }
                const int expected_tier = eligible ? candidate : existing;
                for (int tier = 0; tier < 3; ++tier) {
                    DOCTEST_CHECK((MOX::colony[0].buildings[stations[tier]] != 0) == (tier == expected_tier));
                }
                Check_Popup(0, {station_names[expected_tier]});
            }
        }
    }
}

DOCTEST_TEST_CASE("attack popup text follows the selected colony without duplicating measurement output") {
    TEST_ORBITAL_DEFENSE::Reset_(7, true);
    ERICNET::Add_Building(0, BUILDING_STAR_FORTRESS);
    ERICNET::Add_Building(0, BUILDING_MISSILE_BASE);
    ERICNET::Add_Building(1, BUILDING_BATTLESTATION);
    Check_Popup(0, {"Star Fortress", "Missile Base"});
    Check_Popup(1, {"Battlestation"});
    Check_Popup(0, {"Star Fortress", "Missile Base"});
}