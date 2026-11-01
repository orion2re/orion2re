#include "pch.h"

#include "lua/lua_game_ship_identity.h"

void Legacy_Qsort(void* base, size_t count, size_t width, legacy_qsort_compare compare) {
    uint8_t* bytes = static_cast<uint8_t*>(base);
    for (size_t end = count; end > 1; --end) {
        for (size_t index = 1; index < end; ++index) {
            uint8_t* previous = bytes + (index - 1) * width;
            uint8_t* current = bytes + index * width;
            if (compare(previous, current) <= 0) {
                continue;
            }
            for (size_t offset = 0; offset < width; ++offset) {
                const uint8_t value = previous[offset];
                previous[offset] = current[offset];
                current[offset] = value;
            }
        }
    }
}

namespace {
    s_colony colonies[3];
    s_planet_data planets[5];
    s_player players[MAX_PLAYERS];
    s_ship_data ships[MAX_SHIPS];
    s_star_data stars[1];
    bool buildable[BUILDING_COUNT];
}

namespace TEST_COLBLDG {
    void Set_Buildable_(int16_t building_id, bool value) {
        buildable[building_id] = value;
    }

    void Reset_() {
        memset(colonies, 0, sizeof(colonies));
        memset(planets, 0, sizeof(planets));
        memset(players, 0, sizeof(players));
        memset(ships, 0, sizeof(ships));
        memset(stars, 0xff, sizeof(stars));
        memset(buildable, 0, sizeof(buildable));
        memset(&MOX::settings, 0, sizeof(MOX::settings));
        for (int i = 0; i < 7; ++i) {
            COLBLDG::current_item[i] = COLONY_PRODUCTION_NONE;
        }
        memset(TECHDATA::buildings, 0, sizeof(TECHDATA::buildings));
        memset(COLBLDG::build_list_format, 0, sizeof(COLBLDG::build_list_format));
        COLBLDG::build_list_format[COLBLDG::BUILD_LIST_PRODUCT_TRADE_GOODS].order = -2;
        COLBLDG::build_list_format[COLBLDG::BUILD_LIST_PRODUCT_HOUSING].order = -1;
        COLCALC::disable_housing = 0;
        COLONY::colony_handle = 0;
        COLONY::cheats = 0;
        COLBLDG::active_prod[0] = -1;
        COLBLDG::field_mode = 0;
        MOX::PLAYER_NUM = 0;
        MOX::NUM_PLAYERS = MAX_PLAYERS;
        MOX::NUM_SHIPS = 0;
        orion2re::lua::game::Reset_Ship_Generations();
    }
}

namespace COLCALC {
    uint8_t disable_housing;

    uint8_t __cdecl Colony_Can_Build_Product(s_colony*, int16_t product) {
        if (product <= BUILDING_NO_BUILDING || product >= BUILDING_COUNT) {
            return 0;
        }
        return buildable[product] ? 1 : 0;
    }
}

namespace TECHDATA {
    s_tech_building_data buildings[BUILDING_COUNT];
}

namespace MOX {
    s_colony* colony = colonies;
    s_planet_data* planet = planets;
    s_player* player = players;
    s_ship_data* ship = ships;
    s_star_data* star = stars;
    s_settings settings;
    int16_t PLAYER_NUM;
    int16_t NUM_PLAYERS;
    int16_t NUM_SHIPS;
}

namespace COLONY {
    int16_t colony_handle;
    uint8_t cheats;

    char* __cdecl Real_Building_Name(int16_t building_id) {
        return TECHDATA::buildings[building_id].name;
    }
}

namespace ESTRINGS {
    char* __cdecl E_Strings(int16_t string_id) {
        static char housing[] = "Housing";
        static char trade_goods[] = "Trade Goods";
        return string_id == ESTR_HOUSING ? housing : trade_goods;
    }
}

namespace RUSS {
    void __cdecl PFMT(int32_t, int32_t, const char*, ...) {
    }
}

namespace key {
    int32_t Wait_For_Key() {
        return 0;
    }
}