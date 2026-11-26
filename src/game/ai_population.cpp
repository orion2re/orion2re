#include "pch.h"

#include "legacy_ids.h"
#include "ship_ids.h"

namespace aidudes {
    struct s_blockaded_sort_context {
        const game::s_game_state& world;
        const s_colony* colony;
    };

    static int __cdecl Sort_Pops_FI(void* context, const void* a, const void* b);

    static void Assign_Blockaded_Non_Farmers(const game::s_game_state& world, s_colony* colony, AIDATA::s_colony_assignment* colony_info) {
        const bool is_researching = &world == &game::current_state && EVENTS::Event_Check_Colony_Researching(colony_info->colony_id) != 0;
        POP::e_prof prof = is_researching == 0 ? POP::PROF_WORKER : POP::PROF_SCIENTIST;

        while ((uint8_t)colony_info->cur_pop_idx < (uint8_t)colony_info->pop_end_idx) {
            s_colony_pop* pop = &colony->pop[(uint8_t)colony_info->cur_pop_idx];
            pop->profession = prof;
            pop->assigned = true;
            colony_info->cur_pop_idx++;
        }
    }

    void __cdecl Do_Blockaded_Colony(game::s_game_state* world, AIDATA::s_colony_assignment* colony_info) {
        s_colony* colony = &world->colonies.at(colony_info->colony_id);
        Assign_Required_Colonists(world, colony_info);

        s_blockaded_sort_context sort_context{*world, colony};
        qsort(&colony->pop[(uint8_t)colony_info->first_assignable_pop_idx],
            (uint8_t)colony->n_pops - (uint8_t)colony_info->first_assignable_pop_idx,
            sizeof(s_colony_pop),
            Sort_Pops_FI, &sort_context);

        if (colony->food2_per_farmer == 0) {
            Assign_Blockaded_Non_Farmers(*world, colony, colony_info);
        } else {
            COLCALC::Pre_Import_Computing(*world, colony_info->colony_id, colony);

            while ((uint8_t)colony_info->cur_pop_idx < (uint8_t)colony_info->pop_end_idx) {
                int32_t total_food_needed = (uint8_t)colony->food2_needed_for_our_empire +
                    (uint8_t)colony->food2_needed_for_assimilated +
                    (uint8_t)colony->food2_needed_for_conquered +
                    (uint8_t)colony->food2_needed_for_natives;

                if (total_food_needed > colony->production[0] * 2) {
                    if ((uint8_t)colony_info->n_farmers < (uint8_t)colony->max_farms) {
                        s_colony_pop* pop = &colony->pop[(uint8_t)colony_info->cur_pop_idx];
                        pop->profession = POP::PROF_FARMER;
                        pop->assigned = true;
                        colony_info->cur_pop_idx++;
                        colony_info->n_farmers++;
                    } else if (colony->food2_per_farmer == 0) {
                        Assign_Blockaded_Non_Farmers(*world, colony, colony_info);
                        break;
                    }
                } else {
                    colony_info->pop_end_idx--;
                    s_colony_pop* pop = &colony->pop[(uint8_t)colony_info->pop_end_idx];
                    pop->profession = POP::PROF_WORKER;
                    pop->assigned = true;
                }
                COLCALC::Pre_Import_Computing(*world, colony_info->colony_id, colony);
            }
        }

        COLCALC::Pre_Import_Computing(*world, colony_info->colony_id, colony);
    }

    static int __cdecl Sort_Pops_FI(void* context, const void* a, const void* b) {
        const auto& [world, colony] = *static_cast<s_blockaded_sort_context*>(context);
        const s_colony_pop* pop1 = (const s_colony_pop*)a;
        const s_colony_pop* pop2 = (const s_colony_pop*)b;
        const auto worthiness = [&](const s_colony_pop* pop) -> int16_t {
            return COLCALC::Colony_Pop_Prod_Produced(world, colony, pop, 0) -
                COLCALC::Colony_Pop_Prod_Produced(world, colony, pop, 1);
        };
        int16_t worth1 = worthiness(pop1);
        int16_t worth2 = worthiness(pop2);

        if (worth2 == worth1) {
            int16_t prod1 = COLCALC::Colony_Pop_Prod_Produced(world, colony, pop1, 0);
            int16_t prod2 = COLCALC::Colony_Pop_Prod_Produced(world, colony, pop2, 0);

            // If worthiness is equal, sort by production ascending
            return (int)prod1 - (int)prod2;
        }

        // Sort by worthiness descending
        return (int)worth2 - (int)worth1;
    }

    void __cdecl Assign_Required_Colonists(game::s_game_state* world, AIDATA::s_colony_assignment* colony_info) {
        s_colony* colony = &world->colonies.at(colony_info->colony_id);
        s_colony_pop* pop_ptr = colony->pop;
        char do_not_research = world->players.at(colony->owner.id).do_not_research;
        int16_t non_immune_pops = 0;

        colony_info->first_assignable_pop_idx = 0;
        colony_info->n_farmers = 0;
        colony_info->n_workers = 0;

        for (int i = 0; i < (int)colony->n_pops; ++i) {
            const auto& pop_entry = pop_ptr[i];

            if (POP::Is_Android_Or_Native(pop_entry)) {
                colony_info->first_assignable_pop_idx++;
                POP::e_prof prof = pop_entry.profession;

                if (prof == POP::PROF_FARMER) {
                    colony_info->n_farmers++;
                } else if (prof == POP::PROF_WORKER) {
                    colony_info->n_workers++;
                } else if (prof == POP::PROF_SCIENTIST) {
                    if (do_not_research != 0) {
                        pop_ptr[i].assigned = false;
                    }
                }
            } else {
                pop_ptr[i].assigned = false;
            }

            // Re-evaluate current pop entry state after potential modifications
            const auto& current_pop = pop_ptr[i];
            const auto player = POP::Group_Player(current_pop);

            // Check if population is affected by pollution (not Android/Native and not Tolerant)
            if (player != game::NO_PLAYER) {
                if (world->players[player.id].traits[TRAIT_ENVIRONMENT_IMMUNE] == 0) {
                    if (!current_pop.conquered || (colony->occupation_policy >= 2 && colony->occupation_policy <= 3)) {
                        non_immune_pops++;
                    }
                }
            }
        }

        // Determine if Lithovore trait (eats minerals) applies to this colony
        if ((non_immune_pops * 2 < (int)colony->n_pops) &&
            (world->players.at(colony->owner.id).traits[TRAIT_EATS_MINERALS] != 0) &&
            (world->players.at(colony->owner.id).surplus_food >= 0)) {
            colony_info->eats_minerals = 1;
        }

        // Sort population for AI processing
        qsort(colony->pop, (size_t)colony->n_pops, sizeof(s_colony_pop), aidudes::Sort_Pops_Fixed);

        colony_info->cur_pop_idx = colony_info->first_assignable_pop_idx;
        colony_info->pop_end_idx = colony->n_pops;
        colony_info->pop_tail_idx = colony->n_pops;
    }

    int __cdecl Sort_Pops_Fixed(const void* a, const void* b) {
        const bool flag1 = POP::Is_Android_Or_Native(*static_cast<const s_colony_pop*>(a));
        const bool flag2 = POP::Is_Android_Or_Native(*static_cast<const s_colony_pop*>(b));
        return (int)flag2 - (int)flag1;
    }
} // namespace aidudes
