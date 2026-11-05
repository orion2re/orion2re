#include "rpc/turn_fixture.h"
#include "rpc/rpc_combat_control.h"
#include "rpc/rpc_tactical_view.h"
#include "game/combat.h"
#include "doctest/doctest.h"
#include <memory>

namespace {
    struct s_withdrawal {
        COMBAT::s_battle_state battle;
        std::vector<COMBAT::s_event> outcomes;
        uint64_t subscription;
        int16_t previous_ground = MOX::crew_data[1].ground;
    };
    void Receive_Outcome(const COMBAT::s_event* event, void* context) noexcept {
        if (event->kind == COMBAT::RETREAT_RESOLVED) ((s_withdrawal*)context)->outcomes.push_back(*event);
    }
    void Initialize_Withdrawal(s_withdrawal* fixture) {
        TEST_TACTICAL_TURN::Reset();
        TEST_TACTICAL_TURN::Withdrawal();
        COMBAT::Open_Battle(&fixture->battle, 0);
        COMBAT::Select_Unit(1);
        COMBAT::Open_Control_Decision(1);
        fixture->subscription = COMBAT::Subscribe(Receive_Outcome, fixture);
    }
    void Shutdown_Withdrawal(s_withdrawal* fixture) {
        COMBAT::Detach_Control(fixture);
        COMBAT::Unsubscribe(fixture->subscription);
        COMBAT::Close_Battle(&fixture->battle, 0);
        COMBAT::Release_Battle(&fixture->battle);
        TEST_TACTICAL_TURN::Release_Boarding();
        MOX::crew_data[1].ground = fixture->previous_ground;
    }
    void Begin_Withdrawal(int16_t actor = 1, bool received = false) {
        MOX::cur_ship = actor;
        MOX::cur_ptr = 0;
        MOX::ship_turn_order[0] = actor;
        MOX::ship_turn_order[1] = -10;
        COMBAT::Open_Control_Decision(actor);
        const COMBAT1::s_operation operation{COMBAT1::RETREAT, actor};
        DOCTEST_REQUIRE(rpc::Validate(&operation) == COMBAT1::APPLIED);
        int16_t next = 0;
        COMBAT1::Apply_Tactical_Operation(&operation, 0, &next, received);
        DOCTEST_CHECK(next == 1);
        DOCTEST_CHECK(MOX::combat_data[actor].is_retreating == 1);
        DOCTEST_CHECK(MOX::combat_data[actor].unit_status == 0);
    }
    void Retire_Other_Own() {
        CMBTFIRE::Destroy_Ship(3, 380, 200, 1);
    }
    struct s_script {
        int step = 0;
    };
    bool Take_Script(COMBAT1::s_operation* operation, COMBAT1::e_result*, void* context) {
        auto* script = (s_script*)context;
        if (script->step >= 3) return false;
        const int16_t actors[] = {1, 3, 3};
        if (MOX::cur_ship != actors[script->step]) return false;
        *operation = {script->step == 1 ? COMBAT1::DONE : COMBAT1::RETREAT, MOX::cur_ship};
        ++script->step;
        return true;
    }
    bool Take_Done(COMBAT1::s_operation* operation, COMBAT1::e_result*, void*) {
        *operation = {COMBAT1::DONE, MOX::cur_ship};
        return true;
    }
    void Complete_Operation(COMBAT1::e_result result, void*) {
        DOCTEST_CHECK(result == COMBAT1::APPLIED);
    }
    struct s_read {
        rpc::s_view view;
        rpc::s_options options;
    };
    void Receive_Read(const COMBAT::s_event* event, void* context) noexcept {
        if (event->kind != COMBAT::SERVICE_READS || !event->safe) return;
        auto* read = (s_read*)context;
        DOCTEST_CHECK(rpc::Read_Tactical_View(&read->view));
        DOCTEST_CHECK(rpc::Read_Options(&read->options));
    }
} // namespace

DOCTEST_TEST_CASE("withdrawal command schedules and native next pass actually escapes in each initiative mode") {
    for (int mode : {0, 1, 2}) {
        s_withdrawal fixture;
        Initialize_Withdrawal(&fixture);
        COMBAT1::ship_initiative = mode;
        MOX::combat_init = mode != 0;
        Retire_Other_Own();
        Begin_Withdrawal();
        DOCTEST_CHECK(fixture.outcomes.empty());
        const auto seed = game_random::Get_Random_Seed();
        COMBAT1::Do_Combat_Turn(0, 0);
        DOCTEST_CHECK(MOX::combat_data[1].unit_status == 5);
        DOCTEST_CHECK(MOX::combat_data[1].is_retreating == 2);
        DOCTEST_REQUIRE(fixture.outcomes.size() == 1);
        DOCTEST_CHECK(fixture.outcomes[0].retreat_outcome == COMBAT::ESCAPED);
        DOCTEST_CHECK(COMBAT1::Check_For_Winner() == -1);
        DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
        DOCTEST_CHECK_FALSE(COMBAT1::Combat_Ship_Participates_In_Current_Pass(1));
        Shutdown_Withdrawal(&fixture);
    }
}

DOCTEST_TEST_CASE("settled no-actor grid refresh rebuilds occupancy without actor-dependent legal moves") {
    s_withdrawal fixture;
    Initialize_Withdrawal(&fixture);
    const auto actor = MOX::combat_data[1];
    const auto seed = game_random::Get_Random_Seed();
    for (int16_t index : {int16_t(-1), MOX::combat_total_ship_count}) {
        MOX::cur_ship = index;
        COMBAT1::Assign_Combat_Grids(index);
        DOCTEST_CHECK(MOX::combat_grid[actor.x][actor.y] == 1);
        DOCTEST_CHECK(std::memcmp(&actor, &MOX::combat_data[1], sizeof(actor)) == 0);
        for (int x = 0; x < MOX::combat_grid_width; ++x)
            for (int y = 0; y < MOX::combat_grid_height; ++y)
                DOCTEST_CHECK(MOX::combat_grid[x][y] != 2000);
    }
    DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
    Shutdown_Withdrawal(&fixture);
}

DOCTEST_TEST_CASE("retreat advances to the remaining eligible native actor in classic and initiative orders") {
    for (int mode : {0, 1, 2}) {
        s_withdrawal fixture;
        Initialize_Withdrawal(&fixture);
        COMBAT1::ship_initiative = mode;
        MOX::combat_init = mode != 0;
        MOX::ship_turn_order[0] = 1;
        MOX::ship_turn_order[1] = 3;
        MOX::ship_turn_order[2] = -10;
        COMBAT1::Reset_Strict_Combat_Sequence();
        const COMBAT1::s_operation retreat{COMBAT1::RETREAT, 1};
        DOCTEST_REQUIRE(rpc::Validate(&retreat) == COMBAT1::APPLIED);
        int16_t next = 0;
        COMBAT1::Apply_Tactical_Operation(&retreat, 0, &next);
        DOCTEST_CHECK(next == 0);
        DOCTEST_CHECK(MOX::cur_ship == 3);
        DOCTEST_CHECK(MOX::ship_turn_order[MOX::cur_ptr] == 3);
        DOCTEST_CHECK(MOX::combat_data[1].is_retreating == 1);
        COMBAT::Open_Control_Decision(3);
        const COMBAT1::s_operation done{COMBAT1::DONE, 3};
        DOCTEST_CHECK(rpc::Validate(&done) == COMBAT1::APPLIED);
        Shutdown_Withdrawal(&fixture);
    }
}

DOCTEST_TEST_CASE("withdrawal eligibility reads native refusals without changing bytes or RNG") {
    s_withdrawal fixture;
    Initialize_Withdrawal(&fixture);
    const auto seed = game_random::Get_Random_Seed();
    const auto actor = MOX::combat_data[1], enemy = MOX::combat_data[2];
    DOCTEST_CHECK(COMBAT1::Check_Human_Retreat(1) == 0);
    DOCTEST_CHECK(std::memcmp(&actor, &MOX::combat_data[1], sizeof(actor)) == 0);
    DOCTEST_CHECK(std::memcmp(&enemy, &MOX::combat_data[2], sizeof(enemy)) == 0);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
    MOX::combat_data[1].current_speed = 0;
    DOCTEST_CHECK(COMBAT1::Check_Human_Retreat(1) == 79);
    MOX::combat_data[1].ship_idx = -1;
    MOX::combat_data[1].ftl_type = DRIVE_NO_DRIVE;
    DOCTEST_CHECK(COMBAT1::Check_Human_Retreat(1) == 59);
    CMBTFIRE::Remove_Stasis_Beam(2);
    struct_::Set_Bit_Field(MOX::combat_data[2].special_device_flags, SPECIAL_WARP_DISSIPATOR);
    DOCTEST_CHECK(COMBAT1::Check_Human_Retreat(1) == 58);
    struct_::Set_Bit_Field(MOX::combat_data[2].special_device_damage_flags, SPECIAL_WARP_DISSIPATOR);
    DOCTEST_CHECK(COMBAT1::Check_Human_Retreat(1) == 59);
    Shutdown_Withdrawal(&fixture);
}

DOCTEST_TEST_CASE("pending retreat is deferred by stasis and nonparticipating Time Warp passes") {
    s_withdrawal fixture;
    Initialize_Withdrawal(&fixture);
    Retire_Other_Own();
    Begin_Withdrawal();
    MOX::combat_data[1].stasis_source_idx = 2;
    COMBAT1::Do_Combat_Turn(0, 0);
    DOCTEST_CHECK(MOX::combat_data[1].is_retreating == 1);
    CMBTFIRE::Remove_Stasis_Beam(1);
    COMBAT1::Do_Combat_Turn(0, 1);
    DOCTEST_CHECK(MOX::combat_data[1].is_retreating == 1);
    struct_::Set_Bit_Field(MOX::combat_data[1].special_device_flags, SPECIAL_TIME_WARP_FACILITATOR);
    COMBAT1::Do_Combat_Turn(0, 1);
    DOCTEST_CHECK(MOX::combat_data[1].is_retreating == 2);
    DOCTEST_REQUIRE(fixture.outcomes.size() == 1);
    DOCTEST_CHECK(fixture.outcomes[0].retreat_outcome == COMBAT::ESCAPED);
    Shutdown_Withdrawal(&fixture);
}

DOCTEST_TEST_CASE("native destruction capture validation and closure have distinct withdrawal outcomes") {
    for (int route : {0, 1, 2, 3}) {
        DOCTEST_CAPTURE(route);
        s_withdrawal fixture;
        Initialize_Withdrawal(&fixture);
        Begin_Withdrawal();
        if (route == 0) CMBTFIRE::Destroy_Ship(1, 260, 200, 1);
        if (route == 1) CMBTFIR2::Capture_Ship(1, 260, 200);
        if (route == 2) {
            MOX::combat_data[1].drive_current_hits = 50;
            DOCTEST_CHECK_FALSE(COMBAT1::Validate_Retreating_Ship(1));
        }
        if (route == 3) COMBAT::End_Pending_Retreats();
        DOCTEST_REQUIRE(fixture.outcomes.size() == 1);
        const COMBAT::e_retreat_outcome expected[] = {COMBAT::DESTROYED, COMBAT::CAPTURED, COMBAT::CANCELLED, COMBAT::BATTLE_ENDED};
        DOCTEST_CHECK(fixture.outcomes[0].retreat_outcome == expected[route]);
        DOCTEST_CHECK(MOX::combat_data[1].is_retreating != 2);
        Shutdown_Withdrawal(&fixture);
    }
}

DOCTEST_TEST_CASE("received and local native retreat preserve command state and RNG") {
    s_withdrawal fixture;
    Initialize_Withdrawal(&fixture);
    Begin_Withdrawal();
    const auto expected = MOX::combat_data[1];
    const auto seed = game_random::Get_Random_Seed();
    TEST_TACTICAL_TURN::Reset();
    TEST_TACTICAL_TURN::Withdrawal();
    Begin_Withdrawal(1, true);
    DOCTEST_CHECK(std::memcmp(&expected, &MOX::combat_data[1], sizeof(expected)) == 0);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
    Shutdown_Withdrawal(&fixture);
}

DOCTEST_TEST_CASE("withdrawal production round owner consumes explicit remaining actions to last ship escape") {
    s_withdrawal fixture;
    Initialize_Withdrawal(&fixture);
    s_script script;
    DOCTEST_REQUIRE(COMBAT::Attach_Control(Take_Script, Complete_Operation, &script));
    const auto winner = COMBINIT::Run_Tactical_Combat_Rounds(0, 1, 0);
    COMBAT::Detach_Control(&script);
    DOCTEST_CHECK(winner == -1);
    DOCTEST_CHECK(script.step == 3);
    DOCTEST_CHECK(MOX::combat_data[1].is_retreating == 2);
    DOCTEST_CHECK(MOX::combat_data[3].is_retreating == 2);
    DOCTEST_CHECK(fixture.outcomes.size() == 2);
    COMBAT::Retire_Battle();
    DOCTEST_CHECK(fixture.battle.hits.capacity() == 0);
    DOCTEST_CHECK(fixture.battle.boarding.raid_text.capacity() == std::string().capacity());
    Shutdown_Withdrawal(&fixture);
}

DOCTEST_TEST_CASE("native pass cancels withdrawal when speed or aggregate prevention changes") {
    for (bool dissipator : {false, true}) {
        s_withdrawal fixture;
        Initialize_Withdrawal(&fixture);
        Retire_Other_Own();
        Begin_Withdrawal();
        if (dissipator) {
            CMBTFIRE::Remove_Stasis_Beam(2);
            struct_::Set_Bit_Field(MOX::combat_data[2].special_device_flags, SPECIAL_WARP_DISSIPATOR);
        } else
            MOX::combat_data[1].drive_current_hits = 50;
        DOCTEST_REQUIRE(COMBAT::Attach_Control(Take_Done, Complete_Operation, &fixture));
        COMBAT1::Do_Combat_Turn(0, 0);
        DOCTEST_CHECK(MOX::combat_data[1].is_retreating == 0);
        DOCTEST_CHECK(MOX::combat_data[1].unit_status == 0);
        DOCTEST_REQUIRE(fixture.outcomes.size() == 1);
        DOCTEST_CHECK(fixture.outcomes[0].retreat_outcome == COMBAT::CANCELLED);
        Shutdown_Withdrawal(&fixture);
    }
}

DOCTEST_TEST_CASE("retreat options preserve native speed configuration and actor authority") {
    s_withdrawal fixture;
    Initialize_Withdrawal(&fixture);
    auto& actor = MOX::combat_data[1];
    const COMBAT1::s_operation retreat{COMBAT1::RETREAT, 1};
    for (bool ics : {false, true}) {
        const auto previous = COMBAT1::interactive_combat_stats;
        COMBAT1::interactive_combat_stats = ics;
        actor.black_hole_source_idx = 2;
        COMBAT1::Recalculate_Combat_Ship(1);
        DOCTEST_CHECK(rpc::Validate(&retreat) == COMBAT1::ILLEGAL);
        actor.black_hole_source_idx = COMBAT_SHIP_NONE;
        actor.ftl_type = DRIVE_NO_DRIVE;
        MOX::player[0].traits[TRAIT_TRANS_DIMENSIONAL] = 1;
        COMBAT1::Recalculate_Combat_Ship(1);
        DOCTEST_CHECK(rpc::Validate(&retreat) == COMBAT1::APPLIED);
        MOX::player[0].traits[TRAIT_TRANS_DIMENSIONAL] = 0;
        COMBAT1::Recalculate_Combat_Ship(1);
        DOCTEST_CHECK(rpc::Validate(&retreat) == COMBAT1::ILLEGAL);
        actor.ftl_type = 1;
        actor.drive_current_hits = 100;
        COMBAT1::Recalculate_Combat_Ship(1);
        COMBAT1::interactive_combat_stats = previous;
    }
    for (int kind : {0, 1, 2, 3}) {
        actor.stasis_source_idx = kind == 0 ? 2 : COMBAT_SHIP_NONE;
        actor.is_captured = kind == 1;
        MOX::auto_combat = kind == 2;
        actor.owner = kind == 3 ? 1 : 0;
        DOCTEST_CHECK(rpc::Validate(&retreat) == COMBAT1::NO_CONTROL);
    }
    Shutdown_Withdrawal(&fixture);
}

DOCTEST_TEST_CASE("displayed native retreat rotates before actual escape unlike the fast route") {
    s_withdrawal fixture;
    Initialize_Withdrawal(&fixture);
    Begin_Withdrawal();
    TEST_TACTICAL_TURN::Withdrawal_Display();
    MOX::cur_ship = 1;
    MOX::combat_data[2].stasis_source_idx = COMBAT_SHIP_NONE;
    const auto seed = game_random::Get_Random_Seed();
    COMBAT1::Retreat_Ship(1);
    DOCTEST_CHECK(MOX::combat_data[1].facing_dir == 8);
    DOCTEST_CHECK(MOX::combat_data[1].is_retreating == 2);
    DOCTEST_REQUIRE(fixture.outcomes.size() == 1);
    DOCTEST_CHECK(fixture.outcomes[0].retreat_outcome == COMBAT::ESCAPED);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
    Shutdown_Withdrawal(&fixture);
}

DOCTEST_TEST_CASE("retreat rotation resolves actual incoming missiles and can destroy the withdrawing ship") {
    s_withdrawal fixture;
    Initialize_Withdrawal(&fixture);
    Begin_Withdrawal();
    TEST_TACTICAL_TURN::Withdrawal_Display();
    MOX::cur_ship = 1;
    MOX::combat_data[1].structure_max = 1;
    MOX::combat_data[1].armor_remaining = 0;
    MOX::combat_data[2].stasis_source_idx = COMBAT_SHIP_NONE;
    TEST_TACTICAL_TURN::Add_Missile(0);
    MOX::missile[0].x = 261;
    MOX::missile[0].y = 200;
    TECHDATA::weapons[WEAPON_NUCLEAR_MISSILE].min_damage = 100;
    TECHDATA::weapons[WEAPON_NUCLEAR_MISSILE].max_damage = 100;
    const auto seed = game_random::Get_Random_Seed();
    COMBAT1::Retreat_Ship(1);
    DOCTEST_CHECK(MOX::combat_data[1].is_retreating == 0);
    DOCTEST_CHECK(MOX::combat_data[1].unit_status == 5);
    DOCTEST_REQUIRE(fixture.outcomes.size() == 1);
    DOCTEST_CHECK(fixture.outcomes[0].retreat_outcome == COMBAT::DESTROYED);
    DOCTEST_CHECK(game_random::Get_Random_Seed() != seed);
    Shutdown_Withdrawal(&fixture);
}

DOCTEST_TEST_CASE("withdrawal reads expose pending state without changing native memory or RNG") {
    s_withdrawal fixture;
    Initialize_Withdrawal(&fixture);
    Begin_Withdrawal();
    s_read read;
    auto before = std::make_unique<s_combat_data[]>(MAX_COMBAT_SHIPS);
    std::copy_n(MOX::combat_data, MAX_COMBAT_SHIPS, before.get());
    const auto seed = game_random::Get_Random_Seed();
    const auto subscription = COMBAT::Subscribe(Receive_Read, &read);
    COMBAT::Service_Reads(true);
    COMBAT::Unsubscribe(subscription);
    DOCTEST_CHECK_FALSE(read.options.available);
    bool pending = false;
    for (const rpc::s_unit& row : read.view.units)
        if (row.slot == 1 && row.retreat_pending) pending = true;
    DOCTEST_CHECK(pending);
    DOCTEST_CHECK(std::memcmp(before.get(), MOX::combat_data, sizeof(s_combat_data) * MAX_COMBAT_SHIPS) == 0);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
    Shutdown_Withdrawal(&fixture);
}

DOCTEST_TEST_CASE("received retreat packet is applied by the real remote tactical input owner") {
    s_withdrawal fixture;
    Initialize_Withdrawal(&fixture);
    CMBTFIRE::Remove_Stasis_Beam(2);
    MOX::net_flag = 1;
    MOX::net_destination = 0;
    MOX::combat_done_waiting = 64; // Fixture supplies native barrier arrivals, not a real session.
    MOX::new_message = {};
    MOX::new_message.message_code = TACTICAL_NET_MSG_RETREAT;
    MOX::new_message.random_seed = 4567;
    MOX::tactical_net_choice = TACTICAL_NET_MSG_RETREAT;
    COMBAT1::Do_Combat_Turn(1, 0);
    DOCTEST_CHECK(MOX::combat_data[2].is_retreating == 1);
    DOCTEST_CHECK(MOX::combat_data[2].unit_status == 0);
    DOCTEST_CHECK(MOX::tactical_net_choice == 0);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == 4567);
    DOCTEST_CHECK(fixture.outcomes.empty()); // Foreign withdrawal is not an own outcome stream.
    MOX::net_flag = 0;
    Shutdown_Withdrawal(&fixture);
}

DOCTEST_TEST_CASE("local flee input schedules through the same native operation and later pass escapes") {
    s_withdrawal fixture;
    Initialize_Withdrawal(&fixture);
    Retire_Other_Own();
    key::Push_Key_Code('r', 1);
    COMBAT1::Do_Combat_Turn(0, 0);
    DOCTEST_CHECK(MOX::combat_data[1].is_retreating == 1);
    DOCTEST_CHECK(fixture.outcomes.empty());
    COMBAT1::Do_Combat_Turn(0, 0);
    DOCTEST_CHECK(MOX::combat_data[1].is_retreating == 2);
    DOCTEST_REQUIRE(fixture.outcomes.size() == 1);
    DOCTEST_CHECK(fixture.outcomes[0].retreat_outcome == COMBAT::ESCAPED);
    Shutdown_Withdrawal(&fixture);
}

DOCTEST_TEST_CASE("AI native withdrawal retains its additional commitment state and later escapes") {
    s_withdrawal fixture;
    Initialize_Withdrawal(&fixture);
    Retire_Other_Own();
    const auto allowed = MOX::ai_retreat_flag;
    const auto stay = CMBTAI::ships_without_ammo_stay;
    MOX::ai_retreat_flag = 1;
    CMBTAI::ships_without_ammo_stay = 0;
    MOX::player_mode[0] = -1;
    CMBTAI::Do_Auto_Ship_Turn();
    DOCTEST_CHECK(MOX::combat_data[1].is_retreating == 1);
    DOCTEST_CHECK(MOX::combat_data[1].field40_0x47 == -1);
    COMBAT1::Do_Combat_Turn(0, 0);
    DOCTEST_CHECK(MOX::combat_data[1].is_retreating == 2);
    DOCTEST_REQUIRE(fixture.outcomes.size() == 1);
    DOCTEST_CHECK(fixture.outcomes[0].retreat_outcome == COMBAT::ESCAPED);
    MOX::ai_retreat_flag = allowed;
    CMBTAI::ships_without_ammo_stay = stay;
    Shutdown_Withdrawal(&fixture);
}

DOCTEST_TEST_CASE("native turn limit ends a battle with deferred withdrawal still pending") {
    s_withdrawal fixture;
    Initialize_Withdrawal(&fixture);
    Begin_Withdrawal();
    MOX::combat_data[1].stasis_source_idx = 2;
    const auto limit = COMBAT1::max_combat_turns;
    COMBAT1::max_combat_turns = 0;
    MOX::combat_turn = 1;
    DOCTEST_REQUIRE(COMBAT::Attach_Control(Take_Done, Complete_Operation, &fixture));
    const auto winner = COMBINIT::Run_Tactical_Combat_Rounds(0, 1, 0);
    COMBAT1::max_combat_turns = limit;
    DOCTEST_CHECK(winner == -1);
    DOCTEST_CHECK(MOX::combat_data[1].is_retreating == 1);
    DOCTEST_CHECK(fixture.outcomes.empty());
    // Same native retirement boundary as Tactical_Combat; fixture owns the memory teardown.
    COMBAT::End_Pending_Retreats();
    COMBAT::Retire_Battle();
    DOCTEST_REQUIRE(fixture.outcomes.size() == 1);
    DOCTEST_CHECK(fixture.outcomes[0].retreat_outcome == COMBAT::BATTLE_ENDED);
    Shutdown_Withdrawal(&fixture);
}

DOCTEST_TEST_CASE("repeated withdrawal contexts release fixture arenas and native report storage") {
    for (int iteration = 0; iteration < 4; ++iteration) {
        {
            s_withdrawal fixture;
            Initialize_Withdrawal(&fixture);
            Retire_Other_Own();
            Begin_Withdrawal();
            COMBAT1::Do_Combat_Turn(0, 0);
            DOCTEST_REQUIRE(fixture.outcomes.size() == 1);
            COMBAT::Retire_Battle();
            DOCTEST_CHECK(fixture.battle.hits.capacity() == 0);
            DOCTEST_CHECK(fixture.battle.boarding.raid_text.capacity() == std::string().capacity());
            Shutdown_Withdrawal(&fixture);
        }
        DOCTEST_CHECK(MOX::screen_seg == nullptr);
        DOCTEST_CHECK(MOX::global_cache_seg == nullptr);
        DOCTEST_CHECK(video::refresh_scan_list == nullptr);
        DOCTEST_CHECK(video::refresh_stencil_list == nullptr);
    }
}
