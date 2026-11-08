#include "pch.h"
#include "legacy_world.h"

#include "build_queue.h"
#include "lua/lua_game.h"

namespace COLCALC {
    static uint64_t Integer_Square_Root(uint64_t value) {
        if (value <= UINT32_MAX) {
            return special::isqrt(static_cast<uint32_t>(value));
        }
        uint64_t result = 0;
        uint64_t bit = UINT64_C(1) << 62;
        while (bit > value) {
            bit >>= 2;
        }
        while (bit != 0) {
            if (value >= result + bit) {
                value -= result + bit;
                result = (result >> 1) + bit;
            } else {
                result >>= 1;
            }
            bit >>= 2;
        }
        return result;
    }

    static bool Government_Ignores_Mixed_Race_Penalty(const s_colony* colony) {
        uint8_t owner = (uint8_t)colony->owner;
        if (owner >= MAX_PLAYERS) {
            return false;
        }

        uint8_t government = MOX::player[owner].traits[TRAIT_CURRENT_GOVERNMENT];
        return government < GOVERNMENT_COUNT &&
            ignore_mixed_race_penalty[government] != 0;
    }

    static int16_t Prepare_Production_Item(uint32_t colony_id, s_colony* colony) {
        int16_t current_product = colony->producing[0];
        if (BUILD_QUEUE::Is_Legacy_Autobuild(colony->auto_building) &&
            (current_product == COLONY_PRODUCTION_HOUSING ||
                current_product == COLONY_PRODUCTION_TRADE_GOODS ||
                current_product == COLONY_PRODUCTION_NONE)) {
            AIBUILD::Player_Colony_Autobuild(&AIDATA::state, &game_world::current, colony_id, colony);
        }

        colony->just_produced = COLONY_PRODUCTION_NONE;
        return colony->producing[0];
    }

    static bool Is_Multibuild_Nonbuilding_Product(int16_t product) {
        return COLBLDG::Colony_Production_Is_Ship(product) ||
            product == COLONY_PRODUCTION_FREIGHTERS ||
            product == COLONY_PRODUCTION_SPY ||
            product == COLONY_PRODUCTION_FARMER ||
            product == COLONY_PRODUCTION_WORKER ||
            product == COLONY_PRODUCTION_SCIENTIST;
    }

    ColonyBaseFunc colony_empire_base_functions[3] = {
        Colony_Empire_Base_Food2_Produced,
        Colony_Empire_Base_Industry_Produced,
        Colony_Empire_Base_Research_Produced,
    };

    char* government_morale_msg;
    char* mixed_races_msg;
    char* unification_has_no_morale_msg;
    char* research_summary_msg;
    char* no_capital_msg;
    char* no_barracks_msg;
    char* morale_summary_msg;
    char* total_morale_msg;
    char* command_summary_msg;
    char* ship_maintenance_costs_msg;
    char* d_from_1_s_ship_msg;
    char* d_from_d_battlestations_msg;
    char* from_warlord_msg;
    char* d_from_1_star_base_msg;
    char* d_from_1_noncombat_ship_msg;
    char* d_from_d_fortresses_msg;
    char* starting_command_points_msg;
    char* d_from_d_star_bases_msg;
    char* total_command_point_used_msg;
    char* d_from_d_s_ships_msg;
    char* d_from_d_noncombat_ships_msg;
    char* total_command_points_msg;
    char* bc_summary_msg;
    char* total_command_points_used_msg;
    char* from_imperium_msg;
    char* d_from_1_battlestation_msg;
    char* d_from_1_fortress_msg;

    int8_t size_climate_max_pop_lookup[PLANET_CLIMATE_COUNT] = {25, 25, 25, 25, 25, 25, 40, 60, 80, 100};
    char* government_bonus_msg;
    char* produced_by_farmers_msg;
    char* produced_by_workers_msg;
    char* government_penalty_msg;
    char* native_bonus_msg;
    char* conquered_penalty_msg;
    char* produced_by_scientists_msg;
    char* anomaly_msg;
    char* bonus_msg;
    char* food_summary_msg;
    char* blockaded_penalty_msg;
    char* morale_penalty_msg;
    char* total_research_produced_msg;
    char* artifacts_bonus_msg;
    char* total_food_produced_msg;
    char* gravity_penalty_msg;
    char* aquatic_bonus_msg;
    char* leader_bonus_msg;
    char* android_bonus_msg;
    char* intelligence_bonus_msg;
    char* morale_bonus_msg;
    char* special_income_msg;
    char* taxed_from_industry_msg;
    char* used_msg;
    char* pollution_penalty_msg;
    char* replicated_to_d_food_msg;
    char* taxes_collected_msg;
    char* trade_goods_msg;
    char* total_industry_produced_msg;
    char* total_income_msg;
    char* industry_summary_msg;

    char* taxed_msg;
    char* base_msg;

    char tmp_quotient[20];

    int16_t __cdecl Get_System_Population(int16_t player_idx, int16_t star_idx) {
        int16_t total_pop = 0;

        for (int i = 0; i < 5; ++i) {
            int16_t planet_idx = MOX::star[star_idx].planet_index[i];

            if (planet_idx != -1) {
                int16_t colony_idx = MOX::planet[planet_idx].colony_index;

                if (colony_idx != -1) {
                    // Check if colony belongs to the specified player
                    if (MOX::colony[colony_idx].owner == player_idx) {
                        // n_pops is accumulated as an unsigned byte value
                        total_pop += (uint8_t)MOX::colony[colony_idx].n_pops;
                    }
                }
            }
        }

        return total_pop;
    }

    void __cdecl Show_Command_Points(int player_idx) {
        char msg_buffer[3000];
        STRBUILD::s_builder builder = STRBUILD::Init(msg_buffer, sizeof(msg_buffer));

        // Assembly calculates offset: player_idx * 3753 (0xEA9)
        Compute_Player_Maintenance(&MOX::player[player_idx], &builder);

        TEXTBOX::Text_Box(command_summary_msg, msg_buffer);
    }

    int __cdecl Colony_Officer(s_colony* colony) {
        int16_t star_idx = MOX::planet[colony->planet].star_index;

        int8_t officer_idx = (int8_t)MOX::star[star_idx].officer_index[colony->owner];

        if (officer_idx != -1) {
            if (MOX::leaders[officer_idx].eta != 0) {
                return -1;
            }
        }

        return (int)officer_idx;
    }

    void __cdecl Twiddle_Initial_Homeworlds() {
        int min_command_points = 30000;

        CONTACT::Compute_Contacts();
        RUSS::Mox_Update();

        for (int16_t i = 0; i < MOX::NUM_COLONIES; ++i) {
            Twiddle_Initial_Homeworld(&MOX::colony[i]);
        }

        SHIPMOVE::Update_All_Ship_Ranges();
        All_Colony_Calculations();

        for (int i = 0; i < MOX::NUM_PLAYERS; ++i) {
            s_player* player = &MOX::player[i];
            int16_t star_idx = MOX::planet[player->home_planet_id].star_index;
            MOX::star[star_idx].colonize_player = (int8_t)i;

            if (player->command_points < (int16_t)min_command_points) {
                min_command_points = player->command_points;
            }
        }

        if (MOX::settings.starting_civilization_level ==
            STARTING_CIVILIZATION_LEVEL_ADVANCED) {
            int starting_ship_points = (min_command_points - 2) / 2;

            for (int i = 0; i < MOX::NUM_PLAYERS; ++i) {
                s_player* player = &MOX::player[i];
                RUSS::Mox_Update();

                int16_t star_idx = MOX::planet[player->home_planet_id].star_index;
                MOX::star[star_idx].colonize_player = (int8_t)i;

                player->bc = (player->traits[TRAIT_TAX_BONUS] * 100 + 200);

                for (int16_t s = 0; s < MOX::NUM_STARS; ++s) {
                    s_star_data* star = &MOX::star[s];
                    if (star->system_special < SYSTEM_SPECIAL_SPLINTER_COLONY ||
                        star->system_special > SYSTEM_SPECIAL_MAROONED_HERO) {
                        if (SHIPMOVE::Star_In_Normal_Range_Of_Player(star, (int16_t)i)) {
                            star->visited |= (1 << i);

                            if (star->system_special > SYSTEM_SPECIAL_STABLE_WORMHOLE &&
                                star->system_special < SYSTEM_SPECIAL_GOLD_DEPOSITS) {
                                if (Monster_At_Star(s) == 0) {
                                    star->system_special = SYSTEM_SPECIAL_NO_SPECIAL;
                                }
                            }
                        }
                    }
                }

                int freighter_pool = 10;
                if (player->surplus_freighters < 0) freighter_pool -= player->surplus_freighters;
                if (player->surplus_food < 0) freighter_pool -= player->surplus_food;
                player->n_freighters = (int16_t)((freighter_pool + 4) / 5) * 5;

                int16_t first_colony = Get_First_Player_Colony_Idx(i);
                int pool_remaining = min_command_points;
                if (pool_remaining < 0) {
                    pool_remaining = 0;
                }

                int max_design_idx;
                if (starting_ship_points < 0) {
                    max_design_idx = 0;
                } else {
                    max_design_idx = starting_ship_points;
                    if (max_design_idx >= 4) {
                        max_design_idx = 4;
                        if (player->tech_applications[TECH_APP_TITAN_CONSTRUCTION] != TECH_RESEARCH_STATUS_RESEARCHED) {
                            max_design_idx = 3;
                        }
                    }
                }

                for (;;) {
                    RUSS::Mox_Update();
                    if (pool_remaining == 0) {
                        break;
                    }

                    if (pool_remaining - 1 < max_design_idx) {
                        max_design_idx = pool_remaining - 1;
                    }

                    MOX::ship[MOX::NUM_SHIPS].owner = (int8_t)i;
                    INITSHIP::Update_Player_Ship_Designs((int16_t)i);
                    COLBLDG::Create_Ship((int16_t)i, (int16_t)max_design_idx, first_colony);

                    pool_remaining -= (max_design_idx + 1);
                }
            }
            All_Colony_Calculations();
        }
    }

    void __cdecl Compute_All_Star_Colony_Stuff() {
        int16_t num_stars = MOX::NUM_STARS;
        s_star_data* star_ptr = MOX::star;
        s_star_data* end_ptr = &MOX::star[num_stars];

        if (star_ptr < end_ptr) {
            do {
                Compute_Star_Colony_Stuff(star_ptr);
                star_ptr++;
            } while (star_ptr < end_ptr);
        }
    }

    void __cdecl Star_Colony_Calculation(int16_t star_idx, int16_t player_idx) {
        s_star_data* star = &MOX::star[star_idx];

        for (int i = 0; i < 5; ++i) {
            int16_t planet_idx = star->planet_index[i];
            if (planet_idx != -1) {
                int16_t colony_idx = MOX::planet[planet_idx].colony_index;
                if (colony_idx != -1) {
                    s_colony* colony = &MOX::colony[colony_idx];

                    if (colony->owner == player_idx && colony->outpost_flag == 0) {
                        Recalculate_Colony(colony);
                    }
                }
            }
        }

        Update_Player_Stats(&MOX::player[player_idx]);
    }

    void __cdecl Compute_Star_Colony_Stuff(s_star_data* star) {
        uint8_t old_interdictor_state = star->has_warp_field_interdictor;

        star->has_colony = 0;
        star->has_warp_field_interdictor = 0;
        star->has_tachyon = 0;
        star->has_subspace = 0;
        star->has_stargate = 0;
        star->has_jumpgate = 0;
        star->has_artemis_net = 0;
        star->has_dimensional_portal = 0;

        for (int16_t i = 0; i < 5; ++i) {
            int16_t p_idx = star->planet_index[i];
            if (p_idx == -1) continue;

            int16_t c_idx = MOX::planet[p_idx].colony_index;
            if (c_idx == -1) continue;

            s_colony* colony = &MOX::colony[c_idx];
            int8_t owner = colony->owner;
            uint8_t owner_bit = (uint8_t)(1 << (owner & 0x1F));

            star->has_colony |= owner_bit;

            if (colony->buildings[BUILDING_WARP_INTERDICTOR] != 0) {
                star->has_warp_field_interdictor |= owner_bit;
            }

            if (colony->buildings[BUILDING_STAR_BASE] != 0 ||
                colony->buildings[BUILDING_BATTLESTATION] != 0 ||
                colony->buildings[BUILDING_STAR_FORTRESS] != 0) {
                if (MOX::player[owner].tech_applications[TECH_APP_TACHYON_COMMUNICATIONS] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    star->has_tachyon |= owner_bit;
                }
                if (MOX::player[owner].tech_applications[TECH_APP_SUB_SPACE_COMMUNICATIONS] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    star->has_subspace |= owner_bit;
                }
            }

            if (colony->buildings[BUILDING_ARTEMIS_SYSTEM_NET] != 0) {
                star->has_artemis_net |= owner_bit;
            }

            if (colony->buildings[BUILDING_DIMENSIONAL_PORTAL] != 0) {
                star->has_dimensional_portal |= owner_bit;
            }

            if (MOX::player[owner].tech_applications[TECH_APP_JUMP_GATE] == TECH_RESEARCH_STATUS_RESEARCHED) {
                star->has_jumpgate |= owner_bit;
            }

            if (MOX::player[owner].tech_applications[TECH_APP_STAR_GATE] == TECH_RESEARCH_STATUS_RESEARCHED) {
                star->has_stargate |= owner_bit;
            }
        }

        if ((star->has_warp_field_interdictor != 0) != (old_interdictor_state != 0)) {
            No_Op1();
        }
    }

    void __cdecl Do_Colony_Owner_Change_Star_Colony_Stuff(s_colony* colony) {
        s_planet_data* planet = &MOX::planet[colony->planet];
        const int16_t star_idx = planet->star_index;
        s_star_data* star = &MOX::star[star_idx];

        COLCALC::Compute_Star_Colony_Stuff(star);

        if (colony->buildings[BUILDING_WARP_INTERDICTOR] != 0) {
            SHIPMOVE::Update_Ship_ETAs(-1);
        }
    }

    void __cdecl Surrender_Star(int16_t star_idx, int16_t player_idx, int16_t to_player_idx) {
        s_star_data* star = &MOX::star[star_idx];

        star->surrender_to[player_idx] = (int8_t)to_player_idx;

        for (int i = 0; i < (int)MOX::NUM_PLAYERS; ++i) {
            if (star->surrender_to[i] == (int8_t)player_idx) {
                star->surrender_to[i] = (int8_t)to_player_idx;
            }
        }

        if (star->surrender_to[to_player_idx] != (int8_t)-1) {
            int8_t indirect_target = star->surrender_to[to_player_idx];
            for (int i = 0; i < (int)MOX::NUM_PLAYERS; ++i) {
                if (star->surrender_to[i] == (int8_t)to_player_idx) {
                    star->surrender_to[i] = indirect_target;
                }
            }
        }
    }

    void __cdecl Surrender_Empire(int16_t player_id, int16_t target_id) {
        MOX::player[player_id].surrender_to = (int8_t)target_id;

        if (MOX::NUM_PLAYERS > 0) {
            for (int i = 0; i < (int)MOX::NUM_PLAYERS; ++i) {
                if (MOX::player[i].surrender_to == (int8_t)player_id) {
                    MOX::player[i].surrender_to = (int8_t)target_id;
                }
            }
        }

        if (MOX::player[target_id].surrender_to != -1) {
            if (MOX::NUM_PLAYERS > 0) {
                for (int i = 0; i < (int)MOX::NUM_PLAYERS; ++i) {
                    if (MOX::player[i].surrender_to == (int8_t)target_id) {
                        MOX::player[i].surrender_to = MOX::player[target_id].surrender_to;
                    }
                }
            }
        }
    }

    void __cdecl Init_Surrenders() {
        for (int16_t i = 0; i < MOX::NUM_STARS; ++i) {
            for (int j = 0; j < (int)MOX::NUM_PLAYERS; ++j) {
                MOX::star[i].surrender_to[j] = -1;
            }
        }

        for (int i = 0; i < (int)MOX::NUM_PLAYERS; ++i) {
            MOX::player[i].surrender_to = -1;
        }
    }

    bool __cdecl May_Surrender_Empire(int16_t player_idx, int16_t target_idx) {
        if (MOX::player[player_idx].surrender_to != (int8_t)target_idx && MOX::player[target_idx].surrender_to == -1) {
            return true;
        }
        return false;
    }

    void __cdecl No_Op1() {
        return;
    }

    int16_t __cdecl Player_N_Turns_Until_Research_Complete(s_player* plr) {
        uint8_t field_idx = (uint8_t)plr->current_research_field;

        if (plr->tech_fields[field_idx] == 3) {
            return 0;
        }

        int64_t produced = plr->research_produced;
        int64_t cost = Player_Research_Cost(plr, field_idx);
        int64_t accumulated = plr->research_accumulated;

        if (produced == 0 && accumulated <= cost) {
            return -1;
        }

        int16_t turns = 0;

        int32_t total_chance = 0;
        do {
            turns++;
            accumulated += produced;
            int32_t current_turn_chance = Chance_For_Research_Breakthrough_Aux(accumulated, produced, cost);
            total_chance += current_turn_chance;
        } while (total_chance < 100);

        return turns;
    }

    int32_t __cdecl Chance_For_Research_Breakthrough(s_player* plr) {
        int64_t cost = Player_Research_Cost(plr, (uint16_t)(uint8_t)plr->current_research_field);

        int64_t produced = plr->research_produced;
        int64_t accumulated_total =
            (int64_t)plr->research_accumulated + produced;

        return Chance_For_Research_Breakthrough_Aux(accumulated_total, produced, cost);
    }

    int32_t __cdecl Chance_For_Research_Breakthrough_Aux(int64_t accumulated_total, int64_t produced, int64_t cost) {
        int64_t chance = 0;

        if (cost > 0 && cost < accumulated_total) {
            chance = ((accumulated_total - cost) * 100) / cost;

            if (chance > 100) {
                return 100;
            }

            if (chance == 0) {
                chance = 1;
            }
        }

        return (int32_t)chance;
    }

    void __cdecl Pre_Import_Computing(s_colony* colony) {
        // Reset maintenance accumulators
        colony->maintenance[ECON_FOOD] = 0;
        colony->maintenance[ECON_INDUSTRY] = 0;
        colony->maintenance[ECON_RESEARCH] = 0;
        colony->maintenance[ECON_BC] = 0;

        colony->food2_per_farmer = (uint8_t)Colony_Food2_Per_Farmer(colony, (int16_t)colony->owner, nullptr);
        Colony_Environmental_Stuff(colony);
        colony->industry_per_worker = (uint8_t)Colony_Industry_Per_Worker(colony, (int16_t)(int8_t)colony->owner, nullptr);
        colony->research_per_scientist = (int8_t)Colony_Research_Per_Scientist(colony, (int16_t)(int8_t)colony->owner, nullptr);

        Colony_Morale(colony, nullptr);
        Colony_Industry_Maintenance(colony);
        Colony_Industry_Production(colony, nullptr);
        Colony_Food_Production(colony, nullptr);
        Colony_Food_Maintenance(colony);
        Colony_Replicators(colony);

        // Calculate industry imports needed to cover maintenance, capped by production capacity
        int16_t maintenance_val = (int16_t)(uint8_t)colony->maintenance[ECON_INDUSTRY];
        colony->imports[ECON_INDUSTRY] = maintenance_val;
        if (colony->production[ECON_INDUSTRY] <= maintenance_val) {
            colony->imports[ECON_INDUSTRY] = colony->production[ECON_INDUSTRY];
        }

        Colony_Research_Production(colony, nullptr);
        Colony_Industry_To_Tax(colony);
        Colony_BC_Production(colony, nullptr);
        Colony_BC_Maintenance(colony);
    }

    void __cdecl Recalculate_Colony(s_colony* colony) {
        Pre_Import_Computing(colony);

        int16_t owner_id = static_cast<int16_t>(colony->owner);
        Pass_Out_Imports(owner_id, colony);
    }

    int __cdecl Player_Research_Cost(s_player* plr, int16_t tech_field) {
        uint32_t total_cost = 0;

        if (tech_field > 0) {
            total_cost = TECHDATA::technology_fields[tech_field].cost;

            if (tech_field >= 75) {
                uint32_t modifier = static_cast<uint32_t>(plr->hyper_advanced_tech[tech_field - 75]);

                total_cost += modifier * hyper_advanced_cost_increment;
            }
        }

        return static_cast<int>(total_cost);
    }

    int16_t __cdecl Colony_Food2_Per_Farmer(const s_colony* colony, int16_t player_idx, s_colony_job_production* out) {
        int16_t food_total = (int16_t)MOX::planet[colony->planet].food_per_farmer;

        if (out != nullptr) {
            out->base_prod = food_total;
        }

        if (MOX::player[player_idx].tech_applications[TECH_APP_BIOMORPHIC_FUNGI] == TECH_RESEARCH_STATUS_RESEARCHED &&
            Biomorphic_Fungi_Applies(food_total)) {
            food_total += food_farmer_bonus.biomorphic_fungi;
            if (out != nullptr) {
                out->tech_apps[TECH_APP_BIOMORPHIC_FUNGI] = food_farmer_bonus.biomorphic_fungi;
            }
        }

        // Apply building bonuses if current base production is above zero
        if (food_total > 0) {
            if (colony->buildings[BUILDING_WEATHER_CONTROLLER] != 0) {
                food_total += food_farmer_bonus.weather_controller;
                if (out != nullptr) {
                    out->buildings_prod[BUILDING_WEATHER_CONTROLLER] = food_farmer_bonus.weather_controller;
                }
            }
            if (colony->buildings[BUILDING_ASTRO_UNIVERSITY] != 0) {
                food_total += food_farmer_bonus.astro_university;
                if (out != nullptr) {
                    out->buildings_prod[BUILDING_ASTRO_UNIVERSITY] = food_farmer_bonus.astro_university;
                }
            }
        }

        return food_total;
    }

    int16_t __cdecl Colony_Industry_Per_Worker(const s_colony* colony, int16_t player_idx, s_colony_job_production* out) {
        int16_t industry;

        int8_t mineral_class = MOX::planet[colony->planet].mineral_class;
        industry = minerals_per_mine[mineral_class];

        if (out != nullptr) {
            out->base_prod = industry;
        }

        if (colony->buildings[BUILDING_ASTRO_UNIVERSITY]) {
            industry += production_worker_bonus.astro_university;
            if (out != nullptr) {
                out->buildings_prod[BUILDING_ASTRO_UNIVERSITY] =
                    production_worker_bonus.astro_university;
            }
        }

        if (colony->buildings[BUILDING_AUTOMATED_FACTORY]) {
            industry += production_worker_bonus.automated_factory;
            if (out != nullptr) {
                out->buildings_prod[BUILDING_AUTOMATED_FACTORY] =
                    production_worker_bonus.automated_factory;
            }
        }

        if (colony->buildings[BUILDING_ROBO_MINER_PLANT]) {
            industry += production_worker_bonus.robo_miner_plant;
            if (out != nullptr) {
                out->buildings_prod[BUILDING_ROBO_MINER_PLANT] =
                    production_worker_bonus.robo_miner_plant;
            }
        }

        if (colony->buildings[BUILDING_DEEP_CORE_MINE]) {
            industry += production_worker_bonus.deep_core_mine;
            if (out != nullptr) {
                out->buildings_prod[BUILDING_DEEP_CORE_MINE] =
                    production_worker_bonus.deep_core_mine;
            }
        }

        if (MOX::player[player_idx].tech_applications[TECH_APP_MICROLITE_CONSTRUCTION] == TECH_RESEARCH_STATUS_RESEARCHED) {
            industry += production_worker_bonus.microlite_construction;
            if (out != nullptr) {
                out->tech_apps[TECH_APP_MICROLITE_CONSTRUCTION] =
                    production_worker_bonus.microlite_construction;
            }
        }

        return (int16_t)industry;
    }

    int16_t __cdecl Colony_Research_Per_Scientist(const s_colony* colony, int16_t player_idx, s_colony_job_production* out) {
        int16_t research_per_scientist = research_scientist_bonus.base_scientist;

        if (out != nullptr) {
            out->base_prod = research_scientist_bonus.base_scientist;
        }

        if (colony->buildings[BUILDING_ASTRO_UNIVERSITY] != 0) {
            research_per_scientist += research_scientist_bonus.astro_university;
            if (out != nullptr) {
                out->buildings_prod[BUILDING_ASTRO_UNIVERSITY] =
                    research_scientist_bonus.astro_university;
            }
        }

        if (colony->buildings[BUILDING_SUPERCOMPUTER] != 0) {
            research_per_scientist += research_scientist_bonus.supercomputer;
            if (out != nullptr) {
                out->buildings_prod[BUILDING_SUPERCOMPUTER] =
                    research_scientist_bonus.supercomputer;
            }
        }

        if (colony->buildings[BUILDING_GALACTIC_CYBERNET] != 0) {
            research_per_scientist += research_scientist_bonus.galactic_cybernet;
            if (out != nullptr) {
                out->buildings_prod[BUILDING_GALACTIC_CYBERNET] =
                    research_scientist_bonus.galactic_cybernet;
            }
        }

        if (colony->buildings[BUILDING_RESEARCH_LAB] != 0) {
            research_per_scientist += research_scientist_bonus.research_lab;
            if (out != nullptr) {
                out->buildings_prod[BUILDING_RESEARCH_LAB] =
                    research_scientist_bonus.research_lab;
            }
        }

        int8_t special = MOX::planet[colony->planet].planet_special;

        if (special == SYSTEM_SPECIAL_ANCIENT_ARTIFACTS) {
            research_per_scientist += research_scientist_bonus.artifacts_planet;
            if (out != nullptr) {
                out->artifacts_bonus += research_scientist_bonus.artifacts_planet;
            }
        } else if (special == SYSTEM_SPECIAL_ORION_STAR) {
            research_per_scientist += research_scientist_bonus.orion_planet;
            if (out != nullptr) {
                out->artifacts_bonus += research_scientist_bonus.orion_planet;
            }
        }

        return research_per_scientist;
    }

    void __cdecl Colony_Environmental_Stuff(s_colony* colony) {
        int8_t climate = MOX::planet[colony->planet].climate;
        colony->climate = climate;

        if (climate == PLANET_CLIMATE_RADIATED) {
            if (colony->buildings[BUILDING_BARRIER_SHIELD] != 0 ||
                colony->buildings[BUILDING_FLUX_SHIELD] != 0 ||
                colony->buildings[BUILDING_RADIATION_SHIELD] != 0) {
                colony->climate = PLANET_CLIMATE_BARREN;
            }
        } else if (climate == PLANET_CLIMATE_TOXIC &&
            terraform_toxic_building != BUILDING_NO_BUILDING &&
            colony->buildings[terraform_toxic_building] != 0) {
            colony->climate = PLANET_CLIMATE_BARREN;
        }

        int16_t food_per_farmer = Colony_Food2_Per_Farmer(colony, colony->owner, nullptr);
        if (food_per_farmer != 0) {
            colony->max_farms = -1;
        } else {
            colony->max_farms = 0;
        }
    }

    int32_t __cdecl Mixed_Race_Morale_Penalty(s_colony* colony) {
        if (Government_Ignores_Mixed_Race_Penalty(colony)) {
            return 0;
        }

        uint8_t n_pops = static_cast<uint8_t>(colony->n_pops);

        if (n_pops > 1 && colony->buildings[BUILDING_ALIEN_CONTROL_CENTER] == 0) {
            uint32_t first_biological_race = 0xFFFFFFFF;

            for (uint32_t i = 0; i < static_cast<uint32_t>(n_pops); ++i) {
                uint32_t race_id = POP::Get_Race(colony->pop[i]);

                if (race_id < MAX_PLAYERS) {
                    if (first_biological_race == 0xFFFFFFFF) {
                        first_biological_race = race_id;
                    } else if (race_id != first_biological_race) {
                        return -4;
                    }
                }
            }
        }

        return 0;
    }

    bool __cdecl Colony_Is_Blockaded(s_colony* colony) {
        int16_t planet_idx = colony->planet;

        int16_t star_idx = MOX::planet[planet_idx].star_index;

        uint8_t blockaded_mask = MOX::star[star_idx].blockaded;

        return (bool)((blockaded_mask >> (colony->owner & 0x1F)) & 1);
    }

    void __cdecl Colony_Specialty(s_colony* colony) {
        int16_t total_production = 0;
        int16_t max_production = 0;
        int16_t best_index = 0;

        for (int16_t i = 0; i < ECON_BC; ++i) {
            int16_t current_val = colony->production[i];
            total_production += current_val;

            if (current_val > max_production) {
                max_production = current_val;
                best_index = i;
            }
        }

        // Logic equivalent to: max_production > floor(total_production / 2)
        if ((total_production / 2) < max_production) {
            colony->specialty = static_cast<int8_t>(best_index);
        } else {
            colony->specialty = -1;
        }
    }

    void __cdecl Colony_Pop_Grows(s_colony* colony) {
        int total_pops = static_cast<uint8_t>(colony->n_pops);
        if (total_pops == 0) {
            for (int i = 0; i < 10; ++i) {
                colony->pop_growth[i] = 0;
                colony->pop_roundoff[i] = 0;
            }
            return;
        }

        int16_t colony_idx = (int16_t)(colony - MOX::colony);
        if (EVENTS::Event_Check_Space_Anomaly(colony_idx)) {
            for (int i = 0; i < 10; ++i) {
                colony->pop_growth[i] = 0;
            }
            return;
        }

        bool pop_boom = static_cast<bool>(EVENTS::Event_Check_Population_Boom(colony_idx));
        bool plague = static_cast<bool>(EVENTS::Event_Check_Plague(colony_idx));

        int food_pool = (colony->imports[ECON_FOOD] + colony->production[ECON_FOOD]) * 2;
        int industry_pool = colony->production[ECON_INDUSTRY] * 2;

        int food_needs_empire = (uint8_t)colony->food2_needed_for_our_empire;
        int food_needs_assimilated = (uint8_t)colony->food2_needed_for_assimilated;
        int food_needs_conquered = (uint8_t)colony->food2_needed_for_conquered;
        int food_needs_natives = (uint8_t)colony->food2_needed_for_natives;
        int ind_needs_empire = (uint8_t)colony->industry2_needed_for_our_empire;
        int ind_needs_androids = (uint8_t)colony->industry2_needed_for_androids;
        int ind_needs_assimilated = (uint8_t)colony->industry2_needed_for_assimilated;
        int ind_needs_conquered = (uint8_t)colony->industry2_needed_for_conquered;

        // Prioritize food needs
        while (food_pool > 0 && food_needs_empire > 0) {
            food_pool--;
            food_needs_empire--;
        }
        while (food_pool > 0 && food_needs_assimilated > 0) {
            food_pool--;
            food_needs_assimilated--;
        }
        while (food_pool > 0 && food_needs_conquered > 0) {
            food_pool--;
            food_needs_conquered--;
        }
        while (food_pool > 0 && food_needs_natives > 0) {
            food_pool--;
            food_needs_natives--;
        }

        // Prioritize industry needs
        while (industry_pool > 0 && ind_needs_empire > 0) {
            industry_pool--;
            ind_needs_empire--;
        }
        while (industry_pool > 1 && ind_needs_androids > 1) {
            industry_pool -= 2;
            ind_needs_androids -= 2;
        }
        while (industry_pool > 0 && ind_needs_assimilated > 0) {
            industry_pool--;
            ind_needs_assimilated--;
        }
        while (industry_pool > 0 && ind_needs_conquered > 0) {
            industry_pool--;
            ind_needs_conquered--;
        }

        for (int i = 0; i < 10; ++i) colony->pop_growth[i] = 0;

        int owner_penalty_sum = ind_needs_empire + food_needs_empire;
        colony->pop_growth[static_cast<uint8_t>(colony->owner)] = (owner_penalty_sum < 1) ? 0 : static_cast<int16_t>(owner_penalty_sum * -25);
        colony->pop_growth[8] = (ind_needs_androids < 1) ? 0 : static_cast<int16_t>(ind_needs_androids * -500);
        colony->pop_growth[9] = (food_needs_natives < 1) ? 0 : static_cast<int16_t>(food_needs_natives * -25);

        int conquered_penalty_sum = food_needs_assimilated + ind_needs_conquered + ind_needs_assimilated + food_needs_conquered;
        int total_lack_denom = static_cast<uint8_t>(colony->industry2_needed_for_conquered) + static_cast<uint8_t>(colony->food2_needed_for_assimilated) + static_cast<uint8_t>(colony->food2_needed_for_conquered) + static_cast<uint8_t>(colony->industry2_needed_for_assimilated);

        if (conquered_penalty_sum > 0 && total_lack_denom > 0) {
            for (int i = 0; i < static_cast<int>(MOX::NUM_PLAYERS); ++i) {
                if (i != static_cast<uint8_t>(colony->owner)) {
                    int race_needs = static_cast<uint8_t>(colony->food2_needed_for_empire[i]) + static_cast<uint8_t>(colony->industry2_needed_for_empire[i]);
                    colony->pop_growth[i] += static_cast<int16_t>((race_needs * conquered_penalty_sum * -25) / total_lack_denom);
                }
            }
        }

        uint8_t race_pop_count[10] = {0};
        uint8_t race_conquered_count[10] = {0};
        for (int i = 0; i < total_pops; ++i) {
            int8_t r = POP::Get_Race(colony->pop[i]);
            if (r < 10) {
                race_pop_count[r]++;
                if (POP::Is_Conquered(colony->pop[i])) {
                    race_conquered_count[r]++;
                }
            }
        }

        for (int16_t i = 0; i < static_cast<int>(MOX::NUM_PLAYERS); ++i) {
            if (race_pop_count[i] == 0) continue;

            int limit = static_cast<int>(Colony_Race_Pop_Limit(colony, i));
            int64_t native_growth = 0;
            if (total_pops < limit) {
                int leader_idx = Colony_Officer(colony);
                int growth_modifier = MOX::player[i].traits[TRAIT_POP_GROWTH_BONUS] +
                    100 + growth_rate_bonus.general_modifier;
                s_player* owner_ptr = &MOX::player[static_cast<uint8_t>(colony->owner)];

                if (owner_ptr->tech_applications[TECH_APP_UNIVERSAL_ANTIDOTE] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    growth_modifier += growth_rate_bonus.antidote_tech;
                } else if (owner_ptr->tech_applications[TECH_APP_MICROBIOTICS] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    growth_modifier += growth_rate_bonus.microbiotics_tech;
                }
                if (cumulative_growth_tech_bonuses != 0 &&
                    owner_ptr->tech_applications[TECH_APP_UNIVERSAL_ANTIDOTE] == TECH_RESEARCH_STATUS_RESEARCHED &&
                    owner_ptr->tech_applications[TECH_APP_MICROBIOTICS] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    growth_modifier += growth_rate_bonus.microbiotics_tech;
                }

                if (leader_idx != -1) {
                    uint32_t skills = MOX::leaders[leader_idx].special_skills;
                    if (skills & LEADER_COLONY_SKILL_MEDICINE2) {
                        int level = OFFICER::Owned_Officer_Level(leader_idx);
                        growth_modifier += (level * 3 + 3) * 5;
                    } else if (skills & LEADER_COLONY_SKILL_MEDICINE) {
                        int level = OFFICER::Owned_Officer_Level(leader_idx);
                        growth_modifier += (level * 5 + 5) * 2;
                    }
                }

                int extra_growth = 0;
                if (colony->buildings[BUILDING_CLONING_CENTER] != 0) {
                    extra_growth =
                        (race_pop_count[i] * cloning_center_growth_bonus) /
                        total_pops;
                }
                if (pop_boom) growth_modifier += growth_rate_bonus.boom_event;
                if (plague) growth_modifier += growth_rate_bonus.plague_event;

                if (colony->producing[0] == COLONY_PRODUCTION_HOUSING) {
                    const int housing_population_divisor =
                        housing_no_popagg != 0 ? 1 : total_pops;
                    growth_modifier +=
                        ((colony->production[ECON_INDUSTRY] - colony->maintenance[ECON_INDUSTRY]) *
                            housing_formula_factor) /
                        housing_population_divisor;
                }

                if (i == static_cast<uint8_t>(colony->owner) || race_conquered_count[i] == 0) {
                    if (owner_ptr->objectives != PLAYER_OBJECTIVE_HUMAN) {
                        growth_modifier += static_cast<int>(MOX::settings.game_difficulty);
                    }
                } else if (growth_modifier > 0 && colony->occupation_policy == 0) {
                    growth_modifier = ((race_pop_count[i] - race_conquered_count[i]) * growth_modifier) / race_pop_count[i];
                }

                const uint64_t base_growth_val_sq =
                    (static_cast<uint64_t>(race_pop_count[i]) *
                        static_cast<uint64_t>(limit - total_pops) *
                        growth_formula_factor) /
                    static_cast<uint64_t>(limit);
                native_growth =
                    (static_cast<int64_t>(
                         Integer_Square_Root(base_growth_val_sq)) *
                        static_cast<int64_t>(growth_modifier)) /
                        100 +
                    extra_growth;
            }
            orion2re::lua::game::s_population_growth_context context;
            context.colony_id = colony_idx;
            context.planet_id = colony->planet;
            context.owner_id = static_cast<uint8_t>(colony->owner);
            context.race_id = i;
            context.race_population = race_pop_count[i];
            context.total_population = total_pops;
            context.population_limit = limit;
            context.climate = static_cast<uint8_t>(colony->climate);
            context.housing =
                colony->producing[0] == COLONY_PRODUCTION_HOUSING;
            context.population_boom = pop_boom;
            context.plague = plague;
            const int64_t growth =
                static_cast<int64_t>(colony->pop_growth[i]) +
                native_growth;
            colony->pop_growth[i] =
                orion2re::lua::game::Modify_Population_Growth(
                    &context, growth);
        }
    }

    void __cdecl Post_Import_Computing(s_colony* colony) {
        COLCALC::Colony_Pop_Grows(colony);
        COLCALC::Colony_Specialty(colony);
    }

    int16_t __cdecl Planet_Max_Population_For_Player(s_planet_data* planet, int16_t player_idx) {
        if (planet->colony_index != -1) {
            s_colony* colony = &MOX::colony[planet->colony_index];

            if (colony->outpost_flag == 0) {
                if (colony->n_pops == 0) {
                    return Colony_Race_Pop_Limit(colony, player_idx);
                }

                uint8_t races_present[10];
                memset(races_present, 0, sizeof(races_present));

                for (int i = 0; i < (int)colony->n_pops; ++i) {
                    races_present[POP::Get_Race(colony->pop[i])] = 1;
                }

                int16_t max_limit = 0;
                for (int race = 0; race < 10; ++race) {
                    if (races_present[race]) {
                        int16_t race_limit = Colony_Race_Pop_Limit(colony, (int16_t)race);
                        if (max_limit < race_limit) {
                            max_limit = race_limit;
                        }
                    }
                }
                return max_limit;
            }
        }

        int16_t pop_limit = Size_And_Climate_Race_Pop_Limit(planet->size, (int)planet->climate, player_idx);

        if (MOX::player[player_idx].tech_applications[TECH_APP_ADVANCED_CITY_PLANNING] == TECH_RESEARCH_STATUS_RESEARCHED) {
            pop_limit += population_bonus.advanced_city_planning;
        }

        if (pop_limit > 42) {
            pop_limit = 42;
        }

        return pop_limit;
    }

    int16_t __cdecl Size_And_Climate_Race_Pop_Limit(int8_t size, int climate, int16_t player_idx) {
        bool is_immune = false;
        const s_player* player = nullptr;

        if (player_idx < MAX_PLAYERS) {
            player = &MOX::player[player_idx];
            if (player->traits[TRAIT_ENVIRONMENT_IMMUNE] != 0) {
                is_immune = true;
            }
        } else if (player_idx == MAX_PLAYERS) {
            // Player index 8 is considered immune (e.g., Neutral/Orion)
            is_immune = true;
        }

        int adjusted_climate = COLCALC::Calc_Player_Climate(player, climate);
        int max_pop = COLCALC::Size_And_Climate_Max_Population(size, adjusted_climate, is_immune);

        if (player_idx < MAX_PLAYERS && MOX::player[player_idx].traits[TRAIT_SUBTERRANEAN] != 0) {
            max_pop += pop_bonus_subterranean[size];
        }

        return static_cast<short>(max_pop);
    }

    void __cdecl All_Colony_Calculations() {
        RUSS::Assert_Settings();

        for (int16_t i = 0; i < MOX::NUM_COLONIES; ++i) {
            s_colony* colony = &MOX::colony[i];
            if (colony->owner != -1 && colony->outpost_flag == 0) {
                Pre_Import_Computing(colony);
            }
        }

        for (int16_t i = 0; i < MOX::NUM_PLAYERS; ++i) {
            Pass_Out_Imports(i, nullptr);
        }

        for (int16_t i = 0; i < MOX::NUM_COLONIES; ++i) {
            s_colony* colony = &MOX::colony[i];
            if (colony->owner != -1 && colony->outpost_flag == 0) {
                Post_Import_Computing(colony);
            }
        }

        for (int16_t i = 0; i < MOX::NUM_PLAYERS; ++i) {
            Update_Player_Stats(&MOX::player[i]);
        }

        for (int16_t i = 0; i < MOX::NUM_PLAYERS; ++i) {
            s_player* player = &MOX::player[i];
            player->total_research = 0;
            for (int32_t j = 0; j < TECH_FIELD_COUNT; ++j) {
                if (player->tech_fields[j] == 3) {
                    player->total_research++;
                }
            }
            player->total_ships = 0;
            player->total_colonies = 0;
        }

        for (int16_t i = 0; i < MOX::NUM_COLONIES; ++i) {
            s_colony* colony = &MOX::colony[i];
            if (colony->owner >= 0 && (uint8_t)colony->n_pops > 4) {
                MOX::player[colony->owner].total_colonies++;
            }
        }

        for (int16_t i = 0; i < MOX::NUM_SHIPS; ++i) {
            s_ship_data* ship = &MOX::ship[i];
            if (ship->owner < MAX_PLAYERS && ship->status < 5 && ship->d.ship_type == SHIP_TYPE_COMBAT) {
                MOX::player[ship->owner].total_ships += (1 << (ship->d.size & 0x1F));
            }
        }
    }

    short __cdecl Colony_Race_Pop_Limit(s_colony* colony, int16_t player_idx) {
        // Calculate base limit from planet size and climate
        int16_t pop_limit = (int)Size_And_Climate_Race_Pop_Limit(
            MOX::planet[colony->planet].size,
            (uint8_t)colony->climate,
            player_idx);

        // Tech bonus: Advanced City Planning
        if (MOX::player[colony->owner].tech_applications[TECH_APP_ADVANCED_CITY_PLANNING] == TECH_RESEARCH_STATUS_RESEARCHED) {
            pop_limit += population_bonus.advanced_city_planning;
        }

        // Building bonus: Biospheres
        if (colony->buildings[BUILDING_BIOSPHERES] != 0) {
            pop_limit += pop_bonus_biospheres;
        }

        if (pop_limit > 42) {
            pop_limit = 42;
        }

        return pop_limit;
    }

    int16_t __cdecl Get_First_Player_Colony_Idx(int32_t player_idx) {
        int16_t index = 0;
        while (MOX::colony[index].owner != player_idx) {
            ++index;
        }
        return index;
    }

    void __cdecl Colony_Industry_Maintenance(s_colony* colony) {
        int16_t colony_idx = (int16_t)(((uintptr_t)colony - (uintptr_t)MOX::colony) / sizeof(s_colony));

        if (EVENTS::Event_Check_Space_Anomaly(colony_idx) != 0) {
            colony->maintenance[ECON_INDUSTRY] = 0;
            return;
        }

        // Initialize industry requirement fields
        colony->industry2_needed_for_our_empire = 0;
        colony->industry2_needed_for_androids = 0;
        colony->industry2_needed_for_assimilated = 0;
        colony->industry2_needed_for_conquered = 0;
        for (int i = 0; i < MAX_PLAYERS; ++i) {
            colony->industry2_needed_for_empire[i] = 0;
        }

        int16_t owner_race_idx = (int8_t)colony->owner;
        int n_pops = (uint8_t)colony->n_pops;

        if (n_pops != 0) {
            for (int i = 0; i < n_pops; ++i) {
                uint32_t pop_data = colony->pop[i];
                int16_t race = (int16_t)POP::Get_Race(pop_data);

                if (race < MAX_PLAYERS) {
                    int8_t cost = (MOX::player[race].traits[TRAIT_CYBERNETIC] != 0) ? 1 : 0;

                    if (race == owner_race_idx) {
                        colony->industry2_needed_for_our_empire += cost;
                    } else if (!POP::Is_Conquered(pop_data)) {
                        colony->industry2_needed_for_assimilated += cost;
                    } else {
                        colony->industry2_needed_for_conquered += cost;
                    }
                    colony->industry2_needed_for_empire[race] += cost;
                } else if (race == 8) {
                    // Androids (race index 8) always contribute 2 to industry maintenance
                    colony->industry2_needed_for_androids += 2;
                }
            }
        }

        // Final colony maintenance is (total_industry_needed + 1) / 2
        int total_needed = (uint8_t)colony->industry2_needed_for_our_empire +
            (uint8_t)colony->industry2_needed_for_androids +
            (uint8_t)colony->industry2_needed_for_assimilated +
            (uint8_t)colony->industry2_needed_for_conquered;

        colony->maintenance[ECON_INDUSTRY] = (int8_t)((total_needed + 1) / 2);
    }

    void __cdecl Colony_Industry_To_Tax(s_colony* colony) {
        int16_t industry_prod = colony->production[ECON_INDUSTRY];
        int32_t taxable_industry = (int32_t)industry_prod - (int32_t)(uint8_t)colony->maintenance[ECON_INDUSTRY];

        if (taxable_industry < 1) {
            colony->n_industry_taxed = 0;
            return;
        }

        uint8_t tax_rate = MOX::player[(int8_t)colony->owner].tax_rate;
        int16_t taxed_amount = (int16_t)(((int32_t)tax_rate * taxable_industry) / 100);

        colony->n_industry_taxed = taxed_amount;
        colony->production[ECON_INDUSTRY] = industry_prod - taxed_amount;
    }

    bool __cdecl Monster_At_Star(int16_t star_idx) {
        s_ship_data* ship_list = MOX::ship;
        int16_t num_ships = MOX::NUM_SHIPS;

        for (int16_t i = 0; i < num_ships; ++i) {
            s_ship_data* current_ship = &ship_list[i];

            // In MoO2, owner indices 8 and above typically represent monsters or guardians
            if (current_ship->owner >= MAX_PLAYERS && current_ship->location == star_idx) {
                return true;
            }
        }

        return false;
    }

    void __cdecl Show_Industry_Production(int16_t colony_idx) {
        char buffer[3000];
        STRBUILD::s_builder builder = STRBUILD::Init(buffer, sizeof(buffer));

        s_colony* colony = &MOX::colony[colony_idx];

        Colony_Industry_Production(colony, &builder);
        TEXTBOX::Text_Box(industry_summary_msg, buffer);
    }

    void __cdecl Show_Research_Production(int16_t colony_idx) {
        char buffer[3000];
        STRBUILD::s_builder builder = STRBUILD::Init(buffer, sizeof(buffer));

        Colony_Research_Production(&MOX::colony[colony_idx], &builder);
        TEXTBOX::Text_Box(research_summary_msg, buffer);
    }

    void __cdecl Colony_Food_Maintenance(s_colony* colony) {
        int16_t colony_idx = (int16_t)(colony - MOX::colony);

        if (EVENTS::Event_Check_Space_Anomaly(colony_idx) != 0) {
            colony->maintenance[ECON_FOOD] = 0;
            return;
        }

        // Initialize food requirement trackers
        for (int i = 0; i < MAX_PLAYERS; ++i) {
            colony->food2_needed_for_empire[i] = 0;
        }
        colony->food2_needed_for_our_empire = 0;
        colony->food2_needed_for_assimilated = 0;
        colony->food2_needed_for_conquered = 0;
        colony->food2_needed_for_natives = 0;

        for (int i = 0; i < (int)(uint8_t)colony->n_pops; ++i) {
            uint32_t pop_bits = colony->pop[i];
            uint32_t race = POP::Get_Race(pop_bits);
            bool conquered = POP::Is_Conquered(pop_bits);

            if (race < MAX_PLAYERS) {
                s_player* player_ptr = &MOX::player[race];
                uint8_t food_rate_half;

                if (player_ptr->traits[TRAIT_CYBERNETIC] != 0) {
                    food_rate_half = 1;
                } else if (player_ptr->traits[TRAIT_EATS_MINERALS] != 0) {
                    food_rate_half = 0;
                } else {
                    food_rate_half = 2;
                }

                if (race == (uint32_t)(uint8_t)colony->owner) {
                    colony->food2_needed_for_our_empire += food_rate_half;
                } else if (!conquered) {
                    colony->food2_needed_for_assimilated += food_rate_half;
                } else {
                    colony->food2_needed_for_conquered += food_rate_half;
                }

                colony->food2_needed_for_empire[race] += food_rate_half;
            } else if (race == 9) {
                // Natives consume fixed amount (2 units half-food = 1.0 food)
                colony->food2_needed_for_natives += 2;
            }
        }

        int32_t sum_food2 = (uint8_t)colony->food2_needed_for_our_empire +
            (uint8_t)colony->food2_needed_for_assimilated +
            (uint8_t)colony->food2_needed_for_conquered +
            (uint8_t)colony->food2_needed_for_natives;

        // Final food maintenance is half the total required half-units, rounded up
        colony->maintenance[ECON_FOOD] = (int8_t)((sum_food2 + 1) / 2);
    }

    uint8_t __cdecl Leader_At_Anomaly(s_leader_data* leader) {
        uint8_t result = 0;

        if (leader->status == 1 && leader->eta == 0 && leader->type == LEADER_TYPE_COLONY) {
            if (EVENTS::Event_Check_Space_Anomaly_At_Star(leader->location) != 0) {
                result = 1;
            }
        }

        return result;
    }

    int32_t __cdecl Tribute_Received_From(int32_t receiver_idx, int32_t payer_idx) {
        s_player* payer = &MOX::player[payer_idx];

        int32_t tribute_percent = static_cast<int32_t>(payer->tribute_treaty[receiver_idx]);

        int32_t net_production = static_cast<int32_t>(payer->bc_produced) - payer->tribute_income;

        int32_t result = (tribute_percent * net_production) / 20;

        if (result < 0) {
            result = 0;
        }

        return result;
    }

    bool __cdecl Player_Owns_Officer(s_player* plr, s_leader_data* leader) {
        if ((plr - MOX::player) == static_cast<intptr_t>(leader->player_index)) {
            if (leader->status == 0 || leader->status == 1) {
                return true;
            }
        }
        return false;
    }

    void __cdecl Colony_BC_Maintenance(s_colony* colony) {
        if (colony->n_pops == 0) {
            colony->maintenance[ECON_BC] = 0;
        } else {
            int16_t colony_idx = (int16_t)(colony - MOX::colony);
            if (EVENTS::Event_Check_Space_Anomaly(colony_idx) != 0) {
                colony->maintenance[ECON_BC] = 0;
            } else {
                int32_t total_building_maintenance = 0;
                for (int32_t i = 0; i < BUILDING_COUNT; ++i) {
                    if (colony->buildings[i] != 0) {
                        total_building_maintenance += TECHDATA::buildings[i].maintenance;
                    }
                }

                int32_t climate_modifier = climate_maintenance_modifiers[(uint8_t)colony->climate] + 100;
                int32_t calculated_bc = (((climate_modifier * total_building_maintenance) * 4 + 200) * 25) / 10000;
                calculated_bc += (uint8_t)colony->n_food_replicated * replicator_food_bc;
                colony->maintenance[ECON_BC] = (uint8_t)calculated_bc;
            }
        }

        colony->imports[ECON_BC] = static_cast<int16_t>(static_cast<uint8_t>(colony->maintenance[ECON_BC])) - colony->production[ECON_BC];
    }

    int __cdecl Size_And_Climate_Max_Population(int8_t size, int climate, uint8_t is_immune) {
        if (population_capacity_table_enabled) {
            return is_immune != 0
                ? population_capacity_table_tolerant[climate][size]
                : population_capacity_table[climate][size];
        }

        // Calculate population factor based on size and climate
        int32_t population_factor =
            (int32_t)is_immune * population_bonus.tolerant +
            population_capacity.climate[climate];

        // Cap the factor at 100%
        if (population_factor > 100) {
            population_factor = 100;
        }

        // Multiply by the configured size capacity and round to nearest integer.
        // Equivalent to: (factor * max_pop + 50) / 100.
        int32_t planet_max_val = population_capacity.size[size];
        int32_t result = (int32_t)(population_factor * planet_max_val + 50) / 100;

        return result;
    }

    void __cdecl Do_Player_Colony_Calculations(int16_t player_idx) {
        RUSS::Mox_Update();

        s_colony* colony = MOX::colony;
        s_colony* end = &MOX::colony[MOX::NUM_COLONIES];

        while (colony < end) {
            if (colony->owner == player_idx && colony->outpost_flag == 0) {
                COLCALC::Pre_Import_Computing(colony);
                RUSS::Mox_Update();
            }

            colony++;
            end = &MOX::colony[MOX::NUM_COLONIES];
        }

        COLCALC::Do_Player_Colony_Post_Production(player_idx);
        RUSS::Mox_Update();
    }

    void __cdecl Do_Player_Colony_Post_Production(int16_t player_idx) {
        Pass_Out_Imports(player_idx, nullptr);

        s_colony* colony = MOX::colony;
        s_colony* end = &MOX::colony[MOX::NUM_COLONIES];

        while (colony < end) {
            if (colony->owner == player_idx && colony->outpost_flag == 0) {
                Post_Import_Computing(colony);
                RUSS::Mox_Update();
            }

            colony++;
            end = &MOX::colony[MOX::NUM_COLONIES];
        }

        Update_Player_Stats(&MOX::player[player_idx]);
    }

    bool __cdecl Player_May_Get_Tech_App(s_player* plr, int tech_app) {
        if (plr->tech_applications[tech_app] == TECH_RESEARCH_STATUS_RESEARCHED) {
            return false;
        }

        switch (tech_app) {
            case TECH_APP_NO_TECH:
                return false;

            case TECH_APP_CONFEDERATION:
                if (plr->traits[TRAIT_CURRENT_GOVERNMENT] != GOVERNMENT_FEUDAL) {
                    return false;
                }
                break;

            case TECH_APP_DIMENSIONAL_PORTAL:
                return (bool)MOX::settings.xenons_exist;

            case TECH_APP_FEDERATION:
                if (plr->traits[TRAIT_CURRENT_GOVERNMENT] != GOVERNMENT_DEMOCRACY) {
                    return false;
                }
                break;

            case TECH_APP_GALACTIC_UNIFICATION:
                if (plr->traits[TRAIT_CURRENT_GOVERNMENT] != GOVERNMENT_UNIFICATION) {
                    return false;
                }
                break;

            case TECH_APP_IMPERIUM:
                if (plr->traits[TRAIT_CURRENT_GOVERNMENT] != GOVERNMENT_DICTATORSHIP) {
                    return false;
                }
                break;

            default:
                if (MOX::settings.strategic_combat_flag != 0) {
                    if (TECHDATA::technology_applications[tech_app].in_strategic == 0) {
                        return false;
                    }
                }

                if (tech_app >= 204) {
                    return false;
                }

                if (TECHDATA::technology_applications[tech_app].tech_field_id == TECH_FIELD_INVALID) {
                    return false;
                }
                break;
        }

        return true;
    }

    int16_t __cdecl Gravity_Player_Production_Factor(const s_colony* colony, int16_t player_idx) {
        if (colony->buildings[BUILDING_GRAVITY_GENERATOR] != 0 || player_idx >= MAX_PLAYERS) {
            return 4;
        }

        int8_t gravity_class = MOX::planet[colony->planet].gravity_class;

        if (gravity_class == PLANET_GRAVITY_LOW_G) { // Low Gravity
            const s_player& player = MOX::player[player_idx];
            return (player.traits[TRAIT_LOW_G_WORLD] == 0) ? 3 : 4;
        }

        if (gravity_class == PLANET_GRAVITY_NORMAL_G) { // Normal Gravity
            const s_player& player = MOX::player[player_idx];
            return (player.traits[TRAIT_LOW_G_WORLD] == 0) ? 4 : 3;
        }

        if (gravity_class == PLANET_GRAVITY_HEAVY_G) { // Heavy Gravity
            const s_player& player = MOX::player[player_idx];
            return (player.traits[TRAIT_HEAVY_G_WORLD] == 0) ? 2 : 4;
        }

        // The original binary falls through with an undefined register value here.
        // Valid game data should only use the 0..2 gravity classes above.
        return 4;
    }

    int16_t __cdecl Colony_Pop_Prod_Produced(const s_colony* colony, const uint32_t* pop_unit, int16_t mode) {
        uint32_t pop_mask = POP::Load(pop_unit);

        // Call the empire-specific base production function
        int16_t base_prod = colony_empire_base_functions[mode](
            colony,
            static_cast<int16_t>(colony->owner),
            (int16_t)POP::Get_Race(pop_mask),
            nullptr);

        // Apply multipliers if base production is positive
        if (base_prod > 0) {
            if (POP::Is_Conquered(pop_mask)) {
                base_prod *= 3;
            } else {
                base_prod *= 4;
            }

            // Apply the production factor (e.g., gravity modifier)
            int16_t gravity_factor = Gravity_Player_Production_Factor(colony, (int16_t)POP::Get_Race(pop_mask));
            base_prod = (int16_t)(gravity_factor * base_prod);
        }

        // If mode is 0, the result is halved
        if (mode == 0) {
            base_prod /= 2;
        }

        return base_prod;
    }

    int16_t __cdecl Colony_Pop_Base_Prod_Produced(const s_colony* colony, const uint32_t* pop, int16_t job_type, s_colony_job_production* out) {
        int16_t base_produced;
        int16_t min_limit = 0;

        base_produced = colony_empire_base_functions[job_type](
            colony,
            static_cast<int16_t>(colony->owner),
            (int16_t)POP::Get_Race(POP::Load(pop)),
            out);

        if (base_produced < 1) {
            if (job_type != 0 || colony->food2_per_farmer != 0) {
                min_limit = 1;
            }
        }

        if (base_produced < min_limit) {
            base_produced = min_limit;
        }

        return base_produced;
    }

    int32_t __cdecl Colony_Producing_Product_Cost(s_colony* colony) {
        return Colony_Product_Cost(colony, colony->producing[0]);
    }

    int32_t __cdecl Colony_Cost_To_Buy_Given_Product(s_colony* colony, int32_t product, int32_t production_spent) {
        switch (product) {
            case COLONY_PRODUCTION_AI_UNASSIGNED:
            case COLONY_PRODUCTION_AI_TRADE_GOODS:
            case COLONY_PRODUCTION_HOUSING:
            case COLONY_PRODUCTION_TRADE_GOODS:
            case COLONY_PRODUCTION_NONE:
                // Certain special production projects (e.g., Trade, Housing) cannot be bought.
                return 0;

            default:
                return Production_Cost_To_Buy(
                    static_cast<int16_t>(Colony_Product_Cost(colony, static_cast<int16_t>(product))),
                    production_spent);
        }
    }

    int32_t __cdecl Ship_Type_Cost_For_Player(int32_t ship_type, int16_t player_idx) {
        int32_t cost = 0;

        if (ship_type == SHIP_TYPE_COLONY) {
            cost = lander_ship_cost.colony_ship;
        } else if (ship_type == SHIP_TYPE_TRANSPORT) {
            cost = lander_ship_cost.transport_ship;
        } else if (ship_type == SHIP_TYPE_OUTPOST) {
            cost = lander_ship_cost.outpost_ship;
        }

        DESIGN::Cost_Reduction_For_Govt_Type(player_idx, &cost);

        return cost;
    }

    bool __cdecl Colony_Can_Afford_To_Buy_Product(int16_t colony_idx) {
        s_colony* colony = &MOX::colony[colony_idx];

        int32_t current_bc = MOX::player[(int)colony->owner].bc;

        int32_t cost = Colony_Cost_To_Buy_Producing_Product(colony);

        return current_bc >= cost;
    }

    int32_t __cdecl Colony_Cost_To_Buy_Producing_Product(s_colony* colony) {
        return COLCALC::Colony_Cost_To_Buy_Given_Product(
            colony,
            static_cast<int32_t>(colony->producing[0]),
            static_cast<int32_t>(colony->production_spent));
    }

    void __cdecl Colony_Buy_Outright(int16_t colony_idx) {
        s_colony* colony = &MOX::colony[colony_idx];
        s_player* player = &MOX::player[colony->owner];

        int32_t buy_cost = Colony_Cost_To_Buy_Producing_Product(colony);
        player->bc -= buy_cost;

        int32_t product_total_cost = Colony_Producing_Product_Cost(colony);
        colony->bought_outright = (int16_t)product_total_cost - colony->production_spent;
    }

    int16_t __cdecl Get_Govt_Type_Tech_App(int16_t govt_type) {
        switch (govt_type) {
            case GOVERNMENT_CONFEDERATION:
                return TECH_APP_CONFEDERATION;
            case GOVERNMENT_IMPERIUM:
                return TECH_APP_IMPERIUM;
            case GOVERNMENT_FEDERATION:
                return TECH_APP_FEDERATION;
            case GOVERNMENT_GALACTIC_UNIFICATION:
                return TECH_APP_GALACTIC_UNIFICATION;
            default:
                return 0;
        }
    }

    int16_t __cdecl Colony_N_Turns_To_Produce(s_colony* colony) {
        int32_t cost = Colony_Product_Cost(colony, colony->producing[0]);
        int32_t spent = (int32_t)colony->production_spent;

        if (spent >= cost) {
            return 1;
        }

        int32_t net_production = (int32_t)colony->production[ECON_INDUSTRY] - static_cast<int32_t>(static_cast<uint8_t>(colony->maintenance[ECON_INDUSTRY]));

        if (net_production <= 0) {
            return -1;
        }

        int32_t remaining_cost = cost - spent;

        int32_t turns = (remaining_cost + net_production - 1) / net_production;

        if (turns <= 0) {
            return 1;
        }

        return (int16_t)turns;
    }

    bool __cdecl Housing_Is_Useless(s_colony* colony) {
        int16_t max_pop = Planet_Max_Population_For_Player(&MOX::planet[colony->planet], colony->owner);

        return max_pop == (uint8_t)colony->n_pops;
    }

    void __cdecl Colony_Calculation(int16_t colony_idx) {
        s_colony* colony = &MOX::colony[colony_idx];

        if (colony->outpost_flag == 0) {
            Recalculate_Colony(colony);
        }

        Update_Player_Stats(&MOX::player[colony->owner]);
    }

    int16_t __cdecl N_Colonies_And_Outposts_At_Star(int16_t star_index) {
        int16_t count = 0;
        for (int i = 0; i < 5; ++i) {
            int16_t planet_idx = MOX::star[star_index].planet_index[i];

            // Check if planet exists in this orbit and if it has a colony/outpost
            if (planet_idx > -1) {
                if (MOX::planet[planet_idx].colony_index > -1) {
                    count++;
                }
            }
        }
        return count;
    }

    void __cdecl Ensure_Uncreative_Field_OK(s_player* player, int field_idx) {
        int selected_tech_id = -1;
        int candidates_count = 0;
        int owned_count = 0;

        s_tech_field_data* field = &TECHDATA::technology_fields[field_idx];
        int16_t* techs = field->tech;

        for (int i = 0; i < 4; ++i) {
            int tech_id = (int)(int16_t)techs[i];

            if (tech_id > 0) {
                if (player->tech_applications[tech_id] == TECH_RESEARCH_STATUS_AVAILABLE) {
                    // Player already owns a tech in this field
                    owned_count++;
                } else if (player->tech_applications[tech_id] == TECH_RESEARCH_STATUS_UNAVAILABLE) {
                    // Candidate selection logic
                    bool is_valid = true;

                    // Tech 0x34 (52) is restricted if xenons (Antarans) do not exist
                    if (MOX::settings.xenons_exist == 0 && tech_id == TECH_APP_DIMENSIONAL_PORTAL) {
                        is_valid = false;
                    }

                    // Special field index 6 (Advanced Science) is excluded
                    if (field_idx == 6) {
                        is_valid = false;
                    }

                    // If strategic combat is enabled, restricted to strategic techs
                    if (is_valid && MOX::settings.strategic_combat_flag != 0) {
                        if (TECHDATA::technology_applications[tech_id].in_strategic == 0) {
                            is_valid = false;
                        }
                    }

                    if (is_valid) {
                        candidates_count++;
                        // Reservoir sampling: pick new candidate with 1/N probability
                        if (game_random::Random(candidates_count) == 1) {
                            selected_tech_id = tech_id;
                        }
                    }
                }
            }
        }

        // If no tech is owned in this field and we found valid candidates, unlock the picked one
        if (owned_count == 0 && candidates_count > 0) {
            player->tech_applications[selected_tech_id] = TECH_RESEARCH_STATUS_AVAILABLE;
        }
    }

    void __cdecl Advance_Player_Field(s_player* player, int field_idx, bool skip_first_advance) {
        if (field_idx == TECH_FIELD_STARTING_TECH) {
            return;
        }

        int loop_count = 0;
        int techs_known = 0;

        uint8_t* player_tech_fields = player->tech_fields;
        uint8_t* player_tech_applications = player->tech_applications;

        do {
            if (!skip_first_advance || loop_count != 0) {
                player_tech_fields[field_idx] = 3;
                field_idx = (int)TECHDATA::technology_fields[field_idx].next_field_id;
                player_tech_fields[field_idx] = 2;
            }

            techs_known = 0;
            int16_t* techs_in_field = TECHDATA::technology_fields[field_idx].tech;
            for (int i = 0; i < 4; ++i) {
                int16_t tech_id = techs_in_field[i];
                if (tech_id > 0 && player_tech_applications[tech_id] == TECH_RESEARCH_STATUS_AVAILABLE) {
                    techs_known++;
                }
            }

            loop_count++;
        } while (techs_known == 0 && field_idx < 75);
    }

    void __cdecl Compute_Potential_Colony_Info(s_player* plr, s_planet_data* planet, s_potential_colony_info* results) {
        results->pop_growth_bonus = 0;
        results->maintenance = 0;
        results->field2_0x4 = 0;
        results->worker_bonus = 0;
        results->food = 0;
        results->industry = 0;
        results->research = 0;

        results->pop_growth_bonus = static_cast<int16_t>(plr->traits[TRAIT_POP_GROWTH_BONUS]);

        results->maintenance =
            static_cast<int16_t>(climate_maintenance_modifiers[planet->climate]);

        if (planet->gravity_class == PLANET_GRAVITY_LOW_G) {
            if (plr->traits[TRAIT_LOW_G_WORLD] == 0) {
                results->worker_bonus = -25;
            }
        } else if (planet->gravity_class == PLANET_GRAVITY_NORMAL_G) {
            if (plr->traits[TRAIT_LOW_G_WORLD] != 0) {
                results->worker_bonus = -25;
            }
        } else if (planet->gravity_class == PLANET_GRAVITY_HEAVY_G) {
            if (plr->traits[TRAIT_HEAVY_G_WORLD] == 0) {
                results->worker_bonus = -50;
            }
        }

        int16_t food_val = static_cast<int16_t>(planet->food_per_farmer);

        if (plr->tech_applications[TECH_APP_BIOMORPHIC_FUNGI] == TECH_RESEARCH_STATUS_RESEARCHED &&
            Biomorphic_Fungi_Applies(food_val)) {
            food_val += food_farmer_bonus.biomorphic_fungi;
        }

        results->food = food_val;

        if (food_val > 0) {
            results->food += static_cast<int16_t>(plr->traits[TRAIT_FOOD_BONUS]);

            int adjusted_climate = Calc_Player_Climate(plr, planet->climate);
            if (adjusted_climate != planet->climate) {
                results->food +=
                    (static_cast<int16_t>(MOX::food_per_farmer_table[adjusted_climate]) -
                        static_cast<int16_t>(MOX::food_per_farmer_table[planet->climate]));
            }

            if (results->food < 1) {
                results->food = 1;
            }
        }

        results->industry = static_cast<int16_t>(minerals_per_mine[planet->mineral_class]);

        if (plr->tech_applications[TECH_APP_MICROLITE_CONSTRUCTION] == TECH_RESEARCH_STATUS_RESEARCHED) {
            results->industry += production_worker_bonus.microlite_construction;
        }

        results->industry += static_cast<int16_t>(plr->traits[TRAIT_PRODUCTION_BONUS]);

        if (results->industry < 1) {
            results->industry = 1;
        }

        results->research = research_scientist_bonus.base_scientist +
            static_cast<int16_t>(plr->traits[TRAIT_RESEARCH_BONUS]);
        if (plr->tech_applications[TECH_APP_HEIGHTENED_INTELLIGENCE] == TECH_RESEARCH_STATUS_RESEARCHED) {
            results->research += research_scientist_bonus.heightened_intelligence;
        }

        if (planet->planet_special == SYSTEM_SPECIAL_ANCIENT_ARTIFACTS) {
            results->research += research_scientist_bonus.artifacts_planet;
        } else if (planet->planet_special == SYSTEM_SPECIAL_ORION_STAR) {
            results->research += research_scientist_bonus.orion_planet;
        }
    }

    int16_t N_Living_Human_Players() {
        int16_t human_count = 0;

        for (int32_t i = 0; i < static_cast<int32_t>(MOX::NUM_PLAYERS); ++i) {
            s_player& player = MOX::player[i];

            if (player.eliminated == 0 && player.objectives == PLAYER_OBJECTIVE_HUMAN) {
                human_count++;
            }
        }

        return human_count;
    }

    int16_t N_Living_Players() {
        int16_t living_count = 0;
        int32_t num_players = (int32_t)MOX::NUM_PLAYERS;

        for (int32_t i = 0; i < num_players; ++i) {
            if (MOX::player[i].eliminated == 0) {
                living_count++;
            }
        }

        return living_count;
    }

    void __cdecl Apply_All_Player_Changes() {
        for (int16_t i = 0; i < MOX::NUM_PLAYERS; ++i) {
            s_player* plr = &MOX::player[i];

            if (plr->eliminated == 0) {
                maintain::Player_Maintenance(plr);

                // Add surplus BC to player treasury
                plr->bc += static_cast<int>(plr->surplus_bc);

                Update_Player_Stats(plr);
                Check_For_Research_Breakthrough(plr);
                Compute_Player_Total_Known_Tech_Cost(plr);

                // Notify if the player is running out of money
                if (plr->surplus_bc < 0 && plr->bc < 50) {
                    s_MSG msg = {};
                    msg.msg_type = MSG_TYPE_FINANCIAL;
                    msg.u.financial.a = (int16_t)plr->bc;
                    msg.u.financial.b = plr->surplus_bc;
                    MSG_::Add_Msg(msg, (int16_t)i);
                }

                // Flag interesting events like food or freighter shortages
                if (plr->surplus_food < 0 || plr->surplus_freighters < 0) {
                    REPORT::something_interesting_happened[i] = 1;
                }
            }
        }
    }

    void __cdecl Do_Surrenders() {
        if (MOX::NUM_STARS > 0) {
            for (int16_t star_idx = 0; star_idx < MOX::NUM_STARS; ++star_idx) {
                if (MOX::NUM_PLAYERS > 0) {
                    for (int16_t player_idx = 0; player_idx < MOX::NUM_PLAYERS; ++player_idx) {
                        int8_t target = MOX::star[star_idx].surrender_to[player_idx];
                        if (target >= 0 && target < MAX_PLAYERS) {
                            Do_Surrender_Star(star_idx, player_idx, target);
                        }
                    }
                }
            }
        }

        if (MOX::NUM_PLAYERS > 0) {
            for (int16_t player_idx = 0; player_idx < (int16_t)MOX::NUM_PLAYERS; ++player_idx) {
                int8_t target = MOX::player[player_idx].surrender_to;
                if (target != -1) {
                    Do_Surrender_Empire(player_idx, target);
                }
            }
        }
    }

    void __cdecl Compute_Blockades() {
        // Step 1: Initialize blockade statuses for all systems
        for (int16_t i = 0; i < MOX::NUM_STARS; ++i) {
            MOX::star[i].blockaded = 0;
            memset(MOX::star[i].blockaded_by, 0, MAX_PLAYERS);
        }

        // Step 2: Calculate blockades based on ship positions
        for (int16_t i = 0; i < MOX::NUM_SHIPS; ++i) {
            s_ship_data* ship = &MOX::ship[i];

            // Only active combat ships inside known star systems can blockade
            if (ship->status == 0 && ship->location < MAX_STARS && ship->d.ship_type == SHIP_TYPE_COMBAT) {
                int16_t star_id = ship->location;
                int8_t ship_owner = ship->owner;
                s_star_data* star = &MOX::star[star_id];

                if (ship_owner < MAX_PLAYERS) {
                    for (int p = 0; p < MOX::NUM_PLAYERS; ++p) {
                        // A blockade occurs if the ship owner is at war with the colony owner
                        if (p != ship_owner && (star->has_colony & (1 << p))) {
                            int8_t treaty = MOX::player[ship_owner].treaty[p];
                            if (treaty >= 4 && treaty <= 6) { // Hostile/War state
                                star->blockaded |= (1 << p);
                                star->blockaded_by[p] |= (1 << ship_owner);
                            }
                        }
                    }
                } else {
                    // Special entities (monsters/Antarans) blockade all colonies in the system
                    star->blockaded = star->has_colony;
                }
            }
        }

        // Step 3: Apply diplomatic penalties for ongoing blockades
        for (int16_t i = 0; i < MOX::NUM_STARS; ++i) {
            s_star_data* star = &MOX::star[i];
            if (star->blockaded != 0) {
                for (int p_target = 0; p_target < MOX::NUM_PLAYERS; ++p_target) {
                    if ((star->blockaded >> p_target) & 1) {
                        for (int p_attacker = 0; p_attacker < MOX::NUM_PLAYERS; ++p_attacker) {
                            // Penalty occurs if p_attacker is blockading p_target,
                            // and p_attacker doesn't own a colony here themselves
                            if (((star->blockaded_by[p_target] >> p_attacker) & 1) &&
                                !((star->has_colony >> p_attacker) & 1)) {
                                int16_t penalty = -static_cast<int16_t>(game_random::Random(5));
                                diplomac::Change_Relations(penalty, p_attacker, p_target, 7 /* RELATION_BLOCKADE */, i, 0);
                            }
                        }
                    }
                }
            }
        }
    }

    /** Finds the highest diplomatic or charisma bonus provided by any owned leader. */
    int __cdecl Best_Leader_Diplomat_Bonus(int player_idx) {
        int max_bonus = 0;
        s_player* plr = &MOX::player[player_idx];

        for (int i = 0; i < MAX_LEADERS; ++i) {
            s_leader_data* leader = &MOX::leaders[i];

            if (Player_Owns_Officer(plr, leader)) {
                int bonus = 0;

                if (leader->general_skills & LEADER_GENERAL_SKILL_DIPLOMAT2) {
                    int16_t level = OFFICER::Owned_Officer_Level(i);
                    bonus = (level * 3 + 3) * 5;
                } else if (leader->general_skills & LEADER_GENERAL_SKILL_DIPLOMAT) {
                    int16_t level = OFFICER::Owned_Officer_Level(i);
                    bonus = (level * 5 + 5) * 2;
                }

                if (bonus > max_bonus) {
                    max_bonus = bonus;
                }
            }
        }

        return max_bonus;
    }

    int16_t __cdecl Get_Player_Max_Population(int16_t player_idx) {
        int16_t total_max_pop = 0;
        s_colony* colony = MOX::colony;

        s_colony* end = &MOX::colony[MOX::NUM_COLONIES];

        while (colony < end) {
            if (colony->owner == player_idx && colony->outpost_flag == 0) {
                total_max_pop += Colony_Race_Pop_Limit(colony, player_idx);
            }
            colony++;

            end = &MOX::colony[MOX::NUM_COLONIES];
        }

        return total_max_pop;
    }

    void __cdecl Do_Surrender_Empire(int16_t surrender_id, int16_t receiver_id) {
        s_player* surrender_plr = &MOX::player[surrender_id];
        s_player* receiver_plr = &MOX::player[receiver_id];

        const uint8_t surrender_bit = (uint8_t)(1u << (uint8_t)surrender_id);
        const uint8_t receiver_bit = (uint8_t)(1u << (uint8_t)receiver_id);
        for (int16_t i = 0; i < MOX::NUM_STARS; ++i) {
            if ((MOX::star[i].visited & surrender_bit) != 0) {
                MOX::star[i].visited |= receiver_bit;
            }
        }

        for (int16_t i = 0; i < MOX::NUM_SHIPS; ++i) {
            s_ship_data* ship = &MOX::ship[i];
            if (ship->owner == (int8_t)surrender_id) {
                ship->owner = (int8_t)receiver_id;
                if (ship->officer_index != -1) {
                    OFFICER::Deassign_Officer(ship->officer_index);
                    ship->officer_index = -1;
                }
            }
        }

        MAINSCR::Update_Ship_Icons_In_Between_Turns();

        for (int i = TECH_APP_ZORTRIUM_ARMOR; i >= 0; --i) {
            if (surrender_plr->tech_applications[i] == TECH_RESEARCH_STATUS_RESEARCHED) {
                if (Player_May_Get_Tech_App(receiver_plr, (int16_t)i)) {
                    Player_Gets_Tech_App(receiver_plr, (int16_t)i);
                }
            }
        }

        for (int16_t i = 0; i < MOX::NUM_COLONIES; ++i) {
            s_colony* colony = &MOX::colony[i];
            if (colony->owner == (int8_t)surrender_id) {
                Do_Surrender_Colony(colony, receiver_id);
                int16_t star_idx = MOX::planet[colony->planet].star_index;
                Compute_Star_Colony_Stuff(&MOX::star[star_idx]);
            }
        }

        int total_spies = BILL::Get_Their_Spy_Number(receiver_id, receiver_id);
        for (int i = 0; i < MOX::NUM_PLAYERS; ++i) {
            total_spies += BILL::Get_Their_Spy_Number(surrender_id, (int16_t)i);
        }

        if (total_spies > 63) {
            total_spies = 63;
        }

        BILL::Set_Their_Spy_Number(receiver_id, receiver_id, (uint8_t)total_spies);

        receiver_plr->n_freighters += surrender_plr->n_freighters;
        receiver_plr->bc += surrender_plr->bc;
        surrender_plr->surrender_to = -1;

        Eliminate_Player(surrender_plr);
    }

    void __cdecl Produce_Ground_Military(s_colony* colony) {
        int16_t infantry_barracks_limit = invasion::Colony_Infantry_Barracks_Limit(colony);
        if (infantry_barracks_limit > colony->military[0]) {
            if (colony->buildings[BUILDING_MARINE_BARRACKS] != 0) {
                ++colony->infantry_roundoff;
                if (colony->infantry_roundoff == 5) {
                    colony->infantry_roundoff = 0;
                    ++colony->military[0];
                }
            }
        } else if (infantry_barracks_limit < colony->military[0] &&
            invasion::Colony_Infantry_Limit(colony) < colony->military[0]) {
            --colony->military[0];
        }

        int tank_barracks_limit = invasion::Colony_Tank_Barracks_Limit(colony);
        if (tank_barracks_limit > colony->military[1]) {
            if (colony->buildings[BUILDING_ARMOR_BARRACKS] != 0) {
                ++colony->tank_roundoff;
                if (colony->tank_roundoff == 5) {
                    colony->tank_roundoff = 0;
                    ++colony->military[1];
                    return;
                }
            }
        } else if (tank_barracks_limit < colony->military[1] &&
            invasion::Colony_Tank_Limit(colony) < colony->military[1]) {
            --colony->military[1];
        }
    }

    void __cdecl Apply_Colony_Changes(s_colony* colony) {
        const int16_t colony_idx = (int16_t)(colony - MOX::colony);

        if (EVENTS::Event_Check_Space_Anomaly(colony_idx) == 0) {
            if (colony->occupation_policy == 4) {
                colony->occupation_policy = 3;
            }
            if (colony->n_turns_existed != -1) {
                colony->n_turns_existed++;
            }
            if (colony->n_pops != 0) {
                Apply_Colony_Pop_Growth(colony);
                Apply_Assimilation(colony);
                Produce_Ground_Military(colony);
                Apply_Production(colony);
            } else {
                Ghost_Colony_Stuff(colony);
            }
        }
    }

    void __cdecl Apply_Colony_Pop_Growth(s_colony* colony) {
        const int16_t colony_idx = (int16_t)(colony - MOX::colony);

        if (EVENTS::Event_Check_Space_Anomaly(colony_idx) != 0) {
            return;
        }

        if (colony->n_pops == 0) {
            return;
        }

        if (colony->occupation_policy == 0) {
            int16_t removable_count = 0;
            uint32_t* selected_pop = nullptr;
            const uint8_t pop_count = static_cast<uint8_t>(colony->n_pops);

            for (int32_t i = 0; i < pop_count; ++i) {
                uint32_t* pop = &colony->pop[i];

                if (COLONY::Colony_Pop_Is_Annihilation_Target(colony, POP::Load(pop))) {
                    ++removable_count;
                    if (game_random::Random(removable_count) == 1) {
                        selected_pop = pop;
                    }
                }
            }

            if (removable_count == 0) {
                colony->occupation_policy = 4;
            } else {
                if (removable_count == 1) {
                    colony->occupation_policy = 4;

                    s_MSG msg = {};
                    msg.msg_type = MSG_TYPE_COLONY;
                    msg.u.colony.planet_idx = colony->planet;
                    msg.u.colony.subtype = MSG_COLONY_SUBTYPE_GENOCIDE;
                    msg.u.colony.c.assimilation.val = static_cast<uint8_t>(POP::Get_Race(POP::Load(selected_pop)));

                    MSG_::Add_Msg(msg, static_cast<int16_t>(static_cast<int8_t>(colony->owner)));
                }

                --colony->n_pops;
                POP::Store(selected_pop, colony->pop[static_cast<uint8_t>(colony->n_pops)]);
            }
        }

        if (colony->n_pops == 0) {
            return;
        }

        const int32_t owner_idx = static_cast<int8_t>(colony->owner);
        s_player* const owner_player = &MOX::player[owner_idx];
        const bool preserve_last_population =
            planet_extinction == 0 || owner_player->objectives != PLAYER_OBJECTIVE_HUMAN;
        int16_t race_counts[10] = {};
        uint8_t protected_race[10] = {};
        bool had_negative_growth = false;

        {
            const uint8_t pop_count = static_cast<uint8_t>(colony->n_pops);
            for (int32_t i = 0; i < pop_count; ++i) {
                ++race_counts[POP::Get_Race(colony->pop[i])];
            }
        }

        if (race_counts[owner_idx] > 0) {
            protected_race[owner_idx] = 1;
        } else {
            int16_t best_race = -1;
            int16_t best_count = 0;

            for (int16_t i = 0; i < MOX::NUM_PLAYERS; ++i) {
                if (best_count < race_counts[i]) {
                    best_race = i;
                    best_count = race_counts[i];
                }
            }

            if (best_race != -1) {
                protected_race[best_race] = 1;
            } else if (race_counts[8] > 0) {
                protected_race[8] = 1;
            } else {
                protected_race[9] = 1;
            }
        }

        for (int32_t race_idx = 0; race_idx < 10; ++race_idx) {
            if (race_idx >= MOX::NUM_PLAYERS && race_idx != 8 && race_idx != 9) {
                continue;
            }

            if (colony->pop_growth[race_idx] >= 0) {
                continue;
            }

            had_negative_growth = true;
            colony->pop_roundoff[race_idx] =
                static_cast<int16_t>(colony->pop_roundoff[race_idx] + colony->pop_growth[race_idx]);

            while (colony->pop_roundoff[race_idx] < 0) {
                const int32_t protected_count = static_cast<int32_t>(protected_race[race_idx]);
                if (protected_count > race_counts[race_idx] ||
                    (preserve_last_population && protected_count == race_counts[race_idx])) {
                    colony->pop_roundoff[race_idx] = 0;
                } else {
                    int16_t selected_special = -1;
                    int16_t selected_normal = -1;
                    int16_t special_count = 0;
                    int16_t normal_count = 0;
                    const uint8_t pop_count = static_cast<uint8_t>(colony->n_pops);

                    for (int16_t pop_idx = 0; pop_idx < static_cast<int16_t>(pop_count); ++pop_idx) {
                        const uint32_t pop = colony->pop[pop_idx];
                        if (POP::Get_Race(pop) != static_cast<uint32_t>(race_idx)) {
                            continue;
                        }

                        if (POP::Is_Farmer(pop)) {
                            ++normal_count;
                            if (game_random::Random(normal_count) == 1) {
                                selected_normal = pop_idx;
                            }
                        } else {
                            ++special_count;
                            if (game_random::Random(special_count) == 1) {
                                selected_special = pop_idx;
                            }
                        }
                    }

                    const int16_t remove_idx = (special_count > 0) ? selected_special : selected_normal;

                    --race_counts[race_idx];
                    --colony->n_pops;
                    colony->pop[remove_idx] = colony->pop[static_cast<uint8_t>(colony->n_pops)];
                    colony->pop_roundoff[race_idx] =
                        static_cast<int16_t>(colony->pop_roundoff[race_idx] + 1000);
                }
            }
        }

        if (had_negative_growth) {
            int16_t food_shortage =
                static_cast<int16_t>(static_cast<uint8_t>(colony->maintenance[0])) -
                colony->production[0];
            food_shortage = static_cast<int16_t>(food_shortage - colony->imports[0]);

            const int16_t industry_shortage =
                static_cast<int16_t>(static_cast<uint8_t>(colony->maintenance[1])) -
                colony->production[1];

            if (food_shortage > 0 || industry_shortage > 0) {
                s_MSG msg = {};
                msg.msg_type = MSG_TYPE_COLONY;
                msg.u.colony.planet_idx = colony->planet;
                msg.u.colony.subtype = MSG_COLONY_SUBTYPE_STARVATION;

                if (food_shortage > 0) {
                    msg.u.colony.c.starvation.a = (int8_t)food_shortage;
                }
                if (industry_shortage > 0) {
                    msg.u.colony.c.starvation.b = (int8_t)industry_shortage;
                }

                MSG_::Add_Msg(msg, static_cast<int16_t>(static_cast<int8_t>(colony->owner)));
            }
        }

        int16_t player_order[10] = {};

        for (int16_t i = 0; i < MOX::NUM_PLAYERS; ++i) {
            player_order[i] = i;
        }

        {
            const int16_t original_first_player = player_order[0];
            player_order[0] = player_order[owner_idx];
            player_order[owner_idx] = original_first_player;
        }

        RUSS::Shuffle_Sint(&player_order[1], static_cast<int16_t>(MOX::NUM_PLAYERS - 1));

        if (MOX::NUM_PLAYERS <= 0) {
            return;
        }

        for (int16_t order_idx = 0; order_idx < MOX::NUM_PLAYERS; ++order_idx) {
            const int16_t race_idx = player_order[order_idx];

            if (colony->pop_growth[race_idx] <= 0) {
                continue;
            }

            colony->pop_roundoff[race_idx] =
                static_cast<int16_t>(colony->pop_roundoff[race_idx] + colony->pop_growth[race_idx]);

            while (colony->pop_roundoff[race_idx] >= 1000) {
                const uint8_t pop_count = static_cast<uint8_t>(colony->n_pops);

                if (COLCALC::Colony_Race_Pop_Limit(colony, race_idx) <= static_cast<int16_t>(pop_count)) {
                    colony->pop_roundoff[race_idx] = 999;
                    break;
                }

                int32_t job_class = -1;

                if ((housing_creates_no_farmers == 0 ||
                        colony->producing[0] != COLONY_PRODUCTION_HOUSING) &&
                    colony->food2_per_farmer > new_farmer_food_threshold) {
                    const int32_t food_balance =
                        static_cast<uint8_t>(colony->food2_needed_for_our_empire) -
                        static_cast<int32_t>(colony->production[0]) * 2 +
                        static_cast<uint8_t>(colony->food2_needed_for_assimilated) +
                        static_cast<uint8_t>(colony->food2_needed_for_conquered) +
                        static_cast<uint8_t>(colony->food2_needed_for_natives);

                    if (food_balance > 0) {
                        job_class = 0;
                    } else if (food_balance == 0) {
                        if (MOX::player[race_idx].traits[TRAIT_EATS_MINERALS] == 0) {
                            job_class = 0;
                        }
                    } else if (food_balance == -1) {
                        if (MOX::player[race_idx].traits[TRAIT_EATS_MINERALS] == 0 &&
                            MOX::player[race_idx].traits[TRAIT_CYBERNETIC] == 0) {
                            job_class = 0;
                        }
                    }

                    if (job_class != 0) {
                        if (owner_player->surplus_food < 0) {
                            job_class = 0;
                        } else if (owner_player->surplus_food == 0 && owner_player->traits[TRAIT_EATS_MINERALS] == 0) {
                            bool found_later_owned_colony = false;

                            for (s_colony* scan = colony + 1;
                                 scan < MOX::colony + MOX::NUM_COLONIES;
                                 ++scan) {
                                if (scan->owner == colony->owner && scan->outpost_flag == 0) {
                                    found_later_owned_colony = true;
                                    break;
                                }
                            }

                            if (found_later_owned_colony) {
                                job_class = 0;
                            }
                        }
                    }
                }

                if (job_class == -1) {
                    const int16_t industry_output = colony->production[1];
                    const int16_t industry_needed = static_cast<uint8_t>(colony->maintenance[1]);

                    if (industry_needed > industry_output) {
                        job_class = 1;
                    } else if (industry_needed == industry_output &&
                        MOX::player[race_idx].traits[TRAIT_CYBERNETIC] != 0) {
                        const int32_t industry_balance =
                            static_cast<uint8_t>(colony->industry2_needed_for_our_empire) +
                            static_cast<uint8_t>(colony->industry2_needed_for_androids) +
                            static_cast<uint8_t>(colony->industry2_needed_for_assimilated) +
                            static_cast<uint8_t>(colony->industry2_needed_for_conquered);

                        if ((industry_balance & 1) == 0) {
                            job_class = 1;
                        }
                    }
                }

                if (job_class == -1 &&
                    EVENTS::Event_Check_Colony_Researching(colony_idx) != 0) {
                    job_class = 2;
                }

                if (job_class == -1) {
                    uint32_t job_counts[3] = {};

                    for (int32_t i = 0; i < pop_count; ++i) {
                        ++job_counts[POP::Get_Prof(colony->pop[i])];
                    }

                    job_class = 1;
                    if (job_counts[1] + job_counts[2] != 0) {
                        job_counts[0] = 0;
                        job_class = RUSS::Get_Weighted_Choice_Int(job_counts, 3);
                    }
                }

                POP::value_type new_pop = 0;
                POP::Set_Prof(&new_pop, static_cast<POP::e_prof>(job_class));
                POP::Set_Assigned(&new_pop, true);
                POP::Set_Race(&new_pop, static_cast<uint32_t>(race_idx));
                POP::Set_Original_Owner(&new_pop, static_cast<uint8_t>(colony->owner));
                colony->pop[pop_count] = new_pop;

                ++colony->n_pops;
                colony->pop_roundoff[race_idx] =
                    static_cast<int16_t>(colony->pop_roundoff[race_idx] - 1000);
            }
        }
    }

    void __cdecl Apply_All_Colony_Changes() {
        s_colony* current = MOX::colony;
        int16_t num_colonies = MOX::NUM_COLONIES;
        if (num_colonies <= 0) {
            return;
        }

        do {
            if (current->owner != -1 && current->outpost_flag == 0) {
                Apply_Colony_Changes(current);
            }

            ++current;
            num_colonies = MOX::NUM_COLONIES;
            if (num_colonies <= 0) {
                break;
            }
        } while (current < &MOX::colony[num_colonies]);
    }

    bool __cdecl May_Surrender_Star(int16_t star_idx, int16_t player_idx, int16_t recipient_idx) {
        if (MOX::player[player_idx].home_planet_id != -1) {
            if (MOX::planet[MOX::player[player_idx].home_planet_id].star_index == star_idx) {
                return false;
            }
        }

        // Check if the system is already set to surrender to this recipient
        if (MOX::star[star_idx].surrender_to[recipient_idx] == (int8_t)player_idx) {
            return false;
        }

        // Check if the player already has a surrender order for this system
        if (MOX::star[star_idx].surrender_to[player_idx] != -1) {
            return false;
        }

        // Recipient must be able to reach the star
        if (!SHIPMOVE::Star_In_Normal_Range_Of_Player(&MOX::star[star_idx], (int16_t)recipient_idx)) {
            return false;
        }

        bool has_other_colony = false;
        // Verify player has colonies in other star systems
        for (int16_t i = 0; i < MOX::NUM_STARS; ++i) {
            // has_colony is a bitmask of players present in the system
            if (((uint8_t)MOX::star[i].has_colony >> (uint8_t)player_idx) & 1) {
                if (&MOX::star[i] != &MOX::star[star_idx]) {
                    has_other_colony = true;
                }
            }
        }

        return has_other_colony;
    }

    void __cdecl Do_Surrender_Colony(s_colony* colony, int16_t new_owner) {
        int8_t old_owner = colony->owner;

        // Check for Capitol building and remove it upon surrender
        if (colony->buildings[BUILDING_CAPITOL] != 0) {
            colony->buildings[BUILDING_CAPITOL] = 0;
        }

        colony->owner = (char)new_owner;
        colony->auto_building = 0;

        // Update ownership of individual population units
        for (int i = 0; i < (int)(uint8_t)colony->n_pops; ++i) {
            uint32_t pop_val = colony->pop[i];
            if (POP::Get_Original_Owner(pop_val) == (uint32_t)old_owner) {
                POP::Set_Original_Owner(&colony->pop[i], (uint32_t)new_owner);
            }
        }

        Do_Colony_Owner_Change_Star_Colony_Stuff(colony);
        Colony_Environmental_Stuff(colony);
        invasion::Enforce_Population_Limits_At_Colony(colony);

        int16_t colony_idx = (int16_t)(colony - MOX::colony);
        COLBLDG::Clear_Colony_Queue(colony_idx);
        COLCALC::Colony_Calculation(colony_idx);

        colony->n_turns_existed = 0;

        // Update the 'visited' bitmask for the star system to reflect the new owner
        int16_t star_idx = MOX::planet[colony->planet].star_index;
        MOX::star[star_idx].visited |= (1 << (uint8_t)new_owner);
    }

    void __cdecl Do_Surrender_Star(int16_t star_idx, int16_t old_owner, int16_t new_owner) {
        s_star_data& star = MOX::star[star_idx];

        for (int i = 0; i < 5; ++i) {
            int16_t planet_idx = star.planet_index[i];
            if (planet_idx != -1) {
                int16_t colony_idx = MOX::planet[planet_idx].colony_index;
                if (colony_idx != -1) {
                    s_colony& colony = MOX::colony[colony_idx];
                    if (colony.owner == (int8_t)old_owner) {
                        COLCALC::Do_Surrender_Colony(&colony, new_owner);
                    }
                }
            }
        }

        // Update ownership bitmasks
        star.has_colony &= ~(1 << (uint8_t)old_owner);
        star.has_colony |= (1 << (uint8_t)new_owner);
        star.visited |= (1 << (uint8_t)new_owner);

        COLCALC::Compute_Star_Colony_Stuff(&star);

        star.colonize_player = -1;

        if (star.officer_index[old_owner] != -1) {
            OFFICER::Move_Officer_To_Pool((int16_t)star.officer_index[old_owner]);
            star.officer_index[old_owner] = -1;
        }

        star.surrender_to[old_owner] = -1;
    }

    void __cdecl Give_Player_Field(s_player* player, int field_id, char get_all_flag) {
        if (field_id < 75 /* HYPER_ADVANCED_THRESHOLD */) {
            Advance_Player_Field(player, field_id, false);

            bool get_all = (player->traits[TRAIT_CREATIVE] != 0) || (get_all_flag != 0);

            // Certain fields automatically grant all technologies.
            if (field_id >= 22 && field_id <= 57) {
                switch (field_id) {
                    case TECH_FIELD_CHEMISTRY:
                    case TECH_FIELD_NUCLEAR_FISSION:
                    case TECH_FIELD_PHYSICS:
                        get_all = true;
                        break;
                    case 23:
                    case 28:
                    case 29:
                        get_all = true;
                        break;
                    default:
                        break;
                }
            }

            if (get_all) {
                // Creative players or forced breakthrough: get all available technologies in the field
                int16_t* techs = TECHDATA::technology_fields[field_id].tech;
                for (int i = 0; i < 4; ++i) {
                    int16_t tech_id = techs[i];
                    if (tech_id > 0 && player->tech_applications[tech_id] == TECH_RESEARCH_STATUS_AVAILABLE) {
                        Player_Gets_Tech_App(player, (int16_t)tech_id);
                    }
                }
            } else {
                // Non-Creative: only get the specific technology they were researching
                uint16_t tech_id = static_cast<uint16_t>(static_cast<uint8_t>(player->current_research_application));
                Player_Gets_Tech_App(player, (int16_t)tech_id);
            }

            if (DESIGN::rebuild_ship_designs_after_field[field_id]) {
                short player_idx = static_cast<short>(player - MOX::player);
                INITSHIP::Init_Ship_Designs(player_idx);
            }
        } else {
            // Hyper-advanced technology increment
            player->hyper_advanced_tech[field_id - 75]++;
        }
    }

    int32_t __cdecl Colony_Product_Cost(s_colony* colony, int16_t product_id) {
        if (COLBLDG::Colony_Production_Is_Building(product_id)) {
            int32_t base_cost = TECHDATA::buildings[product_id].cost;

            switch (product_id) {
                case BUILDING_BARRIER_SHIELD:
                    if (colony->buildings[BUILDING_FLUX_SHIELD]) {
                        return base_cost - TECHDATA::buildings[BUILDING_FLUX_SHIELD].cost;
                    }
                    if (colony->buildings[BUILDING_RADIATION_SHIELD]) {
                        return base_cost - TECHDATA::buildings[BUILDING_RADIATION_SHIELD].cost;
                    }
                    return base_cost;

                case BUILDING_FLUX_SHIELD:
                    if (colony->buildings[BUILDING_RADIATION_SHIELD]) {
                        return base_cost - TECHDATA::buildings[BUILDING_RADIATION_SHIELD].cost;
                    }
                    return base_cost;

                case BUILDING_STAR_FORTRESS:
                    if (colony->buildings[BUILDING_BATTLESTATION]) {
                        return base_cost - TECHDATA::buildings[BUILDING_BATTLESTATION].cost;
                    }
                    if (colony->buildings[BUILDING_STAR_BASE]) {
                        return base_cost - TECHDATA::buildings[BUILDING_STAR_BASE].cost;
                    }
                    return base_cost;

                case BUILDING_BATTLESTATION:
                    if (colony->buildings[BUILDING_STAR_BASE]) {
                        return base_cost - TECHDATA::buildings[BUILDING_STAR_BASE].cost;
                    }
                    return base_cost;

                case BUILDING_TERRAFORMING: {
                    int32_t n_terraforms = MOX::planet[colony->planet].n_terraforms;
                    return base_cost + (n_terraforms * misc_costs.terraforming_step);
                }

                default:
                    return base_cost;
            }
        }

        if (COLBLDG::Colony_Production_Is_Queued_Ship(product_id)) {
            int16_t ship_index = COLBLDG::Colony_Production_Ship_Index(product_id);
            return MOX::ship[ship_index].d.cost;
        }

        if (COLBLDG::Colony_Production_Is_Ship_Design(product_id)) {
            int design_index = COLBLDG::Colony_Production_Ship_Design_Index(product_id);
            return MOX::player[colony->owner].ship_designs[design_index].cost;
        }

        switch (product_id) {
            case COLONY_PRODUCTION_OUTPOST_SHIP:
                return Ship_Type_Cost_For_Player(SHIP_TYPE_OUTPOST, colony->owner);

            case COLONY_PRODUCTION_COLONY_SHIP:
                return Ship_Type_Cost_For_Player(SHIP_TYPE_COLONY, colony->owner);

            case COLONY_PRODUCTION_TRANSPORT_SHIP:
                return Ship_Type_Cost_For_Player(SHIP_TYPE_TRANSPORT, colony->owner);

            case COLONY_PRODUCTION_SPY:
                return misc_costs.spy;

            case COLONY_PRODUCTION_FREIGHTERS: {
                int32_t cost = misc_costs.freighters;
                const int8_t government =
                    MOX::player[colony->owner].traits[TRAIT_CURRENT_GOVERNMENT];
                if (feudal_affects_freighters != 0 &&
                    (government == GOVERNMENT_FEUDAL ||
                        government == GOVERNMENT_CONFEDERATION)) {
                    DESIGN::Cost_Reduction_For_Govt_Type(colony->owner, &cost);
                }
                return cost;
            }

            case COLONY_PRODUCTION_FARMER:
                return misc_costs.android_farmer;

            case COLONY_PRODUCTION_WORKER:
                return misc_costs.android_worker;

            case COLONY_PRODUCTION_SCIENTIST:
                return misc_costs.android_scientist;

            default:
                return 0;
        }
    }

    void __cdecl Compute_Player_Total_Known_Tech_Cost(s_player* player) {
        player->total_known_tech_cost = 0;

        // Process standard technology fields (0 to 74)
        for (int i = 0; i < 75; ++i) {
            s_tech_field_data& field = TECHDATA::technology_fields[i];

            int divisor = 1;
            int numerator = (player->tech_fields[i] == 3) ? 1 : 0;

            int16_t* techs = field.tech;

            for (int j = 0; j < 4; ++j) {
                int16_t tech_id = techs[j];
                if (tech_id > 0) {
                    divisor++;
                    if (player->tech_applications[tech_id] == TECH_RESEARCH_STATUS_RESEARCHED) {
                        numerator++;
                    }
                }
            }

            player->total_known_tech_cost += (int32_t)((uint32_t)field.cost * (uint32_t)numerator) / divisor;
        }

        // Process hyper-advanced technology fields (75 to 82)
        for (int i = 75; i < TECH_FIELD_COUNT; ++i) {
            uint8_t level = player->hyper_advanced_tech[i - 75];
            if (level > 0) {
                const s_tech_field_data& field = TECHDATA::technology_fields[i];
                player->total_known_tech_cost +=
                    field.cost + (level - 1) * hyper_advanced_cost_increment;
            }
        }
    }

    bool __cdecl Colony_Can_Build_Large_Ships(s_colony* colony) {
        if (colony->buildings[BUILDING_STAR_BASE] != 0 ||
            colony->buildings[BUILDING_BATTLESTATION] != 0 ||
            colony->buildings[BUILDING_STAR_FORTRESS] != 0) {
            return true;
        }
        return false;
    }

    void __cdecl Check_For_Eliminated_Players() {
        uint8_t has_colonies[MAX_PLAYERS];
        memset(has_colonies, 0, sizeof(has_colonies));

        // Mark players that currently own at least one colony (excluding outposts)
        for (int16_t i = 0; i < MOX::NUM_COLONIES; ++i) {
            s_colony* colony = &MOX::colony[i];
            if ((uint8_t)colony->owner < MAX_PLAYERS && colony->outpost_flag == 0) {
                has_colonies[(uint8_t)colony->owner] = 1;
            }
        }

        // Check each player to see if they should be eliminated
        for (int i = 0; i < (int)MOX::NUM_PLAYERS; ++i) {
            s_player* player = &MOX::player[i];

            if (player->eliminated == 0) {
                // If the player has no colonies left, eliminate them from the game
                if (has_colonies[i] == 0) {
                    Eliminate_Player(player);
                    EVENTS::Event_Race_Eliminated((int8_t)i);
                }

                // Reset the last attacker info for this turn
                player->last_attacker = -1;
            }
        }
    }

    void __cdecl Delete_Obsolete_Artifical_Planets_Under_Construction(int16_t star_idx) {
        if (!COLCALC::Star_Has_Asteroid_Or_Gas_Giant(star_idx)) {
            for (int32_t planet_slot = 0; planet_slot < 5; ++planet_slot) {
                const int16_t planet_idx = MOX::star[star_idx].planet_index[planet_slot];
                if (planet_idx != -1) {
                    const int16_t colony_idx = MOX::planet[planet_idx].colony_index;
                    if (colony_idx != -1) {
                        COLBLDG::Delete_Prod_From_Colony_Queue(colony_idx, BUILDING_ARTIFICIAL_PLANET);
                    }
                }
            }
        }
    }

    char* __cdecl Format_Quotient(int32_t numerator, int32_t denominator) {
        int32_t val = numerator;

        if (val < 0) {
            val = (int32_t)(0u - (uint32_t)val);
            tmp_quotient[0] = '-';
        } else {
            tmp_quotient[0] = '+';
        }

        int32_t n_chars = snprintf(&tmp_quotient[1], sizeof(tmp_quotient) - 1, "%d", val / denominator);
        char* fraction_ptr = &tmp_quotient[1] + n_chars;

        int32_t remainder = val % denominator;
        if (remainder != 0) {
            int32_t multiplier = 10;
            int32_t count = 1;
            int32_t scaled_remainder = (int32_t)((uint32_t)remainder * (uint32_t)multiplier);

            while (((scaled_remainder / denominator) * denominator) != scaled_remainder) {
                if (count >= 5) {
                    break;
                }

                multiplier = (int32_t)((uint32_t)multiplier * 10u);
                count++;
                scaled_remainder = (int32_t)((uint32_t)remainder * (uint32_t)multiplier);
            }

            if (multiplier != 0 && n_chars >= 0 && static_cast<size_t>(n_chars) < sizeof(tmp_quotient) - 1) {
                size_t remaining_size = sizeof(tmp_quotient) - 1 - static_cast<size_t>(n_chars);
                snprintf(fraction_ptr, remaining_size, ".%d", scaled_remainder / denominator);
            }
        }

        return tmp_quotient;
    }

    short __cdecl N_Prods_In_Colony_Queue(int16_t colony_idx, short item_id) {
        short count = 0;
        for (short i = 0; i < 7; i++) {
            if (MOX::colony[colony_idx].producing[i] == item_id) {
                count++;
            }
        }
        return count;
    }

    bool __cdecl Star_Has_Asteroid_Or_Gas_Giant(int16_t star_index) {
        bool found = false;

        for (int i = 0; i < 5; ++i) {
            int16_t planet_index = MOX::star[star_index].planet_index[i];

            if (planet_index != -1) {
                // s_planet_data size is 17 (0x11). planet_type is at offset 0x4.
                int8_t planet_type = MOX::planet[planet_index].planet_type;

                // Checks for specific planet types (likely 1 = Asteroids, 2 = Gas Giant)
                if (planet_type == PLANET_TYPE_ASTEROID || planet_type == PLANET_TYPE_GAS_GIANT) {
                    found = true;
                }
            }
        }

        return found;
    }

    void __cdecl Eliminate_Player(s_player* player) {
        int16_t player_idx = static_cast<int16_t>(player - MOX::player);

        player->eliminated = 1;

        // Destroy all outposts owned by the eliminated player
        for (int16_t i = 0; i < MOX::NUM_COLONIES; ++i) {
            s_colony* colony = &MOX::colony[i];
            if (colony->owner == player_idx && colony->outpost_flag != 0) {
                BOMB::Destroy_Colony(colony, -1);
            }
        }

        // Release all leaders hired by the eliminated player
        for (int i = 0; i < MAX_LEADERS; ++i) {
            s_leader_data* leader = &MOX::leaders[i];
            if (leader->player_index == player_idx) {
                leader->status = -2 /* STATUS_RELEASED? */;
                leader->eta = 0;
                if (leader->location != -1) {
                    if (leader->type == LEADER_TYPE_SHIP) {
                        MOX::ship[leader->location].officer_index = -1;
                    } else {
                        MOX::star[leader->location].officer_index[player_idx] = -1;
                    }
                    leader->location = -1;
                }
                leader->player_index = -1;
            }
        }

        // Destroy all ships owned by the eliminated player
        for (int16_t i = 0; i < MOX::NUM_SHIPS; ++i) {
            if (MOX::ship[i].owner == player_idx) {
                SHIPS::Kill_Ship(i);
            }
        }

        MAINSCR::Update_Ship_Icons_In_Between_Turns();

        // Clean up diplomatic relations and spies for/against this player
        for (int i = 0; i < (int)MOX::NUM_PLAYERS; ++i) {
            BILL::Set_Their_Spy_Number(player_idx, static_cast<int16_t>(i), 0);
            if (player_idx != i) {
                spy::Bring_Home_Spies_Vs(static_cast<int16_t>(i), (int16_t)player_idx);
            }

            // Clear treaties from others towards the eliminated player
            MOX::player[i].treaty[player_idx] = 0;
            MOX::player[i].trade_treaty[player_idx] = 0;
            MOX::player[i].research_treaty[player_idx] = 0;
            MOX::player[i].tribute_treaty[player_idx] = 0;

            // Clear treaties from the eliminated player towards others
            MOX::player[player_idx].treaty[i] = 0;
            MOX::player[player_idx].trade_treaty[i] = 0;
            MOX::player[player_idx].research_treaty[i] = 0;
            MOX::player[player_idx].tribute_treaty[i] = 0;
        }

        // Reset economic/logistic stats
        player->n_freighters = 0;
        player->bc = 0;
        player->settlers_freighted = 0;

        // Record the date of defeat for the attacker
        if (player->last_attacker >= 0 && player->last_attacker < MAX_PLAYERS) {
            MOX::player[player->last_attacker].player_defeated[player_idx] = static_cast<uint16_t>(MOX::stardate);
        }
    }

    char* __cdecl Player_Name(int32_t player_idx) {
        if (player_idx < MAX_PLAYERS) {
            return MOX::player[player_idx].race_name;
        }

        return MOX::race_names[player_idx + 5];
    }

    int16_t __cdecl Colony_Planetary_Shield(s_colony* colony) {
        if (colony->buildings[BUILDING_BARRIER_SHIELD] != 0) {
            return 3;
        }

        if (colony->buildings[BUILDING_FLUX_SHIELD] != 0) {
            return 2;
        }

        if (colony->buildings[BUILDING_RADIATION_SHIELD] != 0) {
            return 1;
        }

        return 0;
    }

    uint8_t __cdecl Colony_Can_Build_Product(s_colony* colony, int16_t product) {
        uint8_t owner = colony->owner;

        if (COLBLDG::Colony_Production_Is_Building(product)) {
            if (MOX::player[owner].tech_applications[TECHDATA::buildings[product].tech_app] == TECH_RESEARCH_STATUS_RESEARCHED &&
                colony->buildings[product] == 0) {
                switch (product) {
                    case BUILDING_ALIEN_CONTROL_CENTER: {
                        if (colony->n_pops > 0) {
                            int32_t first_race = -1;
                            for (int i = 0; i < colony->n_pops; ++i) {
                                if (POP::Is_Conquered(colony->pop[i])) {
                                    return 1;
                                }
                                uint32_t race = POP::Get_Race(colony->pop[i]);
                                if (race < MAX_PLAYERS) {
                                    if (first_race == -1) {
                                        first_race = race;
                                    } else if (first_race != race) {
                                        uint8_t government =
                                            MOX::player[owner].traits[TRAIT_CURRENT_GOVERNMENT];
                                        if (government / 2 != GOVERNMENT_UNIFICATION / 2 &&
                                            (government != GOVERNMENT_FEDERATION ||
                                                ignore_mixed_race_penalty[GOVERNMENT_FEDERATION] == 0)) {
                                            return 1;
                                        }
                                    }
                                }
                            }
                        }
                        return 2;
                    }
                    case BUILDING_ARTEMIS_SYSTEM_NET:
                    case BUILDING_DIMENSIONAL_PORTAL: {
                        uint8_t retval = 1;
                        int16_t star_index = MOX::planet[colony->planet].star_index;
                        for (int i = 0; i < 5; ++i) {
                            int16_t p = MOX::star[star_index].planet_index[i];
                            if (p != -1) {
                                int16_t c = MOX::planet[p].colony_index;
                                if (c != -1 && MOX::colony[c].owner == owner) {
                                    if (MOX::colony[c].buildings[product] != 0 ||
                                        COLCALC::N_Prods_In_Colony_Queue(c, product) >= 1) {
                                        retval = 2;
                                    }
                                }
                            }
                        }
                        return retval;
                    }
                    case BUILDING_ATMOSPHERE_RENEWER:
                    case BUILDING_POLLUTION_PROCESSOR: {
                        if (colony->buildings[BUILDING_CORE_WASTE_DUMP] != 0) return 0;
                        return 1;
                    }
                    case BUILDING_CAPITOL: {
                        if (MOX::player[owner].traits[TRAIT_CURRENT_GOVERNMENT] / 2 == 3) return 0;
                        int16_t home_planet_id = MOX::player[owner].home_planet_id;
                        if (home_planet_id == -1) return 1;
                        int16_t c = MOX::planet[home_planet_id].colony_index;
                        if (MOX::colony[c].buildings[BUILDING_CAPITOL] != 0) return 0;
                        return 1;
                    }
                    case BUILDING_COLONY_BASE: {
                        bool has_planet_type_3 = false;
                        int16_t star_index = MOX::planet[colony->planet].star_index;
                        for (int i = 0; i < 5; ++i) {
                            int16_t p = MOX::star[star_index].planet_index[i];
                            if (p != -1) {
                                if (MOX::planet[p].planet_type == PLANET_TYPE_PLANET) {
                                    has_planet_type_3 = true;
                                    if (MOX::planet[p].colony_index == -1) {
                                        return 1;
                                    }
                                }
                            }
                        }
                        if (has_planet_type_3) return 2;
                        return 0;
                    }
                    case BUILDING_GAIA_TRANSFORMATION: {
                        if (colony->climate == PLANET_CLIMATE_TERRAN) return 1;
                        return 0;
                    }
                    case BUILDING_CURRENCY_EXCHANGE: {
                        return 0;
                    }
                    case BUILDING_HOLO_SIMULATOR:
                    case BUILDING_PLEASURE_DOME: {
                        if (MOX::player[owner].traits[TRAIT_CURRENT_GOVERNMENT] / 2 == 3) return 0;
                        return 1;
                    }
                    case BUILDING_GRAVITY_GENERATOR: {
                        if (colony->n_pops > 0) {
                            int8_t gravity = MOX::planet[colony->planet].gravity_class;
                            for (int i = 0; i < colony->n_pops; ++i) {
                                uint32_t race = POP::Get_Race(colony->pop[i]);
                                if (race < MAX_PLAYERS) {
                                    if (gravity == PLANET_GRAVITY_LOW_G) {
                                        if (MOX::player[race].traits[TRAIT_LOW_G_WORLD] == 0) return 1;
                                    } else if (gravity == PLANET_GRAVITY_NORMAL_G) {
                                        if (MOX::player[race].traits[TRAIT_LOW_G_WORLD] != 0) return 1;
                                    } else if (gravity == PLANET_GRAVITY_HEAVY_G) {
                                        if (MOX::player[race].traits[TRAIT_HEAVY_G_WORLD] == 0) return 1;
                                    }
                                }
                            }
                        }
                        return 2;
                    }
                    case BUILDING_RADIATION_SHIELD:
                        if (colony->buildings[BUILDING_FLUX_SHIELD] != 0) return 0;
                        // fallthrough
                    case BUILDING_FLUX_SHIELD:
                        if (colony->buildings[BUILDING_BARRIER_SHIELD] != 0) return 0;
                        return 1;
                    case BUILDING_SOIL_ENRICHMENT: {
                        if ((MOX::planet[colony->planet].environmental_alterations & 2) != 0) return 0;
                        if (colony->climate >= PLANET_CLIMATE_DESERT && colony->climate <= PLANET_CLIMATE_GAIA) return 1;
                        return 0;
                    }
                    case BUILDING_STAR_BASE:
                        if (colony->buildings[BUILDING_BATTLESTATION] != 0) return 0;
                        // fallthrough
                    case BUILDING_BATTLESTATION:
                        if (colony->buildings[BUILDING_STAR_FORTRESS] != 0) return 0;
                        return 1;
                    case BUILDING_TERRAFORMING: {
                        if (colony->climate >= PLANET_CLIMATE_BARREN && colony->climate <= PLANET_CLIMATE_ARID) return 1;
                        return 0;
                    }
                    case BUILDING_WEATHER_CONTROLLER: {
                        if (colony->food2_per_farmer == 0) return 2;
                        if (weather_control_climate == PLANET_CLIMATE_RADIATED) {
                            return (colony->climate == PLANET_CLIMATE_RADIATED) ? 2 : 1;
                        }
                        return (uint8_t)colony->climate < weather_control_climate ? 2 : 1;
                    }
                    case BUILDING_ARTIFICIAL_PLANET: {
                        return COLCALC::Star_Has_Asteroid_Or_Gas_Giant(MOX::planet[colony->planet].star_index);
                    }
                    default:
                        return 1;
                }
            }
        } else {
            if (COLBLDG::Colony_Production_Is_Ship(product)) {
                if (MOX::player[owner].ship_range == 0 || MOX::player[owner].ship_speed == 0) return 0;

                s_ship_design* ship_design;
                if (COLBLDG::Colony_Production_Is_Queued_Ship(product)) {
                    ship_design = &MOX::ship[COLBLDG::Colony_Production_Ship_Index(product)].d;
                } else {
                    ship_design = &MOX::player[owner].ship_designs[COLBLDG::Colony_Production_Ship_Design_Index(product)];
                }

                switch (ship_design->ship_type) {
                    case SHIP_TYPE_COMBAT:
                        if (ship_design->size < minimum_orbital_base_ship_class) return 1;
                        if (COLCALC::Colony_Can_Build_Large_Ships(colony)) {
                            if (ship_design->size == 4) {
                                if (MOX::player[owner].tech_applications[TECH_APP_TITAN_CONSTRUCTION] == TECH_RESEARCH_STATUS_RESEARCHED) return 1;
                                return 0;
                            }
                            if (ship_design->size == 5) {
                                if (MOX::player[owner].tech_applications[TECH_APP_DOOM_STAR_CONSTRUCTION] == TECH_RESEARCH_STATUS_RESEARCHED) return 1;
                                return 0;
                            }
                            return 1;
                        }
                        return 0;
                    case SHIP_TYPE_COLONY:
                        if (MOX::player[owner].tech_applications[TECH_APP_COLONY_SHIP] == TECH_RESEARCH_STATUS_RESEARCHED) return 1;
                        return 0;
                    case SHIP_TYPE_TRANSPORT:
                        if (colony->buildings[BUILDING_MARINE_BARRACKS] != 0 && MOX::player[owner].tech_applications[TECH_APP_TRANSPORT] == TECH_RESEARCH_STATUS_RESEARCHED) return 1;
                        return 0;
                    case SHIP_TYPE_OUTPOST:
                        if (MOX::player[owner].tech_applications[TECH_APP_OUTPOST_SHIP] == TECH_RESEARCH_STATUS_RESEARCHED) return 1;
                        return 0;
                    default:
                        return 0;
                }
            } else {
                switch (product) {
                    case COLONY_PRODUCTION_AI_UNASSIGNED:
                    case COLONY_PRODUCTION_AI_TRADE_GOODS:
                    case COLONY_PRODUCTION_TRADE_GOODS:
                    case COLONY_PRODUCTION_NONE:
                        return 1;
                    case COLONY_PRODUCTION_SPY:
                        return MOX::player[owner].tech_applications[TECH_APP_SPY_NETWORK] ==
                            TECH_RESEARCH_STATUS_RESEARCHED;
                    case COLONY_PRODUCTION_FREIGHTERS:
                        if (MOX::player[owner].tech_applications[TECH_APP_FREIGHTERS] == TECH_RESEARCH_STATUS_RESEARCHED) return 1;
                        return 0;
                    case COLONY_PRODUCTION_SCIENTIST:
                        if (androids_available != 0 ||
                            MOX::player[owner].tech_applications[TECH_APP_ANDROID_SCIENTISTS] == TECH_RESEARCH_STATUS_RESEARCHED) {
                            return (COLCALC::Room_For_Another_Dude(colony, 8) == 1) ? 1 : 2;
                        }
                        return 0;
                    case COLONY_PRODUCTION_WORKER:
                        if (androids_available != 0 ||
                            MOX::player[owner].tech_applications[TECH_APP_ANDROID_WORKERS] == TECH_RESEARCH_STATUS_RESEARCHED) {
                            return (COLCALC::Room_For_Another_Dude(colony, 8) == 1) ? 1 : 2;
                        }
                        return 0;
                    case COLONY_PRODUCTION_FARMER:
                        if (androids_available != 0 ||
                            MOX::player[owner].tech_applications[TECH_APP_ANDROID_FARMERS] == TECH_RESEARCH_STATUS_RESEARCHED) {
                            return (COLCALC::Room_For_Another_Dude(colony, 8) == 1) ? 1 : 2;
                        }
                        return 0;
                    case COLONY_PRODUCTION_HOUSING:
                        return disable_housing == 0;
                    default:
                        return 0;
                }
            }
        }
        return 0;
    }

    /**
     * @brief Identifies systems owned by a player that are eligible for surrender.
     * @param player_idx Index of the player potentially surrendering systems.
     * @param target_player_idx Index of the player the systems would surrender to.
     * @param star_list Output array to store indices of eligible stars.
     * @return Number of eligible systems found.
     */
    int16_t __cdecl Get_Possible_Surrender_Systems(int16_t player_idx, int16_t target_player_idx, int16_t* star_list) {
        int16_t result_count = 0;
        int16_t range = MOX::player[target_player_idx].ship_range;
        int16_t colony_system_count = 0;

        for (int16_t i = 0; i < MOX::NUM_STARS; ++i) {
            s_star_data* star = &MOX::star[i];

            // Check if player has any colony in this system via bitmask
            if ((star->has_colony >> (player_idx & 0x1f)) & 1) {
                colony_system_count++;

                if (SHIPMOVE::Star_In_Range_Of_Player(star, target_player_idx, range)) {
                    bool has_capitol = false;

                    // Check if any colony owned by the surrendering player is their capitol.
                    for (int p = 4; p >= 0; --p) {
                        int16_t planet_idx = star->planet_index[p];
                        if (planet_idx != -1) {
                            int16_t colony_idx = MOX::planet[planet_idx].colony_index;
                            if (colony_idx != -1) {
                                s_colony* colony = &MOX::colony[colony_idx];
                                if (colony->owner == (char)player_idx && colony->buildings[BUILDING_CAPITOL] != 0) {
                                    has_capitol = true;
                                    break;
                                }
                            }
                        }
                    }

                    // A player's capitol system is not eligible for surrender.
                    if (!has_capitol) {
                        if (star_list != nullptr) {
                            star_list[result_count] = i;
                        }
                        result_count++;
                    }
                }
            }
        }

        // A player will not surrender if they only have one system remaining
        if (colony_system_count == 1) {
            result_count = 0;
        }

        return result_count;
    }

    int16_t __cdecl Get_Surrender_System(int16_t player_idx, int16_t target_player_idx, int16_t selection_mode) {
        int16_t possible_stars[MAX_STARS];
        s_star_weight star_weights[MAX_STARS];
        int16_t result_star_idx = -1;

        // Populate potential stars to surrender
        int16_t num_stars = Get_Possible_Surrender_Systems(player_idx, target_player_idx, possible_stars);

        if (num_stars != 0) {
            for (int i = 0; i < (int)num_stars; ++i) {
                int16_t star_idx = possible_stars[i];
                star_weights[i].star_idx = star_idx;
                star_weights[i].weight = 0;

                // Calculate weight based on total population of colonies owned by the player in this system
                for (int p = 0; p < 5; ++p) {
                    int16_t planet_idx = MOX::star[star_idx].planet_index[p];
                    if (planet_idx != -1) {
                        int16_t colony_idx = MOX::planet[planet_idx].colony_index;
                        if (colony_idx != -1 && MOX::colony[colony_idx].owner == player_idx) {
                            star_weights[i].weight += MOX::colony[colony_idx].n_pops;
                        }
                    }
                }
            }

            // Sort systems by population weight
            qsort(star_weights, (size_t)num_stars, sizeof(s_star_weight), Sort_star_weights);

            int half_count = (num_stars + 1) / 2;
            int chosen_idx;

            if (selection_mode == 0) {
                // Pick from the "bottom" (lower weight) half
                chosen_idx = game_random::Random(half_count) - 1;
            } else {
                // Pick from the "top" (higher weight) half
                chosen_idx = num_stars - game_random::Random(half_count);
            }

            result_star_idx = star_weights[chosen_idx].star_idx;
        }

        return result_star_idx;
    }

    int32_t __cdecl Colony_N_Pops_Of_Empire(s_colony* colony, uint32_t empire_id) {
        int32_t pop_count = 0;
        uint32_t* pop = colony->pop;
        uint32_t* pop_end = colony->pop + static_cast<uint8_t>(colony->n_pops);

        while (pop < pop_end) {
            if (POP::Get_Race(POP::Load(pop)) == empire_id) {
                ++pop_count;
            }
            ++pop;
        }

        return pop_count;
    }

    bool __cdecl Room_For_Another_Dude(s_colony* colony, int race_idx) {
        return SETTLER::Room_For_Another(colony, (int16_t)race_idx, 0);
    }

    void __cdecl Compute_Total_Building_Costs(int32_t* player_costs) {
        memset(player_costs, 0, MAX_PLAYERS * sizeof(int32_t));

        for (int16_t i = 0; i < MOX::NUM_COLONIES; ++i) {
            s_colony* colony = &MOX::colony[i];

            if (colony->owner != -1 && colony->outpost_flag == 0) {
                for (int b = 0; b < BUILDING_COUNT; ++b) {
                    if (colony->buildings[b] != 0) {
                        player_costs[colony->owner] += TECHDATA::buildings[b].cost;
                    }
                }
            }
        }
    }

    int32_t __cdecl Sort_star_weights(const void* a, const void* b) {
        auto lhs = static_cast<const s_star_weight*>(a);
        auto rhs = static_cast<const s_star_weight*>(b);
        return static_cast<int32_t>(lhs->weight) - static_cast<int32_t>(rhs->weight);
    }

    char* __cdecl Load_Colcalc_Strings(char* strings) {
        char* cursor = strings;

        COLCALC::anomaly_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::no_barracks_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::unification_has_no_morale_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::government_morale_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::no_capital_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::mixed_races_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::bonus_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::leader_bonus_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::total_morale_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::morale_summary_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::produced_by_farmers_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::aquatic_bonus_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::native_bonus_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::android_bonus_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::base_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::gravity_penalty_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::conquered_penalty_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::blockaded_penalty_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::morale_bonus_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::morale_penalty_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::government_bonus_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::government_penalty_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::total_food_produced_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::food_summary_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::produced_by_workers_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::used_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::pollution_penalty_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::replicated_to_d_food_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::taxed_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::total_industry_produced_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::industry_summary_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::produced_by_scientists_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::artifacts_bonus_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::intelligence_bonus_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::total_research_produced_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::research_summary_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::taxes_collected_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::special_income_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::trade_goods_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::taxed_from_industry_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::total_income_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::bc_summary_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::starting_command_points_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::d_from_d_star_bases_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::d_from_1_star_base_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::d_from_d_battlestations_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::d_from_1_battlestation_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::d_from_d_fortresses_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::d_from_1_fortress_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::from_warlord_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::from_imperium_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::total_command_points_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::d_from_1_s_ship_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::d_from_d_s_ships_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::d_from_1_noncombat_ship_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::d_from_d_noncombat_ships_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::total_command_point_used_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::total_command_points_used_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::ship_maintenance_costs_msg = cursor;
        cursor += strlen(cursor) + 1;

        COLCALC::command_summary_msg = cursor;
        return cursor + strlen(cursor) + 1;
    }

    void __cdecl Apply_Assimilation(s_colony* colony) {
        const int16_t colony_index = (int16_t)(colony - MOX::colony);
        if (EVENTS::Event_Check_Space_Anomaly(colony_index) != 0) {
            return;
        }

        if (colony->occupation_policy != 3) {
            return;
        }

        const int32_t colony_owner = static_cast<int8_t>(colony->owner);
        const uint8_t pop_count = static_cast<uint8_t>(colony->n_pops);

        uint32_t occupation_points = static_cast<uint8_t>(colony->occupation_points);
        uint32_t assimilation_gain =
            static_cast<uint8_t>(
                COLCALC::assimilation_rate_by_government[static_cast<int8_t>(MOX::player[colony_owner].traits[TRAIT_CURRENT_GOVERNMENT])]);

        for (uint32_t* pop_record_ptr = colony->pop;
             pop_record_ptr < colony->pop + pop_count;
             ++pop_record_ptr) {
            uint32_t pop_record = POP::Load(pop_record_ptr);
            const uint32_t pop_owner = POP::Get_Original_Owner(pop_record);

            if (MOX::player[pop_owner].traits[TRAIT_CURRENT_GOVERNMENT] == GOVERNMENT_FEUDAL) {
                POP::Set_Original_Owner(pop_record_ptr, static_cast<uint8_t>(colony_owner));
            }
        }

        if (colony->buildings[BUILDING_ALIEN_CONTROL_CENTER] != 0) {
            assimilation_gain = 0x78;
        }

        if (MOX::player[colony_owner].traits[TRAIT_CHARISMATIC] != 0) {
            assimilation_gain *= 2;
        } else if (MOX::player[colony_owner].traits[TRAIT_REPULSIVE] != 0) {
            assimilation_gain /= 2;
        }

        occupation_points += assimilation_gain;

        if (occupation_points >= 0xF0) {
            uint32_t conversions_remaining = occupation_points / 0xF0;
            occupation_points %= 0xF0;

            do {
                int16_t candidate_count = 0;
                int16_t chosen_pop_idx = -1;

                for (int16_t pop_idx = 0; pop_idx < pop_count; ++pop_idx) {
                    const uint32_t pop_record = colony->pop[pop_idx];
                    if (POP::Is_Conquered(pop_record) && POP::Get_Race(pop_record) < MAX_PLAYERS) {
                        ++candidate_count;
                        if (game_random::Random(candidate_count) == 1) {
                            chosen_pop_idx = pop_idx;
                        }
                    }
                }

                if (candidate_count != 0) {
                    if (candidate_count == 1) {
                        colony->occupation_policy = 4;

                        s_MSG msg = {};
                        msg.msg_type = MSG_TYPE_COLONY;
                        msg.u.colony.planet_idx = colony->planet;
                        msg.u.colony.subtype = MSG_COLONY_SUBTYPE_ASSIMILATION;
                        msg.u.colony.c.assimilation.val =
                            static_cast<uint8_t>(POP::Get_Race(colony->pop[chosen_pop_idx]));

                        MSG_::Add_Msg(msg, static_cast<int16_t>(colony_owner));
                    }

                    POP::value_type pop_record = colony->pop[chosen_pop_idx];
                    POP::Set_Conquered(&pop_record, false);
                    POP::Set_Original_Owner(&pop_record, static_cast<uint8_t>(colony_owner));
                    colony->pop[chosen_pop_idx] = pop_record;
                }

                --conversions_remaining;
            } while (conversions_remaining != 0);
        }

        colony->occupation_points = static_cast<char>(occupation_points);
    }

    void __cdecl Apply_Production(s_colony* colony) {
        const int32_t owner_player_idx = static_cast<int32_t>(colony->owner);
        const game_world::s_view world{.colonies = MOX::colony, .colony_count = &MOX::NUM_COLONIES};
        const auto colony_id = legacy_world::Colony_Id(&world, colony);
        if (!colony_id) return;
        const int16_t colony_idx = (int16_t)*colony_id;
        s_player& player = MOX::player[owner_player_idx];

        if (EVENTS::Event_Check_Space_Anomaly(colony_idx) != 0) {
            return;
        }

        if (colony->producing[0] == COLONY_PRODUCTION_TRADE_GOODS ||
            colony->producing[0] == COLONY_PRODUCTION_AI_TRADE_GOODS ||
            colony->producing[0] == COLONY_PRODUCTION_NONE) {
            // Income for this turn was calculated before legacy autobuild selects a new item.
            Prepare_Production_Item(*colony_id, colony);
            return;
        }

        int32_t production_pool = (int32_t)colony->production_spent;
        const int32_t current_net_production = std::max(
            (int32_t)colony->production[ECON_INDUSTRY] -
                (int32_t)(uint8_t)colony->maintenance[ECON_INDUSTRY],
            0);
        bool added_turn_production = false;
        bool completed_product = false;
        for (;;) {
            bool removed_existing_pop = false;
            const int16_t current_product = Prepare_Production_Item(*colony_id, colony);

            if (current_product == COLONY_PRODUCTION_HOUSING) {
                if (disable_housing != 0) {
                    colony->producing[0] = COLONY_PRODUCTION_NONE;
                    COLONY::Fix_Colony_Queue(colony_idx);
                    continue;
                }
                if (!COLCALC::Housing_Is_Useless(colony)) {
                    break;
                }

                colony->just_produced = current_product;
                colony->producing[0] = COLONY_PRODUCTION_NONE;
                COLONY::Fix_Colony_Queue(colony_idx);
                break;
            }

            if (current_product == COLONY_PRODUCTION_TRADE_GOODS ||
                current_product == COLONY_PRODUCTION_AI_TRADE_GOODS ||
                current_product == COLONY_PRODUCTION_NONE) {
                break;
            }

            if (completed_product && multibuild_nonbuilding_products_only != 0 &&
                !Is_Multibuild_Nonbuilding_Product(current_product)) {
                break;
            }

            if (multibuild == 0) {
                production_pool += current_net_production;
            } else if (!added_turn_production) {
                production_pool += current_net_production;
                added_turn_production = true;
            }

            production_pool += static_cast<int32_t>(colony->bought_outright);
            colony->bought_outright = 0;

            const int32_t product_cost =
                static_cast<int32_t>(COLCALC::Colony_Producing_Product_Cost(colony));

            if (production_pool >= product_cost) {
                s_MSG build_msg{};
                build_msg.msg_type = MSG_TYPE_COLONY;
                build_msg.u.colony.planet_idx = colony->planet;
                build_msg.u.colony.subtype = MSG_COLONY_SUBTYPE_PRODUCTION_COMPLETED;
                build_msg.u.colony.c.build.build_id = current_product;

                if (COLBLDG::Colony_Production_Is_Queued_Ship(current_product)) {
                    const int16_t ship_idx = COLBLDG::Colony_Production_Ship_Index(current_product);
                    s_ship_data* ship = &MOX::ship[ship_idx];

                    if (ship->status == 4) {
                        build_msg.u.colony.c.build.is_refit = 1;
                    } else {
                        const int8_t base_crew_quality =
                            colony->buildings[BUILDING_SPACE_ACADEMY] != 0
                            ? CREW_QUALITY_REGULAR
                            : CREW_QUALITY_GREEN;
                        ship->crew_quality = base_crew_quality;
                        ship->crew_experience =
                            MOX::crew_data[base_crew_quality].experience;

                        if (player.traits[TRAIT_WARLORD] != 0) {
                            ++ship->crew_quality;
                        }
                    }
                }

                if (colony->producing[1] == COLONY_PRODUCTION_NONE) {
                    colony->just_produced = colony->producing[0];
                }

                if (COLBLDG::Colony_Production_Is_Building(current_product)) {
                    ERICNET::Add_Building(colony_idx, current_product);

                    if (colony->producing[1] == COLONY_PRODUCTION_REPEAT) {
                        COLCALC::Colony_Calculation(colony_idx);
                        if (!COLCALC::Colony_Can_Build_Product(colony, current_product)) {
                            colony->producing[0] = COLONY_PRODUCTION_NONE;
                        }
                    } else {
                        colony->producing[0] = COLONY_PRODUCTION_NONE;
                    }

                    if (current_product == BUILDING_TERRAFORMING) {
                        if (!COLCALC::Colony_Can_Build_Product(colony, BUILDING_TERRAFORMING)) {
                            COLBLDG::Delete_Prod_From_Colony_Queue(colony_idx, BUILDING_TERRAFORMING);
                        }
                    } else if (current_product == BUILDING_ARTIFICIAL_PLANET) {
                        const int16_t planet_idx = colony->planet;
                        const int16_t star_idx = MOX::planet[planet_idx].star_index;
                        COLCALC::Delete_Obsolete_Artifical_Planets_Under_Construction(star_idx);
                    }
                } else {
                    if (COLBLDG::Colony_Production_Is_Queued_Ship(current_product)) {
                        const int16_t ship_idx = COLBLDG::Colony_Production_Ship_Index(current_product);
                        s_ship_data* ship = &MOX::ship[ship_idx];

                        if (ship->status == 4) {
                            ship->status = 0;
                            ship->d.cost = DESIGN::Cost_Of_Design(static_cast<int16_t>(owner_player_idx), &ship->d);
                            colony->producing[0] = COLONY_PRODUCTION_NONE;
                        } else if (ship->status == 6) {
                            ship->status = 0;

                            if (colony->producing[1] == COLONY_PRODUCTION_REPEAT) {
                                const int16_t created_ship_idx =
                                    COLBLDG::Create_Ship(static_cast<int16_t>(owner_player_idx), 0, colony_idx);

                                if (created_ship_idx != -1) {
                                    memcpy(&MOX::ship[created_ship_idx].d, &ship->d, sizeof(s_ship_design));
                                    MOX::ship[created_ship_idx].status = 6;
                                    colony->producing[0] = COLBLDG::Colony_Production_From_Ship_Index(created_ship_idx);
                                } else {
                                    colony->producing[0] = COLONY_PRODUCTION_NONE;
                                }
                            } else {
                                colony->producing[0] = COLONY_PRODUCTION_NONE;
                            }
                        }

                        ship->just_built = 1;
                        MOX::star[ship->location].ignore_combat_ships = 0;

                        if (ship->d.ship_type == SHIP_TYPE_COLONY || ship->d.ship_type == SHIP_TYPE_OUTPOST) {
                            MOX::star[ship->location].ignore_colony_ships |=
                                static_cast<uint8_t>(1u << static_cast<uint8_t>(ship->owner));
                        }
                    } else {
                        switch (current_product) {
                            case COLONY_PRODUCTION_FREIGHTERS:
                                player.n_freighters = static_cast<int16_t>(player.n_freighters + 5);
                                player.bc += freighters_cash_bonus;
                                break;

                            case COLONY_PRODUCTION_SPY:
                                if (BILL::Add_Their_Spy(static_cast<int16_t>(owner_player_idx), static_cast<int16_t>(owner_player_idx)) == 0) {
                                    player.bc += product_cost / 2;
                                    COLBLDG::Delete_Prod_From_Colony_Queue(colony_idx, COLONY_PRODUCTION_SPY);
                                }
                                break;

                            case COLONY_PRODUCTION_SCIENTIST:
                            case COLONY_PRODUCTION_WORKER:
                            case COLONY_PRODUCTION_FARMER: {
                                bool can_add_pop = true;
                                const int32_t empire_pop_limit =
                                    static_cast<int32_t>(COLCALC::Colony_Race_Pop_Limit(colony, 8));
                                const int32_t empire_pop_count =
                                    COLCALC::Colony_N_Pops_Of_Empire(colony, 8);

                                if (empire_pop_limit <= empire_pop_count) {
                                    can_add_pop = false;
                                } else if (!COLCALC::Room_For_Another_Dude(colony, 8)) {
                                    uint32_t* type_9_pop = nullptr;
                                    uint32_t* flagged_pop = nullptr;
                                    uint32_t* foreign_pop = nullptr;
                                    uint32_t* own_pop = nullptr;

                                    uint32_t* pop_ptr = &colony->pop[0];
                                    uint32_t* pop_end = &colony->pop[static_cast<uint8_t>(colony->n_pops)];

                                    while (pop_ptr < pop_end) {
                                        const uint32_t pop_value = POP::Load(pop_ptr);
                                        const uint32_t pop_type = POP::Get_Race(pop_value);

                                        if (POP::Is_Native(pop_value)) {
                                            type_9_pop = pop_ptr;
                                        } else if (!POP::Is_Android(pop_value)) {
                                            if (POP::Is_Conquered(pop_value)) {
                                                flagged_pop = pop_ptr;
                                            } else if (owner_player_idx != static_cast<int32_t>(pop_type)) {
                                                foreign_pop = pop_ptr;
                                            } else {
                                                own_pop = pop_ptr;
                                            }
                                        }

                                        ++pop_ptr;
                                    }

                                    uint32_t* selected_pop = type_9_pop;
                                    if (selected_pop == nullptr) {
                                        selected_pop = flagged_pop;
                                    }
                                    if (selected_pop == nullptr) {
                                        selected_pop = foreign_pop;
                                    }
                                    if (selected_pop == nullptr) {
                                        selected_pop = own_pop;
                                    }

                                    if (selected_pop == nullptr) {
                                        can_add_pop = false;
                                    } else {
                                        s_MSG pop_removed_msg{};
                                        pop_removed_msg.msg_type = MSG_TYPE_COLONY;
                                        pop_removed_msg.u.colony.planet_idx = colony->planet;
                                        pop_removed_msg.u.colony.subtype = MSG_COLONY_SUBTYPE_ANDROID_KILLED_ORGANIC;
                                        pop_removed_msg.u.colony.c.android_killed.race_idx =
                                            static_cast<int8_t>(POP::Get_Race(POP::Load(selected_pop)));
                                        MSG_::Add_Msg(pop_removed_msg, static_cast<int16_t>(owner_player_idx));

                                        const uint8_t last_pop_index =
                                            static_cast<uint8_t>(static_cast<uint8_t>(colony->n_pops) - 1);
                                        colony->n_pops = last_pop_index;
                                        POP::Store(selected_pop, colony->pop[last_pop_index]);
                                        removed_existing_pop = true;
                                    }
                                }

                                if (can_add_pop) {
                                    uint32_t new_pop = 0;
                                    POP::Set_Assigned(&new_pop, true);
                                    POP::Set_Race(&new_pop, POP::RACE_ANDROID);
                                    POP::Set_Original_Owner(&new_pop, static_cast<uint32_t>(owner_player_idx));

                                    if (current_product == COLONY_PRODUCTION_SCIENTIST) {
                                        POP::Set_Prof(&new_pop, POP::PROF_SCIENTIST);
                                    } else if (current_product == COLONY_PRODUCTION_WORKER) {
                                        POP::Set_Prof(&new_pop, POP::PROF_WORKER);
                                    } else if (current_product == COLONY_PRODUCTION_FARMER) {
                                        POP::Set_Prof(&new_pop, POP::PROF_FARMER);
                                    }

                                    colony->pop[static_cast<uint8_t>(colony->n_pops)] = new_pop;
                                    colony->n_pops = static_cast<uint8_t>(colony->n_pops + 1);
                                } else {
                                    s_MSG pop_failed_msg{};
                                    pop_failed_msg.msg_type = MSG_TYPE_COLONY;
                                    pop_failed_msg.u.colony.planet_idx = colony->planet;
                                    pop_failed_msg.u.colony.subtype = MSG_COLONY_SUBTYPE_NO_ROOM_FOR_ANDROID;
                                    MSG_::Add_Msg(pop_failed_msg, static_cast<int16_t>(owner_player_idx));

                                    production_pool += product_cost;
                                }
                                break;
                            }

                            default:
                                break;
                        }

                        if (colony->producing[1] == COLONY_PRODUCTION_REPEAT) {
                            if (removed_existing_pop) {
                                colony->producing[0] = COLONY_PRODUCTION_NONE;
                                colony->producing[1] = COLONY_PRODUCTION_NONE;
                            }
                        } else {
                            colony->producing[0] = COLONY_PRODUCTION_NONE;
                        }
                    }
                }

                production_pool -= product_cost;
                COLONY::Fix_Colony_Queue(colony_idx);

                if (colony->producing[0] == COLONY_PRODUCTION_NONE &&
                    BUILD_QUEUE::Is_Legacy_Autobuild(colony->auto_building)) {
                    AIBUILD::Player_Colony_Autobuild(&AIDATA::state, &game_world::current, *colony_id, colony);
                    build_msg.u.colony.c.build.append_fmt = 1;
                }

                build_msg.u.colony.c.build.product_id = colony->producing[0];
                MSG_::Add_Msg(build_msg, static_cast<int16_t>(owner_player_idx));
                if (multibuild != 0) {
                    if (multibuild_nonbuilding_products_only != 0 &&
                        !Is_Multibuild_Nonbuilding_Product(current_product)) {
                        break;
                    }
                    completed_product = true;
                    continue;
                }
            }

            break;
        }

        if (production_pool >= 0x7FFF) {
            colony->production_spent = 0x7FFF;
            return;
        }

        colony->production_spent = static_cast<int16_t>(production_pool);
    }

    void __cdecl Ghost_Colony_Stuff(s_colony* colony) {
        int32_t building_idx = 0;
        int32_t surviving_building_count = 0;

        for (building_idx = 0; building_idx < BUILDING_COUNT; ++building_idx) {
            if (colony->buildings[building_idx] != 0) {
                if (game_random::Random(10) == 1) {
                    const int16_t colony_idx = (int16_t)(colony - MOX::colony);
                    ERICNET::Remove_Building(colony_idx, static_cast<int16_t>(building_idx));
                } else {
                    ++surviving_building_count;
                }
            }
        }

        if (surviving_building_count == 0) {
            bool has_incoming_settlers = false;

            for (int32_t player_idx = 0; player_idx < MOX::NUM_PLAYERS; ++player_idx) {
                s_player* player = &MOX::player[player_idx];

                if (player->settlers_freighted > 0) {
                    for (int32_t settler_idx = 0; settler_idx < player->settlers_freighted; ++settler_idx) {
                        if (SETTLER::Get_Dest_Planet(player->settlers[settler_idx].val) == colony->planet) {
                            has_incoming_settlers = true;
                        }
                    }
                }
            }

            if (zero_colony_lost || !has_incoming_settlers) {
                BOMB::Destroy_Colony(colony, 3);
            }
        }
    }
} // namespace COLCALC
