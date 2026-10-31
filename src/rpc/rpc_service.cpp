#include "rpc_service.h"
#include "rpc_output.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <mutex>
#include <limits>
#include <thread>
#include <utility>

#include <libwebsockets.h>
#include <uv.h>

namespace rpc {
    namespace {
        struct s_connection {
            lws* socket = nullptr;
            uint64_t client = 0;
            uint64_t epoch = 0;
            uint64_t opened_ms = 0;
            uint64_t rate_window_ms = 0;
            uint32_t messages = 0;
            uint32_t callbacks = 0;
            bool authenticated = false;
            bool controller = false;
            bool assembling = false;
            bool close_after_output = false;
            std::string public_id;
            std::string input;
            s_output_queue output;
        };

        struct s_service_state {
            s_service_config config;
            s_protocol protocol;
            s_bridge owned_bridge;
            s_bridge* bridge = nullptr;
            std::thread thread;
            std::mutex startup_mutex;
            std::condition_variable startup_condition;
            bool ready = false;
            bool successful = false;
            std::atomic<bool> stopping = false;
            uv_loop_t loop;
            uv_async_t wake;
            uv_timer_t timer;
            bool loop_initialized = false;
            bool wake_initialized = false;
            bool timer_initialized = false;
            lws_context* context = nullptr;
            uint16_t port = 0;
            uint64_t next_client = 0;
            uint64_t controller = 0;
            size_t output_bytes = 0;
            std::array<s_connection, 4> connections;
        };

        int Callback(lws* socket, lws_callback_reasons reason, void* user, void* input, size_t size);

        const lws_protocols protocols[] = {
            {"orion2.rpc", Callback, 0, 4096, 0, nullptr, 4096},
            {nullptr, nullptr, 0, 0, 0, nullptr, 0},
        };

        const lws_retry_bo_t idle_policy = {nullptr, 0, 0, 5, 10, 0};

        s_connection* Find_Connection(s_service_state* state, lws* socket) {
            for (s_connection& connection : state->connections) {
                if (connection.socket == socket) {
                    return &connection;
                }
            }
            return nullptr;
        }

        int Close(lws* socket, lws_close_status status) {
            lws_close_reason(socket, status, nullptr, 0);
            return -1;
        }

        void Forget_Connection(s_service_state* state, s_connection* connection) {
            Cancel_Client(state->bridge, connection->client);
            if (state->controller == connection->client) {
                state->controller = 0;
            }
            state->output_bytes -= connection->output.bytes;
            *connection = {};
        }

        bool Queue_Output(s_service_state* state, s_connection* connection, std::string json, const std::string* state_key = nullptr) {
            if (!Queue_Output(&connection->output, std::move(json), state_key, &state->output_bytes)) return false;
            lws_callback_on_writable(connection->socket);
            return true;
        }

        bool Read_Header(lws* socket, lws_token_indexes token, std::string* value) {
            const int size = lws_hdr_total_length(socket, token);
            if (size < 0 || size >= 256) {
                return false;
            }
            char text[256];
            const int copied = lws_hdr_copy(socket, text, sizeof(text), token);
            if (copied < 0 || copied != size) {
                return false;
            }
            value->assign(text, (size_t)size);
            return true;
        }

        bool Local_Origin(const std::string& origin) {
            if (origin.empty()) {
                return true;
            }
            size_t start = 0;
            if (origin.starts_with("http://")) {
                start = 7;
            } else if (origin.starts_with("https://")) {
                start = 8;
            } else {
                return false;
            }
            const std::string authority = origin.substr(start);
            for (const char* host : {"localhost", "127.0.0.1", "[::1]"}) {
                const size_t size = std::strlen(host);
                if (authority == host) {
                    return true;
                }
                if (authority.starts_with(host) && authority.size() > size + 1 && authority[size] == ':') {
                    uint32_t port = 0;
                    bool valid = true;
                    for (size_t index = size + 1; index < authority.size(); ++index) {
                        if (authority[index] < '0' || authority[index] > '9' || port > 6553) {
                            valid = false;
                            break;
                        }
                        port = port * 10 + (uint32_t)(authority[index] - '0');
                    }
                    if (valid && port > 0 && port <= 65535) {
                        return true;
                    }
                }
            }
            return false;
        }

        bool Allowed_Upgrade(s_service_state* state, lws* socket) {
            if (state->stopping || std::none_of(state->connections.begin(), state->connections.end(), [](const s_connection& connection) { return connection.socket == nullptr; })) {
                return false;
            }
            std::string host, origin, path;
            if (!Read_Header(socket, WSI_TOKEN_HOST, &host) || !Read_Header(socket, WSI_TOKEN_ORIGIN, &origin) ||
                !Read_Header(socket, WSI_TOKEN_GET_URI, &path) || path != "/rpc" ||
                lws_hdr_total_length(socket, WSI_TOKEN_HTTP_URI_ARGS) != 0) {
                return false;
            }
            const std::string port = ":" + std::to_string(state->port);
            return (host == "127.0.0.1" + port || host == "localhost" + port) && Local_Origin(origin);
        }

        std::string New_Public_Id() {
            uint8_t random[16];
            if (uv_random(nullptr, nullptr, random, sizeof(random), 0, nullptr) != 0) {
                return {};
            }
            constexpr char hex[] = "0123456789abcdef";
            std::string id;
            id.reserve(32);
            for (uint8_t value : random) {
                id += hex[value >> 4];
                id += hex[value & 15];
            }
            return id;
        }

        bool Handle_Message(s_service_state* state, s_connection* connection) {
            s_request request;
            int32_t code = Parse_Request(connection->input.data(), connection->input.size(), &request);
            if (code != 0) {
                return Queue_Output(state, connection, Error_Response(&request, code));
            }
            if (request.notification) {
                return true;
            }
            if (!connection->authenticated && request.method != "rpc.hello") {
                return Queue_Output(state, connection, Error_Response(&request, RPC_UNAUTHORIZED));
            }
            const s_method_descriptor* method = Find_Method(request.method);
            if (method == nullptr || !method->implemented) {
                return Queue_Output(state, connection, Error_Response(&request, RPC_METHOD_NOT_FOUND));
            }
            if (!Validate_Params(&state->protocol, &request)) {
                return Queue_Output(state, connection, Error_Response(&request, RPC_INVALID_PARAMS));
            }
            if (request.method == "rpc.hello") {
                std::string key;
                bool controller = false;
                if (!Read_Hello(&request, &key, &controller) || !Keys_Match(state->config.key, key)) {
                    connection->close_after_output = true;
                    return Queue_Output(state, connection, Error_Response(&request, RPC_UNAUTHORIZED));
                }
                if (controller && state->controller != 0 && state->controller != connection->client) {
                    return Queue_Output(state, connection, Error_Response(&request, RPC_CONTROL_REQUIRED));
                }
                if (connection->authenticated && controller != connection->controller) {
                    return Queue_Output(state, connection, Error_Response(&request, RPC_INVALID_PARAMS));
                }
                if (!controller && state->controller == connection->client) {
                    state->controller = 0;
                }
                if (controller) {
                    state->controller = connection->client;
                }
                if (!Authorize_Client(state->bridge, connection->client, connection->epoch)) return false;
                connection->authenticated = true;
                connection->controller = controller;
                std::string result = "{\"connectionId\":" + Json_String(connection->public_id);
                result += ",\"contractVersion\":\"orion2.rpc/0\",\"build\":\"orion2re\",\"authorization\":\"authorized\",\"role\":";
                result += Json_String(controller ? "controller" : "reader") + "}";
                return Queue_Output(state, connection, Result_Response(&request, result));
            }
            if (request.method == "rpc.describe") {
                return Queue_Output(state, connection, Result_Response(&request, Describe_Methods(&request)));
            }
            const int32_t queued = Submit_Work(state->bridge, connection->client, connection->epoch, &request, Monotonic_Ms() + Protocol_Limits()->request_timeout_ms);
            return queued == 0 || Queue_Output(state, connection, Error_Response(&request, queued));
        }

        int Callback(lws* socket, lws_callback_reasons reason, void* user, void* input, size_t size) {
            (void)user;
            auto* state = (s_service_state*)lws_context_user(lws_get_context(socket));
            if (state == nullptr) {
                return 0;
            }
            if (reason == LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION) {
                return Allowed_Upgrade(state, socket) ? 0 : -1;
            }
            if (reason == LWS_CALLBACK_HTTP) {
                return -1;
            }
            if (reason == LWS_CALLBACK_ESTABLISHED) {
                for (s_connection& connection : state->connections) {
                    if (connection.socket != nullptr) {
                        continue;
                    }
                    if (state->next_client == std::numeric_limits<uint64_t>::max()) {
                        return -1;
                    }
                    connection.client = ++state->next_client;
                    connection.public_id = New_Public_Id();
                    if (connection.public_id.empty() || !Register_Client(state->bridge, connection.client, &connection.epoch)) {
                        connection = {};
                        return -1;
                    }
                    connection.socket = socket;
                    connection.opened_ms = uv_now(&state->loop);
                    connection.rate_window_ms = connection.opened_ms;
                    connection.input.reserve(Protocol_Limits()->input_bytes);
                    return 0;
                }
                return -1;
            }
            s_connection* connection = Find_Connection(state, socket);
            if (connection == nullptr) {
                return 0;
            }
            if (reason == LWS_CALLBACK_CLOSED) {
                Forget_Connection(state, connection);
                return 0;
            }
            if (!Client_Is_Current(state->bridge, connection->client, connection->epoch)) {
                return Close(socket, LWS_CLOSE_STATUS_POLICY_VIOLATION);
            }
            if (reason == LWS_CALLBACK_RECEIVE) {
                const uint64_t now = uv_now(&state->loop);
                if (now - connection->rate_window_ms >= 1000) {
                    connection->rate_window_ms = now;
                    connection->callbacks = 0;
                    connection->messages = 0;
                }
                if (lws_frame_is_binary(socket)) {
                    return Close(socket, LWS_CLOSE_STATUS_UNACCEPTABLE_OPCODE);
                }
                if (++connection->callbacks > Protocol_Limits()->receive_callbacks_per_second || connection->close_after_output) {
                    return Close(socket, LWS_CLOSE_STATUS_POLICY_VIOLATION);
                }
                if (!connection->assembling) {
                    if (!lws_is_first_fragment(socket)) {
                        return -1;
                    }
                    connection->assembling = true;
                }
                const size_t remaining = Protocol_Limits()->input_bytes - connection->input.size();
                if (size > remaining || lws_remaining_packet_payload(socket) > remaining - size) {
                    return Close(socket, LWS_CLOSE_STATUS_MESSAGE_TOO_LARGE);
                }
                connection->input.append((const char*)input, size);
                if (lws_is_final_fragment(socket) && lws_remaining_packet_payload(socket) == 0) {
                    if (++connection->messages > Protocol_Limits()->messages_per_second || !Handle_Message(state, connection)) {
                        return Close(socket, LWS_CLOSE_STATUS_POLICY_VIOLATION);
                    }
                    connection->input.clear();
                    connection->assembling = false;
                }
            } else if (reason == LWS_CALLBACK_SERVER_WRITEABLE) {
                if (connection->output.messages.empty()) {
                    return connection->close_after_output ? Close(socket, LWS_CLOSE_STATUS_POLICY_VIOLATION) : 0;
                }
                const std::string& message = connection->output.messages.front().json;
                const size_t count = std::min((size_t)4096, message.size() - connection->output.offset);
                unsigned char buffer[LWS_PRE + 4096];
                std::memcpy(buffer + LWS_PRE, message.data() + connection->output.offset, count);
                const bool final = connection->output.offset + count == message.size();
                const int written = lws_write(socket, buffer + LWS_PRE, count, lws_write_ws_flags(LWS_WRITE_TEXT, connection->output.offset == 0, final));
                if (written < 0 || (size_t)written < count) {
                    return -1;
                }
                if (!Advance_Output(&connection->output, count, &state->output_bytes)) return -1;
                if (!connection->output.messages.empty() || connection->close_after_output) {
                    lws_callback_on_writable(socket);
                }
            }
            return 0;
        }

        void Tick(uv_timer_t* timer) {
            auto* state = (s_service_state*)timer->data;
            const uint64_t now = uv_now(&state->loop);
            Expire_Work(state->bridge, Monotonic_Ms());
            // This is our own libuv timer, not a callback servicing any wsi. Close
            // synchronously: an async timeout alone need not wake lws' foreign-loop timer.
            for (s_connection& connection : state->connections) {
                if (connection.socket != nullptr &&
                    ((!connection.authenticated && now - connection.opened_ms >= Protocol_Limits()->authentication_deadline_ms) ||
                        !Client_Is_Current(state->bridge, connection.client, connection.epoch))) {
                    lws_set_timeout(connection.socket, PENDING_TIMEOUT_USER_OK, LWS_TO_KILL_SYNC);
                }
            }
            s_publication publication;
            while (Take_Publication(state->bridge, &publication)) {
                for (s_connection& connection : state->connections) {
                    if (connection.socket != nullptr && connection.authenticated && connection.client == publication.client &&
                        connection.epoch == publication.epoch) {
                        if (!Queue_Output(state, &connection, std::move(publication.json), &publication.state_key)) {
                            lws_set_timeout(connection.socket, PENDING_TIMEOUT_USER_OK, LWS_TO_KILL_SYNC);
                        }
                        break;
                    }
                }
            }
        }

        void Wake(uv_async_t* wake) {
            (void)wake;
        }

        void Signal_Startup(s_service_state* state, bool successful) {
            std::lock_guard lock(state->startup_mutex);
            state->successful = successful;
            state->ready = true;
            state->startup_condition.notify_one();
        }

        void Destroy_Backend(s_service_state* state) {
            Stop_Bridge(state->bridge);
            if (state->timer_initialized) {
                uv_timer_stop(&state->timer);
                uv_close((uv_handle_t*)&state->timer, nullptr);
            }
            if (state->wake_initialized) {
                uv_close((uv_handle_t*)&state->wake, nullptr);
            }
            if (state->context != nullptr) {
                lws_context_destroy(state->context);
            }
            if (state->loop_initialized) {
                while (state->context != nullptr || uv_loop_alive(&state->loop)) {
                    uv_run(&state->loop, UV_RUN_NOWAIT);
                    if (state->context != nullptr) {
                        lws_context_destroy(state->context);
                    }
                }
                uv_loop_close(&state->loop);
            }
            Shutdown_Protocol(&state->protocol);
        }

        void Run_Service(s_service_state* state) {
            bool initialized = Initialize_Protocol(&state->protocol);
            if (initialized) {
                state->loop_initialized = uv_loop_init(&state->loop) == 0;
                initialized = state->loop_initialized;
            }
            if (initialized) {
                state->wake_initialized = uv_async_init(&state->loop, &state->wake, Wake) == 0;
                initialized = state->wake_initialized;
            }
            if (initialized) {
                state->timer_initialized = uv_timer_init(&state->loop, &state->timer) == 0;
                initialized = state->timer_initialized;
            }
            if (initialized) {
                state->timer.data = state;
                uv_timer_start(&state->timer, Tick, 100, 100);
                void* loops[] = {&state->loop};
                lws_context_creation_info info = {};
                info.port = state->config.port;
                info.iface = "127.0.0.1";
                info.vhost_name = "orion2-rpc";
                info.protocols = protocols;
                info.user = state;
                info.options = LWS_SERVER_OPTION_LIBUV | LWS_SERVER_OPTION_VALIDATE_UTF8;
                info.foreign_loops = loops;
                info.pcontext = &state->context;
                info.fd_limit_per_thread = 16;
                info.max_http_header_data = 1024;
                info.max_http_header_pool = 4;
                info.timeout_secs = 5;
                info.timeout_secs_ah_idle = 5;
                info.retry_and_idle_policy = &idle_policy;
                state->context = lws_create_context(&info);
                initialized = state->context != nullptr;
                if (initialized) {
                    state->port = (uint16_t)lws_get_vhost_listen_port(lws_get_vhost_by_name(state->context, "orion2-rpc"));
                    initialized = state->port != 0;
                }
            }
            Signal_Startup(state, initialized);
            if (initialized) {
                while (!state->stopping) {
                    uv_run(&state->loop, UV_RUN_ONCE);
                }
            }
            Destroy_Backend(state);
        }
    } // namespace

    bool Start_Service(s_service* service, const s_service_config* config, std::string* diagnostic) {
        if (service == nullptr || config == nullptr || service->state != nullptr || config->key.empty() ||
            config->key.size() > 384 || config->key.find('\0') != std::string::npos || Json_String(config->key).empty()) {
            if (diagnostic != nullptr) {
                *diagnostic = "RPC requires a nonempty configured key of at most 384 UTF-8 bytes";
            }
            return false;
        }
        auto* state = new s_service_state;
        service->state = state;
        state->config = *config;
        state->bridge = config->bridge != nullptr ? config->bridge : &state->owned_bridge;
        state->thread = std::thread(Run_Service, state);
        std::unique_lock lock(state->startup_mutex);
        state->startup_condition.wait(lock, [state] { return state->ready; });
        const bool successful = state->successful;
        lock.unlock();
        if (!successful) {
            Stop_Service(service);
            if (diagnostic != nullptr) {
                *diagnostic = "RPC loopback listener initialization failed";
            }
        }
        return successful;
    }

    void Stop_Service(s_service* service) {
        if (service == nullptr || service->state == nullptr) {
            return;
        }
        auto* state = (s_service_state*)service->state;
        if (state->successful) {
            state->stopping = true;
            uv_async_send(&state->wake);
        }
        if (state->thread.joinable()) {
            state->thread.join();
        }
        delete state;
        service->state = nullptr;
    }

    uint16_t Service_Port(const s_service* service) {
        return service != nullptr && service->state != nullptr ? ((const s_service_state*)service->state)->port : 0;
    }
} // namespace rpc
