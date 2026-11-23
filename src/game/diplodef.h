#pragma once

#include <cstdint>

namespace diplodef {
    extern int16_t personality_relation_modifiers[MAX_PLAYERS];
    extern int16_t threat_demand_chance[10];
    extern int16_t personality_threat_modifiers[10];
    extern int16_t next_council_meeting_turn;
    extern int16_t COUNCIL_FLAG;
    extern game::id_player council_winner;
    extern int16_t personality_reward_proposal_chance[10];
    extern int16_t personality_max_threats[10];
    extern int16_t personality_break_treaty_war[10];
} // namespace diplodef

namespace DIPLODEF = diplodef;