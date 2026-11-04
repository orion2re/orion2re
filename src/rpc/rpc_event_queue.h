#pragma once

#include "rpc_protocol.h"

#include <array>
#include <cstdint>
#include <mutex>
#include <string_view>

namespace rpc {
    constexpr size_t MAX_PENDING_EVENTS = 64;
    constexpr size_t MAX_PENDING_RESPONSE_BYTES = 8 * MAX_RESPONSE_BYTES;
    struct s_event_position {
        uint64_t sequence = 0;
        uint64_t epoch = 1;
        uint64_t read_epoch = 1;
        bool stopped = false;
    };
    struct s_pending_event {
        uint64_t sequence = 0;
        uint8_t kind = 0;
        std::array<char, 33> battle_id = {};
        std::array<char, 33> unit_id = {};
        uint64_t connection = 0;
        uint64_t read_epoch = 0;
        uint64_t selection = 0;
        std::string encoded_id;
        std::string response;
    };
    struct s_event_batch {
        std::array<s_pending_event, MAX_PENDING_EVENTS> items;
        size_t count = 0;
        size_t response_bytes = 0;
        uint64_t overflow_position = 0;
        s_event_position position;
    };
    // Ordered pending delivery only, never a game-state publication/cache.
    struct s_event_queue {
        std::mutex mutex;
        s_event_batch pending;
        size_t reserved_response_bytes = 0; // Includes a batch being handed to connection output.
    };

    bool Push_Event(s_event_queue* queue, uint8_t kind, std::string_view battle_id, std::string_view unit_id = {});
    bool Push_Response(s_event_queue* queue, uint64_t connection, uint64_t epoch, uint64_t read_epoch, std::string response, uint64_t selection = 0, std::string encoded_id = {});
    void Invalidate_Read_Output(s_event_queue* queue);
    s_event_position Event_Position(s_event_queue* queue);
    s_event_batch Take_Events(s_event_queue* queue);
    void Release_Event_Batch(s_event_queue* queue, const s_event_batch* batch);
    void Revoke_Events(s_event_queue* queue);
    void Stop_Events(s_event_queue* queue);
    bool Event_Delivery_Expired(const s_session* session, const s_event_batch* batch);
} // namespace rpc
