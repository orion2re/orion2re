#include "pch.h"
#include "rpc/rpc_service.h"
#include "rpc/rpc_tactical.h"
#include "game/combat.h"

#include <iostream>
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
    COMBAT::s_battle_state battle;
    while (std::getline(std::cin, command) && command != "stop") {
        if (command == "open") {
            COMBAT::Release_Battle(&battle);
            COMBAT::Open_Battle(&battle, 0);
        } else if (command == "close") {
            COMBAT::Close_Battle(&battle, 0);
            COMBAT::Release_Battle(&battle);
        } else if (command == "battle" || command == "hidden" || command == "switch") {
            const int16_t viewer = command == "hidden" ? -1 : (command == "switch" ? 1 : 0);
            COMBAT::s_battle_state completed;
            COMBAT::Open_Battle(&completed, viewer);
            COMBAT::Close_Battle(&completed, viewer);
            COMBAT::Release_Battle(&completed);
        } else if (command == "revoke") {
            COMBAT::Revoke();
        }
        std::cout << "done" << std::endl;
    }
    COMBAT::Release_Battle(&battle);
    rpc::Unsubscribe_Tactical_Events(&forwarder);
    rpc::Stop_Service(&service);
    return 0;
}
