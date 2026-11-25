#include "pch.h"

#include <array>

#include "doctest/doctest.h"

namespace {
    auto& players = game::current_state.players;
    struct fatal_exit {};
} // namespace

namespace MOX {
    s_animation_header* screen_seg;

    s_colony* colony;

    int32_t stardate;
    s_settings settings;
} // namespace MOX

namespace raceopt {
    int16_t __cdecl Player_Score_Modifier(game::id_player player) {
        DOCTEST_CHECK(SCORE::hi->player == player);
        return 100;
    }
} // namespace raceopt

namespace exitm {
    [[noreturn]] void __cdecl Exit_With_Message(const char*) {
        throw fatal_exit{};
    }
} // namespace exitm

DOCTEST_TEST_CASE("hotseat player scores align temporary state and restore the screen arena") {
    players = ::player_roster::Make_Roster();
    MOX::settings = {};
    game::current_state.player_count = 2;
    allocate::mark_stack_pos[0] = 0;
    players[1].player_defeated[0] = 1;
    players[1].tech_fields[0] = 3;
    players[1].captured_population = 3;

    for (uint32_t preceding_size = 0; preceding_size < 16; ++preceding_size) {
        alignas(s_hiscore) std::array<uint8_t, 1024> storage;
        storage.fill(0xA5);
        auto* header = (allocate::s_alloc_header*)storage.data();
        *header = {0, (uint32_t)(storage.size() - sizeof(*header)), 0};
        MOX::screen_seg = (s_animation_header*)(header + 1);
        allocate::Allocate_Next_Block(MOX::screen_seg, preceding_size);
        const uint32_t saved = header->current;

        for (int16_t player_idx : {1, 0, 1}) {
            const auto player = game::current_state.players[player_idx].id;
            DOCTEST_CHECK(SCORE::Player_Score(player) == (player_idx == 1 ? 59 : 0));
            DOCTEST_CHECK(SCORE::hi->player == player);
            DOCTEST_CHECK((uintptr_t)SCORE::hi % alignof(s_hiscore) == 0);
            DOCTEST_CHECK(allocate::Get_Max_Block_Space(SCORE::hi) == sizeof(s_hiscore));
            DOCTEST_CHECK(header->current == saved);
            DOCTEST_CHECK(allocate::mark_stack_pos[0] == 0);
        }
        for (uint32_t i = 0; i < preceding_size; ++i) {
            DOCTEST_CHECK(((uint8_t*)MOX::screen_seg)[sizeof(*header) + i] == 0xA5);
        }
    }

    SCORE::hi = nullptr;
    MOX::screen_seg = nullptr;
}
