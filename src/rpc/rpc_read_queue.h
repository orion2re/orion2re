#pragma once

#include "rpc_protocol.h"

#include <array>
#include <mutex>
#include <vector>

namespace rpc {
    constexpr size_t MAX_PENDING_READS = 8;
    struct s_read_slot {
        s_read_request request;
        bool executing = false;
    };
    struct s_read_queue {
        std::mutex mutex;
        std::array<s_read_slot, MAX_PENDING_READS> slots;
        uint64_t next_ticket = 0;
        bool stopped = false;
    };
    uint64_t Monotonic_Ms();
    bool Enqueue_Read(s_read_queue* queue, s_read_request request);
    bool Take_Read(s_read_queue* queue, s_read_request* request);
    bool Finish_Read(s_read_queue* queue, uint64_t ticket);
    void Cancel_Reads(s_read_queue* queue, uint64_t connection = 0, bool stop = false, std::vector<s_read_request>* cancelled = nullptr);
    bool Expire_Read(s_read_queue* queue, s_read_request* request);
} // namespace rpc
