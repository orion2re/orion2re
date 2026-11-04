#pragma once

#include <cstdint>
#include <vector>

namespace tactical_events {
    enum e_retreat_outcome : int;
}

namespace tactical_view {
    enum e_kind {
        SHIP,
        PLANET,
        SATELLITE,
        MONSTER,
        CONTACT,
    };
    struct s_unit {
        int16_t slot = -1; // Callback-local native identity, never a wire ID.
        e_kind kind = SHIP;
        bool attacker = false;
        bool owned = false;
        bool contact = false;
        bool retreat_pending = false;
        int16_t x = 0, y = 0, facing = 0, map_size = 0;
        char name[31] = {};
        bool own_details = false;
        bool in_stasis = false;
        int32_t structure = 0, armor = 0, drive = 0, shield = 0, computer = 0;
        int16_t speed = 0, movement = 0;
    };
    struct s_view {
        bool observed = false;
        bool local_attacker = false;
        bool initiative = false;
        int32_t round = 0;
        int16_t width = 0, height = 0, selected = -1;
        std::vector<s_unit> units;
    };
    struct s_missile {
        int16_t slot = -1, x = 0, y = 0, quantity = 0, type = 0;
        bool owned = false;
        char name[80] = {};
    };
    // All native reads and operation markers are game-thread only.
    int16_t Local_Observer();
    bool Unit_Disclosed(int16_t slot);
    void Unit_Changed(int16_t slot, bool map_change = false);
    // Native semantic producers only; no retained withdrawal report or tracking state.
    void Resolve_Retreat(int16_t slot, tactical_events::e_retreat_outcome outcome);
    void End_Pending_Retreats();
    // Valid only inside an explicitly classified native Service_Reads(true).
    bool Read_View(s_view* result);
    bool Read_Unit(int16_t slot, s_unit* result);
    bool Missile_Disclosed(int16_t slot);
    void Missile_Changed(int16_t slot);
    bool Read_Missile(int16_t slot, s_missile* result);
    bool Read_Missiles(std::vector<s_missile>* result);
    void Record_Unit_Hit(int16_t slot, int32_t amount, int32_t secondary_amount = 0, bool biological = false);
    void Record_Missile_Hit(int16_t slot, int32_t destroyed);
    struct s_reader {
        int16_t (*observer)() = nullptr;
        bool (*view)(s_view*) = nullptr;
        bool (*unit)(int16_t, s_unit*) = nullptr;
    };
} // namespace tactical_view
