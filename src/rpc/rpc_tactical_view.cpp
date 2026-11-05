#include "pch.h"
#include "rpc_tactical_view.h"
#include "game/combat.h"

namespace rpc {
    static bool Read_Row(int16_t slot, s_unit* result, bool detail) {
        if (result == nullptr) return false;
        *result = {};
        const int16_t viewer = COMBAT::Current_Viewer();
        if (!COMBAT::Reads_Allowed() || viewer != COMBAT::Local_Observer() || !COMBAT::Unit_Disclosed(slot)) return false;
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

    bool Read_Tactical_Unit(int16_t slot, s_unit* result) {
        return Read_Row(slot, result, true);
    }

    bool Read_Tactical_View(s_view* result) {
        if (result == nullptr) return false;
        *result = {};
        if (!COMBAT::Reads_Allowed()) return false;
        const int16_t viewer = COMBAT::Current_Viewer();
        if (viewer < 0 || COMBAT::Local_Observer() != viewer || MOX::combat_data == nullptr ||
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
            if (i == COMBAT::Current_Unit()) result->selected = i;
        }
        return true;
    }

    bool Read_Tactical_Missile(int16_t slot, s_missile* result) {
        if (result == nullptr || !COMBAT::Reads_Allowed() || !COMBAT::Missile_Disclosed(slot)) return false;
        *result = {};
        const auto& missile = MOX::missile[slot];
        result->slot = slot;
        result->x = missile.x / 20;
        result->y = missile.y / 20;
        result->quantity = missile.quantity;
        result->type = TECHDATA::weapons[missile.type].type;
        result->owned = missile.owner == COMBAT::Current_Viewer();
        strlcpy(result->name, TECHDATA::weapons[missile.type].name, sizeof(result->name));
        return true;
    }

    bool Read_Tactical_Missiles(std::vector<s_missile>* result) {
        if (result == nullptr || !COMBAT::Reads_Allowed() ||
            COMBAT::Current_Viewer() < 0 || COMBAT::Local_Observer() != COMBAT::Current_Viewer()) return false;
        result->clear();
        for (int16_t i = 0; i < MAX_COMBAT_FLYERS; ++i) {
            s_missile missile;
            if (Read_Tactical_Missile(i, &missile)) result->push_back(missile);
        }
        return true;
    }
    bool Read_Hit_History(s_hit_history* result) {
        const auto* scope = COMBAT::Current_Battle();
        if (result == nullptr || !COMBAT::Reads_Allowed() || scope == nullptr) return false;
        *result = {};
        result->next_sequence = scope->next_hit_sequence;
        for (size_t i = 0; i < scope->hits.size(); ++i)
            result->hits.push_back(scope->hits[(scope->hit_head + i) % scope->hits.size()]);
        result->first_sequence = result->hits.empty() ? result->next_sequence : result->hits.front().sequence;
        return true;
    }
} // namespace rpc
