#include <iostream>
#include <limits>
#include <string>

#include "rpc/rpc_service.h"

int main() {
    rpc::s_service service;
    rpc::s_service_config config;
    config.port = 0;
    config.key = "rpc-test-key-not-a-real-secret";
    rpc::s_bridge bridge;
    config.bridge = &bridge;
    std::string diagnostic;
    if (!rpc::Start_Service(&service, &config, &diagnostic)) {
        std::cerr << diagnostic << '\n';
        return 1;
    }
    std::cout << rpc::Service_Port(&service) << std::endl;
    std::string command;
    while (std::getline(std::cin, command) && !command.empty() && command != "quit") {
        if (command == "revoke") {
            rpc::Revoke_Bridge(&bridge);
        } else if (command == "publish" || command == "publish-large" || command == "overflow") {
            // Local test fixture for the first connection, not a wire method or native game data.
            rpc::s_request request;
            request.id_json = "99";
            request.method = "fixture";
            request.params_json = "{}";
            if (rpc::Submit_Work(&bridge, 1, 1, &request, std::numeric_limits<uint64_t>::max()) == 0) {
                rpc::s_work work;
                if (rpc::Take_Work(&bridge, 0, &work)) {
                    const size_t size = command == "publish-large" ? 524288 : 32768;
                    const std::string result = command != "overflow" ? rpc::Result_Response(&request, "{\"fixture\":" + rpc::Json_String(std::string(size, 'x')) + "}") : std::string(1048577, 'x');
                    rpc::Complete_Work(&bridge, &work, &result);
                }
            }
        }
    }
    rpc::Stop_Service(&service);
    return 0;
}
