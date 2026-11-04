#include "rpc_tactical.h"
#include "rpc_protocol.h"
#include "rpc_tactical_reads.h"
#include "game/tactical_events.h"

#include <uv.h>
#include <algorithm>

namespace rpc {
    namespace {
        void Revoke(s_tactical_forwarder* forwarder) noexcept {
            forwarder->viewer = -1;
            forwarder->battle_id[0] = '\0';
            forwarder->references.clear();
            forwarder->missile_references.clear();
            forwarder->selected.clear();
            forwarder->decision_id.clear();
            forwarder->report_id.clear();
            Revoke_Event_Delivery(forwarder->service);
        }

        bool Take(tactical_control::s_operation* operation, void* context) {
            auto* forwarder = (s_tactical_forwarder*)context;
            s_operation_request request;
            if (!Take_Operation(forwarder->service, &request)) return false;
            const auto reject = [&](int error) { Complete_Operation(forwarder->service, &request, error); return false; };
            if (forwarder->retiring || request.request.battle_id != forwarder->battle_id ||
                forwarder->reader.observer == nullptr || forwarder->reader.observer() != forwarder->viewer ||
                forwarder->authorize == nullptr) return reject(-32004);
            const auto resolve = [&](const std::string& token, bool owned) -> int16_t {
                for (const auto& reference : forwarder->references)
                    if (reference.token.data() == token && forwarder->authorize(reference.slot, owned)) return reference.slot;
                return -1;
            };
            operation->actor = resolve(request.actor_id, true);
            if (operation->actor < 0) return reject(-32004);
            if (request.method == OP_SELECT || request.method == OP_FIRE || request.method == OP_BOARD) {
                if (request.missile_target) {
                    operation->target = -1;
                    for (const auto& reference : forwarder->missile_references)
                        if (reference.token.data() == request.target_id && forwarder->authorize_missile != nullptr && forwarder->authorize_missile(reference.slot)) operation->target = reference.slot;
                } else
                    operation->target = resolve(request.target_id, request.method == OP_SELECT);
                if (operation->target < 0) return reject(-32004);
            }
            if (forwarder->decision_id != request.decision_id || forwarder->decision_generation != tactical_control::Generation()) return reject(-32010);
            switch (request.method) {
                case OP_SELECT: operation->kind = tactical_control::SELECT; break;
                case OP_MOVE: operation->kind = tactical_control::MOVE; break;
                case OP_FACE: operation->kind = tactical_control::FACE; break;
                case OP_FIRE: operation->kind = tactical_control::FIRE; break;
                case OP_DONE: operation->kind = tactical_control::DONE; break;
                case OP_FIRE_SELECTION: operation->kind = tactical_control::FIRE_SELECTION; break;
                case OP_BOARD: operation->kind = tactical_control::BOARD; break;
                case OP_RETREAT: operation->kind = tactical_control::RETREAT; break;
                default: return reject(-32004);
            }
            operation->x = request.x;
            operation->y = request.y;
            operation->missile_target = request.missile_target;
            operation->bank_mask = request.bank_mask;
            operation->boarding_mode = request.boarding_mode;
            operation->marines = request.marines;
            for (unsigned i = 0; i < 8; ++i) operation->bank_states[i] = request.bank_states[i];
            forwarder->executing = std::move(request);
            return true;
        }
        void Complete(tactical_control::e_result result, void* context) {
            auto* forwarder = (s_tactical_forwarder*)context;
            const int codes[] = {0, -32004, -32009, -32010, -32011, -32016};
            Complete_Operation(forwarder->service, &forwarder->executing, codes[result], result == tactical_control::APPLIED && forwarder->executing.method == OP_BOARD ? Boarding_Result_Json(tactical_events::Boarding_Result()) : std::string());
            forwarder->executing = {};
        }

        bool Take_Report(uint64_t generation, void* context) {
            auto* f = (s_tactical_forwarder*)context;
            s_operation_request request;
            if (!Take_Operation(f->service, &request, true)) return false;
            if (!tactical_events::Boarding_Report_Active() || generation != f->report_generation ||
                f->reader.observer == nullptr || f->reader.observer() != f->viewer || f->retiring ||
                request.request.battle_id != f->battle_id || request.report_id != f->report_id || f->report_id.empty()) {
                Complete_Operation(f->service, &request, -32004);
                return false;
            }
            f->acknowledgement = std::move(request);
            return true;
        }
        void Complete_Report(void* context) {
            auto* f = (s_tactical_forwarder*)context;
            if (f->acknowledgement.method == OP_NONE) return;
            Complete_Operation(f->service, &f->acknowledgement);
            f->acknowledgement = {};
        }

        void Forward(const tactical_events::s_event* event, void* context) noexcept {
            auto* forwarder = (s_tactical_forwarder*)context;
            if ((event->kind == tactical_events::VIEW_INVALIDATED || event->kind == tactical_events::UNIT_SELECTED ||
                    event->kind == tactical_events::ATTENTION_REQUIRED || event->kind == tactical_events::ATTENTION_CLEARED) &&
                forwarder->reader.observer != nullptr && forwarder->reader.observer() != forwarder->viewer) {
                Revoke(forwarder);
                return;
            }
            if (event->kind == tactical_events::REVOKED) {
                Revoke(forwarder);
            } else if (event->kind == tactical_events::SERVICE_READS) {
                const bool active = forwarder->battle_id[0] != '\0' && !forwarder->retiring;
                if (active && forwarder->reader.observer != nullptr && forwarder->reader.observer() != forwarder->viewer) {
                    Revoke(forwarder);
                    return;
                }
                Execute_Reads(forwarder->service, event->safe, active, Execute_Tactical_Read, forwarder);
                if ((event->safe || !active) && (!active || tactical_control::Actor() < 0)) {
                    s_operation_request request;
                    if (Take_Operation(forwarder->service, &request))
                        Complete_Operation(forwarder->service, &request, active ? -32009 : -32004);
                }
            } else if (event->kind == tactical_events::RETREAT_RESOLVED) {
                if (forwarder->reader.observer == nullptr || forwarder->reader.observer() != forwarder->viewer) {
                    Revoke(forwarder);
                    return;
                }
                // Outcome identity is the previously disclosed lifetime, never a new action handle.
                for (const auto& reference : forwarder->references) {
                    if (reference.slot != event->unit) continue;
                    const char* outcomes[] = {"escaped", "destroyed", "cancelled", "interrupted", "interrupted"};
                    std::string detail = "\"outcome\":\"" + std::string(outcomes[event->retreat_outcome]) + "\"";
                    if (event->retreat_outcome == tactical_events::CAPTURED) detail += ",\"reason\":\"captured\"";
                    if (event->retreat_outcome == tactical_events::BATTLE_ENDED) detail += ",\"reason\":\"battleEnded\"";
                    Publish_Event(forwarder->service, EVENT_COMBAT_RETREAT, forwarder->battle_id, reference.token.data(), std::move(detail));
                    break;
                }
            } else if (event->kind == tactical_events::BOARDING_RESOLVED) {
                if (forwarder->reader.observer == nullptr || forwarder->reader.observer() != forwarder->viewer) {
                    Revoke(forwarder);
                    return;
                }
                forwarder->report_id.clear();
                forwarder->report_generation = tactical_events::Boarding_Generation();
                if (tactical_events::Boarding_Report_Active()) {
                    unsigned char bytes[16];
                    if (uv_random(nullptr, nullptr, bytes, sizeof(bytes), 0, nullptr) != 0) {
                        Revoke(forwarder);
                        return;
                    }
                    constexpr char hex[] = "0123456789abcdef";
                    for (auto b : bytes) {
                        forwarder->report_id += hex[b >> 4];
                        forwarder->report_id += hex[b & 15];
                    }
                }
                Open_Boarding_Report(forwarder->service, forwarder->report_id);
                Publish_Event(forwarder->service, EVENT_COMBAT_BOARDING, forwarder->battle_id, {}, Boarding_Result_Json(tactical_events::Boarding_Result(), forwarder->report_id));
            } else if (event->kind == tactical_events::BOARDING_REPORT_CLOSED) {
                forwarder->report_id.clear();
                Close_Boarding_Report(forwarder->service);
            } else if (event->kind == tactical_events::UNIT_RETIRED || event->kind == tactical_events::MISSILE_RETIRED) {
                auto& references = event->kind == tactical_events::UNIT_RETIRED ? forwarder->references : forwarder->missile_references;
                const auto end = std::remove_if(references.begin(), references.end(), [event](const auto& reference) { return reference.slot == event->unit; });
                if (end != references.end()) {
                    references.erase(end, references.end());
                    Invalidate_Read_Delivery(forwarder->service);
                }
            } else if (event->kind == tactical_events::RETIRING) {
                forwarder->retiring = true;
                forwarder->report_id.clear();
                Close_Boarding_Report(forwarder->service);
                forwarder->references.clear();
                forwarder->missile_references.clear();
                forwarder->selected.clear();
                Invalidate_Read_Delivery(forwarder->service);
                Execute_Reads(forwarder->service, false, false, Execute_Tactical_Read, forwarder);
            } else if (event->kind == tactical_events::OPENED) {
                if (forwarder->battle_id[0] != '\0' || (forwarder->viewer >= 0 && forwarder->viewer != event->viewer)) Revoke(forwarder);
                unsigned char bytes[16];
                if (uv_random(nullptr, nullptr, bytes, sizeof(bytes), 0, nullptr) != 0) {
                    Revoke(forwarder);
                    return;
                }
                constexpr char hex[] = "0123456789abcdef";
                for (size_t i = 0; i < sizeof(bytes); ++i) {
                    forwarder->battle_id[i * 2] = hex[bytes[i] >> 4];
                    forwarder->battle_id[i * 2 + 1] = hex[bytes[i] & 15];
                }
                forwarder->battle_id[32] = '\0';
                forwarder->viewer = event->viewer;
                forwarder->retiring = false;
                Publish_Event(forwarder->service, EVENT_COMBAT_OPENED, forwarder->battle_id);
            } else if (forwarder->battle_id[0] != '\0') {
                if (forwarder->viewer != event->viewer) {
                    Revoke(forwarder);
                    return;
                }
                if (event->kind == tactical_events::VIEW_INVALIDATED) {
                    Publish_Event(forwarder->service, EVENT_COMBAT_INVALIDATED, forwarder->battle_id);
                } else if (event->kind == tactical_events::ATTENTION_REQUIRED || event->kind == tactical_events::ATTENTION_CLEARED) {
                    Publish_Event(forwarder->service, event->kind == tactical_events::ATTENTION_REQUIRED ? EVENT_COMBAT_ATTENTION : EVENT_COMBAT_ATTENTION_CLEARED, forwarder->battle_id);
                } else if (event->kind == tactical_events::UNIT_SELECTED) {
                    s_unit unit;
                    std::string selected;
                    if (forwarder->reader.unit != nullptr && forwarder->reader.unit(event->unit, &unit)) selected = Tactical_Unit_Token(forwarder, &unit);
                    if (selected == forwarder->selected) return;
                    forwarder->selected = selected;
                    Publish_Event(forwarder->service, EVENT_COMBAT_SELECTED, forwarder->battle_id, selected);
                    Publish_Event(forwarder->service, EVENT_COMBAT_INVALIDATED, forwarder->battle_id);
                } else if (event->kind == tactical_events::CLOSED) {
                    forwarder->report_id.clear();
                    Close_Boarding_Report(forwarder->service);
                    if (!forwarder->retiring) Invalidate_Read_Delivery(forwarder->service);
                    Publish_Event(forwarder->service, EVENT_COMBAT_CLOSED, forwarder->battle_id);
                    forwarder->battle_id[0] = '\0';
                    forwarder->references.clear();
                    forwarder->missile_references.clear();
                    forwarder->selected.clear();
                }
            }
        }
    } // namespace

    bool Subscribe_Tactical_Events(s_tactical_forwarder* forwarder, s_service* service) {
        if (forwarder == nullptr || service == nullptr || service->state == nullptr || forwarder->subscription != 0) return false;
        forwarder->service = service;
        forwarder->subscription = tactical_events::Subscribe(Forward, forwarder);
        if (forwarder->subscription != 0 && tactical_control::Attach(Take, Complete, forwarder) &&
            tactical_control::Attach_Report(Take_Report, Complete_Report, forwarder)) return true;
        tactical_control::Detach(forwarder);
        if (forwarder->subscription != 0) tactical_events::Unsubscribe(forwarder->subscription);
        *forwarder = {};
        return false;
    }

    void Unsubscribe_Tactical_Events(s_tactical_forwarder* forwarder) {
        if (forwarder == nullptr || forwarder->subscription == 0) return;
        tactical_events::Unsubscribe(forwarder->subscription);
        tactical_control::Detach(forwarder);
        Revoke(forwarder);
        *forwarder = {};
    }
} // namespace rpc
