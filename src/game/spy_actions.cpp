#include "pch.h"

namespace spy {
    void __cdecl Compute_Spy_Bonuses(s_player* player, int16_t* defense_bonus_out, int16_t* attack_bonus_out) {
        int32_t telepathic_trait_bonus =
            (int8_t)player->traits[TRAIT_TELEPATHIC] * (int8_t)telepathic_bonus;
        int32_t tech_and_trait_bonus = Tech_Spy_Bonus(player) + player->traits[TRAIT_SPYING_BONUS];
        int32_t base_bonus = tech_and_trait_bonus + telepathic_trait_bonus;

        int32_t max_leader_def_bonus = 0;
        int32_t max_leader_atk_bonus = 0;

        int32_t player_idx = (int32_t)(player - MOX::player);

        for (int32_t i = 0; i < MAX_LEADERS; ++i) {
            s_leader_data* leader = &MOX::leaders[i];

            if (leader->player_index == player_idx) {
                if (leader->status >= 0 && leader->status <= 2) {
                    if (!COLCALC::Leader_At_Anomaly(leader)) {
                        int32_t current_def_skill = 0;
                        if (leader->general_skills & LEADER_GENERAL_SKILL_SPY_MASTER2) {
                            current_def_skill = OFFICER::Owned_Officer_Level(i) * 3 + 3;
                        } else if (leader->general_skills & LEADER_GENERAL_SKILL_SPY_MASTER) {
                            current_def_skill = OFFICER::Owned_Officer_Level(i) * 2 + 2;
                        }

                        if (current_def_skill > max_leader_def_bonus) {
                            max_leader_def_bonus = current_def_skill;
                        }

                        int32_t current_atk_skill = 0;
                        if (leader->general_skills & LEADER_GENERAL_SKILL_TELEPATH2) {
                            current_atk_skill = (OFFICER::Owned_Officer_Level(i) + 1) * 3;
                        } else if (leader->general_skills & LEADER_GENERAL_SKILL_TELEPATH) {
                            current_atk_skill = (OFFICER::Owned_Officer_Level(i) + 1) * 2;
                        }

                        if (current_atk_skill > max_leader_atk_bonus) {
                            max_leader_atk_bonus = current_atk_skill;
                        }
                    }
                }
            }
        }

        *defense_bonus_out = (int16_t)(max_leader_def_bonus + base_bonus);

        int32_t gov_bonus = lookup_table[player->traits[TRAIT_CURRENT_GOVERNMENT]];
        *attack_bonus_out = (int16_t)(gov_bonus + max_leader_atk_bonus + base_bonus);
    }

    void __cdecl Bring_Home_Spies_Vs(int16_t player_idx, int16_t enemy_idx) {
        int16_t spies_at_enemy = BILL::Get_Their_Spy_Number(player_idx, enemy_idx);

        if (spies_at_enemy != 0) {
            int16_t spies_at_home = BILL::Get_Their_Spy_Number(player_idx, player_idx);
            int32_t total_spies = (int32_t)spies_at_home + (int32_t)spies_at_enemy;

            if (total_spies > 0x3f /* 63 */) {
                total_spies = 0x3f;
            }

            BILL::Set_Their_Spy_Number(player_idx, player_idx, (uint8_t)total_spies);
            BILL::Set_Their_Spy_Number(player_idx, enemy_idx, 0);
        }
    }

    void __cdecl Resolve_Assassins() {
        for (int32_t leader_idx = MAX_LEADERS - 1;
             leader_idx >= 0;
             --leader_idx) {
            s_leader_data* leader = &MOX::leaders[leader_idx];

            // player_index != -1 and status is active (0) or assigned (1)
            if (leader->player_index != -1 && (leader->status == 0 || leader->status == 1)) {
                int32_t chance = 0;

                if ((leader->general_skills & LEADER_GENERAL_SKILL_ASSASSIN2) != 0) {
                    chance = OFFICER::Owned_Officer_Level(leader_idx) * 3 + 3;
                } else if ((leader->general_skills & LEADER_GENERAL_SKILL_ASSASSIN) != 0) {
                    chance = OFFICER::Owned_Officer_Level(leader_idx) * 2 + 2;
                }

                if (chance != 0) {
                    if ((int16_t)game_random::Random(100) <= chance) {
                        spy::Leader_Assassinates(leader);
                    }
                }
            }
        }
    }

    void __cdecl Leader_Assassinates(s_leader_data* leader) {
        int32_t total_assassins = 0;
        int32_t total_defenders = 0;
        int32_t assassin_plr_a = -1;
        int32_t assassin_plr_b = -1;
        int16_t victim_player_idx = leader->player_index;

        if (MOX::NUM_PLAYERS > 0) {
            for (int16_t i = 0; i < MOX::NUM_PLAYERS; i++) {
                if (i == victim_player_idx) continue;
                if (MOX::player[i].eliminated != 0) continue;

                int16_t spies_attacking = BILL::Get_Their_Spy_Number(i, victim_player_idx);
                if (spies_attacking > 0) {
                    total_assassins += spies_attacking;
                    if ((int32_t)game_random::Random(total_assassins) <= spies_attacking) {
                        assassin_plr_a = i;
                        assassin_plr_b = victim_player_idx;
                    }
                }

                if (total_assassins == 0) {
                    int16_t spies_of_victim_on_i = BILL::Get_Their_Spy_Number(victim_player_idx, i);
                    if (spies_of_victim_on_i > 0) {
                        int16_t spies_defending = BILL::Get_Their_Spy_Number(i, i);
                        if (spies_defending > 0) {
                            total_defenders += spies_defending;
                            if ((int32_t)game_random::Random(total_defenders) <= spies_defending) {
                                assassin_plr_a = i;
                                assassin_plr_b = i;
                            }
                        }
                    }
                }
            }
        }

        if (assassin_plr_a != -1) {
            s_MSG msg = {};
            msg.msg_type = MSG_TYPE_SPY_ASSASSINATION;
            msg.u.spy_assasination.plr_a = (int8_t)assassin_plr_a;
            msg.u.spy_assasination.plr_b = (int8_t)assassin_plr_b;

            int32_t leader_idx = (int32_t)(leader - MOX::leaders);
            msg.u.spy_assasination.leader_id = (int8_t)leader_idx;
            msg.u.spy_assasination.victim_id = (int8_t)victim_player_idx;

            MSG_::Add_Msg(msg, victim_player_idx);
            MSG_::Add_Msg(msg, (int16_t)assassin_plr_a);

            int16_t spies = BILL::Get_Their_Spy_Number((int16_t)assassin_plr_a, (int16_t)assassin_plr_b);
            BILL::Set_Their_Spy_Number((int16_t)assassin_plr_a, (int16_t)assassin_plr_b, spies - 1);
        }
    }

    int32_t __cdecl N_Spies_Bonus(int32_t spy_count) {
        if (spy_count <= count_bonus.plus_two) {
            return spy_count * 2;
        }

        if (spy_count <= count_bonus.plus_one) {
            return spy_count + count_bonus.plus_two;
        }

        // Equivalent to (spy_count - plus_one) / 2 + plus_one + plus_two.
        return (spy_count - count_bonus.plus_one) / 2 +
            count_bonus.plus_one + count_bonus.plus_two;
    }

    /**
     * Attempts to find a candidate player to "frame" for a spy action.
     *
     * @param player_idx_a The player performing the spy action.
     * @param player_idx_b The victim player being spied upon.
     * @param framed_player_idx Output pointer for the player index to be framed.
     * @param tech_app_id Tech application id to exclude candidates who already know it, or -1.
     * @return true if any candidates were found, false otherwise.
     */
    bool __cdecl Frame_Another(int16_t player_idx_a, int16_t player_idx_b, int16_t* framed_player_idx, int32_t tech_app_id) {
        int32_t candidate_count = 0;
        uint8_t treaty_counts[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        int8_t framed_candidates[8] = {-1, -1, -1, -1, -1, -1, -1, -1};

        for (int32_t i = 0; i < (int32_t)MOX::NUM_PLAYERS; ++i) {
            // A player cannot frame themselves or the victim they are already spying on
            if (i == (int32_t)player_idx_a || i == (int32_t)player_idx_b) {
                continue;
            }

            // The potential candidate must have contact with both the actor and the victim
            if (MOX::player[i].contact[player_idx_a] == 0) {
                continue;
            }

            if (MOX::player[i].contact[player_idx_b] == 0) {
                continue;
            }

            // If a tech application id is provided, ensure the candidate doesn't already know it
            if (!frame_anyone && tech_app_id != -1) {
                if (MOX::player[i].tech_applications[tech_app_id] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    continue;
                }
            }

            candidate_count++;

            // Get the treaty level between the actor and the potential frame candidate
            uint8_t treaty_type = (uint8_t)MOX::player[player_idx_a].treaty[i];

            // Reservoir sampling: select one candidate per treaty category at random
            treaty_counts[treaty_type]++;
            if (game_random::Random((int32_t)treaty_counts[treaty_type]) == 1) {
                framed_candidates[treaty_type] = (int8_t)i;
            }
        }

        // Selection priority based on treaty type
        int8_t selected_idx = -1;
        if (framed_candidates[6] != -1) {
            selected_idx = framed_candidates[6];
        } else if (framed_candidates[5] != -1) {
            selected_idx = framed_candidates[5];
        } else if (framed_candidates[4] != -1) {
            selected_idx = framed_candidates[4];
        } else if (framed_candidates[0] != -1) {
            selected_idx = framed_candidates[0];
        } else if (framed_candidates[1] != -1) {
            selected_idx = framed_candidates[1];
        } else if (framed_candidates[3] != -1) {
            selected_idx = framed_candidates[3];
        } else if (framed_candidates[2] != -1) {
            selected_idx = framed_candidates[2];
        }

        if (selected_idx != -1) {
            *framed_player_idx = (int16_t)selected_idx;
        }

        return candidate_count > 0;
    }
} // namespace spy
