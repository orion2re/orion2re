#pragma once

#include "strbuild.h"

namespace COLCALC {
    enum class e_psionics_government_limit : uint8_t {
        imperium = GOVERNMENT_IMPERIUM,
        federation = GOVERNMENT_FEDERATION,
    };

    struct s_command_points {
        int8_t starting;
        int8_t star_base;
        int8_t battlestation;
        int8_t star_fortress;
        int8_t tachyon_communications;
        int8_t sub_space_communications;
        int8_t hyperspace_communications;
        int8_t warlord_planet;
    };

    extern s_command_points command_points;
    extern uint8_t ship_command_points_usage[SHIP_SIZE_COUNT];

    struct s_government_bonus {
        s_ratio16 cost_reduction;
        int8_t science;
        int8_t money;
        int8_t food;
        int8_t production;
    };

    extern s_government_bonus government_bonuses[GOVERNMENT_COUNT];

    struct s_lander_ship_cost {
        int32_t colony_ship;
        int32_t outpost_ship;
        int32_t transport_ship;
    };

    extern s_lander_ship_cost lander_ship_cost;

    struct s_population_capacity {
        uint8_t size[PLANET_SIZE_COUNT];
        uint8_t climate[PLANET_CLIMATE_COUNT];
    };

    struct s_population_bonus {
        uint8_t advanced_city_planning;
        int8_t tolerant;
    };

    extern s_population_capacity population_capacity;
    extern bool population_capacity_table_enabled;
    extern uint8_t population_capacity_table[PLANET_CLIMATE_COUNT][PLANET_SIZE_COUNT];
    extern uint8_t population_capacity_table_tolerant[PLANET_CLIMATE_COUNT][PLANET_SIZE_COUNT];
    extern s_population_bonus population_bonus;

    struct s_misc_costs {
        uint16_t freighters;
        uint16_t android_farmer;
        uint16_t android_worker;
        uint16_t android_scientist;
        uint16_t spy;
        uint16_t terraforming_step;
    };

    extern s_misc_costs misc_costs;

    struct s_growth_rate_bonus {
        uint8_t general_modifier;
        uint8_t microbiotics_tech;
        uint8_t antidote_tech;
        uint8_t boom_event;
        int16_t plague_event;
    };

    extern s_growth_rate_bonus growth_rate_bonus;

    struct s_trade_goods_ratio {
        uint16_t normal;
        uint16_t fantastic_traders;
    };

    extern s_trade_goods_ratio trade_goods_ratio;

    struct s_bc_bonus {
        uint16_t spaceport;
        uint16_t planetary_stock_exchange;
        uint16_t galactic_currency_exchange;
    };

    extern s_bc_bonus bc_bonus;

    struct s_planet_bc_bonus {
        uint8_t gold_deposits;
        uint8_t gem_deposits;
    };

    extern s_planet_bc_bonus planet_bc_bonus;

    struct s_food_colony_bonus {
        uint8_t hydroponic_farms;
        uint8_t subterranean_farms;
    };

    struct s_food_farmer_bonus {
        uint8_t soil_enrichment;
        uint8_t biomorphic_fungi;
        uint8_t weather_controller;
        uint8_t astro_university;
        uint8_t native;
        uint8_t android_farmer;
    };

    struct s_morale_bonus {
        uint8_t holo_simulator;
        uint8_t pleasure_dome;
        uint8_t psionics;
        uint8_t virtual_reality_network;
    };

    enum e_fungi_climate : uint8_t {
        FUNGI_CLIMATE_CLASSIC,
        FUNGI_CLIMATE_REVERSED,
        FUNGI_CLIMATE_UNITED,
    };

    enum e_android_morale : uint8_t {
        ANDROID_MORALE_WORKERS_ONLY,
        ANDROID_MORALE_FARMERS_ONLY,
        ANDROID_MORALE_SCIENTISTS_ONLY,
        ANDROID_MORALE_FARMERS_AND_WORKERS,
        ANDROID_MORALE_WORKERS_AND_SCIENTISTS,
        ANDROID_MORALE_FARMERS_AND_SCIENTISTS,
        ANDROID_MORALE_ALL_DROIDS,
        ANDROID_MORALE_NONE,
    };

    extern s_food_colony_bonus food_colony_bonus;
    extern s_food_farmer_bonus food_farmer_bonus;
    extern s_morale_bonus morale_bonus;
    extern e_psionics_government_limit psionics_government_limit;
    extern uint8_t fungi_climate;
    extern uint8_t android_morale;

    struct s_research_colony_bonus {
        uint8_t research_lab;
        uint8_t supercomputer;
        uint8_t galactic_cybernet;
        uint8_t autolab;
    };

    extern s_research_colony_bonus research_colony_bonus;

    struct s_research_scientist_bonus {
        uint8_t base_scientist;
        uint8_t artifacts_planet;
        uint8_t orion_planet;
        uint8_t research_lab;
        uint8_t supercomputer;
        uint8_t galactic_cybernet;
        uint8_t astro_university;
        uint8_t android_scientist;
        uint8_t heightened_intelligence;
    };

    extern s_research_scientist_bonus research_scientist_bonus;

    struct s_production_worker_bonus {
        int8_t automated_factory;
        int8_t robo_miner_plant;
        int8_t deep_core_mine;
        int8_t recyclotron;
        int8_t astro_university;
        int8_t android_worker;
        int8_t microlite_construction;
    };

    struct s_production_colony_bonus {
        uint8_t automated_factory;
        uint8_t robo_miners;
        uint8_t deep_core_mines;
        uint8_t robotic_factory[MINERAL_RESOURCE_COUNT];
    };

    extern s_production_worker_bonus production_worker_bonus;
    extern s_production_colony_bonus production_colony_bonus;
    extern uint8_t planet_no_pollution_prod[PLANET_SIZE_COUNT];
    extern uint8_t nano_disassemblers_coeff;
    extern uint16_t pollution_processor_ratio;
    extern uint16_t atmospheric_renewer_ratio;
    extern uint8_t pop_bonus_biospheres;
    extern uint8_t pop_bonus_subterranean[PLANET_SIZE_COUNT];
    extern uint8_t weather_control_climate;
    extern uint8_t terraform_toxic_building;
    extern uint8_t aquatic_treats_tundra_as_swamp;

    extern uint8_t ignore_mixed_race_penalty[GOVERNMENT_COUNT];
    extern uint8_t housing_creates_no_farmers;
    extern uint8_t housing_no_popagg;
    extern uint8_t new_farmer_food_threshold;
    extern uint8_t cumulative_growth_tech_bonuses;
    extern uint8_t disable_housing;
    extern uint8_t androids_available;
    extern uint8_t planet_extinction;
    extern uint8_t zero_colony_lost;
    extern uint8_t feudal_affects_freighters;
    extern uint8_t keep_research_on_breakthrough;
    extern uint8_t keep_research_on_finds;
    extern uint8_t fixed_research_cost;
    extern uint16_t hyper_advanced_cost_increment;
    extern uint8_t scout_lab_research_bonus[SHIP_SIZE_COUNT];
    extern uint8_t replicator_food_pp;
    extern uint8_t replicator_food_bc;
    extern uint8_t freighters_cash_bonus;
    extern uint8_t multibuild;
    extern uint8_t multibuild_nonbuilding_products_only;
    extern uint8_t minimum_orbital_base_ship_class;
    extern uint32_t growth_formula_factor;
    extern uint8_t housing_formula_factor;
    extern uint8_t cloning_center_growth_bonus;

    typedef int16_t(__cdecl* ColonyBaseFunc)(const s_colony* colony, int16_t owner, int16_t player_idx, s_colony_job_production* out);

    extern ColonyBaseFunc colony_empire_base_functions[3];

    extern uint8_t minerals_per_mine[8];

    extern uint8_t climate_maintenance_modifiers[PLANET_CLIMATE_COUNT];

    extern int8_t morale_lookup_table[GOVERNMENT_COUNT];
    extern int8_t non_capital_morale_lookup[GOVERNMENT_COUNT];
    extern int8_t barracks_morale_lookup[GOVERNMENT_COUNT];

    struct s_colony_morale {
        int8_t (&government_penalty)[GOVERNMENT_COUNT];
        int8_t (&no_capitol_penalty)[GOVERNMENT_COUNT];
        uint8_t (&ignore_mixed_race_penalty)[GOVERNMENT_COUNT];
    };

    extern s_colony_morale morale;

    extern int8_t colony_job_production_difficulty_lookup[8];

    extern char* government_morale_msg;
    extern char* mixed_races_msg;
    extern char* unification_has_no_morale_msg;
    extern char* research_summary_msg;
    extern char* no_capital_msg;
    extern char* no_barracks_msg;
    extern char* morale_summary_msg;
    extern char* total_morale_msg;
    extern char* command_summary_msg;
    extern char* ship_maintenance_costs_msg;
    extern char* d_from_1_s_ship_msg;
    extern char* d_from_d_battlestations_msg;
    extern char* from_warlord_msg;
    extern char* d_from_1_star_base_msg;
    extern char* d_from_1_noncombat_ship_msg;
    extern char* d_from_d_fortresses_msg;
    extern char* starting_command_points_msg;
    extern char* d_from_d_star_bases_msg;
    extern char* total_command_point_used_msg;
    extern char* d_from_d_s_ships_msg;
    extern char* d_from_d_noncombat_ships_msg;
    extern char* total_command_points_msg;
    extern char* bc_summary_msg;
    extern char* total_command_points_used_msg;
    extern char* from_imperium_msg;
    extern char* d_from_1_battlestation_msg;
    extern char* d_from_1_fortress_msg;

    uint8_t __cdecl Colony_Can_Build_Product(s_colony* colony, int16_t product);

    bool __cdecl Room_For_Another_Dude(s_colony* colony, int race_idx);

    void Format_Food2(uint16_t food2, char* buffer, uint32_t buffer_size);

    int32_t __cdecl Production_Cost_To_Buy(int32_t product_cost, int32_t production_spent);

    int32_t __cdecl Colony_Cost_To_Buy_Given_Product(s_colony* colony, int32_t product, int32_t production_spent);

    int32_t __cdecl Colony_Cost_To_Buy_Producing_Product(s_colony* colony);

    bool __cdecl Colony_Can_Afford_To_Buy_Product(int16_t colony_idx);

    int32_t __cdecl Colony_Product_Cost(s_colony* colony, int16_t product_id);

    int32_t __cdecl Colony_Producing_Product_Cost(s_colony* colony);

    int32_t __cdecl Ship_Type_Cost_For_Player(int32_t ship_type, int16_t player_idx);

    void __cdecl Colony_Buy_Outright(int16_t colony_idx);

    int16_t __cdecl Colony_N_Turns_To_Produce(s_colony* colony);

    void __cdecl Apply_Colony_Changes(s_colony* colony);

    void __cdecl Apply_Colony_Pop_Growth(s_colony* colony);

    extern int8_t assimilation_rate_by_government[GOVERNMENT_COUNT];

    void __cdecl Apply_Assimilation(s_colony* colony);

    void __cdecl Produce_Ground_Military(s_colony* colony);

    void __cdecl Apply_Production(s_colony* colony);

    void __cdecl Ghost_Colony_Stuff(s_colony* colony);

    void __cdecl Apply_All_Colony_Changes();
    void __cdecl Do_Surrenders();
    void __cdecl All_Colony_Calculations();
    void __cdecl Apply_All_Player_Changes();
    void __cdecl Compute_Blockades();

    void __cdecl Do_Colony_Owner_Change_Star_Colony_Stuff(s_colony* colony);

    void __cdecl Colony_Environmental_Stuff(s_colony* colony);

    void __cdecl Compute_Star_Colony_Stuff(s_star_data* star);

    int32_t __cdecl Mixed_Race_Morale_Penalty(s_colony* colony);

    char* __cdecl Player_Name(int32_t player_idx);

    int __cdecl Colony_Officer(s_colony* colony);

    short __cdecl Colony_Race_Pop_Limit(s_colony* colony, int16_t player_idx);

    int16_t __cdecl Get_Player_Max_Population(int16_t player_idx);
    int __cdecl Best_Leader_Diplomat_Bonus(int player_idx);

    bool __cdecl Star_Has_Asteroid_Or_Gas_Giant(int16_t star_index);

    /**
     * Sums building construction costs for each player's completed colonies.
     * @param player_costs Output array indexed by player.
     */
    void __cdecl Compute_Total_Building_Costs(int32_t* player_costs);

    extern int8_t difficulty_bc_lookup_table[8];
    extern int8_t size_climate_max_pop_lookup[PLANET_CLIMATE_COUNT];
    extern char* government_bonus_msg;
    extern char* produced_by_farmers_msg;
    extern char* produced_by_workers_msg;
    extern char* government_penalty_msg;
    extern char* native_bonus_msg;
    extern char* conquered_penalty_msg;
    extern char* produced_by_scientists_msg;
    extern char* anomaly_msg;
    extern char* bonus_msg;
    extern char* food_summary_msg;
    extern char* blockaded_penalty_msg;
    extern char* morale_penalty_msg;
    extern char* total_research_produced_msg;
    extern char* artifacts_bonus_msg;
    extern char* total_food_produced_msg;
    extern char* gravity_penalty_msg;
    extern char* aquatic_bonus_msg;
    extern char* leader_bonus_msg;
    extern char* android_bonus_msg;
    extern char* intelligence_bonus_msg;
    extern char* morale_bonus_msg;
    extern char* special_income_msg;
    extern char* taxed_from_industry_msg;
    extern char* used_msg;
    extern char* pollution_penalty_msg;
    extern char* replicated_to_d_food_msg;
    extern char* taxes_collected_msg;
    extern char* trade_goods_msg;
    extern char* total_industry_produced_msg;
    extern char* total_income_msg;
    extern char* industry_summary_msg;

    extern char* taxed_msg;
    extern char* base_msg;

    char* __cdecl Load_Colcalc_Strings(char* strings);

    int16_t __cdecl Colony_Industry_Per_Worker(const s_colony* colony, int16_t player_idx, s_colony_job_production* out);
    int16_t __cdecl Colony_Food2_Per_Farmer(const s_colony* colony, int16_t player_idx, s_colony_job_production* out);
    int16_t __cdecl Colony_Research_Per_Scientist(const s_colony* colony, int16_t player_idx, s_colony_job_production* out);

    bool __cdecl Biomorphic_Fungi_Applies(int16_t food2_per_farmer);

    void __cdecl Twiddle_Initial_Homeworlds();
    void __cdecl Twiddle_Initial_Homeworld(s_colony* colony);

    /**
     * Recomputes a colony's local production and maintenance state before empire-wide import distribution.
     * @param colony Colony to prepare.
     */
    void __cdecl Pre_Import_Computing(s_colony* colony);

    /**
     * Distributes empire food imports among a player's colonies and optionally finalizes one target colony.
     * @param player_idx Player whose colonies are being balanced.
     * @param colony Optional colony to ensure receives post-import recomputation.
     */
    void __cdecl Pass_Out_Imports(int16_t player_idx, s_colony* colony);

    void __cdecl Recalculate_Colony(s_colony* colony);

    bool __cdecl Colony_Can_Build_Large_Ships(s_colony* colony);

    /**
     * Recomputes colony-derived control and infrastructure flags for every star system.
     */
    void __cdecl Compute_All_Star_Colony_Stuff();
    void __cdecl Compute_Star_Colony_Stuff(s_star_data* star);

    void __cdecl Star_Colony_Calculation(int16_t star_idx, int16_t player_idx);

    void __cdecl Init_Surrenders();

    void __cdecl Surrender_Empire(int16_t player_id, int16_t target_id);

    bool __cdecl May_Surrender_Star(int16_t star_idx, int16_t player_idx, int16_t recipient_idx);

    void __cdecl Surrender_Star(int16_t star_idx, int16_t player_idx, int16_t to_player_idx);

    void __cdecl Do_Surrender_Colony(s_colony* colony, int16_t new_owner);

    void __cdecl Do_Colony_Owner_Change_Star_Colony_Stuff(s_colony* colony);

    void __cdecl Do_Surrender_Star(int16_t star_idx, int16_t old_owner, int16_t new_owner);

    void __cdecl Do_Surrender_Empire(int16_t player_idx, int16_t target_idx);

    void __cdecl Eliminate_Player(s_player* player);

    bool __cdecl May_Surrender_Empire(int16_t player_idx, int16_t target_idx);

    int32_t __cdecl Sort_star_weights(const void* a, const void* b);

    int16_t __cdecl Get_Surrender_System(int16_t player_idx, int16_t target_player_idx, int16_t selection_mode);

    void __cdecl No_Op1();

    int __cdecl Player_Research_Cost(s_player* plr, int16_t tech_field);

    int32_t __cdecl Chance_For_Research_Breakthrough_Aux(int64_t accumulated_total, int64_t produced, int64_t cost);

    int16_t __cdecl Player_N_Turns_Until_Research_Complete(s_player* plr);

    int32_t __cdecl Chance_For_Research_Breakthrough(s_player* plr);

    int16_t __cdecl Get_First_Player_Colony_Idx(int32_t player_idx);

    inline bool __cdecl Monster_At_Star(int16_t star_idx);

    void __cdecl Delete_Obsolete_Artifical_Planets_Under_Construction(int16_t star_idx);

    int32_t __cdecl Colony_N_Pops_Of_Empire(s_colony* colony, uint32_t empire_id);

    uint8_t __cdecl Leader_At_Anomaly(s_leader_data* leader);

    void __cdecl All_Colony_Calculations();

    void __cdecl Colony_Calculation(int16_t colony_idx);

    void __cdecl Colony_Pop_Grows(s_colony* colony);

    void __cdecl Colony_Specialty(s_colony* colony);

    void __cdecl Post_Import_Computing(s_colony* colony);

    void __cdecl Update_Player_Stats(s_player* player);

    void __cdecl Colony_Environmental_Stuff(s_colony* colony);

    /**
     * Computes a colony's morale and optionally writes a short breakdown string.
     * @param colony Colony to evaluate.
     * @param debug_out Optional output buffer for the morale breakdown.
     */
    void __cdecl Colony_Morale(s_colony* colony, STRBUILD::s_builder* debug_out);

    void __cdecl Colony_Industry_Maintenance(s_colony* colony);

    /**
     * Computes colony industry output, pollution, and optional production breakdown text.
     * @param colony Colony to evaluate.
     * @param debug_out Optional output buffer for the industry summary.
     */
    void __cdecl Colony_Industry_Production(s_colony* colony, STRBUILD::s_builder* debug_out);

    void __cdecl Show_Industry_Production(int16_t colony_idx);

    void __cdecl Show_Research_Production(int16_t colony_idx);

    int __cdecl Colony_Job_Production(s_colony* colony, short econ_type, s_colony_job_production* out);

    int16_t __cdecl Colony_Pop_Base_Prod_Produced(const s_colony* colony, const uint32_t* pop, int16_t job_type, s_colony_job_production* out);

    int16_t __cdecl Colony_Pop_Prod_Produced(const s_colony* colony, const uint32_t* pop_unit, int16_t mode);

    extern char tmp_quotient[20];
    char* __cdecl Format_Quotient(int32_t value, int32_t divisor);

    /**
     * Computes colony food output and optionally writes a production breakdown text.
     * @param colony Colony to evaluate.
     * @param debug_out Optional output buffer for the food summary.
     */
    void __cdecl Colony_Food_Production(s_colony* colony, STRBUILD::s_builder* debug_out);

    int16_t __cdecl Colony_Empire_Base_Food2_Produced(const s_colony* colony, int16_t owner_idx, int16_t player_idx, s_colony_job_production* out);

    int16_t __cdecl Colony_Empire_Base_Industry_Produced(const s_colony* colony, int16_t type, int16_t player_idx, s_colony_job_production* out);

    int16_t __cdecl Colony_Empire_Base_Research_Produced(const s_colony* colony, int16_t research_type, int16_t player_idx, s_colony_job_production* out);

    void __cdecl Colony_Food_Maintenance(s_colony* colony);

    void __cdecl Colony_Replicators(s_colony* colony);

    void __cdecl Colony_Research_Production(s_colony* colony, STRBUILD::s_builder* debug_out);

    void __cdecl Colony_Industry_To_Tax(s_colony* colony);

    void __cdecl Colony_BC_Production(s_colony* colony, STRBUILD::s_builder* debug_buf);

    void __cdecl Colony_BC_Maintenance(s_colony* colony);

    bool __cdecl Colony_Is_Blockaded(s_colony* colony);

    int __cdecl Calc_Player_Climate(const s_player* player, int climate);

    int __cdecl Size_And_Climate_Max_Population(int8_t size, int climate, uint8_t is_immune);

    int16_t __cdecl Size_And_Climate_Race_Pop_Limit(int8_t size, int climate, int16_t player_idx);

    int32_t __cdecl Tribute_Received_From(int32_t receiver_idx, int32_t payer_idx);

    bool __cdecl Player_May_Get_Tech_App(s_player* plr, int tech_app);

    void __cdecl Player_Gets_Tech_App(s_player* plr, int16_t tech_app_id);

    inline bool __cdecl Field_Is_Gray_Box(int32_t field_id) {
        switch (field_id) {
            case 0x16:
            case 0x17:
            case 0x1c:
            case 0x1d:
            case 0x37:
            case 0x39:
                return true;
            default:
                return false;
        }
    }

    void __cdecl Advance_Player_Field(s_player* plr, int32_t field_id, bool skip_first_advance);

    void __cdecl Ensure_Uncreative_Field_OK(s_player* player, int field_idx);

    void __cdecl Do_Player_Colony_Calculations(int16_t player_idx);

    void __cdecl Do_Player_Colony_Post_Production(int16_t player_idx);

    bool __cdecl Player_Owns_Officer(s_player* plr, s_leader_data* leader);

    /**
     * Recalculates player maintenance totals, command points, and optional maintenance breakdown text.
     * @param plr Player to update.
     * @param debug_out Optional output buffer for the maintenance summary.
     */
    void __cdecl Compute_Player_Maintenance(s_player* plr, STRBUILD::s_builder* debug_out);

    int16_t __cdecl Gravity_Player_Production_Factor(const s_colony* colony, int16_t player_idx);

    int16_t __cdecl Get_Govt_Type_Tech_App(int16_t govt_type);

    void __cdecl Compute_Potential_Colony_Info(s_player* plr, s_planet_data* planet, s_potential_colony_info* results);

    int16_t __cdecl Planet_Max_Population_For_Player(s_planet_data* planet, int16_t player_idx);

    bool __cdecl Housing_Is_Useless(s_colony* colony);

    int16_t __cdecl N_Colonies_And_Outposts_At_Star(int16_t star_index);

    int16_t N_Living_Human_Players();

    int16_t N_Living_Players();

    void __cdecl Check_For_Eliminated_Players();

    void __cdecl Apply_All_Player_Changes();

    void __cdecl Check_For_Research_Breakthrough(s_player* player);

    void __cdecl Resolve_Research_Completion(s_player* player, bool keep_points, int64_t points_spent);

    void __cdecl Give_Player_Field(s_player* player, int field_id, char get_all_flag);

    void __cdecl Compute_Player_Total_Known_Tech_Cost(s_player* player);

    short __cdecl N_Prods_In_Colony_Queue(int16_t colony_idx, short item_id);

    int16_t __cdecl Colony_Planetary_Shield(s_colony* colony);

    void __cdecl Show_Command_Points(int player_idx);
    int16_t __cdecl Get_System_Population(int16_t player_idx, int16_t star_idx);

    int16_t __cdecl Get_Possible_Surrender_Systems(int16_t player_idx, int16_t target_player_idx, int16_t* star_list);
} // namespace COLCALC