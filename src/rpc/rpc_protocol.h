#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace rpc {
    enum e_error : int32_t {
        RPC_PARSE_ERROR = -32700,
        RPC_INVALID_REQUEST = -32600,
        RPC_METHOD_NOT_FOUND = -32601,
        RPC_INVALID_PARAMS = -32602,
        RPC_INTERNAL_ERROR = -32603,
        RPC_UNAUTHORIZED = -32001,
        RPC_BUSY = -32002,
        RPC_CONTEXT_UNAVAILABLE = -32003,
        RPC_OBJECT_UNAVAILABLE = -32004,
        RPC_LIMIT_EXCEEDED = -32005,
        RPC_REQUEST_EXPIRED = -32006,
        RPC_CONTROL_REQUIRED = -32007,
    };

    struct s_request {
        std::string id_json;
        std::string method;
        std::string params_json;
        bool notification = false;
    };

    struct s_protocol {
        void* definitions = nullptr;
    };

    struct s_method_descriptor {
        const char* name;
        const char* request_type;
        const char* result_type;
        const char* role;
        const char* availability;
        bool implemented;
    };

    struct s_protocol_limits {
        size_t input_bytes;
        uint32_t depth;
        size_t request_id_bytes;
        uint64_t authentication_deadline_ms;
        size_t output_bytes_per_connection;
        size_t output_bytes_global;
        size_t output_messages_per_connection;
        uint32_t messages_per_second;
        uint32_t receive_callbacks_per_second;
        uint64_t request_timeout_ms;
        uint32_t commands_per_game_poll;
        uint64_t game_poll_budget_us;
        uint64_t capture_interval_ms;
    };

    bool Initialize_Protocol(s_protocol* protocol);
    const s_protocol_limits* Protocol_Limits();
    void Shutdown_Protocol(s_protocol* protocol);
    int32_t Parse_Request(const char* bytes, size_t size, s_request* request);
    const s_method_descriptor* Find_Method(std::string_view name);
    bool Validate_Params(const s_protocol* protocol, const s_request* request);
    bool Read_Hello(const s_request* request, std::string* key, bool* controller);
    std::string Describe_Methods(const s_request* request);
    std::string Json_String(std::string_view text);
    std::string Error_Response(const s_request* request, int32_t code);
    std::string Result_Response(const s_request* request, std::string_view result_json);
    bool Keys_Match(std::string_view configured, std::string_view supplied);
    uint64_t Monotonic_Us();
    uint64_t Monotonic_Ms();
    bool New_Opaque_Id(std::string* id);
} // namespace rpc
