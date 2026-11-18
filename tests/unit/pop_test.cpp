#include "pch.h"

#include "doctest/doctest.h"
#include "game/settler_serialization.h"

namespace {
    uint32_t Legacy_Set_Pop_Race(uint32_t pop, uint32_t race) {
        return ((race ^ pop) & 0x0000000Fu) ^ pop;
    }

    uint32_t Legacy_Set_Pop_Original_Owner(uint32_t pop, uint32_t owner) {
        return (((owner << 4) ^ pop) & 0x00000070u) ^ pop;
    }

    uint32_t Legacy_Set_Pop_Prof(uint32_t pop, uint32_t prof) {
        return (((prof << 7) ^ pop) & 0x00000180u) ^ pop;
    }

    POP::value_type Legacy_Add_Settler_To_Pop(
            POP::value_type initial_pop, uint32_t settler_data) {
        uint32_t race_info = settler_data >> 16;
        POP::value_type pop = initial_pop;

        pop = (pop ^ race_info) & 0x0000000F ^ pop;
        pop = ((race_info << 4) ^ pop) & 0x00000070 ^ pop;
        pop = ((settler_data >> 17) ^ pop) & 0x00000180 ^ pop;
        pop |= 0x00000200;
        pop &= 0xFFFFFBFF;

        return pop;
    }

    POP::value_type Helper_Add_Settler_To_Pop(
            POP::value_type initial_pop, const SETTLER::s_settler& settler_data) {
        POP::value_type pop = initial_pop;

        POP::Set_Race(&pop, settler_data.race);
        POP::Set_Original_Owner(&pop, settler_data.race);
        POP::Set_Prof(&pop, settler_data.profession);
        POP::Set_Assigned(&pop, true);
        POP::Set_Conquered(&pop, false);

        return pop;
    }

    uint32_t Legacy_Encode_Settler_From_Pop(POP::value_type pop) {
        uint32_t encoded_settler = pop;

        encoded_settler ^= ((pop << 16) ^ encoded_settler) & 0x000F0000;
        encoded_settler ^= ((pop << 17) ^ encoded_settler) & 0x03000000;

        return encoded_settler;
    }

    SETTLER::s_settler Helper_Encode_Settler_From_Pop(POP::value_type pop) {
        return {.race = static_cast<uint8_t>(POP::Get_Race(pop)), .profession = POP::Get_Prof(pop)};
    }
}

DOCTEST_TEST_SUITE("pop helpers") {

DOCTEST_TEST_CASE("unaligned scalar access preserves native bytes and adjacent storage") {
    alignas(uint64_t) uint8_t bytes[32];
    for (size_t offset = 1; offset < alignof(uint64_t); ++offset) {
        memset(bytes, 0xA5, sizeof(bytes));
        const uint32_t word = 0x89ABCDEFu;
        unaligned::Store<uint32_t>(bytes + offset, word);
        DOCTEST_CHECK(unaligned::Load<uint32_t>(bytes + offset) == word);
        DOCTEST_CHECK(memcmp(bytes + offset, &word, sizeof(word)) == 0);
        DOCTEST_CHECK(bytes[offset - 1] == 0xA5);
        DOCTEST_CHECK(bytes[offset + sizeof(word)] == 0xA5);

        const uint64_t wide = UINT64_C(0xFEDCBA9876543210);
        unaligned::Store<uint64_t>(bytes + offset, wide);
        DOCTEST_CHECK(unaligned::Load<uint64_t>(bytes + offset) == wide);
        DOCTEST_CHECK(memcmp(bytes + offset, &wide, sizeof(wide)) == 0);
        DOCTEST_CHECK(bytes[offset + sizeof(wide)] == 0xA5);

        unaligned::Store<int16_t>(bytes + offset, INT16_MIN);
        DOCTEST_CHECK(unaligned::Load<int16_t>(bytes + offset) == INT16_MIN);
        unaligned::Store<int32_t>(bytes + offset, INT32_MIN);
        DOCTEST_CHECK(unaligned::Load<int32_t>(bytes + offset) == INT32_MIN);

        char text[] = "packed pointer";
        unaligned::Store<char*>(bytes + offset, text);
        DOCTEST_CHECK(unaligned::Load<char*>(bytes + offset) == text);
    }
}

DOCTEST_TEST_CASE("packed pop setters preserve unrelated bits at every word alignment") {
#pragma pack(push, 1)
    struct packed_pop {
        POP::value_type value;
    };
#pragma pack(pop)
    alignas(uint32_t) uint8_t bytes[16];
    constexpr POP::value_type initial = 0xA5A5FFFFu;
    for (size_t offset = 0; offset < alignof(uint32_t); ++offset) {
        memset(bytes, 0xCC, sizeof(bytes));
        auto* pop = new (bytes + offset) packed_pop{initial};
        POP::Set_Race(&pop->value, POP::RACE_ANDROID);
        POP::Set_Original_Owner(&pop->value, 3);
        POP::Set_Prof(&pop->value, POP::PROF_SCIENTIST);
        POP::Set_Assigned(&pop->value, false);
        POP::Set_Conquered(&pop->value, false);
        const POP::value_type value = POP::Load(&pop->value);
        DOCTEST_CHECK(POP::Get_Race(value) == POP::RACE_ANDROID);
        DOCTEST_CHECK(POP::Get_Original_Owner(value) == 3);
        DOCTEST_CHECK(POP::Is_Scientist(value));
        DOCTEST_CHECK_FALSE(POP::Is_Assigned(value));
        DOCTEST_CHECK_FALSE(POP::Is_Conquered(value));
        DOCTEST_CHECK((value & 0xFFFFF800u) == (initial & 0xFFFFF800u));
        DOCTEST_CHECK(bytes[offset + sizeof(*pop)] == 0xCC);
        POP::Set_Assigned(&pop->value, true);
        POP::Set_Conquered(&pop->value, true);
        DOCTEST_CHECK(POP::Is_Assigned(POP::Load(&pop->value)));
        DOCTEST_CHECK(POP::Is_Conquered(POP::Load(&pop->value)));
    }
}

DOCTEST_TEST_CASE("legacy expanded settler ignores reserved bits and recognizes transit sentinel") {
    const auto settler = SAVEGAME::Decode_Legacy_Settler(0xFFFFFD5703E8FFFFull, false);
    DOCTEST_CHECK_FALSE(settler.source_colony.has_value());
    DOCTEST_CHECK(settler.destination_planet == game::id_planet{1000});
    DOCTEST_CHECK(settler.race == 7);
    DOCTEST_CHECK(settler.eta == 5);
    DOCTEST_CHECK(settler.profession == POP::PROF_WORKER);
}

DOCTEST_TEST_CASE("pop subfield setters match the legacy masked updates") {
    for (uint32_t pop : {0u, 0xFFFFFFFFu, 0x12345678u, 0x00000400u, 0x00000280u}) {
        for (uint32_t race = 0; race < 16; ++race) {
            uint32_t actual = pop;
            POP::Set_Race(&actual, race);
            DOCTEST_CHECK(actual == Legacy_Set_Pop_Race(pop, race));
        }

        for (uint32_t owner = 0; owner < 8; ++owner) {
            uint32_t actual = pop;
            POP::Set_Original_Owner(&actual, owner);
            DOCTEST_CHECK(actual == Legacy_Set_Pop_Original_Owner(pop, owner));
        }

        for (uint32_t prof = 0; prof < 4; ++prof) {
            uint32_t actual = pop;
            POP::Set_Prof(&actual, static_cast<POP::e_prof>(prof));
            DOCTEST_CHECK(actual == Legacy_Set_Pop_Prof(pop, prof));
        }
    }
}

DOCTEST_TEST_CASE("expanded legacy settler fields decode independently") {
    for (uint32_t i = 0; i < 16; ++i) {
        const uint64_t packed = 0x8000FFFF0000ull | (uint64_t{i} << 32) |
            (uint64_t{15 - i} << 36) | (uint64_t{i % 4} << 40) | i;
        const auto settler = SAVEGAME::Decode_Legacy_Settler(packed, false);
        DOCTEST_CHECK(settler.source_colony == game::id_colony{i});
        DOCTEST_CHECK(settler.destination_planet == game::id_planet{65535});
        DOCTEST_CHECK(settler.race == i);
        DOCTEST_CHECK(settler.eta == 15 - i);
        DOCTEST_CHECK(settler.profession == static_cast<POP::e_prof>(i % 4));
    }
}

DOCTEST_TEST_CASE("legacy settler records migrate all fields") {
    const auto migrated = SAVEGAME::Decode_Legacy_Settler(0xDAB4C382u, true);

    DOCTEST_CHECK(migrated.source_colony == game::id_colony{0x82});
    DOCTEST_CHECK(migrated.destination_planet == game::id_planet{0xC3});
    DOCTEST_CHECK(migrated.race == 4u);
    DOCTEST_CHECK(migrated.eta == 0xBu);
    DOCTEST_CHECK(migrated.profession == POP::PROF_SCIENTIST);

    for (uint32_t legacy_source = 250;
         legacy_source <= UINT8_MAX; ++legacy_source) {
        DOCTEST_CHECK_FALSE(SAVEGAME::Decode_Legacy_Settler(legacy_source, true).source_colony.has_value());
    }
}

DOCTEST_TEST_CASE("settler-to-pop rewrite matches the original bit twiddling") {
    const uint32_t initial_values[] = {
        0u,
        0xFFFFFFFFu,
        0x00000400u,
        0x13579BDFu,
    };

    for (uint32_t initial_pop : initial_values) {
        for (uint32_t race = 0; race < 16; ++race) {
            for (uint32_t prof = 0; prof < 4; ++prof) {
                const SETTLER::s_settler settler_data{.race = static_cast<uint8_t>(race), .profession = static_cast<POP::e_prof>(prof)};
                const uint32_t legacy_settler_data =
                        (race << 16) | (prof << 24);

                DOCTEST_CHECK(Helper_Add_Settler_To_Pop(initial_pop, settler_data) ==
                              Legacy_Add_Settler_To_Pop(initial_pop, legacy_settler_data));
            }
        }
    }
}

DOCTEST_TEST_CASE("pop-to-settler encoding keeps race and profession equivalent to the original code") {
    for (uint32_t race = 0; race < 16; ++race) {
        for (uint32_t prof = 0; prof < 4; ++prof) {
            uint32_t pop = 0xABCDEF00u;
            POP::Set_Race(&pop, race);
            POP::Set_Prof(&pop, static_cast<POP::e_prof>(prof));

            const auto legacy = Legacy_Encode_Settler_From_Pop(pop);
            const auto helper = Helper_Encode_Settler_From_Pop(pop);
            const auto migrated = SAVEGAME::Decode_Legacy_Settler(legacy, true);

            DOCTEST_CHECK(helper.race == migrated.race);
            DOCTEST_CHECK(helper.profession == migrated.profession);
        }
    }
}

DOCTEST_TEST_CASE("semantic getters expose profession assignment conquest and special-race behavior") {
    uint32_t pop = 0;
    POP::Set_Race(&pop, POP::RACE_ANDROID);
    POP::Set_Original_Owner(&pop, 3);
    POP::Set_Prof(&pop, POP::PROF_WORKER);
    POP::Set_Assigned(&pop, true);
    POP::Set_Conquered(&pop, true);

    DOCTEST_CHECK(POP::Get_Race(pop) == POP::RACE_ANDROID);
    DOCTEST_CHECK(POP::Get_Original_Owner(pop) == 3);
    DOCTEST_CHECK(POP::Get_Prof(pop) == POP::PROF_WORKER);
    DOCTEST_CHECK(POP::Is_Worker(pop));
    DOCTEST_CHECK(POP::Is_Assigned(pop));
    DOCTEST_CHECK(POP::Is_Conquered(pop));
    DOCTEST_CHECK(POP::Is_Android(pop));
    DOCTEST_CHECK(POP::Is_Android_Or_Native(pop));
    DOCTEST_CHECK(POP::Get_Effective_Player(pop, 5) == 5);

    POP::Set_Race(&pop, 2);

    DOCTEST_CHECK(!POP::Is_Android_Or_Native(pop));
    DOCTEST_CHECK(POP::Get_Effective_Player(pop, 5) == 2);
}

}