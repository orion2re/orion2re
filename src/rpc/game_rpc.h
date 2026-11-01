#pragma once

#include "rpc/rpc_game.h"

namespace game_rpc {
    struct s_ship_association {
        int16_t node = -1;
        int16_t head = -1;
        int16_t slot = -1;
        int16_t owner = -1;
        int16_t location = -1;
        int8_t status = -1;
        uint64_t generation = 0;
        bool valid = true;
        rpc::s_ship_observation ship;
    };

    // Game-thread-owned transient associations; never used as public identities.
    struct s_fleet_eta_adapter {
        rpc::s_game_store* store = nullptr;
        uint64_t events = 0;
        uint64_t info = 0;
        uint64_t identity = 0;
        uint64_t destination = 0;
        bool owner_present = false;
        bool initialized = false;
        bool presenting = false;
        bool printing = false;
        bool failed = false;
        bool frame = false;
        bool frame_accepted = false;
        bool contextual_destination = false;
        bool pending_contextual_destination = false;
        int16_t route_node = -1;
        int16_t route_ship = -1;
        int16_t route_destination = -1;
        int16_t authorized_head = -1;
        int16_t pending_head = -1;
        std::vector<s_ship_association> associations;
        rpc::s_map_observation pending;
    };

    bool Initialize_Fleet_Eta_Adapter(s_fleet_eta_adapter* adapter, rpc::s_game_store* store);
    void Shutdown_Fleet_Eta_Adapter(s_fleet_eta_adapter* adapter);
    bool Subscribe_To_Game();
    void Unsubscribe_From_Game();
    bool Read_Map_Observation(rpc::s_map_observation* observation);
    // Only at SHIP_ICONS_REFRESHED, while the freshly built native links are valid.
    bool Read_Refreshed_Ship_Observation(rpc::s_map_observation* observation);
} // namespace game_rpc
