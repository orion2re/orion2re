#include "pch.h"

#include "legacy_ids.h"
#include "ship_ids.h"

namespace aidudes {
    static void Assign_Blockaded_Non_Farmers(s_colony* colony, AIDATA::s_colony_assignment* colony_info) {
        int16_t is_researching = EVENTS::Event_Check_Colony_Researching(colony_info->colony_id);
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

        qsort(&colony->pop[(uint8_t)colony_info->first_assignable_pop_idx],
            (uint8_t)colony->n_pops - (uint8_t)colony_info->first_assignable_pop_idx,
            sizeof(s_colony_pop),
            Sort_Pops_FI, colony);

        if (colony->food2_per_farmer == 0) {
            Assign_Blockaded_Non_Farmers(colony, colony_info);
        } else {
            COLCALC::Pre_Import_Computing(colony_info->colony_id, colony);

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
                        Assign_Blockaded_Non_Farmers(colony, colony_info);
                        break;
                    }
                } else {
                    colony_info->pop_end_idx--;
                    s_colony_pop* pop = &colony->pop[(uint8_t)colony_info->pop_end_idx];
                    pop->profession = POP::PROF_WORKER;
                    pop->assigned = true;
                }
                COLCALC::Pre_Import_Computing(colony_info->colony_id, colony);
            }
        }

        COLCALC::Pre_Import_Computing(colony_info->colony_id, colony);
    }

    int16_t __cdecl FI_Worthiness(const s_colony* colony, const s_colony_pop* pop) {
        int16_t worthiness_0;
        int16_t worthiness_1;

        worthiness_0 = COLCALC::Colony_Pop_Prod_Produced(colony, pop, 0);
        worthiness_1 = COLCALC::Colony_Pop_Prod_Produced(colony, pop, 1);

        return worthiness_0 - worthiness_1;
    }

    int __cdecl Sort_Pops_FI(void* context, const void* a, const void* b) {
        auto* Sort_Pops_colony_data = static_cast<s_colony*>(context);
        const s_colony_pop* pop1 = (const s_colony_pop*)a;
        const s_colony_pop* pop2 = (const s_colony_pop*)b;
        int16_t worth1 = FI_Worthiness(Sort_Pops_colony_data, pop1);
        int16_t worth2 = FI_Worthiness(Sort_Pops_colony_data, pop2);

        if (worth2 == worth1) {
            int16_t prod1 = COLCALC::Colony_Pop_Prod_Produced(Sort_Pops_colony_data, pop1, 0);
            int16_t prod2 = COLCALC::Colony_Pop_Prod_Produced(Sort_Pops_colony_data, pop2, 0);

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
            uint32_t race_idx = pop_entry.group;

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
            uint32_t current_race = current_pop.group;

            // Check if population is affected by pollution (not Android/Native and not Tolerant)
            if (current_race < POP::RACE_ANDROID) {
                if (world->players[current_race].traits[TRAIT_ENVIRONMENT_IMMUNE] == 0) {
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
