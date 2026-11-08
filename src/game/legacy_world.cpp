#include "pch.h"

#include "legacy_world.h"

namespace legacy_world {
    game_world::s_view World() {
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
            {&game_random::random_seed, game_random::Random_From_Seed},
        };
    }
} // namespace legacy_world
