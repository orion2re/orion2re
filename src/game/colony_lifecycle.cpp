#include "pch.h"
#include "bomb.h"
#include "mox.h"
#include "events.h"
#include "msg.h"
#include "invasion.h"
#include "colbldg.h"
#include "colcalc.h"
#include "officer.h"

namespace BOMB {
    int32_t __cdecl Handle_Leader_At_Lost_Colony(s_colony* colony) {
        int32_t owner_idx = colony->owner;
        int16_t star_idx = MOX::planet[colony->planet].star_index;
        int32_t killed_leader_idx = -1;
        int32_t leader_idx = MOX::star[star_idx].officer_index[owner_idx];

        if (leader_idx != -1) {
            bool other_colony_found = false;
            int16_t current_colony_idx = (int16_t)(colony - MOX::colony);

            for (int32_t planet_slot_idx = 0; planet_slot_idx < 5; ++planet_slot_idx) {
                int16_t p_idx = MOX::star[star_idx].planet_index[planet_slot_idx];
                if (p_idx != -1) {
                    int16_t c_idx = MOX::planet[p_idx].colony_index;
                    if (c_idx != -1 && MOX::colony[c_idx].owner == owner_idx && c_idx != current_colony_idx) {
                        other_colony_found = true;
                    }
                }
            }

            if (!other_colony_found) {
                if (MOX::leaders[leader_idx].eta > 0) {
                    OFFICER::Move_Officer_To_Pool(leader_idx);
                } else {
                    OFFICER::Kill_Officer(leader_idx);
                    killed_leader_idx = leader_idx;
                }
                MOX::star[star_idx].officer_index[owner_idx] = -1;
            }
        }

        return killed_leader_idx;
    }

    void __cdecl Destroy_Colony(s_colony* colony, int32_t capturing_player) {
        s_star_data* star;
        int16_t star_idx;
        int16_t colony_idx;
        int16_t leader_idx;
        s_MSG msg;

        star_idx = MOX::planet[colony->planet].star_index;
        star = &MOX::star[star_idx];
        colony_idx = (int16_t)(colony - MOX::colony);

        EVENTS::Kill_All_Events_At_Colony(colony_idx);

        leader_idx = BOMB::Handle_Leader_At_Lost_Colony(colony);

        if (capturing_player != -1) {
            const int16_t capture_reason = (int16_t)capturing_player;

            memset(&msg, 0, sizeof(s_MSG));
            msg.msg_type = MSG_TYPE_LOST_COLONY;
            msg.u.lost_colony.planet_idx = colony->planet;
            msg.u.lost_colony.reason = capture_reason;
            msg.u.lost_colony.leader_idx = leader_idx;
            msg.u.lost_colony.colony_or_outpost = colony->outpost_flag;
            MSG_::Add_Msg(msg, colony->owner);
        }

        if (MOX::player[(uint8_t)colony->owner].home_planet_id == colony->planet) {
            invasion::Assign_New_Home_Planet(colony->owner, colony_idx);
        }

        COLBLDG::Clear_Colony_Queue(colony_idx);

        MOX::planet[colony->planet].colony_index = -1;

        memset(colony, 0, sizeof(s_colony));
        colony->planet = -1;
        colony->owner = -1;

        COLCALC::Compute_Star_Colony_Stuff(star);
    }
} // namespace BOMB
