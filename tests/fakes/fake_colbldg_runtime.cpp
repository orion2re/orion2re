#include "pch.h"

#include "lua/lua_game_ship_identity.h"

namespace {
    char housing[] = "Housing";
    char trade_goods[] = "Trade Goods";
}

namespace TEST_COLBLDG {
    void Set_Buildable_(int16_t building_id, bool value) {
        TECHDATA::buildings[building_id].tech_app = value ? TECH_APP_BIOLOGY : TECH_APP_NO_TECH;
        MOX::player[0].tech_applications[TECH_APP_BIOLOGY] = TECH_RESEARCH_STATUS_RESEARCHED;
    }

    void Reset_() {
        memset(MOX::colony, 0, sizeof(s_colony) * MAX_COLONIES);
        memset(MOX::planet, 0, sizeof(s_planet_data) * MAX_PLANETS);
        memset(MOX::player, 0, sizeof(s_player) * MAX_PLAYERS);
        memset(MOX::ship, 0, sizeof(s_ship_data) * MAX_SHIPS);
        memset(MOX::star, 0xff, sizeof(s_star_data) * (MAX_STARS + 1));
        for (char*& text : ESTRINGS::strings) text = trade_goods;
        ESTRINGS::strings[ESTR_HOUSING] = housing;
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
        MOX::NUM_SHIPS = 0;
        orion2re::lua::game::Reset_Ship_Generations();
    }
}