#include "turn_fixture.h"
#include "game/tactical_view.h"
#include "rpc/tactical_rpc.h"

#include <iostream>
#include <memory>

int main() {
    TEST_TACTICAL_TURN::Reset();
    rpc::s_service service;
    rpc::s_service_config config;
    config.port = 0;
    config.key = "rpc-test-key-not-a-real-secret";
    std::string diagnostic;
    if (!rpc::Start_Service(&service, &config, &diagnostic)) return 1;
    rpc::s_tactical_forwarder forwarder;
    forwarder.reader = {tactical_view::Local_Observer, tactical_view::Read_View, tactical_view::Read_Unit};
    forwarder.options = tactical_control::Read_Options;
    forwarder.authorize = tactical_control::Authorize;
    forwarder.missile = tactical_view::Read_Missile;
    forwarder.missiles = tactical_view::Read_Missiles;
    forwarder.authorize_missile = tactical_view::Missile_Disclosed;
    if (!rpc::Subscribe_Tactical_Events(&forwarder, &service)) return 1;
    std::cout << rpc::Service_Port(&service) << std::endl;
    std::unique_ptr<tactical_events::s_battle_scope> battle;
    rpc::s_operation_request delivery;
    std::string command;
    while (std::getline(std::cin, command) && command != "stop") {
        if (command == "open") {
            if (battle) {
                tactical_events::Retire_Battle();
                battle->Close(0);
            }
            battle.reset();
            TEST_TACTICAL_TURN::Reset();
            battle = std::make_unique<tactical_events::s_battle_scope>();
            battle->Open(0);
            tactical_events::Select_Unit(1);
            tactical_control::Open_Decision(1);
        } else if (command == "poll") {
            if (fields::input_delay > 0) fields::Get_Input();
            if (battle && MOX::cur_ship > 0 && fields::input_delay <= 0)
                tactical_control::Open_Decision(MOX::cur_ship);
            else
                tactical_control::Close_Decision();
            tactical_events::Service_Reads(true);
        } else if (command == "boarding" || command == "boarding-report") {
            TEST_TACTICAL_TURN::Boarding(command == "boarding-report");
        } else if (command == "enemy-raid") {
            MOX::cur_ship = 2;
            MOX::combat_data[2].marine_count = 12;
            MOX::combat_data[2].crew_quality = 1;
            COMBAT1::Apply_Received_Boarding_Action(2, 1, 3, 1);
        } else if (command == "consume" || command == "consume-wait") {
            int16_t next = 0;
            const auto deadline = SDL_GetTicks() + 2000;
            while (!COMBAT1::Consume_Tactical_Operation(0, &next) && command == "consume-wait" && SDL_GetTicks() < deadline) {
                tactical_events::Service_Reads(true);
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
            tactical_events::Service_Reads();
        } else if (command == "local") {
            const tactical_control::s_operation operation{tactical_control::MOVE, MOX::cur_ship, -1, 14, 10};
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
            tactical_control::Invalidate();
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
            for (size_t i = 0; i < tactical_events::MAX_HIT_RECORDS + 10; ++i) tactical_view::Record_Unit_Hit(2, 1);
        } else if (command == "missile-reuse") {
            TEST_TACTICAL_TURN::Add_Missile(0);
        } else if (command == "missile-remove") {
            CMBTMIS::Kill_Missile(0);
        } else if (command == "local-bank") {
            COMBAT1::Cycle_Weapon_Bank(0);
        } else if (command == "native-fire") {
            const tactical_control::s_operation operation{tactical_control::FIRE, MOX::cur_ship, 2};
            TEST_TACTICAL_TURN::Apply(&operation);
        } else if (command == "initiative") {
            MOX::combat_init = 1;
            tactical_control::Invalidate();
        } else if (command == "remote") {
            MOX::net_flag = 1;
            MOX::combat_data[MOX::cur_ship].owner = 1;
        } else if (command == "ai") {
            MOX::auto_combat = 1;
        } else if (command == "scan") {
            MOX::skan_flag = 1;
        } else if (command == "hide-target") {
            tactical_events::Retire_Unit(2);
            MOX::combat_data[2].special_status_flag = 4;
        } else if (command == "reuse-target") {
            tactical_events::Retire_Unit(2);
            MOX::combat_data[2].unit_status = 0;
            strlcpy(MOX::combat_data[2].name, "Replacement", sizeof(MOX::combat_data[2].name));
        } else if (command == "idle") {
            for (int i = 0; i < 100; ++i) {
                tactical_control::Open_Decision(MOX::cur_ship);
                tactical_events::Service_Reads(true);
            }
        } else if (command == "attention") {
            tactical_events::s_local_attention attention;
        } else if (command == "revoke") {
            tactical_events::Revoke();
        } else if (command == "close") {
            tactical_events::Retire_Battle();
            if (battle) battle->Close(0);
            battle.reset();
        }
        std::cout << "done" << std::endl;
    }
    battle.reset();
    rpc::Unsubscribe_Tactical_Events(&forwarder);
    rpc::Stop_Service(&service);
    TEST_TACTICAL_TURN::Release_Boarding();
}
