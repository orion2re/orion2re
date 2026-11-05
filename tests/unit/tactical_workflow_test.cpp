#include "rpc/turn_fixture.h"
#include "rpc/rpc_combat_control.h"
#include "doctest/doctest.h"

namespace {
    struct s_workflow_read {
        CMBTDRW1::s_combat_detail detail;
        rpc::s_options options;
        bool scanned = false;
        bool condition = false;
        CMBTDRW1::s_damage_bars bars;
        std::string help;
        unsigned changes = 0;
    };
    void Read_Workflow(const COMBAT::s_event* event, void* context) noexcept {
        auto* read = (s_workflow_read*)context;
        if (event->kind == COMBAT::VIEW_INVALIDATED) ++read->changes;
        if (event->kind != COMBAT::SERVICE_READS || !event->safe) return;
        read->scanned = CMBTDRW1::Read_Combat_Detail(2, &read->detail);
        read->condition = CMBTDRW1::Read_Damage_Bars(2, &read->bars);
        CMBTDRW1::Read_Tactical_Help(5, &read->help);
        DOCTEST_CHECK(rpc::Read_Options(&read->options));
    }
    struct s_native_answer {
        int16_t value = 0, marines = 0, mode = 0;
        unsigned taken = 0, completed = 0;
        unsigned reports = 0;
    };
    bool Take_Answer(COMBAT1::s_operation* operation, COMBAT1::e_result* result, void* context) {
        auto* answer = (s_native_answer*)context;
        const auto* input = CMBTDRW1::Current_Tactical_Input();
        if (input == nullptr) return false;
        operation->kind = COMBAT1::ANSWER_CONFIRMATION;
        operation->value = answer->value;
        if (input->kind == CMBTDRW1::BOARDING_CHOICE) {
            operation->kind = COMBAT1::ANSWER_BOARDING;
            operation->boarding_mode = answer->mode;
            operation->marines = answer->marines;
        } else if (input->kind == CMBTDRW1::RETURN_TO_MANUAL)
            operation->kind = COMBAT1::MANUAL_CONTROL;
        else if (input->kind == CMBTDRW1::TACTICAL_INFORMATION)
            operation->kind = COMBAT1::ACK_INFORMATION;
        *result = rpc::Validate(operation);
        DOCTEST_REQUIRE(*result == COMBAT1::APPLIED);
        ++answer->taken;
        return true;
    }
    void Complete_Answer(COMBAT1::e_result result, void* context) {
        DOCTEST_CHECK(result == COMBAT1::APPLIED);
        ++((s_native_answer*)context)->completed;
    }
    bool Take_Auto_Report(uint64_t generation, void* context) {
        DOCTEST_CHECK(BOARDING::Boarding_Report_Active());
        DOCTEST_CHECK(generation == BOARDING::Boarding_Generation());
        ++((s_native_answer*)context)->reports;
        return true;
    }
    void Complete_Auto_Report(void*) {}
    void Open_Workflow(COMBAT::s_battle_state* battle, int mode = 0) {
        TEST_TACTICAL_TURN::Reset();
        TEST_TACTICAL_TURN::Workflow(mode);
        COMBAT::Open_Battle(battle, 0);
        COMBAT::Select_Unit(1);
        COMBAT::Open_Control_Decision(1);
    }
    void Close_Workflow(COMBAT::s_battle_state* battle) {
        COMBAT::Detach_Control(battle);
        COMBAT::Close_Battle(battle, 0);
        COMBAT::Release_Battle(battle);
        TEST_TACTICAL_TURN::Release_Boarding();
    }
} // namespace

DOCTEST_TEST_CASE("tactical scan help systems and choices read live state without preparing presentation") {
    COMBAT::s_battle_state battle;
    Open_Workflow(&battle);
    s_workflow_read read;
    const auto subscription = COMBAT::Subscribe(Read_Workflow, &read);
    COMBAT::Service_Reads(true);
    DOCTEST_REQUIRE(read.scanned);
    DOCTEST_CHECK(read.detail.name == MOX::combat_data[2].name);
    DOCTEST_CHECK(read.detail.marines == MOX::combat_data[2].marine_count);
    DOCTEST_CHECK(read.help.find("Synthetic tactical help") != std::string::npos);
    const auto generation = rpc::Control_Generation();
    const auto seed = game_random::Get_Random_Seed();
    const auto scratch = MOX::global_combat_data;
    const auto changes = read.changes;
    std::vector<s_combat_data> units(MOX::combat_data, MOX::combat_data + MOX::combat_total_ship_count);
    std::vector<char> grid(sizeof(MOX::combat_grid)), order(sizeof(MOX::ship_turn_order));
    std::memcpy(grid.data(), MOX::combat_grid, grid.size());
    std::memcpy(order.data(), MOX::ship_turn_order, order.size());
    for (int i = 0; i < 10; ++i) COMBAT::Service_Reads(true);
    DOCTEST_CHECK(std::memcmp(units.data(), MOX::combat_data, units.size() * sizeof(units[0])) == 0);
    DOCTEST_CHECK(std::memcmp(grid.data(), MOX::combat_grid, grid.size()) == 0);
    DOCTEST_CHECK(std::memcmp(order.data(), MOX::ship_turn_order, order.size()) == 0);
    DOCTEST_CHECK(std::memcmp(&scratch, &MOX::global_combat_data, sizeof(scratch)) == 0);
    DOCTEST_CHECK(rpc::Control_Generation() == generation);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
    DOCTEST_CHECK(read.changes == changes);
    MOX::combat_data[2].special_status_flag = 4;
    COMBAT::Service_Reads(true);
    DOCTEST_CHECK(read.scanned); // Native scan is separately authorized, unlike the short contact row.
    MOX::combat_data[2].size_class = 6;
    COMBAT::Service_Reads(true);
    DOCTEST_CHECK_FALSE(read.scanned);
    MOX::combat_data[2].size_class = 0;
    COMBAT1::disable_scan = 1;
    COMBAT::Service_Reads(true);
    DOCTEST_CHECK_FALSE(read.scanned);
    MOX::combat_data[2].special_status_flag = 0;
    COMBAT::Service_Reads(true);
    DOCTEST_CHECK(read.condition);
    COMBAT1::disable_scan = 0;
    MOX::combat_data[2].structure_damage = 17;
    COMBAT::Service_Reads(true);
    DOCTEST_CHECK(read.detail.structure == MOX::combat_data[2].structure_max - 17);
    DOCTEST_CHECK(read.bars.structure == 2);
    MOX::combat_data[2].stasis_source_idx = COMBAT_SHIP_NONE;
    MOX::combat_data[2].reflected_damage_pool = 1;
    COMBAT::Service_Reads(true);
    DOCTEST_CHECK(read.bars.effect == CMBTDRW1::EFFECT_ABSORBER);
    COMBAT::Unsubscribe(subscription);
    Close_Workflow(&battle);
}

DOCTEST_TEST_CASE("native stasis confirmation explicitly declines then releases without firing") {
    COMBAT::s_battle_state battle;
    Open_Workflow(&battle);
    s_native_answer answer;
    DOCTEST_REQUIRE(COMBAT::Attach_Control(Take_Answer, Complete_Answer, &answer));
    COMBAT1::s_operation operation;
    operation.kind = COMBAT1::REMOVE_STASIS;
    operation.actor = 1;
    operation.target = 2;
    const auto seed = game_random::Get_Random_Seed();
    const auto weapons = MOX::combat_data[1];
    DOCTEST_REQUIRE(rpc::Validate(&operation) == COMBAT1::APPLIED);
    TEST_TACTICAL_TURN::Apply(&operation);
    DOCTEST_CHECK(CMBTMOV1::Ship_Is_In_Stasis(2));
    DOCTEST_CHECK(answer.taken == 1);
    DOCTEST_CHECK(answer.completed == 1);
    answer.value = 1;
    MOX::combat_data[2].special_status_flag = 4;
    COMBAT::Open_Control_Decision(1);
    DOCTEST_CHECK(rpc::Validate(&operation) == COMBAT1::APPLIED); // Native enemy-click stasis branch precedes phasing refusal.
    COMBAT1::s_operation fire;
    fire.kind = COMBAT1::FIRE;
    fire.actor = 1;
    fire.target = 2;
    DOCTEST_CHECK(rpc::Validate(&fire) == COMBAT1::UNAVAILABLE);
    TEST_TACTICAL_TURN::Apply(&operation);
    DOCTEST_CHECK_FALSE(CMBTMOV1::Ship_Is_In_Stasis(2));
    DOCTEST_CHECK(answer.completed == 2);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
    DOCTEST_CHECK(std::memcmp(weapons.weapons, MOX::combat_data[1].weapons, sizeof(weapons.weapons)) == 0);
    DOCTEST_CHECK(CMBTDRW1::Current_Tactical_Input() == nullptr);
    COMBAT::Detach_Control(&answer);
    Close_Workflow(&battle);
}

DOCTEST_TEST_CASE("native self destruct retains explicit no and yes and next actor handling") {
    COMBAT::s_battle_state battle;
    Open_Workflow(&battle);
    s_native_answer answer;
    DOCTEST_REQUIRE(COMBAT::Attach_Control(Take_Answer, Complete_Answer, &answer));
    COMBAT1::Set_Input_Mode(COMBAT1::OPTIONS_MODE);
    COMBAT::Open_Control_Decision(1);
    COMBAT1::s_operation operation;
    operation.kind = COMBAT1::SELF_DESTRUCT;
    operation.actor = 1;
    DOCTEST_REQUIRE(rpc::Validate(&operation) == COMBAT1::APPLIED);
    TEST_TACTICAL_TURN::Apply(&operation);
    DOCTEST_CHECK(MOX::combat_data[1].unit_status == 0);
    DOCTEST_CHECK(MOX::order_info_flag == 0);
    answer.value = 1;
    TEST_TACTICAL_TURN::Apply(&operation);
    DOCTEST_CHECK(MOX::combat_data[1].unit_status != 0);
    DOCTEST_CHECK(MOX::cur_ship == 3);
    DOCTEST_CHECK(answer.completed == 2);
    COMBAT::Detach_Control(&answer);
    Close_Workflow(&battle);
}

DOCTEST_TEST_CASE("side wait and end side use native sequencing in initiative and strict passes") {
    for (int mode : {1, 2}) {
        for (bool received : {false, true}) {
            COMBAT::s_battle_state battle;
            Open_Workflow(&battle, mode);
            DOCTEST_REQUIRE(COMBAT1::Combat_Side_Wait_Is_Available());
            COMBAT1::s_operation operation;
            operation.kind = COMBAT1::SIDE_WAIT;
            operation.actor = 1;
            DOCTEST_REQUIRE(rpc::Validate(&operation) == COMBAT1::APPLIED);
            const auto seed = game_random::Get_Random_Seed();
            int16_t next = 0;
            COMBAT1::Apply_Tactical_Operation(&operation, 0, &next, received);
            DOCTEST_CHECK(next == 0);
            DOCTEST_CHECK_FALSE(CMBTFIR2::Combat_Ship_Done(1));
            if (mode == 2) {
                DOCTEST_CHECK(COMBAT1::Current_Pass_Is_Strict_Wait());
                DOCTEST_CHECK_FALSE(COMBAT1::Combat_Side_Wait_Is_Available());
            }
            COMBAT::Open_Control_Decision(MOX::cur_ship);
            operation.kind = COMBAT1::END_SIDE;
            operation.actor = MOX::cur_ship;
            DOCTEST_REQUIRE(rpc::Validate(&operation) == COMBAT1::APPLIED);
            COMBAT1::Apply_Tactical_Operation(&operation, 0, &next, received);
            DOCTEST_CHECK(MOX::side_end_of_turn[0] == 1);
            DOCTEST_CHECK(next == 1);
            DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
            Close_Workflow(&battle);
        }
    }
}

DOCTEST_TEST_CASE("native boarding choice cancellation and overload acknowledgement stay owner consumed") {
    COMBAT::s_battle_state battle;
    Open_Workflow(&battle);
    s_native_answer answer;
    DOCTEST_REQUIRE(COMBAT::Attach_Control(Take_Answer, Complete_Answer, &answer));
    const auto seed = game_random::Get_Random_Seed();
    int16_t marines = -1;
    char text[] = "Synthetic boarding choice";
    DOCTEST_CHECK(CMBTDRW1::Board_Popup(0, 6, text, &marines) == 0);
    DOCTEST_CHECK(marines == 6);
    DOCTEST_CHECK(answer.completed == 1);
    CMBTDRW1::Show_Absorber_Overload_Warning();
    DOCTEST_CHECK(answer.completed == 2);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
    COMBAT::Detach_Control(&answer);
    Close_Workflow(&battle);
}

DOCTEST_TEST_CASE("planet defenses and satellites use the native human actor path without ship-only authority") {
    COMBAT::s_battle_state battle;
    Open_Workflow(&battle);
    s_colony colony = {};
    s_planet_data planet = {};
    auto* previous_colony = MOX::colony;
    auto* previous_planet = MOX::planet;
    MOX::colony = &colony;
    MOX::planet = &planet;
    MOX::cmbt_colony_id = 0;
    colony.buildings[BUILDING_MISSILE_BASE] = 1;
    MOX::combat_data[0] = MOX::combat_data[1];
    MOX::combat_data[0].ship_idx = -1;
    MOX::combat_data[0].ftl_type = 0;
    MOX::combat_data[0].movement_left = 0;
    MOX::combat_data[0].current_speed = 0;
    MOX::combat_data[0].x = 40;
    MOX::combat_data[3].ship_idx = -1;
    MOX::combat_data[3].ftl_type = 0;
    MOX::combat_data[3].movement_left = 0;
    MOX::combat_data[3].current_speed = 0;
    MOX::cur_ship = 0;
    MOX::cur_ptr = 0;
    MOX::ship_turn_order[0] = 0;
    COMBAT::Select_Unit(0);
    COMBAT::Open_Control_Decision(0);
    COMBAT1::s_operation operation;
    operation.kind = COMBAT1::DONE;
    operation.actor = 0;
    DOCTEST_CHECK(rpc::Validate(&operation) == COMBAT1::APPLIED);
    operation.kind = COMBAT1::SELF_DESTRUCT;
    DOCTEST_CHECK(rpc::Validate(&operation) == COMBAT1::ILLEGAL);
    operation.kind = COMBAT1::SELECT;
    operation.target = 3;
    DOCTEST_REQUIRE(rpc::Validate(&operation) == COMBAT1::APPLIED);
    TEST_TACTICAL_TURN::Apply(&operation);
    COMBAT::Open_Control_Decision(3);
    operation.kind = COMBAT1::DONE;
    operation.actor = 3;
    DOCTEST_CHECK(rpc::Validate(&operation) == COMBAT1::APPLIED);
    MOX::colony = previous_colony;
    MOX::planet = previous_planet;
    MOX::cmbt_colony_id = -1;
    Close_Workflow(&battle);
}

DOCTEST_TEST_CASE("native explicit auto combat can return through its existing AI input checks") {
    COMBAT::s_battle_state battle;
    Open_Workflow(&battle);
    s_native_answer answer;
    DOCTEST_REQUIRE(COMBAT::Attach_Control(Take_Answer, Complete_Answer, &answer));
    DOCTEST_REQUIRE(BOARDING::Attach_Report_Input(Take_Auto_Report, Complete_Auto_Report, &answer));
    COMBAT1::Set_Auto_Combat(false);
    DOCTEST_CHECK(MOX::auto_combat_flag[0] == 1);
    CMBTAI::Do_Auto_Ship_Turn();
    DOCTEST_CHECK(answer.taken == 1);
    DOCTEST_CHECK(answer.completed == 1);
    DOCTEST_CHECK(MOX::auto_combat_flag[0] == 0);
    DOCTEST_CHECK(MOX::take_control_flag == 0);
    COMBAT::Detach_Control(&answer);
    Close_Workflow(&battle);
}

DOCTEST_TEST_CASE("local end side remains available in classic initiative strict and Time Warp input") {
    for (int mode : {0, 1, 2}) {
        for (bool time_warp : {false, true}) {
            COMBAT::s_battle_state battle;
            Open_Workflow(&battle, mode);
            struct_::Set_Bit_Field(MOX::combat_data[1].special_device_flags, SPECIAL_TIME_WARP_FACILITATOR);
            key::Push_Key_Code('t', 1);
            COMBAT1::Do_Combat_Turn(0, time_warp ? 1 : 0);
            DOCTEST_CHECK(COMBAT::Current_Control_Actor() == -1);
            DOCTEST_CHECK_FALSE(COMBAT1::Current_Pass_Is_Time_Warp());
            if (mode != 0) DOCTEST_CHECK(MOX::side_end_of_turn[0] == 1);
            Close_Workflow(&battle);
        }
    }
}

DOCTEST_TEST_CASE("super fast automation retains the native human-versus-human restriction and explicit disable") {
    COMBAT::s_battle_state battle;
    Open_Workflow(&battle);
    DOCTEST_CHECK(COMBAT1::Auto_Combat_Is_Available(true));
    MOX::player[1].objectives = PLAYER_OBJECTIVE_HUMAN;
    DOCTEST_CHECK_FALSE(COMBAT1::Auto_Combat_Is_Available(true));
    MOX::player[1].objectives = PLAYER_OBJECTIVE_DIPLOMATIC;
    const auto seed = game_random::Get_Random_Seed();
    COMBAT1::Set_Auto_Combat(true);
    DOCTEST_CHECK(MOX::super_fast_flag == 1);
    DOCTEST_CHECK(MOX::auto_combat_flag[0] == 1);
    CMBTAI::Toggle_Auto_Combat_Flags();
    DOCTEST_CHECK(MOX::super_fast_flag == 0);
    DOCTEST_CHECK(MOX::auto_combat_flag[0] == 0);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
    Close_Workflow(&battle);
}

DOCTEST_TEST_CASE("local stasis decline and owner-confirmed send differ from native received application") {
    COMBAT::s_battle_state battle;
    Open_Workflow(&battle);
    s_native_answer answer;
    DOCTEST_REQUIRE(COMBAT::Attach_Control(Take_Answer, Complete_Answer, &answer));
    MOX::net_flag = 1;
    MOX::net_destination = 1;
    MOX::combat_done_waiting = 0; // Native fixture loopback, not a multiplayer session.
    MOX::new_message = {};
    COMBAT1::s_operation operation;
    operation.kind = COMBAT1::REMOVE_STASIS;
    operation.actor = 1;
    operation.target = 2;
    const auto seed = game_random::Get_Random_Seed();
    key::Push_Key_Code('n', 1);
    TEST_TACTICAL_TURN::Apply(&operation);
    DOCTEST_CHECK(CMBTMOV1::Ship_Is_In_Stasis(2));
    DOCTEST_CHECK(int(MOX::new_message.message_code) == 0);
    DOCTEST_CHECK(answer.taken == 0);
    answer.value = 1;
    TEST_TACTICAL_TURN::Apply(&operation);
    DOCTEST_CHECK_FALSE(CMBTMOV1::Ship_Is_In_Stasis(2));
    DOCTEST_CHECK(int(MOX::new_message.message_code) == TACTICAL_NET_MSG_REMOVE_STASIS);
    DOCTEST_CHECK(int(MOX::new_message.arg1) == 2);
    DOCTEST_CHECK(uint32_t(MOX::new_message.random_seed) == seed);
    DOCTEST_CHECK(answer.taken == 1);
    MOX::combat_data[2].stasis_source_idx = 2;
    MOX::new_message = {};
    MOX::tactical_net_choice = 0;
    int16_t next = 0;
    COMBAT1::Apply_Tactical_Operation(&operation, 0, &next, true);
    DOCTEST_CHECK_FALSE(CMBTMOV1::Ship_Is_In_Stasis(2));
    DOCTEST_CHECK(int(MOX::new_message.message_code) == 0);
    DOCTEST_CHECK(answer.taken == 1);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
    MOX::net_flag = 0;
    COMBAT::Detach_Control(&answer);
    Close_Workflow(&battle);
}

DOCTEST_TEST_CASE("native scan and boarding modes retain missile interception and facing controls") {
    COMBAT::s_battle_state battle;
    Open_Workflow(&battle);
    TEST_TACTICAL_TURN::Add_Missile(0);
    s_workflow_read read;
    const auto subscription = COMBAT::Subscribe(Read_Workflow, &read);
    for (auto mode : {COMBAT1::SCAN_MODE, COMBAT1::BOARDING_MODE}) {
        COMBAT1::Set_Input_Mode(mode);
        COMBAT::Open_Control_Decision(1);
        COMBAT::Service_Reads(true);
        DOCTEST_REQUIRE(read.options.available);
        DOCTEST_REQUIRE(read.options.missile_targets.size() == 1);
        DOCTEST_CHECK(read.options.missile_targets[0].weapons != 0);
        COMBAT1::s_operation operation;
        operation.kind = COMBAT1::FIRE;
        operation.actor = 1;
        operation.target = 0;
        operation.missile_target = true;
        DOCTEST_CHECK(rpc::Validate(&operation) == COMBAT1::APPLIED);
        operation.kind = COMBAT1::FACE;
        operation.x = 13;
        operation.y = 11;
        DOCTEST_CHECK(rpc::Validate(&operation) == COMBAT1::APPLIED);
    }
    COMBAT::Unsubscribe(subscription);
    Close_Workflow(&battle);
}
