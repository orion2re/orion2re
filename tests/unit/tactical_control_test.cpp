#include "rpc/turn_fixture.h"
#include "game/tactical_view.h"
#include "doctest/doctest.h"
#include <memory>

namespace {
    struct s_probe {
        tactical_control::s_options options;
        tactical_events::s_hit_history history;
        std::vector<tactical_view::s_missile> missiles;
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
            tactical_events::Read_Hit_History(&probe->history);
            tactical_view::Read_Missiles(&probe->missiles);
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

DOCTEST_TEST_CASE("explicit bank states match local and network-applied native cycles without RNG") {
    for (int8_t initial : {-1, 0, 1})
        for (int8_t desired : {-1, 0, 1}) {
            TEST_TACTICAL_TURN::Reset();
            MOX::combat_data[1].weapons[0].active = initial;
            const auto seed = game_random::Get_Random_Seed();
            while (MOX::combat_data[1].weapons[0].active != desired) COMBAT1::Cycle_Weapon_Bank(0);
            const auto expected = MOX::combat_data[1];
            TEST_TACTICAL_TURN::Reset();
            MOX::combat_data[1].weapons[0].active = initial;
            tactical_control::s_operation selection{tactical_control::FIRE_SELECTION, 1};
            selection.bank_mask = 1;
            selection.bank_states[0] = desired;
            TEST_TACTICAL_TURN::Apply(&selection);
            DOCTEST_CHECK(std::memcmp(&expected, &MOX::combat_data[1], sizeof(expected)) == 0);
            DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
            MOX::combat_data[1].weapons[0].active = initial;
            while (MOX::combat_data[1].weapons[0].active != desired) COMBAT1::Cycle_Weapon_Bank(0, true);
            DOCTEST_CHECK(MOX::combat_data[1].weapons[0].active == desired);
        }
}

DOCTEST_TEST_CASE("deliberate salvo preserves temporary reset persistent off and native hit history") {
    TEST_TACTICAL_TURN::Reset();
    tactical_events::s_battle_scope battle;
    battle.Open(0);
    tactical_control::Open_Decision(1);
    MOX::combat_data[1].weapons[1] = MOX::combat_data[1].weapons[0];
    MOX::combat_data[1].weapons[2] = MOX::combat_data[1].weapons[0];
    MOX::combat_data[1].weapons[1].active = 0;
    MOX::combat_data[1].weapons[2].active = -1;
    s_probe probe;
    const tactical_control::s_operation fire{tactical_control::FIRE, 1, 2};
    TEST_TACTICAL_TURN::Apply(&fire);
    DOCTEST_CHECK(MOX::combat_data[1].weapons[0].shots_left == 0);
    DOCTEST_CHECK(MOX::combat_data[1].weapons[1].shots_left == 2);
    DOCTEST_CHECK(MOX::combat_data[1].weapons[1].active == 1);
    DOCTEST_CHECK(MOX::combat_data[1].weapons[2].shots_left == 2);
    DOCTEST_CHECK(MOX::combat_data[1].weapons[2].active == -1);
    tactical_events::Service_Reads(true);
    DOCTEST_REQUIRE_FALSE(probe.history.hits.empty());
    DOCTEST_CHECK(std::string(probe.history.hits[0].target_name) == "Opponent");
    const auto seed = game_random::Get_Random_Seed();
    const auto count = probe.history.hits.size();
    tactical_events::Service_Reads(true);
    DOCTEST_CHECK(probe.history.hits.size() == count);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
    battle.Close(0);
    DOCTEST_CHECK(battle.hits.capacity() == 0);
}

DOCTEST_TEST_CASE("missile reads cover the global native domain without mutating or simulating") {
    TEST_TACTICAL_TURN::Reset();
    tactical_events::s_battle_scope battle;
    battle.Open(0);
    TEST_TACTICAL_TURN::Add_Missile(4999);
    TEST_TACTICAL_TURN::Add_Missile(4998, WEAPON_ANTI_MATTER_TORP);
    MOX::missile[4999].x = 70 * 20;
    tactical_control::Open_Decision(1);
    const auto missile = MOX::missile[4999];
    const auto seed = game_random::Get_Random_Seed();
    s_probe probe;
    tactical_events::Service_Reads(true);
    DOCTEST_REQUIRE(probe.missiles.size() == 2);
    DOCTEST_CHECK(probe.missiles[1].slot == 4999);
    DOCTEST_CHECK(probe.missiles[1].x == 70);
    DOCTEST_CHECK(probe.options.missile_targets.size() == 2);
    DOCTEST_CHECK(probe.options.missile_targets[0].weapons == 0);
    DOCTEST_CHECK(std::memcmp(&missile, &MOX::missile[4999], sizeof(missile)) == 0);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
    battle.Close(0);
}

DOCTEST_TEST_CASE("native anti-missile fire intercepts missiles and refuses fighters torpedoes and heavy beams") {
    TEST_TACTICAL_TURN::Reset();
    tactical_events::s_battle_scope battle;
    battle.Open(0);
    TEST_TACTICAL_TURN::Add_Missile(0);
    tactical_control::Open_Decision(1);
    tactical_control::s_operation fire{tactical_control::FIRE, 1, 0};
    fire.missile_target = true;
    MOX::combat_data[1].weapons[0].specials = WEAPON_MOD_FLAG_HEAVY_MOUNT;
    DOCTEST_CHECK(tactical_control::Validate(&fire) == tactical_control::ILLEGAL);
    MOX::combat_data[1].weapons[0] = {WEAPON_ANTI_MISSILE_ROCKET, 2, WEAPON_FIRING_ARC_ALL_SECTORS, 0, 5, 1, 2, 0};
    TECHDATA::weapons[WEAPON_ANTI_MISSILE_ROCKET].type = WEAPON_TYPE_SPECIAL;
    const auto accuracy = CMBTSPEC::anti_missile_rocket_accuracy;
    CMBTSPEC::anti_missile_rocket_accuracy.base = 101;
    CMBTSPEC::anti_missile_rocket_accuracy.range_multiplier = 0;
    TEST_TACTICAL_TURN::Add_Missile(0, WEAPON_INTERCEPTOR);
    DOCTEST_CHECK(tactical_control::Validate(&fire) == tactical_control::ILLEGAL);
    TEST_TACTICAL_TURN::Add_Missile(0, WEAPON_ANTI_MATTER_TORP);
    DOCTEST_CHECK(tactical_control::Validate(&fire) == tactical_control::ILLEGAL);
    TEST_TACTICAL_TURN::Add_Missile(0);
    DOCTEST_REQUIRE(tactical_control::Validate(&fire) == tactical_control::APPLIED);
    TEST_TACTICAL_TURN::Apply(&fire);
    DOCTEST_CHECK(int(MOX::missile[0].quantity) == 4);
    DOCTEST_CHECK(int(MOX::combat_data[1].weapons[0].ammo) == 3);
    DOCTEST_CHECK(MOX::combat_data[1].weapons[0].shots_left == 0);
    s_probe probe;
    tactical_events::Service_Reads(true);
    DOCTEST_REQUIRE(probe.history.hits.size() == 1);
    DOCTEST_CHECK(probe.history.hits[0].kind == tactical_events::MISSILES_DESTROYED);
    DOCTEST_CHECK(probe.history.hits[0].amount == 2);
    CMBTSPEC::anti_missile_rocket_accuracy = accuracy;
    battle.Close(0);
}

DOCTEST_TEST_CASE("native hit history is bounded filtered and releases storage between combats") {
    TEST_TACTICAL_TURN::Reset();
    for (int combat = 0; combat < 20; ++combat) {
        tactical_events::s_battle_scope battle;
        battle.Open(0);
        for (size_t i = 0; i < tactical_events::MAX_HIT_RECORDS + 10; ++i) tactical_view::Record_Unit_Hit(2, 1);
        DOCTEST_CHECK(battle.hits.size() == tactical_events::MAX_HIT_RECORDS);
        MOX::combat_data[2].special_status_flag = 4;
        const auto sequence = battle.next_hit_sequence;
        tactical_view::Record_Unit_Hit(2, 999);
        DOCTEST_CHECK(battle.next_hit_sequence == sequence);
        MOX::combat_data[2].special_status_flag = 0;
        s_probe probe;
        tactical_events::Service_Reads(true);
        DOCTEST_CHECK(probe.history.first_sequence == 11);
        if (combat % 2)
            tactical_events::Revoke();
        else
            tactical_events::Retire_Battle();
        DOCTEST_CHECK(battle.hits.capacity() == 0);
        battle.Close(0);
        DOCTEST_CHECK(battle.hits.capacity() == 0);
    }
}

DOCTEST_TEST_CASE("beam missile bounds guards preserve in-range displacement behavior and RNG") {
    TEST_TACTICAL_TURN::Reset();
    TECHDATA::weapons[WEAPON_LASER_CANNON].min_damage = 100;
    TECHDATA::weapons[WEAPON_LASER_CANNON].max_damage = 100;
    TEST_TACTICAL_TURN::Add_Missile(2);
    int16_t primary = 0, secondary = 0, reflected = 0;
    CMBTFIRE::Fire_Beam_Weapon(1, 2, WEAPON_LASER_CANNON, 0, 1, &primary, &secondary, &reflected);
    const int expected = primary;
    const auto seed = game_random::Get_Random_Seed();
    DOCTEST_CHECK(expected > 0);

    TEST_TACTICAL_TURN::Reset();
    TECHDATA::weapons[WEAPON_LASER_CANNON].min_damage = 100;
    TECHDATA::weapons[WEAPON_LASER_CANNON].max_damage = 100;
    TEST_TACTICAL_TURN::Add_Missile(2);
    struct_::Set_Bit_Field(MOX::combat_data[2].special_device_flags, SPECIAL_DISPLACEMENT_DEVICE);
    const auto chance = CMBTFIR2::combat_bonus.displacement_device_beam_miss_percent;
    CMBTFIR2::combat_bonus.displacement_device_beam_miss_percent = 100;
    CMBTFIRE::Fire_Beam_Weapon(1, 2, WEAPON_LASER_CANNON, 0, 1, &primary, &secondary, &reflected);
    DOCTEST_CHECK(primary == 0);
    DOCTEST_CHECK(game_random::Get_Random_Seed() != seed);
    CMBTFIR2::combat_bonus.displacement_device_beam_miss_percent = chance;

    MOX::combat_data[2].facing_dir = 5;
    MOX::combat_data[2].size_class = 3;
    const auto legacy_shape = BEAMS::Beam_Target_Shape(2);
    DOCTEST_CHECK(legacy_shape.facing == 5);
    DOCTEST_CHECK(legacy_shape.size == 3);

    for (int16_t slot : {int16_t(MAX_COMBAT_SHIPS), int16_t(MAX_COMBAT_FLYERS - 1)}) {
        TEST_TACTICAL_TURN::Reset();
        TECHDATA::weapons[WEAPON_LASER_CANNON].min_damage = 100;
        TECHDATA::weapons[WEAPON_LASER_CANNON].max_damage = 100;
        TEST_TACTICAL_TURN::Add_Missile(slot);
        auto units = std::make_unique<s_combat_data[]>(MAX_COMBAT_SHIPS);
        std::copy_n(MOX::combat_data, MAX_COMBAT_SHIPS, units.get());
        auto* original = MOX::combat_data;
        MOX::combat_data = units.get();
        CMBTFIRE::Fire_Beam_Weapon(1, slot, WEAPON_LASER_CANNON, 0, 1, &primary, &secondary, &reflected);
        DOCTEST_CHECK(int(primary) == expected);
        DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
        const auto shape = BEAMS::Beam_Target_Shape(slot);
        DOCTEST_CHECK(shape.facing == 0);
        DOCTEST_CHECK(shape.size == 0);
        game_random::Set_Random_Seed(12345);
        tactical_control::s_operation fire{tactical_control::FIRE, 1, slot};
        fire.missile_target = true;
        TEST_TACTICAL_TURN::Apply(&fire);
        const auto actor = MOX::combat_data[1];
        const auto missile = MOX::missile[slot];
        const auto applied_seed = game_random::Get_Random_Seed();
        MOX::combat_data[1] = original[1];
        TEST_TACTICAL_TURN::Add_Missile(slot);
        game_random::Set_Random_Seed(12345);
        CMBTFIRE::Fire_Ship(1, 260, 200, slot, 280, 200, 1);
        COMBAT1::Add_Tactical_Combat_Fields();
        COMBAT1::Assign_Combat_Grids(1);
        for (auto& weapon : MOX::combat_data[1].weapons)
            if (weapon.active == 0) weapon.active = 1;
        DOCTEST_CHECK(std::memcmp(&actor, &MOX::combat_data[1], sizeof(actor)) == 0);
        DOCTEST_CHECK(std::memcmp(&missile, &MOX::missile[slot], sizeof(missile)) == 0);
        DOCTEST_CHECK(game_random::Get_Random_Seed() == applied_seed);
        MOX::combat_data = original;
    }
}
