#include "tactical_events.h"
#include "tactical_control.h"

#include <array>
#include <limits>
#include <utility>

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

        void Release_Boarding(s_battle_scope* scope) {
            BOARDING::s_result empty;
            std::swap(scope->boarding, empty);
            scope->boarding_active = false;
        }

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
        tactical_control::Close_Decision();
        if (current != nullptr) {
            current->retiring = true;
            std::vector<s_hit>().swap(current->hits);
            current->hit_head = 0;
            current->next_hit_sequence = 1;
            Release_Boarding(current);
        }
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
    s_local_attention::s_local_attention() noexcept : viewer(Current_Viewer()) {
        tactical_control::Close_Decision();
        if (viewer >= 0) Publish(ATTENTION_REQUIRED, viewer);
    }
    s_local_attention::~s_local_attention() {
        if (viewer >= 0 && Current_Viewer() == viewer) Publish(ATTENTION_CLEARED, viewer);
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
        tactical_control::Close_Decision();
        current->selected = unit;
        current->selection_pending = true;
    }

    void Retire_Unit(int16_t unit) noexcept {
        if (tactical_control::Actor() == unit) tactical_control::Close_Decision();
        if (Current_Viewer() >= 0) {
            Publish(UNIT_RETIRED, current->viewer, unit);
            if (current->selected == unit) {
                current->selected = -1;
                current->selection_pending = true;
            }
        }
    }

    void Retire_Battle() noexcept {
        tactical_control::Close_Decision();
        if (Current_Viewer() < 0) return;
        std::vector<s_hit>().swap(current->hits);
        current->hit_head = 0;
        current->next_hit_sequence = 1;
        Release_Boarding(current);
        current->retiring = true;
        Publish(RETIRING, current->viewer);
    }

    void Retire_Missile(int16_t missile) noexcept {
        if (Current_Viewer() < 0) return;
        tactical_control::Invalidate();
        Invalidate_View();
        Publish(MISSILE_RETIRED, current->viewer, missile);
    }

    void Append_Hit(const s_hit* hit) {
        if (Current_Viewer() < 0 || hit == nullptr || (hit->amount <= 0 && hit->secondary_amount <= 0)) return;
        s_hit record = *hit;
        record.sequence = current->next_hit_sequence++;
        if (current->hits.size() < MAX_HIT_RECORDS) {
            current->hits.push_back(record);
        } else {
            current->hits[current->hit_head] = record;
            current->hit_head = (current->hit_head + 1) % MAX_HIT_RECORDS;
        }
        Invalidate_View();
    }

    bool Read_Hit_History(s_hit_history* result) {
        if (result == nullptr || !Reads_Allowed() || Current_Viewer() < 0) return false;
        *result = {};
        result->next_sequence = current->next_hit_sequence;
        for (size_t i = 0; i < current->hits.size(); ++i)
            result->hits.push_back(current->hits[(current->hit_head + i) % current->hits.size()]);
        result->first_sequence = result->hits.empty() ? result->next_sequence : result->hits.front().sequence;
        return true;
    }

    void Record_Boarding_Result(const BOARDING::s_result* result) {
        if (Current_Viewer() < 0 || result == nullptr) return;
        BOARDING::s_result replacement = *result;
        std::swap(current->boarding, replacement);
        ++current->boarding_generation;
        current->boarding_active = false;
        Invalidate_View();
    }
    const BOARDING::s_result* Boarding_Result() noexcept {
        return Current_Viewer() >= 0 && current->boarding.available ? &current->boarding : nullptr;
    }
    uint64_t Boarding_Generation() noexcept {
        return Current_Viewer() >= 0 ? current->boarding_generation : 0;
    }
    bool Boarding_Report_Active() noexcept {
        return Current_Viewer() >= 0 && current->boarding_active;
    }
    bool Boarding_Report_Timed() noexcept {
        return Boarding_Report_Active() && current->boarding_timed;
    }
    void Open_Boarding_Report(bool timed) noexcept {
        if (Boarding_Result() == nullptr) return;
        current->boarding_active = true;
        current->boarding_timed = timed;
        Publish_Boarding_Result();
    }
    void Publish_Boarding_Result() noexcept {
        if (Boarding_Result() != nullptr) Publish(BOARDING_RESOLVED, current->viewer);
    }
    void Close_Boarding_Report() noexcept {
        if (!Boarding_Report_Active()) return;
        current->boarding_active = false;
        Publish(BOARDING_REPORT_CLOSED, current->viewer);
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
        Release_Boarding(this);
        std::vector<s_hit>().swap(hits);
        hit_head = 0;
        next_hit_sequence = 1;
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
