#pragma once

#include <cstdint>
#include "leader_read.h"

namespace game_events {
    enum e_event {
        THREAD_STARTED,
        THREAD_STOPPING,
        MENU_ENTERED,
        PERSPECTIVE_SELECTION_BEGIN,
        WORLD_REPLACE_BEGIN,
        WORLD_REPLACE_COMMITTED,
        WORLD_REPLACE_FAILED,
        WORLD_UPDATE_BEGIN,
        WORLD_UPDATE_COMMITTED,
        WORLD_UPDATE_FAILED,
        RESOLUTION_BEGIN,
        RESOLUTION_END,
        MAP_OBSERVATION_POINT,
        IDLE_OBSERVATION_POINT,
        SHIP_ICONS_REFRESHED,
        FLEET_BOX_INFO_CLEARED,
        FLEET_BOX_INFO_READY,
        SHIP_ICONS_REBUILD_BEGIN,
        FLEET_BOX_VIEW_ENTERED,
        FLEET_BOX_VIEW_INITIALIZED,
        FLEET_BOX_VIEW_LEAVING,
        MAIN_VIEW_INPUT_ENTERED,
        MAIN_VIEW_INPUT_LEAVING,
        MAIN_VIEW_INPUT_INTERRUPTED,
        MAIN_VIEW_DRAW_ENTERED,
        MAIN_VIEW_DRAW_LEAVING,
        FLEET_BOX_PRINT_ENTERED,
        FLEET_BOX_PRINT_LEAVING,
        SHIP_IDENTITIES_CHANGING,
        SHIP_IDENTITIES_RESETTING,
        MAIN_SCREEN_FRAME_ENTERED,
        MAIN_SCREEN_FRAME_LEAVING,
        SHIP_DESTINATION_LINE_DRAWN,
        LEADER_VIEW_CLEARED,
        LEADER_VIEW_READY,
    };

    struct s_fleet_box_node {
        int16_t node;
        int16_t ship;
    };

    // Native fleet-box presentation result, borrowed only during publication.
    // Only the cached-route branch; star is -1 when its name is not disclosed.
    struct s_fleet_box_info {
        int16_t ship;
        uint8_t turns_left;
        int16_t star;
        // Transient producer reference, not an identity across node rebuilds.
        int16_t node;
        int16_t box_head = -1;
        bool box_open = false;
        const s_fleet_box_node* members = nullptr;
        int16_t member_count = 0;
        const s_fleet_box_node* selected = nullptr;
        int16_t selected_count = 0;
    };

    using t_callback = void (*)(e_event event, void* context);
    // Actual native line endpoints' semantic references, not name knowledge.
    struct s_ship_destination_line {
        int16_t node;
        int16_t ship;
        int16_t destination;
        bool box_selected;
    };
    using t_ship_destination_callback = void (*)(const s_ship_destination_line* line, void* context);
    using t_fleet_box_callback = void (*)(const s_fleet_box_info* info, void* context);
    using t_ship_identity_callback = void (*)(int16_t ship, void* context);
    // Synchronous observation only. Register while quiescent or on the publishing
    // thread; callbacks must not throw or recursively mutate the game.
    uint64_t Subscribe(t_callback callback, void* context);
    uint64_t Subscribe_Fleet_Box_Info(t_fleet_box_callback callback, void* context);
    uint64_t Subscribe_Ship_Identity(t_ship_identity_callback callback, void* context);
    uint64_t Subscribe_Ship_Destination(t_ship_destination_callback callback, void* context);
    void Unsubscribe(uint64_t subscription);
    void Publish(e_event event);
    void Publish_Fleet_Box_Info(const s_fleet_box_info* info);
    void Publish_Ship_Identity_Changing(int16_t ship);
    void Publish_Ship_Destination(const s_ship_destination_line* line);

    struct s_leader_view {
        const leader_read::s_leader* leaders;
        int16_t count;
    };
    // Callback-local payload only; no live UI pointers may escape publication.
    const s_leader_view* Published_Leader_View();
    void Publish_Leader_View(leader_read::e_kind kind, const leader_read::s_leader* leaders, int16_t count);
    void Present_Leader_Text(const char* text);
    void Close_Leader_Text(const char* text);
    void Ready_Leader_View(leader_read::e_kind kind);

    struct s_leader_view_scope {
        leader_read::e_kind kind;
        const char* text;
        const leader_read::s_leader* name;
        s_leader_view_scope* parent;
        bool ready = false;
        bool closed = false;
        bool revoked = false;
        explicit s_leader_view_scope(leader_read::e_kind kind, const char* text = nullptr, const leader_read::s_leader* name = nullptr);
        ~s_leader_view_scope();
        void Close();
        s_leader_view_scope(const s_leader_view_scope&) = delete;
        s_leader_view_scope& operator=(const s_leader_view_scope&) = delete;
    };

    struct s_leader_interruption_scope {
        bool interrupted = false;
        explicit s_leader_interruption_scope(const char* own_text = nullptr);
        ~s_leader_interruption_scope();
        s_leader_interruption_scope(const s_leader_interruption_scope&) = delete;
        s_leader_interruption_scope& operator=(const s_leader_interruption_scope&) = delete;
    };

    struct s_observation_scope {
        e_event leaving;
        s_observation_scope(e_event entered, e_event leaving);
        ~s_observation_scope();
        s_observation_scope(const s_observation_scope&) = delete;
        s_observation_scope& operator=(const s_observation_scope&) = delete;
    };

    // Main-view owner lifetime, not proof of an open box or a valid selection.
    struct s_fleet_box_view_scope {
        s_fleet_box_view_scope();
        ~s_fleet_box_view_scope();
        s_fleet_box_view_scope(const s_fleet_box_view_scope&) = delete;
        s_fleet_box_view_scope& operator=(const s_fleet_box_view_scope&) = delete;
        void Initialized();

      private:
        bool initialized = false;
    };

    // One top-level main-view input poll, not the enclosing owner's lifetime.
    // An interruption does not restore authority when a child returns.
    struct s_main_view_input_scope {
        s_main_view_input_scope();
        ~s_main_view_input_scope();
        s_main_view_input_scope(const s_main_view_input_scope&) = delete;
        s_main_view_input_scope& operator=(const s_main_view_input_scope&) = delete;
    };

    struct s_world_update_scope {
        bool replacement;
        bool committed = false;
        explicit s_world_update_scope(bool new_world);
        ~s_world_update_scope();
        void Commit();
    };

    struct s_resolution_scope {
        s_resolution_scope();
        ~s_resolution_scope();
    };
} // namespace game_events
