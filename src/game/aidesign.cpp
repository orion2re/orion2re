#include "pch.h"
#include "legacy_world.h"

namespace aidesign {
    static constexpr s_ship_design_config DEFAULT_PRIORITIES = {
        .special_priority = {
            .bio_weapon = {
                {SPECIAL_PHASE_SHIFTER, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_SUB_SPACE_TELEPORTER, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_AUGMENTED_ENGINES, AI_SPECIAL_ITEM_SPECIAL},
                {WEAPON_BIO_TERMINATOR, AI_SPECIAL_ITEM_WEAPON},
                {WEAPON_DEATH_SPORE, AI_SPECIAL_ITEM_WEAPON},
            },
            .capture = {
                {SPECIAL_TRANSPORTERS, AI_SPECIAL_ITEM_SPECIAL},
                {WEAPON_TRACTOR_BEAM, AI_SPECIAL_ITEM_WEAPON},
                {WEAPON_ASSAULT_SHUTTLE, AI_SPECIAL_ITEM_WEAPON},
                {SPECIAL_TROOP_PODS, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_WARP_DISSIPATOR, AI_SPECIAL_ITEM_SPECIAL},
            },
            .cloaking = {
                {SPECIAL_PHASING_CLOAK, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_CLOAKING_DEVICE, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_STEALTH_FIELD, AI_SPECIAL_ITEM_SPECIAL},
            },
            .beam_defense = {
                {SPECIAL_INERTIAL_NULLIFIER, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_DISPLACEMENT_DEVICE, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_AUGMENTED_ENGINES, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_INERTIAL_STABILIZER, AI_SPECIAL_ITEM_SPECIAL},
            },
            .missile_defense = {
                {SPECIAL_WIDE_AREA_JAMMER, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_MULTI_WAVE_ECM_JAMMER, AI_SPECIAL_ITEM_SPECIAL},
                {WEAPON_SPATIAL_COMPRESSOR, AI_SPECIAL_ITEM_WEAPON},
                {SPECIAL_ECM_JAMMER, AI_SPECIAL_ITEM_SPECIAL},
                {WEAPON_PULSAR, AI_SPECIAL_ITEM_WEAPON},
                {SPECIAL_DISPLACEMENT_DEVICE, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_LIGHTNING_FIELD, AI_SPECIAL_ITEM_SPECIAL},
                {WEAPON_ANTI_MISSILE_ROCKET, AI_SPECIAL_ITEM_WEAPON},
            },
            .armor = {
                {SPECIAL_AUTOMATED_REPAIR_UNIT, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_HEAVY_ARMOR, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_REINFORCED_HULL, AI_SPECIAL_ITEM_SPECIAL},
            },
            .shield = {
                {SPECIAL_HARD_SHIELDS, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_MULTI_PHASED_SHIELDS, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_SHIELD_CAPACITOR, AI_SPECIAL_ITEM_SPECIAL},
            },
            .beam_offense = {
                {SPECIAL_TIME_WARP_FACILITATOR, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_ACHILLES_TARGETING_UNIT, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_HIGH_ENERGY_FOCUS, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_RANGEMASTER_UNIT, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_STRUCTURAL_ANALYZER, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_HYPER_X_CAPACITORS, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_BATTLE_SCANNER, AI_SPECIAL_ITEM_SPECIAL},
            },
            .missile_offense = {
                {SPECIAL_TIME_WARP_FACILITATOR, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_FAST_MISSILE_RACKS, AI_SPECIAL_ITEM_SPECIAL},
            },
            .special_weapon = {
                {WEAPON_STELLAR_CONVERTER, AI_SPECIAL_ITEM_WEAPON},
                {WEAPON_BLACK_HOLE_GENERATOR, AI_SPECIAL_ITEM_WEAPON},
                {SPECIAL_ENERGY_ABSORBER, AI_SPECIAL_ITEM_SPECIAL},
                {WEAPON_PLASMA_WEB, AI_SPECIAL_ITEM_WEAPON},
                {WEAPON_GYRO_DESTABILIZER, AI_SPECIAL_ITEM_WEAPON},
            },
        },
        .special_device_priority = {
            {SPECIAL_QUANTUM_DETONATOR, AI_SPECIAL_ITEM_SPECIAL},
            {WEAPON_SPATIAL_COMPRESSOR, AI_SPECIAL_ITEM_WEAPON},
            {WEAPON_PULSAR, AI_SPECIAL_ITEM_WEAPON},
            {WEAPON_STASIS_FIELD, AI_SPECIAL_ITEM_WEAPON},
            {SPECIAL_PHASING_CLOAK, AI_SPECIAL_ITEM_SPECIAL},
            {SPECIAL_CLOAKING_DEVICE, AI_SPECIAL_ITEM_SPECIAL},
            {SPECIAL_STEALTH_FIELD, AI_SPECIAL_ITEM_SPECIAL},
            {SPECIAL_REFLECTION_FIELD, AI_SPECIAL_ITEM_SPECIAL},
            {SPECIAL_PHASE_SHIFTER, AI_SPECIAL_ITEM_SPECIAL},
            {SPECIAL_SUB_SPACE_TELEPORTER, AI_SPECIAL_ITEM_SPECIAL},
            {SPECIAL_WARP_DISSIPATOR, AI_SPECIAL_ITEM_SPECIAL},
            {SPECIAL_SCOUT_LAB, AI_SPECIAL_ITEM_SPECIAL},
        },
        .defense_priority = {
            .human_non_beams = {
                {SPECIAL_SECURITY_STATIONS, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_DISPLACEMENT_DEVICE, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_LIGHTNING_FIELD, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_HARD_SHIELDS, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_WIDE_AREA_JAMMER, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_MULTI_WAVE_ECM_JAMMER, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_HEAVY_ARMOR, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_INERTIAL_NULLIFIER, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_ECM_JAMMER, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_REINFORCED_HULL, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_INERTIAL_STABILIZER, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_AUGMENTED_ENGINES, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_SHIELD_CAPACITOR, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_MULTI_PHASED_SHIELDS, AI_SPECIAL_ITEM_SPECIAL},
                {WEAPON_ANTI_MISSILE_ROCKET, AI_SPECIAL_ITEM_WEAPON},
                {SPECIAL_REFLECTION_FIELD, AI_SPECIAL_ITEM_SPECIAL},
                {WEAPON_SPATIAL_COMPRESSOR, AI_SPECIAL_ITEM_WEAPON},
                {WEAPON_PULSAR, AI_SPECIAL_ITEM_WEAPON},
                {SPECIAL_AUTOMATED_REPAIR_UNIT, AI_SPECIAL_ITEM_SPECIAL},
            },
            .human_beams = {
                {SPECIAL_SECURITY_STATIONS, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_DISPLACEMENT_DEVICE, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_LIGHTNING_FIELD, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_HARD_SHIELDS, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_WIDE_AREA_JAMMER, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_MULTI_WAVE_ECM_JAMMER, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_HEAVY_ARMOR, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_INERTIAL_NULLIFIER, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_ECM_JAMMER, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_REINFORCED_HULL, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_INERTIAL_STABILIZER, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_AUGMENTED_ENGINES, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_SHIELD_CAPACITOR, AI_SPECIAL_ITEM_SPECIAL},
                {SPECIAL_MULTI_PHASED_SHIELDS, AI_SPECIAL_ITEM_SPECIAL},
                {WEAPON_ANTI_MISSILE_ROCKET, AI_SPECIAL_ITEM_WEAPON},
                {SPECIAL_REFLECTION_FIELD, AI_SPECIAL_ITEM_SPECIAL},
                {WEAPON_SPATIAL_COMPRESSOR, AI_SPECIAL_ITEM_WEAPON},
                {WEAPON_PULSAR, AI_SPECIAL_ITEM_WEAPON},
                {SPECIAL_AUTOMATED_REPAIR_UNIT, AI_SPECIAL_ITEM_SPECIAL},
            },
        },
    };

    s_rules rules{.priorities = DEFAULT_PRIORITIES};

    void Determine_Beam_Weapon_Modifications(
        const game::s_view* world,
        game::id_player player_idx,
        int16_t hull_size,
        int16_t* type0,
        int16_t* spec0,
        int16_t* arc0,
        int16_t* type1,
        int16_t* spec1,
        int16_t* arc1,
        int16_t* type2,
        int16_t* spec2,
        int16_t* arc2) {
        const auto legacy_player = legacy_world::Index(player_idx);
        if (!legacy_player) return;
        int16_t weapon_count = 0;
        int16_t weapon_id[DESIGN_WEAPON_CODE_COUNT];
        int16_t weapon_mods[DESIGN_WEAPON_CODE_COUNT];
        int16_t ai_group[DESIGN_WEAPON_CODE_COUNT];
        int16_t spec2_mods[DESIGN_WEAPON_CODE_COUNT];
        int16_t spec1_mods[DESIGN_WEAPON_CODE_COUNT];

        for (int16_t i = 1; i <= WEAPON_TRACTOR_BEAM; ++i) {
            if (TECHDATA::weapons[i].type == WEAPON_TYPE_BEAM && world->players[legacy_world::Slot(player_idx)].tech_applications[TECHDATA::weapons[i].tech_app_id] == TECH_RESEARCH_STATUS_RESEARCHED) {
                weapon_id[weapon_count] = i;
                spec1_mods[weapon_count] = 0;
                spec2_mods[weapon_count] = 0;
                weapon_mods[weapon_count] = 0;
                ai_group[weapon_count] = TECHDATA::technology_fields[TECHDATA::technology_applications[TECHDATA::weapons[i].tech_app_id].tech_field_id].ai_group;
                weapon_count++;
            }
        }

        if (weapon_count == 0) {
            *type2 = 0;
            *type1 = 0;
            *type0 = 0;
            return;
        }

        int16_t max_idx = weapon_count - 1;
        bool swapped;
        if (max_idx > 0) {
            do {
                swapped = false;
                for (int16_t i = 0; i < max_idx; ++i) {
                    if (ai_group[i + 1] < ai_group[i]) {
                        int16_t temp_ai = ai_group[i];
                        ai_group[i] = ai_group[i + 1];
                        ai_group[i + 1] = temp_ai;

                        int16_t temp_id = weapon_id[i];
                        weapon_id[i] = weapon_id[i + 1];
                        weapon_id[i + 1] = temp_id;

                        swapped = true;
                    }
                }
            } while (swapped);
        }

        int16_t first_living_player = 0;
        while (world->players[first_living_player].eliminated != 0) {
            first_living_player++;
        }

        int16_t best_comp = INITSHIP::Best_Computer(*legacy_player);
        int16_t comp_bonus = TECHDATA::computers[best_comp].bonus;
        comp_bonus += world->players[legacy_world::Slot(player_idx)].traits[TRAIT_SHIP_ATTACK];

        int16_t target_defense;
        int16_t target_shield_value;

        if (world->players[legacy_world::Slot(player_idx)].objectives == PLAYER_OBJECTIVE_HUMAN) {
            target_shield_value = 1;
            target_defense = TECHDATA::drives[DRIVE_FUSION_DRIVE].combat_speed_min[HULL_CRUISER] + 20;
        } else {
            target_shield_value = AIPOWER::Get_Best_Ship_Shield_Value(TECHDATA::shields, legacy_world::Player_Id(first_living_player));
            int16_t best_drive = INITSHIP::Best_Warp_Drive(first_living_player);
            target_defense = world->players[first_living_player].traits[TRAIT_SHIP_DEFENSE] + TECHDATA::drives[best_drive].combat_speed_min[2] * 5;
        }

        int16_t net_bonus = comp_bonus - target_defense;

        for (int16_t i = 0; i < weapon_count; ++i) {
            int16_t w_id = weapon_id[i];
            int16_t min_lvl = DESIGN::Miniaturization_Level(*legacy_player, TECHDATA::weapons[w_id].tech_app_id);
            if (min_lvl > 5) min_lvl = 5;

            int16_t cur_bonus = net_bonus;

            if (TECHDATA::weapon_modifications[WEAPON_MOD_CONTINUOUS].level <= min_lvl) {
                weapon_mods[i] += WEAPON_MOD_FLAG_CONTINUOUS;
                if (net_bonus < 50) {
                    spec1_mods[i] += WEAPON_MOD_FLAG_CONTINUOUS;
                    spec2_mods[i] += WEAPON_MOD_FLAG_CONTINUOUS;
                    cur_bonus = net_bonus + 25;
                }
            }

            if (cur_bonus > 0 && TECHDATA::weapon_modifications[WEAPON_MOD_NO_RANGE_DISSIPATION].level <= min_lvl) {
                spec1_mods[i] += WEAPON_MOD_FLAG_NO_RANGE_DISSIPATION;
                spec2_mods[i] += WEAPON_MOD_FLAG_NO_RANGE_DISSIPATION;
            }

            if (cur_bonus >= 0 && TECHDATA::weapon_modifications[WEAPON_MOD_AUTO_FIRE].level <= min_lvl) {
                if (cur_bonus > 30) {
                    weapon_mods[i] += WEAPON_MOD_FLAG_AUTO_FIRE;
                }
                spec1_mods[i] += WEAPON_MOD_FLAG_AUTO_FIRE;
                spec2_mods[i] += WEAPON_MOD_FLAG_AUTO_FIRE;
            }

            uint16_t avail = TECHDATA::weapons[w_id].available_mods;

            weapon_mods[i] = (weapon_mods[i] + WEAPON_MOD_FLAG_POINT_DEFENSE) & avail;
            if ((weapon_mods[i] & WEAPON_MOD_FLAG_POINT_DEFENSE) == 0) {
                weapon_mods[i] = 0;
            }

            if (TECHDATA::weapon_modifications[WEAPON_MOD_ARMOR_PIERCING].level <= min_lvl) {
                spec1_mods[i] += WEAPON_MOD_FLAG_ARMOR_PIERCING;
                spec2_mods[i] += WEAPON_MOD_FLAG_ARMOR_PIERCING;
            }

            if (TECHDATA::weapon_modifications[WEAPON_MOD_SHIELD_PIERCING].level <= min_lvl) {
                spec1_mods[i] += WEAPON_MOD_FLAG_SHIELD_PIERCING;
                spec2_mods[i] += WEAPON_MOD_FLAG_SHIELD_PIERCING;
            }

            if (TECHDATA::weapon_modifications[WEAPON_MOD_ENVELOPING].level <= min_lvl) {
                spec1_mods[i] += WEAPON_MOD_FLAG_ENVELOPING;
                spec2_mods[i] += WEAPON_MOD_FLAG_ENVELOPING;
            }

            spec1_mods[i] &= avail;

            spec2_mods[i] = (spec2_mods[i] + 2) & avail;
            if ((spec2_mods[i] & 2) == 0) {
                spec2_mods[i] = 0;
            }
        }

        if (world->settings->game_difficulty < GAME_DIFFICULTY_AVERAGE) {
            int16_t idx1 = weapon_count - 1;
            while (idx1 >= 0) {
                if ((TECHDATA::weapons[weapon_id[idx1]].available_mods & WEAPON_MOD_FLAG_POINT_DEFENSE) != 0) {
                    break;
                }
                idx1--;
            }

            if (idx1 < 0) {
                *type0 = 0;
                *spec0 = 0;
            } else {
                *type0 = weapon_id[idx1];
                *spec0 = WEAPON_MOD_FLAG_POINT_DEFENSE;
            }

            *type1 = weapon_id[weapon_count - 1];
            *spec1 = 0;

            int16_t idx2 = weapon_count - 1;
            while (idx2 >= 0) {
                if ((TECHDATA::weapons[weapon_id[idx2]].available_mods & WEAPON_MOD_FLAG_HEAVY_MOUNT) != 0) {
                    break;
                }
                idx2--;
            }

            if (idx2 < 0) {
                *type2 = 0;
                *spec2 = 0;
            } else {
                *type2 = weapon_id[idx2];
                *spec2 = WEAPON_MOD_FLAG_HEAVY_MOUNT;
            }
        } else {
            if (weapon_count == 1) {
                if ((TECHDATA::weapons[weapon_id[0]].available_mods & WEAPON_MOD_FLAG_POINT_DEFENSE) == 0) {
                    *type0 = 0;
                } else {
                    *type0 = weapon_id[0];
                    *spec0 = weapon_mods[0] & 0x7ffd;
                }

                *type1 = weapon_id[0];
                *spec1 = spec1_mods[0] & 0x7ff9;
            } else {
                int16_t pd_weapon = INITSHIP::Best_Point_Defense_Weapon_By_Size(*legacy_player);
                int16_t pd_idx = 0;
                for (pd_idx = 0; pd_idx < weapon_count; pd_idx++) {
                    if (weapon_id[pd_idx] == pd_weapon) {
                        break;
                    }
                }

                if (pd_idx >= weapon_count) {
                    *type0 = 0;
                } else {
                    *type0 = weapon_id[pd_idx];
                    *spec0 = weapon_mods[pd_idx] & 0x7ffd;
                }

                int16_t max_damage_idx = weapon_count - 1;
                if (max_damage_idx < 0) {
                    *type1 = 0;
                } else {
                    int16_t best_idx = 0;
                    if (max_damage_idx != 0) {
                        int32_t best_val = 0;
                        for (int16_t i = 0; i < weapon_count; i++) {
                            int16_t w_id = weapon_id[i];
                            if (w_id != 0) {
                                int32_t space = DESIGN::Weapon_Space(*legacy_player, w_id, 1, WEAPON_FIRING_ARC_FORWARD, 1, 0);
                                uint16_t natural_mods = TECHDATA::weapons[w_id].natural_mods;
                                int32_t damage = TECHDATA::weapons[w_id].max_damage;

                                if ((natural_mods & WEAPON_NATURAL_FLAG_IGNORES_SHIELDS) == 0) {
                                    damage -= target_shield_value;
                                }

                                int32_t val = (damage * 10) / space;

                                if ((natural_mods & WEAPON_NATURAL_FLAG_EXTRA_STRUCTURAL_DAMAGE) != 0) {
                                    val += TECHDATA::Extra_Structural_Damage(val);
                                }
                                if ((natural_mods & WEAPON_NATURAL_FLAG_NO_RANGE_DISSIPATION) != 0) {
                                    val = (val * 5) / 4;
                                }
                                if ((natural_mods & WEAPON_NATURAL_FLAG_ALWAYS_HITS) != 0) {
                                    val = (val * 5) / 4;
                                }
                                if ((natural_mods & WEAPON_NATURAL_FLAG_ENVELOPING) != 0) {
                                    val *= 2;
                                }
                                if ((natural_mods & WEAPON_NATURAL_FLAG_KILLS_MARINES) != 0 && world->players[legacy_world::Slot(player_idx)].ship_special_theme == AI_SHIP_SPECIAL_THEME_CAPTURE) {
                                    val *= 2;
                                }

                                if (val > best_val) {
                                    best_val = val;
                                    best_idx = i;
                                }
                            }
                        }
                    }
                    *type1 = weapon_id[best_idx];
                    *spec1 = spec1_mods[best_idx] & 0x7ff9;
                }
            }

            int16_t best_idx = -1;
            int32_t best_val = -1;
            for (int16_t i = 0; i < weapon_count; i++) {
                int16_t w_id = weapon_id[i];
                if (w_id != 0 && (TECHDATA::weapons[w_id].available_mods & WEAPON_MOD_FLAG_HEAVY_MOUNT) != 0) {
                    int32_t space = DESIGN::Weapon_Space(*legacy_player, w_id, 1, WEAPON_FIRING_ARC_FORWARD, 1, 2);
                    uint16_t natural_mods = TECHDATA::weapons[w_id].natural_mods;
                    int32_t damage = TECHDATA::weapons[w_id].max_damage * 3;

                    if ((natural_mods & WEAPON_NATURAL_FLAG_IGNORES_SHIELDS) == 0) {
                        damage = damage / 2 - target_shield_value;
                    } else {
                        damage = damage / 2;
                    }

                    int32_t val = (damage * 10) / space;

                    if ((natural_mods & WEAPON_NATURAL_FLAG_EXTRA_STRUCTURAL_DAMAGE) != 0) {
                        val += TECHDATA::Extra_Structural_Damage(val);
                    }
                    if ((natural_mods & WEAPON_NATURAL_FLAG_NO_RANGE_DISSIPATION) != 0) {
                        val = (val * 5) / 4;
                    }
                    if ((natural_mods & WEAPON_NATURAL_FLAG_ALWAYS_HITS) != 0) {
                        val = (val * 5) / 4;
                    }
                    if ((natural_mods & WEAPON_NATURAL_FLAG_ENVELOPING) != 0) {
                        val *= 2;
                    }
                    if ((natural_mods & WEAPON_NATURAL_FLAG_KILLS_MARINES) != 0 && world->players[legacy_world::Slot(player_idx)].ship_special_theme == AI_SHIP_SPECIAL_THEME_CAPTURE) {
                        val *= 2;
                    }

                    if (val > best_val) {
                        best_val = val;
                        best_idx = i;
                    }
                }
            }
            if (best_idx < 0) {
                *type2 = 0;
                *spec2 = 0;
            } else {
                *type2 = weapon_id[best_idx];
                *spec2 = spec2_mods[best_idx] & 0x7ffb;
            }
        }

        *arc1 = WEAPON_FIRING_ARC_FORWARD;
        *arc2 = WEAPON_FIRING_ARC_FORWARD;
        *arc0 = WEAPON_FIRING_ARC_FORWARD;

        int16_t comp_tech = Get_Competition_Player_Tech_Values(&world->players[0], 0x12);
        int16_t r_base = (int16_t)((0x14 - comp_tech) * hull_size);

        int16_t spec0_mod_count = 0;
        for (uint16_t m = WEAPON_MOD_FLAG_ARMOR_PIERCING; m < WEAPON_MOD_FLAG_MIRV; m *= 2) {
            if ((m & *spec0) != 0) {
                spec0_mod_count++;
            }
        }

        int16_t spec1_mod_count = 0;
        for (uint16_t m = WEAPON_MOD_FLAG_ARMOR_PIERCING; m < WEAPON_MOD_FLAG_MIRV; m *= 2) {
            if ((m & *spec1) != 0) {
                spec1_mod_count++;
            }
        }

        int16_t spec2_mod_count = 0;
        for (uint16_t m = WEAPON_MOD_FLAG_ARMOR_PIERCING; m < WEAPON_MOD_FLAG_MIRV; m *= 2) {
            if ((m & *spec2) != 0) {
                spec2_mod_count++;
            }
        }

        int32_t arc0_threshold = (int32_t)r_base + (int32_t)spec0_mod_count * 20;
        if ((int32_t)game_random::Random(100) < arc0_threshold) {
            int32_t arc0_inner_threshold = (int32_t)(r_base / 2) + (int32_t)spec0_mod_count * 20;
            if ((int32_t)game_random::Random(100) < arc0_inner_threshold) {
                *arc0 = WEAPON_FIRING_ARC_360;
            } else {
                *arc0 = WEAPON_FIRING_ARC_FORWARD_EXTENDED;
            }
        }

        int32_t arc1_threshold = (int32_t)spec1_mod_count * 20 + (int32_t)(r_base / 2);
        if ((int32_t)game_random::Random(100) < arc1_threshold) {
            *arc1 = WEAPON_FIRING_ARC_FORWARD_EXTENDED;
        }

        int32_t arc2_threshold = (int32_t)spec2_mod_count * 20 + (int32_t)(r_base / 4);
        if ((int32_t)game_random::Random(100) < arc2_threshold) {
            *arc2 = WEAPON_FIRING_ARC_FORWARD_EXTENDED;
        }
    }

    int32_t Add_Specials_To_Design(const game::s_view* world, const struct s_spcl* list, int16_t count, game::id_player player_idx, struct s_ship_design* design, int32_t space_available, int16_t* weapon_slot_idx_ptr) {
        const auto legacy_player = legacy_world::Index(player_idx);
        if (!legacy_player) return 0;
        int32_t total_space_used = 0;
        int16_t already_has_anti_missile_rocket = 0;

        for (int16_t i = 0; i < count; ++i) {
            int16_t item_id = list[i].id;

            if (list[i].type == AI_SPECIAL_ITEM_SPECIAL) {
                int16_t tech_app_id = TECHDATA::specials[item_id].tech_app_id;

                if (world->players[legacy_world::Slot(player_idx)].tech_applications[tech_app_id] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    if (Special_Legal_For_Design(TECHDATA::specials, design, item_id)) {
                        // Check if player has Stealth trait, if so skip Stealth Field device
                        if (rules.allow_stealth_field || world->players[legacy_world::Slot(player_idx)].traits[TRAIT_STEALTHY_SHIPS] == 0 || item_id != SPECIAL_STEALTH_FIELD) {
                            int8_t mini_cat = DESIGN::Miniaturization_Category(tech_app_id);
                            int16_t tech_lvl = DESIGN::Tech_Level(*legacy_player, tech_app_id);
                            int16_t base_space = TECHDATA::specials[item_id].space_per_ship_type[design->size];
                            int16_t actual_space = (int16_t)DESIGN::Space_Given_M_Level(base_space, tech_lvl, mini_cat);

                            if (actual_space < space_available) {
                                struct_::Set_Bit_Field(design->special_device_flags, item_id);
                                total_space_used += actual_space;
                                space_available -= actual_space;
                            }
                        }
                    }
                }
            } else {
                if (*weapon_slot_idx_ptr < 8) {
                    if (item_id == WEAPON_ANTI_MISSILE_ROCKET) {
                        for (int16_t slot = 0; slot < *weapon_slot_idx_ptr; ++slot) {
                            if (design->ship_weapon[slot].type == WEAPON_ANTI_MISSILE_ROCKET) {
                                already_has_anti_missile_rocket = 1;
                            }
                        }
                    }

                    int16_t weapon_tech_app_id = TECHDATA::weapons[item_id].tech_app_id;
                    if (world->players[legacy_world::Slot(player_idx)].tech_applications[weapon_tech_app_id] == TECH_RESEARCH_STATUS_RESEARCHED && !already_has_anti_missile_rocket) {
                        int32_t weapon_unit_space = DESIGN::Weapon_Space(*legacy_player, item_id, 1, WEAPON_FIRING_ARC_360, 1, 0);
                        int16_t max_count = (int16_t)(space_available / weapon_unit_space);

                        if (item_id == WEAPON_ANTI_MISSILE_ROCKET) {
                            if (max_count > (int16_t)design->size * 10) {
                                max_count = (int16_t)design->size * 10;
                            }
                        }

                        if (max_count > 0) {
                            int16_t items_this_type = 0;

                            design->ship_weapon[*weapon_slot_idx_ptr].type = item_id;
                            design->ship_weapon[*weapon_slot_idx_ptr].firing_arc = WEAPON_FIRING_ARC_360;
                            design->ship_weapon[*weapon_slot_idx_ptr].specials = 0;
                            design->ship_weapon[*weapon_slot_idx_ptr].ammo = 1;

                            while (max_count > 99) {
                                if (*weapon_slot_idx_ptr >= 8) {
                                    break;
                                }

                                items_this_type += 99;
                                design->ship_weapon[*weapon_slot_idx_ptr].count = 99;
                                design->ship_weapon[*weapon_slot_idx_ptr].current_count = 99;
                                max_count -= 99;
                                ++(*weapon_slot_idx_ptr);

                                if (*weapon_slot_idx_ptr < 8) {
                                    design->ship_weapon[*weapon_slot_idx_ptr].type = item_id;
                                    design->ship_weapon[*weapon_slot_idx_ptr].firing_arc = WEAPON_FIRING_ARC_360;
                                    design->ship_weapon[*weapon_slot_idx_ptr].specials = 0;
                                    design->ship_weapon[*weapon_slot_idx_ptr].ammo = 1;
                                }
                            }

                            if (*weapon_slot_idx_ptr < 8) {
                                items_this_type += max_count;
                                design->ship_weapon[*weapon_slot_idx_ptr].count = (int8_t)max_count;
                                design->ship_weapon[*weapon_slot_idx_ptr].current_count = (int8_t)max_count;
                            }

                            ++(*weapon_slot_idx_ptr);

                            int16_t space_calc = (int16_t)DESIGN::Weapon_Space(*legacy_player, item_id, 1, WEAPON_FIRING_ARC_360, 1, 0);
                            int32_t used_space = space_calc * items_this_type;
                            total_space_used += used_space;
                            space_available -= used_space;
                        }
                    }
                }
            }
        }

        return total_space_used;
    }

    int32_t Add_Beam_Weapon_To_Design(const game::s_view* world, game::id_player player_idx, struct s_ship_design* design, int16_t* weapon_index_ptr, int32_t space, int16_t weapon_class) {
        const auto legacy_player = legacy_world::Index(player_idx);
        if (!legacy_player) return 0;
        int16_t types[4] = {0};
        int16_t specials[4] = {0};
        int16_t arcs[4] = {0};
        int16_t total_added = 0;

        Determine_Beam_Weapon_Modifications(
            world,
            player_idx,
            static_cast<int16_t>(design->size),
            &types[0],
            &specials[0],
            &arcs[0],
            &types[1],
            &specials[1],
            &arcs[1],
            &types[2],
            &specials[2],
            &arcs[2]);

        specials[0] |= WEAPON_MOD_FLAG_POINT_DEFENSE;
        specials[2] |= WEAPON_MOD_FLAG_HEAVY_MOUNT;

        int16_t weapon_type = types[weapon_class];
        int16_t weapon_specials = specials[weapon_class];
        int16_t weapon_arc = arcs[weapon_class];

        int32_t unit_space = DESIGN::Weapon_Space(*legacy_player, weapon_type, 1, (uint8_t)weapon_arc, 0, weapon_specials);
        if (unit_space == 0) {
            return 0;
        }

        int32_t quantity = space / unit_space;

        if (quantity <= 0 || *weapon_index_ptr >= 8 || weapon_type == 0) {
            return 0;
        }

        design->ship_weapon[*weapon_index_ptr].type = weapon_type;
        design->ship_weapon[*weapon_index_ptr].firing_arc = static_cast<int8_t>(weapon_arc);
        design->ship_weapon[*weapon_index_ptr].specials = weapon_specials;
        design->ship_weapon[*weapon_index_ptr].ammo = 0;

        while (quantity > 99) {
            if (*weapon_index_ptr >= 8) {
                break;
            }

            design->ship_weapon[*weapon_index_ptr].count = 99;
            design->ship_weapon[*weapon_index_ptr].current_count = 99;
            total_added += 99;
            quantity -= 99;

            (*weapon_index_ptr)++;

            if (*weapon_index_ptr < 8) {
                design->ship_weapon[*weapon_index_ptr].type = weapon_type;
                design->ship_weapon[*weapon_index_ptr].firing_arc = static_cast<int8_t>(weapon_arc);
                design->ship_weapon[*weapon_index_ptr].specials = weapon_specials;
                design->ship_weapon[*weapon_index_ptr].ammo = 0;
            }
        }

        if (*weapon_index_ptr < 8) {
            design->ship_weapon[*weapon_index_ptr].count = static_cast<int8_t>(quantity);
            design->ship_weapon[*weapon_index_ptr].current_count = static_cast<int8_t>(quantity);
            total_added += quantity;
        }

        (*weapon_index_ptr)++;

        int32_t final_unit_space = DESIGN::Weapon_Space(*legacy_player, weapon_type, 1, (uint8_t)weapon_arc, 0, weapon_specials);
        return final_unit_space * total_added;
    }

    int16_t Get_Ship_Class(const game::s_view* world, game::id_player player_idx) {
        const s_player* player = &world->players[legacy_world::Slot(player_idx)];
        int16_t fighters_avail = Fighters_Available(player);
        int16_t special_weapons_avail = Special_Weapons_Available(player);
        int16_t special_devices_avail = Special_Devices_Available(
            &rules.priorities, player);

        if (*world->combat_x == -1000) {
            return 7;
        }

        int16_t result = 0;

        auto get_default_class = [&]() -> int16_t {
            return (player->tech_applications[TECH_APP_LASER_CANNON] == TECH_RESEARCH_STATUS_RESEARCHED) ? 0 : 3;
        };

        auto get_fighters_fallback = [&]() -> int16_t {
            return (fighters_avail != 0) ? 1 : 2;
        };

        switch ((uint8_t)player->ship_design_theme) {
            case AI_SHIP_DESIGN_THEME_BALANCED:
            case AI_SHIP_DESIGN_THEME_HUMAN: {
                switch ((int16_t)game_random::Random(20)) {
                    case 1:
                    case 2:
                    case 3:
                    case 4:
                    case 5:
                    case 6:
                    case 7:
                    case 8:
                    case 9:
                    case 10:
                    case 11:
                    case 12:
                    case 13:
                        return get_default_class();
                    case 14:
                        return 3;
                    case 15:
                        return (fighters_avail != 0) ? 4 : get_default_class();
                    case 16:
                        return (fighters_avail != 0) ? 1 : get_default_class();
                    case 17:
                        return 2;
                    case 18:
                    case 19:
                        return (special_weapons_avail != 0) ? 6 : get_default_class();
                    case 20:
                        return (special_devices_avail != 0) ? 5 : get_default_class();
                    default:
                        return 0;
                }
            }

            case AI_SHIP_DESIGN_THEME_CARRIER: {
                switch ((int16_t)game_random::Random(20)) {
                    case 1:
                        return get_default_class();
                    case 2:
                        return 3;
                    case 3:
                    case 4:
                    case 5:
                    case 6:
                    case 7:
                    case 8:
                    case 9:
                    case 10:
                    case 11:
                    case 12:
                    case 13:
                    case 14:
                    case 15:
                        return (fighters_avail != 0) ? 4 : get_default_class();
                    case 16:
                    case 17:
                    case 18:
                        return get_fighters_fallback();
                    case 19:
                        return (special_weapons_avail != 0) ? 6 : get_fighters_fallback();
                    case 20:
                        return (special_devices_avail != 0) ? 5 : get_fighters_fallback();
                    default:
                        return 0;
                }
            }

            case AI_SHIP_DESIGN_THEME_MISSILE:
                switch ((int16_t)game_random::Random(20)) {
                    case 1:
                        result = get_default_class();
                        break;
                    case 2:
                    case 3:
                    case 4:
                    case 5:
                    case 6:
                    case 7:
                    case 8:
                    case 9:
                    case 10:
                    case 11:
                    case 12:
                    case 13:
                    case 14:
                        result = 3;
                        break;
                    case 15:
                        result = (fighters_avail != 0) ? 4 : 3;
                        break;
                    case 16:
                    case 17:
                    case 18:
                        result = 2;
                        break;
                    case 19:
                        result = (special_weapons_avail != 0) ? 6 : 2;
                        break;
                    case 20:
                        result = (special_devices_avail != 0) ? 5 : 2;
                        break;
                    default:
                        break;
                }
                [[fallthrough]];

            case AI_SHIP_DESIGN_THEME_SPECIAL:
                switch ((int16_t)game_random::Random(20)) {
                    case 1:
                        return get_default_class();
                    case 2:
                        return 3;
                    case 3:
                        return (fighters_avail != 0) ? 4 : get_default_class();
                    case 4:
                    case 5:
                        return get_fighters_fallback();
                    case 6:
                    case 7:
                        return 2;
                    case 8:
                    case 9:
                    case 10:
                    case 11:
                        return (special_weapons_avail != 0) ? 6 : get_fighters_fallback();
                    case 12:
                    case 13:
                    case 14:
                    case 15:
                        return (special_weapons_avail != 0) ? 6 : 2;
                    case 16:
                    case 17:
                        return (special_devices_avail != 0) ? 5 : get_fighters_fallback();
                    case 18:
                    case 19:
                    case 20:
                        result = (special_devices_avail != 0) ? 5 : 2;
                        break;
                    default:
                        break;
                }
                break;

            default:
                break;
        }

        return result;
    }

    // Callers explicitly retain the legacy player-zero comparison perspective.
    int16_t __cdecl Get_Competition_Player_Tech_Values(const s_player* player, int16_t tech_type) {
        int16_t max_ai_group = 0;
        int16_t i = 0;

        do {
            const auto& app = TECHDATA::technology_applications[i];

            if (app.ai_group == tech_type) {
                if (player->tech_applications[i] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    int16_t field_id = app.tech_field_id;
                    uint8_t field_group = TECHDATA::technology_fields[field_id].ai_group;

                    if (max_ai_group < field_group) {
                        max_ai_group = field_group;
                    }
                }
            }
            i++;
        } while (i < TECH_APP_COUNT);

        return max_ai_group;
    }

    bool __cdecl Best_Weapons_Are_Beams(const s_player* player) {
        uint16_t beam_tech_value = Get_Competition_Player_Tech_Values(player, 0x15);
        uint16_t comparison_tech_value = Get_Competition_Player_Tech_Values(player, 0x1a);

        return beam_tech_value <= comparison_tech_value;
    }

    int32_t Add_Theme_Special_To_Design(const game::s_view* world, game::id_player player_idx, struct s_ship_design* design, int32_t space, int16_t* weapon_index_ptr) {
        const auto& priorities = rules.priorities;
        int32_t result = 0;
        uint8_t theme = (uint8_t)world->players[legacy_world::Slot(player_idx)].ship_special_theme;

        switch (theme) {
            case AI_SHIP_SPECIAL_THEME_BEAM_DEFENSE:
                result = Add_Specials_To_Design(world, priorities.special_priority.beam_defense, 4, player_idx, design, space, weapon_index_ptr);
                break;
            case AI_SHIP_SPECIAL_THEME_CAPTURE:
                result = Add_Specials_To_Design(world, priorities.special_priority.capture, 5, player_idx, design, space, weapon_index_ptr);
                break;
            case AI_SHIP_SPECIAL_THEME_BIO_WEAPON:
                result = Add_Specials_To_Design(world, priorities.special_priority.bio_weapon, 5, player_idx, design, space, weapon_index_ptr);
                break;
            case AI_SHIP_SPECIAL_THEME_MISSILE_DEFENSE:
                result = Add_Specials_To_Design(world, priorities.special_priority.missile_defense, 8, player_idx, design, space, weapon_index_ptr);
                break;
            case AI_SHIP_SPECIAL_THEME_CLOAKING:
                result = Add_Specials_To_Design(world, priorities.special_priority.cloaking, 3, player_idx, design, space, weapon_index_ptr);
                break;
            case AI_SHIP_SPECIAL_THEME_ARMOR:
                result = Add_Specials_To_Design(world, priorities.special_priority.armor, 3, player_idx, design, space, weapon_index_ptr);
                break;
            case AI_SHIP_SPECIAL_THEME_SHIELD:
                result = Add_Specials_To_Design(world, priorities.special_priority.shield, 3, player_idx, design, space, weapon_index_ptr);
                break;
            case AI_SHIP_SPECIAL_THEME_HUMAN:
                result = Add_Specials_To_Design(world, priorities.special_priority.missile_defense, 8, player_idx, design, space, weapon_index_ptr);
                break;
            default:
                break;
        }

        return result;
    }

    int16_t __cdecl Special_Weapons_Available(const s_player* player_ptr) {
        const auto& player = *player_ptr;

        if (player.tech_applications[TECH_APP_GYRO_DESTABILIZER] == TECH_RESEARCH_STATUS_RESEARCHED ||
            player.tech_applications[TECH_APP_ENERGY_ABSORBER] == TECH_RESEARCH_STATUS_RESEARCHED ||
            player.tech_applications[TECH_APP_BLACK_HOLE_GENERATOR] == TECH_RESEARCH_STATUS_RESEARCHED ||
            player.tech_applications[TECH_APP_STELLAR_CONVERTER] == TECH_RESEARCH_STATUS_RESEARCHED ||
            player.tech_applications[TECH_APP_PLASMA_WEB] == TECH_RESEARCH_STATUS_RESEARCHED) {
            return 1;
        }

        return 0;
    }

    int16_t __cdecl Special_Devices_Available(const s_ship_design_config* priorities, const s_player* player) {
        for (int16_t i = 0; i < 12; ++i) {
            int16_t tech_app_id;
            int16_t id = priorities->special_device_priority[i].id;
            int16_t type = priorities->special_device_priority[i].type;

            if (type == AI_SPECIAL_ITEM_SPECIAL) {
                tech_app_id = (int16_t)TECHDATA::specials[id].tech_app_id;
            } else {
                tech_app_id = (int16_t)TECHDATA::weapons[id].tech_app_id;
            }

            if (player->tech_applications[tech_app_id] == TECH_RESEARCH_STATUS_RESEARCHED) {
                return 1;
            }
        }

        return 0;
    }

    int16_t __cdecl Fighters_Available(const s_player* player_ptr) {
        const auto& player = *player_ptr;

        if (player.tech_applications[TECH_APP_FIGHTER_BAYS] == TECH_RESEARCH_STATUS_RESEARCHED ||
            player.tech_applications[TECH_APP_BOMBER_BAYS] == TECH_RESEARCH_STATUS_RESEARCHED ||
            player.tech_applications[TECH_APP_HEAVY_FIGHTER_BAYS] == TECH_RESEARCH_STATUS_RESEARCHED) {
            return 1;
        }

        return 0;
    }

    int32_t Add_Fighters_To_Design(const game::s_view* world, game::id_player player_idx, s_ship_design* design, int16_t* weapon_slot_idx, int32_t available_space) {
        const auto legacy_player = legacy_world::Index(player_idx);
        if (!legacy_player) return 0;
        int16_t weapon_type;
        int space_per_weapon;
        int16_t total_fighters = 0;
        int32_t total_space_used = 0;

        if (world->players[legacy_world::Slot(player_idx)].tech_applications[TECH_APP_HEAVY_FIGHTER_BAYS] == TECH_RESEARCH_STATUS_RESEARCHED) {
            space_per_weapon = DESIGN::Weapon_Space(*legacy_player, WEAPON_HEAVY_FIGHTER, 1, WEAPON_FIRING_ARC_360, 1, 0);
            if (available_space / space_per_weapon > 0) {
                weapon_type = WEAPON_HEAVY_FIGHTER;
                goto calculate_fighters;
            }
        }

        if (world->players[legacy_world::Slot(player_idx)].tech_applications[TECH_APP_BOMBER_BAYS] == TECH_RESEARCH_STATUS_RESEARCHED) {
            space_per_weapon = DESIGN::Weapon_Space(*legacy_player, WEAPON_BOMBER, 1, WEAPON_FIRING_ARC_360, 1, 0);
            if (available_space / space_per_weapon > 0) {
                weapon_type = WEAPON_BOMBER;
                goto calculate_fighters;
            }
        }

        weapon_type = WEAPON_INTERCEPTOR;

    calculate_fighters:
        space_per_weapon = DESIGN::Weapon_Space(*legacy_player, weapon_type, 1, WEAPON_FIRING_ARC_360, 1, 0);
        int num_fighters = (int)available_space / space_per_weapon;

        if (num_fighters > 0 && weapon_type != 0 && *weapon_slot_idx < 8) {
            int16_t current_idx = *weapon_slot_idx;

            design->ship_weapon[current_idx].type = weapon_type;
            design->ship_weapon[current_idx].firing_arc = WEAPON_FIRING_ARC_360;
            design->ship_weapon[current_idx].specials = 0;
            design->ship_weapon[current_idx].ammo = 1;

            while (num_fighters > 20) {
                if (current_idx >= 8) goto finalize_count;

                design->ship_weapon[current_idx].count = 20;
                design->ship_weapon[current_idx].current_count = 20;
                total_fighters += 20;
                num_fighters -= 20;

                current_idx++;
                *weapon_slot_idx = current_idx;

                if (current_idx < 8) {
                    design->ship_weapon[current_idx].type = weapon_type;
                    design->ship_weapon[current_idx].firing_arc = WEAPON_FIRING_ARC_360;
                    design->ship_weapon[current_idx].specials = 0;
                    design->ship_weapon[current_idx].ammo = 1;
                } else {
                    goto finalize_count;
                }
            }

            if (current_idx < 8) {
                design->ship_weapon[current_idx].count = (int8_t)num_fighters;
                design->ship_weapon[current_idx].current_count = (int8_t)num_fighters;
                total_fighters += (int16_t)num_fighters;
            }

        finalize_count:
            *weapon_slot_idx = *weapon_slot_idx + 1;

            int fighter_unit_space = DESIGN::Weapon_Space(*legacy_player, weapon_type, 1, WEAPON_FIRING_ARC_360, 1, 0);
            total_space_used = fighter_unit_space * total_fighters;
        }

        return total_space_used;
    }

    void Determine_Bomb_Weapon(const game::s_view* world, game::id_player player_idx, int16_t* out_weapon_idx) {
        const auto legacy_player = legacy_world::Index(player_idx);
        if (!legacy_player) return;
        int16_t candidates[DESIGN_WEAPON_CODE_COUNT];
        int16_t scores[DESIGN_WEAPON_CODE_COUNT];
        int num_candidates = 0;

        for (int i = 1; i <= WEAPON_TRACTOR_BEAM; ++i) {
            if (TECHDATA::weapons[i].type == WEAPON_TYPE_BOMB && (TECHDATA::weapons[i].natural_mods & WEAPON_NATURAL_FLAG_BIOLOGICAL) == 0) {
                int16_t tech_app_id = TECHDATA::weapons[i].tech_app_id;
                if (world->players[legacy_world::Slot(player_idx)].tech_applications[tech_app_id] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    candidates[num_candidates] = i;
                    int16_t field_id = TECHDATA::technology_applications[tech_app_id].tech_field_id;
                    scores[num_candidates] = TECHDATA::technology_fields[field_id].ai_group;
                    num_candidates++;
                }
            }
        }

        if (num_candidates == 0) {
            *out_weapon_idx = 0;
            return;
        }

        bool swapped;
        do {
            swapped = false;
            for (int i = 0; i < num_candidates - 1; ++i) {
                if (scores[i + 1] < scores[i]) {
                    int16_t temp_score = scores[i];
                    scores[i] = scores[i + 1];
                    scores[i + 1] = temp_score;

                    int16_t temp_cand = candidates[i];
                    candidates[i] = candidates[i + 1];
                    candidates[i + 1] = temp_cand;
                    swapped = true;
                }
            }
        } while (swapped);

        uint16_t miniaturization_tech_value = Get_Competition_Player_Tech_Values(&world->players[0], 0x21 /* AI_GROUP_MINIATURIZATION */);
        uint16_t force_field_tech_value = Get_Competition_Player_Tech_Values(&world->players[0], 10 /* AI_GROUP_FORCE_FIELDS */);
        int threshold = (miniaturization_tech_value * 2 + 2) / 3 + force_field_tech_value;

        int i = 0;
        for (; i < num_candidates; ++i) {
            int16_t tech_app_id = TECHDATA::weapons[candidates[i]].tech_app_id;
            int16_t min_lvl = DESIGN::Miniaturization_Level(*legacy_player, tech_app_id);
            if (min_lvl > 5) min_lvl = 5;

            if (scores[i] + min_lvl >= threshold) {
                break;
            }
        }

        if (i == num_candidates) {
            *out_weapon_idx = candidates[num_candidates - 1];
        } else {
            *out_weapon_idx = candidates[i];
        }
    }

    int32_t Add_Bomb_Weapon_To_Design(const game::s_view* world, game::id_player player_idx, s_ship_design* design, int16_t* weapon_slot_idx, int32_t space_available) {
        const auto legacy_player = legacy_world::Index(player_idx);
        if (!legacy_player) return 0;
        int16_t bomb_type = 0;
        int16_t total_bombs_added = 0;

        Determine_Bomb_Weapon(world, player_idx, &bomb_type);

        int32_t space_per_bomb = DESIGN::Weapon_Space(*legacy_player, bomb_type, 1, WEAPON_FIRING_ARC_FORWARD, 0, 0);

        if (space_per_bomb == 0) {
            return 0;
        }

        int32_t num_bombs = space_available / space_per_bomb;

        if (num_bombs > 0 && *weapon_slot_idx < 8 && bomb_type != 0) {
            design->ship_weapon[*weapon_slot_idx].type = bomb_type;
            design->ship_weapon[*weapon_slot_idx].firing_arc = WEAPON_FIRING_ARC_360;
            design->ship_weapon[*weapon_slot_idx].specials = 0;
            design->ship_weapon[*weapon_slot_idx].ammo = 10;

            while (num_bombs > 99) {
                if (*weapon_slot_idx >= 8) {
                    break;
                }

                num_bombs -= 99;
                design->ship_weapon[*weapon_slot_idx].count = 99;
                design->ship_weapon[*weapon_slot_idx].current_count = 99;
                total_bombs_added += 99;

                *weapon_slot_idx += 1;

                if (*weapon_slot_idx < 8) {
                    design->ship_weapon[*weapon_slot_idx].type = bomb_type;
                    design->ship_weapon[*weapon_slot_idx].firing_arc = WEAPON_FIRING_ARC_360;
                    design->ship_weapon[*weapon_slot_idx].specials = 0;
                    design->ship_weapon[*weapon_slot_idx].ammo = 10;
                }
            }

            if (*weapon_slot_idx < 8) {
                total_bombs_added += num_bombs;
                design->ship_weapon[*weapon_slot_idx].count = (int8_t)num_bombs;
                design->ship_weapon[*weapon_slot_idx].current_count = (int8_t)num_bombs;
            }

            *weapon_slot_idx += 1;
        }

        int32_t final_space_per_bomb = DESIGN::Weapon_Space(*legacy_player, bomb_type, 1, WEAPON_FIRING_ARC_FORWARD, 0, 0);

        return final_space_per_bomb * total_bombs_added;
    }

    void Determine_Missile_Weapon_Modifications(
        const game::s_view* world,
        game::id_player player_idx,
        uint16_t* out_weapon_id,
        uint16_t* out_weapon_mods,
        int16_t weapon_count_table_idx,
        int32_t available_space,
        int16_t* out_weapon_count) {
        const auto legacy_player = legacy_world::Index(player_idx);
        if (!legacy_player) return;
        uint16_t sort_keys[DESIGN_WEAPON_CODE_COUNT];
        uint16_t weapon_indices[DESIGN_WEAPON_CODE_COUNT];
        uint16_t weapon_mods[DESIGN_WEAPON_CODE_COUNT];
        int16_t count = 0;

        for (int16_t i = 1; i <= WEAPON_TRACTOR_BEAM; ++i) {
            uint8_t type = TECHDATA::weapons[i].type;
            if (type == WEAPON_TYPE_MISSILE || type == WEAPON_TYPE_TORPEDO) {
                uint16_t tech_app_id = TECHDATA::weapons[i].tech_app_id;
                if (world->players[legacy_world::Slot(player_idx)].tech_applications[tech_app_id] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    weapon_indices[count] = i;
                    weapon_mods[count] = 0;
                    int16_t tech_field_id = TECHDATA::technology_applications[tech_app_id].tech_field_id;
                    sort_keys[count] = (uint16_t)TECHDATA::technology_fields[tech_field_id].ai_group;
                    count++;
                }
            }
        }

        if (count == 0) {
            *out_weapon_id = 0;
            return;
        }

        int16_t swapped;
        do {
            swapped = 0;
            if (count - 1 > 0) {
                for (int16_t i = 0; i < count - 1; ++i) {
                    if (sort_keys[i + 1] < sort_keys[i]) {
                        uint16_t temp_idx = weapon_indices[i + 1];
                        uint16_t temp_key = sort_keys[i];
                        uint16_t temp_idx2 = weapon_indices[i];

                        sort_keys[i] = sort_keys[i + 1];
                        sort_keys[i + 1] = temp_key;

                        weapon_indices[i] = temp_idx;
                        weapon_indices[i + 1] = temp_idx2;

                        swapped = 1;
                    }
                }
            }
        } while (swapped == 1);

        int16_t competitor_idx = 0;
        while (world->players[competitor_idx].eliminated != 0) {
            competitor_idx++;
        }

        int16_t max_shield = AIPOWER::Get_Best_Ship_Shield_Value(TECHDATA::shields, legacy_world::Player_Id(competitor_idx));
        int16_t comp_tech = (int16_t)Get_Competition_Player_Tech_Values(&world->players[0], 0x1C);

        if (count > 0) {
            for (int16_t i = 0; i < count; ++i) {
                uint16_t w_idx = weapon_indices[i];
                if (TECHDATA::weapons[w_idx].max_damage < max_shield + 4) {
                    weapon_indices[i] = 0;
                    sort_keys[i] = 0;
                    out_weapon_mods[i] = 0;
                } else {
                    int16_t mini_level = DESIGN::Miniaturization_Level(*legacy_player, TECHDATA::weapons[w_idx].tech_app_id);
                    if (mini_level > 5) {
                        mini_level = 5;
                    }

                    if (mini_level >= 1) {
                        if (comp_tech > 0) {
                            weapon_mods[i] += WEAPON_MOD_FLAG_ECCM;
                        }
                        weapon_mods[i] += WEAPON_MOD_FLAG_NO_RANGE_DISSIPATION | WEAPON_MOD_FLAG_HEAVILY_ARMORED | WEAPON_MOD_FLAG_FAST | WEAPON_MOD_FLAG_OVERLOADED;
                    }

                    if (mini_level >= 2) {
                        weapon_mods[i] += WEAPON_MOD_FLAG_ENVELOPING | WEAPON_MOD_FLAG_MIRV;
                    }

                    if (world->players[legacy_world::Slot(player_idx)].tech_applications[TECH_APP_EMISSIONS_GUIDANCE_SYSTEM] == TECH_RESEARCH_STATUS_RESEARCHED) {
                        if (mini_level >= 2) {
                            weapon_mods[i] += WEAPON_MOD_FLAG_EMISSION_GUIDANCE;
                        }
                    }

                    weapon_mods[i] &= TECHDATA::weapons[w_idx].available_mods;
                }
            }
        }

        if (world->settings->game_difficulty < GAME_DIFFICULTY_AVERAGE) {
            *out_weapon_id = weapon_indices[count - 1];
            *out_weapon_mods = 0;
            return;
        }

        if (count == 1) {
            *out_weapon_id = weapon_indices[0];
            *out_weapon_mods = weapon_mods[0];
        } else {
            int32_t best_score = 0;
            int16_t best_index = 0;

            if (count > 0) {
                for (int16_t i = 0; i < count; ++i) {
                    uint16_t w_idx = weapon_indices[i];
                    if (w_idx != 0) {
                        int32_t space = DESIGN::Weapon_Space(*legacy_player, w_idx, 1, WEAPON_FIRING_ARC_FORWARD, 5, 0);
                        int32_t score = (((int32_t)TECHDATA::weapons[w_idx].max_damage - max_shield) * 10) / space;
                        if (best_score < score) {
                            best_index = i;
                            best_score = score;
                        }
                    }
                }
            }

            *out_weapon_id = weapon_indices[best_index];
            *out_weapon_mods = weapon_mods[best_index] & 0x7FF9;
        }

        uint16_t mods = *out_weapon_mods;
        int32_t space = DESIGN::Weapon_Space(*legacy_player, *out_weapon_id, 1, WEAPON_FIRING_ARC_FORWARD, *out_weapon_count, mods);
        int16_t minimum_weapon_count = MIN_WEAPON_COUNTS[weapon_count_table_idx];
        int32_t fitting_weapon_count = available_space / space;

        if (fitting_weapon_count < minimum_weapon_count ||
            (DESIGN::missile_x3_shot != 0 && fitting_weapon_count == minimum_weapon_count)) {
            if (*out_weapon_count > 5) {
                *out_weapon_count = 5;
            }

            space = DESIGN::Weapon_Space(*legacy_player, *out_weapon_id, 1, WEAPON_FIRING_ARC_FORWARD, *out_weapon_count, mods);
            fitting_weapon_count = available_space / space;

            if (DESIGN::Should_Autodesign_Use_X3_Rack(
                    *out_weapon_count, fitting_weapon_count, minimum_weapon_count)) {
                *out_weapon_count = DESIGN::Missile_Rack_Quantity(1);
                space = DESIGN::Weapon_Space(*legacy_player, *out_weapon_id, 1, WEAPON_FIRING_ARC_FORWARD, *out_weapon_count, mods);
                fitting_weapon_count = available_space / space;
            }

            if (fitting_weapon_count < minimum_weapon_count) {
                if (mods & WEAPON_MOD_FLAG_HEAVILY_ARMORED) {
                    mods ^= WEAPON_MOD_FLAG_HEAVILY_ARMORED;
                    *out_weapon_mods = mods;
                }

                space = DESIGN::Weapon_Space(*legacy_player, *out_weapon_id, 1, WEAPON_FIRING_ARC_FORWARD, *out_weapon_count, mods);

                if ((available_space / space) < MIN_WEAPON_COUNTS[weapon_count_table_idx]) {
                    if (mods & WEAPON_MOD_FLAG_FAST) {
                        mods ^= WEAPON_MOD_FLAG_FAST;
                        *out_weapon_mods = mods;
                    }

                    DESIGN::Weapon_Space(*legacy_player, *out_weapon_id, 1, WEAPON_FIRING_ARC_FORWARD, *out_weapon_count, mods);
                }
            }
        }
    }

    int32_t Add_Missile_Weapon_To_Design(const game::s_view* world, game::id_player player_idx, s_ship_design* design, int16_t* weapon_index_ptr, int32_t space_limit, int16_t ammo) {
        const auto legacy_player = legacy_world::Index(player_idx);
        if (!legacy_player) return 0;
        uint16_t weapon_type = 0;
        uint16_t specials = 0;
        int16_t total_missiles_added = 0;
        int16_t modified_ammo = ammo;

        Determine_Missile_Weapon_Modifications(world, player_idx, &weapon_type, &specials, (int16_t)design->size, space_limit, &modified_ammo);

        if (weapon_type == 0) {
            return 0;
        }

        int32_t space_per_missile = DESIGN::Weapon_Space(*legacy_player, (int16_t)weapon_type, 1, WEAPON_FIRING_ARC_FORWARD, modified_ammo, specials);

        int32_t total_possible = space_limit / space_per_missile;

        if (total_possible <= 0 || *weapon_index_ptr >= 8) {
            return 0;
        }

        int16_t current_idx = *weapon_index_ptr;
        design->ship_weapon[current_idx].type = (int16_t)weapon_type;
        design->ship_weapon[current_idx].firing_arc = WEAPON_FIRING_ARC_360;
        design->ship_weapon[current_idx].specials = (int16_t)specials;
        design->ship_weapon[current_idx].ammo = (int8_t)modified_ammo;

        while (total_possible > 20) {
            if (*weapon_index_ptr >= 8) {
                goto finalize_index;
            }

            current_idx = *weapon_index_ptr;
            design->ship_weapon[current_idx].count = 20;
            design->ship_weapon[current_idx].current_count = 20;
            total_missiles_added += 20;
            total_possible -= 20;

            (*weapon_index_ptr)++;
            current_idx = *weapon_index_ptr;

            if (current_idx < 8) {
                design->ship_weapon[current_idx].type = (int16_t)weapon_type;
                design->ship_weapon[current_idx].firing_arc = WEAPON_FIRING_ARC_360;
                design->ship_weapon[current_idx].specials = (int16_t)specials;
                design->ship_weapon[current_idx].ammo = (int8_t)modified_ammo;
            }
        }

        if (*weapon_index_ptr < 8) {
            current_idx = *weapon_index_ptr;
            total_missiles_added += total_possible;
            design->ship_weapon[current_idx].count = (int8_t)total_possible;
            design->ship_weapon[current_idx].current_count = (int8_t)total_possible;
        }

    finalize_index:
        (*weapon_index_ptr)++;

        int32_t final_space_coeff = DESIGN::Weapon_Space(*legacy_player, (int16_t)weapon_type, 1, WEAPON_FIRING_ARC_FORWARD, modified_ammo, specials);
        return final_space_coeff * total_missiles_added;
    }

    void Auto_Design_Ship(const game::s_view* world, game::id_player player_idx, s_ship_design* design, int16_t hull_idx) {
        const auto legacy_player = legacy_world::Index(player_idx);
        if (!legacy_player) return;
        const auto& design_rules = rules;
        const auto& priorities = design_rules.priorities;
        int16_t ship_class;
        int32_t remaining_space;
        int16_t weapon_slot_idx = 0;
        int32_t current_budget;
        char temp_name[16];
        s_player* player = &world->players[legacy_world::Slot(player_idx)];

        ship_class = Get_Ship_Class(world, player_idx);

        if (hull_idx == SHIP_SIZE_DOOM_STAR && (player->tech_applications[TECH_APP_STELLAR_CONVERTER] == TECH_RESEARCH_STATUS_RESEARCHED)) {
            if (game_random::Random(4) < (uint32_t)(1 + design_rules.stellar_converter_chance)) {
                ship_class = 6;
            }
        }

        if (player->tech_applications[TECH_APP_BLACK_HOLE_GENERATOR] == TECH_RESEARCH_STATUS_RESEARCHED && hull_idx > SHIP_SIZE_HUGE) {
            if (game_random::Random(4) < (uint32_t)(1 + design_rules.black_hole_generator_chance)) {
                ship_class = 6;
            }
        }

        struct_::Clear_Structure(design, 99);

        if (hull_idx == SHIP_SIZE_DOOM_STAR) {
            design->picture_num = 0x2b; /* '+' */
        } else {
            design->picture_num = (int8_t)game_random::Random(8) + (int8_t)hull_idx * 8 - 1;
        }
        uint8_t player_race = player->race;
        DESIGN::Get_Ship_Design_Name(player_race, hull_idx, (int16_t)(design->picture_num % 8), design->name, sizeof(design->name));

        if (ship_class == 7) {
            KEN::Ken_Get_Text_Message(0x54, temp_name, 0xf);
            strlcpy(design->name, temp_name, sizeof(design->name));
        }

        design->ship_type = 0;
        design->size = (int8_t)hull_idx;
        design->shield_type = (int8_t)INITSHIP::Best_Ship_Shield(*legacy_player);
        design->ftl_type = (int8_t)INITSHIP::Best_Warp_Drive(*legacy_player);
        design->speed = static_cast<uint8_t>(
            INITSHIP::Get_FTL_Speed(*legacy_player, (int16_t)design->ftl_type));
        design->computer_type = (int8_t)INITSHIP::Best_Computer(*legacy_player);
        design->armor_type = (int8_t)INITSHIP::Best_Armor(*legacy_player);

        remaining_space = TECHDATA::hull_data[hull_idx].size;
        if (player->tech_applications[TECH_APP_MEGAFLUXERS] == TECH_RESEARCH_STATUS_RESEARCHED) {
            remaining_space = DESIGN::Apply_Megafluxers_Tactical_Bonus(remaining_space);
        }

        if (player->tech_applications[TECH_APP_BATTLE_PODS] == TECH_RESEARCH_STATUS_RESEARCHED) {
            remaining_space -= (int16_t)TECHDATA::specials[SPECIAL_BATTLE_PODS].space_per_ship_type[hull_idx];
            struct_::Set_Bit_Field(design->special_device_flags, SPECIAL_BATTLE_PODS);
        }

        remaining_space -= DESIGN::Shield_Space_Given_M_Level((int16_t)design->shield_type, (int16_t)design->size, *legacy_player);

        const int16_t table_class = (ship_class == 7) ? 0 : ship_class;
        const s_allocation_profile* table =
            &design_rules.allocations.profiles[table_class][hull_idx];
        current_budget = 0;

        switch (ship_class) {
            case 1:
                if (table->theme_special != -1) {
                    current_budget = (remaining_space * table->theme_special) / 100;
                    current_budget -= Add_Theme_Special_To_Design(world, player_idx, design, current_budget, &weapon_slot_idx);
                }
                if (table->defense_special != -1) {
                    current_budget += (remaining_space * table->defense_special) / 100;
                    const s_spcl* def_tbl = Best_Weapons_Are_Beams(&world->players[0]) ? priorities.defense_priority.human_beams : priorities.defense_priority.human_non_beams;
                    current_budget -= Add_Specials_To_Design(world, def_tbl, 19, player_idx, design, current_budget, &weapon_slot_idx);
                }
                if (table->fighters != -1) {
                    current_budget += (remaining_space * table->fighters) / 100;
                    current_budget -= Add_Fighters_To_Design(world, player_idx, design, &weapon_slot_idx, current_budget);
                }
                if (table->heavy_beams != -1) {
                    current_budget += (remaining_space * table->heavy_beams) / 100;
                    current_budget -= Add_Beam_Weapon_To_Design(world, player_idx, design, &weapon_slot_idx, current_budget, 2);
                }
                if (table->beams != -1) {
                    current_budget += (remaining_space * table->beams + 50) / 100;
                    current_budget -= Add_Beam_Weapon_To_Design(world, player_idx, design, &weapon_slot_idx, current_budget, 1);
                }
                break;

            case 2:
                if (table->theme_special != -1) {
                    current_budget = (remaining_space * table->theme_special) / 100;
                    current_budget -= Add_Theme_Special_To_Design(world, player_idx, design, current_budget, &weapon_slot_idx);
                }
                if (table->defense_special != -1) {
                    current_budget += (remaining_space * table->defense_special) / 100;
                    const s_spcl* def_tbl = Best_Weapons_Are_Beams(&world->players[0]) ? priorities.defense_priority.human_beams : priorities.defense_priority.human_non_beams;
                    current_budget -= Add_Specials_To_Design(world, def_tbl, 19, player_idx, design, current_budget, &weapon_slot_idx);
                }
                if (table->missile_weapon != -1) {
                    current_budget += (remaining_space * table->missile_weapon) / 100;
                    current_budget -= Add_Missile_Weapon_To_Design(world, player_idx, design, &weapon_slot_idx, current_budget, 5);
                }
                if (table->heavy_beams != -1) {
                    current_budget += (remaining_space * table->heavy_beams) / 100;
                    current_budget -= Add_Beam_Weapon_To_Design(world, player_idx, design, &weapon_slot_idx, current_budget, 2);
                }
                if (table->beams != -1) {
                    current_budget += (remaining_space * table->beams + 50) / 100;
                    current_budget -= Add_Beam_Weapon_To_Design(world, player_idx, design, &weapon_slot_idx, current_budget, 1);
                }
                break;

            case 3:
                if (table->theme_special != -1) {
                    current_budget = (remaining_space * table->theme_special) / 100;
                    current_budget -= Add_Theme_Special_To_Design(world, player_idx, design, current_budget, &weapon_slot_idx);
                }
                if (table->defense_special != -1) {
                    current_budget += (remaining_space * table->defense_special) / 100;
                    const s_spcl* def_tbl = Best_Weapons_Are_Beams(&world->players[0]) ? priorities.defense_priority.human_beams : priorities.defense_priority.human_non_beams;
                    current_budget -= Add_Specials_To_Design(world, def_tbl, 19, player_idx, design, current_budget, &weapon_slot_idx);
                }
                if (table->missile_special != -1) {
                    current_budget += (remaining_space * table->missile_special) / 100;
                    current_budget -= Add_Specials_To_Design(world, priorities.special_priority.missile_offense, 2, player_idx, design, current_budget, &weapon_slot_idx);
                }
                if (table->missile_weapon != -1) {
                    current_budget += (remaining_space * table->missile_weapon) / 100;
                    current_budget -= Add_Missile_Weapon_To_Design(world, player_idx, design, &weapon_slot_idx, current_budget, 10);
                }
                if (table->beams != -1) {
                    current_budget += (remaining_space * table->beams + 50) / 100;
                    current_budget -= Add_Beam_Weapon_To_Design(world, player_idx, design, &weapon_slot_idx, current_budget, 1);
                }
                if (table->point_defense_beams != -1) {
                    current_budget += (remaining_space * table->point_defense_beams + 50) / 100;
                    current_budget -= Add_Beam_Weapon_To_Design(world, player_idx, design, &weapon_slot_idx, current_budget, 0);
                }
                break;

            case 4:
                if (table->theme_special != -1) {
                    current_budget = (remaining_space * table->theme_special) / 100;
                    current_budget -= Add_Theme_Special_To_Design(world, player_idx, design, current_budget, &weapon_slot_idx);
                }
                if (table->defense_special != -1) {
                    current_budget += (remaining_space * table->defense_special) / 100;
                    const s_spcl* def_tbl = Best_Weapons_Are_Beams(&world->players[0]) ? priorities.defense_priority.human_beams : priorities.defense_priority.human_non_beams;
                    current_budget -= Add_Specials_To_Design(world, def_tbl, 19, player_idx, design, current_budget, &weapon_slot_idx);
                }
                if (table->fighters != -1) {
                    current_budget += (remaining_space * table->fighters) / 100;
                    current_budget -= Add_Fighters_To_Design(world, player_idx, design, &weapon_slot_idx, current_budget);
                }
                if (table->beams != -1) {
                    current_budget += (remaining_space * table->beams + 50) / 100;
                    current_budget -= Add_Beam_Weapon_To_Design(world, player_idx, design, &weapon_slot_idx, current_budget, 1);
                }
                if (table->point_defense_beams != -1) {
                    current_budget += (remaining_space * table->point_defense_beams + 50) / 100;
                    current_budget -= Add_Beam_Weapon_To_Design(world, player_idx, design, &weapon_slot_idx, current_budget, 0);
                }
                break;

            case 5:
                if (table->theme_special != -1) {
                    current_budget = (remaining_space * table->theme_special) / 100;
                    current_budget -= Add_Theme_Special_To_Design(world, player_idx, design, current_budget, &weapon_slot_idx);
                }
                if (table->special_device != -1) {
                    current_budget += (remaining_space * table->special_device) / 100;
                    current_budget -= Add_Specials_To_Design(world, priorities.special_device_priority, 12, player_idx, design, current_budget, &weapon_slot_idx);
                }
                if (table->defense_special != -1) {
                    current_budget += (remaining_space * table->defense_special) / 100;
                    const s_spcl* def_tbl = Best_Weapons_Are_Beams(&world->players[0]) ? priorities.defense_priority.human_beams : priorities.defense_priority.human_non_beams;
                    current_budget -= Add_Specials_To_Design(world, def_tbl, 19, player_idx, design, current_budget, &weapon_slot_idx);
                }
                if (table->heavy_beams != -1) {
                    current_budget += (remaining_space * table->heavy_beams) / 100;
                    current_budget -= Add_Beam_Weapon_To_Design(world, player_idx, design, &weapon_slot_idx, current_budget, 2);
                }
                if (table->beams != -1) {
                    current_budget += (remaining_space * table->beams + 50) / 100;
                    current_budget -= Add_Beam_Weapon_To_Design(world, player_idx, design, &weapon_slot_idx, current_budget, 1);
                }
                break;

            case 6:
                if (table->theme_special != -1) {
                    current_budget = (remaining_space * table->theme_special) / 100;
                    current_budget -= Add_Theme_Special_To_Design(world, player_idx, design, current_budget, &weapon_slot_idx);
                }
                if (table->defense_special != -1) {
                    current_budget += (remaining_space * table->defense_special) / 100;
                    const s_spcl* def_tbl = Best_Weapons_Are_Beams(&world->players[0]) ? priorities.defense_priority.human_beams : priorities.defense_priority.human_non_beams;
                    current_budget -= Add_Specials_To_Design(world, def_tbl, 19, player_idx, design, current_budget, &weapon_slot_idx);
                }
                if (table->special_weapon != -1) {
                    current_budget += (remaining_space * table->special_weapon) / 100;
                    current_budget -= Add_Specials_To_Design(world, priorities.special_priority.special_weapon, 5, player_idx, design, current_budget, &weapon_slot_idx);
                }
                if (table->beams != -1) {
                    current_budget += (remaining_space * table->beams + 50) / 100;
                    current_budget -= Add_Beam_Weapon_To_Design(world, player_idx, design, &weapon_slot_idx, current_budget, 1);
                }
                if (table->heavy_beams != -1) {
                    current_budget += (remaining_space * table->heavy_beams) / 100;
                    current_budget -= Add_Beam_Weapon_To_Design(world, player_idx, design, &weapon_slot_idx, current_budget, 2);
                }
                if (table->point_defense_beams != -1) {
                    current_budget += (remaining_space * table->point_defense_beams + 50) / 100;
                    current_budget -= Add_Beam_Weapon_To_Design(world, player_idx, design, &weapon_slot_idx, current_budget, 0);
                }
                break;

            case 7:
                if (player->tech_applications[TECH_APP_EXTENDED_FUEL_TANKS] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    struct_::Set_Bit_Field(design->special_device_flags, SPECIAL_EXTENDED_FUEL_TANKS);
                    int8_t cat = DESIGN::Miniaturization_Category(TECH_APP_EXTENDED_FUEL_TANKS);
                    int16_t m_level = DESIGN::Miniaturization_Level(*legacy_player, TECH_APP_EXTENDED_FUEL_TANKS);
                    int16_t fuel_tank_base_space = (int16_t)TECHDATA::specials[SPECIAL_EXTENDED_FUEL_TANKS].space_per_ship_type[hull_idx];
                    remaining_space -= DESIGN::Space_Given_M_Level(fuel_tank_base_space, m_level, cat);
                }
                /* fallthrough */
            case 0:
                if (table->theme_special != -1) {
                    current_budget = (remaining_space * table->theme_special) / 100;
                    current_budget -= Add_Theme_Special_To_Design(world, player_idx, design, current_budget, &weapon_slot_idx);
                }
                if (table->defense_special != -1) {
                    current_budget += (remaining_space * table->defense_special) / 100;
                    const s_spcl* def_tbl = Best_Weapons_Are_Beams(&world->players[0]) ? priorities.defense_priority.human_beams : priorities.defense_priority.human_non_beams;
                    current_budget -= Add_Specials_To_Design(world, def_tbl, 19, player_idx, design, current_budget, &weapon_slot_idx);
                }
                if (table->beam_special != -1) {
                    current_budget += (remaining_space * table->beam_special) / 100;
                    current_budget -= Add_Specials_To_Design(world, priorities.special_priority.beam_offense, 7, player_idx, design, current_budget, &weapon_slot_idx);
                }
                if (table->missile_weapon != -1) {
                    current_budget += (remaining_space * table->missile_weapon) / 100;
                    current_budget -= Add_Missile_Weapon_To_Design(world, player_idx, design, &weapon_slot_idx, current_budget, 2);
                }
                if (table->heavy_beams != -1) {
                    current_budget += (remaining_space * table->heavy_beams) / 100;
                    current_budget -= Add_Beam_Weapon_To_Design(world, player_idx, design, &weapon_slot_idx, current_budget, 2);
                }
                if (table->beams != -1) {
                    current_budget += (remaining_space * table->beams + 50) / 100;
                    current_budget -= Add_Beam_Weapon_To_Design(world, player_idx, design, &weapon_slot_idx, current_budget, 1);
                }
                break;
        }

        if (table->bombs != -1) {
            Add_Bomb_Weapon_To_Design(
                world, player_idx, design, &weapon_slot_idx, current_budget + (remaining_space * table->bombs) / 100);
        }

        design->previous_owner = *legacy_player;
        design->cost = DESIGN::Calculate_Design(*legacy_player, design, &design->combat_speed);
        design->date_of_design = (uint16_t)world->Stardate();
    }


} // namespace aidesign