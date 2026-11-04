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
            if (battle && MOX::cur_ship > 0) tactical_control::Open_Decision(MOX::cur_ship);
            tactical_events::Service_Reads(true);
        } else if (command == "consume") {
            int16_t next = 0;
            COMBAT1::Consume_Tactical_Operation(0, &next);
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
}
