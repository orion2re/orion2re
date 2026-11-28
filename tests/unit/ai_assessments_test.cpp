#include "pch.h"
#include "../fixtures/ship_rows.h"

#include <memory>

static_assert(game::player_bitmap::From_Byte(0x81).test(game::id_player{7}));
static_assert(game::player_bitmap::From_Byte(0x81).To_Byte() == 0x81);
static_assert(game::player_bitmap::Only(game::NO_PLAYER).none());
static_assert(game::player_bitmap::Only(game::id_player{65536}).none());
#include "../fixtures/star_roster.h"

#include "doctest/doctest.h"
#include "game/legacy_ids.h"
#include "game/fleet_movement.h"
#include "game/dip_scrn.h"
#include "game/ship_ids.h"
#include "../fixtures/game_state.h"
#include "../fixtures/planet_roster.h"

namespace {
    struct s_saved_rng {
        uint32_t seed = game_random::Get_Random_Seed();
        ~s_saved_rng() { game_random::Set_Random_Seed(seed); }
    };

    template<class T>
    struct s_saved_table {
        T& target;
        std::array<uint8_t, sizeof(T)> bytes;
        explicit s_saved_table(T& table) : target(table) { memcpy(bytes.data(), &table, sizeof(T)); }
        ~s_saved_table() { memcpy(&target, bytes.data(), sizeof(T)); }
    };
}

DOCTEST_TEST_CASE("opportunity and threat scoring use the same supplied events and geometry as ordinary scoring") {
    test::s_game_state fixture;
    test::s_saved_array saved_events{EVENTS::event_data};
    s_saved_rng rng;
    auto world = std::make_unique<game::s_game_state>();
    world->player_count = 2;
    world->players[0].ship_range = 100;
    world->players[0].ship_speed = 2;
    const auto source = game::Create_Star(*world);
    const auto target = game::Create_Star(*world);
    DOCTEST_REQUIRE(source);
    DOCTEST_REQUIRE(target);
    world->stars.at(*target).x = 120;
    world->stars.at(*source).has_colony.set_slot(0);
    world->stars.at(*target).has_colony.set_slot(1);
    world->players[0].offensive_stagepoint = *source;
    const auto planet = game::Create_Planet(*world, {.star_id = *target});
    DOCTEST_REQUIRE(planet);
    const auto colony = game::Create_Colony(*world, {.owner = 1, .planet_id = *planet, .n_pops = 2});
    DOCTEST_REQUIRE(colony);
    world->planets.at(*planet).colony_id = *colony;
    world->stars.at(*target).orbit_planet_ids[0] = *planet;
    constexpr game::id_ship key{90000};
    s_ship_data ship{.id = key};
    ship.owner = game::owner_slot{0};
    ship.position.Set({game::e_ship_location::at_star, *source});
    world->ships.emplace(key, ship);
    AIDATA::s_state ai{};
    ai.initialized = true;
    ai.combat_ships.push_back(key);
    ai.players[0].combat_ship_groups.emplace(*source, AIDATA::s_combat_ship_group{0, 1});
    ai.ships[key].strengths[1] = 100;
    std::ranges::fill(ai.ships[key].bomb_strengths, 100);
    ai.stars[*target].in_range.set_slot(0);
    ai.planets[*planet].base_values[0] = ai.planets[*planet].base_values[1] = 1000;
    struct Animation { s_animation_header header{}; uint32_t offset = offsetof(Animation, pixel); uint8_t pixel = 6; } animation;
    animation.header.width = animation.header.height = animation.header.frame_count = 1;
    test::s_saved_value picture{MOX::nebula_pict_seg[0][3]};
    MOX::nebula_pict_seg[0][3] = &animation.header;
    const s_nebula geometry[]{{30, 0, 0}};
    const auto compare = [&](bool expected) {
        AIDATA::s_movement_workspace work;
        AIMOVE::s_ai_movement_context ctx{game::id_player{0}, &world->players[0], &work, world.get(), GAME_DIFFICULTY_AVERAGE, 0, ai.planning};
        AIMOVE::s_attack_info ordinary{};
        ordinary.target_player = game::id_player{1};
        ordinary.minimum_strength_ratio_percent = 150;
        AIMOVE::Find_Best_Attack_From_Star(&ai, &ctx, *source, &ordinary, 1, 2);
        const auto opportunity = AIMOVE::Find_Opportunity_Attack(&ai, world.get(), GAME_DIFFICULTY_AVERAGE, 0, game::id_player{0}, game::id_player{1}, 150);
        const auto threat = AIMOVE::Player_Threatens_Player(&ai, world.get(), GAME_DIFFICULTY_AVERAGE, 0, game::id_player{0}, game::id_player{1});
        DOCTEST_CHECK(ordinary.target_planet.has_value() == expected);
        DOCTEST_CHECK(opportunity.has_value() == expected);
        DOCTEST_CHECK(threat.has_value() == expected);
        if (ordinary.target_planet && opportunity) {
            DOCTEST_CHECK(opportunity->planet_id == *ordinary.target_planet);
            DOCTEST_CHECK(opportunity->colony_value == ordinary.colony_value);
            DOCTEST_CHECK(opportunity->strength_ratio_percent == ordinary.strength_ratio_percent);
        }
        if (ordinary.target_planet && threat) DOCTEST_CHECK(threat->attack.planet_id == *ordinary.target_planet);
    };
    EVENTS::event_data[EVENT_HYPERSPACE_FLUX].status = 0;
    compare(true);
    ai.planning.hyperspace_flux = true;
    compare(false);
    ai.planning.hyperspace_flux = false;
    EVENTS::event_data[EVENT_HYPERSPACE_FLUX].status = 2;
    compare(true);
    ai.planning.nebulas = geometry;
    ai.stars[*target].possible_strengths[1][0][3] = 1000;
    s_ship_move_info movement{};
    const game::id_ship fleet[]{key};
    DOCTEST_REQUIRE(SHIPMOVE::Evaluate_Fleet_Move(*world, fleet, {game::e_star_destination::star, *target}, &movement, ai.planning) == SHIPMOVE::e_fleet_evaluation::can_move);
    DOCTEST_CHECK(movement.turns_left == 3);
    compare(false);
    EVENTS::event_data[EVENT_HYPERSPACE_FLUX].status = 0;
    compare(false);
}

DOCTEST_TEST_CASE("planet generation links only the supplied world and uses its resolved star") {
    test::s_game_state fixture;
    s_saved_rng rng;
    test::s_saved_value settings{MOX::settings};
    test::s_saved_value count{MAPGEN::n_planets};
    test::s_saved_value companion{MAPGEN::companion_star};
    test::s_saved_array farms{MOX::planet_max_farms};
    test::s_saved_array food{MOX::food_per_farmer_table};
    s_saved_table types{MOX::orbit_to_satellite_type};
    for (auto& row : MOX::orbit_to_satellite_type) std::ranges::fill(row, MAPGEN_SATELLITE_PLANET);
    MOX::settings.galaxy_age = 0;
    MAPGEN::companion_star = 0;
    std::ranges::fill(MOX::planet_max_farms, 7);
    std::ranges::fill(MOX::food_per_farmer_table, 3);
    auto& live = game::current_state;
    const auto star = game::Create_Star(live);
    DOCTEST_REQUIRE(star);
    live.stars.at(*star).spectral_class = STAR_CLASS_BLACK_HOLE;
    auto world = std::make_unique<game::s_game_state>(live);
    world->stars.at(*star).spectral_class = STAR_CLASS_F;
    const auto initial = std::make_unique<game::s_game_state>(*world);
    const auto original_live = live.stars;
    game_random::Set_Random_Seed(1);
    MAPGEN::Planet_Generation(*world, *star);
    DOCTEST_REQUIRE(world->planets.size() == 1);
    const auto& planet = world->planets.begin()->second;
    DOCTEST_CHECK(planet.star_id == *star);
    DOCTEST_CHECK(world->stars.at(*star).orbit_planet_ids[planet.orbit] == planet.id);
    DOCTEST_CHECK(planet.max_farms == 7);
    DOCTEST_CHECK(planet.food_per_farmer == 3);
    DOCTEST_CHECK(live.stars == original_live);
    DOCTEST_CHECK(live.planets.empty());
    live.stars.at(*star).spectral_class = STAR_CLASS_B;
    for (auto& orbit : live.stars.at(*star).orbit_planet_ids) orbit = game::id_planet{80000};
    const auto changed_live = live.stars;
    game_random::Set_Random_Seed(1);
    MAPGEN::companion_star = 0;
    MAPGEN::Planet_Generation(*initial, *star);
    DOCTEST_CHECK(initial->planets == world->planets);
    DOCTEST_CHECK(initial->stars == world->stars);
    DOCTEST_CHECK(live.stars == changed_live);
}

DOCTEST_TEST_CASE("black hole repair uses the supplied roster and nebula probes") {
    test::s_game_state fixture;
    s_saved_rng rng;
    test::s_saved_value settings{MOX::settings};
    test::s_saved_value count{MAPGEN::n_planets};
    test::s_saved_value companion{MAPGEN::companion_star};
    s_saved_table satellites{MOX::class_to_num_satellites};
    s_saved_table types{MOX::orbit_to_satellite_type};
    for (auto& row : MOX::orbit_to_satellite_type) std::ranges::fill(row, MAPGEN_SATELLITE_PLANET);
    for (auto& row : MOX::class_to_num_satellites) std::ranges::fill(row, 1);
    MOX::settings.galaxy_age = 0;
    const auto star = game::Create_Star(game::current_state);
    DOCTEST_REQUIRE(star);
    const auto live_stars = game::current_state.stars;
    struct Animation { s_animation_header header{}; uint32_t offset = offsetof(Animation, pixel); uint8_t pixel = 6; } animation;
    animation.header.width = animation.header.height = animation.header.frame_count = 1;
    test::s_saved_value picture{MOX::nebula_pict_seg[0][3]};
    MOX::nebula_pict_seg[0][3] = &animation.header;
    for (const auto [dx, dy] : {std::pair{0, 0}, {24, 0}, {0, 24}, {24, 24}, {12, 0}, {24, 12}, {12, 24}, {0, 12}}) {
        auto world = std::make_unique<game::s_game_state>(game::current_state);
        auto& system = world->stars.at(*star);
        system.spectral_class = STAR_CLASS_BLACK_HOLE;
        system.x = 100;
        system.y = 200;
        system.visited.set_slot(0);
        const s_nebula geometry[]{{static_cast<int16_t>(100 + dx), static_cast<int16_t>(200 + dy), 0}};
        game_random::Set_Random_Seed(1);
        MAPGEN::companion_star = 0;
        MAPGEN::Black_Hole_Fix(*world, 0, geometry);
        DOCTEST_CHECK(system.spectral_class < STAR_CLASS_BLACK_HOLE);
        DOCTEST_REQUIRE(world->planets.size() == 1);
        const auto& planet = world->planets.begin()->second;
        DOCTEST_CHECK(planet.star_id == *star);
        DOCTEST_CHECK(system.orbit_planet_ids[planet.orbit] == planet.id);
        DOCTEST_CHECK(system.visited.none());
        DOCTEST_CHECK(game::current_state.stars == live_stars);
        DOCTEST_CHECK(game::current_state.planets.empty());
    }
    test::s_saved_value nebula_ptr{MOX::nebula};
    test::s_saved_value nebula_count{MOX::NUM_NEBULAS};
    s_nebula live_geometry[]{{100, 200, 0}};
    MOX::nebula = live_geometry;
    MOX::NUM_NEBULAS = 1;
    auto world = std::make_unique<game::s_game_state>(game::current_state);
    world->stars.at(*star).spectral_class = STAR_CLASS_BLACK_HOLE;
    world->stars.at(*star).x = 100;
    world->stars.at(*star).y = 200;
    MAPGEN::Black_Hole_Fix(*world, 0, {});
    DOCTEST_CHECK(world->stars.at(*star).spectral_class == STAR_CLASS_BLACK_HOLE);
    DOCTEST_CHECK(world->planets.empty());
}

DOCTEST_TEST_CASE("monster setup assigns RNG draws and allocations in draft roster order") {
    test::s_game_state fixture;
    s_saved_rng rng;
    test::s_saved_array names{MOX::race_names};
    for (int i = 14; i < 20; ++i) MOX::race_names[i] = const_cast<char*>("Monster");
    auto& world = game::current_state;
    for (int i = 0; i < 3; ++i) DOCTEST_REQUIRE(game::Create_Star(world));
    const auto replaced = game::Replace_Draft_Star(world, 0);
    DOCTEST_REQUIRE(replaced);
    for (const auto key : world.star_order) {
        auto& star = world.stars.at(key);
        star.system_special = SYSTEM_SPECIAL_SPACE_MONSTER;
        star.twinkling1 = SYSTEM_SPECIAL_NO_SPECIAL;
        for (int orbit = 0; orbit < 2; ++orbit) {
            const auto planet = game::Create_Planet(world, {.star_id = key, .orbit = static_cast<int8_t>(orbit), .planet_type = PLANET_TYPE_PLANET});
            DOCTEST_REQUIRE(planet);
            star.orbit_planet_ids[orbit] = *planet;
        }
    }
    const auto original = std::make_unique<game::s_game_state>(world);
    game_random::Set_Random_Seed(1);
    int row = 0;
    for (const auto key : world.star_order) {
        auto& star = world.stars.at(key);
        const auto type = game_random::Random(5) + 9;
        if (row == 0) DOCTEST_CHECK(type == 11);
        const auto planet = HOMEGEN::Random_Planet(world, key);
        ship_slots::Reserve(world, static_cast<int16_t>(world.ship_slots.size()), game::owner_slot{MAX_PLAYERS});
        DOCTEST_REQUIRE(SHIPS::Build_Nonplayer_Ship(static_cast<int16_t>(type), static_cast<int16_t>(row), 0) != game::NO_SHIP);
        star.system_special = star.twinkling1;
        star.twinkling1 = 0;
        HOMEGEN::Upgrade_Planet_Guarded_By_Monster(&world.planets.at(planet), static_cast<int16_t>(type));
        ++row;
    }
    const auto expected = std::make_unique<game::s_game_state>(world);
    const auto expected_seed = game_random::Get_Random_Seed();
    world = *original;
    game_random::Set_Random_Seed(1);
    INITGAME::Make_System_Monsters_Into_Ships();
    DOCTEST_CHECK(world.ship_slots == expected->ship_slots);
    DOCTEST_CHECK(world.ships == expected->ships);
    DOCTEST_CHECK(world.planets == expected->planets);
    DOCTEST_CHECK(world.stars == expected->stars);
    DOCTEST_CHECK(world.next_ship_id == expected->next_ship_id);
    DOCTEST_CHECK(world.ship_generations == expected->ship_generations);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == expected_seed);
}

DOCTEST_TEST_CASE("system demand choice and acceptance retain identity across modal changes") {
    test::s_game_state fixture;
    auto& world = game::current_state;
    world.player_count = 2;
    world.next_star_id = 70000;
    world.players[0].home_planet_id = world.players[1].home_planet_id = game::NO_PLANET;
    world.players[1].ship_range = 100;
    const auto selected = game::Create_Star(world);
    const auto keeper = game::Create_Star(world);
    const auto receiver = game::Create_Star(world);
    DOCTEST_REQUIRE(selected);
    DOCTEST_REQUIRE(keeper);
    DOCTEST_REQUIRE(receiver);
    for (const auto key : world.star_order) std::ranges::fill(world.stars.at(key).surrender_to, game::star_surrender_order::None());
    world.stars.at(*selected).has_colony.set_slot(0);
    world.stars.at(*keeper).has_colony.set_slot(0);
    world.stars.at(*receiver).has_colony.set_slot(1);
    const auto original = std::make_unique<game::s_game_state>(world);
    test::s_saved_value prompt{DIP_SCRN::system_demand_prompt};
    static int action;
    DIP_SCRN::system_demand_prompt = +[](game::id_star key, game::id_player giver, game::id_player asker) -> int16_t {
        DOCTEST_CHECK(giver == game::id_player{0});
        DOCTEST_CHECK(asker == game::id_player{1});
        if (action == 3) return -1;
        auto& state = game::current_state;
        if (action == 0) {
            std::swap(state.star_order[0], state.star_order[1]);
            state.star_order_sorted = false;
        } else if (action == 1) {
            state.stars.erase(key);
            std::erase(state.star_order, key);
        } else {
            const auto replacement = game::Replace_Draft_Star(state, static_cast<size_t>(game::Star_Slot(state, key)));
            if (!replacement) return -1;
            auto& star = state.stars.at(*replacement);
            star.has_colony.set_slot(0);
            std::ranges::fill(star.surrender_to, game::star_surrender_order::None());
        }
        return 0;
    };
    for (action = 0; action < 4; ++action) {
        world = *original;
        const auto choice = DIP_SCRN::Get_Human_Demand_Choice(game::id_player{0}, game::id_player{1}, 7, *selected);
        DOCTEST_CHECK(choice == (action == 0 ? 1 : 0));
        if (choice == 1) DIP_SCRN::Diplomacy_Give_In_To_Demand(game::id_player{1}, game::id_player{0}, 7, 0, *selected);
        for (const auto& [key, star] : world.stars) {
            if (action == 0 && key == *selected) DOCTEST_CHECK(star.surrender_to[0].Ordered_Recipient() == game::id_player{1});
            else DOCTEST_CHECK(star.surrender_to[0].Is_None());
        }
    }
}

DOCTEST_TEST_CASE("government grants reprice templates without redesigning or changing refit charges") {
    test::s_game_state fixture;
    test::s_saved_array saved_hulls{TECHDATA::hull_data};
    test::s_saved_array saved_drives{TECHDATA::drives};
    test::s_saved_array saved_armor{TECHDATA::armor};
    test::s_saved_array saved_computers{TECHDATA::computers};
    test::s_saved_array saved_shields{TECHDATA::shields};
    test::s_saved_array saved_governments{COLCALC::government_bonuses};
    std::ranges::fill(TECHDATA::drives, s_tech_drive_data{});
    std::ranges::fill(TECHDATA::armor, s_tech_armor_data{});
    std::ranges::fill(TECHDATA::computers, s_tech_computer_data{});
    std::ranges::fill(TECHDATA::shields, s_tech_shield_data{});
    TECHDATA::hull_data[SHIP_SIZE_SMALL].cost = 200;
    TECHDATA::hull_data[SHIP_SIZE_SMALL].size = 100;
    for (const auto [app, government] : {std::pair{TECH_APP_CONFEDERATION, GOVERNMENT_CONFEDERATION},
        std::pair{TECH_APP_FEDERATION, GOVERNMENT_FEDERATION},
        std::pair{TECH_APP_GALACTIC_UNIFICATION, GOVERNMENT_GALACTIC_UNIFICATION},
        std::pair{TECH_APP_IMPERIUM, GOVERNMENT_IMPERIUM}}) {
        COLCALC::government_bonuses[government].cost_reduction = {1, 2};
        for (const bool strategic : {false, true}) {
            DOCTEST_CAPTURE(app);
            DOCTEST_CAPTURE(strategic);
            auto world = std::make_unique<game::s_game_state>();
            world->player_count = 1;
            auto& player = world->players[0];
            player.objectives = PLAYER_OBJECTIVE_HUMAN;
            auto& design = player.ship_designs[0];
            strlcpy(design.name, "Keep this design", sizeof(design.name));
            design.cost = strategic ? 300 : 200;
            design.previous_owner = game::owner_slot{0};
            design.date_of_design = 35001;
            const auto before = design;
            constexpr game::id_ship building{90000}, refitting{90001};
            world->ships.emplace(building, s_ship_data{.d = design, .owner = game::owner_slot{0}, .status = 6, .id = building});
            world->ships.emplace(refitting, s_ship_data{.d = design, .owner = game::owner_slot{0}, .status = 4, .id = refitting});
            world->ships.at(refitting).d.cost = 17;
            const auto seed = game_random::Get_Random_Seed();
            const auto live_player = game::current_state.players[0];
            COLCALC::Player_Gets_Tech_App(*world, &player, app, {GAME_DIFFICULTY_TUTOR, strategic ? int16_t{1} : int16_t{0}, 35123, -1000, true});
            auto expected = before;
            expected.cost = strategic ? 150 : 100;
            DOCTEST_CHECK(design == expected);
            DOCTEST_CHECK(world->ships.at(building).d == expected);
            DOCTEST_CHECK(world->ships.at(refitting).d.cost == 17);
            DOCTEST_CHECK(player.ship_designs[1] == s_ship_design{});
            DOCTEST_CHECK(player.traits[TRAIT_CURRENT_GOVERNMENT] == government);
            DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
            DOCTEST_CHECK(game::current_state.players[0] == live_player);
        }
    }
}

DOCTEST_TEST_CASE("system offers and strategic queries retain sparse star keys across confirmation changes") {
    test::s_game_state fixture;
    auto& world = game::current_state;
    world.player_count = 2;
    world.next_star_id = 70000;
    world.players[0].home_planet_id = world.players[1].home_planet_id = game::NO_PLANET;
    const auto offered = game::Create_Star(world);
    const auto keeper = game::Create_Star(world);
    const auto receiver = game::Create_Star(world);
    DOCTEST_REQUIRE(offered);
    DOCTEST_REQUIRE(keeper);
    DOCTEST_REQUIRE(receiver);
    for (auto key : {*offered, *keeper, *receiver}) std::ranges::fill(world.stars.at(key).surrender_to, game::star_surrender_order::None());
    world.stars.at(*keeper).x = 30;
    for (auto key : {*offered, *keeper}) world.stars.at(key).has_colony.set_slot(0);
    world.stars.at(*receiver).has_colony.set_slot(1);
    world.players[1].ship_range = 100;
    for (const auto [star, owner] : {std::pair{*offered, 0}, std::pair{*keeper, 0}, std::pair{*receiver, 1}}) {
        const auto planet = game::Create_Planet(world, {.star_id = star});
        DOCTEST_REQUIRE(planet);
        const auto colony = game::Create_Colony(world, {.owner = game::id_player{static_cast<uint32_t>(owner)}, .planet_id = *planet});
        DOCTEST_REQUIRE(colony);
        world.stars.at(star).orbit_planet_ids[0] = *planet;
        world.planets.at(*planet).colony_id = *colony;
    }
    const auto selection = game::Star_Key_At_Slot(world, 0);
    std::swap(world.star_order[0], world.star_order[1]);
    world.star_order_sorted = false;
    DOCTEST_CHECK(COMBFIND::Closest_Other_Colony_Star(world, game::id_player{0}, selection) == *keeper);
    DOCTEST_CHECK(COMBFIND::Closest_Other_Colony_Star(world, game::id_player{0}, game::NO_STAR) == game::NO_STAR);
    DOCTEST_CHECK(HACCESS::Player_Has_Colony_In_System(world, game::id_player{0}, selection));
    DOCTEST_CHECK_FALSE(HACCESS::Player_Has_Colony_In_System(world, game::id_player{0}, game::NO_STAR));
    DOCTEST_REQUIRE(DIP_SCRN::Commit_System_Offer(selection, game::id_player{0}, game::id_player{1}));
    DOCTEST_CHECK(world.stars.at(selection).surrender_to[0].Ordered_Recipient() == game::id_player{1});
    DOCTEST_CHECK(world.stars.at(*keeper).surrender_to[0].Is_None());
    const auto selected_slot = game::Star_Slot(world, selection);
    const auto replacement = game::Replace_Draft_Star(world, static_cast<size_t>(selected_slot));
    DOCTEST_REQUIRE(replacement);
    std::ranges::fill(world.stars.at(*replacement).surrender_to, game::star_surrender_order::None());
    world.stars.at(*replacement).has_colony.set_slot(0);
    DOCTEST_REQUIRE(COLCALC::May_Surrender_Star(world, *replacement, game::id_player{0}, game::id_player{1}));
    DOCTEST_CHECK_FALSE(DIP_SCRN::Commit_System_Offer(selection, game::id_player{0}, game::id_player{1}));
    DOCTEST_CHECK(world.stars.at(*replacement).surrender_to[0].Is_None());

    game::owner_bitmap pending[MAX_STARS + 1]{};
    test::s_saved_value saved_pending{COMBFIND::player_needs_to_attack};
    COMBFIND::player_needs_to_attack = &pending;
    pending[game::Star_Slot(world, *keeper)].set(0);
    pending[game::ANTARES_POSITION].set(NONPLAYER_SHIP_TYPE_ANTARAN);
    DOCTEST_CHECK(COMBFIND::Pick_Attacker_At_Star({game::e_star_destination::star, *keeper}) == game::owner_slot{0});
    DOCTEST_CHECK(COMBFIND::Pick_Attacker_At_Star({game::e_star_destination::antares, game::NO_STAR}) == game::owner_slot{NONPLAYER_SHIP_TYPE_ANTARAN});
    DOCTEST_CHECK(COMBFIND::Pick_Attacker_At_Star({}) == game::NO_OWNER);
}

DOCTEST_TEST_CASE("detached AI buying charges only its own budget once and uses its own hull price") {
    test::s_game_state fixture;
    auto& live = game::current_state;
    live.player_count = 1;
    const auto star = game::Create_Star(live);
    DOCTEST_REQUIRE(star);
    const auto planet = game::Create_Planet(live, {.star_id = *star});
    DOCTEST_REQUIRE(planet);
    const auto colony = game::Create_Colony(live, {.owner = 0, .planet_id = *planet});
    DOCTEST_REQUIRE(colony);
    live.players[0].bc = 9000;
    auto world = std::make_unique<game::s_game_state>(live);
    constexpr game::id_ship hull{90000};
    world->ships.emplace(hull, s_ship_data{.id = hull});
    world->ships.at(hull).d.cost = 100;
    auto& order = world->colonies.at(*colony);
    order.producing[0] = game::Queued_Ship_Product(hull);
    order.production_spent = 50;
    order.production[ECON_INDUSTRY] = 5;
    world->players[0].bc = 1000;
    const auto expected_cost = COLCALC::Production_Cost_To_Buy(100, 50);
    AIDATA::s_state ai{};
    ai.colonies[*colony].other_player_arrivals = 1;
    DOCTEST_SUBCASE("the live colony has the same key") {}
    DOCTEST_SUBCASE("the colony key exists only in the supplied world") { live.colonies.clear(); }
    const auto live_colonies = live.colonies;
    const auto live_player = live.players[0];
    AIBUILD::Empire_Buy_Outright(&ai, world.get(), game::id_player{0});
    DOCTEST_CHECK(world->players[0].bc == 1000 - expected_cost);
    DOCTEST_CHECK(static_cast<int>(order.bought_outright) == 50);
    DOCTEST_CHECK(COLCALC::Colony_N_Turns_To_Produce(*world, &order) == 10);
    AIBUILD::Empire_Buy_Outright(&ai, world.get(), game::id_player{0});
    DOCTEST_CHECK(world->players[0].bc == 1000 - expected_cost);
    DOCTEST_CHECK(live.players[0] == live_player);
    DOCTEST_CHECK(live.colonies == live_colonies);
}

DOCTEST_TEST_CASE("detached autobuild eligibility uses its own tech and system queues") {
    test::s_game_state fixture;
    auto& live = game::current_state;
    live.player_count = 1;
    const auto star = game::Create_Star(live);
    DOCTEST_REQUIRE(star);
    const auto planet = game::Create_Planet(live, {.star_id = *star});
    DOCTEST_REQUIRE(planet);
    const auto colony = game::Create_Colony(live, {.owner = 0, .planet_id = *planet, .n_pops = 1});
    DOCTEST_REQUIRE(colony);
    live.stars.at(*star).orbit_planet_ids[0] = *planet;
    live.planets.at(*planet).colony_id = *colony;
    auto world = std::make_unique<game::s_game_state>(live);
    auto& target = world->colonies.at(*colony);
    auto& building = TECHDATA::buildings[BUILDING_ARTEMIS_SYSTEM_NET];
    test::s_saved_value saved_building{building};
    building.tech_app = TECH_APP_ARTEMIS_SYSTEM_NET;
    world->players[0].tech_applications[building.tech_app] = TECH_RESEARCH_STATUS_RESEARCHED;
    world->colonies.at(*colony).producing[0] = BUILDING_ARTEMIS_SYSTEM_NET;
    DOCTEST_CHECK(COLCALC::Colony_Can_Build_Product(*world, &target, BUILDING_ARTEMIS_SYSTEM_NET) == 2);
    world->colonies.at(*colony).producing[0] = COLONY_PRODUCTION_NONE;
    uint16_t buildable[BUILDING_COUNT]{};
    AIBUILD::Find_Buildable_Buildings(*world, &target, buildable);
    DOCTEST_CHECK(buildable[BUILDING_ARTEMIS_SYSTEM_NET] == 1);
    DOCTEST_CHECK(COLCALC::Colony_Can_Build_Product(&live.colonies.at(*colony), BUILDING_ARTEMIS_SYSTEM_NET) == 0);
    test::s_saved_value saved_rules{AIBUILD::rules};
    for (auto& candidate : AIBUILD::rules.autobuild.candidates) candidate.enabled = false;
    auto& choice = AIBUILD::rules.autobuild.candidates[BUILDING_ARTEMIS_SYSTEM_NET];
    choice = {.enabled = true};
    choice.weight.base = choice.weight.clamp.maximum = 100;
    AIBUILD::rules.autobuild.cutoff.mode = production_rules::CUTOFF_DISABLED;
    AIDATA::s_state ai{};
    AIBUILD::Assign_Colony_New_Building(&ai, world.get(), GAME_DIFFICULTY_TUTOR, *colony, &target);
    DOCTEST_CHECK(target.producing[0] == BUILDING_ARTEMIS_SYSTEM_NET);
    DOCTEST_CHECK(live.colonies.at(*colony).producing[0] != BUILDING_ARTEMIS_SYSTEM_NET);
    auto& terraforming = TECHDATA::buildings[BUILDING_TERRAFORMING];
    test::s_saved_value saved_terraforming{terraforming};
    terraforming.cost = 100;
    world->planets.at(*planet).n_terraforms = 2;
    DOCTEST_CHECK(COLCALC::Colony_Product_Cost(*world, &target, BUILDING_TERRAFORMING) == 100 + 2 * COLCALC::misc_costs.terraforming_step);
    DOCTEST_CHECK(live.planets.at(*planet).n_terraforms == 0);
}

DOCTEST_TEST_CASE("explicit grant difficulty governs colony production, taxes, upkeep and growth") {
    test::s_game_state fixture;
    test::s_saved_value settings{MOX::settings};
    auto base = std::make_unique<game::s_game_state>();
    base->player_count = 1;
    base->players[0].objectives = PLAYER_OBJECTIVE_INDUSTRIALIST;
    const auto star = game::Create_Star(*base);
    DOCTEST_REQUIRE(star);
    const auto planet = game::Create_Planet(*base, {.star_id = *star, .size = PLANET_SIZE_HUGE, .climate = PLANET_CLIMATE_TERRAN, .food_per_farmer = 4});
    DOCTEST_REQUIRE(planet);
    const auto colony = game::Create_Colony(*base, {.owner = 0, .planet_id = *planet, .n_pops = 8});
    DOCTEST_REQUIRE(colony);
    base->stars.at(*star).orbit_planet_ids[0] = *planet;
    base->planets.at(*planet).colony_id = *colony;
    for (auto& pop : base->colonies.at(*colony).pop) { pop.group = 0; pop.profession = POP::PROF_WORKER; }
    ship_slots::Resize(*base, 1);
    s_ship_data hull{.id = game::id_ship{90000}};
    hull.owner = game::owner_slot{0};
    hull.d.size = SHIP_SIZE_DOOM_STAR;
    hull.position.Set({game::e_ship_location::at_star, *star});
    ship_slots::Put(*base, 0, hull);
    auto first = std::make_unique<game::s_game_state>(*base);
    auto second = std::make_unique<game::s_game_state>(*base);
    const game::s_tech_grant_context context{GAME_DIFFICULTY_IMPOSSIBLE, 0, 35123, 0, true};
    MOX::settings.game_difficulty = GAME_DIFFICULTY_TUTOR;
    COLCALC::Player_Gets_Tech_App(*first, &first->players[0], TECH_APP_JUMP_GATE, context);
    MOX::settings.game_difficulty = GAME_DIFFICULTY_AVERAGE;
    COLCALC::Player_Gets_Tech_App(*second, &second->players[0], TECH_APP_JUMP_GATE, context);
    DOCTEST_CHECK(first->colonies.at(*colony) == second->colonies.at(*colony));
    DOCTEST_CHECK(first->players[0] == second->players[0]);
    DOCTEST_CHECK(static_cast<int>(first->colonies.at(*colony).n_industry_recyclers) == GAME_DIFFICULTY_IMPOSSIBLE);
    const auto& empire = first->players[0];
    DOCTEST_REQUIRE(empire.command_points_used > empire.command_points);
    DOCTEST_CHECK(empire.maintenance[2] == (12 - context.difficulty) * (empire.command_points_used - empire.command_points));
}

DOCTEST_TEST_CASE("Doom Star refresh and instant upgrade use the detached player's equipment and cost") {
    test::s_game_state fixture;
    auto& live = game::current_state;
    live.player_count = 1;
    constexpr game::id_player player_id{0};
    const auto live_player = live.players[0];
    auto world = std::make_unique<game::s_game_state>(live);
    auto& player = world->players[0];
    player.traits[TRAIT_CURRENT_GOVERNMENT] = GOVERNMENT_DEMOCRACY;
    player.tech_applications[TECH_APP_TRITANIUM_ARMOR] = TECH_RESEARCH_STATUS_RESEARCHED;
    player.total_pop = 100;
    auto& template_hull = TECHDATA::hull_data[HULL_DOOM_STAR];
    test::s_saved_value saved_hull{template_hull};
    char name[] = "Doom Star";
    template_hull.name = name;
    template_hull.cost = 200;
    auto& armor = TECHDATA::armor[ARMOR_TRITANIUM_ARMOR];
    test::s_saved_value saved_armor{armor};
    armor.tech_app_id = TECH_APP_TRITANIUM_ARMOR;
    auto& reduction = COLCALC::government_bonuses[GOVERNMENT_DEMOCRACY].cost_reduction;
    test::s_saved_value saved_reduction{reduction};
    reduction = {1, 4};
    test::s_saved_value saved_date{MOX::stardate};
    MOX::stardate = 35000;

    const auto star = game::Create_Star(*world);
    DOCTEST_REQUIRE(star);
    const auto planet = game::Create_Planet(*world, {.star_id = *star});
    DOCTEST_REQUIRE(planet);
    const auto colony = game::Create_Colony(*world, {.owner = player_id, .planet_id = *planet});
    DOCTEST_REQUIRE(colony);
    constexpr game::id_ship key{90000};
    s_ship_data ship{.id = key};
    ship.owner = factions::From_Player(player_id);
    ship.position.Set({game::e_ship_location::at_star, *star});
    ship.d.ship_type = SHIP_TYPE_COMBAT;
    ship.d.size = SHIP_SIZE_DOOM_STAR;
    ship_slots::Resize(*world, 1);
    AIDATA::s_state ai{};
    auto& work = ai.players[0].colony_work;
    aidudes::s_ai_colony_context ctx{player_id, &player, &work, world.get(), GAME_DIFFICULTY_AVERAGE, 1, 35123, -1000};
    DOCTEST_SUBCASE("queued refresh") {
        ship.status = 6;
        ship_slots::Put(*world, 0, ship);
        world->colonies.at(*colony).producing[0] = game::Queued_Ship_Product(key);
        work.colonies.push_back({.colony_id = *colony});
        aidudes::Keep_Ship_Designs_Up_To_Date(&ctx);
    }
    DOCTEST_SUBCASE("instant upgrade") {
        ship_slots::Put(*world, 0, ship);
        ai.players[0].empire.colony_count = 2;
        ai.players[0].refits.push_back({key, 500});
        const auto seed = game_random::Get_Random_Seed();
        game_random::Set_Random_Seed(1);
        aidudes::Do_Ship_Upgrade_Cheats(&ai, &ctx);
        game_random::Set_Random_Seed(seed);
        DOCTEST_CHECK(ai.players[0].refits.empty());
    }
    const auto& installed = world->ships.at(key).d;
    DOCTEST_CHECK(installed.armor_type == ARMOR_TRITANIUM_ARMOR);
    DOCTEST_CHECK(installed.cost == 225);
    DOCTEST_CHECK(installed.date_of_design == 35123);
    DOCTEST_CHECK(live.players[0] == live_player);
    DOCTEST_CHECK(live.ships.empty());
}

DOCTEST_TEST_CASE("ground-tech assessment uses the supplied player's researched equipment") {
    test::s_game_state fixture;
    auto& live = game::current_state;
    live.player_count = 1;
    auto world = std::make_unique<game::s_game_state>(live);
    live.players[0].tech_applications[TECH_APP_POWERED_ARMOR] = TECH_RESEARCH_STATUS_RESEARCHED;
    world->players[0].tech_applications[TECH_APP_BATTLEOIDS] = TECH_RESEARCH_STATUS_RESEARCHED;
    AIDATA::s_state ai{};
    aitech::Compute_AI_Tech_Info(&ai, &live, GAME_DIFFICULTY_AVERAGE, 0);
    DOCTEST_CHECK(ai.players[0].technology.ground_tech_score == 8);
    aitech::Compute_AI_Tech_Info(&ai, world.get(), GAME_DIFFICULTY_AVERAGE, 0);
    DOCTEST_CHECK(ai.players[0].technology.ground_tech_score == 5);
    DOCTEST_CHECK(live.players[0].tech_applications[TECH_APP_BATTLEOIDS] != TECH_RESEARCH_STATUS_RESEARCHED);
}

DOCTEST_TEST_CASE("home climate and planet selection use the supplied orbits and spectral class") {
    test::s_game_state fixture;
    auto& live = game::current_state;
    const auto star = game::Create_Star(live);
    DOCTEST_REQUIRE(star);
    const auto live_planet = game::Create_Planet(live, {.star_id = *star});
    DOCTEST_REQUIRE(live_planet);
    live.stars.at(*star).orbit_planet_ids[0] = *live_planet;
    live.stars.at(*star).spectral_class = STAR_CLASS_F;

    auto detached = std::make_unique<game::s_game_state>(live);
    const auto planet = game::Create_Planet(*detached, {.star_id = *star, .planet_type = PLANET_TYPE_PLANET});
    DOCTEST_REQUIRE(planet);
    auto& system = detached->stars.at(*star);
    system.orbit_planet_ids[0] = *planet;
    system.spectral_class = STAR_CLASS_BLACK_HOLE;
    auto& home = detached->planets.at(*planet);
    home.climate = PLANET_CLIMATE_TERRAN;
    DOCTEST_CHECK(HOMEGEN::Random_Planet(*detached, *star) == *planet);
    DOCTEST_CHECK(HOMEGEN::Enforce_Climate(*detached, *star, PLANET_CLIMATE_TERRAN) == *planet);
    DOCTEST_CHECK(home.food_per_farmer == 0);

    test::s_saved_array saved_weights{MOX::climate_roll_table[PLANET_CLIMATE_TERRAN]};
    std::ranges::fill(MOX::climate_roll_table[PLANET_CLIMATE_TERRAN], 1);
    const auto original_star = live.stars.at(*star);
    const auto original_planet = live.planets.at(*live_planet);
    home.climate = PLANET_CLIMATE_BARREN;
    DOCTEST_CHECK(HOMEGEN::Enforce_Climate(*detached, *star, PLANET_CLIMATE_TERRAN) == *planet);
    DOCTEST_CHECK(home.climate == PLANET_CLIMATE_TERRAN);
    DOCTEST_CHECK(system.spectral_class < STAR_CLASS_BLACK_HOLE);
    DOCTEST_CHECK(home.food_per_farmer == MOX::food_per_farmer_table[PLANET_CLIMATE_TERRAN]);
    DOCTEST_CHECK(live.stars.at(*star) == original_star);
    DOCTEST_CHECK(live.planets.at(*live_planet) == original_planet);
}

DOCTEST_TEST_CASE("colony AI classifies, sorts and recalculates the supplied colonies") {
    test::s_game_state fixture;
    auto& live = game::current_state;
    live.player_count = 1;
    const game::id_player owner{0};
    live.players[0].objectives = PLAYER_OBJECTIVE_INDUSTRIALIST;
    live.players[0].do_not_research = 1;
    live.players[0].traits[TRAIT_CURRENT_GOVERNMENT] = GOVERNMENT_UNIFICATION;
    std::array<game::id_star, 2> stars;
    std::array<game::id_planet, 2> planets;
    std::array<game::id_colony, 2> colonies;
    for (int i = 0; i < 2; ++i) {
        const auto star = game::Create_Star(live);
        DOCTEST_REQUIRE(star);
        stars[i] = *star;
        const auto planet = game::Create_Planet(live, {.star_id = *star, .gravity_class = PLANET_GRAVITY_NORMAL_G});
        DOCTEST_REQUIRE(planet);
        planets[i] = *planet;
        const auto id = game::Create_Colony(live, {.owner = 0, .planet_id = *planet, .n_pops = 3});
        DOCTEST_REQUIRE(id);
        colonies[i] = *id;
        live.planets.at(*planet).colony_id = *id;
        live.planets.at(*planet).food_per_farmer = 2;
        live.stars.at(*star).orbit_planet_ids[0] = *planet;
        auto& colony = live.colonies.at(*id);
        std::ranges::fill(colony.producing, COLONY_PRODUCTION_NONE);
        colony.producing[0] = BUILDING_AUTOMATED_FACTORY;
        colony.bought_outright = 1;
        colony.food2_per_farmer = 2;
    }
    live.stars.at(stars[0]).blockaded.set(owner);
    auto world = std::make_unique<game::s_game_state>(live);
    world->stars.at(stars[0]).blockaded.reset(owner);
    world->stars.at(stars[1]).blockaded.set(owner);
    world->planets.at(planets[0]).food_per_farmer = 16;
    world->planets.at(planets[1]).food_per_farmer = 20;
    const auto old_colonies = live.colonies;
    const auto old_player = live.players[0];

    AIDATA::s_state ai{};
    aidudes::All_Colony_AI(&ai, world.get(), GAME_DIFFICULTY_TUTOR, 0, 35000, 0);
    const auto& work = ai.players[0].colony_work;
    DOCTEST_CHECK(work.complete);
    DOCTEST_REQUIRE(work.colonies.size() == 2);
    DOCTEST_CHECK(work.blockaded_count == 1);
    DOCTEST_CHECK(work.colonies[0].colony_id == colonies[1]);
    DOCTEST_CHECK(work.colonies[1].colony_id == colonies[0]);
    DOCTEST_CHECK(work.colonies[1].n_workers >= 1);
    DOCTEST_CHECK(world->colonies.at(colonies[0]).food2_per_farmer == 16);
    DOCTEST_CHECK(world->colonies.at(colonies[1]).food2_per_farmer == 20);
    DOCTEST_CHECK(live.colonies == old_colonies);
    DOCTEST_CHECK(live.players[0] == old_player);
}

DOCTEST_TEST_CASE("detached empire production recalculates the world whose order changed") {
    test::s_game_state fixture;
    auto& live = game::current_state;
    live.player_count = 1;
    const auto star = game::Create_Star(live);
    DOCTEST_REQUIRE(star);
    const auto planet = game::Create_Planet(live, {.star_id = *star});
    DOCTEST_REQUIRE(planet);
    const auto colony = game::Create_Colony(live, {.owner = 0, .planet_id = *planet, .n_pops = 1});
    DOCTEST_REQUIRE(colony);
    live.planets.at(*planet).food_per_farmer = 2;
    live.colonies.at(*colony).food2_per_farmer = 22;
    auto world = std::make_unique<game::s_game_state>(live);
    auto& draft = world->colonies.at(*colony);
    world->planets.at(*planet).food_per_farmer = 8;
    std::ranges::fill(draft.producing, COLONY_PRODUCTION_NONE);
    test::s_saved_value saved_rules{AIBUILD::rules};
    AIBUILD::rules.strategic.expansion.enabled = false;
    AIBUILD::rules.strategic.emergency_defense.enabled = false;
    AIBUILD::rules.strategic.economic_emergency.enabled = false;
    DOCTEST_SUBCASE("trade goods") {
        draft.producing[0] = COLONY_PRODUCTION_TRADE_GOODS;
    }
    DOCTEST_SUBCASE("economic emergency") {
        world->players[0].surplus_bc = -10;
        world->players[0].total_maintenance = 10;
        draft.production[ECON_INDUSTRY] = 5;
        AIBUILD::rules.strategic.economic_emergency.enabled = true;
        AIBUILD::rules.strategic.economic_emergency.product = COLONY_PRODUCTION_TRADE_GOODS;
    }
    const auto live_colony = live.colonies.at(*colony);
    const auto live_player = live.players[0];
    AIDATA::s_state ai{};
    aidudes::Assign_Empire_Building(&ai, world.get(), 0, 0, 35000, 0, game::id_player{0});
    DOCTEST_CHECK(draft.food2_per_farmer == 8);
    DOCTEST_CHECK(live.colonies.at(*colony) == live_colony);
    DOCTEST_CHECK(live.players[0] == live_player);
}

DOCTEST_TEST_CASE("detached empire production queues a new hull without allocating in the live roster") {
    test::s_game_state fixture;
    auto& live = game::current_state;
    live.player_count = 1;
    const auto star = game::Create_Star(live);
    DOCTEST_REQUIRE(star);
    const auto planet = game::Create_Planet(live, {.star_id = *star});
    DOCTEST_REQUIRE(planet);
    const auto colony = game::Create_Colony(live, {.owner = 0, .planet_id = *planet, .n_pops = 1});
    DOCTEST_REQUIRE(colony);
    auto world = std::make_unique<game::s_game_state>(live);
    world->next_ship_id = 90000;
    auto& player = world->players[0];
    player.ship_speed = player.ship_range = 5;
    player.ship_designs[SHIP_SIZE_SMALL].size = SHIP_SIZE_SMALL;
    player.ship_designs[SHIP_SIZE_SMALL].ship_type = SHIP_TYPE_COMBAT;
    strlcpy(player.ship_designs[SHIP_SIZE_SMALL].name, "Candidate", sizeof(player.ship_designs[SHIP_SIZE_SMALL].name));
    player.ship_designs[SHIP_SIZE_MEDIUM].cost = INT16_MAX;
    auto& order = world->colonies.at(*colony);
    std::ranges::fill(order.producing, COLONY_PRODUCTION_NONE);
    order.production[ECON_INDUSTRY] = 20;
    test::s_saved_value saved_rules{AIBUILD::rules};
    AIBUILD::rules.strategic.expansion.enabled = false;
    AIBUILD::rules.strategic.emergency_defense.enabled = false;
    AIBUILD::rules.strategic.economic_emergency.enabled = false;
    for (auto& choice : AIBUILD::rules.strategic.allocation.candidates) choice.enabled = false;
    auto& combat = AIBUILD::rules.strategic.allocation.candidates[AIBUILD::ALLOCATION_COMBAT_SHIPS];
    combat = {.enabled = true};
    combat.weight.base = 100;
    combat.weight.clamp.maximum = 100;
    AIDATA::s_state ai{};
    ai.players[0].build.combat_strength.additional_wanted = 10;

    char name_format[] = "%s %d";
    test::s_saved_value saved_format{ESTRINGS::strings[0x2a]};
    ESTRINGS::strings[0x2a] = name_format;
    DOCTEST_SUBCASE("matching live colony key") {}
    DOCTEST_SUBCASE("missing live colony key") { live.colonies.clear(); }

    const auto live_player = live.players[0];
    const auto live_next_id = live.next_ship_id;
    aidudes::Assign_Empire_Building(&ai, world.get(), 0, 1, 35000, 0, game::id_player{0});
    const auto key = game::Queued_Ship_Key(order.producing[0]);
    DOCTEST_REQUIRE(key == game::id_ship{90000});
    DOCTEST_REQUIRE(world->ships.contains(key));
    DOCTEST_CHECK(world->ships.at(key).status == 6);
    DOCTEST_CHECK(world->ships.at(key).position.star_id == *star);
    DOCTEST_CHECK(static_cast<int>(player.ships_built[SHIP_SIZE_SMALL]) == 1);
    DOCTEST_CHECK(world->ship_generations[0] == 1);
    DOCTEST_CHECK(live.ships.empty());
    DOCTEST_CHECK(live.ship_slots.empty());
    DOCTEST_CHECK(live.ship_generations[0] == 0);
    DOCTEST_CHECK(live.next_ship_id == live_next_id);
    DOCTEST_CHECK(live.players[0] == live_player);
}

DOCTEST_TEST_CASE("blockaded colonists farm according to their own planet's food output") {
    test::s_game_state fixture;
    auto& live = game::current_state;
    live.player_count = 1;
    live.players[0].objectives = PLAYER_OBJECTIVE_HUMAN;
    live.players[0].traits[TRAIT_CURRENT_GOVERNMENT] = GOVERNMENT_UNIFICATION;
    auto& government = COLCALC::government_bonuses[GOVERNMENT_UNIFICATION];
    test::s_saved_value saved_government{government};
    government.food = 0;
    const auto star = game::Create_Star(live);
    DOCTEST_REQUIRE(star);
    const auto planet = game::Create_Planet(live, {.star_id = *star, .gravity_class = PLANET_GRAVITY_NORMAL_G});
    DOCTEST_REQUIRE(planet);
    live.planets.at(*planet).food_per_farmer = 2;
    live.stars.at(*star).blockaded.set(game::id_player{0});
    const auto colony = game::Create_Colony(live, {.owner = 0, .planet_id = *planet, .n_pops = 3});
    DOCTEST_REQUIRE(colony);
    live.colonies.at(*colony).food2_per_farmer = 2;
    const auto original = live.colonies.at(*colony);
    auto rich_world = std::make_unique<game::s_game_state>(live);
    auto poor_world = std::make_unique<game::s_game_state>(live);
    rich_world->planets.at(*planet).food_per_farmer = 16;
    AIDATA::s_colony_assignment rich{.colony_id = *colony}, poor{.colony_id = *colony};
    aidudes::Do_Blockaded_Colony(rich_world.get(), &rich);
    aidudes::Do_Blockaded_Colony(poor_world.get(), &poor);
    DOCTEST_CHECK(rich_world->colonies.at(*colony).food2_per_farmer == 16);
    DOCTEST_CHECK(poor_world->colonies.at(*colony).food2_per_farmer == 2);
    DOCTEST_CHECK(rich.n_farmers < poor.n_farmers);
    DOCTEST_CHECK(live.colonies.at(*colony) == original);
}

DOCTEST_TEST_CASE("Dragon popup captures a star key at selection before roster order changes") {
    test::s_game_state fixture;
    auto& world = game::current_state;
    world.next_star_id = 90000;
    const auto origin = game::Create_Star(world);
    const auto target = game::Create_Star(world);
    DOCTEST_REQUIRE(origin);
    DOCTEST_REQUIRE(target);
    test::s_saved_array saved_map_fields{MOX::galaxy_map_star_field};
    test::s_saved_array saved_popup_fields{MAINPUPS::mp_fields};
    test::s_saved_value saved_mode{MAINPUPS::field_mode};
    MAINPUPS::mp_fields[2] = 4;
    MAINPUPS::mp_fields[25] = 5;
    MAINPUPS::field_mode = 1;
    MOX::galaxy_map_star_field[0] = 6;
    MOX::galaxy_map_star_field[1] = 7;
    int16_t accepted = 0;
    const auto selected = MAINPUPS::Evaluate_Dragon_New_Star_Input(7, -1, &accepted, *origin);
    DOCTEST_REQUIRE(accepted == 1);
    DOCTEST_CHECK(selected == *target);
    std::swap(world.star_order[0], world.star_order[1]);
    DOCTEST_CHECK(game::Star_Key_At_Row(world, 1) == *origin);
    DOCTEST_CHECK(selected == game::Star_Key_At_Row(world, 0));
    accepted = 0;
    DOCTEST_CHECK(MAINPUPS::Evaluate_Dragon_New_Star_Input(4, -1, &accepted, *origin) == game::NO_STAR);
    DOCTEST_CHECK(accepted == 1);
}

DOCTEST_TEST_CASE("detached ship prices use the priced hull without a live editor") {
    test::s_game_state fixture;
    auto world = std::make_unique<game::s_game_state>();
    world->player_count = 1;
    const auto player = world->players[0].id;
    world->players[0].traits[TRAIT_CURRENT_GOVERNMENT] = -1;
    auto& small = TECHDATA::hull_data[SHIP_SIZE_SMALL];
    auto& shield = TECHDATA::shields[SHIELD_CLASS_I_SHIELD];
    auto& computer = TECHDATA::computers[COMPUTER_ELECTRONIC_COMPUTER];
    auto& drive = TECHDATA::drives[DRIVE_NO_DRIVE];
    auto& armor = TECHDATA::armor[ARMOR_NO_ARMOR];
    test::s_saved_value saved_small{small};
    test::s_saved_value saved_shield{shield};
    test::s_saved_value saved_computer{computer};
    test::s_saved_value saved_drive{drive};
    test::s_saved_value saved_armor{armor};
    small.size = 100;
    small.cost = 100;
    drive.cost_per_ship_type[SHIP_SIZE_SMALL] = 0;
    armor.cost_percent = 0;
    shield.tech_app_id = computer.tech_app_id = TECH_APP_NO_TECH;
    shield.cost_per_ship_type[SHIP_SIZE_SMALL] = 12;
    shield.cost_per_ship_type[SHIP_SIZE_LARGE] = 90;
    computer.cost_by_ship_type[SHIP_SIZE_SMALL] = 9;
    computer.cost_by_ship_type[SHIP_SIZE_LARGE] = 60;
    s_ship_design design{};
    design.size = SHIP_SIZE_SMALL;
    design.shield_type = SHIELD_CLASS_I_SHIELD;
    design.computer_type = COMPUTER_ELECTRONIC_COMPUTER;
    MOX::design->ship_size = SHIP_SIZE_LARGE;

    const auto price = DESIGN::Calculate_Design(*world, player, &design, nullptr);
    DOCTEST_REQUIRE(price == 121);
    DOCTEST_CHECK(static_cast<int>(MOX::design->ship_size) == SHIP_SIZE_LARGE);
    MOX::design = nullptr;
    DOCTEST_CHECK(DESIGN::Calculate_Design(*world, player, &design, nullptr) == price);
    DOCTEST_CHECK(MOX::design == nullptr);
}

DOCTEST_TEST_CASE("attack forecast buys queued reinforcements at the supplied ship's price") {
    test::s_game_state fixture;
    auto world = std::make_unique<game::s_game_state>();
    world->player_count = 2;
    const auto star = game::Create_Star(*world);
    DOCTEST_REQUIRE(star);
    const auto planet = game::Create_Planet(*world, {.star_id = *star});
    DOCTEST_REQUIRE(planet);
    const auto colony = game::Create_Colony(*world, {.owner = 1, .planet_id = *planet});
    DOCTEST_REQUIRE(colony);
    constexpr game::id_ship attacker{70000}, queued{70001};
    s_ship_data attacking{};
    attacking.id = attacker;
    attacking.owner = game::owner_slot{0};
    world->ships.emplace(attacker, attacking);
    s_ship_data reinforcement{};
    reinforcement.id = queued;
    reinforcement.owner = game::owner_slot{1};
    reinforcement.d.ship_type = SHIP_TYPE_COMBAT;
    reinforcement.d.cost = 100;
    reinforcement.d.ship_weapon[0] = {.type = WEAPON_LASER_CANNON, .count = 1, .current_count = 1, .specials = WEAPON_MOD_FLAG_SHIELD_PIERCING};
    auto& weapon = TECHDATA::weapons[WEAPON_LASER_CANNON];
    test::s_saved_value saved_weapon{weapon};
    weapon.max_damage = 100;
    weapon.type = WEAPON_TYPE_BEAM;
    world->ships.emplace(queued, reinforcement);
    reinforcement.d.cost = 10;
    game::current_state.ships.emplace(queued, reinforcement);
    world->colonies.at(*colony).producing[0] = game::Queued_Ship_Product(queued);
    const auto cheap = COLCALC::Production_Cost_To_Buy(10, 0);
    const auto expensive = COLCALC::Production_Cost_To_Buy(100, 0);
    DOCTEST_REQUIRE(cheap > 0);
    DOCTEST_REQUIRE(cheap < expensive);
    AIDATA::s_state ai{};
    AIDATA::s_movement_workspace work{};
    AIMOVE::s_ai_movement_context ctx{game::id_player{0}, &world->players[0], &work, world.get(), 0, 0};
    ai.ships[attacker].strengths[1] = 8;
    ai.stars[*star].possible_strengths[1][0][1] = 1;
    DOCTEST_REQUIRE(AIPOWER::Ship_Strength_Vs_Ships(world.get(), 0, queued, game::owner_slot{0}) > 6);
    const auto would_flee = [&](int32_t budget) {
        world->players[1].bc = budget;
        int8_t attack_type = 0;
        uint8_t flee = 0;
        AIMOVE::Compute_Attack_Worthiness(&ai, &ctx, *colony, std::span{&attacker, 1}, 1, &attack_type, &flee, nullptr, 0);
        return flee != 0;
    };
    DOCTEST_CHECK_FALSE(would_flee(cheap));
    DOCTEST_CHECK(would_flee(expensive));
    game::current_state.ships.erase(queued);
    DOCTEST_CHECK_FALSE(would_flee(cheap));
}

DOCTEST_TEST_CASE("detached leader tech grants refresh designs, fleet range, and colony production") {
    test::s_game_state fixture;
    auto& fuel = TECHDATA::fuel[FUEL_COUNT - 1];
    test::s_saved_value saved_fuel{fuel};
    fuel.tech_app_id = TECH_APP_DEUTERIUM_FUEL_CELLS;
    fuel.ship_range = 9;
    auto& armor = TECHDATA::armor[ARMOR_TRITANIUM_ARMOR];
    test::s_saved_value saved_armor{armor};
    armor.tech_app_id = TECH_APP_TRITANIUM_ARMOR;
    test::s_saved_value hull_space{TECHDATA::hull_data[SHIP_SIZE_SMALL].size};
    TECHDATA::hull_data[SHIP_SIZE_SMALL].size = 100;
    test::s_saved_value microlite{COLCALC::production_worker_bonus.microlite_construction};
    COLCALC::production_worker_bonus.microlite_construction = 2;
    auto world = std::make_unique<game::s_game_state>();
    world->player_count = 1;
    auto& player = world->players[0];
    player.objectives = PLAYER_OBJECTIVE_HUMAN;
    player.ship_range = -1;
    const auto star = game::Create_Star(*world);
    DOCTEST_REQUIRE(star);
    const auto planet = game::Create_Planet(*world, {.star_id = *star});
    DOCTEST_REQUIRE(planet);
    const auto colony_id = game::Create_Colony(*world, {.owner = 0, .planet_id = *planet, .n_pops = 1});
    DOCTEST_REQUIRE(colony_id);
    auto& colony = world->colonies.at(*colony_id);
    colony.pop[0].profession = POP::PROF_WORKER;
    test_ships::Rows(*world).Set_Key_At_Row(0, game::id_ship{70000});
    auto& ship = world->ships.at(game::id_ship{70000});
    ship.owner = factions::From_Player(player.id);
    ship.status = 6;
    ship.d.armor_type = ARMOR_TITANIUM_ARMOR;
    game::current_state.player_count = 1;
    game::current_state.players[0].ship_range = 2;
    const auto live = game::current_state.players[0];

    constexpr game::s_tech_grant_context tactical{GAME_DIFFICULTY_TUTOR, 0, 35123, -1000, true};
    test::s_saved_value saved_mode{MOX::settings.strategic_combat_flag};
    MOX::settings.strategic_combat_flag = 1;
    COLCALC::Player_Gets_Tech_App(*world, &player, fuel.tech_app_id, tactical);
    DOCTEST_CHECK(player.tech_applications[fuel.tech_app_id] == TECH_RESEARCH_STATUS_RESEARCHED);
    DOCTEST_CHECK(player.ship_range == fuel.ship_range);
    const auto industry = colony.industry_per_worker;
    COLCALC::Player_Gets_Tech_App(*world, &player, TECH_APP_MICROLITE_CONSTRUCTION, tactical);
    DOCTEST_CHECK(colony.industry_per_worker == industry + 2);
    DOCTEST_CHECK(player.industry_produced == colony.production[ECON_INDUSTRY]);
    COLCALC::Player_Gets_Tech_App(*world, &player, TECH_APP_TRITANIUM_ARMOR, tactical);
    DOCTEST_CHECK(player.ship_designs[0].armor_type == ARMOR_TRITANIUM_ARMOR);
    DOCTEST_CHECK(ship.d.armor_type == ARMOR_TRITANIUM_ARMOR);
    auto strategic = tactical;
    strategic.strategic_combat = 1;
    MOX::settings.strategic_combat_flag = 0;
    test::s_saved_array saved_hulls{TECHDATA::hull_data};
    char hull_name[] = "Hull";
    for (auto& hull : TECHDATA::hull_data) hull.name = hull_name;
    COLCALC::Player_Gets_Tech_App(*world, &player, TECH_APP_TRITANIUM_ARMOR, strategic);
    DOCTEST_CHECK(player.ship_designs[SHIP_SIZE_SMALL].armor_type == ARMOR_TRITANIUM_ARMOR);
    DOCTEST_CHECK(player.ship_designs[SHIP_SIZE_SMALL].date_of_design == 35123);
    DOCTEST_CHECK(game::current_state.players[0] == live);
}

DOCTEST_TEST_CASE("detached uncreative tech grants repair both research paths with supplied eligibility") {
    test::s_game_state fixture;
    auto world = std::make_unique<game::s_game_state>();
    world->player_count = 1;
    auto& player = world->players[0];
    player.traits[TRAIT_UNCREATIVE] = 1;
    constexpr int field = 7;
    constexpr int successor = 75;
    constexpr auto granted = TECH_APP_JUMP_GATE;
    constexpr auto remaining = TECH_APP_DIMENSIONAL_PORTAL;
    auto& field_data = TECHDATA::technology_fields[field];
    test::s_saved_value saved_field{field_data};
    field_data = {};
    field_data.tech[0] = granted;
    field_data.tech[1] = remaining;
    field_data.next_field_id = successor;
    auto& grant_data = TECHDATA::technology_applications[granted];
    auto& candidate_data = TECHDATA::technology_applications[remaining];
    test::s_saved_value saved_grant{grant_data};
    test::s_saved_value saved_candidate{candidate_data};
    grant_data.tech_field_id = field;
    candidate_data.in_strategic = 0;
    player.tech_fields[field] = 2;
    player.tech_applications[remaining] = TECH_RESEARCH_STATUS_UNAVAILABLE;
    test::s_saved_value saved_policy{COLCALC::keep_research_on_finds};
    COLCALC::keep_research_on_finds = 1;
    test::s_saved_value saved_settings{MOX::settings};
    MOX::settings.strategic_combat_flag = 1;
    MOX::settings.xenons_exist = 0;
    DOCTEST_SUBCASE("the current application was granted") {
        player.current_research_field = field;
        player.current_research_application = granted;
    }
    DOCTEST_SUBCASE("a different application was granted") {
        player.current_research_application = TECH_APP_NO_TECH;
    }
    const auto live = game::current_state.players[0];
    COLCALC::Player_Gets_Tech_App(*world, &player, granted, {GAME_DIFFICULTY_TUTOR, 0, 35000, 0, true});
    DOCTEST_CHECK(player.tech_applications[remaining] == TECH_RESEARCH_STATUS_AVAILABLE);
    DOCTEST_CHECK(player.tech_fields[field] == 2);
    DOCTEST_CHECK(player.tech_fields[successor] != 2);
    if (player.current_research_field == field) DOCTEST_CHECK(player.current_research_application == remaining);
    DOCTEST_CHECK(game::current_state.players[0] == live);
}

DOCTEST_TEST_CASE("detached ETA uses the supplied nebula geometry, not the session geometry") {
    test::s_game_state fixture;
    auto world = std::make_unique<game::s_game_state>();
    const auto destination = game::Create_Star(*world);
    DOCTEST_REQUIRE(destination);
    world->stars.at(*destination).x = 120;
    constexpr game::id_ship key{90000};
    s_ship_data ship{.status = 1, .id = key};
    ship.owner = game::owner_slot{0};
    ship.position.Set({game::e_ship_location::in_flight, *destination});
    ship.travelling_speed = 2;
    ship_slots::Resize(*world, 1);
    ship_slots::Put(*world, 0, ship);

    struct Animation {
        s_animation_header header{};
        uint32_t offset = offsetof(Animation, pixel);
        uint8_t pixel = 6;
    } animation;
    animation.header.width = animation.header.height = animation.header.frame_count = 1;
    test::s_saved_value picture{MOX::nebula_pict_seg[0][3]};
    test::s_saved_value nebula_ptr{MOX::nebula};
    test::s_saved_value nebula_count{MOX::NUM_NEBULAS};
    MOX::nebula_pict_seg[0][3] = &animation.header;
    const s_nebula supplied[]{{30, 0, 0}};
    s_nebula session[]{supplied[0]};
    MOX::nebula = session;
    MOX::NUM_NEBULAS = 1;

    SHIPMOVE::Update_Ship_ETAs(*world, game::NO_PLAYER);
    DOCTEST_CHECK(world->ships.at(key).turns_left == 2);
    SHIPMOVE::Update_Ship_ETAs(*world, game::NO_PLAYER, supplied);
    DOCTEST_CHECK(world->ships.at(key).turns_left == 3);
    const auto planet = game::Create_Planet(*world, {.star_id = *destination});
    DOCTEST_REQUIRE(planet);
    const auto colony = game::Create_Colony(*world, {.owner = 1, .planet_id = *planet});
    DOCTEST_REQUIRE(colony);
    world->stars.at(*destination).orbit_planet_ids[0] = *planet;
    world->planets.at(*planet).colony_id = *colony;
    world->player_count = 2;
    world->colonies.at(*colony).buildings[BUILDING_WARP_INTERDICTOR] = 1;
    world->stars.at(*destination).has_warp_field_interdictor.set_slot(1);
    ERICNET::Remove_Building(*world, *colony, BUILDING_WARP_INTERDICTOR, supplied);
    DOCTEST_CHECK(world->colonies.at(*colony).buildings[BUILDING_WARP_INTERDICTOR] == 0);
    s_ship_move_info info{};
    const game::id_ship fleet[]{key};
    const game::s_star_destination target{game::e_star_destination::star, *destination};
    DOCTEST_REQUIRE(SHIPMOVE::Evaluate_Fleet_Move(*world, fleet, target, &info, std::nullopt, supplied) == SHIPMOVE::e_fleet_evaluation::can_move);
    DOCTEST_CHECK(info.turns_left == 3);
    DOCTEST_CHECK(world->ships.at(key).turns_left == 3);
    session[0].x = 300;
    world->colonies.at(*colony).buildings[BUILDING_WARP_INTERDICTOR] = 1;
    world->stars.at(*destination).has_warp_field_interdictor.set_slot(1);
    ERICNET::Remove_Building(*world, *colony, BUILDING_WARP_INTERDICTOR, supplied);
    DOCTEST_REQUIRE(SHIPMOVE::Evaluate_Fleet_Move(*world, fleet, target, &info, std::nullopt, supplied) == SHIPMOVE::e_fleet_evaluation::can_move);
    DOCTEST_CHECK(info.turns_left == 3);
    DOCTEST_CHECK(world->ships.at(key).turns_left == 3);
    DOCTEST_CHECK(game::current_state.ships.empty());
}

DOCTEST_TEST_CASE("detached route caches and high-level planning share supplied geometry including in-flight alternatives") {
    test::s_game_state fixture;
    auto world = std::make_unique<game::s_game_state>();
    world->player_count = 1;
    auto& player = world->players[0];
    player.objectives = PLAYER_OBJECTIVE_HUMAN;
    player.ship_speed = 2;
    player.ship_range = 100;
    player.tech_applications[TECH_APP_HYPERSPACE_COMMUNICATIONS] = TECH_RESEARCH_STATUS_RESEARCHED;
    const auto source = game::Create_Star(*world);
    const auto target = game::Create_Star(*world);
    const auto alternate = game::Create_Star(*world);
    DOCTEST_REQUIRE(source);
    DOCTEST_REQUIRE(target);
    DOCTEST_REQUIRE(alternate);
    world->stars.at(*target).x = 120;
    world->stars.at(*alternate).x = 90;
    for (auto key : {*source, *target, *alternate}) world->stars.at(key).has_colony.set_slot(0);
    const auto planet = game::Create_Planet(*world, {.star_id = *source, .food_per_farmer = 4});
    DOCTEST_REQUIRE(planet);
    const auto colony = game::Create_Colony(*world, {.owner = 0, .planet_id = *planet, .n_pops = 1});
    DOCTEST_REQUIRE(colony);
    world->stars.at(*source).orbit_planet_ids[0] = *planet;
    world->planets.at(*planet).colony_id = *colony;
    constexpr game::id_ship ship_key{90000};
    ship_slots::Resize(*world, 1);
    s_ship_data ship{.status = 1, .id = ship_key};
    ship.owner = game::owner_slot{0};
    ship.position.Set({game::e_ship_location::in_flight, *target});
    ship.travelling_speed = 2;
    ship.turns_left = 3;
    ship_slots::Put(*world, 0, ship);
    struct Animation { s_animation_header header{}; uint32_t offset = offsetof(Animation, pixel); uint8_t pixel = 6; } animation;
    animation.header.width = animation.header.height = animation.header.frame_count = 1;
    test::s_saved_value picture{MOX::nebula_pict_seg[0][3]};
    test::s_saved_value nebula_ptr{MOX::nebula};
    test::s_saved_value nebula_count{MOX::NUM_NEBULAS};
    MOX::nebula_pict_seg[0][3] = &animation.header;
    const s_nebula geometry[]{{30, 0, 0}};
    s_nebula live_geometry[]{{300, 0, 0}};
    MOX::nebula = live_geometry;
    MOX::NUM_NEBULAS = 1;
    const game::s_planning_context inputs{geometry};
    AIDATA::s_state ai{};
    DOCTEST_REQUIRE(AIDATA::Compute_AI_Data(&ai, world.get(), GAME_DIFFICULTY_TUTOR, 0, 35000, inputs));
    DOCTEST_CHECK(AIDATA::N_Turns_From_To(&ai.routes, *source, *target, player.id).value_or(0) == 3);
    DOCTEST_CHECK(ai.routes.ship_direct.Find(ship_key, *alternate).value_or(0) == 2);
    AIMOVE::s_ai_movement_context ctx{player.id, &player, nullptr, world.get(), 0, 0, ai.planning};
    const game::id_ship fleet[]{ship_key};
    auto route = AIMOVE::Move_Ships_With_Possible_Intermediate(&ctx, fleet, *target);
    DOCTEST_REQUIRE(route);
    DOCTEST_CHECK(route->turns == 3);
    live_geometry[0].x = 30;
    route = AIMOVE::Move_Ships_With_Possible_Intermediate(&ctx, fleet, *target);
    DOCTEST_REQUIRE(route);
    DOCTEST_CHECK(route->turns == 3);
    DOCTEST_CHECK(game::current_state.ships.empty());
}

DOCTEST_TEST_CASE("detached settlement and fleet preflight consult supplied event values") {
    test::s_game_state fixture;
    test::s_saved_array saved_events{EVENTS::event_data};
    auto world = std::make_unique<game::s_game_state>();
    world->player_count = 1;
    world->next_colony_id = 70000;
    const auto from = game::Create_Star(*world);
    const auto to = game::Create_Star(*world);
    DOCTEST_REQUIRE(from);
    DOCTEST_REQUIRE(to);
    world->stars.at(*to).x = 120;
    const auto p1 = game::Create_Planet(*world, {.star_id = *from, .size = PLANET_SIZE_HUGE, .climate = PLANET_CLIMATE_TERRAN});
    const auto p2 = game::Create_Planet(*world, {.star_id = *to, .size = PLANET_SIZE_HUGE, .climate = PLANET_CLIMATE_TERRAN});
    DOCTEST_REQUIRE(p1);
    DOCTEST_REQUIRE(p2);
    const auto c1 = game::Create_Colony(*world, {.owner = 0, .planet_id = *p1, .n_pops = 2});
    const auto c2 = game::Create_Colony(*world, {.owner = 0, .planet_id = *p2, .n_pops = 1});
    DOCTEST_REQUIRE(c1);
    DOCTEST_REQUIRE(c2);
    world->players[0].n_freighters = 5;
    world->players[0].ship_range = 100;
    world->players[0].ship_speed = 2;
    world->stars.at(*from).has_colony.set_slot(0);
    world->stars.at(*to).has_colony.set_slot(0);
    EVENTS::event_data[EVENT_TIME_SPACE_ANOMALY].status = 2;
    EVENTS::event_data[EVENT_TIME_SPACE_ANOMALY].star_target = *from;
    EVENTS::event_data[EVENT_PLAGUE].status = 2;
    EVENTS::event_data[EVENT_PLAGUE].colony_target = *c1;
    EVENTS::event_data[EVENT_HYPERSPACE_FLUX].status = 2;
    s_settler_settle_result result{};
    SETTLER::Pop_Tries_To_Settle(*world, *c1, 0, *c2, &result);
    DOCTEST_CHECK(result.source_space_anomaly == 0);
    DOCTEST_CHECK(result.source_plague == 0);
    DOCTEST_CHECK(result.can_settle == 1);
    auto inputs = game::s_planning_context{};
    inputs.anomaly_star = *from;
    SETTLER::Pop_Tries_To_Settle(*world, *c1, 0, *c2, &result, inputs);
    DOCTEST_CHECK(result.source_space_anomaly == 1);
    inputs.anomaly_star = game::NO_STAR;
    inputs.plague_colony = *c1;
    SETTLER::Pop_Tries_To_Settle(*world, *c1, 0, *c2, &result, inputs);
    DOCTEST_CHECK(result.source_plague == 1);
    ship_slots::Resize(*world, 1);
    s_ship_data ship{.id = game::id_ship{90000}};
    ship.owner = game::owner_slot{0};
    ship.position.Set({game::e_ship_location::at_star, *from});
    ship_slots::Put(*world, 0, ship);
    const game::id_ship fleet[]{ship.id};
    s_ship_move_info move{};
    const game::s_star_destination dest{game::e_star_destination::star, *to};
    DOCTEST_CHECK(SHIPMOVE::Evaluate_Fleet_Move(*world, fleet, dest, &move) == SHIPMOVE::e_fleet_evaluation::can_move);
    inputs.hyperspace_flux = true;
    DOCTEST_CHECK(SHIPMOVE::Evaluate_Fleet_Move(*world, fleet, dest, &move, inputs) == SHIPMOVE::e_fleet_evaluation::blocked);
    DOCTEST_CHECK(move.hyperspace_flux == 1);
}

DOCTEST_TEST_CASE("AI hiring and rescue change only the supplied roster") {
    test::s_game_state fixture;
    auto storage = std::make_unique<game::s_game_state>();
    auto& world = *storage;
    world.player_count = 1;
    world.players[0].objectives = PLAYER_OBJECTIVE_MILITARIST;
    world.players[0].bc = 1000;
    world.players[0].officer_for_hire = game::id_leader{0};
    world.leaders[0].id = game::id_leader{0};
    world.leaders[0].type = LEADER_TYPE_SHIP;
    world.leaders[0].special_skills = LEADER_SHIP_SKILL_HELMSMAN;
    world.leaders[0].skill_value = 10;
    game::current_state.leaders[0].skill_value = 1000;
    const auto rescue = game::Create_Star(world, {.system_special = 80});
    DOCTEST_REQUIRE(rescue);
    world.leaders[1].id = game::id_leader{1};
    world.leaders[1].type = LEADER_TYPE_SHIP;
    world.leaders[1].status = 5;
    world.leaders[1].Set_Star_Location(*rescue);

    const auto expected_cost = OFFICER::Officer_Cost(world, game::id_player{0}, world.leaders[0].id);
    DOCTEST_REQUIRE(expected_cost > 0);
    AIDATA::s_state ai{};
    aileader::Do_AI_Leaders(&ai, &world, {GAME_DIFFICULTY_TUTOR, 0, 35000, 0, true});
    DOCTEST_CHECK(world.players[0].bc == 1000 - expected_cost);
    DOCTEST_CHECK(world.players[0].officer_for_hire == game::NO_LEADER);
    DOCTEST_CHECK(world.leaders[0].employer == game::id_player{0});
    DOCTEST_CHECK(world.leaders[0].status == 0);
    DOCTEST_CHECK(world.leaders[1].employer == game::id_player{0});
    DOCTEST_CHECK(world.leaders[1].star_location == game::NO_STAR);
    DOCTEST_CHECK(game::current_state.leaders[0].employer == game::NO_PLAYER);
    DOCTEST_CHECK(game::current_state.leaders[1].employer == game::NO_PLAYER);
}

DOCTEST_TEST_CASE("destroying a detached colony clears its own officer and system flags") {
    test::s_game_state fixture;
    auto storage = std::make_unique<game::s_game_state>();
    auto& world = *storage;
    world.player_count = 1;
    const auto star = game::Create_Star(world);
    DOCTEST_REQUIRE(star);
    const auto planet = game::Create_Planet(world, {.star_id = *star});
    DOCTEST_REQUIRE(planet);
    const auto colony = game::Create_Colony(world, {.owner = 0, .planet_id = *planet});
    DOCTEST_REQUIRE(colony);
    world.stars.at(*star).orbit_planet_ids[0] = *planet;
    world.stars.at(*star).officer_id[0] = game::id_leader{0};
    world.stars.at(*star).has_colony.set(game::id_player{0});
    world.leaders[0].id = game::id_leader{0};
    world.leaders[0].employer = game::id_player{0};
    world.leaders[0].type = LEADER_TYPE_COLONY;
    world.leaders[0].status = 1;
    world.leaders[0].Set_Star_Location(*star);
    world.colonies.at(*colony).bought_outright = 20;
    const auto live_cash = game::current_state.players[0].bc;

    BOMB::Destroy_Colony(world, *colony, -1);
    DOCTEST_CHECK_FALSE(world.colonies.contains(*colony));
    DOCTEST_CHECK(world.planets.at(*planet).colony_id == game::NO_COLONY);
    DOCTEST_CHECK_FALSE(world.stars.at(*star).has_colony.any());
    DOCTEST_CHECK(world.stars.at(*star).officer_id[0] == game::NO_LEADER);
    DOCTEST_CHECK(world.leaders[0].status == -2);
    DOCTEST_CHECK(world.players[0].bc > 0);
    DOCTEST_CHECK(game::current_state.players[0].bc == live_cash);
    DOCTEST_CHECK(game::current_state.leaders[0].status == -1);
}

using game::id_ship;
using legacy_world::Ship_Id;
using legacy_world::Star_Id;
using legacy_world::Player_Id;

DOCTEST_TEST_CASE("AI ship order uses the supplied roster rows for orbiting ships") {
    auto state_storage = std::make_unique<game::s_game_state>();
    auto& state = *state_storage;
    DOCTEST_REQUIRE(game::Create_Star(state));
    const auto second = game::Create_Star(state);
    state.next_star_id = 90000;
    const auto first = game::Replace_Draft_Star(state, 0);
    DOCTEST_REQUIRE(first);
    DOCTEST_REQUIRE(second);

    s_ship_data ships[4]{};
    const game::id_ship keys[] = {{90003}, {90001}, {90000}, {90002}};
    for (int row = 0; row < 4; ++row) ships[row].id = keys[row];
    ships[0].position.Set({game::e_ship_location::at_star, *first});
    ships[1].position.Set({game::e_ship_location::at_star, *second});
    ships[2].position.Set({game::e_ship_location::at_star, game::NO_STAR, game::e_ship_target::antares});
    ships[3].position.Set({game::e_ship_location::in_flight, *first});
    test_ships::Rows(state).assign(std::begin(ships), std::end(ships));
    ship_slots::Resize(state, 4);
    state.next_ship_id = 90004;
    auto& world = state;

    DOCTEST_CHECK(AIDATA::Compare_Combat_Ships(world, keys[0], keys[1]) < 0);
    DOCTEST_CHECK(AIDATA::Compare_Combat_Ships(world, keys[1], keys[2]) < 0);
    DOCTEST_CHECK(AIDATA::Compare_Combat_Ships(world, keys[2], keys[3]) < 0);
    test_ships::Rows(state)[1].owner = factions::From_Legacy(1);
    DOCTEST_CHECK(AIDATA::Compare_Combat_Ships(world, keys[1], keys[3]) > 0);
}

DOCTEST_TEST_CASE("advanced start ranking keeps planet identities attached to scores") {
    s_adv_civ_player_planet candidates[] = {
        {.worth = 20, .planet_id = game::id_planet{90001}},
        {.worth = 5, .planet_id = game::id_planet{12}},
        {.worth = 30, .planet_id = game::id_planet{70000}},
    };
    Legacy_Qsort(candidates, std::size(candidates), sizeof(candidates[0]), ADVCIV::qsort_planet_worth);
    DOCTEST_CHECK(candidates[0].planet_id == game::id_planet{70000});
    DOCTEST_CHECK(candidates[1].planet_id == game::id_planet{90001});
    DOCTEST_CHECK(candidates[2].planet_id == game::id_planet{12});
}

DOCTEST_TEST_CASE("advanced start candidates resolve sparse orbit IDs in orbit order") {
    auto state_storage = std::make_unique<game::s_game_state>();
    auto& state = *state_storage;
    state.player_count = 1;
    DOCTEST_REQUIRE(game::Create_Star(state));
    state.next_star_id = 90000;
    const auto system = *game::Create_Star(state);
    const s_planet_data unoccupied{.star_id = system, .planet_type = PLANET_TYPE_PLANET};
    state.planets.emplace(game::id_planet{17}, unoccupied);
    state.planets.emplace(game::id_planet{70000}, unoccupied);
    auto& star = state.stars.at(system);
    star.orbit_planet_ids[0] = game::id_planet{70000};
    star.orbit_planet_ids[1] = game::id_planet{18}; // A missing key must not resolve to another record.
    star.orbit_planet_ids[2] = game::id_planet{17};

    s_adv_civ_player candidates{};
    candidates.stars[1].star_slot = 1;
    auto* previous = ADVCIV::adv_civ_player;
    ADVCIV::adv_civ_player = &candidates;
    ADVCIV::Build_Adv_Civ_Planet_List(state);
    ADVCIV::adv_civ_player = previous;

    DOCTEST_REQUIRE(candidates.candidate_planet_count == 2);
    DOCTEST_CHECK(candidates.planets[0].planet_id == game::id_planet{70000});
    DOCTEST_CHECK(candidates.planets[1].planet_id == game::id_planet{17});
}

DOCTEST_TEST_CASE("advanced start reservations follow star IDs rather than slots or live colony flags") {
    auto state_storage = std::make_unique<game::s_game_state>();
    auto& state = *state_storage;
    const auto empty = *game::Create_Star(state);
    state.next_star_id = 90000;
    const auto reserved = *game::Create_Star(state);
    const game::id_player owner{1};
    const ADVCIV::planned_colony_owners owners{{reserved, owner}};

    DOCTEST_CHECK_FALSE(state.stars.at(reserved).has_colony.test(owner));
    DOCTEST_CHECK(game::Star_Slot(state, reserved) == 1);
    DOCTEST_CHECK(ADVCIV::Planned_Colony_Owner(owners, reserved) == owner);

    std::swap(state.star_order[0], state.star_order[1]);
    state.star_order_sorted = false;
    DOCTEST_CHECK(game::Star_Slot(state, reserved) == 0);
    DOCTEST_CHECK(game::Star_Key_At_Slot(state, 1) == empty);
    DOCTEST_CHECK(ADVCIV::Planned_Colony_Owner(owners, reserved) == owner);
    DOCTEST_CHECK(ADVCIV::Planned_Colony_Owner(owners, empty) == game::NO_PLAYER);
    DOCTEST_CHECK(ADVCIV::Planned_Colony_Owner(owners, game::NO_STAR) == game::NO_PLAYER);

    // The scoring path also consults the plan, not the live system bitmap.
    s_adv_civ_player candidates{};
    for (auto& candidate : candidates.stars) candidate.star_slot = -1;
    std::array<s_adv_civ_player, 2> player_scratch{};
    player_scratch[owner.id] = candidates;
    test::s_saved_value saved_players(ADVCIV::adv_civ_player);
    ADVCIV::adv_civ_player = player_scratch.data();
    s_planet_data planet{.star_id = reserved, .planet_type = PLANET_TYPE_PLANET};
    auto& player = state.players[owner.id];
    int32_t worth = 0;
    int8_t has_colony = 0;
    int16_t proximity_bonus = 0;
    ADVCIV::Get_Adv_Civ_Planet_Worthiness(state, owners, planet, player, &worth, &has_colony, &proximity_bonus);
    DOCTEST_CHECK(has_colony == 1);
    ADVCIV::Get_Adv_Civ_Planet_Worthiness(state, {}, planet, player, &worth, &has_colony, &proximity_bonus);
    DOCTEST_CHECK(has_colony == 0);
}

DOCTEST_TEST_CASE("nested game-state fixtures restore the outer roster") {
    test::s_game_state outer;
    outer.ships.resize(2);
    ship_slots::Resize(game::current_state, 2);
    game::current_state.player_count = 3;
    {
        test::s_game_state inner;
        DOCTEST_CHECK(game::current_state.ship_slots.size() == 0);
        DOCTEST_CHECK(game::current_state.player_count == 0);
    }
    DOCTEST_CHECK(game::current_state.ship_slots.size() == 2);
    DOCTEST_CHECK(game::current_state.player_count == 3);
}

DOCTEST_TEST_CASE("technology defaults bind to canonical starting fields") {
    test::s_saved_array fields{MOX::first_field_in_group};
    DOCTEST_CHECK(&TECHDATA::starting_techfield.biology == &MOX::first_field_in_group[1]);
    DOCTEST_CHECK(&TECHDATA::starting_techfield.xenon == &MOX::first_field_in_group[9]);
    MOX::first_field_in_group[1] = 17;
    MOX::first_field_in_group[9] = 75;
    DOCTEST_CHECK(TECHDATA::starting_techfield.biology == 17);
    DOCTEST_CHECK(TECHDATA::starting_techfield.xenon == 75);
}

DOCTEST_TEST_CASE("refit desirability uses the supplied owner and assessment") {
    s_player owner{};
    s_ship_data ship{};
    AIDATA::s_technology_assessment assessment;
    assessment.best_armor = 2;
    owner.ship_designs[SHIP_SIZE_SMALL] = ship.d;
    DOCTEST_CHECK(AIDATA::Refit_Desirability(&owner, &assessment, &ship) == 0);
    owner.ship_designs[SHIP_SIZE_SMALL].cost = 1;
    DOCTEST_CHECK(AIDATA::Refit_Desirability(&owner, &assessment, &ship) > 0);
}

DOCTEST_TEST_CASE("AI strength and technology leaves read supplied records instead of world globals") {
    s_player players[3] = {};
    players[1].objectives = PLAYER_OBJECTIVE_HUMAN;
    players[2].objectives = PLAYER_OBJECTIVE_MILITARIST;
    AIDATA::s_ship_assessment ship_assessment;
    ship_assessment.strengths[1] = 10;
    ship_assessment.strengths[2] = 40;
    AIDATA::s_empire_assessment empire;
    empire.reachable_player_count = 2;
    empire.reachable_players[1] = empire.reachable_players[2] = true;
    s_ship_data ship{};
    AIBUILD::s_opponent_view opponents{players, 3, 1};
    DOCTEST_CHECK(AIBUILD::Mean_Ship_Strength(&opponents, &ship_assessment, &empire, &ship) == 25);
    opponents.human_opponent_bias = 3;
    DOCTEST_CHECK(AIBUILD::Mean_Ship_Strength(&opponents, &ship_assessment, &empire, &ship) == 17);

    s_tech_armor_data armor[ARMOR_COUNT] = {};
    armor[ARMOR_TRITANIUM_ARMOR].tech_app_id = TECH_APP_TRITANIUM_ARMOR;
    players[0].tech_applications[TECH_APP_TRITANIUM_ARMOR] = TECH_RESEARCH_STATUS_RESEARCHED;
    DOCTEST_CHECK(aitech::Player_Best_Armor(armor, &players[0]) == ARMOR_TRITANIUM_ARMOR);

    s_tech_weapon_mod_data modifications[WEAPON_MOD_COUNT] = {};
    modifications[WEAPON_MOD_HEAVY_MOUNT].space = 20;
    DOCTEST_CHECK(AIPOWER::Modified_Weapon_Strength(modifications, 100, WEAPON_MOD_FLAG_HEAVY_MOUNT) == 120);
}

DOCTEST_TEST_CASE("star assessments name treaty partners and hostile players by actual treaty state") {
    test::s_game_state game_state;
    auto& players = game::current_state.players;
    s_colony colonies[3] = {};
    test_planets::Resize_Dense(3);
    s_star_data stars[1] = {};
    game::current_state.player_count = 3;
    int16_t colony_count = 3;
    int16_t star_count = 1;
    stars[0].has_colony = game::player_bitmap::From_Byte(7);
    stars[0].wormhole_star_id = game::NO_STAR;
    test_stars::Set(0, stars[0]);
    for (uint32_t i = 0; i < 3; ++i) {
        players[i].ship_range = 6;
        colonies[i].owner = legacy_world::Decode_Player_Slot((int8_t)i);
        colonies[i].planet_id = SERIAL::Decode_Legacy_Planet_Reference((int16_t)i);
        game::current_state.colonies.insert_or_assign(game::id_colony{static_cast<uint32_t>(i)}, colonies[i]);
    }
    players[0].treaty[1] = 1;
    players[0].treaty[2] = 4;
    AIDATA::s_state ai;
    auto& world = game::current_state;
    AIDATA::Compute_Star_Player_Range_Info(&ai, &world);
    const auto& assessment = ai.stars.at(Star_Id(0)).neighborhood.per_player[0];
    DOCTEST_CHECK(assessment.treaty_partners_in_range == 1);
    DOCTEST_CHECK(assessment.hostile_players_in_range == 1);
    DOCTEST_CHECK(assessment.neutral_players_in_range == 0);
    DOCTEST_CHECK(assessment.covering_colony_count == 1);
}

DOCTEST_TEST_CASE("AI strength lookup does not narrow a ship identity to a legacy index") {
    AIDATA::s_state ai;
    constexpr id_ship ship_id = Ship_Id(0x12345678u);
    ai.ships[ship_id].strengths[1] = 123;
    ai.ships[ship_id].strengths_in_nebula[1] = 277;
    ai.ships[Ship_Id(0)].strengths[1] = 7;
    s_star_data stars[1] = {};
    game::current_state.player_count = 2;
    test_stars::Set(0, stars[0]);
    auto& world = game::current_state;
    const auto faction = game::owner_bitmap{}.set(1);
    DOCTEST_CHECK(AIMOVE::Ship_Min_Strength_Versus_Players_At_Star(&ai, &world, 0, ship_id, faction, Star_Id(0)) == 123);
    test_stars::Record(0).in_nebula = 1;
    DOCTEST_CHECK(AIMOVE::Ship_Min_Strength_Versus_Players_At_Star(&ai, &world, 0, ship_id, faction, Star_Id(0)) == 277);
    DOCTEST_CHECK(ai.ships.size() == 2);
}

DOCTEST_TEST_CASE("range assessments use the supplied star keys, parents, and coordinates") {
    test::s_game_state fixture;
    auto state_storage = std::make_unique<game::s_game_state>();
    auto& state = *state_storage;
    state.next_star_id = 90000;
    const auto home = *game::Create_Star(state, {.x = 0, .has_colony = game::player_bitmap::From_Byte(1)});
    const auto far = *game::Create_Star(state, {.x = 300});
    const auto via = *game::Create_Star(state, {.x = 330});
    state.stars.at(home).wormhole_star_id = via;
    const auto planet = *game::Create_Planet(state, {.star_id = home});
    DOCTEST_REQUIRE(game::Create_Colony(state, {.owner = 0, .planet_id = planet}));

    test_stars::Set(far.id, {.x = 0});
    test_stars::Set(home.id, {.x = 0});
    test_stars::Set(via.id, {.x = 0});
    game::current_state.star_order = {far, home, via};
    game::current_state.star_order_sorted = false;

    auto& player = state.players[0];
    player.ship_range = 1;
    state.player_count = 1;
    auto& world = state;
    AIDATA::s_state ai;
    AIDATA::Compute_Star_Player_Range_Info(&ai, &world);
    DOCTEST_CHECK(ai.stars.at(home).neighborhood.per_player[0].covering_colony_count == 1);
    DOCTEST_CHECK(ai.stars.at(far).neighborhood.per_player[0].covering_colony_count == 1);

    int8_t normal[MAX_PLAYERS], extended[MAX_PLAYERS];
    AIDATA::Find_Players_In_Range(&world, far, normal, extended);
    DOCTEST_CHECK(normal[0] == 0);
    DOCTEST_CHECK(extended[0] == 0);
    DOCTEST_CHECK_FALSE(AIDATA::Star_In_Range_Of_Given_Star(&world, home, far, 1));
}

DOCTEST_TEST_CASE("AI staging and officer assignment ignore the singleton roster and player array") {
    test::s_game_state fixture;
    test_stars::Reset(2);
    auto state_storage = std::make_unique<game::s_game_state>();
    auto& state = *state_storage;
    state.next_star_id = 90000;
    const auto home = *game::Create_Star(state, {.has_colony = game::player_bitmap::From_Byte(1)});
    const auto other = *game::Create_Star(state, {.x = 30, .has_colony = game::player_bitmap::From_Byte(1)});
    for (auto& [key, star] : state.stars) star.officer_id[0] = game::NO_LEADER;
    const auto planet = *game::Create_Planet(state, {.star_id = home});
    DOCTEST_REQUIRE(game::Create_Colony(state, {.owner = 0, .planet_id = planet, .n_pops = 4}));
    auto& player = state.players[0];
    player.home_planet_id = planet;
    state.player_count = 1;
    auto& world = state;
    AIDATA::s_state ai;
    ai.stars[home].stage_weights[0] = 5;
    ai.stars[other].stage_weights[0] = 10;
    AISTAGE::Compute_Defensive_Stage_Points(&ai, &world, Player_Id(0));
    DOCTEST_CHECK(player.offensive_stagepoint == other);
    std::reverse(state.star_order.begin(), state.star_order.end());
    state.star_order_sorted = false;
    DOCTEST_CHECK(player.offensive_stagepoint == other);
    DOCTEST_CHECK(state.stars.at(other).is_stagepoint.test(game::id_player{0}));
    DOCTEST_CHECK_FALSE(ai.stars.contains(Star_Id(0)));

    auto& leader = state.leaders[0];
    leader.type = LEADER_TYPE_COLONY;
    leader.status = 0;
    leader.special_skills = LEADER_COLONY_SKILL_FINANCIAL_LEADER;
    aileader::Assign_Star_Officer(&world, &ai, Player_Id(0), game::id_leader{0});
    DOCTEST_CHECK(leader.star_location == home);
    DOCTEST_CHECK(leader.eta == 0);
    DOCTEST_CHECK(state.stars.at(home).officer_id[0] == game::id_leader{0});
    DOCTEST_CHECK(state.stars.at(other).officer_id[0] == game::NO_LEADER);
}

DOCTEST_TEST_CASE("colony proximity scores do not depend on star rows") {
    test::s_game_state fixture;
    auto state_storage = std::make_unique<game::s_game_state>();
    auto& state = *state_storage;
    state.next_star_id = 90000;
    const auto home = *game::Create_Star(state, {.x = 0, .has_colony = game::player_bitmap::From_Byte(1)});
    const auto near = *game::Create_Star(state, {.x = 60, .has_colony = game::player_bitmap::From_Byte(2)});
    const auto distant = *game::Create_Star(state, {.x = 240, .has_colony = game::player_bitmap::From_Byte(2)});
    const auto hole = *game::Create_Star(state, {.x = 30, .spectral_class = STAR_CLASS_BLACK_HOLE});
    const auto planet = *game::Create_Planet(state, {.star_id = home});
    s_colony colony{.owner = 0, .planet_id = planet};
    auto& players = state.players;
    players[0].ship_range = 2;
    s_settings settings{};
    settings.game_difficulty = GAME_DIFFICULTY_AVERAGE;
    state.player_count = 2;
    auto& world = state;
    AIDATA::s_state ai;
    ai.stars[home].neighborhood.per_player[0].covering_colony_count = 1;
    ai.stars[near].neighborhood.per_player[0].covering_colony_count = 2;
    ai.stars[distant].neighborhood.per_player[0].covering_colony_count = 1;
    ai.stars[hole].neighborhood.per_player[0].covering_colony_count = 1;
    const auto check_scores = [&] {
        DOCTEST_CHECK(AIDATA::Proximity_Worth_To_Player(&ai, &world, settings.game_difficulty, &colony, &players[0]) == 140);
        DOCTEST_CHECK(AIDATA::Proximity_Worth_To_Player(&ai, &world, settings.game_difficulty, &colony, &players[1]) == 250);
    };
    check_scores();
    state.star_order = {hole, distant, near, home};
    state.star_order_sorted = false;
    check_scores();
}
