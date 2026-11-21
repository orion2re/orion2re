#include "pch.h"

namespace aibomb {

    bool __cdecl AI_Invasion_Will_Succeed(int8_t attacker_id, int16_t* ship_list, int32_t n_ships, game::id_colony colony_id, s_colony* colony) {
        s_invasion_side_info invader;
        s_invasion_side_info defender;

        invasion::Get_Invasion_Info(ship_list, n_ships, colony_id, colony, &invader, &defender);

        if (COMBFIND::current_star_has_player[attacker_id].transport_count > 0) {
            invader.current_troop_type_idx = UNIT_MARINES;
            invader.unit_counts[UNIT_MARINES] += COMBFIND::current_star_has_player[attacker_id].transport_count * 4 - 2;
        }

        if (invader.unit_counts[UNIT_MARINES] > 0) {
            s_invasion_side_info backup_invader = invader;
            s_invasion_side_info backup_defender = defender;

            int32_t remaining_tries = 10;
            int32_t invader_wins = 0;

            do {
                if (invader_wins >= 7 || invader_wins + remaining_tries < 7) {
                    break;
                }

                remaining_tries--;

                invasion::Resolve_Ground_Combat(&invader, &defender);

                if ((uint8_t)invader.current_troop_type_idx < UNIT_COUNT) {
                    invader_wins++;
                }

                invader = backup_invader;
                defender = backup_defender;

            } while (remaining_tries > 0);

            return invader_wins >= 7;
        }

        return false;
    }

} // namespace aibomb