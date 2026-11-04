#include "rpc_service.h"
#include "rpc_protocol.h"
#include "rpc_event_queue.h"
#include "rpc_read_queue.h"
#include "rpc_operation_queue.h"

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
        struct s_output {
            std::string json;
            uint64_t read_epoch = 0;
            uint8_t kind = 0;
            std::array<char, 33> battle_id = {};
        };
        struct s_connection {
            lws* socket = nullptr;
            s_session session;
            uint64_t opened_ms = 0;
            uint64_t rate_window_ms = 0;
            unsigned messages = 0, callbacks = 0;
            bool assembling = false;
            std::string input;
            std::deque<s_output> output;
            size_t output_bytes = 0, output_offset = 0;
            uint64_t generation = 0;
            uint64_t read_selection = 1;
            unsigned pending_reads = 0;
            bool expired = false;
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
            s_event_queue events;
            s_read_queue reads;
            s_operation_queue operations;
            uint64_t next_connection = 0;
        };

        void Cancel_Connection_Work(s_service_state* state, const s_connection* connection) {
            Cancel_Reads(&state->reads, connection->generation);
            s_operation_request cancelled;
            Cancel_Operation(&state->operations, connection->generation, 0, &cancelled);
        }

        bool Queue_Output(s_connection* connection, s_output output) {
            if (connection->output.size() >= MAX_OUTPUT_MESSAGES || output.json.size() > MAX_OUTPUT_BYTES - connection->output_bytes) return false;
            connection->output_bytes += output.json.size();
            connection->output.push_back(std::move(output));
            lws_callback_on_writable(connection->socket);
            return true;
        }

        void Deliver(s_service_state* state) {
            auto batch = Take_Events(&state->events);
            for (auto& connection : state->connections) {
                if (connection.socket == nullptr) continue;
                auto& session = connection.session;
                if (Event_Delivery_Expired(&session, &batch)) connection.expired = true;
                if (!session.authenticated || session.close_after_output || connection.expired) {
                    Cancel_Connection_Work(state, &connection);
                    if (connection.expired) lws_callback_on_writable(connection.socket);
                    continue;
                }
                for (auto& event : batch.items) {
                    if (event.sequence == 0) continue;
                    if (event.kind == 0) {
                        if (event.connection != connection.generation) continue;
                        if (event.selection == connection.read_selection && connection.pending_reads > 0) --connection.pending_reads;
                        if (event.selection != 0 && event.selection != connection.read_selection) {
                            Error_Response(event.encoded_id, -32004, "Unavailable").swap(event.response);
                            event.read_epoch = 0;
                        }
                        if ((event.read_epoch != 0 && event.read_epoch != batch.position.read_epoch) ||
                            !Queue_Output(&connection, {std::move(event.response), event.read_epoch})) connection.expired = true;
                    } else if (event.sequence > session.events_after && (session.events & event.kind)) {
                        const char* name = "combat.viewInvalidated";
                        if (event.kind == EVENT_COMBAT_OPENED)
                            name = "combat.opened";
                        else if (event.kind == EVENT_COMBAT_CLOSED)
                            name = "combat.closed";
                        else if (event.kind == EVENT_COMBAT_SELECTED)
                            name = "combat.unitSelected";
                        else if (event.kind == EVENT_COMBAT_ATTENTION || event.kind == EVENT_COMBAT_ATTENTION_CLEARED)
                            name = "combat.localAttentionChanged";
                        else if (event.kind == EVENT_COMBAT_BOARDING)
                            name = "combat.boardingResolved";
                        std::string json = "{\"jsonrpc\":\"2.0\",\"method\":\"" + std::string(name) +
                            "\",\"params\":{\"battleId\":\"" + std::string(event.battle_id.data()) + "\"";
                        if (event.kind == EVENT_COMBAT_SELECTED) json += ",\"unitId\":" + (event.unit_id[0] == '\0' ? std::string("null") : "\"" + std::string(event.unit_id.data()) + "\"");
                        if (event.kind == EVENT_COMBAT_BOARDING) json += ",\"boarding\":" + event.response;
                        if (event.kind == EVENT_COMBAT_ATTENTION || event.kind == EVENT_COMBAT_ATTENTION_CLEARED)
                            json += std::string(",\"required\":") + (event.kind == EVENT_COMBAT_ATTENTION ? "true" : "false");
                        json += "}}";
                        // Only an adjacent complete unsent hint can be replaced.
                        const bool coalesce = event.kind == EVENT_COMBAT_INVALIDATED && !connection.output.empty() &&
                            (connection.output.size() > 1 || connection.output_offset == 0) &&
                            connection.output.back().kind == event.kind && connection.output.back().battle_id == event.battle_id;
                        if (!coalesce && !Queue_Output(&connection, {std::move(json), event.read_epoch, event.kind, event.battle_id})) connection.expired = true;
                    }
                    if (connection.expired) {
                        Cancel_Connection_Work(state, &connection);
                        lws_callback_on_writable(connection.socket);
                        break;
                    }
                }
            }
            // Release the separate budget only after transfer or destruction of payloads.
            for (auto& event : batch.items) std::string().swap(event.response);
            Release_Event_Batch(&state->events, &batch);
        }

        int Callback(lws* socket, lws_callback_reasons reason, void* user, void* input, size_t size);
        const lws_protocols protocols[] = {
            {"orion2.rpc", Callback, 0, 4096, 0, nullptr, 4096},
            {nullptr, nullptr, 0, 0, 0, nullptr, 0},
        };
        const lws_retry_bo_t idle_policy = {nullptr, 0, 0, 5, 10, 0};

        int Close(lws* socket, lws_close_status status) {
            auto* state = (s_service_state*)lws_context_user(lws_get_context(socket));
            for (const auto& connection : state->connections)
                if (connection.socket == socket) Cancel_Connection_Work(state, &connection);
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
            Deliver(state);
            if (connection->expired) return false;
            const auto position = Event_Position(&state->events);
            if (position.stopped || (connection->session.authenticated && connection->session.auth_epoch != position.epoch)) return false;
            const bool was_authenticated = connection->session.authenticated;
            bool selection_updated = false;
            const bool control_available = std::none_of(state->connections.begin(), state->connections.end(), [](const auto& item) { return item.session.controller; });
            std::string response;
            s_read_request read;
            s_operation_request operation;
            if (!Process_Request(connection->input, state->config.key, control_available, &connection->session, &response, &selection_updated, &read, &operation)) return false;
            if (connection->session.close_after_output) Cancel_Connection_Work(state, connection);
            const auto current = Event_Position(&state->events);
            if (!was_authenticated && connection->session.authenticated) connection->session.auth_epoch = current.epoch;
            if (selection_updated) {
                connection->session.events_after = current.sequence;
                if ((connection->session.events & 7) != 7) {
                    ++connection->read_selection;
                    std::vector<s_read_request> cancelled;
                    Cancel_Reads(&state->reads, connection->generation, false, &cancelled);
                    for (const auto& request : cancelled) {
                        if (!Queue_Output(connection, {Error_Response(request.encoded_id, -32004, "Unavailable")})) return false;
                    }
                    connection->pending_reads = 0;
                    s_operation_request cancelled_operation;
                    if (Cancel_Operation(&state->operations, connection->generation, 0, &cancelled_operation))
                        if (!Queue_Output(connection, {Error_Response(cancelled_operation.request.encoded_id, -32013, "Cancelled before dispatch")})) return false;
                }
            }
            if (read.method != READ_NONE) {
                read.connection = connection->generation;
                read.epoch = connection->session.auth_epoch;
                read.selection = connection->read_selection;
                read.deadline_ms = Monotonic_Ms() + READ_DEADLINE_MS;
                if (connection->pending_reads >= 2 || !Enqueue_Read(&state->reads, read)) {
                    response = Error_Response(read.encoded_id, -32006, "Busy");
                } else {
                    ++connection->pending_reads;
                }
            }
            if (operation.method != OP_NONE) {
                int admission_error = -32006;
                operation.request.connection = connection->generation;
                operation.request.epoch = connection->session.auth_epoch;
                operation.request.deadline_ms = Monotonic_Ms() + 2000;
                if (operation.method == OP_CANCEL) {
                    s_operation_request cancelled;
                    if (Cancel_Operation(&state->operations, connection->generation, operation.sequence, &cancelled)) {
                        if (!Queue_Output(connection, {Error_Response(cancelled.request.encoded_id, -32013, "Cancelled before dispatch")})) return false;
                        response = "{\"jsonrpc\":\"2.0\",\"id\":" + operation.request.encoded_id + ",\"result\":{\"cancelled\":true}}";
                    } else
                        response = Error_Response(operation.request.encoded_id, -32014, "Not pending or already dispatched");
                } else if (!Enqueue_Operation(&state->operations, operation, &admission_error)) {
                    response = Error_Response(operation.request.encoded_id, admission_error, admission_error == -32004 ? "Unavailable" : "Not admitted: busy");
                }
            }
            if (response.empty()) return true;
            return Queue_Output(connection, {std::move(response)});
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
                    connection.generation = ++state->next_connection;
                    connection.opened_ms = connection.rate_window_ms = uv_now(&state->loop);
                    return 0;
                }
                return -1;
            }
            auto found = std::find_if(state->connections.begin(), state->connections.end(), [socket](const auto& item) { return item.socket == socket; });
            if (found == state->connections.end()) return 0;
            auto* connection = &*found;
            if (reason == LWS_CALLBACK_CLOSED) {
                Cancel_Connection_Work(state, connection);
                *connection = {};
                return 0;
            }
            if (reason == LWS_CALLBACK_RECEIVE) {
                if (connection->expired) return Close(socket, LWS_CLOSE_STATUS_POLICY_VIOLATION);
                const uint64_t now = uv_now(&state->loop);
                if (now - connection->rate_window_ms >= 1000) {
                    connection->rate_window_ms = now;
                    connection->callbacks = connection->messages = 0;
                }
                if (lws_frame_is_binary(socket)) return Close(socket, LWS_CLOSE_STATUS_UNACCEPTABLE_OPCODE);
                if (++connection->callbacks > 128 || connection->session.close_after_output) return Close(socket, LWS_CLOSE_STATUS_POLICY_VIOLATION);
                if (!connection->assembling) {
                    if (!lws_is_first_fragment(socket)) return Close(socket, LWS_CLOSE_STATUS_POLICY_VIOLATION);
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
                const auto position = Event_Position(&state->events);
                if (connection->expired || position.stopped || (connection->session.authenticated && connection->session.auth_epoch != position.epoch)) return Close(socket, LWS_CLOSE_STATUS_POLICY_VIOLATION);
                if (connection->output.empty()) return connection->session.close_after_output ? Close(socket, LWS_CLOSE_STATUS_POLICY_VIOLATION) : 0;
                const auto& output = connection->output.front();
                if (output.read_epoch != 0 && output.read_epoch != position.read_epoch) return Close(socket, LWS_CLOSE_STATUS_POLICY_VIOLATION);
                const auto& message = output.json;
                const size_t count = std::min(size_t(4096), message.size() - connection->output_offset);
                unsigned char buffer[LWS_PRE + 4096];
                std::memcpy(buffer + LWS_PRE, message.data() + connection->output_offset, count);
                const bool final = connection->output_offset + count == message.size();
                const int written = lws_write(socket, buffer + LWS_PRE, count, lws_write_ws_flags(LWS_WRITE_TEXT, connection->output_offset == 0, final));
                if (written < 0 || (size_t)written < count) return Close(socket, LWS_CLOSE_STATUS_POLICY_VIOLATION);
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
            s_read_request expired;
            while (Expire_Read(&state->reads, &expired)) {
                Push_Response(&state->events, expired.connection, expired.epoch, 0, Error_Response(expired.encoded_id, -32006, "Busy"), expired.selection, expired.encoded_id);
            }
            s_operation_request expired_operation;
            if (Cancel_Operation(&state->operations, 0, 0, &expired_operation, false, true))
                Push_Response(&state->events, expired_operation.request.connection, expired_operation.request.epoch, 0, Error_Response(expired_operation.request.encoded_id, -32015, "Expired before dispatch"), 0, expired_operation.request.encoded_id);
            Deliver(state);
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
            if (state->stopping) {
                uv_stop(&state->loop);
                return;
            }
            Deliver(state);
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
                initialized = uv_timer_start(&state->timer, Check_Connections, 50, 50) == 0;
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
        Stop_Events(&state->events);
        Cancel_Reads(&state->reads, 0, true);
        s_operation_request cancelled;
        Cancel_Operation(&state->operations, 0, 0, &cancelled, true);
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

    bool Publish_Event(s_service* service, uint8_t kind, std::string_view battle_id, std::string_view unit_id, std::string detail) {
        if (service == nullptr || service->state == nullptr) return false;
        auto* state = (s_service_state*)service->state;
        const bool queued = Push_Event(&state->events, kind, battle_id, unit_id, std::move(detail));
        uv_async_send(&state->wake);
        return queued;
    }

    void Revoke_Event_Delivery(s_service* service) {
        if (service == nullptr || service->state == nullptr) return;
        auto* state = (s_service_state*)service->state;
        Set_Boarding_Report(&state->operations, {});
        Revoke_Events(&state->events);
        Cancel_Reads(&state->reads);
        s_operation_request cancelled;
        Cancel_Operation(&state->operations, 0, 0, &cancelled);
        uv_async_send(&state->wake);
    }

    void Invalidate_Read_Delivery(s_service* service) {
        if (service == nullptr || service->state == nullptr) return;
        auto* state = (s_service_state*)service->state;
        Invalidate_Read_Output(&state->events);
        s_operation_request cancelled;
        if (Cancel_Operation(&state->operations, 0, 0, &cancelled))
            Push_Response(&state->events, cancelled.request.connection, cancelled.request.epoch, 0, Error_Response(cancelled.request.encoded_id, -32004, "Unavailable"), 0, cancelled.request.encoded_id);
        uv_async_send(&state->wake);
    }

    void Execute_Reads(s_service* service, bool safe, bool observed, t_execute_read execute, void* context) {
        if (service == nullptr || service->state == nullptr || execute == nullptr || (observed && !safe)) return;
        auto* state = (s_service_state*)service->state;
        for (unsigned count = 0; count < 2; ++count) {
            s_read_request request;
            if (!Take_Read(&state->reads, &request)) break;
            const auto position = Event_Position(&state->events);
            bool dependent = false;
            std::string response;
            if (request.epoch != position.epoch || position.stopped) {
                Finish_Read(&state->reads, request.ticket);
                continue;
            }
            if (Monotonic_Ms() >= request.deadline_ms) {
                response = Error_Response(request.encoded_id, -32006, "Busy");
            } else {
                response = execute(&request, context, &dependent);
            }
            if (response.size() > MAX_RESPONSE_BYTES) {
                Error_Response(request.encoded_id, -32005, "Limit exceeded").swap(response);
                dependent = false;
            }
            if (Monotonic_Ms() >= request.deadline_ms) {
                Error_Response(request.encoded_id, -32006, "Busy").swap(response);
                dependent = false;
            }
            if (!Finish_Read(&state->reads, request.ticket)) continue;
            Push_Response(&state->events, request.connection, request.epoch, dependent ? position.read_epoch : 0, std::move(response), request.selection, request.encoded_id);
            uv_async_send(&state->wake);
        }
    }

    bool Take_Operation(s_service* service, s_operation_request* request, bool report_only) {
        if (service == nullptr || service->state == nullptr) return false;
        auto* state = (s_service_state*)service->state;
        if (!Take_Operation(&state->operations, request, report_only)) return false;
        const auto position = Event_Position(&state->events);
        if (position.stopped || request->request.epoch != position.epoch) {
            Finish_Operation(&state->operations, request->child);
            return false;
        }
        return true;
    }

    void Complete_Operation(s_service* service, const s_operation_request* request, int error, std::string boarding) {
        auto* state = (s_service_state*)service->state;
        std::string response;
        if (error != 0) {
            const char* message = "Unavailable";
            switch (error) {
                case -32009: message = "No native control"; break;
                case -32010: message = "Decision conflict"; break;
                case -32011: message = "Illegal operation"; break;
                case -32016: message = "Local attention required"; break;
                default: break;
            }
            response = Error_Response(request->request.encoded_id, error, message);
        } else
            response = "{\"jsonrpc\":\"2.0\",\"id\":" + request->request.encoded_id +
                ",\"result\":{\"operationSeq\":" + std::to_string(request->sequence) + ",\"state\":\"applied\"" +
                (boarding.empty() ? "" : ",\"boarding\":" + boarding) + "}}";
        // Completion is an outcome, not a read: retirement/unsubscribe cannot turn it into a false refusal.
        Push_Response(&state->events, request->request.connection, request->request.epoch, 0, std::move(response), 0, request->request.encoded_id);
        Finish_Operation(&state->operations, request->child);
        uv_async_send(&state->wake);
    }
    void Close_Boarding_Report(s_service* service) {
        auto* state = (s_service_state*)service->state;
        Set_Boarding_Report(&state->operations, {});
        s_operation_request cancelled;
        // Only queued acknowledgement work can coexist with an executing parent.
        if (Cancel_Operation(&state->operations, 0, 0, &cancelled)) {
            Push_Response(&state->events, cancelled.request.connection, cancelled.request.epoch, 0, Error_Response(cancelled.request.encoded_id, -32004, "Unavailable"));
            uv_async_send(&state->wake);
        }
    }
    void Open_Boarding_Report(s_service* service, std::string report_id) {
        auto* state = (s_service_state*)service->state;
        Close_Boarding_Report(service);
        Set_Boarding_Report(&state->operations, std::move(report_id));
    }
} // namespace rpc
