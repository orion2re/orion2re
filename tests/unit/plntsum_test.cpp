#include "pch.h"
#include "../fixtures/ship_rows.h"
#include "game/legacy_ship_location_codec.h"
#include "../fixtures/star_roster.h"
#include "../fixtures/colony_roster.h"
#include "../fixtures/planet_roster.h"

#include <algorithm>
#include <vector>

#include "doctest/doctest.h"
#include "../fakes/fake_plntsum_runtime.h"

namespace {
    void Add_System(int16_t star_idx, int16_t first_planet, int rotation = 0) {
        test_stars::Resize(std::max<int16_t>(std::ssize(game::current_state.stars), star_idx + 1));
        auto& star = test_stars::Record(star_idx);
        star.visited = game::player_bitmap::Only(MOX::local_player);
        std::ranges::fill(star.officer_id, game::NO_LEADER);
        for (int orbit = 0; orbit < 5; ++orbit) {
            int16_t planet_idx = first_planet + orbit;
            auto& planet = test_planets::Record(planet_idx);
            planet.colony_id = game::NO_COLONY;
            planet.star_id = game::id_star{static_cast<uint32_t>(star_idx)};
            planet.orbit = static_cast<int8_t>(orbit);
            planet.planet_type = PLANET_TYPE_PLANET;
            planet.mineral_class = (orbit + rotation) % 5;
            planet.gravity_class = PLANET_GRAVITY_NORMAL_G;
            planet.food_per_farmer = 2;
            test_stars::Record(star_idx).orbit_planet_ids[orbit] = game::id_planet{static_cast<uint32_t>(planet_idx)};
        }
        test_planets::Resize_Dense(first_planet + 5);
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
            actual.push_back(PLNTSUM::planet_summary[i].planet_id.get().id);
        }
        std::sort(actual.begin(), actual.end());
        std::sort(expected.begin(), expected.end());
        DOCTEST_CHECK(actual == expected);
    }

    void Add_Ship(int16_t owner, int16_t location) {
        ship_slots::Resize(game::current_state, std::max<int16_t>(1, static_cast<int16_t>(game::current_state.ship_slots.size())));
        test_ships::Rows(game::current_state).Set_Key_At_Row(0, game::id_ship{90000});
        game::current_state.next_ship_id = 90001;
        test_ships::Rows(game::current_state)[0].owner = factions::From_Legacy(owner);
        test_ships::Rows(game::current_state)[0].position.Set(legacy_world::Decode_Stored_Ship_Location(location, std::ssize(game::current_state.stars)));
        test_ships::Rows(game::current_state)[0].status = 0;
        MOX::ship_stack_count = 1;
        MOX::ship_stack_start[0] = 0;
        MOX::ship_node[0].ship_idx = 0;
        MOX::ship_node[0].next_node = -1;
    }

    int16_t Add_Orderable_Ship(int16_t type, int16_t planet_idx = -1, bool moving = false) {
        test_stars::Record(0);
        const auto ship_idx = static_cast<int16_t>(game::current_state.ship_slots.size());
        ship_slots::Resize(game::current_state, ship_idx + 1);
        game::current_state.next_ship_id = 90001u + static_cast<uint32_t>(ship_idx);
        test_ships::Rows(game::current_state).Set_Key_At_Row(ship_idx, game::id_ship{game::current_state.next_ship_id - 1});
        s_ship_data* ship = &test_ships::Rows(game::current_state)[ship_idx];
        ship->owner = factions::From_Legacy(MOX::Legacy_Local_Player_Slot());
        ship->d.ship_type = type;
        ship->position.Set({moving ? game::e_ship_location::in_flight : game::e_ship_location::at_star, game::id_star{0}});
        ship->x = moving ? 42 : 0;
        ship->y = moving ? 24 : 0;
        ship->colonization_target = SERIAL::Decode_Legacy_Planet_Reference(planet_idx);
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
                        test_stars::Record(0).visited.set(game::id_player{0}, knowledge == 0);
                        game::current_state.players[0].traits[TRAIT_OMNISCIENCE] = knowledge == 1;
                        game::current_state.players[0].has_galactic_lore = knowledge == 2;
                        // Empty, enemy colony, own colony, enemy outpost, own ship,
                        // enemy ship in orbit, or enemy ship en route.
                        if (presence >= 1 && presence <= 3) {
                            test_planets::Record(0).colony_id = game::id_colony{0};
                            test_colonies::Record(0).planet_id = game::id_planet{0};
                            test_colonies::Record(0).owner = legacy_world::Decode_Player_Slot(presence == 2 ? 0 : 1);
                            test_colonies::Record(0).outpost_flag = presence == 3;
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
    constexpr game::id_colony colony_id{70000};
    test_planets::Record(0).colony_id = colony_id;
    test_colonies::Record(colony_id.id).owner = legacy_world::Decode_Player_Slot(1);
    test_colonies::Record(colony_id.id).planet_id = game::id_planet{0};
    game::current_state.players[1].home_planet_id = game::NO_PLANET;
    Add_Ship(1, 0);
    PLNTSUM::Set_Planet_Explored_Flags();
    Set_Filters(7);
    Check_List({});

    BOMB::Destroy_Colony(game::current_state, colony_id, -1);
    DOCTEST_CHECK(test_planets::Record(0).colony_id == game::NO_COLONY);
    DOCTEST_CHECK_FALSE(game::current_state.colonies.contains(colony_id));
    DOCTEST_CHECK(game::current_state.next_colony_id == colony_id.id + 1);
    DOCTEST_CHECK(test_stars::Record(0).visited == game::player_bitmap::From_Byte(1));
    Check_List({});
    // Model Kill_Ship's retirement, including a stack that has not yet been rebuilt.
    test_ships::Rows(game::current_state)[0].status = 5;
    test_ships::Rows(game::current_state)[0].position.Set(game::NO_SHIP_LOCATION);
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
            MOX::local_player = legacy_world::Decode_Player_Slot(player);
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
    test_planets::Record(0).colony_id = game::id_colony{0};
    test_colonies::Record(0).owner = legacy_world::Decode_Player_Slot(1);
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
    test_stars::Record(0).visited.reset();
    game::current_state.players[0].has_galactic_lore = 1;
    PLNTSUM::Set_Planet_Explored_Flags();
    Set_Filters(6);
    Check_List({2, 3, 4});
    Set_Filters(7);
    Check_List({});
    Set_Filters(3);
    Check_List({});

    test_stars::Record(0).visited = game::player_bitmap::From_Byte(1);
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
        test_ships::Rows(game::current_state)[choice].position.Set({game::e_ship_location::in_flight, game::id_star{0}});
        test_ships::Rows(game::current_state)[choice].colonization_target = game::id_planet{0};
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
                int16_t row = PLNTSUM::Planet_Summary_List_Id(game::id_planet{0});
                DOCTEST_REQUIRE(row >= 0);
                // These are deliberately not the dispatched ship.
                const auto free_key = game::Ship_Key_At_Row(game::current_state, free_ship);
                PLNTSUM::planet_summary[row].best_colony_ship = free_key;
                PLNTSUM::planet_summary[row].best_outpost_ship = free_key;
                PLNTSUM::send_choice = idle ? (type == SHIP_TYPE_COLONY ? 0 : 1) : -1;
                PLNTSUM::Undo_Send_Order(game::id_planet{0}, type);
                DOCTEST_CAPTURE(type);
                DOCTEST_CAPTURE(idle);
                DOCTEST_CAPTURE(moving);
                DOCTEST_CHECK(TEST_PLNTSUM::confirmation_count == 1);
                DOCTEST_CHECK(TEST_PLNTSUM::moved_ship == sent);
                DOCTEST_CHECK(TEST_PLNTSUM::moved_to_star == 0);
                DOCTEST_CHECK(PLNTSUM::Colony_Ship_Auto_Colonization(&test_ships::Rows(game::current_state)[sent]) == game::NO_PLANET);
                DOCTEST_CHECK(PLNTSUM::planet_summary[row].colony_eta == -1);
                DOCTEST_CHECK(PLNTSUM::planet_summary[row].outpost_eta == -1);
                DOCTEST_CHECK(PLNTSUM::screen->refresh_fields);
                DOCTEST_CHECK(PLNTSUM::Ship_Is_Not_Colonizing(&test_ships::Rows(game::current_state)[sent]) == !moving);
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
    test_ships::Rows(game::current_state)[Add_Orderable_Ship(SHIP_TYPE_COLONY, 0, true)].owner = factions::From_Legacy(1);
    test_ships::Rows(game::current_state)[Add_Orderable_Ship(SHIP_TYPE_COLONY, 0, true)].status = 5;
    test_ships::Rows(game::current_state)[Add_Orderable_Ship(SHIP_TYPE_COLONY, 0, true)].position.Set(game::NO_SHIP_LOCATION);
    Add_Orderable_Ship(SHIP_TYPE_OUTPOST, 0, true);
    Add_Orderable_Ship(SHIP_TYPE_COLONY, 1, true);
    Build_Planet_List();
    PLNTSUM::Determine_Button_Status();
    PLNTSUM::Undo_Send_Order(game::id_planet{0}, SHIP_TYPE_COLONY);
    DOCTEST_CHECK(TEST_PLNTSUM::moved_ship == displayed);
    DOCTEST_CHECK(PLNTSUM::Colony_Ship_Auto_Colonization(&test_ships::Rows(game::current_state)[0]) == game::id_planet{0});
}

DOCTEST_TEST_CASE("summary ETA and recall use the wide planet key rather than its row") {
    TEST_PLNTSUM::Reset_();
    Add_System(0, 0);
    constexpr game::id_planet target{70000};
    test_planets::Set(target.id, test_planets::Record(4));
    DOCTEST_REQUIRE(game::Erase_Planet(game::current_state, game::id_planet{4}));
    test_stars::Record(0).orbit_planet_ids[4] = target;
    const auto sent = Add_Orderable_Ship(SHIP_TYPE_COLONY, -1, true);
    test_ships::Rows(game::current_state)[sent].colonization_target = target;

    Build_Planet_List();
    const auto row = PLNTSUM::Planet_Summary_List_Id(target);
    DOCTEST_REQUIRE(row >= 0);
    DOCTEST_CHECK(PLNTSUM::planet_summary[row].planet_id.get() == target);
    PLNTSUM::Update_Ship_Etas();
    DOCTEST_CHECK(PLNTSUM::planet_summary[row].colony_eta >= 0);

    PLNTSUM::Undo_Send_Order(target, SHIP_TYPE_COLONY);
    DOCTEST_CHECK(TEST_PLNTSUM::moved_ship == sent);
    DOCTEST_CHECK(test_ships::Rows(game::current_state)[sent].colonization_target == game::NO_PLANET);
    DOCTEST_CHECK(PLNTSUM::planet_summary[row].colony_eta == -1);
}

DOCTEST_TEST_CASE("recall confirms a ship key even when its row changes") {
    TEST_PLNTSUM::Reset_();
    Add_System(0, 0);
    const auto sent = Add_Orderable_Ship(SHIP_TYPE_COLONY, 0, true);
    const auto other = Add_Orderable_Ship(SHIP_TYPE_COLONY);
    const auto key = test_ships::Rows(game::current_state)[sent].id;
    Build_Planet_List();

    DOCTEST_SUBCASE("reordered ship") {
        TEST_PLNTSUM::on_confirmation = [] { test_ships::Rows(game::current_state).Swap_Rows(0, 1); };
        PLNTSUM::Undo_Send_Order(game::id_planet{0}, SHIP_TYPE_COLONY);
        DOCTEST_CHECK(TEST_PLNTSUM::moved_ship == other);
        const auto ship_entry = game::current_state.ships.find(key);
        const auto* ship = ship_entry == game::current_state.ships.end() ? nullptr : &ship_entry->second;
        DOCTEST_REQUIRE(ship);
        DOCTEST_CHECK(ship->colonization_target == game::NO_PLANET);
    }
    DOCTEST_SUBCASE("replaced ship") {
        TEST_PLNTSUM::on_confirmation = [] { test_ships::Rows(game::current_state).Set_Key_At_Row(0, game::id_ship{90002}); };
        PLNTSUM::Undo_Send_Order(game::id_planet{0}, SHIP_TYPE_COLONY);
        DOCTEST_CHECK(TEST_PLNTSUM::moved_ship == -1);
        DOCTEST_CHECK(test_ships::Rows(game::current_state)[sent].colonization_target == game::id_planet{0});
    }
    DOCTEST_CHECK(TEST_PLNTSUM::confirmation_count == 1);
    TEST_PLNTSUM::on_confirmation = nullptr;
}

DOCTEST_TEST_CASE("declined unavailable or missing recall orders do not move a ship") {
    for (int scenario = 0; scenario < 3; ++scenario) {
        TEST_PLNTSUM::Reset_();
        Add_System(0, 0);
        int16_t sent = Add_Orderable_Ship(SHIP_TYPE_COLONY, scenario == 2 ? 1 : 0, true);
        Build_Planet_List();
        TEST_PLNTSUM::confirm_recall = false;
        TEST_PLNTSUM::can_select_ship = scenario != 1;
        PLNTSUM::Undo_Send_Order(game::id_planet{0}, SHIP_TYPE_COLONY);
        DOCTEST_CHECK(TEST_PLNTSUM::moved_ship == -1);
        DOCTEST_CHECK(TEST_PLNTSUM::confirmation_count == (scenario == 0 ? 1 : 0));
        DOCTEST_CHECK(TEST_PLNTSUM::warning_count == (scenario == 1 ? 1 : 0));
        DOCTEST_CHECK(PLNTSUM::Colony_Ship_Auto_Colonization(&test_ships::Rows(game::current_state)[sent]) == game::id_planet{scenario == 2 ? 1u : 0u});
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
                const auto planet = PLNTSUM::planet_summary[row].planet_id.get().id;
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
