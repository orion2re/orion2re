#pragma once

#include <cstdint>

#include "consts.h"
#include "object_ids.h"
#include "player_bitmap.h"
#include "star_surrender.h"

struct s_star_data {
    char name[15];
    int16_t x;
    int16_t y;
    int8_t size;
    game::owner_slot owner;
    int8_t pict_type;
    int8_t spectral_class;
    int8_t last_planet_selected[MAX_PLAYERS];
    int8_t black_hole_blocks[BITMAP(MAX_STARS)];
    int8_t system_special;
    game::id_star wormhole_star_id = game::NO_STAR;
    game::player_bitmap blockaded;
    game::player_bitmap blockaded_by[MAX_PLAYERS];
    game::player_bitmap visited;
    game::player_bitmap just_visited;
    game::player_bitmap ignore_colony_ships;
    game::player_bitmap ignore_combat_ships;
    game::owner_slot colonize_player;
    game::player_bitmap has_colony;
    game::player_bitmap has_warp_field_interdictor;
    game::id_star next_wfi_in_list = game::NO_STAR;
    game::player_bitmap has_tachyon;
    game::player_bitmap has_subspace;
    game::player_bitmap has_stargate;
    game::player_bitmap has_jumpgate;
    game::player_bitmap has_artemis_net;
    game::player_bitmap has_dimensional_portal;
    game::player_bitmap is_stagepoint;
    int8_t officer_index[MAX_PLAYERS];
    game::id_planet orbit_planet_ids[5] = {
        game::NO_PLANET, game::NO_PLANET, game::NO_PLANET, game::NO_PLANET, game::NO_PLANET,
    };
    game::id_star relocate_ship_to[MAX_PLAYERS] = {
        game::NO_STAR, game::NO_STAR, game::NO_STAR, game::NO_STAR,
        game::NO_STAR, game::NO_STAR, game::NO_STAR, game::NO_STAR,
    };
    int8_t twinkling1;
    int8_t twinkling2;
    uint8_t not_used3;
    game::star_surrender_order surrender_to[MAX_PLAYERS];
    uint8_t in_nebula;
    uint8_t artifacts_gave_app;
    game::id_star id = game::NO_STAR;

    bool operator==(const s_star_data&) const = default;
};
