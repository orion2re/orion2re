#include "pch.h"

namespace COLONY {
    bool Can_Scrap_Building(const s_colony& colony, uint16_t stardate) {
        return colony.last_turn_building_destroyed != stardate ||
            colony.buildings_scrapped_this_turn < max_buildings_scrapped_per_turn;
    }

    void Record_Building_Scrap(s_colony* colony, uint16_t stardate) {
        if (colony->last_turn_building_destroyed != stardate) {
            colony->buildings_scrapped_this_turn = 0;
        }
        colony->last_turn_building_destroyed = stardate;
        ++colony->buildings_scrapped_this_turn;
    }

    bool __cdecl Colony_Has_Natives(int16_t colony_idx) {
        uint8_t n_pops = MOX::colony[colony_idx].n_pops;

        if (n_pops != 0) {
            for (int16_t i = 0; i < (int16_t)n_pops; i++) {
                if (POP::Is_Native(MOX::colony[colony_idx].pop[i])) {
                    return true;
                }
            }
        }

        return false;
    }

    bool __cdecl Colony_Needs_Occupation_Policy(int16_t colony_idx) {
        const s_colony* colony = &MOX::colony[colony_idx];
        if (colony->occupation_policy != 4) {
            return false;
        }

        for (int16_t i = 0; i < colony->n_pops; ++i) {
            if (POP::Is_Conquered(colony->pop[i]) ||
                (allow_native_annihilation != 0 && POP::Is_Native(colony->pop[i]))) {
                return true;
            }
        }

        return false;
    }

    bool __cdecl Colony_Pop_Is_Annihilation_Target(const s_colony* colony, uint32_t pop) {
        const uint32_t race_idx = POP::Get_Race(pop);
        if (allow_native_annihilation != 0 && race_idx == POP::RACE_NATIVE) {
            return true;
        }

        return race_idx < MAX_PLAYERS &&
            static_cast<int32_t>(static_cast<int8_t>(colony->owner)) != static_cast<int32_t>(race_idx) &&
            POP::Is_Conquered(pop);
    }
} // namespace COLONY
