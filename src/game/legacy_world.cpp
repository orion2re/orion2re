#include "pch.h"

#include "legacy_world.h"

namespace legacy_world {
    game::s_view World() {
        return {
            MOX::player,
            MOX::ship,
            &game::current_state,
            &MOX::settings,
            &MOX::NUM_PLAYERS,
            &MOX::NUM_SHIPS,
            &MOX::stardate,
            &MOX::cur_combat_x,
        };
    }
} // namespace legacy_world
