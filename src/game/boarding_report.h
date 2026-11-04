#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

namespace BOARDING {
    // Resolved native presentation facts, not inputs to a combat calculator.
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
    // Nested native attempts retain only the caller's selected commitment.
    struct s_attempt {
        int16_t committed;
        s_attempt* previous;
        explicit s_attempt(int16_t marines);
        ~s_attempt();
    };
    int16_t Committed_Marines();
} // namespace BOARDING
