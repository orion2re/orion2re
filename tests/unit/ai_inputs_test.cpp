#include "pch.h"
#include "../fixtures/ship_rows.h"
#include "game/fleet_movement.h"
#include "game/legacy_ship_location_codec.h"
#include "../fixtures/star_roster.h"

#include "doctest/doctest.h"
#include "game/legacy_ids.h"
#include "game/ship_ids.h"
#include <type_traits>

using game::id_ship;
using legacy_world::Ship_Id;
using legacy_world::Star_Id;
using legacy_world::Player_Id;

static_assert(std::is_aggregate_v<game::id_ship>);
static_assert(std::is_aggregate_v<game::id_colony>);
static_assert(std::is_aggregate_v<game::id_planet>);
static_assert(std::is_aggregate_v<game::id_star>);
static_assert(std::is_aggregate_v<game::id_player>);
static_assert(std::is_aggregate_v<game::id_leader>);
static_assert(!std::is_convertible_v<uint32_t, id_ship>);
static_assert(!std::is_convertible_v<id_ship, uint32_t>);
static_assert(!std::is_convertible_v<id_ship, game::id_colony>);
static_assert(!std::is_convertible_v<game::id_planet, game::id_star>);
static_assert(!std::is_convertible_v<game::id_player, game::id_leader>);
static_assert(!std::is_invocable_v<decltype(&AIPOWER::Max_Ship_Hits),
    game::s_game_state*, game::id_colony>);
using t_star_routes = AIDATA::s_direct_route_table<game::id_star>;

template<typename Source>
concept Can_Store_Star_Route = requires(t_star_routes& routes, Source source,
    game::id_star destination, game::id_player player, uint8_t turns) {
    routes.Store(source, destination, player, turns);
};

static_assert(Can_Store_Star_Route<game::id_star>);
static_assert(!Can_Store_Star_Route<id_ship>);

DOCTEST_TEST_CASE_TEMPLATE("ID structs provide value equality and preserve integer hashing", t_id,
    game::id_ship, game::id_colony, game::id_planet,
    game::id_star, game::id_player, game::id_leader) {
    constexpr t_id zero{};
    static_assert(zero.id == 0);
    static_assert(t_id{7} == t_id{7});
    static_assert(t_id{7} != t_id{8});

    for (uint32_t value : {0u, 7u, 0x80000000u, UINT32_MAX}) {
        const t_id original{value};
        t_id copy = original;
        DOCTEST_CHECK(copy == original);
        DOCTEST_CHECK(copy.id == value);
        DOCTEST_CHECK(std::hash<t_id>{}(copy) == std::hash<uint32_t>{}(value));

        std::unordered_map<t_id, int> entries;
        entries.emplace(original, 42);
        DOCTEST_CHECK(entries.at(copy) == 42);
        copy.id ^= 1u;
        DOCTEST_CHECK(copy != original);
        DOCTEST_CHECK(entries.find(copy) == entries.end());
        DOCTEST_CHECK(original.id == value);
    }
}

namespace {
    auto legacy_ships = []() {
        auto rows = test_ships::Rows(game::current_state);
        rows.resize(2);
        return rows;
    }();
    test_stars::Slots<2> legacy_stars;
    std::vector<id_ship> queried_ships;
    std::vector<int16_t> queried_locations;
    int16_t queried_destination;
    bool queried;
    std::optional<int16_t> proposed_origin;

    void Reset_Movement_Ships() {
        auto& state = game::current_state;
        legacy_ships.assign(2, s_ship_data{});
        ship_slots::Resize(state, 2);
        state.next_ship_id = 2;
        legacy_ships.Set_Key_At_Row(0, id_ship{0});
        legacy_ships.Set_Key_At_Row(1, id_ship{1});
    }
} // namespace

namespace MOX {

} // namespace MOX

namespace SHIPMOVE {
    e_fleet_evaluation Evaluate_Fleet_Move(game::s_game_state& view, std::span<const game::id_ship> keys,
        game::s_star_destination destination, s_ship_move_info* result, const game::s_planning_context&, std::optional<game::id_star> origin) {
        return Evaluate_Fleet_Move(view, keys, destination, result, origin);
    }
    // Probe the movement service without linking its simulation dependencies.
    e_fleet_evaluation Evaluate_Fleet_Move(game::s_game_state& view, std::span<const game::id_ship> keys, game::s_star_destination destination,
        s_ship_move_info* result, std::optional<game::id_star> origin) {
        if (!result) return e_fleet_evaluation::invalid_input;
        *result = {};
        const auto target = game::Destination_Row(view, destination);
        const auto source = origin ? game::Star_Row(view, *origin) : -1;
        if (target < 0 || (origin && source < 0) || keys.empty()) return e_fleet_evaluation::invalid_input;
        std::vector<int16_t> locations;
        for (const auto key : keys) {
            const auto ship_entry = view.ships.find(key);
            const auto* ship = ship_entry == view.ships.end() ? nullptr : &ship_entry->second;
            if (!ship) return e_fleet_evaluation::invalid_input;
            locations.push_back(game::Ship_Orbit_Row(view, ship->position));
        }
        queried = true;
        queried_ships.assign(keys.begin(), keys.end());
        queried_locations = std::move(locations);
        queried_destination = target;
        proposed_origin = origin ? std::optional<int16_t>{static_cast<int16_t>(source)} : std::nullopt;
        result->moving = 1;
        result->turns_left = 3;
        return e_fleet_evaluation::can_move;
    }

    bool Order_Fleet_Move(game::s_game_state& view, std::span<const game::id_ship> keys, game::s_star_destination destination, const s_ship_move_info* result) {
        s_ship_move_info probe{};
        return result && Evaluate_Fleet_Move(view, keys, destination, &probe) == e_fleet_evaluation::can_move;
    }
} // namespace SHIPMOVE

DOCTEST_TEST_CASE("AI turn reset does not own or reset world inputs") {
    game::s_game_state state;
    state.player_count = 1;
    auto& world = state;
    AIDATA::s_state first;
    AIDATA::s_state second;
    first.players[0].build.transports.current = 3;
    first.ships[Ship_Id(UINT32_MAX)].refit_desirability = 9;

    AIDATA::Begin_Turn(&first, 42);
    DOCTEST_CHECK(first.ships.empty());
    DOCTEST_CHECK(first.players[0].build.transports.current == 0);
    state.player_count = 2;
    DOCTEST_CHECK(world.player_count == 2);
    DOCTEST_CHECK_FALSE(second.initialized);
}

DOCTEST_TEST_CASE("ship views use the records and count of their owning state") {
    game::s_game_state first;
    game::s_game_state second;
    test_ships::Rows(first).resize(1);
    test_ships::Rows(second).resize(2);
    ship_slots::Resize(first, 1);
    ship_slots::Resize(second, 2);
    test_ships::Rows(first)[0].owner = factions::From_Legacy(1);
    test_ships::Rows(second)[0].owner = factions::From_Legacy(2);

    auto& first_view = first;
    auto& second_view = second;
    DOCTEST_CHECK(first_view.ship_slots.size() == 1);
    DOCTEST_CHECK(second_view.ship_slots.size() == 2);
    DOCTEST_CHECK(factions::Legacy_Code(test_ships::Rows(first_view)[0].owner) == 1);
    DOCTEST_CHECK(factions::Legacy_Code(test_ships::Rows(second_view)[0].owner) == 2);
    test_ships::Rows(first_view)[0].owner = factions::From_Legacy(3);
    DOCTEST_CHECK(factions::Legacy_Code(test_ships::Rows(first)[0].owner) == 3);
    DOCTEST_CHECK(factions::Legacy_Code(test_ships::Rows(second)[0].owner) == 2);
}

DOCTEST_TEST_CASE("typed ID compatibility preserves zero and rejects absent or unrepresentable indices") {
    using namespace legacy_world;
    for (int32_t absent : {-1, -2}) {
        DOCTEST_CHECK_FALSE(Try_Ship_Id(absent));
        DOCTEST_CHECK_FALSE(Try_Player_Id(absent));
        DOCTEST_CHECK_FALSE(Try_Leader_Id(absent));
    }
    for (int16_t index : {int16_t{0}, int16_t{INT16_MAX}}) {
        DOCTEST_CHECK(Try_Ship_Id(index)->id == index);
        DOCTEST_CHECK(Try_Player_Id(index)->id == index);
        DOCTEST_CHECK(Try_Leader_Id(index)->id == index);
    }
    for (uint32_t value : {uint32_t{INT16_MAX} + 1, UINT32_MAX}) {
        DOCTEST_CHECK(Slot(Ship_Id(value)) == value);
        DOCTEST_CHECK(Star_Id(value).id == value);
        DOCTEST_CHECK(Slot(Player_Id(value)) == value);
        DOCTEST_CHECK(Slot(Leader_Id(value)) == value);
    }
}

DOCTEST_TEST_CASE("read-only bit fields have a distinct const-correct interface") {
    const uint8_t bits[] = {1, 0x80};
    DOCTEST_CHECK(struct_::Read_Bit_Field(bits, 0) == 1);
    DOCTEST_CHECK(struct_::Read_Bit_Field(bits, 1) == 0);
    DOCTEST_CHECK(struct_::Read_Bit_Field(bits, 15) == 1);
}

DOCTEST_TEST_CASE("fleet evaluation passes a proposed origin without modifying repeated ship IDs") {
    test_stars::Reset(2);
    Reset_Movement_Ships();
    legacy_ships[0].x = legacy_ships[1].x = 10;
    legacy_ships[0].y = legacy_ships[1].y = 20;
    legacy_stars[1].x = 100;
    legacy_stars[1].y = 200;
    const id_ship ids[] = {Ship_Id(0), Ship_Id(0), Ship_Id(1)};
    s_ship_move_info result{};
    proposed_origin.reset();
    DOCTEST_CHECK(SHIPMOVE::Evaluate_Fleet_Move(game::current_state, ids, {game::e_star_destination::star, Star_Id(0)}, &result, Star_Id(1)) == SHIPMOVE::e_fleet_evaluation::can_move);
    DOCTEST_CHECK(queried_ships == std::vector<id_ship>{Ship_Id(0), Ship_Id(0), Ship_Id(1)});
    DOCTEST_CHECK(queried_locations == std::vector<int16_t>{0, 0, 0});
    DOCTEST_CHECK(proposed_origin == 1);
    DOCTEST_CHECK(queried_destination == 0);
    for (size_t row = 0; row < legacy_ships.size(); ++row) {
        const auto& ship = legacy_ships[row];
        DOCTEST_CHECK(game::Ship_Orbit_Row(game::current_state, ship.position) == 0);
        DOCTEST_CHECK((int16_t)ship.x == 10);
        DOCTEST_CHECK((int16_t)ship.y == 20);
    }
    queried = false;
    const id_ship invalid[] = {Ship_Id(UINT32_MAX)};
    SHIPMOVE::Evaluate_Fleet_Move(game::current_state, invalid, {game::e_star_destination::star, Star_Id(0)}, &result);
    DOCTEST_CHECK_FALSE(queried);
    DOCTEST_CHECK(result.moving == 0);
    queried = false;
    SHIPMOVE::Evaluate_Fleet_Move(game::current_state, {}, {game::e_star_destination::star, Star_Id(0)}, &result);
    DOCTEST_CHECK_FALSE(queried);
    const id_ship nonexistent[] = {Ship_Id(2)};
    DOCTEST_CHECK(SHIPMOVE::Evaluate_Fleet_Move(game::current_state, nonexistent, {game::e_star_destination::star, Star_Id(0)}, &result) == SHIPMOVE::e_fleet_evaluation::invalid_input);
    DOCTEST_CHECK_FALSE(queried);
}

DOCTEST_TEST_CASE("fleet collection preserves cached order and mission filtering") {
    s_ship_data ships[2] = {};
    ships[0].id = id_ship{0};
    ships[1].id = id_ship{1};
    ships[1].mission = 5;
    s_player player{};
    player.offensive_stagepoint = game::NO_STAR;
    game::s_game_state state;
    game::Create_Star(state);
    test_ships::Rows(state).assign(std::begin(ships), std::end(ships));
    ship_slots::Resize(state, 2);
    state.next_ship_id = 2;
    auto& world = state;
    AIDATA::s_state ai;
    ai.combat_ships = {Ship_Id(0), Ship_Id(1)};
    ai.players[0].combat_ship_groups[Star_Id(0)] = {0, 2};
    AIMOVE::s_ai_movement_context context{Player_Id(0), &player, nullptr, &world};
    std::vector<id_ship> collected{Ship_Id(UINT32_MAX)};

    AIMOVE::Gather_Player_Ships_At_Star(&ai, &context, Player_Id(0), Star_Id(0), &collected);
    DOCTEST_CHECK(collected == std::vector<id_ship>{Ship_Id(0)});
    player.offensive_stagepoint = game::Star_Key_At_Slot(state, 0);
    DOCTEST_REQUIRE(game::Create_Star(state));
    std::reverse(state.star_order.begin(), state.star_order.end());
    state.star_order_sorted = false;
    AIMOVE::Gather_Player_Ships_At_Star(&ai, &context, Player_Id(0), Star_Id(0), &collected);
    DOCTEST_CHECK(collected == std::vector<id_ship>{Ship_Id(0), Ship_Id(1)});
    test_ships::Rows(state)[1].owner = factions::From_Legacy(1);
    AIMOVE::Gather_Player_Ships_At_Star(&ai, &context, Player_Id(0), Star_Id(0), &collected);
    DOCTEST_CHECK(collected == std::vector<id_ship>{Ship_Id(0)});
    AIMOVE::Gather_Player_Ships_At_Star(&ai, &context, Player_Id(0), Star_Id(1), &collected);
    DOCTEST_CHECK(collected.empty());
}

DOCTEST_TEST_CASE("fleet planning returns the evaluated direct route") {
    test_stars::Reset(2);
    Reset_Movement_Ships();
    auto& world = game::current_state;
    AIMOVE::s_ai_movement_context context{Player_Id(0), nullptr, nullptr, &world};
    const id_ship ids[] = {Ship_Id(0)};
    queried = false;
    const auto route = AIMOVE::Move_Ships_With_Possible_Intermediate(&context, ids, Star_Id(1));
    DOCTEST_REQUIRE(route);
    DOCTEST_CHECK(queried);
    DOCTEST_CHECK(queried_ships == std::vector<id_ship>{Ship_Id(0)});
    DOCTEST_CHECK(queried_destination == 1);
    DOCTEST_CHECK(route->next_star == Star_Id(1));
    DOCTEST_CHECK(route->turns == 3);
}

DOCTEST_TEST_CASE("movement planning rejects destinations unsupported by the legacy engine") {
    game::s_game_state state;
    test_ships::Rows(state).assign(1, s_ship_data{.id = id_ship{0}});
    ship_slots::Resize(state, 1);
    state.next_ship_id = 1;
    auto& world = state;
    AIMOVE::s_ai_movement_context context{Player_Id(0), nullptr, nullptr, &world};
    const id_ship ids[] = {Ship_Id(0)};
    queried = false;
    auto route = AIMOVE::Move_Ships_With_Possible_Intermediate(&context, ids, Star_Id(UINT32_MAX));
    DOCTEST_CHECK_FALSE(route);
    DOCTEST_CHECK_FALSE(queried);
}

DOCTEST_TEST_CASE("fleet evaluation and ordering share native request validation") {
    test_stars::Reset(std::ssize(game::current_state.stars));
    Reset_Movement_Ships();
    const id_ship ships[] = {Ship_Id(0)};
    const id_ship missing[] = {Ship_Id(static_cast<int16_t>(game::current_state.ship_slots.size()))};
    s_ship_move_info result{};
    queried = false;
    DOCTEST_CHECK(SHIPMOVE::Evaluate_Fleet_Move(game::current_state, missing, {game::e_star_destination::star, Star_Id(0)}, &result) == SHIPMOVE::e_fleet_evaluation::invalid_input);
    DOCTEST_CHECK_FALSE(SHIPMOVE::Order_Fleet_Move(game::current_state, missing, {game::e_star_destination::star, Star_Id(0)}, &result));
    DOCTEST_CHECK_FALSE(queried);
    const auto outside = Star_Id(std::ssize(game::current_state.stars) + 1);
    DOCTEST_CHECK(SHIPMOVE::Evaluate_Fleet_Move(game::current_state, ships, {game::e_star_destination::star, outside}, &result) == SHIPMOVE::e_fleet_evaluation::invalid_input);
    DOCTEST_CHECK_FALSE(SHIPMOVE::Order_Fleet_Move(game::current_state, ships, {game::e_star_destination::star, outside}, &result));
    DOCTEST_CHECK_FALSE(queried);
    DOCTEST_CHECK(SHIPMOVE::Evaluate_Fleet_Move(game::current_state, ships, {game::e_star_destination::star, Star_Id(0)}, &result, Star_Id(std::ssize(game::current_state.stars))) == SHIPMOVE::e_fleet_evaluation::invalid_input);
    DOCTEST_CHECK_FALSE(queried);
    // Antares is not a star key; the movement service takes a distinct destination tag.
    DOCTEST_CHECK(SHIPMOVE::Evaluate_Fleet_Move(game::current_state, ships, {game::e_star_destination::star, Star_Id(std::ssize(game::current_state.stars))}, &result) == SHIPMOVE::e_fleet_evaluation::invalid_input);
    DOCTEST_CHECK_FALSE(SHIPMOVE::Order_Fleet_Move(game::current_state, ships, {game::e_star_destination::star, Star_Id(std::ssize(game::current_state.stars))}, &result));
    const game::s_star_destination antares{game::e_star_destination::antares, game::NO_STAR};
    DOCTEST_CHECK(SHIPMOVE::Evaluate_Fleet_Move(game::current_state, ships, antares, &result) == SHIPMOVE::e_fleet_evaluation::can_move);
    DOCTEST_CHECK(SHIPMOVE::Order_Fleet_Move(game::current_state, ships, antares, &result));

    auto& state = game::current_state;
    state.next_star_id = 90000;
    const auto key = game::Replace_Draft_Star(state, 0);
    DOCTEST_REQUIRE(key == game::id_star{90000});
    DOCTEST_CHECK(SHIPMOVE::Evaluate_Fleet_Move(game::current_state, ships, {game::e_star_destination::star, *key}, &result) == SHIPMOVE::e_fleet_evaluation::can_move);
    DOCTEST_CHECK(queried_destination == 0);
}

DOCTEST_TEST_CASE("AI movement rejects wide IDs rather than aliasing legacy index zero") {
    Reset_Movement_Ships();
    test_stars::Reset(2);
    const id_ship valid[] = {Ship_Id(0)};
    s_ship_move_info result{};
    for (uint32_t id : {uint32_t{INT16_MAX} + 1, uint32_t{1} << 16, UINT32_MAX}) {
        const id_ship invalid[] = {Ship_Id(id)};
        queried = false;
        result.moving = 1;
        DOCTEST_CHECK(SHIPMOVE::Evaluate_Fleet_Move(game::current_state, invalid, {game::e_star_destination::star, Star_Id(0)}, &result) == SHIPMOVE::e_fleet_evaluation::invalid_input);
        DOCTEST_CHECK(result.moving == 0);
        DOCTEST_CHECK_FALSE(SHIPMOVE::Order_Fleet_Move(game::current_state, invalid, {game::e_star_destination::star, Star_Id(0)}, &result));
        DOCTEST_CHECK(SHIPMOVE::Evaluate_Fleet_Move(game::current_state, valid, {game::e_star_destination::star, Star_Id(id)}, &result) == SHIPMOVE::e_fleet_evaluation::invalid_input);
        DOCTEST_CHECK_FALSE(SHIPMOVE::Order_Fleet_Move(game::current_state, valid, {game::e_star_destination::star, Star_Id(id)}, &result));
        DOCTEST_CHECK(SHIPMOVE::Evaluate_Fleet_Move(game::current_state, valid, {game::e_star_destination::star, Star_Id(0)}, &result, Star_Id(id)) == SHIPMOVE::e_fleet_evaluation::invalid_input);
        DOCTEST_CHECK_FALSE(queried);

        auto& world = game::current_state;
        AIMOVE::s_ai_movement_context context{Player_Id(0), nullptr, nullptr, &world};
        DOCTEST_CHECK_FALSE(AIMOVE::Move_Ships_With_Possible_Intermediate(&context, invalid, Star_Id(0)));
        DOCTEST_CHECK_FALSE(queried);
    }
}
