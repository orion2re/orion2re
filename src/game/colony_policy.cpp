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

    bool __cdecl Colony_Has_Natives(game::id_colony colony_idx) {
        uint8_t n_pops = game::current_state.colonies.at(colony_idx).n_pops;

        if (n_pops != 0) {
            for (int16_t i = 0; i < (int16_t)n_pops; i++) {
                if (POP::Is_Native(game::current_state.colonies.at(colony_idx).pop[i])) {
                    return true;
                }
            }
        }

        return false;
    }

    bool __cdecl Colony_Needs_Occupation_Policy(game::id_colony colony_idx) {
        const s_colony* colony = &game::current_state.colonies.at(colony_idx);
        if (colony->occupation_policy != 4) {
            return false;
        }

        for (int16_t i = 0; i < colony->n_pops; ++i) {
            if (colony->pop[i].conquered ||
                (allow_native_annihilation != 0 && POP::Is_Native(colony->pop[i]))) {
                return true;
            }
        }

        return false;
    }

    bool __cdecl Colony_Pop_Is_Annihilation_Target(const s_colony* colony, const s_colony_pop& pop) {
        if (allow_native_annihilation != 0 && POP::Is_Native(pop)) {
            return true;
        }

        const auto player = POP::Group_Player(pop);
        return player != game::NO_PLAYER && player != colony->owner && pop.conquered;
    }
} // namespace COLONY
