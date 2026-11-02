#pragma once

#include <string>
namespace rpc {
    struct s_game_store;
    struct s_map_observation;
} // namespace rpc
namespace test_native_ships {
    void Seed();
    void Initialize(rpc::s_game_store* store, rpc::s_map_observation* map);
    bool Command(const std::string& command);
    void Shutdown();
} // namespace test_native_ships
