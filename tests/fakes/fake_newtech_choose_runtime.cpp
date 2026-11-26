#include "pch.h"

namespace {
auto& players = game::current_state.players;
uint32_t selected_weight;
}

namespace TEST_NEWTECH_CHOOSE {
void Reset_() {
    players = ::player_roster::Make_Roster();
    memset(MOX::tech_categories, 0, sizeof(MOX::tech_categories));
    memset(TECHDATA::technology_applications, 0,
           sizeof(TECHDATA::technology_applications));
    memset(TECHDATA::technology_fields, 0,
           sizeof(TECHDATA::technology_fields));
    selected_weight = 0;
    game::current_state.player_count = 1;
    MOX::settings = {};
    MOX::stardate = 0;
}

s_player* Player_() {
    return &players[0];
}

uint32_t Selected_Weight_() {
    return selected_weight;
}
}

namespace MOX {
int8_t tech_categories[51][2];

int32_t stardate;
s_settings settings;
}

namespace TECHDATA {
s_tech_app_data technology_applications[TECH_APP_COUNT];
s_tech_field_data technology_fields[TECH_FIELD_COUNT];
}

namespace aitech {
s_rules rules;
}

namespace COLCALC {
bool __cdecl Player_May_Get_Tech_App(const s_player*, int) {
    return true;
}
}

namespace RUSS {
int16_t __cdecl Get_Weighted_Choice_Int(uint32_t* weights, int16_t count) {
    selected_weight = count > 1 ? weights[1] : 0;
    return 1;
}
}

namespace game_random {
uint32_t __cdecl Random(uint32_t) {
    return 1;
}
}
