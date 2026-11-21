#pragma once

#include <cstdint>

#include "consts.h"
#include "object_ids.h"
#include "orion2_consts.h"

// Population identities are player slots, androids, and natives, not stock-race IDs.
inline constexpr int COLONY_POP_RACE_COUNT = 10;

struct s_colony {
    int8_t owner;
    int8_t allocated_to;
    game::id_planet planet_id = game::NO_PLANET;
    int16_t officer;
    uint8_t outpost_flag;
    int8_t morale;
    int16_t pollution;
    uint8_t n_pops;
    int8_t specialty;
    uint32_t pop[MAX_COLONY_POPULATION];
    int16_t pop_roundoff[COLONY_POP_RACE_COUNT];
    int16_t pop_growth[COLONY_POP_RACE_COUNT];
    int8_t n_turns_existed;
    uint8_t food2_per_farmer;
    uint8_t industry_per_worker;
    int8_t research_per_scientist;
    uint8_t max_farms;
    int8_t max_population;
    uint8_t climate;
    int16_t ground_strength;
    int16_t space_strength;
    int16_t production[ECON_COUNT];
    uint8_t maintenance[ECON_COUNT];
    int16_t imports[ECON_COUNT];
    int8_t n_industry_recyclers;
    uint8_t food2_needed_for_our_empire;
    uint8_t food2_needed_for_assimilated;
    uint8_t food2_needed_for_conquered;
    uint8_t food2_needed_for_natives;
    uint8_t industry2_needed_for_our_empire;
    uint8_t industry2_needed_for_androids;
    uint8_t industry2_needed_for_assimilated;
    uint8_t industry2_needed_for_conquered;
    int8_t food2_needed_for_empire[8];
    int8_t industry2_needed_for_empire[8];
    int8_t n_food_replicated;
    int16_t producing[7];
    int16_t just_produced;
    int16_t production_spent;
    int16_t n_industry_taxed;
    uint8_t auto_building;
    int16_t production_surplus;
    int16_t bought_outright;
    int8_t occupation_points;
    int8_t occupation_policy;
    int16_t military[2];
    int8_t tank_roundoff;
    int8_t infantry_roundoff;
    uint8_t buildings[BUILDING_COUNT];
    uint16_t last_turn_building_destroyed;
    uint8_t buildings_scrapped_this_turn;

    bool operator==(const s_colony&) const = default;
};

static_assert(alignof(s_colony) >= alignof(uint32_t));
