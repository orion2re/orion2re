#include "tactical_events.h"

#include <array>
#include <limits>

namespace tactical_events {
    namespace {
        struct s_listener {
            uint64_t id = 0;
            t_callback callback = nullptr;
            void* context = nullptr;
        };
        std::array<s_listener, 4> listeners;
        uint64_t next_id = 0;

        void Publish(e_kind kind, int16_t viewer) noexcept {
            const s_event event{kind, viewer};
            const auto limit = next_id;
            for (const auto& listener : listeners) {
                if (listener.id != 0 && listener.id <= limit) listener.callback(&event, listener.context);
            }
        }
    } // namespace

    uint64_t Subscribe(t_callback callback, void* context) {
        if (callback == nullptr || next_id == std::numeric_limits<uint64_t>::max()) return 0;
        for (auto& listener : listeners) {
            if (listener.id == 0) {
                listener = {++next_id, callback, context};
                return listener.id;
            }
        }
        return 0;
    }

    void Unsubscribe(uint64_t subscription) {
        for (auto& listener : listeners) {
            if (listener.id == subscription) listener = {};
        }
    }

    void Revoke() noexcept {
        Publish(REVOKED, -1);
    }

    void s_battle_scope::Open(int16_t local_observer) noexcept {
        if (initialized) return;
        initialized = true;
        viewer = local_observer;
        if (viewer < 0) return;
        opened = true;
        Publish(OPENED, viewer);
    }

    void s_battle_scope::Close(int16_t local_observer) noexcept {
        if (!opened) return;
        opened = false;
        if (local_observer == viewer) {
            Publish(CLOSED, viewer);
        } else {
            Revoke();
        }
    }

    s_battle_scope::~s_battle_scope() {
        if (opened) Revoke();
    }
} // namespace tactical_events
