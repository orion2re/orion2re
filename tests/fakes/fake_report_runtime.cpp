#include "pch.h"
#include "../fixtures/ship_rows.h"
#include "game/fleet_movement.h"
#include "game/legacy_ship_location_codec.h"
#include "../fixtures/star_roster.h"
#include "../fixtures/colony_roster.h"
#include "../fixtures/planet_roster.h"

namespace {
    test_colonies::Slots<MAX_COLONIES> colonies;

    auto& players = game::current_state.players;
    auto ships = []() {
        auto rows = test_ships::Rows(game::current_state);
        return rows;
    }();
    test_stars::Slots<MAX_STARS> stars;
    char planet_name[] = "Planet";
    char trash_colony_base_fmt[] = "Trash colony base for %d BC?";
    char colonize_preselected_planet_fmt[] = "Colonize %s?";
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
    void (*on_confirmation)();
    bool accept_officer_offer;
    s_ship_data* cleared_auto_colonization_ship;
    game::id_planet colonized_planet;
    bool founding_succeeds;
    int founding_attempts;
    int colony_assets_consumed;

    void Reset_() {
        founding_succeeds = true;
        founding_attempts = 0;
        colony_assets_consumed = 0;
        colonies.Reset();
        test_planets::Reset(0);
        players = ::player_roster::Make_Roster();
        test_stars::Reset(1, true);
        game::current_state = game::s_game_state{};
        memset(REPORT::done_colonize_check, 0, sizeof(REPORT::done_colonize_check));
        memset(&MOX::settings, 0, sizeof(MOX::settings));
        memset(MSG_::msgs, 0, sizeof(MSG_::msgs));
        memset(TECHDATA::technology_applications, 0, sizeof(TECHDATA::technology_applications));
        MOX::antarans = {};
        game::current_state.player_count = 0;
        MOX::local_player = game::id_player{0};
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

        test_stars::Resize(1);
        ship_slots::Resize(game::current_state, 1);
        game::current_state.next_ship_id = 91001;
        test_ships::Rows(game::current_state).Set_Key_At_Row(0, game::id_ship{91000});
        test_stars::Record(0).colonize_player = factions::From_Legacy(0);
        test_stars::Record(0).ignore_colony_ships.reset();
        test_ships::Rows(game::current_state)[0].owner = factions::From_Legacy(0);
        test_ships::Rows(game::current_state)[0].position.Set(legacy_world::Location_From_Slot(game::e_ship_location::at_star, 0, std::ssize(game::current_state.stars)));
        test_ships::Rows(game::current_state)[0].d.ship_type = SHIP_TYPE_OUTPOST;
        REPORT::no_outpost_prompt = 0;
        COLONY::allow_native_annihilation = 0;
        OFFICER::rejected_offer_keeps_timer = 0;
        accept_officer_offer = false;
        REPORT::really_trash_colony_base_fmt = trash_colony_base_fmt;
        REPORT::colonize_preselected_planet_fmt = colonize_preselected_planet_fmt;
        colonizable_planet_count = 0;
        confirmation_result = 0;
        on_confirmation = nullptr;
        cleared_auto_colonization_ship = nullptr;
        colonized_planet = game::NO_PLANET;
    }

    void Add_Colony_Ship_() {
        const auto key = game::Allocate_Ship_Id(game::current_state);
        test_ships::Rows(game::current_state).Set_Key_At_Row(1, key);
        test_ships::Rows(game::current_state)[1].owner = factions::From_Legacy(0);
        test_ships::Rows(game::current_state)[1].position.Set(legacy_world::Location_From_Slot(game::e_ship_location::at_star, 0, std::ssize(game::current_state.stars)));
        test_ships::Rows(game::current_state)[1].d.ship_type = SHIP_TYPE_COLONY;
        ship_slots::Resize(game::current_state, 2);
    }

    void Add_Colony_Base_() {
        test_stars::Record(0).orbit_planet_ids[0] = game::id_planet{0};
        test_planets::Record(0).colony_id = game::id_colony{0};
        test_colonies::Record(0).owner = legacy_world::Decode_Player_Slot(0);
        test_colonies::Record(0).buildings[BUILDING_COLONY_BASE] = 1;
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

    int8_t game_type;
    s_antaran antarans;

    game::id_player local_player{0};
    int32_t stardate;
    int16_t current_screen;
    int16_t return_screen;
    int16_t screen_data;
    uint16_t global_esc;

    s_settings settings;
}

namespace OFFICER {
    int8_t __cdecl Officer_Made_Level_Report(game::id_player player_key, int8_t show) {
        int player = legacy_world::Encode_Player_Slot(player_key);
        ++TEST_REPORT::level_report_calls;
        TEST_REPORT::level_report_player = player;
        TEST_REPORT::level_report_show = show;
        return TEST_REPORT::level_report_result;
    }
    int32_t __cdecl Calculate_Officer_Cost(game::id_player, int16_t) {
        return 0;
    }

    void __cdecl Set_Officer_To_Player(int16_t officer_idx, game::id_player player_key, uint8_t is_instant) {
        int player_idx = legacy_world::Encode_Player_Slot(player_key);
        auto& leader = game::current_state.leaders.at(officer_idx);
        leader.employer = legacy_world::Decode_Player_Slot(player_idx);
        leader.status = is_instant == 0 ? 4 : 0;
    }
}

namespace COLCALC {
    void __cdecl Do_Player_Colony_Calculations(game::id_player) {
        ++TEST_REPORT::recalculation_calls;
    }

    void __cdecl Update_Player_Stats(s_player*) {
    }
}

namespace COLONIZE {
    bool __cdecl Make_New_Colony(game::id_planet id, game::id_player) {
        ++TEST_REPORT::founding_attempts;
        if (!TEST_REPORT::founding_succeeds) return false;
        TEST_REPORT::colonized_planet = id;
        return true;
    }

    bool __cdecl Make_New_Outpost(game::id_planet, game::id_player) {
        ++TEST_REPORT::founding_attempts;
        return TEST_REPORT::founding_succeeds;
    }

    int16_t Star_N_Colonizable_Planets_For_Player(s_star_data*, game::id_player) {
        return TEST_REPORT::colonizable_planet_count;
    }
}

namespace COLONY {
    int16_t orbit_temp;

    void __cdecl Colony_Screen() {
    }

    bool __cdecl Colony_Needs_Occupation_Policy(game::id_colony colony_idx) {
        const s_colony* colony = &test_colonies::Record(colony_idx.id);
        if (colony->occupation_policy != 4) {
            return false;
        }
        for (int16_t i = 0; i < colony->n_pops; ++i) {
            if (colony->pop[i].conquered ||
                (allow_native_annihilation != 0 && POP::Is_Native(colony->pop[i]))) {
                return true;
            }
        }
        return false;
    }
}

namespace ERICNET {
    void __cdecl Remove_Building(game::s_game_state&, game::id_colony, int16_t) {
    }
}

namespace GENDRAW {
    void __cdecl Message_Box(const char* text) {
        TEST_REPORT::message = text;
    }
    int16_t __cdecl Do_Confirmation_Box(const char*) {
        if (TEST_REPORT::on_confirmation) TEST_REPORT::on_confirmation();
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
    char* Get_Planet_Name(const game::s_game_state&, game::id_planet) {
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

    game::id_planet __cdecl New_Colony_Selection_Popup(int16_t, uint8_t) {
        return game::NO_PLANET;
    }

    game::id_planet __cdecl New_Outpost_Selection_Popup(int16_t) {
        return game::NO_PLANET;
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
    game::id_planet Colony_Ship_Auto_Colonization(const s_ship_data* ship) {
        return ship ? ship->colonization_target : game::NO_PLANET;
    }
}

namespace SHIPS {
    void __cdecl Clear_Auto_Colonization_Flag(s_ship_data* ship) {
        TEST_REPORT::cleared_auto_colonization_ship = ship;
        ship->colonization_target = game::NO_PLANET;
    }

    void Kill_Ship(s_ship_data&) {
        ++TEST_REPORT::colony_assets_consumed;
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
    void Npc_Diplomacy_Screen(game::id_player player, int8_t) {
        TEST_REPORT::dialog_player = legacy_world::Encode_Player_Slot(player);
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
        game::current_state.players[player].picked_evolutionary_upgrade = 1;
    }
}

namespace SCIENCE {
    void __cdecl Show_Off_Stolen_Tech(uint16_t tech, int16_t player) {
        ++TEST_REPORT::science_calls;
        TEST_REPORT::science_kind = 1;
        TEST_REPORT::science_tech = tech;
        TEST_REPORT::science_source = player;
    }
    void __cdecl Show_Off_Artifact_Tech(uint16_t tech, game::id_planet planet) {
        ++TEST_REPORT::science_calls;
        TEST_REPORT::science_kind = 3;
        TEST_REPORT::science_tech = tech;
        TEST_REPORT::science_source = planet.id;
    }
}

namespace SHIPMOVE {
    e_fleet_evaluation Evaluate_Fleet_Move(game::s_game_state&, std::span<const game::id_ship> keys, game::s_star_destination destination,
        s_ship_move_info* result, std::optional<game::id_star>) {
        if (keys.size() != 1 || !game::current_state.ships.contains(keys.front()) ||
            game::Destination_Row(game::current_state, destination) < 0) return e_fleet_evaluation::invalid_input;
        ++TEST_REPORT::move_attempts;
        *result = {};
        result->moving = TEST_REPORT::relocation_allowed;
        return result->moving ? e_fleet_evaluation::can_move : e_fleet_evaluation::blocked;
    }
    bool Order_Fleet_Move(game::s_game_state&, std::span<const game::id_ship> keys, game::s_star_destination destination, const s_ship_move_info*) {
        if (keys.size() != 1) return false;
        const auto row = game::Ship_Row(game::current_state, keys.front());
        const auto star = game::Destination_Row(game::current_state, destination);
        if (row < 0 || star < 0) return false;
        ++TEST_REPORT::move_calls;
        TEST_REPORT::moved_ship = row;
        TEST_REPORT::moved_destination = star;
        return true;
    }
}

namespace NEXTTURN {
    void Do_Autosave() { ++TEST_REPORT::autosave_calls; }
}
