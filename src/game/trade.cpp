#include "pch.h"

namespace TRADE {
    void __cdecl Process_Research_Agreement(s_player* player, game::id_player other) {
        const auto other_player_idx = other.id;
        int32_t goal = Research_Agreement_Goal(player, other);
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

    void __cdecl Process_Trade_Agreement(s_player* player, game::id_player other) {
        const auto other_player_idx = other.id;
        int16_t goal = Trade_Agreement_Goal(player, other);
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
        for (int32_t i = 0; i < game::current_state.player_count; ++i) {
            s_player* current_player = &game::current_state.players[i];

            for (int16_t other_idx = 0; other_idx < game::current_state.player_count; ++other_idx) {
                const auto other = game::current_state.players[other_idx].id;
                if (current_player->trade_treaty[other_idx] != 0) {
                    Process_Trade_Agreement(current_player, other);
                }

                if (current_player->research_treaty[other_idx] != 0) {
                    Process_Research_Agreement(current_player, other);
                }
            }
        }

        Compute_N_Trade_And_Research_Pops();
    }

    int16_t __cdecl Base_Trade_Agreement_Goal(s_player* player, game::id_player other) {
        int16_t other_trade_pops = game::current_state.players.at(other.id).n_trade_pops;
        int16_t player_trade_pops = player->n_trade_pops;

        int16_t min_pops = (other_trade_pops < player_trade_pops) ? other_trade_pops : player_trade_pops;
        int16_t goal = min_pops / 2;

        if (EVENTS::Event_Check_Pirate_Activity(player->id) != 0 ||
            EVENTS::Event_Check_Pirate_Activity(other) != 0) {
            goal /= 2;
        }

        return goal;
    }

    int32_t __cdecl Get_Combined_Research_Points(s_player* player, game::id_player other) {
        int16_t other_research_pops = game::current_state.players.at(other.id).n_research_pops;
        int16_t player_research_pops = player->n_research_pops;

        int16_t min_research_pops = other_research_pops;
        if (player_research_pops < other_research_pops) {
            min_research_pops = player_research_pops;
        }

        return (int32_t)min_research_pops / 2;
    }

    int16_t __cdecl Trade_Agreement_Goal(s_player* player, game::id_player other) {
        int32_t trait_multiplier = 100;
        int16_t base_goal = Base_Trade_Agreement_Goal(player, other);

        if (player->traits[TRAIT_CURRENT_GOVERNMENT] == GOVERNMENT_DEMOCRACY) {
            trait_multiplier = 150;
        } else if (player->traits[TRAIT_CURRENT_GOVERNMENT] == GOVERNMENT_FEDERATION) {
            trait_multiplier = 175;
        }

        if (player->traits[TRAIT_FANTASTIC_TRADERS] != 0) {
            trait_multiplier += 50;
        }

        int16_t max_leader_bonus = 0;

        for (int32_t i = 0; i < MAX_LEADERS; ++i) {
            auto* leader = &game::current_state.leaders.at(i);

            if (leader->employer == player->id &&
                leader->status >= 0 && leader->status < 3) {
                int32_t leader_bonus = 0;
                if (leader->general_skills & LEADER_GENERAL_SKILL_TRADER2) {
                    int16_t level = OFFICER::Owned_Officer_Level(i);
                    leader_bonus = (level * 3 + 3) * 5;
                } else if (leader->general_skills & LEADER_GENERAL_SKILL_TRADER) {
                    int16_t level = OFFICER::Owned_Officer_Level(i);
                    leader_bonus = (level * 5 + 5) * 2;
                }

                if (max_leader_bonus < (int16_t)leader_bonus) {
                    max_leader_bonus = (int16_t)leader_bonus;
                }
            }
        }

        return ((int32_t)base_goal * (max_leader_bonus + trait_multiplier)) / 100;
    }

    int32_t __cdecl Research_Agreement_Goal(s_player* player, game::id_player other) {
        int32_t combined_points = Get_Combined_Research_Points(player, other);
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
        for (int32_t i = 0; i < game::current_state.player_count; ++i) {
            game::current_state.players[i].n_trade_pops = 0;
            game::current_state.players[i].n_research_pops = 0;
        }

        for (auto& [roster_id, roster_colony] : game::current_state.colonies) {
            s_colony& colony = roster_colony;

            if (legacy_world::Encode_Player_Slot(colony.owner) != -1) {
                int16_t colony_trade_pops = 0;
                int16_t colony_research_pops = 0;
                uint8_t n_pops = (uint8_t)colony.n_pops;

                for (int32_t j = 0; j < n_pops; ++j) {
                    const auto& pop_unit = colony.pop[j];

                    if (POP::Group_Player(pop_unit) != game::NO_PLAYER) {
                        colony_trade_pops++;
                        colony_research_pops++;
                    } else if (POP::Is_Android(pop_unit) && pop_unit.profession == POP::PROF_SCIENTIST) {
                        colony_research_pops++;
                    }
                }

                game::current_state.players[colony.owner.id].n_trade_pops += colony_trade_pops;
                game::current_state.players[colony.owner.id].n_research_pops += colony_research_pops;
            }
        }
    }

    void __cdecl Start_Trade_Agreement(s_player* player, game::id_player other) {
        int16_t base_goal = TRADE::Base_Trade_Agreement_Goal(player, other);
        const auto player_idx = player->id.id;
        const auto other_player_idx = other.id;
        s_player* other_player = &game::current_state.players[other_player_idx];

        player->trade_treaty[other_player_idx] = 1;
        player->current_trade_agreement_level[other_player_idx] = -base_goal;
        player->trade_agreement_goal[other_player_idx] = TRADE::Trade_Agreement_Goal(player, other);

        other_player->trade_treaty[player_idx] = 1;
        other_player->current_trade_agreement_level[player_idx] = -base_goal;
        other_player->trade_agreement_goal[player_idx] = TRADE::Trade_Agreement_Goal(other_player, player->id);

        COLCALC::Update_Player_Stats(player);
        COLCALC::Update_Player_Stats(other_player);
    }

    void __cdecl Start_Research_Agreement(s_player* player, game::id_player other) {
        const auto combined_points = (int16_t)TRADE::Get_Combined_Research_Points(player, other);
        const auto player_idx = player->id.id;
        const auto other_player_idx = other.id;

        player->research_treaty[other_player_idx] = 1;
        player->current_research_agreement_level[other_player_idx] = -combined_points;

        auto* other_player = &game::current_state.players[other_player_idx];

        player->research_agreement_goal[other_player_idx] = TRADE::Research_Agreement_Goal(player, other);

        other_player->research_treaty[player_idx] = 1;
        other_player->current_research_agreement_level[player_idx] = -combined_points;

        other_player->research_agreement_goal[player_idx] = TRADE::Research_Agreement_Goal(other_player, player->id);

        COLCALC::Update_Player_Stats(player);
        COLCALC::Update_Player_Stats(other_player);
    }
} // namespace TRADE
