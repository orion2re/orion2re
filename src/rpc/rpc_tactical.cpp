#include "pch.h"
#include "rpc_tactical.h"
#include "rpc_protocol.h"
#include "rpc_tactical_reads.h"
#include "game/combat.h"

#include <uv.h>

namespace rpc {
    namespace {
        void Invalidate_Inspection(uint64_t generation, void* context) {
            Set_Decision_Generation((s_service*)context, generation);
        }
        void Revoke(s_tactical_forwarder* forwarder) noexcept {
            forwarder->viewer = -1;
            forwarder->battle_id[0] = '\0';
            forwarder->references.clear();
            forwarder->missile_references.clear();
            forwarder->selected.clear();
            forwarder->decision_id.clear();
            forwarder->manual_id.clear();
            forwarder->report_id.clear();
            Revoke_Event_Delivery(forwarder->service);
        }

        bool Reject_Operation(s_tactical_forwarder* forwarder, const s_operation_request* request, int error) {
            Complete_Operation(forwarder->service, request, error);
            return false;
        }

        int16_t Resolve_Unit(const s_tactical_forwarder* forwarder, std::string_view token, bool owned) {
            for (const s_tactical_forwarder::s_reference& reference : forwarder->references)
                if (reference.token.data() == token && forwarder->authorize(reference.slot, owned)) return reference.slot;
            return -1;
        }

        bool Take(COMBAT1::s_operation* operation, COMBAT1::e_result* result, void* context) {
            auto* forwarder = (s_tactical_forwarder*)context;
            s_operation_request request;
            const bool input = forwarder->input != nullptr && forwarder->input() != nullptr;
            if (!Take_Operation(forwarder->service, &request, input)) return false;
            if (forwarder->retiring || request.request.battle_id != forwarder->battle_id ||
                forwarder->reader.observer == nullptr || forwarder->reader.observer() != forwarder->viewer ||
                forwarder->authorize == nullptr) return Reject_Operation(forwarder, &request, -32004);
            if (input) {
                const bool manual = request.method == OP_MANUAL && forwarder->input()->kind == CMBTDRW1::RETURN_TO_MANUAL;
                if (forwarder->input_retired || request.report_id.empty() || request.report_id != (manual ? forwarder->manual_id : forwarder->decision_id) ||
                    (!manual && forwarder->decision_generation != Control_Generation())) return Reject_Operation(forwarder, &request, -32004);
                switch (request.method) {
                    case OP_CONFIRMATION: operation->kind = COMBAT1::ANSWER_CONFIRMATION; break;
                    case OP_BOARDING_CHOICE: operation->kind = COMBAT1::ANSWER_BOARDING; break;
                    case OP_INFORMATION: operation->kind = COMBAT1::ACK_INFORMATION; break;
                    case OP_MANUAL: operation->kind = COMBAT1::MANUAL_CONTROL; break;
                    default: return Reject_Operation(forwarder, &request, -32004);
                }
                operation->value = request.value;
                operation->marines = request.marines;
                operation->boarding_mode = request.boarding_mode;
                forwarder->acknowledgement = std::move(request);
                *result = forwarder->validate != nullptr ? forwarder->validate(operation) : COMBAT1::NO_CONTROL;
                return true;
            }
            operation->actor = Resolve_Unit(forwarder, request.actor_id, true);
            if (operation->actor < 0) return Reject_Operation(forwarder, &request, -32004);
            if (request.method == OP_SELECT || request.method == OP_FIRE || request.method == OP_BOARD || request.method == OP_REMOVE_STASIS) {
                if (request.missile_target) {
                    operation->target = -1;
                    for (const auto& reference : forwarder->missile_references)
                        if (reference.token.data() == request.target_id && forwarder->authorize_missile != nullptr && forwarder->authorize_missile(reference.slot)) operation->target = reference.slot;
                } else
                    operation->target = Resolve_Unit(forwarder, request.target_id, request.method == OP_SELECT);
                if (operation->target < 0) return Reject_Operation(forwarder, &request, -32004);
            }
            if (forwarder->decision_id != request.decision_id || forwarder->decision_generation != Control_Generation()) return Reject_Operation(forwarder, &request, -32010);
            switch (request.method) {
                case OP_SELECT: operation->kind = COMBAT1::SELECT; break;
                case OP_MOVE: operation->kind = COMBAT1::MOVE; break;
                case OP_FACE: operation->kind = COMBAT1::FACE; break;
                case OP_FIRE: operation->kind = COMBAT1::FIRE; break;
                case OP_DONE: operation->kind = COMBAT1::DONE; break;
                case OP_FIRE_SELECTION: operation->kind = COMBAT1::FIRE_SELECTION; break;
                case OP_BOARD: operation->kind = COMBAT1::BOARD; break;
                case OP_RETREAT: operation->kind = COMBAT1::RETREAT; break;
                case OP_WAIT: operation->kind = COMBAT1::WAIT; break;
                case OP_REMOVE_STASIS: operation->kind = COMBAT1::REMOVE_STASIS; break;
                case OP_SELF_DESTRUCT: operation->kind = COMBAT1::SELF_DESTRUCT; break;
                case OP_SIDE_WAIT: operation->kind = COMBAT1::SIDE_WAIT; break;
                case OP_END_SIDE: operation->kind = COMBAT1::END_SIDE; break;
                case OP_INPUT_MODE: operation->kind = COMBAT1::INPUT_MODE; break;
                case OP_TACTICAL_OPTION: operation->kind = COMBAT1::TACTICAL_OPTION; break;
                case OP_AUTO_COMBAT: operation->kind = COMBAT1::AUTO_COMBAT; break;
                default: return Reject_Operation(forwarder, &request, -32004);
            }
            operation->x = request.x;
            operation->y = request.y;
            operation->missile_target = request.missile_target;
            operation->bank_mask = request.bank_mask;
            operation->boarding_mode = request.boarding_mode;
            operation->marines = request.marines;
            operation->value = request.value;
            operation->option = request.option;
            for (unsigned i = 0; i < 8; ++i) operation->bank_states[i] = request.bank_states[i];
            forwarder->executing = std::move(request);
            *result = forwarder->validate != nullptr ? forwarder->validate(operation) : COMBAT1::NO_CONTROL;
            return true;
        }
        void Complete(COMBAT1::e_result result, void* context) {
            auto* forwarder = (s_tactical_forwarder*)context;
            const int codes[] = {0, -32004, -32009, -32010, -32011, -32016};
            if (forwarder->acknowledgement.method != OP_NONE) {
                Complete_Operation(forwarder->service, &forwarder->acknowledgement, codes[result]);
                forwarder->acknowledgement = {};
                return;
            }
            Complete_Operation(forwarder->service, &forwarder->executing, codes[result], result == COMBAT1::APPLIED && forwarder->executing.method == OP_BOARD ? Boarding_Result_Json(BOARDING::Boarding_Result()) : std::string());
            forwarder->executing = {};
        }

        bool Take_Report(uint64_t generation, void* context) {
            auto* f = (s_tactical_forwarder*)context;
            s_operation_request request;
            if (!Take_Operation(f->service, &request, true)) return false;
            if (!BOARDING::Boarding_Report_Active() || generation != f->report_generation ||
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

        void Forward(const COMBAT::s_event* event, void* context) noexcept {
            auto* forwarder = (s_tactical_forwarder*)context;
            if ((event->kind == COMBAT::VIEW_INVALIDATED || event->kind == COMBAT::UNIT_SELECTED ||
                    event->kind == COMBAT::ATTENTION_REQUIRED || event->kind == COMBAT::ATTENTION_CLEARED || event->kind == COMBAT::ATTENTION_UPDATED) &&
                forwarder->reader.observer != nullptr && forwarder->reader.observer() != forwarder->viewer) {
                Revoke(forwarder);
                return;
            }
            if (event->kind == COMBAT::REVOKED) {
                Revoke(forwarder);
            } else if (event->kind == COMBAT::SERVICE_INPUT) {
                if (forwarder->reader.observer != nullptr && forwarder->reader.observer() != forwarder->viewer) {
                    Revoke(forwarder);
                    return;
                }
                if (forwarder->reader.observer != nullptr && forwarder->reader.observer() == forwarder->viewer &&
                    forwarder->battle_id[0] != '\0' && !forwarder->retiring)
                    Execute_Reads(forwarder->service, true, true, Execute_Tactical_Read, forwarder);
            } else if (event->kind == COMBAT::SERVICE_READS) {
                const bool active = forwarder->battle_id[0] != '\0' && !forwarder->retiring;
                if (active && forwarder->reader.observer != nullptr && forwarder->reader.observer() != forwarder->viewer) {
                    Revoke(forwarder);
                    return;
                }
                Execute_Reads(forwarder->service, event->safe, active, Execute_Tactical_Read, forwarder);
                if ((event->safe || !active) && (!active || (Control_Actor() < 0 && (forwarder->input == nullptr || forwarder->input() == nullptr)))) {
                    s_operation_request request;
                    if (Take_Operation(forwarder->service, &request))
                        Complete_Operation(forwarder->service, &request, active ? -32009 : -32004);
                }
            } else if (event->kind == COMBAT::RETREAT_RESOLVED) {
                if (forwarder->reader.observer == nullptr || forwarder->reader.observer() != forwarder->viewer) {
                    Revoke(forwarder);
                    return;
                }
                // Outcome identity is the previously disclosed lifetime, never a new action handle.
                for (const auto& reference : forwarder->references) {
                    if (reference.slot != event->unit) continue;
                    const char* outcomes[] = {"escaped", "destroyed", "cancelled", "interrupted", "interrupted"};
                    std::string detail = "\"outcome\":\"" + std::string(outcomes[event->retreat_outcome]) + "\"";
                    if (event->retreat_outcome == COMBAT::CAPTURED) detail += ",\"reason\":\"captured\"";
                    if (event->retreat_outcome == COMBAT::BATTLE_ENDED) detail += ",\"reason\":\"battleEnded\"";
                    Publish_Event(forwarder->service, EVENT_COMBAT_RETREAT, forwarder->battle_id, reference.token.data(), std::move(detail));
                    break;
                }
            } else if (event->kind == COMBAT::BOARDING_RESOLVED) {
                if (forwarder->reader.observer == nullptr || forwarder->reader.observer() != forwarder->viewer) {
                    Revoke(forwarder);
                    return;
                }
                forwarder->report_id.clear();
                forwarder->report_generation = BOARDING::Boarding_Generation();
                if (BOARDING::Boarding_Report_Active()) {
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
                Publish_Event(forwarder->service, EVENT_COMBAT_BOARDING, forwarder->battle_id, {}, Boarding_Result_Json(BOARDING::Boarding_Result(), forwarder->report_id));
            } else if (event->kind == COMBAT::BOARDING_REPORT_CLOSED) {
                forwarder->report_id.clear();
                Close_Boarding_Report(forwarder->service);
            } else if (event->kind == COMBAT::UNIT_RETIRED || event->kind == COMBAT::MISSILE_RETIRED) {
                const auto* input = forwarder->input != nullptr ? forwarder->input() : nullptr;
                if (event->kind == COMBAT::UNIT_RETIRED && input != nullptr &&
                    (input->actor == event->unit || input->target == event->unit)) {
                    forwarder->input_retired = true;
                    Close_Boarding_Report(forwarder->service);
                }
                auto& references = event->kind == COMBAT::UNIT_RETIRED ? forwarder->references : forwarder->missile_references;
                bool removed = false;
                for (size_t i = 0; i < references.size();) {
                    if (references[i].slot == event->unit) {
                        references.erase(references.begin() + i);
                        removed = true;
                    } else {
                        ++i;
                    }
                }
                if (removed) Invalidate_Read_Delivery(forwarder->service);
            } else if (event->kind == COMBAT::RETIRING) {
                forwarder->retiring = true;
                forwarder->manual_id.clear();
                forwarder->report_id.clear();
                Close_Boarding_Report(forwarder->service);
                forwarder->references.clear();
                forwarder->missile_references.clear();
                forwarder->selected.clear();
                Invalidate_Read_Delivery(forwarder->service);
                Execute_Reads(forwarder->service, false, false, Execute_Tactical_Read, forwarder);
            } else if (event->kind == COMBAT::OPENED) {
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
                if (event->kind == COMBAT::VIEW_INVALIDATED) {
                    Publish_Event(forwarder->service, EVENT_COMBAT_INVALIDATED, forwarder->battle_id);
                } else if (event->kind == COMBAT::ATTENTION_REQUIRED || event->kind == COMBAT::ATTENTION_CLEARED || event->kind == COMBAT::ATTENTION_UPDATED) {
                    if (event->kind == COMBAT::ATTENTION_REQUIRED) forwarder->input_retired = false;
                    std::string detail;
                    if (forwarder->input != nullptr && forwarder->input() != nullptr) {
                        detail = "\"input\":" + Tactical_Input_Json(forwarder);
                    } else if (event->kind == COMBAT::ATTENTION_CLEARED && !BOARDING::Boarding_Report_Active()) {
                        forwarder->manual_id.clear();
                        Close_Boarding_Report(forwarder->service);
                    }
                    Publish_Event(forwarder->service, event->kind == COMBAT::ATTENTION_CLEARED ? EVENT_COMBAT_ATTENTION_CLEARED : EVENT_COMBAT_ATTENTION, forwarder->battle_id, {}, std::move(detail));
                } else if (event->kind == COMBAT::UNIT_SELECTED) {
                    s_unit unit;
                    std::string selected;
                    if (forwarder->reader.unit != nullptr && forwarder->reader.unit(event->unit, &unit)) selected = Tactical_Unit_Token(forwarder, &unit);
                    if (selected == forwarder->selected) return;
                    forwarder->selected = selected;
                    Publish_Event(forwarder->service, EVENT_COMBAT_SELECTED, forwarder->battle_id, selected);
                    Publish_Event(forwarder->service, EVENT_COMBAT_INVALIDATED, forwarder->battle_id);
                } else if (event->kind == COMBAT::CLOSED) {
                    forwarder->manual_id.clear();
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
        if (!Bind_Control_Delivery(Invalidate_Inspection, service)) return false;
        forwarder->subscription = COMBAT::Subscribe(Forward, forwarder);
        if (forwarder->subscription != 0 && COMBAT::Attach_Control(Take, Complete, forwarder) &&
            BOARDING::Attach_Report_Input(Take_Report, Complete_Report, forwarder)) return true;
        COMBAT::Detach_Control(forwarder);
        Bind_Control_Delivery(nullptr, nullptr);
        if (forwarder->subscription != 0) COMBAT::Unsubscribe(forwarder->subscription);
        *forwarder = {};
        return false;
    }

    void Unsubscribe_Tactical_Events(s_tactical_forwarder* forwarder) {
        if (forwarder == nullptr || forwarder->subscription == 0) return;
        COMBAT::Unsubscribe(forwarder->subscription);
        COMBAT::Detach_Control(forwarder);
        Bind_Control_Delivery(nullptr, nullptr);
        Revoke(forwarder);
        *forwarder = {};
    }
} // namespace rpc
