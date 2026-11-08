#include "pch.h"

#include <algorithm>
#include <vector>

#include "doctest/doctest.h"
#include "../fakes/fake_plntsum_runtime.h"

namespace {
    void Add_System(int16_t star_idx, int16_t first_planet, int rotation = 0) {
        MOX::star[star_idx].visited = 1 << MOX::PLAYER_NUM;
        for (int orbit = 0; orbit < 5; ++orbit) {
            int16_t planet_idx = first_planet + orbit;
            auto& planet = MOX::planet[planet_idx];
            planet.star_index = star_idx;
            planet.planet_type = PLANET_TYPE_PLANET;
            planet.mineral_class = (orbit + rotation) % 5;
            planet.gravity_class = PLANET_GRAVITY_NORMAL_G;
            planet.food_per_farmer = 2;
            MOX::star[star_idx].planet_index[orbit] = planet_idx;
        }
        MOX::planet_count = first_planet + 5;
    }

    void Set_Filters(int mask) {
        PLNTSUM::filter_out_enemy_controlled_planets = (mask & 1) != 0;
        PLNTSUM::filter_out_planets_out_of_range = (mask & 2) != 0;
        PLNTSUM::filter_out_mineral_scarcity_planets = (mask & 4) != 0;
    }

    void Check_List(std::vector<int16_t> expected) {
        PLNTSUM::Filter_Explored_Planets(&MOX::scroll_bar->total_rows);
        std::vector<int16_t> actual;
        for (int i = 0; i < MOX::scroll_bar->total_rows; ++i) {
            actual.push_back(PLNTSUM::planet_summary[i].explored);
        }
        std::sort(actual.begin(), actual.end());
        std::sort(expected.begin(), expected.end());
        DOCTEST_CHECK(actual == expected);
    }

    void Add_Ship(int16_t owner, int16_t location) {
        MOX::ship[0].owner = owner;
        MOX::ship[0].location = location;
        MOX::ship[0].status = 0;
        MOX::ship_stack_count = 1;
        MOX::ship_stack_start[0] = 0;
        MOX::ship_node[0].ship_idx = 0;
        MOX::ship_node[0].next_node = -1;
    }

    int16_t Add_Orderable_Ship(int16_t type, int16_t planet_idx = -1, bool moving = false) {
        int16_t ship_idx = MOX::NUM_SHIPS++;
        s_ship_data* ship = &MOX::ship[ship_idx];
        ship->owner = MOX::PLAYER_NUM;
        ship->d.ship_type = type;
        ship->location = moving ? SHIP_LOCATION_MOVING_OFFSET : 0;
        ship->x = moving ? 42 : 0;
        ship->y = moving ? 24 : 0;
        if (planet_idx >= 0) {
            PLNTSUM::Set_Auto_Colonize_Flag(ship, planet_idx);
        }
        MOX::ship_stack_start[MOX::ship_stack_count++] = ship_idx;
        MOX::ship_node[ship_idx].ship_idx = ship_idx;
        MOX::ship_node[ship_idx].next_node = -1;
        return ship_idx;
    }

    void Build_Planet_List() {
        PLNTSUM::Set_Planet_Explored_Flags();
        PLNTSUM::Filter_Explored_Planets(&MOX::scroll_bar->total_rows);
    }

    void Check_Field(int16_t field_id, int16_t x, int16_t y, int16_t x_end, int16_t y_end, char hotkey = '\0') {
        DOCTEST_REQUIRE(field_id >= 0);
        DOCTEST_REQUIRE(field_id < TEST_PLNTSUM::field_count);
        const auto* field = &TEST_PLNTSUM::fields[field_id];
        DOCTEST_CHECK(field->x == x);
        DOCTEST_CHECK(field->y == y);
        DOCTEST_CHECK(field->x_end == x_end);
        DOCTEST_CHECK(field->y_end == y_end);
        DOCTEST_CHECK(field->hotkey == hotkey);
    }
} // namespace

DOCTEST_TEST_SUITE_BEGIN("planet summary filters");

DOCTEST_TEST_CASE("mixed minerals remain planet-local across filter permutations") {
    // Exercise each toggle order in both directions without resetting the displayed list.
    const int orders[][3] = {{1, 2, 4}, {1, 4, 2}, {2, 1, 4}, {2, 4, 1}, {4, 1, 2}, {4, 2, 1}};
    for (int rotation = 0; rotation < 5; ++rotation) {
        for (int sort = 0; sort < 3; ++sort) {
            for (int knowledge = 0; knowledge < 4; ++knowledge) {
                for (bool reachable : {false, true}) {
                    for (int presence = 0; presence < 7; ++presence) {
                        TEST_PLNTSUM::Reset_();
                        Add_System(0, 0, rotation);
                        PLNTSUM::sort_choice = sort;
                        TEST_PLNTSUM::in_range[0] = reachable;
                        // Visited, racial omniscience, Galactic Lore only, or unknown.
                        MOX::star[0].visited = knowledge == 0 ? 1 : 0;
                        MOX::player[0].traits[TRAIT_OMNISCIENCE] = knowledge == 1;
                        MOX::player[0].has_galactic_lore = knowledge == 2;
                        // Empty, enemy colony, own colony, enemy outpost, own ship,
                        // enemy ship in orbit, or enemy ship en route.
                        if (presence >= 1 && presence <= 3) {
                            MOX::planet[0].colony_index = 0;
                            MOX::colony[0].planet = 0;
                            MOX::colony[0].owner = presence == 2 ? 0 : 1;
                            MOX::colony[0].outpost_flag = presence == 3;
                        } else if (presence >= 4) {
                            Add_Ship(presence == 4 ? 0 : 1,
                                presence == 6 ? SHIP_LOCATION_MOVING_OFFSET : 0);
                        }
                        PLNTSUM::Set_Planet_Explored_Flags();
                        for (const auto& order : orders) {
                            int mask = 0;
                            for (int step = 0; step < 7; ++step) {
                                if (step > 0) mask ^= order[(step - 1) % 3];
                                DOCTEST_CAPTURE(rotation);
                                DOCTEST_CAPTURE(sort);
                                DOCTEST_CAPTURE(knowledge);
                                DOCTEST_CAPTURE(reachable);
                                DOCTEST_CAPTURE(presence);
                                DOCTEST_CAPTURE(mask);
                                Set_Filters(mask);
                                std::vector<int16_t> expected;
                                bool enemy = presence == 1 || presence == 3 || presence == 5;
                                bool system_visible = knowledge != 3 &&
                                    (!(mask & 1) || (knowledge != 2 && !enemy)) &&
                                    (!(mask & 2) || reachable);
                                if (system_visible) {
                                    for (int16_t i = 0; i < 5; ++i) {
                                        if (i == 0 && (presence == 2 || presence == 3)) continue;
                                        int minerals = (i + rotation) % 5;
                                        if ((mask & 4) && (minerals == MINERAL_RESOURCE_POOR || minerals == MINERAL_RESOURCE_ULTRA_POOR)) continue;
                                        expected.push_back(i);
                                    }
                                }
                                Check_List(expected);
                            }
                        }
                    }
                }
            }
        }
    }
}

DOCTEST_TEST_CASE("destroyed occupants no longer hide mixed-mineral systems") {
    TEST_PLNTSUM::Reset_();
    Add_System(0, 0);
    MOX::planet[0].colony_index = 0;
    MOX::colony[0].owner = 1;
    MOX::colony[0].planet = 0;
    MOX::player[1].home_planet_id = -1;
    Add_Ship(1, 0);
    PLNTSUM::Set_Planet_Explored_Flags();
    Set_Filters(7);
    Check_List({});

    BOMB::Destroy_Colony(&MOX::colony[0], -1);
    DOCTEST_CHECK(MOX::planet[0].colony_index == -1);
    DOCTEST_CHECK(MOX::colony[0].owner == -1);
    DOCTEST_CHECK(MOX::star[0].visited == 1);
    Check_List({});
    // Model Kill_Ship's retirement, including a stack that has not yet been rebuilt.
    MOX::ship[0].status = 5;
    MOX::ship[0].location = -1;
    Check_List({2, 3, 4});
    MOX::ship_stack_count = 0;
    Check_List({2, 3, 4});
    // Reopening the screen rebuilds exploration flags after the destruction.
    PLNTSUM::Set_Planet_Explored_Flags();
    MOX::scroll_bar->total_rows = 0;
    Check_List({2, 3, 4});
}

DOCTEST_TEST_CASE("high planet indices and nonzero player visitation bits remain independent") {
    for (int16_t first : {int16_t{0}, int16_t{254}, int16_t{510}, int16_t{MAX_PLANETS - 5}}) {
        for (int16_t player : {int16_t{0}, int16_t{7}}) {
            TEST_PLNTSUM::Reset_();
            MOX::PLAYER_NUM = player;
            Add_System(MAX_STARS - 1, first);
            PLNTSUM::Set_Planet_Explored_Flags();
            Set_Filters(7);
            DOCTEST_CAPTURE(first);
            DOCTEST_CAPTURE(player);
            Check_List({int16_t(first + 2), int16_t(first + 3), int16_t(first + 4)});
        }
    }
}

DOCTEST_TEST_CASE("presence and range exclusions do not leak between systems") {
    TEST_PLNTSUM::Reset_();
    Add_System(0, 0);
    Add_System(1, 5);
    Add_System(2, 10);
    MOX::planet[0].colony_index = 0;
    MOX::colony[0].owner = 1;
    TEST_PLNTSUM::in_range[1] = false;
    PLNTSUM::Set_Planet_Explored_Flags();
    Set_Filters(7);
    Check_List({12, 13, 14});
    Set_Filters(6);
    Check_List({2, 3, 4, 12, 13, 14});
    Set_Filters(7);
    Check_List({12, 13, 14});
    Set_Filters(5);
    Check_List({7, 8, 9, 12, 13, 14});
    Set_Filters(7);
    Check_List({12, 13, 14});
    Set_Filters(3);
    Check_List({10, 11, 12, 13, 14});
    Set_Filters(7);
    Check_List({12, 13, 14});
}

DOCTEST_TEST_CASE("Galactic Lore alone reproduces the whole-system exclusion regardless of minerals") {
    TEST_PLNTSUM::Reset_();
    Add_System(0, 0);
    MOX::star[0].visited = 0;
    MOX::player[0].has_galactic_lore = 1;
    PLNTSUM::Set_Planet_Explored_Flags();
    Set_Filters(6);
    Check_List({2, 3, 4});
    Set_Filters(7);
    Check_List({});
    Set_Filters(3);
    Check_List({});

    MOX::star[0].visited = 1;
    PLNTSUM::Set_Planet_Explored_Flags();
    Set_Filters(7);
    Check_List({2, 3, 4});
}

DOCTEST_TEST_SUITE_END();

DOCTEST_TEST_SUITE_BEGIN("planet summary ship orders");

DOCTEST_TEST_CASE("ship discovery resets stale flags and distinguishes colony from outpost ships") {
    for (int mask = 0; mask < 4; ++mask) {
        TEST_PLNTSUM::Reset_();
        if (mask & 1) Add_Orderable_Ship(SHIP_TYPE_COLONY);
        if (mask & 2) Add_Orderable_Ship(SHIP_TYPE_OUTPOST);
        PLNTSUM::found_colony_ship = 1;
        PLNTSUM::found_outpost_ship = 1;
        PLNTSUM::Check_For_Colony_And_Outpost_Ships();
        DOCTEST_CAPTURE(mask);
        DOCTEST_CHECK(PLNTSUM::found_colony_ship == ((mask & 1) != 0));
        DOCTEST_CHECK(PLNTSUM::found_outpost_ship == ((mask & 2) != 0));
    }
}

DOCTEST_TEST_CASE("button status preserves only an explicit and still available send choice") {
    for (int mask = 0; mask < 4; ++mask) {
        for (int16_t choice = -1; choice < 2; ++choice) {
            TEST_PLNTSUM::Reset_();
            Add_System(0, 0);
            if (mask & 1) Add_Orderable_Ship(SHIP_TYPE_COLONY);
            if (mask & 2) Add_Orderable_Ship(SHIP_TYPE_OUTPOST);
            Build_Planet_List();
            PLNTSUM::send_choice = choice;
            PLNTSUM::Determine_Button_Status();
            DOCTEST_CAPTURE(mask);
            DOCTEST_CAPTURE(choice);
            bool selected = choice >= 0 && (mask & (1 << choice));
            DOCTEST_CHECK(PLNTSUM::send_choice == (selected ? choice : -1));
            DOCTEST_CHECK(PLNTSUM::draw_colony_button_status == ((mask & 1) ? (choice == 0 ? 2 : 1) : 0));
            DOCTEST_CHECK(PLNTSUM::draw_outpost_button_status == ((mask & 2) ? (choice == 1 ? 2 : 1) : 0));
        }
    }
}

DOCTEST_TEST_CASE("send controls switch and toggle without enabling an unavailable ship type") {
    for (int mask = 1; mask < 4; ++mask) {
        TEST_PLNTSUM::Reset_();
        Add_System(0, 0);
        if (mask & 1) Add_Orderable_Ship(SHIP_TYPE_COLONY);
        if (mask & 2) Add_Orderable_Ship(SHIP_TYPE_OUTPOST);
        Build_Planet_List();
        PLNTSUM::Determine_Button_Status();
        PLNTSUM::Add_Plntsum_Fields();
        for (int16_t choice : {int16_t{0}, int16_t{1}, int16_t{0}}) {
            if (!(mask & (1 << choice))) continue;
            int16_t input_id = PLNTSUM::screen->buttons[8 + choice];
            if (PLNTSUM::send_choice == choice) {
                PLNTSUM::Handle_Send_Button(input_id);
            }
            PLNTSUM::Handle_Send_Button(input_id);
            DOCTEST_CHECK(PLNTSUM::send_choice == choice);
            DOCTEST_CHECK(PLNTSUM::draw_colony_button_status == ((mask & 1) ? (choice == 0 ? 2 : 1) : 0));
            DOCTEST_CHECK(PLNTSUM::draw_outpost_button_status == ((mask & 2) ? (choice == 1 ? 2 : 1) : 0));
            PLNTSUM::Handle_Send_Button(input_id);
            DOCTEST_CHECK(PLNTSUM::send_choice == -1);
            DOCTEST_CHECK(PLNTSUM::draw_colony_button_status == ((mask & 1) != 0));
            DOCTEST_CHECK(PLNTSUM::draw_outpost_button_status == ((mask & 2) != 0));
            PLNTSUM::Handle_Send_Button(input_id);
        }
        if (mask != 3) {
            int16_t selected = PLNTSUM::send_choice;
            PLNTSUM::Handle_Send_Button(PLNTSUM::screen->buttons[mask == 1 ? 9 : 8]);
            DOCTEST_CHECK(PLNTSUM::send_choice == selected);
        }
    }
}

DOCTEST_TEST_CASE("sending the last selected ship disarms rather than switching to the other type") {
    for (int16_t choice = 0; choice < 2; ++choice) {
        TEST_PLNTSUM::Reset_();
        Add_System(0, 0);
        Add_Orderable_Ship(SHIP_TYPE_COLONY);
        Add_Orderable_Ship(SHIP_TYPE_OUTPOST);
        Build_Planet_List();
        PLNTSUM::send_choice = choice;
        PLNTSUM::Determine_Button_Status();
        MOX::ship[choice].location = SHIP_LOCATION_MOVING_OFFSET;
        PLNTSUM::Set_Auto_Colonize_Flag(&MOX::ship[choice], 0);
        PLNTSUM::Determine_Button_Status();
        DOCTEST_CHECK(PLNTSUM::send_choice == -1);
        DOCTEST_CHECK(PLNTSUM::draw_colony_button_status == (choice == 0 ? 0 : 1));
        DOCTEST_CHECK(PLNTSUM::draw_outpost_button_status == (choice == 1 ? 0 : 1));
    }
}

DOCTEST_TEST_CASE("no target rows or hyperspace flux disables and disarms both send modes") {
    for (bool flux : {false, true}) {
        TEST_PLNTSUM::Reset_();
        Add_System(0, 0);
        Add_Orderable_Ship(SHIP_TYPE_COLONY);
        Add_Orderable_Ship(SHIP_TYPE_OUTPOST);
        Build_Planet_List();
        PLNTSUM::send_choice = 0;
        TEST_PLNTSUM::hyperspace_flux = flux;
        if (!flux) MOX::scroll_bar->total_rows = 0;
        PLNTSUM::Determine_Button_Status();
        DOCTEST_CHECK(PLNTSUM::send_choice == -1);
        DOCTEST_CHECK(PLNTSUM::draw_colony_button_status == 0);
        DOCTEST_CHECK(PLNTSUM::draw_outpost_button_status == 0);
    }
}

DOCTEST_TEST_CASE("send hitboxes cover the drawn buttons and retain their keyboard shortcuts") {
    TEST_PLNTSUM::Reset_();
    Add_Orderable_Ship(SHIP_TYPE_COLONY);
    Add_Orderable_Ship(SHIP_TYPE_OUTPOST);
    PLNTSUM::Check_For_Colony_And_Outpost_Ships();
    PLNTSUM::Add_Plntsum_Fields();
    Check_Field(PLNTSUM::screen->buttons[8], 454, 386, 609, 408, 'C');
    Check_Field(PLNTSUM::screen->buttons[9], 454, 413, 610, 437, 'O');
}

DOCTEST_TEST_CASE("a hyperspace flux warning clears the previous selection when switching modes") {
    for (int16_t choice = 0; choice < 2; ++choice) {
        TEST_PLNTSUM::Reset_();
        Add_System(0, 0);
        Add_Orderable_Ship(SHIP_TYPE_COLONY);
        Add_Orderable_Ship(SHIP_TYPE_OUTPOST);
        Build_Planet_List();
        PLNTSUM::Determine_Button_Status();
        PLNTSUM::Add_Plntsum_Fields();
        PLNTSUM::Handle_Send_Button(PLNTSUM::screen->buttons[8 + choice]);
        TEST_PLNTSUM::hyperspace_flux = 1;
        PLNTSUM::Handle_Send_Button(PLNTSUM::screen->buttons[9 - choice]);
        DOCTEST_CHECK(TEST_PLNTSUM::warning_count == 1);
        DOCTEST_CHECK(PLNTSUM::send_choice == -1);
        DOCTEST_CHECK(PLNTSUM::draw_colony_button_status == 1);
        DOCTEST_CHECK(PLNTSUM::draw_outpost_button_status == 1);
    }
}

DOCTEST_TEST_CASE("ship icon hitboxes match their draw coordinates on every visible row") {
    TEST_PLNTSUM::Reset_();
    auto* icon = PLNTSUM::screen->pics[13];
    for (int16_t row = 0; row < 8; ++row) {
        int16_t field_id;
        PLNTSUM::Draw_Colony_ETA(row, row);
        PLNTSUM::Add_Colony_Ship_Icon_Field(&field_id, row);
        Check_Field(field_id, TEST_PLNTSUM::draw_x, TEST_PLNTSUM::draw_y, TEST_PLNTSUM::draw_x + icon->width - 1, TEST_PLNTSUM::draw_y + icon->height - 1);
        PLNTSUM::Draw_Outpost_ETA(row, row);
        PLNTSUM::Add_Outpost_Ship_Icon_Field(&field_id, row);
        Check_Field(field_id, TEST_PLNTSUM::draw_x, TEST_PLNTSUM::draw_y, TEST_PLNTSUM::draw_x + icon->width - 1, TEST_PLNTSUM::draw_y + icon->height - 1);
    }
}

DOCTEST_TEST_CASE("recall targets the dispatched ship with or without an idle alternative") {
    for (int16_t type : {SHIP_TYPE_COLONY, SHIP_TYPE_OUTPOST}) {
        for (bool idle : {false, true}) {
            for (bool moving : {false, true}) {
                TEST_PLNTSUM::Reset_();
                Add_System(0, 0);
                int16_t sent = Add_Orderable_Ship(type, 0, moving);
                int16_t free_ship = idle ? Add_Orderable_Ship(type) : -1;
                Build_Planet_List();
                PLNTSUM::Determine_Button_Status();
                PLNTSUM::Add_Plntsum_Fields();
                int16_t row = PLNTSUM::Planet_Summary_List_Id(0);
                // These are deliberately not the dispatched ship.
                PLNTSUM::planet_summary[row].best_colony_ship_idx = free_ship;
                PLNTSUM::planet_summary[row].best_outpost_ship_idx = free_ship;
                PLNTSUM::send_choice = idle ? (type == SHIP_TYPE_COLONY ? 0 : 1) : -1;
                PLNTSUM::Undo_Send_Order(0, type);
                DOCTEST_CAPTURE(type);
                DOCTEST_CAPTURE(idle);
                DOCTEST_CAPTURE(moving);
                DOCTEST_CHECK(TEST_PLNTSUM::confirmation_count == 1);
                DOCTEST_CHECK(TEST_PLNTSUM::moved_ship == sent);
                DOCTEST_CHECK(TEST_PLNTSUM::moved_to_star == 0);
                DOCTEST_CHECK(PLNTSUM::Colony_Ship_Auto_Colonization(&MOX::ship[sent]) == PLANET_INDEX_NONE);
                DOCTEST_CHECK(PLNTSUM::planet_summary[row].colony_eta == -1);
                DOCTEST_CHECK(PLNTSUM::planet_summary[row].outpost_eta == -1);
                DOCTEST_CHECK(PLNTSUM::screen->refresh_fields);
                DOCTEST_CHECK(PLNTSUM::Ship_Is_Not_Colonizing(&MOX::ship[sent]) == !moving);
                PLNTSUM::Determine_Button_Status();
                DOCTEST_CHECK(PLNTSUM::send_choice == (idle ? (type == SHIP_TYPE_COLONY ? 0 : 1) : -1));
            }
        }
    }
}

DOCTEST_TEST_CASE("recall matches the displayed ship and ignores foreign retired or invalid ships") {
    TEST_PLNTSUM::Reset_();
    Add_System(0, 0);
    Add_Orderable_Ship(SHIP_TYPE_COLONY, 0, true);
    int16_t displayed = Add_Orderable_Ship(SHIP_TYPE_COLONY, 0, true);
    MOX::ship[Add_Orderable_Ship(SHIP_TYPE_COLONY, 0, true)].owner = 1;
    MOX::ship[Add_Orderable_Ship(SHIP_TYPE_COLONY, 0, true)].status = 5;
    MOX::ship[Add_Orderable_Ship(SHIP_TYPE_COLONY, 0, true)].location = -1;
    Add_Orderable_Ship(SHIP_TYPE_OUTPOST, 0, true);
    Add_Orderable_Ship(SHIP_TYPE_COLONY, 1, true);
    Build_Planet_List();
    PLNTSUM::Determine_Button_Status();
    PLNTSUM::Undo_Send_Order(0, SHIP_TYPE_COLONY);
    DOCTEST_CHECK(TEST_PLNTSUM::moved_ship == displayed);
    DOCTEST_CHECK(PLNTSUM::Colony_Ship_Auto_Colonization(&MOX::ship[0]) == 0);
}

DOCTEST_TEST_CASE("declined unavailable or missing recall orders do not move a ship") {
    for (int scenario = 0; scenario < 3; ++scenario) {
        TEST_PLNTSUM::Reset_();
        Add_System(0, 0);
        int16_t sent = Add_Orderable_Ship(SHIP_TYPE_COLONY, scenario == 2 ? 1 : 0, true);
        Build_Planet_List();
        TEST_PLNTSUM::confirm_recall = false;
        TEST_PLNTSUM::can_select_ship = scenario != 1;
        PLNTSUM::Undo_Send_Order(0, SHIP_TYPE_COLONY);
        DOCTEST_CHECK(TEST_PLNTSUM::moved_ship == -1);
        DOCTEST_CHECK(TEST_PLNTSUM::confirmation_count == (scenario == 0 ? 1 : 0));
        DOCTEST_CHECK(TEST_PLNTSUM::warning_count == (scenario == 1 ? 1 : 0));
        DOCTEST_CHECK(PLNTSUM::Colony_Ship_Auto_Colonization(&MOX::ship[sent]) == (scenario == 2 ? 1 : 0));
    }
}

DOCTEST_TEST_CASE("in flight ship icons survive entry filtering scrolling and sorting without free ships") {
    TEST_PLNTSUM::Reset_();
    Add_System(0, 0);
    Add_System(1, 5);
    Add_Orderable_Ship(SHIP_TYPE_COLONY, 0, true);
    Add_Orderable_Ship(SHIP_TYPE_OUTPOST, 9, true);
    Build_Planet_List();
    PLNTSUM::Determine_Button_Status();
    PLNTSUM::Add_Plntsum_Fields();
    DOCTEST_CHECK_FALSE(PLNTSUM::Send_Colony_Button_Selectable());
    DOCTEST_CHECK_FALSE(PLNTSUM::Send_Outpost_Button_Selectable());
    for (int mask : {0, 4, 0}) {
        Set_Filters(mask);
        Build_Planet_List();
        PLNTSUM::Determine_Button_Status();
        PLNTSUM::screen->refresh_fields = true;
        PLNTSUM::Refresh_Plntsum_Fields();
        for (int sort = 0; sort < 3; ++sort) {
            if (PLNTSUM::sort_choice != sort) {
                PLNTSUM::sort_choice = sort;
                PLNTSUM::Sort_Planet_Summary();
            }
            for (int row = 0; row < MOX::scroll_bar->total_rows; ++row) {
                MOX::scroll_bar->first_visible_row = row;
                PLNTSUM::Refresh_Plntsum_Fields();
                int16_t planet = PLNTSUM::planet_summary[row].explored;
                DOCTEST_CAPTURE(mask);
                DOCTEST_CAPTURE(sort);
                DOCTEST_CAPTURE(row);
                DOCTEST_CAPTURE(planet);
                DOCTEST_CAPTURE(PLNTSUM::planet_summary[row].colony_eta);
                DOCTEST_CHECK((PLNTSUM::screen->colony_icons[0] >= 0) == (planet == 0));
                DOCTEST_CHECK((PLNTSUM::screen->output_icons[0] >= 0) == (planet == 9));
                for (int i = MOX::scroll_bar->total_rows - row; i < 8; ++i) {
                    DOCTEST_CHECK(PLNTSUM::screen->colony_icons[i] == -1000);
                    DOCTEST_CHECK(PLNTSUM::screen->output_icons[i] == -1000);
                }
            }
        }
        MOX::scroll_bar->first_visible_row = 0;
    }
}

DOCTEST_TEST_CASE("field refresh tracks list changes and waits for scroll bar drag release") {
    TEST_PLNTSUM::Reset_();
    Add_System(0, 0);
    Add_System(1, 5);
    Build_Planet_List();
    PLNTSUM::Add_Plntsum_Fields();
    PLNTSUM::Refresh_Plntsum_Fields();
    DOCTEST_CHECK(TEST_PLNTSUM::clear_count == 0);
    TEST_PLNTSUM::mouse_button = 1;
    MOX::scroll_bar->first_visible_row = 2;
    PLNTSUM::Refresh_Plntsum_Fields();
    DOCTEST_CHECK(TEST_PLNTSUM::clear_count == 0);
    DOCTEST_CHECK(PLNTSUM::screen->refresh_fields);
    TEST_PLNTSUM::mouse_button = 0;
    PLNTSUM::Refresh_Plntsum_Fields();
    DOCTEST_CHECK(TEST_PLNTSUM::clear_count == 1);
    DOCTEST_CHECK_FALSE(PLNTSUM::screen->refresh_fields);
    PLNTSUM::sort_choice = 1;
    PLNTSUM::Refresh_Plntsum_Fields();
    DOCTEST_CHECK(TEST_PLNTSUM::clear_count == 2);
    --MOX::scroll_bar->total_rows;
    PLNTSUM::Refresh_Plntsum_Fields();
    DOCTEST_CHECK(TEST_PLNTSUM::clear_count == 3);
    PLNTSUM::screen->refresh_fields = true;
    PLNTSUM::Refresh_Plntsum_Fields();
    DOCTEST_CHECK(TEST_PLNTSUM::clear_count == 4);
    PLNTSUM::Refresh_Plntsum_Fields();
    DOCTEST_CHECK(TEST_PLNTSUM::clear_count == 4);
}

DOCTEST_TEST_SUITE_END();