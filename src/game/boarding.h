#pragma once
#include <cstdint>

namespace BOARDING {
    struct s_board_range {
        uint8_t normal;
        uint8_t transporters;
    };

    struct s_boarding {
        uint8_t damper_survival_divisor;
        uint8_t auto_close_results;
        uint8_t raid_damage_per_marine;
        uint8_t raid_items_per_marine;
    };

    enum class e_boarding_filter : uint8_t {
        allowed,
        target_in_stasis,
        transporter_blocked_by_damper,
        target_in_black_hole,
        attacker_in_black_hole,
        target_phased_out,
    };

    extern uint8_t telepathic_multiple_capture;
    extern uint8_t kill_excess_marines_after_capture;
    extern uint8_t board_in_black_hole;
    extern s_boarding boarding;
    extern s_board_range board_range;

    int16_t __cdecl Captured_Marine_Count(
        int16_t target_idx, int16_t surviving_marines);

    e_boarding_filter __cdecl Filter_Boarding(
        int16_t attacker_idx, int16_t target_idx);

    struct s_human_check {
        bool allowed = true, shield_warning = false;
        int16_t message = 0;
        const char* warning = nullptr;
    };
    s_human_check Check_Human_Boarding(int16_t attacker, int16_t target);
    int16_t Maximum_Commitment(int16_t actor);
} // namespace BOARDING
