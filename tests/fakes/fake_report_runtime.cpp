#include "pch.h"

namespace {
    s_colony colonies[MAX_COLONIES];
    s_planet_data planets[MAX_PLANETS];
    s_player players[MAX_PLAYERS];
    s_ship_data ships[MAX_SHIPS];
    s_star_data stars[MAX_STARS];
    s_leader_data leaders[MAX_LEADERS];
    char planet_name[] = "Planet";
    char trash_colony_base_fmt[] = "Trash colony base for %d BC?";
}

namespace TEST_REPORT {
    int16_t colonizable_planet_count;
    int16_t confirmation_result;
    bool accept_officer_offer;
    s_ship_data* cleared_auto_colonization_ship;

    void Reset_() {
        memset(colonies, 0, sizeof(colonies));
        memset(planets, 0, sizeof(planets));
        memset(players, 0, sizeof(players));
        memset(ships, 0, sizeof(ships));
        memset(stars, 0xff, sizeof(stars));
        memset(leaders, 0, sizeof(leaders));
        memset(REPORT::done_colonize_check, 0, sizeof(REPORT::done_colonize_check));
        memset(&MOX::settings, 0, sizeof(MOX::settings));
        memset(MSG_::msgs, 0, sizeof(MSG_::msgs));

        MOX::NUM_STARS = 1;
        MOX::NUM_SHIPS = 1;
        MOX::star[0].colonize_player = 0;
        MOX::star[0].ignore_colony_ships = 0;
        MOX::ship[0].owner = 0;
        MOX::ship[0].location = 0;
        MOX::ship[0].d.ship_type = SHIP_TYPE_OUTPOST;
        REPORT::no_outpost_prompt = 0;
        COLONY::allow_native_annihilation = 0;
        OFFICER::rejected_offer_keeps_timer = 0;
        accept_officer_offer = false;
        REPORT::really_trash_colony_base_fmt = trash_colony_base_fmt;
        colonizable_planet_count = 0;
        confirmation_result = 0;
        cleared_auto_colonization_ship = nullptr;
    }

    void Add_Colony_Ship_() {
        MOX::ship[1].owner = 0;
        MOX::ship[1].location = 0;
        MOX::ship[1].d.ship_type = SHIP_TYPE_COLONY;
        MOX::NUM_SHIPS = 2;
    }

    void Add_Colony_Base_() {
        MOX::star[0].planet_index[0] = 0;
        MOX::planet[0].colony_index = 0;
        MOX::colony[0].owner = 0;
        MOX::colony[0].buildings[BUILDING_COLONY_BASE] = 1;
    }
}

namespace MSG_ {
    s_MSG msgs[MAX_MESSAGES];
    uint8_t first_msg[MAX_PLAYERS];
    uint8_t n_msgs[MAX_PLAYERS];

    int16_t __cdecl N_Msgs_For_Player(int16_t player_idx) {
        return n_msgs[player_idx];
    }

    int16_t __cdecl First_Msg_For_Player(int16_t player_idx) {
        return first_msg[player_idx];
    }
}

namespace MOX {
    int16_t NUM_COLONIES;
    int16_t NUM_STARS;
    int16_t NUM_SHIPS;
    int16_t PLAYER_NUM;
    int32_t stardate;
    int16_t current_screen;
    int16_t return_screen;
    int16_t screen_data;
    uint16_t global_esc;
    s_colony* colony = colonies;
    s_planet_data* planet = planets;
    s_player* player = players;
    s_ship_data* ship = ships;
    s_star_data* star = stars;
    s_leader_data* leaders = ::leaders;
    s_settings settings;
}

namespace OFFICER {
    int32_t __cdecl Calculate_Officer_Cost(int16_t, int16_t) {
        return 0;
    }

    void __cdecl Set_Officer_To_Player(int16_t officer_idx, int16_t player_idx, uint8_t is_instant) {
        MOX::leaders[officer_idx].player_index = (int8_t)player_idx;
        MOX::leaders[officer_idx].status = is_instant == 0 ? 4 : 0;
    }
}

namespace COLCALC {
    void __cdecl Do_Player_Colony_Calculations(int16_t) {
    }

    void __cdecl Update_Player_Stats(s_player*) {
    }
}

namespace COLONIZE {
    void __cdecl Make_New_Colony(int16_t, int16_t) {
    }

    void __cdecl Make_New_Outpost(int16_t, int16_t) {
    }

    int16_t Star_N_Colonizable_Planets_For_Player(s_star_data*, int16_t) {
        return TEST_REPORT::colonizable_planet_count;
    }
}

namespace COLONY {
    int16_t orbit_temp;

    void __cdecl Colony_Screen() {
    }

    bool __cdecl Colony_Needs_Occupation_Policy(int16_t colony_idx) {
        const s_colony* colony = &MOX::colony[colony_idx];
        if (colony->occupation_policy != 4) {
            return false;
        }
        for (int16_t i = 0; i < colony->n_pops; ++i) {
            if (POP::Is_Conquered(colony->pop[i]) ||
                (allow_native_annihilation != 0 && POP::Is_Native(colony->pop[i]))) {
                return true;
            }
        }
        return false;
    }
}

namespace ERICNET {
    void __cdecl Remove_Building(int16_t, int16_t) {
    }
}

namespace GENDRAW {
    int16_t __cdecl Do_Confirmation_Box(const char*) {
        return TEST_REPORT::confirmation_result;
    }
}

namespace HACCESS {
    char* __cdecl Get_Planet_Name(int16_t) {
        return planet_name;
    }
}

namespace MAINPUPS {
    bool __cdecl Random_New_Officer_Popup(int16_t) {
        return TEST_REPORT::accept_officer_offer;
    }

    int16_t __cdecl New_Colony_Selection_Popup(int16_t, uint8_t) {
        return PLANET_INDEX_NONE;
    }

    int16_t __cdecl New_Outpost_Selection_Popup(int16_t) {
        return PLANET_INDEX_NONE;
    }
}

namespace MAINSCR {
    void __cdecl Update_Ship_Icons_In_Between_Turns() {
    }
}

namespace MOVEBOX {
    void __cdecl Remove_Box_Check() {
    }
}

namespace PLNTSUM {
    int16_t __cdecl Colony_Ship_Auto_Colonization(s_ship_data*) {
        return PLANET_INDEX_NONE;
    }
}

namespace SHIPS {
    void __cdecl Clear_Auto_Colonization_Flag(s_ship_data* ship) {
        TEST_REPORT::cleared_auto_colonization_ship = ship;
    }

    void __cdecl Kill_Ship(int16_t) {
    }
}

namespace TECHDATA {
    s_tech_building_data buildings[BUILDING_COUNT];
}

namespace TECH {
    void __cdecl Tech_Select() {
    }
}

namespace namestar {
    int8_t __cdecl Can_Change_Star_Name(int16_t) {
        return 0;
    }

    void __cdecl Change_Star_Name_Popup(int16_t, int16_t, int16_t, int8_t) {
    }

    int16_t __cdecl Star_Name_Popup_Screen_Center_X() {
        return 0;
    }

    int16_t __cdecl Star_Name_Popup_Screen_Center_Y() {
        return 0;
    }
}

namespace struct_ {
    int16_t __cdecl Test_Bit_Field(void* bit_field, uint16_t bit_index) {
        const uint8_t* bytes = static_cast<const uint8_t*>(bit_field);
        return (bytes[bit_index / 8] >> (bit_index % 8)) & 1;
    }

    void __cdecl Set_Bit_Field(void* bit_field, uint16_t bit_index) {
        uint8_t* bytes = static_cast<uint8_t*>(bit_field);
        bytes[bit_index / 8] |= (uint8_t)(1u << (bit_index % 8));
    }
}
