#include "pch.h"
#include "../fixtures/ship_rows.h"

#include <memory>
#include "game/legacy_ship_location_codec.h"
namespace COMBFIND {
    void __cdecl Process_Retreating_Ships() {}
}
#include "../fixtures/star_roster.h"
#include "../fixtures/colony_roster.h"
#include "../fixtures/planet_roster.h"

#include "lua/lua_game_ship_identity.h"

namespace {

    auto& players = game::current_state.players;
    auto ships = []() {
        auto rows = test_ships::Rows(game::current_state);
        return rows;
    }();
    test_stars::Slots<MAX_STARS> stars;
    test_colonies::Slots<MAX_COLONIES> colonies;

    bool hyperspace_flux;
    int16_t movement_speed;
    bool navigator_pointer_supplied;
} // namespace

namespace TEST_SHIPMOVE {
    int icon_refresh_count;
    int8_t icon_refresh_ship_status;
    int16_t pooled_officer, killed_officer;
    int16_t best_warp[MAX_PLAYERS], ftl_speed[MAX_PLAYERS];
    int drive_queries, speed_queries;
    void (*on_confirmation)();
    std::vector<s_MSG> messages;
    std::vector<int16_t> message_players;

    void Reset_() {
        test_planets::Reset(0);
        players = ::player_roster::Make_Roster();
        stars.Reset();
        game::current_state = std::move(*std::make_unique<game::s_game_state>());
        colonies.Reset();
        memset(TECHDATA::fuel, 0, sizeof(TECHDATA::fuel));
        memset(best_warp, 0, sizeof(best_warp));
        memset(ftl_speed, 0, sizeof(ftl_speed));
        MOX::antarans = {};
        MOX::game_type = 0;
        icon_refresh_count = drive_queries = speed_queries = 0;
        pooled_officer = killed_officer = -1;
        on_confirmation = nullptr;
        messages.clear();
        message_players.clear();

        game::current_state.player_count = 1;
        ship_slots::Resize(game::current_state, 1);
        test_stars::Resize(2);
        MOX::local_player = game::id_player{0};
        MOX::stardate = 35000;
        hyperspace_flux = false;
        movement_speed = 0;
        navigator_pointer_supplied = false;
        SHIPMOVE::phase_shifter_speed_multiplier = {1, 1};
        orion2re::lua::game::Reset_Ship_Generations();

        players[0].ship_range = 20;
        players[0].ship_speed = 4;
        ships[0].owner = factions::From_Legacy(0);
        ships[0].officer_index = -1;
        ships[0].position.Set(legacy_world::Location_From_Slot(game::e_ship_location::at_star, 0, std::ssize(game::current_state.stars)));
        stars[0].x = 0;
        stars[0].y = 0;
        stars[0].has_colony = game::player_bitmap::From_Byte(1);
        stars[1].x = 120;
        stars[1].y = 0;
    }

    void Set_Hyperspace_Flux_(bool active) {
        hyperspace_flux = active;
    }

    int16_t Movement_Speed_() {
        return movement_speed;
    }

    bool Navigator_Pointer_Supplied_() {
        return navigator_pointer_supplied;
    }
} // namespace TEST_SHIPMOVE

namespace game_random {
    uint32_t __cdecl Random(uint32_t) {
        return 1;
    }
} // namespace game_random

namespace MOX {
    s_antaran antarans;
    int8_t game_type;


    game::id_player local_player{0};
    int32_t stardate;

} // namespace MOX

namespace EVENTS {
    int16_t __cdecl Event_Check_Hyperspace_Flux() {
        return hyperspace_flux ? 1 : 0;
    }

    void __cdecl Event_Orion_Discovered(game::id_player) {}
} // namespace EVENTS

namespace REPORT {
    uint8_t something_interesting_happened[MAX_PLAYERS];
}

namespace MAINSCR {
    void __cdecl Update_Ship_Icons_In_Between_Turns() {
        ++TEST_SHIPMOVE::icon_refresh_count;
        TEST_SHIPMOVE::icon_refresh_ship_status = test_ships::Rows(game::current_state)[0].status;
    }
} // namespace MAINSCR

namespace HAROLD {
    int16_t __cdecl XY_To_Star_Id(int16_t x, int16_t y) {
        for (int16_t i = 0; i < std::ssize(game::current_state.stars); ++i) {
            if (test_stars::Record(i).x == x && test_stars::Record(i).y == y) {
                return i;
            }
        }
        return -1;
    }
} // namespace HAROLD

namespace GEO {
    int32_t __cdecl Parsecs_Between_Stars(int16_t star_idx1, int16_t star_idx2) {
        return Parsecs_Between_Points(
            test_stars::Record(star_idx1).x,
            test_stars::Record(star_idx1).y,
            test_stars::Record(star_idx2).x,
            test_stars::Record(star_idx2).y);
    }

    int32_t __cdecl Parsecs_Between_Points(int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
        int distance = abs(x2 - x1) + abs(y2 - y1);
        return (distance + 29) / 30;
    }

    void Move_Player_1_Turn_To_Star(game::owner_slot, int16_t source_x, int16_t source_y,
        const s_star_data& target, int16_t speed, int16_t* out_x, int16_t* out_y,
        int8_t* navigator_flag, int8_t* direction) {
        (void)source_y;
        movement_speed = speed;
        navigator_pointer_supplied = navigator_flag != nullptr;
        *out_x = (int16_t)(source_x + speed * 30);
        if (*out_x > target.x) {
            *out_x = target.x;
        }
        *out_y = target.y;
        *direction = 0;
    }

    bool Black_Hole_Blocks_Points(const game::s_game_state&, int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
        (void)x1;
        (void)y1;
        (void)x2;
        (void)y2;
        return false;
    }
} // namespace GEO

namespace struct_ {
    int16_t __cdecl Test_Bit_Field(void* bit_field, uint16_t bit_index) {
        uint8_t* bytes = static_cast<uint8_t*>(bit_field);
        return (bytes[bit_index >> 3] >> (bit_index & 7)) & 1;
    }
} // namespace struct_

namespace COMBFIND {
    int16_t Closest_Other_Colony_Star(const game::s_game_state&, game::id_player player_idx, int16_t star_idx) {
        (void)player_idx;
        (void)star_idx;
        return -1;
    }
} // namespace COMBFIND

namespace OFFICER {
    void __cdecl Move_Officer_To_Pool(int16_t officer) {
        TEST_SHIPMOVE::pooled_officer = officer;
        auto& leader = game::current_state.leaders.at(officer);
        leader.status = 4;
        leader.eta = 0;
    }
    void __cdecl Kill_Officer(int16_t officer) {
        TEST_SHIPMOVE::killed_officer = officer;
        auto& leader = game::current_state.leaders.at(officer);
        leader.employer = game::NO_PLAYER;
        leader.status = -1;
    }
    int16_t Owned_Officer_Level(const game::s_game_state& state, int16_t leader_idx) {
        return state.leaders.at(leader_idx).xp;
    }
} // namespace OFFICER

namespace GENDRAW {
    int16_t __cdecl Confirmation_Box(const char* message) {
        (void)message;
        if (TEST_SHIPMOVE::on_confirmation) TEST_SHIPMOVE::on_confirmation();
        return 1;
    }
} // namespace GENDRAW

namespace TECHDATA {
    s_tech_drive_data drives[DRIVE_COUNT];
    s_tech_fuel_data fuel[FUEL_COUNT];
}

namespace INITSHIP {
    int16_t Best_Warp_Drive(game::owner_slot faction) {
        const auto player = faction.code;
        ++TEST_SHIPMOVE::drive_queries;
        return TEST_SHIPMOVE::best_warp[player];
    }
    int16_t __cdecl Calc_Player_FTL_Speed(int16_t player) {
        ++TEST_SHIPMOVE::speed_queries;
        return TEST_SHIPMOVE::ftl_speed[player];
    }
    int16_t __cdecl Get_FTL_Speed(int16_t, int16_t drive) {
        return TECHDATA::drives[drive].warp_speed;
    }
}

namespace MSG_ {
    void __cdecl Add_Msg(s_MSG message, int16_t player) {
        TEST_SHIPMOVE::messages.push_back(message);
        TEST_SHIPMOVE::message_players.push_back(player);
    }
}
