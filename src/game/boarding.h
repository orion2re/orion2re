#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

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

    int16_t Captured_Marine_Count(
        int16_t target_idx, int16_t surviving_marines);

    e_boarding_filter Filter_Boarding(
        int16_t attacker_idx, int16_t target_idx);

    struct s_human_check {
        bool allowed = true, shield_warning = false;
        int16_t message = 0;
        const char* warning = nullptr;
    };
    s_human_check Check_Human_Boarding(int16_t attacker, int16_t target);
    int16_t Maximum_Commitment(int16_t actor);

    // Authorization is captured before resolution can change target ownership or disclosure.
    struct s_raid_result_input {
        bool authorized = false;
        int16_t attacker_owner = -1;
        int16_t target = -1;
        int16_t committed = -1;
        int16_t arrived = 0;
        int16_t attacker_survived = 0;
        int16_t defender_survived = 0;
        int16_t attacker_strength = 0;
        int16_t defender_strength = 0;
        int16_t damage = 0;
        std::string_view text;
    };
    struct s_capture_result_input {
        bool authorized = false;
        int16_t attacker = -1;
        int16_t target = -1;
        int16_t committed = -1;
        int16_t arrived = 0;
        int16_t attacker_survived = 0;
        int16_t defender_started = 0;
        int16_t defender_survived = 0;
        int16_t attacker_strength = 0;
        int16_t defender_strength = 0;
        int16_t message_id = 0;
    };
    struct s_result {
        bool available = false, capture = false, captured = false, destroyed = false;
        bool content_available = true, raid_damage = false;
        int16_t committed = -1, arrived = 0, attacker_lost = 0, attacker_survived = 0;
        int16_t defender_started = 0, defender_lost = 0, defender_survived = 0;
        int16_t attacker_strength = 0, defender_strength = 0;
        int16_t captured_marines = 0;
        bool now_owned = false, target_captured = false, attacker_owned = false;
        char target_name[80] = {};
        std::string raid_text;
    };
    constexpr size_t MAX_REPORT_TEXT = 1024 * 1024;
    bool Result_Authorized(int16_t target);
    void Record_Raid_Result(const s_raid_result_input* input);
    void Record_Capture_Result(const s_capture_result_input* input);
    void Record_Boarding_Result(const s_result* result);
    void Publish_Boarding_Result() noexcept;
    const s_result* Boarding_Result() noexcept;
    uint64_t Boarding_Generation() noexcept;
    void Open_Boarding_Report(bool timed) noexcept;
    void Close_Boarding_Report() noexcept;
    bool Boarding_Report_Active() noexcept;
    bool Boarding_Report_Timed() noexcept;
    using t_report_take = bool (*)(uint64_t, void*);
    using t_report_complete = void (*)(void*);
    bool Attach_Report_Input(t_report_take input, t_report_complete output, void* owner);
    void Detach_Report_Input(void* owner);
    bool Take_Report_Acknowledgement(uint64_t report);
    void Complete_Report_Acknowledgement();
} // namespace BOARDING
