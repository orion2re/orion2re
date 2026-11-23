#include "pch.h"

#include "doctest/doctest.h"
#include "game/legacy_ids.h"

using game::id_ship;
using legacy_world::Ship_Id;
using legacy_world::Star_Id;

namespace {
    struct s_random_seed_scope {
        uint32_t original = game_random::Get_Random_Seed();

        ~s_random_seed_scope() {
            game_random::Set_Random_Seed(original);
        }
    };
} // namespace

DOCTEST_TEST_CASE("global RNG preserves classic draws and seed advancement") {
    s_random_seed_scope scope;
    game_random::Set_Random_Seed(0);
    const uint32_t expected[] = {1, 828, 653, 838, 54, 759};
    DOCTEST_CHECK(game_random::Random(0) == 1);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == 0);
    for (uint32_t value : expected) DOCTEST_CHECK(game_random::Random(1000) == value);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == 3256818826u);
}

DOCTEST_TEST_CASE("weighted choices use the global RNG with explicit absence and fixed-width positions") {
    s_random_seed_scope scope;
    game_random::Set_Random_Seed(0);
    const uint16_t zero[] = {0, 0};
    DOCTEST_CHECK_FALSE(RUSS::Choose_Weighted_Index_U16(zero, 2));
    DOCTEST_CHECK(game_random::Get_Random_Seed() == 0);

    const uint16_t weights[] = {0, 5, 0, 7};
    DOCTEST_CHECK(RUSS::Choose_Weighted_Index_U16(weights, 4) == 1);
    DOCTEST_CHECK(RUSS::Choose_Weighted_Index_U16(weights, 4) == 3);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == 3554416254u);

    std::vector<uint16_t> large(40000);
    large.back() = 1;
    DOCTEST_CHECK(RUSS::Choose_Weighted_Index_U16(large.data(), 40000) == 39999);
}

DOCTEST_TEST_CASE("typed ship ID shuffling keeps the legacy permutation and RNG consumption") {
    s_random_seed_scope scope;
    int16_t old_ids[] = {0, 1, 2, 3, 4, 5};
    id_ship ids[] = {Ship_Id(0), Ship_Id(1), Ship_Id(2), Ship_Id(3), Ship_Id(4), Ship_Id(5)};
    const uint32_t expected[] = {4, 0, 1, 5, 3, 2};
    game_random::Set_Random_Seed(42);
    RUSS::Shuffle_Sint(old_ids, 6);
    const uint32_t legacy_seed = game_random::Get_Random_Seed();
    game_random::Set_Random_Seed(42);
    game_random::Shuffle<game::id_ship>(ids);
    for (uint32_t i = 0; i < 6; ++i) {
        DOCTEST_CHECK(ids[i] == Ship_Id(expected[i]));
        DOCTEST_CHECK(ids[i] == Ship_Id(old_ids[i]));
    }
    DOCTEST_CHECK(game_random::Get_Random_Seed() == legacy_seed);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == 2219150180u);
}

DOCTEST_TEST_CASE("weighted choice helpers and legacy calls share the global RNG stream") {
    s_random_seed_scope scope;
    game_random::Set_Random_Seed(0);
    uint8_t bytes[] = {0, 5, 0, 7};
    uint16_t shorts[] = {0, 5, 0, 7};
    uint32_t ints[] = {0, 5, 0, 7};
    DOCTEST_CHECK(RUSS::Choose_Weighted_Index_U8(bytes, 4) == 1);
    DOCTEST_CHECK(RUSS::Choose_Weighted_Index_U32(ints, 4) == 3);
    const uint32_t seed = game_random::Get_Random_Seed();

    game_random::Set_Random_Seed(0);
    DOCTEST_CHECK(RUSS::Get_Weighted_Choice_Char(bytes, 4) == 1);
    DOCTEST_CHECK(RUSS::Get_Weighted_Choice_Short(shorts, 4) == 3);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);

    game_random::Set_Random_Seed(0);
    DOCTEST_CHECK(RUSS::Get_Weighted_Choice_Int(ints, 4) == 1);
    DOCTEST_CHECK(game_random::Random(12) == 10);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);

    DOCTEST_CHECK_FALSE(RUSS::Choose_Weighted_Index_U8(bytes, 0));
    DOCTEST_CHECK_FALSE(RUSS::Choose_Weighted_Index_U32(ints, 0));
    DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
}

DOCTEST_TEST_CASE("typed star ID shuffling keeps the legacy permutation and RNG consumption") {
    s_random_seed_scope scope;
    int16_t indices[] = {0, 1, 2, 3, 4, 5};
    game::id_star ids[] = {Star_Id(0), Star_Id(1), Star_Id(2), Star_Id(3), Star_Id(4), Star_Id(5)};
    game_random::Set_Random_Seed(42);
    RUSS::Shuffle_Sint(indices, 6);
    const uint32_t legacy_seed = game_random::Get_Random_Seed();
    game_random::Set_Random_Seed(42);
    game_random::Shuffle<game::id_star>(ids);
    for (size_t i = 0; i < std::size(ids); ++i) DOCTEST_CHECK(ids[i] == Star_Id(indices[i]));
    DOCTEST_CHECK(game_random::Get_Random_Seed() == legacy_seed);
}
