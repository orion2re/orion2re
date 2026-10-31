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
    };

    using t_callback = void (*)(e_event event, void* context);
    // Synchronous observation only. Register while quiescent or on the publishing
    // thread; callbacks must not throw or recursively mutate the game.
    uint64_t Subscribe(t_callback callback, void* context);
    void Unsubscribe(uint64_t subscription);
    void Publish(e_event event);

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
