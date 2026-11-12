#include "pch.h"

namespace invasion {
    int32_t __cdecl Colony_Infantry_Limit(s_colony* colony) {
        int32_t limit =
            COLCALC::population_capacity.size[MOX::planet[colony->planet].size];

        if (MOX::player[colony->owner].traits[TRAIT_WARLORD] == 0) {
            limit /= 2;
        }

        if (limit == 0) {
            limit = 1;
        }

        return limit;
    }

    int __cdecl Colony_N_Militia(s_colony* colony) {
        int count = 0;
        uint8_t n_pops = (uint8_t)colony->n_pops;

        for (uint8_t i = 0; i < n_pops; ++i) {
            uint32_t pop_flags = colony->pop[i];

            if (((pop_flags & 0x0F) < MAX_PLAYERS) && ((pop_flags & 0x0400) == 0)) {
                count++;
            }
        }

        return count / 5;
    }

    int16_t __cdecl Colony_Infantry_Barracks_Limit(s_colony* colony) {
        int32_t limit = (uint8_t)colony->n_pops;

        if (MOX::player[colony->owner].traits[TRAIT_WARLORD] == 0) {
            limit /= 2;
        }

        if (limit == 0) {
            limit = 1;
        }

        return (int16_t)limit;
    }

    int __cdecl Colony_Tank_Barracks_Limit(s_colony* colony) {
        int limit;
        int pops = (uint8_t)colony->n_pops;

        if (MOX::player[colony->owner].traits[30 /* TRAIT_WARLORD */] == 0) {
            limit = pops / 4;
        } else {
            limit = pops / 2;
        }

        if (limit == 0) {
            limit = 1;
        }

        return limit;
    }

    int32_t __cdecl Colony_Tank_Limit(s_colony* colony) {
        int32_t tank_limit =
            COLCALC::population_capacity.size[MOX::planet[colony->planet].size];

        if (MOX::player[colony->owner].traits[TRAIT_WARLORD] == 0) {
            tank_limit /= 4;
        } else {
            tank_limit /= 2;
        }

        if (tank_limit == 0) {
            tank_limit = 1;
        }

        return tank_limit;
    }
} // namespace invasion
