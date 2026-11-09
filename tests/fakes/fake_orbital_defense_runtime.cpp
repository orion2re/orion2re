#include "pch.h"

#include <string>
#include <vector>

#include "doctest/doctest.h"

namespace {
    AIDATA::s_state ai;
    int16_t preferred_station;
    bool can_build_large_ships;
} // namespace

namespace AIBUILD {

    int16_t Player_War_Level(const game::s_view*, game::t_player_id) {
        return 1;
    }
    int16_t __cdecl Industry_Worth(s_colony*) {
        return 100;
    }
    bool __cdecl Colony_Is_Conquered(const game::s_view*, s_colony*) {
        return false;
    }
    s_opponent_view Opponents(const game::s_view*) { return {}; }

    int16_t __cdecl Mean_Ship_Strength(const s_opponent_view*, const AIDATA::s_ship_assessment*, const AIDATA::s_empire_assessment*, const s_ship_data*) {
        return 1;
    }
    int16_t __cdecl Refit_Cost(const s_player*, const s_ship_design*, const s_ship_design*) {
        return 100;
    }

    void Build_Production_Context(AIDATA::s_state*, const game::s_view*, game::t_colony_id, const s_colony*, int32_t, production_rules::s_context* context) {
        *context = {};
    }

    void __cdecl Force_Produce(const game::s_view*, game::t_colony_id* queue, uint32_t index, int32_t* count, int16_t product) {
        DOCTEST_REQUIRE(*count == 1);
        DOCTEST_REQUIRE(index == 0);
        MOX::colony[legacy_world::Slot(queue[index])].producing[0] = product;
        --*count;
    }
} // namespace AIBUILD

namespace TEST_ORBITAL_DEFENSE {
    const game::s_view* World_() {
        static uint32_t seed;
        static const game::s_view world{
            .players = MOX::player,
            .ships = MOX::ship,
            .stars = MOX::star,
            .colonies = MOX::colony,
            .planets = MOX::planet,
            .settings = &MOX::settings,
            .colony_count = &MOX::NUM_COLONIES,
            .random_seed = &seed,
        };
        return &world;
    }
    AIDATA::s_state* State_() {
        return &ai;
    }
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
        ai = {};
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
        ai.players[0].build.combat_strength.total = 0;
        ai.players[0].build.combat_strength.additional_wanted = 100;
        AIBUILD::rules.strategic.emergency_defense.enabled = false;
        AIBUILD::rules.strategic.expansion.enabled = false;
        AIBUILD::rules.strategic.economic_emergency.enabled = false;
        AIBUILD::rules.strategic.allocation = {};
        AIBUILD::rules.strategic.allocation.candidates[0].enabled = true;
        AIBUILD::rules.strategic.allocation.candidates[0].weight.base = 1;
        AIBUILD::rules.strategic.allocation.candidates[0].weight.clamp.maximum = INT32_MAX;
        TECHDATA::buildings[BUILDING_STAR_BASE].name = const_cast<char*>("Star Base");
        TECHDATA::buildings[BUILDING_BATTLESTATION].name = const_cast<char*>("Battlestation");
        TECHDATA::buildings[BUILDING_STAR_FORTRESS].name = const_cast<char*>("Star Fortress");
        TECHDATA::buildings[BUILDING_MISSILE_BASE].name = const_cast<char*>("Missile Base");
    }
} // namespace TEST_ORBITAL_DEFENSE

namespace game_random {
    uint32_t __cdecl Random_From_Seed(uint32_t*, uint32_t maximum) { return Random(maximum); }
}

namespace RUSS {
    std::optional<uint32_t> Choose_Weighted_Index_U32(uint32_t*, const uint32_t* weights, uint32_t count) {
        int16_t choice = Get_Weighted_Choice_Int(const_cast<uint32_t*>(weights), (int16_t)count);
        return choice < 0 ? std::nullopt : std::optional<uint32_t>{(uint32_t)choice};
    }

    std::optional<uint32_t> Choose_Weighted_Index_U16(uint32_t*, const uint16_t* weights, uint32_t count) {
        int16_t choice = Get_Weighted_Choice_Short(weights, (int16_t)count);
        return choice < 0 ? std::nullopt : std::optional<uint32_t>{(uint32_t)choice};
    }

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
    std::optional<game::t_planet_id> Best_Uncolonized_Undestined_Planet(AIDATA::s_state*, const game::s_view*, game::t_player_id) {
        return std::nullopt;
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