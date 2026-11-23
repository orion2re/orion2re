#include "pch.h"
#include "player_roster.h"

using game::current_state;

namespace {
    COMBINIT::s_satellite_capture_state satellite_capture_state;

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
} // namespace

namespace COMBINIT {
    uint8_t real_attack_flag;

    void Reactivate_Combat_Missiles_For_Pass(bool time_warp_only) {
        if (!time_warp_only) {
            Reactivate_Combat_Missiles();
        }
    }

    void __cdecl Reset_Satellite_Capture() {
        Reset_Satellite_Capture_State(&satellite_capture_state);
    }

    void __cdecl Record_Captured_Satellite(game::id_colony colony_idx, int16_t building_idx) {
        Record_Captured_Satellite_State(&satellite_capture_state, colony_idx, building_idx);
    }

    int16_t __cdecl Take_Captured_Satellite(game::id_colony colony_idx) {
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

    void Load_Combat_Ship(const s_ship_data& backing, game::id_ship key, int16_t combat_idx) {
        const auto* ship = &backing;
        s_combat_data* combat = &MOX::combat_data[combat_idx];

        strlcpy(combat->name, ship->d.name, sizeof(combat->name));
        combat->owner = factions::From_Legacy(factions::Legacy_Code(ship->owner));
        combat->previous_owner = factions::From_Legacy(factions::Legacy_Code(ship->d.previous_owner.get()));
        combat->ship_id = key;
        combat->shield_type = ship->d.shield_type;
        combat->computer_type = ship->d.computer_type;

        if (factions::Legacy_Code(combat->owner) < MAX_PLAYERS) {
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
        if (officer_idx < 0 || current_state.leaders.at(officer_idx).eta != 0) {
            combat->officer_idx = -1;
        } else {
            combat->officer_idx = officer_idx;
        }

        if (ship->d.ship_type == SHIP_TYPE_COMBAT && factions::Legacy_Code(ship->owner) <= MAX_PLAYERS) {
            combat->marine_count = (uint8_t)TECHDATA::hull_data[combat->size_class].marines;
            if (struct_::Test_Bit_Field(combat->special_device_flags, SPECIAL_TROOP_PODS) == 1 &&
                struct_::Test_Bit_Field(combat->special_device_damage_flags, SPECIAL_TROOP_PODS) == 0) {
                combat->marine_count *= 2;
            }
        } else {
            combat->marine_count = -1;
        }

        combat->structure_max = INITSHIP::Get_Ship_Structure(*ship, MOX::settings.strategic_combat_flag != 0);
        combat->armor_remaining = INITSHIP::Get_Ship_Armor_Hits(*ship) - ship->armor_damage;
        combat->armor_type = ship->d.armor_type;
        combat->structure_damage = ship->structural_damage;

        if (combat->structure_damage > combat->structure_max) {
            combat->structure_damage = combat->structure_max - 1;
        }

        if (combat->size_class == SHIP_SIZE_DOOM_STAR &&
            factions::Legacy_Code(combat->previous_owner) < MAX_PLAYERS) {
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

        if (factions::Legacy_Code(combat->owner) > MAX_PLAYERS && combat->ftl_type == DRIVE_NO_DRIVE) {
            switch (factions::Legacy_Code(combat->owner)) {
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
                    (MOX::combat_init != 0 || factions::Legacy_Code(ship.owner) == (int8_t)player_idx) &&
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

    void __cdecl Repair_All_Combat_Ships(bool time_warp_only) {
        int32_t attacker_bonus;
        int32_t defender_bonus;

        attacker_bonus = CMBTFIR2::Get_Fleet_Engineer_Bonus(MOX::attacking_faction.code);
        defender_bonus = CMBTFIR2::Get_Fleet_Engineer_Bonus(MOX::defending_faction.code);

        if (const auto* attacker = player_roster::Find(current_state, factions::Player(MOX::attacking_faction));
            attacker && attacker->traits[TRAIT_CYBERNETIC] != 0) {
            attacker_bonus += repair_rate.cyber;
        }

        if (const auto* defender = player_roster::Find(current_state, factions::Player(MOX::defending_faction));
            defender && defender->traits[TRAIT_CYBERNETIC] != 0) {
            defender_bonus += repair_rate.cyber;
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

                if (factions::Legacy_Code(MOX::combat_data[i].owner) == MOX::attacking_faction.code) {
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
            victor_player = MOX::defending_faction.code;
        } else if (combat_winner == 666) {
            victor_player = -1;
        } else {
            victor_player = MOX::attacking_faction.code;
        }

        for (int16_t i = 1; i < MOX::combat_total_ship_count; ++i) {
            COMBINIT::Init_Special_Devices(i);
            s_combat_data* ship = &MOX::combat_data[i];

            if (Is_Combat_Satellite(ship) &&
                (ship->unit_status == 5 ||
                    victor_player == MOX::attacking_faction.code ||
                    factions::Legacy_Code(ship->owner) != MOX::defending_faction.code)) {
                if (star_idx != game::ANTARES_POSITION) {
                    s_colony* col = &game::current_state.colonies.at(MOX::cmbt_colony_id);
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
                                factions::Legacy_Code(ship->owner), victor_player, MOX::attacking_faction.code)) {
                            Record_Captured_Satellite(MOX::cmbt_colony_id, satellite_building);
                        }
                        ERICNET::Remove_Building(game::current_state, MOX::cmbt_colony_id, satellite_building);
                    }
                }
                continue;
            }

            const bool discard_monster_capture =
                factions::Legacy_Code(ship->owner) >= MAX_PLAYERS &&
                factions::Legacy_Code(ship->previous_owner) != factions::Legacy_Code(ship->owner) &&
                crystal_keep == 0;
            auto* strategic = game::Find_Ship(current_state, ship->ship_id);

            if ((ship->unit_status == 5 && ship->is_retreating != 2) ||
                (ship->is_captured != 0 && factions::Legacy_Code(ship->owner) != victor_player) ||
                discard_monster_capture) {
                ship->unit_status = 5;

                if (MOX::attacking_faction.code == factions::Legacy_Code(ship->owner)) {
                    attacker_losses += (ship->size_class + 1);
                    attacker_power_lost += (1 << (ship->size_class + 1));
                } else {
                    defender_losses += (ship->size_class + 1);
                    defender_power_lost += (1 << (ship->size_class + 1));
                }

                if (strategic && ship->officer_idx > -1) {
                    int16_t officer_idx = ship->officer_idx;
                    if (current_state.leaders.at(officer_idx).eta > 0) {
                        OFFICER::Move_Officer_To_Pool(officer_idx);
                    } else {
                        if (game::current_state.players.at(factions::Player(ship->owner).id).tech_applications[TECH_APP_SURVIVAL_PODS] == TECH_RESEARCH_STATUS_RESEARCHED) {
                            leaders[leader_count] = officer_idx;
                            leader_count++;
                            strategic->officer_index = -1;
                        } else {
                            OFFICER::Kill_Officer(officer_idx);
                        }
                    }
                }
            } else {
                if (COMBINIT::Does_Combat_Ship_Have_Special(i, SPECIAL_AUTOMATED_REPAIR_UNIT) == 1 ||
                    CMBTFIR2::Get_Fleet_Engineer_Bonus(factions::Legacy_Code(ship->owner)) > 0 ||
                    (factions::Legacy_Code(ship->owner) < MAX_PLAYERS && (game::current_state.players.at(factions::Player(ship->owner).id).tech_applications[TECH_APP_ADVANCED_DAMAGE_CONTROL] == TECH_RESEARCH_STATUS_RESEARCHED || game::current_state.players.at(factions::Player(ship->owner).id).traits[TRAIT_CYBERNETIC] != 0))) {
                    CMBTFIR2::Clear_Internal_Space(ship);
                    for (int16_t j = 0; j < 5; ++j) {
                        ship->special_device_damage_flags[j] = 0;
                    }
                    ship->structure_damage = 0;
                    if (strategic) {
                        ship->armor_remaining = INITSHIP::Get_Ship_Armor_Hits(*strategic);
                    }
                }

                const bool retained_crystal_capture =
                    crystal_keep != 0 &&
                    factions::Legacy_Code(ship->owner) == NONPLAYER_SHIP_TYPE_CRYSTAL &&
                    factions::Legacy_Code(ship->previous_owner) != factions::Legacy_Code(ship->owner) &&
                    factions::Legacy_Code(ship->owner) == victor_player &&
                    strategic;
                if (retained_crystal_capture) {
                    strategic->d.speed = 0;
                }

                COMBINIT::Unload_Combat_Ship(i);
            }
        }

        const auto attacking_player = factions::Player(MOX::attacking_faction);
        const auto defending_player = factions::Player(MOX::defending_faction);
        if (auto* attacker = player_roster::Find(current_state, attacking_player)) {
            if (MOX::bio_weapon_flag != 0) {
                COMBINIT::Apply_Bio_Weapon_Penalty(MOX::attacking_faction.code, star_idx, colony_pop_lost);
            }
            if (auto* defender = player_roster::Find(current_state, defending_player)) {
                attacker->ship_war_losses[defending_player.id] += (int16_t)attacker_power_lost;
                defender->ship_war_losses[attacking_player.id] += (int16_t)defender_power_lost;

                if (defender_losses <= 0) {
                    defender_losses = 1;
                }

                if (COMBINIT::real_attack_flag != 0) {
                    if (colony_pop_lost > 0) {
                        NETMOX::Russ_Change_Relations(-10000, MOX::attacking_faction.code, MOX::defending_faction.code, 10, star_idx, colony_pop_lost);
                    }
                    NETMOX::Russ_Change_Relations(-15 * defender_losses, MOX::attacking_faction.code, MOX::defending_faction.code, 11, star_idx, 0);
                }
            }
        }

        for (int16_t i = 1; i < MOX::combat_total_ship_count; ++i) {
            s_combat_data* ship = &MOX::combat_data[i];
            if ((ship->unit_status == 0 || ship->is_retreating == 2) &&
                ship->officer_idx != -1 &&
                factions::Legacy_Code(ship->owner) == victor_player &&
                game::Find_Ship(current_state, ship->ship_id)) {
                int16_t xp = (victor_player == MOX::attacking_faction.code) ? defender_losses : attacker_losses;
                xp /= 2;
                if (xp < 1) {
                    xp = 1;
                }
                OFFICER::Give_Officer_XP(legacy_world::Decode_Player_Slot(victor_player), ship->officer_idx, xp);
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
                    if (ship->unit_status == 5 && ship->is_retreating != 2) continue;
                    auto* strategic = game::Find_Ship(current_state, ship->ship_id);
                    auto& leader = current_state.leaders.at(leader_id);
                    if (!strategic || leader.employer != legacy_world::Decode_Player_Slot(factions::Legacy_Code(ship->owner))) continue;
                    if (strategic->officer_index == -1) {
                        ship_idx_found = 1;
                        if (auto* original = game::Find_Leader_Ship(current_state, leader)) original->officer_index = -1;
                        strategic->officer_index = leader_id;
                        ship->officer_idx = leader_id;
                        game::Set_Leader_Ship(leader, *strategic, -1);
                    }
                    ship_assigned = 1;
                }

                if (ship_idx_found != 0 || ship_assigned != 0) {
                    if (current_state.leaders.at(leader_id).employer == legacy_world::Decode_Player_Slot(victor_player)) {
                        int16_t xp = (victor_player == MOX::attacking_faction.code) ? defender_losses : attacker_losses;
                        xp /= 2;
                        if (xp < 1) {
                            xp = 1;
                        }
                        OFFICER::Give_Officer_XP(legacy_world::Decode_Player_Slot(victor_player), leader_id, xp);
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
            auto* strategic = game::Find_Ship(current_state, ship->ship_id);

            if (star_idx == game::ANTARES_POSITION &&
                Is_Generated_Antaran_Combat_Ship(ship) &&
                ship->unit_status == 5) {
                antarans::Kill_Antaran_Defensive_Ship(ship->size_class);
            } else if (ship->unit_status == 5 && strategic && ship->is_retreating != 2) {
                if (factions::Legacy_Code(ship->previous_owner) == MAX_PLAYERS) {
                    antarans::Kill_Antaran_Offensive_Ship(ship->size_class);
                }
                if (strategic->officer_index > -1) {
                    OFFICER::Move_Officer_To_Pool(strategic->officer_index);
                }
                SHIPS::Kill_Ship(*strategic);
            } else if (factions::Legacy_Code(ship->owner) == victor_player && strategic) {
                int16_t xp = (victor_player == MOX::attacking_faction.code) ? defender_losses : attacker_losses;
                xp /= 2;
                if (xp < 1) {
                    xp = 1;
                }
                strategic->crew_experience += xp;
            }
        }

        if (MOX::cmbt_colony_id != game::NO_COLONY) {
            s_colony* col = &game::current_state.colonies.at(MOX::cmbt_colony_id);
            if (col->outpost_flag == 0 && col->n_pops == 0) {
                int16_t has_buildings = 0;
                for (int16_t i = 0; i < BUILDING_COUNT; ++i) {
                    if (has_buildings != 0) {
                        break;
                    }
                    has_buildings += col->buildings[i];
                }
                if (has_buildings == 0) {
                    BOMB::Destroy_Colony(game::current_state, MOX::cmbt_colony_id, 1);
                }
            }
        }
    }

    void __cdecl Unload_Combat_Ship(int16_t combat_ship_idx) {
        auto& unit = MOX::combat_data[combat_ship_idx];
        auto* strategic = game::Find_Ship(current_state, unit.ship_id);
        if (!strategic) return;
        auto& ship = *strategic;

        if (ship.d.combat_speed == 0) {
            unit.drive_current_hits = unit.drive_max_hits;
        } else {
            while (unit.current_speed < 1) {
                unit.drive_current_hits++;
                unit.current_speed = COMBAT1::Calc_Current_Speed(combat_ship_idx);
            }
        }

        if (unit.drive_max_hits < unit.drive_current_hits) unit.drive_current_hits = unit.drive_max_hits;

        ship.owner = factions::From_Legacy(factions::Legacy_Code(unit.owner));
        for (int16_t i = 0; i < 8; ++i) {
            ship.d.ship_weapon[i].current_count = unit.weapons[i].count;
        }
        for (int16_t i = 0; i < 5; ++i) {
            ship.special_device_damage_flags[i] = unit.special_device_damage_flags[i];
        }

        ship.structural_damage = unit.structure_damage;
        ship.armor_damage = INITSHIP::Get_Ship_Armor_Hits(ship) - unit.armor_remaining;
        ship.computer_damage = unit.computer_max_hits - unit.computer_current_hits;

        if (unit.shield_max_hits > 0) {
            ship.shield_damage_percent = static_cast<int8_t>(Damage_Percent_From_Hits(unit.shield_max_hits, unit.shield_current_hits));
        }
        if (unit.drive_max_hits > 0) {
            ship.drive_damage_percent = static_cast<int8_t>(Damage_Percent_From_Hits(unit.drive_max_hits, unit.drive_current_hits));
        }

        for (int16_t i = 0; i < 8; ++i) {
            if (TECHDATA::weapons[unit.weapons[i].weapon_id].type == WEAPON_TYPE_FIGHTER) {
                ship.d.ship_weapon[i].current_count = Surviving_Weapon_Count_From_Hits(unit.weapon_hits[i], unit.weapon_hits_per_unit[i]);
            }
        }
    }

    void __cdecl Apply_Bio_Weapon_Penalty(int16_t player_idx, int16_t star_idx, int16_t colony_pop_lost) {
        if (player_idx < MAX_PLAYERS) {
            for (int16_t i = 0; i < game::current_state.player_count; ++i) {
                if (game::current_state.players[i].objectives != PLAYER_OBJECTIVE_HUMAN && i != player_idx) {
                    int16_t change = -(int16_t)game_random::Random(10);
                    NETMOX::Russ_Change_Relations(change, player_idx, i, 6, star_idx, colony_pop_lost);
                }
            }
        }
    }
} // namespace COMBINIT
