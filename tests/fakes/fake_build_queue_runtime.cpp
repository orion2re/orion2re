#include "pch.h"
#include "game/legacy_ship_location_codec.h"
#include "../fixtures/star_roster.h"
#include "../fixtures/colony_roster.h"
#include "../fixtures/planet_roster.h"

#include "fake_build_queue_runtime.h"
#include "game/build_queue.h"

namespace {
    test_colonies::Slots<4> colonies;

    test_stars::Slots<2> stars;
    auto& ships = []() -> auto& {
        auto& rows = game::current_state.ship_records;
        rows.resize(16);
        return rows;
    }();
    auto& players = game::current_state.players;
}

namespace TEST_BUILD_QUEUE {
    int16_t rejected_product = INT16_MIN;
    int16_t next_ship = 0;
    int colony_calculations = 0;
    int player_calculations = 0;

    void Reset_() {
        colonies.Reset();
        test_planets::Reset(4);
        test_stars::Reset(stars.size(), true);
        ships.assign(16, s_ship_data{});
        players = ::player_roster::Make_Roster();
        game::current_state.ship_count = 0;
        game::current_state.next_ship_id = 90000;
        MOX::local_player = game::id_player{0};
        rejected_product = INT16_MIN;
        next_ship = 0;
        colony_calculations = 0;
        player_calculations = 0;
        BUILD_QUEUE::autobuild_settings = {};
        for (int colony_idx = 0; colony_idx < 4; ++colony_idx) {
            colonies[colony_idx].owner = legacy_world::Decode_Player_Slot(0);
            colonies[colony_idx].planet_id = SERIAL::Decode_Legacy_Planet_Reference(colony_idx);
            for (int slot = 0; slot < 7; ++slot) {
                colonies[colony_idx].producing[slot] = COLONY_PRODUCTION_NONE;
            }
            test_planets::Record(colony_idx).star_id = game::id_star{static_cast<uint32_t>(0)};
            test_planets::Record(colony_idx).planet_type = PLANET_TYPE_PLANET;
            test_planets::Record(colony_idx).colony_id = game::id_colony{static_cast<uint32_t>(colony_idx)};
        }
        stars[0].orbit_planet_ids[0] = game::id_planet{0};
        stars[0].orbit_planet_ids[1] = game::id_planet{1};
        players[0].bc = 10000;
        players[0].ship_range = 5;
        players[0].ship_speed = 5;
    }
}

namespace MOX {
    game::id_player local_player{0};

}

namespace COLBLDG {
    game::id_ship Create_Ship_From_Design(game::id_player player, const s_ship_design* design, game::id_colony colony_idx) {
        if (TEST_BUILD_QUEUE::next_ship < 0 || TEST_BUILD_QUEUE::next_ship >= 16) {
            return game::NO_SHIP;
        }
        const int16_t ship_idx = TEST_BUILD_QUEUE::next_ship++;
        game::current_state.ship_records[ship_idx].owner = factions::From_Player(player);
        game::current_state.ship_records[ship_idx].position.Set(legacy_world::Location_From_Slot(game::e_ship_location::at_star, game::current_state.planets.at(test_colonies::Record(colony_idx.id).planet_id).star_id.id, -1));
        game::current_state.ship_records[ship_idx].d = *design;
        game::current_state.ship_count = std::max<int16_t>(game::current_state.ship_count, ship_idx + 1);
        const game::id_ship key{game::current_state.next_ship_id++};
        game::current_state.ship_records.Set_Key_At_Row(ship_idx, key);
        return key;
    }

    game::id_ship Create_Ship(game::id_player player, int16_t design_idx, game::id_colony colony_idx) {
        return Create_Ship_From_Design(
            player, &game::current_state.players.at(player.id).ship_designs[design_idx], colony_idx);
    }

    void Delete_Ship(s_ship_data& ship) {
        ship.status = 5;
        ship.position.Set(game::NO_SHIP_LOCATION);
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

    uint8_t __cdecl Colony_Can_Build_Product(s_colony*, game::colony_product product) {
        return product == TEST_BUILD_QUEUE::rejected_product ? 0 : 1;
    }

    int32_t __cdecl Colony_Product_Cost(s_colony*, game::colony_product) {
        return 100;
    }

    int32_t __cdecl Colony_Cost_To_Buy_Given_Product(s_colony*, game::colony_product,
                                                       int32_t production_spent) {
        return std::max<int32_t>(0, 100 - production_spent);
    }

    int32_t __cdecl Colony_Cost_To_Buy_Producing_Product(s_colony* colony) {
        return Colony_Cost_To_Buy_Given_Product(
            colony, colony->producing[0], colony->production_spent);
    }

    void __cdecl Colony_Buy_Outright(game::id_colony colony_idx) {
        s_colony& colony = test_colonies::Record(colony_idx.id);
        const int32_t cost = Colony_Cost_To_Buy_Producing_Product(&colony);
        game::current_state.players[legacy_world::Encode_Player_Slot(colony.owner)].bc -= cost;
        colony.bought_outright = static_cast<int16_t>(100 - colony.production_spent);
    }

    bool __cdecl Colony_Is_Blockaded(s_colony*) {
        return false;
    }

    void __cdecl Colony_Calculation(game::id_colony) {
        ++TEST_BUILD_QUEUE::colony_calculations;
    }

    void __cdecl Do_Player_Colony_Calculations(game::id_player) {
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
