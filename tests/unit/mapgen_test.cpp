#include "pch.h"

#include <stdexcept>

#include "doctest/doctest.h"

#include "lua/lua_game.h"
#include "lua/lua_game_internal.h"
#include "lua/lua_game_ship_identity.h"

namespace TEST_MAPGEN {
    void Reset_();
    void Set_Planet_(int16_t planet_idx, int16_t star_idx, int8_t orbit,
                     int8_t planet_type, int16_t colony_idx);
    void Set_Food_Star_(int16_t star_idx);
    void Set_Weighted_Choices_(const int16_t* choices, int16_t count);
    int16_t Weighted_Choice_Count_();
    const uint16_t* Last_Short_Weights_();
    void Set_Random_Result_(uint32_t result);
    int Random_Calls_();
    void Enable_Deposit_Rules_();
    void Enable_Home_System_Edits_();
    void Fail_Generated_Event_For_Planet_(int planet_id);
    int Home_System_Event_Calls_();
    int16_t Home_System_Event_Player_(int index);
    int First_Generated_Planet_Event_Order_();
    bool Generated_Event_Observed_Home_System_();
    int Generated_Planet_Event_Calls_();
    int16_t Generated_Planet_Event_Planet_(int index);
    void Enable_Galaxy_Provider_(bool fails, bool invalid);
    int Galaxy_Provider_Calls_();
    bool Galaxy_Provider_Isolated_();
    uint64_t Galaxy_Provider_Topology_Generation_();
    uint64_t Galaxy_Provider_Ship_Generation_();
} // namespace TEST_MAPGEN

DOCTEST_TEST_CASE("planet size thresholds and mineral climate modifiers use their data tables") {
    TEST_MAPGEN::Reset_();
    const uint8_t thresholds[] = {2, 4, 6, 8, 10};
    memcpy(MOX::planet_size_table, thresholds, sizeof(thresholds));
    TEST_MAPGEN::Set_Random_Result_(6);
    DOCTEST_CHECK(MAPGEN::Generate_Size() == PLANET_SIZE_MEDIUM);
    TEST_MAPGEN::Set_Random_Result_(11);
    DOCTEST_CHECK(MAPGEN::Generate_Size() == 0xff);
    MOX::climate_modifier_table[MINERAL_RESOURCE_ABUNDANT] = -3;
    DOCTEST_CHECK(MAPGEN::Get_Climate_Modifier(MINERAL_RESOURCE_ABUNDANT) == -3);
    DOCTEST_CHECK(MAPGEN::Get_Climate_Modifier(-1) == 0);
    DOCTEST_CHECK(MAPGEN::Get_Climate_Modifier(MINERAL_RESOURCE_COUNT) == 0);
}

DOCTEST_TEST_CASE("satellite count uses the spectral column and one-based random row") {
    TEST_MAPGEN::Reset_();
    MOX::star[0].spectral_class = STAR_CLASS_G;
    MOX::class_to_num_satellites[4][STAR_CLASS_G] = 3;
    TEST_MAPGEN::Set_Random_Result_(5);
    DOCTEST_CHECK(MAPGEN::Generate_Number_Of_Satellites(0) == 3);
    MOX::star[0].spectral_class = STAR_CLASS_BLACK_HOLE;
    DOCTEST_CHECK(MAPGEN::Generate_Number_Of_Satellites(0) == 0);
    DOCTEST_CHECK(TEST_MAPGEN::Random_Calls_() == 2);
}

DOCTEST_TEST_CASE("satellite type rejects impossible inner gas giants and creates companion stars") {
    TEST_MAPGEN::Reset_();
    DOCTEST_CHECK(MAPGEN::Generate_Satellite_Type(0, -1) == MAPGEN_SATELLITE_ASTEROID);
    DOCTEST_CHECK(MAPGEN::Generate_Satellite_Type(0, 5) == MAPGEN_SATELLITE_ASTEROID);
    for (int roll = 0; roll < 10; ++roll) {
        MOX::orbit_to_satellite_type[roll][0] = MAPGEN_SATELLITE_GAS_GIANT;
    }
    DOCTEST_CHECK(MAPGEN::Generate_Satellite_Type(0, 0) == MAPGEN_SATELLITE_ASTEROID);
    DOCTEST_CHECK(TEST_MAPGEN::Random_Calls_() == 0);
    MOX::star[0].spectral_class = STAR_CLASS_B;
    MOX::orbit_to_satellite_type[1][0] = MAPGEN_SATELLITE_COMPANION_STAR;
    TEST_MAPGEN::Set_Random_Result_(2);
    DOCTEST_CHECK(MAPGEN::Generate_Satellite_Type(0, 0) == 5);
    DOCTEST_CHECK(MAPGEN::companion_star == 1);
    DOCTEST_CHECK(TEST_MAPGEN::Random_Calls_() == 2);
    MOX::orbit_to_satellite_type[0][4] = MAPGEN_SATELLITE_INVALID;
    TEST_MAPGEN::Set_Random_Result_(1);
    DOCTEST_CHECK(MAPGEN::Generate_Satellite_Type(0, 4) == MAPGEN_SATELLITE_ASTEROID);
}

namespace {
    struct s_galaxy_snapshot {
        std::vector<s_star_data> stars;
        std::vector<s_planet_data> planets;
        std::vector<s_player> players;
        std::vector<s_colony> colonies;
        std::vector<s_ship_data> ships;
        std::vector<s_leader_data> leaders;
        std::vector<s_nebula> nebulas;
        std::vector<uint64_t> ship_generations;
        uint64_t topology_generation = 0;
        int16_t map_width = 0;
        int16_t map_height = 0;
        int16_t player_count = 0;
        int16_t star_count = 0;
        int16_t planet_count = 0;
        int16_t colony_count = 0;
        int16_t ship_count = 0;
        uint8_t nebula_count = 0;
    };

    s_galaxy_snapshot Capture_Galaxy_() {
        s_galaxy_snapshot snapshot = {
            {MOX::star, MOX::star + MAX_STARS},
            {MOX::planet, MOX::planet + MAX_PLANETS},
            {MOX::player, MOX::player + MAX_PLAYERS},
            {MOX::colony, MOX::colony + MAX_COLONIES},
            {MOX::ship, MOX::ship + MAX_SHIPS},
            {MOX::leaders, MOX::leaders + MAX_LEADERS},
            {MOX::nebula, MOX::nebula + MAX_NEBULAS},
            std::vector<uint64_t>(MAX_SHIPS),
            orion2re::lua::game::internal::Topology_Generation(),
            MOX::MAP_MAX_X,
            MOX::MAP_MAX_Y,
            MOX::NUM_PLAYERS,
            MOX::NUM_STARS,
            MOX::planet_count,
            MOX::NUM_COLONIES,
            MOX::NUM_SHIPS,
            MOX::NUM_NEBULAS,
        };
        DOCTEST_REQUIRE(orion2re::lua::game::Capture_Ship_Generation_State(
            snapshot.ship_generations.data(), MAX_SHIPS));
        return snapshot;
    }

    void Check_Galaxy_Equals_(const s_galaxy_snapshot& expected) {
        DOCTEST_CHECK(memcmp(MOX::star,
                          expected.stars.data(),
                          MAX_STARS * sizeof(s_star_data)) == 0);
        DOCTEST_CHECK(memcmp(MOX::planet,
                          expected.planets.data(),
                          MAX_PLANETS * sizeof(s_planet_data)) == 0);
        DOCTEST_CHECK(memcmp(MOX::player,
                          expected.players.data(),
                          MAX_PLAYERS * sizeof(s_player)) == 0);
        DOCTEST_CHECK(memcmp(MOX::colony,
                          expected.colonies.data(),
                          MAX_COLONIES * sizeof(s_colony)) == 0);
        DOCTEST_CHECK(memcmp(MOX::ship,
                          expected.ships.data(),
                          MAX_SHIPS * sizeof(s_ship_data)) == 0);
        DOCTEST_CHECK(memcmp(MOX::leaders,
                          expected.leaders.data(),
                          MAX_LEADERS * sizeof(s_leader_data)) == 0);
        DOCTEST_CHECK(memcmp(MOX::nebula,
                          expected.nebulas.data(),
                          MAX_NEBULAS * sizeof(s_nebula)) == 0);
        DOCTEST_CHECK(MOX::NUM_STARS == expected.star_count);
        DOCTEST_CHECK(MOX::planet_count == expected.planet_count);
        DOCTEST_CHECK(MOX::NUM_COLONIES == expected.colony_count);
        DOCTEST_CHECK(MOX::NUM_SHIPS == expected.ship_count);
        DOCTEST_CHECK(MOX::NUM_NEBULAS == expected.nebula_count);
        DOCTEST_CHECK(orion2re::lua::game::internal::Topology_Generation() ==
            expected.topology_generation);
        DOCTEST_CHECK(MOX::MAP_MAX_X == expected.map_width);
        DOCTEST_CHECK(MOX::MAP_MAX_Y == expected.map_height);
        DOCTEST_CHECK(MOX::NUM_PLAYERS == expected.player_count);
        std::vector<uint64_t> ship_generations(MAX_SHIPS);
        DOCTEST_REQUIRE(orion2re::lua::game::Capture_Ship_Generation_State(
            ship_generations.data(), MAX_SHIPS));
        DOCTEST_CHECK(ship_generations == expected.ship_generations);
    }

    void Set_Valid_Galaxy_Candidate_() {
        MOX::NUM_STARS = 1;
        MOX::NUM_PLAYERS = 1;
        MOX::planet_count = 1;
        MOX::NUM_COLONIES = 1;
        TEST_MAPGEN::Set_Planet_(
            0, 0, 0, PLANET_TYPE_PLANET, 0);
        MOX::star[0].x = 10;
        MOX::star[0].y = 20;
        MOX::star[0].spectral_class = STAR_CLASS_G;
        MOX::star[0].owner = 0;
        MOX::star[0].wormhole_star_id = -1;
        MOX::planet[0].size = PLANET_SIZE_MEDIUM;
        MOX::planet[0].gravity_class = PLANET_GRAVITY_NORMAL_G;
        MOX::planet[0].climate = PLANET_CLIMATE_TERRAN;
        MOX::planet[0].mineral_class = MINERAL_RESOURCE_ABUNDANT;
        MOX::player[0].home_planet_id = 0;
        MOX::colony[0].planet = 0;
        MOX::colony[0].owner = 0;
    }
} // namespace

DOCTEST_TEST_CASE("Cluster defaults to Huge generation geometry") {
    const auto& huge = MAPGEN::layout[GALAXY_SIZE_HUGE];
    const auto& cluster = MAPGEN::layout[GALAXY_SIZE_CLUSTER];

    DOCTEST_CHECK(MAPGEN::star_count_by_galaxy_size[GALAXY_SIZE_CLUSTER] ==
        MAPGEN::star_count_by_galaxy_size[GALAXY_SIZE_HUGE]);
    DOCTEST_CHECK(Galaxy_Size_For_Classic_Rules_(GALAXY_SIZE_CLUSTER) ==
        GALAXY_SIZE_HUGE);
    DOCTEST_CHECK(cluster.automatic == huge.automatic);
    DOCTEST_CHECK(cluster.width == huge.width);
    DOCTEST_CHECK(cluster.height == huge.height);
    DOCTEST_CHECK(cluster.star_spacing == huge.star_spacing);
    DOCTEST_CHECK(cluster.maximum_display_scale ==
        huge.maximum_display_scale);
    DOCTEST_CHECK(cluster.maximum_zoom_level == huge.maximum_zoom_level);
    DOCTEST_CHECK(cluster.initial_zoom_level == huge.initial_zoom_level);
}

DOCTEST_TEST_CASE("planet trimming preserves generated Orion and compacted references") {
    TEST_MAPGEN::Reset_();
    MOX::NUM_STARS = 4;
    MOX::NUM_PLAYERS = 1;
    MAPGEN::special_generation_config.generate_orion = 1;

    TEST_MAPGEN::Set_Planet_(0, 1, 0, PLANET_TYPE_PLANET, -1);
    TEST_MAPGEN::Set_Planet_(1, 0, 1, PLANET_TYPE_PLANET, -1);
    TEST_MAPGEN::Set_Planet_(2, 2, 0, PLANET_TYPE_PLANET, 0);
    TEST_MAPGEN::Set_Planet_(3, 2, 1, PLANET_TYPE_PLANET, -1);
    TEST_MAPGEN::Set_Planet_(4, 3, 0, PLANET_TYPE_PLANET, -1);
    TEST_MAPGEN::Set_Planet_(5, 1, 1, PLANET_TYPE_GAS_GIANT, -1);
    MOX::planet_count = 6;

    MOX::star[0].system_special = SYSTEM_SPECIAL_ORION_STAR;
    MOX::planet[1].planet_special = SYSTEM_SPECIAL_ORION_STAR;
    MOX::player[0].home_planet_id = 2;
    MOX::colony[0].planet = 2;
    TEST_MAPGEN::Set_Food_Star_(3);

    MAPGEN::Enforce_Planet_Max(5);

    DOCTEST_CHECK(MOX::planet_count == 4);
    DOCTEST_CHECK(MOX::planet[0].star_index == 0);
    DOCTEST_CHECK(MOX::planet[0].planet_special == SYSTEM_SPECIAL_ORION_STAR);
    DOCTEST_CHECK(MOX::star[0].system_special == SYSTEM_SPECIAL_ORION_STAR);
    DOCTEST_CHECK((MOX::star[0].planet_index[1] == 0));

    DOCTEST_CHECK((MOX::player[0].home_planet_id == 1));
    DOCTEST_CHECK(MOX::colony[0].planet == 1);
    DOCTEST_CHECK((MOX::star[2].planet_index[0] == 1));
    DOCTEST_CHECK((MOX::star[2].planet_index[1] == 2));
    DOCTEST_CHECK((MOX::star[3].planet_index[0] == 3));
    DOCTEST_CHECK((MOX::star[1].planet_index[0] == -1));
    DOCTEST_CHECK((MOX::star[1].planet_index[1] == -1));
}

DOCTEST_TEST_CASE("planet trimming counts and removes gas giants at the threshold") {
    for (int16_t limit : {2, 3, 4}) {
        TEST_MAPGEN::Reset_();
        MOX::NUM_STARS = 3;
        MOX::NUM_PLAYERS = 1;
        MAPGEN::special_generation_config.generate_orion = 1;
        TEST_MAPGEN::Set_Planet_(0, 1, 1, PLANET_TYPE_GAS_GIANT, -1);
        TEST_MAPGEN::Set_Planet_(1, 2, 0, PLANET_TYPE_PLANET, 0);
        TEST_MAPGEN::Set_Planet_(2, 2, 1, PLANET_TYPE_ASTEROID, -1);
        MOX::planet_count = 3;
        MOX::player[0].home_planet_id = 1;
        MOX::colony[0].planet = 1;

        MAPGEN::Enforce_Planet_Max(limit);

        const bool trimmed = limit <= 3;
        DOCTEST_CHECK(MOX::planet_count == (trimmed ? 2 : 3));
        DOCTEST_CHECK((MOX::star[1].planet_index[1] == (trimmed ? -1 : 0)));
        DOCTEST_CHECK((MOX::player[0].home_planet_id == (trimmed ? 0 : 1)));
        DOCTEST_CHECK(MOX::colony[0].planet == (trimmed ? 0 : 1));
        DOCTEST_CHECK(TEST_MAPGEN::Random_Calls_() == (trimmed ? 1 : 0));
    }
}

DOCTEST_TEST_CASE("planet trimming retains gas giants in protected systems") {
    TEST_MAPGEN::Reset_();
    MOX::NUM_STARS = 4;
    MOX::NUM_PLAYERS = 1;
    MAPGEN::special_generation_config.generate_orion = 1;
    TEST_MAPGEN::Set_Planet_(0, 3, 1, PLANET_TYPE_GAS_GIANT, -1);
    TEST_MAPGEN::Set_Planet_(1, 0, 1, PLANET_TYPE_GAS_GIANT, -1);
    TEST_MAPGEN::Set_Planet_(2, 1, 0, PLANET_TYPE_PLANET, 0);
    TEST_MAPGEN::Set_Planet_(3, 1, 1, PLANET_TYPE_GAS_GIANT, -1);
    TEST_MAPGEN::Set_Planet_(4, 2, 0, PLANET_TYPE_PLANET, -1);
    TEST_MAPGEN::Set_Planet_(5, 2, 1, PLANET_TYPE_GAS_GIANT, -1);
    MOX::planet_count = 6;
    MOX::player[0].home_planet_id = 2;
    MOX::colony[0].planet = 2;
    TEST_MAPGEN::Set_Food_Star_(2);

    MAPGEN::Enforce_Planet_Max(2);

    DOCTEST_CHECK(MOX::planet_count == 5);
    DOCTEST_CHECK((MOX::star[3].planet_index[1] == -1));
    DOCTEST_CHECK((MOX::star[0].planet_index[1] == 0));
    DOCTEST_CHECK((MOX::star[1].planet_index[1] == 2));
    DOCTEST_CHECK((MOX::star[2].planet_index[1] == 4));
    DOCTEST_CHECK((MOX::player[0].home_planet_id == 1));
    DOCTEST_CHECK(MOX::colony[0].planet == 1);
    DOCTEST_CHECK(TEST_MAPGEN::Random_Calls_() == 1);
}

DOCTEST_TEST_CASE("planet trimming does not protect star zero when Orion is disabled") {
    TEST_MAPGEN::Reset_();
    MOX::NUM_STARS = 2;
    MOX::NUM_PLAYERS = 1;
    MAPGEN::special_generation_config.generate_orion = 0;

    TEST_MAPGEN::Set_Planet_(0, 0, 0, PLANET_TYPE_PLANET, -1);
    TEST_MAPGEN::Set_Planet_(1, 1, 0, PLANET_TYPE_PLANET, 0);
    MOX::planet_count = 2;
    MOX::player[0].home_planet_id = 1;
    MOX::colony[0].planet = 1;

    MAPGEN::Enforce_Planet_Max(2);

    DOCTEST_CHECK(MOX::planet_count == 1);
    DOCTEST_CHECK(MOX::planet[0].star_index == 1);
    DOCTEST_CHECK((MOX::player[0].home_planet_id == 0));
    DOCTEST_CHECK(MOX::colony[0].planet == 0);
    DOCTEST_CHECK((MOX::star[0].planet_index[0] == -1));
    DOCTEST_CHECK((MOX::star[1].planet_index[0] == 0));

    MAPGEN::special_generation_config.generate_orion = 1;
}

DOCTEST_TEST_CASE("configured planet threshold allows crowded classic galaxies") {
    struct s_case {
        int16_t stars;
        int16_t limit;
        int16_t retained;
    };
    const s_case cases[] = {
        {20, 250, 100},
        {49, 250, 245},
        {50, 250, 249},
        {55, 250, 249},
        {55, 260, 259},
        {55, 400, 275},
        {55, 0, 275},
        {72, 250, 249},
        {72, 400, 360},
        {72, 0, 360},
        {73, 250, 365},
    };

    for (const s_case& test_case : cases) {
        TEST_MAPGEN::Reset_();
        MOX::NUM_STARS = test_case.stars;
        MOX::NUM_PLAYERS = 0;
        MAPGEN::planet_limit = test_case.limit;
        MOX::planet_count = test_case.stars * 5;
        for (int16_t planet = 0; planet < MOX::planet_count; ++planet) {
            const int8_t types[] = {
                PLANET_TYPE_PLANET,
                PLANET_TYPE_GAS_GIANT,
                PLANET_TYPE_ASTEROID,
            };
            TEST_MAPGEN::Set_Planet_(
                planet, planet / 5, planet % 5, types[planet % 3], -1);
        }

        MAPGEN::Enforce_Configured_Planet_Max();

        DOCTEST_CHECK(MOX::planet_count == test_case.retained);
        for (int16_t planet = 0; planet < MOX::planet_count; ++planet) {
            const s_planet_data& body = MOX::planet[planet];
            DOCTEST_CHECK(
                (MOX::star[body.star_index].planet_index[body.orbit] == planet));
        }
    }
    MAPGEN::planet_limit = 250;
}

DOCTEST_TEST_CASE("forced food climate retries the configured weighted distribution") {
    TEST_MAPGEN::Reset_();
    MOX::g_galaxy_age = GALAXY_AGE_YOUNG;
    MOX::normal_gal_climate_roll_table[
        PLANET_CLIMATE_TOXIC][PLANET_GROUP_LIFE_SUPPORTING] = 1;
    MOX::normal_gal_climate_roll_table[
        PLANET_CLIMATE_TERRAN][PLANET_GROUP_LIFE_SUPPORTING] = 1;
    MOX::normal_gal_climate_roll_table[
        PLANET_CLIMATE_GAIA][PLANET_GROUP_LIFE_SUPPORTING] = 1;
    MOX::food_per_farmer_table[PLANET_CLIMATE_TERRAN] = 2;
    MOX::food_per_farmer_table[PLANET_CLIMATE_GAIA] = 2;
    MAPGEN::make_food_planet = 1;
    const int16_t choices[] = {
        PLANET_CLIMATE_TOXIC,
        PLANET_CLIMATE_TERRAN,
    };
    TEST_MAPGEN::Set_Weighted_Choices_(choices, 2);

    DOCTEST_CHECK(MAPGEN::Generate_Climate(PLANET_GROUP_LIFE_SUPPORTING) ==
                  PLANET_CLIMATE_TERRAN);
    DOCTEST_CHECK(TEST_MAPGEN::Weighted_Choice_Count_() == 2);
}

DOCTEST_TEST_CASE("forced food climate uses its unique configured choice directly") {
    for (int group = 0; group < PLANET_GROUP_COUNT; ++group) {
        TEST_MAPGEN::Reset_();
        MOX::g_galaxy_age = GALAXY_AGE_AVERAGE;
        MOX::normal_gal_climate_roll_table[
            PLANET_CLIMATE_GAIA][group] = UINT8_MAX;
        MOX::food_per_farmer_table[PLANET_CLIMATE_GAIA] = UINT8_MAX;
        MAPGEN::make_food_planet = 1;

        DOCTEST_CHECK(MAPGEN::Generate_Climate(group) == PLANET_CLIMATE_GAIA);
        DOCTEST_CHECK(TEST_MAPGEN::Weighted_Choice_Count_() == 0);
    }
}

DOCTEST_TEST_CASE("mineral generation uses the one-based random roll") {
    TEST_MAPGEN::Reset_();
    MOX::star[0].spectral_class = STAR_CLASS_F;
    MAPGEN::mineral_class[9][STAR_CLASS_F] = MINERAL_RESOURCE_RICH;
    TEST_MAPGEN::Set_Random_Result_(10);

    DOCTEST_CHECK(MAPGEN::Generate_Mineral_Class(0, 0) ==
                  MINERAL_RESOURCE_RICH);
}

DOCTEST_TEST_CASE("planet-special selection ignores no-special and keeps unused retries") {
    TEST_MAPGEN::Reset_();
    MOX::planet_special_weighted_chance[0] = 255;
    MOX::planet_special_weighted_chance[SYSTEM_SPECIAL_SPACE_DEBRIS] = 128;
    MOX::planet_special_weighted_chance[SYSTEM_SPECIAL_ORION_STAR] = 255;
    const int16_t choices[] = {
        SYSTEM_SPECIAL_ORION_STAR,
        SYSTEM_SPECIAL_SPACE_DEBRIS,
    };
    TEST_MAPGEN::Set_Weighted_Choices_(choices, 2);

    int16_t wormhole = 0;
    int8_t special = SYSTEM_SPECIAL_NO_SPECIAL;
    MAPGEN::Get_A_Planet_Or_Star_Special(
        0, &wormhole, &special, 0, 0);

    const uint16_t* weights = TEST_MAPGEN::Last_Short_Weights_();
    DOCTEST_CHECK(weights[SYSTEM_SPECIAL_NO_SPECIAL] == 0);
    DOCTEST_CHECK(weights[SYSTEM_SPECIAL_SPACE_DEBRIS] == 128);
    DOCTEST_CHECK(weights[SYSTEM_SPECIAL_ORION_STAR] == 255);
    DOCTEST_CHECK(TEST_MAPGEN::Weighted_Choice_Count_() == 2);
    DOCTEST_CHECK(special == SYSTEM_SPECIAL_SPACE_DEBRIS);
}

DOCTEST_TEST_CASE("Marooned Hero eligibility follows galaxy size instead of local zoom") {
    struct s_case {
        int8_t galaxy_size;
        int16_t maximum_heroes;
    };
    const s_case cases[] = {
        {GALAXY_SIZE_SMALL, 2},
        {GALAXY_SIZE_MEDIUM, 3},
        {GALAXY_SIZE_LARGE, 4},
        {GALAXY_SIZE_HUGE, 5},
        {GALAXY_SIZE_MAXIMUM, 5},
        {GALAXY_SIZE_CLUSTER, 5},
    };

    for (const s_case& test_case : cases) {
        TEST_MAPGEN::Reset_();
        MOX::settings.galaxy_size = test_case.galaxy_size;
        MOX::zoom_count = 15;
        MOX::planet_special_weighted_chance[SYSTEM_SPECIAL_MAROONED_HERO] = 37;
        MAPGEN::n_marooned_heroes = test_case.maximum_heroes + 1;
        const int16_t choice = SYSTEM_SPECIAL_SPACE_DEBRIS;
        TEST_MAPGEN::Set_Weighted_Choices_(&choice, 1);

        int16_t wormhole = 0;
        int8_t special = SYSTEM_SPECIAL_NO_SPECIAL;
        MAPGEN::Get_A_Planet_Or_Star_Special(
            0, &wormhole, &special, 0, 0);

        DOCTEST_CHECK(TEST_MAPGEN::Last_Short_Weights_()
                          [SYSTEM_SPECIAL_MAROONED_HERO] == 0);

        MAPGEN::n_marooned_heroes = test_case.maximum_heroes;
        TEST_MAPGEN::Set_Weighted_Choices_(&choice, 1);
        MAPGEN::Get_A_Planet_Or_Star_Special(
            0, &wormhole, &special, 0, 0);

        DOCTEST_CHECK(TEST_MAPGEN::Last_Short_Weights_()
                          [SYSTEM_SPECIAL_MAROONED_HERO] == 37);
    }
}

DOCTEST_TEST_CASE("star-special gate uses its independent threshold") {
    TEST_MAPGEN::Reset_();
    MOX::settings.game_difficulty = GAME_DIFFICULTY_AVERAGE;
    MAPGEN::monster_generation_config.natural_chance[GAME_DIFFICULTY_AVERAGE] = 0;
    MOX::planet_special = 64;
    MOX::planet_special_weighted_chance[0] = 255;
    const int16_t choices[] = {SYSTEM_SPECIAL_SPACE_DEBRIS};
    TEST_MAPGEN::Set_Weighted_Choices_(choices, 1);

    int16_t wormhole = 0;
    uint8_t counts[SYSTEM_SPECIAL_COUNT] = {};
    TEST_MAPGEN::Set_Random_Result_(64);
    MAPGEN::Generate_Star_Special(0, &wormhole, counts);
    DOCTEST_CHECK(TEST_MAPGEN::Weighted_Choice_Count_() == 0);
    DOCTEST_CHECK(counts[SYSTEM_SPECIAL_NO_SPECIAL] == 1);

    TEST_MAPGEN::Set_Random_Result_(65);
    MAPGEN::Generate_Star_Special(0, &wormhole, counts);
    DOCTEST_CHECK(TEST_MAPGEN::Weighted_Choice_Count_() == 1);
    DOCTEST_CHECK(counts[SYSTEM_SPECIAL_SPACE_DEBRIS] == 1);
}

DOCTEST_TEST_CASE("planet generated event applies after native transformations") {
    TEST_MAPGEN::Reset_();
    TEST_MAPGEN::Enable_Deposit_Rules_();
    MOX::NUM_STARS = PLANET_SIZE_COUNT;
    MOX::planet_count = PLANET_SIZE_COUNT;
    for (int16_t size = PLANET_SIZE_TINY; size < PLANET_SIZE_COUNT;
         ++size) {
        TEST_MAPGEN::Set_Planet_(
            size, size, 0, PLANET_TYPE_PLANET, -1);
        s_planet_data& planet = MOX::planet[size];
        planet.size = static_cast<int8_t>(size);
        planet.gravity_class = PLANET_GRAVITY_LOW_G;
        planet.climate = PLANET_CLIMATE_BARREN;
        planet.mineral_class = MINERAL_RESOURCE_POOR;
        planet.planet_special = size % 2 == 0
            ? SYSTEM_SPECIAL_GOLD_DEPOSITS
            : SYSTEM_SPECIAL_GEM_DEPOSITS;
        MOX::star[size].system_special = planet.planet_special;
    }

    DOCTEST_CHECK_FALSE(MAPGEN::Finalize_Configured_New_Game());

    const int16_t expected_minerals[PLANET_SIZE_COUNT] = {
        MINERAL_RESOURCE_RICH,
        MINERAL_RESOURCE_ABUNDANT,
        MINERAL_RESOURCE_ABUNDANT,
        MINERAL_RESOURCE_ABUNDANT,
        MINERAL_RESOURCE_POOR,
    };
    for (int16_t size = PLANET_SIZE_TINY; size < PLANET_SIZE_COUNT;
         ++size) {
        DOCTEST_CHECK(MOX::planet[size].mineral_class ==
            expected_minerals[size]);
        DOCTEST_CHECK(MOX::planet[size].gravity_class ==
            PLANET_GRAVITY_NORMAL_G);
        DOCTEST_CHECK(TEST_MAPGEN::Generated_Planet_Event_Planet_(size) ==
            size);
    }
    DOCTEST_CHECK(TEST_MAPGEN::Generated_Planet_Event_Calls_() ==
        PLANET_SIZE_COUNT);
}

DOCTEST_TEST_CASE(
    "planet generated event observes finalized VDC home-system edits") {
    TEST_MAPGEN::Reset_();
    TEST_MAPGEN::Enable_Home_System_Edits_();
    TEST_MAPGEN::Enable_Deposit_Rules_();
    MOX::NUM_STARS = 1;
    MOX::NUM_PLAYERS = 1;
    MOX::planet_count = 1;
    TEST_MAPGEN::Set_Planet_(0, 0, 0, PLANET_TYPE_PLANET, -1);
    MOX::player[0].home_planet_id = 0;
    MOX::planet[0].size = PLANET_SIZE_MEDIUM;
    MOX::planet[0].gravity_class = PLANET_GRAVITY_LOW_G;
    MOX::planet[0].climate = PLANET_CLIMATE_BARREN;
    MOX::planet[0].mineral_class = MINERAL_RESOURCE_POOR;
    MOX::planet_max_farms[PLANET_SIZE_HUGE] = 7;
    MOX::food_per_farmer_table[PLANET_CLIMATE_GAIA] = 6;

    DOCTEST_CHECK_FALSE(MAPGEN::Finalize_Configured_New_Game());

    DOCTEST_CHECK(TEST_MAPGEN::Home_System_Event_Calls_() == 1);
    DOCTEST_CHECK(TEST_MAPGEN::First_Generated_Planet_Event_Order_() == 1);
    REQUIRE(TEST_MAPGEN::Generated_Planet_Event_Calls_() == 1);
    DOCTEST_CHECK(TEST_MAPGEN::Generated_Event_Observed_Home_System_());
    DOCTEST_CHECK(MOX::planet[0].size == PLANET_SIZE_HUGE);
    DOCTEST_CHECK(MOX::planet[0].climate == PLANET_CLIMATE_GAIA);
    DOCTEST_CHECK(MOX::planet[0].mineral_class == MINERAL_RESOURCE_POOR);
    DOCTEST_CHECK(MOX::planet[0].gravity_class == PLANET_GRAVITY_NORMAL_G);
    DOCTEST_CHECK(MOX::planet[0].food_per_farmer == 6);
    DOCTEST_CHECK(MOX::planet[0].max_farms == 7);
    DOCTEST_CHECK(MOX::planet[0].planet_special ==
        SYSTEM_SPECIAL_GOLD_DEPOSITS);
    DOCTEST_CHECK(MOX::star[0].system_special ==
        SYSTEM_SPECIAL_GOLD_DEPOSITS);
}

DOCTEST_TEST_CASE("finalization preserves soil enrichment after planet refresh") {
    TEST_MAPGEN::Reset_();
    MOX::NUM_STARS = 1;
    MOX::planet_count = 2;
    TEST_MAPGEN::Set_Planet_(0, 0, 0, PLANET_TYPE_PLANET, -1);
    TEST_MAPGEN::Set_Planet_(1, 0, 1, PLANET_TYPE_PLANET, -1);
    MOX::food_per_farmer_table[PLANET_CLIMATE_TERRAN] = 4;
    for (int16_t planet_idx = 0; planet_idx < MOX::planet_count; ++planet_idx) {
        MOX::planet[planet_idx].size = PLANET_SIZE_MEDIUM;
        MOX::planet[planet_idx].climate = PLANET_CLIMATE_TERRAN;
        MOX::planet[planet_idx].mineral_class = MINERAL_RESOURCE_ABUNDANT;
        MOX::planet[planet_idx].gravity_class = PLANET_GRAVITY_NORMAL_G;
    }
    MOX::planet[0].environmental_alterations = 0x2;
    MOX::planet[0].food_per_farmer = 6;
    MOX::planet[1].food_per_farmer = 4;

    DOCTEST_CHECK_FALSE(MAPGEN::Finalize_Configured_New_Game());
    DOCTEST_CHECK(MOX::planet[0].food_per_farmer == 6);
    DOCTEST_CHECK(MOX::planet[1].food_per_farmer == 4);
    DOCTEST_CHECK(MOX::planet[0].environmental_alterations == 0x2);
}

DOCTEST_TEST_CASE("soil enrichment survives a home-system climate change") {
    TEST_MAPGEN::Reset_();
    TEST_MAPGEN::Enable_Home_System_Edits_();
    MOX::NUM_STARS = 1;
    MOX::NUM_PLAYERS = 1;
    MOX::planet_count = 1;
    TEST_MAPGEN::Set_Planet_(0, 0, 0, PLANET_TYPE_PLANET, -1);
    MOX::player[0].home_planet_id = 0;
    MOX::planet[0].size = PLANET_SIZE_MEDIUM;
    MOX::planet[0].climate = PLANET_CLIMATE_TERRAN;
    MOX::planet[0].mineral_class = MINERAL_RESOURCE_ABUNDANT;
    MOX::planet[0].gravity_class = PLANET_GRAVITY_NORMAL_G;
    MOX::planet[0].environmental_alterations = 0x2;
    MOX::planet[0].food_per_farmer = 6;
    MOX::food_per_farmer_table[PLANET_CLIMATE_TERRAN] = 4;
    MOX::food_per_farmer_table[PLANET_CLIMATE_GAIA] = 6;

    DOCTEST_CHECK_FALSE(MAPGEN::Finalize_Configured_New_Game());
    DOCTEST_CHECK(MOX::planet[0].climate == PLANET_CLIMATE_GAIA);
    DOCTEST_CHECK(MOX::planet[0].food_per_farmer == 8);
    DOCTEST_CHECK(MOX::planet[0].environmental_alterations == 0x2);
}

DOCTEST_TEST_CASE("galaxy generator fallback leaves native state untouched") {
    TEST_MAPGEN::Reset_();
    Set_Valid_Galaxy_Candidate_();
    const s_galaxy_snapshot before = Capture_Galaxy_();
    std::string diagnostic = "stale";

    DOCTEST_REQUIRE(MAPGEN::Apply_Lua_Galaxy_Generator(&diagnostic));

    DOCTEST_CHECK(diagnostic.empty());
    DOCTEST_CHECK(TEST_MAPGEN::Galaxy_Provider_Calls_() == 0);
    DOCTEST_CHECK(TEST_MAPGEN::Random_Calls_() == 0);
    Check_Galaxy_Equals_(before);
}

DOCTEST_TEST_CASE(
    "network host runs galaxy generator and clients await transferred state") {
    TEST_MAPGEN::Reset_();
    Set_Valid_Galaxy_Candidate_();
    TEST_MAPGEN::Enable_Galaxy_Provider_(false, false);
    MOX::food_per_farmer_table[PLANET_CLIMATE_GAIA] = 6;
    MOX::planet_max_farms[PLANET_SIZE_MEDIUM] = 7;
    MOX::game_type = 2;
    std::string diagnostic;

    DOCTEST_REQUIRE(MAPGEN::Apply_Lua_Galaxy_Generator(&diagnostic));

    DOCTEST_CHECK(TEST_MAPGEN::Galaxy_Provider_Calls_() == 1);
    DOCTEST_CHECK(TEST_MAPGEN::Random_Calls_() == 1);
    DOCTEST_CHECK((MOX::star[0].x == 77));
    DOCTEST_CHECK(MOX::planet[0].climate == PLANET_CLIMATE_GAIA);

    TEST_MAPGEN::Reset_();
    Set_Valid_Galaxy_Candidate_();
    TEST_MAPGEN::Enable_Galaxy_Provider_(false, false);
    MOX::game_type = 3;
    const s_galaxy_snapshot client_before_transfer = Capture_Galaxy_();

    DOCTEST_REQUIRE(MAPGEN::Apply_Lua_Galaxy_Generator(&diagnostic));

    DOCTEST_CHECK(TEST_MAPGEN::Galaxy_Provider_Calls_() == 0);
    DOCTEST_CHECK(TEST_MAPGEN::Random_Calls_() == 0);
    Check_Galaxy_Equals_(client_before_transfer);
}

DOCTEST_TEST_CASE(
    "galaxy generator commits a validated isolated candidate atomically") {
    TEST_MAPGEN::Reset_();
    Set_Valid_Galaxy_Candidate_();
    TEST_MAPGEN::Enable_Galaxy_Provider_(false, false);
    MOX::food_per_farmer_table[PLANET_CLIMATE_GAIA] = 6;
    MOX::planet_max_farms[PLANET_SIZE_MEDIUM] = 7;
    s_star_data* live_stars = MOX::star;
    s_planet_data* live_planets = MOX::planet;
    s_galaxy_snapshot expected = Capture_Galaxy_();
    expected.stars[0].x = 77;
    expected.planets[0].climate = PLANET_CLIMATE_GAIA;
    expected.planets[0].food_per_farmer = 6;
    expected.planets[0].max_farms = 7;
    expected.topology_generation += 2;
    for (uint64_t& generation : expected.ship_generations) {
        generation = generation == 0 ? 2 : generation + 2;
    }
    std::string diagnostic;

    DOCTEST_REQUIRE(MAPGEN::Apply_Lua_Galaxy_Generator(&diagnostic));

    DOCTEST_CHECK(TEST_MAPGEN::Galaxy_Provider_Calls_() == 1);
    DOCTEST_CHECK(TEST_MAPGEN::Galaxy_Provider_Isolated_());
    DOCTEST_CHECK(MOX::star == live_stars);
    DOCTEST_CHECK(MOX::planet == live_planets);
    DOCTEST_CHECK(TEST_MAPGEN::Galaxy_Provider_Topology_Generation_() ==
        expected.topology_generation - 1);
    DOCTEST_CHECK(TEST_MAPGEN::Galaxy_Provider_Ship_Generation_() ==
        expected.ship_generations[0] - 1);
    DOCTEST_CHECK(TEST_MAPGEN::Random_Calls_() == 1);
    Check_Galaxy_Equals_(expected);
}

DOCTEST_TEST_CASE(
    "galaxy generator failure discards its candidate after consuming RNG") {
    TEST_MAPGEN::Reset_();
    Set_Valid_Galaxy_Candidate_();
    TEST_MAPGEN::Set_Random_Result_(40);
    TEST_MAPGEN::Enable_Galaxy_Provider_(true, false);
    const s_galaxy_snapshot before = Capture_Galaxy_();
    std::string diagnostic;

    DOCTEST_CHECK_FALSE(MAPGEN::Apply_Lua_Galaxy_Generator(&diagnostic));

    DOCTEST_CHECK(diagnostic == "provider failed");
    DOCTEST_CHECK(TEST_MAPGEN::Galaxy_Provider_Topology_Generation_() ==
        before.topology_generation + 1);
    DOCTEST_CHECK(TEST_MAPGEN::Galaxy_Provider_Ship_Generation_() ==
        before.ship_generations[0] + 1);
    Check_Galaxy_Equals_(before);
    DOCTEST_CHECK(TEST_MAPGEN::Random_Calls_() == 1);
}

DOCTEST_TEST_CASE("galaxy generator validation rejects invalid candidate") {
    TEST_MAPGEN::Reset_();
    Set_Valid_Galaxy_Candidate_();
    TEST_MAPGEN::Enable_Galaxy_Provider_(false, true);
    const s_galaxy_snapshot before = Capture_Galaxy_();
    std::string diagnostic;

    DOCTEST_CHECK_FALSE(MAPGEN::Apply_Lua_Galaxy_Generator(&diagnostic));

    DOCTEST_CHECK(diagnostic.find("invalid construction state") !=
        std::string::npos);
    DOCTEST_CHECK(TEST_MAPGEN::Random_Calls_() == 1);
    Check_Galaxy_Equals_(before);
}

DOCTEST_TEST_CASE("home-system events dispatch in ascending player order") {
    TEST_MAPGEN::Reset_();
    MOX::NUM_STARS = 2;
    MOX::NUM_PLAYERS = 2;
    MOX::planet_count = 2;
    for (int16_t player_idx = 0; player_idx < MOX::NUM_PLAYERS;
         ++player_idx) {
        TEST_MAPGEN::Set_Planet_(
            player_idx, player_idx, 0, PLANET_TYPE_PLANET, -1);
        MOX::player[player_idx].home_planet_id = player_idx;
        MOX::planet[player_idx].size = PLANET_SIZE_MEDIUM;
        MOX::planet[player_idx].gravity_class = PLANET_GRAVITY_NORMAL_G;
        MOX::planet[player_idx].climate = PLANET_CLIMATE_TERRAN;
        MOX::planet[player_idx].mineral_class = MINERAL_RESOURCE_ABUNDANT;
    }

    DOCTEST_CHECK_FALSE(MAPGEN::Finalize_Configured_New_Game());

    REQUIRE(TEST_MAPGEN::Home_System_Event_Calls_() == 2);
    DOCTEST_CHECK(TEST_MAPGEN::Home_System_Event_Player_(0) == 0);
    DOCTEST_CHECK(TEST_MAPGEN::Home_System_Event_Player_(1) == 1);
}

DOCTEST_TEST_CASE("planet generated event failure leaves native planets unchanged") {
    TEST_MAPGEN::Reset_();
    TEST_MAPGEN::Enable_Home_System_Edits_();
    TEST_MAPGEN::Enable_Deposit_Rules_();
    TEST_MAPGEN::Fail_Generated_Event_For_Planet_(1);
    MOX::NUM_STARS = 1;
    MOX::NUM_PLAYERS = 1;
    MOX::planet_count = 2;
    for (int16_t planet_idx = 0; planet_idx < MOX::planet_count;
         ++planet_idx) {
        TEST_MAPGEN::Set_Planet_(
            planet_idx, 0, planet_idx, PLANET_TYPE_PLANET, -1);
        s_planet_data& planet = MOX::planet[planet_idx];
        planet.size = PLANET_SIZE_SMALL;
        planet.gravity_class = PLANET_GRAVITY_LOW_G;
        planet.climate = PLANET_CLIMATE_BARREN;
        planet.mineral_class = MINERAL_RESOURCE_POOR;
        planet.planet_special = planet_idx == 0
            ? SYSTEM_SPECIAL_GOLD_DEPOSITS
            : SYSTEM_SPECIAL_NO_SPECIAL;
    }
    MOX::player[0].home_planet_id = 0;
    MOX::star[0].system_special = SYSTEM_SPECIAL_GOLD_DEPOSITS;

    DOCTEST_CHECK_THROWS_AS(
        MAPGEN::Finalize_Configured_New_Game(), std::runtime_error);

    for (int16_t planet_idx = 0; planet_idx < MOX::planet_count;
         ++planet_idx) {
        DOCTEST_CHECK(MOX::planet[planet_idx].mineral_class ==
            MINERAL_RESOURCE_POOR);
        DOCTEST_CHECK(MOX::planet[planet_idx].gravity_class ==
            PLANET_GRAVITY_LOW_G);
        DOCTEST_CHECK(MOX::planet[planet_idx].size == PLANET_SIZE_SMALL);
        DOCTEST_CHECK(MOX::planet[planet_idx].climate ==
            PLANET_CLIMATE_BARREN);
        DOCTEST_CHECK(MOX::planet[planet_idx].planet_special ==
            (planet_idx == 0 ? SYSTEM_SPECIAL_GOLD_DEPOSITS
                             : SYSTEM_SPECIAL_NO_SPECIAL));
    }
    DOCTEST_CHECK(MOX::star[0].system_special ==
        SYSTEM_SPECIAL_GOLD_DEPOSITS);
}