#include "pch.h"

#include "doctest/doctest.h"
#include "fake_plntsum_runtime.h"

namespace {
    s_player players[MAX_PLAYERS];
    s_planet_data planets[MAX_PLANETS];
    s_star_data stars[MAX_STARS];
    s_colony colonies[2];
    s_ship_data ships[8];
    s_scroll_bar scroll;
    s_plntsum_screen screen;
    s_animation_header icon;
    s_plntsum_ship ship_summary[MAX_SHIPS];
    s_plntsum_planet summary[MAX_PLANETS];
    s_plntsum_planet temporary[MAX_PLANETS];
    bool allocation_pushed;
} // namespace

namespace MOX {
    s_player* player = players;
    s_planet_data* planet = planets;
    s_star_data* star = stars;
    s_colony* colony = colonies;
    s_ship_data* ship = ships;
    s_leader_data* leaders = nullptr;
    s_scroll_bar* scroll_bar = &scroll;
    s_animation_header* screen_seg = nullptr;
    int16_t PLAYER_NUM;
    int16_t planet_count;
    int16_t scanned_field;
    int16_t ship_stack_count;
    int16_t NUM_SHIPS;
    int16_t NUM_STARS;
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

    void Reset_() {
        memset(players, 0, sizeof(players));
        memset(planets, 0, sizeof(planets));
        memset(stars, 0, sizeof(stars));
        memset(colonies, 0, sizeof(colonies));
        memset(ships, 0, sizeof(ships));
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
        warning_count = 0;
        moved_ship = -1;
        moved_to_star = -1;
        memset(summary, 0xFF, sizeof(summary));
        memset(MOX::ship_stack_start, 0xFF, sizeof(MOX::ship_stack_start));
        memset(MOX::ship_node, 0xFF, sizeof(MOX::ship_node));
        for (auto& planet : planets) {
            planet.colony_index = -1;
            planet.planet_type = PLANET_TYPE_ASTEROID;
        }
        for (auto& star : stars) {
            memset(star.officer_index, 0xFF, sizeof(star.officer_index));
            for (int orbit = 0; orbit < 5; ++orbit) {
                star.planet_index[orbit] = -1;
            }
        }
        for (bool& reachable : in_range) {
            reachable = true;
        }
        MOX::PLAYER_NUM = 0;
        MOX::planet_count = 0;
        MOX::ship_stack_count = 0;
        MOX::NUM_SHIPS = 0;
        MOX::NUM_STARS = 0;
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

    bool __cdecl One_Leader_With_Galactic_Lore(int16_t player_idx) {
        return MOX::player[player_idx].has_galactic_lore != 0;
    }
} // namespace OFFICER

namespace SHIPMOVE {
    bool __cdecl Star_In_Extended_Range_Of_Player(s_star_data* star, int16_t) {
        return TEST_PLNTSUM::in_range[star - MOX::star];
    }

    void __cdecl Ships_Try_To_Move_To(int16_t* ship_list, int16_t, s_ship_move_info* move_info) {
        DOCTEST_REQUIRE(ship_list[0] >= 0);
        DOCTEST_REQUIRE(ship_list[0] < MOX::NUM_SHIPS);
        DOCTEST_REQUIRE(ship_list[1] == -1);
        memset(move_info, 0, sizeof(*move_info));
        move_info->moving = 1;
        move_info->turns_left = 2;
    }

    void __cdecl Make_Ships_Move_To(int16_t* ship_list, int16_t star_idx, s_ship_move_info*) {
        TEST_PLNTSUM::moved_ship = ship_list[0];
        TEST_PLNTSUM::moved_to_star = star_idx;
        s_ship_data* ship = &MOX::ship[ship_list[0]];
        ship->location = ship->location >= SHIP_LOCATION_MOVING_OFFSET ? SHIP_LOCATION_MOVING_OFFSET + star_idx : star_idx;
    }
} // namespace SHIPMOVE

namespace HACCESS {
    int16_t __cdecl Ship_Owner(int16_t ship_idx) {
        return MOX::ship[ship_idx].owner;
    }

    bool __cdecl Player_Can_Order_Ship(int16_t player_idx, s_ship_data* ship) {
        return ship->owner == player_idx && ship->status < 3;
    }

    int16_t __cdecl Ship_Icon_Id_From_Node_Ptr(int16_t node_idx) {
        return MOX::ship_node[node_idx].ship_idx;
    }

    int16_t __cdecl Nearest_Star_To_XY(int16_t*) {
        return 0;
    }

    char* __cdecl Get_Planet_Name_No_Prime(int16_t) {
        static char name[] = "Test planet";
        return name;
    }
} // namespace HACCESS

namespace COLCALC {
    void __cdecl Compute_Star_Colony_Stuff(s_star_data*) {}

    uint8_t minerals_per_mine[8] = {1, 2, 3, 5, 8};

    int16_t __cdecl Planet_Max_Population_For_Player(s_planet_data*, int16_t) {
        return 10;
    }
} // namespace COLCALC

namespace EVENTS {
    void __cdecl Kill_All_Events_At_Colony(int16_t) {}

    int16_t __cdecl Event_Check_Hyperspace_Flux() {
        return TEST_PLNTSUM::hyperspace_flux;
    }
} // namespace EVENTS

namespace COLBLDG {
    void __cdecl Clear_Colony_Queue(int16_t) {}
} // namespace COLBLDG

namespace MSG_ {
    void __cdecl Add_Msg(s_MSG, int16_t) {}
} // namespace MSG_

namespace invasion {
    void __cdecl Assign_New_Home_Planet(int8_t, int16_t) {
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
        return TEST_PLNTSUM::confirm_recall;
    }

    void __cdecl Warning_Box(const char*) {
        ++TEST_PLNTSUM::warning_count;
    }
} // namespace GENDRAW

namespace MAINSCR {
    bool __cdecl Ship_Can_Be_Selected(int16_t ship_idx) {
        DOCTEST_REQUIRE(ship_idx >= 0);
        DOCTEST_REQUIRE(ship_idx < MOX::NUM_SHIPS);
        return TEST_PLNTSUM::can_select_ship;
    }
} // namespace MAINSCR

namespace SHIPS {
    void __cdecl Clear_Auto_Colonization_Flag(s_ship_data* ship) {
        ship->d.ship_weapon[1].type = 0;
        ship->d.ship_weapon[0].type = -1;
    }
} // namespace SHIPS