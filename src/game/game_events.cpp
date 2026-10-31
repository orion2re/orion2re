#include "game_events.h"

#include <limits>

namespace game_events {
    namespace {
        struct s_listener {
            uint64_t id = 0;
            t_callback callback = nullptr;
            void* context = nullptr;
        };
        s_listener listeners[8];
        uint64_t next_id = 0;
        uint32_t count = 0;
    } // namespace

    uint64_t Subscribe(t_callback callback, void* context) {
        if (callback == nullptr || next_id == std::numeric_limits<uint64_t>::max()) return 0;
        for (s_listener& listener : listeners) {
            if (listener.id == 0) {
                listener = {++next_id, callback, context};
                ++count;
                return listener.id;
            }
        }
        return 0;
    }

    void Unsubscribe(uint64_t subscription) {
        if (subscription == 0) return;
        for (s_listener& listener : listeners) {
            if (listener.id == subscription) {
                listener = {};
                --count;
                return;
            }
        }
    }

    void Publish(e_event event) {
        if (count == 0) return;
        uint64_t selected[8];
        for (uint32_t index = 0; index < 8; ++index) selected[index] = listeners[index].id;
        for (uint64_t id : selected) {
            if (id == 0) continue;
            for (const s_listener& listener : listeners) {
                if (listener.id == id) {
                    const s_listener call = listener;
                    call.callback(event, call.context);
                    break;
                }
            }
        }
    }

    s_world_update_scope::s_world_update_scope(bool new_world) : replacement(new_world) {
        Publish(replacement ? WORLD_REPLACE_BEGIN : WORLD_UPDATE_BEGIN);
    }

    s_world_update_scope::~s_world_update_scope() {
        Publish(replacement ? (committed ? WORLD_REPLACE_COMMITTED : WORLD_REPLACE_FAILED) : (committed ? WORLD_UPDATE_COMMITTED : WORLD_UPDATE_FAILED));
    }

    void s_world_update_scope::Commit() {
        committed = true;
    }

    s_resolution_scope::s_resolution_scope() {
        Publish(RESOLUTION_BEGIN);
    }

    s_resolution_scope::~s_resolution_scope() {
        Publish(RESOLUTION_END);
    }
} // namespace game_events
