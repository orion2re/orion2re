#include "pch.h"
#include "../fixtures/star_roster.h"
#include "../fixtures/colony_roster.h"
#include "../fixtures/planet_roster.h"

#include "doctest/doctest.h"
#include "game/colbldg.h"
#include "game/colrefit.h"
#include "lua/lua_game_ship_identity.h"
#include "../fixtures/game_state.h"

DOCTEST_TEST_CASE("queue deletion addresses a wide colony ID in the supplied state") {
    game::s_game_state state;
    state.next_colony_id = 70000;
    const auto id = game::Create_Colony(state, {});
    DOCTEST_REQUIRE(id == game::id_colony{70000});
    auto& colony = state.colonies.at(*id);
    std::ranges::fill(colony.producing, COLONY_PRODUCTION_NONE);
    colony.producing[0] = BUILDING_AUTOMATED_FACTORY;
    colony.producing[1] = BUILDING_TERRAFORMING;
    colony.producing[2] = BUILDING_BIOSPHERES;

    COLBLDG::Delete_Prod_From_Colony_Queue(state, *id, BUILDING_TERRAFORMING);

    DOCTEST_CHECK(colony.producing[0] == BUILDING_AUTOMATED_FACTORY);
    DOCTEST_CHECK(colony.producing[1] == BUILDING_BIOSPHERES);
    DOCTEST_CHECK(colony.producing[2] == COLONY_PRODUCTION_NONE);
}

DOCTEST_TEST_CASE_FIXTURE(test::s_game_state, "queued ship deletion follows the supplied roster across row changes") {
    auto& live = game::current_state;
    live.ship_count = 2;
    live.ship_records.resize(2);
    live.ship_records.Replace_Row(1, s_ship_data{.status = 4, .id = game::id_ship{90000}});
    const auto live_ship = live.ship_records[1];

    game::s_game_state state;
    state.next_colony_id = 70000;
    const auto id = game::Create_Colony(state, {});
    DOCTEST_REQUIRE(id);
    state.ship_records.resize(2);
    state.ship_count = 2;
    const game::id_ship original{90000}, other{90001}, replacement{90002};
    state.ship_records.Replace_Row(0, s_ship_data{.status = 4, .id = original});
    state.ship_records.Replace_Row(1, s_ship_data{.status = 4, .id = other});
    auto& colony = state.colonies.at(*id);
    std::ranges::fill(colony.producing, COLONY_PRODUCTION_NONE);
    colony.producing[1] = game::Queued_Ship_Product(original);

    DOCTEST_SUBCASE("reordered original") {
        state.ship_records.Swap_Rows(0, 1);
        COLBLDG::Delete_From_Colony_Queue(state, *id, 1);
        DOCTEST_CHECK((state.ship_records[1].status == 5));
        DOCTEST_CHECK(state.ship_records[1].position.Get() == game::NO_SHIP_LOCATION);
        DOCTEST_CHECK((state.ship_records[0].status == 4));
    }
    DOCTEST_SUBCASE("stale product") {
        state.ship_records.Set_Key_At_Row(0, replacement);
        COLBLDG::Delete_From_Colony_Queue(state, *id, 1);
        DOCTEST_CHECK((state.ship_records[0].status == 4));
        DOCTEST_CHECK((state.ship_records[1].status == 4));
    }
    DOCTEST_CHECK(colony.producing[1] == COLONY_PRODUCTION_NONE);
    DOCTEST_CHECK(live.ship_records[1] == live_ship);
}

DOCTEST_TEST_CASE("colony navigation wraps over sparse keys and skips outposts") {
    test::s_game_state fixture;
    const game::id_colony home{0};
    const game::id_colony outpost{70000};
    const game::id_colony other{80000};
    test_colonies::Record(home.id).owner = legacy_world::Decode_Player_Slot(0);
    test_colonies::Set(outpost.id, {.owner = 0, .outpost_flag = 1});
    test_colonies::Record(other.id).owner = legacy_world::Decode_Player_Slot(0);
    DOCTEST_CHECK(COLONY::Get_Next_Colony(home, 0) == other);
    DOCTEST_CHECK(COLONY::Get_Next_Colony(home, 1) == other);
    DOCTEST_CHECK(COLONY::Get_Next_Colony(other, 0) == home);
}

namespace {
    struct s_building_state : test::s_game_state {
        test::s_saved_array<s_tech_building_data, BUILDING_COUNT> buildings{TECHDATA::buildings};
        test::s_saved_array<COLBLDG::s_build_list_format, COLBLDG::BUILD_LIST_PRODUCT_COUNT> formats{COLBLDG::build_list_format};
        test::s_saved_array<char*, ESTRINGS_COUNT> strings{ESTRINGS::strings};
        test::s_saved_array<int16_t, 54> building_indexes{COLBLDG::building_indexes};
        test::s_saved_array<int16_t, 54> military_indexes{COLBLDG::military_indexes};
        test::s_saved_array<game::colony_product, 7> items{COLBLDG::current_item};
        test::s_saved_array<int16_t, 4> active{COLBLDG::active_prod};
        int16_t building_count = COLBLDG::n_building_indexes;
        int16_t military_count = COLBLDG::n_military_indexes;
        game::id_colony handle = COLONY::colony_handle;
        uint8_t cheats = COLONY::cheats;
        uint8_t housing = COLCALC::disable_housing;
        uint8_t mode = COLBLDG::field_mode;
        game::id_ship refitting = COLBLDG::refitting_ship;
        bool had_pending = COLBLDG::Pending_Refit_Design() != nullptr;
        s_ship_design pending = {};

        s_building_state() {
            if (had_pending) pending = *COLBLDG::Pending_Refit_Design();
        }

        ~s_building_state() {
            if (had_pending) COLBLDG::Set_Pending_Refit_Design(refitting, &pending);
            else COLBLDG::Clear_Pending_Refit_Design();
            COLBLDG::n_building_indexes = building_count;
            COLBLDG::n_military_indexes = military_count;
            COLONY::colony_handle = handle;
            COLONY::cheats = cheats;
            COLCALC::disable_housing = housing;
            COLBLDG::field_mode = mode;
        }
    };
}

namespace TEST_COLBLDG {
    void Reset_();
    void Set_Buildable_(int16_t building_id, bool value);
}

DOCTEST_TEST_CASE_FIXTURE(s_building_state, "build-list format defaults preserve classic ordering and colors") {
    TEST_COLBLDG::Reset_();

    DOCTEST_CHECK(COLBLDG::build_list_format[COLBLDG::BUILD_LIST_PRODUCT_TRADE_GOODS].order == -2);
    DOCTEST_CHECK(COLBLDG::build_list_format[COLBLDG::BUILD_LIST_PRODUCT_HOUSING].order == -1);

    const COLBLDG::s_build_list_color& normal =
        COLBLDG::build_list_colors[COLBLDG::BUILD_LIST_COLOR_DEFAULT][COLBLDG::BUILD_LIST_HIGHLIGHT_NORMAL];
    const COLBLDG::s_build_list_color& queued =
        COLBLDG::build_list_colors[COLBLDG::BUILD_LIST_COLOR_DEFAULT][COLBLDG::BUILD_LIST_HIGHLIGHT_QUEUED];
    DOCTEST_CHECK(normal.base == 0xCB);
    DOCTEST_CHECK(normal.dim == 0xCA);
    DOCTEST_CHECK(normal.dark == 0xB7);
    DOCTEST_CHECK(queued.base == 0xBB);
    DOCTEST_CHECK(queued.dim == 0xBA);
    DOCTEST_CHECK(queued.dark == 0xB8);
}

DOCTEST_TEST_CASE_FIXTURE(s_building_state, "build-list ordering includes Housing and Trade Goods") {
    TEST_COLBLDG::Reset_();

    static char alpha[] = "Alpha";
    static char zeta[] = "Zeta";
    TECHDATA::buildings[BUILDING_ARMOR_BARRACKS].name = alpha;
    TECHDATA::buildings[BUILDING_AUTOMATED_FACTORY].name = zeta;
    TEST_COLBLDG::Set_Buildable_(BUILDING_ARMOR_BARRACKS, true);
    TEST_COLBLDG::Set_Buildable_(BUILDING_AUTOMATED_FACTORY, true);

    COLBLDG::build_list_format[COLBLDG::BUILD_LIST_PRODUCT_HOUSING].order = -4;
    COLBLDG::build_list_format[COLBLDG::BUILD_LIST_PRODUCT_TRADE_GOODS].order = 4;

    COLBLDG::Calculate_Building_Array();

    DOCTEST_REQUIRE(COLBLDG::n_building_indexes == 4);
    DOCTEST_CHECK(COLBLDG::building_indexes[0] == COLONY_PRODUCTION_HOUSING);
    DOCTEST_CHECK(COLBLDG::building_indexes[1] == BUILDING_ARMOR_BARRACKS);
    DOCTEST_CHECK(COLBLDG::building_indexes[2] == BUILDING_AUTOMATED_FACTORY);
    DOCTEST_CHECK(COLBLDG::building_indexes[3] == COLONY_PRODUCTION_TRADE_GOODS);
}

DOCTEST_TEST_CASE_FIXTURE(s_building_state, "disabled Housing is omitted from the build list") {
    TEST_COLBLDG::Reset_();
    COLCALC::disable_housing = 1;

    COLBLDG::Calculate_Building_Array();

    DOCTEST_REQUIRE(COLBLDG::n_building_indexes == 1);
    DOCTEST_CHECK(COLBLDG::building_indexes[0] ==
                  COLONY_PRODUCTION_TRADE_GOODS);
}

DOCTEST_TEST_CASE_FIXTURE(s_building_state, "enemy outpost is not a colony base target") {
    TEST_COLBLDG::Reset_();

    for (int16_t i = 0; i < 5; ++i) {
        test_stars::Record(0).orbit_planet_ids[i] = game::id_planet{static_cast<uint32_t>(i)};
        test_planets::Record(i).planet_type = PLANET_TYPE_PLANET;
    }

    test_planets::Record(0).colony_id = game::id_colony{0};
    test_colonies::Record(0).owner = legacy_world::Decode_Player_Slot(0);

    test_planets::Record(1).colony_id = game::NO_COLONY;

    test_planets::Record(2).colony_id = game::id_colony{1};
    test_colonies::Record(1).owner = legacy_world::Decode_Player_Slot(0);
    test_colonies::Record(1).outpost_flag = 1;

    test_planets::Record(3).colony_id = game::id_colony{2};
    test_colonies::Record(2).owner = legacy_world::Decode_Player_Slot(1);
    test_colonies::Record(2).outpost_flag = 1;

    test_planets::Record(4).colony_id = game::NO_COLONY;
    test_planets::Record(4).planet_type = PLANET_TYPE_ASTEROID;

    DOCTEST_CHECK(COLBLDG::Star_N_Uncolonized_Worlds(0) == 2);
}

DOCTEST_TEST_CASE_FIXTURE(s_building_state, "queuing the last legal Colony Base removes it from the military list") {
    TEST_COLBLDG::Reset_();

    test_colonies::Record(0).owner = legacy_world::Decode_Player_Slot(0);
    test_colonies::Record(0).planet_id = game::id_planet{0};
    test_planets::Record(0).colony_id = game::id_colony{0};
    test_planets::Record(0).planet_type = PLANET_TYPE_PLANET;
    test_planets::Record(0).star_id = game::id_star{static_cast<uint32_t>(0)};
    test_planets::Record(1).colony_id = game::NO_COLONY;
    test_planets::Record(1).planet_type = PLANET_TYPE_PLANET;
    test_planets::Record(1).star_id = game::id_star{static_cast<uint32_t>(0)};
    test_stars::Record(0).orbit_planet_ids[0] = game::id_planet{0};
    test_stars::Record(0).orbit_planet_ids[1] = game::id_planet{1};
    TEST_COLBLDG::Set_Buildable_(BUILDING_COLONY_BASE, true);

    COLBLDG::Calculate_Military_Array();
    DOCTEST_REQUIRE(COLBLDG::n_military_indexes == 2);
    DOCTEST_CHECK(COLBLDG::military_indexes[0] == BUILDING_COLONY_BASE);

    COLBLDG::current_item[0] = BUILDING_COLONY_BASE;
    COLBLDG::Calculate_Military_Array();

    DOCTEST_CHECK(COLBLDG::n_military_indexes == 0);
}

DOCTEST_TEST_CASE_FIXTURE(s_building_state, "pending refit design owns an independent transient copy") {
    s_ship_design source = {};
    strcpy(source.name, "Refit");
    source.size = SHIP_SIZE_LARGE;
    source.cost = 321;

    COLBLDG::Set_Pending_Refit_Design(game::id_ship{90000}, &source);
    source.cost = 999;

    s_ship_design* pending = COLBLDG::Pending_Refit_Design();
    DOCTEST_REQUIRE(pending != nullptr);
    DOCTEST_CHECK(strcmp(pending->name, "Refit") == 0);
    DOCTEST_CHECK(pending->size == SHIP_SIZE_LARGE);
    DOCTEST_CHECK((pending->cost == 321));
    DOCTEST_CHECK(COLBLDG::refitting_ship == game::id_ship{90000});

    COLBLDG::Clear_Pending_Refit_Design();
    DOCTEST_CHECK(COLBLDG::Pending_Refit_Design() == nullptr);
}

DOCTEST_TEST_CASE_FIXTURE(s_building_state, "pending refit cannot change a replacement ship") {
    TEST_COLBLDG::Reset_();
    auto& state = game::current_state;
    state.ship_count = 1;
    state.ship_records.Set_Key_At_Row(0, game::id_ship{90000});
    s_ship_design new_design{};
    new_design.cost = 100;
    COLBLDG::Set_Pending_Refit_Design(game::id_ship{90000}, &new_design);
    state.ship_records.Set_Key_At_Row(0, game::id_ship{90001});
    const auto& ship = state.ship_records[0];
    const auto replacement = ship;
    COLBLDG::Add_Refited_Ship_To_Queue();
    DOCTEST_CHECK(ship == replacement);
    DOCTEST_CHECK(COLBLDG::Pending_Refit_Design() == nullptr);
}

DOCTEST_TEST_CASE_FIXTURE(s_building_state, "refit design selection follows the original ship key") {
    TEST_COLBLDG::Reset_();
    test::s_saved_array<int16_t, 12> fields{COLREFIT::fields};
    test::s_saved_value<decltype(COLREFIT::esc)> esc{COLREFIT::esc};
    test::s_saved_value<decltype(COLREFIT::scanned_ship_d_index)> scanned{COLREFIT::scanned_ship_d_index};
    test::s_saved_value<decltype(COLREFIT::lscanned_field)> scanned_field{COLREFIT::lscanned_field};
    test::s_saved_value<decltype(MOX::screen_star_id)> star{MOX::screen_star_id};
    test::s_saved_value<decltype(MOX::temp_star_handle)> temp_star{MOX::temp_star_handle};
    auto& state = game::current_state;
    state.ship_count = 2;
    MOX::screen_star_id = game::Star_Key_At_Row(state, 0);
    const game::id_ship key{90000}, other{90001}, replacement{90002};
    s_ship_data ship{};
    ship.id = key;
    ship.d.ship_type = SHIP_TYPE_COMBAT;
    ship.d.size = SHIP_SIZE_SMALL;
    ship.position.Set({game::e_ship_location::at_star, MOX::screen_star_id});
    state.ship_records.Replace_Row(0, ship);
    ship.id = other;
    state.ship_records.Replace_Row(1, ship);
    game::current_state.players[0].ship_designs[0].size = SHIP_SIZE_SMALL;
    COLREFIT::fields[5] = 100;
    int16_t selected = 100, hovered = 100;

    state.ship_records.Swap_Rows(0, 1);
    DOCTEST_CHECK(COLREFIT::Evaluate2_Input(key, &selected, &hovered) == -3);
    DOCTEST_CHECK(COLBLDG::refitting_ship == key);
    COLBLDG::Clear_Pending_Refit_Design();

    state.ship_records.Set_Key_At_Row(1, replacement);
    DOCTEST_CHECK(COLREFIT::Evaluate2_Input(key, &selected, &hovered) == -1);
    DOCTEST_CHECK(COLBLDG::Pending_Refit_Design() == nullptr);
}

DOCTEST_TEST_CASE_FIXTURE(s_building_state, "ship allocation appends without a pre-seeded owner") {
    TEST_COLBLDG::Reset_();
    test_colonies::Record(0).planet_id = game::id_planet{0};
    test_planets::Record(0).star_id = game::id_star{static_cast<uint32_t>(0)};
    game::current_state.ship_count = 1;
    game::current_state.ship_records[0].owner = factions::From_Legacy(0);
    game::current_state.ship_records[0].status = 0;
    s_ship_design design = {};
    design.ship_type = SHIP_TYPE_OUTPOST;

    const auto key =
        COLBLDG::Create_Ship_From_Design(game::id_player{1}, &design, game::id_colony{0});

    DOCTEST_REQUIRE(key != game::NO_SHIP);
    const auto ship_idx = game::Ship_Row(game::current_state, key);
    DOCTEST_REQUIRE(ship_idx == 1);
    DOCTEST_CHECK(game::current_state.ship_count == 2);
    DOCTEST_CHECK(factions::Legacy_Code(game::current_state.ship_records[ship_idx].owner) == 1);
    DOCTEST_CHECK(game::current_state.ship_records[ship_idx].d.ship_type == SHIP_TYPE_OUTPOST);
    DOCTEST_CHECK(orion2re::lua::game::Ship_Generation(ship_idx) == 1);
}

DOCTEST_TEST_CASE_FIXTURE(s_building_state, "ship allocation reuses only an owner-matched destroyed slot") {
    TEST_COLBLDG::Reset_();
    test_colonies::Record(0).planet_id = game::id_planet{0};
    test_planets::Record(0).star_id = game::id_star{static_cast<uint32_t>(0)};
    game::current_state.ship_count = 2;
    game::current_state.ship_records[0].owner = factions::From_Legacy(0);
    game::current_state.ship_records[0].status = 5;
    game::current_state.ship_records[1].owner = factions::From_Legacy(1);
    game::current_state.ship_records[1].status = 5;
    s_ship_design design = {};

    const auto key =
        COLBLDG::Create_Ship_From_Design(game::id_player{1}, &design, game::id_colony{0});

    DOCTEST_REQUIRE(key != game::NO_SHIP);
    const auto ship_idx = game::Ship_Row(game::current_state, key);
    DOCTEST_REQUIRE(ship_idx == 1);
    DOCTEST_CHECK(game::current_state.ship_count == 2);
    DOCTEST_CHECK(game::current_state.ship_records[0].status == 5);
    DOCTEST_CHECK(orion2re::lua::game::Ship_Generation(ship_idx) == 1);
}

DOCTEST_TEST_CASE_FIXTURE(s_building_state, "ship allocation fails when capacity has no reusable slot") {
    TEST_COLBLDG::Reset_();
    test_colonies::Record(0).planet_id = game::id_planet{0};
    test_planets::Record(0).star_id = game::id_star{static_cast<uint32_t>(0)};
    game::current_state.ship_count = MAX_SHIPS;
    for (int16_t ship_idx = 0; ship_idx < MAX_SHIPS; ++ship_idx) {
        game::current_state.ship_records[ship_idx].owner = factions::From_Legacy(0);
        game::current_state.ship_records[ship_idx].status = 0;
    }
    s_ship_design design = {};

    DOCTEST_CHECK(COLBLDG::Create_Ship_From_Design(game::id_player{1}, &design, game::id_colony{0}) == game::NO_SHIP);
    DOCTEST_CHECK(game::current_state.ship_count == MAX_SHIPS);
}

DOCTEST_TEST_CASE_FIXTURE(s_building_state, "moving a Repeat pair forward preserves unrelated queue entries") {
    TEST_COLBLDG::Reset_();
    const game::colony_product initial[7] = {
        BUILDING_AUTOMATED_FACTORY,
        BUILDING_RESEARCH_LAB,
        COLONY_PRODUCTION_REPEAT,
        BUILDING_SPACEPORT,
        COLONY_PRODUCTION_NONE,
        COLONY_PRODUCTION_NONE,
        COLONY_PRODUCTION_NONE,
    };
    memcpy(COLBLDG::current_item, initial, sizeof(initial));

    COLBLDG::Insert_Before_Queue_Slot(1, 0);

    const game::colony_product expected[7] = {
        BUILDING_RESEARCH_LAB,
        COLONY_PRODUCTION_REPEAT,
        BUILDING_AUTOMATED_FACTORY,
        BUILDING_SPACEPORT,
        COLONY_PRODUCTION_NONE,
        COLONY_PRODUCTION_NONE,
        COLONY_PRODUCTION_NONE,
    };
    DOCTEST_CHECK(std::ranges::equal(COLBLDG::current_item, expected));
    DOCTEST_CHECK(COLBLDG::active_prod[0] == -1);
}

DOCTEST_TEST_CASE_FIXTURE(s_building_state, "moving before a Repeat marker preserves its product pair") {
    TEST_COLBLDG::Reset_();
    const game::colony_product initial[7] = {
        BUILDING_AUTOMATED_FACTORY,
        BUILDING_RESEARCH_LAB,
        COLONY_PRODUCTION_REPEAT,
        BUILDING_SPACEPORT,
        BUILDING_MARINE_BARRACKS,
        COLONY_PRODUCTION_NONE,
        COLONY_PRODUCTION_NONE,
    };
    memcpy(COLBLDG::current_item, initial, sizeof(initial));

    COLBLDG::Insert_Before_Queue_Slot(4, 2);

    const game::colony_product expected[7] = {
        BUILDING_AUTOMATED_FACTORY,
        BUILDING_MARINE_BARRACKS,
        BUILDING_RESEARCH_LAB,
        COLONY_PRODUCTION_REPEAT,
        BUILDING_SPACEPORT,
        COLONY_PRODUCTION_NONE,
        COLONY_PRODUCTION_NONE,
    };
    DOCTEST_CHECK(std::ranges::equal(COLBLDG::current_item, expected));
    DOCTEST_CHECK(COLBLDG::active_prod[0] == -1);
}

DOCTEST_TEST_CASE_FIXTURE(s_building_state, "moving a Repeat pair backward preserves unrelated queue entries") {
    TEST_COLBLDG::Reset_();
    const game::colony_product initial[7] = {
        BUILDING_AUTOMATED_FACTORY,
        BUILDING_RESEARCH_LAB,
        BUILDING_SPACEPORT,
        COLONY_PRODUCTION_REPEAT,
        BUILDING_MARINE_BARRACKS,
        COLONY_PRODUCTION_NONE,
        COLONY_PRODUCTION_NONE,
    };
    memcpy(COLBLDG::current_item, initial, sizeof(initial));

    COLBLDG::Insert_Before_Queue_Slot(3, 0);

    const game::colony_product expected[7] = {
        BUILDING_SPACEPORT,
        COLONY_PRODUCTION_REPEAT,
        BUILDING_AUTOMATED_FACTORY,
        BUILDING_RESEARCH_LAB,
        BUILDING_MARINE_BARRACKS,
        COLONY_PRODUCTION_NONE,
        COLONY_PRODUCTION_NONE,
    };
    DOCTEST_CHECK(std::ranges::equal(COLBLDG::current_item, expected));
    DOCTEST_CHECK(COLBLDG::active_prod[0] == -1);
}
