#include "pch.h"
#include "game/ship_config.h"
#include "game/shipstak.h"

namespace INITSHIP {
    namespace {
        int16_t Weapon_Selection_Max_Damage(
            const s_tech_weapon_data& weapon, bool strategic_combat) {
            if (strategic_combat) {
                return weapon.strat_max_damage;
            }
            return weapon.max_damage;
        }

        int16_t Best_Beam_Weapon_For_Mode(
            int16_t player_idx, uint16_t required_mods, bool strategic_combat) {
            int16_t best_weapon_idx = WEAPON_LASER_CANNON;
            int16_t max_damage = 0;

            for (int16_t i = 0; i < 40; ++i) {
                const s_tech_weapon_data& weapon = TECHDATA::weapons[i];
                if (MOX::player[player_idx]
                            .tech_applications[weapon.tech_app_id] ==
                        TECH_RESEARCH_STATUS_RESEARCHED &&
                    (weapon.available_mods & required_mods) == required_mods &&
                    weapon.type == WEAPON_TYPE_BEAM) {
                    int16_t weapon_damage = Weapon_Selection_Max_Damage(
                        weapon, strategic_combat);
                    if (weapon_damage > max_damage) {
                        max_damage = weapon_damage;
                        best_weapon_idx = i;
                    }
                }
            }

            return best_weapon_idx;
        }

        int16_t Best_Missile_Weapon_For_Mode(
            int16_t player_idx, bool strategic_combat) {
            int16_t best_idx = WEAPON_NUCLEAR_MISSILE;
            int16_t max_damage = 0;

            for (int16_t i = 0; i < 40; ++i) {
                const s_tech_weapon_data& weapon = TECHDATA::weapons[i];
                if (MOX::player[player_idx]
                            .tech_applications[weapon.tech_app_id] ==
                        TECH_RESEARCH_STATUS_RESEARCHED &&
                    (weapon.type == WEAPON_TYPE_MISSILE ||
                        weapon.type == WEAPON_TYPE_TORPEDO)) {
                    int16_t weapon_damage = Weapon_Selection_Max_Damage(
                        weapon, strategic_combat);
                    if (weapon_damage > max_damage) {
                        best_idx = i;
                        max_damage = weapon_damage;
                    }
                }
            }

            return best_idx;
        }

        int16_t Best_Bomb_Weapon_For_Mode(
            int16_t player_idx, bool strategic_combat) {
            int16_t best_weapon_idx = WEAPON_NUCLEAR_BOMB;
            int16_t max_damage = 0;

            for (int16_t i = 0; i < 40; ++i) {
                const s_tech_weapon_data& weapon = TECHDATA::weapons[i];
                if (MOX::player[player_idx]
                            .tech_applications[weapon.tech_app_id] ==
                        TECH_RESEARCH_STATUS_RESEARCHED &&
                    weapon.type == WEAPON_TYPE_BOMB) {
                    int16_t weapon_damage = Weapon_Selection_Max_Damage(
                        weapon, strategic_combat);
                    if (weapon_damage > max_damage) {
                        best_weapon_idx = i;
                        max_damage = weapon_damage;
                    }
                }
            }

            return best_weapon_idx;
        }

        namespace stealth_landers {
            struct s_device_lookup {
                uint16_t tech_application;
                uint16_t special;
                const int16_t* bonus;
            };

            constexpr s_device_lookup device_lookup[] = {
                {TECH_APP_STEALTH_FIELD, SPECIAL_STEALTH_FIELD, &SHIPSTAK::stealth_bonus.stealth_field},
                {TECH_APP_CLOAKING_DEVICE, SPECIAL_CLOAKING_DEVICE, &SHIPSTAK::stealth_bonus.cloaking_device},
                {TECH_APP_PHASING_CLOAK, SPECIAL_PHASING_CLOAK, &SHIPSTAK::stealth_bonus.phasing_cloak},
            };
        } // namespace stealth_landers

        void Equip_Best_Stealth_Device(int16_t player_idx,
            s_ship_design* design) {
            int16_t best_bonus = INT16_MIN;
            uint16_t best_special = 0;

            for (const stealth_landers::s_device_lookup& device :
                stealth_landers::device_lookup) {
                if (MOX::player[player_idx]
                            .tech_applications[device.tech_application] ==
                        TECH_RESEARCH_STATUS_RESEARCHED &&
                    *device.bonus >= best_bonus) {
                    best_bonus = *device.bonus;
                    best_special = device.special;
                }
            }

            if (best_special != 0) {
                struct_::Set_Bit_Field(design->special_device_flags,
                    best_special);
            }
        }

        void Complete_Configured_Design(s_ship_design* design, const char* name, uint8_t owner, int16_t cost, uint8_t speed, int16_t combat_speed_drive, int16_t combat_speed_bonus) {
            strlcpy(design->name, name, sizeof(design->name));
            design->ship_type = SHIP_TYPE_COMBAT;
            design->previous_owner = owner;
            design->cost = cost;
            design->speed = speed;
            design->combat_speed = static_cast<uint8_t>(
                TECHDATA::drives[combat_speed_drive]
                    .combat_speed_min[design->size] +
                combat_speed_bonus);
        }

        int16_t Monster_Combat_Speed_Drive(const s_ship_design& design,
            uint8_t fallback_drive) {
            return design.ftl_type == DRIVE_NO_DRIVE ? fallback_drive : design.ftl_type;
        }

        void Load_Configured_Antaran_Design(s_ship_design* design, int16_t hull_size) {
            int16_t difficulty = MOX::settings.game_difficulty;
            if (difficulty < GAME_DIFFICULTY_TUTOR ||
                difficulty > GAME_DIFFICULTY_IMPOSSIBLE) {
                difficulty = GAME_DIFFICULTY_AVERAGE;
            }
            if (hull_size < SHIP_SIZE_SMALL || hull_size > SHIP_SIZE_TITAN) {
                return;
            }
            const int16_t hull = hull_size;
            *design = SHIP_CONFIG::antaran_offensive_templates[difficulty][hull];
            const char* names[] = {
                "Raider",
                "Marauder",
                global::s_Intruder_005576a0,
                "Interdictor",
                "Harbinger",
            };
            Complete_Configured_Design(design, names[hull], 8, 500, 1, design->ftl_type == DRIVE_NO_DRIVE ? Best_Warp_Drive(8) : design->ftl_type, 0);
        }

        int16_t Clamp_Combat_Hits(int64_t hits) {
            if (hits < 0) {
                return 0;
            }
            if (hits > INT16_MAX) {
                return INT16_MAX;
            }
            return static_cast<int16_t>(hits);
        }

        void Load_Configured_Antaran_Combat_Ship(int16_t combat_idx,
            int16_t hull_size) {
            int16_t difficulty = MOX::settings.game_difficulty;
            if (difficulty < GAME_DIFFICULTY_TUTOR ||
                difficulty > GAME_DIFFICULTY_IMPOSSIBLE) {
                difficulty = GAME_DIFFICULTY_AVERAGE;
            }

            s_combat_data* ship = &MOX::combat_data[combat_idx];
            struct_::Clear_Structure(ship, sizeof(*ship));

            const s_ship_design& design =
                SHIP_CONFIG::antaran_defensive_templates[difficulty][hull_size];
            const char* names[] = {
                "Raider",
                "Marauder",
                global::s_Intruder_005576a0,
                "Interdictor",
                "Harbinger",
            };

            strlcpy(ship->name, names[hull_size], sizeof(ship->name));
            ship->ship_idx = -1;
            ship->owner = 8;
            ship->previous_owner = 8;
            ship->size_class = design.size;
            ship->shield_type = design.shield_type;
            ship->ftl_type = design.ftl_type;
            ship->computer_type = design.computer_type;
            ship->armor_type = design.armor_type;
            ship->picture_num = design.picture_num;

            memcpy(ship->special_device_flags, design.special_device_flags, sizeof(ship->special_device_flags));

            for (int i = 0; i < 8; ++i) {
                const s_ship_weapons& source = design.ship_weapon[i];
                s_combat_weapon_slot& destination = ship->weapons[i];
                destination.weapon_id = source.type;
                destination.count = source.current_count;
                destination.arc = source.firing_arc == WEAPON_FIRING_ARC_360 ? WEAPON_FIRING_ARC_ALL_SECTORS : source.firing_arc;
                destination.specials = source.specials;
                destination.ammo = source.ammo;
                destination.active = source.current_count > 0 ? 1 : 0;
                destination.shots_left = source.current_count;
            }

            ship->crew_quality = CREW_QUALITY_ELITE;
            ship->officer_idx = -1;
            int64_t marine_count =
                TECHDATA::hull_data[ship->size_class].marines;
            if (struct_::Test_Bit_Field(ship->special_device_flags,
                    SPECIAL_TROOP_PODS)) {
                marine_count *= 2;
            }
            if (marine_count > UINT8_MAX) {
                marine_count = UINT8_MAX;
            }
            ship->marine_count = static_cast<uint8_t>(marine_count);

            const int64_t armor_factor =
                TECHDATA::armor[ship->armor_type].ships_bonus + 100;
            int64_t structure = armor_factor *
                TECHDATA::hull_data[ship->size_class].structure_hp / 100;
            if (struct_::Test_Bit_Field(ship->special_device_flags,
                    SPECIAL_REINFORCED_HULL)) {
                structure *= 3;
            }
            ship->structure_max = Clamp_Combat_Hits(structure);

            int64_t armor = armor_factor *
                TECHDATA::hull_data[ship->size_class].armor_hp / 100;
            if (struct_::Test_Bit_Field(ship->special_device_flags,
                    SPECIAL_HEAVY_ARMOR)) {
                armor *= 3;
            }
            ship->armor_remaining = Clamp_Combat_Hits(armor);

            ship->combat_speed_base =
                TECHDATA::drives[ship->ftl_type].combat_speed_min[ship->size_class];
            CMBTFIR2::Init_Internal_Space(ship);
            ship->shield_arc_max = COMBINIT::Calc_Combat_Shield_Max(combat_idx);
            for (int i = 0; i < 4; ++i) {
                ship->shield_arc_current[i] = ship->shield_arc_max;
            }
        }
    } // namespace

    void __cdecl Init_Ship_Designs(int16_t player_idx) {
        s_player* plr = &MOX::player[player_idx];
        int16_t design_flag = 0;

        if (MOX::settings.starting_civilization_level !=
                STARTING_CIVILIZATION_LEVEL_ADVANCED &&
            plr->objectives == PLAYER_OBJECTIVE_HUMAN) {
            design_flag = 1;
        }

        if (MOX::settings.strategic_combat_flag == 1) {
            for (int16_t i = 0; i < SHIP_SIZE_COUNT; ++i) {
                struct_::Clear_Structure(&plr->ship_designs[i], sizeof(plr->ship_designs[i]));
                Auto_Strategic_Design_Ship(player_idx, i, &plr->ship_designs[i]);
            }
            return;
        }

        MOX::cur_combat_x = -1000;
        aidesign::Auto_Design_Ship(player_idx, &plr->ship_designs[0], 0);
        MOX::cur_combat_x = 0;

        for (int16_t i = 1; i < SHIP_SIZE_COUNT; ++i) {
            aidesign::Auto_Design_Ship(player_idx, &plr->ship_designs[i], i - design_flag);
        }

        if (plr->objectives != PLAYER_OBJECTIVE_HUMAN) {
            aidesign::Auto_Design_Ship(player_idx, &plr->ship_designs[SHIP_SIZE_DOOM_STAR], HULL_DOOM_STAR);

            int16_t pic_mod_8 = (int16_t)(plr->ship_designs[SHIP_SIZE_DOOM_STAR].picture_num % 8);
            DESIGN::Get_Ship_Design_Name(
                (int16_t)plr->race,
                (int16_t)plr->ship_designs[SHIP_SIZE_DOOM_STAR].size,
                pic_mod_8,
                plr->ship_designs[SHIP_SIZE_DOOM_STAR].name,
                sizeof(plr->ship_designs[SHIP_SIZE_DOOM_STAR].name));
        }
    }

    void __cdecl Update_Player_Strategic_Ship_Designs(int16_t player_idx) {
        s_player* player = &MOX::player[player_idx];

        for (int16_t i = 0; i < SHIP_SIZE_COUNT; ++i) {
            s_ship_design* design = &player->ship_designs[i];
            int8_t old_picture_num = design->picture_num;

            struct_::Clear_Structure(design, sizeof(s_ship_design));
            Auto_Strategic_Design_Ship(player_idx, i, design);

            int8_t new_picture_num;
            if (i == SHIP_SIZE_DOOM_STAR) {
                new_picture_num = 0x2b; // '+'
            } else {
                new_picture_num = old_picture_num + 1;
                // Each design slot has 8 associated picture indices
                if (new_picture_num == (i * 8 + 8)) {
                    new_picture_num = (int8_t)(i * 8);
                }
            }
            design->picture_num = new_picture_num;
        }
    }

    void __cdecl Repair_Ship_Full(int16_t ship_idx) {
        s_ship_data* ship = &MOX::ship[ship_idx];

        ship->structural_damage = 0;
        ship->drive_damage_percent = 0;
        ship->shield_damage_percent = 0;
        ship->computer_damage = 0;
        ship->armor_damage = 0;

        for (int i = 0; i < 8; ++i) {
            ship->d.ship_weapon[i].current_count = ship->d.ship_weapon[i].count;
        }

        for (int i = 0; i < 5; ++i) {
            ship->special_device_damage_flags[i] = 0;
        }
    }

    void __cdecl Update_Player_Ship_Designs(int16_t player_idx) {
        if (MOX::settings.strategic_combat_flag == 1) {
            Update_Player_Strategic_Ship_Designs(player_idx);
            return;
        }

        if (MOX::player[player_idx].objectives == PLAYER_OBJECTIVE_HUMAN) {
            Update_Automatic_Design_Equipment(player_idx);
            Update_Equipment_On_Ships_Being_Built(player_idx);
            return;
        }

        Update_Equipment_On_Ships_Being_Built(player_idx);

        for (int16_t i = 0; i < SHIP_SIZE_COUNT; ++i) {
            aidesign::Auto_Design_Ship(player_idx, &MOX::player[player_idx].ship_designs[i], i);
        }
    }

    int16_t __cdecl Best_Ship_Fuel(int16_t player_idx) {
        if (player_idx >= MAX_PLAYERS) {
            return FUEL_THORIUM_FUEL_CELLS;
        }

        int16_t best_fuel_idx = 0;
        int16_t best_range = TECHDATA::fuel[FUEL_NO_FUEL_CELLS].ship_range;

        for (int16_t i = 0; i < FUEL_COUNT; ++i) {
            // Check if player has researched this fuel technology (application value 3)
            // and if it provides a better range than the current best.
            if (MOX::player[player_idx].tech_applications[TECHDATA::fuel[i].tech_app_id] == TECH_RESEARCH_STATUS_RESEARCHED) {
                if (TECHDATA::fuel[i].ship_range > best_range) {
                    best_range = TECHDATA::fuel[i].ship_range;
                    best_fuel_idx = i;
                }
            }
        }

        return best_fuel_idx;
    }

    int16_t __cdecl Best_Warp_Drive(int16_t player_idx) {
        if (player_idx >= MAX_PLAYERS) {
            // Handle special players (Guardians, Antarans, etc.)
            switch (player_idx) {
                case 8: // Antarans
                    return DRIVE_INTERPHASED_DRIVE;
                case 12:
                    return SHIP_CONFIG::monster.drives.dragon;
                case 13:
                    return SHIP_CONFIG::monster.drives.eel;
                case 9:
                    return SHIP_CONFIG::monster.drives.guardian;
                case 11:
                    return SHIP_CONFIG::monster.drives.crystal;
                case 10:
                    return SHIP_CONFIG::monster.drives.amoeba;
                case 14:
                    return SHIP_CONFIG::monster.drives.hydra;
                default:
                    return DRIVE_NO_DRIVE;
            }
        }

        int16_t best_warp_index = DRIVE_NO_DRIVE;
        // Search through warp drive techs 1-6 (Nuclear to Interdimensional)
        for (int16_t i = DRIVE_NUCLEAR_DRIVE; i < DRIVE_COUNT; ++i) {
            int16_t app_idx = TECHDATA::drives[i].tech_app_id;
            // Status 3 indicates that the technology has been researched
            if (MOX::player[player_idx].tech_applications[app_idx] == TECH_RESEARCH_STATUS_RESEARCHED) {
                best_warp_index = i;
            }
        }

        return best_warp_index;
    }

    int16_t __cdecl Calc_Player_FTL_Speed(int16_t player_idx) {
        int16_t speed = 1;

        if (player_idx < MAX_PLAYERS) {
            int16_t drive_idx = Best_Warp_Drive(player_idx);
            speed = TECHDATA::drives[drive_idx].warp_speed;

            if (MOX::player[player_idx].traits[TRAIT_TRANS_DIMENSIONAL] != 0) {
                speed += td_ftl_bonus;
            }
        }

        return speed;
    }

    int16_t __cdecl Get_FTL_Speed(int16_t player_idx, int16_t drive_idx) {
        int16_t speed = 0;

        if (player_idx < MAX_PLAYERS) {
            speed = TECHDATA::drives[drive_idx].warp_speed;

            if (MOX::player[player_idx].traits[TRAIT_TRANS_DIMENSIONAL] != 0) {
                return speed + td_ftl_bonus;
            }
        } else if (drive_idx > 0) {
            speed = 1;
        }

        return speed;
    }

    void __cdecl Update_Equipment_On_Ships_Being_Built(int16_t player_idx) {
        if (MOX::player[player_idx].eliminated != 0) {
            return;
        }

        int16_t best_armor = Best_Armor(player_idx);
        int16_t best_ftl = Best_Warp_Drive(player_idx);

        for (int16_t ship_idx = 0; ship_idx < MOX::NUM_SHIPS; ++ship_idx) {
            s_ship_data* ship = &MOX::ship[ship_idx];

            if (ship->owner == static_cast<int8_t>(player_idx) && (ship->status == 4 || ship->status == 6)) {
                if (static_cast<int16_t>(ship->d.ftl_type) != best_ftl) {
                    int16_t old_ftl = static_cast<int16_t>(ship->d.ftl_type);
                    int8_t ship_size = ship->d.size;

                    uint8_t old_bonus = TECHDATA::drives[old_ftl].combat_speed_min[ship_size];

                    ship->d.ftl_type = static_cast<int8_t>(best_ftl);
                    ship->d.speed = static_cast<uint8_t>(Get_FTL_Speed(player_idx, best_ftl));

                    uint8_t new_bonus = TECHDATA::drives[best_ftl].combat_speed_min[ship_size];
                    ship->d.combat_speed = (int8_t)((uint8_t)ship->d.combat_speed - old_bonus + new_bonus);
                }

                ship->d.armor_type = static_cast<int8_t>(best_armor);
            }
        }
    }

    void __cdecl Update_Automatic_Design_Equipment(int16_t player_idx) {
        int16_t best_armor_type = Best_Armor(player_idx);
        int16_t best_ftl_type = Best_Warp_Drive(player_idx);

        s_player* player = &MOX::player[player_idx];

        for (int design_idx = 0; design_idx < SHIP_SIZE_COUNT; ++design_idx) {
            s_ship_design* design = &player->ship_designs[design_idx];

            design->armor_type = static_cast<int8_t>(best_armor_type);

            if (static_cast<int16_t>(design->ftl_type) != best_ftl_type) {
                int8_t old_ftl = design->ftl_type;
                int8_t ship_size = design->size;

                // Adjust combat speed based on the bonus difference between old and new drives
                uint8_t old_bonus = TECHDATA::drives[old_ftl].combat_speed_min[ship_size];

                design->ftl_type = static_cast<int8_t>(best_ftl_type);
                design->speed = static_cast<uint8_t>(Get_FTL_Speed(player_idx, best_ftl_type));

                uint8_t new_bonus = TECHDATA::drives[best_ftl_type].combat_speed_min[ship_size];
                design->combat_speed = (int8_t)((uint8_t)design->combat_speed - old_bonus + new_bonus);
            }

            design->cost = DESIGN::Cost_Of_Design(player_idx, design);
        }
    }

    int16_t __cdecl Best_Armor(int16_t player_idx) {
        if (player_idx >= MAX_PLAYERS) {
            // Antarans or Space Monsters
            if (player_idx == 8 || player_idx == 9) {
                return ARMOR_XENTRONIUM_ARMOR;
            }
            if (player_idx >= 10 && player_idx <= 14) {
                return ARMOR_NO_ARMOR;
            }
            return ARMOR_TITANIUM_ARMOR;
        }

        int16_t best_armor_idx = ARMOR_TITANIUM_ARMOR;

        // Check for Tritanium (2) through Xentronium (6)
        for (int16_t i = ARMOR_TRITANIUM_ARMOR; i < ARMOR_COUNT; ++i) {
            int16_t tech_id = TECHDATA::armor[i].tech_app_id;

            // 3 indicates the tech application is fully researched/available
            if (MOX::player[player_idx].tech_applications[tech_id] == TECH_RESEARCH_STATUS_RESEARCHED) {
                best_armor_idx = i;
            }
        }

        return best_armor_idx;
    }

    int16_t __cdecl Best_Computer(int16_t player_idx) {
        if (player_idx >= MAX_PLAYERS) {
            return COMPUTER_MOLECULARTRONIC_COMPUTER;
        }

        int16_t best_computer_idx = COMPUTER_NO_COMPUTER;

        for (int16_t i = COMPUTER_ELECTRONIC_COMPUTER; i < COMPUTER_COUNT; ++i) {
            if (MOX::player[player_idx].tech_applications[TECHDATA::computers[i].tech_app_id] == TECH_RESEARCH_STATUS_RESEARCHED) {
                best_computer_idx = i;
            }
        }

        return best_computer_idx;
    }

    int16_t __cdecl Best_Ship_Shield(int16_t player_idx) {
        int16_t best_shield_idx = SHIELD_NO_SHIELD;

        if (player_idx < MAX_PLAYERS) {
            for (int16_t i = SHIELD_CLASS_I_SHIELD; i < SHIELD_COUNT; ++i) {
                if (MOX::player[player_idx].tech_applications[TECHDATA::shields[i].tech_app_id] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    best_shield_idx = i;
                }
            }
            return best_shield_idx;
        }

        if (player_idx == 8) {
            return 7;
        }

        if (player_idx == 9) {
            return SHIELD_CLASS_V_SHIELD;
        }

        return SHIELD_NO_SHIELD;
    }

    int16_t __cdecl Best_Shield_Special(int16_t player_idx) {
        if (player_idx < MAX_PLAYERS) {
            if (MOX::player[player_idx].tech_applications[TECH_APP_HARD_SHIELDS] == TECH_RESEARCH_STATUS_RESEARCHED) {
                return SPECIAL_HARD_SHIELDS;
            }
        }
        return 0;
    }

    int16_t __cdecl Get_Structure(int16_t hull_index, bool is_fort, int16_t player_idx) {
        int16_t armor_idx = Best_Armor(player_idx);

        int16_t base_structure = TECHDATA::hull_data[hull_index].structure_hp;
        int16_t absorption_pct = TECHDATA::armor[armor_idx].ships_bonus;

        // Formula: base_structure + (absorption_pct * base_structure) / 100
        int32_t bonus = ((int32_t)absorption_pct * (int32_t)base_structure) / 100;
        int16_t total_structure = base_structure + (int16_t)bonus;

        if (is_fort) {
            total_structure *= 3;
        }

        return total_structure;
    }

    void __cdecl Auto_Strategic_Design_Ship(int16_t player_idx, int16_t design_idx, s_ship_design* design) {
        // Copy the default name for this hull type (e.g., "Frigate")
        strlcpy(design->name, TECHDATA::hull_data[design_idx].name, sizeof(design->name));

        design->size = static_cast<int8_t>(design_idx);
        design->ship_type = SHIP_TYPE_COMBAT;
        design->ftl_type = static_cast<int8_t>(Best_Warp_Drive(player_idx));
        design->shield_type = static_cast<int8_t>(Best_Ship_Shield(player_idx));
        design->computer_type = static_cast<int8_t>(Best_Computer(player_idx));
        design->armor_type = static_cast<int8_t>(Best_Armor(player_idx));

        // Calculate the build cost including government bonuses/penalties
        design->cost = Cost_Of_Strategic_Design(player_idx, design);

        // Fill weapons and special systems
        Update_Strategic_Design_Weapons_Specials(design, player_idx, design_idx);

        // Assign visual representation
        if (design_idx == HULL_DOOM_STAR) {
            design->picture_num = 0x2B; /* '+' sign for starbases */
        } else {
            design->picture_num = static_cast<int8_t>(design_idx) << 3;
        }

        design->previous_owner = (uint8_t)player_idx;
        design->speed = static_cast<uint8_t>(Get_FTL_Speed(player_idx, static_cast<int16_t>(design->ftl_type)));
        design->date_of_design = static_cast<uint16_t>(MOX::stardate);
    }

    void __cdecl Load_Small_Antaran_Design(s_ship_design* design) {
        Load_Configured_Antaran_Design(design, SHIP_SIZE_SMALL);
    }

    void __cdecl Load_Medium_Antaran_Design(s_ship_design* design) {
        Load_Configured_Antaran_Design(design, SHIP_SIZE_MEDIUM);
    }

    void __cdecl Load_Large_Antaran_Design(s_ship_design* design) {
        Load_Configured_Antaran_Design(design, SHIP_SIZE_LARGE);
    }

    void __cdecl Load_Huge_Antaran_Design(s_ship_design* design) {
        Load_Configured_Antaran_Design(design, SHIP_SIZE_HUGE);
    }

    void __cdecl Load_Titan_Antaran_Design(s_ship_design* design) {
        Load_Configured_Antaran_Design(design, SHIP_SIZE_TITAN);
    }

    void __cdecl Load_Antaran_Ship_Design(s_ship_design* design, int16_t hull_size) {
        Load_Configured_Antaran_Design(design, hull_size);
    }

    void __cdecl Load_Colony_Ship_Design(int16_t player_idx, s_ship_design* design) {
        struct_::Clear_Structure(design, sizeof(*design));

        const char* localized_name = MOX::ship_type_names[SHIP_TYPE_COLONY][(uint8_t)MOX::settings.language];
        strlcpy(design->name, localized_name, sizeof(design->name));

        design->size = landers_size_class;
        design->ship_type = SHIP_TYPE_COLONY;
        design->shield_type = SHIELD_NO_SHIELD;

        design->ftl_type = (int8_t)Best_Warp_Drive(player_idx);
        design->computer_type = COMPUTER_NO_COMPUTER;

        design->armor_type = (int8_t)Best_Armor(player_idx);

        struct_::Set_Bit_Field(design->special_device_flags, SPECIAL_EXTENDED_FUEL_TANKS);
        if (lander_ship_uses_stealth.colony_ship != 0) {
            Equip_Best_Stealth_Device(player_idx, design);
        }
        design->picture_num = 45;

        design->speed = static_cast<uint8_t>(TECHDATA::drives[design->ftl_type].warp_speed);
        design->cost = (int16_t)COLCALC::Ship_Type_Cost_For_Player(SHIP_TYPE_COLONY, player_idx);

        design->combat_speed = TECHDATA::drives[design->ftl_type].combat_speed_min[design->size];
    }

    void __cdecl Load_Outpost_Ship_Design(int16_t player_idx, s_ship_design* design) {
        struct_::Clear_Structure(design, sizeof(*design));

        strlcpy(design->name, MOX::ship_type_names[SHIP_TYPE_OUTPOST][(uint8_t)MOX::settings.language], sizeof(design->name));

        design->size = landers_size_class;
        design->ship_type = SHIP_TYPE_OUTPOST;
        design->shield_type = SHIELD_NO_SHIELD;
        design->ftl_type = static_cast<int8_t>(Best_Warp_Drive(player_idx));
        design->computer_type = COMPUTER_NO_COMPUTER;

        design->armor_type = (int8_t)Best_Armor(player_idx);

        struct_::Set_Bit_Field(design->special_device_flags, SPECIAL_EXTENDED_FUEL_TANKS);
        if (lander_ship_uses_stealth.outpost_ship != 0) {
            Equip_Best_Stealth_Device(player_idx, design);
        }

        design->picture_num = 46 /* '.' */;

        design->speed = static_cast<uint8_t>(TECHDATA::drives[design->ftl_type].warp_speed);
        design->combat_speed = static_cast<int8_t>(TECHDATA::drives[design->ftl_type].combat_speed_min[design->size]);

        design->cost = static_cast<int16_t>(COLCALC::Ship_Type_Cost_For_Player(SHIP_TYPE_OUTPOST, player_idx));
    }

    void __cdecl Load_Doom_Star_Ship_Design(
        int16_t owner_idx, s_ship_design* design, bool strategic_combat) {
        if (strategic_combat) {
            struct_::Clear_Structure(design, sizeof(*design));
            Auto_Strategic_Design_Ship(owner_idx, HULL_DOOM_STAR, design);
            return;
        }

        aidesign::Auto_Design_Ship(owner_idx, design, HULL_DOOM_STAR);
        strlcpy(design->name, TECHDATA::hull_data[HULL_DOOM_STAR].name, sizeof(design->name));
    }

    void __cdecl Repair_Ships_At_Colonies() {
        int16_t best_warp_drives[MAX_PLAYERS];

        for (int16_t i = 0; i < MAX_PLAYERS; ++i) {
            best_warp_drives[i] = Best_Warp_Drive(i);
        }

        if (MOX::NUM_SHIPS > 0) {
            for (int16_t i = 0; i < MOX::NUM_SHIPS; ++i) {
                s_ship_data& ship = MOX::ship[i];

                if (ship.owner < MAX_PLAYERS && ship.status == 0) {
                    int16_t best_ftl = best_warp_drives[(uint8_t)ship.owner];
                    if (best_ftl != (int16_t)ship.d.ftl_type) {
                        uint8_t old_modifier = TECHDATA::drives[ship.d.ftl_type].combat_speed_min[(uint8_t)ship.d.size];
                        ship.d.ftl_type = (int8_t)best_ftl;
                        uint8_t new_modifier = TECHDATA::drives[ship.d.ftl_type].combat_speed_min[(uint8_t)ship.d.size];

                        ship.d.combat_speed = (int8_t)((uint8_t)ship.d.combat_speed - old_modifier +
                            new_modifier);
                    }
                }

                if (ship.owner > 8) {
                    Repair_Ship_Full(i);
                } else {
                    if (ship.location >= 0 && ship.location < MAX_STARS &&
                        ship.status == 0 && ship.d.ship_type == SHIP_TYPE_COMBAT) {
                        if (struct_::Test_Bit_Field(&MOX::star[ship.location].has_colony, (int16_t)ship.owner)) {
                            Repair_Ship_Full(i);
                        }
                    }
                }
            }
        }
    }

    int16_t __cdecl Get_Ship_Armor_Hits(int16_t ship_idx) {
        s_ship_data* ship = &MOX::ship[ship_idx];

        int16_t armor_absorption_pct = TECHDATA::armor[(int16_t)ship->d.armor_type].ships_bonus;
        int16_t base_armor_hp = TECHDATA::hull_data[(int16_t)ship->d.size].armor_hp;

        int16_t has_special_armor = (int16_t)struct_::Test_Bit_Field(ship->d.special_device_flags, SPECIAL_HEAVY_ARMOR);

        int16_t total_hits = (int16_t)(((int32_t)armor_absorption_pct * base_armor_hp) / 100) + base_armor_hp;

        if (has_special_armor) {
            total_hits *= 3;
        }

        return total_hits;
    }

    int16_t __cdecl Get_Ship_Structure(int16_t ship_idx) {
        int16_t result;
        s_ship_data* ship = &MOX::ship[ship_idx];
        const uint8_t builder = ship->d.previous_owner;
        const SHIP_CONFIG::s_monster_hit_points& hit_points = SHIP_CONFIG::monster.hit_points;

        if (builder <= 9) {
            return Get_Design_Structure(ship->d);
        }

        switch (builder) {
            case 10:
                result = SHIP_CONFIG::monster.hit_points.event_amoeba;
                if (ship->d.ftl_type <= 0) {
                    return SHIP_CONFIG::monster.hit_points.system_amoeba;
                }
                break;

            case 11:
                result = hit_points.event_crystal >= 0 ? hit_points.event_crystal : hit_points.event_crystal_dragon;
                if (ship->d.ftl_type <= 0) {
                    return hit_points.system_crystal >= 0 ? hit_points.system_crystal : hit_points.system_crystal_hydra_dragon;
                }
                break;

            case 12:
                result = hit_points.event_dragon >= 0 ? hit_points.event_dragon : hit_points.event_crystal_dragon;
                if (ship->d.ftl_type <= 0) {
                    return hit_points.system_dragon >= 0 ? hit_points.system_dragon : hit_points.system_crystal_hydra_dragon;
                }
                break;

            case 13:
                result = SHIP_CONFIG::monster.hit_points.event_eel;
                if (ship->d.ftl_type <= 0) {
                    return SHIP_CONFIG::monster.hit_points.system_eel;
                }
                break;

            case 14:
                result = SHIP_CONFIG::monster.hit_points.event_hydra;
                if (ship->d.ftl_type <= 0) {
                    return hit_points.system_hydra >= 0 ? hit_points.system_hydra : hit_points.system_crystal_hydra_dragon;
                }
                break;

            default:
                break;
        }

        return result;
    }

    void __cdecl Get_Ship_Combat_Bonuses(int16_t ship_idx, int16_t* ship_defense, int16_t* ship_missile_defense, int16_t* ship_attack) {
        int16_t defense_bonus = 0;
        int16_t missile_defense_bonus = 0;
        int16_t attack_bonus = 0;

        s_ship_data* ship = &MOX::ship[ship_idx];
        s_ship_design design_copy;
        memcpy(&design_copy, &ship->d, sizeof(s_ship_design));

        Get_Design_Combat_Bonuses(design_copy, &attack_bonus, &defense_bonus, &missile_defense_bonus);

        if (ship->officer_index > -1) {
            int16_t helmsman_bonus = CMBTFIR2::Helmsman_Bonus(ship->officer_index);
            defense_bonus += helmsman_bonus;
            missile_defense_bonus += (int16_t)(helmsman_bonus / 2);

            attack_bonus += CMBTFIR2::Weaponry_Bonus(ship->officer_index);
        }

        int16_t crew_def_bonus = MOX::crew_data[(int)ship->crew_quality].defense;
        defense_bonus += crew_def_bonus;
        missile_defense_bonus += (int16_t)(crew_def_bonus / 2);
        attack_bonus += MOX::crew_data[(int)ship->crew_quality].attack;

        if (ship->owner < MAX_PLAYERS) {
            s_player* owner_player = &MOX::player[(int)ship->owner];

            int16_t trait_def_bonus = (int16_t)owner_player->traits[TRAIT_SHIP_DEFENSE];
            defense_bonus += trait_def_bonus;
            missile_defense_bonus += trait_def_bonus;

            attack_bonus += (int16_t)owner_player->traits[TRAIT_SHIP_ATTACK];

            if (ship->d.ship_type != SHIP_TYPE_COMBAT) {
                defense_bonus = 0;
                missile_defense_bonus = 0;
            } else if (MOX::settings.strategic_combat_flag != 0) {
                defense_bonus +=
                    (Best_Warp_Drive(ship->owner) -
                        DRIVE_NUCLEAR_DRIVE) *
                        10 +
                    TECHDATA::hull_data[ship->d.size].strat_def_bonus;
            } else if (owner_player->traits[TRAIT_TRANS_DIMENSIONAL] != 0) {
                defense_bonus += COMBAT1::td_combat_speed_bonus * 5;
            }
        }

        *ship_defense = defense_bonus;
        *ship_missile_defense = missile_defense_bonus;
        *ship_attack = attack_bonus;
    }

    int16_t __cdecl Cost_Of_Strategic_Design(int16_t govt_type, s_ship_design* design) {
        int32_t hull_cost = (int16_t)TECHDATA::hull_data[(int16_t)design->size].cost;
        int32_t reduced_cost = (hull_cost * 3) / 2;

        DESIGN::Cost_Reduction_For_Govt_Type(govt_type, &reduced_cost);

        return (int16_t)reduced_cost;
    }

    int16_t __cdecl Best_Beam_Weapon(
        int16_t player_idx, uint16_t required_mods) {
        // Special handling for non-standard players (monsters, Antarans, etc.)
        if (player_idx >= MAX_PLAYERS) {
            switch (player_idx) {
                case 8:
                    return 11;
                case 9:
                    return 4;
                case 10:
                    return 1;
                case 11:
                    return 42;
                case 12:
                    return 9;
                case 13:
                    return 1;
                case 14:
                    return 43;
                default:
                    // Fallback to default weapon index
                    return 3;
            }
        }

        return Best_Beam_Weapon_For_Mode(
            player_idx, required_mods, MOX::settings.strategic_combat_flag == 1);
    }

    int16_t __cdecl Best_Missile_Weapon(int16_t player_idx) {
        if (player_idx < MAX_PLAYERS) {
            return Best_Missile_Weapon_For_Mode(
                player_idx, MOX::settings.strategic_combat_flag == 1);
        }

        // Special handling for predefined entities or specific player indices >= 8
        switch (player_idx) {
            case 8:
                return 17; /* 0x11 */
            case 9:
                return 20; /* 0x14 */
            case 10:
                return 17; /* 0x11 */
            case 11:
                return 17; /* 0x11 */
            case 12:
                return 40; /* 0x28 */
            case 13:
                return 17; /* 0x11 */
            case 14:
                return 17; /* 0x11 */
            default:
                return 14; /* 0x0E */
        }
    }

    int16_t __cdecl Best_Point_Defense_Weapon_By_Size(int16_t player_idx) {
        int16_t best_dmg_per_space = 0;
        int16_t best_weapon_idx = 3;

        if (player_idx < MAX_PLAYERS) {
            for (int16_t i = 0; i < 40; ++i) {
                const s_tech_weapon_data& weapon = TECHDATA::weapons[i];

                if (MOX::player[player_idx].tech_applications[weapon.tech_app_id] == TECH_RESEARCH_STATUS_RESEARCHED &&
                    (weapon.available_mods & 0x4 /* WPN_SPECIAL_POINT_DEFENSE */) != 0 &&
                    weapon.type == WEAPON_TYPE_BEAM) {
                    int16_t mini_level = DESIGN::Miniaturization_Level(player_idx, weapon.tech_app_id);

                    if (mini_level >= (int16_t)TECHDATA::weapon_modifications[WEAPON_MOD_POINT_DEFENSE].level) {
                        int16_t current_dmg = Point_Defense_Damage_Per_Space(i, player_idx);

                        if (current_dmg >= best_dmg_per_space) {
                            best_dmg_per_space = current_dmg;
                            best_weapon_idx = i;
                        }
                    }
                }
            }
            return best_weapon_idx;
        }

        return 4;
    }

    int16_t __cdecl Point_Defense_Damage_Per_Space(int16_t weapon_idx, int16_t player_idx) {
        int16_t damage = TECHDATA::weapons[weapon_idx].max_damage;
        int32_t space = DESIGN::Weapon_Space(player_idx, weapon_idx, 1, 1, 1, 4);
        int32_t scaled_damage = (int32_t)damage * 100;
        return (int16_t)(scaled_damage / space) + damage;
    }

    int16_t __cdecl Get_Design_Structure(s_ship_design d) {
        int16_t has_special = (int16_t)struct_::Test_Bit_Field(d.special_device_flags, SPECIAL_REINFORCED_HULL);
        int16_t hull_idx = (int16_t)d.size;
        int16_t absorption = TECHDATA::armor[(int16_t)d.armor_type].ships_bonus;

        if (MOX::settings.strategic_combat_flag != 0) {
            return TECHDATA::hull_data[hull_idx].strat_hits;
        }

        int16_t base_structure = TECHDATA::hull_data[hull_idx].structure_hp;

        // Calculate structure bonus from armor
        int32_t total_structure = base_structure + (absorption * base_structure) / 100;

        if (has_special != 0) {
            // Reinforced Hull triples the structure
            total_structure *= 3;
        }

        return (short)total_structure;
    }

    int16_t __cdecl Best_Missile_Weapon_NT(int16_t player_idx) {
        if (satellite_uses_torpedoes != 0) {
            return Best_Missile_Weapon(player_idx);
        }

        int16_t best_weapon_idx = 14;
        int16_t max_damage = 0;

        if (player_idx < MAX_PLAYERS) {
            for (int i = 0; i < 40; ++i) {
                const auto& weapon = TECHDATA::weapons[i];
                if (MOX::player[player_idx].tech_applications[weapon.tech_app_id] == TECH_RESEARCH_STATUS_RESEARCHED &&
                    weapon.type == WEAPON_TYPE_MISSILE) {
                    int16_t weapon_damage = Weapon_Selection_Max_Damage(
                        weapon, MOX::settings.strategic_combat_flag == 1);
                    if (weapon_damage > max_damage) {
                        best_weapon_idx = (int16_t)i;
                        max_damage = weapon_damage;
                    }
                }
            }
            return best_weapon_idx;
        } else {
            switch (player_idx) {
                case 8: return 17;
                case 9: return 20;
                case 10: return 17;
                case 11: return 17;
                case 12: return 40;
                case 13: return 17;
                case 14: return 17;
                default: return 14;
            }
        }
    }

    int16_t __cdecl Best_Strat_Special_Weapon(int16_t player_idx, int16_t size) {
        if (player_idx >= MAX_PLAYERS) {
            switch (player_idx) {
                case 8:
                    return 37 /* 0x25 */;
                case 10:
                    return 45 /* 0x2d */;
                case 13:
                    return 44 /* 0x2c */;
                case 9:
                case 11:
                case 12:
                case 14:
                default:
                    return 0;
            }
        }

        int16_t best_weapon = 0;
        int16_t max_priority = 0;

        for (int16_t i = 0; i < 40; ++i) {
            // Specific weapon availability logic
            if (i == 37 && size < 4) continue;
            if (i == 38 && size < 5) continue;

            s_tech_weapon_data& weapon = TECHDATA::weapons[i];
            s_player& player = MOX::player[player_idx];

            if (player.tech_applications[weapon.tech_app_id] == TECH_RESEARCH_STATUS_RESEARCHED) {
                if (weapon.type == WEAPON_TYPE_SPECIAL || weapon.type == WEAPON_TYPE_FIGHTER) {
                    if ((uint8_t)weapon.strat_max_damage > max_priority) {
                        best_weapon = i;
                        max_priority = (uint8_t)weapon.strat_max_damage;
                    }
                }
            }
        }

        return best_weapon;
    }

    void __cdecl Update_Strategic_Design_Weapons_Specials(s_ship_design* design, int16_t player_idx, int16_t design_idx) {
        int16_t weapon_count = 0;
        int16_t best_weapon;
        int16_t count;

        best_weapon = Best_Beam_Weapon_For_Mode(player_idx, 0, true);
        count = Strat_Beam_Weapon_Count(player_idx, design_idx);
        if (best_weapon > 0 && count > 0) {
            design->ship_weapon[weapon_count].type = (int8_t)best_weapon;
            design->ship_weapon[weapon_count].count = (int8_t)count;
            design->ship_weapon[weapon_count].current_count = (int8_t)count;
            design->ship_weapon[weapon_count].firing_arc = WEAPON_FIRING_ARC_360;
            design->ship_weapon[weapon_count].specials = 0;
            design->ship_weapon[weapon_count].ammo = 0x5;
            weapon_count = 1;
        }

        best_weapon = Best_Missile_Weapon_For_Mode(player_idx, true);
        count = Strat_Missile_Weapon_Count(design_idx);
        if (best_weapon > 0 && count > 0) {
            design->ship_weapon[weapon_count].type = (int8_t)best_weapon;
            design->ship_weapon[weapon_count].count = (int8_t)count;
            design->ship_weapon[weapon_count].current_count = (int8_t)count;
            design->ship_weapon[weapon_count].firing_arc = WEAPON_FIRING_ARC_360;
            design->ship_weapon[weapon_count].specials = 0;
            design->ship_weapon[weapon_count].ammo = 0x5;
            weapon_count++;
        }

        best_weapon = Best_Strat_Special_Weapon(player_idx, design_idx);
        count = Strat_Special_Weapon_Count(design_idx);
        if (best_weapon > 0 && count > 0) {
            design->ship_weapon[weapon_count].type = (int8_t)best_weapon;
            design->ship_weapon[weapon_count].count = (int8_t)count;
            design->ship_weapon[weapon_count].current_count = (int8_t)count;
            design->ship_weapon[weapon_count].firing_arc = WEAPON_FIRING_ARC_360;
            design->ship_weapon[weapon_count].specials = 0;
            design->ship_weapon[weapon_count].ammo = 0x5;
            weapon_count++;
        }

        best_weapon = Best_Bomb_Weapon_For_Mode(player_idx, true);
        count = Strat_Bomb_Weapon_Count(design_idx);
        if (best_weapon > 0 && count > 0) {
            design->ship_weapon[weapon_count].type = (int8_t)best_weapon;
            design->ship_weapon[weapon_count].count = (int8_t)count;
            design->ship_weapon[weapon_count].current_count = (int8_t)count;
            design->ship_weapon[weapon_count].firing_arc = WEAPON_FIRING_ARC_360;
            design->ship_weapon[weapon_count].specials = 0;
            design->ship_weapon[weapon_count].ammo = 0xA;
        }

        int16_t special;

        special = Best_Beam_To_Hit_Special(player_idx);
        if (special > 0) {
            struct_::Set_Bit_Field(design->special_device_flags, special);
        }

        special = Best_Beam_Damage_Special(player_idx);
        if (special > 0) {
            struct_::Set_Bit_Field(design->special_device_flags, special);
        }

        special = Best_Missile_Damage_Special(player_idx);
        if (special > 0) {
            struct_::Set_Bit_Field(design->special_device_flags, special);
        }

        special = Best_Shield_Special(player_idx);
        if (special > 0) {
            struct_::Set_Bit_Field(design->special_device_flags, special);
        }

        special = Best_Beam_Defense_Special(player_idx);
        if (special > 0) {
            struct_::Set_Bit_Field(design->special_device_flags, special);
        }

        special = Best_Missile_Defense_Special(player_idx);
        if (special > 0) {
            struct_::Set_Bit_Field(design->special_device_flags, special);
        }
    }

    void __cdecl Update_Strategic_Ship(int16_t ship_idx) {
        s_ship_data* ship = &MOX::ship[ship_idx];
        int16_t owner = (int16_t)ship->owner;

        if (owner < MAX_PLAYERS && MOX::settings.strategic_combat_flag != 0) {
            ship->d.ftl_type = static_cast<int8_t>(Best_Warp_Drive(owner));
            ship->d.speed = static_cast<uint8_t>(Get_FTL_Speed(owner, static_cast<int16_t>(ship->d.ftl_type)));
            ship->d.shield_type = static_cast<int8_t>(Best_Ship_Shield(owner));
            ship->d.computer_type = static_cast<int8_t>(Best_Computer(owner));
            ship->d.armor_type = static_cast<int8_t>(Best_Armor(owner));

            Update_Strategic_Design_Weapons_Specials(&ship->d, owner, static_cast<int16_t>(ship->d.size));
        }
    }

    int16_t __cdecl Strat_Missile_Weapon_Count(int16_t hull_idx) {
        return TECHDATA::hull_data[hull_idx].strat_missile;
    }

    int16_t __cdecl Strat_Special_Weapon_Count(int16_t hull_idx) {
        return TECHDATA::hull_data[hull_idx].strat_special;
    }

    int16_t __cdecl Strat_Beam_Weapon_Count(int16_t player_idx, int16_t hull_idx) {
        int16_t weapon_count = TECHDATA::hull_data[hull_idx].strat_beam;

        if (player_idx < MAX_PLAYERS) {
            // Check for Megafluxers tech application
            if (MOX::player[player_idx].tech_applications[TECH_APP_MEGAFLUXERS] == TECH_RESEARCH_STATUS_RESEARCHED) {
                weapon_count = (int16_t)((weapon_count * (100 + DESIGN::megafluxers_strategic_bonus)) / 100);
            }
        }

        return weapon_count;
    }
    int16_t __cdecl Best_Bomb_Weapon(int16_t player_idx) {
        if (player_idx >= MAX_PLAYERS) {
            return 24 /* 0x18 */;
        }

        return Best_Bomb_Weapon_For_Mode(
            player_idx, MOX::settings.strategic_combat_flag == 1);
    }

    int16_t __cdecl Best_Point_Defense_Weapon(int16_t player_idx) {
        int16_t best_weapon_idx = 3;
        int16_t max_damage = 0;

        if (player_idx < MAX_PLAYERS) {
            for (int16_t i = 0; i < 40; ++i) {
                int16_t tech_app_id = TECHDATA::weapons[i].tech_app_id;

                if (MOX::player[player_idx].tech_applications[tech_app_id] ==
                        TECH_RESEARCH_STATUS_RESEARCHED &&
                    (TECHDATA::weapons[i].available_mods &
                        WEAPON_MOD_FLAG_POINT_DEFENSE) != 0 &&
                    TECHDATA::weapons[i].type == WEAPON_TYPE_BEAM &&
                    (TECHDATA::weapons[i].natural_mods &
                        WEAPON_NATURAL_FLAG_ANTI_SHIELD) == 0) {
                    int16_t mini_level = DESIGN::Miniaturization_Level(player_idx, tech_app_id);

                    // Check if miniaturization level meets requirements for point defense mod
                    // modification index 2 corresponds to Point Defense modification.
                    if (mini_level >= (int16_t)TECHDATA::weapon_modifications[WEAPON_MOD_POINT_DEFENSE].level) {
                        if (TECHDATA::weapons[i].max_damage > max_damage) {
                            max_damage = TECHDATA::weapons[i].max_damage;
                            best_weapon_idx = i;
                        }
                    }
                }
            }
        } else {
            best_weapon_idx = 4;
        }

        return best_weapon_idx;
    }

    /**
 * @brief Returns the maximum number of bomb slots available for a given ship hull.
 * @param hull_idx The index of the ship hull in the technology data.
 * @return The number of bomb weapon slots.
 */
    int16_t __cdecl Strat_Bomb_Weapon_Count(int16_t hull_idx) {
        return TECHDATA::hull_data[hull_idx].strat_bomb;
    }

    int16_t __cdecl Best_Beam_To_Hit_Special(int16_t player_idx) {
        int16_t best_special_idx = 0;
        uint16_t max_bonus = 0;

        // Ensure player_idx is within valid bounds
        if (player_idx < MAX_PLAYERS) {
            for (int16_t special_idx = 0; special_idx < 39 /* 0x27 */; ++special_idx) {
                const s_tech_specials_data& spec = TECHDATA::specials[special_idx];

                // Check if the player has researched the tech associated with this special device
                // 3 usually corresponds to the application being learned/available
                if (MOX::player[player_idx].tech_applications[spec.tech_app_id] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    if (spec.category == SPECIAL_CATEGORY_BEAM_ACCURACY) {
                        if (spec.strategic > max_bonus) {
                            max_bonus = spec.strategic;
                            best_special_idx = special_idx;
                        }
                    }
                }
            }
        }

        return best_special_idx;
    }

    int16_t __cdecl Best_Beam_Damage_Special(int16_t player_idx) {
        int16_t best_idx = 0;
        uint16_t max_bonus = 0;

        if (player_idx < MAX_PLAYERS) {
            for (int16_t i = 0; i < 39; ++i) {
                const s_tech_specials_data& special = TECHDATA::specials[i];

                // Check if player has fully researched the tech for this special device
                if (MOX::player[player_idx].tech_applications[special.tech_app_id] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    if (special.category == SPECIAL_CATEGORY_BEAM_DAMAGE || special.category == SPECIAL_CATEGORY_BEAM_MISSILE_DAMAGE) {
                        if (max_bonus < special.strategic) {
                            best_idx = i;
                            max_bonus = special.strategic;
                        }
                    }
                }
            }
        }

        return best_idx;
    }

    int16_t __cdecl Best_Beam_Defense_Special(int16_t player_idx) {
        int16_t best_special_idx = 0;
        uint16_t max_bonus = 0;

        if (player_idx < MAX_PLAYERS) {
            for (int16_t i = 0; i < 39; ++i) {
                const s_tech_specials_data& special = TECHDATA::specials[i];

                // Check if the player has researched the technology for this special device
                if (MOX::player[player_idx].tech_applications[special.tech_app_id] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    if (special.category == SPECIAL_CATEGORY_BEAM_DEFENSE || special.category == SPECIAL_CATEGORY_BEAM_MISSILE_DEFENSE) {
                        if (special.strategic > max_bonus) {
                            best_special_idx = i;
                            max_bonus = special.strategic;
                        }
                    }
                }
            }
        }

        return best_special_idx;
    }

    /**
 * Finds the missile defense special device with the highest bonus value that the player has researched.
 * 
 * @param player_idx The index of the player.
 * @return The index of the best missile defense special device found, or 0 if none or invalid player.
 */
    int16_t __cdecl Best_Missile_Defense_Special(int16_t player_idx) {
        int16_t best_special_idx = 0;
        uint16_t best_bonus = 0;

        if (player_idx < MAX_PLAYERS) {
            for (int16_t special_idx = 0; special_idx < 39 /* 0x27 */; ++special_idx) {
                const s_tech_specials_data& special = TECHDATA::specials[special_idx];

                // Check if the player has researched the tech application associated with this special
                // Value 3 typically indicates the tech application is known/learned.
                if (MOX::player[player_idx].tech_applications[special.tech_app_id] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    if (special.category == SPECIAL_CATEGORY_MISSILE_DEFENSE || special.category == SPECIAL_CATEGORY_BEAM_MISSILE_DEFENSE) {
                        if (special.strategic > best_bonus) {
                            best_bonus = special.strategic;
                            best_special_idx = special_idx;
                        }
                    }
                }
            }
        }

        return best_special_idx;
    }

    void __cdecl Load_Small_Antaran_Combat_Ship(int16_t ship_idx) {
        Load_Configured_Antaran_Combat_Ship(ship_idx, SHIP_SIZE_SMALL);
    }

    void __cdecl Load_Medium_Antaran_Combat_Ship(int16_t ship_idx) {
        Load_Configured_Antaran_Combat_Ship(ship_idx, SHIP_SIZE_MEDIUM);
    }

    void __cdecl Get_Design_Combat_Bonuses(s_ship_design design, int16_t* attack_bonus_out, int16_t* defense_rating_out, int16_t* missile_defense_out) {
        int16_t missile_defense = 0;
        int16_t attack_bonus = TECHDATA::computers[design.computer_type].bonus;

        if (struct_::Test_Bit_Field(design.special_device_flags, SPECIAL_BATTLE_SCANNER)) {
            attack_bonus += CMBTFIR2::combat_bonus.battle_scanner_beam_offense;
        }

        int16_t current_speed = design.combat_speed;
        if (struct_::Test_Bit_Field(design.special_device_flags, 2)) {
            current_speed += DESIGN::augmented_engines_speed_bonus;
        }

        int16_t defense_rating = current_speed * 5;

        if (struct_::Test_Bit_Field(design.special_device_flags, 9)) {
            missile_defense = CMBTMIS::missile_defense.ecm_jammer_missile_defense;
        }

        if (struct_::Test_Bit_Field(design.special_device_flags, 17 /* 0x11 */)) {
            defense_rating += CMBTFIR2::combat_bonus.inertial_nullifier_beam_defense;
            missile_defense += CMBTMIS::missile_defense.inertial_nullifier_missile_defense;
        }

        if (struct_::Test_Bit_Field(design.special_device_flags, 18 /* 0x12 */)) {
            defense_rating += CMBTFIR2::combat_bonus.inertial_stabilizer_beam_defense;
            missile_defense += CMBTMIS::missile_defense.inertial_stabilizer_missile_defense;
        }

        if (struct_::Test_Bit_Field(design.special_device_flags, 21 /* 0x15 */)) {
            missile_defense += CMBTMIS::missile_defense.multi_wave_jammer_missile_defense;
        }

        if (struct_::Test_Bit_Field(design.special_device_flags, 38 /* 0x26 */)) {
            missile_defense += CMBTMIS::missile_defense.wide_area_jammer_missile_defense;
        }

        *defense_rating_out = defense_rating;
        *missile_defense_out = missile_defense;
        *attack_bonus_out = attack_bonus;
    }

    void __cdecl Load_Large_Antaran_Combat_Ship(int16_t combat_idx) {
        Load_Configured_Antaran_Combat_Ship(combat_idx, SHIP_SIZE_LARGE);
    }

    void __cdecl Load_Huge_Antaran_Combat_Ship(int16_t ship_idx) {
        Load_Configured_Antaran_Combat_Ship(ship_idx, SHIP_SIZE_HUGE);
    }

    void __cdecl Load_Titan_Antaran_Combat_Ship(int16_t ship_index) {
        Load_Configured_Antaran_Combat_Ship(ship_index, SHIP_SIZE_TITAN);
    }

    void __cdecl Load_Guardian_Ship_Design(s_ship_design* design) {
        *design = SHIP_CONFIG::guardian_template;
        Complete_Configured_Design(design, MOX::race_names[14], 9, 0, design->ftl_type == DRIVE_NO_DRIVE ? 0 : 1, Monster_Combat_Speed_Drive(*design, SHIP_CONFIG::monster.drives.guardian), 0);
    }

    void __cdecl Load_Amoeba_Ship_Design(s_ship_design* design, uint8_t is_large) {
        *design = SHIP_CONFIG::amoeba_templates[is_large != 0];
        Complete_Configured_Design(design, MOX::race_names[15], 10, 0, design->ftl_type == DRIVE_NO_DRIVE ? 0 : 1, Monster_Combat_Speed_Drive(*design, SHIP_CONFIG::monster.drives.amoeba), 0);
    }

    void __cdecl Load_Crystal_Ship_Design(s_ship_design* design, uint8_t variant) {
        *design = SHIP_CONFIG::crystal.templates[variant != 0];
        design->shield_type =
            static_cast<int8_t>(SHIP_CONFIG::crystal.shared.shield);
        design->computer_type =
            static_cast<int8_t>(SHIP_CONFIG::crystal.shared.computer);
        memset(design->special_device_flags, 0, sizeof(design->special_device_flags));
        if (SHIP_CONFIG::crystal.shared.special != 0) {
            struct_::Set_Bit_Field(design->special_device_flags,
                SHIP_CONFIG::crystal.shared.special);
        }
        Complete_Configured_Design(design, MOX::race_names[16], 11, 0, design->ftl_type == DRIVE_NO_DRIVE ? 0 : 1, Monster_Combat_Speed_Drive(*design, SHIP_CONFIG::monster.drives.crystal), 0);
    }

    void __cdecl Load_Dragon_Ship_Design(s_ship_design* design, uint8_t is_monster) {
        *design = SHIP_CONFIG::dragon_templates[is_monster != 0];
        Complete_Configured_Design(design, MOX::race_names[17], 12, 0, design->ftl_type == DRIVE_NO_DRIVE ? 0 : 1, Monster_Combat_Speed_Drive(*design, SHIP_CONFIG::monster.drives.dragon), 4);
    }

    /**
 * Loads the ship design for a Space Eel (Young/Small or Greater).
 * @param design The ship design structure to populate.
 * @param is_greater_eel If non-zero, loads Greater Eel design; otherwise, Young/Small Eel.
 */
    void __cdecl Load_Eel_Ship_Design(s_ship_design* design, uint8_t is_greater_eel) {
        *design = SHIP_CONFIG::eel_templates[is_greater_eel != 0];
        Complete_Configured_Design(design, MOX::race_names[18], 13, 0, design->ftl_type == DRIVE_NO_DRIVE ? 0 : 1, Monster_Combat_Speed_Drive(*design, SHIP_CONFIG::monster.drives.eel), 8);
    }

    void __cdecl Load_Hydra_Ship_Design(s_ship_design* design, uint8_t is_monster) {
        *design = SHIP_CONFIG::hydra_templates[is_monster != 0];
        Complete_Configured_Design(design, MOX::race_names[19], 14, 0, design->ftl_type == DRIVE_NO_DRIVE ? 0 : 1, Monster_Combat_Speed_Drive(*design, SHIP_CONFIG::monster.drives.hydra), 0);
    }

    void __cdecl Load_Transport_Ship_Design(int16_t player_idx, s_ship_design* design) {
        struct_::Clear_Structure(design, sizeof(*design));

        // Copy localized "Transport" name
        const char* localized_name = MOX::ship_type_names[SHIP_TYPE_TRANSPORT][(uint8_t)MOX::settings.language];
        strlcpy(design->name, localized_name, sizeof(design->name));

        design->size = INITSHIP::landers_size_class;
        design->ship_type = SHIP_TYPE_TRANSPORT;
        design->shield_type = SHIELD_NO_SHIELD;

        design->ftl_type = (int8_t)INITSHIP::Best_Warp_Drive(player_idx);
        design->computer_type = COMPUTER_NO_COMPUTER;

        design->armor_type = (int8_t)INITSHIP::Best_Armor(player_idx);

        // Clear special devices
        design->special_device_flags[0] = 0;
        design->special_device_flags[1] = 0;
        design->special_device_flags[2] = 0;
        design->special_device_flags[3] = 0;
        design->special_device_flags[4] = 0;

        if (transport_ship_extended_range != 0) {
            struct_::Set_Bit_Field(
                design->special_device_flags, SPECIAL_EXTENDED_FUEL_TANKS);
        }

        // Reset all weapon slots
        for (int16_t i = 0; i < 8; ++i) {
            design->ship_weapon[i].type = 0;
            design->ship_weapon[i].count = 0;
        }

        if (lander_ship_uses_stealth.transport_ship != 0) {
            Equip_Best_Stealth_Device(player_idx, design);
        }

        design->picture_num = 47 /* '/' */;

        // Derive speeds from drive technology
        design->speed = static_cast<uint8_t>(TECHDATA::drives[design->ftl_type].warp_speed);
        design->combat_speed = (int8_t)TECHDATA::drives[design->ftl_type].combat_speed_min[design->size];

        // Set production cost
        design->cost = (int16_t)COLCALC::Ship_Type_Cost_For_Player(SHIP_TYPE_TRANSPORT, player_idx);
    }

    void __cdecl Load_Loknar_Ship_Design(s_ship_design* design) {
        *design = SHIP_CONFIG::avenger_template;
        Complete_Configured_Design(design, "Avenger", 8, 500, 8, design->ftl_type == DRIVE_NO_DRIVE ? Best_Warp_Drive(8) : design->ftl_type, 2);
    }

    void __cdecl Update_Ship_Names_To_New_Language(void) {
        for (int16_t ship_index = 0; ship_index < MAX_SHIPS; ++ship_index) {
            s_ship_data* ship = &MOX::ship[ship_index];
            const int32_t ship_type = (int32_t)ship->d.ship_type;

            if (ship_type != SHIP_TYPE_COMBAT) {
                const char* localized_name = MOX::ship_type_names[ship_type][(uint8_t)MOX::settings.language];

                strlcpy(ship->d.name, localized_name, sizeof(ship->d.name));
            }
        }
    }

    int16_t __cdecl Best_Missile_Damage_Special(int16_t player_idx) {
        int16_t best_special_idx = 0;
        uint16_t best_strategic_value = 0;

        if (player_idx < MAX_PLAYERS) {
            for (int16_t special_idx = 0; special_idx < 39; ++special_idx) {
                const int16_t tech_app_id =
                    static_cast<int16_t>(TECHDATA::specials[special_idx].tech_app_id);

                if (MOX::player[player_idx].tech_applications[tech_app_id] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    const int8_t category = TECHDATA::specials[special_idx].category;

                    if ((category == SPECIAL_CATEGORY_MISSILE_DAMAGE || category == SPECIAL_CATEGORY_BEAM_MISSILE_DAMAGE) &&
                        best_strategic_value < TECHDATA::specials[special_idx].strategic) {
                        best_special_idx = special_idx;
                        best_strategic_value = TECHDATA::specials[special_idx].strategic;
                    }
                }
            }
        } else {
            best_special_idx = 0;
        }

        if (best_special_idx == 34) {
            best_special_idx = 0;
        }

        return best_special_idx;
    }

    int16_t __cdecl Ship_Has_Weapon(int16_t ship_index, int16_t weapon_type) {
        int16_t has_weapon = 0;
        int16_t weapon_slot = 0;

        while (weapon_slot < 8) {
            if (has_weapon != 0) {
                break;
            }

            if (MOX::ship[ship_index].d.ship_weapon[weapon_slot].type == weapon_type) {
                has_weapon = 1;
            }

            ++weapon_slot;
        }

        return has_weapon;
    }

} // namespace INITSHIP