#include "pch.h"

#include "doctest/doctest.h"
#include "game/settler_serialization.h"
#include "game/colony_pop_serialization.h"

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

    uint32_t Legacy_Add_Settler_To_Pop(
            uint32_t initial_pop, uint32_t settler_data) {
        uint32_t race_info = settler_data >> 16;
        uint32_t pop = initial_pop;

        pop = (pop ^ race_info) & 0x0000000F ^ pop;
        pop = ((race_info << 4) ^ pop) & 0x00000070 ^ pop;
        pop = ((settler_data >> 17) ^ pop) & 0x00000180 ^ pop;
        pop |= 0x00000200;
        pop &= 0xFFFFFBFF;

        return pop;
    }

    uint32_t Helper_Add_Settler_To_Pop(
            uint32_t initial_pop, const s_settler_info& settler_data) {
        auto pop = SERIAL::Decode_Colony_Pop(initial_pop);

        POP::Set_Race(&pop, settler_data.race);
        POP::Set_Original_Owner(&pop, settler_data.race);
        POP::Set_Prof(&pop, static_cast<POP::e_prof>(settler_data.profession));
        POP::Set_Assigned(&pop, true);
        POP::Set_Conquered(&pop, false);

        return SERIAL::Encode_Colony_Pop(pop);
    }

    uint32_t Legacy_Encode_Settler_From_Pop(uint32_t pop) {
        uint32_t encoded_settler = pop;

        encoded_settler ^= ((pop << 16) ^ encoded_settler) & 0x000F0000;
        encoded_settler ^= ((pop << 17) ^ encoded_settler) & 0x03000000;

        return encoded_settler;
    }

    s_settler_info Helper_Encode_Settler_From_Pop(const s_colony_pop& pop) {
        return {.race = static_cast<uint8_t>(POP::Get_Race(pop)), .profession = static_cast<uint8_t>(POP::Get_Prof(pop))};
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

DOCTEST_TEST_CASE("colony population words preserve modeled bits and discard reserved bits") {
    for (uint32_t bits = 0; bits <= SERIAL::COLONY_POP_FIELDS_MASK; ++bits) {
        for (const auto flags : {0u, 0xA5A5F800u, 0xFFFFF800u}) {
            const auto word = bits | flags;
            const auto pop = SERIAL::Decode_Colony_Pop(word);
            DOCTEST_CHECK(pop == SERIAL::Decode_Colony_Pop(bits));
            DOCTEST_CHECK(SERIAL::Encode_Colony_Pop(pop) == bits);
        }
    }
}

DOCTEST_TEST_CASE("aligned settler records round-trip through unaligned wire buffers") {
    const s_settler_info settler{
        .source_colony = game::id_colony{80000}, .destination = game::id_planet{70000},
        .race = 7, .eta = 5, .profession = POP::PROF_WORKER};
    alignas(uint64_t) uint8_t bytes[SERIAL::SETTLER_BYTES + alignof(uint64_t)];
    for (size_t offset = 0; offset < alignof(uint64_t); ++offset) {
        memset(bytes, 0xCC, sizeof(bytes));
        SERIAL::Writer_ writer(bytes + offset, SERIAL::SETTLER_BYTES, "settler overflow");
        SERIAL::Write_Settler(writer, settler);
        DOCTEST_REQUIRE(writer.Length() == 11);
        SERIAL::Reader_ reader(bytes + offset, SERIAL::SETTLER_BYTES, "settler underflow");
        DOCTEST_CHECK(SERIAL::Read_Settler(reader) == settler);
        DOCTEST_CHECK(reader.Remaining() == 0);
        DOCTEST_CHECK(bytes[offset + SERIAL::SETTLER_BYTES] == 0xCC);
    }
}

DOCTEST_TEST_CASE("pop subfield setters match the legacy masked updates") {
    for (uint32_t pop : {0u, 0xFFFFFFFFu, 0x12345678u, 0x00000400u, 0x00000280u}) {
        for (uint32_t race = 0; race < 16; ++race) {
            auto actual = SERIAL::Decode_Colony_Pop(pop);
            POP::Set_Race(&actual, race);
            DOCTEST_CHECK(SERIAL::Encode_Colony_Pop(actual) == (Legacy_Set_Pop_Race(pop, race) & 0x7ffu));
        }

        for (uint32_t owner = 0; owner < 8; ++owner) {
            auto actual = SERIAL::Decode_Colony_Pop(pop);
            POP::Set_Original_Owner(&actual, owner);
            DOCTEST_CHECK(SERIAL::Encode_Colony_Pop(actual) == (Legacy_Set_Pop_Original_Owner(pop, owner) & 0x7ffu));
        }

        for (uint32_t prof = 0; prof < 4; ++prof) {
            auto actual = SERIAL::Decode_Colony_Pop(pop);
            POP::Set_Prof(&actual, static_cast<POP::e_prof>(prof));
            DOCTEST_CHECK(SERIAL::Encode_Colony_Pop(actual) == (Legacy_Set_Pop_Prof(pop, prof) & 0x7ffu));
        }
    }
}

DOCTEST_TEST_CASE("legacy settler records migrate all meaningful fields") {
    const auto migrated = SERIAL::Decode_Classic_Settler(0xDAB4C382u);
    DOCTEST_CHECK(migrated.source_colony == game::id_colony{0x82});
    DOCTEST_CHECK(migrated.destination == game::id_planet{0xC3});
    DOCTEST_CHECK(migrated.race == 4);
    DOCTEST_CHECK(migrated.eta == 0xB);
    DOCTEST_CHECK(migrated.profession == POP::PROF_SCIENTIST);
    DOCTEST_CHECK(migrated == SERIAL::Decode_Classic_Settler(0xDAB4C382u & 0x03ffffffu));
    DOCTEST_CHECK(SERIAL::Decode_Classic_Settler(0xfa).source_colony == game::NO_COLONY);
    const auto packed = SERIAL::Decode_Packed_Settler(UINT64_MAX);
    DOCTEST_CHECK(packed.source_colony == game::NO_COLONY);
    DOCTEST_CHECK(packed.destination == game::id_planet{UINT32_MAX});
    DOCTEST_CHECK(packed == SERIAL::Decode_Packed_Settler((UINT64_C(1) << 42) - 1));
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
                const s_settler_info settler_data{.race = static_cast<uint8_t>(race), .profession = static_cast<uint8_t>(prof)};
                const uint32_t legacy_settler_data =
                        (race << 16) | (prof << 24);

                DOCTEST_CHECK(Helper_Add_Settler_To_Pop(initial_pop, settler_data) ==
                              (Legacy_Add_Settler_To_Pop(initial_pop, legacy_settler_data) & 0x7ffu));
            }
        }
    }
}

DOCTEST_TEST_CASE("pop-to-settler encoding keeps race and profession equivalent to the original code") {
    for (uint32_t race = 0; race < 16; ++race) {
        for (uint32_t prof = 0; prof < 4; ++prof) {
            auto pop = SERIAL::Decode_Colony_Pop(0xABCDEF00u);
            POP::Set_Race(&pop, race);
            POP::Set_Prof(&pop, static_cast<POP::e_prof>(prof));

            const auto legacy = Legacy_Encode_Settler_From_Pop(SERIAL::Encode_Colony_Pop(pop));
            const auto helper = Helper_Encode_Settler_From_Pop(pop);
            const auto migrated = SERIAL::Decode_Classic_Settler(legacy);

            DOCTEST_CHECK(helper.race == migrated.race);
            DOCTEST_CHECK(helper.profession == migrated.profession);
        }
    }
}

DOCTEST_TEST_CASE("semantic getters expose profession assignment conquest and special-race behavior") {
    s_colony_pop pop{};
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
