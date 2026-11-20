#pragma once

#include <cstdint>

struct s_leader_data {
    char name[15];
    char title[20];
    uint8_t type;
    int16_t xp;
    uint32_t general_skills;
    uint32_t special_skills;
    uint8_t tech_application[3];
    uint8_t pict_num;
    int16_t skill_value;
    uint8_t level;
    int16_t location;
    int8_t eta;
    char display_level_popup;
    int8_t status;
    int8_t player_index;

    bool operator==(const s_leader_data&) const = default;
};
