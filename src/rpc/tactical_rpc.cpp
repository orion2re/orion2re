#include "tactical_rpc.h"
#include "rpc_protocol.h"
#include "tactical_reads.h"
#include "game/tactical_events.h"

#include <uv.h>
#include <algorithm>

namespace rpc {
    namespace {
        void Revoke(s_tactical_forwarder* forwarder) noexcept {
            forwarder->viewer = -1;
            forwarder->battle_id[0] = '\0';
            forwarder->references.clear();
            forwarder->selected.clear();
            Revoke_Event_Delivery(forwarder->service);
        }

        void Forward(const tactical_events::s_event* event, void* context) noexcept {
            auto* forwarder = (s_tactical_forwarder*)context;
            if ((event->kind == tactical_events::VIEW_INVALIDATED || event->kind == tactical_events::UNIT_SELECTED) &&
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
            } else if (event->kind == tactical_events::UNIT_RETIRED) {
                auto& references = forwarder->references;
                const auto end = std::remove_if(references.begin(), references.end(), [event](const auto& reference) { return reference.slot == event->unit; });
                if (end != references.end()) {
                    references.erase(end, references.end());
                    Invalidate_Read_Delivery(forwarder->service);
                }
            } else if (event->kind == tactical_events::RETIRING) {
                forwarder->retiring = true;
                forwarder->references.clear();
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
                } else if (event->kind == tactical_events::UNIT_SELECTED) {
                    tactical_view::s_unit unit;
                    std::string selected;
                    if (forwarder->reader.unit != nullptr && forwarder->reader.unit(event->unit, &unit)) selected = Tactical_Unit_Token(forwarder, &unit);
                    if (selected == forwarder->selected) return;
                    forwarder->selected = selected;
                    Publish_Event(forwarder->service, EVENT_COMBAT_SELECTED, forwarder->battle_id, selected);
                    Publish_Event(forwarder->service, EVENT_COMBAT_INVALIDATED, forwarder->battle_id);
                } else if (event->kind == tactical_events::CLOSED) {
                    if (!forwarder->retiring) Invalidate_Read_Delivery(forwarder->service);
                    Publish_Event(forwarder->service, EVENT_COMBAT_CLOSED, forwarder->battle_id);
                    forwarder->battle_id[0] = '\0';
                    forwarder->references.clear();
                    forwarder->selected.clear();
                }
            }
        }
    } // namespace

    bool Subscribe_Tactical_Events(s_tactical_forwarder* forwarder, s_service* service) {
        if (forwarder == nullptr || service == nullptr || service->state == nullptr || forwarder->subscription != 0) return false;
        forwarder->service = service;
        forwarder->subscription = tactical_events::Subscribe(Forward, forwarder);
        if (forwarder->subscription != 0) return true;
        *forwarder = {};
        return false;
    }

    void Unsubscribe_Tactical_Events(s_tactical_forwarder* forwarder) {
        if (forwarder == nullptr || forwarder->subscription == 0) return;
        tactical_events::Unsubscribe(forwarder->subscription);
        Revoke(forwarder);
        *forwarder = {};
    }
} // namespace rpc
