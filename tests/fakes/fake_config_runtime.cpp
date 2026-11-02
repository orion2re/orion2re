#include "pch.h"

namespace MOX {
#ifndef TEST_CONFIG_RUNTIME_EXTERNAL_SETTINGS
    s_settings settings = {};
#endif
#ifdef TEST_CONFIG_RUNTIME_EXTERNAL_KEN
    int16_t NUM_PLAYERS = 1;
    s_player _config_test_players[1] = {};
    s_player* player = _config_test_players;
#endif
#ifndef TEST_CONFIG_RUNTIME_EXTERNAL_SHIPS
    int16_t NUM_SHIPS;
#endif
    uint8_t food_per_farmer_table[PLANET_CLIMATE_COUNT] = {
        0,
        0,
        0,
        2,
        2,
        4,
        4,
        2,
        4,
        6,
    };
    int16_t ranged_to_hit_penalty[9] = {};
    int16_t ranged_damage_penalty[9] = {};
    uint8_t star_class_table[STAR_CLASS_COUNT][GALAXY_AGE_COUNT] = {
        {20, 10, 5},
        {25, 15, 5},
        {10, 16, 30},
        {10, 16, 21},
        {32, 37, 30},
        {1, 2, 3},
        {2, 4, 6},
    };
    int8_t class_to_num_satellites[10][6] = {
        {0, 0, 1, 2, 0, 0},
        {1, 1, 2, 2, 1, 0},
        {1, 1, 2, 2, 1, 0},
        {2, 1, 2, 3, 1, 0},
        {3, 2, 3, 3, 2, 0},
        {3, 2, 3, 4, 2, 0},
        {4, 3, 4, 4, 2, 0},
        {4, 3, 4, 5, 3, 1},
        {5, 4, 5, 5, 3, 1},
        {5, 4, 5, 5, 4, 1},
    };
    uint8_t orbit_to_satellite_type[10][5] = {
        {1, 1, 1, 1, 1},
        {4, 1, 1, 1, 2},
        {3, 2, 1, 2, 2},
        {3, 3, 2, 2, 2},
        {3, 3, 2, 2, 2},
        {3, 3, 3, 3, 2},
        {3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3},
        {3, 3, 3, 3, 3},
    };
    uint8_t planet_size_table[PLANET_SIZE_COUNT] = {1, 3, 7, 9, 10};
    int16_t first_field_in_group[10] = {0, 18, 55, 57, 29, 7, 22, 28, 10, 74};
    uint8_t normal_gal_climate_roll_table[PLANET_CLIMATE_COUNT][PLANET_GROUP_COUNT] = {
        {15, 15, 10, 20},
        {55, 50, 15, 0},
        {25, 25, 10, 70},
        {5, 10, 10, 0},
        {0, 5, 10, 8},
        {0, 0, 10, 2},
        {0, 0, 11, 0},
        {0, 0, 11, 0},
        {0, 0, 11, 0},
        {0, 0, 2, 0},
    };
    uint8_t old_gal_climate_roll_table[PLANET_CLIMATE_COUNT][PLANET_GROUP_COUNT] = {
        {15, 5, 5, 20},
        {40, 30, 8, 0},
        {20, 20, 8, 50},
        {25, 25, 13, 0},
        {0, 20, 13, 30},
        {0, 0, 13, 0},
        {0, 0, 13, 0},
        {0, 0, 13, 0},
        {10, 0, 0, 0},
        {4, 0, 0, 0},
    };
    uint8_t planet_special = 64;
    uint8_t planet_special_weighted_chance[SYSTEM_SPECIAL_COUNT] = {
        64,
        5,
        3,
        2,
        3,
        2,
        9,
        4,
        3,
        0,
        5,
        0,
    };
} // namespace MOX

namespace antarans {
    s_antaran_fleet_config fleet_config = {
        .growth_delay = {200, 100, 0},
        .resource_growth = {
            .frequency = 25,
            .offensive_multiplier = {100, 150, 200},
            .defensive_multiplier = {100, 150, 200},
        },
        .attack = {
            .roll_range = 200,
            .minimum_offensive_force_shift = 2,
            .impossible_human_weight_multiplier = 3,
            .lucky_victim_weight_divisor = 3,
        },
        .ship_limit = {
            .offensive = {4, 4, 3, 2, 2},
            .defensive = {0, 0, 3, 2, 7},
        },
        .ship_cost = {2, 5, 12, 30, 75},
        .retirement = {
            {100, {100, 150, 200}},
            {199, {100, 150, 200}},
        },
    };
}

namespace COLBLDG {
    bool warn_blockers = true;
}

namespace platform {
    uint8_t g_vsync_enabled = 1;
    e_present_scaler g_present_scaler = PRESENT_SCALER_PIXELART;
    int32_t g_window_width = 640;
    int32_t g_window_height = 480;
    uint8_t g_fullscreen = 0;
} // namespace platform

namespace debug_overlay {
    uint8_t g_enabled;
    uint16_t g_font_size = 10;
    uint16_t g_ui_scale_percent = 80;
} // namespace debug_overlay

namespace ERIC {
    s_fade_speed fade_speed = {25, 25};
}

namespace MAINSCR {
    uint8_t wormhole_link_color = 4;
}

namespace NEWPUP {
    uint8_t rebel_display;
}

namespace REPORT {
    uint8_t no_outpost_prompt;
}

namespace MAPGEN {
    int16_t star_count_by_galaxy_size[GALAXY_SIZE_COUNT] = {20, 36, 54, 71, 99, 71};
    int16_t& maximum_star_count = star_count_by_galaxy_size[GALAXY_SIZE_MAXIMUM];
    s_map_layout layout[GALAXY_SIZE_COUNT] = {
        {0, 506, 400, 10, 10, 0, 0},
        {0, 759, 600, 15, 15, 1, 1},
        {0, 1012, 800, 20, 20, 2, 2},
        {0, 1518, 1200, 30, 30, 3, 3},
        {1, 1800, 1350, 30, 36, 3, 3},
        {0, 1011, 800, 15, 20, 3, 3},
    };
    s_planet_generation_config planet_generation_config = {};
    s_home_system_config home_system_config = {3};
    s_special_generation_config special_generation_config = {1, 1, 1, 1, 1, 1};
    s_monster_generation_config monster_generation_config = {
        255, 0, 0, 0, {5, 5, 5, 8, 12}, 20};
    int16_t planet_limit = 250;
    uint8_t starting_reveal_radius;
    uint8_t star_visual_size_weight[3] = {3, 4, 3};
    s_nebula_count_config nebula_count[GALAXY_SIZE_COUNT] = {
        {0, 1, 0},
        {1, 2, 0},
        {1, 3, 0},
        {2, 4, 0},
        {2, 4, 1},
        {2, 4, 0},
    };
    uint8_t satellite_orbit_weight[5][GALAXY_AGE_COUNT] = {
        {25, 20, 10},
        {18, 20, 22},
        {17, 20, 30},
        {15, 20, 33},
        {25, 20, 5},
    };
    int8_t environment_group[6][5] = {
        {PLANET_GROUP_BOMBARDED, PLANET_GROUP_BOMBARDED, PLANET_GROUP_BOMBARDED, PLANET_GROUP_BOMBARDED, PLANET_GROUP_HOSTILE},
        {PLANET_GROUP_BOMBARDED, PLANET_GROUP_BOMBARDED, PLANET_GROUP_HOSTILE, PLANET_GROUP_LIFE_SUPPORTING, PLANET_GROUP_LIFELESS},
        {PLANET_GROUP_BOMBARDED, PLANET_GROUP_HOSTILE, PLANET_GROUP_LIFE_SUPPORTING, PLANET_GROUP_LIFE_SUPPORTING, PLANET_GROUP_LIFELESS},
        {PLANET_GROUP_HOSTILE, PLANET_GROUP_LIFE_SUPPORTING, PLANET_GROUP_LIFE_SUPPORTING, PLANET_GROUP_LIFE_SUPPORTING, PLANET_GROUP_LIFELESS},
        {PLANET_GROUP_HOSTILE, PLANET_GROUP_LIFE_SUPPORTING, PLANET_GROUP_LIFELESS, PLANET_GROUP_LIFELESS, PLANET_GROUP_LIFELESS},
        {PLANET_GROUP_BOMBARDED, PLANET_GROUP_BOMBARDED, PLANET_GROUP_HOSTILE, PLANET_GROUP_LIFE_SUPPORTING, PLANET_GROUP_LIFELESS},
    };
    int8_t mineral_class[10][6] = {
        {MINERAL_RESOURCE_ABUNDANT, MINERAL_RESOURCE_POOR, MINERAL_RESOURCE_POOR, MINERAL_RESOURCE_ULTRA_POOR, MINERAL_RESOURCE_ULTRA_POOR, MINERAL_RESOURCE_POOR},
        {MINERAL_RESOURCE_ABUNDANT, MINERAL_RESOURCE_POOR, MINERAL_RESOURCE_POOR, MINERAL_RESOURCE_POOR, MINERAL_RESOURCE_ULTRA_POOR, MINERAL_RESOURCE_ABUNDANT},
        {MINERAL_RESOURCE_ABUNDANT, MINERAL_RESOURCE_ABUNDANT, MINERAL_RESOURCE_POOR, MINERAL_RESOURCE_POOR, MINERAL_RESOURCE_POOR, MINERAL_RESOURCE_ABUNDANT},
        {MINERAL_RESOURCE_ABUNDANT, MINERAL_RESOURCE_ABUNDANT, MINERAL_RESOURCE_ABUNDANT, MINERAL_RESOURCE_POOR, MINERAL_RESOURCE_POOR, MINERAL_RESOURCE_ABUNDANT},
        {MINERAL_RESOURCE_RICH, MINERAL_RESOURCE_ABUNDANT, MINERAL_RESOURCE_ABUNDANT, MINERAL_RESOURCE_POOR, MINERAL_RESOURCE_POOR, MINERAL_RESOURCE_ABUNDANT},
        {MINERAL_RESOURCE_RICH, MINERAL_RESOURCE_ABUNDANT, MINERAL_RESOURCE_ABUNDANT, MINERAL_RESOURCE_ABUNDANT, MINERAL_RESOURCE_POOR, MINERAL_RESOURCE_ABUNDANT},
        {MINERAL_RESOURCE_RICH, MINERAL_RESOURCE_RICH, MINERAL_RESOURCE_ABUNDANT, MINERAL_RESOURCE_ABUNDANT, MINERAL_RESOURCE_ABUNDANT, MINERAL_RESOURCE_RICH},
        {MINERAL_RESOURCE_RICH, MINERAL_RESOURCE_RICH, MINERAL_RESOURCE_RICH, MINERAL_RESOURCE_ABUNDANT, MINERAL_RESOURCE_ABUNDANT, MINERAL_RESOURCE_RICH},
        {MINERAL_RESOURCE_ULTRA_RICH, MINERAL_RESOURCE_RICH, MINERAL_RESOURCE_RICH, MINERAL_RESOURCE_ABUNDANT, MINERAL_RESOURCE_ABUNDANT, MINERAL_RESOURCE_RICH},
        {MINERAL_RESOURCE_ULTRA_RICH, MINERAL_RESOURCE_ULTRA_RICH, MINERAL_RESOURCE_ULTRA_RICH, MINERAL_RESOURCE_RICH, MINERAL_RESOURCE_ABUNDANT, MINERAL_RESOURCE_ULTRA_RICH},
    };
    int8_t planet_gravity[MINERAL_RESOURCE_COUNT][PLANET_SIZE_COUNT] = {};
} // namespace MAPGEN

namespace SHIPMOVE {
    uint8_t jump_gate_speed_bonus = 3;
    uint16_t warp_interdictor_range = 8100;
    bool jump_gate_pass_nebula;
} // namespace SHIPMOVE

namespace CMBTSPEC {
    uint8_t leader_dies_with_no;
    uint8_t compressor_per_size_class_damage;
} // namespace CMBTSPEC

namespace ERICNET {
    int16_t buildlist[INITIAL_BUILDING_CAPACITY] = {
        BUILDING_STAR_FORTRESS,
        BUILDING_BATTLESTATION,
        BUILDING_STAR_BASE,
        BUILDING_HYDROPONIC_FARM,
        BUILDING_MARINE_BARRACKS,
        BUILDING_BIOSPHERES,
        BUILDING_AUTOMATED_FACTORY,
        BUILDING_HOLO_SIMULATOR,
        BUILDING_SOIL_ENRICHMENT,
        BUILDING_ASTRO_UNIVERSITY,
        BUILDING_PLEASURE_DOME,
        BUILDING_RECYCLOTRON,
        BUILDING_ROBOTIC_FACTORY,
        BUILDING_SPACEPORT,
        BUILDING_ARMOR_BARRACKS,
        BUILDING_DEEP_CORE_MINE,
        BUILDING_CORE_WASTE_DUMP,
        BUILDING_POLLUTION_PROCESSOR,
        BUILDING_RESEARCH_LAB,
        BUILDING_CLONING_CENTER,
        BUILDING_SUBTERRANEAN_FARMS,
        BUILDING_FOOD_REPLICATORS,
        BUILDING_RADIATION_SHIELD,
        BUILDING_GRAVITY_GENERATOR,
        BUILDING_CURRENCY_EXCHANGE,
        BUILDING_GALACTIC_CYBERNET,
        BUILDING_FLUX_SHIELD,
        BUILDING_MISSILE_BASE,
        BUILDING_FIGHTER_GARRISON,
        BUILDING_GROUND_BATTERIES,
        BUILDING_AUTOLAB,
        BUILDING_ATMOSPHERE_RENEWER,
        BUILDING_BARRIER_SHIELD,
        BUILDING_ROBO_MINER_PLANT,
        BUILDING_STOCK_EXCHANGE,
        BUILDING_SUPERCOMPUTER,
        BUILDING_WEATHER_CONTROLLER,
        BUILDING_DIMENSIONAL_PORTAL,
        BUILDING_WARP_INTERDICTOR,
        BUILDING_ARTEMIS_SYSTEM_NET,
        BUILDING_STELLAR_CONVERTER,
        BUILDING_SPACE_ACADEMY,
        BUILDING_NO_BUILDING,
        BUILDING_NO_BUILDING,
    };
}

namespace npcdiplo {
    uint16_t ai_surrender_delay = 150;
}

namespace CMBTMIS {
    int8_t missile_initiative = -1;
}

namespace CMBTAI {
    uint8_t ships_without_ammo_stay;
    uint8_t human_fleet_auto_retreat;
} // namespace CMBTAI

namespace CMBTFIRE {
    s_mount_damage mount_dmg[HIGH_ENERGY_FOCUS_PRESENCE_COUNT] = {
        {50, 100, 150},
        {100, 150, 200},
    };
}

#ifndef TEST_CONFIG_RUNTIME_EXTERNAL_KEN
namespace KEN {
    uint8_t personalities[14][10] = {
        {3, 4, 4, 4, 4, 4, 4, 4, 5, 5},
        {1, 2, 2, 2, 2, 2, 2, 2, 3, 3},
        {1, 1, 0, 2, 2, 2, 2, 2, 2, 2},
        {0, 0, 1, 1, 1, 1, 1, 2, 2, 2},
        {1, 1, 2, 2, 3, 3, 3, 5, 5, 5},
        {3, 4, 4, 4, 4, 4, 4, 4, 5, 5},
        {0, 0, 0, 0, 0, 0, 0, 1, 2, 2},
        {0, 2, 2, 3, 3, 3, 3, 3, 3, 3},
        {0, 1, 1, 1, 1, 1, 1, 1, 2, 2},
        {3, 3, 4, 5, 5, 5, 5, 5, 5, 5},
        {1, 2, 2, 2, 2, 2, 2, 2, 3, 3},
        {0, 0, 0, 0, 0, 0, 0, 2, 2, 3},
        {3, 4, 4, 4, 4, 4, 5, 5, 5, 5},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    };
}
#endif

namespace DESBOX {
    s_mods_available mods_available = {};
}

namespace CMBTFIR2 {
    s_artemis_system_net artemis_system_net = {
        .random_total_damage = 401,
        .minimum_total_damage = 149,
        .counter = 20,
        .single_hit_damage = 20,
        .inherent_modifications = 0,
        .trigger_chance = {20, 30, 40, 50, 80, 100},
    };
    s_combat_bonus combat_bonus = {
        50,
        20,
        0,
        10,
        20,
        50,
        100,
        0,
        80,
        80,
        10,
        30,
        0,
        15,
    };
    s_ship_explosion ship_explosion = {5, 3};
    s_quantum_detonator quantum_detonator = {6};
    uint16_t systems_repair_percent = 50;
    s_defensive_fire_config defensive_fire = {
        .initiative_enabled = 0,
        .maximum_range = 8,
        .alternate_enabled = 0,
        .alternate_chance = {100, 95, 90, 80, 70, 50, 30, 10},
    };
} // namespace CMBTFIR2

namespace SCIENCE {
    uint8_t finds_presented_by_scientist;
}

namespace OFFICER {
    uint8_t leader_warlord_bonus;
    uint8_t leaders_no_limit;
    s_commando_hire_cost commando_hire_cost = {1, 3};
    uint8_t megawealth_doubles_hire_cost = 1;
} // namespace OFFICER

namespace DIP_SCRN {
    uint8_t noreport;
}

namespace diplomac {
    uint8_t alternate_diplomacy;
    int16_t starting_relations[STARTING_RELATIONS_RACE_COUNT][STARTING_RELATIONS_RACE_COUNT] = {
        {0, 0, -24, 24, 0, 0, -12, 0, -24, 0, 12, 0, 24, 0},
        {0, 0, -12, 24, 0, 0, 0, -12, 0, 0, 0, 0, 0, 0},
        {-24, -12, 0, 0, 12, 0, 0, 12, 0, 0, 0, 12, 0, 0},
        {24, 24, 0, 0, -24, 0, 0, 0, 24, -12, 0, -12, 0, 0},
        {0, 0, 12, -24, 0, 0, 0, 12, -12, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {-12, 0, 0, 0, 0, 0, 0, 0, -12, 0, -12, 12, 24, 0},
        {0, -12, 12, 0, 12, 0, 0, 0, 12, 0, 0, -12, -12, 0},
        {-24, 0, 0, 24, -12, 0, -12, 12, 0, 0, 0, 0, 12, 0},
        {0, 0, 0, -12, 0, 0, 0, 0, 0, 0, 0, 0, 12, 0},
        {12, 0, 0, 0, 0, 0, -12, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 12, -12, 0, 0, 12, -12, 0, 0, 0, 0, 0, 0},
        {24, 0, 0, 0, 0, 0, 24, -12, 12, 12, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    };
}

namespace aitech {
    uint8_t ai_hypertech_threshold[TECH_APP_COUNT - TECH_APP_BIOLOGY] = {};
}

namespace aidesign {
    bool allow_stealth_field = false;
    uint8_t ship_design_bhg_chance = 3;
    uint8_t ship_design_stellar_chance = 3;
    s_ship_design_allocations ship_design_allocations =
        default_ship_design_allocations;
    s_ship_design_config ship_design = {
        .special_priority = {
            .bio_weapon = {
                {SPECIAL_PHASE_SHIFTER, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_SUB_SPACE_TELEPORTER, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_AUGMENTED_ENGINES, AI_SPECIAL_ITEM_SPECIAL},
                {WEAPON_BIO_TERMINATOR, AI_SPECIAL_ITEM_WEAPON},
                {WEAPON_DEATH_SPORE, AI_SPECIAL_ITEM_WEAPON},
            },
            .capture = {
                {SPECIAL_TRANSPORTERS, AI_SPECIAL_ITEM_SPECIAL},
                {WEAPON_TRACTOR_BEAM, AI_SPECIAL_ITEM_WEAPON},
                {WEAPON_ASSAULT_SHUTTLE, AI_SPECIAL_ITEM_WEAPON},
                {SPECIAL_TROOP_PODS, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_WARP_DISSIPATOR, AI_SPECIAL_ITEM_SPECIAL},
            },
            .cloaking = {
                {SPECIAL_PHASING_CLOAK, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_CLOAKING_DEVICE, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_STEALTH_FIELD, AI_SPECIAL_ITEM_SPECIAL},
            },
            .beam_defense = {
                {SPECIAL_INERTIAL_NULLIFIER, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_DISPLACEMENT_DEVICE, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_AUGMENTED_ENGINES, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_INERTIAL_STABILIZER, AI_SPECIAL_ITEM_SPECIAL},
            },
            .missile_defense = {
                {SPECIAL_WIDE_AREA_JAMMER, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_MULTI_WAVE_ECM_JAMMER, AI_SPECIAL_ITEM_SPECIAL},
                {WEAPON_SPATIAL_COMPRESSOR, AI_SPECIAL_ITEM_WEAPON},
                {SPECIAL_ECM_JAMMER, AI_SPECIAL_ITEM_SPECIAL},
                {WEAPON_PULSAR, AI_SPECIAL_ITEM_WEAPON},
                {SPECIAL_DISPLACEMENT_DEVICE, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_LIGHTNING_FIELD, AI_SPECIAL_ITEM_SPECIAL},
                {WEAPON_ANTI_MISSILE_ROCKET, AI_SPECIAL_ITEM_WEAPON},
            },
            .armor = {
                {SPECIAL_AUTOMATED_REPAIR_UNIT, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_HEAVY_ARMOR, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_REINFORCED_HULL, AI_SPECIAL_ITEM_SPECIAL},
            },
            .shield = {
                {SPECIAL_HARD_SHIELDS, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_MULTI_PHASED_SHIELDS, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_SHIELD_CAPACITOR, AI_SPECIAL_ITEM_SPECIAL},
            },
            .beam_offense = {
                {SPECIAL_TIME_WARP_FACILITATOR, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_ACHILLES_TARGETING_UNIT, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_HIGH_ENERGY_FOCUS, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_RANGEMASTER_UNIT, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_STRUCTURAL_ANALYZER, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_HYPER_X_CAPACITORS, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_BATTLE_SCANNER, AI_SPECIAL_ITEM_SPECIAL},
            },
            .missile_offense = {
                {SPECIAL_TIME_WARP_FACILITATOR, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_FAST_MISSILE_RACKS, AI_SPECIAL_ITEM_SPECIAL},
            },
            .special_weapon = {
                {WEAPON_STELLAR_CONVERTER, AI_SPECIAL_ITEM_WEAPON},
                {WEAPON_BLACK_HOLE_GENERATOR, AI_SPECIAL_ITEM_WEAPON},
                {SPECIAL_ENERGY_ABSORBER, AI_SPECIAL_ITEM_SPECIAL},
                {WEAPON_PLASMA_WEB, AI_SPECIAL_ITEM_WEAPON},
                {WEAPON_GYRO_DESTABILIZER, AI_SPECIAL_ITEM_WEAPON},
            },
        },
        .special_device_priority = {
            {SPECIAL_QUANTUM_DETONATOR, AI_SPECIAL_ITEM_SPECIAL},
            {WEAPON_SPATIAL_COMPRESSOR, AI_SPECIAL_ITEM_WEAPON},
            {WEAPON_PULSAR, AI_SPECIAL_ITEM_WEAPON},
            {WEAPON_STASIS_FIELD, AI_SPECIAL_ITEM_WEAPON},
            {SPECIAL_PHASING_CLOAK, AI_SPECIAL_ITEM_SPECIAL},
            {SPECIAL_CLOAKING_DEVICE, AI_SPECIAL_ITEM_SPECIAL},
            {SPECIAL_STEALTH_FIELD, AI_SPECIAL_ITEM_SPECIAL},
            {SPECIAL_REFLECTION_FIELD, AI_SPECIAL_ITEM_SPECIAL},
            {SPECIAL_PHASE_SHIFTER, AI_SPECIAL_ITEM_SPECIAL},
            {SPECIAL_SUB_SPACE_TELEPORTER, AI_SPECIAL_ITEM_SPECIAL},
            {SPECIAL_WARP_DISSIPATOR, AI_SPECIAL_ITEM_SPECIAL},
            {SPECIAL_SCOUT_LAB, AI_SPECIAL_ITEM_SPECIAL},
        },
        .defense_priority = {
            .human_non_beams = {
                {SPECIAL_SECURITY_STATIONS, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_DISPLACEMENT_DEVICE, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_LIGHTNING_FIELD, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_HARD_SHIELDS, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_WIDE_AREA_JAMMER, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_MULTI_WAVE_ECM_JAMMER, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_HEAVY_ARMOR, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_INERTIAL_NULLIFIER, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_ECM_JAMMER, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_REINFORCED_HULL, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_INERTIAL_STABILIZER, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_AUGMENTED_ENGINES, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_SHIELD_CAPACITOR, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_MULTI_PHASED_SHIELDS, AI_SPECIAL_ITEM_SPECIAL},
                {WEAPON_ANTI_MISSILE_ROCKET, AI_SPECIAL_ITEM_WEAPON},
                {SPECIAL_REFLECTION_FIELD, AI_SPECIAL_ITEM_SPECIAL},
                {WEAPON_SPATIAL_COMPRESSOR, AI_SPECIAL_ITEM_WEAPON},
                {WEAPON_PULSAR, AI_SPECIAL_ITEM_WEAPON},
                {SPECIAL_AUTOMATED_REPAIR_UNIT, AI_SPECIAL_ITEM_SPECIAL},
            },
            .human_beams = {
                {SPECIAL_SECURITY_STATIONS, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_DISPLACEMENT_DEVICE, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_LIGHTNING_FIELD, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_HARD_SHIELDS, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_WIDE_AREA_JAMMER, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_MULTI_WAVE_ECM_JAMMER, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_HEAVY_ARMOR, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_INERTIAL_NULLIFIER, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_ECM_JAMMER, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_REINFORCED_HULL, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_INERTIAL_STABILIZER, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_AUGMENTED_ENGINES, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_SHIELD_CAPACITOR, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_MULTI_PHASED_SHIELDS, AI_SPECIAL_ITEM_SPECIAL},
                {WEAPON_ANTI_MISSILE_ROCKET, AI_SPECIAL_ITEM_WEAPON},
                {SPECIAL_REFLECTION_FIELD, AI_SPECIAL_ITEM_SPECIAL},
                {WEAPON_SPATIAL_COMPRESSOR, AI_SPECIAL_ITEM_WEAPON},
                {WEAPON_PULSAR, AI_SPECIAL_ITEM_WEAPON},
                {SPECIAL_AUTOMATED_REPAIR_UNIT, AI_SPECIAL_ITEM_SPECIAL},
            },
        },
    };
} // namespace aidesign

namespace aidudes {
    bool builds_colony_bases_prewarp;
}

namespace RACECUST {
    s_variants variants = {};
} // namespace RACECUST

namespace TEXT_FORMAT {
    s_format format[ROW_COUNT] = {};
}

namespace MOX {
    s_crew_data crew_data[CREW_QUALITY_COUNT] = {
        {0, 0, 0, 0},
        {50, 15, 15, 5},
        {150, 30, 30, 10},
        {500, 50, 50, 15},
        {1000, 75, 75, 20},
    };
    s_skill_data skill_data[54] = {};
} // namespace MOX