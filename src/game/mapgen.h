#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace MAPGEN {
    constexpr int32_t STAR_NAME_COUNT = 829;
    struct s_map_layout {
        uint8_t automatic;
        uint16_t width;
        uint16_t height;
        uint16_t star_spacing;
        uint16_t maximum_display_scale;
        uint16_t maximum_zoom_level;
        uint16_t initial_zoom_level;
    };

    struct s_planet_generation_config {
        uint8_t convert_toxic_to_radiated;
        uint8_t improve_ultra_poor_minerals;
        uint8_t normalize_low_gravity;
        uint8_t normalize_heavy_gravity;
        uint8_t upgrade_tiny_planets;
        uint8_t upgrade_small_planets;
    };

    struct s_home_system_config {
        uint8_t minimum_planets;
        uint8_t normalize_unoccupied_planets;
        uint8_t use_fixed_unoccupied_planets;
        uint8_t improve_fourth_unoccupied_planet;
        uint8_t improve_first_unoccupied_planet;
        uint8_t make_first_unoccupied_planet_huge;
        uint8_t make_first_unoccupied_planet_rich;
        uint8_t minimum_starting_planet_production;
        uint8_t huge_starting_planet;
        uint8_t rich_starting_planet;
        uint8_t ultra_rich_starting_planet;
    };

    struct s_special_generation_config {
        uint8_t generate_black_holes;
        uint8_t generate_wormholes;
        uint8_t generate_nebulas;
        uint8_t generate_splinter_colonies;
        uint8_t generate_orion;
        uint8_t artifacts_grant_technology;
        uint8_t replace_splinter_colonies_with_gems;
        uint8_t replace_natives_with_gold;
        uint8_t highlight_marooned_heroes;
        uint8_t remove_marooned_heroes;
    };

    struct s_monster_generation_config {
        uint8_t count;
        uint8_t terraform_guarded_natives;
        uint8_t recalculate_guarded_planet_gravity;
        uint8_t improve_guarded_planets;
        uint8_t natural_chance[GAME_DIFFICULTY_COUNT];
        uint8_t natural_count_cap;
    };

    struct s_nebula_count_config {
        uint8_t minimum;
        uint8_t maximum;
        uint8_t scale_with_star_count;
    };

    extern int16_t star_count_by_galaxy_size[GALAXY_SIZE_COUNT];
    extern int16_t& maximum_star_count;
    extern s_map_layout layout[GALAXY_SIZE_COUNT];
    extern s_planet_generation_config planet_generation_config;
    extern s_home_system_config home_system_config;
    extern s_special_generation_config special_generation_config;
    extern s_monster_generation_config monster_generation_config;
    extern int16_t planet_limit;
    extern uint8_t starting_reveal_radius;
    extern uint8_t star_visual_size_weight[3];
    extern s_nebula_count_config nebula_count[GALAXY_SIZE_COUNT];
    extern uint8_t satellite_orbit_weight[5][GALAXY_AGE_COUNT];
    extern int8_t environment_group[6][5];
    extern int8_t mineral_class[10][6];
    extern int8_t planet_gravity[MINERAL_RESOURCE_COUNT][PLANET_SIZE_COUNT];
    extern uint8_t n_marooned_heroes;
    extern uint8_t n_wormholes;
    extern uint8_t make_food_planet;
    extern uint8_t n_space_monsters;
    extern uint16_t companion_star;
    extern uint8_t n_maps_generated;
    extern uint16_t tests_failed[4];
    extern int8_t* evan_ptr;
    extern uint32_t num_maps_to_generate;
    extern void* name_seg;
    extern uint16_t* name_index;
    extern s_animation_header* fill_msg_bitmap;
    extern char fill_msg[36];
    extern int16_t fill_msg_ctr;
    extern int8_t first_time_map_is_generated;
    extern uint8_t special_count[SYSTEM_SPECIAL_COUNT];
    extern uint32_t n_stars;
    extern uint32_t n_planets;
    extern int16_t t_roll;
    extern uint32_t satellite_stat[6];
    extern uint32_t nebula_stat[MAX_NEBULAS + 1];
    extern int16_t star_count;
    extern s_planet_data* pl;
    extern const char s_raceopt_lbx[12];
    extern const char s_starname_lbx[13];
    extern uint8_t MAPGEN_TRACE;

    void __cdecl Clear_Planets_In_Star_Structure();

    int16_t __cdecl Gas_Giants();

    int16_t __cdecl Home_Planet(int16_t planet_id);

    int8_t __cdecl Generate_Spectral_Class(int16_t star_type_index);
    /** Generates an age-weighted spectral class that is not a black hole. */
    int8_t __cdecl Generate_Non_Black_Hole_Spectral_Class(int16_t star_type_index);
    uint8_t __cdecl Generate_Food_Per_Farmer(int16_t star_index, int16_t planet_index);
    uint8_t __cdecl Generate_Max_Farms(int16_t star_idx, int16_t planet_idx);
    int8_t __cdecl Generate_Orbit(int16_t star_id);
    uint8_t __cdecl Generate_Satellite_Type(int16_t star_idx, int8_t orbit);
    void __cdecl Generate_Satellite(int16_t star_idx, int16_t orbit, int16_t type);
    uint8_t __cdecl Generate_Size();

    int8_t __cdecl Generate_Mineral_Class(int16_t star_index, int16_t unused_planet_idx);

    int8_t __cdecl Get_Climate_Modifier(int8_t climate_index);

    int8_t __cdecl Generate_Gravity_Class(int16_t planet_idx);

    int16_t __cdecl Get_Planet_Group(int16_t planet_idx);

    void __cdecl Update_Planet_Group_Cache();

    int8_t __cdecl Generate_Climate(int16_t planet_group);

    /**
     * Generates a full galaxy map for the requested size, age, and nebula frequency.
     * @param gal_size Requested galaxy size preset.
     * @param gal_age Requested galaxy age preset.
     * @param nebula_freq Requested nebula frequency setting.
     */
    void __cdecl Universe_Generation(uint8_t gal_size, uint8_t gal_age, int8_t nebula_freq);

    /** Shuffles the star-name index table in place. */
    void __cdecl Randomize_Star_Names(uint16_t* star_name_indices);
    /** Saves the shuffled vocabulary before screen allocations are reused. */
    void Prepare_Generated_Star_Names();
    void Reset_Generated_Star_Names();

    /** Initializes map bounds and zoom settings for the current galaxy size. */
    void __cdecl Init_Main_Screen(uint8_t gal_size);

    void __cdecl Generate_Number_Of_Nebulas(int16_t galaxy_size, int16_t unused_nebula_freq);

    void __cdecl Generate_Nebulas();

    void __cdecl Build_Min_Point_Distances(int16_t x, int16_t y, int16_t min_x, int16_t min_y, int16_t max_x, int16_t max_y, int32_t dist_sq, int16_t* out_points, int16_t* count, int16_t max_count);

    /**
     * Places stars into the galaxy grid and optionally assigns spectral classes and names.
     * @param x_cells Horizontal placement cell count.
     * @param y_cells Vertical placement cell count.
     * @param galaxy_age Spectral-generation column to use.
     * @param generate_spectral Non-zero to generate spectral classes.
     * @param use_names Non-zero to assign names during placement.
     */
    bool __cdecl Set_Star_XYs(int16_t x_cells, int16_t y_cells, int16_t galaxy_age, int8_t generate_spectral, int8_t use_names);

    bool __cdecl Star_XY_Invalid(int16_t star_idx, int16_t width, int16_t height, int16_t x_scaled);

    /** Copies the loaded name-table entry into the target star record. */
    void __cdecl Get_Star_Name(int16_t star_idx);
    /** Copies a name from the current galaxy's shuffled vocabulary. */
    bool Copy_Generated_Star_Name(int16_t star_idx, char* name, size_t name_size);
    int16_t __cdecl Generate_Number_Of_Satellites(int16_t star_idx);
    void __cdecl Planet_Generation(int16_t star_idx);
    /** Assigns one generated special to a star system and updates summary counts. */
    void __cdecl Generate_Star_Special(int16_t star_idx, int16_t* wormhole_out, uint8_t* special_counts);
    /** Re-rolls black holes that overlap nebulae into habitable stars. */
    void __cdecl Black_Hole_Fix(int16_t galaxy_age);

    /** Removes excess non-home, non-gas-giant planets until the map reaches the requested cap. */
    void __cdecl Enforce_Planet_Max(int16_t max_planets);

    /** Applies the configured classic-galaxy threshold or the extended capacity. */
    void Enforce_Configured_Planet_Max();

    void __cdecl Set_Black_Hole_Visited_Flags();

    void __cdecl Generate_Wormhole_Links();

    void __cdecl Assign_Marooned_Heroes();

    /** Applies configured transformations that require a completed new-game galaxy. */
    bool __cdecl Finalize_Configured_New_Game(bool* succeeded = nullptr);

    /** Runs the selected galaxy generator over disposable construction state. */
    bool __cdecl Apply_Lua_Galaxy_Generator(
        std::string* diagnostic = nullptr);

    /** Recomputes dependent farm, food, and colony fields for a generated planet. */
    void __cdecl Set_Secondary_Planet_Data(int16_t star_idx, int16_t planet_idx);

    /**
     * Chooses an eligible planet or system special for a star.
     * @param star_idx Star to evaluate.
     * @param wormhole_out Set to non-zero when a wormhole special is chosen.
     * @param special_out Receives the chosen special id, or 0 on failure.
     * @param commit_changes Non-zero to write the special back into map state.
     * @param excluded_mask Bitmask of specials to skip during selection.
     */
    void __cdecl Get_A_Planet_Or_Star_Special(int16_t star_idx, int16_t* wormhole_out, int8_t* special_out, uint8_t commit_changes, int32_t excluded_mask);

    /** Loads animation frames for each generated nebula type. */
    void __cdecl Load_Nebula_Pictures();
} // namespace MAPGEN