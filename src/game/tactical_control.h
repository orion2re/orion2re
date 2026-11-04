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
    };
    struct s_weapon {
        int16_t slot = 0;
        char name[80] = {};
        int16_t type = 0, count = 0, selection = 0, shots = 0, ammo = 0, arc = 0;
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
} // namespace tactical_control
