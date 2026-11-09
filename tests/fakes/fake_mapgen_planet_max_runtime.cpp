#include "pch.h"

#include <stdexcept>

#include "lua/lua_game.h"
#include "lua/lua_game_internal.h"
#include "lua/lua_game_ship_identity.h"

namespace {
    alignas(std::max_align_t) uint8_t allocation_pool[512 * 1024];
    s_planet_data planets[MAX_PLANETS];
    s_star_data stars[MAX_STARS];
    s_player players[MAX_PLAYERS];
    s_colony colonies[MAX_COLONIES];
    s_ship_data ships[MAX_SHIPS];
    s_leader_data leaders[MAX_LEADERS];
    s_nebula nebulas[MAX_NEBULAS];
    uint32_t allocation_offset;
    int16_t food_star;
    int16_t weighted_choices[8];
    int16_t weighted_choice_count;
    int16_t weighted_choice_index;
    uint16_t last_short_weights[SYSTEM_SPECIAL_COUNT];
    uint32_t random_result;
    int random_calls;
    bool deposit_rules_enabled;
    bool home_system_edits_enabled;
    int fail_generated_planet;
    int home_system_event_calls;
    int generated_planet_event_calls;
    int generated_event_sequence;
    int first_generated_planet_event_order;
    bool generated_event_observed_home_system;
    int16_t home_system_player_order[MAX_PLAYERS];
    int16_t generated_planet_order[MAX_PLANETS];
    bool galaxy_provider_active;
    bool galaxy_provider_fails;
    bool galaxy_provider_invalid;
    int galaxy_provider_calls;
    bool galaxy_provider_isolated;
    uint64_t galaxy_provider_topology_generation;
    uint64_t galaxy_provider_ship_generation;

    void* Allocate_(uint32_t size) {
        uint32_t alignment = (uint32_t)alignof(std::max_align_t);
        allocation_offset = (allocation_offset + alignment - 1) & ~(alignment - 1);
        if (allocation_offset + size > sizeof(allocation_pool)) return nullptr;

        void* result = &allocation_pool[allocation_offset];
        allocation_offset += size;
        return result;
    }
}

namespace TEST_MAPGEN {
    void Reset_() {
        memset(allocation_pool, 0, sizeof(allocation_pool));
        memset(planets, 0, sizeof(planets));
        memset(stars, 0, sizeof(stars));
        memset(players, 0, sizeof(players));
        memset(colonies, 0, sizeof(colonies));
        memset(ships, 0, sizeof(ships));
        memset(leaders, 0, sizeof(leaders));
        memset(nebulas, 0, sizeof(nebulas));
        memset(MOX::food_per_farmer_table, 0, sizeof(MOX::food_per_farmer_table));
        COLCALC::food_farmer_bonus.soil_enrichment = 2;
        memset(MOX::normal_gal_climate_roll_table, 0,
               sizeof(MOX::normal_gal_climate_roll_table));
        memset(MOX::old_gal_climate_roll_table, 0,
               sizeof(MOX::old_gal_climate_roll_table));

        for (int16_t star_idx = 0; star_idx < MAX_STARS; ++star_idx) {
            for (int16_t orbit = 0; orbit < 5; ++orbit) {
                stars[star_idx].planet_index[orbit] = -1;
            }
        }
        for (int16_t planet_idx = 0; planet_idx < MAX_PLANETS; ++planet_idx) {
            planets[planet_idx].colony_index = -1;
        }

        MOX::NUM_STARS = 0;
        MOX::NUM_PLAYERS = 0;
        MOX::planet_count = 0;
        MOX::NUM_COLONIES = 0;
        MOX::NUM_SHIPS = 0;
        MOX::NUM_NEBULAS = 0;
        MOX::game_type = 0;
        MOX::MAP_MAX_X = 506;
        MOX::MAP_MAX_Y = 400;
        const uint64_t empty_ship_generations[MAX_SHIPS] = {};
        orion2re::lua::game::Restore_Ship_Generation_State(
            empty_ship_generations, MAX_SHIPS);
        allocation_offset = 0;
        food_star = -1;
        memset(weighted_choices, 0, sizeof(weighted_choices));
        weighted_choice_count = 0;
        weighted_choice_index = 0;
        memset(last_short_weights, 0, sizeof(last_short_weights));
        random_result = 1;
        random_calls = 0;
        deposit_rules_enabled = false;
        home_system_edits_enabled = false;
        fail_generated_planet = -1;
        home_system_event_calls = 0;
        generated_planet_event_calls = 0;
        generated_event_sequence = 0;
        first_generated_planet_event_order = 0;
        generated_event_observed_home_system = false;
        memset(home_system_player_order,
            -1,
            sizeof(home_system_player_order));
        memset(generated_planet_order, -1, sizeof(generated_planet_order));
        galaxy_provider_active = false;
        galaxy_provider_fails = false;
        galaxy_provider_invalid = false;
        galaxy_provider_calls = 0;
        galaxy_provider_isolated = false;
        galaxy_provider_topology_generation = 0;
        galaxy_provider_ship_generation = 0;
        orion2re::lua::game::internal::Restore_Topology_Generation(1);
    }

    void Set_Planet_(int16_t planet_idx, int16_t star_idx, int8_t orbit,
                     int8_t planet_type, int16_t colony_idx) {
        planets[planet_idx].star_index = star_idx;
        planets[planet_idx].orbit = orbit;
        planets[planet_idx].planet_type = planet_type;
        planets[planet_idx].colony_index = colony_idx;
        stars[star_idx].planet_index[(uint8_t)orbit] = planet_idx;
    }

    void Set_Food_Star_(int16_t star_idx) {
        food_star = star_idx;
    }

    void Set_Weighted_Choices_(const int16_t* choices, int16_t count) {
        memcpy(weighted_choices, choices, (size_t)count * sizeof(*choices));
        weighted_choice_count = count;
        weighted_choice_index = 0;
    }

    int16_t Weighted_Choice_Count_() {
        return weighted_choice_index;
    }

    const uint16_t* Last_Short_Weights_() {
        return last_short_weights;
    }

    void Set_Random_Result_(uint32_t result) {
        random_result = result;
    }

    int Random_Calls_() {
        return random_calls;
    }

    void Enable_Deposit_Rules_() {
        deposit_rules_enabled = true;
    }

    void Enable_Home_System_Edits_() {
        home_system_edits_enabled = true;
    }

    void Fail_Generated_Event_For_Planet_(int planet_id) {
        fail_generated_planet = planet_id;
    }

    int Generated_Planet_Event_Calls_() {
        return generated_planet_event_calls;
    }

    int Home_System_Event_Calls_() {
        return home_system_event_calls;
    }

    int16_t Home_System_Event_Player_(int index) {
        return home_system_player_order[index];
    }

    int First_Generated_Planet_Event_Order_() {
        return first_generated_planet_event_order;
    }

    bool Generated_Event_Observed_Home_System_() {
        return generated_event_observed_home_system;
    }

    int16_t Generated_Planet_Event_Planet_(int index) {
        return generated_planet_order[index];
    }

    void Enable_Galaxy_Provider_(bool fails, bool invalid) {
        galaxy_provider_active = true;
        galaxy_provider_fails = fails;
        galaxy_provider_invalid = invalid;
    }

    int Galaxy_Provider_Calls_() {
        return galaxy_provider_calls;
    }

    bool Galaxy_Provider_Isolated_() {
        return galaxy_provider_isolated;
    }

    uint64_t Galaxy_Provider_Topology_Generation_() {
        return galaxy_provider_topology_generation;
    }

    uint64_t Galaxy_Provider_Ship_Generation_() {
        return galaxy_provider_ship_generation;
    }
}

namespace orion2re::lua {
    bool Has_Active_Galaxy_Generator() {
        return galaxy_provider_active;
    }

    bool Dispatch_Active_Galaxy_Generator(
        bool* invoked, std::string* diagnostic) {
        ++galaxy_provider_calls;
        *invoked = galaxy_provider_active;
        if (!galaxy_provider_active) return true;
        galaxy_provider_isolated = MOX::star != stars &&
            MOX::planet != planets && MOX::player != players &&
            MOX::colony != colonies && MOX::ship != ships &&
            MOX::leaders != leaders && MOX::nebula != nebulas;
        galaxy_provider_topology_generation =
            game::internal::Topology_Generation();
        galaxy_provider_ship_generation = game::Ship_Generation(0);
        MOX::star[0].x = galaxy_provider_invalid ? MOX::MAP_MAX_X : 77;
        MOX::planet[0].climate = PLANET_CLIMATE_GAIA;
        game_random::Random(100);
        if (galaxy_provider_fails) {
            *diagnostic = "provider failed";
            return false;
        }
        return true;
    }
} // namespace orion2re::lua

namespace orion2re::lua::game::internal {
    namespace {
        uint64_t topology_generation = 1;
    }

    void Advance_Topology_Generation() {
        ++topology_generation;
    }

    uint64_t Topology_Generation() {
        return topology_generation;
    }

    void Restore_Topology_Generation(uint64_t generation) {
        topology_generation = generation;
    }

    void Reset_Leader_Nebula_Generations() {}
} // namespace orion2re::lua::game::internal

namespace orion2re::lua::game {
    namespace {
        uint64_t ship_generations[MAX_SHIPS];
        uint64_t* active_ship_generations = ship_generations;
    } // namespace

    s_ship_generation_scope::s_ship_generation_scope(uint64_t* generations)
        : previous_(active_ship_generations) {
        active_ship_generations = generations;
    }

    s_ship_generation_scope::~s_ship_generation_scope() {
        active_ship_generations = previous_;
    }

    bool Capture_Ship_Generation_State(uint64_t* generations, size_t count) {
        if (generations == nullptr || count != MAX_SHIPS) return false;
        memcpy(generations, active_ship_generations, sizeof(ship_generations));
        return true;
    }

    bool Restore_Ship_Generation_State(
        const uint64_t* generations, size_t count) {
        if (generations == nullptr || count != MAX_SHIPS) return false;
        memcpy(active_ship_generations, generations, sizeof(ship_generations));
        return true;
    }

    uint64_t Ship_Generation(int16_t ship_id) {
        return ship_id < 0 || ship_id >= MAX_SHIPS
            ? 0
            : active_ship_generations[ship_id];
    }
} // namespace orion2re::lua::game

namespace orion2re::lua::game {
    void Home_System_Generated(const s_home_system_generated_event* event) {
        home_system_player_order[home_system_event_calls++] = event->player_id;
        if (!home_system_edits_enabled) return;
        s_planet_data& homeworld =
            MOX::planet[MOX::player[event->player_id].home_planet_id];
        homeworld.size = PLANET_SIZE_HUGE;
        homeworld.climate = PLANET_CLIMATE_GAIA;
        homeworld.planet_special = SYSTEM_SPECIAL_GOLD_DEPOSITS;
        MOX::star[event->system_id].system_special =
            SYSTEM_SPECIAL_GOLD_DEPOSITS;
    }

    void Planet_Generated(const s_planet_generated_event* event) {
        if (first_generated_planet_event_order == 0) {
            first_generated_planet_event_order = ++generated_event_sequence;
        }
        generated_planet_order[generated_planet_event_calls++] =
            event->planet_id;
        if (event->planet_id == fail_generated_planet) {
            throw std::runtime_error("generated planet event failed");
        }
        s_planet_data& planet = MOX::planet[event->planet_id];
        if (event->planet_id == 0 &&
            planet.size == PLANET_SIZE_HUGE &&
            planet.climate == PLANET_CLIMATE_GAIA &&
            planet.planet_special == SYSTEM_SPECIAL_GOLD_DEPOSITS) {
            generated_event_observed_home_system = true;
        }
        if (!deposit_rules_enabled ||
            (planet.planet_special != SYSTEM_SPECIAL_GOLD_DEPOSITS &&
                planet.planet_special != SYSTEM_SPECIAL_GEM_DEPOSITS) ||
            planet.mineral_class > MINERAL_RESOURCE_POOR) {
            return;
        }
        planet.gravity_class = PLANET_GRAVITY_NORMAL_G;
        if (planet.size == PLANET_SIZE_HUGE) {
            planet.mineral_class = MINERAL_RESOURCE_POOR;
        } else if (planet.size >= PLANET_SIZE_SMALL) {
            planet.mineral_class = MINERAL_RESOURCE_ABUNDANT;
        } else {
            planet.mineral_class = MINERAL_RESOURCE_RICH;
        }
    }
} // namespace orion2re::lua::game

namespace MOX {
    int16_t NUM_PLAYERS;
    int16_t NUM_STARS;
    int16_t NUM_SHIPS;
    int16_t NUM_COLONIES;
    int16_t planet_count;
    s_planet_data* planet = planets;
    s_star_data* star = stars;
    s_player* player = players;
    s_colony* colony = colonies;
    s_ship_data* ship = ships;
    s_leader_data* leaders = ::leaders;
    s_nebula* nebula = nebulas;
    uint8_t NUM_NEBULAS;
    int8_t game_type;
    int16_t MAP_MAX_X;
    int16_t MAP_MAX_Y;
    s_animation_header* screen_seg = nullptr;
    uint8_t g_galaxy_age;
    int16_t zoom_count;
    s_settings settings;
    uint8_t planet_special = 64;
    uint8_t planet_special_weighted_chance[SYSTEM_SPECIAL_COUNT] = {
        64, 5, 3, 2, 3, 2, 9, 4, 3, 0, 5, 0,
    };
    uint8_t planet_max_farms[PLANET_SIZE_COUNT];
    uint8_t food_per_farmer_table[PLANET_CLIMATE_COUNT];
    uint8_t normal_gal_climate_roll_table[PLANET_CLIMATE_COUNT][PLANET_GROUP_COUNT];
    uint8_t old_gal_climate_roll_table[PLANET_CLIMATE_COUNT][PLANET_GROUP_COUNT];
}

namespace allocate {
    void* __cdecl Allocate_First_Block(void*, uint32_t size) {
        allocation_offset = 0;
        return Allocate_(size);
    }

    void* __cdecl Allocate_Next_Block(void*, uint32_t size) {
        return Allocate_(size);
    }
}

namespace capture {
    uint16_t __cdecl Check_Release_Version() {
        return 1;
    }
}

namespace game_random {
    uint32_t __cdecl Random(uint32_t) {
        ++random_calls;
        return random_result;
    }
}

namespace GEO {
    void __cdecl Initialize_Black_Hole_Blocks(int16_t) {}

    void __cdecl Initialize_Star_In_Nebula_Info() {}

    bool __cdecl Map_Is_Connected() {
        return true;
    }

    int32_t __cdecl Parsecs_Between_Stars(
        int16_t star_idx_1, int16_t star_idx_2) {
        const int32_t dx = MOX::star[star_idx_1].x -
            MOX::star[star_idx_2].x;
        const int32_t dy = MOX::star[star_idx_1].y -
            MOX::star[star_idx_2].y;
        return dx * dx + dy * dy <= 8 * 8 * 30 * 30 ? 8 : 9;
    }
} // namespace GEO

namespace struct_ {
    int16_t __cdecl Test_Bit_Field(void*, uint16_t) {
        return 0;
    }
} // namespace struct_

namespace RUSS {
    int16_t __cdecl Get_Weighted_Choice_Char(uint8_t*, int16_t) {
        if (weighted_choice_index >= weighted_choice_count) return -1;
        return weighted_choices[weighted_choice_index++];
    }

    int16_t __cdecl Get_Weighted_Choice_Short(const uint16_t* weights, int16_t count) {
        if (count == SYSTEM_SPECIAL_COUNT) {
            memcpy(last_short_weights, weights, sizeof(last_short_weights));
        }
        if (weighted_choice_index >= weighted_choice_count) return -1;
        return weighted_choices[weighted_choice_index++];
    }
}

namespace COLCALC {
    s_food_farmer_bonus food_farmer_bonus = {2};
}

namespace MOX2 {
    uint8_t user_wants_n_space_monsters;
}

namespace HAROLD {
    bool __cdecl Habitable_Spectral_Class(int16_t) {
        return true;
    }

    int16_t __cdecl Planet_Type(int16_t star_idx, int16_t orbit) {
        int16_t planet_idx = MOX::star[star_idx].planet_index[orbit];
        return planet_idx >= 0 ? MOX::planet[planet_idx].planet_type : -1;
    }
}

namespace HOMEGEN {
    bool __cdecl Is_Added_Food_World(int16_t star_idx) {
        return star_idx == food_star;
    }
}

namespace key {
    int32_t Wait_For_Key() {
        return 0;
    }
}

namespace RUSS {
    void __cdecl PFMT(int32_t, int32_t, const char*, ...) {}
}