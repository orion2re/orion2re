#include "pch.h"

namespace AIPOWER {
    int16_t __cdecl Get_Best_Ship_Shield_Value(int16_t player_idx) {
        if (player_idx >= MAX_PLAYERS) {
            switch (player_idx) {
                case MAX_PLAYERS:
                    return 6;
                case 9:
                    return 8;
                case 10:
                case 11:
                case 12:
                case 13:
                case 14:
                    return 0;
                default: {
                    // The binary returns an uninitialized 16-bit stack value here for out-of-range player_idx.
                    // We intentionally normalize that undefined behavior to 0.
                    return 0;
                }
            }
        }

        int16_t shield_idx = INITSHIP::Best_Ship_Shield(player_idx);
        return TECHDATA::shields[shield_idx].shield_class;
    }

    uint32_t __cdecl Missile_Base_Strength(int16_t player_idx, int16_t target_idx) {
        if (MOX::settings.strategic_combat_flag == 1) {
            return 90 /* 0x5a */;
        }

        int16_t weapon_idx = INITSHIP::Best_Missile_Weapon_NT(player_idx);

        // weapon_idx is used here to get space cost
        int32_t weapon_space = DESIGN::Weapon_Space(player_idx, weapon_idx, 1, WEAPON_FIRING_ARC_360, COMBINIT::missile_base_ammo_basis, 0);

        int16_t capacity = (int16_t)(COMBINIT::ground_defense.missile_base_space / weapon_space);

        int16_t shield_val = AIPOWER::Get_Best_Ship_Shield_Value(target_idx);

        // Calculate base damage per weapon mount minus shield reduction
        int32_t net_damage = (int32_t)TECHDATA::weapons[weapon_idx].max_damage - (int32_t)shield_val;

        if (net_damage <= 0) {
            net_damage = 0;
        }

        uint32_t strength = (uint32_t)net_damage * (uint32_t)capacity * 2;

        if (strength > 64000 /* 0xfa00 */) {
            strength = 64000;
        }

        return strength;
    }

    uint16_t __cdecl Colony_Ground_Strength_Vs_Player(int16_t colony_idx, int16_t player_idx) {
        uint16_t strength = 0;
        s_colony* colony = &MOX::colony[colony_idx];

        // BUILDING_MISSILE_BASE: 26
        if (colony->buildings[BUILDING_MISSILE_BASE] != 0) {
            strength = AIPOWER::Missile_Base_Strength((int16_t)colony->owner, player_idx);
        }

        // BUILDING_GROUND_BATTERIES: 27
        if (colony->buildings[BUILDING_GROUND_BATTERIES] != 0) {
            strength += AIPOWER::Phasor_Base_Strength((int16_t)colony->owner, player_idx);
        }

        // BUILDING_STELLAR_CONVERTER: 42
        if (colony->buildings[BUILDING_STELLAR_CONVERTER] != 0) {
            if (MOX::settings.strategic_combat_flag == 1) {
                strength += 200;
            } else {
                strength += 500;
            }
        }

        // BUILDING_FIGHTER_GARRISON: 47
        if (colony->buildings[BUILDING_FIGHTER_GARRISON] != 0) {
            strength += AIPOWER::Fighter_Garrison_Strength((int16_t)colony->owner, player_idx);
        }

        // Cap maximum defensive strength
        if (strength > 64000) {
            strength = 64000;
        }

        return strength;
    }

    uint32_t __cdecl Phasor_Base_Strength(int16_t player_idx, int16_t target_idx) {
        if (MOX::settings.strategic_combat_flag == 1) {
            return 120 /* 0x78 */;
        }

        int16_t weapon_idx = INITSHIP::Best_Beam_Weapon(
            player_idx, WEAPON_MOD_FLAG_HEAVY_MOUNT);
        int32_t space = DESIGN::Weapon_Space(player_idx, weapon_idx, 1, WEAPON_FIRING_ARC_360, 1, 2);

        int16_t weapon_count = (int16_t)(COMBINIT::ground_defense.battery_hv_space / space);

        int16_t shield_value = AIPOWER::Get_Best_Ship_Shield_Value(target_idx);

        int32_t max_damage = TECHDATA::weapons[weapon_idx].max_damage;
        int32_t base_damage = ((max_damage * 3) / 2) - shield_value;

        if (base_damage < 0) {
            base_damage = 0;
        }

        uint32_t total_power = (uint32_t)base_damage * (uint32_t)weapon_count;

        if ((TECHDATA::weapons[weapon_idx].natural_mods & WEAPON_NATURAL_FLAG_IGNORES_ARMOR_STRUCTURE) != 0) {
            total_power *= 3;
        }

        if (total_power > 64000 /* 0xFA00 */) {
            total_power = 64000 /* 0xFA00 */;
        }

        return total_power;
    }

    uint16_t __cdecl Fighter_Garrison_Strength(int16_t player_idx, int16_t target_player_idx) {
        int16_t pd_weapon_idx = INITSHIP::Best_Point_Defense_Weapon(player_idx);
        int16_t bomb_weapon_idx = INITSHIP::Best_Bomb_Weapon(player_idx);
        int16_t bomber_count = 0;
        int16_t shield_value = Get_Best_Ship_Shield_Value(target_player_idx);
        int16_t heavy_fighter_count = 0;

        if (MOX::settings.strategic_combat_flag == 1) {
            return 120;
        }

        int16_t pd_count = COMBINIT::ground_defense.garrison_interceptors;
        s_player* player = &MOX::player[player_idx];

        if (player->tech_applications[TECH_APP_HEAVY_FIGHTER_BAYS] == TECH_RESEARCH_STATUS_RESEARCHED) {
            pd_count = 0;
            bomber_count = 0;
            heavy_fighter_count =
                COMBINIT::ground_defense.garrison_heavy_fighters;
        } else if (player->tech_applications[TECH_APP_BOMBER_BAYS] == TECH_RESEARCH_STATUS_RESEARCHED) {
            pd_count = 0;
            bomber_count = COMBINIT::ground_defense.garrison_bombers;
        }

        int32_t bomb_dmg = (int32_t)TECHDATA::weapons[bomb_weapon_idx].max_damage - shield_value;
        if (bomb_dmg < 0) {
            bomb_dmg = 0;
        }

        int32_t pd_dmg = (int32_t)TECHDATA::weapons[pd_weapon_idx].max_damage - shield_value;
        if (pd_dmg < 0) {
            pd_dmg = 0;
        }

        uint16_t base_strength = (int16_t)bomb_dmg * ((heavy_fighter_count * 3 / 2) + bomber_count) +
            (int16_t)pd_dmg * (pd_count + heavy_fighter_count + bomber_count);

        base_strength &= 0x3fff;

        uint16_t final_strength = base_strength * 2;
        if (final_strength > 64000 /* 0xFA00 */) {
            final_strength = 64000;
        }

        return final_strength;
    }
} // namespace AIPOWER
