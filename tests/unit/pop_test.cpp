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
            DOCTEST_CHECK(pop.group == (word & 0x0f));
            DOCTEST_CHECK(pop.original_owner.id == ((word & 0x70) >> 4));
            DOCTEST_CHECK(pop.profession == ((word & 0x180) >> 7));
            DOCTEST_CHECK(pop.assigned == ((word & 0x200) != 0));
            DOCTEST_CHECK(pop.conquered == ((word & 0x400) != 0));
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

DOCTEST_TEST_CASE("pop field assignments match the legacy masked updates") {
    for (uint32_t pop : {0u, 0xFFFFFFFFu, 0x12345678u, 0x00000400u, 0x00000280u}) {
        for (uint32_t race = 0; race < 16; ++race) {
            auto actual = SERIAL::Decode_Colony_Pop(pop);
            actual.group = static_cast<uint8_t>(race);
            DOCTEST_CHECK(SERIAL::Encode_Colony_Pop(actual) == (Legacy_Set_Pop_Race(pop, race) & 0x7ffu));
        }

        for (uint32_t owner = 0; owner < 8; ++owner) {
            auto actual = SERIAL::Decode_Colony_Pop(pop);
            actual.original_owner = game::id_player{owner};
            DOCTEST_CHECK(SERIAL::Encode_Colony_Pop(actual) == (Legacy_Set_Pop_Original_Owner(pop, owner) & 0x7ffu));
        }

        for (uint32_t prof = 0; prof < 4; ++prof) {
            auto actual = SERIAL::Decode_Colony_Pop(pop);
            actual.profession = static_cast<POP::e_prof>(prof);
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

DOCTEST_TEST_CASE("population groups are not species or always player slots") {
    s_colony_pop pop{.group = POP::RACE_ANDROID};
    DOCTEST_CHECK(POP::Is_Android(pop));
    DOCTEST_CHECK(POP::Is_Android_Or_Native(pop));
    DOCTEST_CHECK(POP::Get_Effective_Player(pop, 5) == 5);
    DOCTEST_CHECK(POP::Group_Player(pop) == game::NO_PLAYER);

    pop.group = POP::RACE_NATIVE;
    DOCTEST_CHECK(POP::Group_Player(pop) == game::NO_PLAYER);
    DOCTEST_CHECK(POP::Get_Effective_Player(pop, 5) == 5);

    pop.group = 2;
    DOCTEST_CHECK(!POP::Is_Android_Or_Native(pop));
    DOCTEST_CHECK(POP::Get_Effective_Player(pop, 5) == 2);
    DOCTEST_CHECK(POP::Group_Player(pop) == game::id_player{2});
}

}
