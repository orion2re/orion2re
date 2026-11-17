#pragma once
#include <cstdint>

struct s_planet_data;

namespace HOMEGEN {
#pragma pack(push, 1)
    struct s_star_dist {
        int16_t dist;
        int16_t index;
    };
#pragma pack(pop)

    extern const char s_Orion[8];
    extern int32_t HOME_WORLD_GEN_MAX_CTR;
    extern s_good_star* good_stars;
    extern int16_t nearest_food_worlds[MAX_PLAYERS];
    extern s_star_dist* star_list;

    /** Returns whether world_id is already tracked in the nearest food-world list. */
    bool __cdecl Is_Added_Food_World(int16_t world_id);

    /** Generates player home worlds and Orion, returning nonzero on success. */
    uint16_t __cdecl Generate_Home_Worlds(game::s_world_state* world_state);

#pragma pack(push, 1)
    struct CandidateList {
        int16_t stars[20];
        int16_t count;
    };

    struct s_orion_candidate {
        int32_t dist;
        int16_t star_idx;
    };

    struct StarCandidate {
        int32_t distance;
        int16_t star_id;
    };
#pragma pack(pop)

    /** Compares Orion placement candidates by distance. */
    int __cdecl qsort_1(const void* candidate_a, const void* candidate_b);

    /** Builds a candidate list of home stars that satisfy the minimum separation. */
    uint8_t __cdecl Build_Home_Star_List(int16_t* home_stars_out, int32_t min_dist_sq);

    /** Compares star-distance records by their distance field. */
    int32_t __cdecl qsort_star_dist_list(const void* star_dist_a, const void* star_dist_b);

    /** Returns the squared distance between two stars. */
    int32_t __cdecl Dist_Between_Stars(int16_t star_a, int16_t star_b);

    /** Returns the squared distance between two map points. */
    int32_t __cdecl Dist_Between_Points(int16_t x1, int16_t y1, int16_t x2, int16_t y2);

    /** Prepares the home-world generation progress message and backing bitmap. */
    void __cdecl Set_Up_Home_Gen_Msg();

    /** Builds per-star distance data for stars at or beyond min_dist_sq. */
    void __cdecl Build_Min_Star_Distances(int32_t min_dist_sq);

    /** Randomizes the order of the selected home star IDs. */
    void __cdecl Randomize_Home_Worlds(int16_t* home_star_ids);

    /** Applies homeworld guarantees and trait-based adjustments to player starts. */
    void __cdecl Modify_Home_Worlds(::game::s_world_state* world_state);

    /** Creates the Orion system and returns the relocated star index. */
    int16_t __cdecl Generate_Orion(::game::s_world_state* world_state);

    /** Chooses a star to receive the Orion system. */
    int16_t __cdecl Get_Orion_Star_Id(game::s_world_state* world_state);

    /** Places space monsters on eligible systems. */
    void __cdecl Make_System_Monsters(game::s_world_state* world_state);

    /** Assigns race-specific names to home stars. */
    void __cdecl Assign_Home_Star_Names(::game::s_world_state* world_state);

    /** Ensures a home system contains a planet with the requested climate. */
    void Enforce_Climate(game::s_world_state* world_state, int16_t star_idx, int16_t climate_id, game::id_planet* out_planet_idx);

    /** Clears system and planet specials from the given star system. */
    void __cdecl Remove_Specials(::game::s_world_state* world_state, int16_t star_idx);

    /** Sets a planet's size field. */
    void __cdecl Set_Planet_Size(::game::s_world_state* world_state, ::game::id_planet planet_index, int8_t size);

    /** Forces a planet to medium size and refreshes dependent farm data. */
    void __cdecl Enforce_Medium_Planet_Size(::game::s_world_state* world_state, ::game::id_planet planet_idx);

    /** Forces the specified body to be a normal planet. */
    void __cdecl Enforce_Planet_Type(::game::s_world_state* world_state, int16_t star_idx, ::game::id_planet planet_idx);

    /** Forces a planet to at least abundant mineral richness. */
    void __cdecl Enforce_Planet_Abundant_Mineral_Class(::game::s_world_state* world_state, ::game::id_planet planet_idx);

    /** Forces a planet to the requested mineral richness and adjusts gravity data. */
    void __cdecl Enforce_Mineral_Richness(::game::s_world_state* world_state, ::game::id_planet planet_index, int16_t mineral_class);

    /** Marks a planet and its star with the artifacts special. */
    void __cdecl Set_System_Planet_Artifact_Special(::game::s_world_state* world_state, ::game::id_planet planet_idx, int16_t star_idx);

    /** Sets homeworld gravity and downgrades invalid ultra-rich starts when needed. */
    void __cdecl Enforce_Gravity(::game::s_world_state* world_state, ::game::id_planet planet_idx, int8_t gravity_class, int16_t player_idx);

    /** Guarantees a nearby planet-bearing system within min_dist of a home star. */
    void __cdecl Guarantee_A_Planet_At_Min_Dist(game::s_world_state* world_state, int16_t star_idx, int16_t min_dist, int16_t* home_star_ids);

    /** Ensures each non-lithovore colony has at least one nearby food planet. */
    uint8_t __cdecl Ensure_At_Least_1_Food_Planet(::game::s_world_state* world_state);

    /** Appends planets from a non-food system to planet_list and updates count_in_out. */
    void Add_Non_Food_Planets_To_List(int16_t star_idx, game::id_planet* planet_list, int16_t* count_in_out);

    /** Chooses a usable orbit slot in a star system. */
    int16_t __cdecl Usable_Orbit(::game::s_world_state* world_state, int16_t star_idx);

    /** Creates or rewrites a planet record in the given system orbit. */
    void __cdecl Make_One_Planet(::game::s_world_state* world_state, int16_t star_idx, ::game::id_planet planet_idx, int16_t orbit_idx, int8_t is_food_planet);

    /** Ensures a system contains at least three planetary bodies. */
    void __cdecl Enforce_3_Planet_Minimum(game::s_world_state* world_state, int16_t star_idx);

    /** Chooses a random existing planet in the system. */
    game::id_planet Random_Planet(game::s_world_state* world_state, int16_t star_idx);

    /** Assigns a compatible special to a monster-guarded star system. */
    void __cdecl Set_Special_For_Star_Guarded_By_Monster(::game::s_world_state* world_state, s_star_data* star, int16_t monster_type);

    /** Upgrades a monster-guarded planet and refreshes its parent star special. */
    void Upgrade_Planet_Guarded_By_Monster(game::s_world_state* world_state, game::id_planet planet_index, int16_t monster_type);

    /** Returns whether the supplied star currently has a real planet. */
    bool __cdecl Star_Has_Planet(::game::s_world_state* world_state, int16_t star_index);
} // namespace HOMEGEN