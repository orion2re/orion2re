#include "pch.h"

namespace CMBTFIR2 {
    s_ship_explosion ship_explosion = {5, 3};
    uint16_t systems_repair_percent = 50;

    namespace {
        int16_t Wrap_Int16(int64_t value) {
            int32_t wrapped = (int32_t)(value % 0x10000);
            if (wrapped < 0) {
                wrapped += 0x10000;
            }
            if (wrapped > INT16_MAX) {
                wrapped -= 0x10000;
            }
            return (int16_t)wrapped;
        }

        int32_t Wrap_Int32(int64_t value) {
            int64_t wrapped = value % INT64_C(0x100000000);
            if (wrapped < 0) {
                wrapped += INT64_C(0x100000000);
            }
            if (wrapped > INT32_MAX) {
                wrapped -= INT64_C(0x100000000);
            }
            return (int32_t)wrapped;
        }

        int16_t Max_Total_Internal_Space_For_Repair(int16_t ship_idx) {
            const s_combat_data* ship = &MOX::combat_data[ship_idx];
            int64_t total_space =
                Wrap_Int16(ship->drive_max_hits) / 2;
            total_space += ship->computer_max_hits;

            for (int i = 0; i < 8; ++i) {
                total_space += ship->weapon_total_hits[i];
                total_space += ship->special_device_hits[i];
            }

            total_space += ship->shield_max_hits;
            return Wrap_Int16(total_space);
        }
    } // namespace

    void __cdecl Init_Internal_Space(s_combat_data* ship) {
        int16_t hull_class = ship->size_class;
        if (COMBINIT::Is_Combat_Satellite(ship)) {
            hull_class = COMBINIT::Combat_Satellite_Hull_Class(ship);
        }

        if (ship->ftl_type <= 0) {
            ship->drive_max_hits = 0;
        } else {
            ship->drive_max_hits = TECHDATA::hull_data[hull_class].drive_hp;
        }

        if (struct_::Test_Bit_Field(ship->special_device_flags, 27 /* SPEC_STRUCTURAL_ANALYZER */) &&
            !struct_::Test_Bit_Field(ship->special_device_damage_flags, 27 /* SPEC_STRUCTURAL_ANALYZER */)) {
            ship->drive_max_hits *= 3;
        }

        ship->computer_max_hits = ship->computer_type > COMPUTER_NO_COMPUTER
            ? TECHDATA::hull_data[hull_class].computer_hp
            : 0;

        for (int i = 0; i < 8; ++i) {
            if (ship->weapons[i].weapon_id < 1) {
                ship->weapon_unit_hits[i] = 0;
                ship->weapon_total_hits[i] = 0;
            } else {
                const int16_t weapon_id = ship->weapons[i].weapon_id;
                const int16_t rack_type =
                    weapon_id == WEAPON_ANTI_MISSILE_ROCKET ||
                        TECHDATA::weapons[weapon_id].type == WEAPON_TYPE_BOMB
                    ? 0
                    : ship->weapons[i].ammo;
                int16_t space = DESIGN::One_Weapon_Space_Without_M_Level(
                    weapon_id,
                    ship->weapons[i].arc,
                    rack_type,
                    ship->weapons[i].specials);

                int16_t hits = space / 10;
                if (hits < 2) {
                    hits = 1;
                }
                ship->weapon_unit_hits[i] = hits;
                ship->weapon_total_hits[i] = (int16_t)ship->weapons[i].count * hits;
            }
        }

        memset(ship->special_device_hits, 0, sizeof(ship->special_device_hits));

        int device_idx = 0;
        for (int bit = 0; bit < 39; ++bit) {
            if (device_idx >= 8) {
                break;
            }

            if (struct_::Test_Bit_Field(ship->special_device_flags, bit) &&
                !struct_::Test_Bit_Field(ship->special_device_damage_flags, bit)) {
                if (bit == 27 /* SPEC_STRUCTURAL_ANALYZER */ || bit == 11 /* SPEC_HEAVIER_ARMOR */ ||
                    bit == 4 /* SPEC_BATTLE_PODS */ || bit == 14 /* SPEC_REINFORCED_HULL */ ||
                    bit == 24 /* SPEC_STEALTH_FIELD */) {
                    ship->special_device_hits[device_idx] = 0;
                } else {
                    int16_t hits = TECHDATA::specials[bit].space_per_ship_type[ship->size_class] / 10;
                    if (hits < 2) {
                        hits = 1;
                    }
                    hits++;
                    ship->special_device_hits[device_idx] = hits;
                }
                device_idx++;
            }
        }

        if (ship->shield_type <= 0) {
            ship->shield_max_hits = 0;
        } else {
            ship->shield_max_hits = TECHDATA::hull_data[hull_class].shield_hp;
        }

        CMBTFIR2::Clear_Internal_Space(ship);
    }

    void __cdecl Repair_Combat_Ship(int16_t ship_idx, int16_t amount) {
        s_combat_data* ship = &MOX::combat_data[ship_idx];
        int16_t armor_hits = 0;

        if (ship_idx == 0) {
            if (ship->structure_damage > 0) {
                int32_t repair = (amount * ship->structure_damage) / 100;
                if (repair < 2) repair = 1;
                ship->structure_damage -= (int16_t)repair;
                return;
            }
            if (ship->structure_max > 0) {
                int32_t repair = (amount * ship->structure_max) / 100;
                if (repair < 2) repair = 1;
                ship->structure_max -= (int16_t)repair;
                return;
            }
            return;
        }

        if (ship->ship_idx >= 0) {
            armor_hits = INITSHIP::Get_Ship_Armor_Hits(ship->ship_idx);
        } else if (COMBINIT::Is_Combat_Satellite(ship)) {
            const int16_t hull_class =
                COMBINIT::Combat_Satellite_Hull_Class(ship);
            const int16_t hull_armor = TECHDATA::hull_data[hull_class].armor_hp;
            armor_hits = hull_armor +
                (int16_t)((int32_t)TECHDATA::armor[ship->armor_type].ships_bonus *
                    hull_armor / 100);
        } else {
            const int16_t hull_armor =
                TECHDATA::hull_data[ship->size_class].armor_hp;
            armor_hits = hull_armor +
                (int16_t)((int32_t)TECHDATA::armor[ship->armor_type].ships_bonus *
                    hull_armor / 100);
        }

        if (ship->ship_idx < 0 &&
            COMBINIT::Does_Combat_Ship_Have_Special(ship_idx, SPECIAL_HEAVY_ARMOR)) {
            armor_hits *= 3;
        }

        int32_t hull_repair = (amount * (ship->structure_max + armor_hits)) / 100;
        if (hull_repair < 1) hull_repair = 1;

        ship->structure_damage -= (int16_t)hull_repair;
        if (ship->structure_damage < 0) {
            ship->armor_remaining -= ship->structure_damage;
            ship->structure_damage = 0;
            if (ship->armor_remaining > armor_hits) {
                ship->armor_remaining = armor_hits;
            }
        }

        const int32_t proportional_space =
            (int32_t)amount * Max_Total_Internal_Space_For_Repair(ship_idx);
        int32_t pool = Wrap_Int32(
                           (int64_t)proportional_space * systems_repair_percent) /
            10000;
        if (pool < 2) pool = 1;

        if (ship->drive_current_hits < ship->drive_max_hits && pool > 0) {
            int32_t needed = ship->drive_max_hits - ship->drive_current_hits;
            int32_t amt = pool < needed ? (int32_t)pool : needed;
            ship->drive_current_hits += amt;
            pool -= amt;
        }

        if (ship->computer_current_hits < ship->computer_max_hits && pool > 0) {
            int32_t needed = ship->computer_max_hits - ship->computer_current_hits;
            int32_t amt = pool < needed ? (int32_t)pool : needed;
            ship->computer_current_hits += amt;
            pool -= amt;
        }

        for (int i = 0; i < 8 && pool > 0; i++) {
            if (ship->weapon_hits[i] < ship->weapon_total_hits[i]) {
                int16_t needed = ship->weapon_total_hits[i] - ship->weapon_hits[i];
                int16_t amt = (pool < (int32_t)needed) ? (int16_t)pool : needed;
                ship->weapon_hits[i] += amt;
                ship->weapons[i].count =
                    (ship->weapon_hits[i] + ship->weapon_hits_per_unit[i] - 1) /
                    ship->weapon_hits_per_unit[i];
                pool -= (int32_t)amt;
            }
        }

        if (ship->shield_current_hits < ship->shield_max_hits && pool > 0) {
            int32_t needed = ship->shield_max_hits - ship->shield_current_hits;
            int32_t amt = pool < needed ? (int32_t)pool : needed;
            ship->shield_current_hits += amt;
            ship->shield_arc_max = COMBINIT::Calc_Combat_Shield_Max(ship_idx);
            pool -= amt;
        }

        for (int i = 0; i < 8 && pool > 0; i++) {
            if (ship->special_hits[i] < ship->special_device_hits[i]) {
                int16_t needed = ship->special_device_hits[i] - ship->special_hits[i];
                int16_t amt = (pool < (int32_t)needed) ? (int16_t)pool : needed;
                ship->special_hits[i] += amt;
                pool -= (int32_t)amt;
            }
        }

        int special_slot_idx = 0;
        for (int special_id = 0; special_id < SPECIAL_COUNT && special_slot_idx < 8; ++special_id) {
            if (struct_::Test_Bit_Field(ship->special_device_flags, special_id)) {
                if (ship->special_hits[special_slot_idx] > 0) {
                    struct_::Clear_Bit_Field(ship->special_device_damage_flags, special_id);
                    if (special_id == SPECIAL_HARD_SHIELDS) {
                        ship->shield_arc_max = COMBINIT::Calc_Combat_Shield_Max(ship_idx);
                    }
                }
                ++special_slot_idx;
            }
        }
    }

    void __cdecl Clear_Internal_Space(s_combat_data* combat_data) {
        memcpy(
            &combat_data->drive_current_hits,
            &combat_data->drive_max_hits,
            sizeof(combat_data->drive_max_hits) +
                sizeof(combat_data->computer_max_hits) +
                sizeof(combat_data->weapon_total_hits) +
                sizeof(combat_data->weapon_unit_hits) +
                sizeof(combat_data->special_device_hits) +
                sizeof(combat_data->shield_max_hits));

        for (int i = 0; i < 8; ++i) {
            if (combat_data->weapon_hits_per_unit[i] > 0) {
                combat_data->weapons[i].count =
                    combat_data->weapon_hits[i] / combat_data->weapon_hits_per_unit[i];
            }
        }
    }

    int16_t __cdecl Damage_From_Ship_Exploding(int16_t ship_idx) {
        s_combat_data* ship = &MOX::combat_data[ship_idx];
        int16_t damage = (int16_t)(TECHDATA::hull_data[ship->size_class].drive_hp *
            ship_explosion.drive_damage_multiplier);

        if (ship->reflected_damage_pool == INT16_MAX) {
            if (MOX::apply_damage_to_planet != 0) {
                palette::Refresh_Palette();
                GENDRAW::Warning_Box("Absorber Overloaded");
                CMBTDRW1::Refresh_Combat_Screen_Full();
            }
            damage *= 77;
        } else if (COMBINIT::Does_Combat_Ship_Have_Special(
                       ship_idx, SPECIAL_QUANTUM_DETONATOR)) {
            damage *= ship_explosion.quantum_detonator_damage_multiplier;
        }

        return damage;
    }
} // namespace CMBTFIR2
