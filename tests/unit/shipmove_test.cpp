#include "pch.h"
#include "../fixtures/ship_rows.h"

#include <memory>
#include "game/ship_slots.h"
#include "game/legacy_ship_location_codec.h"
#include "../fixtures/star_roster.h"
#include "../fixtures/colony_roster.h"
#include "../fixtures/planet_roster.h"

#include "doctest/doctest.h"
#include "game/fleet_movement.h"
#include "game/shipmove.h"
#include "lua/lua_game_ship_identity.h"

namespace TEST_SHIPMOVE {
    extern int icon_refresh_count;
    extern int8_t icon_refresh_ship_status;
    extern int16_t pooled_officer, killed_officer;
    extern int16_t best_warp[MAX_PLAYERS], ftl_speed[MAX_PLAYERS];
    extern int drive_queries, speed_queries;
    extern void (*on_confirmation)();
    extern std::vector<s_MSG> messages;
    extern std::vector<int16_t> message_players;
    void Reset_();
    void Set_Hyperspace_Flux_(bool active);
    int16_t Movement_Speed_();
    bool Navigator_Pointer_Supplied_();
} // namespace TEST_SHIPMOVE

namespace {
    game::id_ship Bind_Test_Ship(int row) {
        auto& state = game::current_state;
        DOCTEST_REQUIRE(row >= 0);
        DOCTEST_REQUIRE(row < static_cast<int16_t>(state.ship_slots.size()));
        const auto existing = test_ships::Rows(state)[row].id;
        if (existing != game::NO_SHIP) return existing;
        state.next_ship_id = std::max(state.next_ship_id, 70000u);
        const auto key = game::Allocate_Ship_Id(state);
        DOCTEST_REQUIRE(key != game::NO_SHIP);
        test_ships::Rows(state).Set_Key_At_Row(row, key);
        return key;
    }

    game::s_star_destination Destination(int row) {
        return game::Destination_At_Row(game::current_state, row);
    }
}

DOCTEST_TEST_CASE("ship records follow sparse keys independently of projected rows") {
    auto state_storage = std::make_unique<game::s_game_state>();
    auto& state = *state_storage;
    state.ship_slots.resize(4, game::NO_SHIP);
    const game::id_ship first{90000};
    const game::id_ship second{70000};
    ship_slots::Put(state, 0, s_ship_data{.id = first});
    ship_slots::Put(state, 3, s_ship_data{.id = second});
    const auto* record = &state.ships.at(first);
    DOCTEST_CHECK(state.ships.size() == 2);
    std::swap(state.ship_slots[0], state.ship_slots[3]);
    DOCTEST_CHECK(&state.ships.at(first) == record);
    DOCTEST_CHECK(state.ship_slots[3] == first);
    DOCTEST_CHECK(state.ship_slots[0] == second);
    DOCTEST_CHECK_THROWS_AS(ship_slots::Put(state, 1, *record), std::logic_error);
    DOCTEST_CHECK(state.ship_slots[3] == first);
    DOCTEST_CHECK(state.ship_slots[1] == game::NO_SHIP);

    const game::id_ship replacement{120000};
    ship_slots::Put(state, 3, ship_slots::Vacant(game::owner_slot{1}));
    DOCTEST_CHECK_FALSE(state.ships.contains(first));
    DOCTEST_CHECK(state.ship_slots[3] == game::NO_SHIP);
    DOCTEST_CHECK(state.ship_slots.size() == 4);
    DOCTEST_CHECK(ship_slots::Owner(state, 3) == game::owner_slot{1});
    ship_slots::Put(state, 3, s_ship_data{.owner = game::owner_slot{1}, .id = replacement});
    DOCTEST_CHECK(state.ship_slots[3] == replacement);
    DOCTEST_CHECK(state.ship_slots[0] == second);
    DOCTEST_CHECK_FALSE(state.ships.contains(first));
}

DOCTEST_TEST_CASE("fuel loss marks ships in the supplied state, not the live roster") {
    TEST_SHIPMOVE::Reset_();
    auto state_storage = std::make_unique<game::s_game_state>();
    auto& state = *state_storage;
    state.next_star_id = 90000;
    DOCTEST_REQUIRE(game::Create_Star(state, {.has_colony = game::player_bitmap::From_Byte(1)}));
    const auto distant = *game::Create_Star(state, {.x = 300});
    test_ships::Rows(state).resize(1);
    ship_slots::Resize(state, 1);
    test_ships::Rows(state)[0].position.Set({game::e_ship_location::at_star, distant});
    state.players[0].ship_range = 2;
    state.player_count = 1;
    lostship::Mark_Out_Of_Range_Ships(state, game::id_player{0});

    DOCTEST_CHECK((test_ships::Rows(state)[0].status == 9));
    DOCTEST_CHECK((test_ships::Rows(game::current_state)[0].status == 0));
}

DOCTEST_TEST_CASE("nonplayer slot reuse does not reuse a ship identity") {
    TEST_SHIPMOVE::Reset_();
    auto& state = game::current_state;
    ship_slots::Resize(state, 1);
    state.next_ship_id = 90000;
    test_ships::Rows(game::current_state)[0].owner = factions::From_Legacy(MAX_PLAYERS);
    test_ships::Rows(game::current_state)[0].status = 5;
    DOCTEST_REQUIRE(SHIPMOVE::Create_Nonplayer_Ship(game::owner_slot{MAX_PLAYERS}, nullptr, 0, 0) == game::id_ship{90000});
    DOCTEST_CHECK(test_ships::Rows(game::current_state)[0].id == game::id_ship{90000});
    test_ships::Rows(game::current_state)[0].status = 5;
    DOCTEST_REQUIRE(SHIPMOVE::Create_Nonplayer_Ship(game::owner_slot{MAX_PLAYERS}, nullptr, 0, 0) == game::id_ship{90001});
    DOCTEST_CHECK(test_ships::Rows(game::current_state)[0].id == game::id_ship{90001});
    DOCTEST_CHECK(state.next_ship_id == 90002);
}

DOCTEST_TEST_CASE("stopping transit changes location and activity without arrival side effects") {
    TEST_SHIPMOVE::Reset_();
    auto* ship = &test_ships::Rows(game::current_state)[0];
    ship->status = 2;
    ship->position.Set(legacy_world::Location_From_Slot(game::e_ship_location::in_wormhole, 1, std::ssize(game::current_state.stars)));
    ship->x = 37;
    ship->y = 19;
    ship->mission = 5;
    DOCTEST_REQUIRE(SHIPMOVE::Stop_Ship_Transit(game::current_state, ship, Destination(0)));
    DOCTEST_CHECK((int8_t)ship->status == 0);
    DOCTEST_CHECK(game::Ship_Orbit_Row(game::current_state, ship->position) == 0);
    DOCTEST_CHECK((int16_t)ship->x == 37);
    DOCTEST_CHECK((int16_t)ship->y == 19);
    DOCTEST_CHECK((int8_t)ship->mission == 5);
    ship->status = 2;
    DOCTEST_CHECK_FALSE(SHIPMOVE::Stop_Ship_Transit(game::current_state, ship, {game::e_star_destination::star, game::NO_STAR}));
    DOCTEST_CHECK((int8_t)ship->status == 2);
    DOCTEST_REQUIRE(SHIPMOVE::Stop_Ship_Transit(game::current_state, ship, {}));
    DOCTEST_CHECK(game::Ship_Orbit_Row(game::current_state, ship->position) == -1);
    DOCTEST_CHECK((int8_t)ship->status == 0);
}

DOCTEST_TEST_CASE("hypothetical fleet origins preserve legacy evaluation without mutating ships") {
    struct s_case {
        int8_t status;
        bool partial;
    };
    const s_case cases[] = {{0, false}, {1, false}, {2, false}, {0, true}, {1, true}, {2, true}};
    for (auto test : cases) {
        const int8_t status = test.status;
        TEST_SHIPMOVE::Reset_();
        test_stars::Resize(3);
        ship_slots::Resize(game::current_state, 2);
        test_stars::Record(2).x = 240;
        test_stars::Record(0).has_jumpgate = game::player_bitmap::From_Byte(1);
        test_stars::Record(2).has_jumpgate = game::player_bitmap::From_Byte(1);
        for (int32_t i = 0; i < 2; ++i) {
            if (i != 0) {
                auto copy = test_ships::Rows(game::current_state)[0];
                copy.id = game::id_ship{static_cast<uint32_t>(i)};
                test_ships::Rows(game::current_state).Replace_Row(i, copy);
            }
            test_ships::Rows(game::current_state)[i].owner = factions::From_Legacy(0);
            test_ships::Rows(game::current_state)[i].status = status;
            test_ships::Rows(game::current_state)[i].position.Set({static_cast<game::e_ship_location>(status), game::id_star{1}});
            test_ships::Rows(game::current_state)[i].x = 120;
            test_ships::Rows(game::current_state)[i].travelling_speed = 6;
            test_ships::Rows(game::current_state)[i].group_has_navigator = 1;
            test_ships::Rows(game::current_state)[i].special_device_damage_flags[0] |= 1;
            test_ships::Rows(game::current_state)[i].prior_jump_gate_stardate = MOX::stardate;
            test_ships::Rows(game::current_state)[i].prior_jump_gate_destination = game::id_star{2};
        }
        auto& state = game::current_state;
        const auto lead = Bind_Test_Ship(0);
        const auto escort = Bind_Test_Ship(1);
        const std::array<s_ship_data, 2> before{test_ships::Rows(state)[0], test_ships::Rows(state)[1]};
        const auto ships = test.partial ? std::vector{lead, lead} : std::vector{lead, escort, lead};
        for (const auto key : ships) {
            const auto ship_entry = state.ships.find(key);
            auto* ship = ship_entry == state.ships.end() ? nullptr : &ship_entry->second;
            ship->position.Set(legacy_world::Location_From_Slot(game::e_ship_location::at_star, 0, std::ssize(game::current_state.stars)));
            ship->x = test_stars::Record(0).x;
            ship->y = test_stars::Record(0).y;
        }
        s_ship_move_info expected{};
        SHIPMOVE::Evaluate_Fleet_Move(game::current_state, ships, Destination(2), &expected);
        for (int32_t i = 0; i < 2; ++i) test_ships::Rows(game::current_state)[i] = before[i];

        s_ship_move_info actual{};
        SHIPMOVE::Evaluate_Fleet_Move(game::current_state, ships, Destination(2), &actual, game::Star_Key_At_Row(state, 0));
        DOCTEST_CHECK(memcmp(&actual, &expected, sizeof(actual)) == 0);
        for (int32_t i = 0; i < 2; ++i) DOCTEST_CHECK(test_ships::Rows(game::current_state)[i] == before[i]);
    }
}

DOCTEST_TEST_CASE("movement to assigned planet star keeps auto-colonization") {
    struct s_case {
        int16_t initial_location;
        int8_t turns_left;
        int16_t expected_location;
    };

    const s_case cases[] = {
        {0, 0, 7},
        {0, 1, 7 + SHIP_LOCATION_WORMHOLE_OFFSET},
        {SHIP_LOCATION_MOVING_OFFSET, 1, 7 + SHIP_LOCATION_MOVING_OFFSET},
    };

    for (const s_case& test_case : cases) {
        TEST_SHIPMOVE::Reset_();
        test_planets::Record(70000).star_id = game::id_star{static_cast<uint32_t>(7)};
        test_stars::Resize(9);
        test_stars::Reset(9);
        test_ships::Rows(game::current_state)[0].d.ship_type = SHIP_TYPE_COLONY;
        test_ships::Rows(game::current_state)[0].colonization_target = game::id_planet{70000};
        test_ships::Rows(game::current_state)[0].position.Set(legacy_world::Decode_Stored_Ship_Location(test_case.initial_location, std::ssize(game::current_state.stars)));

        const game::id_ship ship_list[] = {Bind_Test_Ship(0)};
        s_ship_move_info move_info = {};
        move_info.turns_left = test_case.turns_left;

        DOCTEST_REQUIRE(SHIPMOVE::Order_Fleet_Move(game::current_state, ship_list, Destination(7), &move_info));

        DOCTEST_CHECK(test_ships::Rows(game::current_state)[0].position.Get() == legacy_world::Decode_Stored_Ship_Location(test_case.expected_location, std::ssize(game::current_state.stars)));
        DOCTEST_CHECK(test_ships::Rows(game::current_state)[0].colonization_target == game::id_planet{70000});
    }
}

DOCTEST_TEST_CASE("fleet range refresh uses the highest researched fuel tier and drive services") {
    TEST_SHIPMOVE::Reset_();
    game::current_state.player_count = 2;
    TECHDATA::fuel[1].tech_app_id = TECH_APP_NUCLEAR_DRIVE;
    TECHDATA::fuel[1].ship_range = 4;
    TECHDATA::fuel[2].tech_app_id = TECH_APP_NUCLEAR_BOMB;
    TECHDATA::fuel[2].ship_range = 9;
    for (int i = 0; i < 2; ++i) game::current_state.players[i].tech_applications[TECH_APP_NUCLEAR_DRIVE] = TECH_RESEARCH_STATUS_RESEARCHED;
    game::current_state.players[1].tech_applications[TECH_APP_NUCLEAR_BOMB] = TECH_RESEARCH_STATUS_RESEARCHED;
    TEST_SHIPMOVE::best_warp[0] = 2;
    TEST_SHIPMOVE::best_warp[1] = 3;
    TEST_SHIPMOVE::ftl_speed[0] = 5;
    TEST_SHIPMOVE::ftl_speed[1] = 8;
    SHIPMOVE::Update_All_Ship_Ranges();
    DOCTEST_CHECK((game::current_state.players[0].ship_range == 4));
    DOCTEST_CHECK((game::current_state.players[1].ship_range == 9));
    DOCTEST_CHECK((game::current_state.players[0].best_ftl_drive == 2));
    DOCTEST_CHECK((game::current_state.players[1].best_ftl_drive == 3));
    DOCTEST_CHECK((game::current_state.players[0].ship_speed == 5));
    DOCTEST_CHECK((game::current_state.players[1].ship_speed == 8));
    DOCTEST_CHECK(TEST_SHIPMOVE::drive_queries == 2);
    DOCTEST_CHECK(TEST_SHIPMOVE::speed_queries == 2);
}

DOCTEST_TEST_CASE("spare ship slots reserve bounded Antaran capacity before player rotation") {
    for (int antaran_count : {0, 25}) {
        TEST_SHIPMOVE::Reset_();
        game::current_state.player_count = 2;
        ship_slots::Resize(game::current_state, 0);
        test_ships::Rows(game::current_state).resize(0);
        MOX::antarans.offensive_ships[0] = antaran_count;
        SHIPMOVE::Allocate_New_Ship_Slots();
        const int reserved = antaran_count == 0 ? 2 : 20;
        DOCTEST_CHECK(test_ships::Rows(game::current_state).empty());
        for (int i = 0; i < reserved; ++i) DOCTEST_CHECK(ship_slots::Owner(game::current_state, i) == game::owner_slot{MAX_PLAYERS});
        for (int i = reserved; i < MAX_SHIPS; ++i) DOCTEST_CHECK(ship_slots::Owner(game::current_state, i) == game::owner_slot{(i - reserved) % 2});
    }
}

DOCTEST_TEST_CASE("nonplayer allocation grows through reserved holes without reordering them") {
    TEST_SHIPMOVE::Reset_();
    auto& state = game::current_state;
    test_ships::Rows(state).resize(0);
    ship_slots::Resize(state, 0);
    ship_slots::Reserve(state, 0, game::owner_slot{1});
    ship_slots::Reserve(state, 1, game::owner_slot{2});
    ship_slots::Reserve(state, 2, game::owner_slot{MAX_PLAYERS});
    ship_slots::Reserve(state, 3, game::owner_slot{MAX_PLAYERS});

    const auto key = SHIPMOVE::Create_Nonplayer_Ship(game::owner_slot{MAX_PLAYERS}, nullptr, 0, 0);
    DOCTEST_REQUIRE(key != game::NO_SHIP);
    DOCTEST_REQUIRE(test_ships::Rows(state).size() == 3);
    DOCTEST_CHECK(state.ship_slots.size() == 3);
    DOCTEST_CHECK(state.ship_slots == std::vector{game::NO_SHIP, game::NO_SHIP, key});
    DOCTEST_CHECK(state.ships.size() == 1);
    DOCTEST_CHECK(ship_slots::Owner(state, 0) == game::owner_slot{1});
    DOCTEST_CHECK(ship_slots::Owner(state, 1) == game::owner_slot{2});
    DOCTEST_CHECK(ship_slots::Owner(state, 3) == game::owner_slot{MAX_PLAYERS});
}

DOCTEST_TEST_CASE("whole-turn movement refreshes icons after arrivals and clears idle missions") {
    TEST_SHIPMOVE::Reset_();
    MOX::game_type = 2;
    ship_slots::Resize(game::current_state, 2);
    test_stars::Record(0).just_visited = game::player_bitmap::From_Byte(1);
    test_ships::Rows(game::current_state)[0].status = 1;
    test_ships::Rows(game::current_state)[0].position.Set(legacy_world::Location_From_Slot(game::e_ship_location::in_flight, 1, std::ssize(game::current_state.stars)));
    test_ships::Rows(game::current_state)[0].travelling_speed = 4;
    test_ships::Rows(game::current_state)[0].turns_left = 1;
    test_ships::Rows(game::current_state)[1].mission = 7;
    SHIPMOVE::Move_All_Ships_Toward_Stars();
    DOCTEST_CHECK((game::Ship_Orbit_Row(game::current_state, test_ships::Rows(game::current_state)[0].position) == 1));
    DOCTEST_CHECK((test_ships::Rows(game::current_state)[0].status == 0));
    DOCTEST_CHECK((test_ships::Rows(game::current_state)[1].mission == 0));
    DOCTEST_CHECK((test_stars::Record(0).just_visited.none()));
    DOCTEST_CHECK((test_stars::Record(1).just_visited == game::player_bitmap::From_Byte(1)));
    DOCTEST_CHECK(TEST_SHIPMOVE::icon_refresh_count == 1);
    DOCTEST_CHECK(TEST_SHIPMOVE::icon_refresh_ship_status == 0);
}

DOCTEST_TEST_CASE("Antares portal orders survive ETA updates, arrival and live-star cleanup") {
    TEST_SHIPMOVE::Reset_();
    const auto slot = game::ANTARES_POSITION;
    auto& state = game::current_state;
    state.antares_destination.x = 420;
    state.antares_destination.y = 240;
    ship_slots::Resize(game::current_state, 1);
    const game::id_ship key{90000};
    state.next_ship_id = key.id + 1;
    test_ships::Rows(state).Set_Key_At_Row(0, key);
    auto& ship = test_ships::Rows(state)[0];
    test_stars::Record(0).has_dimensional_portal = game::player_bitmap::From_Byte(1);
    const game::id_ship ships[] = {key};
    const game::s_star_destination destination{game::e_star_destination::antares, game::NO_STAR};
    s_ship_move_info move{};
    DOCTEST_REQUIRE(SHIPMOVE::Apply_Player_Movement_Order(ships, destination, &move));
    DOCTEST_CHECK((ship.travelling_speed == UINT8_MAX));
    SHIPMOVE::Update_Ship_ETAs(game::NO_PLAYER);
    DOCTEST_CHECK((ship.turns_left == 1));

    SHIPMOVE::Move_All_Ships_Toward_Stars();

    DOCTEST_CHECK((game::Ship_Orbit_Row(game::current_state, ship.position) == slot));
    DOCTEST_CHECK((ship.status == 0));
    DOCTEST_CHECK((ship.x == 420));
    DOCTEST_CHECK((ship.y == 240));
    DOCTEST_CHECK_FALSE(state.stars.contains(game::id_star{static_cast<uint32_t>(slot)}));
    test_stars::Record(0).ignore_combat_ships = game::player_bitmap::From_Byte(1);
    SHIPMOVE::Reset_Needless_Ignore_Combat_Ships_Flags();
    DOCTEST_CHECK(test_stars::Record(0).ignore_combat_ships.none());
    lostship::Lose_Out_Of_Range_Ships();
    DOCTEST_CHECK((ship.status == 0));
}

DOCTEST_TEST_CASE("keyed fleet movement preserves departure order across a row reorder") {
    TEST_SHIPMOVE::Reset_();
    auto& state = game::current_state;
    const game::id_ship lead{90001}, escort{90000};
    ship_slots::Resize(state, 2);
    state.next_ship_id = 90002;
    s_ship_data ship{};
    ship.id = lead;
    ship.status = 1;
    ship.x = 30;
    ship.travelling_speed = 7;
    ship.position.Set({game::e_ship_location::in_flight, game::id_star{1}});
    test_ships::Rows(state).Replace_Row(0, ship);
    ship.id = escort;
    ship.travelling_speed = 4;
    test_ships::Rows(state).Replace_Row(1, ship);

    const game::id_ship keys[] = {lead, lead, escort};
    const auto destination = game::Destination_At_Row(state, 1);
    s_ship_move_info before_reorder{};
    DOCTEST_REQUIRE(SHIPMOVE::Evaluate_Fleet_Move(game::current_state, keys, destination, &before_reorder) == SHIPMOVE::e_fleet_evaluation::can_move);
    DOCTEST_CHECK((before_reorder.traveling_speed == 7));

    test_ships::Rows(state).Swap_Rows(0, 1);
    s_ship_move_info keyed{};
    DOCTEST_REQUIRE(SHIPMOVE::Evaluate_Fleet_Move(game::current_state, keys, destination, &keyed) == SHIPMOVE::e_fleet_evaluation::can_move);
    DOCTEST_CHECK((keyed.traveling_speed == before_reorder.traveling_speed));
    DOCTEST_CHECK((keyed.turns_left == before_reorder.turns_left));
    DOCTEST_CHECK((test_ships::Rows(state)[0].travelling_speed == 4));
    DOCTEST_REQUIRE(SHIPMOVE::Order_Fleet_Move(game::current_state, keys, destination, &keyed));
    DOCTEST_CHECK((test_ships::Rows(state)[0].travelling_speed == 7));

    test_ships::Rows(state).Replace_Row(1, s_ship_data{.id = game::id_ship{90002}});
    const auto before = test_ships::Rows(state).Copy_Rows();
    DOCTEST_CHECK(SHIPMOVE::Evaluate_Fleet_Move(game::current_state, keys, destination, &keyed) == SHIPMOVE::e_fleet_evaluation::invalid_input);
    DOCTEST_CHECK_FALSE(SHIPMOVE::Order_Fleet_Move(game::current_state, keys, destination, &before_reorder));
    DOCTEST_CHECK_FALSE(SHIPMOVE::Apply_Player_Movement_Order(keys, destination, &keyed));
    DOCTEST_CHECK(test_ships::Rows(state).Copy_Rows() == before);
}

DOCTEST_TEST_CASE("fleet movement reads and writes the supplied world") {
    TEST_SHIPMOVE::Reset_();
    const auto key = Bind_Test_Ship(0);
    auto detached = std::make_unique<game::s_game_state>(game::current_state);
    const auto source = game::Star_Key_At_Row(*detached, 0);
    const auto target = game::Star_Key_At_Row(*detached, 1);
    detached->stars.at(source).has_stargate.set_slot(0);
    detached->stars.at(target).has_stargate.set_slot(0);
    const auto before = *[&] { const auto entry = game::current_state.ships.find(key); return entry == game::current_state.ships.end() ? nullptr : &entry->second; }();
    const std::array keys{key};
    const game::s_star_destination destination{game::e_star_destination::star, target};
    s_ship_move_info info{};
    DOCTEST_REQUIRE(SHIPMOVE::Evaluate_Fleet_Move(*detached, keys, destination, &info, source) == SHIPMOVE::e_fleet_evaluation::can_move);
    DOCTEST_CHECK((info.has_stargate_route == 1));
    DOCTEST_REQUIRE(SHIPMOVE::Order_Fleet_Move(*detached, keys, destination, &info));
    DOCTEST_CHECK([&] { const auto entry = detached->ships.find(key); return entry == detached->ships.end() ? nullptr : &entry->second; }()->position.star_id.get() == target);
    DOCTEST_CHECK(*[&] { const auto entry = game::current_state.ships.find(key); return entry == game::current_state.ships.end() ? nullptr : &entry->second; }() == before);
}

DOCTEST_TEST_CASE("a player order is cancelled when confirmation replaces the selected ship") {
    TEST_SHIPMOVE::Reset_();
    auto& state = game::current_state;
    ship_slots::Resize(state, 2);
    state.next_ship_id = 90002;
    const game::id_ship key{90000};
    s_ship_data ship{};
    ship.id = key;
    ship.status = 1;
    ship.x = 30;
    ship.travelling_speed = 4;
    ship.position.Set({game::e_ship_location::in_flight, game::id_star{0}});
    test_ships::Rows(state).Replace_Row(0, ship);
    ship.id = game::id_ship{90001};
    ship.position.Set({game::e_ship_location::in_flight, game::id_star{1}});
    // The combined fleet takes longer than this target fleet's current ETA.
    ship.turns_left = 0;
    test_ships::Rows(state).Replace_Row(1, ship);
    TEST_SHIPMOVE::on_confirmation = [] {
        test_ships::Rows(game::current_state).Replace_Row(0, s_ship_data{.id = game::id_ship{90002}});
    };

    const game::id_ship keys[] = {key};
    s_ship_move_info result{};
    DOCTEST_CHECK_FALSE(SHIPMOVE::Apply_Player_Movement_Order(keys, game::Destination_At_Row(state, 1), &result));
    TEST_SHIPMOVE::on_confirmation = nullptr;
    DOCTEST_CHECK(test_ships::Rows(state)[0] == s_ship_data{.id = game::id_ship{90002}});
    DOCTEST_CHECK(test_ships::Rows(state)[1] == ship);
}

DOCTEST_TEST_CASE("captain transfer preview restores keyed ship assignments after a row reorder") {
    TEST_SHIPMOVE::Reset_();
    auto& state = game::current_state;
    ship_slots::Resize(state, 2);
    state.next_ship_id = 90002;
    s_ship_data ship{};
    ship.id = game::id_ship{90000};
    ship.status = 1;
    ship.x = 30;
    ship.travelling_speed = 4;
    ship.officer_index = 0;
    ship.position.Set({game::e_ship_location::in_flight, game::id_star{1}});
    test_ships::Rows(state).Replace_Row(0, ship);
    test_ships::Rows(state).Replace_Row(1, s_ship_data{.officer_index = -1, .id = game::id_ship{90001}});

    auto& leader = state.leaders[0];
    leader.type = LEADER_TYPE_SHIP;
    leader.special_skills = LEADER_SHIP_SKILL_NAVIGATOR;
    game::Set_Leader_Ship(leader, test_ships::Rows(state)[0], 0);
    test_ships::Rows(state).Swap_Rows(0, 1);
    const auto before = leader;
    DOCTEST_REQUIRE(SHIPMOVE::Confirm_Officer_Change(0, &test_ships::Rows(state)[0], 1));
    DOCTEST_CHECK(leader == before);
    DOCTEST_CHECK((test_ships::Rows(state)[0].officer_index == -1));
    DOCTEST_CHECK((test_ships::Rows(state)[1].officer_index == 0));
}

DOCTEST_TEST_CASE("ETA recalculation simulates a moving ship even at maximum travel speed") {
    TEST_SHIPMOVE::Reset_();
    auto& ship = test_ships::Rows(game::current_state)[0];
    ship.status = 2;
    ship.position.Set({game::e_ship_location::in_wormhole, game::id_star{1}});
    ship.travelling_speed = UINT8_MAX;
    ship.turns_left = 1;
    test_stars::Record(1).x = 7800; // More than one simulated step at 255 x 30 units.

    SHIPMOVE::Update_Ship_ETAs(game::NO_PLAYER);
    DOCTEST_CHECK((ship.turns_left == 2));
    DOCTEST_CHECK((ship.x == 0));
}

DOCTEST_TEST_CASE("combat-ignore cleanup keeps flags only while another empire is present") {
    TEST_SHIPMOVE::Reset_();
    game::current_state.player_count = 2;
    test_colonies::Record(0).owner = legacy_world::Decode_Player_Slot(1);
    test_colonies::Record(0).planet_id = game::id_planet{0};
    test_planets::Record(0).star_id = game::id_star{static_cast<uint32_t>(0)};
    test_ships::Rows(game::current_state)[0].position.Set(legacy_world::Location_From_Slot(game::e_ship_location::at_star, 1, std::ssize(game::current_state.stars)));
    test_stars::Record(0).ignore_combat_ships = game::player_bitmap::From_Byte(1);
    test_stars::Record(1).ignore_combat_ships = game::player_bitmap::From_Byte(1);
    SHIPMOVE::Reset_Needless_Ignore_Combat_Ships_Flags();
    DOCTEST_CHECK((test_stars::Record(0).ignore_combat_ships == game::player_bitmap::From_Byte(1)));
    DOCTEST_CHECK((test_stars::Record(1).ignore_combat_ships.none()));
}

DOCTEST_TEST_CASE("hyperspace beasts report the lost ship and officer fate to each player") {
    TEST_SHIPMOVE::Reset_();
    game::current_state.player_count = 2;
    test_ships::Rows(game::current_state)[0].status = 1;
    test_ships::Rows(game::current_state)[0].position.Set(legacy_world::Location_From_Slot(game::e_ship_location::in_flight, 1, std::ssize(game::current_state.stars)));
    test_ships::Rows(game::current_state)[0].officer_index = 0;
    SHIPMOVE::Do_Hyperspace_Beast(game::id_player{0});
    DOCTEST_CHECK(TEST_SHIPMOVE::pooled_officer == 0);
    DOCTEST_CHECK(TEST_SHIPMOVE::killed_officer == 0);
    DOCTEST_CHECK((test_ships::Rows(game::current_state)[0].status == 8));
    DOCTEST_REQUIRE(TEST_SHIPMOVE::messages.size() == 2);
    for (int i = 0; i < 2; ++i) {
        DOCTEST_CHECK(TEST_SHIPMOVE::message_players[i] == i);
        const s_MSG& message = TEST_SHIPMOVE::messages[i];
        DOCTEST_CHECK((message.msg_type == MSG_TYPE_HYPERSPACE_BEAST));
        DOCTEST_CHECK(game::Is_Keyed_Ship_Notice(message.u.hyperspace_beast.ship_idx));
        DOCTEST_CHECK(message.u.hyperspace_beast.ship_id.get() == game::current_state.ship_slots[0]);
        DOCTEST_CHECK(message.u.hyperspace_beast.destination.star_id.get() == game::id_star{1});
        DOCTEST_CHECK((message.u.hyperspace_beast.leader_status == 0));
        DOCTEST_CHECK((message.u.hyperspace_beast.fate == (i == 0 ? 1 : 0)));
    }
}

DOCTEST_TEST_CASE("survival pods transfer a beast victim's officer to a matching travelling ship") {
    TEST_SHIPMOVE::Reset_();
    ship_slots::Resize(game::current_state, 2);
    game::current_state.players[0].tech_applications[TECH_APP_SURVIVAL_PODS] = TECH_RESEARCH_STATUS_RESEARCHED;
    for (int i = 0; i < 2; ++i) {
        test_ships::Rows(game::current_state)[i].owner = factions::From_Legacy(0);
        test_ships::Rows(game::current_state)[i].status = 1;
        test_ships::Rows(game::current_state)[i].position.Set(legacy_world::Location_From_Slot(game::e_ship_location::in_flight, 1, std::ssize(game::current_state.stars)));
        test_ships::Rows(game::current_state)[i].travelling_speed = 4;
        test_ships::Rows(game::current_state)[i].d.ship_type = SHIP_TYPE_COMBAT;
    }
    test_ships::Rows(game::current_state)[1].officer_index = 0;
    SHIPMOVE::Do_Hyperspace_Beast(game::id_player{0});
    DOCTEST_CHECK(TEST_SHIPMOVE::pooled_officer == 0);
    DOCTEST_CHECK(TEST_SHIPMOVE::killed_officer == -1);
    DOCTEST_CHECK((test_ships::Rows(game::current_state)[0].officer_index == 0));
    const auto& leader = game::current_state.leaders[0];
    DOCTEST_CHECK(leader.ship_location == game::current_state.ship_slots[0]);
    DOCTEST_CHECK(leader.status == 1);
    DOCTEST_REQUIRE(TEST_SHIPMOVE::messages.size() == 1);
    DOCTEST_CHECK((TEST_SHIPMOVE::messages[0].u.hyperspace_beast.leader_status == 2));
}

DOCTEST_TEST_CASE("new nonplayer ship lifetime advances Lua slot generation") {
    TEST_SHIPMOVE::Reset_();
    ship_slots::Resize(game::current_state, 0);
    test_ships::Rows(game::current_state).resize(0);
    ship_slots::Reserve(game::current_state, 0, game::owner_slot{MAX_PLAYERS});

    const auto first = SHIPMOVE::Create_Nonplayer_Ship(game::owner_slot{MAX_PLAYERS}, nullptr, 0, 0);
    DOCTEST_REQUIRE(first != game::NO_SHIP);
    DOCTEST_CHECK(game::Ship_Row(game::current_state, first) == 0);
    DOCTEST_CHECK(orion2re::lua::game::Ship_Generation(0) == 1);

    test_ships::Rows(game::current_state)[0].status = 5;
    const auto second = SHIPMOVE::Create_Nonplayer_Ship(game::owner_slot{MAX_PLAYERS}, nullptr, 0, 0);
    DOCTEST_REQUIRE(second != game::NO_SHIP);
    DOCTEST_CHECK(second != first);
    DOCTEST_CHECK(game::Ship_Row(game::current_state, second) == 0);
    DOCTEST_CHECK(orion2re::lua::game::Ship_Generation(0) == 2);
}

DOCTEST_TEST_CASE("movement away from assigned planet clears auto-colonization") {
    TEST_SHIPMOVE::Reset_();
    test_stars::Resize(9);
    test_stars::Reset(9);

    test_planets::Record(70000).star_id = game::id_star{static_cast<uint32_t>(7)};
    test_ships::Rows(game::current_state)[0].d.ship_type = SHIP_TYPE_OUTPOST;
    test_ships::Rows(game::current_state)[0].colonization_target = game::id_planet{70000};

    const game::id_ship ship_list[] = {Bind_Test_Ship(0)};
    s_ship_move_info move_info = {};

    DOCTEST_REQUIRE(SHIPMOVE::Order_Fleet_Move(game::current_state, ship_list, Destination(8), &move_info));

    DOCTEST_CHECK(test_ships::Rows(game::current_state)[0].colonization_target == game::NO_PLANET);
}

DOCTEST_TEST_CASE("in-flight recalculation preserves departure speed during hyperspace flux") {
    TEST_SHIPMOVE::Reset_();
    TEST_SHIPMOVE::Set_Hyperspace_Flux_(true);

    game::current_state.players[0].ship_speed = 8;
    test_ships::Rows(game::current_state)[0].status = 1;
    test_ships::Rows(game::current_state)[0].position.Set(legacy_world::Location_From_Slot(game::e_ship_location::in_flight, 1, std::ssize(game::current_state.stars)));
    test_ships::Rows(game::current_state)[0].x = 30;
    test_ships::Rows(game::current_state)[0].travelling_speed = 4;

    const game::id_ship ship_list[] = {Bind_Test_Ship(0)};
    s_ship_move_info move_info;
    SHIPMOVE::Evaluate_Fleet_Move(game::current_state, ship_list, Destination(1), &move_info);

    DOCTEST_CHECK(move_info.moving == 1);
    DOCTEST_CHECK(move_info.hyperspace_flux == 0);
    DOCTEST_CHECK(move_info.traveling_speed == 4);
    DOCTEST_CHECK(TEST_SHIPMOVE::Movement_Speed_() == 4);
}

DOCTEST_TEST_CASE("in-flight recalculation keeps route after fuel range is lost") {
    TEST_SHIPMOVE::Reset_();

    game::current_state.players[0].ship_range = 0;
    test_ships::Rows(game::current_state)[0].status = 1;
    test_ships::Rows(game::current_state)[0].position.Set(legacy_world::Location_From_Slot(game::e_ship_location::in_flight, 1, std::ssize(game::current_state.stars)));
    test_ships::Rows(game::current_state)[0].x = 30;
    test_ships::Rows(game::current_state)[0].travelling_speed = 4;

    const game::id_ship ship_list[] = {Bind_Test_Ship(0)};
    s_ship_move_info move_info;
    SHIPMOVE::Evaluate_Fleet_Move(game::current_state, ship_list, Destination(1), &move_info);

    DOCTEST_CHECK(move_info.moving == 1);
    DOCTEST_CHECK(move_info.traveling_speed == 4);
}

DOCTEST_TEST_CASE("extended fuel range rounds odd ranges up") {
    TEST_SHIPMOVE::Reset_();

    game::current_state.players[0].ship_range = 9;
    test_stars::Record(1).x = 14 * 30;

    DOCTEST_CHECK_FALSE(SHIPMOVE::Star_In_Normal_Range_Of_Player(game::current_state, test_stars::Record(1), game::id_player{0}));
    DOCTEST_CHECK(SHIPMOVE::Star_In_Extended_Range_Of_Player(game::current_state, test_stars::Record(1), game::id_player{0}));
    DOCTEST_CHECK_FALSE(SHIPMOVE::Star_In_Extended_Range_Of_Player(game::current_state, test_stars::Record(1), game::id_player{65536}));
}

DOCTEST_TEST_CASE("movement stores the numeric navigator bonus") {
    TEST_SHIPMOVE::Reset_();

    auto& leader = game::current_state.leaders[0];
    leader.eta = 0;
    leader.xp = 5;
    leader.special_skills = LEADER_SHIP_SKILL_NAVIGATOR2;
    test_ships::Rows(game::current_state)[0].officer_index = 0;

    const game::id_ship ship_list[] = {Bind_Test_Ship(0)};
    s_ship_move_info move_info;
    SHIPMOVE::Evaluate_Fleet_Move(game::current_state, ship_list, Destination(1), &move_info);
    DOCTEST_REQUIRE(SHIPMOVE::Order_Fleet_Move(game::current_state, ship_list, Destination(1), &move_info));

    DOCTEST_CHECK(move_info.has_navigator == 2);
    DOCTEST_CHECK(test_ships::Rows(game::current_state)[0].group_has_navigator == 2);
    DOCTEST_CHECK(test_ships::Rows(game::current_state)[0].travelling_speed == 6);
}

DOCTEST_TEST_CASE("Phase Shifter scales departing fleet speed using an exact ratio") {
    struct s_case {
        s_ratio16 multiplier;
        uint8_t base_speed;
        bool first_has_phase_shifter;
        bool second_has_phase_shifter;
        int16_t expected_speed;
    };
    const s_case cases[] = {
        {{1, 1}, 4, true, true, 4},
        {{2, 1}, 4, true, true, 8},
        {{1, 2}, 4, true, true, 2},
        {{2, 1}, 4, false, false, 4},
        {{2, 1}, 4, true, false, 4},
        {{2, 1}, 4, false, true, 4},
        {{1, 2}, 4, true, false, 2},
        {{1, 2}, 4, false, true, 2},
        {{1, 2}, 5, true, true, 2},
        {{1, 2}, 1, true, true, 1},
        {{32767, 1}, 255, true, true, 254},
    };

    for (const s_case& test_case : cases) {
        TEST_SHIPMOVE::Reset_();
        SHIPMOVE::phase_shifter_speed_multiplier = test_case.multiplier;
        game::current_state.players[0].ship_speed = test_case.base_speed;
        ship_slots::Resize(game::current_state, 2);
        auto escort = test_ships::Rows(game::current_state)[0];
        escort.id = game::id_ship{1};
        test_ships::Rows(game::current_state).Replace_Row(1, escort);
        const bool equipped[] = {
            test_case.first_has_phase_shifter,
            test_case.second_has_phase_shifter,
        };
        for (int i = 0; i < 2; ++i) {
            if (equipped[i]) {
                test_ships::Rows(game::current_state)[i].d.special_device_flags[SPECIAL_PHASE_SHIFTER / 8] |=
                    1 << (SPECIAL_PHASE_SHIFTER % 8);
            }
        }
        test_ships::Rows(game::current_state)[0].d.combat_speed = 7;

        const game::id_ship ship_list[] = {Bind_Test_Ship(0), Bind_Test_Ship(1)};
        s_ship_move_info move_info;
        SHIPMOVE::Evaluate_Fleet_Move(game::current_state, ship_list, Destination(1), &move_info);

        DOCTEST_CHECK_FALSE(move_info.immobile);
        DOCTEST_CHECK(move_info.traveling_speed == test_case.expected_speed);
        DOCTEST_CHECK(move_info.turns_left == (4 + test_case.expected_speed - 1) / test_case.expected_speed);
        DOCTEST_CHECK(TEST_SHIPMOVE::Movement_Speed_() == test_case.expected_speed);
        DOCTEST_REQUIRE(SHIPMOVE::Order_Fleet_Move(game::current_state, ship_list, Destination(1), &move_info));
        DOCTEST_CHECK(test_ships::Rows(game::current_state)[0].travelling_speed == test_case.expected_speed);
        DOCTEST_CHECK(test_ships::Rows(game::current_state)[1].travelling_speed == test_case.expected_speed);
        DOCTEST_CHECK(test_ships::Rows(game::current_state)[0].d.combat_speed == 7);
    }
    SHIPMOVE::phase_shifter_speed_multiplier = {1, 1};
}

DOCTEST_TEST_CASE("Phase Shifter does not create propulsion or permit nonpositive speed") {
    struct s_case {
        s_ratio16 multiplier;
        uint8_t base_speed;
    };
    const s_case cases[] = {
        {{2, 1}, 0},
        {{0, 1}, 4},
        {{-1, 2}, 4},
    };
    for (const s_case& test_case : cases) {
        TEST_SHIPMOVE::Reset_();
        SHIPMOVE::phase_shifter_speed_multiplier = test_case.multiplier;
        game::current_state.players[0].ship_speed = test_case.base_speed;
        test_ships::Rows(game::current_state)[0].d.special_device_flags[SPECIAL_PHASE_SHIFTER / 8] |=
            1 << (SPECIAL_PHASE_SHIFTER % 8);

        const game::id_ship ship_list[] = {Bind_Test_Ship(0)};
        s_ship_move_info move_info;
        SHIPMOVE::Evaluate_Fleet_Move(game::current_state, ship_list, Destination(1), &move_info);
        DOCTEST_CHECK(move_info.immobile == 1);
        DOCTEST_CHECK(move_info.moving == 0);
        DOCTEST_CHECK(move_info.turns_left == UINT8_MAX);
    }
    SHIPMOVE::phase_shifter_speed_multiplier = {1, 1};
}

DOCTEST_TEST_CASE("Phase Shifter leaves Navigator and Jump Gate bonuses additive") {
    TEST_SHIPMOVE::Reset_();
    SHIPMOVE::phase_shifter_speed_multiplier = {2, 1};
    test_ships::Rows(game::current_state)[0].d.special_device_flags[SPECIAL_PHASE_SHIFTER / 8] |=
        1 << (SPECIAL_PHASE_SHIFTER % 8);
    auto& leader = game::current_state.leaders[0];
    leader.xp = 5;
    leader.special_skills = LEADER_SHIP_SKILL_NAVIGATOR2;
    test_ships::Rows(game::current_state)[0].officer_index = 0;
    test_stars::Record(0).has_jumpgate = game::player_bitmap::From_Byte(1);
    test_stars::Record(1).has_jumpgate = game::player_bitmap::From_Byte(1);

    const game::id_ship ship_list[] = {Bind_Test_Ship(0)};
    s_ship_move_info move_info;
    SHIPMOVE::Evaluate_Fleet_Move(game::current_state, ship_list, Destination(1), &move_info);
    DOCTEST_CHECK(move_info.traveling_speed == 13);
    DOCTEST_CHECK(TEST_SHIPMOVE::Movement_Speed_() == 13);

    game::current_state.players[0].ship_speed = 127;
    SHIPMOVE::Evaluate_Fleet_Move(game::current_state, ship_list, Destination(1), &move_info);
    DOCTEST_CHECK(move_info.traveling_speed == 254);
    DOCTEST_CHECK(TEST_SHIPMOVE::Movement_Speed_() == 254);

    SHIPMOVE::jump_gate_speed_bonus = UINT8_MAX;
    SHIPMOVE::Evaluate_Fleet_Move(game::current_state, ship_list, Destination(1), &move_info);
    DOCTEST_CHECK(move_info.traveling_speed == 254);
    DOCTEST_REQUIRE(SHIPMOVE::Order_Fleet_Move(game::current_state, ship_list, Destination(1), &move_info));
    test_ships::Rows(game::current_state)[0].status = 1;
    test_ships::Rows(game::current_state)[0].position.Set(legacy_world::Location_From_Slot(game::e_ship_location::in_flight, 1, std::ssize(game::current_state.stars)));
    test_ships::Rows(game::current_state)[0].x = 30;
    SHIPMOVE::Evaluate_Fleet_Move(game::current_state, ship_list, Destination(1), &move_info);
    DOCTEST_CHECK(move_info.traveling_speed == 254);
    DOCTEST_CHECK(TEST_SHIPMOVE::Movement_Speed_() == 254);
    SHIPMOVE::jump_gate_speed_bonus = 3;
    SHIPMOVE::phase_shifter_speed_multiplier = {1, 1};
}

DOCTEST_TEST_CASE("Phase Shifter is not reapplied to in-flight departure speed") {
    TEST_SHIPMOVE::Reset_();
    SHIPMOVE::phase_shifter_speed_multiplier = {2, 1};
    test_ships::Rows(game::current_state)[0].d.special_device_flags[SPECIAL_PHASE_SHIFTER / 8] |=
        1 << (SPECIAL_PHASE_SHIFTER % 8);
    test_stars::Record(1).x = 600;
    test_stars::Resize(3);
    test_stars::Record(2).x = 540;

    const game::id_ship ship_list[] = {Bind_Test_Ship(0)};
    s_ship_move_info move_info;
    SHIPMOVE::Evaluate_Fleet_Move(game::current_state, ship_list, Destination(1), &move_info);
    DOCTEST_REQUIRE(move_info.traveling_speed == 8);
    DOCTEST_REQUIRE(SHIPMOVE::Order_Fleet_Move(game::current_state, ship_list, Destination(1), &move_info));
    test_ships::Rows(game::current_state)[0].status = 1;
    test_ships::Rows(game::current_state)[0].position.Set(legacy_world::Location_From_Slot(game::e_ship_location::in_flight, 1, std::ssize(game::current_state.stars)));
    test_ships::Rows(game::current_state)[0].x = 30;
    game::current_state.players[0].ship_speed = 6;

    for (int16_t target : {1, 2}) {
        SHIPMOVE::Evaluate_Fleet_Move(game::current_state, ship_list, Destination(target), &move_info);
        DOCTEST_CHECK(move_info.traveling_speed == 8);
        DOCTEST_CHECK(TEST_SHIPMOVE::Movement_Speed_() == 8);
    }
    SHIPMOVE::phase_shifter_speed_multiplier = {1, 1};
}

DOCTEST_TEST_CASE("Phase Shifter does not change instantaneous travel routes") {
    TEST_SHIPMOVE::Reset_();
    SHIPMOVE::phase_shifter_speed_multiplier = {2, 1};
    test_ships::Rows(game::current_state)[0].d.special_device_flags[SPECIAL_PHASE_SHIFTER / 8] |=
        1 << (SPECIAL_PHASE_SHIFTER % 8);
    test_stars::Record(0).wormhole_star_id = game::id_star{static_cast<uint32_t>(1)};

    const game::id_ship ship_list[] = {Bind_Test_Ship(0)};
    s_ship_move_info move_info;
    SHIPMOVE::Evaluate_Fleet_Move(game::current_state, ship_list, Destination(1), &move_info);
    DOCTEST_CHECK(move_info.uses_wormhole == 1);
    DOCTEST_CHECK(move_info.traveling_speed == UINT8_MAX);
    DOCTEST_CHECK(move_info.turns_left == 1);

    test_stars::Record(0).wormhole_star_id = game::NO_STAR;
    test_stars::Record(0).has_stargate = game::player_bitmap::From_Byte(1);
    test_stars::Record(1).has_stargate = game::player_bitmap::From_Byte(1);
    SHIPMOVE::Evaluate_Fleet_Move(game::current_state, ship_list, Destination(1), &move_info);
    DOCTEST_CHECK(move_info.has_stargate_route == 1);
    DOCTEST_CHECK(move_info.traveling_speed == UINT8_MAX);
    DOCTEST_CHECK(move_info.turns_left == 1);
    SHIPMOVE::phase_shifter_speed_multiplier = {1, 1};
}

DOCTEST_TEST_CASE("jump gate speed preserves the complete unsigned byte domain") {
    struct s_speed_case {
        uint8_t bonus;
        int16_t route_speed;
        uint8_t stored_speed;
    };
    const s_speed_case cases[] = {
        {0, 4, 4},
        {127, 131, 131},
        {128, 132, 132},
        {255, 259, 3},
    };

    for (const s_speed_case& test_case : cases) {
        TEST_SHIPMOVE::Reset_();

        SHIPMOVE::jump_gate_speed_bonus = test_case.bonus;
        SHIPMOVE::jump_gate_pass_nebula = true;
        test_stars::Record(0).has_jumpgate = game::player_bitmap::From_Byte(1);
        test_stars::Record(1).has_jumpgate = game::player_bitmap::From_Byte(1);

        const game::id_ship ship_list[] = {Bind_Test_Ship(0)};
        s_ship_move_info move_info;
        SHIPMOVE::Evaluate_Fleet_Move(game::current_state, ship_list, Destination(1), &move_info);

        DOCTEST_CHECK(move_info.has_jumpgate_route == 1);
        DOCTEST_CHECK(move_info.traveling_speed == test_case.stored_speed);
        DOCTEST_CHECK(TEST_SHIPMOVE::Movement_Speed_() == test_case.route_speed);
        DOCTEST_CHECK_FALSE(TEST_SHIPMOVE::Navigator_Pointer_Supplied_());

        test_ships::Rows(game::current_state)[0].status = 1;
        test_ships::Rows(game::current_state)[0].position.Set(legacy_world::Location_From_Slot(game::e_ship_location::in_flight, 1, std::ssize(game::current_state.stars)));
        test_ships::Rows(game::current_state)[0].x = 30;
        test_ships::Rows(game::current_state)[0].travelling_speed = test_case.stored_speed;
        test_ships::Rows(game::current_state)[0].special_device_damage_flags[0] = 1;
        SHIPMOVE::Evaluate_Fleet_Move(game::current_state, ship_list, Destination(1), &move_info);

        DOCTEST_CHECK(move_info.has_jumpgate_route == 1);
        DOCTEST_CHECK(move_info.traveling_speed == test_case.stored_speed);
        DOCTEST_CHECK(TEST_SHIPMOVE::Movement_Speed_() == test_case.route_speed);

        const auto& state = game::current_state;
        DOCTEST_CHECK(SHIPMOVE::Player_N_Turns_From_Star_To_Star(state, game::id_player{0}, test_stars::Record(0).id, test_stars::Record(1).id) == 1);
        DOCTEST_CHECK(TEST_SHIPMOVE::Movement_Speed_() == test_case.route_speed);
    }

    SHIPMOVE::jump_gate_speed_bonus = 3;
    SHIPMOVE::jump_gate_pass_nebula = false;
}

DOCTEST_TEST_CASE("communications relay ranges use configured parsec limits") {
    TEST_SHIPMOVE::Reset_();

    test_ships::Rows(game::current_state)[0].status = 1;
    test_ships::Rows(game::current_state)[0].x = 90;
    game::current_state.players[0].tech_applications[TECH_APP_TACHYON_COMMUNICATIONS] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    test_stars::Record(0).has_tachyon = game::player_bitmap::From_Byte(1);

    DOCTEST_CHECK(SHIPMOVE::Can_Order_Ship(game::current_state, test_ships::Rows(game::current_state)[0]));

    test_ships::Rows(game::current_state)[0].x = 120;
    DOCTEST_CHECK_FALSE(SHIPMOVE::Can_Order_Ship(game::current_state, test_ships::Rows(game::current_state)[0]));

    test_ships::Rows(game::current_state)[0].x = 180;
    test_stars::Record(0).has_tachyon.reset();
    test_stars::Record(0).has_subspace = game::player_bitmap::From_Byte(1);
    game::current_state.players[0].tech_applications[TECH_APP_SUB_SPACE_COMMUNICATIONS] =
        TECH_RESEARCH_STATUS_RESEARCHED;

    DOCTEST_CHECK(SHIPMOVE::Can_Order_Ship(game::current_state, test_ships::Rows(game::current_state)[0]));

    test_ships::Rows(game::current_state)[0].x = 210;
    DOCTEST_CHECK_FALSE(SHIPMOVE::Can_Order_Ship(game::current_state, test_ships::Rows(game::current_state)[0]));

    SHIPMOVE::ship_communication_range = {3, 6};
}

DOCTEST_TEST_CASE("splitting an in-flight fleet recalculates the remaining ships") {
    TEST_SHIPMOVE::Reset_();

    ship_slots::Resize(game::current_state, 2);
    test_stars::Resize(3);
    test_stars::Record(2).x = 180;
    test_stars::Record(2).y = 0;
    auto& leader = game::current_state.leaders[0];
    leader.eta = 0;
    leader.xp = 5;
    leader.special_skills = LEADER_SHIP_SKILL_NAVIGATOR2;

    for (int16_t i = 0; i < 2; ++i) {
        Bind_Test_Ship(i);
        test_ships::Rows(game::current_state)[i].owner = factions::From_Legacy(0);
        test_ships::Rows(game::current_state)[i].status = 1;
        test_ships::Rows(game::current_state)[i].position.Set({game::e_ship_location::in_flight, game::id_star{1}});
        test_ships::Rows(game::current_state)[i].x = 30;
        test_ships::Rows(game::current_state)[i].y = 0;
        test_ships::Rows(game::current_state)[i].group_has_navigator = 2;
        test_ships::Rows(game::current_state)[i].travelling_speed = 6;
        test_ships::Rows(game::current_state)[i].turns_left = 10;
        test_ships::Rows(game::current_state)[i].officer_index = -1;
    }
    test_ships::Rows(game::current_state)[0].officer_index = 0;

    const game::id_ship ship_list[] = {Bind_Test_Ship(0)};
    s_ship_move_info move_info;
    SHIPMOVE::Evaluate_Fleet_Move(game::current_state, ship_list, Destination(2), &move_info);

    DOCTEST_REQUIRE(SHIPMOVE::Apply_Player_Movement_Order(ship_list, Destination(2), &move_info));
    DOCTEST_CHECK(SHIPMOVE::Ship_Destination_Star(&test_ships::Rows(game::current_state)[0]) == 2);
    DOCTEST_CHECK(test_ships::Rows(game::current_state)[0].travelling_speed == 6);
    DOCTEST_CHECK(SHIPMOVE::Ship_Destination_Star(&test_ships::Rows(game::current_state)[1]) == 1);
    DOCTEST_CHECK(test_ships::Rows(game::current_state)[1].group_has_navigator == 0);
    DOCTEST_CHECK(test_ships::Rows(game::current_state)[1].travelling_speed == 4);
}

DOCTEST_TEST_CASE("joining an in-flight fleet updates only the ships on its route") {
    TEST_SHIPMOVE::Reset_();

    ship_slots::Resize(game::current_state, 4);
    test_stars::Resize(3);
    test_stars::Record(2).x = 180;
    for (int16_t i = 0; i < static_cast<int16_t>(game::current_state.ship_slots.size()); ++i) {
        Bind_Test_Ship(i);
        s_ship_data& ship = test_ships::Rows(game::current_state)[i];
        ship.owner = factions::From_Legacy(0);
        ship.officer_index = -1;
        ship.x = 30;
        ship.y = 0;
    }
    for (int16_t i = 1; i < static_cast<int16_t>(game::current_state.ship_slots.size()); ++i) {
        test_ships::Rows(game::current_state)[i].status = 1;
        test_ships::Rows(game::current_state)[i].position.Set({game::e_ship_location::in_flight, game::id_star{i == 3 ? 1u : 2u}});
        test_ships::Rows(game::current_state)[i].travelling_speed = 4;
    }

    const game::id_ship ship_list[] = {Bind_Test_Ship(0)};
    s_ship_move_info move_info = {};
    DOCTEST_REQUIRE(SHIPMOVE::Apply_Player_Movement_Order(ship_list, Destination(2), &move_info));
    DOCTEST_CHECK(SHIPMOVE::Ship_Destination_Star(&test_ships::Rows(game::current_state)[0]) == 2);
    DOCTEST_CHECK(SHIPMOVE::Ship_Destination_Star(&test_ships::Rows(game::current_state)[1]) == 2);
    DOCTEST_CHECK(SHIPMOVE::Ship_Destination_Star(&test_ships::Rows(game::current_state)[2]) == 2);
    DOCTEST_CHECK(SHIPMOVE::Ship_Destination_Star(&test_ships::Rows(game::current_state)[3]) == 1);
}

DOCTEST_TEST_CASE("complete in-flight fleet can return to its prior jump gate destination once in the same turn") {
    TEST_SHIPMOVE::Reset_();

    ship_slots::Resize(game::current_state, 2);
    test_stars::Resize(3);
    test_stars::Record(1).x = 120;
    test_stars::Record(2).x = 240;
    for (int16_t i = 0; i < static_cast<int16_t>(game::current_state.ship_slots.size()); ++i) {
        s_ship_data& ship = test_ships::Rows(game::current_state)[i];
        ship.owner = factions::From_Legacy(0);
        ship.status = 1;
        ship.position.Set({game::e_ship_location::in_flight, game::id_star{1}});
        ship.x = 30;
        ship.travelling_speed = 7;
        ship.officer_index = -1;
        ship.special_device_damage_flags[0] = 1;
    }

    const game::id_ship complete_fleet[] = {Bind_Test_Ship(0), Bind_Test_Ship(1)};
    s_ship_move_info move_info;
    DOCTEST_REQUIRE(SHIPMOVE::Apply_Player_Movement_Order(complete_fleet, Destination(2), &move_info));
    DOCTEST_CHECK(move_info.has_jumpgate_route == 0);
    DOCTEST_CHECK(test_ships::Rows(game::current_state)[0].prior_jump_gate_destination == game::id_star{1});
    DOCTEST_CHECK(test_ships::Rows(game::current_state)[1].prior_jump_gate_destination == game::id_star{1});

    DOCTEST_REQUIRE(SHIPMOVE::Apply_Player_Movement_Order(complete_fleet, Destination(1), &move_info));
    DOCTEST_CHECK(move_info.has_jumpgate_route == 1);
    DOCTEST_CHECK(move_info.traveling_speed == 7);
    DOCTEST_CHECK(test_ships::Rows(game::current_state)[0].prior_jump_gate_destination == game::NO_STAR);
    DOCTEST_CHECK(test_ships::Rows(game::current_state)[1].prior_jump_gate_destination == game::NO_STAR);
}

DOCTEST_TEST_CASE("prior jump gate destination does not grant a partial fleet or survive the turn") {
    TEST_SHIPMOVE::Reset_();

    ship_slots::Resize(game::current_state, 2);
    test_stars::Resize(3);
    test_stars::Record(1).x = 120;
    test_stars::Record(2).x = 240;
    for (int16_t i = 0; i < static_cast<int16_t>(game::current_state.ship_slots.size()); ++i) {
        s_ship_data& ship = test_ships::Rows(game::current_state)[i];
        ship.owner = factions::From_Legacy(0);
        ship.status = 1;
        ship.position.Set({game::e_ship_location::in_flight, game::id_star{2}});
        ship.x = 30;
        ship.travelling_speed = 4;
        ship.officer_index = -1;
        ship.prior_jump_gate_destination = game::id_star{1};
        ship.prior_jump_gate_stardate = MOX::stardate;
    }

    const game::id_ship partial_fleet[] = {Bind_Test_Ship(0)};
    s_ship_move_info move_info;
    SHIPMOVE::Evaluate_Fleet_Move(game::current_state, partial_fleet, Destination(1), &move_info);
    DOCTEST_CHECK(move_info.has_jumpgate_route == 0);
    DOCTEST_CHECK(move_info.traveling_speed == 4);

    DOCTEST_REQUIRE(SHIPMOVE::Apply_Player_Movement_Order(partial_fleet, Destination(1), &move_info));
    DOCTEST_CHECK(move_info.has_jumpgate_route == 0);
    DOCTEST_CHECK(test_ships::Rows(game::current_state)[0].prior_jump_gate_destination == game::NO_STAR);
    DOCTEST_CHECK(test_ships::Rows(game::current_state)[1].prior_jump_gate_destination == game::NO_STAR);

    const game::id_ship complete_fleet[] = {partial_fleet[0], Bind_Test_Ship(1)};
    for (int16_t i = 0; i < static_cast<int16_t>(game::current_state.ship_slots.size()); ++i) {
        test_ships::Rows(game::current_state)[i].status = 1;
        test_ships::Rows(game::current_state)[i].position.Set({game::e_ship_location::in_flight, game::id_star{2}});
        test_ships::Rows(game::current_state)[i].x = 30;
        test_ships::Rows(game::current_state)[i].prior_jump_gate_destination = game::id_star{1};
        test_ships::Rows(game::current_state)[i].prior_jump_gate_stardate = MOX::stardate;
    }
    MOX::stardate++;
    SHIPMOVE::Evaluate_Fleet_Move(game::current_state, complete_fleet, Destination(1), &move_info);
    DOCTEST_CHECK(move_info.has_jumpgate_route == 0);
    DOCTEST_CHECK(move_info.traveling_speed == 4);
}

DOCTEST_TEST_CASE("unstable wormhole accepts a fleet with one turn remaining") {
    TEST_SHIPMOVE::Reset_();
    test_ships::Rows(game::current_state)[0].status = 1;
    test_ships::Rows(game::current_state)[0].turns_left = 1;
    test_ships::Rows(game::current_state)[0].position.Set({game::e_ship_location::in_flight, game::id_star{1}});

    s_wormhole_data result = {};
    SHIPMOVE::Do_Wormhole_Event(game::id_player{0}, &result);

    DOCTEST_CHECK(result.success == 1);
    DOCTEST_CHECK(test_ships::Rows(game::current_state)[0].status == 0);
    DOCTEST_CHECK((test_ships::Rows(game::current_state)[0].position.Get() == game::s_ship_location{game::e_ship_location::at_star, game::id_star{1}}));
}

DOCTEST_TEST_CASE("unstable wormhole excludes a fleet already arriving") {
    TEST_SHIPMOVE::Reset_();
    test_ships::Rows(game::current_state)[0].status = 1;
    test_ships::Rows(game::current_state)[0].turns_left = 0;
    test_ships::Rows(game::current_state)[0].position.Set({game::e_ship_location::in_flight, game::id_star{1}});

    s_wormhole_data result = {};
    SHIPMOVE::Do_Wormhole_Event(game::id_player{0}, &result);

    DOCTEST_CHECK(result.success == 0);
    DOCTEST_CHECK(test_ships::Rows(game::current_state)[0].status == 1);
}

DOCTEST_TEST_CASE("unstable wormhole can select Antares but skips stale star keys") {
    TEST_SHIPMOVE::Reset_();
    ship_slots::Resize(game::current_state, 3);
    const auto valid = game::s_ship_location{game::e_ship_location::in_flight, game::id_star{1}};
    const auto antares = game::s_ship_location{game::e_ship_location::in_flight, game::NO_STAR, game::e_ship_target::antares};
    const auto missing = game::s_ship_location{game::e_ship_location::in_flight, game::id_star{90000}};
    const game::s_ship_location destinations[] = {valid, antares, missing};
    for (int i = 0; i < static_cast<int16_t>(game::current_state.ship_slots.size()); ++i) {
        auto& ship = test_ships::Rows(game::current_state)[i];
        ship.owner = factions::From_Legacy(0);
        ship.officer_index = -1;
        ship.status = 1;
        ship.turns_left = 1;
        ship.position.Set(destinations[i]);
    }

    s_wormhole_data result{};
    SHIPMOVE::Do_Wormhole_Event(game::id_player{0}, &result);
    DOCTEST_CHECK(result.success == 1);
    DOCTEST_CHECK((result.ships_moved == 1));
    DOCTEST_CHECK(result.destination.kind == game::e_star_destination::antares);
    DOCTEST_CHECK((test_ships::Rows(game::current_state)[0].status == 1));
    DOCTEST_CHECK((test_ships::Rows(game::current_state)[1].status == 0));
    DOCTEST_CHECK(game::Ship_Orbit_Row(game::current_state, test_ships::Rows(game::current_state)[1].position) == game::ANTARES_POSITION);
    DOCTEST_CHECK(test_ships::Rows(game::current_state)[2].position.Get() == missing);

    DOCTEST_REQUIRE(test_ships::Rows(game::current_state)[0].position.Get() == valid);
    SHIPMOVE::Do_Wormhole_Event(game::id_player{0}, &result);
    DOCTEST_CHECK(result.success == 1);
    DOCTEST_CHECK(result.destination.star_id.get() == valid.star_id);
    DOCTEST_CHECK((test_ships::Rows(game::current_state)[0].status == 0));

    SHIPMOVE::Do_Wormhole_Event(game::id_player{0}, &result);
    DOCTEST_CHECK(result.success == 0);
    DOCTEST_CHECK((test_ships::Rows(game::current_state)[2].status == 1));
}
