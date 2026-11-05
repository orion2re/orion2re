#include "pch.h"
#include "combat.h"

#include "lua/lua_game.h"

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

    int16_t Saturate_Int16(int64_t value) {
        if (value > INT16_MAX) {
            return INT16_MAX;
        }
        if (value < INT16_MIN) {
            return INT16_MIN;
        }
        return (int16_t)value;
    }

    int32_t Apply_Ordnance_Bonus(int16_t damage, int16_t owner) {
        return ((int32_t)damage * MOX::ordnance_bonus[owner]) / 100;
    }

    int32_t Octagonal_Distance(int32_t dx, int32_t dy) {
        if (dx < 0) {
            dx = -dx;
        }
        if (dy < 0) {
            dy = -dy;
        }
        return dx > dy ? dx + dy / 2 : dy + dx / 2;
    }

    uint8_t Spherical_Ship_Radius(CMBTFIRE::SPHERICAL_EFFECT effect) {
        switch (effect) {
            case CMBTFIRE::SPHERICAL_EFFECT_PULSAR:
                return CMBTFIRE::spherical_range_to_ship.pulsar;
            case CMBTFIRE::SPHERICAL_EFFECT_SPATIAL_COMPRESSOR:
                return CMBTFIRE::spherical_range_to_ship.spatial_compressor;
            case CMBTFIRE::SPHERICAL_EFFECT_PLASMA_FLUX:
                return CMBTFIRE::spherical_range_to_ship.plasma_flux;
            case CMBTFIRE::SPHERICAL_EFFECT_DRIVE_EXPLOSION:
                return CMBTFIRE::spherical_range_to_ship.drive_explosion;
        }
        return 0;
    }

    uint32_t Spherical_Missile_Radius(
        CMBTFIRE::SPHERICAL_EFFECT effect) {
        switch (effect) {
            case CMBTFIRE::SPHERICAL_EFFECT_PULSAR:
                return CMBTFIRE::spherical_range_to_missile.pulsar;
            case CMBTFIRE::SPHERICAL_EFFECT_SPATIAL_COMPRESSOR:
                return CMBTFIRE::spherical_range_to_missile.spatial_compressor;
            case CMBTFIRE::SPHERICAL_EFFECT_PLASMA_FLUX:
                return CMBTFIRE::spherical_range_to_missile.plasma_flux;
            case CMBTFIRE::SPHERICAL_EFFECT_DRIVE_EXPLOSION:
                return CMBTFIRE::spherical_range_to_missile.drive_explosion;
        }
        return 0;
    }

    bool Can_Bomb_Planet(
        int16_t ship_idx,
        int16_t planet_idx,
        uint8_t bombing_range) {
        int16_t ship_offset_x;
        int16_t ship_offset_y;
        int16_t planet_offset_x;
        int16_t planet_offset_y;
        COMBINIT::Ship_Center_Offsets(
            ship_idx, &ship_offset_x, &ship_offset_y);
        COMBINIT::Ship_Center_Offsets(
            planet_idx, &planet_offset_x, &planet_offset_y);

        int32_t distance_x =
            MOX::combat_data[ship_idx].x * 20 + ship_offset_x -
            (MOX::combat_data[planet_idx].x * 20 + planet_offset_x);
        int32_t distance_y =
            MOX::combat_data[ship_idx].y * 20 + ship_offset_y -
            (MOX::combat_data[planet_idx].y * 20 + planet_offset_y);
        if (distance_x < 0) {
            distance_x = -distance_x;
        }
        if (distance_y < 0) {
            distance_y = -distance_y;
        }

        const int32_t combined_offset_x =
            ship_offset_x + planet_offset_x;
        const int32_t combined_offset_y =
            ship_offset_y + planet_offset_y;
        distance_x = distance_x > combined_offset_x
            ? distance_x - combined_offset_x
            : 0;
        distance_y = distance_y > combined_offset_y
            ? distance_y - combined_offset_y
            : 0;

        const int32_t point_distance = distance_x > distance_y
            ? distance_x + distance_y / 2
            : distance_y + distance_x / 2;
        return point_distance <= 20 * (bombing_range - 1);
    }
} // namespace

namespace CMBTFIRE {
    s_mount_damage mount_dmg[HIGH_ENERGY_FOCUS_PRESENCE_COUNT] = {
        {50, 100, 150},
        {100, 150, 200},
    };

    bool Enveloping_Applies_To_Target(int16_t target_ship_idx) {
        return planet_non_enveloping == 0 || target_ship_idx != 0;
    }

    int32_t Energy_Absorber_Blocked_Damage(int32_t damage) {
        return (int32_t)(((int64_t)damage * absorber_damage_reduction) / 100);
    }

    int32_t Damper_Field_Remaining_Damage(int32_t damage) {
        return (int32_t)(((int64_t)damage * damper_field_ratio) / 100);
    }

    int32_t Ground_Defense_Structure(int16_t armor_bonus) {
        return ((int32_t)(armor_bonus + 100) *
                   ground_defense_armor_multiplier) /
            100;
    }

    bool Spherical_Effect_In_Range(
        int16_t source_ship_idx,
        int16_t target_idx,
        int16_t target_type,
        SPHERICAL_EFFECT effect) {
        int16_t source_offset_x;
        int16_t source_offset_y;
        COMBINIT::Ship_Center_Offsets(
            source_ship_idx, &source_offset_x, &source_offset_y);

        const int32_t source_x =
            MOX::combat_data[source_ship_idx].x * 20 + source_offset_x;
        const int32_t source_y =
            MOX::combat_data[source_ship_idx].y * 20 + source_offset_y;

        if (target_type == TARGET_TYPE_MISSILE) {
            const int32_t distance = Octagonal_Distance(
                source_x - MOX::missile[target_idx].x,
                source_y - MOX::missile[target_idx].y);
            return (uint32_t)distance <= Spherical_Missile_Radius(effect);
        }

        int16_t target_offset_x;
        int16_t target_offset_y;
        COMBINIT::Ship_Center_Offsets(
            target_idx, &target_offset_x, &target_offset_y);
        int32_t distance_x = source_x -
            (MOX::combat_data[target_idx].x * 20 + target_offset_x);
        int32_t distance_y = source_y -
            (MOX::combat_data[target_idx].y * 20 + target_offset_y);
        if (distance_x < 0) {
            distance_x = -distance_x;
        }
        if (distance_y < 0) {
            distance_y = -distance_y;
        }
        distance_x = distance_x > target_offset_x
            ? distance_x - target_offset_x
            : 0;
        distance_y = distance_y > target_offset_y
            ? distance_y - target_offset_y
            : 0;

        const int32_t distance = Octagonal_Distance(distance_x, distance_y);
        return distance < (int32_t)Spherical_Ship_Radius(effect) * 20;
    }

    void __cdecl Destroy_Colony_Defense(int16_t n_to_destroy) {
        int16_t found_count = 0;
        int16_t defense_list[4];

        if (MOX::cmbt_colony_id == -1) {
            return;
        }

        s_colony* colony = &MOX::colony[MOX::cmbt_colony_id];

        if (colony->buildings[BUILDING_MISSILE_BASE] != 0) {
            defense_list[found_count] = BUILDING_MISSILE_BASE;
            found_count++;
        }

        if (colony->buildings[BUILDING_GROUND_BATTERIES] != 0) {
            defense_list[found_count] = BUILDING_GROUND_BATTERIES;
            found_count++;
        }

        if (colony->buildings[BUILDING_STELLAR_CONVERTER] != 0) {
            defense_list[found_count] = BUILDING_STELLAR_CONVERTER;
            found_count++;
        }

        if (colony->buildings[BUILDING_FIGHTER_GARRISON] != 0) {
            defense_list[found_count] = BUILDING_FIGHTER_GARRISON;
            found_count++;
        }

        if (found_count < n_to_destroy) {
            n_to_destroy = found_count;
        }

        if (n_to_destroy > 0) {
            do {
                int random_val = game_random::Random(found_count);
                int idx = random_val - 1;
                int16_t building_id = defense_list[idx];

                ERICNET::Remove_Building(MOX::cmbt_colony_id, building_id);

                switch (building_id) {
                    case BUILDING_MISSILE_BASE:
                        for (int i = 0; i < 8; ++i) {
                            int8_t weapon_type_class = TECHDATA::weapons[MOX::combat_data->weapons[i].weapon_id].type;
                            if (weapon_type_class == WEAPON_TYPE_MISSILE || weapon_type_class == WEAPON_TYPE_TORPEDO) {
                                MOX::combat_data->weapons[i].weapon_id = 0;
                                MOX::combat_data->weapons[i].count = 0;
                            }
                        }
                        break;

                    case BUILDING_GROUND_BATTERIES:
                        for (int i = 0; i < 8; ++i) {
                            int8_t weapon_type_class = TECHDATA::weapons[MOX::combat_data->weapons[i].weapon_id].type;
                            if (weapon_type_class == WEAPON_TYPE_BEAM) {
                                MOX::combat_data->weapons[i].weapon_id = 0;
                                MOX::combat_data->weapons[i].count = 0;
                            }
                        }
                        break;

                    case BUILDING_STELLAR_CONVERTER:
                        for (int i = 0; i < 8; ++i) {
                            int8_t weapon_type_class = TECHDATA::weapons[MOX::combat_data->weapons[i].weapon_id].type;
                            if (weapon_type_class == WEAPON_TYPE_SPECIAL) {
                                MOX::combat_data->weapons[i].weapon_id = 0;
                                MOX::combat_data->weapons[i].count = 0;
                            }
                        }
                        break;

                    case BUILDING_FIGHTER_GARRISON:
                        for (int i = 0; i < 8; ++i) {
                            int8_t weapon_type_class = TECHDATA::weapons[MOX::combat_data->weapons[i].weapon_id].type;
                            if (weapon_type_class == WEAPON_TYPE_FIGHTER) {
                                MOX::combat_data->weapons[i].weapon_id = 0;
                                MOX::combat_data->weapons[i].count = 0;
                            }
                        }
                        break;
                }

                strings::Delete_Structure(idx, defense_list, 2, 4);
                found_count--;
                n_to_destroy--;
            } while (n_to_destroy != 0);
        }

        if (Does_Combat_Planet_Have_Defenses() == 0) {
            struct_::Clear_Bit_Field(
                MOX::combat_data->special_device_flags,
                SPECIAL_DAMPER_FIELD);
            struct_::Clear_Bit_Field(
                MOX::combat_data->special_device_flags,
                SPECIAL_RANGEMASTER_UNIT);
        }
    }

    int16_t __cdecl Does_Combat_Planet_Have_Defenses() {
        if (MOX::cmbt_colony_id != -1) {
            int16_t defenses = 0;

            if (MOX::colony[MOX::cmbt_colony_id].buildings[BUILDING_MISSILE_BASE] != 0) {
                defenses |= COMBAT_PLANET_DEFENSE_MISSILE_BASE;
            }
            if (MOX::colony[MOX::cmbt_colony_id].buildings[BUILDING_GROUND_BATTERIES] != 0) {
                defenses |= COMBAT_PLANET_DEFENSE_GROUND_BATTERIES;
            }
            if (MOX::colony[MOX::cmbt_colony_id].buildings[BUILDING_STELLAR_CONVERTER] != 0) {
                defenses |= COMBAT_PLANET_DEFENSE_STELLAR_CONVERTER;
            }
            if (MOX::colony[MOX::cmbt_colony_id].buildings[BUILDING_FIGHTER_GARRISON] != 0) {
                defenses |= COMBAT_PLANET_DEFENSE_FIGHTER_GARRISON;
            }

            return defenses;
        }
        return 0;
    }

    int16_t __cdecl Is_Combat_Ship_Dead(int16_t ship_idx) {
        int16_t result = 0;

        if (ship_idx != 0) {
            s_combat_data* ship = &MOX::combat_data[ship_idx];

            if (ship->reflected_damage_pool == INT16_MAX) {
                result = 4;
            } else if (ship->structure_damage >= ship->structure_max) {
                result = 1;
            } else if (broken_engine_explodes != 0 &&
                ship->drive_current_hits == 0 &&
                ship->owner <= 9 &&
                ship->ftl_type > 0) {
                result = 3;
            }

            if (result != 0) {
                if (COMBINIT::Does_Combat_Ship_Have_Special(ship_idx, 0x18 /* SPECIAL_STASIS_FIELD */)) {
                    result = 4;
                }
            }
        }

        return result;
    }

    int16_t __cdecl Facing_Shield(int16_t angle) {
        if (angle > 315 || angle <= 45) {
            return 0;
        }
        if (angle <= 135) {
            return 3;
        }
        if (angle <= 225) {
            return 2;
        }
        return 1;
    }

    bool __cdecl Special_Is_On(int16_t ship_idx, int16_t weapon_idx) {
        return MOX::combat_data[ship_idx].weapon_ready_flags[weapon_idx] == 1;
    }

    int8_t Special_Is_Turned_On(int16_t ship_idx, int16_t special_idx) {
        int16_t found_count = 0;
        int8_t is_on = 0;
        bool found_target = false;

        for (int16_t bit_idx = 0; bit_idx < 39; ++bit_idx) {
            if (found_target) {
                break;
            }

            if (special_idx == bit_idx) {
                if (Special_Is_On(ship_idx, found_count)) {
                    is_on = 1;
                    found_target = true;
                }
            }

            if (struct_::Test_Bit_Field(MOX::combat_data[ship_idx].special_device_flags, bit_idx) == 1) {
                found_count++;
            }
        }
        return is_on;
    }

    void __cdecl Remove_Stasis_Beam(int16_t ship_idx) {
        MOX::combat_data[ship_idx].stasis_source_idx = COMBAT_SHIP_NONE;
        COMBAT::Unit_Changed(ship_idx);
    }

    void __cdecl Destroy_Ship_By(int16_t ship_idx, int16_t draw_x, int16_t draw_y, int16_t death_state, int16_t killer_ship_idx) {
        s_combat_data& ship = MOX::combat_data[ship_idx];
        const bool publish_destroyed = ship.unit_status == 0;
        if (publish_destroyed) {
            COMBAT::Resolve_Retreat(ship_idx, COMBAT::DESTROYED);
            COMBAT::Unit_Changed(ship_idx, true);
            COMBAT::Retire_Unit(ship_idx);
        }
        int8_t new_owner;

        if (ship.owner == MOX::combat_attacker_player) {
            new_owner = (int8_t)MOX::combat_defender_player;
        } else {
            new_owner = (int8_t)MOX::combat_attacker_player;
        }

        ship.is_retreating = 0;
        COMBAT1::Remove_Continuing_Effect_Specials(ship_idx);

        if (MOX::apply_damage_to_planet == 1) {
            if (ship.unit_status == 0) {
                int16_t destruction_type = death_state > 1 ? death_state : 1;
                if (MOX::super_fast_flag == 0) {
                    CMBTSPEC::Destroy_Ship_FX(
                        ship_idx, draw_x, draw_y, destruction_type);
                } else {
                    ship.unit_status = 5;
                    if (destruction_type > 1 && ship.previous_owner < 10) {
                        int16_t damage =
                            CMBTFIR2::Damage_From_Ship_Exploding(ship_idx);
                        CMBTSPEC::Do_Spherical_SFX(
                            ship_idx, draw_x, draw_y, destruction_type, damage);
                    }
                }
            }
        } else {
            ship.unit_status = 5;
        }

        Retarget_Missiles_Aimed_At(ship_idx);

        if (ship.previous_owner == 11) {
            for (int16_t i = 1; i < MOX::combat_total_ship_count; ++i) {
                if (MOX::combat_data[i].owner == 11 && i != ship_idx) {
                    COMBAT::Unit_Changed(i, true);
                    COMBAT::Retire_Unit(i);
                    MOX::combat_data[i].owner = new_owner;
                    MOX::combat_data[i].ship_image_seg = nullptr;
                    COMBAT1::Remove_Continuing_Effect_Specials(i);
                }
            }
        }
        if (publish_destroyed) {
            const orion2re::lua::game::s_ship_destroyed_event event = {
                ship_idx,
                killer_ship_idx,
                death_state,
            };
            orion2re::lua::game::Ship_Destroyed(&event);
        }
    }

    void __cdecl Destroy_Ship(int16_t ship_idx, int16_t draw_x, int16_t draw_y, int16_t death_state) {
        Destroy_Ship_By(ship_idx, draw_x, draw_y, death_state, -1);
    }

    int16_t __cdecl Target_Is_Legal_For_Weapon(int16_t weapon_id, uint16_t specials, int16_t target_idx, int16_t target_type) {
        int16_t is_legal = 1;

        if (target_type == 1 /* Missile */) {
            is_legal = CMBTFIRE::Weapon_Can_Fire_At_Missiles(weapon_id, specials);

            int16_t missile_type = MOX::missile[target_idx].type;
            int8_t weapon_type_class = TECHDATA::weapons[missile_type].type;

            if (weapon_type_class == WEAPON_TYPE_FIGHTER) {
                if (weapon_id == WEAPON_ANTI_MISSILE_ROCKET) {
                    is_legal = 0;
                }
            }

            if (weapon_type_class == WEAPON_TYPE_TORPEDO) {
                is_legal = 0;
            }

            return is_legal;
        }

        if (CMBTMOV1::Ship_Is_In_Stasis(target_idx)) {
            is_legal = 0;
        } else {
            if ((TECHDATA::weapons[weapon_id].natural_mods & WEAPON_NATURAL_FLAG_IGNORES_ARMOR_STRUCTURE) != 0) {
                uint8_t prev_owner = MOX::combat_data[target_idx].previous_owner;
                if (prev_owner >= MAX_PLAYERS &&
                    prev_owner != NONPLAYER_SHIP_TYPE_GUARDIAN &&
                    (prev_owner != NONPLAYER_SHIP_TYPE_ANTARAN ||
                        ion_affects_antarans == 0)) {
                    is_legal = 0;
                }
            }

            if (is_legal != 0) {
                int8_t weapon_type_class = TECHDATA::weapons[weapon_id].type;
                if (weapon_type_class == WEAPON_TYPE_BOMB && target_idx != 0) {
                    is_legal = 0;
                } else if (weapon_id == WEAPON_ANTI_MISSILE_ROCKET) {
                    is_legal = 0;
                } else if (target_idx == 0) {
                    if (weapon_id == WEAPON_ASSAULT_SHUTTLE) {
                        is_legal = 0;
                    }
                    if (weapon_type_class == WEAPON_TYPE_SPECIAL) {
                        switch (weapon_id) {
                            case WEAPON_SPATIAL_COMPRESSOR:
                            case WEAPON_STASIS_FIELD:
                            case WEAPON_GYRO_DESTABILIZER:
                            case WEAPON_PLASMA_WEB:
                            case WEAPON_PULSAR:
                            case WEAPON_BLACK_HOLE_GENERATOR:
                            case WEAPON_TRACTOR_BEAM:
                            case WEAPON_PLASMA_FLUX:
                            case WEAPON_CAUSTIC_SLIME:
                                is_legal = 0;
                                break;
                            default:
                                break;
                        }
                    }
                } else if (weapon_id == WEAPON_ASSAULT_SHUTTLE) {
                    if (MOX::combat_data[target_idx].owner > MAX_PLAYERS) {
                        is_legal = 0;
                    }
                }
            }
        }

        if (MOX::combat_data[target_idx].black_hole_flag != 0 && weapon_id == WEAPON_BLACK_HOLE_GENERATOR) {
            is_legal = 0;
        }

        return is_legal;
    }

    void __cdecl Retarget_Missiles_Aimed_At(int16_t unit_idx) {
        for (int16_t i = 0; i < MAX_COMBAT_FLYERS; ++i) {
            s_missile* missile = &MOX::missile[i];

            if (missile->type > 0 && missile->target_is_missile == 0) {
                if (missile->source_unit_idx == unit_idx) {
                    if (missile->target_unit_idx == unit_idx) {
                        CMBTMIS::Kill_Missile(i);
                    }
                } else if (missile->target_unit_idx == unit_idx) {
                    CMBTMIS::Retarget_Missile(i);
                }
            }
        }
    }

    int16_t __cdecl Weapon_In_Range(int16_t weapon_slot_idx, int16_t ship_idx, int16_t target_idx, int16_t target_type, int16_t range) {
        uint16_t specials;
        int16_t weapon_id;
        int16_t in_range;

        specials = MOX::combat_data[ship_idx].weapons[weapon_slot_idx].specials;
        weapon_id = MOX::combat_data[ship_idx].weapons[weapon_slot_idx].weapon_id;

        if (weapon_id == WEAPON_TRACTOR_BEAM && target_type == 0 &&
            MOX::combat_data[ship_idx].special_cooldown == target_idx) {
            return 0;
        }

        const int8_t weapon_type = TECHDATA::weapons[weapon_id].type;
        if (weapon_type == WEAPON_TYPE_BOMB) {
            if (target_idx != 0) {
                return 0;
            }

            uint8_t bombing_range = 3;
            if (COMBINIT::Does_Combat_Ship_Have_Special(
                    ship_idx, SPECIAL_TRANSPORTERS) != 0) {
                bombing_range = BOARDING::board_range.transporters;
            }
            return Can_Bomb_Planet(
                ship_idx, target_idx, bombing_range);
        }

        if (range == 0) {
            switch (weapon_id) {
                case WEAPON_PULSAR:
                    return Spherical_Effect_In_Range(
                        ship_idx, target_idx, target_type, SPHERICAL_EFFECT_PULSAR);
                case WEAPON_SPATIAL_COMPRESSOR:
                    return Spherical_Effect_In_Range(
                        ship_idx, target_idx, target_type, SPHERICAL_EFFECT_SPATIAL_COMPRESSOR);
                case WEAPON_PLASMA_FLUX:
                    return Spherical_Effect_In_Range(
                        ship_idx, target_idx, target_type, SPHERICAL_EFFECT_PLASMA_FLUX);
                default:
                    break;
            }

            if (target_type == 1) {
                range = CMBTAI::Range_To_Missile(ship_idx, target_idx);
            } else {
                range = CMBTAI::Range_To_Ship(ship_idx, target_idx);
            }
        }

        const s_combat_data* source = &MOX::combat_data[ship_idx];
        if (ship_idx == 0 || COMBINIT::Is_Combat_Satellite(source)) {
            range -= defensive_range_bonus;
            if (range < 0) {
                range = 0;
            }
        }

        // Double range flag
        if (specials & 0x4) {
            range *= 2;
        }

        // Half range flag
        if (specials & 0x2) {
            range /= 2;
        }

        in_range = 0;
        switch (weapon_type) {
            case 0: // Standard Beam / Direct fire
                if (range <= 8) {
                    return 1;
                }
                break;

            case 1:
            case 2:
            case 4: // Missiles and Torpedoes
                in_range = 1;
                if (weapon_id == WEAPON_PROTON_TORPEDO &&
                    range > weapon_range.proton_torpedo) {
                    return 0;
                }
                if (weapon_id == WEAPON_DRAGON_BREATH &&
                    range > weapon_range.dragon_breath) {
                    return 0;
                }
                break;

            case 5: // Specials
                if (weapon_id == WEAPON_TRACTOR_BEAM && range <= weapon_range.tractor_beam) return 1;
                if (weapon_id == WEAPON_STASIS_FIELD && range <= weapon_range.stasis_field) return 1;
                if (weapon_id == WEAPON_ANTI_MISSILE_ROCKET && range <= weapon_range.anti_missile_rocket) return 1;
                if (weapon_id == WEAPON_GYRO_DESTABILIZER && range <= weapon_range.gyro_destabilizer) return 1;
                if (weapon_id == WEAPON_PLASMA_WEB && range <= weapon_range.plasma_web) return 1;
                if (weapon_id == WEAPON_PULSAR && range <= 1) return 1;
                if (weapon_id == WEAPON_BLACK_HOLE_GENERATOR && range <= weapon_range.black_hole_generator) return 1;
                if (weapon_id == WEAPON_STELLAR_CONVERTER) return 1;
                if (weapon_id == WEAPON_SPATIAL_COMPRESSOR && range <= 1) return 1;
                if (weapon_id == WEAPON_PLASMA_FLUX && range <= 1) return 1;
                if (weapon_id == WEAPON_CAUSTIC_SLIME && range <= weapon_range.caustic_slime) return 1;
                break;

            default:
                break;
        }

        return in_range;
    }

    int16_t __cdecl Weapon_Can_Fire_At_Missiles(int16_t weapon_idx, uint16_t flags) {
        int16_t result = 0;

        if (weapon_idx == 0) {
            return 0;
        }

        int8_t weapon_type_class = TECHDATA::weapons[weapon_idx].type;

        if ((uint32_t)(int32_t)weapon_type_class > WEAPON_TYPE_SPECIAL) {
            return 0;
        }

        switch (weapon_type_class) {
            case WEAPON_TYPE_BEAM:
                if ((flags & 0x0002) == 0) {
                    return 1;
                }
                break;

            case WEAPON_TYPE_FIGHTER:
                if (weapon_idx == WEAPON_INTERCEPTOR ||
                    (heavy_fighter_can_scramble != 0 &&
                        weapon_idx == WEAPON_HEAVY_FIGHTER)) {
                    result = 1;
                }
                // fallthrough

            case WEAPON_TYPE_SPECIAL:
                if (weapon_idx == WEAPON_PULSAR || weapon_idx == WEAPON_SPATIAL_COMPRESSOR || weapon_idx == WEAPON_ANTI_MISSILE_ROCKET) {
                    result = 1;
                }
                break;

            case WEAPON_TYPE_MISSILE:
            case WEAPON_TYPE_TORPEDO:
            case WEAPON_TYPE_BOMB:
            default:
                break;
        }

        return result;
    }

    void __cdecl Fire_Fighter_Beam(int16_t missile_idx, int16_t shield_arc, int16_t target_is_missile, int16_t* total_damage, int16_t* total_struct_damage) {
        int16_t struct_damage = 0;

        int16_t target_idx = MOX::missile[missile_idx].target_unit_idx;
        int16_t source_owner = MOX::combat_data[MOX::missile[missile_idx].source_unit_idx].owner;

        *total_damage = 0;
        *total_struct_damage = 0;

        int16_t weapon_id = MOX::fighter_beam_type[source_owner];
        uint16_t specials = 0;
        uint16_t weapon_flags = TECHDATA::weapons[weapon_id].natural_mods;

        if (weapon_flags & WEAPON_NATURAL_FLAG_IGNORES_SHIELDS) {
            specials = 0x40;
        }

        uint32_t weapon_damage = (uint32_t)Apply_Ordnance_Bonus(
            TECHDATA::weapons[weapon_id].max_damage, source_owner);
        uint32_t weapon_min_damage = (uint32_t)Apply_Ordnance_Bonus(
            TECHDATA::weapons[weapon_id].min_damage, source_owner);

        int64_t chance_to_hit;
        if (target_is_missile == 0) {
            int64_t ocv = CMBTMIS::Fighter_Ocv(source_owner);
            chance_to_hit = ocv - MOX::combat_data[target_idx].tac_val_defense;
        } else {
            int64_t ocv = CMBTMIS::Fighter_Ocv(source_owner);
            int64_t dcv = CMBTMIS::Missile_Dcv(MOX::missile[target_idx].type, MOX::combat_data[MOX::missile[target_idx].source_unit_idx].owner);
            chance_to_hit = ocv - dcv;
        }

        if (fighters_use_point_defense != 0) {
            chance_to_hit += 25;

            const uint32_t modifier =
                mount_dmg[HIGH_ENERGY_FOCUS_ABSENT].pd;
            uint64_t adjusted_min_damage =
                ((uint64_t)weapon_min_damage * modifier + 50) / 100;
            if (adjusted_min_damage < 1) {
                adjusted_min_damage = 1;
            }
            if (adjusted_min_damage > INT16_MAX) {
                adjusted_min_damage = INT16_MAX;
            }
            weapon_min_damage = (uint32_t)adjusted_min_damage;

            uint64_t adjusted_max_damage =
                ((uint64_t)weapon_damage * modifier + 50) / 100;
            if (adjusted_max_damage < weapon_min_damage) {
                adjusted_max_damage = weapon_min_damage;
            }
            if (adjusted_max_damage > INT16_MAX) {
                adjusted_max_damage = INT16_MAX;
            }
            weapon_damage = (uint32_t)adjusted_max_damage;
        }

        if (weapon_min_damage > INT16_MAX) {
            weapon_min_damage = INT16_MAX;
        }
        if (weapon_damage > INT16_MAX) {
            weapon_damage = INT16_MAX;
        }
        const int16_t minimum_damage = (int16_t)weapon_min_damage;
        const int16_t maximum_damage = (int16_t)weapon_damage;
        int16_t damage_diff = maximum_damage - minimum_damage;

        for (int16_t i = 0; i < MOX::missile[missile_idx].quantity; i++) {
            if (CMBTMIS::Is_Missile_Target_Alive(missile_idx) == 0) {
                break;
            }

            int64_t rand_val = game_random::Random(100);
            if (rand_val > 95) {
                rand_val = 100;
            } else {
                rand_val += chance_to_hit;
                if (rand_val > 100) {
                    rand_val = 100;
                }
            }

            if (rand_val >= 40) {
                int16_t damage_done = minimum_damage;
                if (damage_diff > 0) {
                    damage_done = Saturate_Int16(
                        (int64_t)damage_done +
                        ((rand_val - 40) * (damage_diff + 1)) / 60);
                    if (fighter_beam_no_plus_one != 0 &&
                        damage_done > maximum_damage) {
                        damage_done = maximum_damage;
                    }
                }

                if (target_is_missile == 0) {
                    struct_damage = 0;
                    if (target_idx == 0) {
                        damage_done /= 2;
                    }

                    CMBTFIRE::Apply_Ship_Damage(target_idx, &damage_done, &struct_damage, shield_arc, specials, weapon_flags, 0, 0, 0);

                    *total_damage += damage_done;
                    *total_struct_damage += struct_damage;
                } else {
                    Apply_Damage_To_Missile(target_idx, &damage_done, specials, weapon_flags);
                    MOX::missile[target_idx].quantity -= damage_done;
                    COMBAT::Missile_Changed(target_idx);
                    *total_damage += damage_done;
                }
            }
        }

        if (target_is_missile == 1) CMBTMIS::Record_Missile_Hit(target_idx, *total_damage);
        if (CMBTMIS::Is_Missile_Target_Alive(missile_idx) == 0) {
            if (target_is_missile == 1) {
                CMBTMIS::Retarget_Missile(missile_idx);
                CMBTMIS::Kill_Missile(target_idx);
            }
        }
    }

    void __cdecl Apply_Damage_To_Missile(int16_t missile_idx, int16_t* damage_io, uint16_t specials, uint16_t weapon_flags) {
        s_missile* missile = &MOX::missile[missile_idx];
        int16_t destroyed_count = 0;

        // Calculate health for a single missile in the stack
        int16_t max_hits = (int16_t)CMBTMIS::Get_Missile_Hits(
            missile->type,
            missile->owner,
            missile->specials);

        // Cap the incoming damage to the max hits of a single missile
        if (max_hits < *damage_io) {
            *damage_io = max_hits;
        }

        // Add incoming damage to already accumulated hits on the current missile
        int16_t total_hits = missile->hits_taken + *damage_io;
        *damage_io = total_hits;

        // Determine how many missiles are destroyed by the accumulated damage
        while (max_hits <= *damage_io) {
            *damage_io -= max_hits;
            destroyed_count++;
        }

        // Remaining damage stays as hits_taken on the next missile in stack
        missile->hits_taken = *damage_io;

        // Return the number of destroyed missiles through the IO pointer
        *damage_io = destroyed_count;
    }

    void __cdecl Fire_Fighter_Bomb(int16_t missile_idx, int16_t damage_modifier, int16_t* total_damage, int16_t* total_internal_damage) {
        int16_t target_unit_idx;
        int16_t source_unit_idx;
        int16_t weapon_id;
        uint16_t weapon_flags;
        uint16_t damage_special_flags;
        int16_t min_damage;
        int16_t max_damage;
        int16_t damage_range;
        int16_t ordnance_owner;
        int16_t roll;
        int16_t damage;
        int16_t internal_damage;
        int8_t owner;

        target_unit_idx = MOX::missile[missile_idx].target_unit_idx;
        source_unit_idx = MOX::missile[missile_idx].source_unit_idx;
        owner = MOX::combat_data[source_unit_idx].owner;

        *total_damage = 0;
        *total_internal_damage = 0;

        weapon_id = MOX::fighter_bomb_type[owner];
        weapon_flags = TECHDATA::weapons[weapon_id].natural_mods;
        damage_special_flags = 0;

        if ((weapon_flags & WEAPON_NATURAL_FLAG_IGNORES_SHIELDS) != 0) {
            damage_special_flags = 0x40;
        }

        min_damage = TECHDATA::weapons[weapon_id].min_damage;
        max_damage = TECHDATA::weapons[weapon_id].max_damage;
        ordnance_owner = MOX::missile[missile_idx].owner;
        min_damage = (int16_t)Apply_Ordnance_Bonus(min_damage, ordnance_owner);
        max_damage = (int16_t)Apply_Ordnance_Bonus(max_damage, ordnance_owner);
        damage_range = max_damage - min_damage;

        for (int16_t i = 0; i < MOX::missile[missile_idx].quantity; ++i) {
            roll = (int16_t)game_random::Random(100);
            if (roll > 0) {
                internal_damage = 0;
                if (damage_range > 0) {
                    // Specific weighted damage calculation logic found in original assembly
                    damage = (int16_t)((((100 / (damage_range * 2)) + roll) * damage_range) / 100) + min_damage;
                } else {
                    damage = min_damage;
                }

                Apply_Ship_Damage(target_unit_idx, &damage, &internal_damage, damage_modifier, damage_special_flags, weapon_flags, 0, 0, 0);

                *total_damage += damage;
                *total_internal_damage += internal_damage;
            }
        }
    }

    uint16_t __cdecl Apply_Ship_Damage(
        int16_t target_ship_idx,
        int16_t* dmg_out,
        int16_t* sys_dmg,
        int16_t shield_arc,
        uint16_t specials,
        uint16_t weapon_flags,
        int16_t has_structural_analyzer,
        int16_t force_armor_bypass,
        int16_t internal_only) {
        if (*dmg_out > 0) COMBAT::Unit_Changed(target_ship_idx);
        int16_t internal_hit_type = 0;
        int16_t internal_hit_count = 0;
        int16_t structural_bonus_damage = 0;
        uint16_t retval = 0;

        *sys_dmg = 0;

        if (target_ship_idx == 0) {
            retval = Apply_Damage_To_Planet(dmg_out, specials, weapon_flags);
            *dmg_out += structural_bonus_damage;
            return retval;
        }

        if (COMBINIT::Does_Combat_Ship_Have_Special(target_ship_idx, SPECIAL_ENERGY_ABSORBER)) {
            int16_t reflected =
                (int16_t)Energy_Absorber_Blocked_Damage(*dmg_out);
            if (reflected < 2) {
                reflected = 1;
            }
            *dmg_out -= reflected;
            MOX::combat_data[target_ship_idx].reflected_damage_pool =
                Wrap_Int16(
                    (int32_t)MOX::combat_data[target_ship_idx]
                        .reflected_damage_pool +
                    reflected);
        }

        if (COMBINIT::Does_Combat_Ship_Have_Special(
                target_ship_idx, SPECIAL_DAMPER_FIELD)) {
            *dmg_out = (int16_t)Damper_Field_Remaining_Damage(
                (uint16_t)*dmg_out);
        }

        int16_t remaining_shield_hits = MOX::combat_data[target_ship_idx].shield_arc_current[shield_arc];
        if ((specials & 0x40) != 0) {
            if (!COMBINIT::Does_Combat_Ship_Have_Special(target_ship_idx, 13 /* Multi-Phased Shields */)) {
                remaining_shield_hits = 0;
            }
        }

        if (MOX::combat_data[target_ship_idx].shield_type > 0 &&
            MOX::combat_data[target_ship_idx].shield_current_hits > 0) {
            if (COMBINIT::Does_Combat_Ship_Have_Special(
                    target_ship_idx, SPECIAL_HARD_SHIELDS) &&
                (specials & WEAPON_MOD_FLAG_SHIELD_PIERCING) != 0 &&
                (weapon_flags & WEAPON_NATURAL_FLAG_ANTI_SHIELD) == 0) {
                specials ^= WEAPON_MOD_FLAG_SHIELD_PIERCING;
            }

            if (!(specials & 0x40)) {
                if (!MOX::in_nebula_flag || COMBINIT::Does_Combat_Ship_Have_Special(target_ship_idx, 13)) {
                    *dmg_out -= TECHDATA::shields[MOX::combat_data[target_ship_idx].shield_type].shield_class;
                    if (COMBINIT::Does_Combat_Ship_Have_Special(target_ship_idx, 13)) {
                        *dmg_out -= TECHDATA::hard_shields_damage_reduction;
                    }
                }
            }

            if (*dmg_out < 0) {
                *dmg_out = 0;
            }

            if (*dmg_out > 0) {
                if (!MOX::in_nebula_flag || COMBINIT::Does_Combat_Ship_Have_Special(target_ship_idx, 13)) {
                    retval |= 1;
                }
            }

            if (*dmg_out < remaining_shield_hits) {
                MOX::combat_data[target_ship_idx].shield_arc_current[shield_arc] -= *dmg_out;
                *sys_dmg = *dmg_out;
                *dmg_out = 0;
            } else {
                *dmg_out -= remaining_shield_hits;
                *sys_dmg = remaining_shield_hits;
                MOX::combat_data[target_ship_idx].shield_arc_current[shield_arc] -= remaining_shield_hits;
            }
        }

        if (weapon_flags & WEAPON_NATURAL_FLAG_ANTI_SHIELD) {
            *dmg_out = 0;
        }

        if ((weapon_flags & WEAPON_NATURAL_FLAG_KILLS_MARINES) != 0) {
            if (!COMBINIT::Does_Combat_Ship_Have_Special(
                    target_ship_idx, marine_kill_damage_blocker)) {
                if (MOX::combat_data[target_ship_idx].marine_count != 0xFF) {
                    CMBTFIR2::Kill_Crew(
                        target_ship_idx, *dmg_out / marine_kill_damage);
                }
            }
        }

        if (has_structural_analyzer != 0) {
            const int64_t adjusted_damage =
                ((int64_t)*dmg_out *
                    ((uint32_t)analyzer_bonus + 100)) /
                100;
            *dmg_out = (int16_t)adjusted_damage;
        }

        int16_t ignore_armor;
        if (force_armor_bypass == 0) {
            ignore_armor = (specials & 8) != 0;
            if (COMBINIT::Does_Combat_Ship_Have_Special(target_ship_idx, SPECIAL_HEAVY_ARMOR) ||
                MOX::combat_data[target_ship_idx].armor_type == ARMOR_XENTRONIUM_ARMOR) {
                ignore_armor = 0;
            }
        } else {
            ignore_armor = 1;
        }

        if ((weapon_flags & WEAPON_NATURAL_FLAG_IGNORES_ARMOR_STRUCTURE) && MOX::combat_data[target_ship_idx].owner > 9) {
            *dmg_out /= 4;
        }

        if (*dmg_out > 0) {
            retval |= 2;

            if (weapon_flags & WEAPON_NATURAL_FLAG_EXTRA_STRUCTURAL_DAMAGE) {
                structural_bonus_damage = (int16_t)
                    TECHDATA::Extra_Structural_Damage(*dmg_out);
                MOX::combat_data[target_ship_idx].structure_damage =
                    (int16_t)(MOX::combat_data[target_ship_idx].structure_damage +
                        structural_bonus_damage);
            }

            int16_t bypassed_damage;
            if (ignore_armor) {
                bypassed_damage = *dmg_out;
                int16_t remaining_damage = *dmg_out;

                while (remaining_damage > 0 && !CMBTFIRE::Is_Combat_Ship_Dead(target_ship_idx)) {
                    CMBTFIR2::Select_Internal(target_ship_idx, &internal_hit_type, &internal_hit_count, force_armor_bypass, weapon_flags & WEAPON_NATURAL_FLAG_IGNORES_ARMOR_STRUCTURE, 1, internal_only);
                    if (internal_hit_type == -1) {
                        remaining_damage = 0;
                    }
                    CMBTFIR2::Apply_Internal_Damage(target_ship_idx, &remaining_damage, internal_hit_type, internal_hit_count, 1);
                }
            } else {
                bypassed_damage = 0;
            }

            int16_t remaining_damage = *dmg_out - bypassed_damage;
            while (remaining_damage > 0 && !CMBTFIRE::Is_Combat_Ship_Dead(target_ship_idx)) {
                CMBTFIR2::Select_Internal(target_ship_idx, &internal_hit_type, &internal_hit_count, force_armor_bypass, weapon_flags & WEAPON_NATURAL_FLAG_IGNORES_ARMOR_STRUCTURE, 0, internal_only);
                if (internal_hit_type == -1) {
                    remaining_damage = 0;
                }
                CMBTFIR2::Apply_Internal_Damage(target_ship_idx, &remaining_damage, internal_hit_type, internal_hit_count, 1);
            }
        }

        *dmg_out += structural_bonus_damage;
        return retval;
    }

    void __cdecl Draw_Energy_Absorber(int16_t ship_idx, int16_t x, int16_t y) {
        int16_t lbx_entry;
        s_animation_header* seg;
        int16_t frame_count;
        int16_t off_x, off_y;
        int frame;
        int16_t width, height;

        int8_t size_class = MOX::combat_data[ship_idx].size_class;

        lbx_entry = (int16_t)((-1 - (int)size_class) / 2) + 0x33;
        if (size_class == 4) {
            lbx_entry--;
        }

        seg = (s_animation_header*)buffer::Buffer_Reload("cmbtsfx.lbx", (int)lbx_entry, MOX::global_cache_seg);
        frame_count = animate::Get_Animation_Frame_Count(seg);
        COMBINIT::Ship_Center_Offsets(ship_idx, &off_x, &off_y);

        if (ship_idx == MOX::dont_draw_ship) {
            frame = 1;
        } else {
            frame = ((int)(MOX::ship_frame / 2) + ship_idx) % (int)frame_count;
        }

        animate::Set_Animation_Frame(seg, frame);
        width = animate::Get_Width(seg);
        height = animate::Get_Height(seg);

        animate::Draw(x + off_x - (width / 2), y + off_y - (height / 2), seg);
    }

    int16_t __cdecl Get_Beam_Range_To_Hit_Bonus(int16_t ship_idx, int16_t range) {
        if (COMBINIT::Does_Combat_Ship_Have_Special(ship_idx, SPECIAL_RANGEMASTER_UNIT) == 1) {
            return MOX::ranged_to_hit_penalty[range / 3];
        }

        return MOX::ranged_to_hit_penalty[range];
    }

    void __cdecl Draw_BHG(int16_t ship_idx, int16_t x_adj, int16_t y_adj) {
        int16_t off_x;
        int16_t off_y;
        int16_t v1;
        int16_t v2;
        int16_t w;
        int16_t h;

        COMBINIT::Ship_Center_Offsets(ship_idx, &off_x, &off_y);

        s_combat_data* ship = &MOX::combat_data[ship_idx];

        off_x += (ship->x - MOX::cur_combat_x) * 20 + x_adj;
        off_y += (ship->y - MOX::cur_combat_y) * 20 + y_adj;

        allocate::Push_Block(MOX::screen_seg);
        allocate::Allocate_Next_Block(MOX::screen_seg, 22500 /* 0x57e4 */);
        s_animation_header* h1 = (s_animation_header*)allocate::Allocate_Next_Block(MOX::screen_seg, 22500 /* 0x57e4 */);

        int32_t entry_num = (-1 - ship->size_class) / 2 + 46 /* 0x2e */;
        s_animation_header* anim = (s_animation_header*)buffer::Buffer_Reload("cmbtsfx.lbx", entry_num, MOX::global_cache_seg);

        int16_t frame = (MOX::ship_frame / 2) % 8;

        if (ship->black_hole_flag != 1) {
            frame += 8;
        }

        animate::Set_Animation_Frame(anim, frame);
        bitmap::Draw_Picture_To_Bitmap(h1, anim);

        palette::Restore_Glass_Remap_Block(MOX::bhg_remap_colors);
        bitmap::Set_Picture_Glassed(h1);

        bitmap::Get_Bitmap_Actual_Size(h1, &v1, &v2, &w, &h);
        animate::Draw(off_x - w / 2, off_y - h / 2, h1);

        allocate::Pop_Block(MOX::screen_seg);
        palette::Restore_Glass_Remap_Block(MOX::shield_remap_colors);
    }

    int16_t __cdecl Fire_Bomb(int16_t ship_idx, int16_t unused_target_idx, int16_t weapon_slot_idx) {
        int16_t total_damage = 0;
        (void)unused_target_idx;
        int32_t ordnance_bonus = MOX::ordnance_bonus[MOX::combat_data[ship_idx].owner];
        int16_t weapon_id = MOX::combat_data[ship_idx].weapons[weapon_slot_idx].weapon_id;

        int16_t min_damage = (int16_t)((TECHDATA::weapons[weapon_id].min_damage * ordnance_bonus) / 100);
        int16_t max_damage = (int16_t)((TECHDATA::weapons[weapon_id].max_damage * ordnance_bonus) / 100);

        if (MOX::combat_data[ship_idx].weapons[weapon_slot_idx].shots_left > 0) {
            int32_t damage_range = max_damage - min_damage;

            for (int16_t i = 0; i < MOX::combat_data[ship_idx].weapons[weapon_slot_idx].shots_left; ++i) {
                int16_t current_damage = (int16_t)(((game_random::Random(100) * damage_range) / 100) + min_damage);
                Apply_Damage_To_Planet(&current_damage, 0, TECHDATA::weapons[weapon_id].natural_mods);
                total_damage += current_damage;
            }
        }

        Record_Unit_Hit(0, total_damage, 0, (TECHDATA::weapons[weapon_id].natural_mods & WEAPON_NATURAL_FLAG_BIOLOGICAL) != 0);
        return total_damage;
    }

    int16_t __cdecl Apply_Damage_To_Planet(int16_t* damage, uint16_t specials, uint16_t flags) {
        s_combat_bomb_result bomb_stats;
        int16_t defense_hits = 0;
        int16_t colony_hits = 0;
        int16_t shield_hit_flag = 0;

        struct_::Clear_Structure(&bomb_stats, sizeof(s_combat_bomb_result));

        int16_t best_armor_idx = INITSHIP::Best_Armor(MOX::combat_data->owner);
        int16_t armor_threshold = (int16_t)Ground_Defense_Structure(
            TECHDATA::armor[best_armor_idx].ships_bonus);
        uint16_t civilian_armor_hits = civilian_armor[best_armor_idx];

        (void)specials;

        if (flags & WEAPON_NATURAL_FLAG_BIOLOGICAL) {
            MOX::bio_weapon_flag = 1;
            MOX::combat_bomb_result.is_bio_weapon = 1;
            bomb_stats.is_bio_weapon = 1;

            if ((COMBINIT::planet.shields_block_bioweapons != 0 &&
                    MOX::combat_data->shield_type == BUILDING_NO_BUILDING) ||
                (COMBINIT::planet.shields_block_bioweapons == 0 &&
                    MOX::combat_data->shield_type != BUILDING_BARRIER_SHIELD)) {
                s_player* defender = &MOX::player[MOX::combat_defender_player];

                if (defender->tech_applications[TECH_APP_MICROBIOTICS] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    *damage = *damage / 2;
                } else if (defender->tech_applications[TECH_APP_UNIVERSAL_ANTIDOTE] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    *damage = (int16_t)((*damage + (((int32_t)*damage >> 31) & 3)) >> 2);
                }

                if ((int)game_random::Random(100) <= *damage) {
                    colony_hits = 1;
                    bomb_stats.attacker_idx = (int8_t)MOX::combat_attacker_player;
                    MOX::combat_bomb_result.bio_hits_remaining += 1;
                    bomb_stats.bomb_hits_remaining = 0;
                    bomb_stats.colony_idx = MOX::cmbt_colony_id;
                    bomb_stats.bio_hits_remaining = 1;
                    bomb_stats.is_planetary_hit = 0;
                    MOX::combat_bomb_result.is_bio_weapon = 1;

                    if (MOX::apply_damage_to_planet != 0 && MOX::cmbt_colony_id != -1) {
                        BOMB::Apply_Bomb_Hits(&bomb_stats);
                    }
                }
            }
        } else {
            if (CMBTFIRE::Does_Combat_Planet_Have_Defenses() > 0) {
                if (COMBINIT::Does_Combat_Ship_Have_Special(
                        0, SPECIAL_DAMPER_FIELD) != 0) {
                    defense_hits =
                        *damage - MOX::combat_data->shield_arc_current[1];
                    if (defense_hits < 0) {
                        defense_hits = 0;
                    }
                    defense_hits = (int16_t)Damper_Field_Remaining_Damage(
                        (uint16_t)defense_hits);
                } else {
                    defense_hits =
                        *damage - MOX::combat_data->shield_arc_current[0];
                }
                if (MOX::combat_data->shield_type > 0) {
                    shield_hit_flag = 1;
                }
                colony_hits = 0;
            } else {
                defense_hits = 0;
                if (MOX::combat_data->shield_type > 0) {
                    shield_hit_flag = 1;
                }
                colony_hits = *damage - MOX::combat_data->shield_arc_current[1];
            }

            if (defense_hits > 0 &&
                !(flags & (WEAPON_NATURAL_FLAG_ANTI_SHIELD | WEAPON_NATURAL_FLAG_IGNORES_ARMOR_STRUCTURE))) {
                if (flags & WEAPON_NATURAL_FLAG_EXTRA_STRUCTURAL_DAMAGE) {
                    defense_hits += (int16_t)
                        TECHDATA::Extra_Structural_Damage(defense_hits);
                }
                MOX::combat_data->structure_damage += defense_hits;
                if (MOX::combat_data->structure_damage >= armor_threshold) {
                    int16_t hits = MOX::combat_data->structure_damage / armor_threshold;
                    *damage = hits;
                    CMBTFIRE::Destroy_Colony_Defense(hits);
                    MOX::combat_data->structure_damage %= armor_threshold;
                }
            } else {
                defense_hits = 0;
            }

            if (colony_hits > 0) {
                if (MOX::cmbt_colony_id != -1 &&
                    !(flags & (WEAPON_NATURAL_FLAG_ANTI_SHIELD | WEAPON_NATURAL_FLAG_IGNORES_ARMOR_STRUCTURE))) {
                    if (flags & WEAPON_NATURAL_FLAG_EXTRA_STRUCTURAL_DAMAGE) {
                        colony_hits *= 2;
                    }

                    if (MOX::combat_data->structure_max < 32000) {
                        MOX::combat_data->structure_max += colony_hits;
                    }

                    if (MOX::combat_data->structure_max >= civilian_armor_hits) {
                        bomb_stats.attacker_idx = (int8_t)MOX::combat_attacker_player;
                        bomb_stats.colony_idx = MOX::cmbt_colony_id;
                        bomb_stats.is_planetary_hit = 1;
                        MOX::combat_bomb_result.is_planetary_hit = 1;
                        bomb_stats.bomb_hits_remaining = MOX::combat_data->structure_max / civilian_armor_hits;

                        if (MOX::apply_damage_to_planet != 0 && MOX::cmbt_colony_id != -1) {
                            BOMB::Apply_Bomb_Hits(&bomb_stats);

                            if (bomb_stats.buildings_destroyed[BUILDING_RADIATION_SHIELD] != 0) {
                                MOX::combat_data->shield_type = 0;
                                MOX::combat_data->shield_arc_current[0] -= TECHDATA::planetary_shields[PLANETARY_RADIATION_SHIELD].absorption;
                                MOX::combat_data->shield_arc_current[1] -= TECHDATA::planetary_shields[PLANETARY_RADIATION_SHIELD].absorption;
                            } else if (bomb_stats.buildings_destroyed[BUILDING_BARRIER_SHIELD] != 0) {
                                MOX::combat_data->shield_type = 0;
                                MOX::combat_data->shield_arc_current[0] -= TECHDATA::planetary_shields[PLANETARY_BARRIER_SHIELD].absorption;
                                MOX::combat_data->shield_arc_current[1] -= TECHDATA::planetary_shields[PLANETARY_BARRIER_SHIELD].absorption;
                            } else if (bomb_stats.buildings_destroyed[BUILDING_FLUX_SHIELD] != 0) {
                                MOX::combat_data->shield_type = 0;
                                MOX::combat_data->shield_arc_current[0] -= TECHDATA::planetary_shields[PLANETARY_FLUX_SHIELD].absorption;
                                MOX::combat_data->shield_arc_current[1] -= TECHDATA::planetary_shields[PLANETARY_FLUX_SHIELD].absorption;
                            }
                        }
                        MOX::combat_data->structure_max -=
                            (int16_t)(bomb_stats.total_hits_processed * civilian_armor_hits);
                    }
                } else {
                    if (MOX::cmbt_colony_id != -1) {
                        colony_hits = 0;
                    }
                }
            } else {
                colony_hits = 0;
            }
        }

        *damage = defense_hits + colony_hits;
        return shield_hit_flag;
    }

    int16_t __cdecl Fire_Ship(int16_t source_ship_idx, int16_t sx, int16_t sy, int16_t target_ship_idx, int16_t tx, int16_t ty, int16_t flag) {
        s_beam_fx_entry beam_fx_entries[16];
        int16_t beam_hit_offset_y = 0;
        int16_t beam_hit_offset_x = 0;
        uint8_t target_arc_mask = 0;
        uint8_t target_death_status = 0;
        int16_t target_center_offset_x = 0;
        int16_t target_center_offset_y = 0;
        int16_t secondary_damage = 0;
        int16_t primary_damage = 0;
        int16_t continue_firing = 0;
        int16_t primary_damage_total = 0;
        int16_t secondary_damage_total = 0;
        int16_t total_hits_on_target = 0;
        int16_t beam_fx_count = 0;
        int16_t weapon_type_class = 0;
        int16_t weapon_slot_idx = 0;
        int16_t fire_result_flags = 0;

        struct_::Clear_Structure(beam_fx_entries, sizeof(beam_fx_entries));

        if (flag == 0) {
            int16_t bearing = CMBTDRW1::Relative_Bearing(source_ship_idx, target_ship_idx);
            target_arc_mask = CMBTDRW1::Get_Weapon_Arc(bearing);
            COMBINIT::Ship_Center_Offsets(target_ship_idx, &target_center_offset_x, &target_center_offset_y);
        } else {
            int16_t bearing = CMBTDRW1::Relative_Bearing_XY(
                source_ship_idx,
                MOX::missile[target_ship_idx].x,
                MOX::missile[target_ship_idx].y);
            target_arc_mask = CMBTDRW1::Get_Weapon_Arc(bearing);
            target_center_offset_x = 0;
            target_center_offset_y = 0;
        }

        if (MOX::apply_damage_to_planet == 0) {
            target_arc_mask = WEAPON_FIRING_ARC_ALL_SECTORS;
        }

        Pre_Weapon_Specials(source_ship_idx, target_ship_idx, flag);

        if (flag != 1 && CMBTMOV1::Ship_Is_In_Stasis(target_ship_idx) != 0) {
        } else {
            bool target_dead = false;
            if (flag == 0) {
                if (Is_Combat_Ship_Dead(target_ship_idx) != 0) {
                    target_dead = true;
                }
            } else {
                if (MOX::missile[target_ship_idx].quantity <= 0) {
                    target_dead = true;
                }
            }

            if (!target_dead) {
                for (weapon_slot_idx = 0; weapon_slot_idx < 8; ++weapon_slot_idx) {
                    if (target_death_status != 0 || Is_Combat_Ship_Dead(source_ship_idx) != 0) {
                        break;
                    }

                    total_hits_on_target = 0;
                    int16_t target_secondary_total = 0;
                    secondary_damage_total = 0;
                    primary_damage_total = 0;

                    s_combat_weapon_slot* weapon = &MOX::combat_data[source_ship_idx].weapons[weapon_slot_idx];
                    if (weapon->weapon_id != 0) {
                        fire_result_flags = 0;
                        weapon_type_class = TECHDATA::weapons[weapon->weapon_id].type;

                        if (COMBAT1::Weapon_Is_On(source_ship_idx, weapon_slot_idx) != 0 &&
                            (target_arc_mask & weapon->arc) != 0) {
                            if (weapon_type_class == WEAPON_TYPE_TORPEDO) {
                                if (weapon->ammo != 0) continue;
                            } else {
                                if (weapon->ammo == 0) continue;
                            }

                            if (Weapon_In_Range(weapon_slot_idx, source_ship_idx, target_ship_idx, flag, 0) != 0 &&
                                Target_Is_Legal_For_Weapon(weapon->weapon_id, weapon->specials, target_ship_idx, flag) != 0) {
                                COMBAT::Unit_Changed(source_ship_idx);
                                if (weapon->shots_left == 0 && weapon->status == 1) {
                                    weapon->shots_left = weapon->count;
                                    weapon->status = 0;
                                }

                                if (MOX::combat_data[source_ship_idx].special_status_flag == 1) {
                                    if (MOX::apply_damage_to_planet == 1 && MOX::super_fast_flag == 0) {
                                        CMBTSPEC::Draw_Cloak(source_ship_idx, 2);
                                    }
                                    MOX::combat_data[source_ship_idx].special_status_flag = 2;
                                    COMBAT1::Recalculate_Combat_Ship(source_ship_idx);
                                } else if (MOX::combat_data[source_ship_idx].special_status_flag == 4) {
                                    if (MOX::apply_damage_to_planet == 1 && MOX::super_fast_flag == 0) {
                                        CMBTSPEC::Draw_Cloak(source_ship_idx, 5);
                                    }
                                    MOX::combat_data[source_ship_idx].special_status_flag = 6;
                                }

                                if (MOX::apply_damage_to_planet == 1 && MOX::super_fast_flag == 0) {
                                    bool needs_snap = false;
                                    if (flag == 0) {
                                        if (CMBTDRW1::Ship_Completely_On_Screen(target_ship_idx) == 0) {
                                            needs_snap = true;
                                        }
                                    } else {
                                        if (CMBTMIS::On_Combat_Screen(tx / 20, ty / 20) == 0) {
                                            needs_snap = true;
                                        }
                                    }

                                    if (needs_snap && weapon_type_class == WEAPON_TYPE_BEAM) {
                                        int16_t snap_x, snap_y;
                                        if (flag == 0) {
                                            snap_x = MOX::combat_data[target_ship_idx].x;
                                            snap_y = MOX::combat_data[target_ship_idx].y;
                                        } else {
                                            snap_x = tx / 20;
                                            snap_y = ty / 20;
                                        }
                                        COMBINIT::Snap_Center_Combat_Screen(snap_x, snap_y);
                                    }

                                    if (beam_fx_entries[0].weapon_id != 0 && weapon_type_class != WEAPON_TYPE_BEAM) {
                                        BEAMS::Beam_SFX(
                                            source_ship_idx,
                                            sx + MOX::cur_combat_x * -20,
                                            sy + MOX::cur_combat_y * -20,
                                            target_ship_idx,
                                            tx + MOX::cur_combat_x * -20,
                                            ty + MOX::cur_combat_y * -20,
                                            beam_fx_entries,
                                            flag);
                                        beam_fx_count = 0;
                                        struct_::Clear_Structure(beam_fx_entries, sizeof(beam_fx_entries));
                                    }
                                }

                                continue_firing = 1;
                                while (true) {
                                    if (weapon->shots_left <= 0) break;
                                    if (continue_firing != 1) break;
                                    if (Is_Combat_Ship_Dead(source_ship_idx) != 0) break;

                                    switch (weapon_type_class) {
                                        case WEAPON_TYPE_BEAM: {
                                            int16_t fire_res = Fire_Beam_Weapon(
                                                source_ship_idx, target_ship_idx, weapon->weapon_id, weapon->specials, flag, &beam_hit_offset_x, &beam_hit_offset_y, &primary_damage);
                                            fire_result_flags = fire_res;

                                            if (primary_damage > 0) {
                                                int16_t bearing = CMBTDRW1::Relative_Bearing(source_ship_idx, target_ship_idx);
                                                int16_t shield_facing = Facing_Shield(bearing);
                                                secondary_damage = 0;
                                                Apply_Ship_Damage(
                                                    source_ship_idx, &primary_damage, &secondary_damage, shield_facing, 0, TECHDATA::weapons[weapon->weapon_id].natural_mods, 0, 0, 0);
                                                primary_damage_total += primary_damage;
                                                secondary_damage_total += secondary_damage;
                                            }

                                            beam_fx_entries[beam_fx_count].result_flags |= (fire_result_flags & 1);
                                            if ((fire_result_flags & 2) != 0) {
                                                beam_fx_entries[beam_fx_count].special_flag = 1;
                                            }

                                            beam_fx_entries[beam_fx_count].shot_count++;
                                            beam_fx_entries[beam_fx_count].accum_hit_x += beam_hit_offset_x;
                                            beam_fx_entries[beam_fx_count].accum_hit_y += beam_hit_offset_y;

                                            uint16_t base_specials = weapon->specials;
                                            beam_fx_entries[beam_fx_count].specials = base_specials;

                                            if ((TECHDATA::weapons[weapon->weapon_id].natural_mods & WEAPON_NATURAL_FLAG_ENVELOPING) != 0) {
                                                beam_fx_entries[beam_fx_count].specials = base_specials | WEAPON_MOD_FLAG_ENVELOPING;
                                            }

                                            beam_fx_entries[beam_fx_count].is_damage_entry = 0;
                                            total_hits_on_target += beam_hit_offset_x;
                                            target_secondary_total += beam_hit_offset_y;
                                            weapon->shots_left--;
                                            break;
                                        }
                                        case WEAPON_TYPE_MISSILE:
                                        case WEAPON_TYPE_TORPEDO:
                                        case WEAPON_TYPE_FIGHTER: {
                                            CMBTMIS::Fire_Missile(source_ship_idx, target_ship_idx, weapon_slot_idx, flag);
                                            if (weapon_type_class == WEAPON_TYPE_TORPEDO) {
                                                weapon->ammo = TECHDATA::weapons[weapon->weapon_id].ammo;
                                            } else {
                                                weapon->ammo--;
                                            }
                                            weapon->shots_left = 0;
                                            break;
                                        }
                                        case WEAPON_TYPE_BOMB: {
                                            int16_t bomb_hits = Fire_Bomb(source_ship_idx, target_ship_idx, weapon_slot_idx);
                                            total_hits_on_target = bomb_hits;
                                            weapon->shots_left = 0;
                                            weapon->ammo--;
                                            break;
                                        }
                                        case WEAPON_TYPE_SPECIAL: {
                                            Fire_Special_Weapon(source_ship_idx, sx, sy, target_ship_idx, weapon_slot_idx, flag);
                                            if (weapon->weapon_id == WEAPON_STASIS_FIELD ||
                                                weapon->weapon_id == WEAPON_BLACK_HOLE_GENERATOR) {
                                                continue_firing = 0;
                                            }
                                            break;
                                        }
                                    }

                                    COMBINIT::real_attack_flag = 1;
                                    fire_result_flags = 1;

                                    if (weapon->status == 2) {
                                        weapon->status = 0;
                                    }

                                    if (flag == 0) {
                                        target_death_status = (uint8_t)Is_Combat_Ship_Dead(target_ship_idx);
                                    } else {
                                        int16_t remaining_missiles = MOX::missile[target_ship_idx].quantity - total_hits_on_target;
                                        if (remaining_missiles <= 0 || MOX::missile[target_ship_idx].type == 0) {
                                            target_death_status = 1;
                                        }
                                    }

                                    if (target_death_status != 0) {
                                        break;
                                    }
                                }

                                if (weapon_type_class == WEAPON_TYPE_BEAM) {
                                    if (flag == 0)
                                        Record_Unit_Hit(target_ship_idx, total_hits_on_target, target_secondary_total);
                                    else
                                        CMBTMIS::Record_Missile_Hit(target_ship_idx, total_hits_on_target);
                                    Record_Unit_Hit(source_ship_idx, primary_damage_total, secondary_damage_total);
                                }
                                if (MOX::combat_data[source_ship_idx].special_status_flag == 3) {
                                    MOX::combat_data[source_ship_idx].special_status_flag = 2;
                                }
                                if (MOX::combat_data[source_ship_idx].special_status_flag == 7) {
                                    MOX::combat_data[source_ship_idx].special_status_flag = 6;
                                }

                                if (fire_result_flags == 1 && MOX::apply_damage_to_planet == 1 && MOX::super_fast_flag == 0) {
                                    if (weapon_type_class == WEAPON_TYPE_BEAM) {
                                        beam_fx_entries[beam_fx_count].weapon_id = weapon->weapon_id;
                                        if (primary_damage_total > 0 || secondary_damage_total > 0) {
                                            struct_::Copy_Structure(&beam_fx_entries[beam_fx_count + 1], &beam_fx_entries[beam_fx_count], 15);
                                            beam_fx_entries[beam_fx_count + 1].is_damage_entry = 1;
                                            beam_fx_entries[beam_fx_count + 1].accum_hit_x = primary_damage_total;
                                            beam_fx_entries[beam_fx_count + 1].special_flag = (primary_damage_total > 0) ? 1 : 0;
                                            beam_fx_entries[beam_fx_count + 1].accum_hit_y = secondary_damage_total;
                                            beam_fx_entries[beam_fx_count + 1].result_flags = (secondary_damage_total > 0) ? 1 : 0;

                                            beam_fx_count++;
                                            video::Set_Page_Back();
                                            CMBTDRW1::Draw_Ship_Status(14, 0x178, MOX::cur_ship);
                                            CMBTDRW1::Draw_Grey_Damage_Bars(14, 0x178, MOX::cur_ship);
                                            CMBTDRW1::Draw_Crew_Level_Stars(MOX::cur_ship, 15, 0x17B);
                                            video::Set_Page_Off();
                                        }
                                        beam_fx_count++;
                                    } else if (weapon_type_class == WEAPON_TYPE_BOMB) {
                                        CMBTDRW1::Draw_Bombs(source_ship_idx, target_ship_idx, weapon->weapon_id, total_hits_on_target);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        if (beam_fx_entries[0].weapon_id != 0) {
            BEAMS::Beam_SFX(
                source_ship_idx,
                sx + MOX::cur_combat_x * -20,
                sy + MOX::cur_combat_y * -20,
                target_ship_idx,
                tx + MOX::cur_combat_x * -20,
                ty + MOX::cur_combat_y * -20,
                beam_fx_entries,
                flag);
        }

        int16_t src_dead = Is_Combat_Ship_Dead(source_ship_idx);
        if (src_dead > 0) {
            Destroy_Ship(source_ship_idx, sx, sy, src_dead);
        }

        if (target_death_status != 0 && flag == 0 && MOX::combat_data[target_ship_idx].unit_status == 0) {
            Destroy_Ship_By(target_ship_idx, tx, ty, target_death_status, source_ship_idx);
        }

        MOX::last_ship_status = -1;
        return fire_result_flags;
    }

    void __cdecl Pre_Weapon_Specials(int16_t unit_idx, int16_t target_idx, int16_t target_type) {
        int16_t range;
        int16_t special_idx;
        int16_t bit_idx;

        if (target_type != 0) {
            CMBTMIS::Missile_Type(target_idx);
            if (target_type != 0) {
                range = CMBTAI::Range_To_Missile(unit_idx, target_idx);
            } else {
                range = CMBTAI::Range_To_Ship(unit_idx, target_idx);
            }
        } else {
            range = CMBTAI::Range_To_Ship(unit_idx, target_idx);
        }

        special_idx = 0;
        for (bit_idx = 0; bit_idx < 39; ++bit_idx) {
            // Check for Energy Absorber (Bit 10)
            if (bit_idx == SPECIAL_ENERGY_ABSORBER && target_type == 0 && range <= 8) {
                if (MOX::combat_data[unit_idx].reflected_damage_pool > 0) {
                    if (CMBTFIRE::Special_Is_On(unit_idx, special_idx)) {
                        CMBTFIRE::Energy_Absorber_Attack(unit_idx, target_idx);
                    }
                }
            }

            if (struct_::Test_Bit_Field(MOX::combat_data[unit_idx].special_device_flags, bit_idx)) {
                special_idx++;
            }
        }
    }

    void __cdecl Fire_Special_Weapon(int16_t ship_idx, int16_t x, int16_t y, int16_t target_idx, int16_t weapon_slot_idx, int16_t target_type) {
        int16_t dx;
        int16_t dy;
        int16_t range;

        COMBINIT::Ship_Center_Offsets(ship_idx, &dx, &dy);

        if (target_type == 0) {
            range = CMBTAI::Range_To_Ship(ship_idx, target_idx);
        } else {
            range = special::Range(x + dx, y + dy, MOX::missile[target_idx].x, MOX::missile[target_idx].y);
            range = ((range + 19) / 20 + 2) / 3;
        }

        switch (MOX::combat_data[ship_idx].weapons[weapon_slot_idx].weapon_id) {
            case WEAPON_SPATIAL_COMPRESSOR:
                if (MOX::apply_damage_to_planet == 1) {
                    CMBTSPEC::Do_Spherical_SFX(ship_idx, x, y, 2, MOX::combat_data[ship_idx].weapons[weapon_slot_idx].shots_left);
                }
                MOX::combat_data[ship_idx].weapons[weapon_slot_idx].shots_left = 0;
                break;
            case WEAPON_STASIS_FIELD:
                if (target_idx != 0 && target_type == 0) {
                    CMBTSPEC::Draw_Stasis_Beam(ship_idx, target_idx);
                    Place_Ship_In_Stasis(target_idx, ship_idx);
                    CMBTDRW1::Refresh_Combat_Screen_Full();
                    MOX::combat_data[ship_idx].weapons[weapon_slot_idx].shots_left--;
                }
                break;
            case WEAPON_ANTI_MISSILE_ROCKET:
                if (target_type != 0) {
                    int16_t type = MOX::missile[target_idx].type;
                    if (TECHDATA::weapons[type].type == WEAPON_TYPE_MISSILE || TECHDATA::weapons[type].type == WEAPON_TYPE_FIGHTER) {
                        if (CMBTMIS::Missile_Owner(target_idx) != MOX::combat_data[ship_idx].owner) {
                            CMBTSPEC::Anti_Missile_Rockets(ship_idx, x, y, target_idx, weapon_slot_idx, range);
                        }
                    }
                }
                break;
            case WEAPON_GYRO_DESTABILIZER:
                if (target_idx != 0 && target_type == 0) {
                    CMBTSPEC::Gyro_Destablizer(ship_idx, target_idx, MOX::combat_data[ship_idx].weapons[weapon_slot_idx].shots_left);
                    MOX::combat_data[ship_idx].weapons[weapon_slot_idx].shots_left = 0;
                }
                break;
            case WEAPON_PLASMA_WEB:
            case WEAPON_CAUSTIC_SLIME:
                if (target_idx != 0) {
                    CMBTSPEC::Plasma_Web(ship_idx, target_idx, weapon_slot_idx);
                    MOX::combat_data[ship_idx].weapons[weapon_slot_idx].shots_left = 0;
                } else if (MOX::apply_damage_to_planet == 0) {
                    MOX::combat_data[ship_idx].weapons[weapon_slot_idx].shots_left = 0;
                }
                break;
            case WEAPON_PULSAR:
                if (MOX::apply_damage_to_planet == 1) {
                    CMBTSPEC::Do_Spherical_SFX(ship_idx, x, y, 1, MOX::combat_data[ship_idx].weapons[weapon_slot_idx].shots_left);
                }
                MOX::combat_data[ship_idx].weapons[weapon_slot_idx].shots_left = 0;
                break;
            case WEAPON_BLACK_HOLE_GENERATOR: {
                s_combat_weapon_slot* weapon = &MOX::combat_data[ship_idx].weapons[weapon_slot_idx];
                if (target_idx != 0 && target_type == 0 && weapon->shots_left > 0) {
                    BHG(ship_idx, target_idx);
                    weapon->shots_left--;
                }
                break;
            }
            case WEAPON_STELLAR_CONVERTER:
                if (target_type == 0) {
                    CMBTSPEC::Stellar_Converter(ship_idx, target_idx, MOX::combat_data[ship_idx].weapons[weapon_slot_idx].shots_left);
                    MOX::combat_data[ship_idx].weapons[weapon_slot_idx].shots_left = 0;
                }
                break;
            case WEAPON_TRACTOR_BEAM: {
                const int16_t tractor_target_idx = MOX::combat_data[ship_idx].special_cooldown == COMBAT_SHIP_NONE
                    ? int16_t{-1}
                    : MOX::combat_data[ship_idx].special_cooldown;
                if (target_idx == 0) {
                    if (MOX::apply_damage_to_planet == 0) {
                        MOX::combat_data[ship_idx].weapons[weapon_slot_idx].shots_left = 0;
                        return;
                    }
                } else {
                    if (target_type == 0 && tractor_target_idx != target_idx && MOX::combat_data[ship_idx].weapons[weapon_slot_idx].shots_left > 0) {
                        CMBTSPEC::Tractor_Beam(ship_idx, target_idx);
                        MOX::combat_data[ship_idx].weapons[weapon_slot_idx].shots_left = 0;
                    }
                }

                if (MOX::combat_data[ship_idx].special_cooldown != COMBAT_SHIP_NONE) {
                    MOX::combat_data[ship_idx].weapons[weapon_slot_idx].shots_left = 0;
                }
                break;
            }
            case WEAPON_PLASMA_FLUX:
                if (MOX::apply_damage_to_planet == 1) {
                    CMBTSPEC::Do_Spherical_SFX(ship_idx, x, y, 0, MOX::combat_data[ship_idx].weapons[weapon_slot_idx].shots_left);
                }
                MOX::combat_data[ship_idx].weapons[weapon_slot_idx].shots_left = 0;
                break;
            default:
                break;
        }
    }

    int16_t __cdecl Fire_Beam_Weapon(int16_t source, int16_t target, int16_t weapon_id, uint16_t specials, uint16_t weapon_flags, int16_t* hit_x, int16_t* hit_y, int16_t* damage) {
        int16_t shield_arc = 0;
        *hit_x = 0;
        *damage = 0;
        *hit_y = 0;
        int32_t weapon_dmg_bonus = 0;
        uint16_t return_flags = 0;

        uint16_t weapon_flags_00 = TECHDATA::weapons[weapon_id].natural_mods;
        if (weapon_flags_00 & WEAPON_NATURAL_FLAG_IGNORES_SHIELDS) {
            specials |= 0x40;
        }
        if (weapon_flags_00 & WEAPON_NATURAL_FLAG_ENVELOPING) {
            specials |= WEAPON_MOD_FLAG_ENVELOPING;
        }
        if (weapon_flags_00 & WEAPON_NATURAL_FLAG_CONTINUOUS) {
            specials |= 0x10;
        }

        int16_t max_damage = TECHDATA::weapons[weapon_id].max_damage;
        int16_t min_damage = TECHDATA::weapons[weapon_id].min_damage;

        int16_t range;
        if (weapon_flags == 0) {
            shield_arc = CMBTFIRE::Facing_Shield(CMBTDRW1::Relative_Bearing(target, source));
            range = CMBTAI::Range_To_Ship(source, target);
        } else {
            range = CMBTAI::Range_To_Missile(source, target);
        }

        int32_t hit_modifier = 0;
        CMBTFIRE::Get_Beam_Weapon_Modifiers(source, range, specials, weapon_id, &hit_modifier, &weapon_dmg_bonus);

        int64_t hit_chance;
        if (weapon_flags == 0) {
            hit_chance = MOX::combat_data[source].tac_val_attack - MOX::combat_data[target].tac_val_defense;
        } else {
            int16_t missile_type = MOX::missile[target].type;
            int16_t missile_owner = MOX::combat_data[MOX::missile[target].source_unit_idx].owner;
            int64_t dcv = CMBTMIS::Missile_Dcv(missile_type, missile_owner);
            hit_chance = MOX::combat_data[source].tac_val_attack - dcv;
        }

        int16_t total_shots;
        if ((specials & 0x80) == 0) {
            total_shots = 1;
        } else {
            total_shots = 3;
            hit_chance -= 20;
        }

        if ((specials & 0x10) != 0) {
            hit_chance += 25;
        }

        const int32_t dmg_mod =
            MOX::ordnance_bonus[MOX::combat_data[source].owner] +
            weapon_dmg_bonus - 100;

        int64_t min_dmg_calc = ((int64_t)min_damage * dmg_mod + 50) / 100;
        if (min_dmg_calc <= 1) {
            min_dmg_calc = 1;
        }
        min_damage = Saturate_Int16(min_dmg_calc);

        int64_t max_dmg_calc = ((int64_t)max_damage * dmg_mod + 50) / 100;
        if (max_dmg_calc <= min_damage) {
            max_dmg_calc = min_damage;
        }
        max_damage = Saturate_Int16(max_dmg_calc);

        int16_t continue_firing = 1;
        int16_t damage_variance = max_damage - min_damage;

        int32_t range_modifier = hit_threshold_fixed_value;
        if (simplified_beam_formula != 0) {
            hit_chance += hit_modifier;
        } else {
            range_modifier -= hit_modifier;
        }
        if (range_modifier > 95) {
            range_modifier = 95;
        }

        int16_t shield_hit_y = 0;

        for (int16_t loop_counter = 0; loop_counter < total_shots; ++loop_counter) {
            if (continue_firing == 0) break;

            int64_t roll = game_random::Random(100);
            if ((weapon_flags_00 & WEAPON_NATURAL_FLAG_ALWAYS_HITS) != 0) {
                roll = 100;
            }

            if (roll > 95) {
                roll = 100;
            } else {
                roll += hit_chance;
                if (roll > 100) roll = 100;
            }

            // Preserve legacy in-range missile/ship-slot aliasing, but never read past the ship allocation.
            if (target >= 0 && target < MAX_COMBAT_SHIPS && COMBINIT::Does_Combat_Ship_Have_Special(target, SPECIAL_DISPLACEMENT_DEVICE)) {
                if (game_random::Random(100) <=
                    CMBTFIR2::combat_bonus.displacement_device_beam_miss_percent) {
                    roll = -1000;
                }
            }

            if (roll < range_modifier) continue;

            int64_t current_damage_calc = min_damage;
            if (damage_variance > 0) {
                current_damage_calc =
                    ((int64_t)(roll - range_modifier) *
                        (damage_variance + 1)) /
                        (100 - range_modifier) +
                    min_damage;
            }
            if (current_damage_calc > max_damage) {
                current_damage_calc = max_damage;
            }
            int16_t current_damage = (int16_t)current_damage_calc;

            if (weapon_flags == 0) {
                if (target == 0) {
                    current_damage /= 2;
                }

                if (COMBINIT::Does_Combat_Ship_Have_Special(target, SPECIAL_REFLECTION_FIELD)) {
                    if ((int)game_random::Random(
                            current_damage + CMBTFIR2::combat_bonus.reflection_field_strength) > current_damage) {
                        *damage += current_damage;
                        current_damage = 0;
                    }
                }

                if (current_damage > 0) {
                    if ((specials & WEAPON_MOD_FLAG_ENVELOPING) != 0 &&
                        Enveloping_Applies_To_Target(target)) {
                        for (int16_t shield_idx = 0; shield_idx < 4; ++shield_idx) {
                            int16_t dmg_val = current_damage;
                            int16_t special1 = COMBINIT::Does_Combat_Ship_Have_Special(source, SPECIAL_ACHILLES_TARGETING_UNIT);
                            int16_t special20 = COMBINIT::Does_Combat_Ship_Have_Special(source, SPECIAL_STRUCTURAL_ANALYZER);
                            return_flags |= CMBTFIRE::Apply_Ship_Damage(target, &dmg_val, &shield_hit_y, shield_idx, specials, weapon_flags_00, special20, special1, 0);
                            *hit_x += dmg_val;
                            *hit_y += shield_hit_y;
                        }
                    } else {
                        int16_t special1 = COMBINIT::Does_Combat_Ship_Have_Special(source, SPECIAL_ACHILLES_TARGETING_UNIT);
                        int16_t special20 = COMBINIT::Does_Combat_Ship_Have_Special(source, SPECIAL_STRUCTURAL_ANALYZER);
                        return_flags |= CMBTFIRE::Apply_Ship_Damage(target, &current_damage, &shield_hit_y, shield_arc, specials, weapon_flags_00, special20, special1, 0);
                        *hit_x += current_damage;
                        *hit_y += shield_hit_y;
                    }
                }
            } else {
                return_flags = 2;
                int16_t missile_hits =
                    (specials & WEAPON_MOD_FLAG_ENVELOPING) ? 4 : 1;
                for (int16_t i = 0; i < missile_hits; ++i) {
                    int16_t dmg_val = current_damage;
                    CMBTFIRE::Apply_Damage_To_Missile(target, &dmg_val, specials, weapon_flags_00);
                    if (MOX::missile[target].quantity <= 0 || MOX::missile[target].type == 0) {
                        continue_firing = 0;
                    }
                    *hit_x += dmg_val;
                }
            }
        }

        return (int16_t)return_flags;
    }

    void __cdecl Energy_Absorber_Attack(int16_t source_ship_idx, int16_t target_ship_idx) {
        s_beam_fx_entry beam_fx_entries[16];
        struct_::Clear_Structure(beam_fx_entries, sizeof(beam_fx_entries));

        int32_t damage_bonus = 0;
        int16_t target_shield_arc = Facing_Shield(CMBTDRW1::Relative_Bearing(target_ship_idx, source_ship_idx));

        int16_t range = CMBTAI::Range_To_Ship(source_ship_idx, target_ship_idx);
        int32_t hit_bonus;
        Get_Beam_Weapon_Modifiers(source_ship_idx, range, 0, 3, &hit_bonus, &damage_bonus);

        int32_t bonus = MOX::ordnance_bonus[MOX::combat_data[source_ship_idx].owner] + damage_bonus;
        int16_t damage = Saturate_Int16(
            ((int64_t)(bonus - 100) *
                    MOX::combat_data[source_ship_idx].reflected_damage_pool +
                50) /
            100);

        if (COMBINIT::Does_Combat_Ship_Have_Special(target_ship_idx, SPECIAL_DISPLACEMENT_DEVICE) != 0) {
            if (game_random::Random(100) <=
                CMBTFIR2::combat_bonus.displacement_device_beam_miss_percent) {
                damage = 0;
            }
        }

        if (target_ship_idx == 0) {
            damage /= 2;
        }

        uint16_t damage_result_flags = 0;
        int16_t shield_hit_result = 0;

        if (damage > 0) {
            int16_t analyzer_bonus = COMBINIT::Does_Combat_Ship_Have_Special(source_ship_idx, SPECIAL_ACHILLES_TARGETING_UNIT);
            int16_t structural_analyzer_bonus = COMBINIT::Does_Combat_Ship_Have_Special(source_ship_idx, SPECIAL_STRUCTURAL_ANALYZER);
            damage_result_flags = Apply_Ship_Damage(target_ship_idx, &damage, &shield_hit_result, target_shield_arc, 0, 0, structural_analyzer_bonus, analyzer_bonus, 0);
        }

        int16_t is_dead = Is_Combat_Ship_Dead(target_ship_idx);
        Record_Unit_Hit(target_ship_idx, damage, shield_hit_result);

        if (MOX::apply_damage_to_planet != 0 && MOX::super_fast_flag == 0) {
            if (CMBTDRW1::Ship_Completely_On_Screen(target_ship_idx) == 0) {
                COMBINIT::Snap_Center_Combat_Screen(MOX::combat_data[target_ship_idx].x, MOX::combat_data[target_ship_idx].y);
            }

            beam_fx_entries[0].weapon_id = 0x2e;
            beam_fx_entries[0].shot_count = 1;
            beam_fx_entries[0].special_flag = damage_result_flags & 1;
            beam_fx_entries[0].result_flags = (damage_result_flags & 2) != 0;
            beam_fx_entries[0].accum_hit_x = damage;
            beam_fx_entries[0].accum_hit_y = shield_hit_result;
            beam_fx_entries[0].is_damage_entry = 0;
            beam_fx_entries[0].specials = 0;

            BEAMS::Beam_SFX(
                source_ship_idx,
                (MOX::combat_data[source_ship_idx].x - MOX::cur_combat_x) * 20,
                (MOX::combat_data[source_ship_idx].y - MOX::cur_combat_y) * 20,
                target_ship_idx,
                (MOX::combat_data[target_ship_idx].x - MOX::cur_combat_x) * 20,
                (MOX::combat_data[target_ship_idx].y - MOX::cur_combat_y) * 20,
                beam_fx_entries,
                0);
        }

        if (is_dead != 0) {
            Destroy_Ship_By(target_ship_idx,
                MOX::combat_data[target_ship_idx].x * 20,
                MOX::combat_data[target_ship_idx].y * 20,
                is_dead,
                source_ship_idx);
        }

        MOX::combat_data[source_ship_idx].reflected_damage_pool = 0;
    }

    void __cdecl Get_Beam_Weapon_Modifiers(int16_t ship_idx, int16_t range, uint16_t wpn_modifier_flags, int16_t wpn_idx, int32_t* hit_bonus_out, int32_t* dmg_mod_out) {
        int16_t effective_range;

        *dmg_mod_out = 100;
        *hit_bonus_out = 0;

        if (wpn_modifier_flags & WEAPON_MOD_FLAG_HEAVY_MOUNT) {
            effective_range = range / 2;
        } else if (wpn_modifier_flags & WEAPON_MOD_FLAG_POINT_DEFENSE) {
            effective_range = range * 2;
            if (effective_range > 8) {
                effective_range = 8;
            }
        } else {
            effective_range = range;
        }

        *hit_bonus_out = CMBTFIRE::Get_Beam_Range_To_Hit_Bonus(ship_idx, effective_range);

        const bool has_double_range_penalty =
            (TECHDATA::weapons[wpn_idx].natural_mods &
                WEAPON_NATURAL_FLAG_DOUBLE_RANGE_PENALTY) != 0;
        if (has_double_range_penalty && double_range_accuracy_penalty != 0) {
            *hit_bonus_out *= 2;
        }

        const int focus_state =
            COMBINIT::Does_Combat_Ship_Have_Special(
                ship_idx, SPECIAL_HIGH_ENERGY_FOCUS)
            ? HIGH_ENERGY_FOCUS_PRESENT
            : HIGH_ENERGY_FOCUS_ABSENT;
        const s_mount_damage* mount_damage = &mount_dmg[focus_state];

        if (!(wpn_modifier_flags & 0x20) && !(TECHDATA::weapons[wpn_idx].natural_mods & WEAPON_NATURAL_FLAG_NO_RANGE_DISSIPATION)) {
            int32_t range_damage_penalty =
                MOX::ranged_damage_penalty[effective_range];
            if (double_range_damage_penalty != 0) {
                range_damage_penalty *= 2;
            }
            *dmg_mod_out += range_damage_penalty;
        }

        uint16_t configured_damage;
        if (wpn_modifier_flags & WEAPON_MOD_FLAG_POINT_DEFENSE) {
            *hit_bonus_out += 25;
            configured_damage = mount_damage->pd;
        } else if (wpn_modifier_flags & WEAPON_MOD_FLAG_HEAVY_MOUNT) {
            configured_damage = mount_damage->heavy;
        } else {
            configured_damage = mount_damage->nomod;
        }

        *dmg_mod_out += (int32_t)configured_damage - 100;
        if (*dmg_mod_out < 0) {
            *dmg_mod_out = 0;
        }
    }

    void __cdecl Place_Ship_In_Stasis(int16_t ship_idx, int16_t source_idx) {
        MOX::combat_data[ship_idx].stasis_source_idx = (uint16_t)source_idx;
        CMBTFIRE::Retarget_Missiles_Aimed_At(ship_idx);
        COMBAT1::Remove_Continuing_Effect_Specials(ship_idx);
    }

    void __cdecl BHG(int16_t source_idx, int16_t target_idx) {
        int16_t x1, y1, x2, y2;
        int16_t frame_count;
        s_animation_header* anim_seg;
        s_animation_header* temp_bitmap;
        int32_t sound_handle;
        int16_t speed_step;
        int16_t loop_counter;

        if (MOX::apply_damage_to_planet != 0 && MOX::super_fast_flag == 0) {
            anim_seg = (s_animation_header*)buffer::Buffer_Reload("cmbtsfx.lbx", 0x2F /* 47 */, MOX::global_cache_seg);
            animate::Draw_Palette(anim_seg);
            frame_count = animate::Get_Animation_Frame_Count(anim_seg);

            COMBINIT::Ship_Center_Offsets(source_idx, &x1, &y1);
            x1 += (MOX::combat_data[source_idx].x - MOX::cur_combat_x) * 20;
            y1 += (MOX::combat_data[source_idx].y - MOX::cur_combat_y) * 20;

            COMBINIT::Ship_Center_Offsets(target_idx, &x2, &y2);
            x2 += (MOX::combat_data[target_idx].x - MOX::cur_combat_x) * 20;
            y2 += (MOX::combat_data[target_idx].y - MOX::cur_combat_y) * 20;

            y2 -= animate::Get_Height(anim_seg) / 2;

            if (MOX::speedx2_flag == 1 || MOX::net_flag != 0) {
                speed_step = 2;
            } else {
                speed_step = 1;
            }

            allocate::Push_Block(MOX::screen_seg);
            temp_bitmap = (s_animation_header*)allocate::Allocate_Next_Block(MOX::screen_seg, 50000 /* 0xC350 */);

            sound_handle = ERICNET::Mox_Play_Sound(10);
            loop_counter = 0;

            do {
                timer::Mark_Time();
                video::Set_Page_Off();

                int16_t current_frame = loop_counter % frame_count;
                loop_counter += speed_step;

                animate::Set_Animation_Frame(anim_seg, (int32_t)current_frame);
                anim_seg = (s_animation_header*)buffer::Buffer_Reload("cmbtsfx.lbx", 0x2F, MOX::global_cache_seg);
                bitmap::Draw_Picture_To_Bitmap((s_animation_header*)temp_bitmap, anim_seg);

                graphics::Set_Window(0, 0, 639 /* 0x27F */, 350 /* 0x15E */);
                video::Clip_On();
                bitmap::Draw_Bitmap_Line(x1, y1, x2, y2, temp_bitmap);
                graphics::Reset_Window();
                video::Clip_Off();

                CMBTDRW1::Draw_Main_Combat_Screen();
                RUSS::Mox_Sync_Update();
                timer::Release_Time(1);
            } while (loop_counter < 12);

            MOX::dont_draw_ship = -1;
            sound::Fadeout_Sound(sound_handle, 10);
            allocate::Pop_Block(MOX::screen_seg);
        }

        MOX::combat_data[target_idx].black_hole_flag = 1;
        MOX::combat_data[target_idx].black_hole_source_idx = (uint16_t)source_idx;
        COMBAT1::Recalculate_Combat_Ship(target_idx);
    }
    void Record_Unit_Hit(int16_t slot, int32_t amount, int32_t secondary_amount, bool biological) {
        if (COMBAT::Local_Observer() != COMBAT::Current_Viewer() || !COMBAT::Unit_Disclosed(slot)) return;
        const auto& unit = MOX::combat_data[slot];
        if (unit.owner != COMBAT::Current_Viewer() && unit.special_status_flag == 4) return;
        if (amount <= 0 && secondary_amount <= 0) return;
        COMBAT::s_hit hit;
        hit.round = int32_t(MOX::combat_turn) + 1;
        hit.kind = biological ? COMBAT::BIOLOGICAL_HITS : COMBAT::UNIT_DAMAGE;
        hit.amount = uint16_t(amount);
        hit.secondary_amount = uint16_t(secondary_amount);
        strlcpy(hit.target_name, unit.name, sizeof(hit.target_name));
        COMBAT::Append_Hit(&hit);
    }
} // namespace CMBTFIRE