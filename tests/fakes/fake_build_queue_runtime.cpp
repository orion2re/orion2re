#include "pch.h"
#include "../fixtures/planet_roster.h"

#include "fake_build_queue_runtime.h"
#include "game/build_queue.h"

namespace {
    s_colony colonies[4];

    s_star_data stars[2];
    s_ship_data ships[16];
    s_player players[MAX_PLAYERS];
}

namespace TEST_BUILD_QUEUE {
    int16_t rejected_product = INT16_MIN;
    int16_t next_ship = 0;
    int colony_calculations = 0;
    int player_calculations = 0;

    void Reset_() {
        memset(colonies, 0, sizeof(colonies));
        test_planets::Reset(4);
        memset(stars, 0xff, sizeof(stars));
        memset(ships, 0, sizeof(ships));
        memset(players, 0, sizeof(players));
        MOX::NUM_COLONIES = 2;
        MOX::NUM_SHIPS = 0;
        MOX::PLAYER_NUM = 0;
        rejected_product = INT16_MIN;
        next_ship = 0;
        colony_calculations = 0;
        player_calculations = 0;
        BUILD_QUEUE::autobuild_settings = {};
        for (int colony_idx = 0; colony_idx < 4; ++colony_idx) {
            colonies[colony_idx].owner = 0;
            colonies[colony_idx].planet_id = SERIAL::Decode_Legacy_Planet_Reference(colony_idx);
            for (int slot = 0; slot < 7; ++slot) {
                colonies[colony_idx].producing[slot] = COLONY_PRODUCTION_NONE;
            }
            test_planets::Record(colony_idx).star_index = 0;
            test_planets::Record(colony_idx).planet_type = PLANET_TYPE_PLANET;
            test_planets::Record(colony_idx).colony_index = colony_idx;
        }
        stars[0].orbit_planet_ids[0] = game::id_planet{0};
        stars[0].orbit_planet_ids[1] = game::id_planet{1};
        players[0].bc = 10000;
        players[0].ship_range = 5;
        players[0].ship_speed = 5;
    }
}

namespace MOX {
    int16_t PLAYER_NUM;
    int16_t NUM_COLONIES;
    int16_t NUM_SHIPS;
    s_colony* colony = colonies;

    s_star_data* star = stars;
    s_ship_data* ship = ships;
    s_player* player = players;
}

namespace COLBLDG {
    int16_t __cdecl Create_Ship_From_Design(int16_t player_idx, const s_ship_design* design, int16_t colony_idx) {
        if (TEST_BUILD_QUEUE::next_ship < 0 || TEST_BUILD_QUEUE::next_ship >= 16) {
            return -1;
        }
        const int16_t ship_idx = TEST_BUILD_QUEUE::next_ship++;
        MOX::ship[ship_idx].owner = static_cast<int8_t>(player_idx);
        MOX::ship[ship_idx].location = game::current_state.planets.at(MOX::colony[colony_idx].planet_id.get()).star_index;
        MOX::ship[ship_idx].d = *design;
        MOX::NUM_SHIPS = std::max<int16_t>(MOX::NUM_SHIPS, ship_idx + 1);
        return ship_idx;
    }

    int16_t __cdecl Create_Ship(int16_t player_idx, int16_t design_idx, int16_t colony_idx) {
        return Create_Ship_From_Design(
            player_idx, &MOX::player[player_idx].ship_designs[design_idx], colony_idx);
    }

    void __cdecl Delete_Ship(int16_t ship_idx) {
        MOX::ship[ship_idx].status = 5;
        MOX::ship[ship_idx].location = -1;
    }
}

namespace COLCALC {
    int32_t __cdecl Production_Cost_To_Buy(int32_t product_cost, int32_t production_spent) {
        if (production_spent >= product_cost) {
            return 0;
        }

        const int32_t half_cost = product_cost / 2;
        const int32_t tenth_cost = product_cost / 10;
        if (production_spent >= half_cost) {
            return (product_cost - production_spent) * 2;
        }

        int32_t cost = (product_cost - half_cost) * 2;
        if (production_spent >= tenth_cost) {
            return cost + (half_cost - production_spent) * 5;
        }

        cost += (half_cost - tenth_cost) * 5;
        return cost + (tenth_cost - production_spent) * 10;
    }

    uint8_t __cdecl Colony_Can_Build_Product(s_colony*, int16_t product) {
        return product == TEST_BUILD_QUEUE::rejected_product ? 0 : 1;
    }

    int32_t __cdecl Colony_Product_Cost(s_colony*, int16_t) {
        return 100;
    }

    int32_t __cdecl Colony_Cost_To_Buy_Given_Product(s_colony*, int32_t,
                                                       int32_t production_spent) {
        return std::max<int32_t>(0, 100 - production_spent);
    }

    int32_t __cdecl Colony_Cost_To_Buy_Producing_Product(s_colony* colony) {
        return Colony_Cost_To_Buy_Given_Product(
            colony, colony->producing[0], colony->production_spent);
    }

    void __cdecl Colony_Buy_Outright(int16_t colony_idx) {
        s_colony& colony = MOX::colony[colony_idx];
        const int32_t cost = Colony_Cost_To_Buy_Producing_Product(&colony);
        MOX::player[colony.owner].bc -= cost;
        colony.bought_outright = static_cast<int16_t>(100 - colony.production_spent);
    }

    bool __cdecl Colony_Is_Blockaded(s_colony*) {
        return false;
    }

    void __cdecl Colony_Calculation(int16_t) {
        ++TEST_BUILD_QUEUE::colony_calculations;
    }

    void __cdecl Do_Player_Colony_Calculations(int16_t) {
        ++TEST_BUILD_QUEUE::player_calculations;
    }
}

namespace INITSHIP {
    void __cdecl Load_Colony_Ship_Design(int16_t, s_ship_design* design) {
        design->ship_type = SHIP_TYPE_COLONY;
    }

    void __cdecl Load_Outpost_Ship_Design(int16_t, s_ship_design* design) {
        design->ship_type = SHIP_TYPE_OUTPOST;
    }

    void __cdecl Load_Transport_Ship_Design(int16_t, s_ship_design* design) {
        design->ship_type = SHIP_TYPE_TRANSPORT;
    }
}
