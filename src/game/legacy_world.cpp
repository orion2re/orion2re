#include "pch.h"

#include "legacy_world.h"

namespace legacy_world {
    game::s_view World() {
        return {
            MOX::player,
            MOX::ship,
            MOX::star,
            MOX::colony,
            MOX::planet,
            MOX::leaders,
            &MOX::settings,
            &MOX::NUM_PLAYERS,
            &MOX::NUM_SHIPS,
            &MOX::NUM_STARS,
            &MOX::NUM_COLONIES,
            &MOX::planet_count,
            &MOX::stardate,
            &MOX::cur_combat_x,
        };
    }
} // namespace legacy_world
