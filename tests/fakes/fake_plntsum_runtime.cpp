#include "pch.h"
#include "game/fleet_movement.h"
#include "game/legacy_ship_location_codec.h"
#include "../fixtures/star_roster.h"
#include "../fixtures/colony_roster.h"
#include "../fixtures/planet_roster.h"

#include "doctest/doctest.h"
#include "fake_plntsum_runtime.h"

namespace {
    auto& players = game::current_state.players;

    test_stars::Slots<MAX_STARS + 1> stars;
    test_colonies::Slots<2> colonies;
    auto& ships = []() -> auto& {
        auto& rows = game::current_state.ship_records;
        rows.resize(8);
        return rows;
    }();
    s_scroll_bar scroll;
    s_plntsum_screen screen;
    s_animation_header icon;
    s_plntsum_ship ship_summary[MAX_SHIPS];
    s_plntsum_planet summary[MAX_PLANETS];
    s_plntsum_planet temporary[MAX_PLANETS];
    bool allocation_pushed;
} // namespace

namespace MOX {


    s_scroll_bar* scroll_bar = &scroll;
    s_animation_header* screen_seg = nullptr;
    game::id_player local_player{0};

    int16_t scanned_field;
    int16_t ship_stack_count;

    int8_t global_var;
    s_ship_move_info g_ship_move_info;
    size_t h_string_ix[397];
    std::vector<char> h_strings = {'t', 'e', 's', 't', '\0'};
    int16_t ship_stack_start[MAX_SHIPS];
    s_ship_node ship_node[MAX_SHIPS];
} // namespace MOX

namespace TEST_PLNTSUM {
    bool in_range[MAX_STARS];
    s_field fields[512];
    int16_t field_count;
    int clear_count;
    int16_t mouse_button;
    int16_t hyperspace_flux;
    bool confirm_recall;
    bool can_select_ship;
    int confirmation_count;
    int warning_count;
    int16_t moved_ship;
    int16_t moved_to_star;
    int16_t draw_x;
    int16_t draw_y;
    void (*on_confirmation)() = nullptr;

    void Reset_() {
        players = ::player_roster::Make_Roster();
        test_planets::Reset(0);
        stars.Reset();
        colonies.Reset();
        ships.assign(8, s_ship_data{});
        memset(&scroll, 0, sizeof(scroll));
        memset(&screen, 0, sizeof(screen));
        memset(ship_summary, 0xFF, sizeof(ship_summary));
        icon.width = 12;
        icon.height = 10;
        screen.pics[13] = &icon;
        field_count = 0;
        clear_count = 0;
        mouse_button = 0;
        hyperspace_flux = 0;
        confirm_recall = true;
        can_select_ship = true;
        confirmation_count = 0;
        on_confirmation = nullptr;
        warning_count = 0;
        moved_ship = -1;
        moved_to_star = -1;
        memset(summary, 0xFF, sizeof(summary));
        memset(MOX::ship_stack_start, 0xFF, sizeof(MOX::ship_stack_start));
        memset(MOX::ship_node, 0xFF, sizeof(MOX::ship_node));
        for (auto& [id, planet] : game::current_state.planets) {
            planet.colony_id = game::NO_COLONY;
            planet.planet_type = PLANET_TYPE_ASTEROID;
        }
        for (bool& reachable : in_range) {
            reachable = true;
        }
        MOX::local_player = game::id_player{0};
        test_planets::Resize_Dense(0);
        MOX::ship_stack_count = 0;
        game::current_state.ship_count = 0;
        game::current_state.next_ship_id = 0;
        test_stars::Resize(0);
        MOX::scanned_field = -1;
        PLNTSUM::planet_summary = summary;
        PLNTSUM::ship_summary = ship_summary;
        PLNTSUM::screen = &screen;
        PLNTSUM::send_choice = -1;
        PLNTSUM::found_colony_ship = 0;
        PLNTSUM::found_outpost_ship = 0;
        PLNTSUM::draw_colony_button_status = 0;
        PLNTSUM::draw_outpost_button_status = 0;
        PLNTSUM::sort_choice = 0;
        PLNTSUM::filter_out_enemy_controlled_planets = 0;
        PLNTSUM::filter_out_planets_out_of_range = 0;
        PLNTSUM::filter_out_mineral_scarcity_planets = 0;
        PLNTSUM::filter_out_adverse_gravity_planets = 0;
        PLNTSUM::filter_out_hostile_environment_planets = 0;
        PLNTSUM::Reset_Explored_Planets();
        DOCTEST_REQUIRE_FALSE(allocation_pushed);
    }
} // namespace TEST_PLNTSUM

namespace allocate {
    void __cdecl Push_Block(void*) {
        DOCTEST_REQUIRE_FALSE(allocation_pushed);
        allocation_pushed = true;
    }

    void* __cdecl Allocate_Next_Block(void*, uint32_t size) {
        DOCTEST_REQUIRE(allocation_pushed);
        DOCTEST_REQUIRE(size <= sizeof(temporary));
        return temporary;
    }

    void __cdecl Pop_Block(void*) {
        DOCTEST_REQUIRE(allocation_pushed);
        allocation_pushed = false;
    }
} // namespace allocate

namespace RUSS {
    void __cdecl Mox_Update() {}
} // namespace RUSS

namespace OFFICER {
    void __cdecl Move_Officer_To_Pool(int16_t) {
        DOCTEST_FAIL("Unexpected officer in colony destruction fixture");
    }

    void __cdecl Kill_Officer(int16_t) {
        DOCTEST_FAIL("Unexpected officer in colony destruction fixture");
    }

    bool __cdecl One_Leader_With_Galactic_Lore(game::id_player player_key) {
        int player_idx = legacy_world::Encode_Player_Slot(player_key);
        return game::current_state.players[player_idx].has_galactic_lore != 0;
    }
} // namespace OFFICER

namespace SHIPMOVE {
    bool Star_In_Extended_Range_Of_Player(const game::s_game_state&, const s_star_data& star, game::id_player) {
        return TEST_PLNTSUM::in_range[star.id.id];
    }

    e_fleet_evaluation Evaluate_Fleet_Move(std::span<const game::id_ship> keys, game::s_star_destination destination,
        s_ship_move_info* move_info, std::optional<game::id_star>) {
        auto& state = game::current_state;
        if (!move_info) return e_fleet_evaluation::invalid_input;
        *move_info = {};
        if (keys.size() != 1 || !game::Find_Ship(state, keys.front()) || game::Destination_Row(state, destination) < 0) {
            return e_fleet_evaluation::invalid_input;
        }
        move_info->moving = 1;
        move_info->turns_left = 2;
        return e_fleet_evaluation::can_move;
    }

    bool Order_Fleet_Move(std::span<const game::id_ship> keys, game::s_star_destination destination, const s_ship_move_info* info) {
        auto& state = game::current_state;
        auto* ship = keys.size() == 1 ? game::Find_Ship(state, keys.front()) : nullptr;
        const auto star = game::Destination_Row(state, destination);
        if (!ship || star < 0 || !info) return false;
        TEST_PLNTSUM::moved_ship = game::Ship_Row(state, keys.front());
        TEST_PLNTSUM::moved_to_star = star;
        const auto kind = ship->position.kind == game::e_ship_location::at_star ? game::e_ship_location::at_star : game::e_ship_location::in_flight;
        ship->position.Set(legacy_world::Location_From_Slot(kind, star, std::ssize(state.stars)));
        return true;
    }
} // namespace SHIPMOVE

namespace HACCESS {
    int16_t __cdecl Ship_Owner(int16_t ship_idx) {
        return factions::Legacy_Code(game::current_state.ship_records[ship_idx].owner);
    }

    bool Player_Can_Order_Ship(game::id_player player, const s_ship_data* ship) {
        return TEST_PLNTSUM::can_select_ship && factions::Player(ship->owner) == player && ship->status < 3;
    }

    int16_t __cdecl Ship_Icon_Id_From_Node_Ptr(int16_t node_idx) {
        return MOX::ship_node[node_idx].ship_idx;
    }

    game::id_star Nearest_Friendly_Star(game::id_ship, game::id_player) {
        return game::Star_Key_At_Row(game::current_state, 0);
    }

    char* Get_Planet_Name_No_Prime(const game::s_game_state&, game::id_planet) {
        static char name[] = "Test planet";
        return name;
    }
} // namespace HACCESS

namespace COLCALC {
    void __cdecl Compute_Star_Colony_Stuff(s_star_data*) {}

    int16_t __cdecl Planet_Max_Population_For_Player(s_planet_data*, game::id_player) {
        return 10;
    }
} // namespace COLCALC

namespace EVENTS {
    void __cdecl Kill_All_Events_At_Colony(game::id_colony) {}

    int16_t __cdecl Event_Check_Hyperspace_Flux() {
        return TEST_PLNTSUM::hyperspace_flux;
    }
} // namespace EVENTS

namespace COLBLDG {
    void __cdecl Clear_Colony_Queue(game::s_game_state&, game::id_colony) {}
} // namespace COLBLDG

namespace MSG_ {
    void __cdecl Add_Msg(s_MSG, int16_t) {}
} // namespace MSG_

namespace invasion {
    void __cdecl Assign_New_Home_Planet(const game::s_game_state&, int8_t, game::id_colony) {
        DOCTEST_FAIL("Unexpected homeworld in colony destruction fixture");
    }
} // namespace invasion

namespace animate {
    int16_t __cdecl Get_Width(s_animation_header* pic) {
        return pic->width;
    }

    int16_t __cdecl Get_Height(s_animation_header* pic) {
        return pic->height;
    }

    void __cdecl Draw(int16_t x, int16_t y, s_animation_header*) {
        TEST_PLNTSUM::draw_x = x;
        TEST_PLNTSUM::draw_y = y;
    }
} // namespace animate

namespace fields {
    int16_t __cdecl Add_Hidden_Field(int16_t x, int16_t y, int16_t x_end, int16_t y_end, const char* hotkey, int8_t) {
        using namespace TEST_PLNTSUM;
        DOCTEST_REQUIRE(field_count < 512);
        TEST_PLNTSUM::fields[field_count] = {x, y, x_end, y_end, hotkey[0]};
        return field_count++;
    }

    int16_t __cdecl Add_Multi_Button_Field(int16_t x, int16_t y, const char*, s_animation_header*, int16_t*, int16_t, const char* hotkey, int8_t sound) {
        return Add_Hidden_Field(x, y, x, y, hotkey, sound);
    }

    int16_t __cdecl Add_Radio_Button_Field(int16_t x, int16_t y, const char*, s_animation_header*, int16_t*, const char* hotkey, int8_t sound) {
        return Add_Hidden_Field(x, y, x, y, hotkey, sound);
    }

    int16_t __cdecl Add_Button_Field(int16_t x, int16_t y, const char*, s_animation_header*, const char* hotkey, int8_t sound) {
        return Add_Hidden_Field(x, y, x, y, hotkey, sound);
    }

    int16_t __cdecl Add_Scroll_Field(int16_t x, int16_t y, int16_t, int16_t, int16_t, int16_t, int16_t width, int16_t height, int16_t*, const char* hotkey, int8_t sound) {
        return Add_Hidden_Field(x, y, x + width - 1, y + height - 1, hotkey, sound);
    }

    void __cdecl Clear_Fields() {
        ++TEST_PLNTSUM::clear_count;
        TEST_PLNTSUM::field_count = 0;
    }
} // namespace fields

namespace mouse {
    int16_t __cdecl Mouse_Button() {
        return TEST_PLNTSUM::mouse_button;
    }
} // namespace mouse

namespace MOVEBOX {
    void __cdecl Get_Galaxy_Map_Star_XY(int16_t, int16_t, int16_t, int16_t, int16_t, int16_t, int16_t, int16_t, int16_t* x, int16_t* y) {
        *x = 450;
        *y = 20;
    }
} // namespace MOVEBOX

namespace GENDRAW {
    int16_t __cdecl Do_Confirmation_Box(const char*) {
        ++TEST_PLNTSUM::confirmation_count;
        if (TEST_PLNTSUM::on_confirmation) TEST_PLNTSUM::on_confirmation();
        return TEST_PLNTSUM::confirm_recall;
    }

    void __cdecl Warning_Box(const char*) {
        ++TEST_PLNTSUM::warning_count;
    }
} // namespace GENDRAW

namespace SHIPS {
    void __cdecl Clear_Auto_Colonization_Flag(s_ship_data* ship) {
        ship->colonization_target = game::NO_PLANET;
    }
} // namespace SHIPS
