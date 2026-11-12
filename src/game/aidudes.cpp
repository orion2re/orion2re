#include "pch.h"

namespace aidudes {

    int16_t n_our_blockaded;
    int16_t n_our_unblockaded;
    s_aidudes_colony_info* our_blockaded;
    s_player* p_ai;
    s_aidudes_colony_info* our_unblockaded;
    int16_t n_our_colonies;
    s_aidudes_colony_info* Sort_Pops_colony_info;
    s_colony* Sort_Pops_colony_data;
    int16_t i_ai;
    s_aidudes_colony_info (*our_colonies)[MAX_COLONIES];

    void __cdecl All_Colony_AI() {
        int16_t num_players = (int16_t)MOX::NUM_PLAYERS;
        s_player* plr = MOX::player;

        for (int16_t i = 0; i < num_players; ++i, ++plr) {
            if (plr->objectives != PLAYER_OBJECTIVE_HUMAN) {
                RUSS::Mox_Update();
                Colony_AI(i);
            }
        }

        RUSS::Mox_Update();

        s_colony* col = MOX::colony;
        s_colony* end = MOX::colony + (int16_t)MOX::NUM_COLONIES;
        while (col < end) {
            if (col->producing[0] == COLONY_PRODUCTION_AI_TRADE_GOODS) {
                col->producing[0] = COLONY_PRODUCTION_TRADE_GOODS;
            }
            ++col;
        }
    }

    void __cdecl Industry_Sanity_Check() {
        int n_colonies = (int)aidudes::n_our_unblockaded;
        s_aidudes_colony_info* colony_infos = aidudes::our_unblockaded;

        for (int i = 0; i < n_colonies; ++i) {
            s_aidudes_colony_info* info = &colony_infos[i];
            int16_t colony_idx = info->colony_idx;
            s_colony* col = &MOX::colony[colony_idx];

            bool is_colony_producing_trade_goods = (col->producing[0] == COLONY_PRODUCTION_TRADE_GOODS);

            if ((is_colony_producing_trade_goods && aidudes::p_ai->do_not_research == 0) ||
                EVENTS::Event_Check_Colony_Researching(colony_idx) != 0) {
                for (int j = (int)info->pop_tail_idx - 1; j >= (int)info->first_assignable_pop_idx; --j) {
                    uint32_t* pop_entry = &col->pop[j];

                    if (!POP::Is_Worker(POP::Load(pop_entry))) {
                        break;
                    }

                    POP::Set_Prof(pop_entry, POP::PROF_SCIENTIST);
                }

                COLCALC::Colony_Calculation(colony_idx);
            }
        }
    }

    void __cdecl Colony_AI_Given_Tax() {
        for (int i = 0; i < aidudes::n_our_blockaded; ++i) {
            aidudes::Do_Blockaded_Colony(&aidudes::our_blockaded[i]);
        }

        for (int i = 0; i < aidudes::n_our_unblockaded; ++i) {
            aidudes::Do_Unblockaded_Colony(&aidudes::our_unblockaded[i]);
        }

        aidudes::Assign_Additional_Research_Industry();
        aidudes::Assign_Empire_Building(aidudes::i_ai);
        aidudes::Assign_Buildings();
        aidudes::Industry_Sanity_Check();

        if (aidudes::n_our_unblockaded > 0) {
            aidudes::Assign_Additional_Unblockaded_Farmers();
        } else {
            COLCALC::Do_Player_Colony_Post_Production(aidudes::i_ai);
        }
    }

    void __cdecl Colony_AI(int16_t i_ai) {
        s_aidudes_colony_info* our_colonies = ai_state::Prepare_Colony_State(i_ai);
        bool free_our_colonies = false;
        if (our_colonies == nullptr) {
            our_colonies = (s_aidudes_colony_info*)calloc(
                MAX_COLONIES, sizeof(s_aidudes_colony_info));
            if (our_colonies == nullptr) {
                allocate::Allocation_Error(MAX_COLONIES * sizeof(s_aidudes_colony_info));
            }
            free_our_colonies = true;
        }

        aidudes::i_ai = i_ai;
        aidudes::our_colonies = (s_aidudes_colony_info(*)[MAX_COLONIES])our_colonies;
        aidudes::p_ai = &MOX::player[i_ai];

        aidudes::Collect_AI_Colonies();

        if (MOX::settings.game_difficulty > GAME_DIFFICULTY_TUTOR) {
            if (AIBUILD::Player_War_Level(aidudes::i_ai) < 3) {
                aidudes::Clear_Build_Queues_If_New_Tech(aidudes::p_ai->got_new_app);
            }
        }

        aidudes::Keep_Ship_Designs_Up_To_Date();
        aidudes::Freighter_Cheat(aidudes::i_ai);
        aidudes::Do_Ship_Upgrade_Cheats();
        AIBUILD::Empire_Buy_Outright(aidudes::i_ai);
        aidudes::Colony_AI_Given_Tax();
        RUSS::Mox_Update();

#if defined(ORION2RE_ENABLE_DEBUG_OVERLAY)
        ai_state::Capture_Colony_State(i_ai, *aidudes::our_colonies,
            aidudes::n_our_colonies, aidudes::n_our_blockaded);
#endif

        aidudes::our_colonies = nullptr;
        aidudes::our_blockaded = nullptr;
        aidudes::our_unblockaded = nullptr;
        aidudes::p_ai = nullptr;
        aidudes::Sort_Pops_colony_info = nullptr;
        aidudes::Sort_Pops_colony_data = nullptr;
        if (free_our_colonies) free(our_colonies);
    }

    void __cdecl Clear_Build_Queues_If_New_Tech(bool force_clear) {
        s_aidudes_colony_info* current_info = *our_colonies;
        int num_colonies = (int)n_our_colonies;

        if (num_colonies == 0) return;

        for (int i = 0; i < num_colonies; ++i, ++current_info) {
            int16_t colony_idx = current_info->colony_idx;
            s_colony* colony = &MOX::colony[colony_idx];

            bool should_rebuild = force_clear;
            if (!should_rebuild) {
                int16_t producing = colony->producing[0];
                if (producing == COLONY_PRODUCTION_NONE ||
                    producing == COLONY_PRODUCTION_HOUSING ||
                    producing == COLONY_PRODUCTION_TRADE_GOODS ||
                    producing == COLONY_PRODUCTION_REPEAT) {
                    should_rebuild = true;
                }
            }

            if (should_rebuild) {
                int16_t producing = colony->producing[0];
                if (COLBLDG::Colony_Production_Is_Queued_Ship(producing)) {
                    int16_t ship_idx = COLBLDG::Colony_Production_Ship_Index(producing);
                    if (MOX::ship[ship_idx].status == 4) {
                        return; // Abandon evaluation entirely if an outdated ship is being built
                    }
                }

                uint8_t weights[BUILDING_COUNT + 1];
                memset(weights, 0, sizeof(weights));
                weights[BUILDING_COUNT] = 55; // Default/Keep Current weight

                if (colony->n_pops >= 5) {
                    // TECH_APP_SPACEPORT (164), BUILDING_SPACEPORT (39)
                    if (p_ai->tech_applications[TECH_APP_SPACEPORT] == TECH_RESEARCH_STATUS_RESEARCHED && colony->buildings[BUILDING_SPACEPORT] == 0) {
                        weights[BUILDING_SPACEPORT] = colony->n_pops + 25;
                    }
                    // TECH_APP_PLANETARY_STOCK_EXCHANGE (135), BUILDING_STOCK_EXCHANGE (29)
                    if (p_ai->tech_applications[TECH_APP_PLANETARY_STOCK_EXCHANGE] == TECH_RESEARCH_STATUS_RESEARCHED && colony->buildings[BUILDING_STOCK_EXCHANGE] == 0) {
                        weights[BUILDING_STOCK_EXCHANGE] = colony->n_pops + 20;
                    }
                }

                // TECH_APP_SUBTERRANEAN_FARMS (178), BUILDING_SUBTERRANEAN_FARMS (43)
                if (p_ai->tech_applications[TECH_APP_SUBTERRANEAN_FARMS] == TECH_RESEARCH_STATUS_RESEARCHED && colony->buildings[BUILDING_SUBTERRANEAN_FARMS] == 0) {
                    weights[BUILDING_SUBTERRANEAN_FARMS] = 20;
                }
                // TECH_APP_HYDROPONIC_FARM (87), BUILDING_HYDROPONIC_FARM (21)
                if (p_ai->tech_applications[TECH_APP_HYDROPONIC_FARM] == TECH_RESEARCH_STATUS_RESEARCHED && colony->buildings[BUILDING_HYDROPONIC_FARM] == 0) {
                    weights[BUILDING_HYDROPONIC_FARM] = 20;
                }
                // TECH_APP_AUTOMATED_FACTORIES (22), BUILDING_AUTOMATED_FACTORY (7)
                if (p_ai->tech_applications[TECH_APP_AUTOMATED_FACTORIES] == TECH_RESEARCH_STATUS_RESEARCHED && colony->buildings[BUILDING_AUTOMATED_FACTORY] == 0) {
                    weights[BUILDING_AUTOMATED_FACTORY] = 35;
                }
                // TECH_APP_ROBO_MINERS (156), BUILDING_ROBO_MINER_PLANT (36)
                if (p_ai->tech_applications[TECH_APP_ROBO_MINERS] == TECH_RESEARCH_STATUS_RESEARCHED && colony->buildings[BUILDING_ROBO_MINER_PLANT] == 0) {
                    weights[BUILDING_ROBO_MINER_PLANT] = 20;
                }
                // TECH_APP_DEEP_CORE_MINING (49), BUILDING_DEEP_CORE_MINE (12)
                if (p_ai->tech_applications[TECH_APP_DEEP_CORE_MINING] == TECH_RESEARCH_STATUS_RESEARCHED && colony->buildings[BUILDING_DEEP_CORE_MINE] == 0) {
                    weights[BUILDING_DEEP_CORE_MINE] = 10;
                }
                // TECH_APP_ROBOTIC_FACTORY (154), BUILDING_ROBOTIC_FACTORY (34)
                if (p_ai->tech_applications[TECH_APP_ROBOTIC_FACTORY] == TECH_RESEARCH_STATUS_RESEARCHED && colony->buildings[BUILDING_ROBOTIC_FACTORY] == 0) {
                    weights[BUILDING_ROBOTIC_FACTORY] = 7;
                }

                if (colony->n_pops >= 5) {
                    // TECH_APP_RECYCLOTRON (152), BUILDING_RECYCLOTRON (33)
                    if (p_ai->tech_applications[TECH_APP_RECYCLOTRON] == TECH_RESEARCH_STATUS_RESEARCHED && colony->buildings[BUILDING_RECYCLOTRON] == 0) {
                        weights[BUILDING_RECYCLOTRON] = colony->n_pops + 15;
                    }
                    if (p_ai->do_not_research == 0) {
                        // TECH_APP_RESEARCH_LABORATORY (155), BUILDING_RESEARCH_LAB (35)
                        if (p_ai->tech_applications[TECH_APP_RESEARCH_LABORATORY] == TECH_RESEARCH_STATUS_RESEARCHED && colony->buildings[BUILDING_RESEARCH_LAB] == 0) {
                            weights[BUILDING_RESEARCH_LAB] = 20;
                        }
                        // TECH_APP_PLANETARY_SUPERCOMPUTER (136), BUILDING_SUPERCOMPUTER (30)
                        if (p_ai->tech_applications[TECH_APP_PLANETARY_SUPERCOMPUTER] == TECH_RESEARCH_STATUS_RESEARCHED && colony->buildings[BUILDING_SUPERCOMPUTER] == 0) {
                            weights[BUILDING_SUPERCOMPUTER] = 10;
                        }
                    }
                }

                int player_idx = (int)(p_ai - MOX::player);
                int trait_check = MOX::player[player_idx].traits[TRAIT_CURRENT_GOVERNMENT] / 2;

                if (trait_check != GOVERNMENT_UNIFICATION / 2) {
                    if (trait_check == GOVERNMENT_DICTATORSHIP / 2 || trait_check == GOVERNMENT_FEUDAL / 2) {
                        // TECH_APP_MARINE_BARRACKS (103), BUILDING_MARINE_BARRACKS (22), BUILDING_ARMOR_BARRACKS (2)
                        if (p_ai->tech_applications[TECH_APP_MARINE_BARRACKS] == TECH_RESEARCH_STATUS_RESEARCHED && colony->buildings[BUILDING_MARINE_BARRACKS] == 0 && colony->buildings[BUILDING_ARMOR_BARRACKS] == 0) {
                            weights[BUILDING_MARINE_BARRACKS] = 37;
                        }
                        // TECH_APP_ARMOR_BARRACKS (14), BUILDING_ARMOR_BARRACKS (2), BUILDING_MARINE_BARRACKS (22)
                        if (p_ai->tech_applications[TECH_APP_ARMOR_BARRACKS] == TECH_RESEARCH_STATUS_RESEARCHED && colony->buildings[BUILDING_ARMOR_BARRACKS] == 0 && colony->buildings[BUILDING_MARINE_BARRACKS] == 0) {
                            weights[BUILDING_ARMOR_BARRACKS] = 10;
                        }
                    }
                    if (colony->n_pops >= 3) {
                        // TECH_APP_HOLO_SIMULATOR (86), BUILDING_HOLO_SIMULATOR (20)
                        if (p_ai->tech_applications[TECH_APP_HOLO_SIMULATOR] == TECH_RESEARCH_STATUS_RESEARCHED && colony->buildings[BUILDING_HOLO_SIMULATOR] == 0) {
                            weights[BUILDING_HOLO_SIMULATOR] = colony->n_pops + 25;
                        }
                        // TECH_APP_PLEASURE_DOME (141), BUILDING_PLEASURE_DOME (31)
                        if (p_ai->tech_applications[TECH_APP_PLEASURE_DOME] == TECH_RESEARCH_STATUS_RESEARCHED && colony->buildings[BUILDING_PLEASURE_DOME] == 0) {
                            weights[BUILDING_PLEASURE_DOME] = colony->n_pops + 10;
                        }
                    }
                }

                // TECH_APP_BIOSPHERES (61), BUILDING_BIOSPHERES (15)
                if (p_ai->tech_applications[TECH_APP_BIOSPHERES] == TECH_RESEARCH_STATUS_RESEARCHED && colony->buildings[BUILDING_BIOSPHERES] == 0) {
                    weights[BUILDING_BIOSPHERES] = 30;
                }

                if (colony->pollution >= 5 && current_info->eats_minerals == 0) {
                    // TECH_APP_CORE_WASTE_DUMPS (50), BUILDING_CORE_WASTE_DUMP (13)
                    if (p_ai->tech_applications[TECH_APP_CORE_WASTE_DUMPS] == TECH_RESEARCH_STATUS_RESEARCHED && colony->buildings[BUILDING_CORE_WASTE_DUMP] == 0) {
                        weights[BUILDING_CORE_WASTE_DUMP] = (uint8_t)special::isqrt(colony->pollution) + 8;
                    }
                    // TECH_APP_ATMOSPHERIC_RENEWER (19), BUILDING_ATMOSPHERE_RENEWER (5)
                    if (p_ai->tech_applications[TECH_APP_ATMOSPHERIC_RENEWER] == TECH_RESEARCH_STATUS_RESEARCHED && colony->buildings[BUILDING_ATMOSPHERE_RENEWER] == 0) {
                        weights[BUILDING_ATMOSPHERE_RENEWER] = (uint8_t)special::isqrt(colony->pollution) + 8;
                    }
                    // TECH_APP_POLLUTION_PROCESSOR (142), BUILDING_POLLUTION_PROCESSOR (32)
                    if (p_ai->tech_applications[TECH_APP_POLLUTION_PROCESSOR] == TECH_RESEARCH_STATUS_RESEARCHED && colony->buildings[BUILDING_POLLUTION_PROCESSOR] == 0) {
                        weights[BUILDING_POLLUTION_PROCESSOR] = 10;
                    }
                }

                // TECH_APP_PLANETARY_GRAVITY_GENERATOR (131), BUILDING_GRAVITY_GENERATOR (25)
                if (p_ai->tech_applications[TECH_APP_PLANETARY_GRAVITY_GENERATOR] == TECH_RESEARCH_STATUS_RESEARCHED && colony->buildings[BUILDING_GRAVITY_GENERATOR] == 0) {
                    int8_t gravity = MOX::planet[colony->planet].gravity_class;
                    if (gravity == PLANET_GRAVITY_LOW_G) {
                        if (p_ai->traits[TRAIT_LOW_G_WORLD] == 0) {
                            weights[BUILDING_GRAVITY_GENERATOR] = 10;
                        }
                    } else if (gravity == PLANET_GRAVITY_NORMAL_G) {
                        if (p_ai->traits[TRAIT_LOW_G_WORLD] != 0) {
                            weights[BUILDING_GRAVITY_GENERATOR] = 10;
                        }
                    } else if (gravity == PLANET_GRAVITY_HEAVY_G) {
                        if (p_ai->traits[TRAIT_HEAVY_G_WORLD] == 0) {
                            weights[BUILDING_GRAVITY_GENERATOR] = 20;
                        }
                    }
                }

                int16_t choice = RUSS::Get_Weighted_Choice_Char(weights, BUILDING_COUNT + 1);
                if (choice < BUILDING_COUNT) {
                    COLBLDG::Clear_Colony_Queue(colony_idx);
                    colony->producing[0] = choice;
                }
            }
        }
    }

    int __cdecl Sort_Colonies_Blockaded(const void* a, const void* b) {
        const s_aidudes_colony_info* info1 = (const s_aidudes_colony_info*)a;
        const s_aidudes_colony_info* info2 = (const s_aidudes_colony_info*)b;

        s_colony* colony1 = &MOX::colony[info1->colony_idx];
        s_colony* colony2 = &MOX::colony[info2->colony_idx];

        bool blockaded2 = COLCALC::Colony_Is_Blockaded(colony2);
        bool blockaded1 = COLCALC::Colony_Is_Blockaded(colony1);

        return (int)blockaded2 - (int)blockaded1;
    }

    void __cdecl Collect_AI_Colonies() {
        n_our_colonies = 0;
        n_our_unblockaded = 0;
        n_our_blockaded = 0;

        memset(*our_colonies, 0, (size_t)MOX::NUM_COLONIES * sizeof(s_aidudes_colony_info));

        for (int16_t i = 0; i < (int)MOX::NUM_COLONIES; ++i) {
            s_colony* colony_ptr = &MOX::colony[i];

            if (colony_ptr->owner == (char)i_ai) {
                if (EVENTS::Event_Check_Space_Anomaly(i) == 0 && colony_ptr->outpost_flag == 0) {
                    (*our_colonies)[n_our_colonies].colony_idx = i;

                    if (COLCALC::Colony_Is_Blockaded(colony_ptr)) {
                        n_our_blockaded++;
                    } else {
                        n_our_unblockaded++;
                    }
                    n_our_colonies++;
                }
            }
        }

        qsort(*our_colonies, (size_t)n_our_colonies, sizeof(s_aidudes_colony_info), Sort_Colonies_Blockaded);

        our_blockaded = (s_aidudes_colony_info*)*our_colonies;
        our_unblockaded = &((*our_colonies)[n_our_blockaded]);
    }

    void __cdecl Compute_Max_Min_R_I_Worthiness(s_aidudes_colony_info* info) {
        if (info->cur_pop_idx < info->pop_end_idx) {
            s_colony* colony = &MOX::colony[info->colony_idx];

            uint32_t* cur_pop_ptr = &colony->pop[info->cur_pop_idx];
            int16_t cur_prod_2 = COLCALC::Colony_Pop_Prod_Produced(colony, cur_pop_ptr, 2);
            int16_t cur_prod_1 = COLCALC::Colony_Pop_Prod_Produced(colony, cur_pop_ptr, 1);
            info->best_worker_to_farmer_score = cur_prod_2 - cur_prod_1;

            uint32_t* end_pop_ptr = &colony->pop[info->pop_end_idx - 1];
            int16_t end_prod_2 = COLCALC::Colony_Pop_Prod_Produced(colony, end_pop_ptr, 2);
            int16_t end_prod_1 = COLCALC::Colony_Pop_Prod_Produced(colony, end_pop_ptr, 1);
            info->worst_worker_to_farmer_score = end_prod_2 - end_prod_1;
        } else {
            info->best_worker_to_farmer_score = (int16_t)0x8001;
            info->worst_worker_to_farmer_score = 0x7FFF;
        }
    }

    void __cdecl Assign_Buildings() {
        s_aidudes_colony_info* it = *our_colonies;
        s_aidudes_colony_info* end = it + n_our_colonies;

        while (it < end) {
            AIBUILD::Assign_Colony_Building(&MOX::colony[it->colony_idx]);
            ++it;
            end = (*our_colonies) + n_our_colonies;
        }
    }

    void __cdecl Freighter_Cheat(int32_t player_idx) {
        s_player* plr = &MOX::player[player_idx];

        if (plr->surplus_freighters < 0 || plr->surplus_food < 0) {
            int32_t threshold = 1 - (int32_t)MOX::settings.game_difficulty * (int32_t)plr->surplus_freighters;
            int32_t roll = (int32_t)game_random::Random(50);

            if (threshold >= roll) {
                plr->n_freighters += 5;
            } else if (plr->n_freighters == 0 && MOX::settings.game_difficulty >= GAME_DIFFICULTY_EASY) {
                plr->n_freighters = 5;
            }
        }
    }

    void __cdecl Assign_Additional_Research_Industry() {
        int industry_mult = 10;
        int research_mult = 18;

        if (aidudes::p_ai->surplus_bc < 0) {
            research_mult = 18 + (int)special::isqrt((uint32_t)-aidudes::p_ai->surplus_bc);
        }

        if (aidudes::p_ai->do_not_research == 0) {
            if (aidudes::p_ai->objectives == PLAYER_OBJECTIVE_TECHNOLOGIST) {
                industry_mult = 12;
            } else if (aidudes::p_ai->objectives == PLAYER_OBJECTIVE_INDUSTRIALIST) {
                research_mult += 2;
            }
        } else {
            industry_mult = 0;
        }

        COLCALC::Do_Player_Colony_Calculations(aidudes::i_ai);

        while (true) {
            s_aidudes_colony_info* best_colony = nullptr;

            if ((uint32_t)aidudes::p_ai->research_produced * (uint32_t)research_mult < (uint32_t)aidudes::p_ai->industry_produced * (uint32_t)industry_mult) {
                short max_score = (short)0x8001;
                for (int i = 0; i < (int)aidudes::n_our_unblockaded; ++i) {
                    if (aidudes::our_unblockaded[i].best_worker_to_farmer_score > max_score) {
                        max_score = aidudes::our_unblockaded[i].best_worker_to_farmer_score;
                        best_colony = &aidudes::our_unblockaded[i];
                    }
                }

                if (best_colony == nullptr) {
                    return;
                }
                aidudes::Grab_Scientist(best_colony);
            } else {
                short min_score = 0x7fff;
                for (int i = 0; i < (int)aidudes::n_our_unblockaded; ++i) {
                    if (aidudes::our_unblockaded[i].worst_worker_to_farmer_score < min_score) {
                        min_score = aidudes::our_unblockaded[i].worst_worker_to_farmer_score;
                        best_colony = &aidudes::our_unblockaded[i];
                    }
                }

                if (best_colony == nullptr) {
                    return;
                }
                aidudes::Grab_Worker(best_colony);
            }

            COLCALC::Colony_Calculation(best_colony->colony_idx);
        }
    }

    void __cdecl Grab_Scientist(s_aidudes_colony_info* colony_info) {
        int16_t colony_idx = colony_info->colony_idx;
        uint8_t cur_pop_idx = (uint8_t)colony_info->cur_pop_idx;

        s_colony& colony = MOX::colony[colony_idx];

        uint32_t* pop_flags = &colony.pop[cur_pop_idx];

        POP::Set_Prof(pop_flags, POP::PROF_SCIENTIST);
        POP::Set_Assigned(pop_flags, true);

        colony_info->cur_pop_idx = cur_pop_idx + 1;

        Compute_Max_Min_R_I_Worthiness(colony_info);
    }

    void __cdecl Grab_Worker(s_aidudes_colony_info* info) {
        s_colony* colony = &MOX::colony[info->colony_idx];

        info->pop_end_idx--;

        uint32_t* pop_record = &colony->pop[(uint8_t)info->pop_end_idx];

        POP::Set_Prof(pop_record, POP::PROF_WORKER);
        POP::Set_Assigned(pop_record, true);

        Compute_Max_Min_R_I_Worthiness(info);
    }

    void __cdecl Do_Unblockaded_Colony(s_aidudes_colony_info* info) {
        s_colony* colony = &MOX::colony[info->colony_idx];
        uint8_t n_pops = (uint8_t)colony->n_pops;

        // Calculate minimum required workers (ceiling of n_pops / 5)
        int32_t n_required_workers = (n_pops + 4) / 5;

        Assign_Required_Colonists(info);

        Sort_Pops_colony_data = colony;
        qsort(&colony->pop[info->first_assignable_pop_idx],
            n_pops - info->first_assignable_pop_idx,
            4,
            Sort_Pops_FR_I);

        if (EVENTS::Event_Check_Colony_Researching(info->colony_idx)) {
            n_required_workers = 0;
        }

        COLCALC::Pre_Import_Computing(colony);

        // Main loop to shift population to workers based on industry needs
        while (info->cur_pop_idx < info->pop_tail_idx) {
            int32_t needed_industry = (uint8_t)colony->industry2_needed_for_conquered +
                (uint8_t)colony->industry2_needed_for_our_empire +
                (uint8_t)colony->industry2_needed_for_androids +
                (uint8_t)colony->industry2_needed_for_assimilated;

            if (info->n_workers >= n_required_workers && needed_industry <= colony->production[1] * 2) {
                break;
            }

            info->pop_tail_idx--;
            uint8_t idx = info->pop_tail_idx;
            POP::Set_Prof(&colony->pop[idx], POP::PROF_WORKER);
            POP::Set_Assigned(&colony->pop[idx], true);
            info->n_workers++;

            COLCALC::Pre_Import_Computing(colony);
        }

        info->pop_end_idx = info->pop_tail_idx;
        info->pollution_at_start = (uint8_t)colony->pollution;

        Sort_Pops_colony_data = colony;
        qsort(&colony->pop[info->first_assignable_pop_idx],
            info->pop_end_idx - info->cur_pop_idx,
            4,
            Sort_Pops_R_I);

        Compute_Max_Min_R_I_Worthiness(info);
    }

    int __cdecl _Sort_Pops_colony(const void* a, const void* b) {
        s_colony* colony = aidudes::Sort_Pops_colony_data;

        // Get production values for both pops across two different production types
        int16_t p1_prod_1 = COLCALC::Colony_Pop_Prod_Produced(colony, (const uint32_t*)a, 1);
        int16_t p2_prod_1 = COLCALC::Colony_Pop_Prod_Produced(colony, (const uint32_t*)b, 1);
        int16_t p1_prod_2 = COLCALC::Colony_Pop_Prod_Produced(colony, (const uint32_t*)a, 2);
        int16_t p2_prod_2 = COLCALC::Colony_Pop_Prod_Produced(colony, (const uint32_t*)b, 2);

        // Calculate differences in production contributions
        int16_t diff1 = p1_prod_2 - p1_prod_1;
        int16_t diff2 = p2_prod_2 - p2_prod_1;

        // Primary sort: difference between production types
        if (diff2 != diff1) {
            return (int)diff2 - (int)diff1;
        }

        // Secondary sort: specific production type 1 contribution
        if (p1_prod_1 != p2_prod_1) {
            return (int)p1_prod_1 - (int)p2_prod_1;
        }

        // Tertiary sort: masked raw value comparison
        uint32_t val1 = POP::Load(a);
        uint32_t val2 = POP::Load(b);

        return (int)POP::Get_Race(val1) - (int)POP::Get_Race(val2);
    }

    int __cdecl Sort_Pops_F_IR(const void* a, const void* b) {
        const uint32_t* pop1 = (const uint32_t*)a;
        const uint32_t* pop2 = (const uint32_t*)b;

        int prod1 = (int)COLCALC::Colony_Pop_Prod_Produced(Sort_Pops_colony_data, pop1, 0);
        int prod2 = (int)COLCALC::Colony_Pop_Prod_Produced(Sort_Pops_colony_data, pop2, 0);

        int score1;
        if (POP::Is_Worker(POP::Load(pop1))) {
            score1 = prod1 - (int)COLCALC::Colony_Pop_Prod_Produced(Sort_Pops_colony_data, pop1, 1);
            if (Sort_Pops_colony_info->pollution_at_start < Sort_Pops_colony_data->pollution) {
                score1 += 1000;
            }
        } else {
            score1 = prod1 - (int)COLCALC::Colony_Pop_Prod_Produced(Sort_Pops_colony_data, pop1, 2);
        }

        int score2;
        if (POP::Is_Worker(POP::Load(pop2))) {
            score2 = prod2 - (int)COLCALC::Colony_Pop_Prod_Produced(Sort_Pops_colony_data, pop2, 1);
            if (Sort_Pops_colony_info->pollution_at_start < Sort_Pops_colony_data->pollution) {
                score2 += 1000;
            }
        } else {
            score2 = prod2 - (int)COLCALC::Colony_Pop_Prod_Produced(Sort_Pops_colony_data, pop2, 2);
        }

        if (score2 != score1) {
            return score2 - score1;
        }

        if (prod1 != prod2) {
            return prod1 - prod2;
        }

        uint32_t val1 = POP::Load(pop1);
        uint32_t val2 = POP::Load(pop2);

        if (POP::Get_Prof(val1) == POP::Get_Prof(val2)) {
            return (int)POP::Get_Race(val1) - (int)POP::Get_Race(val2);
        }

        return (int)POP::Get_Prof(val1) - (int)POP::Get_Prof(val2);
    }

    void __cdecl Make_First_Unassigned_Into_Farmer(s_aidudes_colony_info* info) {
        s_colony* colony = &MOX::colony[info->colony_idx];
        int16_t old_pollution = colony->pollution;

        uint32_t* pop = &colony->pop[(uint8_t)info->cur_pop_idx];
        POP::Set_Prof(pop, POP::PROF_FARMER);
        POP::Set_Assigned(pop, true);

        info->cur_pop_idx++;
        info->n_farmers++;

        COLCALC::Pre_Import_Computing(colony);

        if (colony->pollution < old_pollution) {
            aidudes::Sort_Pops_colony_info = info;
            aidudes::Sort_Pops_colony_data = colony;

            size_t n_to_sort = (size_t)(info->pop_end_idx - info->cur_pop_idx);
            qsort(&colony->pop[(uint8_t)info->cur_pop_idx], n_to_sort, sizeof(uint32_t), aidudes::Sort_Pops_F_IR);
        }

        if (info->n_farmers < (uint8_t)colony->max_farms && info->cur_pop_idx < (uint8_t)colony->n_pops) {
            info->best_worker_to_farmer_score = aidudes::F_IR_Worthiness(info, &colony->pop[(uint8_t)info->cur_pop_idx]);
        } else {
            info->best_worker_to_farmer_score = (int16_t)0x8001;
        }
    }

    void __cdecl Assign_Additional_Unblockaded_Farmers() {
        s_aidudes_colony_info* candidates[MAX_COLONIES];
        int num_candidates = 0;
        bool freighter_shortage_flag = false;
        bool allocation_failed = false;

        if (aidudes::n_our_unblockaded > 0) {
            s_aidudes_colony_info* info = aidudes::our_unblockaded;
            s_aidudes_colony_info* end = aidudes::our_unblockaded + aidudes::n_our_unblockaded;

            while (info < end) {
                s_colony* col = &MOX::colony[info->colony_idx];
                info->cur_pop_idx = info->first_assignable_pop_idx;
                info->pop_end_idx = info->pop_tail_idx;

                if ((uint8_t)info->cur_pop_idx < (uint8_t)info->pop_tail_idx &&
                    (uint8_t)info->n_farmers < (uint8_t)col->max_farms &&
                    col->food2_per_farmer != 0) {
                    aidudes::Sort_Pops_colony_data = col;
                    aidudes::Sort_Pops_colony_info = info;

                    qsort(&col->pop[(uint8_t)info->cur_pop_idx],
                        (uint8_t)info->pop_end_idx - (uint8_t)info->cur_pop_idx,
                        4,
                        aidudes::Sort_Pops_F_IR);

                    info->best_worker_to_farmer_score = aidudes::F_IR_Worthiness(info, &col->pop[(uint8_t)info->cur_pop_idx]);
                    candidates[num_candidates++] = info;
                }
                info++;
            }

            COLCALC::Do_Player_Colony_Post_Production(aidudes::i_ai);

            while ((aidudes::p_ai->surplus_food < 0 || aidudes::p_ai->surplus_freighters < 0) &&
                (num_candidates > 0 && !allocation_failed)) {
                int best_candidate_idx;

                if (aidudes::p_ai->surplus_freighters < 1) {
                    best_candidate_idx = -1;
                    freighter_shortage_flag = true;

                    for (int i = 0; i < num_candidates; ++i) {
                        s_aidudes_colony_info* candidate = candidates[i];
                        s_colony* col = &MOX::colony[candidate->colony_idx];

                        int food_needed = (uint8_t)col->food2_needed_for_our_empire +
                            (uint8_t)col->food2_needed_for_assimilated +
                            (uint8_t)col->food2_needed_for_conquered +
                            (uint8_t)col->food2_needed_for_natives;

                        if (food_needed > (col->production[0] * 2)) {
                            if (best_candidate_idx == -1 || candidate->best_worker_to_farmer_score > candidates[best_candidate_idx]->best_worker_to_farmer_score) {
                                best_candidate_idx = i;
                            }
                        }
                    }
                } else {
                    best_candidate_idx = 0;
                    for (int i = num_candidates - 1; i > 0; --i) {
                        if (candidates[i]->best_worker_to_farmer_score > candidates[best_candidate_idx]->best_worker_to_farmer_score) {
                            best_candidate_idx = i;
                        }
                    }
                }

                if (best_candidate_idx == -1) {
                    allocation_failed = true;
                } else {
                    s_aidudes_colony_info* target_info = candidates[best_candidate_idx];
                    aidudes::Make_First_Unassigned_Into_Farmer(target_info);
                    COLCALC::Colony_Calculation(target_info->colony_idx);

                    s_colony* target_col = &MOX::colony[target_info->colony_idx];
                    if ((uint8_t)target_info->cur_pop_idx >= (uint8_t)target_info->pop_end_idx ||
                        (uint8_t)target_info->n_farmers >= (uint8_t)target_col->max_farms) {
                        num_candidates--;
                        if (best_candidate_idx < num_candidates) {
                            memmove(&candidates[best_candidate_idx], &candidates[best_candidate_idx + 1], (num_candidates - best_candidate_idx) * sizeof(s_aidudes_colony_info*));
                        }
                    }
                }
            }
        }

        if (freighter_shortage_flag) {
            if ((int)game_random::Random(10) <= (int)MOX::settings.game_difficulty) {
                aidudes::p_ai->n_freighters += 5;
            }
        }
    }

    int __cdecl F_IR_Worthiness(s_aidudes_colony_info* info, uint32_t* flags) {
        s_colony* colony = &MOX::colony[info->colony_idx];
        int16_t base_production = COLCALC::Colony_Pop_Prod_Produced(colony, flags, 0);
        int worthiness;

        if (POP::Is_Worker(POP::Load(flags))) {
            int16_t mode1_production = COLCALC::Colony_Pop_Prod_Produced(colony, flags, 1);
            worthiness = (int)base_production - (int)mode1_production;

            // If pollution has increased since the start of evaluation
            if (colony->pollution > info->pollution_at_start) {
                return worthiness + 1000;
            }
        } else {
            int16_t mode2_production = COLCALC::Colony_Pop_Prod_Produced(colony, flags, 2);
            worthiness = (int)base_production - (int)mode2_production;
        }

        return worthiness;
    }

    int __cdecl Sort_Pops_FR_I(const void* a, const void* b) {
        const uint32_t* pop1 = (const uint32_t*)a;
        const uint32_t* pop2 = (const uint32_t*)b;

        if (aidudes::Sort_Pops_colony_data->food2_per_farmer == 0) {
            return aidudes::_Sort_Pops_colony(a, b);
        }

        int16_t prod_0_p1 = COLCALC::Colony_Pop_Prod_Produced(aidudes::Sort_Pops_colony_data, pop1, 0);
        int16_t prod_1_p1 = COLCALC::Colony_Pop_Prod_Produced(aidudes::Sort_Pops_colony_data, pop1, 1);
        int16_t prod_2_p1 = COLCALC::Colony_Pop_Prod_Produced(aidudes::Sort_Pops_colony_data, pop1, 2);

        int16_t score_p1 = prod_2_p1 - (prod_1_p1 * 2) + prod_0_p1;

        int16_t prod_0_p2 = COLCALC::Colony_Pop_Prod_Produced(aidudes::Sort_Pops_colony_data, pop2, 0);
        int16_t prod_1_p2 = COLCALC::Colony_Pop_Prod_Produced(aidudes::Sort_Pops_colony_data, pop2, 1);
        int16_t prod_2_p2 = COLCALC::Colony_Pop_Prod_Produced(aidudes::Sort_Pops_colony_data, pop2, 2);

        int16_t score_p2 = prod_2_p2 - (prod_1_p2 * 2) + prod_0_p2;

        if (score_p2 != score_p1) {
            return (int32_t)score_p2 - (int32_t)score_p1;
        }

        if (prod_0_p2 != prod_0_p1) {
            return (int32_t)prod_0_p2 - (int32_t)prod_0_p1;
        }

        if (prod_1_p2 != prod_1_p1) {
            return (int32_t)prod_1_p1 - (int32_t)prod_1_p2;
        }

        return (int)POP::Get_Race(POP::Load(pop1)) - (int)POP::Get_Race(POP::Load(pop2));
    }

    int __cdecl Sort_Pops_R_I(const void* p1, const void* p2) {
        int16_t p1_prod1 = COLCALC::Colony_Pop_Prod_Produced(aidudes::Sort_Pops_colony_data, (const uint32_t*)p1, 1);
        int16_t p2_prod1 = COLCALC::Colony_Pop_Prod_Produced(aidudes::Sort_Pops_colony_data, (const uint32_t*)p2, 1);
        int16_t p1_prod2 = COLCALC::Colony_Pop_Prod_Produced(aidudes::Sort_Pops_colony_data, (const uint32_t*)p1, 2);
        int16_t p2_prod2 = COLCALC::Colony_Pop_Prod_Produced(aidudes::Sort_Pops_colony_data, (const uint32_t*)p2, 2);

        int16_t p2_delta = p2_prod2 - p2_prod1;
        int16_t p1_delta = p1_prod2 - p1_prod1;

        if (p2_delta != p1_delta) {
            return (int)p2_delta - (int)p1_delta;
        }

        if (p2_prod1 == p1_prod1) {
            return (int)POP::Get_Race(POP::Load(p1)) - (int)POP::Get_Race(POP::Load(p2));
        }

        return (int)p1_prod1 - (int)p2_prod1;
    }
} // namespace aidudes