#include "rpc/turn_fixture.h"
#include "rpc/rpc_combat_control.h"
#include "rpc/rpc_tactical_view.h"
#include "game/combat.h"
#include "doctest/doctest.h"
#include <memory>
#include "game/boarding.h"

namespace {
    struct s_probe {
        rpc::s_options options;
        rpc::s_hit_history history;
        std::vector<rpc::s_missile> missiles;
        bool ok = false;
        uint64_t subscription;
    };
    void Read(const COMBAT::s_event* event, void* context) noexcept {
        if (event->kind != COMBAT::SERVICE_READS || !event->safe) return;
        auto* probe = (s_probe*)context;
        probe->ok = rpc::Read_Options(&probe->options);
        rpc::Read_Hit_History(&probe->history);
        rpc::Read_Tactical_Missiles(&probe->missiles);
    }
} // namespace

namespace {
    struct s_boarding_fixture {
        COMBAT::s_battle_state battle;
        int16_t previous_ground = MOX::crew_data[1].ground;
    };
    void Initialize_Boarding(s_boarding_fixture* fixture, bool reports = false) {
        TEST_TACTICAL_TURN::Reset();
        TEST_TACTICAL_TURN::Boarding(reports);
        COMBAT::Open_Battle(&fixture->battle, 0);
        COMBAT::Select_Unit(1);
        COMBAT::Open_Control_Decision(1);
    }
    void Shutdown_Boarding(s_boarding_fixture* fixture) {
        COMBAT::Close_Battle(&fixture->battle, 0);
        COMBAT::Release_Battle(&fixture->battle);
        TEST_TACTICAL_TURN::Release_Boarding();
        MOX::crew_data[1].ground = fixture->previous_ground;
    }
    void Board(int16_t mode, int16_t marines) {
        COMBAT1::s_operation operation{COMBAT1::BOARD, 1, 2};
        operation.boarding_mode = mode;
        operation.marines = marines;
        DOCTEST_REQUIRE(rpc::Validate(&operation) == COMBAT1::APPLIED);
        TEST_TACTICAL_TURN::Apply(&operation);
    }
    bool Consume_Report(uint64_t, void* context) {
        *(bool*)context = true;
        return true;
    }
    bool Consume_Shuttle_Report(uint64_t generation, void* context) {
        DOCTEST_CHECK(TEXTBOX::field_save_ix == 1);
        return Consume_Report(generation, context);
    }
    void Complete_Report(void*) {}
    struct s_ack {
        bool taken = false, completed = false;
    };
    bool Take_Acknowledgement(uint64_t generation, void* context) {
        auto* ack = (s_ack*)context;
        DOCTEST_CHECK(BOARDING::Boarding_Report_Active());
        DOCTEST_CHECK(generation == BOARDING::Boarding_Generation());
        DOCTEST_CHECK(TEXTBOX::field_save_ix == 1);
        ack->taken = true;
        return true;
    }
    void Complete_Acknowledgement(void* context) {
        ((s_ack*)context)->completed = true;
    }
} // namespace

DOCTEST_TEST_CASE("generic textboxes never acquire or consume boarding report authority") {
    s_boarding_fixture fixture;
    Initialize_Boarding(&fixture, true);
    bool consumed = false;
    DOCTEST_REQUIRE(BOARDING::Attach_Report_Input(Consume_Report, Complete_Report, &consumed));
    const auto remaining = allocate::Get_Remaining_Block_Space(MOX::screen_seg);
    key::Push_Key_Code(27, 1);
    TEXTBOX::Text_Box("Generic popup", "Synthetic generic textbox");
    TEXTBOX::Timed_Text_Box("Generic popup", "Synthetic generic textbox", 1);
    DOCTEST_CHECK_FALSE(consumed);
    DOCTEST_CHECK_FALSE(BOARDING::Boarding_Report_Active());
    DOCTEST_CHECK(TEXTBOX::field_save_ix == 0);
    DOCTEST_CHECK(allocate::Get_Remaining_Block_Space(MOX::screen_seg) == remaining);
    COMBAT::Detach_Control(&consumed);
    Shutdown_Boarding(&fixture);
}

DOCTEST_TEST_CASE("boarding commitment is an explicit argument and never a prior result context") {
    s_boarding_fixture fixture;
    Initialize_Boarding(&fixture);
    Board(2, 4);
    DOCTEST_REQUIRE(BOARDING::Boarding_Result() != nullptr);
    DOCTEST_CHECK(BOARDING::Boarding_Result()->committed == 4);
    auto& target = MOX::combat_data[2];
    target.owner = 1;
    target.is_captured = 0;
    target.marine_count = 0;
    CMBTFIR2::Resolve_Capture(1, 2, 1, 320, 200);
    DOCTEST_REQUIRE(BOARDING::Boarding_Result() != nullptr);
    DOCTEST_CHECK(BOARDING::Boarding_Result()->committed == -1);
    DOCTEST_CHECK(BOARDING::Boarding_Result()->arrived == 1);
    COMBAT::Retire_Battle();
    DOCTEST_CHECK(fixture.battle.boarding.raid_text.capacity() == std::string().capacity());
    Shutdown_Boarding(&fixture);
}

DOCTEST_TEST_CASE("boarding bounds and legal reads preserve native bytes and RNG") {
    s_boarding_fixture fixture;
    Initialize_Boarding(&fixture);
    s_probe probe;
    probe.subscription = COMBAT::Subscribe(Read, &probe);
    for (uint8_t marines : {uint8_t(0), uint8_t(1), uint8_t(12), uint8_t(127), uint8_t(128), uint8_t(255)}) {
        MOX::combat_data[1].marine_count = marines;
        const auto seed = game_random::Get_Random_Seed();
        const auto actor = MOX::combat_data[1], target = MOX::combat_data[2];
        COMBAT::Service_Reads(true);
        DOCTEST_CHECK(probe.options.maximum_marines == (int(marines) + 1) / 2);
        DOCTEST_CHECK(probe.options.boarding_targets.empty() == (marines == 0));
        DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
        DOCTEST_CHECK(std::memcmp(&actor, &MOX::combat_data[1], sizeof(actor)) == 0);
        DOCTEST_CHECK(std::memcmp(&target, &MOX::combat_data[2], sizeof(target)) == 0);
    }
    COMBAT::Unsubscribe(probe.subscription);
    Shutdown_Boarding(&fixture);
}

DOCTEST_TEST_CASE("boarding human checks distinguish ordinary transporters and native configuration") {
    s_boarding_fixture fixture;
    Initialize_Boarding(&fixture);
    auto& actor = MOX::combat_data[1];
    auto& target = MOX::combat_data[2];
    DOCTEST_CHECK(BOARDING::Check_Human_Boarding(1, 2).allowed);
    target.drive_current_hits = 100;
    target.combat_speed_base = 20;
    target.current_speed = 20;
    DOCTEST_CHECK_FALSE(BOARDING::Check_Human_Boarding(1, 2).allowed);
    struct_::Set_Bit_Field(actor.special_device_flags, SPECIAL_TRANSPORTERS);
    DOCTEST_CHECK(BOARDING::Check_Human_Boarding(1, 2).allowed);
    struct_::Set_Bit_Field(actor.special_device_damage_flags, SPECIAL_TRANSPORTERS);
    DOCTEST_CHECK_FALSE(BOARDING::Check_Human_Boarding(1, 2).allowed);
    struct_::Clear_Bit_Field(actor.special_device_damage_flags, SPECIAL_TRANSPORTERS);
    for (int i = 0; i < 4; ++i) target.shield_arc_current[i] = 1;
    DOCTEST_CHECK_FALSE(BOARDING::Check_Human_Boarding(1, 2).allowed);
    for (int i = 0; i < 4; ++i) target.shield_arc_current[i] = 0;
    struct_::Set_Bit_Field(target.special_device_flags, SPECIAL_HARD_SHIELDS);
    DOCTEST_CHECK_FALSE(BOARDING::Check_Human_Boarding(1, 2).allowed);
    struct_::Set_Bit_Field(target.special_device_damage_flags, SPECIAL_HARD_SHIELDS);
    DOCTEST_CHECK(BOARDING::Check_Human_Boarding(1, 2).allowed);
    struct_::Set_Bit_Field(target.special_device_flags, SPECIAL_DAMPER_FIELD);
    BOARDING::boarding.damper_survival_divisor = 0;
    DOCTEST_CHECK_FALSE(BOARDING::Check_Human_Boarding(1, 2).allowed);
    target.drive_current_hits = target.current_speed = 0;
    DOCTEST_CHECK(BOARDING::Check_Human_Boarding(1, 2).allowed);
    target.stasis_source_idx = 1;
    DOCTEST_CHECK_FALSE(BOARDING::Check_Human_Boarding(1, 2).allowed);
    target.stasis_source_idx = COMBAT_SHIP_NONE;
    target.special_status_flag = 4;
    DOCTEST_CHECK_FALSE(BOARDING::Check_Human_Boarding(1, 2).allowed);
    target.special_status_flag = 0;
    target.black_hole_source_idx = 1;
    BOARDING::board_in_black_hole = 0;
    DOCTEST_CHECK_FALSE(BOARDING::Check_Human_Boarding(1, 2).allowed);
    BOARDING::board_in_black_hole = 1;
    DOCTEST_CHECK(BOARDING::Check_Human_Boarding(1, 2).allowed);
    Shutdown_Boarding(&fixture);
}

DOCTEST_TEST_CASE("deliberate capture commits selected marines and changes native eligibility") {
    s_boarding_fixture fixture;
    Initialize_Boarding(&fixture);
    Board(2, 4);
    const auto* result = BOARDING::Boarding_Result();
    DOCTEST_REQUIRE(result != nullptr);
    DOCTEST_CHECK(result->captured);
    DOCTEST_CHECK(result->committed == 4);
    DOCTEST_CHECK(result->defender_lost == 2);
    DOCTEST_CHECK(MOX::combat_data[1].marine_count == 8);
    DOCTEST_CHECK(MOX::combat_data[2].owner == 0);
    DOCTEST_CHECK(CMBTFIR2::Already_Boarded(1));
    DOCTEST_CHECK_FALSE(COMBAT1::Combat_Ship_Is_Functional(2));
    DOCTEST_CHECK(MOX::combat_data[2].marine_count == result->attacker_survived);
    Shutdown_Boarding(&fixture);
}

DOCTEST_TEST_CASE("native boarding zero is a committed failed attempt not cancellation") {
    s_boarding_fixture fixture;
    Initialize_Boarding(&fixture);
    const auto seed = game_random::Get_Random_Seed();
    Board(2, 0);
    DOCTEST_CHECK(CMBTFIR2::Already_Boarded(1));
    DOCTEST_CHECK(MOX::combat_data[1].marine_count == 12);
    DOCTEST_CHECK(MOX::combat_data[2].owner == 1);
    DOCTEST_CHECK_FALSE(BOARDING::Boarding_Result()->captured);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
    Shutdown_Boarding(&fixture);
}

DOCTEST_TEST_CASE("native maximum byte commitment retains modulo marine deduction") {
    s_boarding_fixture fixture;
    Initialize_Boarding(&fixture);
    MOX::combat_data[1].marine_count = 255;
    Board(2, 128);
    DOCTEST_CHECK(BOARDING::Boarding_Result()->committed == 128);
    DOCTEST_CHECK(MOX::combat_data[1].marine_count == 127);
    Shutdown_Boarding(&fixture);
}

DOCTEST_TEST_CASE("native timed capture report expires without an automatic remote answer") {
    s_boarding_fixture fixture;
    Initialize_Boarding(&fixture, true);
    BOARDING::boarding.auto_close_results = 1;
    Board(2, 4);
    DOCTEST_CHECK_FALSE(BOARDING::Boarding_Report_Active());
    DOCTEST_CHECK(BOARDING::Boarding_Result()->captured);
    DOCTEST_CHECK(TEXTBOX::field_save_ix == 0);
    Shutdown_Boarding(&fixture);
}

DOCTEST_TEST_CASE("assault shuttle capture reaches the actual boarding report owner") {
    s_boarding_fixture fixture;
    Initialize_Boarding(&fixture, true);
    bool consumed = false;
    DOCTEST_REQUIRE(BOARDING::Attach_Report_Input(Consume_Shuttle_Report, Complete_Report, &consumed));
    TECHDATA::weapons[WEAPON_ASSAULT_SHUTTLE].type = WEAPON_TYPE_FIGHTER;
    TEST_TACTICAL_TURN::Add_Missile(0, WEAPON_ASSAULT_SHUTTLE, 0);
    CMBTMIS::Resolve_Missile(0, 320, 200);
    DOCTEST_CHECK(consumed);
    COMBAT::Detach_Control(&consumed);
    DOCTEST_REQUIRE(BOARDING::Boarding_Result() != nullptr);
    DOCTEST_CHECK(BOARDING::Boarding_Result()->captured);
    DOCTEST_CHECK(BOARDING::Boarding_Result()->committed == -1);
    DOCTEST_CHECK(MOX::combat_data[2].owner == 0);
    Shutdown_Boarding(&fixture);
}

DOCTEST_TEST_CASE("native boarding popup cancellation changes no gameplay or RNG") {
    s_boarding_fixture fixture;
    Initialize_Boarding(&fixture);
    const auto remaining = allocate::Get_Remaining_Block_Space(MOX::screen_seg);
    const auto actor = MOX::combat_data[1], target = MOX::combat_data[2];
    const auto seed = game_random::Get_Random_Seed();
    key::Push_Key_Code(27, 1);
    COMBAT1::Initiate_Boarding_Action(1, 2);
    DOCTEST_CHECK(std::memcmp(&actor, &MOX::combat_data[1], sizeof(actor)) == 0);
    DOCTEST_CHECK(std::memcmp(&target, &MOX::combat_data[2], sizeof(target)) == 0);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
    DOCTEST_CHECK(allocate::Get_Remaining_Block_Space(MOX::screen_seg) == remaining);
    Shutdown_Boarding(&fixture);
}

DOCTEST_TEST_CASE("native capture can fail and configured garrison limits and telepathy remain native") {
    s_boarding_fixture fixture;
    Initialize_Boarding(&fixture);
    MOX::crew_data[1].ground = -200;
    Board(2, 1);
    DOCTEST_CHECK_FALSE(BOARDING::Boarding_Result()->captured);
    DOCTEST_CHECK(BOARDING::Boarding_Result()->attacker_lost == 1);
    DOCTEST_CHECK(MOX::combat_data[1].marine_count == 11);
    TEST_TACTICAL_TURN::Reset();
    TEST_TACTICAL_TURN::Boarding();
    COMBAT::Open_Control_Decision(1);
    MOX::player[0].traits[TRAIT_TELEPATHIC] = 1;
    BOARDING::kill_excess_marines_after_capture = 1;
    BOARDING::telepathic_multiple_capture = 1;
    const auto hull = MOX::combat_data[2].size_class;
    const auto previous_capacity = TECHDATA::hull_data[hull].marines;
    TECHDATA::hull_data[hull].marines = 1;
    Board(2, 6);
    DOCTEST_CHECK(BOARDING::Boarding_Result()->captured);
    DOCTEST_CHECK(MOX::combat_data[2].marine_count == 1);
    DOCTEST_CHECK(COMBAT1::Combat_Ship_Is_Functional(2));
    TECHDATA::hull_data[hull].marines = previous_capacity;
    Shutdown_Boarding(&fixture);
}

DOCTEST_TEST_CASE("native boarding captures satellites and preserves Quantum Detonator destruction") {
    s_boarding_fixture fixture;
    Initialize_Boarding(&fixture);
    auto& target = MOX::combat_data[2];
    target.ship_idx = -1;
    target.ftl_type = DRIVE_NO_DRIVE;
    Board(2, 4);
    DOCTEST_CHECK(BOARDING::Boarding_Result()->captured);
    DOCTEST_CHECK(COMBINIT::Is_Combat_Satellite(&target));
    TEST_TACTICAL_TURN::Reset();
    TEST_TACTICAL_TURN::Boarding();
    COMBAT::Open_Control_Decision(1);
    target.marine_count = 0;
    struct_::Set_Bit_Field(target.special_device_flags, SPECIAL_QUANTUM_DETONATOR);
    const auto chance = CMBTFIR2::quantum_detonator.explode_chance;
    CMBTFIR2::quantum_detonator.explode_chance = 10;
    Board(2, 4);
    DOCTEST_CHECK(BOARDING::Boarding_Result()->destroyed);
    DOCTEST_CHECK(target.unit_status != 0);
    CMBTFIR2::quantum_detonator.explode_chance = chance;
    Shutdown_Boarding(&fixture);
}

DOCTEST_TEST_CASE("AI native boarding and explicit equal commitment retain resolution and RNG") {
    s_boarding_fixture fixture;
    Initialize_Boarding(&fixture);
    Board(1, 6);
    const auto target = MOX::combat_data[2];
    const int marines = MOX::combat_data[1].marine_count;
    const auto seed = game_random::Get_Random_Seed();
    TEST_TACTICAL_TURN::Reset();
    TEST_TACTICAL_TURN::Boarding();
    CMBTAI::Resolve_Ai_Boarding(1, 2, 1);
    DOCTEST_CHECK(std::memcmp(&target, &MOX::combat_data[2], sizeof(target)) == 0);
    DOCTEST_CHECK(MOX::combat_data[1].marine_count == marines);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
    Shutdown_Boarding(&fixture);
}

DOCTEST_TEST_CASE("local shared boarding commitment matches the controller semantic operation") {
    s_boarding_fixture fixture;
    Initialize_Boarding(&fixture);
    COMBAT1::Commit_Boarding_Action(1, 2, 2, 4);
    const auto actor = MOX::combat_data[1], target = MOX::combat_data[2];
    const auto seed = game_random::Get_Random_Seed();
    TEST_TACTICAL_TURN::Reset();
    TEST_TACTICAL_TURN::Boarding();
    COMBAT::Open_Control_Decision(1);
    Board(2, 4);
    DOCTEST_CHECK(std::memcmp(&actor, &MOX::combat_data[1], sizeof(actor)) == 0);
    DOCTEST_CHECK(std::memcmp(&target, &MOX::combat_data[2], sizeof(target)) == 0);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
    Shutdown_Boarding(&fixture);
}

DOCTEST_TEST_CASE("native raid returns survivors and reports configured internal effects") {
    s_boarding_fixture fixture;
    Initialize_Boarding(&fixture);
    MOX::combat_data[2].computer_current_hits = 100;
    Board(1, 3);
    const auto* result = BOARDING::Boarding_Result();
    DOCTEST_REQUIRE(result != nullptr);
    DOCTEST_CHECK_FALSE(result->capture);
    DOCTEST_CHECK(result->committed == 3);
    DOCTEST_CHECK(result->attacker_survived == 3);
    DOCTEST_CHECK(MOX::combat_data[1].marine_count == 12);
    DOCTEST_CHECK(BOARDING::Boarding_Result()->raid_damage);
    DOCTEST_CHECK((MOX::combat_data[2].computer_current_hits < 100 || MOX::combat_data[2].drive_current_hits < 100));
    DOCTEST_CHECK(MOX::combat_data[2].marine_count == 2 - result->defender_lost);
    Shutdown_Boarding(&fixture);
}

DOCTEST_TEST_CASE("local shared and received boarding preserve results RNG and documented Damper deduction difference") {
    for (int16_t mode : {int16_t(1), int16_t(2)}) {
        s_boarding_fixture fixture;
        Initialize_Boarding(&fixture);
        auto& target = MOX::combat_data[2];
        target.x = 19;
        struct_::Set_Bit_Field(MOX::combat_data[1].special_device_flags, SPECIAL_TRANSPORTERS);
        struct_::Set_Bit_Field(target.special_device_flags, SPECIAL_DAMPER_FIELD);
        Board(mode, 6);
        const auto expected = target;
        const auto seed = game_random::Get_Random_Seed();
        const int local_marines = MOX::combat_data[1].marine_count;
        TEST_TACTICAL_TURN::Reset();
        TEST_TACTICAL_TURN::Boarding();
        target.x = 19;
        struct_::Set_Bit_Field(target.special_device_flags, SPECIAL_DAMPER_FIELD);
        COMBAT1::Apply_Received_Boarding_Action(1, 2, 2, mode);
        DOCTEST_CHECK(std::memcmp(&expected, &target, sizeof(target)) == 0);
        DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
        DOCTEST_CHECK(MOX::combat_data[1].marine_count == local_marines + 4);
        Shutdown_Boarding(&fixture);
    }
}

DOCTEST_TEST_CASE("native boarding report owner consumes explicit acknowledgement and releases media and report storage") {
    for (int iteration = 0; iteration < 4; ++iteration) {
        s_boarding_fixture fixture;
        Initialize_Boarding(&fixture, true);
        s_ack ack;
        const auto remaining = allocate::Get_Remaining_Block_Space(MOX::screen_seg);
        DOCTEST_REQUIRE(BOARDING::Attach_Report_Input(Take_Acknowledgement, Complete_Acknowledgement, &ack));
        Board(iteration % 2 == 0 ? 2 : 1, 4);
        DOCTEST_CHECK(ack.taken);
        DOCTEST_CHECK(ack.completed);
        DOCTEST_CHECK_FALSE(BOARDING::Boarding_Report_Active());
        DOCTEST_CHECK(TEXTBOX::field_save_ix == 0);
        DOCTEST_CHECK(allocate::Get_Remaining_Block_Space(MOX::screen_seg) == remaining);
        COMBAT::Detach_Control(&ack);
        COMBAT::Retire_Battle();
        DOCTEST_CHECK(fixture.battle.boarding.raid_text.capacity() == std::string().capacity());
        DOCTEST_CHECK(fixture.battle.hits.capacity() == 0);
        Shutdown_Boarding(&fixture);
    }
}

DOCTEST_TEST_CASE("tactical choices read production legal grid without state or RNG mutation") {
    TEST_TACTICAL_TURN::Reset();
    COMBAT::s_battle_state battle;
    COMBAT::Open_Battle(&battle, 0);
    COMBAT::Select_Unit(1);
    COMBAT::Open_Control_Decision(1);
    s_probe probe;
    probe.subscription = COMBAT::Subscribe(Read, &probe);
    const auto actor = MOX::combat_data[1];
    const auto seed = game_random::Get_Random_Seed();
    COMBAT::Service_Reads(true);
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
    COMBAT::Close_Battle(&battle, 0);
    COMBAT::Release_Battle(&battle);
    COMBAT::Unsubscribe(probe.subscription);
}

DOCTEST_TEST_CASE("shared tactical operation completes real movement facing firing and done") {
    TEST_TACTICAL_TURN::Reset();
    COMBAT::s_battle_state battle;
    COMBAT::Open_Battle(&battle, 0);
    COMBAT::Select_Unit(1);
    COMBAT::Open_Control_Decision(1);
    COMBAT1::s_operation operation{COMBAT1::MOVE, 1, -1, 14, 10};
    DOCTEST_REQUIRE(rpc::Validate(&operation) == COMBAT1::APPLIED);
    const int cost = CMBTMOV1::Move_Cost(1, 14, 10);
    TEST_TACTICAL_TURN::Apply(&operation);
    DOCTEST_CHECK(int(MOX::combat_data[1].x) == 14);
    DOCTEST_CHECK(int(MOX::combat_data[1].movement_left) == 20 - cost);
    operation = {COMBAT1::FACE, 1, -1, 14, 11};
    TEST_TACTICAL_TURN::Apply(&operation);
    DOCTEST_CHECK(MOX::combat_data[1].facing_dir == CMBTMOV1::Get_Facing(14, 10, 14, 11));
    const auto before = game_random::Get_Random_Seed();
    operation = {COMBAT1::FIRE, 1, 2};
    TEST_TACTICAL_TURN::Apply(&operation);
    DOCTEST_CHECK(MOX::combat_data[1].weapons[0].shots_left == 0);
    DOCTEST_CHECK(game_random::Get_Random_Seed() != before);
    operation = {COMBAT1::DONE, 1};
    TEST_TACTICAL_TURN::Apply(&operation);
    DOCTEST_CHECK(CMBTFIR2::Combat_Ship_Done(1) != 0);
    DOCTEST_CHECK(MOX::ship_turn_order[0] == -1);
    DOCTEST_CHECK(COMBAT::Current_Unit() == -1);
    COMBAT::Close_Battle(&battle, 0);
    COMBAT::Release_Battle(&battle);
}

DOCTEST_TEST_CASE("shared operations match direct native move face and complete firing with controlled RNG") {
    for (auto kind : {COMBAT1::MOVE, COMBAT1::FACE, COMBAT1::FIRE}) {
        TEST_TACTICAL_TURN::Reset();
        const COMBAT1::s_operation operation{kind, 1, 2, 14, 11};
        if (kind == COMBAT1::MOVE) {
            const auto cost = CMBTMOV1::Move_Cost(1, 14, 11);
            const auto used = CMBTFIR2::Used_Teleporter(1);
            const auto actual = CMBTMOV1::Move_Ship(1, 14, 11, cost);
            if (used == CMBTFIR2::Used_Teleporter(1)) MOX::combat_data[1].movement_left -= actual;
            COMBAT1::Assign_Combat_Grids(1);
            CMBTDRW1::Refresh_Combat_Screen_Full();
        } else if (kind == COMBAT1::FACE) {
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
    COMBAT::s_battle_state battle;
    COMBAT::Open_Battle(&battle, 0);
    COMBAT::Select_Unit(1);
    COMBAT::Open_Control_Decision(1);
    s_probe probe;
    probe.subscription = COMBAT::Subscribe(Read, &probe);
    COMBAT::Service_Reads(true);
    DOCTEST_REQUIRE(std::find(probe.options.selectable.begin(), probe.options.selectable.end(), 3) != probe.options.selectable.end());
    CMBTFIR2::Set_Combat_Ship_Done(3);
    const COMBAT1::s_operation select{COMBAT1::SELECT, 1, 3};
    DOCTEST_REQUIRE(rpc::Validate(&select) == COMBAT1::APPLIED);
    TEST_TACTICAL_TURN::Apply(&select);
    DOCTEST_CHECK(MOX::cur_ship == 3);
    DOCTEST_CHECK(MOX::ship_turn_order[MOX::cur_ptr] == 3);
    COMBAT::Open_Control_Decision(3);
    MOX::combat_init = 1;
    COMBAT::Service_Reads(true);
    DOCTEST_CHECK(probe.options.selectable.empty());
    const COMBAT1::s_operation other{COMBAT1::SELECT, 3, 1};
    DOCTEST_CHECK(rpc::Validate(&other) == COMBAT1::ILLEGAL);
    MOX::net_flag = 1;
    MOX::combat_data[3].owner = 1;
    COMBAT::Service_Reads(true);
    DOCTEST_CHECK_FALSE(probe.options.available);
    DOCTEST_CHECK(rpc::Validate(&other) == COMBAT1::NO_CONTROL);
    COMBAT::Close_Battle(&battle, 0);
    COMBAT::Release_Battle(&battle);
    COMBAT::Unsubscribe(probe.subscription);
}

DOCTEST_TEST_CASE("normal fire retains slotless Energy Absorber discharge and rejects stasis decisions first") {
    TEST_TACTICAL_TURN::Reset();
    COMBAT::s_battle_state battle;
    COMBAT::Open_Battle(&battle, 0);
    COMBAT::Select_Unit(1);
    COMBAT::Open_Control_Decision(1);
    MOX::combat_data[1].weapons[0] = {};
    MOX::combat_data[1].reflected_damage_pool = 40;
    struct_::Set_Bit_Field(MOX::combat_data[1].special_device_flags, SPECIAL_ENERGY_ABSORBER);
    MOX::combat_data[1].weapon_ready_flags[0] = 1;
    const COMBAT1::s_operation fire{COMBAT1::FIRE, 1, 2};
    s_probe probe;
    probe.subscription = COMBAT::Subscribe(Read, &probe);
    COMBAT::Service_Reads(true);
    DOCTEST_REQUIRE(probe.options.targets.size() == 1);
    DOCTEST_CHECK(probe.options.targets[0].absorber);
    MOX::combat_data[2].stasis_source_idx = 1;
    DOCTEST_CHECK(rpc::Validate(&fire) == COMBAT1::LOCAL_REQUIRED);
    DOCTEST_CHECK(int(MOX::combat_data[1].reflected_damage_pool) == 40);
    MOX::combat_data[2].stasis_source_idx = COMBAT_SHIP_NONE;
    DOCTEST_REQUIRE(rpc::Validate(&fire) == COMBAT1::APPLIED);
    TEST_TACTICAL_TURN::Apply(&fire);
    DOCTEST_CHECK(int(MOX::combat_data[1].reflected_damage_pool) == 0);
    COMBAT::Close_Battle(&battle, 0);
    COMBAT::Release_Battle(&battle);
    COMBAT::Unsubscribe(probe.subscription);
}

DOCTEST_TEST_CASE("native partial facing and teleporter movement keep their original costs") {
    TEST_TACTICAL_TURN::Reset();
    MOX::combat_data[1].movement_left = 1;
    const COMBAT1::s_operation face{COMBAT1::FACE, 1, -1, 12, 10};
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
    const COMBAT1::s_operation move{COMBAT1::MOVE, 1, -1, 14, 10};
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
            COMBAT1::s_operation selection{COMBAT1::FIRE_SELECTION, 1};
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
    COMBAT::s_battle_state battle;
    COMBAT::Open_Battle(&battle, 0);
    COMBAT::Open_Control_Decision(1);
    MOX::combat_data[1].weapons[1] = MOX::combat_data[1].weapons[0];
    MOX::combat_data[1].weapons[2] = MOX::combat_data[1].weapons[0];
    MOX::combat_data[1].weapons[1].active = 0;
    MOX::combat_data[1].weapons[2].active = -1;
    s_probe probe;
    probe.subscription = COMBAT::Subscribe(Read, &probe);
    const COMBAT1::s_operation fire{COMBAT1::FIRE, 1, 2};
    TEST_TACTICAL_TURN::Apply(&fire);
    DOCTEST_CHECK(MOX::combat_data[1].weapons[0].shots_left == 0);
    DOCTEST_CHECK(MOX::combat_data[1].weapons[1].shots_left == 2);
    DOCTEST_CHECK(MOX::combat_data[1].weapons[1].active == 1);
    DOCTEST_CHECK(MOX::combat_data[1].weapons[2].shots_left == 2);
    DOCTEST_CHECK(MOX::combat_data[1].weapons[2].active == -1);
    COMBAT::Service_Reads(true);
    DOCTEST_REQUIRE_FALSE(probe.history.hits.empty());
    DOCTEST_CHECK(std::string(probe.history.hits[0].target_name) == "Opponent");
    const auto seed = game_random::Get_Random_Seed();
    const auto count = probe.history.hits.size();
    COMBAT::Service_Reads(true);
    DOCTEST_CHECK(probe.history.hits.size() == count);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
    COMBAT::Close_Battle(&battle, 0);
    COMBAT::Release_Battle(&battle);
    DOCTEST_CHECK(battle.hits.capacity() == 0);
    COMBAT::Unsubscribe(probe.subscription);
}

DOCTEST_TEST_CASE("missile reads cover the global native domain without mutating or simulating") {
    TEST_TACTICAL_TURN::Reset();
    COMBAT::s_battle_state battle;
    COMBAT::Open_Battle(&battle, 0);
    TEST_TACTICAL_TURN::Add_Missile(4999);
    TEST_TACTICAL_TURN::Add_Missile(4998, WEAPON_ANTI_MATTER_TORP);
    MOX::missile[4999].x = 70 * 20;
    COMBAT::Open_Control_Decision(1);
    const auto missile = MOX::missile[4999];
    const auto seed = game_random::Get_Random_Seed();
    s_probe probe;
    probe.subscription = COMBAT::Subscribe(Read, &probe);
    COMBAT::Service_Reads(true);
    DOCTEST_REQUIRE(probe.missiles.size() == 2);
    DOCTEST_CHECK(probe.missiles[1].slot == 4999);
    DOCTEST_CHECK(probe.missiles[1].x == 70);
    DOCTEST_CHECK(probe.options.missile_targets.size() == 2);
    DOCTEST_CHECK(probe.options.missile_targets[0].weapons == 0);
    DOCTEST_CHECK(std::memcmp(&missile, &MOX::missile[4999], sizeof(missile)) == 0);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
    COMBAT::Close_Battle(&battle, 0);
    COMBAT::Release_Battle(&battle);
    COMBAT::Unsubscribe(probe.subscription);
}

DOCTEST_TEST_CASE("native anti-missile fire intercepts missiles and refuses fighters torpedoes and heavy beams") {
    TEST_TACTICAL_TURN::Reset();
    COMBAT::s_battle_state battle;
    COMBAT::Open_Battle(&battle, 0);
    TEST_TACTICAL_TURN::Add_Missile(0);
    COMBAT::Open_Control_Decision(1);
    COMBAT1::s_operation fire{COMBAT1::FIRE, 1, 0};
    fire.missile_target = true;
    MOX::combat_data[1].weapons[0].specials = WEAPON_MOD_FLAG_HEAVY_MOUNT;
    DOCTEST_CHECK(rpc::Validate(&fire) == COMBAT1::ILLEGAL);
    MOX::combat_data[1].weapons[0] = {WEAPON_ANTI_MISSILE_ROCKET, 2, WEAPON_FIRING_ARC_ALL_SECTORS, 0, 5, 1, 2, 0};
    TECHDATA::weapons[WEAPON_ANTI_MISSILE_ROCKET].type = WEAPON_TYPE_SPECIAL;
    const auto accuracy = CMBTSPEC::anti_missile_rocket_accuracy;
    CMBTSPEC::anti_missile_rocket_accuracy.base = 101;
    CMBTSPEC::anti_missile_rocket_accuracy.range_multiplier = 0;
    TEST_TACTICAL_TURN::Add_Missile(0, WEAPON_INTERCEPTOR);
    DOCTEST_CHECK(rpc::Validate(&fire) == COMBAT1::ILLEGAL);
    TEST_TACTICAL_TURN::Add_Missile(0, WEAPON_ANTI_MATTER_TORP);
    DOCTEST_CHECK(rpc::Validate(&fire) == COMBAT1::ILLEGAL);
    TEST_TACTICAL_TURN::Add_Missile(0);
    DOCTEST_REQUIRE(rpc::Validate(&fire) == COMBAT1::APPLIED);
    TEST_TACTICAL_TURN::Apply(&fire);
    DOCTEST_CHECK(int(MOX::missile[0].quantity) == 4);
    DOCTEST_CHECK(int(MOX::combat_data[1].weapons[0].ammo) == 3);
    DOCTEST_CHECK(MOX::combat_data[1].weapons[0].shots_left == 0);
    s_probe probe;
    probe.subscription = COMBAT::Subscribe(Read, &probe);
    COMBAT::Service_Reads(true);
    DOCTEST_REQUIRE(probe.history.hits.size() == 1);
    DOCTEST_CHECK(probe.history.hits[0].kind == COMBAT::MISSILES_DESTROYED);
    DOCTEST_CHECK(probe.history.hits[0].amount == 2);
    CMBTSPEC::anti_missile_rocket_accuracy = accuracy;
    COMBAT::Close_Battle(&battle, 0);
    COMBAT::Release_Battle(&battle);
    COMBAT::Unsubscribe(probe.subscription);
}

DOCTEST_TEST_CASE("native hit history is bounded filtered and releases storage between combats") {
    TEST_TACTICAL_TURN::Reset();
    for (int combat = 0; combat < 20; ++combat) {
        COMBAT::s_battle_state battle;
        COMBAT::Open_Battle(&battle, 0);
        for (size_t i = 0; i < COMBAT::MAX_HIT_RECORDS + 10; ++i) CMBTFIRE::Record_Unit_Hit(2, 1);
        DOCTEST_CHECK(battle.hits.size() == COMBAT::MAX_HIT_RECORDS);
        MOX::combat_data[2].special_status_flag = 4;
        const auto sequence = battle.next_hit_sequence;
        CMBTFIRE::Record_Unit_Hit(2, 999);
        DOCTEST_CHECK(battle.next_hit_sequence == sequence);
        MOX::combat_data[2].special_status_flag = 0;
        s_probe probe;
        probe.subscription = COMBAT::Subscribe(Read, &probe);
        COMBAT::Service_Reads(true);
        DOCTEST_CHECK(probe.history.first_sequence == 11);
        if (combat % 2)
            COMBAT::Revoke();
        else
            COMBAT::Retire_Battle();
        DOCTEST_CHECK(battle.hits.capacity() == 0);
        COMBAT::Close_Battle(&battle, 0);
        COMBAT::Release_Battle(&battle);
        DOCTEST_CHECK(battle.hits.capacity() == 0);
        COMBAT::Unsubscribe(probe.subscription);
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
        COMBAT1::s_operation fire{COMBAT1::FIRE, 1, slot};
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
