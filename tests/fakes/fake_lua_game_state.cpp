#include "pch.h"

#include "lua/lua_game_ship_identity.h"

namespace {
    s_player players[MAX_PLAYERS];
    s_combat_data combat_ships[4];
    s_ship_data ships[MAX_SHIPS];
    s_star_data systems[MAX_STARS];
    s_planet_data planets[MAX_PLANETS];
    s_colony colonies[MAX_PLAYERS];
    s_leader_data leaders[MAX_LEADERS];
    s_nebula nebulas[MAX_NEBULAS];
    char star_names[829][15];
    uint16_t star_name_indices[829];
} // namespace

namespace TEST_LUA_GAME {

    void Reset_() {
        memset(players, 0, sizeof(players));
        memset(combat_ships, 0, sizeof(combat_ships));
        memset(ships, 0, sizeof(ships));
        memset(systems, 0, sizeof(systems));
        memset(planets, 0, sizeof(planets));
        memset(colonies, 0, sizeof(colonies));
        memset(leaders, 0, sizeof(leaders));
        memset(nebulas, 0, sizeof(nebulas));
        for (int index = 0; index < 829; ++index) {
            snprintf(star_names[index], sizeof(star_names[index]), "Star %d", index);
            star_name_indices[index] = static_cast<uint16_t>(index);
        }
        MAPGEN::name_seg = star_names;
        MAPGEN::name_index = star_name_indices;
        MAPGEN::Prepare_Generated_Star_Names();
        MAPGEN::name_seg = nullptr;
        MAPGEN::name_index = nullptr;
        MOX::game_type = 0;
        MOX::NUM_PLAYERS = 2;
        MOX::NUM_SHIPS = 2;
        MOX::NUM_STARS = 2;
        MOX::planet_count = 3;
        MOX::NUM_COLONIES = 0;
        MOX::NUM_NEBULAS = 0;
        MOX::combat_total_ship_count = 2;
        players[0].objectives = PLAYER_OBJECTIVE_HUMAN;
        players[0].eliminated = 1;
        players[0].traits[TRAIT_CURRENT_GOVERNMENT] =
            GOVERNMENT_DEMOCRACY;
        players[0].home_planet_id = 0;
        players[1].objectives = PLAYER_OBJECTIVE_EXPANSIONIST;
        players[1].traits[TRAIT_CURRENT_GOVERNMENT] =
            GOVERNMENT_UNIFICATION;
        players[1].home_planet_id = -1;
        for (s_star_data& system : systems) {
            for (int index = 0; index < 5; ++index) {
                system.planet_index[index] = -1;
            }
        }
        for (s_planet_data& planet : planets) {
            planet.colony_index = -1;
        }
        planets[0].star_index = 0;
        planets[0].orbit = 0;
        planets[0].planet_type = PLANET_TYPE_PLANET;
        planets[0].size = PLANET_SIZE_MEDIUM;
        planets[0].gravity_class = PLANET_GRAVITY_NORMAL_G;
        planets[0].climate = PLANET_CLIMATE_TERRAN;
        planets[0].mineral_class = MINERAL_RESOURCE_ABUNDANT;
        planets[0].planet_special = SYSTEM_SPECIAL_NO_SPECIAL;
        planets[1].star_index = 1;
        planets[1].orbit = 3;
        planets[1].planet_type = PLANET_TYPE_GAS_GIANT;
        planets[1].size = PLANET_SIZE_LARGE;
        planets[1].gravity_class = PLANET_GRAVITY_HEAVY_G;
        planets[1].climate = PLANET_CLIMATE_BARREN;
        planets[1].mineral_class = MINERAL_RESOURCE_RICH;
        planets[1].planet_special = SYSTEM_SPECIAL_GOLD_DEPOSITS;
        planets[2].star_index = 0;
        planets[2].orbit = 2;
        planets[2].planet_type = PLANET_TYPE_ASTEROID;
        planets[2].size = PLANET_SIZE_SMALL;
        planets[2].gravity_class = PLANET_GRAVITY_LOW_G;
        planets[2].climate = PLANET_CLIMATE_TOXIC;
        planets[2].mineral_class = MINERAL_RESOURCE_ULTRA_POOR;
        planets[2].planet_special = SYSTEM_SPECIAL_SPACE_DEBRIS;
        systems[0].planet_index[0] = 0;
        systems[0].planet_index[2] = 2;
        systems[0].x = 10;
        systems[0].y = 20;
        systems[1].planet_index[3] = 1;
        systems[1].x = 30;
        systems[1].y = 40;
        for (std::int16_t field_id = 0;
             field_id < TECH_FIELD_COUNT;
             ++field_id) {
            s_tech_field_data& field =
                TECHDATA::technology_fields[field_id];
            field.tech_field_id = field_id;
        }
        for (std::int16_t application_id = 0;
             application_id < TECH_APP_COUNT;
             ++application_id) {
            s_tech_app_data& application =
                TECHDATA::technology_applications[application_id];
            application.tech_app_id = application_id;
        }
        orion2re::lua::game::Restore_Legacy_Ship_Generations();
    }

} // namespace TEST_LUA_GAME

namespace MOX {
    int16_t first_field_in_group[10] = {
        0, 18, 55, 57, 29, 7, 22, 28, 10, 74};
    uint8_t food_per_farmer_table[PLANET_CLIMATE_COUNT] =
        {0, 0, 0, 2, 2, 4, 4, 2, 4, 6};
    int8_t game_type;
    int16_t NUM_PLAYERS;
    int16_t NUM_SHIPS;
    int16_t NUM_STARS;
    int16_t planet_count;
    int16_t NUM_COLONIES;
    int16_t combat_total_ship_count;
    s_player* player = players;
    s_combat_data* combat_data = combat_ships;
    s_ship_data* ship = ships;
    s_star_data* star = systems;
    s_planet_data* planet = planets;
    s_colony* colony = colonies;
    s_leader_data* leaders = ::leaders;
    s_nebula* nebula = nebulas;
    uint8_t NUM_NEBULAS;
    int16_t MAP_MAX_X = 506;
    int16_t MAP_MAX_Y = 400;
    s_settings settings = {};
    void* techname_seg;
    const char* roman_literals[21] = {nullptr, "I"};
} // namespace MOX

namespace config {
    bool Is_Active_String(const char*) {
        return false;
    }
} // namespace config

namespace farload {
    void* __cdecl Far_Reload(const char*, int32_t, void*) {
        return nullptr;
    }
} // namespace farload

namespace LOADER {
    void __cdecl Load_Player_Special_Names() {
    }
} // namespace LOADER

namespace RUSS {
    void __cdecl Mox_Update() {
    }
} // namespace RUSS

namespace CONFIG_REFRESH {
    void Tech_Names_Loaded() {
    }
} // namespace CONFIG_REFRESH

namespace COLCALC {
    void __cdecl Player_Gets_Tech_App(s_player*, int16_t) {
    }

} // namespace COLCALC

namespace newtech {
    int16_t __cdecl Choose_Tech_Application(int16_t) {
        return 1;
    }
} // namespace newtech

namespace INITSHIP {
    int16_t __cdecl Best_Warp_Drive(int16_t) {
        return 0;
    }

    int16_t __cdecl Calc_Player_FTL_Speed(int16_t) {
        return 0;
    }
} // namespace INITSHIP

namespace exitm {

    [[noreturn]] void __cdecl Exit_With_Message(const char*) {
        std::abort();
    }

} // namespace exitm