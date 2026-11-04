#include "rpc/rpc_service.h"

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
    std::cout << rpc::Service_Port(&service) << std::endl;
    std::string command;
    std::getline(std::cin, command);
    rpc::Stop_Service(&service);
    return 0;
}
