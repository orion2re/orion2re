#include "pch.h"

#include "doctest/doctest.h"
#include "game/legacy_world.h"
#include "game/ai_compat.h"
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
    const game::s_view*, game::id_colony>);
using t_star_routes = AIDATA::s_direct_route_table<game::id_star>;
static_assert(!std::is_invocable_v<decltype(&t_star_routes::Store), t_star_routes*,
    id_ship, game::id_star, game::id_player, uint8_t>);

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
    s_ship_data legacy_ships[2];
    s_star_data legacy_stars[2];
    std::vector<int16_t> queried_ships;
    std::vector<int16_t> queried_locations;
    int16_t queried_destination;
    bool queried;
    std::optional<int16_t> proposed_origin;
} // namespace

namespace MOX {
    s_ship_data* ship = legacy_ships;
    s_star_data* star = legacy_stars;
    int16_t NUM_SHIPS = 2;
    int16_t NUM_STARS = 2;
} // namespace MOX

namespace SHIPMOVE {
    void __cdecl Ships_Try_To_Move_To(int16_t* ships, int16_t destination, s_ship_move_info* result) {
        queried = true;
        queried_ships.clear();
        queried_locations.clear();
        queried_destination = destination;
        for (int32_t i = 0; ships[i] != -1; ++i) {
            queried_ships.push_back(ships[i]);
            queried_locations.push_back((int16_t)MOX::ship[ships[i]].location);
        }
        *result = {};
        result->moving = 1;
        result->turns_left = 3;
    }

    void __cdecl Make_Ships_Move_To(::game::s_world_state* world_state, int16_t* ships, int16_t destination, s_ship_move_info* result) {
        Ships_Try_To_Move_To(ships, destination, result);
    }

    void Evaluate_Fleet_Move_From_Star(int16_t* ships, int16_t destination, int16_t origin, s_ship_move_info* result) {
        proposed_origin = origin;
        Ships_Try_To_Move_To(ships, destination, result);
    }
} // namespace SHIPMOVE

DOCTEST_TEST_CASE("AI turn reset does not own or reset world inputs") {
    int16_t player_count = 1;
    game::s_view world;
    AIDATA::s_state first;
    AIDATA::s_state second;
    world.player_count = &player_count;
    first.players[0].build.transports.current = 3;
    first.ships[Ship_Id(UINT32_MAX)].refit_desirability = 9;

    AIDATA::Begin_Turn(&first, 42);
    DOCTEST_CHECK(first.ships.empty());
    DOCTEST_CHECK(first.players[0].build.transports.current == 0);
    player_count = 2;
    DOCTEST_CHECK(world.Player_Count() == 2);
    DOCTEST_CHECK_FALSE(second.initialized);
}

DOCTEST_TEST_CASE("typed ID compatibility preserves zero and rejects absent or unrepresentable indices") {
    using namespace legacy_world;
    for (int32_t absent : {-1, -2}) {
        DOCTEST_CHECK_FALSE(Try_Ship_Id(absent));
        DOCTEST_CHECK_FALSE(Try_Colony_Id(absent));
        DOCTEST_CHECK_FALSE(Try_Planet_Id(absent));
        DOCTEST_CHECK_FALSE(Try_Star_Id(absent));
        DOCTEST_CHECK_FALSE(Try_Player_Id(absent));
        DOCTEST_CHECK_FALSE(Try_Leader_Id(absent));
    }
    for (int16_t index : {int16_t{0}, int16_t{INT16_MAX}}) {
        DOCTEST_CHECK(Index(*Try_Ship_Id(index)) == index);
        DOCTEST_CHECK(Index(*Try_Colony_Id(index)) == index);
        DOCTEST_CHECK(Index(*Try_Planet_Id(index)) == index);
        DOCTEST_CHECK(Index(*Try_Star_Id(index)) == index);
        DOCTEST_CHECK(Index(*Try_Player_Id(index)) == index);
        DOCTEST_CHECK(Index(*Try_Leader_Id(index)) == index);
    }
    for (uint32_t value : {uint32_t{INT16_MAX} + 1, UINT32_MAX}) {
        DOCTEST_CHECK_FALSE(Index(Ship_Id(value)));
        DOCTEST_CHECK_FALSE(Index(Colony_Id(value)));
        DOCTEST_CHECK_FALSE(Index(legacy_world::Planet_Id(Planet_Id(legacy_world::Planet_Id(value)))));
        DOCTEST_CHECK_FALSE(Index(Star_Id(value)));
        DOCTEST_CHECK_FALSE(Index(Player_Id(value)));
        DOCTEST_CHECK_FALSE(Index(Leader_Id(value)));
        DOCTEST_CHECK(Slot(Ship_Id(value)) == value);
        DOCTEST_CHECK(Slot(Colony_Id(value)) == value);
        DOCTEST_CHECK(Slot(legacy_world::Planet_Id(Planet_Id(legacy_world::Planet_Id(value)))) == value);
        DOCTEST_CHECK(Slot(Star_Id(value)) == value);
        DOCTEST_CHECK(Slot(Player_Id(value)) == value);
        DOCTEST_CHECK(Slot(Leader_Id(value)) == value);
    }
}

DOCTEST_TEST_CASE("legacy ship-list encoding accepts zero, rejects narrowing, and owns its terminator") {
    const id_ship ids[] = {Ship_Id(0), Ship_Id(INT16_MAX)};
    auto encoded = legacy_world::Ship_List(ids);
    DOCTEST_REQUIRE(encoded);
    DOCTEST_CHECK(*encoded == std::vector<int16_t>{0, INT16_MAX, -1});
    const id_ship invalid[] = {Ship_Id(UINT32_MAX)};
    DOCTEST_CHECK_FALSE(legacy_world::Ship_List(invalid));
    auto empty = legacy_world::Ship_List({});
    DOCTEST_REQUIRE(empty);
    DOCTEST_CHECK(*empty == std::vector<int16_t>{-1});
}

DOCTEST_TEST_CASE("read-only bit fields have a distinct const-correct interface") {
    const uint8_t bits[] = {1, 0x80};
    DOCTEST_CHECK(struct_::Read_Bit_Field(bits, 0) == 1);
    DOCTEST_CHECK(struct_::Read_Bit_Field(bits, 1) == 0);
    DOCTEST_CHECK(struct_::Read_Bit_Field(bits, 15) == 1);
}

DOCTEST_TEST_CASE("fleet evaluation passes a proposed origin without modifying repeated ship IDs") {
    memset(legacy_ships, 0, sizeof(legacy_ships));
    legacy_ships[0].x = legacy_ships[1].x = 10;
    legacy_ships[0].y = legacy_ships[1].y = 20;
    legacy_stars[1].x = 100;
    legacy_stars[1].y = 200;
    const id_ship ids[] = {Ship_Id(0), Ship_Id(0), Ship_Id(1)};
    s_ship_move_info result{};
    proposed_origin.reset();
    DOCTEST_CHECK(ai_compat::Evaluate_Fleet_Move(ids, Star_Id(0), &result, Star_Id(1)) == SHIPMOVE::e_fleet_evaluation::can_move);
    DOCTEST_CHECK(queried_ships == std::vector<int16_t>{0, 0, 1});
    DOCTEST_CHECK(queried_locations == std::vector<int16_t>{0, 0, 0});
    DOCTEST_CHECK(proposed_origin == 1);
    DOCTEST_CHECK(queried_destination == 0);
    for (const auto& ship : legacy_ships) {
        DOCTEST_CHECK((int16_t)ship.location == 0);
        DOCTEST_CHECK((int16_t)ship.x == 10);
        DOCTEST_CHECK((int16_t)ship.y == 20);
    }
    queried = false;
    const id_ship invalid[] = {Ship_Id(UINT32_MAX)};
    ai_compat::Evaluate_Fleet_Move(invalid, Star_Id(0), &result);
    DOCTEST_CHECK_FALSE(queried);
    DOCTEST_CHECK(result.moving == 0);
    queried = false;
    ai_compat::Evaluate_Fleet_Move({}, Star_Id(0), &result);
    DOCTEST_CHECK_FALSE(queried);
    const id_ship nonexistent[] = {Ship_Id(2)};
    DOCTEST_CHECK(ai_compat::Evaluate_Fleet_Move(nonexistent, Star_Id(0), &result) == SHIPMOVE::e_fleet_evaluation::invalid_input);
    DOCTEST_CHECK_FALSE(queried);
}

DOCTEST_TEST_CASE("fleet collection preserves cached order and mission filtering") {
    s_ship_data ships[2] = {};
    ships[1].mission = 5;
    s_player player{};
    player.offensive_stagepoint = -1;
    const game::s_view world{.ships = ships};
    AIDATA::s_state ai;
    ai.combat_ships = {Ship_Id(0), Ship_Id(1)};
    ai.players[0].combat_ship_groups[Star_Id(0)] = {0, 2};
    AIMOVE::s_ai_movement_context context{Player_Id(0), &player, nullptr, &world};
    std::vector<id_ship> collected{Ship_Id(UINT32_MAX)};

    AIMOVE::Gather_Player_Ships_At_Star(&ai, &context, Player_Id(0), Star_Id(0), &collected);
    DOCTEST_CHECK(collected == std::vector<id_ship>{Ship_Id(0)});
    player.offensive_stagepoint = 0;
    AIMOVE::Gather_Player_Ships_At_Star(&ai, &context, Player_Id(0), Star_Id(0), &collected);
    DOCTEST_CHECK(collected == std::vector<id_ship>{Ship_Id(0), Ship_Id(1)});
    ships[1].owner = 1;
    AIMOVE::Gather_Player_Ships_At_Star(&ai, &context, Player_Id(0), Star_Id(0), &collected);
    DOCTEST_CHECK(collected == std::vector<id_ship>{Ship_Id(0)});
    AIMOVE::Gather_Player_Ships_At_Star(&ai, &context, Player_Id(0), Star_Id(1), &collected);
    DOCTEST_CHECK(collected.empty());
}

DOCTEST_TEST_CASE("fleet planning returns the evaluated direct route") {
    memset(legacy_ships, 0, sizeof(legacy_ships));
    const game::s_view world{.ships = legacy_ships};
    AIMOVE::s_ai_movement_context context{Player_Id(0), nullptr, nullptr, &world};
    const id_ship ids[] = {Ship_Id(0)};
    queried = false;
    const auto route = AIMOVE::Move_Ships_With_Possible_Intermediate(&::game::world, &context, ids, Star_Id(1));
    DOCTEST_REQUIRE(route);
    DOCTEST_CHECK(queried);
    DOCTEST_CHECK(queried_ships == std::vector<int16_t>{0});
    DOCTEST_CHECK(queried_destination == 1);
    DOCTEST_CHECK(route->next_star == Star_Id(1));
    DOCTEST_CHECK(route->turns == 3);
}

DOCTEST_TEST_CASE("movement planning rejects destinations unsupported by the legacy engine") {
    s_ship_data ships[1] = {};
    game::s_view world;
    world.ships = ships;
    AIMOVE::s_ai_movement_context context{Player_Id(0), nullptr, nullptr, &world};
    const id_ship ids[] = {Ship_Id(0)};
    queried = false;
    auto route = AIMOVE::Move_Ships_With_Possible_Intermediate(&::game::world, &context, ids, Star_Id(UINT32_MAX));
    DOCTEST_CHECK_FALSE(route);
    DOCTEST_CHECK_FALSE(queried);
}

DOCTEST_TEST_CASE("fleet evaluation and ordering share native request validation") {
    const id_ship ships[] = {Ship_Id(0)};
    const id_ship missing[] = {Ship_Id(MOX::NUM_SHIPS)};
    s_ship_move_info result{};
    queried = false;
    DOCTEST_CHECK(ai_compat::Evaluate_Fleet_Move(missing, Star_Id(0), &result) == SHIPMOVE::e_fleet_evaluation::invalid_input);
    DOCTEST_CHECK_FALSE(ai_compat::Order_Fleet_Move(missing, Star_Id(0), &result));
    DOCTEST_CHECK_FALSE(queried);
    const auto outside = Star_Id(MOX::NUM_STARS + 1);
    DOCTEST_CHECK(ai_compat::Evaluate_Fleet_Move(ships, outside, &result) == SHIPMOVE::e_fleet_evaluation::invalid_input);
    DOCTEST_CHECK_FALSE(ai_compat::Order_Fleet_Move(ships, outside, &result));
    DOCTEST_CHECK_FALSE(queried);
    DOCTEST_CHECK(ai_compat::Evaluate_Fleet_Move(ships, Star_Id(0), &result, Star_Id(MOX::NUM_STARS)) == SHIPMOVE::e_fleet_evaluation::invalid_input);
    DOCTEST_CHECK_FALSE(queried);
    // The portal destination is valid; unlike the destination, an origin must be an actual star.
    DOCTEST_CHECK(ai_compat::Evaluate_Fleet_Move(ships, Star_Id(MOX::NUM_STARS), &result) == SHIPMOVE::e_fleet_evaluation::can_move);
    DOCTEST_CHECK(ai_compat::Order_Fleet_Move(ships, Star_Id(MOX::NUM_STARS), &result));
}

DOCTEST_TEST_CASE("AI movement rejects wide IDs rather than aliasing legacy index zero") {
    const id_ship valid[] = {Ship_Id(0)};
    s_ship_move_info result{};
    for (uint32_t id : {uint32_t{INT16_MAX} + 1, uint32_t{1} << 16, UINT32_MAX}) {
        const id_ship invalid[] = {Ship_Id(id)};
        queried = false;
        result.moving = 1;
        DOCTEST_CHECK(ai_compat::Evaluate_Fleet_Move(invalid, Star_Id(0), &result) == SHIPMOVE::e_fleet_evaluation::invalid_input);
        DOCTEST_CHECK(result.moving == 0);
        DOCTEST_CHECK_FALSE(ai_compat::Order_Fleet_Move(invalid, Star_Id(0), &result));
        DOCTEST_CHECK(ai_compat::Evaluate_Fleet_Move(valid, Star_Id(id), &result) == SHIPMOVE::e_fleet_evaluation::invalid_input);
        DOCTEST_CHECK_FALSE(ai_compat::Order_Fleet_Move(valid, Star_Id(id), &result));
        DOCTEST_CHECK(ai_compat::Evaluate_Fleet_Move(valid, Star_Id(0), &result, Star_Id(id)) == SHIPMOVE::e_fleet_evaluation::invalid_input);
        DOCTEST_CHECK_FALSE(queried);

        const game::s_view world{.ships = legacy_ships};
        AIMOVE::s_ai_movement_context context{Player_Id(0), nullptr, nullptr, &world};
        DOCTEST_CHECK_FALSE(AIMOVE::Move_Ships_With_Possible_Intermediate(&::game::world, &context, invalid, Star_Id(0)));
        DOCTEST_CHECK_FALSE(queried);
    }
}

DOCTEST_TEST_CASE("native fleet movement accepts only terminated legacy index lists") {
    s_ship_move_info result{};
    int16_t valid[] = {0, 0, 1, -1};
    proposed_origin.reset();
    DOCTEST_CHECK(SHIPMOVE::Evaluate_Fleet_Move(valid, 0, &result, 1) == SHIPMOVE::e_fleet_evaluation::can_move);
    DOCTEST_CHECK(queried_ships == std::vector<int16_t>{0, 0, 1});
    DOCTEST_CHECK(proposed_origin == 1);
    DOCTEST_CHECK(valid[3] == -1);

    queried = false;
    int16_t unterminated[] = {0};
    int16_t empty[] = {-1};
    int16_t early_end[] = {0, -1, 1, -1};
    int16_t negative[] = {-2, -1};
    for (auto list : {std::span<int16_t>{unterminated}, std::span<int16_t>{empty},
             std::span<int16_t>{early_end}, std::span<int16_t>{negative}}) {
        DOCTEST_CHECK(SHIPMOVE::Evaluate_Fleet_Move(list, 0, &result) == SHIPMOVE::e_fleet_evaluation::invalid_input);
        DOCTEST_CHECK_FALSE(SHIPMOVE::Order_Fleet_Move(list, 0, &result));
    }
    DOCTEST_CHECK(SHIPMOVE::Evaluate_Fleet_Move(valid, -1, &result) == SHIPMOVE::e_fleet_evaluation::invalid_input);
    DOCTEST_CHECK(SHIPMOVE::Evaluate_Fleet_Move(valid, 0, &result, -1) == SHIPMOVE::e_fleet_evaluation::invalid_input);
    DOCTEST_CHECK_FALSE(SHIPMOVE::Order_Fleet_Move(valid, -1, &result));
    DOCTEST_CHECK_FALSE(queried);
}
