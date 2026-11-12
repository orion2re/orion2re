#pragma once
#include <cstdint>
#include "world_view.h"

namespace NEXTTURN {
    bool Validate_Turn_Inputs(const game::s_view* world);
    /** Advances the game to the next turn and handles any end-of-game transitions. */
    void __cdecl Next_Turn();
    /** Writes the periodic autosave slot when autosaving is enabled for the current game mode. */
    void __cdecl Do_Autosave();

    /** Processes end-of-turn simulation, AI, events, and state updates for the next turn. */
    bool Next_Turn_Calc();

    /** Returns the winning player index, or -1 when no winner has been determined yet. */
    int16_t Get_Winner();
} // namespace NEXTTURN