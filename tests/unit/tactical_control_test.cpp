#include "rpc/turn_fixture.h"
#include "game/tactical_view.h"
#include "doctest/doctest.h"

namespace {
    struct s_probe {
        tactical_control::s_options options;
        bool ok = false;
        uint64_t subscription;
        s_probe() : subscription(tactical_events::Subscribe(Read, this)) {}
        ~s_probe() {
            tactical_events::Unsubscribe(subscription);
        }
        static void Read(const tactical_events::s_event* event, void* context) noexcept {
            if (event->kind != tactical_events::SERVICE_READS || !event->safe) return;
            auto* probe = (s_probe*)context;
            probe->ok = tactical_control::Read_Options(&probe->options);
        }
    };
} // namespace

DOCTEST_TEST_CASE("tactical choices read production legal grid without state or RNG mutation") {
    TEST_TACTICAL_TURN::Reset();
    tactical_events::s_battle_scope battle;
    battle.Open(0);
    tactical_events::Select_Unit(1);
    tactical_control::Open_Decision(1);
    s_probe probe;
    const auto actor = MOX::combat_data[1];
    const auto seed = game_random::Get_Random_Seed();
    tactical_events::Service_Reads(true);
    DOCTEST_REQUIRE(probe.ok);
    DOCTEST_REQUIRE(probe.options.available);
    DOCTEST_CHECK_FALSE(probe.options.cells.empty());
    DOCTEST_REQUIRE(probe.options.targets.size() == 1);
    DOCTEST_CHECK(probe.options.targets[0].slot == 2);
    DOCTEST_CHECK(probe.options.targets[0].weapons == 1);
    DOCTEST_CHECK(std::memcmp(&actor, &MOX::combat_data[1], sizeof(actor)) == 0);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
    for (auto cell : probe.options.cells) {
        DOCTEST_CHECK(MOX::combat_grid[cell.x][cell.y] == 2000);
        DOCTEST_CHECK(cell.cost == CMBTMOV1::Move_Cost(1, cell.x, cell.y));
    }
    battle.Close(0);
}

DOCTEST_TEST_CASE("shared tactical operation completes real movement facing firing and done") {
    TEST_TACTICAL_TURN::Reset();
    tactical_events::s_battle_scope battle;
    battle.Open(0);
    tactical_events::Select_Unit(1);
    tactical_control::Open_Decision(1);
    tactical_control::s_operation operation{tactical_control::MOVE, 1, -1, 14, 10};
    DOCTEST_REQUIRE(tactical_control::Validate(&operation) == tactical_control::APPLIED);
    const int cost = CMBTMOV1::Move_Cost(1, 14, 10);
    TEST_TACTICAL_TURN::Apply(&operation);
    DOCTEST_CHECK(int(MOX::combat_data[1].x) == 14);
    DOCTEST_CHECK(int(MOX::combat_data[1].movement_left) == 20 - cost);
    operation = {tactical_control::FACE, 1, -1, 14, 11};
    TEST_TACTICAL_TURN::Apply(&operation);
    DOCTEST_CHECK(MOX::combat_data[1].facing_dir == CMBTMOV1::Get_Facing(14, 10, 14, 11));
    const auto before = game_random::Get_Random_Seed();
    operation = {tactical_control::FIRE, 1, 2};
    TEST_TACTICAL_TURN::Apply(&operation);
    DOCTEST_CHECK(MOX::combat_data[1].weapons[0].shots_left == 0);
    DOCTEST_CHECK(game_random::Get_Random_Seed() != before);
    operation = {tactical_control::DONE, 1};
    TEST_TACTICAL_TURN::Apply(&operation);
    DOCTEST_CHECK(CMBTFIR2::Combat_Ship_Done(1) != 0);
    DOCTEST_CHECK(MOX::ship_turn_order[0] == -1);
    DOCTEST_CHECK(tactical_events::Current_Unit() == -1);
    battle.Close(0);
}

DOCTEST_TEST_CASE("shared operations match direct native move face and complete firing with controlled RNG") {
    for (auto kind : {tactical_control::MOVE, tactical_control::FACE, tactical_control::FIRE}) {
        TEST_TACTICAL_TURN::Reset();
        const tactical_control::s_operation operation{kind, 1, 2, 14, 11};
        if (kind == tactical_control::MOVE) {
            const auto cost = CMBTMOV1::Move_Cost(1, 14, 11);
            const auto used = CMBTFIR2::Used_Teleporter(1);
            const auto actual = CMBTMOV1::Move_Ship(1, 14, 11, cost);
            if (used == CMBTFIR2::Used_Teleporter(1)) MOX::combat_data[1].movement_left -= actual;
            COMBAT1::Assign_Combat_Grids(1);
            CMBTDRW1::Refresh_Combat_Screen_Full();
        } else if (kind == tactical_control::FACE) {
            CMBTMOV1::Turn_To_Face(1, 14, 11);
        } else {
            CMBTFIRE::Fire_Ship(1, 260, 200, 2, 320, 200, 0);
            COMBAT1::Add_Tactical_Combat_Fields();
            COMBAT1::Assign_Combat_Grids(1);
            for (auto& weapon : MOX::combat_data[1].weapons)
                if (weapon.active == 0) weapon.active = 1;
        }
        const auto actor = MOX::combat_data[1], target = MOX::combat_data[2];
        const auto seed = game_random::Get_Random_Seed();
        TEST_TACTICAL_TURN::Reset();
        TEST_TACTICAL_TURN::Apply(&operation);
        DOCTEST_CHECK(std::memcmp(&actor, &MOX::combat_data[1], sizeof(actor)) == 0);
        DOCTEST_CHECK(std::memcmp(&target, &MOX::combat_data[2], sizeof(target)) == 0);
        DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
    }
}

DOCTEST_TEST_CASE("native classic reinsertion initiative selection and authority stay distinct") {
    TEST_TACTICAL_TURN::Reset();
    tactical_events::s_battle_scope battle;
    battle.Open(0);
    tactical_events::Select_Unit(1);
    tactical_control::Open_Decision(1);
    s_probe probe;
    tactical_events::Service_Reads(true);
    DOCTEST_REQUIRE(std::find(probe.options.selectable.begin(), probe.options.selectable.end(), 3) != probe.options.selectable.end());
    CMBTFIR2::Set_Combat_Ship_Done(3);
    const tactical_control::s_operation select{tactical_control::SELECT, 1, 3};
    DOCTEST_REQUIRE(tactical_control::Validate(&select) == tactical_control::APPLIED);
    TEST_TACTICAL_TURN::Apply(&select);
    DOCTEST_CHECK(MOX::cur_ship == 3);
    DOCTEST_CHECK(MOX::ship_turn_order[MOX::cur_ptr] == 3);
    tactical_control::Open_Decision(3);
    MOX::combat_init = 1;
    tactical_events::Service_Reads(true);
    DOCTEST_CHECK(probe.options.selectable.empty());
    const tactical_control::s_operation other{tactical_control::SELECT, 3, 1};
    DOCTEST_CHECK(tactical_control::Validate(&other) == tactical_control::ILLEGAL);
    MOX::net_flag = 1;
    MOX::combat_data[3].owner = 1;
    tactical_events::Service_Reads(true);
    DOCTEST_CHECK_FALSE(probe.options.available);
    DOCTEST_CHECK(tactical_control::Validate(&other) == tactical_control::NO_CONTROL);
    battle.Close(0);
}

DOCTEST_TEST_CASE("normal fire retains slotless Energy Absorber discharge and rejects stasis decisions first") {
    TEST_TACTICAL_TURN::Reset();
    tactical_events::s_battle_scope battle;
    battle.Open(0);
    tactical_events::Select_Unit(1);
    tactical_control::Open_Decision(1);
    MOX::combat_data[1].weapons[0] = {};
    MOX::combat_data[1].reflected_damage_pool = 40;
    struct_::Set_Bit_Field(MOX::combat_data[1].special_device_flags, SPECIAL_ENERGY_ABSORBER);
    MOX::combat_data[1].weapon_ready_flags[0] = 1;
    const tactical_control::s_operation fire{tactical_control::FIRE, 1, 2};
    s_probe probe;
    tactical_events::Service_Reads(true);
    DOCTEST_REQUIRE(probe.options.targets.size() == 1);
    DOCTEST_CHECK(probe.options.targets[0].absorber);
    MOX::combat_data[2].stasis_source_idx = 1;
    DOCTEST_CHECK(tactical_control::Validate(&fire) == tactical_control::LOCAL_REQUIRED);
    DOCTEST_CHECK(int(MOX::combat_data[1].reflected_damage_pool) == 40);
    MOX::combat_data[2].stasis_source_idx = COMBAT_SHIP_NONE;
    DOCTEST_REQUIRE(tactical_control::Validate(&fire) == tactical_control::APPLIED);
    TEST_TACTICAL_TURN::Apply(&fire);
    DOCTEST_CHECK(int(MOX::combat_data[1].reflected_damage_pool) == 0);
    battle.Close(0);
}

DOCTEST_TEST_CASE("native partial facing and teleporter movement keep their original costs") {
    TEST_TACTICAL_TURN::Reset();
    MOX::combat_data[1].movement_left = 1;
    const tactical_control::s_operation face{tactical_control::FACE, 1, -1, 12, 10};
    const int desired = CMBTMOV1::Get_Facing(13, 10, 12, 10);
    TEST_TACTICAL_TURN::Apply(&face);
    DOCTEST_CHECK(int(MOX::combat_data[1].movement_left) == 0);
    DOCTEST_CHECK(int(MOX::combat_data[1].facing_dir) != desired);

    TEST_TACTICAL_TURN::Reset();
    MOX::combat_data[1].movement_left = 0;
    struct_::Set_Bit_Field(MOX::combat_data[1].special_device_flags, SPECIAL_SUB_SPACE_TELEPORTER);
    MOX::combat_data[1].weapon_ready_flags[0] = 1;
    COMBAT1::Assign_Combat_Grids(1);
    DOCTEST_REQUIRE(MOX::combat_grid[14][10] == 2000);
    const tactical_control::s_operation move{tactical_control::MOVE, 1, -1, 14, 10};
    TEST_TACTICAL_TURN::Apply(&move);
    DOCTEST_CHECK(CMBTFIR2::Used_Teleporter(1) != 0);
    DOCTEST_CHECK(int(MOX::combat_data[1].x) == 14);
    DOCTEST_CHECK(int(MOX::combat_data[1].movement_left) == 0);
}
