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
    game::id_leader __cdecl Handle_Leader_At_Lost_Colony(game::s_game_state& state, game::id_colony colony_id) {
        const auto& colony = state.colonies.at(colony_id);
        const auto owner_idx = colony.owner.id;
        if (owner_idx >= MAX_PLAYERS) return game::NO_LEADER;
        const auto parent_star_id = state.planets.at(colony.planet_id).star_id;
        auto killed_leader = game::NO_LEADER;
        auto& star = state.stars.at(parent_star_id);
        const auto officer = star.officer_id[owner_idx];

        if (officer.id < state.leaders.size()) {
            bool other_colony_found = false;

            for (int32_t planet_slot_idx = 0; planet_slot_idx < 5; ++planet_slot_idx) {
                const auto p_idx = star.orbit_planet_ids[planet_slot_idx];
                if (p_idx != game::NO_PLANET) {
                    const auto other_id = state.planets.at(p_idx).colony_id;
                    if (other_id != game::NO_COLONY && other_id != colony_id && state.colonies.at(other_id).owner == colony.owner) {
                        other_colony_found = true;
                    }
                }
            }

            if (!other_colony_found) {
                if (state.leaders.at(officer.id).eta > 0) {
                    OFFICER::Move_Officer_To_Pool(state, officer);
                } else {
                    OFFICER::Kill_Officer(state, officer);
                    killed_leader = officer;
                }
                star.officer_id[owner_idx] = game::NO_LEADER;
            }
        }

        return killed_leader;
    }

    void __cdecl Destroy_Colony(game::s_game_state& state, game::id_colony colony_id, int32_t capturing_player) {
        const auto it = state.colonies.find(colony_id);
        if (it == state.colonies.end()) return;
        auto& colony = it->second;
        auto& planet = state.planets.at(colony.planet_id);
        auto& star = state.stars.at(planet.star_id);
        const bool live = &state == &game::current_state;

        if (live) EVENTS::Kill_All_Events_At_Colony(colony_id);

        const auto lost_leader = BOMB::Handle_Leader_At_Lost_Colony(state, colony_id);

        if (live && capturing_player != -1) {
            const int16_t capture_reason = (int16_t)capturing_player;

            s_MSG msg{};
            msg.msg_type = MSG_TYPE_LOST_COLONY;
            msg.u.lost_colony.planet_id = colony.planet_id;
            msg.u.lost_colony.reason = capture_reason;
            msg.u.lost_colony.leader_idx = leader_slots::Encode(lost_leader);
            msg.u.lost_colony.colony_or_outpost = colony.outpost_flag;
            MSG_::Add_Msg(msg, colony.owner);
        }

        if (state.players.at(colony.owner.id).home_planet_id == colony.planet_id) {
            invasion::Assign_New_Home_Planet(state, colony.owner, colony_id);
        }

        COLBLDG::Clear_Colony_Queue(state, colony_id);

        planet.colony_id = game::NO_COLONY;
        state.colonies.erase(it);
        COLCALC::Compute_Star_Colony_Stuff(state, star);
    }
} // namespace BOMB
