#include "pch.h"
#include "../fixtures/ship_rows.h"
#include "../fixtures/star_roster.h"
#include "../fixtures/colony_roster.h"
#include "../fixtures/planet_roster.h"

namespace {
    test_colonies::Slots<2> colonies;

    test_stars::Slots<1> stars;
    auto ships = []() {
        auto rows = test_ships::Rows(game::current_state);
        return rows;
    }();
    auto& players = game::current_state.players;
}

namespace test::fakes {
    int16_t instructor_bonuses[MAX_LEADERS][2];
    int32_t last_played_sound = -1;
    int32_t environmental_refresh_count = 0;
    int32_t colony_can_build_product_count = 0;
    s_colony* last_environmental_colony = nullptr;
}

namespace MOX {

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
    bool __cdecl Officer_Has_Special_Skill(game::id_player, int16_t leader_idx, int16_t skill_idx) {
        return leader_idx >= 0 && leader_idx < MAX_LEADERS &&
            skill_idx >= 0x16 && skill_idx <= 0x17 &&
            test::fakes::instructor_bonuses[leader_idx][skill_idx - 0x16] != 0;
    }

    int32_t __cdecl Officer_Skill_Bonus(uint32_t, int16_t leader_idx, int16_t skill_idx) {
        return Officer_Has_Special_Skill(game::id_player{0}, leader_idx, skill_idx)
            ? test::fakes::instructor_bonuses[leader_idx][skill_idx - 0x16] : 0;
    }
}

namespace HACCESS {
    game::owner_slot __cdecl Ship_Owner(int16_t ship_idx) {
        return test_ships::Rows(game::current_state)[ship_idx].owner;
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

    void __cdecl Set_Colony_Globals(game::id_colony) {}
}

namespace INITGAME {
    game::id_colony __cdecl Init_Colony(game::s_game_state& state, game::id_colony colony_id, game::id_planet planet_id,
                              game::id_player player) {
        if (colony_id == game::NO_COLONY) colony_id = game::id_colony{state.next_colony_id};
        const auto colony_idx = colony_id.id;
        test_colonies::Record(colony_idx).planet_id = planet_id;
        test_colonies::Record(colony_idx).owner = player;
        state.planets.at(planet_id).colony_id = colony_id;
        return colony_id;
    }
}

namespace COLBLDG {
    void __cdecl Delete_From_Colony_Queue(game::s_game_state&, game::id_colony, int16_t) {}
    void __cdecl Delete_Prod_From_Colony_Queue(game::s_game_state&, game::id_colony, game::colony_product) {}

    game::id_ship Create_Ship_From_Design(game::id_player player,
                                             const s_ship_design* design,
                                             game::id_colony) {
        int16_t ship_idx = 0;
        while (ship_idx < 8 && ship_idx < std::ssize(game::current_state.ship_slots)) {
            const s_ship_data& ship = test_ships::Rows(game::current_state)[ship_idx];
            if (ship.owner == factions::From_Player(player) && ship.status == 5) {
                break;
            }
            ++ship_idx;
        }
        if (ship_idx >= 8) {
            return game::NO_SHIP;
        }
        if (ship_idx >= static_cast<int16_t>(game::current_state.ship_slots.size())) {
            ship_slots::Resize(game::current_state, ship_idx + 1);
        }
        test_ships::Rows(game::current_state)[ship_idx].owner = factions::From_Player(player);
        test_ships::Rows(game::current_state)[ship_idx].d = *design;
        test_ships::Rows(game::current_state)[ship_idx].status = 0;
        const game::id_ship key{game::current_state.next_ship_id++};
        test_ships::Rows(game::current_state).Set_Key_At_Row(ship_idx, key);
        return key;
    }

    game::id_ship Create_Ship(game::id_player player, int16_t design_idx,
                                 game::id_colony colony_idx) {
        return Create_Ship_From_Design(
            player, &game::current_state.players.at(player.id).ship_designs[design_idx],
            colony_idx);
    }
}

namespace COLCALC {
    uint8_t __cdecl Colony_Can_Build_Product(s_colony*, game::colony_product) {
        test::fakes::colony_can_build_product_count++;
        return 0;
    }

    short __cdecl Colony_Race_Pop_Limit(const s_colony*, int16_t) {
        return 10;
    }

    void __cdecl Compute_Star_Colony_Stuff(s_star_data*) {}
    void __cdecl Colony_Environmental_Stuff(s_colony* colony) {
        test::fakes::environmental_refresh_count++;
        test::fakes::last_environmental_colony = colony;
    }
    void __cdecl Colony_Calculation(game::id_colony) {}
}

namespace INITSHIP {
    void __cdecl Load_Colony_Ship_Design(game::id_player, s_ship_design* design) {
        *design = {};
        design->ship_type = SHIP_TYPE_COLONY;
    }

    void __cdecl Load_Outpost_Ship_Design(game::id_player, s_ship_design* design) {
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

    void __cdecl Assign_New_Home_Planet(game::s_game_state&, game::id_player, game::id_colony) {}
    void __cdecl Enforce_Population_Limits_At_Colony(s_colony*) {}
}

namespace SHIPMOVE {
    void __cdecl Update_Ship_ETAs(game::id_player) {}
}
