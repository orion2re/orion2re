#include "pch.h"
#include "tactical_control.h"
#include "tactical_events.h"
#include "tactical_view.h"
#include "boarding.h"

namespace tactical_control {
    namespace {
        bool Controlled() {
            const int16_t actor = Actor();
            return tactical_events::Current_Viewer() == tactical_view::Local_Observer() &&
                tactical_events::Current_Viewer() >= 0 && MOX::combat_data != nullptr &&
                MOX::combat_total_ship_count <= MAX_COMBAT_SHIPS &&
                actor > 0 && actor < MOX::combat_total_ship_count && actor == MOX::cur_ship &&
                MOX::cur_ptr >= 0 && MOX::cur_ptr < MAX_COMBAT_SHIPS &&
                MOX::ship_turn_order[MOX::cur_ptr] == actor &&
                MOX::combat_data[actor].owner == tactical_view::Local_Observer() &&
                !COMBINIT::Is_Combat_Satellite(&MOX::combat_data[actor]) &&
                COMBAT1::Combat_Ship_Is_Functional(actor) && !COMBAT1::Ship_Controlled_By_Computer(actor) &&
                !CMBTFIRE::Is_Combat_Ship_Dead(actor) && MOX::combat_data[actor].is_retreating == 0 &&
                MOX::side_end_of_turn[MOX::combat_data[actor].owner] == 0 &&
                COMBAT1::Combat_Ship_Participates_In_Current_Pass(actor) &&
                (MOX::net_flag == 0 || MOX::combat_data[actor].owner == MOX::PLAYER_NUM) &&
                MOX::tactical_net_choice == 0 && MOX::order_info_flag == 1 &&
                fields::input_delay <= 0 &&
                MOX::skan_flag == 0 && MOX::board_flag == 0;
        }
        bool Selectable(int16_t slot) {
            if (MOX::combat_init != 0 || slot <= 0 || slot >= MOX::combat_total_ship_count ||
                !tactical_view::Unit_Disclosed(slot) ||
                MOX::combat_data[slot].owner != tactical_view::Local_Observer() ||
                COMBINIT::Is_Combat_Satellite(&MOX::combat_data[slot]) ||
                !COMBAT1::Combat_Ship_Is_Functional(slot) ||
                !COMBAT1::Combat_Ship_Participates_In_Current_Pass(slot)) return false;
            // Set_Cur_Ship_To can reuse an empty order entry, including a done ship.
            for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
                const int16_t entry = MOX::ship_turn_order[i];
                if (entry == -10) break;
                if (entry == slot || entry == -1) return true;
            }
            return false;
        }
        bool Disclosed_Target(int16_t slot) {
            return tactical_view::Unit_Disclosed(slot) &&
                MOX::combat_data[slot].owner != tactical_view::Local_Observer() &&
                MOX::combat_data[slot].special_status_flag != 4;
        }
        bool Absorber_Selected(int16_t actor) {
            int16_t index = 0;
            for (int16_t bit = 0; bit < SPECIAL_ENERGY_ABSORBER; ++bit)
                if (struct_::Test_Bit_Field(MOX::combat_data[actor].special_device_flags, bit)) ++index;
            return index < 8 && CMBTFIRE::Special_Is_On(actor, index);
        }
        s_target Target(int16_t actor, int16_t slot, bool missile = false) {
            s_target result{slot, 0, false};
            if (missile) {
                if (!tactical_view::Missile_Disclosed(slot) || MOX::missile[slot].owner == tactical_view::Local_Observer()) return result;
            } else if (!Disclosed_Target(slot) || CMBTMOV1::Ship_Is_In_Stasis(slot))
                return result;
            const auto bearing = missile ? CMBTDRW1::Relative_Bearing_XY(actor, MOX::missile[slot].x, MOX::missile[slot].y) : CMBTDRW1::Relative_Bearing(actor, slot);
            const auto arc = MOX::apply_damage_to_planet == 0 ? WEAPON_FIRING_ARC_ALL_SECTORS : CMBTDRW1::Get_Weapon_Arc(bearing);
            for (int16_t i = 0; i < 8; ++i) {
                const auto& weapon = MOX::combat_data[actor].weapons[i];
                if (weapon.weapon_id == 0 || !COMBAT1::Weapon_Is_On(actor, i) || !(arc & weapon.arc) ||
                    (weapon.shots_left == 0 && weapon.status != 1)) continue;
                const auto type = TECHDATA::weapons[weapon.weapon_id].type;
                if (type == WEAPON_TYPE_TORPEDO ? weapon.ammo != 0 : weapon.ammo == 0) continue;
                if (CMBTFIRE::Weapon_In_Range(i, actor, slot, missile ? 1 : 0, 0) &&
                    CMBTFIRE::Target_Is_Legal_For_Weapon(weapon.weapon_id, weapon.specials, slot, missile ? 1 : 0)) result.weapons |= uint8_t(1u << i);
            }
            result.absorber = !missile && MOX::combat_data[actor].reflected_damage_pool > 0 &&
                Absorber_Selected(actor) && CMBTAI::Range_To_Ship(actor, slot) <= 8;
            return result;
        }
        bool Cell(int16_t x, int16_t y) {
            return x >= 0 && y >= 0 && x < MOX::combat_grid_width && y < MOX::combat_grid_height;
        }
    } // namespace
    bool Read_Options(s_options* result) {
        *result = {};
        if (!tactical_events::Reads_Allowed()) return false;
        if (!Controlled()) return true;
        const int16_t actor = Actor();
        const auto& ship = MOX::combat_data[actor];
        result->available = true;
        result->actor = actor;
        result->generation = Generation();
        result->own_marines = ship.marine_count;
        result->maximum_marines = BOARDING::Maximum_Commitment(actor);
        result->already_boarded = CMBTFIR2::Already_Boarded(actor) != 0;
        result->movement = ship.movement_left;
        result->turn_cost = CMBTMOV1::Turn_Cost(actor);
        result->absorber_selected = Absorber_Selected(actor);
        result->absorber_charge = ship.reflected_damage_pool;
        for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
            if (Selectable(i)) result->selectable.push_back(i);
            if (!Disclosed_Target(i)) continue;
            if (!result->already_boarded && BOARDING::Check_Human_Boarding(actor, i).allowed) result->boarding_targets.push_back(i);
            auto target = Target(actor, i);
            if (target.weapons || target.absorber) result->targets.push_back(target);
        }
        for (int16_t i = 0; i < MAX_COMBAT_FLYERS; ++i)
            if (tactical_view::Missile_Disclosed(i) && MOX::missile[i].owner != tactical_view::Local_Observer())
                result->missile_targets.push_back(Target(actor, i, true));
        for (int16_t i = 0; i < 8; ++i) {
            const auto& weapon = ship.weapons[i];
            if (weapon.weapon_id == 0) continue;
            s_weapon row;
            row.slot = i;
            strlcpy(row.name, TECHDATA::weapons[weapon.weapon_id].name_plural, sizeof(row.name));
            row.type = TECHDATA::weapons[weapon.weapon_id].type;
            row.count = weapon.count;
            row.selection = weapon.active;
            row.shots = weapon.shots_left == 0 && weapon.status == 1 ? weapon.count : weapon.shots_left;
            row.ammo = weapon.ammo;
            row.arc = uint8_t(weapon.arc);
            row.modifiers = weapon.specials;
            row.ready = row.selection == 1 && row.shots > 0 &&
                (row.type == WEAPON_TYPE_TORPEDO ? row.ammo == 0 : row.ammo != 0);
            result->weapons.push_back(row);
        }
        for (int16_t x = 0; x < MOX::combat_grid_width; ++x)
            for (int16_t y = 0; y < MOX::combat_grid_height; ++y)
                if (MOX::combat_grid[x][y] == 2000) result->cells.push_back({x, y, CMBTMOV1::Move_Cost(actor, x, y)});
        return true;
    }
    bool Authorize(int16_t slot, bool owned) {
        return tactical_events::Current_Viewer() == tactical_view::Local_Observer() &&
            tactical_view::Unit_Disclosed(slot) && (owned ? MOX::combat_data[slot].owner == tactical_view::Local_Observer() : Disclosed_Target(slot));
    }
    e_result Validate(const s_operation* operation) {
        if (!Controlled()) return NO_CONTROL;
        if (operation->actor != Actor()) return CONFLICT;
        switch (operation->kind) {
            case SELECT: return Selectable(operation->target) ? APPLIED : ILLEGAL;
            case MOVE: return Cell(operation->x, operation->y) && MOX::combat_grid[operation->x][operation->y] == 2000 ? APPLIED : ILLEGAL;
            case FACE: return Cell(operation->x, operation->y) && MOX::combat_data[Actor()].movement_left > 0 ? APPLIED : ILLEGAL;
            case FIRE: {
                if (operation->missile_target) {
                    if (!tactical_view::Missile_Disclosed(operation->target) || MOX::missile[operation->target].owner == tactical_view::Local_Observer()) return UNAVAILABLE;
                    return Target(Actor(), operation->target, true).weapons ? APPLIED : ILLEGAL;
                }
                if (!Disclosed_Target(operation->target)) return UNAVAILABLE;
                if (CMBTMOV1::Ship_Is_In_Stasis(operation->target)) return LOCAL_REQUIRED;
                const auto target = Target(Actor(), operation->target);
                return target.weapons || target.absorber ? APPLIED : ILLEGAL;
            }
            case DONE: return APPLIED;
            case BOARD:
                if (!Disclosed_Target(operation->target)) return UNAVAILABLE;
                return !CMBTFIR2::Already_Boarded(Actor()) &&
                        BOARDING::Check_Human_Boarding(Actor(), operation->target).allowed &&
                        (operation->boarding_mode == 1 || operation->boarding_mode == 2) &&
                        operation->marines >= 0 && operation->marines <= BOARDING::Maximum_Commitment(Actor())
                    ? APPLIED
                    : ILLEGAL;
            case FIRE_SELECTION:
                if (operation->bank_mask == 0) return ILLEGAL;
                for (unsigned i = 0; i < 8; ++i)
                    if (operation->bank_mask & (1u << i)) {
                        if (MOX::combat_data[Actor()].weapons[i].weapon_id == 0 || operation->bank_states[i] < -1 || operation->bank_states[i] > 1) return ILLEGAL;
                    }
                return APPLIED;
            default: return ILLEGAL;
        }
    }
} // namespace tactical_control
