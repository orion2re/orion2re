#pragma once

#include <cstdint>
#include <vector>

namespace tactical_control {
    enum e_operation {
        NONE,
        SELECT,
        MOVE,
        FACE,
        FIRE,
        DONE,
        FIRE_SELECTION,
        BOARD,
    };
    enum e_result {
        APPLIED,
        UNAVAILABLE,
        NO_CONTROL,
        CONFLICT,
        ILLEGAL,
        LOCAL_REQUIRED,
    };
    struct s_operation {
        e_operation kind = NONE;
        int16_t actor = -1, target = -1, x = 0, y = 0;
        bool missile_target = false;
        uint8_t bank_mask = 0;
        int8_t bank_states[8] = {};
        int16_t boarding_mode = 0, marines = 0;
    };
    struct s_weapon {
        int16_t slot = 0;
        char name[80] = {};
        int16_t type = 0, count = 0, selection = 0, shots = 0, ammo = 0, arc = 0;
        uint16_t modifiers = 0;
        bool ready = false;
    };
    struct s_cell {
        int16_t x, y, cost;
    };
    struct s_target {
        int16_t slot;
        uint8_t weapons;
        bool absorber;
    };
    struct s_options {
        bool available = false;
        int16_t actor = -1, turn_cost = 0, movement = 0, absorber_charge = 0;
        uint64_t generation = 0;
        bool absorber_selected = false;
        std::vector<int16_t> selectable;
        std::vector<s_weapon> weapons;
        std::vector<s_cell> cells;
        std::vector<s_target> targets;
        std::vector<s_target> missile_targets;
        int16_t own_marines = 0, maximum_marines = 0;
        bool already_boarded = false;
        std::vector<int16_t> boarding_targets;
    };
    // Game-thread-only decision metadata. No gameplay projections are retained.
    void Open_Decision(int16_t actor);
    void Close_Decision();
    void Invalidate();
    uint64_t Generation();
    int16_t Actor();
    bool Read_Options(s_options* result);
    bool Authorize(int16_t slot, bool owned);
    e_result Validate(const s_operation* operation);

    using t_take = bool (*)(s_operation*, void*);
    using t_complete = void (*)(e_result, void*);
    bool Attach(t_take take, t_complete complete, void* context);
    void Detach(void* context);
    bool Take(s_operation* operation);
    void Complete(e_result result);
    using t_report_take = bool (*)(uint64_t, void*);
    using t_report_complete = void (*)(void*);
    bool Attach_Report(t_report_take take, t_report_complete complete, void* context);
    bool Take_Report_Acknowledgement(uint64_t report);
    void Complete_Report_Acknowledgement();
} // namespace tactical_control
