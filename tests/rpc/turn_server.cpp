#include "turn_fixture.h"
#include "game/combat.h"
#include "rpc/rpc_tactical.h"

#include <iostream>

int main() {
    TEST_TACTICAL_TURN::Reset();
    rpc::s_service service;
    rpc::s_service_config config;
    config.port = 0;
    config.key = "rpc-test-key-not-a-real-secret";
    std::string diagnostic;
    if (!rpc::Start_Service(&service, &config, &diagnostic)) return 1;
    rpc::s_tactical_forwarder forwarder;
    forwarder.reader = {COMBAT::Local_Observer, rpc::Read_Tactical_View, rpc::Read_Tactical_Unit};
    forwarder.options = rpc::Read_Options;
    forwarder.authorize = rpc::Authorize;
    forwarder.validate = rpc::Validate;
    forwarder.missile = rpc::Read_Tactical_Missile;
    forwarder.missiles = rpc::Read_Tactical_Missiles;
    forwarder.authorize_missile = COMBAT::Missile_Disclosed;
    if (!rpc::Subscribe_Tactical_Events(&forwarder, &service)) return 1;
    std::cout << rpc::Service_Port(&service) << std::endl;
    COMBAT::s_battle_state battle;
    rpc::s_operation_request delivery;
    std::string command;
    while (std::getline(std::cin, command) && command != "stop") {
        if (command == "open") {
            if (battle.initialized) {
                COMBAT::Retire_Battle();
                COMBAT::Close_Battle(&battle, 0);
            }
            COMBAT::Release_Battle(&battle);
            TEST_TACTICAL_TURN::Reset();
            COMBAT::Open_Battle(&battle, 0);
            COMBAT::Select_Unit(1);
            COMBAT::Open_Control_Decision(1);
        } else if (command == "poll") {
            if (fields::input_delay > 0) fields::Get_Input();
            if (battle.initialized && MOX::cur_ship > 0 && fields::input_delay <= 0)
                COMBAT::Open_Control_Decision(MOX::cur_ship);
            else
                COMBAT::Close_Control_Decision();
            COMBAT::Service_Reads(true);
        } else if (command == "boarding" || command == "boarding-report") {
            TEST_TACTICAL_TURN::Boarding(command == "boarding-report");
        } else if (command == "withdrawal") {
            TEST_TACTICAL_TURN::Withdrawal();
        } else if (command == "withdrawal-phase") {
            TEST_TACTICAL_TURN::Withdrawal_Display();
            MOX::super_fast_flag = 1;
            MOX::apply_damage_to_planet = 0;
            COMBAT1::rephase_cloak_next_turn = 0;
            struct_::Set_Bit_Field(MOX::combat_data[1].special_device_flags, SPECIAL_PHASING_CLOAK);
            MOX::combat_data[1].special_status_flag = 7;
            MOX::combat_data[1].special_status_timer = 2;
        } else if (command == "reuse-actor") {
            COMBINIT::Load_Combat_Ship(1, 1);
        } else if (command == "withdrawal-rounds") {
            // Real native rounds consume only explicit client actions; no fixture auto-done/retreat.
            const auto winner = COMBINIT::Run_Tactical_Combat_Rounds(0, 1, 0);
            if (winner != -1 || MOX::combat_data[1].is_retreating != 2 || MOX::combat_data[3].is_retreating != 2) return 2;
            COMBAT::End_Pending_Retreats();
            COMBAT::Retire_Battle();
            COMBAT::Close_Battle(&battle, 0);
            COMBAT::Release_Battle(&battle);
            fields::Deactivate_Auto_Function();
        } else if (command == "retreat-no-speed") {
            MOX::combat_data[1].drive_current_hits = 50;
            COMBAT1::Recalculate_Combat_Ship(1);
            COMBAT::Unit_Changed(1);
        } else if (command == "retreat-blocked") {
            CMBTFIRE::Remove_Stasis_Beam(2);
            struct_::Set_Bit_Field(MOX::combat_data[2].special_device_flags, SPECIAL_WARP_DISSIPATOR);
            COMBAT::Invalidate_Control();
        } else if (command == "retreat-destroy") {
            CMBTFIRE::Destroy_Ship(1, 260, 200, 1);
            COMBAT::Service_Reads(true);
        } else if (command == "retreat-capture") {
            CMBTFIR2::Capture_Ship(1, 260, 200);
            COMBAT::Service_Reads(true);
        } else if (command == "enemy-raid") {
            MOX::cur_ship = 2;
            MOX::combat_data[2].marine_count = 12;
            MOX::combat_data[2].crew_quality = 1;
            COMBAT1::Apply_Received_Boarding_Action(2, 1, 3, 1);
        } else if (command == "consume" || command == "consume-wait") {
            int16_t next = 0;
            const auto deadline = SDL_GetTicks() + 2000;
            while (!COMBAT1::Consume_Tactical_Operation(0, &next) && command == "consume-wait" && SDL_GetTicks() < deadline) {
                COMBAT::Service_Reads(true);
                SDL_Delay(1);
            }
        } else if (command == "begin-delivery") {
            // Synthetic delivery-race test only; normal turn tests use the native consumer.
            rpc::Take_Operation(&service, &delivery);
        } else if (command == "finish-delivery") {
            rpc::Complete_Operation(&service, &delivery);
            delivery = {};
        } else if (command == "last") {
            MOX::super_fast_flag = 0;
        } else if (command == "unsafe") {
            COMBAT::Service_Reads();
        } else if (command == "local") {
            const COMBAT1::s_operation operation{COMBAT1::MOVE, MOX::cur_ship, -1, 14, 10};
            TEST_TACTICAL_TURN::Apply(&operation);
        } else if (command == "missiles") {
            TEST_TACTICAL_TURN::Add_Missile(0);
            TEST_TACTICAL_TURN::Add_Missile(1, WEAPON_ANTI_MATTER_TORP);
            TEST_TACTICAL_TURN::Add_Missile(2, WEAPON_NUCLEAR_MISSILE, 0);
            MOX::missile[0].target_unit_idx = 3;
            MOX::missile[1].target_unit_idx = 3;
            static char rocket_name[] = "Anti-Missile Rockets";
            TECHDATA::weapons[WEAPON_ANTI_MISSILE_ROCKET].type = WEAPON_TYPE_SPECIAL;
            TECHDATA::weapons[WEAPON_ANTI_MISSILE_ROCKET].name_plural = rocket_name;
            MOX::combat_data[1].weapons[1] = {WEAPON_ANTI_MISSILE_ROCKET, 2, WEAPON_FIRING_ARC_ALL_SECTORS, 0, 5, -1, 2, 0};
            MOX::combat_data[1].weapons[2] = MOX::combat_data[1].weapons[0];
            CMBTSPEC::anti_missile_rocket_accuracy.base = 101;
            CMBTSPEC::anti_missile_rocket_accuracy.range_multiplier = 0;
            COMBAT::Invalidate_Control();
        } else if (command == "missile-move") {
            CMBTMIS::Move_Missile(0, 320, 200, 10);
        } else if (command == "high-missiles") {
            TEST_TACTICAL_TURN::Add_Missile(MAX_COMBAT_SHIPS);
            TEST_TACTICAL_TURN::Add_Missile(MAX_COMBAT_FLYERS - 1);
            TECHDATA::weapons[WEAPON_LASER_CANNON].min_damage = 100;
            TECHDATA::weapons[WEAPON_LASER_CANNON].max_damage = 100;
        } else if (command == "all-missiles") {
            for (int16_t i = 0; i < MAX_COMBAT_FLYERS; ++i) TEST_TACTICAL_TURN::Add_Missile(i);
        } else if (command == "history-overflow") {
            // Supplemental retention/delivery setup, not native firing evidence.
            for (size_t i = 0; i < COMBAT::MAX_HIT_RECORDS + 10; ++i) CMBTFIRE::Record_Unit_Hit(2, 1);
        } else if (command == "missile-reuse") {
            TEST_TACTICAL_TURN::Add_Missile(0);
        } else if (command == "missile-remove") {
            CMBTMIS::Kill_Missile(0);
        } else if (command == "local-bank") {
            COMBAT1::Cycle_Weapon_Bank(0);
        } else if (command == "native-fire") {
            const COMBAT1::s_operation operation{COMBAT1::FIRE, MOX::cur_ship, 2};
            TEST_TACTICAL_TURN::Apply(&operation);
        } else if (command == "initiative") {
            MOX::combat_init = 1;
            COMBAT::Invalidate_Control();
        } else if (command == "remote") {
            MOX::net_flag = 1;
            MOX::combat_data[MOX::cur_ship].owner = 1;
        } else if (command == "ai") {
            MOX::auto_combat = 1;
        } else if (command == "scan") {
            MOX::skan_flag = 1;
        } else if (command == "hide-target") {
            COMBAT::Retire_Unit(2);
            MOX::combat_data[2].special_status_flag = 4;
        } else if (command == "reuse-target") {
            COMBAT::Retire_Unit(2);
            MOX::combat_data[2].unit_status = 0;
            strlcpy(MOX::combat_data[2].name, "Replacement", sizeof(MOX::combat_data[2].name));
        } else if (command == "idle") {
            for (int i = 0; i < 100; ++i) {
                COMBAT::Open_Control_Decision(MOX::cur_ship);
                COMBAT::Service_Reads(true);
            }
        } else if (command == "attention") {
            const int16_t attention = COMBAT::Begin_Local_Attention();
            COMBAT::End_Local_Attention(attention);
        } else if (command == "revoke") {
            COMBAT::Revoke();
        } else if (command == "close") {
            COMBAT::End_Pending_Retreats();
            COMBAT::Retire_Battle();
            COMBAT::Close_Battle(&battle, 0);
            COMBAT::Release_Battle(&battle);
        }
        std::cout << "done" << std::endl;
    }
    COMBAT::Release_Battle(&battle);
    rpc::Unsubscribe_Tactical_Events(&forwarder);
    rpc::Stop_Service(&service);
    TEST_TACTICAL_TURN::Release_Boarding();
}
