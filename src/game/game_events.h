#pragma once

#include <cstdint>

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
    };

    // Native fleet-box presentation result, borrowed only during publication.
    // Only the cached-route branch; star is -1 when its name is not disclosed.
    struct s_fleet_box_info {
        int16_t ship;
        uint8_t turns_left;
        int16_t star;
    };

    using t_callback = void (*)(e_event event, void* context);
    using t_fleet_box_callback = void (*)(const s_fleet_box_info* info, void* context);
    // Synchronous observation only. Register while quiescent or on the publishing
    // thread; callbacks must not throw or recursively mutate the game.
    uint64_t Subscribe(t_callback callback, void* context);
    uint64_t Subscribe_Fleet_Box_Info(t_fleet_box_callback callback, void* context);
    void Unsubscribe(uint64_t subscription);
    void Publish(e_event event);
    void Publish_Fleet_Box_Info(const s_fleet_box_info* info);

    struct s_fleet_box_view_scope {
        s_fleet_box_view_scope();
        ~s_fleet_box_view_scope();
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
