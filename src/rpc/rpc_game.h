#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include "rpc_bridge.h"

namespace rpc {
    struct s_system_observation {
        int16_t x = 0;
        int16_t y = 0;
        bool visited = false;
        bool name_available = false;
        std::string name;
    };

    struct s_nebula_observation {
        int16_t x = 0;
        int16_t y = 0;
    };

    struct s_ship_observation {
        int16_t x = 0;
        int16_t y = 0;
        bool owned = false;
        std::string name; // Own name only; foreign names are not read.
    };

    struct s_player_hud {
        int32_t income = 0;
        int32_t food_surplus = 0;
        int32_t freighters = 0;
        int32_t freighters_available = 0;
        int32_t command_points = 0;
        int32_t command_points_available = 0;
    };

    struct s_player_observation {
        int32_t slot = -1; // Private identity; assigned a public handle only after authorization.
        bool visible = false;
        bool local = false;
        bool contacted = false;
        bool eliminated = false;
        uint8_t color = 0;
        std::string name;
        int32_t treasury = 0;
        s_player_hud hud;
    };

    struct s_map_observation {
        int32_t viewer = -1; // Private native identity, never encoded.
        int32_t turn = 0;
        std::vector<s_system_observation> systems;
        std::vector<s_nebula_observation> nebulas;
        bool ships_captured = false;
        std::vector<s_ship_observation> ships;
        bool players_captured = false;
        std::vector<s_player_observation> players;
    };

    struct s_map_snapshot {
        s_map_observation map;
        std::string id;
        uint64_t revision = 0;
        std::vector<std::string> ship_ids; // Read handles scoped to this immutable capture.
        std::vector<std::string> player_ids;
    };

    struct s_player_handle {
        int32_t slot;
        std::string id;
    };

    struct s_map_cursor {
        std::string id;
        std::string snapshot;
        std::string method;
        size_t offset;
        size_t limit;
    };

    using t_new_id = bool (*)(std::string* id, void* context);

    struct s_state_subscription {
        uint64_t client = 0;
        uint64_t epoch = 0;
        uint64_t cursor = 0;
        uint64_t revision = 0;
        std::string id;
        std::vector<std::string> topics;
        std::string projection;
        bool state_changes = false;
    };

    struct s_turn_observer {
        s_client_identity identity;
        int32_t turn;
    };

    // Game-thread-owned. Only copied requests/publications cross the bridge.
    struct s_game_store {
        s_bridge* bridge = nullptr;
        s_protocol protocol;
        t_new_id new_id = nullptr;
        void* id_context = nullptr;
        bool loaded = false;
        bool unavailable = false;
        bool capture_requested = false;
        uint32_t busy_depth = 0;
        int32_t viewer = -1;
        uint64_t revision = 0;
        std::string game_id;
        std::string perspective_id;
        std::vector<std::string> system_ids;
        std::vector<std::string> nebula_ids;
        std::vector<s_player_handle> player_handles;
        std::deque<s_map_snapshot> snapshots;
        std::vector<s_map_cursor> cursors;
        std::vector<s_state_subscription> subscriptions;
        std::vector<s_turn_observer> turn_observers;
    };

    bool Initialize_Game_Store(s_game_store* store, s_bridge* bridge, t_new_id new_id = nullptr, void* context = nullptr);
    void Shutdown_Game_Store(s_game_store* store);
    void Begin_Game_Reset(s_game_store* store);
    bool Commit_Game_Reset(s_game_store* store);
    void End_Game(s_game_store* store);
    void Revoke_Game_View(s_game_store* store);
    void Pause_Game_Queries(s_game_store* store);
    void Resume_Game_Queries(s_game_store* store);
    bool Capture_Map(s_game_store* store, const s_map_observation* observation);
    std::string Game_Query(s_game_store* store, const s_request* request);
    void Pump_Game_Queries(s_game_store* store);
    bool Needs_Game_Capture(s_game_store* store);
} // namespace rpc
