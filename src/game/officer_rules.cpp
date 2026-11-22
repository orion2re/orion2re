#include "pch.h"

using game::current_state;

#include "officer_roster.h"

namespace OFFICER {
    uint8_t leader_warlord_bonus = 0;
    uint8_t leaders_no_limit = 0;
    s_commando_hire_cost commando_hire_cost = {1, 3};
    uint8_t megawealth_doubles_hire_cost = 1;
    int16_t leader_page;

    int16_t Leader_Type_Limit() {
        return leaders_no_limit != 0 ? MAX_LEADERS : LEADERS_PER_PAGE;
    }

    int16_t Total_Leader_Limit() {
        return leaders_no_limit != 0 ? MAX_LEADERS : 8;
    }

    bool Leader_Is_Displayed(const s_leader_data& leader) {
        return leader.player_index == MOX::PLAYER_NUM &&
            leader.status >= 0 &&
            leader.type == (Is_Fleet_Officer_Screen() ? LEADER_TYPE_SHIP : LEADER_TYPE_COLONY);
    }

    int16_t __cdecl Get_Officer_Level(int16_t leader_idx) {
        int16_t current_xp = current_state.leaders.at(leader_idx).xp;

        int16_t level = Get_Officer_Base_Level(current_xp, 0);

        if (level > 5) {
            level = 5;
        }

        return level;
    }

    int16_t __cdecl Get_Officer_Base_Level(int16_t xp, int8_t warlord) {
        int16_t level = 0;
        for (int16_t threshold_idx = 4; threshold_idx >= 0; --threshold_idx) {
            if (xp >= progression.experience_thresholds[threshold_idx]) {
                level = (int16_t)(threshold_idx + 1);
                break;
            }
        }

        if (level == 5) {
            --level;
        }

        if (warlord != 0 && (leader_warlord_bonus != 0 || xp >= progression.experience_thresholds[4])) {
            ++level;
        }

        return level;
    }

    int32_t __cdecl Officer_Skill_Bonus(uint32_t unused, int16_t leader_idx, int16_t skill_idx) {
        int16_t level;
        (void)unused;

        if (current_state.leaders.at(leader_idx).player_index == -1) {
            level = Get_Officer_Level(leader_idx);
        } else {
            level = Owned_Officer_Level(leader_idx);
        }

        int32_t current_level_val = static_cast<int32_t>(level) + 1;
        int32_t skill_div = (int32_t)MOX::skill_data[skill_idx].level_up;
        int32_t bonus_base = (int32_t)MOX::skill_data[skill_idx].strength;

        int32_t intermediate = (current_level_val + skill_div - 1) / skill_div;

        return (intermediate * bonus_base) / 10;
    }

    int16_t __cdecl Owned_Officer_Level(int16_t leader_idx) {
        const auto& leader = current_state.leaders.at(leader_idx);
        const auto owner = leader.player_index;
        const bool warlord = owner >= 0 && owner < MAX_PLAYERS && MOX::player[owner].traits[TRAIT_WARLORD] != 0;
        return Owned_Officer_Level(leader, warlord);
    }

    int16_t Owned_Officer_Level(const s_leader_data& leader, bool warlord) {
        // The special leader at catalog slot 66 never gains a Warlord level.
        return std::min<int16_t>(5, Get_Officer_Base_Level(leader.xp, warlord && leader.id.id != 0x42));
    }

    int16_t __cdecl Officer_Skill_Value(const game::s_game_state& state, int16_t leader_index) {
        int32_t total_value = 0;
        const auto& leader = state.leaders.at(leader_index);

        for (int16_t skill_idx = 0; skill_idx < 54; ++skill_idx) {
            s_skill_data& skill = MOX::skill_data[skill_idx];

            if (skill.skill_type == 0) {
                if ((skill.skill_mask & leader.general_skills) != 0) {
                    int32_t val = skill.cost;
                    if (skill_idx == 2 || skill_idx == 3) {
                        const int32_t commando_cost = (leader.type == LEADER_TYPE_SHIP)
                            ? commando_hire_cost.ship
                            : commando_hire_cost.colony;
                        val = commando_cost;
                        if (skill_idx == 3) {
                            val += commando_cost;
                        }
                    }
                    total_value += val;
                }
            }

            if (leader.type == LEADER_TYPE_SHIP) {
                if (skill.skill_type == 1) {
                    if ((skill.skill_mask & leader.special_skills) != 0) {
                        total_value += skill.cost;
                    }
                }
            } else if (leader.type == LEADER_TYPE_COLONY) {
                if (skill.skill_type == 2) {
                    if ((skill.skill_mask & leader.special_skills) != 0) {
                        total_value += skill.cost;
                    }
                }
            }
        }

        for (int i = 0; i < 3; ++i) {
            uint8_t app_idx = leader.tech_application[i];
            if (app_idx != 0) {
                int16_t field_id = TECHDATA::technology_applications[app_idx].tech_field_id;
                if (field_id <= TECH_FIELD_INVALID) {
                    // Logic from disassembly: resets total_value to 1 if field_id is invalid/negative
                    total_value = 1;
                } else {
                    total_value += OFFICER::Get_Research_Cost_Skill_Value(TECHDATA::technology_fields[field_id].cost);
                }
            }
        }

        return static_cast<int16_t>(total_value);
    }

    bool __cdecl Officer_Has_General_Skill(int16_t player_num, int16_t leader_index, int16_t skill_id) {
        return (MOX::skill_data[skill_id].skill_mask & current_state.leaders.at(leader_index).general_skills) != 0;
    }

    bool __cdecl Officer_Has_Special_Skill(int16_t player_num, int16_t leader_idx, int16_t skill_idx) {
        uint32_t mask = MOX::skill_data[skill_idx].skill_mask;
        uint32_t leader_skills = current_state.leaders.at(leader_idx).special_skills;

        return (mask & leader_skills) != 0;
    }

    int16_t __cdecl Get_Research_Cost_Skill_Value(int32_t cost) {
        if (cost < 1000) {
            return 1;
        }

        if (cost >= 5000) {
            return 4;
        }

        return 3;
    }

    void __cdecl Increase_Officer_Level_By_One(int16_t leader_index) {
        auto& leader = current_state.leaders.at(leader_index);
        int16_t current_level = Get_Officer_Base_Level(leader.xp, 0);

        // Increase level if not already at max (level 5 / Elite)
        if (current_level >= 0 && current_level < 5 &&
            progression.experience_thresholds[current_level] <= 30000) {
            leader.xp = (int16_t)progression.experience_thresholds[current_level];
        }
    }

    int32_t __cdecl Officer_Cost(int16_t player_idx, int16_t leader_idx) {
        uint16_t level_data = Get_Officer_Level(leader_idx);
        int16_t level_multiplier = (int16_t)((level_data & 0xFF) + 1);

        int32_t base_calc =
            static_cast<int32_t>(current_state.leaders.at(leader_idx).skill_value) *
            level_multiplier;
        int16_t bonus = Bonus_For_Famous_Heroes(player_idx);

        int32_t total_cost = base_calc * 10;

        if (megawealth_doubles_hire_cost != 0 &&
            (Officer_Has_General_Skill(player_idx, leader_idx, 28) ||
                Officer_Has_General_Skill(player_idx, leader_idx, 29))) {
            total_cost = base_calc * 20;
        }

        int32_t final_cost = total_cost - static_cast<int32_t>(bonus);

        if (final_cost < 1) {
            return 0;
        }

        return final_cost;
    }

    bool Is_Fleet_Officer_Screen() {
        return MOX::officer_scrn_type == LEADER_TYPE_SHIP;
    }

    int16_t __cdecl Bonus_For_Famous_Heroes(int16_t player_idx) {
        int16_t max_bonus = 0;
        int16_t current_skill_bonus = 0;

        for (int16_t leader_idx = 0; leader_idx < MAX_LEADERS; ++leader_idx) {
            auto& leader = current_state.leaders.at(leader_idx);

            // Check if the leader belongs to the player and is in a valid status (0 or 1)
            if (leader.player_index == player_idx && (leader.status == 0 || leader.status == 1)) {
                bool has_famous = Officer_Has_General_Skill(player_idx, leader_idx, 10 /* FAMOUS */);
                if (has_famous) {
                    current_skill_bonus = (int16_t)Officer_Skill_Bonus(player_idx, leader_idx, 10 /* FAMOUS */);
                } else {
                    bool has_super_famous = Officer_Has_General_Skill(player_idx, leader_idx, 11 /* SUPER_FAMOUS */);
                    if (has_super_famous) {
                        current_skill_bonus = (int16_t)Officer_Skill_Bonus(player_idx, leader_idx, 11 /* SUPER_FAMOUS */);
                    }
                }

                // Track the maximum bonus found across all valid leaders
                if (current_skill_bonus > max_bonus) {
                    max_bonus = current_skill_bonus;
                }
            }
        }

        return max_bonus;
    }

    bool __cdecl Player_Can_Hire_More_Officers(int16_t player_idx) {
        int16_t fleet_officers = N_Fleet_Officers_Owned_By_Player(player_idx);
        int16_t star_officers = N_Star_Officers_Owned_By_Player(player_idx);

        return static_cast<int32_t>(fleet_officers) +
            static_cast<int32_t>(star_officers) <
            Total_Leader_Limit();
    }

    static bool Officer_Status_Ok(int8_t status) {
        return status == 0 || status == 1 || status == 3 || status == 4;
    }

    int16_t __cdecl N_Fleet_Officers_Owned_By_Player(int16_t player_index) {
        int16_t count = 0;

        for (const auto& leader : current_state.leaders) {
            if (leader.player_index == player_index && leader.type == LEADER_TYPE_SHIP && Officer_Status_Ok(leader.status)) {
                ++count;
            }
        }

        return count;
    }

    int16_t __cdecl N_Star_Officers_Owned_By_Player(int16_t player_index) {
        int16_t count = 0;

        for (const auto& leader : current_state.leaders) {
            if (leader.player_index == player_index && leader.type == LEADER_TYPE_COLONY && Officer_Status_Ok(leader.status)) {
                ++count;
            }
        }

        return count;
    }

    void __cdecl Build_Captain_Id_List() {
        int16_t count = 0;
        int16_t matching_leaders = 0;
        const int16_t first_leader =
            static_cast<int16_t>(leader_page * LEADERS_PER_PAGE);

        for (int16_t i = 0; i < MAX_LEADERS; i++) {
            MOX::officer_id_list[i] = -1;
        }

        for (int16_t i = MAX_LEADERS - 1; i >= 0; --i) {
            if (!Leader_Is_Displayed(current_state.leaders.at(i))) {
                continue;
            }
            if (matching_leaders++ < first_leader) {
                continue;
            }

            MOX::officer_id_list[count++] = i;
            if (count >= LEADERS_PER_PAGE) {
                break;
            }
        }

        if (count == 0 && leader_page > 0) {
            leader_page = 0;
            Build_Captain_Id_List();
            return;
        }

        MOX::n_fltscrn_officers = count;
    }
} // namespace OFFICER
