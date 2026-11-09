#include "pch.h"

namespace COMBFIND {
    void __cdecl Reset_One_Shot_System_Special(s_star_data* star) {
        // System specials with value >= 50 (ASCII '2') are treated as one-shot/temporary events.
        if (star->system_special >= 50) {
            if (star->system_special < 90) {
                for (int i = 0; i < 5; ++i) {
                    int16_t planet_idx = star->planet_index[i];
                    if (planet_idx != -1) {
                        // Reset the special status on each planet belonging to this system.
                        MOX::planet[planet_idx].planet_special = SYSTEM_SPECIAL_NO_SPECIAL;
                    }
                }
            }
            // Clear the system special flag
            star->system_special = SYSTEM_SPECIAL_NO_SPECIAL;
        }
    }

    bool Is_Star_Or_Antares_Location(int16_t location, int16_t star_count) {
        return location >= 0 && location <= star_count;
    }

    bool Wandering_Monster_Retires(
        int8_t monster_type, bool amoeba_toxicized_colony) {
        if (monsters_stay) {
            return false;
        }

        if (monster_type == NONPLAYER_SHIP_TYPE_AMOEBA) {
            return amoeba_toxicized_colony;
        }

        if (monster_type == NONPLAYER_SHIP_TYPE_CRYSTAL ||
            monster_type == NONPLAYER_SHIP_TYPE_DRAGON ||
            monster_type == NONPLAYER_SHIP_TYPE_HYDRA) {
            return game_random::Random(monsters_stay_chance) == 1;
        }

        return false;
    }
} // namespace COMBFIND