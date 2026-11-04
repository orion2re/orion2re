#pragma once

#include <cstdint>

namespace tactical_events {
    enum e_retreat_outcome : int;
}

namespace tactical_view {
    // Shared native disclosure predicates and semantic producers, game-thread only.
    int16_t Local_Observer();
    bool Unit_Disclosed(int16_t slot);
    void Unit_Changed(int16_t slot, bool map_change = false);
    // Native semantic producers only; no retained withdrawal report or tracking state.
    void Resolve_Retreat(int16_t slot, tactical_events::e_retreat_outcome outcome);
    void End_Pending_Retreats();
    bool Missile_Disclosed(int16_t slot);
    void Missile_Changed(int16_t slot);
    void Record_Unit_Hit(int16_t slot, int32_t amount, int32_t secondary_amount = 0, bool biological = false);
    void Record_Missile_Hit(int16_t slot, int32_t destroyed);
} // namespace tactical_view
