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

    static bool Read_Row(int16_t slot, s_unit* result, bool detail) {
        if (result == nullptr) return false;
        *result = {};
        const int16_t viewer = tactical_events::Current_Viewer();
        if (!tactical_events::Reads_Allowed() || viewer != Local_Observer() || !Unit_Disclosed(slot)) return false;
        const auto& unit = MOX::combat_data[slot];
        if (unit.unit_status != 0) return false;
        const bool owned = unit.owner == viewer;
        const bool contact = !owned && unit.special_status_flag == 4;
        // Draw_Reduced_Map discloses only these size classes for phased contacts.
        if (contact && (slot == 0 || unit.size_class < 0 || unit.size_class > 5)) return false;
        result->slot = slot;
        result->owned = owned;
        result->retreat_pending = owned && slot != 0 && unit.is_retreating == 1;
        result->contact = contact;
        result->attacker = unit.owner == MOX::combat_attacker_player;
        result->x = unit.x;
        result->y = unit.y;
        if (slot == 0)
            result->map_size = 0; // Planet combat size is not its presented planet size.
        else if (unit.size_class == 0)
            result->map_size = 0;
        else if (unit.size_class <= 2)
            result->map_size = 1;
        else if (unit.size_class == 3)
            result->map_size = 2;
        else
            result->map_size = 3;
        if (contact) {
            result->kind = CONTACT;
            return true;
        }
        if (slot == 0)
            result->kind = PLANET;
        else if (unit.owner >= NONPLAYER_SHIP_TYPE_AMOEBA)
            result->kind = MONSTER;
        else if (COMBINIT::Is_Combat_Satellite(&unit))
            result->kind = SATELLITE;
        else
            result->kind = SHIP;
        if (slot != 0) result->facing = unit.facing_dir;
        if (detail) std::memcpy(result->name, unit.name, sizeof(unit.name));
        if (detail && owned && slot != 0) {
            // Draw_Internals_Display's current values; no render, grids or calculators.
            result->own_details = true;
            result->in_stasis = unit.stasis_source_idx != COMBAT_SHIP_NONE;
            result->structure = int32_t(unit.structure_max) - unit.structure_damage;
            result->armor = unit.armor_remaining;
            result->drive = unit.ftl_type != DRIVE_NO_DRIVE ? unit.drive_current_hits : 0;
            result->shield = unit.shield_type != 0 ? unit.shield_current_hits : 0;
            result->computer = unit.computer_current_hits;
            result->speed = unit.current_speed;
            result->movement = unit.movement_left;
        }
        return true;
    }

    bool Read_Unit(int16_t slot, s_unit* result) {
        return Read_Row(slot, result, true);
    }

    bool Read_View(s_view* result) {
        if (result == nullptr) return false;
        *result = {};
        if (!tactical_events::Reads_Allowed()) return false;
        const int16_t viewer = tactical_events::Current_Viewer();
        if (viewer < 0 || Local_Observer() != viewer || MOX::combat_data == nullptr ||
            MOX::combat_total_ship_count < 0 || MOX::combat_total_ship_count > MAX_COMBAT_SHIPS) return false;
        result->observed = true;
        result->local_attacker = viewer == MOX::combat_attacker_player;
        result->initiative = MOX::combat_init != 0;
        result->round = int32_t(MOX::combat_turn) + 1;
        result->width = MOX::combat_grid_width;
        result->height = MOX::combat_grid_height;
        for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
            s_unit unit;
            if (!Read_Row(i, &unit, false)) continue;
            result->units.push_back(unit);
            if (i == tactical_events::Current_Unit()) result->selected = i;
        }
        return true;
    }
} // namespace tactical_view
