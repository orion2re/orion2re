#include "rpc_service.h"
#include "rpc_protocol.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <mutex>
#include <thread>
#include <utility>

// libuv must declare ssize_t before libwebsockets defines it on Windows.
#include <uv.h>
#include <libwebsockets.h>

namespace rpc {
    namespace {
        struct s_connection {
            lws* socket = nullptr;
            s_session session;
            uint64_t opened_ms = 0;
            uint64_t rate_window_ms = 0;
            unsigned messages = 0, callbacks = 0;
            bool assembling = false;
            std::string input;
            std::deque<std::string> output;
            size_t output_bytes = 0, output_offset = 0;
        };

        struct s_service_state {
            s_service_config config;
            std::thread thread;
            std::mutex startup_mutex;
            std::condition_variable startup_condition;
            bool ready = false, successful = false;
            std::atomic<bool> stopping = false;
            uv_loop_t loop;
            uv_async_t wake;
            uv_timer_t timer;
            bool loop_initialized = false, wake_initialized = false, timer_initialized = false;
            lws_context* context = nullptr;
            uint16_t port = 0;
            std::array<s_connection, MAX_CONNECTIONS> connections;
        };

        int Callback(lws* socket, lws_callback_reasons reason, void* user, void* input, size_t size);
        const lws_protocols protocols[] = {
            {"orion2.rpc", Callback, 0, 4096, 0, nullptr, 4096},
            {nullptr, nullptr, 0, 0, 0, nullptr, 0},
        };
        const lws_retry_bo_t idle_policy = {nullptr, 0, 0, 5, 10, 0};

        int Close(lws* socket, lws_close_status status) {
            lws_close_reason(socket, status, nullptr, 0);
            return -1;
        }

        bool Read_Header(lws* socket, lws_token_indexes token, std::string* value) {
            const int size = lws_hdr_total_length(socket, token);
            if (size < 0 || size >= 256) return false;
            char text[256];
            const int copied = lws_hdr_copy(socket, text, sizeof(text), token);
            if (copied != size) return false;
            value->assign(text, (size_t)size);
            return true;
        }

        bool Local_Origin(const std::string& origin) {
            if (origin.empty()) return true;
            const size_t start = origin.starts_with("http://") ? 7 : (origin.starts_with("https://") ? 8 : 0);
            if (start == 0) return false;
            const std::string authority = origin.substr(start);
            for (const char* host : {"localhost", "127.0.0.1", "[::1]"}) {
                const size_t size = std::strlen(host);
                if (authority == host) return true;
                if (!authority.starts_with(host) || authority.size() <= size + 1 || authority[size] != ':') continue;
                uint32_t port = 0;
                bool valid = true;
                for (size_t index = size + 1; index < authority.size(); ++index) {
                    if (authority[index] < '0' || authority[index] > '9' || port > 6553) {
                        valid = false;
                        break;
                    }
                    port = port * 10 + (uint32_t)(authority[index] - '0');
                }
                if (valid && port > 0 && port <= 65535) return true;
            }
            return false;
        }

        bool Allowed_Upgrade(s_service_state* state, lws* socket) {
            if (state->stopping || std::none_of(state->connections.begin(), state->connections.end(), [](const auto& connection) { return connection.socket == nullptr; })) return false;
            std::string host, origin, path;
            if (!Read_Header(socket, WSI_TOKEN_HOST, &host) || !Read_Header(socket, WSI_TOKEN_ORIGIN, &origin) ||
                !Read_Header(socket, WSI_TOKEN_GET_URI, &path) || path != "/rpc" ||
                lws_hdr_total_length(socket, WSI_TOKEN_HTTP_URI_ARGS) != 0) return false;
            const std::string port = ":" + std::to_string(state->port);
            return (host == "127.0.0.1" + port || host == "localhost" + port) && Local_Origin(origin);
        }

        bool Handle_Message(s_service_state* state, s_connection* connection) {
            const bool control_available = std::none_of(state->connections.begin(), state->connections.end(), [](const auto& item) { return item.session.controller; });
            std::string response;
            if (!Process_Request(connection->input, state->config.key, control_available, &connection->session, &response)) return false;
            if (response.empty()) return true;
            if (connection->output.size() >= MAX_OUTPUT_MESSAGES || response.size() > MAX_OUTPUT_BYTES - connection->output_bytes) return false;
            connection->output_bytes += response.size();
            connection->output.push_back(std::move(response));
            lws_callback_on_writable(connection->socket);
            return true;
        }

        int Callback(lws* socket, lws_callback_reasons reason, void* user, void* input, size_t size) {
            (void)user;
            auto* state = (s_service_state*)lws_context_user(lws_get_context(socket));
            if (state == nullptr) return 0;
            if (reason == LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION) return Allowed_Upgrade(state, socket) ? 0 : -1;
            if (reason == LWS_CALLBACK_HTTP) return -1;
            if (reason == LWS_CALLBACK_ESTABLISHED) {
                for (auto& connection : state->connections) {
                    if (connection.socket != nullptr) continue;
                    connection.socket = socket;
                    connection.opened_ms = connection.rate_window_ms = uv_now(&state->loop);
                    return 0;
                }
                return -1;
            }
            auto found = std::find_if(state->connections.begin(), state->connections.end(), [socket](const auto& item) { return item.socket == socket; });
            if (found == state->connections.end()) return 0;
            auto* connection = &*found;
            if (reason == LWS_CALLBACK_CLOSED) {
                *connection = {};
                return 0;
            }
            if (reason == LWS_CALLBACK_RECEIVE) {
                const uint64_t now = uv_now(&state->loop);
                if (now - connection->rate_window_ms >= 1000) {
                    connection->rate_window_ms = now;
                    connection->callbacks = connection->messages = 0;
                }
                if (lws_frame_is_binary(socket)) return Close(socket, LWS_CLOSE_STATUS_UNACCEPTABLE_OPCODE);
                if (++connection->callbacks > 128 || connection->session.close_after_output) return Close(socket, LWS_CLOSE_STATUS_POLICY_VIOLATION);
                if (!connection->assembling) {
                    if (!lws_is_first_fragment(socket)) return -1;
                    connection->assembling = true;
                }
                const size_t remaining = MAX_INPUT_BYTES - connection->input.size();
                if (size > remaining || lws_remaining_packet_payload(socket) > remaining - size) return Close(socket, LWS_CLOSE_STATUS_MESSAGE_TOO_LARGE);
                connection->input.append((const char*)input, size);
                if (lws_is_final_fragment(socket) && lws_remaining_packet_payload(socket) == 0) {
                    if (++connection->messages > 32 || !Handle_Message(state, connection)) return Close(socket, LWS_CLOSE_STATUS_POLICY_VIOLATION);
                    connection->input.clear();
                    connection->assembling = false;
                }
            } else if (reason == LWS_CALLBACK_SERVER_WRITEABLE) {
                if (connection->output.empty()) return connection->session.close_after_output ? Close(socket, LWS_CLOSE_STATUS_POLICY_VIOLATION) : 0;
                const auto& message = connection->output.front();
                const size_t count = std::min(size_t(4096), message.size() - connection->output_offset);
                unsigned char buffer[LWS_PRE + 4096];
                std::memcpy(buffer + LWS_PRE, message.data() + connection->output_offset, count);
                const bool final = connection->output_offset + count == message.size();
                const int written = lws_write(socket, buffer + LWS_PRE, count, lws_write_ws_flags(LWS_WRITE_TEXT, connection->output_offset == 0, final));
                if (written < 0 || (size_t)written < count) return -1;
                connection->output_offset += count;
                if (final) {
                    connection->output_bytes -= message.size();
                    connection->output.pop_front();
                    connection->output_offset = 0;
                }
                if (!connection->output.empty() || connection->session.close_after_output) lws_callback_on_writable(socket);
            }
            return 0;
        }

        void Check_Connections(uv_timer_t* timer) {
            auto* state = (s_service_state*)timer->data;
            for (auto& connection : state->connections) {
                if (connection.socket != nullptr && !connection.session.authenticated &&
                    uv_now(&state->loop) - connection.opened_ms >= AUTHENTICATION_DEADLINE_MS) {
                    // Synchronous close here wakes no game work and bounds unauthenticated connections.
                    lws_set_timeout(connection.socket, PENDING_TIMEOUT_USER_OK, LWS_TO_KILL_SYNC);
                }
            }
        }

        void Wake(uv_async_t* wake) {
            auto* state = (s_service_state*)wake->data;
            if (state->stopping) uv_stop(&state->loop);
        }

        void Destroy_Backend(s_service_state* state) {
            if (state->timer_initialized) {
                uv_timer_stop(&state->timer);
                uv_close((uv_handle_t*)&state->timer, nullptr);
            }
            if (state->wake_initialized) uv_close((uv_handle_t*)&state->wake, nullptr);
            if (state->context != nullptr) lws_context_destroy(state->context);
            if (state->loop_initialized) {
                // libwebsockets destroys foreign-loop handles asynchronously and
                // clears pcontext only when the context is safe to release.
                while (state->context != nullptr || uv_loop_alive(&state->loop)) {
                    uv_run(&state->loop, UV_RUN_NOWAIT);
                    if (state->context != nullptr) lws_context_destroy(state->context);
                }
                uv_loop_close(&state->loop);
            }
        }

        void Run_Service(s_service_state* state) {
            state->loop_initialized = uv_loop_init(&state->loop) == 0;
            bool initialized = state->loop_initialized;
            if (initialized) initialized = state->wake_initialized = uv_async_init(&state->loop, &state->wake, Wake) == 0;
            if (state->wake_initialized) state->wake.data = state;
            if (initialized) initialized = state->timer_initialized = uv_timer_init(&state->loop, &state->timer) == 0;
            if (initialized) {
                state->timer.data = state;
                initialized = uv_timer_start(&state->timer, Check_Connections, 1000, 1000) == 0;
            }
            if (initialized) {
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
                info.timeout_secs = info.timeout_secs_ah_idle = 5;
                info.retry_and_idle_policy = &idle_policy;
                state->context = lws_create_context(&info);
                initialized = state->context != nullptr;
                if (initialized) {
                    auto* host = lws_get_vhost_by_name(state->context, "orion2-rpc");
                    state->port = host != nullptr ? (uint16_t)lws_get_vhost_listen_port(host) : 0;
                    initialized = state->port != 0;
                }
            }
            {
                std::lock_guard lock(state->startup_mutex);
                state->successful = initialized;
                state->ready = true;
                state->startup_condition.notify_one();
            }
            if (initialized) {
                // Only the async callback stops this loop, so Stop_Service
                // cannot race teardown before it has sent the wakeup.
                uv_run(&state->loop, UV_RUN_DEFAULT);
            }
            Destroy_Backend(state);
        }
    } // namespace

    bool Start_Service(s_service* service, const s_service_config* config, std::string* diagnostic) {
        if (diagnostic != nullptr) diagnostic->clear();
        if (service == nullptr || config == nullptr || service->state != nullptr ||
            config->key.empty() || config->key.size() > 384 || config->key.find('\0') != std::string::npos) {
            if (diagnostic != nullptr) *diagnostic = "RPC requires an unused service and a nonempty configured key of at most 384 bytes";
            return false;
        }
        auto* state = new s_service_state;
        service->state = state;
        state->config = *config;
        state->thread = std::thread(Run_Service, state);
        std::unique_lock lock(state->startup_mutex);
        state->startup_condition.wait(lock, [state] { return state->ready; });
        const bool successful = state->successful;
        lock.unlock();
        if (!successful) {
            Stop_Service(service);
            if (diagnostic != nullptr) *diagnostic = "RPC loopback listener initialization failed";
        }
        return successful;
    }

    void Stop_Service(s_service* service) {
        if (service == nullptr || service->state == nullptr) return;
        auto* state = (s_service_state*)service->state;
        if (state->successful) {
            state->stopping = true;
            uv_async_send(&state->wake);
        }
        if (state->thread.joinable()) state->thread.join();
        delete state;
        service->state = nullptr;
    }

    uint16_t Service_Port(const s_service* service) {
        return service != nullptr && service->state != nullptr ? ((const s_service_state*)service->state)->port : 0;
    }
} // namespace rpc
