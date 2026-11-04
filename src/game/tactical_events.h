#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include "boarding.h"

namespace tactical_events {
    enum e_kind {
        OPENED,
        CLOSED,
        REVOKED,
        SERVICE_READS,
        VIEW_INVALIDATED,
        UNIT_SELECTED,
        UNIT_RETIRED,
        RETIRING,
        ATTENTION_REQUIRED,
        ATTENTION_CLEARED,
        MISSILE_RETIRED,
        BOARDING_RESOLVED,
        BOARDING_REPORT_CLOSED,
        RETREAT_RESOLVED,
    };
    enum e_retreat_outcome : int { ESCAPED,
        DESTROYED,
        CANCELLED,
        CAPTURED,
        BATTLE_ENDED };
    struct s_event {
        e_kind kind;
        int16_t viewer;
        int16_t unit = -1;
        bool safe = false;
        e_retreat_outcome retreat_outcome = CANCELLED;
    };
    using t_callback = void (*)(const s_event* event, void* context) noexcept;

    // Register before the worker starts, or on the publishing game thread.
    // Callbacks are synchronous, observation-only; the payload is callback-local.
    uint64_t Subscribe(t_callback callback, void* context);
    void Unsubscribe(uint64_t subscription);
    void Revoke() noexcept;
    // Only explicit completed tactical boundaries may pass safe=true.
    void Service_Reads(bool safe = false) noexcept;
    void Invalidate_View() noexcept;
    void Select_Unit(int16_t unit) noexcept;
    void Retire_Unit(int16_t unit) noexcept;
    void Retire_Missile(int16_t missile) noexcept;
    void Retire_Battle() noexcept;
    void Resolve_Retreat(int16_t unit, e_retreat_outcome outcome) noexcept;
    int16_t Current_Viewer() noexcept;
    int16_t Current_Unit() noexcept;
    bool Reads_Allowed() noexcept;
    constexpr size_t MAX_HIT_RECORDS = 4096;
    enum e_hit_kind { UNIT_DAMAGE,
        MISSILES_DESTROYED,
        BIOLOGICAL_HITS };
    struct s_hit {
        uint64_t sequence = 0;
        int32_t round = 0;
        e_hit_kind kind = UNIT_DAMAGE;
        char target_name[80] = {};
        int32_t amount = 0, secondary_amount = 0;
    };
    struct s_hit_history {
        uint64_t first_sequence = 1, next_sequence = 1;
        std::vector<s_hit> hits;
    };
    // The tactical module owns the report, independently of observers/transports.
    void Append_Hit(const s_hit* hit);
    bool Read_Hit_History(s_hit_history* result);
    // Produced by native resolution; input owner activation is a separate boundary.
    void Record_Boarding_Result(const BOARDING::s_result* result);
    void Publish_Boarding_Result() noexcept;
    const BOARDING::s_result* Boarding_Result() noexcept;
    uint64_t Boarding_Generation() noexcept;
    void Open_Boarding_Report(bool timed) noexcept;
    void Close_Boarding_Report() noexcept;
    bool Boarding_Report_Active() noexcept;
    bool Boarding_Report_Timed() noexcept;
    int16_t Begin_Local_Attention() noexcept;
    void End_Local_Attention(int16_t viewer) noexcept;

    struct s_battle_scope {
        int16_t viewer = -1;
        bool initialized = false;
        bool opened = false;
        bool retiring = false;
        bool dirty = false;
        bool selection_pending = false;
        int16_t selected = -1;
        uint64_t next_hit_sequence = 1;
        size_t hit_head = 0;
        std::vector<s_hit> hits;
        BOARDING::s_result boarding;
        uint64_t boarding_generation = 0;
        bool boarding_active = false, boarding_timed = false;
        void Open(int16_t local_observer) noexcept;
        void Close(int16_t local_observer) noexcept;
        ~s_battle_scope();
        s_battle_scope();
        s_battle_scope(const s_battle_scope&) = delete;
        s_battle_scope& operator=(const s_battle_scope&) = delete;
    };
} // namespace tactical_events
