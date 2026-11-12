#include "pch.h"

namespace CMBTMIS {
    int64_t Plasma_Torpedo_Remaining_Damage(
        const s_missile* missile, int64_t base_damage, bool resolving_impact) {
        int64_t speed = Missile_Speed(
            missile->type, missile->specials, missile->owner);
        int64_t distance = speed * ((int64_t)missile->fighter_count + 1) * 20 -
            missile->travel_dist;
        if (resolving_impact) distance -= 20;
        if (distance < 0) distance = 0;

        int64_t dissipation = (distance / 20) * plasma_torpedo_dissipation;
        return base_damage - dissipation;
    }

    int32_t __cdecl Missile_Speed(int16_t type, uint16_t flags, int16_t player_idx) {
        bool boost_drive = false;
        bool boost_td = false;
        int32_t missile_speed = 0;

        switch (type) {
            case WEAPON_NUCLEAR_MISSILE:
                missile_speed = speed_base.nuclear_missile;
                boost_drive = true;
                break;
            case WEAPON_MERCULITE_MISSILE:
                missile_speed = speed_base.merculite_missile;
                boost_drive = true;
                break;
            case WEAPON_PULSON_MISSILE:
                missile_speed = speed_base.pulson_missile;
                boost_drive = true;
                break;
            case WEAPON_ZEON_MISSILE:
                missile_speed = speed_base.zeon_missile;
                boost_drive = true;
                break;

            case WEAPON_ANTI_MATTER_TORP:
                missile_speed = speed_base.anti_matter_torpedo;
                break;
            case WEAPON_PROTON_TORPEDO:
                missile_speed = speed_base.proton_torpedo;
                break;

            case WEAPON_PLASMA_TORPEDO:
                missile_speed = speed_base.plasma_torpedo;
                break;
            case WEAPON_DRAGON_BREATH:
                missile_speed = speed_base.plasma_breath;
                break;

            case WEAPON_ASSAULT_SHUTTLE:
                missile_speed = speed_base.assault_shuttle;
                boost_drive = true;
                boost_td = true;
                break;

            case WEAPON_HEAVY_FIGHTER:
                missile_speed = speed_base.heavy_fighter;
                boost_drive = true;
                boost_td = true;
                break;

            case WEAPON_BOMBER:
                missile_speed = speed_base.bomber;
                boost_drive = true;
                boost_td = true;
                break;

            case WEAPON_INTERCEPTOR:
                missile_speed = speed_base.interceptor;
                boost_drive = true;
                boost_td = true;
                break;

            default: {
                char message[64];
                snprintf(message, sizeof(message), "Missile_Speed_ failed, unknown flyer %d", type);
                exitm::Exit_With_Message(message);
            }
        }

        if (boost_drive) {
            int32_t drive_level = INITSHIP::Best_Warp_Drive(player_idx);
            missile_speed += (drive_level * 2) - 2;
        }

        if (boost_td && player_idx >= 0 && player_idx < MAX_PLAYERS &&
            MOX::player[player_idx].traits[TRAIT_TRANS_DIMENSIONAL] != 0) {
            missile_speed += COMBAT1::td_combat_speed_bonus;
        }

        if (flags & WEAPON_MOD_FLAG_FAST) {
            missile_speed += speed_fast_bonus;
        }

        return missile_speed;
    }

    int64_t __cdecl Fighter_Ocv(int16_t player_idx) {
        int64_t ocv;
        int16_t best_computer_idx;
        int16_t pilot_bonus;

        ocv = CMBTFIR2::Fleet_Offensive_Bonus(player_idx);
        best_computer_idx = INITSHIP::Best_Computer(player_idx);

        // Add computer offensive bonus
        ocv += TECHDATA::computers[best_computer_idx].bonus;

        // Players 0-7 get their racial/trained ship attack trait bonus
        if (player_idx < MAX_PLAYERS) {
            ocv += MOX::player[player_idx].traits[TRAIT_SHIP_ATTACK];
        }

        pilot_bonus = CMBTFIR2::Get_Fleet_Pilot_Bonus(player_idx);

        return ocv + pilot_bonus + flyer_combat_modifier.fighters_offense_bonus;
    }

    int32_t __cdecl Get_Missile_Hits(int16_t missile_id, int16_t ship_idx, uint16_t flags) {
        int32_t hits = 0;
        int16_t armor_idx;
        int16_t absorption;

        switch (missile_id) {
            case WEAPON_NUCLEAR_MISSILE:
                hits = flyer_hit_points.nuclear_missile;
                break;

            case WEAPON_MERCULITE_MISSILE:
                hits = flyer_hit_points.merculite_missile;
                break;

            case WEAPON_PULSON_MISSILE:
                hits = flyer_hit_points.pulson_missile;
                break;

            case WEAPON_ZEON_MISSILE:
                hits = flyer_hit_points.zeon_missile;
                break;

            case WEAPON_ANTI_MATTER_TORP:
            case WEAPON_PROTON_TORPEDO:
            case WEAPON_PLASMA_TORPEDO:
            case WEAPON_DRAGON_BREATH:
                hits = 5;
                break;

            case WEAPON_ASSAULT_SHUTTLE:
                armor_idx = INITSHIP::Best_Armor(ship_idx);
                absorption = TECHDATA::armor[armor_idx].ships_bonus;
                hits = (int32_t)flyer_hit_points.assault_shuttle *
                    (absorption + 100) / 200;
                break;

            case WEAPON_HEAVY_FIGHTER:
                armor_idx = INITSHIP::Best_Armor(ship_idx);
                absorption = TECHDATA::armor[armor_idx].ships_bonus;
                hits = (int32_t)flyer_hit_points.heavy_fighter *
                    (absorption + 100) / 200;
                break;

            case WEAPON_BOMBER:
                armor_idx = INITSHIP::Best_Armor(ship_idx);
                absorption = TECHDATA::armor[armor_idx].ships_bonus;
                hits = (int32_t)flyer_hit_points.bomber *
                    (absorption + 100) / 200;
                break;

            case WEAPON_INTERCEPTOR:
                armor_idx = INITSHIP::Best_Armor(ship_idx);
                absorption = TECHDATA::armor[armor_idx].ships_bonus;
                hits = (int32_t)flyer_hit_points.interceptor *
                    (absorption + 100) / 200;
                break;

            default:
                break;
        }

        if (flags & WEAPON_MOD_FLAG_HEAVILY_ARMORED) {
            hits *= 2;
        }

        if (hits == 0) {
            hits = 1;
        }

        return hits;
    }

    int64_t __cdecl Missile_Dcv(int16_t weapon_idx, int16_t player_idx) {
        int64_t bonus = 0;
        int64_t speed = Missile_Speed(weapon_idx, 0, player_idx);
        int64_t base_dcv = speed * 5;

        if (TECHDATA::weapons[weapon_idx].type == WEAPON_TYPE_FIGHTER) {
            bonus = CMBTFIR2::Get_Fleet_Pilot_Bonus(player_idx);
            int16_t fleet_base = 0;
            bonus += CMBTFIR2::Fleet_Defensive_Bonus(player_idx, 0, &fleet_base);

            // Add defense trait bonus if player is valid
            if (player_idx < MAX_PLAYERS && player_idx != -1) {
                bonus += MOX::player[player_idx].traits[TRAIT_SHIP_DEFENSE];
            }
        }

        switch (weapon_idx) {
            case WEAPON_NUCLEAR_MISSILE:
                return base_dcv + flyer_combat_modifier.nuclear_missile_defense_bonus;
            case WEAPON_MERCULITE_MISSILE:
                return base_dcv + flyer_combat_modifier.merculite_missile_defense_bonus;
            case WEAPON_PULSON_MISSILE:
                return base_dcv + flyer_combat_modifier.pulson_missile_defense_bonus;
            case WEAPON_ZEON_MISSILE:
                return base_dcv + flyer_combat_modifier.zeon_missile_defense_bonus;
            case WEAPON_ASSAULT_SHUTTLE:
            case WEAPON_HEAVY_FIGHTER:
            case WEAPON_BOMBER:
            case WEAPON_INTERCEPTOR:
                return base_dcv + bonus;
            default:
                return base_dcv;
        }
    }
} // namespace CMBTMIS
