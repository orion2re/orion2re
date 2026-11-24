#include "pch.h"
#include "../fixtures/ship_rows.h"
#include "../fixtures/star_roster.h"
#include "../fixtures/planet_roster.h"
#include "../fixtures/colony_roster.h"

namespace TEST_COLONY_REPLICATORS {
    void Reset_() {
        test_colonies::Reset();
        memset(TECHDATA::buildings, 0, sizeof(TECHDATA::buildings));
        TECHDATA::buildings[BUILDING_AUTOMATED_FACTORY].maintenance = 2;
        COLCALC::replicator_food_pp = 2;
        COLCALC::replicator_food_bc = 1;
        COLCALC::trade_goods_ratio = {50, 100};
        COLCALC::planet_bc_bonus = {5, 10};
        const int8_t maintenance[PLANET_CLIMATE_COUNT] = {
            50, 25, 0, 25, 0, 0, 0, 0, 0, 0,
        };
        memcpy(COLCALC::climate_maintenance_modifiers, maintenance,
               sizeof(maintenance));
    }
}

namespace TEST_GOVERNMENT_BONUS {
    void Reset_() {
        s_player* players = game::current_state.players.data();
        test_stars::Slots<1> stars;
        memset(players, 0, sizeof(s_player) * MAX_PLAYERS);
        test_colonies::Reset(1);
        test_planets::Reset(1);
        stars.Reset();
        for (int player = 0; player < MAX_PLAYERS; ++player) {
            stars[0].officer_index[player] = -1;
        }
        test_colonies::Record(0).owner = legacy_world::Decode_Player_Slot(0);
        test_colonies::Record(0).planet_id = game::id_planet{0};
        auto& planet = test_planets::Record(0);
        planet.star_id = game::id_star{static_cast<uint32_t>(0)};
        planet.gravity_class = PLANET_GRAVITY_NORMAL_G;
        planet.food_per_farmer = 2;
        planet.mineral_class = 0;
        COLCALC::minerals_per_mine[0] = 1;
        players[0].objectives = PLAYER_OBJECTIVE_HUMAN;
        COLCALC::android_morale = COLCALC::ANDROID_MORALE_WORKERS_ONLY;
        COLCALC::research_colony_bonus = {5, 10, 15, 30};
        COLCALC::research_scientist_bonus = {3, 2, 5, 1, 2, 3, 1, 3, 1};
        COLCALC::planet_bc_bonus = {5, 10};
        const int8_t maintenance[PLANET_CLIMATE_COUNT] = {
            50, 25, 0, 25, 0, 0, 0, 0, 0, 0,
        };
        memcpy(COLCALC::climate_maintenance_modifiers, maintenance,
               sizeof(maintenance));
        game::current_state.player_count = 1;
    }

    void Cleanup_() {
        memset(game::current_state.players.data(), 0, sizeof(s_player) * MAX_PLAYERS);
        test_colonies::Reset();
        test_planets::Reset();
        test_stars::Reset();
        COLCALC::minerals_per_mine[0] = 0;
        game::current_state.player_count = 0;
    }
}

namespace TEST_PLAYER_STATS {
    void Reset_() {
        memset(game::current_state.players.data(), 0, sizeof(s_player) * MAX_PLAYERS);
        game::current_state = game::s_game_state{};
        test_planets::Reset();
        test_stars::Reset();
        memset(&MOX::settings, 0, sizeof(MOX::settings));
        for (auto& leader : game::current_state.leaders) {
            leader.status = -1;
            leader.employer = game::NO_PLAYER;
        }
        game::current_state.player_count = 1;
        ship_slots::Resize(game::current_state, 1);
        const uint8_t defaults[SHIP_SIZE_COUNT] = {1, 2, 4, 8, 16, 32};
        memcpy(COLCALC::scout_lab_research_bonus, defaults, sizeof(defaults));
        COLCALC::command_points = {5, 1, 2, 3, 1, 2, 3, 2};
        const uint8_t command_usage[SHIP_SIZE_COUNT] = {1, 2, 3, 4, 5, 6};
        memcpy(COLCALC::ship_command_points_usage, command_usage,
               sizeof(command_usage));
    }
}
