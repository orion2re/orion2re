#include "pch.h"
#include "rpc_combat_control.h"
#include <limits>

namespace rpc {
    namespace {
        uint64_t control_generation = 1;
        bool observer_attached = false, revoking = false;
        void (*invalidate_delivery)(uint64_t, void*);
        void* delivery_context;
        void Control_Changed() {
            if (control_generation == std::numeric_limits<uint64_t>::max()) {
                if (!revoking) {
                    revoking = true;
                    COMBAT::Revoke();
                    revoking = false;
                }
                return;
            }
            ++control_generation;
            if (invalidate_delivery != nullptr) invalidate_delivery(control_generation, delivery_context);
        }
        void Attach_Observer() {
            if (!observer_attached) observer_attached = COMBAT::Attach_Control_Observer(Control_Changed);
        }
    } // namespace
    bool Bind_Control_Delivery(void (*invalidate)(uint64_t, void*), void* context) {
        if (invalidate != nullptr) {
            Attach_Observer();
            if (!observer_attached) return false;
        }
        invalidate_delivery = invalidate;
        delivery_context = context;
        if (invalidate_delivery != nullptr) invalidate_delivery(control_generation, delivery_context);
        return true;
    }
    uint64_t Control_Generation() {
        Attach_Observer();
        return observer_attached ? control_generation : 0;
    }
    int16_t Control_Actor() {
        Attach_Observer();
        return observer_attached ? COMBAT::Current_Control_Actor() : -1;
    }
} // namespace rpc
