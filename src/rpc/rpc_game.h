#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include "rpc_bridge.h"

namespace rpc {
    enum e_exploration_knowledge {
        EXPLORATION_UNKNOWN,
        EXPLORATION_CURRENT,
        EXPLORATION_EMPTY,
        EXPLORATION_UNAUDITED,
    };

    struct s_system_observation {
        int16_t x = 0;
        int16_t y = 0;
        bool visited = false;
        bool name_available = false;
        std::string name;
        bool name_known = false; // Also permits independently audited contact/Lore names.
        int32_t spectral_class = 0;
        int32_t size = 0;
        bool explored = false;
        int32_t owner_slot = -1;
        std::string owner_id;
        e_exploration_knowledge wormhole_available = EXPLORATION_UNKNOWN;
        int32_t wormhole_system = -1;
        std::string wormhole_id;
        bool in_extended_range = false;
        std::vector<std::string> planet_ids;
    };

    struct s_nebula_observation {
        int16_t x = 0;
        int16_t y = 0;
        int32_t variant = 0;
    };

    struct s_planet_observation {
        bool visible = false;
        int32_t system = -1;
        int32_t orbit = 0;
        int32_t kind = 0; // Native asteroid/gas-giant/planet classification 1..3.
        std::string name;
        int32_t size = 0;
        int32_t gravity = 0;
        int32_t climate = 0;
        int32_t minerals = 0;
        int32_t special = 0;
        int32_t food2 = 0;
        int32_t industry = 0;
        int32_t gravity_penalty = 0;
        bool max_population_available = false;
        int32_t max_population = 0;
        std::string system_id;
        std::string colony_id;
    };

    enum e_ship_knowledge {
        SHIP_UNAUDITED,
        SHIP_CURRENT,
        SHIP_KNOWN_EMPTY,
        SHIP_UNKNOWN,
        SHIP_CONTEXT_REQUIRED,
    };

    struct s_ship_observation {
        int16_t x = 0;
        int16_t y = 0;
        bool owned = false;
        // Own name only; foreign names are not read.
        std::string name;
        // Private authorized-parent binding, discarded before retention.
        int32_t owner_slot = -1;
        // Resolved only from the same capture's public player handles.
        std::string owner_id;
        e_ship_knowledge destination_available = SHIP_UNAUDITED;
        int32_t destination_system = -1; // Private same-capture binding, discarded before retention.
        std::string destination_id;
        e_ship_knowledge eta_available = SHIP_UNAUDITED;
        int32_t eta = 0;
        int32_t slot = -1; // Private same-capture leader binding, discarded before retention.
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
        bool at_war_available = false;
        bool at_war = false;
        bool research_treaty_available = false;
        bool research_treaty = false;
        bool trade_treaty_available = false;
        bool trade_treaty = false;
    };

    struct s_relation_report_entry {
        int32_t slot = -1;
        std::string player_id;
        std::string name;
        bool alliance = false;
    };

    struct s_relation_observation {
        int32_t slot = -1;
        std::string player_id;
        bool available = false;
        std::string relationship;
        int32_t relationship_offset = 0;
        std::string treaty;
        bool ignoring = false;
        bool research = false;
        int32_t research_level = 0;
        bool trade = false;
        int32_t trade_level = 0;
        int32_t tribute_percent = 0;
        bool giving = false;
        bool assignment_available = false;
        int32_t spies = 0;
        int32_t mission = -1;
        bool incoming_available = false;
        int32_t incoming = 0;
        bool report_available = false;
        std::string personality;
        std::string objective;
        bool objective_empty = false;
        std::vector<s_relation_report_entry> report;
    };

    struct s_intelligence_observation {
        bool available = false;
        int32_t agents = 0;
        bool bonuses_available = false;
        int32_t spy_bonus = 0;
        int32_t agent_bonus = 0;
    };

    struct s_colony_observation {
        int32_t owner_slot = -1; // Private authorization binding, discarded before retention.
        int32_t system = -1;     // Private same-capture parent binding, discarded before retention.
        int32_t orbit = 0;       // Public one-based orbital position.
        int32_t population = 0;  // Cached resident population units, not growth or inbound settlers.
        std::string system_id;
        std::string owner_id;
        int32_t infantry = 0;
        int32_t armor = 0;
        std::string planet_id;
    };

    struct s_technology_observation {
        bool application = false;
        std::string name;
        bool cost_available = false;
        int32_t cost = 0;
    };

    struct s_research_option {
        int32_t field = -1;       // Indices into this capture's authorized technologies, not native IDs.
        int32_t application = -1; // -1 is the native no-application choice.
        int32_t remaining = 0;
    };

    struct s_research_observation {
        std::string mode = "no-selection";
        int32_t project = -1;
        int32_t application = -1;
        std::vector<s_technology_observation> technologies;
        std::vector<s_research_option> options;
    };

    struct s_leader_skill {
        std::string name;
        std::string value;
    };

    struct s_leader_observation {
        int32_t slot = -1; // Authorized native identity only; never serialized or used to order rows.
        bool visible = false;
        std::string disclosure;
        std::string role;
        std::string name;
        std::string title;
        std::string rank;
        bool xp_available = false;
        int32_t xp = 0;
        std::vector<s_leader_skill> skills;
        std::string status;
        e_exploration_knowledge assignment_available = EXPLORATION_UNKNOWN;
        std::string assignment_kind;
        std::string assignment_name;
        int32_t assignment_slot = -1; // Private same-capture system/ship binding.
        std::string assignment_id;
        e_exploration_knowledge eta_available = EXPLORATION_UNKNOWN;
        int32_t eta = 0;
        bool hire_cost_available = false;
        int32_t hire_cost = 0;
        int32_t upkeep = 0;
        bool offer_turns_available = false;
        int32_t offer_turns = 0;
        bool benefits_available = false;
        std::string benefits;
        bool partial = false;
        bool skills_available = false;
        bool bonus_available = false;
        int32_t bonus = 0;
    };

    struct s_history_curve {
        int32_t slot = -1;
        std::string player_id;
        std::vector<int32_t> y;
    };

    struct s_history_chart {
        uint8_t series = 0;
        int32_t top_scale = 25;
        std::vector<s_history_curve> curves;
    };

    struct s_history_observation {
        int32_t x_step = 1;
        bool smoothing = false;
        std::vector<std::string> labels;
        std::vector<s_history_chart> charts;
    };

    struct s_message_observation {
        bool text_available = false;
        std::string text;
        bool serious = false;
        int32_t colony_system = -1;
        int32_t colony_orbit = 0;
        std::string colony_id;
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
        bool colonies_captured = false;
        std::vector<s_colony_observation> colonies;
        uint64_t context_epoch = 0; // Private authorization lease, never encoded or compared as data.
        bool planets_captured = false;
        std::vector<s_planet_observation> planets;
        bool exploration_sensitive = false;
        bool research_captured = false;
        s_research_observation research;
        bool leaders_captured = false;
        std::vector<s_leader_observation> leaders;
        bool leader_presentation = false;
        bool leaders_only = false;
        bool relations_captured = false;
        std::vector<s_relation_observation> relations;
        s_intelligence_observation intelligence;
        bool relations_presentation = false;
        bool history_captured = false;
        s_history_observation history;
        bool messages_captured = false;
        std::vector<s_message_observation> messages;
    };

    struct s_map_snapshot {
        s_map_observation map;
        std::string id;
        uint64_t revision = 0;
        std::vector<std::string> ship_ids; // Read handles scoped to this immutable capture.
        std::vector<std::string> player_ids;
        std::vector<std::string> colony_ids; // Read-only observation handles, not action capabilities.
        std::vector<std::string> planet_ids;
        std::vector<std::string> technology_ids;
        std::vector<std::string> research_option_ids;
        std::vector<std::string> leader_ids;
        std::vector<std::string> message_ids;
    };

    struct s_planet_handle {
        std::string system_id;
        int32_t orbit;
        std::string id;
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
        uint64_t baseline_ticket = 0;
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
        std::vector<s_planet_handle> planet_handles;
        std::deque<s_map_snapshot> snapshots;
        std::vector<s_map_cursor> cursors;
        std::vector<s_state_subscription> subscriptions;
        std::vector<s_turn_observer> turn_observers;
        bool context_active = false;
    };

    bool Initialize_Game_Store(s_game_store* store, s_bridge* bridge, t_new_id new_id = nullptr, void* context = nullptr);
    void Shutdown_Game_Store(s_game_store* store);
    void Begin_Game_Reset(s_game_store* store);
    bool Commit_Game_Reset(s_game_store* store);
    void End_Game(s_game_store* store);
    void Revoke_Game_View(s_game_store* store);
    void Revoke_Game_Context(s_game_store* store);
    void Pause_Game_Queries(s_game_store* store);
    void Expire_Game_Messages(s_game_store* store);
    void Resume_Game_Queries(s_game_store* store);
    bool Capture_Map(s_game_store* store, const s_map_observation* observation);
    std::string Game_Query(s_game_store* store, const s_request* request, uint64_t* context_epoch = nullptr);
    void Pump_Game_Queries(s_game_store* store);
    bool Needs_Game_Capture(s_game_store* store);
} // namespace rpc
