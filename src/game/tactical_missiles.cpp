#include "pch.h"
#include "tactical_view.h"
#include "tactical_control.h"
#include "tactical_events.h"

namespace tactical_view {
    bool Missile_Disclosed(int16_t slot) {
        return tactical_events::Current_Viewer() >= 0 && Local_Observer() == tactical_events::Current_Viewer() &&
            MOX::missile != nullptr && slot >= 0 && slot < MAX_COMBAT_FLYERS &&
            MOX::missile[slot].type > WEAPON_NO_WEAPONS && MOX::missile[slot].quantity > 0;
    }

    void Missile_Changed(int16_t slot) {
        if (!Missile_Disclosed(slot)) return;
        tactical_control::Invalidate();
        tactical_events::Invalidate_View();
    }

    bool Read_Missile(int16_t slot, s_missile* result) {
        if (result == nullptr || !tactical_events::Reads_Allowed() || !Missile_Disclosed(slot)) return false;
        *result = {};
        const auto& missile = MOX::missile[slot];
        result->slot = slot;
        result->x = missile.x / 20;
        result->y = missile.y / 20;
        result->quantity = missile.quantity;
        result->type = TECHDATA::weapons[missile.type].type;
        result->owned = missile.owner == tactical_events::Current_Viewer();
        strlcpy(result->name, TECHDATA::weapons[missile.type].name, sizeof(result->name));
        return true;
    }

    bool Read_Missiles(std::vector<s_missile>* result) {
        if (result == nullptr || !tactical_events::Reads_Allowed() ||
            tactical_events::Current_Viewer() < 0 || Local_Observer() != tactical_events::Current_Viewer()) return false;
        result->clear();
        for (int16_t i = 0; i < MAX_COMBAT_FLYERS; ++i) {
            s_missile missile;
            if (Read_Missile(i, &missile)) result->push_back(missile);
        }
        return true;
    }

    void Record_Unit_Hit(int16_t slot, int32_t amount, int32_t secondary_amount, bool biological) {
        if (Local_Observer() != tactical_events::Current_Viewer() || !Unit_Disclosed(slot)) return;
        const auto& unit = MOX::combat_data[slot];
        if (unit.owner != tactical_events::Current_Viewer() && unit.special_status_flag == 4) return;
        if (amount <= 0 && secondary_amount <= 0) return;
        tactical_events::s_hit hit;
        hit.round = int32_t(MOX::combat_turn) + 1;
        hit.kind = biological ? tactical_events::BIOLOGICAL_HITS : tactical_events::UNIT_DAMAGE;
        hit.amount = uint16_t(amount);
        hit.secondary_amount = uint16_t(secondary_amount);
        strlcpy(hit.target_name, unit.name, sizeof(hit.target_name));
        tactical_events::Append_Hit(&hit);
    }

    void Record_Missile_Hit(int16_t slot, int32_t destroyed) {
        if (tactical_events::Current_Viewer() < 0 || Local_Observer() != tactical_events::Current_Viewer() ||
            MOX::missile == nullptr || slot < 0 || slot >= MAX_COMBAT_FLYERS || MOX::missile[slot].type <= WEAPON_NO_WEAPONS) return;
        tactical_events::s_hit hit;
        hit.round = int32_t(MOX::combat_turn) + 1;
        hit.kind = tactical_events::MISSILES_DESTROYED;
        hit.amount = destroyed;
        strlcpy(hit.target_name, TECHDATA::weapons[MOX::missile[slot].type].name, sizeof(hit.target_name));
        tactical_events::Append_Hit(&hit);
    }
} // namespace tactical_view
