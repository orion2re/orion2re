#include "pch.h"

#include <string>
#include <vector>

#include "doctest/doctest.h"

namespace {
    s_ai_refit_player refits[MAX_PLAYERS];
    s_ai_player_info player_info[MAX_PLAYERS];
    s_ai_colony_info colony_info[MAX_COLONIES];
    s_ai_star_info star_info[MAX_STARS];
    int8_t human_ships[MAX_STARS][10];
    uint16_t planet_values[MAX_PLAYERS * MAX_PLANETS];
    int16_t preferred_station;
    bool can_build_large_ships;
} // namespace

namespace MOX {
    s_ship_node ship_node[MAX_SHIPS];
}

namespace AIDATA {
    s_ai_refit_player (*airefit)[MAX_PLAYERS] = &refits;
    s_ai_player_info (*ai_player_info)[MAX_PLAYERS] = &player_info;
    s_ai_colony_info (*ai_colony_info)[MAX_COLONIES] = &colony_info;
    s_ai_star_info (*ai_star_info)[MAX_STARS] = &star_info;
    int8_t (*enroute_human_ships)[MAX_STARS][10] = &human_ships;
    uint16_t (*contextual_planet_values)[MAX_PLAYERS * MAX_PLANETS] = &planet_values;
} // namespace AIDATA

namespace AIBUILD {
    s_player_ship_sizes player_ship_sizes[MAX_PLAYERS];
    int32_t player_total_strength_vs_ships[MAX_PLAYERS][3];
    uint8_t player_n_transports[MAX_PLAYERS][2];
    uint8_t player_n_spies[MAX_PLAYERS][2];
    uint8_t player_n_colony_ships[MAX_PLAYERS][3];
    int8_t player_n_freighter_groups_wanted[MAX_PLAYERS];

    int16_t __cdecl Player_War_Level(int16_t) {
        return 1;
    }
    int16_t __cdecl Industry_Worth(s_colony*) {
        return 100;
    }
    bool __cdecl Colony_Is_Conquered(s_colony*) {
        return false;
    }
    int16_t __cdecl Mean_Ship_Strength(s_ship_data*) {
        return 1;
    }
    int16_t __cdecl Refit_Cost(int16_t, s_ship_design*, s_ship_design*) {
        return 100;
    }

    void Build_Production_Context(const s_colony*, int32_t, production_rules::s_context* context) {
        *context = {};
    }

    void __cdecl Force_Produce(int16_t* queue, int16_t index, int32_t* count, int16_t product) {
        DOCTEST_REQUIRE(*count == 1);
        DOCTEST_REQUIRE(index == 0);
        MOX::colony[queue[index]].producing[0] = product;
        --*count;
    }
} // namespace AIBUILD

namespace TEST_ORBITAL_DEFENSE {
    std::vector<std::string> printed;
    uint16_t station_weights[3];
    int choice_calls;

    void Reset_(int16_t preferred, bool large_ships) {
        memset(MOX::ship, 0, sizeof(s_ship_data) * MAX_SHIPS);
        memset(MOX::ship_node, 0xff, sizeof(MOX::ship_node));
        memset(MOX::colony, 0, sizeof(s_colony) * 2);
        memset(MOX::planet, 0, sizeof(s_planet_data) * 2);
        memset(MOX::star, 0, sizeof(s_star_data));
        memset(MOX::player, 0, sizeof(s_player) * MAX_PLAYERS);
        memset(refits, 0, sizeof(refits));
        memset(player_info, 0, sizeof(player_info));
        memset(colony_info, 0, sizeof(colony_info));
        memset(star_info, 0, sizeof(star_info));
        memset(station_weights, 0, sizeof(station_weights));
        preferred_station = preferred;
        can_build_large_ships = large_ships;
        choice_calls = 0;
        printed.clear();
        MOX::NUM_COLONIES = 1;
        MOX::colony[0].owner = 0;
        MOX::colony[0].planet = 0;
        MOX::colony[0].n_pops = 10;
        MOX::colony[0].producing[0] = COLONY_PRODUCTION_NONE;
        MOX::colony[1].planet = 1;
        MOX::player[0].ship_range = 4;
        MOX::player[0].ship_speed = 2;
        MOX::player[0].tech_applications[TECH_APP_STAR_BASE] = TECH_RESEARCH_STATUS_RESEARCHED;
        MOX::player[0].tech_applications[TECH_APP_BATTLESTATION] = TECH_RESEARCH_STATUS_RESEARCHED;
        MOX::player[0].tech_applications[TECH_APP_STAR_FORTRESS] = TECH_RESEARCH_STATUS_RESEARCHED;
        AIBUILD::player_total_strength_vs_ships[0][0] = 0;
        AIBUILD::player_total_strength_vs_ships[0][2] = 100;
        AIBUILD::strategic_production.emergency_defense.enabled = false;
        AIBUILD::strategic_production.expansion.enabled = false;
        AIBUILD::strategic_production.economic_emergency.enabled = false;
        AIBUILD::strategic_production.allocation = {};
        AIBUILD::strategic_production.allocation.candidates[0].enabled = true;
        AIBUILD::strategic_production.allocation.candidates[0].weight.base = 1;
        AIBUILD::strategic_production.allocation.candidates[0].weight.clamp.maximum = INT32_MAX;
        TECHDATA::buildings[BUILDING_STAR_BASE].name = const_cast<char*>("Star Base");
        TECHDATA::buildings[BUILDING_BATTLESTATION].name = const_cast<char*>("Battlestation");
        TECHDATA::buildings[BUILDING_STAR_FORTRESS].name = const_cast<char*>("Star Fortress");
        TECHDATA::buildings[BUILDING_MISSILE_BASE].name = const_cast<char*>("Missile Base");
    }
} // namespace TEST_ORBITAL_DEFENSE

namespace RUSS {
    void __cdecl Mox_Update() {}

    int16_t __cdecl Get_Weighted_Choice_Int(uint32_t* weights, int16_t count) {
        DOCTEST_REQUIRE(count == 6);
        return weights[0] != 0 ? 0 : -1;
    }

    int16_t __cdecl Get_Weighted_Choice_Short(const uint16_t* weights, int16_t count) {
        DOCTEST_REQUIRE(count == 9);
        ++TEST_ORBITAL_DEFENSE::choice_calls;
        for (int i = 0; i < 3; ++i) TEST_ORBITAL_DEFENSE::station_weights[i] = weights[i + 6];
        // Never force an ineligible outcome: defer if the preferred station has no weight.
        return weights[preferred_station] != 0 ? preferred_station : -1;
    }
} // namespace RUSS

namespace COLCALC {
    bool __cdecl Colony_Is_Blockaded(s_colony*) {
        return false;
    }
    bool __cdecl Colony_Can_Build_Large_Ships(s_colony*) {
        return can_build_large_ships;
    }
    int32_t __cdecl Colony_Product_Cost(s_colony*, int16_t) {
        return 100;
    }
    void __cdecl Do_Player_Colony_Calculations(int16_t) {}
} // namespace COLCALC

namespace EVENTS {
    int16_t __cdecl Event_Check_Space_Anomaly(int16_t) {
        return 0;
    }
} // namespace EVENTS

namespace AIMOVE {
    int16_t __cdecl Best_Uncolonized_Undestined_Planet(int16_t) {
        return -1;
    }
} // namespace AIMOVE

namespace INITSHIP {
    void __cdecl Load_Doom_Star_Ship_Design(int16_t, s_ship_design*, bool) {
        DOCTEST_FAIL("Unexpected Doom Star design request");
    }
    void __cdecl Load_Transport_Ship_Design(int16_t, s_ship_design*) {
        DOCTEST_FAIL("Unexpected transport design request");
    }
} // namespace INITSHIP

namespace OFFICER {
    void __cdecl Move_Officer_To_Pool(int16_t) {
        DOCTEST_FAIL("Unexpected officer transfer");
    }
} // namespace OFFICER

namespace fonts {
    void __cdecl Print(int16_t, int16_t, const char* text) {
        TEST_ORBITAL_DEFENSE::printed.emplace_back(text);
    }
    int16_t __cdecl Get_String_Width(const char* text) {
        return static_cast<int16_t>(strlen(text));
    }
} // namespace fonts

namespace MISC {
    int16_t __cdecl Get_Font_Style() {
        return 1;
    }
} // namespace MISC