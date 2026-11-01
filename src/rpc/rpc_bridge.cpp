#include "rpc_bridge.h"

#include <algorithm>
#include <iterator>
#include <limits>
#include <utility>

namespace rpc {
    namespace {
        constexpr size_t MAX_WORK = 64;
        constexpr size_t MAX_CLIENT_WORK = 16;
        constexpr size_t MAX_REQUEST_BYTES = 65536;
        constexpr size_t MAX_RESULT_BYTES = 1048576;
        constexpr size_t MAX_READY_BYTES = 4194304;

        bool Current(const s_bridge* bridge, uint64_t client, uint64_t epoch) {
            return !bridge->stopped && epoch == bridge->epoch && client != 0 &&
                std::find(std::begin(bridge->clients), std::end(bridge->clients), client) != std::end(bridge->clients);
        }

        void Cancel(s_bridge* bridge, uint64_t client) {
            for (size_t index = 0; index < 4; ++index) {
                if (bridge->clients[index] == client) {
                    bridge->clients[index] = 0;
                    bridge->authorized[index] = false;
                }
            }
            std::erase_if(bridge->tickets, [client](const s_bridge_ticket& ticket) { return ticket.client == client; });
            std::erase_if(bridge->deliveries, [client](const s_context_delivery& item) { return item.client == client; });
            std::erase_if(bridge->pending, [client](const s_work& work) { return work.client == client; });
            std::erase_if(bridge->ready, [bridge, client](const s_publication& result) {
                if (result.client != client) {
                    return false;
                }
                bridge->ready_bytes -= result.json.size();
                return true;
            });
        }

        void Clear(s_bridge* bridge) {
            std::fill(std::begin(bridge->clients), std::end(bridge->clients), 0);
            std::fill(std::begin(bridge->authorized), std::end(bridge->authorized), false);
            bridge->tickets.clear();
            bridge->pending.clear();
            bridge->ready.clear();
            bridge->ready_bytes = 0;
            bridge->deliveries.clear();
        }

        bool Complete(s_bridge* bridge, const s_work* work, const std::string* json, uint64_t context_epoch = 0) {
            if (!Current(bridge, work->client, work->epoch)) {
                return false;
            }
            auto ticket = std::find_if(bridge->tickets.begin(), bridge->tickets.end(), [work](const s_bridge_ticket& item) {
                return item.client == work->client && item.ticket == work->ticket && item.started;
            });
            if (ticket == bridge->tickets.end()) {
                return false;
            }
            const std::string revoked = context_epoch != 0 ? Error_Response(&work->request, RPC_OBJECT_UNAVAILABLE) : std::string();
            const bool stale = context_epoch != 0 && context_epoch != bridge->context_epoch;
            const std::string& output = stale ? revoked : *json;
            if (output.size() > MAX_RESULT_BYTES || output.size() > MAX_READY_BYTES - bridge->ready_bytes) {
                // A result cannot be silently lost: the service observes revocation and closes this client.
                Cancel(bridge, work->client);
                return false;
            }
            if (context_epoch != 0 && !stale) {
                if (bridge->deliveries.size() >= MAX_WORK) {
                    Cancel(bridge, work->client);
                    return false;
                }
                bridge->deliveries.push_back({work->client, work->ticket});
            }
            bridge->ready.push_back({work->client, work->epoch, work->ticket, output, {}, stale ? 0 : context_epoch, revoked});
            bridge->ready_bytes += bridge->ready.back().json.size();
            bridge->tickets.erase(ticket);
            return true;
        }
    } // namespace

    bool Register_Client(s_bridge* bridge, uint64_t client, uint64_t* epoch) {
        if (bridge == nullptr || epoch == nullptr || client == 0) {
            return false;
        }
        std::lock_guard lock(bridge->mutex);
        if (bridge->stopped || std::find(std::begin(bridge->clients), std::end(bridge->clients), client) != std::end(bridge->clients)) {
            return false;
        }
        for (uint64_t& slot : bridge->clients) {
            if (slot == 0) {
                slot = client;
                *epoch = bridge->epoch;
                return true;
            }
        }
        return false;
    }

    bool Client_Is_Current(s_bridge* bridge, uint64_t client, uint64_t epoch) {
        if (bridge == nullptr) {
            return false;
        }
        std::lock_guard lock(bridge->mutex);
        return Current(bridge, client, epoch);
    }

    void Cancel_Client(s_bridge* bridge, uint64_t client) {
        if (bridge != nullptr) {
            std::lock_guard lock(bridge->mutex);
            Cancel(bridge, client);
        }
    }

    bool Authorize_Client(s_bridge* bridge, uint64_t client, uint64_t epoch) {
        if (bridge == nullptr) return false;
        std::lock_guard lock(bridge->mutex);
        if (!Current(bridge, client, epoch)) return false;
        for (size_t index = 0; index < 4; ++index) {
            if (bridge->clients[index] == client) bridge->authorized[index] = true;
        }
        return true;
    }

    void Get_Authorized_Clients(s_bridge* bridge, std::vector<s_client_identity>* clients) {
        if (clients == nullptr) return;
        clients->clear();
        if (bridge == nullptr) return;
        std::lock_guard lock(bridge->mutex);
        if (bridge->stopped) return;
        for (size_t index = 0; index < 4; ++index) {
            if (bridge->authorized[index] && bridge->clients[index] != 0) clients->push_back({bridge->clients[index], bridge->epoch});
        }
    }

    bool Has_Clients(s_bridge* bridge) {
        if (bridge == nullptr) return false;
        std::lock_guard lock(bridge->mutex);
        return !bridge->stopped && std::any_of(std::begin(bridge->clients), std::end(bridge->clients), [](uint64_t client) { return client != 0; });
    }

    void Revoke_Bridge(s_bridge* bridge) {
        if (bridge != nullptr) {
            std::lock_guard lock(bridge->mutex);
            if (bridge->epoch == std::numeric_limits<uint64_t>::max()) {
                bridge->stopped = true;
            } else {
                ++bridge->epoch;
            }
            Clear(bridge);
        }
    }

    uint64_t Bridge_Context_Epoch(s_bridge* bridge) {
        if (bridge == nullptr) return 0;
        std::lock_guard lock(bridge->mutex);
        return bridge->context_epoch;
    }

    bool Context_Response_Pending(s_bridge* bridge, uint64_t client, uint64_t ticket) {
        if (bridge == nullptr || ticket == 0) return false;
        std::lock_guard lock(bridge->mutex);
        return std::any_of(bridge->deliveries.begin(), bridge->deliveries.end(), [=](const s_context_delivery& item) { return item.client == client && item.ticket == ticket; });
    }

    void Acknowledge_Response(s_bridge* bridge, uint64_t client, uint64_t ticket) {
        if (bridge == nullptr || ticket == 0) return;
        std::lock_guard lock(bridge->mutex);
        std::erase_if(bridge->deliveries, [=](const s_context_delivery& item) { return item.client == client && item.ticket == ticket; });
    }

    void Revoke_Bridge_Context(s_bridge* bridge) {
        if (bridge == nullptr) return;
        std::lock_guard lock(bridge->mutex);
        if (bridge->context_epoch == std::numeric_limits<uint64_t>::max()) {
            bridge->stopped = true;
            Clear(bridge);
            return;
        }
        ++bridge->context_epoch;
        bridge->deliveries.clear();
        for (auto item = bridge->ready.begin(); item != bridge->ready.end();) {
            if (item->context_epoch == 0) {
                ++item;
                continue;
            }
            bridge->ready_bytes -= item->json.size();
            if (item->revoked_json.empty()) {
                item = bridge->ready.erase(item);
            } else {
                item->json = std::move(item->revoked_json);
                item->context_epoch = 0;
                item->state_key.clear();
                bridge->ready_bytes += item->json.size();
                ++item;
            }
        }
        if (bridge->ready_bytes > MAX_READY_BYTES) Clear(bridge);
    }

    bool Has_Pending_Work(s_bridge* bridge) {
        if (bridge == nullptr) return false;
        std::lock_guard lock(bridge->mutex);
        return !bridge->stopped && !bridge->pending.empty();
    }

    bool Publish_Event(s_bridge* bridge, uint64_t client, uint64_t epoch, const std::string* json, const std::string* state_key, uint64_t context_epoch) {
        if (bridge == nullptr || json == nullptr) return false;
        std::lock_guard lock(bridge->mutex);
        if (!Current(bridge, client, epoch)) return false;
        if (context_epoch != 0 && context_epoch != bridge->context_epoch) return false;
        if (state_key != nullptr && !state_key->empty()) {
            std::erase_if(bridge->ready, [&](const s_publication& item) {
                if (item.client != client || item.epoch != epoch || item.state_key != *state_key) return false;
                bridge->ready_bytes -= item.json.size();
                return true;
            });
        }
        const size_t client_outstanding = std::count_if(bridge->ready.begin(), bridge->ready.end(), [client](const s_publication& item) { return item.client == client; }) +
            std::count_if(bridge->tickets.begin(), bridge->tickets.end(), [client](const s_bridge_ticket& item) { return item.client == client; });
        if (json->empty() || json->size() > MAX_RESULT_BYTES || json->size() > MAX_READY_BYTES - bridge->ready_bytes ||
            bridge->tickets.size() + bridge->ready.size() >= MAX_WORK ||
            client_outstanding >= MAX_CLIENT_WORK) {
            Cancel(bridge, client);
            return false;
        }
        bridge->ready.push_back({client, epoch, 0, *json, state_key != nullptr ? *state_key : std::string(), context_epoch});
        bridge->ready_bytes += json->size();
        return true;
    }

    void Stop_Bridge(s_bridge* bridge) {
        if (bridge != nullptr) {
            std::lock_guard lock(bridge->mutex);
            bridge->stopped = true;
            Clear(bridge);
        }
    }

    int32_t Submit_Work(s_bridge* bridge, uint64_t client, uint64_t epoch, const s_request* request, uint64_t deadline_ms) {
        if (bridge == nullptr || request == nullptr || request->notification) {
            return RPC_INVALID_REQUEST;
        }
        if (request->method.size() + request->params_json.size() + request->id_json.size() > MAX_REQUEST_BYTES) {
            return RPC_LIMIT_EXCEEDED;
        }
        std::lock_guard lock(bridge->mutex);
        if (!Current(bridge, client, epoch)) {
            return RPC_UNAUTHORIZED;
        }
        const size_t outstanding = bridge->tickets.size() + bridge->ready.size();
        const size_t client_outstanding = std::count_if(bridge->tickets.begin(), bridge->tickets.end(), [client](const s_bridge_ticket& item) { return item.client == client; }) +
            std::count_if(bridge->ready.begin(), bridge->ready.end(), [client](const s_publication& item) { return item.client == client; });
        if (outstanding >= MAX_WORK || client_outstanding >= MAX_CLIENT_WORK ||
            bridge->next_ticket == std::numeric_limits<uint64_t>::max()) {
            return RPC_LIMIT_EXCEEDED;
        }
        const uint64_t ticket = ++bridge->next_ticket;
        bridge->pending.push_back({client, epoch, ticket, deadline_ms, *request});
        bridge->tickets.push_back({client, ticket, false});
        return 0;
    }

    bool Take_Work(s_bridge* bridge, uint64_t now_ms, s_work* work) {
        if (bridge == nullptr || work == nullptr) {
            return false;
        }
        std::lock_guard lock(bridge->mutex);
        while (!bridge->stopped && !bridge->pending.empty()) {
            s_work candidate = std::move(bridge->pending.front());
            bridge->pending.pop_front();
            auto ticket = std::find_if(bridge->tickets.begin(), bridge->tickets.end(), [&candidate](const s_bridge_ticket& item) {
                return item.ticket == candidate.ticket;
            });
            if (ticket == bridge->tickets.end() || !Current(bridge, candidate.client, candidate.epoch)) {
                continue;
            }
            ticket->started = true;
            if (now_ms >= candidate.deadline_ms) {
                const std::string error = Error_Response(&candidate.request, RPC_REQUEST_EXPIRED);
                Complete(bridge, &candidate, &error);
                continue;
            }
            *work = std::move(candidate);
            return true;
        }
        return false;
    }

    bool Complete_Work(s_bridge* bridge, const s_work* work, const std::string* json, uint64_t context_epoch) {
        if (bridge == nullptr || work == nullptr || json == nullptr) {
            return false;
        }
        std::lock_guard lock(bridge->mutex);
        return Complete(bridge, work, json, context_epoch);
    }

    void Expire_Work(s_bridge* bridge, uint64_t now_ms) {
        if (bridge == nullptr) {
            return;
        }
        std::lock_guard lock(bridge->mutex);
        while (!bridge->stopped) {
            auto expired = std::find_if(bridge->pending.begin(), bridge->pending.end(), [now_ms](const s_work& work) {
                return now_ms >= work.deadline_ms;
            });
            if (expired == bridge->pending.end()) {
                break;
            }
            s_work work = std::move(*expired);
            bridge->pending.erase(expired);
            auto ticket = std::find_if(bridge->tickets.begin(), bridge->tickets.end(), [&work](const s_bridge_ticket& item) {
                return item.ticket == work.ticket;
            });
            if (ticket != bridge->tickets.end()) {
                ticket->started = true;
                const std::string error = Error_Response(&work.request, RPC_REQUEST_EXPIRED);
                Complete(bridge, &work, &error);
            }
        }
    }

    bool Take_Publication(s_bridge* bridge, s_publication* publication) {
        if (bridge == nullptr || publication == nullptr) {
            return false;
        }
        std::lock_guard lock(bridge->mutex);
        if (bridge->stopped || bridge->ready.empty()) {
            return false;
        }
        *publication = std::move(bridge->ready.front());
        bridge->ready_bytes -= publication->json.size();
        bridge->ready.pop_front();
        return true;
    }
} // namespace rpc
