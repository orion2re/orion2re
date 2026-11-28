#include "pch.h"
#include "../fixtures/ship_rows.h"
#include "../fixtures/star_roster.h"
#include "../fixtures/colony_roster.h"
#include "../fixtures/planet_roster.h"

namespace {
    test_colonies::Slots<MAX_COLONIES> colonies;

    test_stars::Slots<MAX_STARS> stars;
    auto ships = []() {
        auto rows = test_ships::Rows(game::current_state);
        return rows;
    }();
    auto& players = game::current_state.players;
}

namespace TEST_COLONIZE {
    uint32_t planet_worth;

    void Reset_() {
        planet_worth = 0;
        colonies.Reset();
        test_planets::Reset(0);
        test_stars::Reset(1, true);
        players = ::player_roster::Make_Roster();
        test_stars::Resize(1);
        for (auto& ids : game::current_state.colony_reservations) ids.push_back(game::id_colony{0});
        ship_slots::Resize(game::current_state, 0);
        ship_slots::Resize(game::current_state, 2);
        game::current_state.player_count = 1;
        MOX::stardate = 0;
        COLONIZE::first_pop_to_production = 0;
    }
}

namespace MOX {

    int32_t stardate;

}

namespace AIDATA {
    int32_t __cdecl Uncolonized_Planet_Worth_To_Player(game::s_game_state*, s_planet_data*, s_player*) {
        return TEST_COLONIZE::planet_worth;
    }
}

namespace COLCALC {
    void __cdecl Colony_Calculation(game::id_colony) {
    }

    void __cdecl Compute_Star_Colony_Stuff(s_star_data*) {
    }
}

namespace ERICNET {
    void __cdecl Remove_Building(game::s_game_state&, game::id_colony, int16_t) {
    }
}

namespace INITGAME {
    game::id_colony __cdecl Init_Colony(game::s_game_state& state, game::id_colony colony_id, game::id_planet id, game::id_player player) {
        if (colony_id == game::NO_COLONY) colony_id = game::id_colony{state.next_colony_id};
        const auto colony_idx = colony_id.id;
        test_colonies::Record(colony_idx).owner = player;
        test_colonies::Record(colony_idx).planet_id = id;
        state.planets.at(id).colony_id = colony_id;
        return colony_id;
    }
}

namespace MAINSCR {
    void __cdecl Update_Ship_Icons_In_Between_Turns() {
    }
}

namespace MSG_ {
    void __cdecl Add_Msg(s_MSG, game::id_player) {
    }
}

namespace REPORT {
    game::id_colony __cdecl Player_Colony_Base_At_Star(game::id_player, int16_t) {
        return game::NO_COLONY;
    }

    s_ship_data* __cdecl Player_Ship_Type_At_Star(game::id_player player, int16_t type, int16_t star, int8_t) {
        auto& state = game::current_state;
        for (const auto key : state.ship_slots) {
            if (key == game::NO_SHIP) continue;
            auto& ship = state.ships.at(key);
            if (ship.status == 0 && ship.owner == factions::From_Player(player) &&
                ship.d.ship_type == type && game::Ship_Orbit_Row(state, ship.position) == star) return &ship;
        }
        return nullptr;
    }
}

namespace SHIPS {
    void Kill_Ship(s_ship_data& ship) {
        ship.status = 5;
        ship.position.Set(game::NO_SHIP_LOCATION);
    }
}

namespace TECHDATA {
    s_tech_building_data buildings[BUILDING_COUNT];
}
