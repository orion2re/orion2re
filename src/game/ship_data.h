#pragma once

#include <cstdint>

#include "ship_location.h"
#include "unaligned.h"

#pragma pack(push, 1)

struct s_ship_weapons {
    int16_t type;
    int8_t count;
    int8_t current_count;
    int8_t firing_arc;
    uint16_t specials;
    int8_t ammo;

    bool operator==(const s_ship_weapons&) const = default;
};

struct s_ship_design {
    char name[16];
    int8_t size;
    int8_t ship_type;
    int8_t shield_type;
    int8_t ftl_type;
    uint8_t speed;
    int8_t computer_type;
    int8_t armor_type;
    int8_t special_device_flags[5];
    s_ship_weapons ship_weapon[8];
    int8_t picture_num;
    uint8_t previous_owner;
    int16_t cost;
    uint8_t combat_speed;
    uint16_t date_of_design;

    bool operator==(const s_ship_design&) const = default;
};

#pragma pack(pop)

struct s_ship_data {
    s_ship_design d;
    int8_t owner;
    int8_t status;
    game::s_stored_ship_location position;
    int16_t x;
    int16_t y;
    int8_t group_has_navigator;
    uint8_t travelling_speed;
    uint8_t turns_left;
    int8_t shield_damage_percent;
    int8_t drive_damage_percent;
    int8_t computer_damage;
    int8_t crew_quality;
    uint16_t crew_experience;
    int16_t officer_index;
    uint8_t special_device_damage_flags[5];
    int16_t armor_damage;
    int16_t structural_damage;
    int8_t mission;
    int8_t just_built;
    game::id_star prior_jump_gate_destination{};
    int32_t prior_jump_gate_stardate;
    game::id_planet colonization_target = game::NO_PLANET;
    game::id_ship id = game::NO_SHIP;

    bool operator==(const s_ship_data&) const = default;
};
