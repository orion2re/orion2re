#include "pch.h"
#include "game/fleet_movement.h"
#include "game/legacy_ship_location_codec.h"
#include "../fixtures/star_roster.h"
#include "../fixtures/colony_roster.h"
#include "../fixtures/planet_roster.h"

#include "game/legacy_world.h"

namespace {
    auto& ships = []() -> auto& {
        auto& rows = game::current_state.ship_records;
        rows.resize(MAX_SHIPS);
        return rows;
    }();
    test_stars::Slots<MAX_STARS> stars;
    s_player players[MAX_PLAYERS];
    test_colonies::Slots<MAX_COLONIES> colonies;

    int16_t spawn_count;
    int move_attempt_count;
}

namespace TEST_AIMOVE {
    const game::s_view* World_() {
        static game::s_view world;
        world.state = &game::current_state;
        world.players = players;
        world.player_count = &MOX::NUM_PLAYERS;

        return &world;
    }

    void Reset_() {
        ships.assign(MAX_SHIPS, s_ship_data{});
        stars.Reset();
        memset(players, 0, sizeof(players));
        colonies.Reset();
        test_planets::Reset(0);

        game::current_state.ship_count = 1;
        game::current_state.next_ship_id = 90001;
        ships.Set_Key_At_Row(0, game::id_ship{90000});
        test_stars::Resize(2);
        test_stars::Reset(2);
        stars[1].x = 30;
        MOX::NUM_PLAYERS = 1;
        AIMOVE::rules.eel_spawn_frequency = 30;
        AIMOVE::rules.max_eels = 4;
        ships[0].owner = NONPLAYER_SHIP_TYPE_EEL;
        ships[0].status = 0;
        ships[0].position.Set(legacy_world::Location_From_Slot(game::e_ship_location::at_star, 0, std::ssize(game::current_state.stars)));
        ships[0].d.speed = 1;
        colonies[0].owner = 0;
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
        game::current_state.ship_count = count;
        for (int16_t i = 1; i < count; ++i) {
            ships.Set_Key_At_Row(i, game::id_ship{game::current_state.next_ship_id++});
            ships[i].owner = NONPLAYER_SHIP_TYPE_EEL;
            ships[i].status = 4;
            ships[i].d.speed = 1;
        }
    }

    void Set_Npc_(int8_t owner, uint8_t strategic_speed) {
        ships[0].owner = owner;
        ships[0].d.speed = strategic_speed;
    }

    int Move_Attempt_Count_() {
        return move_attempt_count;
    }
}

namespace MOX {

    int16_t NUM_PLAYERS;
    s_player* player = players;

}

namespace INITSHIP {
    void __cdecl Load_Eel_Ship_Design(s_ship_design* design, uint8_t) {
        memset(design, 0, sizeof(*design));
        design->speed = 1;
    }
}

namespace SHIPMOVE {
    e_fleet_evaluation Evaluate_Fleet_Move(std::span<const game::id_ship> keys, game::s_star_destination destination,
        s_ship_move_info* result, std::optional<game::id_star>) {
        if (!result) return e_fleet_evaluation::invalid_input;
        *result = {};
        if (keys.empty() || game::Destination_Row(game::current_state, destination) < 0) return e_fleet_evaluation::invalid_input;
        ++move_attempt_count;
        return e_fleet_evaluation::blocked;
    }

    bool Order_Fleet_Move(std::span<const game::id_ship>, game::s_star_destination, const s_ship_move_info*) {
        return true;
    }

    game::id_ship Create_Nonplayer_Ship(int8_t, const s_ship_design*, int16_t, int16_t) {
        spawn_count++;
        auto& state = game::current_state;
        const game::id_ship key{state.next_ship_id++};
        state.ship_count = std::max<int16_t>(state.ship_count, 2);
        state.ship_records.Set_Key_At_Row(1, key);
        state.ship_records[1].d.date_of_design = 99;
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
