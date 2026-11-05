#include "pch.h"
#include "combat.h"

namespace {
    constexpr int kPlayerRemapPaletteStart = 144;
    constexpr int kTractorBeamRemapPaletteStart = 80;
    constexpr int kPlasmaWebRemapPaletteStart = 176;
    constexpr int16_t kDeploymentMinY = 4;
    constexpr uint64_t kCombatScrollTickMilliseconds = 55;
    constexpr size_t kCombatDataBytes = sizeof(s_combat_data) * MAX_COMBAT_SHIPS;
    constexpr size_t kCombatMissileBytes = sizeof(s_missile) * MAX_COMBAT_FLYERS;
    COMBINIT::s_satellite_capture_state satellite_capture_state;
    uint64_t previous_combat_scroll_time;

    struct s_deployment_cursor {
        int16_t x;
        int16_t x_step;
        int16_t y_down = MOX::combat_grid_height / 2;
        int16_t y_up = MOX::combat_grid_height / 2;
        bool deploy_down = true;
    };

    void Take_Deployment_Candidate(
        s_deployment_cursor* cursor,
        int16_t ship_size,
        int16_t* out_x,
        int16_t* out_y,
        bool* out_deployed_down) {
        // A column is full only after both vertical cursors are exhausted.
        if (cursor->y_down > MOX::combat_grid_height - 4 &&
            cursor->y_up < kDeploymentMinY) {
            cursor->x += cursor->x_step;
            cursor->y_down = MOX::combat_grid_height / 2;
            cursor->y_up = MOX::combat_grid_height / 2;
            cursor->deploy_down = !cursor->deploy_down;
        }

        bool deployed_down = cursor->deploy_down;
        if (deployed_down && cursor->y_down > MOX::combat_grid_height - 4) {
            deployed_down = false;
        } else if (!deployed_down && cursor->y_up < kDeploymentMinY) {
            deployed_down = true;
        }

        const int16_t spacing =
            COMBINIT::Ship_Size_Offset(ship_size) + 1;
        *out_x = cursor->x;
        if (deployed_down) {
            *out_y = cursor->y_down;
            cursor->y_down += spacing;
        } else {
            cursor->y_up -= spacing;
            *out_y = cursor->y_up;
        }
        *out_deployed_down = deployed_down;
    }

    void Refresh_Combat_Stats(bool time_warp_only) {
        for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
            if (i == 0 && MOX::cmbt_colony_id == -1) {
                continue;
            }
            if (time_warp_only &&
                COMBINIT::Does_Combat_Ship_Have_Special(
                    i, SPECIAL_TIME_WARP_FACILITATOR) == 0) {
                continue;
            }

            MOX::combat_data[i].current_speed = COMBAT1::Calc_Current_Speed(i);
            MOX::combat_data[i].tac_val_attack = CMBTFIR2::Offensive_Combat_Bonus(i);
            MOX::combat_data[i].tac_val_defense = CMBTFIR2::Defensive_Combat_Bonus(i, 0);
            MOX::combat_data[i].tac_val_evade = CMBTFIR2::Defensive_Combat_Bonus(i, 1);
        }
    }

    void Reactivate_Combat_Missiles() {
        for (int16_t i = 0; i < MAX_COMBAT_FLYERS; ++i) {
            if (MOX::missile[i].type > 0) {
                if (MOX::missile[i].travel_dist == 0) {
                    MOX::missile[i].travel_dist = MOX::missile[i].speed * 20;
                }
                MOX::missile[i].is_active = 1;
            }
        }
    }

    bool Install_Satellite_Special(
        s_combat_data* satellite,
        const s_player* defender,
        uint8_t special_id,
        int* special_count) {
        if (*special_count >= 8 ||
            special_id == SPECIAL_NO_SPECIAL ||
            special_id >= SPECIAL_COUNT) {
            return false;
        }

        const uint16_t tech_app_id = TECHDATA::specials[special_id].tech_app_id;
        if (tech_app_id == 0 ||
            tech_app_id >= TECH_APP_COUNT ||
            defender->tech_applications[tech_app_id] !=
                TECH_RESEARCH_STATUS_RESEARCHED) {
            return false;
        }

        struct_::Set_Bit_Field(satellite->special_device_flags, special_id);
        ++*special_count;
        return true;
    }

    uint16_t Defense_Weapon_Mods(
        int16_t defender,
        int16_t weapon_id,
        uint16_t required_mod,
        uint16_t allowed_mods) {
        const int16_t miniaturization = DESIGN::Miniaturization_Level(
            defender, TECHDATA::weapons[weapon_id].tech_app_id);
        uint16_t mods = required_mod;

        for (int mod = WEAPON_MOD_ARMOR_PIERCING;
             mod <= WEAPON_MOD_OVERLOADED;
             ++mod) {
            if (mod == WEAPON_MOD_EMISSIONS_GUIDANCE &&
                MOX::player[defender]
                        .tech_applications[TECH_APP_EMISSIONS_GUIDANCE_SYSTEM] !=
                    TECH_RESEARCH_STATUS_RESEARCHED) {
                continue;
            }

            const uint16_t flag = (uint16_t)(1U << mod);
            if ((TECHDATA::weapons[weapon_id].available_mods & flag) != 0 &&
                TECHDATA::weapon_modifications[mod].level <= miniaturization &&
                (allowed_mods & flag) != 0) {
                mods |= flag;
            }
        }

        return mods;
    }

    int16_t Best_Defense_Point_Defense_Weapon(int16_t defender) {
        int16_t best_weapon_id = WEAPON_LASER_CANNON;
        int32_t best_space = 0;
        int32_t best_damage = 0;
        bool found_weapon = false;

        for (int16_t weapon_id = 1; weapon_id < WEAPON_COUNT; ++weapon_id) {
            const s_tech_weapon_data& weapon = TECHDATA::weapons[weapon_id];
            if (weapon.type != WEAPON_TYPE_BEAM ||
                (weapon.available_mods & WEAPON_MOD_FLAG_POINT_DEFENSE) == 0 ||
                MOX::player[defender]
                        .tech_applications[weapon.tech_app_id] !=
                    TECH_RESEARCH_STATUS_RESEARCHED) {
                continue;
            }

            uint16_t damage_mods = Defense_Weapon_Mods(
                defender,
                weapon_id,
                WEAPON_MOD_FLAG_POINT_DEFENSE,
                COMBINIT::defenses_allowed_mods.pd);
            damage_mods &=
                WEAPON_MOD_FLAG_AUTO_FIRE | WEAPON_MOD_FLAG_ENVELOPING;

            const int32_t space = DESIGN::Weapon_Space(
                defender,
                weapon_id,
                1,
                WEAPON_FIRING_ARC_360,
                0,
                damage_mods);
            int32_t damage = weapon.max_damage;
            if ((damage_mods & WEAPON_MOD_FLAG_ENVELOPING) != 0 ||
                (weapon.natural_mods & WEAPON_NATURAL_FLAG_ENVELOPING) != 0) {
                damage *= 4;
            }
            if ((damage_mods & WEAPON_MOD_FLAG_AUTO_FIRE) != 0) {
                damage *= 3;
            }

            const int64_t candidate_product = (int64_t)best_space * damage;
            const int64_t best_product = (int64_t)space * best_damage;
            if (!found_weapon ||
                candidate_product > best_product ||
                (candidate_product == best_product &&
                    damage < best_damage)) {
                best_weapon_id = weapon_id;
                best_space = space;
                best_damage = damage;
                found_weapon = true;
            }
        }

        return best_weapon_id;
    }
} // namespace

namespace COMBINIT {

    const char s_cmbtmisl_lbx_00551234[13] = "cmbtmisl.lbx";
    const char s_sphersfx_lbx_00551218[13] = "sphersfx.lbx";

    uint8_t real_attack_flag;

    void Reactivate_Combat_Missiles_For_Pass(bool time_warp_only) {
        if (!time_warp_only) {
            Reactivate_Combat_Missiles();
        }
    }

    void __cdecl Reset_Satellite_Capture() {
        Reset_Satellite_Capture_State(&satellite_capture_state);
    }

    void __cdecl Record_Captured_Satellite(int16_t colony_idx, int16_t building_idx) {
        Record_Captured_Satellite_State(&satellite_capture_state, colony_idx, building_idx);
    }

    int16_t __cdecl Take_Captured_Satellite(int16_t colony_idx) {
        return Take_Captured_Satellite_State(&satellite_capture_state, colony_idx);
    }

    int16_t __cdecl Damaged_Drive_Combat_Speed(
        int16_t combat_speed, uint16_t drive_current_hits, uint16_t drive_max_hits) {
        if (drive_max_hits == 0) {
            return 0;
        }

        int32_t half_drive_hits = drive_max_hits / 2;
        int32_t effective_drive_hits =
            (int32_t)drive_current_hits - half_drive_hits;
        if (effective_drive_hits <= 0) {
            return 0;
        }

        return (int16_t)((int32_t)combat_speed * effective_drive_hits /
            (drive_max_hits - half_drive_hits));
    }

    void __cdecl Load_Display_Combat_Ship(int16_t ship_idx) {
        struct_::Clear_Structure(&MOX::global_combat_data, sizeof(MOX::global_combat_data));

        const bool strategic_combat =
            MOX::settings.strategic_combat_flag == 1;
        if (strategic_combat) {
            INITSHIP::Update_Strategic_Ship(ship_idx);
        }

        strlcpy(MOX::global_combat_data.name, MOX::ship[ship_idx].d.name, sizeof(MOX::global_combat_data.name));

        MOX::global_combat_data.owner = MOX::ship[ship_idx].owner;
        MOX::global_combat_data.previous_owner = MOX::ship[ship_idx].d.previous_owner;
        if (MOX::global_combat_data.owner >= MAX_PLAYERS) {
            MOX::global_combat_data.owner =
                static_cast<int8_t>(MOX::global_combat_data.previous_owner);
        }
        MOX::global_combat_data.ship_idx = ship_idx;
        MOX::global_combat_data.shield_type = MOX::ship[ship_idx].d.shield_type;
        MOX::global_combat_data.computer_type = MOX::ship[ship_idx].d.computer_type;
        MOX::global_combat_data.ftl_type = MOX::ship[ship_idx].d.ftl_type;
        MOX::global_combat_data.size_class = MOX::ship[ship_idx].d.size;
        MOX::global_combat_data.unit_status = 0;

        for (int i = 0; i < 5; ++i) {
            MOX::global_combat_data.special_device_flags[i] = MOX::ship[ship_idx].d.special_device_flags[i];
            MOX::global_combat_data.special_device_damage_flags[i] = MOX::ship[ship_idx].special_device_damage_flags[i];
        }

        for (int i = 0; i < 8; ++i) {
            if (MOX::ship[ship_idx].d.ship_type == SHIP_TYPE_COMBAT) {
                MOX::global_combat_data.weapons[i].weapon_id = MOX::ship[ship_idx].d.ship_weapon[i].type;
                MOX::global_combat_data.weapons[i].count = MOX::ship[ship_idx].d.ship_weapon[i].current_count;
                MOX::global_combat_data.weapons[i].arc = MOX::ship[ship_idx].d.ship_weapon[i].firing_arc;
                MOX::global_combat_data.weapons[i].specials = MOX::ship[ship_idx].d.ship_weapon[i].specials;
                MOX::global_combat_data.weapons[i].ammo =
                    (uint8_t)MOX::ship[ship_idx].d.ship_weapon[i].ammo;

                if (MOX::global_combat_data.weapons[i].arc == WEAPON_FIRING_ARC_360) {
                    MOX::global_combat_data.weapons[i].arc = WEAPON_FIRING_ARC_ALL_SECTORS;
                }

                MOX::global_combat_data.weapons[i].active = 1;
                MOX::global_combat_data.weapons[i].shots_left = MOX::global_combat_data.weapons[i].count;

                int16_t weapon_id = MOX::global_combat_data.weapons[i].weapon_id;

                if (TECHDATA::weapons[weapon_id].type == WEAPON_TYPE_TORPEDO) {
                    MOX::global_combat_data.weapons[i].ammo = 0;
                } else if (TECHDATA::weapons[weapon_id].type != WEAPON_TYPE_MISSILE) {
                    MOX::global_combat_data.weapons[i].ammo = TECHDATA::weapons[weapon_id].ammo;
                }

                if (weapon_id == WEAPON_ANTI_MISSILE_ROCKET) {
                    MOX::global_combat_data.weapons[i].ammo *= MOX::global_combat_data.weapons[i].count;
                }

                MOX::global_combat_data.weapons[i].status = 0;
            } else {
                MOX::global_combat_data.weapons[i].weapon_id = WEAPON_NO_WEAPONS;
            }
        }

        for (int i = 0; i < 8; ++i) {
            MOX::global_combat_data.weapon_ready_flags[i] = 1;
        }

        MOX::global_combat_data.crew_quality = MOX::ship[ship_idx].crew_quality;
        MOX::global_combat_data.officer_idx = MOX::ship[ship_idx].officer_index;

        if (!strategic_combat &&
            MOX::ship[ship_idx].d.ship_type == SHIP_TYPE_COMBAT &&
            MOX::ship[ship_idx].owner <= MAX_PLAYERS) {
            MOX::global_combat_data.marine_count = (int8_t)TECHDATA::hull_data[MOX::global_combat_data.size_class].marines;

            if (struct_::Test_Bit_Field(MOX::global_combat_data.special_device_flags, SPECIAL_TROOP_PODS)) {
                if (!struct_::Test_Bit_Field(MOX::global_combat_data.special_device_damage_flags, SPECIAL_TROOP_PODS)) {
                    MOX::global_combat_data.marine_count *= 2;
                }
            }
        } else {
            MOX::global_combat_data.marine_count = -1;
        }

        if (strategic_combat) {
            MOX::global_combat_data.structure_max =
                AIPOWER::Max_Ship_Hits(ship_idx);
            MOX::global_combat_data.armor_remaining = 0;
        } else {
            MOX::global_combat_data.structure_max =
                INITSHIP::Get_Ship_Structure(ship_idx);
            MOX::global_combat_data.armor_remaining =
                INITSHIP::Get_Ship_Armor_Hits(ship_idx) -
                MOX::ship[ship_idx].armor_damage;
        }
        MOX::global_combat_data.armor_type = MOX::ship[ship_idx].d.armor_type;

        if (MOX::global_combat_data.size_class == SHIP_SIZE_DOOM_STAR) {
            MOX::global_combat_data.structure_damage = MOX::ship[ship_idx].structural_damage;
            MOX::global_combat_data.picture_num = 43; /* 0x2B */
        } else {
            MOX::global_combat_data.structure_damage = MOX::ship[ship_idx].structural_damage;
            MOX::global_combat_data.picture_num = MOX::ship[ship_idx].d.picture_num;
        }

        MOX::global_combat_data.is_retreating = 0;

        CMBTFIR2::Init_Internal_Space((s_combat_data*)&MOX::global_combat_data);

        MOX::global_combat_data.shield_current_hits = (uint16_t)((int32_t)(100 - MOX::ship[ship_idx].shield_damage_percent) *
            MOX::global_combat_data.shield_max_hits / 100);

        MOX::global_combat_data.drive_current_hits = (uint16_t)((int32_t)(100 - MOX::ship[ship_idx].drive_damage_percent) *
            MOX::global_combat_data.drive_max_hits / 100);

        MOX::global_combat_data.computer_current_hits = (uint16_t)(MOX::global_combat_data.computer_max_hits -
            MOX::ship[ship_idx].computer_damage);

        if (MOX::global_combat_data.shield_type <= 0) {
            MOX::global_combat_data.shield_arc_max = 0;
        } else {
            if (strategic_combat) {
                MOX::global_combat_data.shield_arc_max =
                    TECHDATA::shields[MOX::global_combat_data.shield_type].shield_class;
                if (SHIPSTAK::Ship_Has_Special(
                        ship_idx, SPECIAL_HARD_SHIELDS) != 0) {
                    MOX::global_combat_data.shield_arc_max +=
                        TECHDATA::hard_shields_damage_reduction;
                }
            } else {
                MOX::global_combat_data.shield_arc_max =
                    (int16_t)TECHDATA::Shield_Strength(
                        MOX::global_combat_data.size_class,
                        MOX::global_combat_data.shield_type);
            }

            if (struct_::Test_Bit_Field(MOX::global_combat_data.special_device_flags, SPECIAL_MULTI_PHASED_SHIELDS)) {
                if (!struct_::Test_Bit_Field(MOX::global_combat_data.special_device_damage_flags, SPECIAL_MULTI_PHASED_SHIELDS)) {
                    MOX::global_combat_data.shield_arc_max = (int16_t)
                        TECHDATA::Apply_Multi_Phased_Shields_Bonus(
                            MOX::global_combat_data.shield_arc_max);
                }
            }

            MOX::global_combat_data.shield_arc_max =
                (MOX::global_combat_data.shield_arc_max * MOX::global_combat_data.shield_current_hits + MOX::global_combat_data.shield_max_hits - 1) / MOX::global_combat_data.shield_max_hits;
        }

        for (int i = 0; i < 4; ++i) {
            MOX::global_combat_data.shield_arc_current[i] = MOX::global_combat_data.shield_arc_max;
        }

        const bool real_tactical_ship =
            !strategic_combat &&
            MOX::ship[ship_idx].d.ship_type == SHIP_TYPE_COMBAT;
        if (!real_tactical_ship) {
            MOX::global_combat_data.current_speed = 0;
        } else {
            MOX::global_combat_data.current_speed =
                MOX::ship[ship_idx].d.combat_speed;

            if (struct_::Test_Bit_Field(MOX::ship[ship_idx].d.special_device_flags, SPECIAL_AUGMENTED_ENGINES)) {
                if (!struct_::Test_Bit_Field(MOX::ship[ship_idx].special_device_damage_flags, SPECIAL_AUGMENTED_ENGINES)) {
                    MOX::global_combat_data.current_speed += DESIGN::augmented_engines_speed_bonus;
                }
            }

            MOX::global_combat_data.current_speed = Damaged_Drive_Combat_Speed(
                MOX::global_combat_data.current_speed,
                MOX::global_combat_data.drive_current_hits,
                MOX::global_combat_data.drive_max_hits);
        }

        if (real_tactical_ship && MOX::global_combat_data.owner < MAX_PLAYERS) {
            if (MOX::player[MOX::global_combat_data.owner].traits[TRAIT_TRANS_DIMENSIONAL] != 0) {
                MOX::global_combat_data.current_speed += COMBAT1::td_combat_speed_bonus;
            }
        }
    }

    int16_t __cdecl Ship_Size_Offset(int16_t size) {
        switch (size) {
            case HULL_FRIGATE:
                return 1;
            case HULL_DESTROYER:
            case HULL_CRUISER:
                return 2;
            case HULL_BATTLESHIP:
            case HULL_TITAN:
            case HULL_DOOM_STAR:
                return 3;
            default:
                return 0;
        }
    }

    int8_t __cdecl Colony_Has_Defenses(int16_t colony_idx) {
        s_colony* col = &MOX::colony[colony_idx];

        if (col->buildings[BUILDING_STAR_BASE] != 0 ||
            col->buildings[BUILDING_FIGHTER_GARRISON] != 0 ||
            col->buildings[BUILDING_BATTLESTATION] != 0 ||
            col->buildings[BUILDING_STAR_FORTRESS] != 0 ||
            col->buildings[BUILDING_MISSILE_BASE] != 0 ||
            col->buildings[BUILDING_GROUND_BATTERIES] != 0 ||
            col->buildings[BUILDING_STELLAR_CONVERTER] != 0) {
            return 1;
        }

        return 0;
    }

    int __cdecl qsort_ship_array_by_size(const void* p1, const void* p2) {
        const int16_t* ship_idx_a = (const int16_t*)p1;
        const int16_t* ship_idx_b = (const int16_t*)p2;

        int8_t size_b = MOX::ship[*ship_idx_b].d.size;
        int8_t size_a = MOX::ship[*ship_idx_a].d.size;

        if (size_b == size_a) {
            return 0;
        }

        if (size_b > size_a) {
            return 1;
        }

        return -1;
    }

    int __cdecl qsort_combat_data_by_size(const void* a, const void* b) {
        const s_combat_data* data_a = (const s_combat_data*)a;
        const s_combat_data* data_b = (const s_combat_data*)b;

        if (data_b->size_class == data_a->size_class) {
            return 0;
        }

        if (data_b->size_class > data_a->size_class) {
            return 1;
        }

        return -1;
    }

    void __cdecl Load_Combat_Ship(int16_t ship_idx, int16_t combat_idx) {
        COMBAT::Retire_Unit(combat_idx);
        s_ship_data* ship = &MOX::ship[ship_idx];
        s_combat_data* combat = &MOX::combat_data[combat_idx];

        strlcpy(combat->name, ship->d.name, sizeof(combat->name));
        combat->owner = ship->owner;
        combat->previous_owner = ship->d.previous_owner;
        combat->ship_idx = ship_idx;
        combat->shield_type = ship->d.shield_type;
        combat->computer_type = ship->d.computer_type;

        if (combat->owner < MAX_PLAYERS) {
            combat->ftl_type = ship->d.ftl_type;
        } else {
            combat->ftl_type = (int8_t)INITSHIP::Best_Warp_Drive(combat->owner);
        }

        combat->combat_speed_base = ship->d.combat_speed;
        combat->size_class = ship->d.size;
        combat->unit_status = 0;

        for (int i = 0; i < 5; ++i) {
            combat->special_device_flags[i] = ship->d.special_device_flags[i];
            combat->special_device_damage_flags[i] = ship->special_device_damage_flags[i];
        }

        for (int i = 0; i < 8; ++i) {
            combat->weapons[i].weapon_id = ship->d.ship_weapon[i].type;
            combat->weapons[i].count = ship->d.ship_weapon[i].current_count;
            combat->weapons[i].arc = ship->d.ship_weapon[i].firing_arc;
            combat->weapons[i].specials = ship->d.ship_weapon[i].specials;
            combat->weapons[i].ammo = (uint8_t)ship->d.ship_weapon[i].ammo;

            if (combat->weapons[i].arc == WEAPON_FIRING_ARC_360) {
                combat->weapons[i].arc = WEAPON_FIRING_ARC_ALL_SECTORS;
            }

            combat->weapons[i].active = 1;
            combat->weapons[i].shots_left = combat->weapons[i].count;

            uint16_t weapon_id = combat->weapons[i].weapon_id;
            if (TECHDATA::weapons[weapon_id].type == WEAPON_TYPE_TORPEDO) {
                combat->weapons[i].ammo = 0;
            } else if (TECHDATA::weapons[weapon_id].type != WEAPON_TYPE_MISSILE) {
                combat->weapons[i].ammo = TECHDATA::weapons[weapon_id].ammo;
            }

            if (combat->weapons[i].weapon_id == WEAPON_ANTI_MISSILE_ROCKET) {
                combat->weapons[i].ammo *= combat->weapons[i].count;
            }

            combat->weapons[i].status = 0;
        }

        for (int i = 0; i < 8; ++i) {
            combat->weapon_ready_flags[i] = 1;
        }

        combat->crew_quality = ship->crew_quality;

        int16_t officer_idx = ship->officer_index;
        if (officer_idx < 0 || MOX::leaders[officer_idx].eta != 0) {
            combat->officer_idx = -1;
        } else {
            combat->officer_idx = officer_idx;
        }

        if (ship->d.ship_type == SHIP_TYPE_COMBAT && ship->owner <= MAX_PLAYERS) {
            combat->marine_count = (uint8_t)TECHDATA::hull_data[combat->size_class].marines;
            if (struct_::Test_Bit_Field(combat->special_device_flags, SPECIAL_TROOP_PODS) == 1 &&
                struct_::Test_Bit_Field(combat->special_device_damage_flags, SPECIAL_TROOP_PODS) == 0) {
                combat->marine_count *= 2;
            }
        } else {
            combat->marine_count = -1;
        }

        combat->structure_max = INITSHIP::Get_Ship_Structure(ship_idx);
        combat->armor_remaining = INITSHIP::Get_Ship_Armor_Hits(ship_idx) - ship->armor_damage;
        combat->armor_type = ship->d.armor_type;
        combat->structure_damage = ship->structural_damage;

        if (combat->structure_damage > combat->structure_max) {
            combat->structure_damage = combat->structure_max - 1;
        }

        if (combat->size_class == SHIP_SIZE_DOOM_STAR &&
            combat->previous_owner < MAX_PLAYERS) {
            combat->picture_num = 43 /* 0x2b */;
        } else {
            combat->picture_num = ship->d.picture_num;
        }

        combat->is_retreating = 0;
        CMBTFIR2::Init_Internal_Space(combat);

        combat->shield_current_hits = (uint16_t)((int32_t)(100 - ship->shield_damage_percent) *
            combat->shield_max_hits / 100);
        combat->drive_current_hits = (uint16_t)((int32_t)(100 - ship->drive_damage_percent) *
            combat->drive_max_hits / 100);
        combat->computer_current_hits = (uint16_t)(combat->computer_max_hits - ship->computer_damage);

        combat->shield_arc_max = Calc_Combat_Shield_Max(combat_idx);
        for (int i = 0; i < 4; ++i) {
            combat->shield_arc_current[i] = combat->shield_arc_max;
        }

        if (combat->owner > MAX_PLAYERS && combat->ftl_type == DRIVE_NO_DRIVE) {
            switch (combat->owner) {
                case 9:
                case 11:
                    combat->ftl_type = DRIVE_ANTI_MATTER_DRIVE;
                    break;
                case 10:
                case 14:
                    combat->ftl_type = DRIVE_FUSION_DRIVE;
                    break;
                case 12:
                case 13:
                    combat->ftl_type = DRIVE_INTERPHASED_DRIVE;
                    break;
                default:
                    break;
            }
        }

        Init_Special_Devices(combat_idx);
    }

    void __cdecl Init_Special_Devices(int16_t combat_ship_idx) {
        s_combat_data* ship = &MOX::combat_data[combat_ship_idx];

        ship->stasis_source_idx = COMBAT_SHIP_NONE;
        ship->black_hole_source_idx = COMBAT_SHIP_NONE;
        ship->reflected_damage_pool = 0;
        ship->black_hole_flag = 0;
        ship->combat_status_flags = 0;
        ship->field42_0x49 = 0;
        ship->plasma_web_damage = 0;
        ship->special_cooldown = COMBAT_SHIP_NONE;
        ship->special_status_flag = 0;
        ship->special_status_timer = 0;

        if (COMBINIT::Does_Combat_Ship_Have_Special(combat_ship_idx, SPECIAL_CLOAKING_DEVICE) ||
            (CMBTFIR2::combat_bonus.stealth_field_beam_defense != 0 &&
                COMBINIT::Does_Combat_Ship_Have_Special(combat_ship_idx, SPECIAL_STEALTH_FIELD))) {
            ship->special_status_flag = 1;
        }
        if (COMBINIT::Does_Combat_Ship_Have_Special(combat_ship_idx, SPECIAL_PHASING_CLOAK)) {
            ship->special_status_timer = CMBTFIR2::combat_bonus.phasing_cloak_effective_turns;
            ship->special_status_flag = ship->special_status_timer != 0 ? 4 : 1;
        }

        if (COMBINIT::Does_Combat_Ship_Have_Special(combat_ship_idx, SPECIAL_HYPER_X_CAPACITORS)) {
            for (int i = 0; i < 8; ++i) {
                if (TECHDATA::weapons[ship->weapons[i].weapon_id].type == WEAPON_TYPE_BEAM) {
                    ship->weapons[i].status = 1;
                }
            }
        }

        if (COMBINIT::Does_Combat_Ship_Have_Special(combat_ship_idx, SPECIAL_FAST_MISSILE_RACKS)) {
            for (int i = 0; i < 8; ++i) {
                if (TECHDATA::weapons[ship->weapons[i].weapon_id].type == WEAPON_TYPE_MISSILE) {
                    ship->weapons[i].status = 1;
                }
            }
        }
    }

    int16_t __cdecl Type_Of_Space_Station(int16_t colony_idx) {
        if (MOX::colony[colony_idx].buildings[BUILDING_STAR_BASE] != 0) {
            return HULL_STAR_BASE;
        }
        if (MOX::colony[colony_idx].buildings[BUILDING_BATTLESTATION] != 0) {
            return HULL_BATTLESTATION;
        }
        if (MOX::colony[colony_idx].buildings[BUILDING_STAR_FORTRESS] != 0) {
            return HULL_STAR_FORTRESS;
        }
        return -1;
    }

    int16_t __cdecl Calc_Combat_Shield_Max(int16_t ship_idx) {
        int16_t max_shield = 0;
        s_combat_data* ship = &MOX::combat_data[ship_idx];
        int8_t shield_type = ship->shield_type;

        if (shield_type > 0) {
            int8_t size_class = ship->size_class;

            if (Is_Combat_Satellite(ship)) {
                size_class = (int8_t)Combat_Satellite_Hull_Class(ship);
            }

            max_shield = (int16_t)TECHDATA::Shield_Strength(size_class, shield_type);

            // Shields are suppressed in a nebula unless the ship has specific warp/shield stabilization (Special 13)
            if (MOX::in_nebula_flag != 0) {
                if (COMBINIT::Does_Combat_Ship_Have_Special(ship_idx, SPECIAL_HARD_SHIELDS) == 0) {
                    max_shield = 0;
                }
            }

            // Multi-Phased Shields apply the configured capacity bonus.
            if (COMBINIT::Does_Combat_Ship_Have_Special(ship_idx, SPECIAL_MULTI_PHASED_SHIELDS) != 0) {
                max_shield = (int16_t)
                    TECHDATA::Apply_Multi_Phased_Shields_Bonus(max_shield);
            }
        }

        return max_shield;
    }

    int16_t __cdecl Does_Combat_Ship_Have_Special(int16_t ship_idx, int16_t special_id) {
        int16_t result = 0;

        if (struct_::Test_Bit_Field(MOX::combat_data[ship_idx].special_device_flags, special_id) == 1) {
            if (struct_::Test_Bit_Field(MOX::combat_data[ship_idx].special_device_damage_flags, special_id) == 0) {
                result = 1;
            }
        }

        return result;
    }

    int16_t Run_Tactical_Combat_Rounds(int16_t first_player, int16_t second_player, int16_t winner) {
        while (winner == 0) {
            Refresh_Combat_Stats(false);
            RUSS::Mox_Update();
            Reactivate_Combat_Missiles_For_Pass(false);
            RUSS::Mox_Update();
            CMBTAI::fleet_target = -1;
            if (MOX::combat_init == 0) {
                COMBAT1::Do_Combat_Turn(first_player, 0);
                COMBAT1::Check_For_Winner();
                Plasma_Web_Damage(second_player);
                if (MOX::super_fast_flag != 0) CMBTDRW1::Draw_Super_Fast_Screen();
                winner = COMBAT1::Check_For_Winner();
                if (winner == 0) {
                    CMBTAI::fleet_target = -1;
                    COMBAT1::Do_Combat_Turn(second_player, 0);
                    COMBAT1::Check_For_Winner();
                }
                Plasma_Web_Damage(first_player);
                if (MOX::super_fast_flag != 0) CMBTDRW1::Draw_Super_Fast_Screen();
                winner = COMBAT1::Check_For_Winner();
                if (winner == 0) {
                    CMBTAI::fleet_target = -1;
                    COMBAT1::Do_Combat_Turn(first_player, 1);
                    COMBAT1::Check_For_Winner();
                    Increment_Bhg(first_player, true);
                }
                if (MOX::super_fast_flag != 0) CMBTDRW1::Draw_Super_Fast_Screen();
                winner = COMBAT1::Check_For_Winner();
                if (winner == 0) {
                    CMBTAI::fleet_target = -1;
                    COMBAT1::Do_Combat_Turn(second_player, 1);
                    COMBAT1::Check_For_Winner();
                    Increment_Bhg(second_player, true);
                }
                if (MOX::super_fast_flag != 0) CMBTDRW1::Draw_Super_Fast_Screen();
                winner = COMBAT1::Check_For_Winner();
            } else {
                COMBAT1::Do_Combat_Turn(0, 0);
                Increment_Bhg(0);
                Plasma_Web_Damage(-1);
                winner = COMBAT1::Check_For_Winner();
                if (winner == 0) {
                    COMBAT1::Update_Phasing_Cloak_Timers(true);
                    Recharge_Shields(true);
                    Repair_All_Combat_Ships(true);
                    Refresh_Combat_Stats(true);
                    Reactivate_Combat_Missiles_For_Pass(true);
                    RUSS::Mox_Update();
                    COMBAT1::Do_Combat_Turn(0, 1);
                    Increment_Bhg(0, true);
                    Plasma_Web_Damage(-1, true);
                    winner = COMBAT1::Check_For_Winner();
                }
                if (MOX::super_fast_flag != 0) CMBTDRW1::Draw_Super_Fast_Screen();
            }
            MOX::combat_turn++;
            End_Of_Turn_Bookeeping();
            COMBAT::Invalidate_View();
            COMBAT::Service_Reads(true);
        }
        return winner;
    }

    int16_t Local_Tactical_Observer() {
        return COMBAT::Local_Observer();
    }

    int16_t __cdecl Tactical_Combat(int16_t* plr1_ships, int16_t plr1_ship_count, int16_t* plr2_ships, int16_t plr2_ship_count, int16_t star_idx, int16_t colony_idx, int16_t retreat_flag) {
        COMBAT::s_battle_state battle_events;
        int8_t abort_tactical = 0;
        bool no_defender_ships = star_idx != MOX::NUM_STARS;
        int16_t has_tractor_beam = 0;
        int16_t has_plasma_web = 0;
        int16_t has_special_26 = 0;
        int16_t has_bhg = 0;
        int16_t has_stellar_converter = 0;
        int16_t colony_pop = 0;
        s_mouse_list local_mouse_list = {0, 0, 0, 0, 0x27F, 0x1DF};
        MOX2::action_func_t auto_func;
        void* glass_remap_block;
        char msg_buf[80];

        int16_t combat_winner = 0;

        if (star_idx == MOX::NUM_STARS) {
            ERIC::Play_Dimensional_Portal_Flic();
        }

        MOX::ai_retreat_flag = (int16_t)retreat_flag;
        MOX::combat_turn = 0;
        MOX::combat_clock_frame = 15;
        MOX::bio_weapon_flag = 0;
        MOX::last_ship_status = -2;
        MOX::cur_combat_x = 0;
        MOX::cur_combat_y = 0;
        MOX::ship_status_display = 0;
        MOX::dont_draw_ship = -1;
        MOX::draw_active_missiles = 1;
        MOX::order_info_flag = 1;
        MOX::super_fast_flag = 0;
        COMBINIT::real_attack_flag = 0;

        MOX::shields_flag = (uint8_t)MOX::settings.combat_shields_arc_flag;
        MOX::speedx2_flag = (uint8_t)MOX::settings.combat_speedx2_flag;
        MOX::saved_msl_warning = (uint8_t)MOX::settings.combat_missile_impact_flag;
        MOX::msl_impact_warning_flag = (uint8_t)MOX::settings.combat_missile_impact_flag;
        MOX::draw_legal_boxes_flag = (uint8_t)MOX::settings.combat_legal_moves_flag;
        MOX::grid_flag = (uint8_t)MOX::settings.combat_grid_flag;
        MOX::combat_init = COMBAT1::ship_initiative < 0
            ? MOX::settings.ship_initiative
            : COMBAT1::ship_initiative != 0;

        MOX::take_control_flag = 0;
        MOX::apply_damage_to_planet = 1;
        MOX::missile_warning = 0;
        MOX::ship_frame = 0;
        MOX::combat_far_x = 0;
        MOX::combat_mid_x = 0;
        MOX::combat_near_x = 0;
        MOX::combat_far_y = 0;
        MOX::combat_mid_y = 0;
        MOX::combat_near_y = 0;
        MOX::net_flag = 0;
        MOX::combat_total_ship_count = 1;
        MOX::cmbt_colony_id = colony_idx;

        MOX::combat_data = (s_combat_data*)allocate::Allocate_First_Block(MOX::screen_seg, kCombatDataBytes);
        // Resetting the screen pool invalidates resources from the previous combat.
        MOX::missile = nullptr;
        MOX::darken_plasma_map = nullptr;
        struct_::Clear_Structure(MOX::combat_data, kCombatDataBytes);

        MOX::combat_attacker_player = MOX::ship[plr1_ships[0]].owner;

        if (star_idx == MOX::NUM_STARS) {
            MOX::combat_defender_player = MAX_PLAYERS;
            colony_pop = 0;
        } else if (MOX::cmbt_colony_id == -1) {
            MOX::combat_defender_player = MOX::ship[plr2_ships[0]].owner;
            colony_pop = 0;
        } else {
            MOX::combat_defender_player = MOX::colony[MOX::cmbt_colony_id].owner;
            colony_pop = MOX::colony[MOX::cmbt_colony_id].n_pops;
        }

        if (star_idx == MOX::NUM_STARS) {
            MOX::in_nebula_flag = 0;
        } else {
            MOX::in_nebula_flag = GEO::Point_Is_In_Some_Nebula(MOX::star[star_idx].x, MOX::star[star_idx].y) ? 1 : 0;
        }

        qsort(plr1_ships, plr1_ship_count, sizeof(*plr1_ships), qsort_ship_array_by_size);

        int16_t loaded_ships_1 = 0;
        for (int16_t i = 0; i < plr1_ship_count; ++i) {
            if (MOX::ship[plr1_ships[i]].d.ship_type == SHIP_TYPE_COMBAT && loaded_ships_1 < max_ships_per_side) {
                Load_Combat_Ship(plr1_ships[i], MOX::combat_total_ship_count);

                for (int j = 0; j < 8; ++j) {
                    switch (MOX::combat_data[MOX::combat_total_ship_count].weapons[j].weapon_id) {
                        case WEAPON_PLASMA_WEB:
                        case WEAPON_CAUSTIC_SLIME:
                            has_plasma_web = 1;
                            break;
                        case WEAPON_BLACK_HOLE_GENERATOR:
                            has_bhg = 1;
                            break;
                        case WEAPON_STELLAR_CONVERTER:
                            has_stellar_converter = 1;
                            break;
                        case WEAPON_TRACTOR_BEAM:
                            has_tractor_beam = 1;
                            break;
                    }
                }

                if (Does_Combat_Ship_Have_Special(MOX::combat_total_ship_count, SPECIAL_PHASING_CLOAK) == 0) {
                    COMBINIT::real_attack_flag = 1;
                }

                // Original binary calls this and discards the result.
                Does_Combat_Ship_Have_Special(MOX::combat_total_ship_count, 10);
                if (Does_Combat_Ship_Have_Special(MOX::combat_total_ship_count, SPECIAL_REFLECTION_FIELD) != 0) {
                    has_special_26 = 1;
                }

                MOX::combat_total_ship_count++;
                loaded_ships_1++;
            }
            RUSS::Mox_Update();
        }

        if (MOX::combat_defender_player < MAX_PLAYERS && star_idx < MOX::NUM_STARS) {
            if (HAROLD::Player_Has_Artemis_Net(&MOX::player[MOX::combat_defender_player], &MOX::star[star_idx])) {
                abort_tactical = CMBTFIR2::Apply_Combat_Artemis_Net(0);
                if (abort_tactical == 0) {
                    KEN::Ken_Get_Text_Message(0x53, msg_buf, 80);
                } else {
                    KEN::Ken_Get_Text_Message(0x52, msg_buf, 80);
                }
                palette::Refresh_Palette();
                GENDRAW::Message_Box(msg_buf);
                combat_winner = (int16_t)0xFFFF;
            }
        }

        if (no_defender_ships) {
            for (int16_t i = 0; i < plr2_ship_count; ++i) {
                if (MOX::ship[plr2_ships[i]].d.ship_type == SHIP_TYPE_COMBAT) {
                    no_defender_ships = false;
                    break;
                }
            }
        }

        if (no_defender_ships) {
            if (abort_tactical == 0) {
                if (star_idx != MOX::NUM_STARS) {
                    if (MOX::cmbt_colony_id == -1 || Colony_Has_Defenses(colony_idx) == 0) {
                        combat_winner = 1;
                        abort_tactical = 1;
                    }
                }
            }
        }

        if (abort_tactical == 0) {
            MOX2::Get_Auto_Function(&auto_func);
            graphics::Reset_Window();
            JIM::Play_Combat_Music();
            RUSS::Mox_Update();
            fields::Clear_Fields();
            fields::Deactivate_Auto_Function();
            mouse::Set_Mouse_List(&local_mouse_list, 1);
            MAINSCR::Set_Reload_Fleet_Box_Palette_Flag();
            ERIC::Fast_Fade_Out();
            fonts::Load_Palette(3, 0, 0xFF);
            ERIC::Set_Fade_Percent(0);

            MOX::missile = (s_missile*)allocate::Allocate_Next_Block(MOX::screen_seg, kCombatMissileBytes);
            struct_::Clear_Structure(MOX::missile, kCombatMissileBytes);
            CMBTMIS::Reset_Fighter_Pursuit_History();
            RUSS::Mox_Update();

            MOX::level_symbol_seg = farload::Far_Reload_Next("combat.lbx", 0x2A, MOX::screen_seg);
            RUSS::Mox_Update();
            MOX::scan_button_seg = farload::Far_Reload_Next("combat.lbx", 1, MOX::screen_seg);
            RUSS::Mox_Update();
            MOX::board_button_seg = farload::Far_Reload_Next("combat.lbx", 0x33, MOX::screen_seg);
            RUSS::Mox_Update();
            MOX::auto_button_seg = farload::Far_Reload_Next("combat.lbx", 0x32, MOX::screen_seg);
            RUSS::Mox_Update();
            MOX::flee_button_seg = farload::Far_Reload_Next("combat.lbx", 0x35, MOX::screen_seg);
            RUSS::Mox_Update();
            MOX::cmbt_done_button_seg = farload::Far_Reload_Next("combat.lbx", 0x36, MOX::screen_seg);
            RUSS::Mox_Update();
            MOX::wait_button_seg = farload::Far_Reload_Next("combat.lbx", 0x34, MOX::screen_seg);
            RUSS::Mox_Update();
            MOX::cmbt_info_button_seg = farload::Far_Reload_Next("combat.lbx", 10, MOX::screen_seg);
            RUSS::Mox_Update();
            MOX::cmbt_option_seg = farload::Far_Reload_Next("combat.lbx", 2, MOX::screen_seg);
            RUSS::Mox_Update();
            MOX::self_destruct_seg = farload::Far_Reload_Next("combat.lbx", 3, MOX::screen_seg);
            RUSS::Mox_Update();
            MOX::weapons_button_seg = farload::Far_Reload_Next("combat.lbx", 7, MOX::screen_seg);
            RUSS::Mox_Update();

            for (int16_t i = 0; i < 0x70; ++i) {
                MOX::msl_seg[i] = (s_animation_header*)farload::Far_Reload_Next("cmbtmisl.lbx", i, MOX::screen_seg);
                RUSS::Mox_Update();
            }

            MOX::combat_mouse_grid = (s_mouse_list*)allocate::Allocate_Next_Block(MOX::screen_seg, sizeof(s_mouse_list) * COMBAT_MOUSE_COUNT);
            MOX::specials_button_seg = farload::Far_Reload_Next("combat.lbx", 8, MOX::screen_seg);
            MOX::msl_warning_on_seg = (s_animation_header*)farload::Far_Reload_Next("combat.lbx", 4, MOX::screen_seg);
            RUSS::Mox_Update();
            MOX::warning_missile_seg = (s_animation_header*)farload::Far_Reload_Next("combat.lbx", 0x25, MOX::screen_seg);
            MOX::warning_fighter_seg = farload::Far_Reload_Next("combat.lbx", 0x26, MOX::screen_seg);
            RUSS::Mox_Update();

            for (int16_t i = 0; i < 3; ++i) {
                MOX::ship_cursor[i] = (s_animation_header*)farload::Far_Reload_Next("combat.lbx", i + 0x20, MOX::screen_seg);
                RUSS::Mox_Update();
            }

            MOX::ship_bitmap = (s_animation_header*)allocate::Allocate_Next_Block(MOX::screen_seg, bitmap::Get_Bitmap_Size(0x62, 0x62));
            MOX::draw_ship_bitmap = (s_animation_header*)allocate::Allocate_Next_Block(MOX::screen_seg, bitmap::Get_Bitmap_Size(0x62, 0x62));
            MOX::combat_bitmap = (s_animation_header*)allocate::Allocate_Next_Block(MOX::screen_seg, bitmap::Get_Bitmap_Size(0x62, 0x62));
            RUSS::Mox_Update();

            MOX::shield_div_bitmap[0] = (s_animation_header*)farload::Far_Reload_Next("combat.lbx", 0xE, MOX::screen_seg);
            MOX::shield_div_bitmap[1] = (s_animation_header*)farload::Far_Reload_Next("combat.lbx", 0xF, MOX::screen_seg);
            RUSS::Mox_Update();
            MOX::shield_div_bitmap[2] = (s_animation_header*)farload::Far_Reload_Next("combat.lbx", 0xC, MOX::screen_seg);
            MOX::shield_div_bitmap[3] = (s_animation_header*)farload::Far_Reload_Next("combat.lbx", 0xD, MOX::screen_seg);
            RUSS::Mox_Update();

            for (int16_t i = 0; i < 4; ++i) {
                for (int16_t j = 0; j < 4; ++j) {
                    MOX::shield_mask_bitmap[j + i * 4] = (s_animation_header*)farload::Far_Reload_Next("combat.lbx", j + i * 4 + 0x10, MOX::screen_seg);
                }
                RUSS::Mox_Update();
            }

            MOX::combat_mouse_grid->x1 = 0;
            MOX::combat_mouse_grid->x2 = 0x27F;
            MOX::combat_mouse_grid->y1 = 0;
            MOX::combat_mouse_grid->y2 = 0x15E;
            MOX::combat_mouse_grid->frame = 12;
            MOX::combat_mouse_grid->icon = 2;

            int16_t grid_x = 0;
            int16_t grid_idx = 1;
            for (int16_t col = 0; col < 32; ++col) {
                int16_t grid_y = 0;
                for (int16_t row = 0; row < 18; ++row) {
                    MOX::combat_mouse_grid[grid_idx].x1 = grid_x;
                    MOX::combat_mouse_grid[grid_idx].x2 = grid_x + 19;
                    MOX::combat_mouse_grid[grid_idx].y1 = grid_y;
                    MOX::combat_mouse_grid[grid_idx].y2 = grid_y + 19;
                    MOX::combat_mouse_grid[grid_idx].frame = 0;
                    MOX::combat_mouse_grid[grid_idx].icon = 0;
                    grid_y += 20;
                    grid_idx++;
                }
                grid_x += 20;
            }

            MOX::combat_mouse_grid[COMBAT_MOUSE_PANEL_INDEX].x1 = 0;
            MOX::combat_mouse_grid[COMBAT_MOUSE_PANEL_INDEX].x2 = 0x27F;
            MOX::combat_mouse_grid[COMBAT_MOUSE_PANEL_INDEX].y1 = 0x15F;
            MOX::combat_mouse_grid[COMBAT_MOUSE_PANEL_INDEX].y2 = 0x1DF;
            MOX::combat_mouse_grid[COMBAT_MOUSE_PANEL_INDEX].frame = 0;
            MOX::combat_mouse_grid[COMBAT_MOUSE_PANEL_INDEX].icon = 1;

            if (MOX::game_type != 0 && MOX::game_type != 1) {
                if (MOX::combat_attacker_player < MOX::NUM_PLAYERS && MOX::player[MOX::combat_attacker_player].objectives == PLAYER_OBJECTIVE_HUMAN) {
                    if (MOX::combat_defender_player < MOX::NUM_PLAYERS && MOX::player[MOX::combat_defender_player].objectives == PLAYER_OBJECTIVE_HUMAN) {
                        MOX::net_flag = 1;
                    }
                }
            }

            if (MOX::combat_attacker_player <= MAX_PLAYERS) {
                MOX::fighter_beam_type[MOX::combat_attacker_player] = INITSHIP::Best_Point_Defense_Weapon(MOX::combat_attacker_player);
                MOX::fighter_bomb_type[MOX::combat_attacker_player] = INITSHIP::Best_Bomb_Weapon(MOX::combat_attacker_player);
            }

            if (MOX::combat_defender_player <= MAX_PLAYERS) {
                MOX::fighter_beam_type[MOX::combat_defender_player] = INITSHIP::Best_Point_Defense_Weapon(MOX::combat_defender_player);
                MOX::fighter_bomb_type[MOX::combat_defender_player] = INITSHIP::Best_Bomb_Weapon(MOX::combat_defender_player);
            }

            RUSS::Mox_Update();
            glass_remap_block = palette::Allocate_Glass_Remap_Next_Block(MOX::screen_seg);
            palette::Save_Glass_Remap_Block(glass_remap_block);
            RUSS::Mox_Update();

            if (star_idx == MOX::NUM_STARS) {
                has_bhg = 1;
                has_special_26 = 1;
                Load_Antaran_Defense_Fleet();
                Load_Antaran_Planet();
            } else {
                qsort(plr2_ships, plr2_ship_count, sizeof(*plr2_ships), qsort_ship_array_by_size);
                int16_t loaded_ships_2 = 0;
                for (int16_t i = 0; i < plr2_ship_count; ++i) {
                    if (MOX::ship[plr2_ships[i]].d.ship_type == SHIP_TYPE_COMBAT && loaded_ships_2 < max_ships_per_side) {
                        Load_Combat_Ship(plr2_ships[i], MOX::combat_total_ship_count);

                        for (int j = 0; j < 8; ++j) {
                            switch (MOX::combat_data[MOX::combat_total_ship_count].weapons[j].weapon_id) {
                                case WEAPON_PLASMA_WEB:
                                case WEAPON_CAUSTIC_SLIME:
                                    has_plasma_web = 1;
                                    break;
                                case WEAPON_BLACK_HOLE_GENERATOR:
                                    has_bhg = 1;
                                    break;
                                case WEAPON_STELLAR_CONVERTER:
                                    has_stellar_converter = 1;
                                    break;
                                case WEAPON_TRACTOR_BEAM:
                                    has_tractor_beam = 1;
                                    break;
                            }
                        }

                        // Original binary calls this and discards the result.
                        Does_Combat_Ship_Have_Special(MOX::combat_total_ship_count, 10);
                        if (Does_Combat_Ship_Have_Special(MOX::combat_total_ship_count, SPECIAL_REFLECTION_FIELD) != 0) {
                            has_special_26 = 1;
                        }

                        MOX::combat_total_ship_count++;
                        loaded_ships_2++;
                    }
                    RUSS::Mox_Update();
                }
                Load_Colony_Defense(colony_idx);
            }

            if ((CMBTFIRE::Does_Combat_Planet_Have_Defenses() & COMBAT_PLANET_DEFENSE_STELLAR_CONVERTER) != 0) {
                has_stellar_converter = 1;
            }

            RUSS::Mox_Update();
            Load_Combat_Ship_Palette(MOX::combat_attacker_player);
            RUSS::Mox_Update();
            Load_Combat_Ship_Palette(MOX::combat_defender_player);
            RUSS::Mox_Update();

            bool needs_plasma_darkening = false;
            for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
                s_combat_data* ship = &MOX::combat_data[i];
                if ((CMBTFIR2::combat_bonus.stealth_field_beam_defense != 0 &&
                        struct_::Test_Bit_Field(ship->special_device_flags, SPECIAL_STEALTH_FIELD)) ||
                    struct_::Test_Bit_Field(ship->special_device_flags, SPECIAL_CLOAKING_DEVICE) ||
                    struct_::Test_Bit_Field(ship->special_device_flags, SPECIAL_PHASING_CLOAK)) {
                    needs_plasma_darkening = true;
                    break;
                }
            }

            if (needs_plasma_darkening) {
                MOX::darken_plasma_map = (s_animation_header*)farload::Far_Reload_Next("combat.lbx", 0x31, MOX::screen_seg);
                RUSS::Mox_Update();
                CMBTDRW1::Create_Ship_Darken_Map();
            }

            BEAMS::Load_Combat_Beam_Stuff((int8_t)has_special_26);

            MOX::shield_remap_colors = palette::Allocate_Glass_Remap_Next_Block(MOX::screen_seg);
            RUSS::Mox_Update();
            int16_t shield_c = 23;
            for (int16_t i = 1; i < 16; ++i) {
                palette::Set_Glass_Remap_Color(i, (uint8_t)(i * 2 + 13), (uint8_t)(i * 2 + 30), 63, (uint8_t)shield_c);
                shield_c += 3;
            }
            RUSS::Mox_Update();
            palette::Set_Palette_Gradient(144, 32, 0, 0, 0, 0, 0, 0);
            palette::Set_Palette_Changes(0, 255);
            RUSS::Mox_Update();
            remap::Update_Glass_Remap_Colors();
            palette::Save_Glass_Remap_Block(MOX::shield_remap_colors);
            RUSS::Mox_Update();

            MOX::pulsar_remap_colors = palette::Allocate_Glass_Remap_Next_Block(MOX::screen_seg);
            s_animation_header* pulsar_pal = (s_animation_header*)buffer::Buffer_Reload("cmbtsfx.lbx", 0x34, MOX::global_cache_seg);
            animate::Draw_Palette(pulsar_pal);
            RUSS::Mox_Update();
            int16_t pulsar_c = 53;
            for (int16_t i = 1; i < 16; ++i) {
                palette::Set_Glass_Remap_Color(
                    i,
                    video::current_palette[kPlayerRemapPaletteStart + i].r,
                    video::current_palette[kPlayerRemapPaletteStart + i].g,
                    video::current_palette[kPlayerRemapPaletteStart + i].b,
                    (uint8_t)pulsar_c);
                pulsar_c += 3;
            }
            palette::Set_Palette_Changes(0, 255);
            remap::Update_Glass_Remap_Colors();
            palette::Save_Glass_Remap_Block(MOX::pulsar_remap_colors);

            MOX::warp_core_remap_colors = palette::Allocate_Glass_Remap_Next_Block(MOX::screen_seg);
            s_animation_header* warp_pal = (s_animation_header*)buffer::Buffer_Reload("sphersfx.lbx", 0, MOX::global_cache_seg);
            RUSS::Mox_Update();
            animate::Draw_Palette(warp_pal);
            int16_t warp_c = 53;
            for (int16_t i = 1; i < 16; ++i) {
                palette::Set_Glass_Remap_Color(
                    i,
                    video::current_palette[kPlayerRemapPaletteStart + i].r,
                    video::current_palette[kPlayerRemapPaletteStart + i].g,
                    video::current_palette[kPlayerRemapPaletteStart + i].b,
                    (uint8_t)warp_c);
                warp_c += 3;
            }
            palette::Set_Palette_Changes(0, 255);
            remap::Update_Glass_Remap_Colors();
            palette::Save_Glass_Remap_Block(MOX::warp_core_remap_colors);
            RUSS::Mox_Update();

            if (has_tractor_beam != 0) {
                MOX::small_tractor_seg = (s_animation_header*)farload::Far_Reload_Next("cmbtsfx.lbx", 2, MOX::screen_seg);
                RUSS::Mox_Update();
                MOX::tractor_remap_colors = palette::Allocate_Glass_Remap_Next_Block(MOX::screen_seg);
                int16_t tractor_c = 23;
                for (int16_t i = 1; i < 16; ++i) {
                    palette::Set_Glass_Remap_Color(
                        i,
                        video::current_palette[kTractorBeamRemapPaletteStart + i / 2].r,
                        video::current_palette[kTractorBeamRemapPaletteStart + i / 2].g,
                        video::current_palette[kTractorBeamRemapPaletteStart + i / 2].b,
                        (uint8_t)tractor_c);
                    tractor_c += 3;
                }
                palette::Set_Palette_Changes(0, 255);
                RUSS::Mox_Update();
                remap::Update_Glass_Remap_Colors();
                palette::Save_Glass_Remap_Block(MOX::tractor_remap_colors);
            }

            MOX::stasis_bubble_remap_colors = MOX::shield_remap_colors;

            if (has_bhg != 0) {
                MOX::bhg_remap_colors = palette::Allocate_Glass_Remap_Next_Block(MOX::screen_seg);
                s_animation_header* bhg_pal = (s_animation_header*)buffer::Buffer_Reload("cmbtsfx.lbx", 0x2E, MOX::global_cache_seg);
                animate::Draw_Palette(bhg_pal);
                for (int16_t i = 1; i < 16; ++i) {
                    palette::Set_Glass_Remap_Color(
                        i,
                        video::current_palette[kPlayerRemapPaletteStart + i].r,
                        video::current_palette[kPlayerRemapPaletteStart + i].g,
                        video::current_palette[kPlayerRemapPaletteStart + i].b,
                        i * 2 + 60);
                }
                palette::Set_Palette_Gradient(144, 32, 0, 0, 0, 0, 0, 0);
                palette::Set_Palette_Changes(0, 255);
                RUSS::Mox_Update();
                remap::Update_Glass_Remap_Colors();
                palette::Save_Glass_Remap_Block(MOX::bhg_remap_colors);
            }

            if (has_plasma_web != 0) {
                MOX::plasma_web_remap_colors = palette::Allocate_Glass_Remap_Next_Block(MOX::screen_seg);
                for (int16_t i = 1; i < 16; ++i) {
                    int16_t c = (i * 3 + 15) * 2;
                    if (c > 100) c = 100;
                    palette::Set_Glass_Remap_Color(
                        i,
                        video::current_palette[kPlasmaWebRemapPaletteStart + i].r,
                        video::current_palette[kPlasmaWebRemapPaletteStart + i].g,
                        video::current_palette[kPlasmaWebRemapPaletteStart + i].b,
                        (uint8_t)c);
                }
                RUSS::Mox_Update();
                palette::Set_Palette_Changes(0, 255);
                remap::Update_Glass_Remap_Colors();
                palette::Save_Glass_Remap_Block(MOX::plasma_web_remap_colors);
            }

            if (has_stellar_converter != 0) {
                MOX::stellar_converter_remap_colors = palette::Allocate_Glass_Remap_Next_Block(MOX::screen_seg);
                RUSS::Mox_Update();
                s_animation_header* stellar_pal = (s_animation_header*)buffer::Buffer_Reload("cmbtsfx.lbx", 0x28, MOX::global_cache_seg);
                animate::Draw_Palette(stellar_pal);
                int16_t stellar_c = 53;
                for (int16_t i = 1; i < 16; ++i) {
                    palette::Set_Glass_Remap_Color(
                        i,
                        video::current_palette[kPlayerRemapPaletteStart + i * 2].r,
                        video::current_palette[kPlayerRemapPaletteStart + i * 2].g,
                        video::current_palette[kPlayerRemapPaletteStart + i * 2].b,
                        (uint8_t)stellar_c);
                    stellar_c += 3;
                }
                palette::Set_Palette_Changes(0, 255);
                RUSS::Mox_Update();
                remap::Update_Glass_Remap_Colors();
                palette::Save_Glass_Remap_Block(MOX::stellar_converter_remap_colors);
                palette::Set_Palette_Gradient(144, 32, 0, 0, 0, 0, 0, 0);
            }

            CMBTFIR2::Set_Combat_Ordnance_Values();
            RUSS::Mox_Update();
            Load_Individual_Ship_Pictures(MOX::combat_attacker_player);
            RUSS::Mox_Update();
            Load_Individual_Ship_Pictures(MOX::combat_defender_player);
            RUSS::Mox_Update();
            palette::Restore_Glass_Remap_Block(MOX::shield_remap_colors);

            for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
                Init_Special_Devices(i);
            }

            Deploy_Ships();
            RUSS::Mox_Update();

            MOX::auto_combat_flag[0] = 0;
            MOX::auto_combat_flag[1] = 0;
            MOX::skan_flag = 0;

            COMBAT1::Add_Tactical_Combat_Fields();
            RUSS::Mox_Update();

            combat_winner = COMBAT1::Check_For_Winner();
            fields::Assign_Auto_Function(CMBTDRW1::Draw_Main_Combat_Screen, 1);
            mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);

            int16_t turn_plr1 = MOX::combat_attacker_player;
            int16_t turn_plr2 = MOX::combat_defender_player;
            if (MOX::combat_defender_player == MAX_PLAYERS) {
                turn_plr1 = MAX_PLAYERS;
                turn_plr2 = MOX::combat_attacker_player;
            }

            if (MOX::net_flag != 0) {
                struct_::Clear_Structure(&MOX::new_message, sizeof(MOX::new_message));
                int16_t net_plr = MOX::combat_attacker_player;
                if (MOX::PLAYER_NUM == MOX::combat_attacker_player) {
                    net_plr = MOX::combat_defender_player;
                }
                NETMOX::Mox_Send_Message(MOX::player[net_plr].network_player_id, TACTICAL_NET_PACKET_INIT_SYNC, &MOX::combat_init, sizeof(MOX::combat_init));
                NETMOX::Net_Debug_Log("Wait_Until_Net_Opponent_Finished: caller=%s:%d", __FILE__, __LINE__);
                CMBTMOV1::Wait_Until_Net_Opponent_Finished(0);
            }

            RUSS::Mox_Update();

            if (MOX::net_flag != 0) {
                NETMOX::Net_Debug_Log(
                    "tactical combat loop start local_player=%d attacker=%d defender=%d initiative=%d ships=%d winner=%d",
                    (int)MOX::PLAYER_NUM,
                    (int)MOX::combat_attacker_player,
                    (int)MOX::combat_defender_player,
                    (int)MOX::combat_init,
                    (int)MOX::combat_total_ship_count,
                    (int)combat_winner);
            }

            COMBAT::Open_Battle(&battle_events, Local_Tactical_Observer());
            combat_winner = Run_Tactical_Combat_Rounds(turn_plr1, turn_plr2, combat_winner);

            if (MOX::net_flag != 0) {
                NETMOX::Net_Debug_Log(
                    "tactical combat loop end local_player=%d winner=%d turn=%d cur_ship=%d cur_ptr=%d choice=%d barrier_counter=%u",
                    (int)MOX::PLAYER_NUM,
                    (int)combat_winner,
                    (int)MOX::combat_turn,
                    (int)MOX::cur_ship,
                    (int)MOX::cur_ptr,
                    (int)MOX::tactical_net_choice,
                    (unsigned int)MOX::combat_done_waiting);
            }

            if (MOX::cmbt_colony_id != -1) {
                colony_pop -= MOX::colony[MOX::cmbt_colony_id].n_pops;
            }
        }

        COMBAT::End_Pending_Retreats();
        COMBAT::Retire_Battle();
        End_Of_Combat(star_idx, colony_pop, combat_winner);

        if (combat_winner == 1 || combat_winner == 0x29A) {
            if (MOX::combat_attacker_player < MAX_PLAYERS && MOX::combat_defender_player >= 10 && MOX::ship[plr2_ships[0]].d.speed > 0) {
                NETMOX::Russ_Event_Monster_Update(MOX::combat_defender_player, -1, MOX::combat_attacker_player);
            }
        }

        if (combat_winner == -1 || combat_winner == 0x29A) {
            if (MOX::combat_defender_player < MAX_PLAYERS && MOX::combat_attacker_player >= 10 && MOX::ship[plr1_ships[0]].d.speed > 0) {
                NETMOX::Russ_Event_Monster_Update(MOX::combat_attacker_player, -1, MOX::combat_defender_player);
            }
        }

        if (abort_tactical == 0) {
            fields::Clear_Fields();
            fields::Deactivate_Auto_Function();
            fields::Deactivate_Help_List();
            mouse::Set_Mouse_List(&local_mouse_list, 1);
            ERIC::Fast_Fade_Out();
            palette::Set_Palette_Gradient(160, 95, 0, 0, 0, 0, 0, 0);
            MOX2::Put_Auto_Function(&auto_func);
            MOX::super_fast_flag = 0;
            sound::Fadeout_Sound(-1, 30);
            JIM::Play_Background_Music();
            MAINSCR::Update_Ship_Icons_In_Between_Turns();
            palette::Restore_Glass_Remap_Block(glass_remap_block);
            MISC::Fade_Into_Screen(MAINSCR::Draw_Mini_Main_Screen, &MOX::draw_main_screen_to_back, 0);
        }

        MAINSCR::Allocate_First_Screen_Seg(5);

        MOX::settings.combat_shields_arc_flag = (char)MOX::shields_flag;
        MOX::settings.combat_speedx2_flag = (char)MOX::speedx2_flag;
        MOX::settings.combat_missile_impact_flag = (char)MOX::msl_impact_warning_flag;
        MOX::settings.combat_legal_moves_flag = (char)MOX::draw_legal_boxes_flag;
        MOX::settings.combat_grid_flag = (char)MOX::grid_flag;

        if (combat_winner == 0x29A) {
            combat_winner = 0;
        }

        MOX::FULL_DRAW_SCREEN_FLAG = MOX::net_flag;

        COMBAT::Close_Battle(&battle_events, Local_Tactical_Observer());
        COMBAT::Release_Battle(&battle_events);
        return combat_winner;
    }

    void __cdecl Load_Combat_Antaran_Ship(int16_t ship_idx, int16_t ship_size) {
        switch (ship_size) {
            case HULL_FRIGATE:
                INITSHIP::Load_Small_Antaran_Combat_Ship(ship_idx);
                break;
            case HULL_DESTROYER:
                INITSHIP::Load_Medium_Antaran_Combat_Ship(ship_idx);
                break;
            case HULL_CRUISER:
                INITSHIP::Load_Large_Antaran_Combat_Ship(ship_idx);
                break;
            case HULL_BATTLESHIP:
                INITSHIP::Load_Huge_Antaran_Combat_Ship(ship_idx);
                break;
            case HULL_TITAN:
                INITSHIP::Load_Titan_Antaran_Combat_Ship(ship_idx);
                break;
            default:
                break;
        }
    }

    void __cdecl Load_Antaran_Defense_Fleet() {
        // Ensure at least one Antaran Frigate (defensive_ships[4]) is present
        if (MOX::antarans.defensive_ships[4] < 1) {
            MOX::antarans.defensive_ships[4] = 1;
        }

        int16_t loaded_ships = 0;
        for (int16_t ship_type_idx = 4; ship_type_idx >= 0; --ship_type_idx) {
            for (int16_t i = 0; i < MOX::antarans.defensive_ships[ship_type_idx] &&
                 loaded_ships < max_ships_per_side;
                 ++i) {
                Load_Combat_Antaran_Ship(MOX::combat_total_ship_count, ship_type_idx);
                MOX::combat_total_ship_count++;
                ++loaded_ships;
            }
        }

        Load_Antaran_Star_Fortress();
    }

    void __cdecl Load_Antaran_Star_Fortress() {
        int16_t n = MOX::combat_total_ship_count;
        s_combat_data* ship = &MOX::combat_data[n];
        const SHIP_CONFIG::s_antaran_fortress& fortress =
            SHIP_CONFIG::antaran_fortress;

        ship->computer_type = COMPUTER_MOLECULARTRONIC_COMPUTER;
        ship->armor_type = ARMOR_XENTRONIUM_ARMOR;

        strlcpy(ship->name, TECHDATA::hull_data[HULL_STAR_FORTRESS].name, sizeof(ship->name));

        ship->structure_max = (int16_t)(TECHDATA::hull_data[HULL_STAR_FORTRESS].structure_hp +
            ((int32_t)TECHDATA::armor[ship->armor_type].ships_bonus * TECHDATA::hull_data[HULL_STAR_FORTRESS].structure_hp) / 100);
        ship->structure_max = (int16_t)(ship->structure_max * reinforced_hull_multiplier.antaran_fortress);

        ship->armor_remaining = (int16_t)(TECHDATA::hull_data[HULL_STAR_FORTRESS].armor_hp +
            ((int32_t)TECHDATA::armor[ship->armor_type].ships_bonus * TECHDATA::hull_data[HULL_STAR_FORTRESS].armor_hp) / 100);
        ship->armor_remaining = (int16_t)(ship->armor_remaining *
            antaran_fortress_armor_multiplier);

        int16_t fortress_weapon_space =
            (int16_t)(antaran_star_fortress_space / 2);

        ship->size_class = SHIP_SIZE_TITAN +
            (satellite_size_class_plus_one != 0 ? 1 : 0);
        ship->picture_num = 5;
        ship->owner = (int8_t)MOX::combat_defender_player;
        ship->previous_owner = (uint8_t)MOX::combat_defender_player;
        ship->ship_idx = -1;
        ship->ftl_type = DRIVE_NO_DRIVE;
        ship->shield_type = SHIELD_NO_SHIELD;
        ship->unit_status = 0;

        for (int i = 0; i < 5; ++i) {
            ship->special_device_flags[i] = 0;
            ship->special_device_damage_flags[i] = 0;
        }

        for (uint8_t special : fortress.specials) {
            struct_::Set_Bit_Field(ship->special_device_flags, special);
        }

        for (int i = 0; i < 8; ++i) {
            struct_::Clear_Structure(&ship->weapons[i], sizeof(ship->weapons[i]));
        }

        int w_idx = 0;
        const SHIP_CONFIG::s_antaran_fortress_primary_weapon& primary0 =
            fortress.primary_weapons[0];
        ship->weapons[w_idx].weapon_id = primary0.weapon.type;
        ship->weapons[w_idx].active = 1;
        ship->weapons[w_idx].ammo = -1;
        ship->weapons[w_idx].arc = primary0.weapon.arc;
        ship->weapons[w_idx].specials = primary0.weapon.mods;

        int space0 = DESIGN::Weapon_Space(MOX::combat_defender_player, ship->weapons[w_idx].weapon_id, 1, WEAPON_FIRING_ARC_FORWARD, 0, primary0.space_mods);
        int count0 = (fortress_weapon_space / 2) / space0;
        if (count0 > 99) {
            count0 = 99;
        }
        ship->weapons[w_idx].count = (uint8_t)count0;
        ship->weapons[w_idx].shots_left = (uint8_t)count0;

        w_idx++;
        const SHIP_CONFIG::s_antaran_fortress_primary_weapon& primary1 =
            fortress.primary_weapons[1];
        ship->weapons[w_idx].weapon_id = primary1.weapon.type;
        ship->weapons[w_idx].active = 1;
        ship->weapons[w_idx].ammo = -1;
        ship->weapons[w_idx].arc = primary1.weapon.arc;
        ship->weapons[w_idx].specials = primary1.weapon.mods;

        int space1 = DESIGN::Weapon_Space(MOX::combat_defender_player, ship->weapons[w_idx].weapon_id, 1, WEAPON_FIRING_ARC_FORWARD, 0, primary1.space_mods);
        int count1 = (fortress_weapon_space / 4) / space1;
        if (count1 > 198) {
            count1 = 198;
        }

        if (count1 > 99) {
            int extra = (count1 - 1) / 99;
            count1 = count1 - extra * 99;
            for (int i = 0; i < extra; ++i) {
                ship->weapons[w_idx].count = 99;
                ship->weapons[w_idx].shots_left = 99;
                w_idx++;
                ship->weapons[w_idx].weapon_id = fortress.overflow_weapons[0].type;
                ship->weapons[w_idx].active = 1;
                ship->weapons[w_idx].ammo = -1;
                ship->weapons[w_idx].arc = fortress.overflow_weapons[0].arc;
                ship->weapons[w_idx].specials = fortress.overflow_weapons[0].mods;
            }
        }
        ship->weapons[w_idx].count = (uint8_t)count1;
        ship->weapons[w_idx].shots_left = (uint8_t)count1;

        w_idx++;
        const SHIP_CONFIG::s_antaran_fortress_primary_weapon& primary2 =
            fortress.primary_weapons[2];
        ship->weapons[w_idx].weapon_id = primary2.weapon.type;
        ship->weapons[w_idx].active = 1;
        ship->weapons[w_idx].ammo = -1;
        ship->weapons[w_idx].arc = primary2.weapon.arc;
        ship->weapons[w_idx].specials = primary2.weapon.mods;

        int space2 = DESIGN::Weapon_Space(MOX::combat_defender_player, ship->weapons[w_idx].weapon_id, 1, WEAPON_FIRING_ARC_FORWARD, 0, primary2.space_mods);
        int count2 = (fortress_weapon_space / 4) / space2;
        if (count2 > 198) {
            count2 = 198;
        }

        if (count2 > 99) {
            int extra = (count2 - 1) / 99;
            count2 = count2 - extra * 99;
            for (int i = 0; i < extra; ++i) {
                ship->weapons[w_idx].count = 99;
                ship->weapons[w_idx].shots_left = 99;
                w_idx++;
                ship->weapons[w_idx].weapon_id = fortress.overflow_weapons[1].type;
                ship->weapons[w_idx].active = 1;
                ship->weapons[w_idx].ammo = -1;
                ship->weapons[w_idx].arc = fortress.overflow_weapons[1].arc;
                ship->weapons[w_idx].specials = fortress.overflow_weapons[1].mods;
            }
        }
        ship->weapons[w_idx].count = (uint8_t)count2;
        ship->weapons[w_idx].shots_left = (uint8_t)count2;

        w_idx++;
        const SHIP_CONFIG::s_antaran_fortress_primary_weapon& primary3 =
            fortress.primary_weapons[3];
        ship->weapons[w_idx].weapon_id = primary3.weapon.type;
        ship->weapons[w_idx].active = 1;
        ship->weapons[w_idx].ammo = -1;
        ship->weapons[w_idx].arc = primary3.weapon.arc;
        ship->weapons[w_idx].specials = primary3.weapon.mods;

        int space3 = DESIGN::Weapon_Space(MOX::combat_defender_player, ship->weapons[w_idx].weapon_id, 1, WEAPON_FIRING_ARC_FORWARD, 0, primary3.space_mods);
        int count3 = (fortress_weapon_space / 2) / space3;
        if (count3 > 99) {
            count3 = 99;
        }
        ship->weapons[w_idx].count = (uint8_t)count3;
        ship->weapons[w_idx].shots_left = (uint8_t)count3;

        for (int i = 0; i < 8; ++i) {
            ship->weapon_ready_flags[i] = 1;
        }

        ship->crew_quality = CREW_QUALITY_ELITE;
        ship->officer_idx = 0x42;
        ship->marine_count = (uint8_t)TECHDATA::hull_data[HULL_STAR_FORTRESS].marines;
        ship->current_speed = 0;
        ship->structure_damage = 0;
        ship->is_retreating = 0;

        CMBTFIR2::Init_Internal_Space(ship);

        ship->shield_arc_max = 0;
        for (int i = 0; i < 4; ++i) {
            ship->shield_arc_current[i] = ship->shield_arc_max;
        }

        MOX::combat_total_ship_count++;
    }

    void __cdecl Load_Antaran_Planet() {
        struct_::Clear_Structure(MOX::combat_data, sizeof(*MOX::combat_data));

        strlcpy(MOX::combat_data->name, "Antares", sizeof(MOX::combat_data->name));
        MOX::combat_data->ship_idx = -1;
        MOX::combat_data->officer_idx = -1;
        MOX::combat_data->size_class = SHIP_SIZE_DOOM_STAR;
        MOX::combat_data->unit_status = 0;
        MOX::combat_data->owner = MAX_PLAYERS;
        MOX::combat_data->previous_owner = MAX_PLAYERS;
        MOX::combat_data->ftl_type = DRIVE_NO_DRIVE;
        MOX::combat_data->is_retreating = 0;
        MOX::combat_data->shield_type = SHIELD_NO_SHIELD;
        MOX::combat_data->shield_arc_max = 0;

        for (int i = 0; i < 4; ++i) {
            MOX::combat_data->shield_arc_current[i] = 0;
        }

        MOX::combat_data->computer_type = COMPUTER_MOLECULARTRONIC_COMPUTER;
        MOX::combat_data->armor_type = ARMOR_XENTRONIUM_ARMOR;

        for (int i = 0; i < 5; ++i) {
            MOX::combat_data->special_device_flags[i] = 0;
            MOX::combat_data->special_device_damage_flags[i] = 0;
        }

        CMBTFIR2::Init_Internal_Space(MOX::combat_data);

        MOX::combat_data->shield_max_hits = 0;
        MOX::combat_data->shield_current_hits = 0;

        s_animation_header* palette_anim = (s_animation_header*)buffer::Buffer_Reload("cmbtplnt.lbx", 62, MOX::global_cache_seg);
        animate::Draw_Palette(palette_anim);
        palette::Refresh_Palette();

        MOX::combat_planet_seg = (s_animation_header*)farload::Far_Reload_Next("cmbtplnt.lbx", 60 /* 0x3C */, MOX::screen_seg);
        RUSS::Mox_Update();

        MOX::combat_planet_seg2 = (s_animation_header*)farload::Far_Reload_Next("cmbtplnt.lbx", 61 /* 0x3D */, MOX::screen_seg);
        RUSS::Mox_Update();
    }

    void __cdecl Load_Combat_Ship_Palette(int16_t player_idx) {
        s_animation_header* ship_anim;
        int lbx_index;
        int16_t planet_size;
        int16_t player_color;
        char lbx_filename[32];
        uint8_t color_table[32];

        for (int i = 0; i < 32; ++i) {
            lbx_filename[i] = (char)i + 0x20;
            color_table[i] = (uint8_t)i + 0x60;
        }

        if (player_idx < MAX_PLAYERS) {
            strlcpy(lbx_filename, "cmbtshp.lbx", sizeof(lbx_filename));
            lbx_index = (int)MOX::player[player_idx].color * 45 + 44;
        } else {
            strlcpy(lbx_filename, "monster.lbx", sizeof(lbx_filename));
            lbx_index = player_idx + 5;
        }

        ship_anim = (s_animation_header*)buffer::Buffer_Reload(lbx_filename, lbx_index, MOX::global_cache_seg);

        if (player_idx == MOX::combat_defender_player && player_idx <= MAX_PLAYERS && (MOX::cmbt_colony_id > -1 || player_idx == MAX_PLAYERS)) {
            if (player_idx == MAX_PLAYERS) {
                planet_size = 2;
                player_color = 2;
            } else {
                player_color = (int16_t)MOX::player[player_idx].color;
                planet_size = (int16_t)MOX::planet[MOX::colony[MOX::cmbt_colony_id].planet].size;
            }

            if (planet_size > 0) {
                MOX::combat_map_planet_seg = (s_animation_header*)farload::Far_Reload_Next("combat.lbx", (int)planet_size + (int)player_color * 4 + 0x36, MOX::screen_seg);
            }

            RUSS::Mox_Update();

            if (planet_size > 0) {
                replace::Shift_Palette_Range(ship_anim, 0x60);
                animate::Draw_Palette(ship_anim);
                replace::Replace_Color_Range(MOX::combat_map_planet_seg, color_table, 0x50, 0x10);
            }
        }

        if (player_idx == MOX::combat_attacker_player) {
            replace::Shift_Palette_Range(ship_anim, 0x20);
            animate::Draw_Palette(ship_anim);
            RUSS::Mox_Update();
        } else {
            replace::Shift_Palette_Range(ship_anim, 0x60);
            animate::Draw_Palette(ship_anim);
            RUSS::Mox_Update();
        }
    }

    int8_t __cdecl Ship_Can_Deploy_At(int16_t x, int16_t y, int16_t ship_idx) {
        int8_t result = 1;
        int16_t size_threshold = 0;

        if (MOX::cmbt_colony_id != -1) {
            int16_t planet_idx = MOX::colony[MOX::cmbt_colony_id].planet;
            int8_t planet_size = MOX::planet[planet_idx].size;

            switch (planet_size) {
                case PLANET_SIZE_TINY:
                    size_threshold = 3;
                    break;
                case PLANET_SIZE_SMALL:
                    size_threshold = 4;
                    break;
                case PLANET_SIZE_MEDIUM:
                    size_threshold = 6;
                    break;
                case PLANET_SIZE_LARGE:
                    size_threshold = 7;
                    break;
                case PLANET_SIZE_HUGE:
                    size_threshold = 8;
                    break;
                default:
                    break;
            }

            // Check proximity to the primary combat object (usually a colony/starbase)
            if (MOX::combat_data->x <= x + 2 && x <= MOX::combat_data->x + size_threshold) {
                if (MOX::combat_data->y + size_threshold >= y && y + 2 >= MOX::combat_data->y) {
                    result = 0;
                }
            }
        }

        // Check proximity to another specific unit if provided
        if (ship_idx > -1) {
            if (MOX::combat_data[ship_idx].x <= x + 2 && x <= MOX::combat_data[ship_idx].x + 3) {
                if (MOX::combat_data[ship_idx].y + 3 >= y && y + 2 >= MOX::combat_data[ship_idx].y) {
                    result = 0;
                }
            }
        }

        return result;
    }

    void __cdecl Load_Combat_Colony_Picture(int16_t colony_idx) {
        int16_t planet_idx = MOX::colony[colony_idx].planet;
        s_planet_data* planet = &MOX::planet[planet_idx];

        int8_t climate = planet->climate;
        int8_t size = planet->size;

        s_animation_header* palette_anim = (s_animation_header*)buffer::Buffer_Reload("cmbtplnt.lbx", climate * 6 + 5, MOX::global_cache_seg);
        animate::Draw_Palette(palette_anim);
        RUSS::Mox_Update();

        MOX::combat_planet_seg = (s_animation_header*)farload::Far_Reload_Next("cmbtplnt.lbx", size + (climate * 6), MOX::screen_seg);
        RUSS::Mox_Update();

        if (planet->size != 0) {
            MOX::combat_planet_seg2 = (s_animation_header*)farload::Far_Reload_Next("cmbtplnt.lbx", climate * 6, MOX::screen_seg);
            RUSS::Mox_Update();
        } else {
            MOX::combat_planet_seg2 = MOX::combat_planet_seg;
        }
    }

    void __cdecl Load_Colony_Defense(int16_t colony_idx) {
        if (colony_idx < 0) {
            MOX::combat_data->owner = MAX_PLAYERS;
            MOX::combat_data->previous_owner = MAX_PLAYERS;
            MOX::combat_data->ship_idx = -1;
            MOX::combat_data->unit_status = 5;
        } else {
            COMBINIT::Load_Tactical_Colony(colony_idx, 1);
            int16_t station_type = COMBINIT::Type_Of_Space_Station(MOX::cmbt_colony_id);
            if (station_type != -1) {
                COMBINIT::Load_Combat_Satelite(station_type);
                MOX::combat_total_ship_count++;
            }
        }
    }

    void __cdecl Load_Tactical_Colony(int16_t colony_idx, int16_t load_picture) {
        char* planet_name = HACCESS::Get_Planet_Name_No_Prime(MOX::colony[colony_idx].planet);
        strlcpy(MOX::combat_data->name, planet_name, sizeof(MOX::combat_data->name));

        MOX::combat_data->owner = MOX::colony[colony_idx].owner;
        MOX::combat_data->previous_owner = MOX::colony[colony_idx].owner;
        MOX::combat_data->ship_idx = -1;
        MOX::combat_data->ftl_type = DRIVE_NO_DRIVE;
        MOX::combat_data->officer_idx = -1;
        MOX::combat_data->is_retreating = 0;

        int16_t max_hits = 0;
        if (MOX::colony[colony_idx].buildings[BUILDING_RADIATION_SHIELD] != 0) {
            MOX::combat_data->shield_type = 0x1c;
            max_hits = TECHDATA::planetary_shields[PLANETARY_RADIATION_SHIELD].absorption;
        } else if (MOX::colony[colony_idx].buildings[BUILDING_BARRIER_SHIELD] != 0) {
            MOX::combat_data->shield_type = 0x17;
            max_hits = TECHDATA::planetary_shields[PLANETARY_BARRIER_SHIELD].absorption;
        } else if (MOX::colony[colony_idx].buildings[BUILDING_FLUX_SHIELD] != 0) {
            MOX::combat_data->shield_type = 0x18;
            max_hits = TECHDATA::planetary_shields[PLANETARY_FLUX_SHIELD].absorption;
        } else {
            MOX::combat_data->shield_type = SHIELD_NO_SHIELD;
            max_hits = 0;
        }

        MOX::combat_data->shield_arc_max = max_hits;
        for (int i = 0; i < 4; i++) {
            MOX::combat_data->shield_arc_current[i] = max_hits;
        }

        int16_t shield_id = INITSHIP::Best_Ship_Shield(MOX::combat_defender_player);
        MOX::combat_data->shield_arc_current[0] += TECHDATA::shields[shield_id].shield_class;

        MOX::combat_data->computer_type = (int8_t)INITSHIP::Best_Computer(MOX::colony[colony_idx].owner);
        MOX::combat_data->armor_type = (int8_t)INITSHIP::Best_Armor(MOX::colony[colony_idx].owner);

        MOX::combat_data->size_class = satellite_size_class_plus_one != 0
            ? SHIP_SIZE_LARGE
            : SHIP_SIZE_DOOM_STAR;
        MOX::combat_data->unit_status = 0;

        for (int i = 0; i < 5; i++) {
            MOX::combat_data->special_device_flags[i] = 0;
            MOX::combat_data->special_device_damage_flags[i] = 0;
        }

        for (int i = 0; i < 8; i++) {
            MOX::combat_data->weapons[i].weapon_id = WEAPON_NO_WEAPONS;
        }

        int16_t weapon_index = 0;

        if (MOX::colony[colony_idx].buildings[BUILDING_GROUND_BATTERIES] != 0) {
            int16_t weapon_id_1 = INITSHIP::Best_Beam_Weapon(
                MOX::colony[colony_idx].owner,
                WEAPON_MOD_FLAG_HEAVY_MOUNT);
            uint16_t specials_1 = Defense_Weapon_Mods(
                MOX::combat_defender_player,
                weapon_id_1,
                WEAPON_MOD_FLAG_HEAVY_MOUNT,
                defenses_allowed_mods.heavy);

            int32_t space_1 = DESIGN::Weapon_Space(MOX::combat_defender_player, weapon_id_1, 1, WEAPON_FIRING_ARC_360, 1, specials_1);
            int32_t count_1 = ground_defense.battery_hv_space / space_1;

            MOX::combat_data->weapons[0].weapon_id = weapon_id_1;
            MOX::combat_data->weapons[0].count = (uint8_t)count_1;
            MOX::combat_data->weapons[0].arc = WEAPON_FIRING_ARC_ALL_SECTORS;
            MOX::combat_data->weapons[0].specials = specials_1;
            MOX::combat_data->weapons[0].ammo = -1;
            MOX::combat_data->weapons[0].active = 1;
            MOX::combat_data->weapons[0].shots_left = (uint8_t)count_1;
            MOX::combat_data->weapons[0].status = 0;

            int16_t weapon_id_2 = Best_Defense_Point_Defense_Weapon(
                MOX::colony[colony_idx].owner);
            uint16_t specials_2 = Defense_Weapon_Mods(
                MOX::combat_defender_player,
                weapon_id_2,
                WEAPON_MOD_FLAG_POINT_DEFENSE,
                defenses_allowed_mods.pd);

            int32_t space_2 = DESIGN::Weapon_Space(MOX::combat_defender_player, weapon_id_2, 1, WEAPON_FIRING_ARC_360, 1, specials_2);
            int32_t count_2 = ground_defense.battery_pd_space / space_2;

            MOX::combat_data->weapons[1].weapon_id = weapon_id_2;
            MOX::combat_data->weapons[1].count = (uint8_t)count_2;
            MOX::combat_data->weapons[1].arc = WEAPON_FIRING_ARC_ALL_SECTORS;
            MOX::combat_data->weapons[1].specials = specials_2;
            MOX::combat_data->weapons[1].ammo = -1;
            MOX::combat_data->weapons[1].active = 1;
            MOX::combat_data->weapons[1].shots_left = (uint8_t)count_2;
            MOX::combat_data->weapons[1].status = 0;

            weapon_index = 2;
        }

        if (MOX::colony[colony_idx].buildings[BUILDING_FIGHTER_GARRISON] != 0) {
            int16_t weapon_id_fg = WEAPON_INTERCEPTOR;
            int16_t count_fg = ground_defense.garrison_interceptors;

            if (MOX::player[MOX::colony[colony_idx].owner].tech_applications[TECH_APP_HEAVY_FIGHTER_BAYS] == TECH_RESEARCH_STATUS_RESEARCHED) {
                weapon_id_fg = WEAPON_HEAVY_FIGHTER;
                count_fg = ground_defense.garrison_heavy_fighters;
            } else if (MOX::player[MOX::colony[colony_idx].owner].tech_applications[TECH_APP_BOMBER_BAYS] == TECH_RESEARCH_STATUS_RESEARCHED) {
                weapon_id_fg = WEAPON_BOMBER;
                count_fg = ground_defense.garrison_bombers;
            }

            for (int i = 0; i < ground_defense.garrison_slots; ++i) {
                MOX::combat_data->weapons[weapon_index].weapon_id = weapon_id_fg;
                MOX::combat_data->weapons[weapon_index].count = (uint8_t)count_fg;
                MOX::combat_data->weapons[weapon_index].arc = WEAPON_FIRING_ARC_ALL_SECTORS;
                MOX::combat_data->weapons[weapon_index].specials = 0;
                MOX::combat_data->weapons[weapon_index].ammo =
                    ground_defense.garrison_ammo;
                MOX::combat_data->weapons[weapon_index].active = 1;
                MOX::combat_data->weapons[weapon_index].status = 0;
                MOX::combat_data->weapons[weapon_index].shots_left = (uint8_t)count_fg;
                weapon_index++;
            }
        }

        if (MOX::colony[colony_idx].buildings[BUILDING_STELLAR_CONVERTER] != 0) {
            MOX::combat_data->weapons[weapon_index].weapon_id = WEAPON_STELLAR_CONVERTER;
            MOX::combat_data->weapons[weapon_index].count =
                ground_defense.stellar_amount;
            MOX::combat_data->weapons[weapon_index].arc = WEAPON_FIRING_ARC_ALL_SECTORS;
            MOX::combat_data->weapons[weapon_index].specials = 0;
            MOX::combat_data->weapons[weapon_index].status = 0;
            MOX::combat_data->weapons[weapon_index].ammo = -1;
            MOX::combat_data->weapons[weapon_index].active = 1;
            MOX::combat_data->weapons[weapon_index].shots_left =
                ground_defense.stellar_amount;
            weapon_index++;
        }

        if (MOX::colony[colony_idx].buildings[BUILDING_MISSILE_BASE] != 0) {
            int16_t weapon_id_m = INITSHIP::Best_Missile_Weapon_NT(MOX::colony[colony_idx].owner);
            uint16_t specials_m = Defense_Weapon_Mods(
                MOX::combat_defender_player,
                weapon_id_m,
                WEAPON_MOD_FLAG_ECCM,
                defenses_allowed_mods.missile_torp);
            const uint16_t missile_cost_mods =
                specials_m & (uint16_t)~WEAPON_MOD_FLAG_ECCM;

            int32_t space_m = DESIGN::Weapon_Space(MOX::combat_defender_player, weapon_id_m, 1, WEAPON_FIRING_ARC_360, missile_base_ammo_basis, missile_cost_mods);
            int32_t total_count = ground_defense.missile_base_space / space_m;
            int32_t slots = 1;
            int32_t count_per_slot = total_count;

            if (total_count > 10) {
                slots = 2;
                count_per_slot = total_count / 2;
            }

            for (int i = 0; i < slots; i++) {
                MOX::combat_data->weapons[weapon_index].weapon_id = weapon_id_m;
                MOX::combat_data->weapons[weapon_index].count = (uint8_t)count_per_slot;
                MOX::combat_data->weapons[weapon_index].arc = WEAPON_FIRING_ARC_ALL_SECTORS;
                MOX::combat_data->weapons[weapon_index].specials = specials_m;
                MOX::combat_data->weapons[weapon_index].status = 0;

                if (TECHDATA::weapons[weapon_id_m].type == WEAPON_TYPE_MISSILE) {
                    MOX::combat_data->weapons[weapon_index].ammo =
                        ground_defense.missile_base_ammo;
                } else {
                    MOX::combat_data->weapons[weapon_index].ammo = 0;
                }

                MOX::combat_data->weapons[weapon_index].active = 1;
                MOX::combat_data->weapons[weapon_index].shots_left = (uint8_t)count_per_slot;
                weapon_index++;
            }
        }

        if (weapon_index > 0) {
            const s_colony* colony = &MOX::colony[colony_idx];
            const s_player* defender =
                &MOX::player[MOX::combat_defender_player];
            const bool has_ground_batteries =
                colony->buildings[BUILDING_GROUND_BATTERIES] != 0;
            const bool has_stellar_converter =
                colony->buildings[BUILDING_STELLAR_CONVERTER] != 0;

            if (has_ground_batteries && planet.rangemaster != 0 &&
                defender->tech_applications[TECH_APP_RANGEMASTER_UNIT] ==
                    TECH_RESEARCH_STATUS_RESEARCHED) {
                struct_::Set_Bit_Field(
                    MOX::combat_data->special_device_flags,
                    SPECIAL_RANGEMASTER_UNIT);
            }
            if (has_ground_batteries && planet.hef != 0 &&
                defender->tech_applications[TECH_APP_HIGH_ENERGY_FOCUS] ==
                    TECH_RESEARCH_STATUS_RESEARCHED) {
                struct_::Set_Bit_Field(
                    MOX::combat_data->special_device_flags,
                    SPECIAL_HIGH_ENERGY_FOCUS);
            }
            if ((has_ground_batteries || has_stellar_converter) &&
                planet.analyzer != 0 &&
                defender->tech_applications[TECH_APP_STRUCTURAL_ANALYZER] ==
                    TECH_RESEARCH_STATUS_RESEARCHED) {
                struct_::Set_Bit_Field(
                    MOX::combat_data->special_device_flags,
                    SPECIAL_STRUCTURAL_ANALYZER);
            }
            if ((has_ground_batteries || has_stellar_converter) &&
                planet.achilles != 0 &&
                defender->tech_applications[TECH_APP_ACHILLES_TARGETING_UNIT] ==
                    TECH_RESEARCH_STATUS_RESEARCHED) {
                struct_::Set_Bit_Field(
                    MOX::combat_data->special_device_flags,
                    SPECIAL_ACHILLES_TARGETING_UNIT);
            }

            if (planet_damper_enabled != 0 &&
                defender->tech_applications[TECH_APP_DAMPER_FIELD] ==
                    TECH_RESEARCH_STATUS_RESEARCHED) {
                struct_::Set_Bit_Field(
                    MOX::combat_data->special_device_flags,
                    SPECIAL_DAMPER_FIELD);
            }

            if (defender->tech_applications[TECH_APP_BATTLE_SCANNER] ==
                TECH_RESEARCH_STATUS_RESEARCHED) {
                struct_::Set_Bit_Field(MOX::combat_data->special_device_flags, SPECIAL_BATTLE_SCANNER);
            }
        }

        MOX::combat_data->structure_max = 0;
        MOX::combat_data->crew_quality = CREW_QUALITY_REGULAR;
        if (satellite_warlord_bonus != 0 &&
            (uint8_t)MOX::combat_data->owner < MAX_PLAYERS &&
            MOX::player[MOX::combat_data->owner].traits[TRAIT_WARLORD] != 0) {
            ++MOX::combat_data->crew_quality;
        }
        MOX::combat_data->officer_idx = -1;
        MOX::combat_data->marine_count = 0;
        MOX::combat_data->structure_damage = 0;
        MOX::combat_data->current_speed = 0;
        CMBTFIR2::Init_Internal_Space(MOX::combat_data);
        MOX::combat_data->shield_max_hits = 0;
        MOX::combat_data->shield_current_hits = 0;

        if (load_picture != 0) {
            COMBINIT::Load_Combat_Colony_Picture(colony_idx);
        }
    }

    void __cdecl Load_Combat_Satellite_Specials(
        s_combat_data* satellite, const s_player* defender) {
        int special_count = 0;
        if (Install_Satellite_Special(
                satellite, defender, SPECIAL_HEAVY_ARMOR, &special_count)) {
            satellite->armor_remaining *= 3;
        }

        for (int slot = 0; slot < STARBASE_SPECIAL_SLOT_COUNT; ++slot) {
            for (int alternative = 0;
                 alternative < STARBASE_SPECIAL_ALTERNATIVE_COUNT;
                 ++alternative) {
                if (Install_Satellite_Special(
                        satellite,
                        defender,
                        starbase_special[slot][alternative],
                        &special_count)) {
                    break;
                }
            }
        }
    }

    void __cdecl Load_Combat_Satelite(int16_t station_type) {
        int16_t defender = MOX::combat_defender_player;
        MOX::combat_data[MOX::combat_total_ship_count].computer_type = (int8_t)INITSHIP::Best_Computer(defender);
        MOX::combat_data[MOX::combat_total_ship_count].armor_type = (int8_t)INITSHIP::Best_Armor(defender);

        int16_t armor_absorption = TECHDATA::armor[MOX::combat_data[MOX::combat_total_ship_count].armor_type].ships_bonus;
        int32_t beam_space = 0;
        int32_t missile_space = 0;

        if (station_type == HULL_STAR_BASE) {
            strlcpy(MOX::combat_data[MOX::combat_total_ship_count].name, TECHDATA::hull_data[HULL_STAR_BASE].name, sizeof(MOX::combat_data[MOX::combat_total_ship_count].name));
            MOX::combat_data[MOX::combat_total_ship_count].structure_max = TECHDATA::hull_data[HULL_STAR_BASE].structure_hp;
            MOX::combat_data[MOX::combat_total_ship_count].armor_remaining = TECHDATA::hull_data[HULL_STAR_BASE].armor_hp +
                (int16_t)(((int32_t)armor_absorption * TECHDATA::hull_data[HULL_STAR_BASE].armor_hp) / 100);

            int32_t size = TECHDATA::hull_data[HULL_STAR_BASE].size;
            if (MOX::player[defender].tech_applications[TECH_APP_MEGAFLUXERS] == TECH_RESEARCH_STATUS_RESEARCHED) {
                size = DESIGN::Apply_Megafluxers_Tactical_Bonus(size);
            }
            beam_space = size / 2;
            missile_space = size - beam_space;
            MOX::combat_data[MOX::combat_total_ship_count].size_class =
                SHIP_SIZE_LARGE + (satellite_size_class_plus_one != 0 ? 1 : 0);
            MOX::combat_data[MOX::combat_total_ship_count].picture_num = 40;
        } else if (station_type == HULL_BATTLESTATION) {
            strlcpy(MOX::combat_data[MOX::combat_total_ship_count].name, TECHDATA::hull_data[HULL_BATTLESTATION].name, sizeof(MOX::combat_data[MOX::combat_total_ship_count].name));
            MOX::combat_data[MOX::combat_total_ship_count].structure_max = TECHDATA::hull_data[HULL_BATTLESTATION].structure_hp;
            MOX::combat_data[MOX::combat_total_ship_count].armor_remaining = TECHDATA::hull_data[HULL_BATTLESTATION].armor_hp +
                (int16_t)(((int32_t)armor_absorption * TECHDATA::hull_data[HULL_BATTLESTATION].armor_hp) / 100);

            int32_t size = TECHDATA::hull_data[HULL_BATTLESTATION].size;
            if (MOX::player[defender].tech_applications[TECH_APP_MEGAFLUXERS] == TECH_RESEARCH_STATUS_RESEARCHED) {
                size = DESIGN::Apply_Megafluxers_Tactical_Bonus(size);
            }
            beam_space = size / 2;
            missile_space = size - beam_space;
            MOX::combat_data[MOX::combat_total_ship_count].size_class =
                SHIP_SIZE_HUGE + (satellite_size_class_plus_one != 0 ? 1 : 0);
            MOX::combat_data[MOX::combat_total_ship_count].picture_num = 41;
        } else if (station_type == HULL_STAR_FORTRESS) {
            strlcpy(MOX::combat_data[MOX::combat_total_ship_count].name, TECHDATA::hull_data[HULL_STAR_FORTRESS].name, sizeof(MOX::combat_data[MOX::combat_total_ship_count].name));
            MOX::combat_data[MOX::combat_total_ship_count].structure_max = TECHDATA::hull_data[HULL_STAR_FORTRESS].structure_hp;
            MOX::combat_data[MOX::combat_total_ship_count].armor_remaining = TECHDATA::hull_data[HULL_STAR_FORTRESS].armor_hp +
                (int16_t)(((int32_t)armor_absorption * TECHDATA::hull_data[HULL_STAR_FORTRESS].armor_hp) / 100);

            int32_t size = TECHDATA::hull_data[HULL_STAR_FORTRESS].size;
            if (MOX::player[defender].tech_applications[TECH_APP_MEGAFLUXERS] == TECH_RESEARCH_STATUS_RESEARCHED) {
                size = DESIGN::Apply_Megafluxers_Tactical_Bonus(size);
            }
            beam_space = size / 2;
            missile_space = size - beam_space;
            MOX::combat_data[MOX::combat_total_ship_count].size_class =
                SHIP_SIZE_TITAN + (satellite_size_class_plus_one != 0 ? 1 : 0);
            MOX::combat_data[MOX::combat_total_ship_count].picture_num = 42;
        }

        MOX::combat_data[MOX::combat_total_ship_count].owner = (int8_t)defender;
        MOX::combat_data[MOX::combat_total_ship_count].previous_owner = (uint8_t)defender;
        MOX::combat_data[MOX::combat_total_ship_count].ship_idx = -1;
        MOX::combat_data[MOX::combat_total_ship_count].ftl_type = DRIVE_NO_DRIVE;
        MOX::combat_data[MOX::combat_total_ship_count].shield_type = (int8_t)INITSHIP::Best_Ship_Shield(defender);
        MOX::combat_data[MOX::combat_total_ship_count].unit_status = 0;

        for (int i = 0; i < 5; ++i) {
            MOX::combat_data[MOX::combat_total_ship_count].special_device_flags[i] = 0;
            MOX::combat_data[MOX::combat_total_ship_count].special_device_damage_flags[i] = 0;
        }

        Load_Combat_Satellite_Specials(
            &MOX::combat_data[MOX::combat_total_ship_count],
            &MOX::player[defender]);

        MOX::combat_data[MOX::combat_total_ship_count].structure_max +=
            (int16_t)(((int32_t)MOX::combat_data[MOX::combat_total_ship_count].structure_max * armor_absorption) / 100);

        if (COMBINIT::Does_Combat_Ship_Have_Special(MOX::combat_total_ship_count, SPECIAL_REINFORCED_HULL)) {
            MOX::combat_data[MOX::combat_total_ship_count].structure_max *=
                reinforced_hull_multiplier.regular_satellites;
        }

        for (int i = 0; i < 8; ++i) {
            struct_::Clear_Structure(&MOX::combat_data[MOX::combat_total_ship_count].weapons[i], sizeof(MOX::combat_data[MOX::combat_total_ship_count].weapons[i]));
        }

        int beam_idx = 0;
        int16_t beam_id_1 = INITSHIP::Best_Beam_Weapon(
            defender, WEAPON_MOD_FLAG_HEAVY_MOUNT);
        uint16_t beam_mods_1 = Defense_Weapon_Mods(
            defender,
            beam_id_1,
            WEAPON_MOD_FLAG_HEAVY_MOUNT,
            defenses_allowed_mods.heavy);
        MOX::combat_data[MOX::combat_total_ship_count].weapons[beam_idx].weapon_id = beam_id_1;
        MOX::combat_data[MOX::combat_total_ship_count].weapons[beam_idx].active = 1;
        MOX::combat_data[MOX::combat_total_ship_count].weapons[beam_idx].ammo = -1;
        MOX::combat_data[MOX::combat_total_ship_count].weapons[beam_idx].arc = WEAPON_FIRING_ARC_ALL_SECTORS;
        MOX::combat_data[MOX::combat_total_ship_count].weapons[beam_idx].specials = beam_mods_1;
        int beam_space_1 = DESIGN::Weapon_Space(
            defender, beam_id_1, 1, WEAPON_FIRING_ARC_360, 0, beam_mods_1);
        int count1 = (int)(Satellite_Allocated_Space(
                               beam_space, satellite_space_allocation.heavy) /
            beam_space_1);
        if (count1 < 1) count1 = 1;
        if (count1 > 99) count1 = 99;
        MOX::combat_data[MOX::combat_total_ship_count].weapons[beam_idx].count = (uint8_t)count1;
        MOX::combat_data[MOX::combat_total_ship_count].weapons[beam_idx].shots_left = (uint8_t)count1;
        beam_idx++;

        int16_t beam_id_2 = INITSHIP::Best_Beam_Weapon(defender);
        uint16_t beam_mods_2 = Defense_Weapon_Mods(
            defender,
            beam_id_2,
            0,
            defenses_allowed_mods.normal);
        MOX::combat_data[MOX::combat_total_ship_count].weapons[beam_idx].weapon_id = beam_id_2;
        MOX::combat_data[MOX::combat_total_ship_count].weapons[beam_idx].active = 1;
        MOX::combat_data[MOX::combat_total_ship_count].weapons[beam_idx].ammo = -1;
        MOX::combat_data[MOX::combat_total_ship_count].weapons[beam_idx].arc = WEAPON_FIRING_ARC_ALL_SECTORS;
        MOX::combat_data[MOX::combat_total_ship_count].weapons[beam_idx].specials = beam_mods_2;
        int beam_space_2 = DESIGN::Weapon_Space(
            defender, beam_id_2, 1, WEAPON_FIRING_ARC_360, 0, beam_mods_2);
        int count2 = (int)(Satellite_Allocated_Space(
                               beam_space, satellite_space_allocation.normal) /
            beam_space_2);
        if (count2 < 1) count2 = 1;
        if (count2 > 198) count2 = 198;

        if (count2 > 99) {
            int stacks = (count2 - 1) / 99;
            count2 -= stacks * 99;
            for (int i = 0; i < stacks; ++i) {
                MOX::combat_data[MOX::combat_total_ship_count].weapons[beam_idx].count = 99;
                MOX::combat_data[MOX::combat_total_ship_count].weapons[beam_idx].shots_left = 99;
                beam_idx++;
                // Bug in original binary: potential buffer overflow into structure_max since beam_idx is not checked against 8 here
                MOX::combat_data[MOX::combat_total_ship_count].weapons[beam_idx].weapon_id = INITSHIP::Best_Beam_Weapon(defender);
                MOX::combat_data[MOX::combat_total_ship_count].weapons[beam_idx].active = 1;
                MOX::combat_data[MOX::combat_total_ship_count].weapons[beam_idx].ammo = -1;
                MOX::combat_data[MOX::combat_total_ship_count].weapons[beam_idx].arc = WEAPON_FIRING_ARC_ALL_SECTORS;
                MOX::combat_data[MOX::combat_total_ship_count].weapons[beam_idx].specials = beam_mods_2;
            }
        }
        MOX::combat_data[MOX::combat_total_ship_count].weapons[beam_idx].count = (uint8_t)count2;
        MOX::combat_data[MOX::combat_total_ship_count].weapons[beam_idx].shots_left = (uint8_t)count2;
        beam_idx++;

        int16_t pd_id = Best_Defense_Point_Defense_Weapon(defender);
        uint16_t pd_mods = Defense_Weapon_Mods(
            defender,
            pd_id,
            WEAPON_MOD_FLAG_POINT_DEFENSE,
            defenses_allowed_mods.pd);
        MOX::combat_data[MOX::combat_total_ship_count].weapons[beam_idx].weapon_id = pd_id;
        MOX::combat_data[MOX::combat_total_ship_count].weapons[beam_idx].active = 1;
        MOX::combat_data[MOX::combat_total_ship_count].weapons[beam_idx].ammo = -1;
        MOX::combat_data[MOX::combat_total_ship_count].weapons[beam_idx].arc = WEAPON_FIRING_ARC_ALL_SECTORS;
        MOX::combat_data[MOX::combat_total_ship_count].weapons[beam_idx].specials = pd_mods;
        int pd_space = DESIGN::Weapon_Space(
            defender,
            pd_id,
            1,
            WEAPON_FIRING_ARC_360,
            0,
            pd_mods);
        int pd_count = (int)(Satellite_Allocated_Space(
                                 beam_space, satellite_space_allocation.pd) /
            pd_space);
        if (pd_count < 1) pd_count = 1;
        if (pd_count > 198) pd_count = 198;

        if (pd_count > 99) {
            int stacks = (pd_count - 1) / 99;
            pd_count -= stacks * 99;
            for (int i = 0; i < stacks; ++i) {
                MOX::combat_data[MOX::combat_total_ship_count].weapons[beam_idx].count = 99;
                MOX::combat_data[MOX::combat_total_ship_count].weapons[beam_idx].shots_left = 99;
                beam_idx++;
                // Bug in original binary: potential buffer overflow into structure_max since beam_idx is not checked against 8 here
                MOX::combat_data[MOX::combat_total_ship_count].weapons[beam_idx].weapon_id = pd_id;
                MOX::combat_data[MOX::combat_total_ship_count].weapons[beam_idx].active = 1;
                MOX::combat_data[MOX::combat_total_ship_count].weapons[beam_idx].ammo = -1;
                MOX::combat_data[MOX::combat_total_ship_count].weapons[beam_idx].arc = WEAPON_FIRING_ARC_ALL_SECTORS;
                MOX::combat_data[MOX::combat_total_ship_count].weapons[beam_idx].specials = pd_mods;
            }
        }
        MOX::combat_data[MOX::combat_total_ship_count].weapons[beam_idx].count = (uint8_t)pd_count;
        MOX::combat_data[MOX::combat_total_ship_count].weapons[beam_idx].shots_left = (uint8_t)pd_count;
        beam_idx++;

        int16_t missile_id = INITSHIP::Best_Missile_Weapon_NT(defender);
        uint16_t missile_specials = Defense_Weapon_Mods(
            defender,
            missile_id,
            WEAPON_MOD_FLAG_ECCM,
            defenses_allowed_mods.missile_torp);
        const uint16_t missile_cost_mods =
            missile_specials & (uint16_t)~WEAPON_MOD_FLAG_ECCM;

        int missile_space_calc = DESIGN::Weapon_Space(
            defender,
            missile_id,
            1,
            WEAPON_FIRING_ARC_360,
            satellite_ammo_basis,
            missile_cost_mods);
        int missile_count = (int)(Satellite_Allocated_Space(
                                      missile_space, satellite_space_allocation.missile_torp) /
            missile_space_calc);
        if (missile_count < 1) missile_count = 1;
        if (missile_count > 297) missile_count = 297;

        int missile_slot_count = 1;
        if (missile_count == 4) {
            missile_slot_count = 2;
        } else if (missile_count > 4) {
            missile_slot_count = 3;
        }

        const int missile_count_per_slot = missile_count / missile_slot_count;
        const int missile_remainder = missile_count % missile_slot_count;
        for (int missile_slot_idx = 0;
             missile_slot_idx < missile_slot_count;
             ++missile_slot_idx) {
            int slot_missile_count = missile_count_per_slot;
            if (missile_remainder == 2 && missile_slot_idx < 2) {
                ++slot_missile_count;
            } else if (missile_remainder == 1 &&
                missile_slot_idx == missile_slot_count - 1) {
                ++slot_missile_count;
            }

            s_combat_weapon_slot* missile_slot =
                &MOX::combat_data[MOX::combat_total_ship_count]
                     .weapons[beam_idx];
            missile_slot->weapon_id = missile_id;
            missile_slot->active = 1;
            if (TECHDATA::weapons[missile_id].type == WEAPON_TYPE_MISSILE) {
                missile_slot->ammo = 20;
            } else {
                missile_slot->ammo = 0;
            }
            missile_slot->arc = WEAPON_FIRING_ARC_ALL_SECTORS;
            missile_slot->specials = missile_specials;
            missile_slot->count = (uint8_t)slot_missile_count;
            missile_slot->shots_left = (uint8_t)slot_missile_count;
            ++beam_idx;
        }

        for (int i = 0; i < 8; ++i) {
            MOX::combat_data[MOX::combat_total_ship_count].weapon_ready_flags[i] = 1;
        }

        MOX::combat_data[MOX::combat_total_ship_count].crew_quality = CREW_QUALITY_REGULAR;
        if (satellite_warlord_bonus != 0 &&
            (uint16_t)defender < MAX_PLAYERS &&
            MOX::player[defender].traits[TRAIT_WARLORD] != 0) {
            ++MOX::combat_data[MOX::combat_total_ship_count].crew_quality;
        }
        MOX::combat_data[MOX::combat_total_ship_count].officer_idx = -1;
        MOX::combat_data[MOX::combat_total_ship_count].marine_count = (uint8_t)TECHDATA::hull_data[station_type].marines;
        MOX::combat_data[MOX::combat_total_ship_count].current_speed = 0;
        MOX::combat_data[MOX::combat_total_ship_count].structure_damage = 0;
        MOX::combat_data[MOX::combat_total_ship_count].is_retreating = 0;

        if (Does_Combat_Ship_Have_Special(
                MOX::combat_total_ship_count, SPECIAL_DAMPER_FIELD) != 0) {
            MOX::combat_data[MOX::combat_total_ship_count].shield_type =
                SHIELD_NO_SHIELD;
        }

        CMBTFIR2::Init_Internal_Space(&MOX::combat_data[MOX::combat_total_ship_count]);

        if (MOX::combat_data[MOX::combat_total_ship_count].shield_type > 0) {
            MOX::combat_data[MOX::combat_total_ship_count].shield_arc_max = Calc_Combat_Shield_Max(MOX::combat_total_ship_count);
        } else {
            MOX::combat_data[MOX::combat_total_ship_count].shield_arc_max = 0;
        }

        for (int i = 0; i < 4; ++i) {
            MOX::combat_data[MOX::combat_total_ship_count].shield_arc_current[i] = MOX::combat_data[MOX::combat_total_ship_count].shield_arc_max;
        }
    }

    void __cdecl Deploy_Ships() {
        int16_t stationary_ship_idx = -1;
        int16_t defender_deploy_x;
        int16_t attacker_deploy_x;
        int16_t defender_deploy_x_step;
        int16_t attacker_deploy_x_step;
        int8_t attacker_facing;
        int8_t defender_facing;
        int8_t saved_size;

        int16_t side_counts[2] = {};
        for (int16_t i = 1; i < MOX::combat_total_ship_count; ++i) {
            if (!Is_Combat_Satellite(&MOX::combat_data[i])) {
                ++side_counts[MOX::combat_data[i].owner == MOX::combat_attacker_player];
            }
        }
        const int16_t largest_side = side_counts[0] > side_counts[1]
            ? side_counts[0]
            : side_counts[1];
        MOX::combat_grid_width = ORIGINAL_COMBAT_GRID_WIDTH;
        // More vertical lanes increase area without changing the opening firing range.
        MOX::combat_grid_height = largest_side > ORIGINAL_MAX_COMBAT_SHIPS_PER_SIDE
            ? (int16_t)((ORIGINAL_COMBAT_GRID_HEIGHT * largest_side +
                            ORIGINAL_MAX_COMBAT_SHIPS_PER_SIDE - 1) /
                  ORIGINAL_MAX_COMBAT_SHIPS_PER_SIDE)
            : ORIGINAL_COMBAT_GRID_HEIGHT;

        if ((MOX::PLAYER_NUM == MOX::combat_attacker_player) || (MOX::net_flag == 1)) {
            defender_deploy_x = 0x37 /* 55 */;
            attacker_deploy_x = 0x19 /* 25 */;
            defender_deploy_x_step = 3;
            attacker_deploy_x_step = -3;
            MOX::combat_data[0].x = 65;
            attacker_facing = 0;
            defender_facing = 8;
        } else {
            defender_deploy_x = 0x19 /* 25 */;
            attacker_deploy_x = 0x37 /* 55 */;
            defender_deploy_x_step = -3;
            attacker_deploy_x_step = 3;
            MOX::combat_data[0].x = 10;
            attacker_facing = 8;
            defender_facing = 0;
        }

        MOX::combat_data[0].y = MOX::combat_grid_height / 2;
        const int8_t planet_size = MOX::combat_data[0].size_class;
        MOX::combat_data[0].size_class = 100;

        for (int16_t i = 1; i < MOX::combat_total_ship_count; ++i) {
            if (Is_Combat_Satellite(&MOX::combat_data[i])) {
                saved_size = MOX::combat_data[i].size_class;
                MOX::combat_data[i].size_class = 99;
            }
        }

        qsort(MOX::combat_data, MOX::combat_total_ship_count, sizeof(*MOX::combat_data), qsort_combat_data_by_size);

        MOX::combat_data[0].size_class = planet_size;

        for (int16_t i = 1; i < MOX::combat_total_ship_count; ++i) {
            if (Is_Combat_Satellite(&MOX::combat_data[i])) {
                MOX::combat_data[i].size_class = saved_size;
            }
        }

        s_deployment_cursor attacker_cursor = {
            attacker_deploy_x,
            attacker_deploy_x_step,
        };
        s_deployment_cursor defender_cursor = {
            defender_deploy_x,
            defender_deploy_x_step,
        };

        for (int16_t i = 1; i < MOX::combat_total_ship_count; ++i) {
            s_combat_data* ship = &MOX::combat_data[i];
            if (Is_Combat_Satellite(ship)) {
                int16_t x_pos;
                if (defender_deploy_x < 40) {
                    x_pos = MOX::combat_data[0].x + 11;
                } else {
                    x_pos = MOX::combat_data[0].x - 4;
                }
                ship->x = x_pos;
                ship->y = MOX::combat_data[0].y + 1;
                stationary_ship_idx = i;
            } else {
                if (ship->owner == MOX::combat_attacker_player) {
                    int16_t x;
                    int16_t y;
                    bool deployed_down;
                    Take_Deployment_Candidate(
                        &attacker_cursor,
                        ship->size_class,
                        &x,
                        &y,
                        &deployed_down);
                    ship->x = x;
                    ship->y = y;
                    attacker_cursor.deploy_down = !deployed_down;
                    ship->facing_dir = attacker_facing;
                } else {
                    int16_t x;
                    int16_t y;
                    bool deployed_down;
                    do {
                        Take_Deployment_Candidate(
                            &defender_cursor,
                            ship->size_class,
                            &x,
                            &y,
                            &deployed_down);
                    } while (!Ship_Can_Deploy_At(
                        x, y, stationary_ship_idx));

                    ship->x = x;
                    ship->y = y;
                    defender_cursor.deploy_down = !deployed_down;
                    ship->facing_dir = defender_facing;
                }
            }
        }
    }

    void __cdecl Snap_Center_Combat_Screen(int16_t target_x, int16_t target_y) {
        int16_t new_combat_x;
        int16_t new_combat_y;
        int16_t delta_x;
        int16_t delta_y;
        int16_t near_x;
        int16_t mid_x;
        int16_t far_x;

        new_combat_x = target_x - 16;
        new_combat_y = target_y - 9;

        if (new_combat_x < 0) {
            new_combat_x = 0;
        }
        if (new_combat_x > MOX::combat_grid_width - COMBAT_GRID_VIEW_WIDTH) {
            new_combat_x = MOX::combat_grid_width - COMBAT_GRID_VIEW_WIDTH;
        }

        if (new_combat_y < 0) {
            new_combat_y = 0;
        }
        if (new_combat_y > MOX::combat_grid_height - COMBAT_GRID_VIEW_HEIGHT) {
            new_combat_y = MOX::combat_grid_height - COMBAT_GRID_VIEW_HEIGHT;
        }

        delta_x = MOX::cur_combat_x - new_combat_x;
        delta_y = MOX::cur_combat_y - new_combat_y;

        near_x = MOX::combat_near_x + (delta_x * -160);
        MOX::combat_near_y += (delta_y * -160) % 480;

        mid_x = MOX::combat_mid_x + (delta_x * -80);
        MOX::combat_mid_y += (delta_y * -80) % 480;

        far_x = MOX::combat_far_x + (delta_x * -20);
        MOX::combat_far_y += (delta_y * -20) % 480;

        // Normalize negative coordinates for wrapping
        if (near_x < 0) {
            near_x += ((0x27F - near_x) / 0x280) * 0x280;
        }
        if (MOX::combat_near_y < 0) {
            MOX::combat_near_y += ((0x1DF - MOX::combat_near_y) / 0x1E0) * 0x1E0;
        }

        if (mid_x < 0) {
            mid_x += ((0x27F - mid_x) / 0x280) * 0x280;
        }
        if (MOX::combat_mid_y < 0) {
            MOX::combat_mid_y += ((0x1DF - MOX::combat_mid_y) / 0x1E0) * 0x1E0;
        }

        if (far_x < 0) {
            far_x += ((0x27F - far_x) / 0x280) * 0x280;
        }
        if (MOX::combat_far_y < 0) {
            MOX::combat_far_y += ((0x1DF - MOX::combat_far_y) / 0x1E0) * 0x1E0;
        }

        // Apply wrapping within screen resolution (640x480)
        MOX::combat_near_x = near_x % 0x280;
        MOX::combat_near_y = MOX::combat_near_y % 0x1E0;
        MOX::combat_mid_x = mid_x % 0x280;
        MOX::combat_mid_y = MOX::combat_mid_y % 0x1E0;
        MOX::combat_far_x = far_x % 0x280;
        MOX::combat_far_y = MOX::combat_far_y % 0x1E0;

        MOX::cur_combat_x = new_combat_x;
        MOX::cur_combat_y = new_combat_y;

        COMBAT1::Add_Tactical_Combat_Fields();
        COMBAT1::Assign_Combat_Grids(MOX::cur_ship);
        CMBTDRW1::Refresh_Combat_Screen_Full();
    }

    void __cdecl Recharge_Shields(bool time_warp_only) {
        if (MOX::combat_total_ship_count <= 1) {
            return;
        }

        for (int16_t ship_idx = 1; ship_idx < MOX::combat_total_ship_count; ++ship_idx) {
            s_combat_data* ship = &MOX::combat_data[ship_idx];

            if (time_warp_only && Does_Combat_Ship_Have_Special(ship_idx, SPECIAL_TIME_WARP_FACILITATOR) == 0) {
                continue;
            }

            if (ship->shield_type > 0 &&
                CMBTMOV1::Ship_Is_In_Stasis(ship_idx) == 0 &&
                ship->shield_current_hits > 0) {
                uint32_t recharge_rate = shield_recharge_rate.standard;
                if (COMBINIT::Does_Combat_Ship_Have_Special(ship_idx, SPECIAL_SHIELD_CAPACITOR) != 0) {
                    recharge_rate = shield_recharge_rate.capacitor;
                }

                uint64_t recharge_pool =
                    ((uint64_t)ship->shield_arc_max * recharge_rate) / 10;

                if (recharge_pool > 0) {
                    bool changed = true;
                    while (recharge_pool > 0 && changed) {
                        changed = false;
                        for (int16_t arc = 0; arc < 4; ++arc) {
                            if (ship->shield_arc_current[arc] < ship->shield_arc_max) {
                                if (recharge_pool > 0) {
                                    recharge_pool--;
                                    changed = true;
                                    ship->shield_arc_current[arc]++;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    void __cdecl Increment_Bhg(int16_t player_idx, bool time_warp_only) {
        bool collapse[MAX_COMBAT_SHIPS] = {};
        uint8_t collapse_count[MAX_COMBAT_SHIPS] = {};
        uint16_t collapse_source[MAX_COMBAT_SHIPS] = {};

        if (MOX::combat_total_ship_count > 1) {
            for (int16_t i = 1; i < MOX::combat_total_ship_count; ++i) {
                s_combat_data& ship = MOX::combat_data[i];

                if (ship.unit_status == 0 &&
                    (MOX::combat_init != 0 || ship.owner == (int8_t)player_idx) &&
                    (!time_warp_only ||
                        MOX::combat_init == 0 ||
                        Does_Combat_Ship_Have_Special(
                            i, SPECIAL_TIME_WARP_FACILITATOR) != 0)) {
                    if (CMBTMOV1::Ship_Is_In_Stasis(i) == 0 && CMBTMOV1::Ship_In_Black_Hole(i)) {
                        ship.black_hole_flag++;

                        if (ship.black_hole_flag == 3) {
                            collapse[i] = true;
                            collapse_count[i] = ship.black_hole_flag;
                            collapse_source[i] = ship.black_hole_source_idx;
                        }
                    }
                }
            }

            for (int16_t i = 1; i < MOX::combat_total_ship_count; ++i) {
                if (collapse[i]) {
                    for (int16_t pending = 1;
                         pending < MOX::combat_total_ship_count;
                         ++pending) {
                        if (collapse[pending]) {
                            MOX::combat_data[pending].black_hole_flag =
                                collapse_count[pending];
                            MOX::combat_data[pending].black_hole_source_idx =
                                collapse_source[pending];
                        }
                    }

                    s_combat_data& ship = MOX::combat_data[i];
                    int16_t x_pos = (int16_t)ship.x * 20;
                    int16_t y_pos = (int16_t)ship.y * 20;
                    MOX::cur_ship = i;
                    CMBTFIRE::Destroy_Ship(i, x_pos, y_pos, 1);
                }
            }

            for (int16_t i = 1; i < MOX::combat_total_ship_count; ++i) {
                if (collapse[i]) {
                    MOX::combat_data[i].black_hole_flag = 0;
                    MOX::combat_data[i].black_hole_source_idx = COMBAT_SHIP_NONE;
                }
            }
        }
    }

    void __cdecl End_Of_Turn_Bookeeping() {
        COMBAT1::Update_Phasing_Cloak_Timers(false);
        Recharge_Shields(false);
        Repair_All_Combat_Ships(false);

        int16_t turn = MOX::combat_turn;
        if (MOX::combat_data->unit_status == 0 &&
            (int16_t)(turn % ground_defense.garrison_turns_to_replace) == 0) {
            for (int16_t i = 0; i < 8; ++i) {
                s_combat_weapon_slot* slot = &MOX::combat_data->weapons[i];
                uint16_t weapon_id = slot->weapon_id;

                if (TECHDATA::weapons[weapon_id].type == WEAPON_TYPE_FIGHTER) {
                    if (weapon_id == WEAPON_HEAVY_FIGHTER) {
                        slot->count = ground_defense.garrison_heavy_fighters;
                    } else if (weapon_id == WEAPON_BOMBER) {
                        slot->count = ground_defense.garrison_bombers;
                    } else if (weapon_id == WEAPON_INTERCEPTOR) {
                        slot->count = ground_defense.garrison_interceptors;
                    }
                    slot->ammo = 1;
                }
            }
        }
    }

    void __cdecl Scroll_Combat_Screen() {
        uint32_t scroll_timer = 0;
        int16_t next_combat_x;
        int16_t next_combat_y;
        int16_t scroll_speed;
        int16_t old_combat_x;
        int16_t old_combat_y;
        int16_t dir_x;
        int16_t dir_y;
        int16_t diff_x;
        int16_t diff_y;

        while (true) {
            if (mouse::Pointer_X() >= 5 &&
                mouse::Pointer_X() <= 0x27B /* 635 */ &&
                mouse::Pointer_Y() >= 5 &&
                mouse::Pointer_Y() <= 0x1DB /* 475 */) {
                COMBAT1::Assign_Combat_Grids(MOX::cur_ship);
                COMBAT1::Add_Tactical_Combat_Fields();
                CMBTDRW1::Refresh_Combat_Screen_Full();
                MOX::last_ship_status = -1;
                return;
            }

            old_combat_x = MOX::cur_combat_x;
            old_combat_y = MOX::cur_combat_y;
            next_combat_x = old_combat_x;
            next_combat_y = old_combat_y;

            if (scroll_timer != UINT32_MAX) {
                ++scroll_timer;
            }
            if (scroll_timer > 4) {
                scroll_speed = 3;
            } else if (scroll_timer > 2) {
                scroll_speed = 2;
            } else {
                scroll_speed = 1;
            }

            if (scroll_interval != 0) {
                const uint64_t current_time = SDL_GetTicks();
                const uint64_t interval =
                    (uint64_t)scroll_interval * kCombatScrollTickMilliseconds;
                if (current_time - previous_combat_scroll_time < interval) {
                    SDL_Delay(1);
                    continue;
                }
                previous_combat_scroll_time = current_time;
            }

            if (mouse::Pointer_X() < 5) {
                next_combat_x = old_combat_x - scroll_speed;
            } else if (mouse::Pointer_X() > 0x27B /* 635 */) {
                next_combat_x = old_combat_x + scroll_speed;
            }

            if (mouse::Pointer_Y() < 5) {
                next_combat_y = old_combat_y - scroll_speed;
            } else if (mouse::Pointer_Y() > 0x1DB /* 475 */) {
                next_combat_y = old_combat_y + scroll_speed;
            }

            if (next_combat_x < 0) {
                next_combat_x = 0;
            } else if (next_combat_x > MOX::combat_grid_width - COMBAT_GRID_VIEW_WIDTH) {
                next_combat_x = MOX::combat_grid_width - COMBAT_GRID_VIEW_WIDTH;
            }

            if (next_combat_y < 0) {
                next_combat_y = 0;
            } else if (next_combat_y > MOX::combat_grid_height - COMBAT_GRID_VIEW_HEIGHT) {
                next_combat_y = MOX::combat_grid_height - COMBAT_GRID_VIEW_HEIGHT;
            }

            diff_x = (int16_t)(old_combat_x - next_combat_x);
            if (diff_x < 0) {
                dir_x = 1;
            } else {
                dir_x = (diff_x > 0) ? -1 : 0;
            }

            diff_y = (int16_t)(old_combat_y - next_combat_y);
            if (diff_y < 0) {
                dir_y = 1;
            } else {
                dir_y = (diff_y > 0) ? -1 : 0;
            }

            if (next_combat_x != old_combat_x) {
                MOX::combat_near_x += dir_x * -0x50;                        // -80
                MOX::combat_mid_x += dir_x * -0x28;                         // -40
                MOX::combat_far_x += (int16_t)(((int32_t)dir_x * -20) / 2); // -10
            }

            if (next_combat_y != old_combat_y) {
                MOX::combat_near_y += dir_y * -0x50;                        // -80
                MOX::combat_mid_y += dir_y * -0x28;                         // -40
                MOX::combat_far_y += (int16_t)(((int32_t)dir_y * -20) / 2); // -10
            }

            if (MOX::combat_near_x < 0) MOX::combat_near_x += 0x280; // 640
            if (MOX::combat_near_y < 0) MOX::combat_near_y += 0x1E0; // 480
            if (MOX::combat_mid_x < 0) MOX::combat_mid_x += 0x280;
            if (MOX::combat_mid_y < 0) MOX::combat_mid_y += 0x1E0;
            if (MOX::combat_far_x < 0) MOX::combat_far_x += 0x280;
            if (MOX::combat_far_y < 0) MOX::combat_far_y += 0x1E0;

            MOX::combat_near_x %= 0x280;
            MOX::combat_near_y %= 0x1E0;
            MOX::combat_mid_x %= 0x280;
            MOX::combat_mid_y %= 0x1E0;
            MOX::combat_far_x %= 0x280;
            MOX::combat_far_y %= 0x1E0;

            MOX::cur_combat_x = next_combat_x;
            MOX::cur_combat_y = next_combat_y;

            video::Set_Page_Off();
            CMBTDRW1::Scroll_Draw_Combat_Screen();
            RUSS::Mox_Sync_Update();
        }
    }

    void __cdecl Load_Individual_Ship_Pictures(int16_t player_idx) {
        uint8_t attacker_palette[32];
        uint8_t defender_palette[32];
        s_animation_header* ship_cache[45];

        for (int i = 0; i < 32; ++i) {
            attacker_palette[i] = (uint8_t)i + 0x20;
            defender_palette[i] = (uint8_t)i + 0x60;
        }

        for (int i = 0; i < 45; ++i) {
            ship_cache[i] = nullptr;
        }

        if (player_idx < MAX_PLAYERS) {
            for (int i = 0; i < 64; ++i) {
                MOX::fighter_seg[player_idx][i] = (s_animation_header*)farload::Far_Reload_Next("cmbtfgtr.lbx", i, MOX::screen_seg);
                RUSS::Mox_Update();
            }

            if (MOX::combat_attacker_player == player_idx) {
                for (int i = 0; i < 64; ++i) {
                    replace::Replace_Color_Range(MOX::fighter_seg[player_idx][i], attacker_palette, 0x50, 0x10);
                    RUSS::Mox_Update();
                }
            } else {
                for (int i = 0; i < 64; ++i) {
                    replace::Replace_Color_Range(MOX::fighter_seg[player_idx][i], defender_palette, 0x50, 0x10);
                    RUSS::Mox_Update();
                }
            }
        } else if (player_idx == MAX_PLAYERS) {
            for (int i = 0; i < 16; ++i) {
                MOX::fighter_seg[MAX_PLAYERS][i] = (s_animation_header*)farload::Far_Reload_Next("cmbtfgtr.lbx", i + 0x40, MOX::screen_seg);
                RUSS::Mox_Update();
            }

            if (MOX::combat_attacker_player == MAX_PLAYERS) {
                for (int i = 0; i < 16; ++i) {
                    replace::Replace_Color_Range(MOX::fighter_seg[MAX_PLAYERS][i], attacker_palette, 0x50, 0x10);
                    RUSS::Mox_Update();
                }
            } else {
                for (int i = 0; i < 16; ++i) {
                    replace::Replace_Color_Range(MOX::fighter_seg[MAX_PLAYERS][i], defender_palette, 0x50, 0x10);
                    RUSS::Mox_Update();
                }
            }
        }

        for (int16_t i = 1; i < MOX::combat_total_ship_count; ++i) {
            s_combat_data* ship = &MOX::combat_data[i];
            if (ship->owner == player_idx) {
                char lbx_filename[16];
                int16_t lbx_index;

                if ((uint8_t)ship->previous_owner < MAX_PLAYERS && ship->picture_num != 44) {
                    strlcpy(lbx_filename, "cmbtshp.lbx", sizeof(lbx_filename));
                    const int16_t image_player_idx =
                        player_idx > MAX_PLAYERS ? ship->previous_owner : player_idx;
                    lbx_index = (int16_t)MOX::player[image_player_idx].color * 45 + ship->picture_num;
                } else {
                    strlcpy(lbx_filename, "monster.lbx", sizeof(lbx_filename));
                    lbx_index = (ship->picture_num == 44) ? 0x19 : ship->picture_num;
                }

                if (ship_cache[ship->picture_num] == nullptr) {
                    if (allocate::Get_Remaining_Block_Space(MOX::screen_seg) <= 200000 /* 0x30D40 */) {
                        ship->ship_image_seg = nullptr;
                    } else {
                        s_animation_header* anim = (s_animation_header*)farload::Far_Reload_Next(lbx_filename, (int)lbx_index, MOX::screen_seg);
                        ship_cache[ship->picture_num] = anim;
                        RUSS::Mox_Update();

                        if (MOX::combat_defender_player == player_idx) {
                            replace::Replace_Color_Range(anim, defender_palette, 0x20, 0x20);
                        }
                        RUSS::Mox_Update();
                        ship->ship_image_seg = anim;
                    }
                } else {
                    ship->ship_image_seg = ship_cache[ship->picture_num];
                }
            }
        }
    }

    void __cdecl Repair_All_Combat_Ships(bool time_warp_only) {
        int32_t attacker_bonus;
        int32_t defender_bonus;

        attacker_bonus = CMBTFIR2::Get_Fleet_Engineer_Bonus(MOX::combat_attacker_player);
        defender_bonus = CMBTFIR2::Get_Fleet_Engineer_Bonus(MOX::combat_defender_player);

        if (MOX::combat_attacker_player < MAX_PLAYERS) {
            if (MOX::player[MOX::combat_attacker_player].traits[TRAIT_CYBERNETIC] != 0) {
                attacker_bonus += repair_rate.cyber;
            }
        }

        if (MOX::combat_defender_player < MAX_PLAYERS) {
            if (MOX::player[MOX::combat_defender_player].traits[TRAIT_CYBERNETIC] != 0) {
                defender_bonus += repair_rate.cyber;
            }
        }

        for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
            int32_t repair_amount = 0;

            if (time_warp_only && Does_Combat_Ship_Have_Special(i, SPECIAL_TIME_WARP_FACILITATOR) == 0) {
                continue;
            }

            if (MOX::combat_data[i].unit_status == 0 &&
                MOX::combat_data[i].is_captured == 0 &&
                !CMBTMOV1::Ship_Is_In_Stasis(i)) {
                if (COMBINIT::Does_Combat_Ship_Have_Special(i, SPECIAL_AUTOMATED_REPAIR_UNIT) != 0) {
                    repair_amount = repair_rate.aru;
                }

                if (amoeba_regeneration != 0 &&
                    COMBINIT::Does_Combat_Ship_Have_Special(i, SPECIAL_REGENERATION) != 0) {
                    repair_amount += repair_rate.regen;
                }

                if (MOX::combat_data[i].owner == MOX::combat_attacker_player) {
                    repair_amount += attacker_bonus;
                } else {
                    repair_amount += defender_bonus;
                }

                if (repair_amount > 0) {
                    if (repair_amount > INT16_MAX) {
                        repair_amount = INT16_MAX;
                    }
                    CMBTFIR2::Repair_Combat_Ship(i, (int16_t)repair_amount);
                }
            }
        }
    }

    void __cdecl End_Of_Combat(int16_t star_idx, int16_t colony_pop_lost, int16_t combat_winner) {
        int16_t leader_count = 0;
        int16_t defender_losses = 0;
        int16_t attacker_losses = 0;
        int32_t defender_power_lost = 0;
        int32_t attacker_power_lost = 0;
        int16_t leaders[MAX_LEADERS];

        int16_t victor_player;
        if (combat_winner < 0) {
            victor_player = MOX::combat_defender_player;
        } else if (combat_winner == 666) {
            victor_player = -1;
        } else {
            victor_player = MOX::combat_attacker_player;
        }

        for (int16_t i = 1; i < MOX::combat_total_ship_count; ++i) {
            COMBINIT::Init_Special_Devices(i);
            s_combat_data* ship = &MOX::combat_data[i];

            if (Is_Combat_Satellite(ship) &&
                (ship->unit_status == 5 ||
                    victor_player == MOX::combat_attacker_player ||
                    ship->owner != MOX::combat_defender_player)) {
                if (star_idx != MOX::NUM_STARS) {
                    s_colony* col = &MOX::colony[MOX::cmbt_colony_id];
                    int16_t satellite_building = -1;
                    if (col->buildings[BUILDING_STAR_BASE] != 0) {
                        satellite_building = BUILDING_STAR_BASE;
                    } else if (col->buildings[BUILDING_BATTLESTATION] != 0) {
                        satellite_building = BUILDING_BATTLESTATION;
                    } else if (col->buildings[BUILDING_STAR_FORTRESS] != 0) {
                        satellite_building = BUILDING_STAR_FORTRESS;
                    }

                    if (satellite_building != -1) {
                        if (ship->unit_status != 5 &&
                            Satellite_Was_Captured_By_Attacker(
                                ship->owner, victor_player, MOX::combat_attacker_player)) {
                            Record_Captured_Satellite(MOX::cmbt_colony_id, satellite_building);
                        }
                        ERICNET::Remove_Building(MOX::cmbt_colony_id, satellite_building);
                    }
                }
                continue;
            }

            const bool discard_monster_capture =
                ship->owner >= MAX_PLAYERS &&
                ship->previous_owner != ship->owner &&
                crystal_keep == 0;

            if ((ship->unit_status == 5 && ship->is_retreating != 2) ||
                (ship->is_captured != 0 && ship->owner != victor_player) ||
                discard_monster_capture) {
                ship->unit_status = 5;

                if (MOX::combat_attacker_player == ship->owner) {
                    attacker_losses += (ship->size_class + 1);
                    attacker_power_lost += (1 << (ship->size_class + 1));
                } else {
                    defender_losses += (ship->size_class + 1);
                    defender_power_lost += (1 << (ship->size_class + 1));
                }

                if (ship->ship_idx != -1 && ship->officer_idx > -1) {
                    int16_t officer_idx = ship->officer_idx;
                    if (MOX::leaders[officer_idx].eta > 0) {
                        OFFICER::Move_Officer_To_Pool(officer_idx);
                    } else {
                        if (MOX::player[ship->owner].tech_applications[TECH_APP_SURVIVAL_PODS] == TECH_RESEARCH_STATUS_RESEARCHED) {
                            leaders[leader_count] = officer_idx;
                            leader_count++;
                            MOX::ship[ship->ship_idx].officer_index = -1;
                        } else {
                            OFFICER::Kill_Officer(officer_idx);
                        }
                    }
                }
            } else {
                if (COMBINIT::Does_Combat_Ship_Have_Special(i, SPECIAL_AUTOMATED_REPAIR_UNIT) == 1 ||
                    CMBTFIR2::Get_Fleet_Engineer_Bonus(ship->owner) > 0 ||
                    (ship->owner < MAX_PLAYERS && (MOX::player[ship->owner].tech_applications[TECH_APP_ADVANCED_DAMAGE_CONTROL] == TECH_RESEARCH_STATUS_RESEARCHED || MOX::player[ship->owner].traits[TRAIT_CYBERNETIC] != 0))) {
                    CMBTFIR2::Clear_Internal_Space(ship);
                    for (int16_t j = 0; j < 5; ++j) {
                        ship->special_device_damage_flags[j] = 0;
                    }
                    ship->structure_damage = 0;
                    if (ship->ship_idx != -1) {
                        ship->armor_remaining = INITSHIP::Get_Ship_Armor_Hits(ship->ship_idx);
                    }
                }

                const bool retained_crystal_capture =
                    crystal_keep != 0 &&
                    ship->owner == NONPLAYER_SHIP_TYPE_CRYSTAL &&
                    ship->previous_owner != ship->owner &&
                    ship->owner == victor_player &&
                    ship->ship_idx != -1;
                if (retained_crystal_capture) {
                    MOX::ship[ship->ship_idx].d.speed = 0;
                }

                COMBINIT::Unload_Combat_Ship(i);
            }
        }

        if (MOX::combat_attacker_player < MAX_PLAYERS) {
            if (MOX::bio_weapon_flag != 0) {
                COMBINIT::Apply_Bio_Weapon_Penalty(MOX::combat_attacker_player, star_idx, colony_pop_lost);
            }
            if (MOX::combat_defender_player < MAX_PLAYERS) {
                MOX::player[MOX::combat_attacker_player].ship_war_losses[MOX::combat_defender_player] += (int16_t)attacker_power_lost;
                MOX::player[MOX::combat_defender_player].ship_war_losses[MOX::combat_attacker_player] += (int16_t)defender_power_lost;

                if (defender_losses <= 0) {
                    defender_losses = 1;
                }

                if (COMBINIT::real_attack_flag != 0) {
                    if (colony_pop_lost > 0) {
                        NETMOX::Russ_Change_Relations(-10000, MOX::combat_attacker_player, MOX::combat_defender_player, 10, star_idx, colony_pop_lost);
                    }
                    NETMOX::Russ_Change_Relations(-15 * defender_losses, MOX::combat_attacker_player, MOX::combat_defender_player, 11, star_idx, 0);
                }
            }
        }

        for (int16_t i = 1; i < MOX::combat_total_ship_count; ++i) {
            s_combat_data* ship = &MOX::combat_data[i];
            if ((ship->unit_status == 0 || ship->is_retreating == 2) &&
                ship->officer_idx != -1 &&
                ship->owner == victor_player &&
                ship->ship_idx != -1) {
                int16_t xp = (victor_player == MOX::combat_attacker_player) ? defender_losses : attacker_losses;
                xp /= 2;
                if (xp < 1) {
                    xp = 1;
                }
                OFFICER::Give_Officer_XP(victor_player, ship->officer_idx, xp);
            }
        }

        if (leader_count > 0) {
            for (int16_t j = 0; j < leader_count; ++j) {
                int16_t ship_idx_found = 0;
                int16_t ship_assigned = 0;
                int16_t leader_id = leaders[j];

                for (int16_t i = 1; i < MOX::combat_total_ship_count; ++i) {
                    if (ship_idx_found != 0) {
                        break;
                    }
                    s_combat_data* ship = &MOX::combat_data[i];
                    if ((ship->unit_status != 5 || ship->is_retreating == 2) && ship->ship_idx != -1) {
                        if (MOX::leaders[leader_id].player_index == ship->owner) {
                            if (MOX::ship[ship->ship_idx].officer_index == -1) {
                                ship_idx_found = 1;
                                MOX::ship[ship->ship_idx].officer_index = leader_id;
                                ship->officer_idx = leader_id;
                                MOX::ship[MOX::leaders[leader_id].location].officer_index = -1;
                                MOX::leaders[leader_id].location = ship->ship_idx;
                            }
                            ship_assigned = 1;
                        }
                    }
                }

                if (ship_idx_found != 0 || ship_assigned != 0) {
                    if (MOX::leaders[leader_id].player_index == victor_player) {
                        int16_t xp = (victor_player == MOX::combat_attacker_player) ? defender_losses : attacker_losses;
                        xp /= 2;
                        if (xp < 1) {
                            xp = 1;
                        }
                        OFFICER::Give_Officer_XP(victor_player, leader_id, xp);
                    }
                }

                if (ship_idx_found == 0) {
                    if (ship_assigned == 1) {
                        OFFICER::Move_Officer_To_Pool(leader_id);
                    } else {
                        OFFICER::Kill_Officer(leader_id);
                    }
                }
            }
        }

        for (int16_t i = 1; i < MOX::combat_total_ship_count; ++i) {
            s_combat_data* ship = &MOX::combat_data[i];

            if (star_idx == MOX::NUM_STARS &&
                Is_Generated_Antaran_Combat_Ship(ship) &&
                ship->unit_status == 5) {
                antarans::Kill_Antaran_Defensive_Ship(ship->size_class);
            } else if (ship->unit_status == 5 && ship->ship_idx != -1 && ship->is_retreating != 2) {
                if (ship->previous_owner == MAX_PLAYERS) {
                    antarans::Kill_Antaran_Offensive_Ship(ship->size_class);
                }
                if (MOX::ship[ship->ship_idx].officer_index > -1) {
                    OFFICER::Move_Officer_To_Pool(MOX::ship[ship->ship_idx].officer_index);
                }
                SHIPS::Kill_Ship(ship->ship_idx);
            } else if (ship->owner == victor_player && ship->ship_idx != -1) {
                int16_t xp = (victor_player == MOX::combat_attacker_player) ? defender_losses : attacker_losses;
                xp /= 2;
                if (xp < 1) {
                    xp = 1;
                }
                MOX::ship[ship->ship_idx].crew_experience += xp;
            }
        }

        if (MOX::cmbt_colony_id != -1) {
            s_colony* col = &MOX::colony[MOX::cmbt_colony_id];
            if (col->outpost_flag == 0 && col->n_pops == 0) {
                int16_t has_buildings = 0;
                for (int16_t i = 0; i < BUILDING_COUNT; ++i) {
                    if (has_buildings != 0) {
                        break;
                    }
                    has_buildings += col->buildings[i];
                }
                if (has_buildings == 0) {
                    BOMB::Destroy_Colony(col, 1);
                }
            }
        }
    }

    void __cdecl Unload_Combat_Ship(int16_t combat_ship_idx) {
        int16_t ship_idx = MOX::combat_data[combat_ship_idx].ship_idx;

        if (ship_idx != -1) {
            if (MOX::ship[ship_idx].d.combat_speed == 0) {
                MOX::combat_data[combat_ship_idx].drive_current_hits = MOX::combat_data[combat_ship_idx].drive_max_hits;
            } else {
                while (MOX::combat_data[combat_ship_idx].current_speed < 1) {
                    MOX::combat_data[combat_ship_idx].drive_current_hits++;
                    MOX::combat_data[combat_ship_idx].current_speed = COMBAT1::Calc_Current_Speed(combat_ship_idx);
                }
            }

            if (MOX::combat_data[combat_ship_idx].drive_max_hits < MOX::combat_data[combat_ship_idx].drive_current_hits) {
                MOX::combat_data[combat_ship_idx].drive_current_hits = MOX::combat_data[combat_ship_idx].drive_max_hits;
            }

            MOX::ship[ship_idx].owner = MOX::combat_data[combat_ship_idx].owner;

            for (int16_t i = 0; i < 8; i++) {
                MOX::ship[ship_idx].d.ship_weapon[i].current_count = MOX::combat_data[combat_ship_idx].weapons[i].count;
            }

            for (int16_t i = 0; i < 5; i++) {
                MOX::ship[ship_idx].special_device_damage_flags[i] = MOX::combat_data[combat_ship_idx].special_device_damage_flags[i];
            }

            MOX::ship[ship_idx].structural_damage = MOX::combat_data[combat_ship_idx].structure_damage;
            MOX::ship[ship_idx].armor_damage = INITSHIP::Get_Ship_Armor_Hits(ship_idx) - MOX::combat_data[combat_ship_idx].armor_remaining;
            MOX::ship[ship_idx].computer_damage =
                MOX::combat_data[combat_ship_idx].computer_max_hits -
                MOX::combat_data[combat_ship_idx].computer_current_hits;

            if (MOX::combat_data[combat_ship_idx].shield_max_hits > 0) {
                MOX::ship[ship_idx].shield_damage_percent =
                    (int8_t)Damage_Percent_From_Hits(
                        MOX::combat_data[combat_ship_idx].shield_max_hits,
                        MOX::combat_data[combat_ship_idx].shield_current_hits);
            }

            if (MOX::combat_data[combat_ship_idx].drive_max_hits > 0) {
                MOX::ship[ship_idx].drive_damage_percent =
                    (int8_t)Damage_Percent_From_Hits(
                        MOX::combat_data[combat_ship_idx].drive_max_hits,
                        MOX::combat_data[combat_ship_idx].drive_current_hits);
            }

            for (int16_t i = 0; i < 8; ++i) {
                int16_t weapon_id = MOX::combat_data[combat_ship_idx].weapons[i].weapon_id;
                if (TECHDATA::weapons[weapon_id].type == WEAPON_TYPE_FIGHTER) {
                    MOX::ship[ship_idx].d.ship_weapon[i].current_count =
                        Surviving_Weapon_Count_From_Hits(
                            MOX::combat_data[combat_ship_idx].weapon_hits[i],
                            MOX::combat_data[combat_ship_idx].weapon_hits_per_unit[i]);
                }
            }
        }
    }

    void __cdecl Plasma_Web_Damage(int16_t owner_idx, bool time_warp_only) {
        if (MOX::combat_total_ship_count <= 0) {
            return;
        }

        for (int16_t i = 0; i < MOX::combat_total_ship_count; ++i) {
            s_combat_data* ship = &MOX::combat_data[i];

            if (ship->unit_status != 0) {
                continue;
            }

            if (ship->plasma_web_damage <= 0) {
                continue;
            }

            if (owner_idx >= 0 && ship->owner != owner_idx) {
                continue;
            }

            if (time_warp_only && Does_Combat_Ship_Have_Special(i, SPECIAL_TIME_WARP_FACILITATOR) == 0) {
                continue;
            }

            if (CMBTMOV1::Ship_Is_In_Stasis(i) != 0) {
                continue;
            }

            int16_t total_damage = 0;
            int16_t struct_damage_total = 0;

            for (int16_t arc = 0; arc < 4; ++arc) {
                int16_t arc_damage = ship->plasma_web_damage;
                int16_t arc_struct_damage = 0;

                CMBTFIRE::Apply_Ship_Damage(
                    i,
                    &arc_damage,
                    &arc_struct_damage,
                    arc,
                    0,
                    0,
                    0,
                    0,
                    0);

                total_damage += arc_damage;
                struct_damage_total += arc_struct_damage;
            }

            CMBTFIRE::Record_Unit_Hit(i, total_damage, struct_damage_total);
            if (MOX::apply_damage_to_planet == 1 && MOX::super_fast_flag == 0) {
                MOX::cur_ship = i;
                if (CMBTDRW1::Ship_Completely_On_Screen(i) == 0) {
                    COMBINIT::Snap_Center_Combat_Screen(ship->x, ship->y);
                }

                BEAMS::Add_Damage_Indicator_For_Ship(
                    (ship->x - MOX::cur_combat_x) * 20,
                    (ship->y - MOX::cur_combat_y) * 20,
                    i,
                    1,
                    total_damage,
                    struct_damage_total);

                BEAMS::Draw_Damage_Message_Queue_Until_Done();
            }

            int16_t cause = CMBTFIRE::Is_Combat_Ship_Dead(i);
            if (cause != 0) {
                CMBTFIRE::Destroy_Ship(i, ship->x * 20, ship->y * 20, cause);
            } else {
                if (plasma_web_dissipation == PLASMA_WEB_DISSIPATION_HALVE) {
                    ship->plasma_web_damage /= 2;
                } else {
                    ship->plasma_web_damage -= 5;
                    if (ship->plasma_web_damage < 0) {
                        ship->plasma_web_damage = 0;
                    }
                }
            }
        }
    }

    void __cdecl Apply_Bio_Weapon_Penalty(int16_t player_idx, int16_t star_idx, int16_t colony_pop_lost) {
        if (player_idx < MAX_PLAYERS) {
            for (int16_t i = 0; i < MOX::NUM_PLAYERS; ++i) {
                if (MOX::player[i].objectives != PLAYER_OBJECTIVE_HUMAN && i != player_idx) {
                    int16_t change = -(int16_t)game_random::Random(10);
                    NETMOX::Russ_Change_Relations(change, player_idx, i, 6, star_idx, colony_pop_lost);
                }
            }
        }
    }

    void __cdecl Ship_Center_Offsets(int16_t ship_idx, int16_t* out_x, int16_t* out_y) {
        int16_t result_offset = 0;

        if (ship_idx == 0) {
            if (MOX::cmbt_colony_id == -1) {
                result_offset = 60;
            } else {
                int16_t planet_idx = MOX::colony[MOX::cmbt_colony_id].planet;
                int8_t planet_size = MOX::planet[planet_idx].size;

                switch (planet_size) {
                    case PLANET_SIZE_TINY:
                        result_offset = 30;
                        break;
                    case PLANET_SIZE_SMALL:
                        result_offset = 40;
                        break;
                    case PLANET_SIZE_MEDIUM:
                        result_offset = 60;
                        break;
                    case PLANET_SIZE_LARGE:
                        result_offset = 70;
                        break;
                    case PLANET_SIZE_HUGE:
                        result_offset = 80;
                        break;
                    default:
                        result_offset = 0;
                        break;
                }
            }
        } else {
            int8_t size_class = MOX::combat_data[ship_idx].size_class;

            switch (size_class) {
                case SHIP_SIZE_SMALL:
                    result_offset = 10;
                    break;
                case SHIP_SIZE_MEDIUM:
                case SHIP_SIZE_LARGE:
                    result_offset = 20;
                    break;
                case SHIP_SIZE_HUGE:
                case SHIP_SIZE_TITAN:
                case SHIP_SIZE_DOOM_STAR:
                    result_offset = 30;
                    break;
                default:
                    result_offset = 0;
                    break;
            }
        }

        *out_x = result_offset;
        *out_y = result_offset;
    }

} // namespace COMBINIT