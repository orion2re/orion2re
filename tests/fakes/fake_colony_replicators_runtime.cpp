#include "pch.h"

namespace TEST_COLONY_REPLICATORS {
    void Reset_() {
        memset(MOX::colony, 0, sizeof(s_colony) * MAX_COLONIES);
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
        s_player* players = MOX::player;
        s_colony* colonies = MOX::colony;
        s_planet_data* planets = MOX::planet;
        s_star_data* stars = MOX::star;
        memset(players, 0, sizeof(s_player) * MAX_PLAYERS);
        memset(colonies, 0, sizeof(s_colony) * MAX_COLONIES);
        memset(planets, 0, sizeof(s_planet_data) * MAX_PLANETS);
        memset(stars, 0, sizeof(s_star_data) * (MAX_STARS + 1));
        for (int player = 0; player < MAX_PLAYERS; ++player) {
            stars[0].officer_index[player] = -1;
        }
        colonies[0].owner = 0;
        colonies[0].planet = 0;
        planets[0].star_index = 0;
        planets[0].gravity_class = PLANET_GRAVITY_NORMAL_G;
        planets[0].food_per_farmer = 2;
        planets[0].mineral_class = 0;
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
        MOX::NUM_PLAYERS = 1;
        MOX::NUM_COLONIES = 1;
    }

    void Cleanup_() {
        memset(MOX::player, 0, sizeof(s_player) * MAX_PLAYERS);
        memset(MOX::colony, 0, sizeof(s_colony) * MAX_COLONIES);
        memset(MOX::planet, 0, sizeof(s_planet_data) * MAX_PLANETS);
        memset(MOX::star, 0, sizeof(s_star_data) * (MAX_STARS + 1));
        COLCALC::minerals_per_mine[0] = 0;
        MOX::NUM_PLAYERS = 0;
        MOX::NUM_COLONIES = 0;
    }
}

namespace TEST_PLAYER_STATS {
    void Reset_() {
        memset(MOX::player, 0, sizeof(s_player) * MAX_PLAYERS);
        memset(MOX::ship, 0, sizeof(s_ship_data) * MAX_SHIPS);
        memset(MOX::leaders, 0, sizeof(s_leader_data) * MAX_LEADERS);
        memset(MOX::planet, 0, sizeof(s_planet_data) * MAX_PLANETS);
        memset(MOX::star, 0, sizeof(s_star_data) * (MAX_STARS + 1));
        memset(&MOX::settings, 0, sizeof(MOX::settings));
        for (int i = 0; i < MAX_LEADERS; ++i) {
            MOX::leaders[i].status = -1;
            MOX::leaders[i].player_index = -1;
        }
        MOX::NUM_PLAYERS = 1;
        MOX::NUM_COLONIES = 0;
        MOX::NUM_SHIPS = 1;
        const uint8_t defaults[SHIP_SIZE_COUNT] = {1, 2, 4, 8, 16, 32};
        memcpy(COLCALC::scout_lab_research_bonus, defaults, sizeof(defaults));
        COLCALC::command_points = {5, 1, 2, 3, 1, 2, 3, 2};
        const uint8_t command_usage[SHIP_SIZE_COUNT] = {1, 2, 3, 4, 5, 6};
        memcpy(COLCALC::ship_command_points_usage, command_usage,
               sizeof(command_usage));
    }
}
