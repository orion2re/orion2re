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
            if (POP::Is_Conquered(colony->pop[i]) ||
                (allow_native_annihilation != 0 && POP::Is_Native(colony->pop[i]))) {
                return true;
            }
        }

        return false;
    }

    bool __cdecl Colony_Pop_Is_Annihilation_Target(const s_colony* colony, const s_colony_pop& pop) {
        const uint32_t race_idx = POP::Get_Race(pop);
        if (allow_native_annihilation != 0 && race_idx == POP::RACE_NATIVE) {
            return true;
        }

        return race_idx < MAX_PLAYERS &&
            static_cast<int32_t>(static_cast<int8_t>(legacy_world::Encode_Player_Slot(colony->owner))) != static_cast<int32_t>(race_idx) &&
            POP::Is_Conquered(pop);
    }
} // namespace COLONY
