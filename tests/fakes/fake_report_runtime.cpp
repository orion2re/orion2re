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
    char spy_steals_fmt[] = "%s from %s";
    char spy_destroys_fmt[] = "%s %s %s";
    char framed_fmt[] = " Framed %s";
}

namespace TEST_REPORT {
    bool gnn_ready;
    int gnn_calls;
    bool interactive_diplomacy;
    bool diplomacy_ready;
    int service_order, request_order, dialog_order, end_order, update_count;
    int16_t requested_player, dialog_player;
    int movie_calls, room_calls;
    int16_t room_message;
    int evolutionary_calls, discovery_calls;
    int16_t evolutionary_player, discovery_star;
    uint8_t discovery_special;
    int science_calls;
    int science_kind;
    uint16_t science_tech;
    int16_t science_source;
    std::string message;
    int8_t level_report_result;
    int level_report_calls;
    int16_t level_report_player;
    int8_t level_report_show;
    int16_t relocation_destination;
    bool relocation_allowed;
    int move_attempts, move_calls, recalculation_calls, autosave_calls;
    int16_t moved_ship, moved_destination;
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
        memset(TECHDATA::technology_applications, 0, sizeof(TECHDATA::technology_applications));
        MOX::antarans = {};
        MOX::NUM_PLAYERS = 0;
        MOX::NUM_COLONIES = 0;
        MOX::PLAYER_NUM = 0;
        MOX::stardate = 35000;
        MOX::game_type = 0;
        REPORT::Initialize_Reports();
        REPORT::your_spy_steals_fmt = spy_steals_fmt;
        REPORT::your_spy_destroys_fmt = spy_destroys_fmt;
        REPORT::you_framed_fmt = framed_fmt;
        REPORT::enemy_spy_steals_fmt = spy_steals_fmt;
        REPORT::enemy_spy_destroys_fmt = spy_destroys_fmt;
        gnn_ready = false;
        gnn_calls = 0;
        interactive_diplomacy = true;
        diplomacy_ready = false;
        service_order = request_order = dialog_order = end_order = update_count = 0;
        requested_player = dialog_player = -1;
        movie_calls = room_calls = evolutionary_calls = discovery_calls = science_calls = 0;
        room_message = evolutionary_player = discovery_star = science_source = -1;
        discovery_special = 0;
        science_kind = science_tech = 0;
        message.clear();
        level_report_result = 0;
        level_report_calls = 0;
        level_report_player = -1;
        level_report_show = 0;
        relocation_destination = -1;
        relocation_allowed = true;
        move_attempts = move_calls = recalculation_calls = autosave_calls = 0;
        moved_ship = moved_destination = -1;

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
    int16_t NUM_PLAYERS;
    int8_t game_type;
    s_antaran antarans;
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
    int8_t __cdecl Officer_Made_Level_Report(int16_t player, int8_t show) {
        ++TEST_REPORT::level_report_calls;
        TEST_REPORT::level_report_player = player;
        TEST_REPORT::level_report_show = show;
        return TEST_REPORT::level_report_result;
    }
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
        ++TEST_REPORT::recalculation_calls;
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
    void __cdecl Message_Box(const char* text) {
        TEST_REPORT::message = text;
    }
    int16_t __cdecl Do_Confirmation_Box(const char*) {
        return TEST_REPORT::confirmation_result;
    }
}

namespace HACCESS {
    bool __cdecl Star_Has_Relocation(int16_t, int16_t) {
        return TEST_REPORT::relocation_destination >= 0;
    }
    int16_t __cdecl Relocation(int16_t, int16_t) {
        return TEST_REPORT::relocation_destination;
    }
    char* __cdecl Get_Planet_Name(int16_t) {
        return planet_name;
    }
}

namespace MAINPUPS {
    void __cdecl New_System_Discovery_Popup(int16_t star, uint8_t special) {
        ++TEST_REPORT::discovery_calls;
        TEST_REPORT::discovery_star = star;
        TEST_REPORT::discovery_special = special;
    }
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
    s_tech_app_data technology_applications[TECH_APP_COUNT];
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

namespace EVENTS {
    bool __cdecl Check_For_Event_Message() { return TEST_REPORT::gnn_ready; }
    void __cdecl Check_For_Event() { ++TEST_REPORT::gnn_calls; }
}

namespace RUSS {
    void __cdecl Mox_Update() {
        ++TEST_REPORT::update_count;
        TEST_REPORT::diplomacy_ready = true;
    }
}

namespace NETMOX {
    void __cdecl Net_Request_AI_Diplomacy(uint8_t player) {
        TEST_REPORT::requested_player = player;
        TEST_REPORT::request_order = ++TEST_REPORT::service_order;
        TEST_REPORT::diplomacy_ready = false;
    }
    int8_t __cdecl Net_May_Begin_AI_Diplomacy() { return TEST_REPORT::diplomacy_ready; }
    void __cdecl Net_End_AI_Diplomacy() { TEST_REPORT::end_order = ++TEST_REPORT::service_order; }
}

namespace DIP_SCRN {
    int8_t __cdecl Diplomacy_Message_Is_Interactive(int16_t) { return TEST_REPORT::interactive_diplomacy; }
    void __cdecl Npc_Diplomacy_Screen(int16_t player, int8_t) {
        TEST_REPORT::dialog_player = player;
        TEST_REPORT::dialog_order = ++TEST_REPORT::service_order;
    }
}

namespace ERIC {
    void __cdecl Play_Antarans_Attack_Flic() { ++TEST_REPORT::movie_calls; }
}

namespace ANTAROOM {
    void __cdecl Main_Antaran_Room_Screen(int16_t message) {
        ++TEST_REPORT::room_calls;
        TEST_REPORT::room_message = message;
    }
}

namespace raceopt {
    void __cdecl Setup_Evolutionary_Upgrade(int16_t player) {
        ++TEST_REPORT::evolutionary_calls;
        TEST_REPORT::evolutionary_player = player;
        MOX::player[player].picked_evolutionary_upgrade = 1;
    }
}

namespace SCIENCE {
    void __cdecl Show_Off_Stolen_Tech(uint16_t tech, int16_t player) {
        ++TEST_REPORT::science_calls;
        TEST_REPORT::science_kind = 1;
        TEST_REPORT::science_tech = tech;
        TEST_REPORT::science_source = player;
    }
    void __cdecl Show_Off_Artifact_Tech(uint16_t tech, int16_t planet) {
        ++TEST_REPORT::science_calls;
        TEST_REPORT::science_kind = 3;
        TEST_REPORT::science_tech = tech;
        TEST_REPORT::science_source = planet;
    }
}

namespace SHIPMOVE {
    void __cdecl Ships_Try_To_Move_To(int16_t*, int16_t, s_ship_move_info* result) {
        ++TEST_REPORT::move_attempts;
        *result = {};
        result->moving = TEST_REPORT::relocation_allowed;
    }
    void __cdecl Make_Ships_Move_To(int16_t* ships, int16_t destination, s_ship_move_info*) {
        ++TEST_REPORT::move_calls;
        TEST_REPORT::moved_ship = ships[0];
        TEST_REPORT::moved_destination = destination;
    }
}

namespace NEXTTURN {
    void Do_Autosave() { ++TEST_REPORT::autosave_calls; }
}
