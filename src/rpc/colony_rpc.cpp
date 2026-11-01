#include "pch.h"
#include "rpc/colony_rpc.h"
#include "rpc/rpc_game.h"

namespace colony_rpc {
    bool Read_Colonies(rpc::s_map_observation* observation) {
        if (observation == nullptr || observation->viewer != MOX::PLAYER_NUM || !observation->players_captured ||
            MOX::NUM_COLONIES < 0 || MOX::NUM_COLONIES > MAX_COLONIES ||
            (MOX::NUM_COLONIES != 0 && MOX::colony == nullptr)) return false;
        for (int16_t index = 0; index < MOX::NUM_COLONIES; ++index) {
            const s_colony* colony = &MOX::colony[index];
            // Native Colonies/transport lists include own full colonies, not outposts.
            if (colony->owner != observation->viewer || colony->outpost_flag != 0) continue;
            const int16_t planet_index = colony->planet;
            if (planet_index < 0 || planet_index >= MOX::planet_count || MOX::planet == nullptr) return false;
            const s_planet_data* planet = &MOX::planet[planet_index];
            const int16_t system = planet->star_index;
            const int orbit = planet->orbit;
            if (system < 0 || (size_t)system >= observation->systems.size() || orbit < 0 || orbit >= 5 ||
                MOX::star[system].planet_index[orbit] != planet_index || planet->colony_index != index ||
                colony->n_pops > MAX_COLONY_POPULATION || colony->military[0] < 0 || colony->military[1] < 0) return false;
            rpc::s_colony_observation item;
            item.owner_slot = observation->viewer;
            item.system = system;
            item.orbit = orbit + 1;
            item.population = colony->n_pops;
            item.infantry = colony->military[0];
            item.armor = colony->military[1];
            observation->colonies.push_back(std::move(item));
        }
        observation->colonies_captured = true;
        return true;
    }
} // namespace colony_rpc
