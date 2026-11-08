#include "pch.h"

#include "world_view.h"

namespace game_world {
    s_view current;

    bool Has_Strategic_AI_Inputs(const s_view* world) {
        return world != nullptr && world->players && world->ships && world->stars &&
            world->colonies && world->planets && world->leaders && world->settings &&
            world->stardate && world->player_count && world->ship_count && world->star_count &&
            world->colony_count && world->planet_count && world->random.draw &&
            *world->player_count > 0 && *world->player_count <= MAX_PLAYERS &&
            *world->ship_count >= 0 && *world->ship_count <= MAX_SHIPS &&
            *world->star_count >= 0 && *world->star_count <= MAX_STARS &&
            *world->colony_count >= 0 && *world->colony_count <= MAX_COLONIES &&
            *world->planet_count >= 0 && *world->planet_count <= MAX_PLANETS;
    }
} // namespace game_world
