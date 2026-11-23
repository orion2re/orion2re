#include "pch.h"

#include "legacy_world.h"

namespace legacy_world {
    game::s_view World() {
        return {
            &game::current_state,
            &MOX::settings,
            &MOX::stardate,
            &MOX::cur_combat_x,
        };
    }
} // namespace legacy_world
