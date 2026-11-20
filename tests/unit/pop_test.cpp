#include "pch.h"

#include "doctest/doctest.h"

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
            POP::value_type initial_pop, SETTLER::legacy_value_type settler_data) {
        SETTLER::legacy_value_type race_info = settler_data >> 16;
        POP::value_type pop = initial_pop;

        pop = (pop ^ race_info) & 0x0000000F ^ pop;
        pop = ((race_info << 4) ^ pop) & 0x00000070 ^ pop;
        pop = ((settler_data >> 17) ^ pop) & 0x00000180 ^ pop;
        pop |= 0x00000200;
        pop &= 0xFFFFFBFF;

        return pop;
    }

    POP::value_type Helper_Add_Settler_To_Pop(
            POP::value_type initial_pop, SETTLER::value_type settler_data) {
        POP::value_type pop = initial_pop;

        POP::Set_Race(&pop, SETTLER::Get_Race(settler_data));
        POP::Set_Original_Owner(&pop, SETTLER::Get_Race(settler_data));
        POP::Set_Prof(&pop, SETTLER::Get_Prof(settler_data));
        POP::Set_Assigned(&pop, true);
        POP::Set_Conquered(&pop, false);

        return pop;
    }

    SETTLER::legacy_value_type Legacy_Encode_Settler_From_Pop(POP::value_type pop) {
        SETTLER::legacy_value_type encoded_settler = pop;

        encoded_settler ^= ((pop << 16) ^ encoded_settler) & 0x000F0000;
        encoded_settler ^= ((pop << 17) ^ encoded_settler) & 0x03000000;

        return encoded_settler;
    }

    SETTLER::value_type Helper_Encode_Settler_From_Pop(POP::value_type pop) {
        SETTLER::value_type encoded_settler = 0;

        SETTLER::Set_Race(&encoded_settler, POP::Get_Race(pop));
        SETTLER::Set_Prof(&encoded_settler, POP::Get_Prof(pop));

        return encoded_settler;
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

DOCTEST_TEST_CASE("packed settler setters preserve reserved bits and transit sentinel") {
    alignas(uint64_t) uint8_t bytes[24];
    for (size_t offset = 0; offset < alignof(uint64_t); ++offset) {
        memset(bytes, 0xCC, sizeof(bytes));
        auto* settler = new (bytes + offset) s_settler_info{UINT64_MAX};
        SETTLER::Set_Source_Colony(&settler->val, SETTLER::SOURCE_COLONY_IN_TRANSIT);
        DOCTEST_REQUIRE(SETTLER::Try_Set_Dest_Planet(&settler->val, game::id_planet{1000}));
        SETTLER::Set_Race(&settler->val, 7);
        SETTLER::Set_Eta(&settler->val, 5);
        SETTLER::Set_Prof(&settler->val, POP::PROF_WORKER);
        const SETTLER::value_type value = SETTLER::Load(&settler->val);
        DOCTEST_CHECK(SETTLER::Get_Source_Colony(value) == SETTLER::SOURCE_COLONY_IN_TRANSIT);
        DOCTEST_CHECK(SETTLER::Get_Dest_Planet(value) == game::id_planet{1000});
        DOCTEST_CHECK(SETTLER::Get_Race(value) == 7);
        DOCTEST_CHECK(SETTLER::Get_Eta(value) == 5);
        DOCTEST_CHECK(SETTLER::Get_Prof(value) == POP::PROF_WORKER);
        DOCTEST_CHECK((value & UINT64_C(0xFFFFFC0000000000)) == UINT64_C(0xFFFFFC0000000000));
        DOCTEST_CHECK(bytes[offset + sizeof(*settler)] == 0xCC);
    }
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

DOCTEST_TEST_CASE("settler subfield helpers preserve independent 64-bit fields") {
    constexpr int16_t index_values[] = {
        INT16_MIN, -1, 0, 1, 0xFF, 0x100, 0x3FFF, INT16_MAX,
    };
    constexpr SETTLER::value_type settler_values[] = {
        0, UINT64_MAX, 0x123456789ABCDEF0ull,
    };

    for (SETTLER::value_type settler_data : settler_values) {
        for (uint32_t race = 0; race < 16; ++race) {
            SETTLER::value_type actual = settler_data;
            SETTLER::Set_Race(&actual, race);
            DOCTEST_CHECK(SETTLER::Get_Race(actual) == race);
            DOCTEST_CHECK((actual & ~SETTLER::MASK_RACE) ==
                          (settler_data & ~SETTLER::MASK_RACE));
        }

        for (uint32_t prof = 0; prof < 4; ++prof) {
            SETTLER::value_type actual = settler_data;
            SETTLER::Set_Prof(&actual, static_cast<POP::e_prof>(prof));
            DOCTEST_CHECK(SETTLER::Get_Prof(actual) == static_cast<POP::e_prof>(prof));
            DOCTEST_CHECK((actual & ~SETTLER::MASK_PROF) ==
                          (settler_data & ~SETTLER::MASK_PROF));
        }

        for (uint32_t eta = 0; eta < 16; ++eta) {
            SETTLER::value_type actual = settler_data;
            SETTLER::Set_Eta(&actual, eta);
            DOCTEST_CHECK(SETTLER::Get_Eta(actual) == eta);
            DOCTEST_CHECK((actual & ~SETTLER::MASK_ETA) ==
                          (settler_data & ~SETTLER::MASK_ETA));
        }

        for (int16_t source_colony : index_values) {
            SETTLER::value_type actual = settler_data;
            SETTLER::Set_Source_Colony(&actual, source_colony);
            DOCTEST_CHECK(SETTLER::Get_Source_Colony(actual) == source_colony);
            DOCTEST_CHECK((actual & ~SETTLER::MASK_SOURCE_COLONY) ==
                          (settler_data & ~SETTLER::MASK_SOURCE_COLONY));
        }

        for (int16_t dest_planet : index_values) {
            SETTLER::value_type actual = settler_data;
            const auto id = legacy_world::Try_Planet_Id(dest_planet);
            if (!id) {
                actual = (actual & ~SETTLER::MASK_DEST_PLANET) |
                    (static_cast<uint64_t>(static_cast<uint16_t>(dest_planet)) << 16);
                DOCTEST_CHECK_FALSE(SETTLER::Get_Dest_Planet(actual).has_value());
                continue;
            }
            DOCTEST_REQUIRE(SETTLER::Try_Set_Dest_Planet(&actual, *id));
            DOCTEST_CHECK(SETTLER::Get_Dest_Planet(actual) == id);
            DOCTEST_CHECK((actual & ~SETTLER::MASK_DEST_PLANET) ==
                          (settler_data & ~SETTLER::MASK_DEST_PLANET));
        }
    }
}

DOCTEST_TEST_CASE("legacy freight encoding rejects planet IDs it cannot represent") {
    constexpr auto original = UINT64_C(0x123456789ABCDEF0);
    for (const uint32_t id : {uint32_t{INT16_MAX + 1}, uint32_t{UINT16_MAX + 1}, UINT32_MAX}) {
        auto value = original;
        DOCTEST_CHECK_FALSE(SETTLER::Try_Set_Dest_Planet(&value, game::id_planet{id}));
        DOCTEST_CHECK(value == original);
    }
}

DOCTEST_TEST_CASE("legacy settler records migrate all fields") {
    constexpr SETTLER::legacy_value_type legacy = 0xDAB4C382u;
    const SETTLER::value_type migrated = SETTLER::From_Legacy(legacy);

    DOCTEST_CHECK(SETTLER::Get_Source_Colony(migrated) == 0x82);
    DOCTEST_CHECK(SETTLER::Get_Dest_Planet(migrated) == game::id_planet{0xC3});
    DOCTEST_CHECK(SETTLER::Get_Race(migrated) == 4u);
    DOCTEST_CHECK(SETTLER::Get_Eta(migrated) == 0xBu);
    DOCTEST_CHECK(SETTLER::Get_Prof(migrated) == POP::PROF_SCIENTIST);
    DOCTEST_CHECK((migrated & SETTLER::MASK_RESERVED) == 0x0000D80000000000ull);

    for (uint32_t legacy_source = SETTLER::LEGACY_SOURCE_COLONY_IN_TRANSIT;
         legacy_source <= UINT8_MAX; ++legacy_source) {
        const SETTLER::value_type in_transit = SETTLER::From_Legacy(legacy_source);
        DOCTEST_CHECK(SETTLER::Get_Source_Colony(in_transit) ==
                      SETTLER::SOURCE_COLONY_IN_TRANSIT);
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
                SETTLER::value_type settler_data = 0;
                SETTLER::Set_Race(&settler_data, race);
                SETTLER::Set_Prof(&settler_data, static_cast<POP::e_prof>(prof));
                const SETTLER::legacy_value_type legacy_settler_data =
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

            const SETTLER::legacy_value_type legacy = Legacy_Encode_Settler_From_Pop(pop);
            const SETTLER::value_type helper = Helper_Encode_Settler_From_Pop(pop);
            const SETTLER::value_type migrated = SETTLER::From_Legacy(legacy);

            DOCTEST_CHECK(SETTLER::Get_Race(helper) == SETTLER::Get_Race(migrated));
            DOCTEST_CHECK(SETTLER::Get_Prof(helper) == SETTLER::Get_Prof(migrated));
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