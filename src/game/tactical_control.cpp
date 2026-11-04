#include "tactical_control.h"
#include "tactical_events.h"

#include <limits>

namespace tactical_control {
    namespace {
        uint64_t generation = 1;
        int16_t actor = -1;
        t_take take = nullptr;
        t_complete complete = nullptr;
        void* context = nullptr;
    } // namespace
    void Invalidate() {
        if (generation == std::numeric_limits<uint64_t>::max()) {
            actor = -1;
            tactical_events::Revoke();
            return;
        }
        ++generation;
    }
    void Open_Decision(int16_t unit) {
        if (actor == unit) return;
        actor = unit;
        Invalidate();
        tactical_events::Invalidate_View();
    }
    void Close_Decision() {
        if (actor < 0) return;
        actor = -1;
        Invalidate();
    }
    uint64_t Generation() {
        return generation;
    }
    int16_t Actor() {
        return actor;
    }
    bool Attach(t_take input, t_complete output, void* owner) {
        if (take != nullptr || input == nullptr || output == nullptr) return false;
        take = input;
        complete = output;
        context = owner;
        return true;
    }
    void Detach(void* owner) {
        if (context != owner) return;
        take = nullptr;
        complete = nullptr;
        context = nullptr;
    }
    bool Take(s_operation* operation) {
        return take != nullptr && take(operation, context);
    }
    void Complete(e_result result) {
        if (complete != nullptr) complete(result, context);
    }
} // namespace tactical_control
