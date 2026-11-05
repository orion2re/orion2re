#pragma once

#include "pch.h"
#include "game/combat.h"

namespace TEST_TACTICAL_TURN {
    void Boarding(bool reports = false);
    void Release_Boarding();
    void Reset();
    void Withdrawal();
    void Withdrawal_Display();
    void Waiting(int16_t initiative, bool single = false);
    void Apply(const COMBAT1::s_operation* operation);
    void Add_Missile(int16_t slot, int16_t weapon = WEAPON_NUCLEAR_MISSILE, int16_t owner = 1);
} // namespace TEST_TACTICAL_TURN
