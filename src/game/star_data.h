#pragma once

#include <cstdint>

#include "consts.h"
#include "object_ids.h"

struct s_star_data {
    char name[15];
    int16_t x;
    int16_t y;
    int8_t size;
    int8_t owner;
    int8_t pict_type;
    int8_t spectral_class;
    int8_t last_planet_selected[MAX_PLAYERS];
    int8_t black_hole_blocks[BITMAP(MAX_STARS)];
    int8_t system_special;
    game::id_star wormhole_star_id = game::NO_STAR;
    uint8_t blockaded;
    uint8_t blockaded_by[MAX_PLAYERS];
    uint8_t visited;
    uint8_t just_visited;
    uint8_t ignore_colony_ships;
    uint8_t ignore_combat_ships;
    int8_t colonize_player;
    uint8_t has_colony;
    uint8_t has_warp_field_interdictor;
    game::id_star next_wfi_in_list = game::NO_STAR;
    uint8_t has_tachyon;
    uint8_t has_subspace;
    uint8_t has_stargate;
    uint8_t has_jumpgate;
    uint8_t has_artemis_net;
    uint8_t has_dimensional_portal;
    uint8_t is_stagepoint;
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
    int8_t surrender_to[MAX_PLAYERS];
    uint8_t in_nebula;
    uint8_t artifacts_gave_app;
    game::id_star id = game::NO_STAR;

    bool operator==(const s_star_data&) const = default;
};
