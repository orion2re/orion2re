#pragma once

#include "rpc_protocol.h"
#include <mutex>

namespace rpc {
    struct s_operation_queue {
        std::mutex mutex;
        s_operation_request pending;
        bool executing = false, stopped = false;
        s_operation_request acknowledgement;
        bool acknowledgement_executing = false;
        std::string report_id;
    };
    bool Enqueue_Operation(s_operation_queue* queue, const s_operation_request& request, int* error = nullptr);
    void Set_Boarding_Report(s_operation_queue* queue, std::string report_id);
    bool Take_Operation(s_operation_queue* queue, s_operation_request* request, bool report_only = false);
    void Finish_Operation(s_operation_queue* queue, bool child = false);
    // Never cancels executing work. A returned request is guaranteed not to run.
    bool Cancel_Operation(s_operation_queue* queue, uint64_t connection, uint64_t sequence, s_operation_request* cancelled, bool stop = false, bool expired_only = false);
} // namespace rpc
