#include "pch.h"

#include "legacy_world.h"

namespace legacy_world {
    game::s_view World() {
        return {
            MOX::player,
            &game::current_state,
            &MOX::settings,
            &MOX::NUM_PLAYERS,
            &MOX::stardate,
            &MOX::cur_combat_x,
        };
    }
} // namespace legacy_world
