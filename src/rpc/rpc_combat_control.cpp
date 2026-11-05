#include "pch.h"
#include "rpc_combat_control.h"

namespace rpc {
    namespace {
        bool Controlled() {
            const int16_t actor = Control_Actor();
            return COMBAT::Current_Viewer() == COMBAT::Local_Observer() &&
                COMBAT::Current_Viewer() >= 0 && MOX::combat_data != nullptr &&
                MOX::combat_total_ship_count <= MAX_COMBAT_SHIPS &&
                actor >= 0 && actor < MOX::combat_total_ship_count &&
                MOX::combat_data[actor].owner == COMBAT::Local_Observer() &&
                COMBAT1::Human_Input_Is_Available(actor) && CMBTDRW1::Current_Tactical_Input() == nullptr;
        }
        bool Selectable(int16_t slot) {
            if (MOX::combat_init != 0 || slot < 0 || slot >= MOX::combat_total_ship_count ||
                !COMBAT::Unit_Disclosed(slot) ||
                MOX::combat_data[slot].owner != COMBAT::Local_Observer() ||
                (slot == 0 && !CMBTFIRE::Does_Combat_Planet_Have_Defenses()) ||
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
            return COMBAT::Unit_Disclosed(slot) &&
                MOX::combat_data[slot].owner != COMBAT::Local_Observer() &&
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
                if (!COMBAT::Missile_Disclosed(slot) || MOX::missile[slot].owner == COMBAT::Local_Observer()) return result;
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
        if (!COMBAT::Reads_Allowed()) return false;
        if (!Controlled()) return true;
        const int16_t actor = Control_Actor();
        auto& ship = MOX::combat_data[actor];
        result->available = true;
        result->actor = actor;
        result->generation = Control_Generation();
        result->own_marines = ship.marine_count;
        result->maximum_marines = BOARDING::Maximum_Commitment(actor);
        result->already_boarded = CMBTFIR2::Already_Boarded(actor) != 0;
        result->retreat_refusal = COMBAT1::Check_Human_Retreat(actor);
        result->can_wait = COMBAT1::Ship_Wait_Is_Available();
        result->can_side_wait = COMBAT1::Combat_Side_Wait_Is_Available() && MOX::order_info_flag == 1;
        result->can_destruct = COMBAT1::Self_Destruct_Is_Available();
        result->can_super_fast = COMBAT1::Auto_Combat_Is_Available(true);
        result->scan_enabled = COMBAT1::disable_scan == 0;
        result->can_auto = COMBAT1::Auto_Combat_Is_Available(false);
        result->mode = MOX::order_info_flag == 0 ? 3 : MOX::skan_flag ? 1
            : MOX::board_flag                                         ? 2
                                                                      : 0;
        result->warning = MOX::missile_warning;
        result->tactical_options[0] = MOX::msl_impact_warning_flag != 0;
        result->tactical_options[1] = MOX::speedx2_flag != 0;
        result->tactical_options[2] = MOX::draw_legal_boxes_flag != 0;
        result->tactical_options[3] = MOX::shields_flag != 0;
        result->tactical_options[4] = MOX::grid_flag != 0;
        const int special_count = COMBINIT::amoeba_regeneration ? SPECIAL_COUNT : SPECIAL_REGENERATION;
        int16_t special_index = 0;
        for (int16_t bit = 0; bit < special_count; ++bit) {
            if (!struct_::Test_Bit_Field(ship.special_device_flags, bit)) continue;
            s_options::s_system system;
            system.name = TECHDATA::specials[bit].name;
            if (special_index < 8) system.active = CMBTFIRE::Special_Is_On(actor, special_index) ? 1 : 0;
            system.damaged = struct_::Test_Bit_Field(ship.special_device_damage_flags, bit) != 0;
            ++special_index;
            result->systems.push_back(system);
        }
        result->movement = ship.movement_left;
        result->turn_cost = CMBTMOV1::Turn_Cost(actor);
        result->absorber_selected = Absorber_Selected(actor);
        result->absorber_charge = ship.reflected_damage_pool;
        for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
            if (Selectable(i)) result->selectable.push_back(i);
            if (COMBAT::Unit_Disclosed(i) && COMBAT1::Stasis_Release_Is_Available(i)) result->stasis_targets.push_back(i);
            if (!Disclosed_Target(i)) continue;
            if (!result->already_boarded && BOARDING::Check_Human_Boarding(actor, i).allowed) result->boarding_targets.push_back(i);
            auto target = Target(actor, i);
            if (target.weapons || target.absorber) result->targets.push_back(target);
        }
        for (int16_t i = 0; i < MAX_COMBAT_FLYERS; ++i)
            if (COMBAT::Missile_Disclosed(i) && MOX::missile[i].owner != COMBAT::Local_Observer())
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
        if (result->mode != COMBAT1::ORDERS_MODE) {
            result->selectable.clear();
            result->cells.clear();
            result->targets.clear();
            if (result->mode == COMBAT1::OPTIONS_MODE) result->missile_targets.clear();
            result->stasis_targets.clear();
            if (result->mode != COMBAT1::BOARDING_MODE) result->boarding_targets.clear();
        }
        return true;
    }
    bool Authorize(int16_t slot, bool owned) {
        return COMBAT::Current_Viewer() == COMBAT::Local_Observer() &&
            COMBAT::Unit_Disclosed(slot) && (owned ? MOX::combat_data[slot].owner == COMBAT::Local_Observer() : MOX::combat_data[slot].owner != COMBAT::Local_Observer());
    }
    COMBAT1::e_result Validate(const COMBAT1::s_operation* operation) {
        const auto* input = CMBTDRW1::Current_Tactical_Input();
        if (input != nullptr) {
            if (COMBAT::Current_Viewer() < 0 || COMBAT::Current_Viewer() != COMBAT::Local_Observer() || fields::input_delay > 0) return COMBAT1::NO_CONTROL;
            if (operation->kind == COMBAT1::ANSWER_CONFIRMATION)
                return (input->kind == CMBTDRW1::STASIS_CONFIRMATION || input->kind == CMBTDRW1::DESTRUCT_CONFIRMATION) &&
                        (operation->value == 0 || operation->value == 1)
                    ? COMBAT1::APPLIED
                    : COMBAT1::ILLEGAL;
            if (operation->kind == COMBAT1::ANSWER_BOARDING)
                return input->kind == CMBTDRW1::BOARDING_CHOICE && operation->boarding_mode >= 0 && operation->boarding_mode <= 2 &&
                        operation->marines >= input->minimum && operation->marines <= input->maximum
                    ? COMBAT1::APPLIED
                    : COMBAT1::ILLEGAL;
            if (operation->kind == COMBAT1::ACK_INFORMATION)
                return input->kind == CMBTDRW1::TACTICAL_INFORMATION || input->kind == CMBTDRW1::TACTICAL_DETAIL ? COMBAT1::APPLIED : COMBAT1::ILLEGAL;
            if (operation->kind == COMBAT1::MANUAL_CONTROL) {
                const int side = COMBAT::Current_Viewer() == MOX::combat_attacker_player ? 0 : 1;
                return input->kind == CMBTDRW1::RETURN_TO_MANUAL && MOX::auto_combat_flag[side] != 0 ? COMBAT1::APPLIED : COMBAT1::ILLEGAL;
            }
            return COMBAT1::NO_CONTROL;
        }
        if (!Controlled()) return COMBAT1::NO_CONTROL;
        if (operation->actor != Control_Actor()) return COMBAT1::CONFLICT;
        if (operation->kind == COMBAT1::INPUT_MODE)
            return COMBAT1::Input_Mode_Is_Available(COMBAT1::e_input_mode(operation->value)) ? COMBAT1::APPLIED : COMBAT1::ILLEGAL;
        if (operation->kind == COMBAT1::TACTICAL_OPTION)
            return (operation->value == 0 || operation->value == 1) && (operation->option != COMBAT1::CYCLE_WARNING || operation->value == 1) &&
                    COMBAT1::Tactical_Option_Is_Available(COMBAT1::e_tactical_option(operation->option))
                ? COMBAT1::APPLIED
                : COMBAT1::ILLEGAL;
        if (operation->kind == COMBAT1::SELF_DESTRUCT)
            return COMBAT1::Self_Destruct_Is_Available() ? COMBAT1::APPLIED : COMBAT1::ILLEGAL;
        if (operation->kind == COMBAT1::AUTO_COMBAT)
            return (operation->value == 1 || operation->value == 2) && COMBAT1::Auto_Combat_Is_Available(operation->value == 2) ? COMBAT1::APPLIED : COMBAT1::ILLEGAL;
        if (MOX::order_info_flag != 1 && operation->kind != COMBAT1::FIRE_SELECTION) return COMBAT1::ILLEGAL;
        if (operation->kind == COMBAT1::SIDE_WAIT) return COMBAT1::Combat_Side_Wait_Is_Available() ? COMBAT1::APPLIED : COMBAT1::ILLEGAL;
        if (operation->kind == COMBAT1::END_SIDE) return COMBAT1::APPLIED;
        if ((MOX::skan_flag != 0 || MOX::board_flag != 0) &&
            (operation->kind == COMBAT1::MOVE || operation->kind == COMBAT1::SELECT ||
                (operation->kind == COMBAT1::FIRE && !operation->missile_target) || operation->kind == COMBAT1::REMOVE_STASIS)) return COMBAT1::ILLEGAL;
        if (MOX::skan_flag != 0 && operation->kind == COMBAT1::BOARD) return COMBAT1::ILLEGAL;
        switch (operation->kind) {
            case COMBAT1::SELECT: return Selectable(operation->target) ? COMBAT1::APPLIED : COMBAT1::ILLEGAL;
            case COMBAT1::MOVE: return Cell(operation->x, operation->y) && MOX::combat_grid[operation->x][operation->y] == 2000 ? COMBAT1::APPLIED : COMBAT1::ILLEGAL;
            case COMBAT1::FACE: return Cell(operation->x, operation->y) && MOX::combat_data[Control_Actor()].movement_left > 0 ? COMBAT1::APPLIED : COMBAT1::ILLEGAL;
            case COMBAT1::FIRE: {
                if (operation->missile_target) {
                    if (!COMBAT::Missile_Disclosed(operation->target) || MOX::missile[operation->target].owner == COMBAT::Local_Observer()) return COMBAT1::UNAVAILABLE;
                    return Target(Control_Actor(), operation->target, true).weapons ? COMBAT1::APPLIED : COMBAT1::ILLEGAL;
                }
                if (!Disclosed_Target(operation->target)) return COMBAT1::UNAVAILABLE;
                if (CMBTMOV1::Ship_Is_In_Stasis(operation->target)) return COMBAT1::LOCAL_REQUIRED;
                const auto target = Target(Control_Actor(), operation->target);
                return target.weapons || target.absorber ? COMBAT1::APPLIED : COMBAT1::ILLEGAL;
            }
            case COMBAT1::DONE: return COMBAT1::APPLIED;
            case COMBAT1::REMOVE_STASIS:
                return COMBAT::Unit_Disclosed(operation->target) && COMBAT1::Stasis_Release_Is_Available(operation->target) ? COMBAT1::APPLIED : COMBAT1::ILLEGAL;
            case COMBAT1::WAIT: return COMBAT1::Ship_Wait_Is_Available() ? COMBAT1::APPLIED : COMBAT1::ILLEGAL;
            case COMBAT1::RETREAT: return COMBAT1::Check_Human_Retreat(Control_Actor()) == 0 ? COMBAT1::APPLIED : COMBAT1::ILLEGAL;
            case COMBAT1::BOARD:
                if (!Disclosed_Target(operation->target)) return COMBAT1::UNAVAILABLE;
                return !CMBTFIR2::Already_Boarded(Control_Actor()) &&
                        BOARDING::Check_Human_Boarding(Control_Actor(), operation->target).allowed &&
                        (operation->boarding_mode == 1 || operation->boarding_mode == 2) &&
                        operation->marines >= 0 && operation->marines <= BOARDING::Maximum_Commitment(Control_Actor())
                    ? COMBAT1::APPLIED
                    : COMBAT1::ILLEGAL;
            case COMBAT1::FIRE_SELECTION:
                if (operation->bank_mask == 0) return COMBAT1::ILLEGAL;
                for (unsigned i = 0; i < 8; ++i)
                    if (operation->bank_mask & (1u << i)) {
                        if (MOX::combat_data[Control_Actor()].weapons[i].weapon_id == 0 || operation->bank_states[i] < -1 || operation->bank_states[i] > 1) return COMBAT1::ILLEGAL;
                    }
                return COMBAT1::APPLIED;
            default: return COMBAT1::ILLEGAL;
        }
    }
} // namespace rpc
