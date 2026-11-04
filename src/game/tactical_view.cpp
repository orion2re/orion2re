#include "pch.h"
#include "tactical_view.h"
#include "tactical_events.h"
#include "tactical_control.h"

namespace tactical_view {
    int16_t Local_Observer() {
        const int16_t viewer = MOX::PLAYER_NUM;
        if (RUSS::all_ai_game != 0 || MOX::player == nullptr || viewer < 0 ||
            viewer >= MOX::NUM_PLAYERS || viewer >= MAX_PLAYERS ||
            MOX::player[viewer].objectives != PLAYER_OBJECTIVE_HUMAN || MOX::player[viewer].eliminated != 0 ||
            (viewer != MOX::combat_attacker_player && viewer != MOX::combat_defender_player)) return -1;
        return viewer;
    }

    bool Unit_Disclosed(int16_t slot) {
        const int16_t viewer = tactical_events::Current_Viewer();
        if (viewer < 0 || MOX::combat_data == nullptr ||
            MOX::combat_total_ship_count < 0 || MOX::combat_total_ship_count > MAX_COMBAT_SHIPS ||
            slot < 0 || slot >= MOX::combat_total_ship_count) return false;
        const auto& unit = MOX::combat_data[slot];
        if (unit.unit_status != 0) return false;
        return unit.owner == viewer || unit.special_status_flag != 4 || (slot != 0 && unit.size_class >= 0 && unit.size_class <= 5);
    }

    void Unit_Changed(int16_t slot, bool map_change) {
        if (!Unit_Disclosed(slot)) return;
        const bool contact = MOX::combat_data[slot].owner != tactical_events::Current_Viewer() && MOX::combat_data[slot].special_status_flag == 4;
        if (map_change || !contact) {
            tactical_control::Invalidate();
            tactical_events::Invalidate_View();
        }
    }

    void Resolve_Retreat(int16_t slot, tactical_events::e_retreat_outcome outcome) {
        if (tactical_events::Current_Viewer() != Local_Observer() || !Unit_Disclosed(slot)) return;
        const auto& unit = MOX::combat_data[slot];
        if (unit.owner == tactical_events::Current_Viewer() && unit.is_retreating == 1)
            tactical_events::Resolve_Retreat(slot, outcome);
    }

    void End_Pending_Retreats() {
        if (tactical_events::Current_Viewer() < 0 || MOX::combat_data == nullptr || MOX::combat_total_ship_count > MAX_COMBAT_SHIPS) return;
        for (int16_t i = 1; i < MOX::combat_total_ship_count; ++i)
            tactical_view::Resolve_Retreat(i, tactical_events::BATTLE_ENDED);
    }

} // namespace tactical_view
