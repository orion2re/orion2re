#include "pch.h"

#include "legacy_world.h"

namespace legacy_world {
    game::s_view World(game::s_world_state* world_state) {
        return {
            MOX::player,
            MOX::ship,
            MOX::star,
            MOX::colony,
            MOX::planet,
            world_state,
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
