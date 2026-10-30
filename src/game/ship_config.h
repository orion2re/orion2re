#pragma once

#include "game/deps.h"

namespace SHIP_CONFIG {

    struct s_monster_hit_points {
        uint16_t system_eel;
        uint16_t system_amoeba;
        uint16_t system_crystal_hydra_dragon;
        uint16_t event_amoeba;
        uint16_t event_eel;
        uint16_t event_hydra;
        uint16_t event_crystal_dragon;
        int16_t system_crystal;
        int16_t system_hydra;
        int16_t system_dragon;
        int16_t event_crystal;
        int16_t event_dragon;
    };

    struct s_monster_drives {
        uint8_t amoeba;
        uint8_t guardian;
        uint8_t crystal;
        uint8_t dragon;
        uint8_t eel;
        uint8_t hydra;
    };

    struct s_monster_config {
        s_monster_hit_points hit_points;
        s_monster_drives drives;
    };

    struct s_crystal_shared_equipment {
        uint8_t shield;
        uint8_t computer;
        uint8_t special;
    };

    struct s_crystal_config {
        s_ship_design templates[2];
        s_crystal_shared_equipment shared;
    };

    struct s_antaran_fortress_weapon {
        uint16_t type;
        uint8_t arc;
        uint16_t mods;
    };

    struct s_antaran_fortress_primary_weapon {
        s_antaran_fortress_weapon weapon;
        uint16_t space_mods;
    };

    struct s_antaran_fortress {
        s_antaran_fortress_primary_weapon primary_weapons[4];
        s_antaran_fortress_weapon overflow_weapons[2];
        uint8_t specials[8];
    };

    extern s_monster_config monster;
    extern s_crystal_config crystal;
    extern s_antaran_fortress antaran_fortress;

    extern s_ship_design guardian_template;
    extern s_ship_design avenger_template;

    extern s_ship_design amoeba_templates[2];
    extern s_ship_design dragon_templates[2];
    extern s_ship_design eel_templates[2];
    extern s_ship_design hydra_templates[2];

    extern s_ship_design antaran_offensive_templates[GAME_DIFFICULTY_COUNT][5];
    extern s_ship_design antaran_defensive_templates[GAME_DIFFICULTY_COUNT][5];

} // namespace SHIP_CONFIG