#pragma once

#include <cstdint>
#include <vector>

namespace COMBAT {
    struct s_hit;
}

namespace rpc {
    struct s_hit_history {
        uint64_t first_sequence = 1, next_sequence = 1;
        std::vector<COMBAT::s_hit> hits;
    };
    bool Read_Hit_History(s_hit_history* result);
    enum e_tactical_kind { SHIP,
        PLANET,
        SATELLITE,
        MONSTER,
        CONTACT };
    struct s_unit {
        int16_t slot = -1; // Game-thread-only native identity, never a wire reference.
        e_tactical_kind kind = SHIP;
        bool attacker = false, owned = false, contact = false, retreat_pending = false;
        int16_t x = 0, y = 0, facing = 0, map_size = 0;
        char name[31] = {};
        bool own_details = false, in_stasis = false;
        int32_t structure = 0, armor = 0, drive = 0, shield = 0, computer = 0;
        int16_t speed = 0, movement = 0;
    };
    struct s_view {
        bool observed = false, local_attacker = false, initiative = false;
        bool time_warp = false, strict_wait = false, nebula = false;
        int16_t sequence_mode = 0;
        int32_t round = 0;
        int16_t width = 0, height = 0, selected = -1;
        std::vector<s_unit> units;
    };
    struct s_missile {
        int16_t slot = -1, x = 0, y = 0, quantity = 0, type = 0;
        bool owned = false;
        char name[80] = {};
        char target_name[80] = {};
        bool target_disclosed = false;
    };
    struct s_reader {
        int16_t (*observer)() = nullptr;
        bool (*view)(s_view*) = nullptr;
        bool (*unit)(int16_t, s_unit*) = nullptr;
    };
    // Request-local projection only, on the game thread at an explicit native safe point.
    bool Read_Tactical_View(s_view* result);
    bool Read_Tactical_Unit(int16_t slot, s_unit* result);
    bool Read_Tactical_Missile(int16_t slot, s_missile* result);
    bool Read_Tactical_Missiles(std::vector<s_missile>* result);
} // namespace rpc
