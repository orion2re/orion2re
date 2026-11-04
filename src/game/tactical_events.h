#pragma once

#include <cstdint>

namespace tactical_events {
    enum e_kind {
        OPENED,
        CLOSED,
        REVOKED,
    };
    struct s_event {
        e_kind kind;
        int16_t viewer;
    };
    using t_callback = void (*)(const s_event* event, void* context) noexcept;

    // Register before the worker starts, or on the publishing game thread.
    // Callbacks are synchronous, observation-only; the payload is callback-local.
    uint64_t Subscribe(t_callback callback, void* context);
    void Unsubscribe(uint64_t subscription);
    void Revoke() noexcept;

    struct s_battle_scope {
        int16_t viewer = -1;
        bool initialized = false;
        bool opened = false;
        void Open(int16_t local_observer) noexcept;
        void Close(int16_t local_observer) noexcept;
        ~s_battle_scope();
        s_battle_scope() = default;
        s_battle_scope(const s_battle_scope&) = delete;
        s_battle_scope& operator=(const s_battle_scope&) = delete;
    };
} // namespace tactical_events
