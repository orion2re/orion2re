#pragma once
#include <cstdint>
#include <vector>

namespace COMBAT1 {
    struct s_operation;
    enum e_result : int;
} // namespace COMBAT1

namespace rpc {
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
        std::vector<s_target> targets, missile_targets;
        int16_t own_marines = 0, maximum_marines = 0;
        bool already_boarded = false;
        int16_t retreat_refusal = 0;
        std::vector<int16_t> boarding_targets;
    };
    bool Read_Options(s_options* result);
    bool Authorize(int16_t slot, bool owned);
    COMBAT1::e_result Validate(const COMBAT1::s_operation* operation);
    uint64_t Control_Generation();
    int16_t Control_Actor();
} // namespace rpc
