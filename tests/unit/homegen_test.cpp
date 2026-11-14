#include "pch.h"

#include "doctest/doctest.h"

namespace TEST_HOMEGEN {
    void Reset_();
    int32_t Satellite_Generation_Count_();
    void Set_Satellite_Count_(int16_t satellite_count);
    int32_t Normal_Planet_Generation_Count_();
    void Set_Generated_Spectral_Class_(int8_t spectral_class);
    int32_t Non_Black_Hole_Generation_Count_();
    int32_t Special_Generation_Count_();
    int32_t Spectral_Generation_Count_();
}

DOCTEST_TEST_CASE("home-world distances use the real sorter for more than two candidates") {
    HOMEGEN::s_star_dist candidates[] = {{20, 1}, {4, 2}, {12, 3}};
    Legacy_Qsort(candidates, 3, sizeof(candidates[0]), HOMEGEN::qsort_star_dist_list);
    DOCTEST_CHECK((candidates[0].index == 2));
    DOCTEST_CHECK((candidates[1].index == 3));
    DOCTEST_CHECK((candidates[2].index == 1));
}

DOCTEST_TEST_CASE("minimum-distance planet generation preserves existing satellite records") {
    TEST_HOMEGEN::Reset_();
    int16_t home_stars[1] = {0};

    HOMEGEN::Guarantee_A_Planet_At_Min_Dist(0, 4, home_stars);

    DOCTEST_CHECK(MOX::planet_count == 2);
    DOCTEST_CHECK((MOX::star[1].planet_index[2] == 0));
    DOCTEST_CHECK(MOX::planet[0].star_index == 1);
    DOCTEST_CHECK(MOX::planet[0].planet_type == PLANET_TYPE_PLANET);
    DOCTEST_CHECK(TEST_HOMEGEN::Satellite_Generation_Count_() == 2);
    DOCTEST_CHECK(TEST_HOMEGEN::Non_Black_Hole_Generation_Count_() == 0);
}

DOCTEST_TEST_CASE("minimum-distance generation converts black holes through galaxy-age spectral generation") {
    TEST_HOMEGEN::Reset_();
    TEST_HOMEGEN::Set_Generated_Spectral_Class_(STAR_CLASS_G);
    MOX::star[1].spectral_class = STAR_CLASS_BLACK_HOLE;
    int16_t home_stars[1] = {0};

    HOMEGEN::Guarantee_A_Planet_At_Min_Dist(0, 4, home_stars);

    DOCTEST_CHECK(MOX::star[1].spectral_class == STAR_CLASS_G);
    DOCTEST_CHECK(MOX::planet_count == 2);
    DOCTEST_CHECK(TEST_HOMEGEN::Satellite_Generation_Count_() == 2);
    DOCTEST_CHECK(TEST_HOMEGEN::Non_Black_Hole_Generation_Count_() == 1);
}

DOCTEST_TEST_CASE("minimum-distance conversion runs configured generation before its planet guarantee") {
    TEST_HOMEGEN::Reset_();
    TEST_HOMEGEN::Set_Satellite_Count_(1);
    MOX::star[1].spectral_class = STAR_CLASS_BLACK_HOLE;
    int16_t home_stars[1] = {0};

    HOMEGEN::Guarantee_A_Planet_At_Min_Dist(0, 4, home_stars);

    DOCTEST_CHECK(TEST_HOMEGEN::Normal_Planet_Generation_Count_() == 1);
    DOCTEST_CHECK(TEST_HOMEGEN::Satellite_Generation_Count_() == 3);
    DOCTEST_CHECK(MOX::planet_count == 3);
    DOCTEST_CHECK((MOX::star[1].planet_index[2] == 0));
    DOCTEST_CHECK(MOX::planet[0].planet_type == PLANET_TYPE_PLANET);
    DOCTEST_CHECK(HAROLD::N_Planets_In_System(1) == 2);
}

DOCTEST_TEST_CASE("minimum-distance planet guarantee promotes brown dwarfs to red stars") {
    TEST_HOMEGEN::Reset_();
    MOX::star[1].spectral_class = STAR_CLASS_DWARF;
    int16_t home_stars[1] = {0};

    DOCTEST_SUBCASE("existing brown dwarf") {
    }
    DOCTEST_SUBCASE("black hole rerolled as a brown dwarf") {
        MOX::star[1].spectral_class = STAR_CLASS_BLACK_HOLE;
        TEST_HOMEGEN::Set_Generated_Spectral_Class_(STAR_CLASS_DWARF);
        TEST_HOMEGEN::Set_Satellite_Count_(1);
    }

    HOMEGEN::Guarantee_A_Planet_At_Min_Dist(0, 4, home_stars);

    DOCTEST_CHECK(MOX::star[1].spectral_class == STAR_CLASS_M);
    DOCTEST_CHECK(HAROLD::N_Planets_In_System(1) == 2);
}

DOCTEST_TEST_CASE("minimum-distance planet guarantee preserves normal star classes") {
    for (int8_t spectral_class = STAR_CLASS_B; spectral_class <= STAR_CLASS_M; ++spectral_class) {
        TEST_HOMEGEN::Reset_();
        MOX::star[1].spectral_class = spectral_class;
        int16_t home_stars[1] = {0};

        HOMEGEN::Guarantee_A_Planet_At_Min_Dist(0, 4, home_stars);

        DOCTEST_CHECK(MOX::star[1].spectral_class == spectral_class);
        DOCTEST_CHECK(HAROLD::N_Planets_In_System(1) == 2);
    }
}

DOCTEST_TEST_CASE("food-world generation rerolls unsuitable stars without allowing black holes") {
    TEST_HOMEGEN::Reset_();
    TEST_HOMEGEN::Set_Generated_Spectral_Class_(STAR_CLASS_K);
    MOX::star[0].spectral_class = STAR_CLASS_DWARF;

    HOMEGEN::Make_One_Planet(0, 1, 0, 1);

    DOCTEST_CHECK(MOX::star[0].spectral_class == STAR_CLASS_K);
    DOCTEST_CHECK((MOX::star[0].planet_index[0] == 1));
    DOCTEST_CHECK(TEST_HOMEGEN::Non_Black_Hole_Generation_Count_() == 1);
}

DOCTEST_TEST_CASE("home-system minimum follows map configuration") {
    TEST_HOMEGEN::Reset_();
    MAPGEN::home_system_config.minimum_planets = 5;
    MOX::star[0].spectral_class = STAR_CLASS_G;
    MOX::star[0].planet_index[0] = 0;
    MOX::planet_count = 1;
    const int32_t generation_count = TEST_HOMEGEN::Satellite_Generation_Count_();

    HOMEGEN::Enforce_3_Planet_Minimum(0);

    DOCTEST_CHECK(MOX::planet_count == 5);
    DOCTEST_CHECK(TEST_HOMEGEN::Satellite_Generation_Count_() - generation_count == 5);
    MAPGEN::home_system_config.minimum_planets = 3;
}

DOCTEST_TEST_CASE("habitable home-system minimum skips ordinary generation") {
    TEST_HOMEGEN::Reset_();
    TEST_HOMEGEN::Set_Satellite_Count_(5);
    MAPGEN::home_system_config.minimum_planets = 3;
    MOX::star[0].spectral_class = STAR_CLASS_G;

    HOMEGEN::Enforce_3_Planet_Minimum(0);

    DOCTEST_CHECK(TEST_HOMEGEN::Normal_Planet_Generation_Count_() == 0);
    DOCTEST_CHECK(TEST_HOMEGEN::Satellite_Generation_Count_() == 3);
    DOCTEST_CHECK(HAROLD::N_Planets_In_System(0) == 3);
    DOCTEST_CHECK(MOX::planet_count == 4);
    DOCTEST_CHECK((MOX::star[1].planet_index[2] == 0));
    DOCTEST_CHECK(MOX::planet[0].planet_type == PLANET_TYPE_ASTEROID);
}

DOCTEST_TEST_CASE("converted home systems run configured ordinary generation") {
    TEST_HOMEGEN::Reset_();
    TEST_HOMEGEN::Set_Satellite_Count_(1);
    MAPGEN::home_system_config.minimum_planets = 3;
    MOX::star[0].spectral_class = STAR_CLASS_BLACK_HOLE;

    HOMEGEN::Enforce_3_Planet_Minimum(0);

    DOCTEST_CHECK(TEST_HOMEGEN::Normal_Planet_Generation_Count_() == 1);
    DOCTEST_CHECK(TEST_HOMEGEN::Satellite_Generation_Count_() == 4);
    DOCTEST_CHECK(HAROLD::N_Planets_In_System(0) == 3);
}

DOCTEST_TEST_CASE("guarded systems stop when configured specials have no valid choice") {
    TEST_HOMEGEN::Reset_();

    HOMEGEN::Set_Special_For_Star_Guarded_By_Monster(&MOX::star[0], 10);

    DOCTEST_CHECK(TEST_HOMEGEN::Special_Generation_Count_() == 1);
    DOCTEST_CHECK(MOX::star[0].system_special == SYSTEM_SPECIAL_NO_SPECIAL);
}

DOCTEST_TEST_CASE("mineral richness exhaustion selects a compatible configured class") {
    TEST_HOMEGEN::Reset_();
    TEST_HOMEGEN::Set_Generated_Spectral_Class_(STAR_CLASS_DWARF);
    MOX::settings.galaxy_age = GALAXY_AGE_YOUNG;
    MOX::star_class_table[STAR_CLASS_B][GALAXY_AGE_YOUNG] = 1;
    MAPGEN::mineral_class[0][STAR_CLASS_B] = MINERAL_RESOURCE_RICH;
    MAPGEN::planet_gravity[MINERAL_RESOURCE_RICH][PLANET_SIZE_MEDIUM] =
        PLANET_GRAVITY_HEAVY_G;
    MOX::star[0].spectral_class = STAR_CLASS_DWARF;
    MOX::planet[0].star_index = 0;
    MOX::planet[0].size = PLANET_SIZE_MEDIUM;

    HOMEGEN::Enforce_Mineral_Richness(0, MINERAL_RESOURCE_RICH);

    DOCTEST_CHECK(TEST_HOMEGEN::Spectral_Generation_Count_() == 200);
    DOCTEST_CHECK(MOX::star[0].spectral_class == STAR_CLASS_B);
    DOCTEST_CHECK(MOX::planet[0].mineral_class == MINERAL_RESOURCE_RICH);
    DOCTEST_CHECK(MOX::planet[0].gravity_class == PLANET_GRAVITY_HEAVY_G);
}