#include "pch.h"

namespace aidudes {
    static void Assign_Blockaded_Non_Farmers(s_colony* colony, s_aidudes_colony_info* colony_info) {
        int16_t is_researching = EVENTS::Event_Check_Colony_Researching(colony_info->colony_idx);
        POP::e_prof prof = is_researching == 0 ? POP::PROF_WORKER : POP::PROF_SCIENTIST;

        while ((uint8_t)colony_info->cur_pop_idx < (uint8_t)colony_info->pop_end_idx) {
            uint32_t* pop = &colony->pop[(uint8_t)colony_info->cur_pop_idx];
            POP::Set_Prof(pop, prof);
            POP::Set_Assigned(pop, true);
            colony_info->cur_pop_idx++;
        }
    }

    void __cdecl Do_Blockaded_Colony(s_aidudes_colony_info* colony_info) {
        s_colony* colony = &MOX::colony[colony_info->colony_idx];
        Assign_Required_Colonists(colony_info);

        Sort_Pops_colony_data = colony;

        qsort(&colony->pop[(uint8_t)colony_info->first_assignable_pop_idx],
            (uint8_t)colony->n_pops - (uint8_t)colony_info->first_assignable_pop_idx,
            4,
            Sort_Pops_FI);

        if (colony->food2_per_farmer == 0) {
            Assign_Blockaded_Non_Farmers(colony, colony_info);
        } else {
            COLCALC::Pre_Import_Computing(colony);

            while ((uint8_t)colony_info->cur_pop_idx < (uint8_t)colony_info->pop_end_idx) {
                int32_t total_food_needed = (uint8_t)colony->food2_needed_for_our_empire +
                    (uint8_t)colony->food2_needed_for_assimilated +
                    (uint8_t)colony->food2_needed_for_conquered +
                    (uint8_t)colony->food2_needed_for_natives;

                if (total_food_needed > colony->production[0] * 2) {
                    if ((uint8_t)colony_info->n_farmers < (uint8_t)colony->max_farms) {
                        uint32_t* pop = &colony->pop[(uint8_t)colony_info->cur_pop_idx];
                        POP::Set_Prof(pop, POP::PROF_FARMER);
                        POP::Set_Assigned(pop, true);
                        colony_info->cur_pop_idx++;
                        colony_info->n_farmers++;
                    } else if (colony->food2_per_farmer == 0) {
                        Assign_Blockaded_Non_Farmers(colony, colony_info);
                        break;
                    }
                } else {
                    colony_info->pop_end_idx--;
                    uint32_t* pop = &colony->pop[(uint8_t)colony_info->pop_end_idx];
                    POP::Set_Prof(pop, POP::PROF_WORKER);
                    POP::Set_Assigned(pop, true);
                }
                COLCALC::Pre_Import_Computing(colony);
            }
        }

        COLCALC::Pre_Import_Computing(colony);
    }

    int16_t __cdecl FI_Worthiness(const s_colony* colony, const uint32_t* pop) {
        int16_t worthiness_0;
        int16_t worthiness_1;

        worthiness_0 = COLCALC::Colony_Pop_Prod_Produced(colony, pop, 0);
        worthiness_1 = COLCALC::Colony_Pop_Prod_Produced(colony, pop, 1);

        return worthiness_0 - worthiness_1;
    }

    int __cdecl Sort_Pops_FI(const void* a, const void* b) {
        const uint32_t* pop1 = (const uint32_t*)a;
        const uint32_t* pop2 = (const uint32_t*)b;
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

    void __cdecl Assign_Required_Colonists(s_aidudes_colony_info* colony_info) {
        s_colony* colony = &MOX::colony[colony_info->colony_idx];
        uint32_t* pop_ptr = colony->pop;
        char do_not_research = MOX::player[(int)colony->owner].do_not_research;
        int16_t non_immune_pops = 0;

        colony_info->first_assignable_pop_idx = 0;
        colony_info->n_farmers = 0;
        colony_info->n_workers = 0;

        for (int i = 0; i < (int)colony->n_pops; ++i) {
            uint32_t pop_entry = POP::Load(&pop_ptr[i]);
            uint32_t race_idx = POP::Get_Race(pop_entry);

            if (POP::Is_Android_Or_Native(pop_entry)) {
                colony_info->first_assignable_pop_idx++;
                POP::e_prof prof = POP::Get_Prof(pop_entry);

                if (prof == POP::PROF_FARMER) {
                    colony_info->n_farmers++;
                } else if (prof == POP::PROF_WORKER) {
                    colony_info->n_workers++;
                } else if (prof == POP::PROF_SCIENTIST) {
                    if (do_not_research != 0) {
                        POP::Set_Assigned(&pop_ptr[i], false);
                    }
                }
            } else {
                POP::Set_Assigned(&pop_ptr[i], false);
            }

            // Re-evaluate current pop entry state after potential modifications
            uint32_t current_pop = POP::Load(&pop_ptr[i]);
            uint32_t current_race = POP::Get_Race(current_pop);

            // Check if population is affected by pollution (not Android/Native and not Tolerant)
            if (current_race < POP::RACE_ANDROID) {
                if (MOX::player[current_race].traits[TRAIT_ENVIRONMENT_IMMUNE] == 0) {
                    if (!POP::Is_Conquered(current_pop) || (colony->occupation_policy >= 2 && colony->occupation_policy <= 3)) {
                        non_immune_pops++;
                    }
                }
            }
        }

        // Determine if Lithovore trait (eats minerals) applies to this colony
        if ((non_immune_pops * 2 < (int)colony->n_pops) &&
            (MOX::player[(int)colony->owner].traits[TRAIT_EATS_MINERALS] != 0) &&
            (MOX::player[(int)colony->owner].surplus_food >= 0)) {
            colony_info->eats_minerals = 1;
        }

        // Sort population for AI processing
        aidudes::Sort_Pops_colony_data = colony;
        qsort(colony->pop, (size_t)colony->n_pops, 4, aidudes::Sort_Pops_Fixed);

        colony_info->cur_pop_idx = colony_info->first_assignable_pop_idx;
        colony_info->pop_end_idx = colony->n_pops;
        colony_info->pop_tail_idx = colony->n_pops;
    }

    int __cdecl Sort_Pops_Fixed(const void* a, const void* b) {
        const bool flag1 = POP::Is_Android_Or_Native(POP::Load(a));
        const bool flag2 = POP::Is_Android_Or_Native(POP::Load(b));
        return (int)flag2 - (int)flag1;
    }
} // namespace aidudes
