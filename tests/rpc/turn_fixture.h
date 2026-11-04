#pragma once

#include "pch.h"
#include "game/tactical_control.h"
#include "game/tactical_events.h"

namespace TEST_TACTICAL_TURN {
    void Boarding(bool reports = false);
    void Release_Boarding();
    void Reset();
    void Apply(const tactical_control::s_operation* operation);
    void Add_Missile(int16_t slot, int16_t weapon = WEAPON_NUCLEAR_MISSILE, int16_t owner = 1);
} // namespace TEST_TACTICAL_TURN
