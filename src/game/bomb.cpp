#include "pch.h"
#include "game_events.h"
#include "bomb.h"
#include "mox.h"
#include "events.h"
#include "msg.h"
#include "invasion.h"
#include "colbldg.h"
#include "colcalc.h"
#include "officer.h"
#include <cstring>

namespace BOMB {

    int32_t __cdecl Handle_Leader_At_Lost_Colony(s_colony* colony) {
        int32_t owner_idx = colony->owner;
        int16_t star_idx = MOX::planet[colony->planet].star_index;
        int32_t killed_leader_idx = -1;
        int32_t leader_idx = MOX::star[star_idx].officer_index[owner_idx];

        if (leader_idx != -1) {
            bool other_colony_found = false;
            int16_t current_colony_idx = (int16_t)(colony - MOX::colony);

            for (int32_t planet_slot_idx = 0; planet_slot_idx < 5; ++planet_slot_idx) {
                int16_t p_idx = MOX::star[star_idx].planet_index[planet_slot_idx];
                if (p_idx != -1) {
                    int16_t c_idx = MOX::planet[p_idx].colony_index;
                    if (c_idx != -1 && MOX::colony[c_idx].owner == owner_idx && c_idx != current_colony_idx) {
                        other_colony_found = true;
                    }
                }
            }

            if (!other_colony_found) {
                if (MOX::leaders[leader_idx].eta > 0) {
                    OFFICER::Move_Officer_To_Pool(leader_idx);
                } else {
                    OFFICER::Kill_Officer(leader_idx);
                    killed_leader_idx = leader_idx;
                }
                MOX::star[star_idx].officer_index[owner_idx] = -1;
            }
        }

        return killed_leader_idx;
    }

    void __cdecl Destroy_Colony(s_colony* colony, int32_t capturing_player) {
        game_events::Publish_Colony_Replacing(colony->owner, colony->planet);
        s_star_data* star;
        int16_t star_idx;
        int16_t colony_idx;
        int16_t leader_idx;
        s_MSG msg;

        star_idx = MOX::planet[colony->planet].star_index;
        star = &MOX::star[star_idx];
        colony_idx = (int16_t)(colony - MOX::colony);

        EVENTS::Kill_All_Events_At_Colony(colony_idx);

        leader_idx = BOMB::Handle_Leader_At_Lost_Colony(colony);

        if (capturing_player != -1) {
            const int16_t capture_reason = (int16_t)capturing_player;

            memset(&msg, 0, sizeof(s_MSG));
            msg.msg_type = MSG_TYPE_LOST_COLONY;
            msg.u.lost_colony.planet_idx = colony->planet;
            msg.u.lost_colony.reason = capture_reason;
            msg.u.lost_colony.leader_idx = leader_idx;
            msg.u.lost_colony.colony_or_outpost = colony->outpost_flag;
            MSG_::Add_Msg(msg, colony->owner);
        }

        if (MOX::player[(uint8_t)colony->owner].home_planet_id == colony->planet) {
            invasion::Assign_New_Home_Planet(colony->owner, colony_idx);
        }

        COLBLDG::Clear_Colony_Queue(colony_idx);

        MOX::planet[colony->planet].colony_index = -1;

        memset(colony, 0, sizeof(s_colony));
        colony->planet = -1;
        colony->owner = -1;

        COLCALC::Compute_Star_Colony_Stuff(star);
    }

    int32_t __cdecl Resolve_Bomb_Hit(s_colony* colony, s_combat_bomb_result* bomb_result, int32_t available_bomb_strength) {
        int32_t target_building_count = 0;
        int8_t target_building_indices[BUILDING_COUNT];
        s_player* owner_player = &MOX::player[(uint8_t)colony->owner];

        for (int32_t building_idx = 0; building_idx < BUILDING_COUNT; ++building_idx) {
            if (colony->buildings[building_idx] != 0 &&
                (capitol_can_be_bombed || building_idx != BUILDING_CAPITOL) &&
                building_idx != BUILDING_BATTLESTATION &&
                building_idx != BUILDING_STAR_BASE &&
                building_idx != BUILDING_STAR_FORTRESS) {
                target_building_indices[target_building_count] = (int8_t)building_idx;
                target_building_count++;
            }
        }

        int32_t total_targets = (int32_t)colony->military[0] + (int32_t)colony->military[1] + target_building_count;
        if (colony->production_spent != 0) {
            total_targets++;
        }

        int32_t result_code = 0;
        if (colony->n_pops == 1) {
            if (total_targets == 0) {
                int32_t pop_idx = colony->pop[0] & 0xF;
                if (colony->pop_roundoff[pop_idx] <= 100) {
                    bomb_result->population_killed++;
                    colony->pop_roundoff[pop_idx] = 0;
                    colony->n_pops = 0;

                    if (colony->buildings[BUILDING_CAPITOL] != 0) {
                        int16_t colony_idx = (int16_t)(colony - MOX::colony);
                        ERICNET::Remove_Building(colony_idx, BUILDING_CAPITOL);
                    }

                    bomb_result->colony_wiped_out = 1;
                    if (bomb_result->attacker_idx >= 0 && bomb_result->attacker_idx < MAX_PLAYERS) {
                        owner_player->colony_war_losses[bomb_result->attacker_idx] += 10;
                    }
                } else {
                    colony->pop_roundoff[pop_idx] -= 100;
                }
                result_code = 1;
                bomb_result->damage_occurred = 1;
            }
        } else {
            total_targets += (uint8_t)colony->n_pops;
        }

        if (result_code == 0) {
            if (total_targets == 0) {
                return 0;
            }

            int32_t hit_val = game_random::Random(total_targets);
            int32_t hit_idx = hit_val - 1;

            if (hit_idx < target_building_count) {
                int8_t building_id = target_building_indices[hit_idx];
                int16_t colony_idx = (int16_t)(colony - MOX::colony);
                ERICNET::Remove_Building(colony_idx, (int16_t)building_id);
                bomb_result->buildings_destroyed[building_id] = 1;
                bomb_result->damage_occurred = 1;
                if (colony->n_pops == 0 && target_building_count == 1) {
                    bomb_result->colony_wiped_out = 1;
                    if (bomb_result->attacker_idx >= 0 && bomb_result->attacker_idx < MAX_PLAYERS) {
                        owner_player->colony_war_losses[bomb_result->attacker_idx] += 10;
                    }
                }
                return 1;
            }

            hit_idx -= target_building_count;
            if (hit_idx < (int32_t)colony->military[0]) {
                int32_t needed_strength = 1;
                if (owner_player->tech_applications[TECH_APP_POWERED_ARMOR] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    needed_strength = 2;
                }
                if (owner_player->traits[TRAIT_HEAVY_G_WORLD] != 0) {
                    needed_strength++;
                }

                if (available_bomb_strength < needed_strength) {
                    return 0;
                }

                colony->military[0]--;
                bomb_result->damage_occurred = 1;
                bomb_result->infantry_killed++;
                return needed_strength;
            }

            hit_idx -= (int32_t)colony->military[0];
            if (hit_idx < (int32_t)colony->military[1]) {
                int32_t needed_strength = 2;
                if (owner_player->tech_applications[TECH_APP_BATTLEOIDS] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    needed_strength = 3;
                }
                if (owner_player->traits[TRAIT_HEAVY_G_WORLD] != 0) {
                    needed_strength++;
                }

                if (available_bomb_strength < needed_strength) {
                    return 0;
                }

                colony->military[1]--;
                bomb_result->damage_occurred = 1;
                bomb_result->tanks_killed++;
                return needed_strength;
            }

            hit_idx -= (int32_t)colony->military[1];
            if (hit_idx < (int32_t)colony->production_spent) {
                bomb_result->production_lost = colony->production_spent;
                colony->production_spent = 0;
            } else {
                bomb_result->population_killed++;
                colony->n_pops--;
                colony->pop[hit_idx] = colony->pop[colony->n_pops];
                if (bomb_result->attacker_idx >= 0 && bomb_result->attacker_idx < MAX_PLAYERS) {
                    owner_player->colony_war_losses[bomb_result->attacker_idx]++;
                }
            }
            result_code = 1;
            bomb_result->damage_occurred = 1;
        }

        return result_code;
    }

    void __cdecl Resolve_Biological_Hit(s_colony* colony, s_combat_bomb_result* bomb_result) {
        int32_t target_pop_idx = -1;
        int32_t non_android_count = 0;

        for (int32_t pop_slot_idx = 0; pop_slot_idx < (int32_t)(uint8_t)colony->n_pops; ++pop_slot_idx) {
            if ((colony->pop[pop_slot_idx] & 0xf) != POP::RACE_ANDROID) {
                non_android_count++;
                if (game_random::Random(non_android_count) == 1) {
                    target_pop_idx = pop_slot_idx;
                }
            }
        }

        if (target_pop_idx < 0) {
            return;
        }

        bomb_result->damage_occurred = 1;

        if ((uint8_t)colony->n_pops >= 2) {
            int32_t chance_sum = (int32_t)colony->military[0] + (int32_t)colony->military[1] + (int32_t)(uint8_t)colony->n_pops;
            if (game_random::Random(chance_sum) <= (int32_t)(uint8_t)colony->n_pops) {
                bomb_result->population_killed++;
                bomb_result->bio_pop_killed++;

                colony->n_pops--;
                colony->pop[target_pop_idx] = colony->pop[(uint8_t)colony->n_pops];

                if (bomb_result->attacker_idx >= 0 && bomb_result->attacker_idx < MAX_PLAYERS) {
                    MOX::player[(uint8_t)colony->owner].colony_war_losses[bomb_result->attacker_idx]++;
                }
                return;
            }

            if (colony->military[0] != 0) {
                if ((int32_t)game_random::Random((int32_t)colony->military[1] + (int32_t)colony->military[0]) <= (int32_t)colony->military[0]) {
                    colony->military[0]--;
                    bomb_result->infantry_killed++;
                    return;
                }
            }

            colony->military[1]--;
            bomb_result->tanks_killed++;
            return;
        }

        if (colony->military[0] != 0) {
            if ((int32_t)game_random::Random((int32_t)colony->military[1] + (int32_t)colony->military[0]) <= (int32_t)colony->military[0]) {
                colony->military[0]--;
                bomb_result->infantry_killed++;
                return;
            }
        }

        if (colony->military[1] != 0) {
            colony->military[1]--;
            bomb_result->tanks_killed++;
            return;
        }

        int32_t race_idx = (int32_t)(colony->pop[0] & 0xf);
        // BUG: colony->pop_roundoff has 10 elements in orion2.h but 16 are possible here
        if (colony->pop_roundoff[race_idx] <= 100) {
            bomb_result->population_killed++;
            bomb_result->bio_pop_killed++;

            colony->pop_roundoff[race_idx] = 0;
            colony->n_pops = 0;

            bool has_buildings = false;
            for (int32_t building_idx = 0; building_idx < BUILDING_COUNT; ++building_idx) {
                if (colony->buildings[building_idx] != 0) {
                    has_buildings = true;
                    break;
                }
            }

            if (!has_buildings) {
                bomb_result->colony_wiped_out = 1;
                if (bomb_result->attacker_idx >= 0 && bomb_result->attacker_idx < MAX_PLAYERS) {
                    MOX::player[(uint8_t)colony->owner].colony_war_losses[bomb_result->attacker_idx] += 10;
                }
            }
        } else {
            race_idx = (int32_t)(colony->pop[target_pop_idx] & 0xf);
            colony->pop_roundoff[race_idx] -= 100;
        }
    }

    void __cdecl Apply_Bomb_Hits(s_combat_bomb_result* bomb_result) {
        int32_t bio_hits = bomb_result->bio_hits_remaining;
        int32_t bomb_hits = bomb_result->bomb_hits_remaining;
        const int16_t colony_idx = bomb_result->colony_idx;
        s_colony* colony = &MOX::colony[colony_idx];

        while (bio_hits + bomb_hits > 0) {
            if (bio_hits != 0) {
                bio_hits--;
                BOMB::Resolve_Biological_Hit(colony, bomb_result);
            } else {
                int32_t hits_dealt = BOMB::Resolve_Bomb_Hit(colony, bomb_result, bomb_hits);
                if (hits_dealt == 0) {
                    bomb_hits = 0;
                } else {
                    bomb_hits -= hits_dealt;
                    bomb_result->total_hits_processed += hits_dealt;
                }
            }
        }
    }

} // namespace BOMB