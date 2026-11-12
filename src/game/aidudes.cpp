#include "pch.h"
#include "legacy_world.h"

namespace aidudes {

    void __cdecl All_Colony_AI(AIDATA::s_state* ai, const game::s_view* world) {
        int16_t num_players = (int16_t)world->Player_Count();
        s_player* plr = world->players;

        for (int16_t i = 0; i < num_players; ++i, ++plr) {
            if (plr->objectives != PLAYER_OBJECTIVE_HUMAN) {
                Colony_AI(ai, world, legacy_world::Player_Id(i));
            }
        }

        s_colony* col = world->colonies;
        s_colony* end = world->colonies + world->Colony_Count();
        while (col < end) {
            if (col->producing[0] == COLONY_PRODUCTION_AI_TRADE_GOODS) {
                col->producing[0] = COLONY_PRODUCTION_TRADE_GOODS;
            }
            ++col;
        }
    }

    void __cdecl Industry_Sanity_Check(s_ai_colony_context* ctx) {
        const auto* world = ctx->world;
        int n_colonies = (int)(ctx->work->colonies.size() - ctx->work->blockaded_count);
        AIDATA::s_colony_assignment* colony_infos = ctx->work->colonies.data() + ctx->work->blockaded_count;

        for (int i = 0; i < n_colonies; ++i) {
            AIDATA::s_colony_assignment* info = &colony_infos[i];
            const uint32_t colony_idx = legacy_world::Slot(info->colony_id);
            const auto legacy_colony = legacy_world::Index(info->colony_id);
            if (!legacy_colony) continue;
            s_colony* col = &world->colonies[colony_idx];

            bool is_colony_producing_trade_goods = (col->producing[0] == COLONY_PRODUCTION_TRADE_GOODS);

            if ((is_colony_producing_trade_goods && ctx->p_ai->do_not_research == 0) ||
                EVENTS::Event_Check_Colony_Researching(*legacy_colony) != 0) {
                for (int j = (int)info->pop_tail_idx - 1; j >= (int)info->first_assignable_pop_idx; --j) {
                    uint32_t* pop_entry = &col->pop[j];

                    if (!POP::Is_Worker(POP::Load(pop_entry))) {
                        break;
                    }

                    POP::Set_Prof(pop_entry, POP::PROF_SCIENTIST);
                }

                COLCALC::Colony_Calculation(*legacy_colony);
            }
        }
    }

    void __cdecl Colony_AI_Given_Tax(AIDATA::s_state* ai, s_ai_colony_context* ctx) {
        const auto legacy_player = legacy_world::Index(ctx->i_ai);
        if (!legacy_player) return;
        const auto* world = ctx->world;
        for (uint32_t i = 0; i < ctx->work->blockaded_count; ++i) {
            aidudes::Do_Blockaded_Colony(world, &ctx->work->colonies[i]);
        }

        for (uint32_t i = ctx->work->blockaded_count; i < ctx->work->colonies.size(); ++i) {
            aidudes::Do_Unblockaded_Colony(world, &ctx->work->colonies[i]);
        }

        aidudes::Assign_Additional_Research_Industry(ctx);
        aidudes::Assign_Empire_Building(ai, world, ctx->i_ai);
        aidudes::Assign_Buildings(ai, world, ctx->work);
        aidudes::Industry_Sanity_Check(ctx);

        if (ctx->work->colonies.size() > ctx->work->blockaded_count) {
            aidudes::Assign_Additional_Unblockaded_Farmers(ctx);
        } else {
            COLCALC::Do_Player_Colony_Post_Production(*legacy_player);
        }
    }

    void Colony_AI(AIDATA::s_state* ai, const game::s_view* world, game::id_player player_id) {
        const uint32_t player_idx = legacy_world::Slot(player_id);
        auto& work = ai->players[player_idx].colony_work;
        work = {};
        s_ai_colony_context context{player_id, &world->players[player_idx], &work, world};
        s_ai_colony_context* ctx = &context;

        aidudes::Collect_AI_Colonies(ctx);

        if (world->settings->game_difficulty > GAME_DIFFICULTY_TUTOR) {
            if (AIBUILD::Player_War_Level(world, ctx->i_ai) < 3) {
                aidudes::Clear_Build_Queues_If_New_Tech(ctx, ctx->p_ai->got_new_app);
            }
        }

        aidudes::Keep_Ship_Designs_Up_To_Date(ctx);
        aidudes::Freighter_Cheat(world, ctx->i_ai);
        aidudes::Do_Ship_Upgrade_Cheats(ai, ctx);
        AIBUILD::Empire_Buy_Outright(ai, world, ctx->i_ai);
        aidudes::Colony_AI_Given_Tax(ai, ctx);

        work.complete = true;
    }

    void __cdecl Clear_Build_Queues_If_New_Tech(s_ai_colony_context* ctx, bool force_clear) {
        const auto* world = ctx->world;
        s_player* p_ai = ctx->p_ai;
        AIDATA::s_colony_assignment* current_info = ctx->work->colonies.data();
        int num_colonies = (int)ctx->work->colonies.size();

        if (num_colonies == 0) return;

        for (int i = 0; i < num_colonies; ++i, ++current_info) {
            const uint32_t colony_idx = legacy_world::Slot(current_info->colony_id);
            s_colony* colony = &world->colonies[colony_idx];

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
                    if (world->ships[ship_idx].status == 4) {
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

                const uint32_t player_idx = legacy_world::Slot(ctx->i_ai);
                int trait_check = world->players[player_idx].traits[TRAIT_CURRENT_GOVERNMENT] / 2;

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
                    int8_t gravity = world->planets[colony->planet].gravity_class;
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

                auto choice = RUSS::Choose_Weighted_Index_U8(world->random_seed, weights, BUILDING_COUNT + 1);
                if (!choice || *choice < BUILDING_COUNT) {
                    const auto legacy_colony = legacy_world::Index(current_info->colony_id);
                    if (!legacy_colony) continue;
                    COLBLDG::Clear_Colony_Queue(*legacy_colony);
                    colony->producing[0] = choice ? (int16_t)*choice : COLONY_PRODUCTION_NONE;
                }
            }
        }
    }

    int __cdecl Sort_Colonies_Blockaded(void* context, const void* a, const void* b) {
        auto* colonies = static_cast<s_colony*>(context);
        const auto* info1 = static_cast<const AIDATA::s_colony_assignment*>(a);
        const auto* info2 = static_cast<const AIDATA::s_colony_assignment*>(b);

        s_colony* colony1 = &colonies[legacy_world::Slot(info1->colony_id)];
        s_colony* colony2 = &colonies[legacy_world::Slot(info2->colony_id)];

        bool blockaded2 = COLCALC::Colony_Is_Blockaded(colony2);
        bool blockaded1 = COLCALC::Colony_Is_Blockaded(colony1);

        return (int)blockaded2 - (int)blockaded1;
    }

    void __cdecl Collect_AI_Colonies(s_ai_colony_context* ctx) {
        const auto* world = ctx->world;
        ctx->work->colonies.clear();
        ctx->work->blockaded_count = 0;

        for (int16_t i = 0; i < world->Colony_Count(); ++i) {
            s_colony* colony_ptr = &world->colonies[i];

            if (colony_ptr->owner == legacy_world::Slot(ctx->i_ai)) {
                if (EVENTS::Event_Check_Space_Anomaly(i) == 0 && colony_ptr->outpost_flag == 0) {
                    AIDATA::s_colony_assignment info = {};
                    info.colony_id = legacy_world::Colony_Id(i);
                    ctx->work->colonies.push_back(info);

                    if (COLCALC::Colony_Is_Blockaded(colony_ptr)) {
                        ctx->work->blockaded_count++;
                    }
                }
            }
        }

        qsort(ctx->work->colonies.data(), ctx->work->colonies.size(), sizeof(AIDATA::s_colony_assignment), Sort_Colonies_Blockaded, world->colonies);

    }

    void __cdecl Compute_Max_Min_R_I_Worthiness(const game::s_view* world, AIDATA::s_colony_assignment* info) {
        if (info->cur_pop_idx < info->pop_end_idx) {
            s_colony* colony = &world->colonies[legacy_world::Slot(info->colony_id)];

            uint32_t* cur_pop_ptr = &colony->pop[info->cur_pop_idx];
            int16_t cur_prod_2 = COLCALC::Colony_Pop_Prod_Produced(colony, cur_pop_ptr, 2);
            int16_t cur_prod_1 = COLCALC::Colony_Pop_Prod_Produced(colony, cur_pop_ptr, 1);
            info->best_reassignment_score = cur_prod_2 - cur_prod_1;

            uint32_t* end_pop_ptr = &colony->pop[info->pop_end_idx - 1];
            int16_t end_prod_2 = COLCALC::Colony_Pop_Prod_Produced(colony, end_pop_ptr, 2);
            int16_t end_prod_1 = COLCALC::Colony_Pop_Prod_Produced(colony, end_pop_ptr, 1);
            info->worst_reassignment_score = end_prod_2 - end_prod_1;
        } else {
            info->best_reassignment_score = (int16_t)0x8001;
            info->worst_reassignment_score = 0x7FFF;
        }
    }

    void __cdecl Assign_Buildings(AIDATA::s_state* ai, const game::s_view* world, const AIDATA::s_colony_workspace* work) {
        for (const auto& colony : work->colonies) {
            AIBUILD::Assign_Colony_Building(ai, world, colony.colony_id, &world->colonies[legacy_world::Slot(colony.colony_id)]);
        }
    }

    void Freighter_Cheat(const game::s_view* world, game::id_player player_id) {
        const uint32_t player_idx = legacy_world::Slot(player_id);
        s_player* plr = &world->players[player_idx];

        if (plr->surplus_freighters < 0 || plr->surplus_food < 0) {
            int32_t threshold = 1 - (int32_t)world->settings->game_difficulty * (int32_t)plr->surplus_freighters;
            int32_t roll = (int32_t)game_random::Random_From_Seed(world->random_seed, 50);

            if (threshold >= roll) {
                plr->n_freighters += 5;
            } else if (plr->n_freighters == 0 && world->settings->game_difficulty >= GAME_DIFFICULTY_EASY) {
                plr->n_freighters = 5;
            }
        }
    }

    void __cdecl Assign_Additional_Research_Industry(s_ai_colony_context* ctx) {
        const auto legacy_player = legacy_world::Index(ctx->i_ai);
        if (!legacy_player) return;
        const auto* world = ctx->world;
        const auto* p_ai = ctx->p_ai;
        auto* our_unblockaded = ctx->work->colonies.data() + ctx->work->blockaded_count;
        const uint32_t n_our_unblockaded = (uint32_t)ctx->work->colonies.size() - ctx->work->blockaded_count;
        int industry_mult = 10;
        int research_mult = 18;

        if (p_ai->surplus_bc < 0) {
            research_mult = 18 + (int)special::isqrt((uint32_t)-p_ai->surplus_bc);
        }

        if (p_ai->do_not_research == 0) {
            if (p_ai->objectives == PLAYER_OBJECTIVE_TECHNOLOGIST) {
                industry_mult = 12;
            } else if (p_ai->objectives == PLAYER_OBJECTIVE_INDUSTRIALIST) {
                research_mult += 2;
            }
        } else {
            industry_mult = 0;
        }

        COLCALC::Do_Player_Colony_Calculations(*legacy_player);

        while (true) {
            AIDATA::s_colony_assignment* best_colony = nullptr;

            if ((uint32_t)p_ai->research_produced * (uint32_t)research_mult < (uint32_t)p_ai->industry_produced * (uint32_t)industry_mult) {
                short max_score = (short)0x8001;
                for (uint32_t i = 0; i < n_our_unblockaded; ++i) {
                    if (our_unblockaded[i].best_reassignment_score > max_score) {
                        max_score = our_unblockaded[i].best_reassignment_score;
                        best_colony = &our_unblockaded[i];
                    }
                }

                if (best_colony == nullptr) {
                    return;
                }
                aidudes::Grab_Scientist(world, best_colony);
            } else {
                short min_score = 0x7fff;
                for (uint32_t i = 0; i < n_our_unblockaded; ++i) {
                    if (our_unblockaded[i].worst_reassignment_score < min_score) {
                        min_score = our_unblockaded[i].worst_reassignment_score;
                        best_colony = &our_unblockaded[i];
                    }
                }

                if (best_colony == nullptr) {
                    return;
                }
                aidudes::Grab_Worker(world, best_colony);
            }

            const auto legacy_colony = legacy_world::Index(best_colony->colony_id);
            if (!legacy_colony) return;
            COLCALC::Colony_Calculation(*legacy_colony);
        }
    }

    void __cdecl Grab_Scientist(const game::s_view* world, AIDATA::s_colony_assignment* colony_info) {
        const uint32_t colony_idx = legacy_world::Slot(colony_info->colony_id);
        uint8_t cur_pop_idx = (uint8_t)colony_info->cur_pop_idx;

        s_colony& colony = world->colonies[colony_idx];

        uint32_t* pop_flags = &colony.pop[cur_pop_idx];

        POP::Set_Prof(pop_flags, POP::PROF_SCIENTIST);
        POP::Set_Assigned(pop_flags, true);

        colony_info->cur_pop_idx = cur_pop_idx + 1;

        Compute_Max_Min_R_I_Worthiness(world, colony_info);
    }

    void __cdecl Grab_Worker(const game::s_view* world, AIDATA::s_colony_assignment* info) {
        s_colony* colony = &world->colonies[legacy_world::Slot(info->colony_id)];

        info->pop_end_idx--;

        uint32_t* pop_record = &colony->pop[(uint8_t)info->pop_end_idx];

        POP::Set_Prof(pop_record, POP::PROF_WORKER);
        POP::Set_Assigned(pop_record, true);

        Compute_Max_Min_R_I_Worthiness(world, info);
    }

    void __cdecl Do_Unblockaded_Colony(const game::s_view* world, AIDATA::s_colony_assignment* info) {
        s_colony* colony = &world->colonies[legacy_world::Slot(info->colony_id)];
        uint8_t n_pops = (uint8_t)colony->n_pops;

        // Calculate minimum required workers (ceiling of n_pops / 5)
        int32_t n_required_workers = (n_pops + 4) / 5;

        Assign_Required_Colonists(world, info);

        qsort(&colony->pop[info->first_assignable_pop_idx],
            n_pops - info->first_assignable_pop_idx,
            4,
            Sort_Pops_FR_I, colony);

        const auto legacy_colony = legacy_world::Index(info->colony_id);
        if (legacy_colony && EVENTS::Event_Check_Colony_Researching(*legacy_colony)) {
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

        qsort(&colony->pop[info->first_assignable_pop_idx],
            info->pop_end_idx - info->cur_pop_idx,
            4,
            Sort_Pops_R_I, colony);

        Compute_Max_Min_R_I_Worthiness(world, info);
    }

    int __cdecl _Sort_Pops_colony(s_colony* colony, const void* a, const void* b) {

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

    int __cdecl Sort_Pops_F_IR(void* context, const void* a, const void* b) {
        auto* sort = static_cast<s_population_sort_context*>(context);
        auto* Sort_Pops_colony_data = sort->colony;
        const auto* Sort_Pops_colony_info = sort->info;
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

    void __cdecl Make_First_Unassigned_Into_Farmer(const game::s_view* world, AIDATA::s_colony_assignment* info) {
        s_colony* colony = &world->colonies[legacy_world::Slot(info->colony_id)];
        int16_t old_pollution = colony->pollution;

        uint32_t* pop = &colony->pop[(uint8_t)info->cur_pop_idx];
        POP::Set_Prof(pop, POP::PROF_FARMER);
        POP::Set_Assigned(pop, true);

        info->cur_pop_idx++;
        info->n_farmers++;

        COLCALC::Pre_Import_Computing(colony);

        if (colony->pollution < old_pollution) {
            s_population_sort_context sort{colony, info};

            const uint32_t n_to_sort = (uint32_t)(info->pop_end_idx - info->cur_pop_idx);
            qsort(&colony->pop[(uint8_t)info->cur_pop_idx], n_to_sort, sizeof(uint32_t), aidudes::Sort_Pops_F_IR, &sort);
        }

        if (info->n_farmers < (uint8_t)colony->max_farms && info->cur_pop_idx < (uint8_t)colony->n_pops) {
            info->best_reassignment_score = aidudes::F_IR_Worthiness(world, info, &colony->pop[(uint8_t)info->cur_pop_idx]);
        } else {
            info->best_reassignment_score = (int16_t)0x8001;
        }
    }

    void __cdecl Assign_Additional_Unblockaded_Farmers(s_ai_colony_context* ctx) {
        const auto legacy_player = legacy_world::Index(ctx->i_ai);
        if (!legacy_player) return;
        const auto* world = ctx->world;
        std::vector<AIDATA::s_colony_assignment*> candidates(ctx->work->colonies.size());
        int num_candidates = 0;
        bool freighter_shortage_flag = false;
        bool allocation_failed = false;

        if (ctx->work->colonies.size() > ctx->work->blockaded_count) {
            AIDATA::s_colony_assignment* info = ctx->work->colonies.data() + ctx->work->blockaded_count;
            AIDATA::s_colony_assignment* end = ctx->work->colonies.data() + ctx->work->colonies.size();

            while (info < end) {
                s_colony* col = &world->colonies[legacy_world::Slot(info->colony_id)];
                info->cur_pop_idx = info->first_assignable_pop_idx;
                info->pop_end_idx = info->pop_tail_idx;

                if ((uint8_t)info->cur_pop_idx < (uint8_t)info->pop_tail_idx &&
                    (uint8_t)info->n_farmers < (uint8_t)col->max_farms &&
                    col->food2_per_farmer != 0) {
                    s_population_sort_context sort{col, info};

                    qsort(&col->pop[(uint8_t)info->cur_pop_idx],
                        (uint8_t)info->pop_end_idx - (uint8_t)info->cur_pop_idx,
                        4,
                        aidudes::Sort_Pops_F_IR, &sort);

                    info->best_reassignment_score = aidudes::F_IR_Worthiness(world, info, &col->pop[(uint8_t)info->cur_pop_idx]);
                    candidates[num_candidates++] = info;
                }
                info++;
            }

            COLCALC::Do_Player_Colony_Post_Production(*legacy_player);

            while ((ctx->p_ai->surplus_food < 0 || ctx->p_ai->surplus_freighters < 0) &&
                (num_candidates > 0 && !allocation_failed)) {
                int best_candidate_idx;

                if (ctx->p_ai->surplus_freighters < 1) {
                    best_candidate_idx = -1;
                    freighter_shortage_flag = true;

                    for (int i = 0; i < num_candidates; ++i) {
                        AIDATA::s_colony_assignment* candidate = candidates[i];
                        s_colony* col = &world->colonies[legacy_world::Slot(candidate->colony_id)];

                        int food_needed = (uint8_t)col->food2_needed_for_our_empire +
                            (uint8_t)col->food2_needed_for_assimilated +
                            (uint8_t)col->food2_needed_for_conquered +
                            (uint8_t)col->food2_needed_for_natives;

                        if (food_needed > (col->production[0] * 2)) {
                            if (best_candidate_idx == -1 || candidate->best_reassignment_score > candidates[best_candidate_idx]->best_reassignment_score) {
                                best_candidate_idx = i;
                            }
                        }
                    }
                } else {
                    best_candidate_idx = 0;
                    for (int i = num_candidates - 1; i > 0; --i) {
                        if (candidates[i]->best_reassignment_score > candidates[best_candidate_idx]->best_reassignment_score) {
                            best_candidate_idx = i;
                        }
                    }
                }

                if (best_candidate_idx == -1) {
                    allocation_failed = true;
                } else {
                    AIDATA::s_colony_assignment* target_info = candidates[best_candidate_idx];
                    aidudes::Make_First_Unassigned_Into_Farmer(world, target_info);
                    const auto legacy_colony = legacy_world::Index(target_info->colony_id);
                    if (!legacy_colony) return;
                    COLCALC::Colony_Calculation(*legacy_colony);

                    s_colony* target_col = &world->colonies[legacy_world::Slot(target_info->colony_id)];
                    if ((uint8_t)target_info->cur_pop_idx >= (uint8_t)target_info->pop_end_idx ||
                        (uint8_t)target_info->n_farmers >= (uint8_t)target_col->max_farms) {
                        num_candidates--;
                        if (best_candidate_idx < num_candidates) {
                            memmove(&candidates[best_candidate_idx], &candidates[best_candidate_idx + 1], (num_candidates - best_candidate_idx) * sizeof(AIDATA::s_colony_assignment*));
                        }
                    }
                }
            }
        }

        if (freighter_shortage_flag) {
            if ((int)game_random::Random_From_Seed(world->random_seed, 10) <= (int)world->settings->game_difficulty) {
                ctx->p_ai->n_freighters += 5;
            }
        }
    }

    int __cdecl F_IR_Worthiness(const game::s_view* world, AIDATA::s_colony_assignment* info, uint32_t* flags) {
        s_colony* colony = &world->colonies[legacy_world::Slot(info->colony_id)];
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

    int __cdecl Sort_Pops_FR_I(void* context, const void* a, const void* b) {
        auto* colony = static_cast<s_colony*>(context);
        const uint32_t* pop1 = (const uint32_t*)a;
        const uint32_t* pop2 = (const uint32_t*)b;

        if (colony->food2_per_farmer == 0) {
            return aidudes::_Sort_Pops_colony(colony, a, b);
        }

        int16_t prod_0_p1 = COLCALC::Colony_Pop_Prod_Produced(colony, pop1, 0);
        int16_t prod_1_p1 = COLCALC::Colony_Pop_Prod_Produced(colony, pop1, 1);
        int16_t prod_2_p1 = COLCALC::Colony_Pop_Prod_Produced(colony, pop1, 2);

        int16_t score_p1 = prod_2_p1 - (prod_1_p1 * 2) + prod_0_p1;

        int16_t prod_0_p2 = COLCALC::Colony_Pop_Prod_Produced(colony, pop2, 0);
        int16_t prod_1_p2 = COLCALC::Colony_Pop_Prod_Produced(colony, pop2, 1);
        int16_t prod_2_p2 = COLCALC::Colony_Pop_Prod_Produced(colony, pop2, 2);

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

    int __cdecl Sort_Pops_R_I(void* context, const void* p1, const void* p2) {
        auto* colony = static_cast<s_colony*>(context);
        int16_t p1_prod1 = COLCALC::Colony_Pop_Prod_Produced(colony, (const uint32_t*)p1, 1);
        int16_t p2_prod1 = COLCALC::Colony_Pop_Prod_Produced(colony, (const uint32_t*)p2, 1);
        int16_t p1_prod2 = COLCALC::Colony_Pop_Prod_Produced(colony, (const uint32_t*)p1, 2);
        int16_t p2_prod2 = COLCALC::Colony_Pop_Prod_Produced(colony, (const uint32_t*)p2, 2);

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
