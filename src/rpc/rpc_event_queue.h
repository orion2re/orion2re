#pragma once

#include "rpc_protocol.h"

#include <array>
#include <cstdint>
#include <mutex>
#include <string_view>

namespace rpc {
    constexpr size_t MAX_PENDING_EVENTS = 64;
    struct s_event_position {
        uint64_t sequence = 0;
        uint64_t epoch = 1;
        bool stopped = false;
    };
    struct s_pending_event {
        uint64_t sequence = 0;
        uint8_t kind = 0;
        std::array<char, 33> battle_id = {};
    };
    struct s_event_batch {
        std::array<s_pending_event, MAX_PENDING_EVENTS> items;
        size_t count = 0;
        uint64_t overflow_position = 0;
        s_event_position position;
    };
    // Copied lifecycle notifications only, never a game-state publication/cache.
    struct s_event_queue {
        std::mutex mutex;
        s_event_batch pending;
    };

    bool Push_Event(s_event_queue* queue, uint8_t kind, std::string_view battle_id);
    s_event_position Event_Position(s_event_queue* queue);
    s_event_batch Take_Events(s_event_queue* queue);
    void Revoke_Events(s_event_queue* queue);
    void Stop_Events(s_event_queue* queue);
    bool Event_Delivery_Expired(const s_session* session, const s_event_batch* batch);
} // namespace rpc
