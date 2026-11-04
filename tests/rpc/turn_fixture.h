#pragma once

#include "pch.h"
#include "game/tactical_control.h"
#include "game/tactical_events.h"

namespace TEST_TACTICAL_TURN {
    void Reset();
    void Apply(const tactical_control::s_operation* operation);
} // namespace TEST_TACTICAL_TURN
