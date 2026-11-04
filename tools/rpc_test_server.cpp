#include "rpc/rpc_service.h"
#include "rpc/tactical_rpc.h"
#include "game/tactical_events.h"

#include <iostream>
#include <memory>
#include <string>

int main() {
    rpc::s_service service;
    rpc::s_service_config config;
    config.port = 0;
    config.key = "rpc-test-key-not-a-real-secret";
    std::string diagnostic;
    if (!rpc::Start_Service(&service, &config, &diagnostic)) {
        std::cerr << diagnostic << '\n';
        return 1;
    }
    rpc::s_tactical_forwarder forwarder;
    if (!rpc::Subscribe_Tactical_Events(&forwarder, &service)) {
        rpc::Stop_Service(&service);
        return 1;
    }
    std::cout << rpc::Service_Port(&service) << std::endl;
    std::string command;
    std::unique_ptr<tactical_events::s_battle_scope> battle;
    while (std::getline(std::cin, command) && command != "stop") {
        if (command == "open") {
            battle = std::make_unique<tactical_events::s_battle_scope>();
            battle->Open(0);
        } else if (command == "close") {
            if (battle != nullptr) battle->Close(0);
            battle.reset();
        } else if (command == "battle" || command == "hidden" || command == "switch") {
            const int16_t viewer = command == "hidden" ? -1 : (command == "switch" ? 1 : 0);
            tactical_events::s_battle_scope completed;
            completed.Open(viewer);
            completed.Close(viewer);
        } else if (command == "revoke") {
            tactical_events::Revoke();
        }
        std::cout << "done" << std::endl;
    }
    battle.reset();
    rpc::Unsubscribe_Tactical_Events(&forwarder);
    rpc::Stop_Service(&service);
    return 0;
}
