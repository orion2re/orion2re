#include "pch.h"
#include "combat_fleets.h"

using game::current_state;
#include "legacy_ids.h"

#include "lua/lua_game.h"

namespace {
    void Apply_Strategic_Crew_Bonuses() {
        if (COMBAT::strategic_xp_bonus == 0) {
            return;
        }

        for (int16_t i = 1; i < MOX::combat_total_ship_count; ++i) {
            s_qship* combat_ship = &MOX::qcombat_data[i];
            const auto* ship = game::Find_Ship(game::current_state, combat_ship->ship_id);
            if (!ship) {
                continue;
            }

            const uint8_t crew_quality = (uint8_t)ship->crew_quality;
            if (crew_quality >= CREW_QUALITY_COUNT) {
                continue;
            }

            const s_crew_data* crew = &MOX::crew_data[crew_quality];
            combat_ship->base_ocv += crew->attack;
            combat_ship->beam_defense_bonus += crew->defense;
            combat_ship->missile_defense_bonus += crew->defense / 2;
        }
    }
} // namespace

namespace COMBAT {
    constexpr size_t kQCombatDataShipCount = MAX_SHIPS + 1;
    constexpr size_t kQCombatDataSize = kQCombatDataShipCount * sizeof(s_qship);

    const char s_Crystal_00557e24[8] = "Crystal";
    const char s_Eel_00557e2c[4] = "Eel";
    const char s_Hydra_00557e30[8] = "Hydra";
    const char s_Dragon_00557e38[8] = "Dragon";
    const char s_Amoeba_00557e40[8] = "Amoeba";
    const char s_Guardian_00557e48[12] = "Guardian";
    const char s_Antaran_00557e54[8] = "Antaran";

    int16_t Strategic_Combat(std::span<const game::id_ship> plr1_ships, std::span<const game::id_ship> plr2_ships,
        int16_t star_idx, game::id_colony colony_idx, char* summary, int32_t summary_size, char* details, int32_t details_size) {
        const auto* attacker = plr1_ships.empty() ? nullptr : game::Find_Ship(game::current_state, plr1_ships.front());
        const auto* defender = plr2_ships.empty() ? nullptr : game::Find_Ship(game::current_state, plr2_ships.front());
        if (!attacker || (!defender && colony_idx == game::NO_COLONY && star_idx != game::ANTARES_POSITION)) return 0;
        MOX2::action_func_t auto_fn;
        int16_t leader_combat_bonuses[20];

        for (int i = 0; i < 20; i++) {
            leader_combat_bonuses[i] = 0;
        }

        MOX2::Get_Auto_Function(&auto_fn);
        fields::Assign_Auto_Function(MAINSCR::Draw_Mini_Main_Screen, 1);

        MOX::cmbt_colony_id = colony_idx;
        MOX::combat_turn = 0;
        MOX::qcombat_data = static_cast<s_qship*>(allocate::Allocate_First_Block_Aligned(MOX::screen_seg, kQCombatDataSize, alignof(s_qship)));
        struct_::Clear_Structure(MOX::qcombat_data, kQCombatDataSize);

        MOX::attacking_faction = attacker->owner;

        if (game::ANTARES_POSITION == star_idx) {
            MOX::defending_faction = factions::From_Legacy(8);
        } else {
            if (!defender) {
                MOX::defending_faction = factions::From_Player(game::current_state.colonies.at(MOX::cmbt_colony_id).owner);
            } else {
                MOX::defending_faction = defender->owner;
            }
        }

        if (MOX::attacking_faction.code == 9 && MOX::defending_faction.code >= MAX_PLAYERS) {
            MOX::attacking_faction = factions::From_Legacy(9);
            for (const auto key : plr2_ships) {
                if (auto* ship = game::Find_Ship(game::current_state, key)) SHIPS::Kill_Ship(*ship);
            }
            MOX2::Put_Auto_Function(&auto_fn);
            return 1;
        }

        if (MOX::defending_faction.code == 9 && MOX::attacking_faction.code >= MAX_PLAYERS) {
            for (const auto key : plr1_ships) {
                if (auto* ship = game::Find_Ship(game::current_state, key)) SHIPS::Kill_Ship(*ship);
            }
            MOX2::Put_Auto_Function(&auto_fn);
            return -1;
        }

        MOX::colony_defense_flags = 0;
        MOX::combat_total_ship_count = 1;

        int16_t bonus1 = 0;
        int16_t bonus2 = 0;
        int16_t bonus3 = 0;
        int16_t bonus4 = 0;

        Get_Leader_Combat_Bonuses(plr1_ships, &bonus1, &bonus2, &leader_combat_bonuses[MOX::attacking_faction.code], &bonus3, &bonus4);
        Qload_Ships(plr1_ships, bonus1, bonus2, bonus3, bonus4);

        const int16_t attacker_ship_count = MOX::combat_total_ship_count - 1;
        MOX::combat_attacker_count = attacker_ship_count;

        int16_t result = 0;
        int16_t colony_defense_flags = 0;
        int16_t survived_officers_count = 0;
        int16_t size_sum = 0;
        int16_t survived_officer_ids[8] = {};
        bool skip_combat = false;

        if (MOX::defending_faction.code < MAX_PLAYERS && star_idx < std::ssize(current_state.stars)) {
            if (HAROLD::Player_Has_Artemis_Net(&game::current_state.players[MOX::defending_faction.code], &game::current_state.stars.at(game::Star_Key_At_Row(current_state, star_idx)))) {
                if (CMBTFIR2::Apply_Combat_Artemis_Net(1)) {
                    result = -1;
                    skip_combat = true;
                }
            }
        }

        if (!skip_combat) {
            if (game::ANTARES_POSITION == star_idx) {
                QLoad_Antaran_Defense_Fleet();
            } else {
                bonus1 = 0;
                bonus2 = 0;
                bonus3 = 0;
                bonus4 = 0;
                Get_Leader_Combat_Bonuses(plr2_ships, &bonus1, &bonus2, &leader_combat_bonuses[MOX::defending_faction.code], &bonus3, &bonus4);
                Qload_Ships(plr2_ships, bonus1, bonus2, bonus3, bonus4);
            }

            Apply_Strategic_Crew_Bonuses();

            MOX::combat_defender_count = MOX::combat_total_ship_count - attacker_ship_count;
            if (MOX::colony_defense_flags == 0) {
                MOX::combat_defender_count--;
            }

            colony_defense_flags = MOX::colony_defense_flags;

            while (MOX::combat_attacker_count > 0 && MOX::combat_defender_count > 0) {
                if (MOX::combat_turn >= 50) {
                    break;
                }

                MOX::combat_turn++;

                for (int i = 0; i < MOX::combat_total_ship_count; i++) {
                    if (MOX::qcombat_data[i].status == 2) {
                        continue;
                    }

                    int16_t target = QGet_Target(i);
                    if (target != -1) {
                        if (i == 0 && (MOX::colony_defense_flags & 4)) {
                            int16_t sc_target = QGet_Target_SC(i);
                            Stellar_Converter_Attack(sc_target);
                            target = QGet_Target(0);
                        }

                        if (target != -1) {
                            if (MOX::combat_turn < 4 || MOX::qcombat_data[i].size > 5 || i == 0) {
                                if (factions::Legacy_Code(MOX::qcombat_data[i].player) == 8) {
                                    Beam_Attack(i, target);
                                } else {
                                    Missile_Attack(i, target);
                                }
                            }

                            if (MOX::combat_turn > 2 || i == 0) {
                                Strat_Special_Attack(i, target);
                            }

                            if (MOX::combat_turn > 2 || i == 0) {
                                Beam_Attack(i, target);
                            }
                        }
                    }

                    if (MOX::combat_turn > 3) {
                        if (factions::Legacy_Code(MOX::qcombat_data[i].player) == MOX::attacking_faction.code && MOX::qcombat_data[0].status == 0 && MOX::cmbt_colony_id != game::NO_COLONY) {
                            Strat_Bomb_Attack(i);
                        }
                    }
                }

                Resolve_Strat_Colony_Damage();

                for (int i = 0; i < MOX::combat_total_ship_count; i++) {
                    if (MOX::qcombat_data[i].status == 1) {
                        MOX::qcombat_data[i].status = 2;
                        if (factions::Legacy_Code(MOX::qcombat_data[i].player) == MOX::attacking_faction.code) {
                            MOX::combat_attacker_count--;
                        } else {
                            MOX::combat_defender_count--;
                        }
                    }
                }
            }

            if (MOX::combat_turn < 50) {
                result = MOX::combat_attacker_count - MOX::combat_defender_count;
            } else {
                result = (MOX::combat_defender_count <= 0) ? 0 : -1;
            }
        }

        for (int i = 0; i < MOX::combat_total_ship_count; i++) {
            if (MOX::qcombat_data[i].status != 0 && MOX::qcombat_data[i].size > 4 && star_idx != game::ANTARES_POSITION) {
                int16_t building_id = 0;
                if (MOX::qcombat_data[i].size == HULL_STAR_BASE) {
                    building_id = BUILDING_STAR_BASE;
                } else if (MOX::qcombat_data[i].size == HULL_BATTLESTATION) {
                    building_id = BUILDING_BATTLESTATION;
                } else if (MOX::qcombat_data[i].size == HULL_STAR_FORTRESS) {
                    building_id = BUILDING_STAR_FORTRESS;
                }

                if (building_id != 0) {
                    ERICNET::Remove_Building(game::current_state, MOX::cmbt_colony_id, building_id);
                }
            }

            auto* ship = game::Find_Ship(game::current_state, MOX::qcombat_data[i].ship_id);
            if (ship) {
                uint8_t player = (uint8_t)factions::Legacy_Code(MOX::qcombat_data[i].player);
                if (leader_combat_bonuses[player] <= 0 &&
                    (player >= MAX_PLAYERS || (game::current_state.players[player].tech_applications[TECH_APP_ADVANCED_DAMAGE_CONTROL] != TECH_RESEARCH_STATUS_RESEARCHED && game::current_state.players[player].traits[TRAIT_CYBERNETIC] == 0))) {
                    if (MOX::settings.strategic_combat_flag == 1) {
                        ship->structural_damage = MOX::qcombat_data[i].structural_damage;
                    } else {
                        int32_t struct_dmg = MOX::qcombat_data[i].structural_damage;
                        int32_t max_hits = MOX::qcombat_data[i].max_hits;
                        int32_t percentage = (struct_dmg * 100) / max_hits;
                        int32_t structure = INITSHIP::Get_Ship_Structure(*ship, MOX::settings.strategic_combat_flag != 0);
                        ship->structural_damage = (structure * percentage) / 100;
                    }
                } else {
                    MOX::qcombat_data[i].structural_damage = 0;
                    ship->structural_damage = 0;
                }
            }

            if (MOX::qcombat_data[i].status != 0 && ship) {
                if (MOX::attacking_faction.code != factions::Legacy_Code(MOX::qcombat_data[i].player)) {
                    size_sum += MOX::qcombat_data[i].size + 1;
                }

                int16_t officer_idx = MOX::qcombat_data[i].officer_idx;
                if (officer_idx != -1) {
                    if (current_state.leaders.at(officer_idx).eta <= 0) {
                        if (game::current_state.players[MOX::qcombat_data[i].player.code].tech_applications[TECH_APP_SURVIVAL_PODS] == TECH_RESEARCH_STATUS_RESEARCHED) {
                            survived_officer_ids[survived_officers_count] = officer_idx;
                            survived_officers_count++;
                        } else {
                            OFFICER::Kill_Officer(officer_idx);
                        }
                    } else {
                        OFFICER::Move_Officer_To_Pool(officer_idx);
                        ship->officer_index = -1;
                    }
                }
            }
        }

        if (MOX::attacking_faction.code < MAX_PLAYERS && MOX::defending_faction.code < MAX_PLAYERS) {
            if (MOX::cmbt_colony_id != game::NO_COLONY) {
                NETMOX::Russ_Change_Relations(-10000, MOX::attacking_faction.code, MOX::defending_faction.code, 10, star_idx, 1);
            }
            NETMOX::Russ_Change_Relations(size_sum * -15, MOX::attacking_faction.code, MOX::defending_faction.code, 11, star_idx, 0);
        }

        if (survived_officers_count > 0) {
            for (int16_t j = 0; j < survived_officers_count; j++) {
                bool found = false;
                bool move_to_pool = false;
                int16_t officer_idx = survived_officer_ids[j];
                auto& leader = current_state.leaders.at(officer_idx);
                int16_t player_idx = legacy_world::Encode_Player_Slot(leader.employer);

                for (int16_t i = 1; i < MOX::combat_total_ship_count; i++) {
                    const auto key = MOX::qcombat_data[i].ship_id;
                    auto* ship = game::Find_Ship(game::current_state, key);
                    if (MOX::qcombat_data[i].status == 0 && ship) {
                        if ((uint8_t)factions::Legacy_Code(MOX::qcombat_data[i].player) == player_idx) {
                            if (MOX::qcombat_data[i].officer_idx == -1) {
                                found = true;
                                ship->officer_index = officer_idx;
                                MOX::qcombat_data[i].officer_idx = officer_idx;
                                if (auto* original = game::Find_Leader_Ship(current_state, leader)) original->officer_index = -1;
                                game::Set_Leader_Ship(leader, *ship, -1);
                                break;
                            }
                            move_to_pool = true;
                        }
                    }
                }

                if (!found) {
                    if (move_to_pool) {
                        OFFICER::Move_Officer_To_Pool(officer_idx);
                    } else {
                        OFFICER::Kill_Officer(officer_idx);
                    }
                }
            }
        }

        for (int i = 0; i < MOX::combat_total_ship_count; i++) {
            if (star_idx == game::ANTARES_POSITION && factions::Legacy_Code(MOX::qcombat_data[i].player) == 8 && MOX::qcombat_data[i].status != 0 && MOX::qcombat_data[i].size <= 5) {
                antarans::Kill_Antaran_Defensive_Ship(MOX::qcombat_data[i].size);
            } else {
                auto* ship = game::Find_Ship(game::current_state, MOX::qcombat_data[i].ship_id);
                if (MOX::qcombat_data[i].status != 0 && ship) {
                    if (factions::Legacy_Code(MOX::qcombat_data[i].player) == 8) {
                        antarans::Kill_Antaran_Offensive_Ship(MOX::qcombat_data[i].size);
                    }
                    if (ship->officer_index != -1) {
                        OFFICER::Move_Officer_To_Pool(ship->officer_index);
                    }
                    SHIPS::Kill_Ship(*ship);
                }
            }
        }

        if (result >= 1 && MOX::attacking_faction.code < MAX_PLAYERS && MOX::defending_faction.code >= 10) {
            const auto* ship = plr2_ships.empty() ? nullptr : game::Find_Ship(game::current_state, plr2_ships.front());
            if (ship && ship->d.ftl_type > 0) {
                NETMOX::Russ_Event_Monster_Update(MOX::defending_faction.code, game::NO_PLANET, MOX::attacking_faction.code);
                result = 1;
            }
        }

        if (result <= -1 && MOX::defending_faction.code < MAX_PLAYERS && MOX::attacking_faction.code >= 10) {
            const auto* ship = game::Find_Ship(game::current_state, plr1_ships.front());
            if (ship && ship->d.ftl_type > 0) {
                NETMOX::Russ_Event_Monster_Update(MOX::attacking_faction.code, game::NO_PLANET, MOX::defending_faction.code);
                result = -1;
            }
        }

        Build_End_Of_Combat_Message(summary, summary_size, details, details_size, star_idx, colony_defense_flags, result);

        if (MOX::cmbt_colony_id != game::NO_COLONY) {
            if (game::current_state.colonies.at(MOX::cmbt_colony_id).n_pops == 0) {
                int16_t has_buildings = 0;
                for (int i = 0; i < BUILDING_COUNT; i++) {
                    has_buildings += game::current_state.colonies.at(MOX::cmbt_colony_id).buildings[i];
                }

                if (has_buildings == 0) {
                    BOMB::Destroy_Colony(game::current_state, MOX::cmbt_colony_id, 1);
                }
            }
        }

        MOX2::Put_Auto_Function(&auto_fn);
        return result;
    }

    void Get_Leader_Combat_Bonuses(std::span<const game::id_ship> keys, int16_t* helmsman_max, int16_t* ordnance_max, int16_t* engineer_max, int16_t* weaponry_max, int16_t* lore_max) {
        for (const auto key : keys) {
            const auto* ship = game::Find_Ship(game::current_state, key);
            if (!ship) continue;
            const auto officer_idx = ship->officer_index;

            if (officer_idx != -1 && current_state.leaders.at(officer_idx).eta == 0) {
                int16_t bonus;

                bonus = CMBTFIR2::Helmsman_Bonus(officer_idx);
                if (*helmsman_max < bonus) {
                    *helmsman_max = bonus;
                }

                bonus = CMBTFIR2::Leader_Ordnance_Bonus(officer_idx);
                if (*ordnance_max < bonus) {
                    *ordnance_max = bonus;
                }

                bonus = CMBTFIR2::Engineer_Bonus(officer_idx);
                if (*engineer_max < bonus) {
                    *engineer_max = bonus;
                }

                bonus = CMBTFIR2::Weaponry_Bonus(officer_idx);
                if (*weaponry_max < bonus) {
                    *weaponry_max = bonus;
                }

                bonus = CMBTFIR2::Galactic_Lore_Bonus(officer_idx);
                if (*lore_max < bonus) {
                    *lore_max = bonus;
                }
            }
        }
    }

    void Qload_Ships(std::span<const game::id_ship> keys, int16_t beam_defense_bonus, int16_t damage_bonus, int16_t base_ocv_bonus, int16_t unused_lore_bonus) {
        int16_t player_idx;
        int16_t warp_bonus;
        int16_t best_armor_id;
        int16_t armor_absorption;
        int16_t special_ocv = 0;
        int16_t special_missile_dmg = 0;
        int16_t special_beam_def = 0;
        int16_t special_missile_def = 0;
        int16_t special_beam_dmg = 0;

        if (keys.empty()) {
            player_idx = legacy_world::Encode_Player_Slot(game::current_state.colonies.at(MOX::cmbt_colony_id).owner);
        } else {
            const auto* lead = game::Find_Ship(game::current_state, keys.front());
            if (!lead) return;
            player_idx = factions::Legacy_Code(lead->owner);
        }

        int16_t best_missile = INITSHIP::Best_Missile_Weapon(player_idx);
        int16_t best_beam = INITSHIP::Best_Beam_Weapon(player_idx);
        int16_t best_bomb = INITSHIP::Best_Bomb_Weapon(player_idx);
        int16_t best_shield = INITSHIP::Best_Ship_Shield(factions::From_Legacy(player_idx));
        int16_t base_ocv = AIPOWER::Base_Generic_Ocv(&game::current_state, legacy_world::Player_Id(player_idx));
        int16_t best_warp = INITSHIP::Best_Warp_Drive(factions::From_Legacy(player_idx));

        warp_bonus = (best_warp - 1) * 10;
        if (player_idx < MAX_PLAYERS) {
            warp_bonus += game::current_state.players[player_idx].traits[6];
        }

        best_armor_id = INITSHIP::Best_Armor(factions::From_Legacy(player_idx));
        armor_absorption = TECHDATA::armor[best_armor_id].ships_bonus;

        if (!keys.empty()) {
            for (const auto key : keys) {
                auto* ship = game::Find_Ship(game::current_state, key);
                if (ship && ship->d.ship_type == SHIP_TYPE_COMBAT) {
                    uint8_t ship_size = ship->d.size;
                    int16_t best_strat_special = INITSHIP::Best_Strat_Special_Weapon(player_idx, ship_size);
                    auto& world = current_state;
                    INITSHIP::Update_Strategic_Ship(world, MOX::settings.strategic_combat_flag, *ship);

                    s_qship* q = &MOX::qcombat_data[MOX::combat_total_ship_count];
                    q->player = factions::From_Legacy(factions::Legacy_Code(ship->owner));
                    q->ship_id = key;
                    q->size = ship_size;
                    q->status = 0;
                    q->field3_0x4 = -1;
                    q->missile_weapon_id = best_missile;
                    q->beam_weapon_id = best_beam;
                    q->bomb_weapon_id = best_bomb;
                    q->special_weapon_id = best_strat_special;
                    q->shield_id = best_shield;

                    if ((uint8_t)factions::Legacy_Code(q->player) < 10) {
                        q->max_hits = (int16_t)(((int32_t)TECHDATA::hull_data[ship_size].strat_hits * (armor_absorption + 100)) / 100);
                    } else {
                        q->max_hits = TECHDATA::hull_data[ship_size].strat_hits;
                    }

                    q->missile_defense_bonus = 0;
                    q->beam_defense_bonus = TECHDATA::hull_data[ship_size].strat_def_bonus + warp_bonus;
                    q->base_ocv = base_ocv;
                    q->missile_damage_modifier = 0;
                    q->beam_damage_modifier = 0;

                    for (int16_t device_idx = 0; device_idx < 39; ++device_idx) {
                        if (struct_::Read_Bit_Field(ship->d.special_device_flags, device_idx) != 0 &&
                            struct_::Read_Bit_Field(ship->special_device_damage_flags, device_idx) == 0) {
                            int16_t bonus = TECHDATA::specials[device_idx].strategic;
                            switch (TECHDATA::specials[device_idx].category) {
                                case SPECIAL_CATEGORY_BEAM_ACCURACY:
                                    if (special_ocv < bonus) special_ocv = bonus;
                                    break;
                                case SPECIAL_CATEGORY_BEAM_DAMAGE:
                                    if (special_beam_dmg < bonus) special_beam_dmg = bonus;
                                    break;
                                case SPECIAL_CATEGORY_BEAM_DEFENSE:
                                    if (special_beam_def < bonus) special_beam_def = bonus;
                                    break;
                                case SPECIAL_CATEGORY_MISSILE_DEFENSE:
                                    if (special_missile_def < bonus) special_missile_def = bonus;
                                    break;
                                case SPECIAL_CATEGORY_MISSILE_DAMAGE:
                                    if (special_missile_dmg < bonus) special_missile_dmg = bonus;
                                    break;
                                case SPECIAL_CATEGORY_BEAM_MISSILE_DEFENSE:
                                    if (special_beam_def < bonus) special_beam_def = bonus;
                                    if (special_missile_def < bonus) special_missile_def = bonus;
                                    break;
                                case SPECIAL_CATEGORY_BEAM_MISSILE_DAMAGE:
                                    if (special_beam_dmg < bonus) special_beam_dmg = bonus;
                                    if (special_missile_dmg < bonus) special_missile_dmg = bonus;
                                    break;
                            }
                        }
                    }

                    q->base_ocv += base_ocv_bonus + special_ocv;
                    q->beam_defense_bonus += beam_defense_bonus + special_beam_def;
                    q->missile_defense_bonus += special_missile_def;
                    q->beam_damage_modifier += damage_bonus + special_beam_dmg;
                    q->missile_damage_modifier += damage_bonus + special_missile_dmg;
                    q->officer_idx = ship->officer_index;
                    q->structural_damage = ship->structural_damage;

                    MOX::combat_total_ship_count++;
                }
            }
        }

        int16_t best_colony_strat_special = INITSHIP::Best_Strat_Special_Weapon(player_idx, 5);
        s_qship* q0 = &MOX::qcombat_data[0];
        q0->size = 4;
        q0->ship_id = game::NO_SHIP;
        q0->field3_0x4 = -1;
        q0->structural_damage = 0;
        q0->max_hits = 0;
        q0->missile_weapon_id = 0;
        q0->beam_weapon_id = 0;
        q0->bomb_weapon_id = 0;
        q0->special_weapon_id = 0;
        q0->missile_defense_bonus = 0;
        q0->beam_defense_bonus = 0;

        if (MOX::cmbt_colony_id == game::NO_COLONY || MOX::defending_faction.code != player_idx) {
            q0->status = 2 /* INACTIVE */;
        } else {
            q0->player = factions::From_Legacy(legacy_world::Encode_Player_Slot(game::current_state.colonies.at(MOX::cmbt_colony_id).owner));
            q0->max_hits = Get_Colony_Hits();
            q0->ship_id = game::NO_SHIP;

            if (game::current_state.colonies.at(MOX::cmbt_colony_id).buildings[BUILDING_RADIATION_SHIELD] != 0) {
                q0->missile_defense_bonus += 10;
                q0->beam_defense_bonus = (int16_t)(q0->beam_defense_bonus * 10);
            }
            if (game::current_state.colonies.at(MOX::cmbt_colony_id).buildings[BUILDING_BARRIER_SHIELD] != 0) {
                q0->missile_defense_bonus += 30;
                q0->beam_defense_bonus = (int16_t)(q0->beam_defense_bonus * 30);
            }
            if (game::current_state.colonies.at(MOX::cmbt_colony_id).buildings[BUILDING_FLUX_SHIELD] != 0) {
                q0->missile_defense_bonus += 20;
                q0->beam_defense_bonus = (int16_t)(q0->beam_defense_bonus * 20);
            }

            q0->missile_weapon_id = 0;
            if (game::current_state.colonies.at(MOX::cmbt_colony_id).buildings[BUILDING_MISSILE_BASE] != 0) {
                q0->missile_weapon_id = best_missile;
                MOX::colony_defense_flags += 1;
            }
            if (game::current_state.colonies.at(MOX::cmbt_colony_id).buildings[BUILDING_FIGHTER_GARRISON] != 0) {
                q0->special_weapon_id = best_colony_strat_special;
                MOX::colony_defense_flags += 8;
            }
            if (game::current_state.colonies.at(MOX::cmbt_colony_id).buildings[BUILDING_GROUND_BATTERIES] != 0) {
                q0->beam_weapon_id = best_beam;
                MOX::colony_defense_flags += 2;
            }
            if (game::current_state.colonies.at(MOX::cmbt_colony_id).buildings[BUILDING_STELLAR_CONVERTER] != 0) {
                MOX::colony_defense_flags += 4;
            }

            if (MOX::colony_defense_flags == 0) {
                q0->status = 2 /* INACTIVE */;
            } else {
                q0->status = 0 /* ACTIVE */;
            }

            s_colony* colony_ptr = &game::current_state.colonies.at(MOX::cmbt_colony_id);
            if (colony_ptr->buildings[BUILDING_STAR_BASE] != 0) {
                s_qship* qs = &MOX::qcombat_data[MOX::combat_total_ship_count];
                qs->player = factions::From_Legacy(legacy_world::Encode_Player_Slot(colony_ptr->owner));
                qs->ship_id = game::NO_SHIP;
                qs->size = HULL_STAR_BASE;
                qs->status = 0;
                qs->field3_0x4 = -1;
                qs->missile_weapon_id = best_missile;
                qs->beam_weapon_id = best_beam;
                qs->bomb_weapon_id = best_bomb;
                qs->special_weapon_id = best_colony_strat_special;
                qs->shield_id = best_shield;
                qs->max_hits = (int16_t)(((armor_absorption + 100) * (int32_t)TECHDATA::hull_data[HULL_STAR_BASE].strat_hits) / 100);
                qs->missile_defense_bonus = 0;
                qs->beam_defense_bonus = TECHDATA::hull_data[HULL_STAR_BASE].strat_def_bonus;
                qs->base_ocv = base_ocv;
                qs->structural_damage = 0;
                MOX::combat_total_ship_count++;
                return;
            }
            if (colony_ptr->buildings[BUILDING_BATTLESTATION] != 0) {
                s_qship* qs = &MOX::qcombat_data[MOX::combat_total_ship_count];
                qs->player = factions::From_Legacy(legacy_world::Encode_Player_Slot(colony_ptr->owner));
                qs->ship_id = game::NO_SHIP;
                qs->size = HULL_BATTLESTATION;
                qs->status = 0;
                qs->field3_0x4 = -1;
                qs->missile_weapon_id = best_missile;
                qs->beam_weapon_id = best_beam;
                qs->bomb_weapon_id = best_bomb;
                qs->special_weapon_id = best_colony_strat_special;
                qs->shield_id = best_shield;
                qs->max_hits = (int16_t)(((armor_absorption + 100) * (int32_t)TECHDATA::hull_data[HULL_BATTLESTATION].strat_hits) / 100);
                qs->missile_defense_bonus = 0;
                qs->beam_defense_bonus = TECHDATA::hull_data[HULL_BATTLESTATION].strat_def_bonus;
                qs->base_ocv = base_ocv;
                qs->structural_damage = 0;
                MOX::combat_total_ship_count++;
                return;
            }
            if (colony_ptr->buildings[BUILDING_STAR_FORTRESS] != 0) {
                s_qship* qs = &MOX::qcombat_data[MOX::combat_total_ship_count];
                qs->player = factions::From_Legacy(legacy_world::Encode_Player_Slot(colony_ptr->owner));
                qs->ship_id = game::NO_SHIP;
                qs->size = HULL_STAR_FORTRESS;
                qs->status = 0;
                qs->field3_0x4 = -1;
                qs->missile_weapon_id = best_missile;
                qs->beam_weapon_id = best_beam;
                qs->bomb_weapon_id = best_bomb;
                qs->special_weapon_id = best_colony_strat_special;
                qs->shield_id = best_shield;
                qs->max_hits = (int16_t)(((armor_absorption + 100) * (int32_t)TECHDATA::hull_data[HULL_STAR_FORTRESS].strat_hits) / 100);
                qs->missile_defense_bonus = 0;
                qs->beam_defense_bonus = TECHDATA::hull_data[HULL_STAR_FORTRESS].strat_def_bonus;
                qs->base_ocv = base_ocv;
                qs->structural_damage = 0;
                MOX::combat_total_ship_count++;
                return;
            }
        }
    }

    void __cdecl QLoad_Combat_Antaran_Ship(int16_t q_idx, int16_t size_idx) {
        int16_t missile_id;
        int16_t beam_id;
        int16_t bomb_id;
        int16_t shield_id;
        int16_t ocv;
        int16_t armor_idx;
        int16_t absorption_percent;
        int16_t special_weapon_id;

        missile_id = INITSHIP::Best_Missile_Weapon(8);
        beam_id = INITSHIP::Best_Beam_Weapon(8);
        bomb_id = INITSHIP::Best_Bomb_Weapon(8);
        shield_id = INITSHIP::Best_Ship_Shield(factions::From_Legacy(8));
        ocv = AIPOWER::Base_Generic_Ocv(&game::current_state, legacy_world::Player_Id(8));
        armor_idx = INITSHIP::Best_Armor(factions::From_Legacy(8));

        // Explicit lookup based on struct layout and assembly
        absorption_percent = TECHDATA::armor[armor_idx].ships_bonus;

        special_weapon_id = INITSHIP::Best_Strat_Special_Weapon(8, size_idx);

        s_qship& ship = MOX::qcombat_data[q_idx];

        ship.player = factions::From_Legacy(8);
        ship.ship_id = game::NO_SHIP;
        ship.size = (uint8_t)size_idx;
        ship.status = 0;
        ship.field3_0x4 = -1;
        ship.missile_weapon_id = missile_id;
        ship.beam_weapon_id = beam_id;
        ship.bomb_weapon_id = bomb_id;
        ship.special_weapon_id = special_weapon_id;
        ship.shield_id = shield_id;

        if (factions::Legacy_Code(ship.player) <= 9) {
            ship.max_hits = (int16_t)(((int32_t)TECHDATA::hull_data[size_idx].strat_hits * (absorption_percent + 100)) / 100);
        } else {
            ship.max_hits = TECHDATA::hull_data[size_idx].strat_hits;
        }

        ship.missile_defense_bonus = 0;
        ship.beam_defense_bonus = TECHDATA::hull_data[size_idx].strat_def_bonus + 95 /* 0x5f */;
        ship.base_ocv = ocv + 45 /* 0x2d */;
        ship.missile_damage_modifier = 0;
        ship.beam_damage_modifier = 105 /* 0x69 */;
        ship.officer_idx = -1;
        ship.structural_damage = 0;
    }

    void __cdecl QLoad_Antaran_Defense_Fleet() {
        if (MOX::antarans.defensive_ships[4] < 1) {
            MOX::antarans.defensive_ships[4] = 1;
        }

        for (int16_t ship_type = 0; ship_type < 5; ++ship_type) {
            int16_t count = MOX::antarans.defensive_ships[ship_type];
            if (count > 0) {
                for (int16_t i = 0; i < count; ++i) {
                    QLoad_Combat_Antaran_Ship(MOX::combat_total_ship_count, ship_type);
                    MOX::combat_total_ship_count++;
                    MOX::combat_defender_count++;
                }
            }
        }

        s_qship* qship = &MOX::qcombat_data[MOX::combat_total_ship_count];

        qship->player = factions::From_Legacy(8);
        qship->ship_id = game::NO_SHIP;
        qship->size = HULL_STAR_FORTRESS;
        qship->status = 0;
        qship->field3_0x4 = -1;
        qship->missile_weapon_id = INITSHIP::Best_Missile_Weapon(8);
        qship->beam_weapon_id = INITSHIP::Best_Beam_Weapon(8);
        qship->bomb_weapon_id = 0;
        qship->special_weapon_id = 0;
        qship->shield_id = 7;
        qship->max_hits = (int16_t)(((int32_t)TECHDATA::hull_data[HULL_STAR_FORTRESS].strat_hits * 1000) / 100);
        qship->missile_defense_bonus = 0;
        qship->beam_defense_bonus = TECHDATA::hull_data[HULL_STAR_FORTRESS].strat_def_bonus + 45 /* 0x2d */;
        qship->base_ocv = AIPOWER::Base_Generic_Ocv(&game::current_state, legacy_world::Player_Id(8)) + 45 /* 0x2d */;
        qship->missile_damage_modifier = 0;
        qship->beam_damage_modifier = 105 /* 0x69 */;
        qship->structural_damage = 0;

        MOX::combat_total_ship_count++;
        MOX::combat_defender_count++;
    }

    void __cdecl Message_Box_Titled(char* summary, char* details) {
        TEXTBOX::Do_Text_Box(summary, details, 0);
    }

    void __cdecl Resolve_Strat_Colony_Damage() {
        if ((uint8_t)MOX::qcombat_data->status <= 1) {
            if ((MOX::qcombat_data->structural_damage / 40 > 0) && (MOX::cmbt_colony_id != game::NO_COLONY)) {
                // Check and destroy Missile Base
                if ((MOX::qcombat_data->structural_damage / 40 > 0) && (MOX::colony_defense_flags & 0x4)) {
                    MOX::colony_defense_flags ^= 0x4;
                    ERICNET::Remove_Building(game::current_state, MOX::cmbt_colony_id, BUILDING_STELLAR_CONVERTER);
                    MOX::qcombat_data->structural_damage -= 40;
                }

                // Check and destroy Star Base
                if ((MOX::qcombat_data->structural_damage / 40 > 0) && (MOX::colony_defense_flags & 0x1)) {
                    MOX::colony_defense_flags ^= 0x1;
                    ERICNET::Remove_Building(game::current_state, MOX::cmbt_colony_id, BUILDING_MISSILE_BASE);
                    MOX::qcombat_data->structural_damage -= 40;
                }

                // Check and destroy Battle Station
                if ((MOX::qcombat_data->structural_damage / 40 > 0) && (MOX::colony_defense_flags & 0x2)) {
                    MOX::colony_defense_flags ^= 0x2;
                    ERICNET::Remove_Building(game::current_state, MOX::cmbt_colony_id, BUILDING_GROUND_BATTERIES);
                    MOX::qcombat_data->missile_defense_bonus -= 10;
                    MOX::qcombat_data->structural_damage -= 40;
                }

                // Check and destroy Star Fortress
                if ((MOX::qcombat_data->structural_damage / 40 > 0) && (MOX::colony_defense_flags & 0x8)) {
                    MOX::colony_defense_flags ^= 0x8;
                    ERICNET::Remove_Building(game::current_state, MOX::cmbt_colony_id, BUILDING_FIGHTER_GARRISON);
                    MOX::qcombat_data->structural_damage -= 40;
                }

                // Any remaining strategic damage less than 40 is kept
                MOX::qcombat_data->structural_damage %= 40;

                // If all defenses are gone, mark the strategic combat entity as destroyed
                if ((MOX::colony_defense_flags == 0) && (MOX::qcombat_data->status == 0)) {
                    MOX::qcombat_data->status = 1;
                }
            }
        }
    }

    int16_t __cdecl Combat_Satelite_Check(uint8_t type) {
        if (type == BUILDING_BATTLESTATION || type == BUILDING_STAR_BASE || type == BUILDING_STAR_FORTRESS) {
            return 1;
        }
        return 0;
    }

    /**
     * Calculates total hits for a colony during combat based on population, 
     * military units, and non-satellite buildings.
     */
    int16_t __cdecl Get_Colony_Hits() {
        s_colony* colony = &game::current_state.colonies.at(MOX::cmbt_colony_id);

        // Start hits count with population and military units (tanks/infantry)
        // Note: population is treated as unsigned byte here.
        int16_t hits = (uint8_t)colony->n_pops + colony->military[0] + colony->military[1];

        // Add 1 hit for every building that is not a combat satellite
        for (uint8_t i = 0; i < BUILDING_COUNT; ++i) {
            if (colony->buildings[i] != 0) {
                if (Combat_Satelite_Check(i) == 0) {
                    hits++;
                }
            }
        }

        // Each "hit" unit represents 40 actual hit points
        return hits * 40;
    }

    void __cdecl Get_Non_Player_Name(int16_t player_idx, char* out_name, int32_t out_name_size) {
        if (out_name_size <= 0) {
            return;
        }

        const char* name = "";

        switch (player_idx) {
            case 8:
                name = s_Antaran_00557e54;
                break;
            case 9:
                name = s_Guardian_00557e48;
                break;
            case 10:
                name = s_Amoeba_00557e40;
                break;
            case 11:
                name = s_Crystal_00557e24;
                break;
            case 12:
                name = s_Dragon_00557e38;
                break;
            case 13:
                name = s_Eel_00557e2c;
                break;
            case 14:
                name = s_Hydra_00557e30;
                break;
            default:
                break;
        }

        strlcpy(out_name, name, (size_t)out_name_size);
    }

    void __cdecl Build_End_Of_Combat_Message(char* result_msg, int32_t result_msg_size, char* stats_msg, int32_t stats_msg_size, int16_t star_idx, int16_t colony_damage_flags, int16_t outcome_type) {
        if (result_msg_size <= 0 || stats_msg_size <= 0) {
            return;
        }

        const size_t result_capacity = (size_t)result_msg_size;
        const size_t stats_capacity = (size_t)stats_msg_size;
        int16_t attacker_total_ships[SHIP_SIZE_COUNT];
        int16_t attacker_lost_ships[SHIP_SIZE_COUNT];
        int16_t defender_total_ships[SHIP_SIZE_COUNT];
        int16_t defender_lost_ships[SHIP_SIZE_COUNT];
        int16_t special_hull_idx = -1;
        int16_t special_alive = 1;
        char attacker_race_name[20];
        char defender_race_name[20];
        char itoa_buffer[12];
        char result_prefix[80];
        char msg_buffer[80];

        for (int i = 0; i < SHIP_SIZE_COUNT; ++i) {
            attacker_total_ships[i] = 0;
            attacker_lost_ships[i] = 0;
            defender_total_ships[i] = 0;
            defender_lost_ships[i] = 0;
        }

        if (MOX::combat_total_ship_count > 1) {
            for (int i = 1; i < MOX::combat_total_ship_count; ++i) {
                s_qship* ship = &MOX::qcombat_data[i];
                uint8_t size = ship->size;
                if (size < SHIP_SIZE_COUNT) {
                    if (factions::Legacy_Code(ship->player) == MOX::attacking_faction.code) {
                        if (ship->status == 0) {
                            attacker_total_ships[size]++;
                        } else {
                            attacker_lost_ships[size]++;
                        }
                    } else {
                        if (ship->status == 0) {
                            defender_total_ships[size]++;
                        } else {
                            defender_lost_ships[size]++;
                        }
                    }
                } else {
                    special_hull_idx = (int16_t)size;
                    special_alive = (ship->status == 0);
                }
            }
        }

        for (int i = 0; i < SHIP_SIZE_COUNT; ++i) {
            attacker_total_ships[i] += attacker_lost_ships[i];
            defender_total_ships[i] += defender_lost_ships[i];
        }

        KEN::Ken_Get_Text_Message(98 /* 0x62 */, result_prefix, 80);
        strlcpy(result_msg, result_prefix, result_capacity);

        const char* location_name;
        if (star_idx == game::ANTARES_POSITION) {
            location_name = string::s_Antares;
        } else if (MOX::cmbt_colony_id == game::NO_COLONY) {
            location_name = game::current_state.stars.at(game::Star_Key_At_Row(current_state, star_idx)).name;
        } else {
            location_name = HACCESS::Get_Planet_Name(game::current_state, game::current_state.colonies.at(MOX::cmbt_colony_id).planet_id);
        }
        strlcat(result_msg, location_name, result_capacity);

        if (MOX::attacking_faction.code < MAX_PLAYERS) {
            strlcpy(attacker_race_name, game::current_state.players[MOX::attacking_faction.code].race_name, sizeof(attacker_race_name));
        } else {
            Get_Non_Player_Name(MOX::attacking_faction.code, attacker_race_name, sizeof(attacker_race_name));
        }

        if (MOX::defending_faction.code < MAX_PLAYERS) {
            strlcpy(defender_race_name, game::current_state.players[MOX::defending_faction.code].race_name, sizeof(defender_race_name));
        } else {
            Get_Non_Player_Name(MOX::defending_faction.code, defender_race_name, sizeof(defender_race_name));
        }

        strlcpy(stats_msg, string::s_X35, stats_capacity);
        strlcat(stats_msg, attacker_race_name, stats_capacity);
        strlcat(stats_msg, string::s_X255, stats_capacity);
        strlcat(stats_msg, defender_race_name, stats_capacity);
        strlcat(stats_msg, string::s_Newline, stats_capacity);

        KEN::Ken_Get_Text_Message(99 /* 0x63 */, msg_buffer, 80);
        strlcat(stats_msg, msg_buffer, stats_capacity);
        strlcat(stats_msg, string::s_Xplus25, stats_capacity);

        KEN::Ken_Get_Text_Message(100 /* 0x64 */, msg_buffer, 80);
        strlcat(stats_msg, msg_buffer, stats_capacity);
        strlcat(stats_msg, string::s_X227, stats_capacity);

        KEN::Ken_Get_Text_Message(99 /* 0x63 */, msg_buffer, 80);
        strlcat(stats_msg, msg_buffer, stats_capacity);
        strlcat(stats_msg, string::s_Xplus21, stats_capacity);

        KEN::Ken_Get_Text_Message(100 /* 0x64 */, msg_buffer, 80);
        strlcat(stats_msg, msg_buffer, stats_capacity);
        strlcat(stats_msg, string::s_Newline, stats_capacity);

        for (int i = 0; i < SHIP_SIZE_COUNT; ++i) {
            if (attacker_total_ships[i] + defender_total_ships[i] > 0) {
                strlcat(stats_msg, string::s_X18, stats_capacity);
                if (attacker_total_ships[i] == 0) {
                    strlcat(stats_msg, string::s_Dash, stats_capacity);
                } else {
                    snprintf(itoa_buffer, sizeof(itoa_buffer), "%i", attacker_total_ships[i]);
                    strlcat(stats_msg, itoa_buffer, stats_capacity);
                }

                strlcat(stats_msg, string::s_X80, stats_capacity);
                if (attacker_lost_ships[i] == 0) {
                    strlcat(stats_msg, string::s_Dash, stats_capacity);
                } else {
                    snprintf(itoa_buffer, sizeof(itoa_buffer), "%i", attacker_lost_ships[i]);
                    strlcat(stats_msg, itoa_buffer, stats_capacity);
                }

                strlcat(stats_msg, string::s_X120, stats_capacity);
                strlcat(stats_msg, TECHDATA::hull_data[i].size_name, stats_capacity);

                strlcat(stats_msg, string::s_X250, stats_capacity);
                if (defender_total_ships[i] == 0) {
                    strlcat(stats_msg, string::s_Dash, stats_capacity);
                } else {
                    snprintf(itoa_buffer, sizeof(itoa_buffer), "%i", defender_total_ships[i]);
                    strlcat(stats_msg, itoa_buffer, stats_capacity);
                }

                strlcat(stats_msg, string::s_X312, stats_capacity);
                if (defender_lost_ships[i] == 0) {
                    strlcat(stats_msg, string::s_Dash, stats_capacity);
                } else {
                    snprintf(itoa_buffer, sizeof(itoa_buffer), "%i", defender_lost_ships[i]);
                    strlcat(stats_msg, itoa_buffer, stats_capacity);
                }
                strlcat(stats_msg, string::s_Newline, stats_capacity);
            }
        }

        if (special_hull_idx != -1) {
            strlcat(stats_msg, string::s_X120, stats_capacity);
            strlcat(stats_msg, TECHDATA::hull_data[special_hull_idx].name, stats_capacity);
            strlcat(stats_msg, string::s_X250, stats_capacity);
            strlcat(stats_msg, string::s_1, stats_capacity);
            strlcat(stats_msg, string::s_X312, stats_capacity);
            if (special_alive) {
                strlcat(stats_msg, string::s_Dash, stats_capacity);
            } else {
                strlcat(stats_msg, string::s_1, stats_capacity);
            }
            strlcat(stats_msg, string::s_Newline, stats_capacity);
        }

        if (colony_damage_flags & 0x1) {
            strlcat(stats_msg, string::s_X120, stats_capacity);
            strlcat(stats_msg, TECHDATA::buildings[BUILDING_MISSILE_BASE].name, stats_capacity);
            strlcat(stats_msg, string::s_X250, stats_capacity);
            strlcat(stats_msg, string::s_1, stats_capacity);
            strlcat(stats_msg, string::s_X312, stats_capacity);
            if (MOX::colony_defense_flags & 0x1) {
                strlcat(stats_msg, string::s_Dash, stats_capacity);
            } else {
                strlcat(stats_msg, string::s_1, stats_capacity);
            }
            strlcat(stats_msg, string::s_Newline, stats_capacity);
        }

        if (colony_damage_flags & 0x2) {
            strlcat(stats_msg, string::s_X120, stats_capacity);
            strlcat(stats_msg, TECHDATA::buildings[BUILDING_GROUND_BATTERIES].name, stats_capacity);
            strlcat(stats_msg, string::s_X250, stats_capacity);
            strlcat(stats_msg, string::s_1, stats_capacity);
            strlcat(stats_msg, string::s_X312, stats_capacity);
            if (MOX::colony_defense_flags & 0x2) {
                strlcat(stats_msg, string::s_Dash, stats_capacity);
            } else {
                strlcat(stats_msg, string::s_1, stats_capacity);
            }
            strlcat(stats_msg, string::s_Newline, stats_capacity);
        }

        if (colony_damage_flags & 0x8) {
            strlcat(stats_msg, string::s_X120, stats_capacity);
            strlcat(stats_msg, TECHDATA::buildings[BUILDING_FIGHTER_GARRISON].name, stats_capacity);
            strlcat(stats_msg, string::s_X250, stats_capacity);
            strlcat(stats_msg, string::s_1, stats_capacity);
            strlcat(stats_msg, string::s_X312, stats_capacity);
            if (MOX::colony_defense_flags & 0x8) {
                strlcat(stats_msg, string::s_Dash, stats_capacity);
            } else {
                strlcat(stats_msg, string::s_1, stats_capacity);
            }
            strlcat(stats_msg, string::s_Newline, stats_capacity);
        }

        if (colony_damage_flags & 0x4) {
            strlcat(stats_msg, string::s_X120, stats_capacity);
            strlcat(stats_msg, TECHDATA::buildings[BUILDING_STELLAR_CONVERTER].name, stats_capacity);
            strlcat(stats_msg, string::s_X250, stats_capacity);
            strlcat(stats_msg, string::s_1, stats_capacity);
            strlcat(stats_msg, string::s_X312, stats_capacity);
            if (MOX::colony_defense_flags & 0x4) {
                strlcat(stats_msg, string::s_Dash, stats_capacity);
            } else {
                strlcat(stats_msg, string::s_1, stats_capacity);
            }
            strlcat(stats_msg, string::s_Newline, stats_capacity);
        }

        strlcat(stats_msg, string::s_Newline2, stats_capacity);
        strlcat(stats_msg, string::s_2, stats_capacity);
        if (outcome_type > 0) {
            KEN::Ken_Get_Text_Message(101 /* 0x65 */, msg_buffer, 80);
        } else if (outcome_type < 0) {
            KEN::Ken_Get_Text_Message(102 /* 0x66 */, msg_buffer, 80);
        } else {
            KEN::Ken_Get_Text_Message(103 /* 0x67 */, msg_buffer, 80);
        }
        strlcat(stats_msg, msg_buffer, stats_capacity);
    }

    int16_t Strategic_Bombardment(std::span<const game::id_ship> attackers, game::id_colony colony_id) {
        const auto* lead = attackers.empty() ? nullptr : game::Find_Ship(game::current_state, attackers.front());
        if (!lead) return 0;
        MOX::cmbt_colony_id = colony_id;
        MOX::qcombat_data = static_cast<s_qship*>(allocate::Allocate_First_Block_Aligned(MOX::screen_seg, kQCombatDataSize, alignof(s_qship)));
        MOX::combat_attacker_count = static_cast<int16_t>(attackers.size());
        MOX::combat_defender_count = 0;

        MOX::attacking_faction = lead->owner;
        MOX::defending_faction = factions::From_Player(game::current_state.colonies.at(colony_id).owner);

        MOX::combat_total_ship_count = 1;
        MOX::colony_defense_flags = 0;

        COMBAT::Qload_Ships(attackers, 0, 0, 0, 0);
        COMBAT::Qload_Ships({}, 0, 0, 0, 0);

        int16_t rounds = 0;
        do {
            int16_t ship_idx = 1;
            if (MOX::combat_total_ship_count > 1) {
                do {
                    COMBAT::Missile_Attack(ship_idx, 0);
                    COMBAT::Beam_Attack(ship_idx, 0);
                    COMBAT::Strat_Bomb_Attack(ship_idx);

                    if (MOX::qcombat_data->structural_damage > 30000) {
                        rounds = 6;
                        break;
                    }
                    ship_idx++;
                } while (ship_idx < MOX::combat_total_ship_count);
            }
            rounds++;
        } while (rounds < 3);

        return MOX::qcombat_data->structural_damage / 40 /* 0x28 */;
    }

    void __cdecl Bombardment(s_combat_bomb_result* result) {
        struct_::Clear_Structure(&MOX::combat_bomb_result, sizeof(s_combat_bomb_result));

        const auto colony_idx = result->colony_id.get();
        std::vector<game::id_ship> ships;
        const auto planet_idx = current_state.colonies.at(colony_idx).planet_id;
        const auto star_row = game::Star_Row(current_state, current_state.planets.at(planet_idx).star_id);
        for (const auto key : game::Ships_In_Battle_Orbit(current_state, star_row, game::Received_Combat_Order(current_state, star_row))) {
            const auto* ship = game::Find_Ship(std::as_const(current_state), key);
            if (ship && ship->owner == result->attacker.get() && ship->status == 0 && ship->d.ship_type == SHIP_TYPE_COMBAT) {
                ships.push_back(key);
            }
        }

        if (MOX::settings.strategic_combat_flag == 1) {
            int16_t remaining = Strategic_Bombardment(ships, colony_idx);

            result->bomb_hits_remaining = remaining;
            result->is_planetary_hit = 1;
            result->bio_hits_remaining = 0;
            result->is_bio_weapon = 0;
        } else {
            Tactical_Bombardment(ships, result);
        }
    }

} // namespace COMBAT
