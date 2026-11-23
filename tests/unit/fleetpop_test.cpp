#include "pch.h"
#include "game/legacy_ship_location_codec.h"
#include "../fixtures/star_roster.h"
#include "../fixtures/colony_roster.h"
#include "../fixtures/planet_roster.h"

#include "doctest/doctest.h"
#include "game/fleetpop.h"
#include "game/shipstak.h"
#include "../fixtures/game_state.h"

namespace TEST_FLEETPOP {
    void Reset_();
}

namespace {
    struct s_fleet_state : test::s_game_state {
        test::s_saved_array<s_ship_node, MAX_SHIPS> nodes{MOX::ship_node};
        test::s_saved_array<int16_t, MAX_SHIPS> starts{MOX::ship_stack_start};
        test::s_saved_array<s_ship_icon, SHIP_ICON_COUNT> icons{MOX::ship_icon};
        test::s_saved_value<std::set<game::id_ship>> selection{MOX::fleet_selected_ships};
        decltype(SHIPSTAK::scan_bonus) scan = SHIPSTAK::scan_bonus;
        decltype(SHIPSTAK::stealth_bonus) stealth = SHIPSTAK::stealth_bonus;
        decltype(SHIPSTAK::stealth_size_penalty) penalty = SHIPSTAK::stealth_size_penalty;
        int16_t stack_count = MOX::ship_stack_count;
        int16_t icon_count = MOX::ship_icon_count;
        int16_t selected = MOX::small_ship_stack_ptr;
        int16_t scanned = MOX::scanned_small_ship;
        game::id_star star = MOX::galaxy_map_scanned_star;
        int16_t fleet = MOX::fleet_box_ship_stack;

        ~s_fleet_state() {
            SHIPSTAK::scan_bonus = scan;
            SHIPSTAK::stealth_bonus = stealth;
            SHIPSTAK::stealth_size_penalty = penalty;
            MOX::ship_stack_count = stack_count;
            MOX::ship_icon_count = icon_count;
            MOX::small_ship_stack_ptr = selected;
            MOX::scanned_small_ship = scanned;
            MOX::galaxy_map_scanned_star = star;
            MOX::fleet_box_ship_stack = fleet;
        }
    };
}

DOCTEST_TEST_CASE_FIXTURE(s_fleet_state, "fleet at last active star exposes build outpost button") {
    TEST_FLEETPOP::Reset_();
    game::current_state.ship_records[0].position.Set(legacy_world::Location_From_Slot(game::e_ship_location::at_star, static_cast<int16_t>(std::ssize(game::current_state.stars) - 1), std::ssize(game::current_state.stars)));

    DOCTEST_CHECK(FLEETPOP::Set_Build_Outpost_Button_Flag(1) == 1);
}

DOCTEST_TEST_CASE_FIXTURE(s_fleet_state, "fleet selection follows a ship key across reordered rows") {
    TEST_FLEETPOP::Reset_();
    auto& state = game::current_state;
    state.ship_count = 2;
    state.next_ship_id = 90002;
    game::current_state.ship_records[0] = {};
    game::current_state.ship_records[1] = {};
    game::current_state.ship_records.Set_Key_At_Row(0, game::id_ship{90000});
    game::current_state.ship_records.Set_Key_At_Row(1, game::id_ship{90001});
    MOX::ship_node[0].ship_idx = 0;
    MOX::ship_node[1].ship_idx = 1;
    MOX::fleet_selected_ships = {game::id_ship{90000}};

    state.ship_records.Swap_Rows(0, 1);
    FLEETPOP::Reload_Ship_Selection_Status();
    DOCTEST_CHECK((MOX::ship_node[0].selected == 0));
    DOCTEST_CHECK((MOX::ship_node[1].selected == 1));
}

DOCTEST_TEST_CASE_FIXTURE(s_fleet_state, "fleet selection distinguishes node and stack order and rejects a cyclic stack") {
    TEST_FLEETPOP::Reset_();
    auto& state = game::current_state;
    const game::id_ship first{90000}, second{90001};
    state.next_ship_id = 90003;
    state.ship_count = 3;
    s_ship_data transport{};
    transport.d.ship_type = SHIP_TYPE_TRANSPORT;
    transport.id = first;
    state.ship_records.Replace_Row(0, transport);
    transport.id = second;
    state.ship_records.Replace_Row(1, transport);
    transport.id = game::id_ship{90002};
    transport.owner = 1;
    state.ship_records.Replace_Row(2, transport);
    MOX::ship_node[0].ship_idx = 0;
    MOX::ship_node[0].selected = 1;
    MOX::ship_node[0].next_node = 2;
    MOX::ship_node[1].ship_idx = 1;
    MOX::ship_node[1].selected = 1;
    MOX::ship_node[1].next_node = 0;
    MOX::ship_node[2].ship_idx = 2;
    MOX::ship_node[2].selected = 1;
    MOX::ship_node[2].next_node = -1;
    MOX::ship_stack_start[0] = 1;

    DOCTEST_CHECK((HACCESS::Selected_Fleet_Ship_Keys(SHIP_TYPE_TRANSPORT) == std::vector{first, second}));
    DOCTEST_CHECK(HACCESS::Selected_Fleet_Ship_Keys(SHIP_TYPE_OUTPOST).empty());
    DOCTEST_CHECK((HACCESS::Selected_Stack_Ship_Keys(0) == std::vector{second, first}));
    MOX::ship_node[2].next_node = 1;
    DOCTEST_CHECK(HACCESS::Selected_Stack_Ship_Keys(0).empty());
}

DOCTEST_TEST_CASE_FIXTURE(s_fleet_state, "big fleet icons do not select a ship that reused the original row") {
    TEST_FLEETPOP::Reset_();
    auto& state = game::current_state;
    const game::id_ship selected{90000};
    state.next_ship_id = 90003;
    state.ship_records.Set_Key_At_Row(0, selected);
    state.ship_records.Set_Key_At_Row(1, game::id_ship{90001});
    s_big_icon icon{};
    icon.selected = 1;
    icon.ship_key = selected;
    test::s_saved_value saved_icons{MOX::fltscrn_big_icon};
    test::s_saved_value saved_count{MOX::n_fltscrn_big_icons};
    MOX::fltscrn_big_icon = &icon;
    MOX::n_fltscrn_big_icons = 1;

    state.ship_records.Swap_Rows(0, 1);
    DOCTEST_CHECK(FLT1::First_Selected_Ship() == selected);
    DOCTEST_CHECK(FLT1::Count_Selected_Ships() == 1);
    state.ship_records.Set_Key_At_Row(1, game::id_ship{90002});
    DOCTEST_CHECK(FLT1::First_Selected_Ship() == game::NO_SHIP);
    DOCTEST_CHECK(FLT1::Count_Selected_Ships() == 0);
}

DOCTEST_TEST_CASE_FIXTURE(s_fleet_state, "fleet focus follows its original ship through a stack rebuild") {
    TEST_FLEETPOP::Reset_();
    auto& state = game::current_state;
    const game::id_ship original{90000}, other{90001};
    state.ship_count = 2;
    state.next_ship_id = 90003;
    state.ship_records.Set_Key_At_Row(0, original);
    state.ship_records.Set_Key_At_Row(1, other);
    test::s_saved_value saved_count{MOX::fleet_box.count};
    test::s_saved_value saved_icon{MOX::fleet_movement_icon[0]};
    MOX::fleet_box.count = 1;
    MOX::fleet_movement_icon[0].ship_node_idx = 0;
    MOX::ship_node[0].ship_idx = 0;
    const auto focus = HACCESS::Fleet_Movement_Ship_Key(0);
    DOCTEST_REQUIRE(focus == original);

    state.ship_records.Swap_Rows(0, 1);
    MOX::ship_stack_count = 2;
    for (int16_t stack = 0; stack < 2; ++stack) {
        MOX::ship_stack_start[stack] = stack;
        MOX::ship_node[stack].ship_idx = stack;
        MOX::ship_node[stack].next_node = -1;
    }
    DOCTEST_CHECK(HACCESS::Fleet_Movement_Ship_Key(0) == other);
    DOCTEST_CHECK(HACCESS::Ship_Stack_From_Ship_Key(focus) == 1);
    state.ship_records.Set_Key_At_Row(1, game::id_ship{90002});
    DOCTEST_CHECK(HACCESS::Ship_Stack_From_Ship_Key(focus) == -1);
}

DOCTEST_TEST_CASE_FIXTURE(s_fleet_state, "build outpost button rejects invalid direct-star boundaries") {
    TEST_FLEETPOP::Reset_();

    const int16_t invalid_locations[] = {
        static_cast<int16_t>(game::current_state.stars.size()),
        static_cast<int16_t>(MAX_STARS),
        static_cast<int16_t>(SHIP_LOCATION_MOVING_OFFSET - 1),
    };

    for (int16_t location : invalid_locations) {
        TEST_FLEETPOP::Reset_();
        game::current_state.ship_records[0].position.Set(legacy_world::Location_From_Slot(game::e_ship_location::at_star, location, std::ssize(game::current_state.stars)));

        DOCTEST_CAPTURE(location);
        DOCTEST_CHECK(FLEETPOP::Set_Build_Outpost_Button_Flag(1) == 0);
    }
}

DOCTEST_TEST_CASE_FIXTURE(s_fleet_state, "fleet en route does not expose build outpost button") {
    TEST_FLEETPOP::Reset_();
    game::current_state.ship_records[0].position.Set({game::e_ship_location::in_wormhole, game::id_star{1}});

    DOCTEST_CHECK(FLEETPOP::Set_Build_Outpost_Button_Flag(1) == 0);
}

DOCTEST_TEST_CASE_FIXTURE(s_fleet_state, "fleet navigation is a no-op without ship stacks") {
    TEST_FLEETPOP::Reset_();
    MOX::ship_stack_count = 0;
    MOX::small_ship_stack_ptr = -1;

    FLT1::Next_Ship_Icon();
    DOCTEST_CHECK(MOX::small_ship_stack_ptr == -1);

    FLT1::Previous_Ship_Icon();
    DOCTEST_CHECK(MOX::small_ship_stack_ptr == -1);
}

DOCTEST_TEST_CASE_FIXTURE(s_fleet_state, "Antares fleet display and combat flags do not require live star lookup") {
    TEST_FLEETPOP::Reset_();
    auto& ship = game::current_state.ship_records[0];
    ship.position.Set({game::e_ship_location::in_flight, game::NO_STAR, game::e_ship_target::antares});
    ship.turns_left = 1;
    int16_t value = -1, position = -1, info = -1;
    FLEETPOP::Get_Absolute_Location_Scan_Info(0, &value, &position, &info);
    DOCTEST_CHECK(value == 1);
    DOCTEST_CHECK(position == game::ANTARES_POSITION);
    DOCTEST_CHECK(info == 2);
    ship.position.Set({game::e_ship_location::at_star, game::NO_STAR, game::e_ship_target::antares});
    ship.status = 0;
    ship.d.ship_type = SHIP_TYPE_COMBAT;
    game::current_state.antares_destination.ignore_combat_ships = 1;
    DOCTEST_CHECK(FLEETPOP::Set_Engage_Enemy_Forces_Flag(1) == 1);
    DOCTEST_CHECK(FLEETPOP::Set_Attack_Antares_Button_Flag() == 0);
}

DOCTEST_TEST_CASE_FIXTURE(s_fleet_state, "colony warning only considers enemy fleets targeting live colony stars") {
    TEST_FLEETPOP::Reset_();
    auto& ship = game::current_state.ship_records[0];
    ship.owner = 1;
    ship.status = 1;
    const auto colony_star = game::Star_Key_At_Row(game::current_state, 1);
    game::current_state.stars.at(colony_star).has_colony = 1;

    ship.position.Set({game::e_ship_location::in_flight, game::NO_STAR, game::e_ship_target::antares});
    DOCTEST_CHECK_FALSE(HACCESS::Enemy_Ship_Going_To_My_Colony());
    ship.position.Set(game::NO_SHIP_LOCATION);
    DOCTEST_CHECK_FALSE(HACCESS::Enemy_Ship_Going_To_My_Colony());
    ship.position.Set({game::e_ship_location::in_flight, game::id_star{90000}});
    DOCTEST_CHECK_FALSE(HACCESS::Enemy_Ship_Going_To_My_Colony());
    ship.position.Set({game::e_ship_location::in_flight, colony_star});
    DOCTEST_CHECK(HACCESS::Enemy_Ship_Going_To_My_Colony());
}

DOCTEST_TEST_CASE_FIXTURE(s_fleet_state, "fleet summary counts only ships linked into the visible stack") {
    TEST_FLEETPOP::Reset_();

    MOX::ship_node[0].next_node = 1;
    MOX::ship_node[1].ship_idx = 1;
    MOX::ship_node[1].next_node = -1;

    game::current_state.ship_records[0].d.ship_type = SHIP_TYPE_COMBAT;
    game::current_state.ship_records[0].d.size = SHIP_SIZE_SMALL;
    game::current_state.ship_records[1].d.ship_type = SHIP_TYPE_OUTPOST;
    game::current_state.ship_records[2].d.ship_type = SHIP_TYPE_COLONY;

    int16_t total_count;
    int16_t colony_ships_count;
    int16_t outposts_count;
    int16_t transports_count;
    int16_t ship_sizes[SHIP_SIZE_DOOM_STAR + 1];

    SYS::N_Ships_By_Type_In_Stack(
        0,
        &total_count,
        &colony_ships_count,
        &outposts_count,
        &transports_count,
        ship_sizes);

    DOCTEST_CHECK(total_count == 2);
    DOCTEST_CHECK(ship_sizes[SHIP_SIZE_SMALL] == 1);
    DOCTEST_CHECK(colony_ships_count == 0);
    DOCTEST_CHECK(outposts_count == 1);
    DOCTEST_CHECK(transports_count == 0);
}

DOCTEST_TEST_CASE_FIXTURE(s_fleet_state, "Space Scanner uses its independent strategic range") {
    TEST_FLEETPOP::Reset_();
    game::current_state.ship_records[0].owner = 1;
    game::current_state.ship_records[0].x = 90;
    game::current_state.ship_records[1].owner = 0;
    game::current_state.ship_records[1].status = 0;
    game::current_state.ship_records[1].x = 0;
    MOX::player[0].tech_applications[TECH_APP_SPACE_SCANNER] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    SHIPSTAK::scan_bonus.no_scanner = 2;
    SHIPSTAK::scan_bonus.space_scanner = 3;

    SHIPSTAK::Remove_Non_Detected_Ships();

    DOCTEST_CHECK(MOX::ship_stack_count == 1);
    DOCTEST_CHECK(MOX::ship_stack_start[0] == 0);
}

DOCTEST_TEST_CASE_FIXTURE(s_fleet_state, "maximum strategic scan bonuses do not wrap negative") {
    TEST_FLEETPOP::Reset_();
    game::current_state.ship_records[0].owner = 1;
    game::current_state.ship_records[0].x = 90;
    game::current_state.ship_records[1].owner = 0;
    game::current_state.ship_records[1].status = 0;
    game::current_state.ship_records[1].x = 0;
    struct_::Set_Bit_Field(
        game::current_state.ship_records[1].d.special_device_flags, SPECIAL_BATTLE_SCANNER);
    SHIPSTAK::scan_bonus.no_scanner = 127;
    SHIPSTAK::scan_bonus.battle_scanner = 32767;

    SHIPSTAK::Remove_Non_Detected_Ships();

    DOCTEST_CHECK(MOX::ship_stack_count == 1);
    DOCTEST_CHECK(MOX::ship_stack_start[0] == 0);
}

DOCTEST_TEST_CASE_FIXTURE(s_fleet_state, "configured planet proximity determines same-location detection") {
    TEST_FLEETPOP::Reset_();
    game::current_state.ship_count = 1;
    game::current_state.ship_records[0].owner = 1;
    test_colonies::Record(0).owner = 0;
    test_colonies::Record(0).planet_id = game::id_planet{0};
    test_planets::Record(0).star_id = game::id_star{static_cast<uint32_t>(0)};
    test_stars::Record(0).x = 0;
    test_stars::Record(0).y = 0;
    MOX::player[1].traits[TRAIT_STEALTHY_SHIPS] = 1;
    SHIPSTAK::scan_bonus.planet = 9;
    SHIPSTAK::stealth_bonus.stealthy_ships = 10;
    SHIPSTAK::stealth_bonus.stealth_field = 0;
    SHIPSTAK::stealth_bonus.cloaking_device = 0;
    SHIPSTAK::stealth_bonus.phasing_cloak = 0;

    SHIPSTAK::Remove_Non_Detected_Ships();

    DOCTEST_CHECK(MOX::ship_stack_count == 0);

    TEST_FLEETPOP::Reset_();
    game::current_state.ship_count = 1;
    game::current_state.ship_records[0].owner = 1;
    test_colonies::Record(0).owner = 0;
    test_colonies::Record(0).planet_id = game::id_planet{0};
    test_planets::Record(0).star_id = game::id_star{static_cast<uint32_t>(0)};
    test_stars::Record(0).x = 0;
    test_stars::Record(0).y = 0;
    MOX::player[1].traits[TRAIT_STEALTHY_SHIPS] = 1;
    SHIPSTAK::scan_bonus.planet = 10;

    SHIPSTAK::Remove_Non_Detected_Ships();

    DOCTEST_CHECK(MOX::ship_stack_count == 1);
}

DOCTEST_TEST_CASE_FIXTURE(s_fleet_state, "configured fleet proximity determines same-location detection") {
    TEST_FLEETPOP::Reset_();
    game::current_state.ship_records[0].owner = 1;
    game::current_state.ship_records[1].owner = 0;
    game::current_state.ship_records[1].status = 0;
    MOX::player[1].traits[TRAIT_STEALTHY_SHIPS] = 1;
    SHIPSTAK::scan_bonus.fleet = 9;
    SHIPSTAK::stealth_bonus.stealthy_ships = 10;
    SHIPSTAK::stealth_bonus.stealth_field = 0;
    SHIPSTAK::stealth_bonus.cloaking_device = 0;
    SHIPSTAK::stealth_bonus.phasing_cloak = 0;

    SHIPSTAK::Remove_Non_Detected_Ships();

    DOCTEST_CHECK(MOX::ship_stack_count == 0);

    TEST_FLEETPOP::Reset_();
    game::current_state.ship_records[0].owner = 1;
    game::current_state.ship_records[1].owner = 0;
    game::current_state.ship_records[1].status = 0;
    MOX::player[1].traits[TRAIT_STEALTHY_SHIPS] = 1;
    SHIPSTAK::scan_bonus.fleet = 10;

    SHIPSTAK::Remove_Non_Detected_Ships();

    DOCTEST_CHECK(MOX::ship_stack_count == 1);
}

DOCTEST_TEST_CASE_FIXTURE(s_fleet_state, "race_tech retains size penalty for zero-value stealth device") {
    TEST_FLEETPOP::Reset_();
    game::current_state.ship_records[0].owner = 1;
    game::current_state.ship_records[0].status = 1;
    game::current_state.ship_records[0].x = 90;
    game::current_state.ship_records[0].d.size = SHIP_SIZE_MEDIUM;
    struct_::Set_Bit_Field(
        game::current_state.ship_records[0].d.special_device_flags, SPECIAL_STEALTH_FIELD);
    game::current_state.ship_records[1].owner = 0;
    game::current_state.ship_records[1].status = 0;
    game::current_state.ship_records[1].x = 0;
    SHIPSTAK::scan_bonus.no_scanner = 2;
    SHIPSTAK::stealth_bonus.stealth_field = 0;
    SHIPSTAK::stealth_bonus.cloaking_device = 0;
    SHIPSTAK::stealth_bonus.phasing_cloak = 0;
    SHIPSTAK::stealth_size_penalty =
        SHIPSTAK::e_stealth_size_penalty::race_tech;

    SHIPSTAK::Remove_Non_Detected_Ships();

    DOCTEST_CHECK(MOX::ship_stack_count == 1);
    DOCTEST_CHECK(MOX::ship_stack_start[0] == 0);
}

DOCTEST_TEST_CASE_FIXTURE(s_fleet_state, "race_tech retains size penalty when device bonuses cancel") {
    TEST_FLEETPOP::Reset_();
    game::current_state.ship_records[0].owner = 1;
    game::current_state.ship_records[0].status = 1;
    game::current_state.ship_records[0].x = 90;
    game::current_state.ship_records[0].d.size = SHIP_SIZE_MEDIUM;
    struct_::Set_Bit_Field(
        game::current_state.ship_records[0].d.special_device_flags, SPECIAL_STEALTH_FIELD);
    struct_::Set_Bit_Field(
        game::current_state.ship_records[0].d.special_device_flags, SPECIAL_CLOAKING_DEVICE);
    game::current_state.ship_records[1].owner = 0;
    game::current_state.ship_records[1].status = 0;
    game::current_state.ship_records[1].x = 0;
    SHIPSTAK::scan_bonus.no_scanner = 2;
    SHIPSTAK::stealth_bonus.stealth_field = 25;
    SHIPSTAK::stealth_bonus.cloaking_device = -25;
    SHIPSTAK::stealth_bonus.phasing_cloak = 0;
    SHIPSTAK::stealth_size_penalty =
        SHIPSTAK::e_stealth_size_penalty::race_tech;

    SHIPSTAK::Remove_Non_Detected_Ships();

    DOCTEST_CHECK(MOX::ship_stack_count == 1);
    DOCTEST_CHECK(MOX::ship_stack_start[0] == 0);
}
