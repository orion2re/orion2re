#include "tactical_events.h"

#include <array>
#include <limits>

namespace tactical_events {
    namespace {
        struct s_listener {
            uint64_t id = 0;
            t_callback callback = nullptr;
            void* context = nullptr;
        };
        std::array<s_listener, 4> listeners;
        uint64_t next_id = 0;
        s_battle_scope* current = nullptr;
        bool reads_allowed = false;

        void Publish(e_kind kind, int16_t viewer, int16_t unit = -1, bool safe = false) noexcept {
            const s_event event{kind, viewer, unit, safe};
            const auto limit = next_id;
            for (const auto& listener : listeners) {
                if (listener.id != 0 && listener.id <= limit) listener.callback(&event, listener.context);
            }
        }
    } // namespace

    uint64_t Subscribe(t_callback callback, void* context) {
        if (callback == nullptr || next_id == std::numeric_limits<uint64_t>::max()) return 0;
        for (auto& listener : listeners) {
            if (listener.id == 0) {
                listener = {++next_id, callback, context};
                return listener.id;
            }
        }
        return 0;
    }

    void Unsubscribe(uint64_t subscription) {
        for (auto& listener : listeners) {
            if (listener.id == subscription) listener = {};
        }
    }

    void Revoke() noexcept {
        if (current != nullptr) current->retiring = true;
        Publish(REVOKED, -1);
    }

    int16_t Current_Viewer() noexcept {
        return current != nullptr && current->opened && !current->retiring ? current->viewer : -1;
    }

    int16_t Current_Unit() noexcept {
        return Current_Viewer() >= 0 ? current->selected : -1;
    }

    bool Reads_Allowed() noexcept {
        return reads_allowed;
    }

    void Service_Reads(bool safe) noexcept {
        // A nested generic input/network pump cannot inherit a read-safe point.
        const bool previous = reads_allowed;
        reads_allowed = safe && Current_Viewer() >= 0;
        if (reads_allowed && current->selection_pending) {
            current->selection_pending = false;
            Publish(UNIT_SELECTED, current->viewer, current->selected, true);
        }
        if (reads_allowed && current->dirty) {
            current->dirty = false;
            Publish(VIEW_INVALIDATED, current->viewer, -1, true);
        }
        Publish(SERVICE_READS, Current_Viewer(), -1, reads_allowed);
        reads_allowed = previous;
    }

    void Invalidate_View() noexcept {
        if (Current_Viewer() >= 0) current->dirty = true;
    }

    void Select_Unit(int16_t unit) noexcept {
        if (Current_Viewer() < 0 || current->selected == unit) return;
        current->selected = unit;
        current->selection_pending = true;
    }

    void Retire_Unit(int16_t unit) noexcept {
        if (Current_Viewer() >= 0) {
            Publish(UNIT_RETIRED, current->viewer, unit);
            if (current->selected == unit) {
                current->selected = -1;
                current->selection_pending = true;
            }
        }
    }

    void Retire_Battle() noexcept {
        if (Current_Viewer() < 0) return;
        current->retiring = true;
        Publish(RETIRING, current->viewer);
    }

    void s_battle_scope::Open(int16_t local_observer) noexcept {
        if (initialized) return;
        initialized = true;
        if (current != nullptr && current != this) Revoke();
        current = this;
        viewer = local_observer;
        if (viewer < 0) return;
        opened = true;
        Publish(OPENED, viewer);
    }

    void s_battle_scope::Close(int16_t local_observer) noexcept {
        if (!opened) return;
        opened = false;
        if (current != this) return;
        if (local_observer == viewer) {
            Publish(CLOSED, viewer);
        } else {
            Revoke();
        }
    }

    s_battle_scope::~s_battle_scope() {
        if (current == this) {
            if (opened) Revoke();
            current = nullptr;
        }
    }
} // namespace tactical_events
