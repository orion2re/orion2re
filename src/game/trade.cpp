#include "pch.h"

namespace TRADE {
    void __cdecl Process_Research_Agreement(s_player* player, int16_t other_player_idx) {
        int32_t goal = Research_Agreement_Goal(player, other_player_idx);
        int16_t current_level = player->current_research_agreement_level[other_player_idx];

        if (current_level < goal) {
            int16_t increment = 0;
            int16_t remainder = goal % 5;

            if (remainder != 0) {
                if ((int16_t)game_random::Random(5) <= remainder) {
                    increment = 1;
                }
            }

            int16_t next_level = (goal / 5) + current_level + increment;

            if (next_level <= goal) {
                current_level = next_level;
            } else {
                current_level = goal;
            }
        } else {
            current_level = goal;
        }

        player->research_agreement_goal[other_player_idx] = goal;
        player->current_research_agreement_level[other_player_idx] = current_level;
    }

    void __cdecl Process_Trade_Agreement(s_player* player, int16_t other_player_idx) {
        int16_t goal = Trade_Agreement_Goal(&game::world, player, other_player_idx);
        int16_t current_level = player->current_trade_agreement_level[other_player_idx];

        if (current_level < goal) {
            int16_t increment = 0;
            int16_t remainder = goal % 5;

            if (remainder != 0) {
                if ((int16_t)game_random::Random(5) <= remainder) {
                    increment = 1;
                }
            }

            current_level += (goal / 5) + increment;

            if (current_level > goal) {
                current_level = goal;
            }
        } else {
            current_level = goal;
        }

        player->trade_agreement_goal[other_player_idx] = goal;
        player->current_trade_agreement_level[other_player_idx] = current_level;
    }

    void __cdecl Process_Trade_And_Research_Agreements() {
        for (int32_t i = 0; i < MOX::NUM_PLAYERS; ++i) {
            s_player* current_player = &MOX::player[i];

            for (int16_t other_idx = 0; other_idx < MOX::NUM_PLAYERS; ++other_idx) {
                if (current_player->trade_treaty[other_idx] != 0) {
                    Process_Trade_Agreement(current_player, other_idx);
                }

                if (current_player->research_treaty[other_idx] != 0) {
                    Process_Research_Agreement(current_player, other_idx);
                }
            }
        }

        Compute_N_Trade_And_Research_Pops();
    }

    int16_t __cdecl Base_Trade_Agreement_Goal(s_player* player, int16_t other_player_idx) {
        int16_t other_trade_pops = MOX::player[other_player_idx].n_trade_pops;
        int16_t player_trade_pops = player->n_trade_pops;

        int16_t min_pops = (other_trade_pops < player_trade_pops) ? other_trade_pops : player_trade_pops;
        int16_t goal = min_pops / 2;

        uint8_t player_idx = (uint8_t)(player - MOX::player);
        uint8_t other_idx = (uint8_t)other_player_idx;

        if (EVENTS::Event_Check_Pirate_Activity(player_idx) != 0 ||
            EVENTS::Event_Check_Pirate_Activity(other_idx) != 0) {
            goal /= 2;
        }

        return goal;
    }

    int32_t __cdecl Get_Combined_Research_Points(s_player* player, int16_t other_player_idx) {
        int16_t other_research_pops = MOX::player[other_player_idx].n_research_pops;
        int16_t player_research_pops = player->n_research_pops;

        int16_t min_research_pops = other_research_pops;
        if (player_research_pops < other_research_pops) {
            min_research_pops = player_research_pops;
        }

        return (int32_t)min_research_pops / 2;
    }

    int16_t __cdecl Trade_Agreement_Goal(game::s_world_state* world_state, s_player* player, int16_t other_player_idx) {
        int32_t trait_multiplier = 100;
        int16_t base_goal = Base_Trade_Agreement_Goal(player, other_player_idx);

        if (player->traits[TRAIT_CURRENT_GOVERNMENT] == GOVERNMENT_DEMOCRACY) {
            trait_multiplier = 150;
        } else if (player->traits[TRAIT_CURRENT_GOVERNMENT] == GOVERNMENT_FEDERATION) {
            trait_multiplier = 175;
        }

        if (player->traits[TRAIT_FANTASTIC_TRADERS] != 0) {
            trait_multiplier += 50;
        }

        int16_t max_leader_bonus = 0;
        int32_t current_player_index = (int32_t)(player - MOX::player);

        for (const auto& leader_entry : world_state->leaders) {
            const int32_t i = static_cast<int32_t>(leader_entry.first.id);
            s_leader_data* leader = &game::Leader(world_state, i);

            if ((int32_t)leader->player_index == current_player_index &&
                leader->status >= 0 && leader->status < 3) {
                int32_t leader_bonus = 0;
                if (leader->general_skills & LEADER_GENERAL_SKILL_TRADER2) {
                    int16_t level = OFFICER::Owned_Officer_Level(world_state, i);
                    leader_bonus = (level * 3 + 3) * 5;
                } else if (leader->general_skills & LEADER_GENERAL_SKILL_TRADER) {
                    int16_t level = OFFICER::Owned_Officer_Level(world_state, i);
                    leader_bonus = (level * 5 + 5) * 2;
                }

                if (max_leader_bonus < (int16_t)leader_bonus) {
                    max_leader_bonus = (int16_t)leader_bonus;
                }
            }
        }

        return ((int32_t)base_goal * (max_leader_bonus + trait_multiplier)) / 100;
    }

    int32_t __cdecl Research_Agreement_Goal(s_player* player, int16_t other_player_idx) {
        int32_t combined_points = Get_Combined_Research_Points(player, other_player_idx);
        int16_t truncated_combined_points = (int16_t)combined_points;

        switch (player->traits[TRAIT_CURRENT_GOVERNMENT]) {
            case GOVERNMENT_FEUDAL:
                return (int16_t)((int32_t)truncated_combined_points / 2);
            case GOVERNMENT_CONFEDERATION:
                return (int16_t)(((int32_t)truncated_combined_points * 3) / 4);
            case GOVERNMENT_DEMOCRACY:
                return (int16_t)(((int32_t)truncated_combined_points * 3) / 2);
            case GOVERNMENT_FEDERATION:
                return (int16_t)(((int32_t)truncated_combined_points * 7) / 4);
            default:
                return combined_points;
        }
    }

    void __cdecl Compute_N_Trade_And_Research_Pops() {
        for (int32_t i = 0; i < MOX::NUM_PLAYERS; ++i) {
            MOX::player[i].n_trade_pops = 0;
            MOX::player[i].n_research_pops = 0;
        }

        for (int16_t i = 0; i < MOX::NUM_COLONIES; ++i) {
            s_colony& colony = MOX::colony[i];

            if (colony.owner != -1) {
                int16_t colony_trade_pops = 0;
                int16_t colony_research_pops = 0;
                uint8_t n_pops = (uint8_t)colony.n_pops;

                for (int32_t j = 0; j < n_pops; ++j) {
                    uint32_t pop_unit = colony.pop[j];
                    uint32_t type = pop_unit & 0x0F;

                    if (type < 8) {
                        colony_trade_pops++;
                        colony_research_pops++;
                    } else if (type == 8 && (pop_unit & 0x180) == 0x100) {
                        colony_research_pops++;
                    }
                }

                MOX::player[colony.owner].n_trade_pops += colony_trade_pops;
                MOX::player[colony.owner].n_research_pops += colony_research_pops;
            }
        }
    }

    void __cdecl Start_Trade_Agreement(s_player* player, int16_t other_player_idx) {
        int16_t base_goal = TRADE::Base_Trade_Agreement_Goal(player, other_player_idx);
        int16_t player_idx = (int16_t)(player - MOX::player);
        s_player* other_player = &MOX::player[other_player_idx];

        player->trade_treaty[other_player_idx] = 1;
        player->current_trade_agreement_level[other_player_idx] = -base_goal;
        player->trade_agreement_goal[other_player_idx] = TRADE::Trade_Agreement_Goal(&game::world, player, other_player_idx);

        other_player->trade_treaty[player_idx] = 1;
        other_player->current_trade_agreement_level[player_idx] = -base_goal;
        other_player->trade_agreement_goal[player_idx] = TRADE::Trade_Agreement_Goal(&game::world, other_player, player_idx);

        COLCALC::Update_Player_Stats(&game::world, player);
        COLCALC::Update_Player_Stats(&game::world, other_player);
    }

    void __cdecl Start_Research_Agreement(s_player* player, int16_t other_player_idx) {
        int16_t combined_points;
        int16_t player_idx;
        s_player* other_player;
        int32_t agreement_goal;

        combined_points = (int16_t)TRADE::Get_Combined_Research_Points(player, other_player_idx);
        player_idx = (int16_t)(player - MOX::player);

        player->research_treaty[other_player_idx] = 1;
        player->current_research_agreement_level[other_player_idx] = -combined_points;

        other_player = &MOX::player[other_player_idx];

        agreement_goal = TRADE::Research_Agreement_Goal(player, other_player_idx);
        player->research_agreement_goal[other_player_idx] = agreement_goal;

        other_player->research_treaty[player_idx] = 1;
        other_player->current_research_agreement_level[player_idx] = -combined_points;

        agreement_goal = TRADE::Research_Agreement_Goal(other_player, player_idx);
        other_player->research_agreement_goal[player_idx] = agreement_goal;

        COLCALC::Update_Player_Stats(&game::world, player);
        COLCALC::Update_Player_Stats(&game::world, other_player);
    }
} // namespace TRADE