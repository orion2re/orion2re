#include "pch.h"
#include "../fixtures/ship_rows.h"
#include "game/fleet_movement.h"
#include "game/legacy_ship_location_codec.h"
#include "../fixtures/star_roster.h"
#include "../fixtures/colony_roster.h"
#include "../fixtures/planet_roster.h"

#include "game/legacy_ids.h"
#include "game/ship_ids.h"

namespace {
    auto ships = []() {
        auto rows = test_ships::Rows(game::current_state);
        return rows;
    }();
    test_stars::Slots<MAX_STARS> stars;
    auto& players = game::current_state.players;
    test_colonies::Slots<MAX_COLONIES> colonies;

    int16_t spawn_count;
    int move_attempt_count;
}

namespace TEST_AIMOVE {
    game::s_game_state* World_() {
        return &game::current_state;
    }

    void Reset_() {
        stars.Reset();
        players = ::player_roster::Make_Roster();
        colonies.Reset();
        test_planets::Reset(0);

        ship_slots::Resize(game::current_state, 1);
        game::current_state.next_ship_id = 90001;
        ships.Set_Key_At_Row(0, game::id_ship{90000});
        test_stars::Resize(2);
        test_stars::Reset(2);
        stars[1].x = 30;
        game::current_state.player_count = 1;
        AIMOVE::rules.eel_spawn_frequency = 30;
        AIMOVE::rules.max_eels = 4;
        ships[0].owner = factions::From_Legacy(NONPLAYER_SHIP_TYPE_EEL);
        ships[0].status = 0;
        ships[0].position.Set(legacy_world::Location_From_Slot(game::e_ship_location::at_star, 0, std::ssize(game::current_state.stars)));
        ships[0].d.speed = 1;
        colonies[0].owner = legacy_world::Decode_Player_Slot(0);
        colonies[0].planet_id = game::id_planet{0};
        test_planets::Record(0).star_id = game::id_star{static_cast<uint32_t>(1)};
        spawn_count = 0;
        move_attempt_count = 0;
    }

    int16_t Spawn_Count_() {
        return spawn_count;
    }

    uint16_t Spawned_Eel_Timer_() {
        return ships[1].d.date_of_design;
    }

    void Set_Wandering_Eel_Count_(int16_t count) {
        ship_slots::Resize(game::current_state, count);
        for (int16_t i = 1; i < count; ++i) {
            ships.Set_Key_At_Row(i, game::id_ship{game::current_state.next_ship_id++});
            ships[i].owner = factions::From_Legacy(NONPLAYER_SHIP_TYPE_EEL);
            ships[i].status = 4;
            ships[i].d.speed = 1;
        }
    }

    void Set_Npc_(int8_t owner, uint8_t strategic_speed) {
        ships[0].owner = factions::From_Legacy(owner);
        ships[0].d.speed = strategic_speed;
    }

    int Move_Attempt_Count_() {
        return move_attempt_count;
    }
}

namespace MOX {



}

namespace INITSHIP {
    void __cdecl Load_Eel_Ship_Design(s_ship_design* design, uint8_t) {
        memset(design, 0, sizeof(*design));
        design->speed = 1;
    }
}

namespace SHIPMOVE {
    e_fleet_evaluation Evaluate_Fleet_Move(game::s_game_state& state, std::span<const game::id_ship> keys, game::s_star_destination destination,
        s_ship_move_info* result, std::optional<game::id_star>) {
        if (!result) return e_fleet_evaluation::invalid_input;
        *result = {};
        if (keys.empty() || game::Destination_Row(state, destination) < 0) return e_fleet_evaluation::invalid_input;
        ++move_attempt_count;
        return e_fleet_evaluation::blocked;
    }

    bool Order_Fleet_Move(game::s_game_state&, std::span<const game::id_ship>, game::s_star_destination, const s_ship_move_info*) {
        return true;
    }

    game::id_ship Create_Nonplayer_Ship(game::id_faction, const s_ship_design*, int16_t, int16_t) {
        spawn_count++;
        auto& state = game::current_state;
        const game::id_ship key{state.next_ship_id++};
        ship_slots::Resize(state, std::max<int16_t>(static_cast<int16_t>(state.ship_slots.size()), 2));
        test_ships::Rows(state).Set_Key_At_Row(1, key);
        test_ships::Rows(state)[1].d.date_of_design = 99;
        return key;
    }

}

namespace MSG_ {
    void __cdecl Add_Msg_All(s_MSG) {
    }
}

namespace SHIPS {
    void Kill_Ship(s_ship_data&) {
    }
}

namespace GEO {
    int32_t __cdecl Parsecs_Between_Points(int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
        return (std::abs(x1 - x2) + std::abs(y1 - y2) + 29) / 30;
    }
}

namespace game_random {
    uint32_t __cdecl Random(uint32_t) {
        return 1;
    }
}
