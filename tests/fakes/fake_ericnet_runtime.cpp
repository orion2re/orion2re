#include "pch.h"

namespace {
    s_colony colonies[2];
    s_planet_data planets[2];
    s_star_data stars[1];
    s_ship_data ships[MAX_SHIPS];
    s_leader_data leaders[MAX_LEADERS];
    s_player players[MAX_PLAYERS];
}

namespace test::fakes {
    int16_t instructor_bonuses[MAX_LEADERS][2];
    int32_t last_played_sound = -1;
    int32_t environmental_refresh_count = 0;
    int32_t colony_can_build_product_count = 0;
    s_colony* last_environmental_colony = nullptr;
}

namespace MOX {
    int16_t NUM_COLONIES = 0;
    int16_t NUM_SHIPS = 0;
    s_colony* colony = colonies;
    s_planet_data* planet = planets;
    s_star_data* star = stars;
    s_ship_data* ship = ships;
    s_leader_data* leaders = ::leaders;
    s_player* player = players;
    int16_t super_fast_flag;
    s_settings settings = {};
    s_crew_data crew_data[CREW_QUALITY_COUNT] = {
        {0, 0, 0, 0}, {50, 15, 15, 5}, {150, 30, 30, 10},
        {500, 50, 50, 15}, {1000, 75, 75, 20},
    };
    uint8_t food_per_farmer_table[PLANET_CLIMATE_COUNT];
    uint8_t planet_max_farms[PLANET_SIZE_COUNT];
}

namespace OFFICER {
    bool __cdecl Officer_Has_Special_Skill(int16_t, int16_t leader_idx, int16_t skill_idx) {
        return leader_idx >= 0 && leader_idx < MAX_LEADERS &&
            skill_idx >= 0x16 && skill_idx <= 0x17 &&
            test::fakes::instructor_bonuses[leader_idx][skill_idx - 0x16] != 0;
    }

    int32_t __cdecl Officer_Skill_Bonus(uint32_t, int16_t leader_idx, int16_t skill_idx) {
        return Officer_Has_Special_Skill(0, leader_idx, skill_idx)
            ? test::fakes::instructor_bonuses[leader_idx][skill_idx - 0x16] : 0;
    }
}

namespace HACCESS {
    int16_t __cdecl Ship_Owner(int16_t ship_idx) {
        return MOX::ship[ship_idx].owner;
    }
}

namespace sound {
    int32_t __cdecl Play_Sound(int32_t resource_id) {
        test::fakes::last_played_sound = resource_id;
        return resource_id;
    }
}

namespace TECHDATA {
    s_tech_building_data buildings[BUILDING_COUNT];
}

namespace game_random {
    uint32_t __cdecl Random(uint32_t) {
        return 1;
    }
}

namespace key {
    int32_t Wait_For_Key() {
        return 0;
    }
}

namespace RUSS {
    void __cdecl PFMT(int32_t, int32_t, const char*, ...) {}
}

namespace COLONY {
    uint8_t debug_flag = 0;

    void __cdecl Set_Colony_Globals(int16_t) {}
}

namespace INITGAME {
    void __cdecl Init_Colony(int16_t colony_idx, int16_t planet_idx,
                              uint8_t player_num) {
        MOX::colony[colony_idx].planet = planet_idx;
        MOX::colony[colony_idx].owner = static_cast<int8_t>(player_num);
        MOX::planet[planet_idx].colony_index = colony_idx;
    }
}

namespace COLBLDG {
    void __cdecl Delete_From_Colony_Queue(int16_t, int16_t) {}
    void __cdecl Delete_Prod_From_Colony_Queue(int16_t, int16_t) {}

    int16_t __cdecl Create_Ship_From_Design(int16_t player_idx,
                                             const s_ship_design* design,
                                             int16_t) {
        int16_t ship_idx = 0;
        while (ship_idx < 8) {
            const s_ship_data& ship = MOX::ship[ship_idx];
            if (ship_idx >= MOX::NUM_SHIPS ||
                (ship.owner == static_cast<int8_t>(player_idx) &&
                    ship.status == 5)) {
                break;
            }
            ++ship_idx;
        }
        if (ship_idx >= 8) {
            return -1;
        }
        if (ship_idx >= MOX::NUM_SHIPS) {
            MOX::NUM_SHIPS = ship_idx + 1;
        }
        MOX::ship[ship_idx].owner = static_cast<int8_t>(player_idx);
        MOX::ship[ship_idx].d = *design;
        MOX::ship[ship_idx].status = 0;
        return ship_idx;
    }

    int16_t __cdecl Create_Ship(int16_t player_idx, int16_t design_idx,
                                 int16_t colony_idx) {
        return Create_Ship_From_Design(
            player_idx, &MOX::player[player_idx].ship_designs[design_idx],
            colony_idx);
    }
}

namespace COLCALC {
    uint8_t __cdecl Colony_Can_Build_Product(s_colony*, int16_t) {
        test::fakes::colony_can_build_product_count++;
        return 0;
    }

    short __cdecl Colony_Race_Pop_Limit(s_colony*, int16_t) {
        return 10;
    }

    void __cdecl Compute_Star_Colony_Stuff(s_star_data*) {}
    void __cdecl Colony_Environmental_Stuff(s_colony* colony) {
        test::fakes::environmental_refresh_count++;
        test::fakes::last_environmental_colony = colony;
    }
    void __cdecl Colony_Calculation(int16_t) {}
}

namespace INITSHIP {
    void __cdecl Load_Colony_Ship_Design(int16_t, s_ship_design* design) {
        *design = {};
        design->ship_type = SHIP_TYPE_COLONY;
    }

    void __cdecl Load_Outpost_Ship_Design(int16_t, s_ship_design* design) {
        *design = {};
        design->ship_type = SHIP_TYPE_OUTPOST;
    }
}

namespace invasion {
    int __cdecl Colony_Tank_Barracks_Limit(s_colony*) {
        return 0;
    }

    int16_t __cdecl Colony_Infantry_Barracks_Limit(s_colony*) {
        return 0;
    }

    void __cdecl Assign_New_Home_Planet(int8_t, int16_t) {}
    void __cdecl Enforce_Population_Limits_At_Colony(s_colony*) {}
}

namespace SHIPMOVE {
    void __cdecl Update_Ship_ETAs(int16_t) {}
}