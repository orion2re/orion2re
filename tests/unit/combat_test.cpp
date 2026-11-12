#include "pch.h"

#include "doctest/doctest.h"
#include "game/combat.h"
#include "lua/lua_game.h"

namespace {
    constexpr int16_t kAttacker = 2;
    constexpr int16_t kBeamWeapon = 1;
    constexpr int16_t kMissileWeapon = 2;
    constexpr int16_t kTorpedoWeapon = 3;
    constexpr int16_t kBombWeapon = 4;
    constexpr int16_t kBestPointDefenseWeapon = 5;
    constexpr int16_t kBestBombWeapon = 6;

    s_combat_data combat_data[MAX_COMBAT_SHIPS];
    s_missile missiles[MAX_COMBAT_FLYERS];
    s_ship_data ships[MAX_SHIPS];
    s_colony colonies[1];
    s_player players[MAX_PLAYERS];
    s_qship qcombat_data[MAX_SHIPS + 1];
    uint32_t random_result;
    uint32_t random_bound;
    int random_calls;
    int allocation_count;
    int16_t beam_damage_adjustment;
    int beam_damage_calls;
    orion2re::lua::game::s_combat_beam_damage_context beam_damage_contexts[3];
} // namespace

namespace TEST_BOMBARDMENT_AMMO {
    void Reset_() {
        memset(combat_data, 0, sizeof(combat_data));
        memset(missiles, 0, sizeof(missiles));
        memset(ships, 0, sizeof(ships));
        memset(colonies, 0, sizeof(colonies));
        memset(players, 0, sizeof(players));
        memset(qcombat_data, 0, sizeof(qcombat_data));
        memset(TECHDATA::hull_data, 0, sizeof(TECHDATA::hull_data));
        memset(TECHDATA::shields, 0, sizeof(TECHDATA::shields));
        memset(TECHDATA::weapons, 0, sizeof(TECHDATA::weapons));
        memset(MOX::fighter_beam_type, 0, sizeof(MOX::fighter_beam_type));
        memset(MOX::fighter_bomb_type, 0, sizeof(MOX::fighter_bomb_type));

        ships[0].owner = kAttacker;
        ships[0].d.ship_type = SHIP_TYPE_COMBAT;
        colonies[0].owner = 1;

        TECHDATA::weapons[kBeamWeapon].type = WEAPON_TYPE_BEAM;
        TECHDATA::weapons[kMissileWeapon].type = WEAPON_TYPE_MISSILE;
        TECHDATA::weapons[kTorpedoWeapon].type = WEAPON_TYPE_TORPEDO;
        TECHDATA::weapons[kBombWeapon].type = WEAPON_TYPE_BOMB;

        MOX::qcombat_data = ::qcombat_data;
        MOX::combat_total_ship_count = 2;
        MOX::cmbt_colony_id = -1;
        MOX::combat_attacker_player = kAttacker;
        random_result = 1;
        random_bound = 0;
        random_calls = 0;
        CMBTMIS::missile_defense.tachyon_scanner = 20;
        CMBTMIS::missile_defense.neutron_scanner = 40;
        CMBTMIS::missile_defense.sensors = 70;
        allocation_count = 0;
        COMBINIT::max_ships_per_side = ORIGINAL_MAX_COMBAT_SHIPS_PER_SIDE;
        beam_damage_adjustment = 0;
        beam_damage_calls = 0;
        memset(beam_damage_contexts, 0, sizeof(beam_damage_contexts));
    }
} // namespace TEST_BOMBARDMENT_AMMO

namespace TEST_COMBAT_BEAM_DAMAGE {
    void Set_Adjustment_(int16_t adjustment) {
        beam_damage_adjustment = adjustment;
    }

    int Calls_() {
        return beam_damage_calls;
    }

    const orion2re::lua::game::s_combat_beam_damage_context& Context_(
        int index) {
        return beam_damage_contexts[index];
    }
} // namespace TEST_COMBAT_BEAM_DAMAGE

namespace orion2re::lua::game {
    std::int16_t Modify_Combat_Beam_Damage(
        const s_combat_beam_damage_context* context, std::int64_t damage) {
        beam_damage_contexts[beam_damage_calls++] = *context;
        return static_cast<std::int16_t>(damage + beam_damage_adjustment);
    }
} // namespace orion2re::lua::game

namespace MOX {
    s_animation_header* screen_seg;
    s_player* player = players;
    s_ship_data* ship = ships;
    s_colony* colony = colonies;
    s_qship* qcombat_data = ::qcombat_data;
    s_combat_data* combat_data;
    s_missile* missile;
    int16_t fighter_beam_type[9];
    int16_t fighter_bomb_type[9];
    int16_t cmbt_colony_id;
    uint16_t apply_damage_to_planet;
    int16_t combat_total_ship_count;
    int16_t combat_attacker_player;
    int16_t combat_defender_player;
    s_combat_bomb_result combat_bomb_result;
} // namespace MOX

namespace TECHDATA {
    s_tech_hull_data hull_data[HULL_CLASS_COUNT];
    s_tech_shield_data shields[SHIELD_COUNT];
    s_tech_weapon_data weapons[WEAPON_COUNT];
} // namespace TECHDATA

namespace game_random {
    uint32_t __cdecl Random(uint32_t bound) {
        random_bound = bound;
        ++random_calls;
        return random_result;
    }
} // namespace game_random

namespace allocate {
    void __cdecl Push_Block(void*) {
    }

    void* __cdecl Allocate_Next_Block(void*, uint32_t) {
        return allocation_count++ == 0
            ? static_cast<void*>(combat_data)
            : static_cast<void*>(missiles);
    }

    void __cdecl Pop_Block(void*) {
    }
} // namespace allocate

namespace struct_ {
    void __cdecl Clear_Structure(void* dest, uint32_t size) {
        memset(dest, 0, size);
    }
} // namespace struct_

namespace INITSHIP {
    int16_t __cdecl Best_Armor(int16_t) {
        return 0;
    }

    int16_t __cdecl Best_Point_Defense_Weapon(int16_t) {
        return kBestPointDefenseWeapon;
    }

    int16_t __cdecl Best_Bomb_Weapon(int16_t) {
        return kBestBombWeapon;
    }

    int16_t __cdecl Strat_Missile_Weapon_Count(int16_t hull_idx) {
        return TECHDATA::hull_data[hull_idx].strat_missile;
    }

    int16_t __cdecl Strat_Special_Weapon_Count(int16_t hull_idx) {
        return TECHDATA::hull_data[hull_idx].strat_special;
    }
} // namespace INITSHIP

namespace COMBINIT {
    void __cdecl Load_Combat_Ship(int16_t, int16_t combat_idx) {
        s_combat_data* ship = &MOX::combat_data[combat_idx];
        const int16_t weapon_ids[] = {
            kBeamWeapon,
            kMissileWeapon,
            kTorpedoWeapon,
            kBombWeapon,
        };

        for (int16_t i = 0; i < 4; ++i) {
            ship->weapons[i].weapon_id = weapon_ids[i];
            ship->weapons[i].count = 1;
            ship->weapons[i].ammo = 20;
        }
    }

    void __cdecl Load_Tactical_Colony(int16_t, int16_t) {
        MOX::combat_data[0].structure_max = 100;
    }

    void __cdecl Init_Special_Devices(int16_t) {
    }
} // namespace COMBINIT

namespace CMBTFIR2 {
    void __cdecl Set_Combat_Ordnance_Values() {
    }

    int16_t __cdecl Offensive_Combat_Bonus(int16_t) {
        return 0;
    }

    int16_t __cdecl Defensive_Combat_Bonus(int16_t, int16_t) {
        return 0;
    }
} // namespace CMBTFIR2

namespace CMBTFIRE {
    int16_t __cdecl Fire_Ship(int16_t, int16_t, int16_t, int16_t, int16_t, int16_t, int16_t) {
        return 0;
    }
} // namespace CMBTFIRE

DOCTEST_TEST_CASE("tactical bombardment orders supplied ships by descending hull size") {
    TEST_BOMBARDMENT_AMMO::Reset_();
    int16_t indices[] = {0, 1, 2};
    const int8_t sizes[] = {1, 3, 2};
    for (int i = 0; i < 3; ++i) {
        ships[i].owner = kAttacker;
        ships[i].d.ship_type = SHIP_TYPE_COMBAT;
        ships[i].d.size = sizes[i];
    }
    s_combat_bomb_result result = {};
    result.attacker_idx = kAttacker;
    COMBAT::Tactical_Bombardment(indices, 3, &result);
    DOCTEST_CHECK(indices[0] == 1);
    DOCTEST_CHECK(indices[1] == 2);
    DOCTEST_CHECK(indices[2] == 0);
    DOCTEST_CHECK(MOX::combat_total_ship_count == 4);
}

DOCTEST_TEST_CASE("stellar converters choose the strongest active enemy ship without a random draw") {
    TEST_BOMBARDMENT_AMMO::Reset_();
    MOX::combat_total_ship_count = 5;
    qcombat_data[1].player = kAttacker;
    qcombat_data[0].player = qcombat_data[2].player = qcombat_data[3].player = qcombat_data[4].player = 1;
    qcombat_data[0].max_hits = 10000;
    qcombat_data[2].max_hits = 100;
    qcombat_data[2].structural_damage = 10;
    qcombat_data[3].max_hits = 150;
    qcombat_data[3].structural_damage = 50;
    qcombat_data[4].max_hits = 1000;
    qcombat_data[4].status = 1;
    DOCTEST_CHECK(COMBAT::QGet_Target_SC(1) == 3);
    qcombat_data[3].status = 1;
    DOCTEST_CHECK(COMBAT::QGet_Target_SC(1) == 2);
    qcombat_data[2].player = kAttacker;
    DOCTEST_CHECK(COMBAT::QGet_Target_SC(1) == -1);
    DOCTEST_CHECK(random_calls == 0);
}

DOCTEST_TEST_CASE("tactical bombardment advances only torpedo cooldown") {
    TEST_BOMBARDMENT_AMMO::Reset_();
    int16_t ship_indices[] = {0};
    s_combat_bomb_result result = {};
    result.attacker_idx = kAttacker;

    COMBAT::Tactical_Bombardment(ship_indices, 1, &result);

    const s_combat_data* ship = &combat_data[1];
    DOCTEST_CHECK(ship->weapons[0].ammo == 20);
    DOCTEST_CHECK((ship->weapons[1].ammo == 20));
    DOCTEST_CHECK(ship->weapons[2].ammo == 10);
    DOCTEST_CHECK((ship->weapons[3].ammo == 20));
}

DOCTEST_TEST_CASE("tactical bombardment refreshes attacker fighter weapons") {
    TEST_BOMBARDMENT_AMMO::Reset_();
    int16_t ship_indices[] = {0};
    s_combat_bomb_result result = {};
    result.attacker_idx = kAttacker;

    COMBAT::Tactical_Bombardment(ship_indices, 1, &result);

    DOCTEST_CHECK(MOX::fighter_beam_type[kAttacker] == kBestPointDefenseWeapon);
    DOCTEST_CHECK(MOX::fighter_bomb_type[kAttacker] == kBestBombWeapon);
}

DOCTEST_TEST_CASE("tactical bombardment enforces ship caps without counting support ships") {
    const int16_t limits[] = {1, ORIGINAL_MAX_COMBAT_SHIPS_PER_SIDE, MAX_COMBAT_SHIPS_PER_SIDE};
    for (int16_t limit : limits) {
        TEST_BOMBARDMENT_AMMO::Reset_();
        COMBINIT::max_ships_per_side = limit;
        int16_t ship_indices[MAX_SHIPS];
        for (int16_t i = 0; i < MAX_SHIPS; ++i) {
            ship_indices[i] = i;
            ships[i].owner = kAttacker;
            ships[i].d.ship_type = i % 2 == 0 ? SHIP_TYPE_COLONY : SHIP_TYPE_COMBAT;
        }
        s_combat_bomb_result result = {};
        result.attacker_idx = kAttacker;

        COMBAT::Tactical_Bombardment(ship_indices, MAX_SHIPS, &result);

        DOCTEST_CHECK(MOX::combat_total_ship_count == limit + 1);
        DOCTEST_CHECK(combat_data[limit].weapons[0].count == 1);
        DOCTEST_CHECK(combat_data[limit + 1].weapons[0].count == 0);
    }
    TEST_BOMBARDMENT_AMMO::Reset_();
}

DOCTEST_TEST_CASE("strategic target selection supports expanded fleets with one RNG draw") {
    const int16_t enemy_counts[] = {1, 502, 503, 1141, MAX_SHIPS - 1};
    for (int16_t enemy_count : enemy_counts) {
        TEST_BOMBARDMENT_AMMO::Reset_();
        qcombat_data[1].player = kAttacker;
        MOX::combat_total_ship_count = enemy_count + 2;
        for (int16_t i = 2; i < MOX::combat_total_ship_count; ++i) {
            qcombat_data[i].player = 1;
        }

        const uint32_t selections[] = {1, static_cast<uint32_t>(enemy_count / 2 + 1), static_cast<uint32_t>(enemy_count)};
        for (uint32_t selection : selections) {
            random_result = selection;
            random_calls = 0;

            DOCTEST_CHECK(COMBAT::QGet_Target(1) == selection + 1);
            DOCTEST_CHECK(random_bound == enemy_count);
            DOCTEST_CHECK(random_calls == 1);
        }
    }
}

DOCTEST_TEST_CASE("strategic target selection preserves eligible target order") {
    TEST_BOMBARDMENT_AMMO::Reset_();
    MOX::combat_total_ship_count = 8;
    MOX::cmbt_colony_id = 0;
    qcombat_data[1].player = kAttacker;
    qcombat_data[2].player = kAttacker;
    qcombat_data[3].status = 1;
    qcombat_data[5].status = 2;
    qcombat_data[6].player = kAttacker;
    const int16_t expected_targets[] = {4, 7};

    for (uint32_t selection = 1; selection <= 2; ++selection) {
        random_result = selection;
        random_calls = 0;

        DOCTEST_CHECK(COMBAT::QGet_Target(1) == expected_targets[selection - 1]);
        DOCTEST_CHECK(random_bound == 2);
        DOCTEST_CHECK(random_calls == 1);
    }
}

DOCTEST_TEST_CASE("strategic target selection falls back to colony only for attacker") {
    TEST_BOMBARDMENT_AMMO::Reset_();
    qcombat_data[1].player = kAttacker;

    DOCTEST_CHECK(COMBAT::QGet_Target(1) == -1);
    MOX::cmbt_colony_id = 0;
    DOCTEST_CHECK(COMBAT::QGet_Target(1) == 0);
    qcombat_data[0].status = 1;
    DOCTEST_CHECK(COMBAT::QGet_Target(1) == -1);
    qcombat_data[0].status = 2;
    DOCTEST_CHECK(COMBAT::QGet_Target(1) == -1);
    qcombat_data[0].status = 0;
    qcombat_data[1].player = 1;
    DOCTEST_CHECK(COMBAT::QGet_Target(1) == -1);
    DOCTEST_CHECK(random_calls == 0);
}

DOCTEST_TEST_CASE("strategic attacks use strategic weapon damage") {
    TEST_BOMBARDMENT_AMMO::Reset_();
    constexpr int16_t attacker_idx = 1;
    constexpr int16_t target_idx = 0;
    constexpr int16_t weapon_id = 7;
    constexpr int16_t hull_idx = 0;

    s_tech_weapon_data& weapon = TECHDATA::weapons[weapon_id];
    weapon.min_damage = 100;
    weapon.max_damage = 100;
    weapon.strat_min_damage = 7;
    weapon.strat_max_damage = 7;

    TECHDATA::hull_data[hull_idx].strat_beam = 1;
    TECHDATA::hull_data[hull_idx].strat_missile = 1;
    TECHDATA::hull_data[hull_idx].strat_special = 1;
    TECHDATA::hull_data[hull_idx].strat_bomb = 1;

    qcombat_data[attacker_idx].player = kAttacker;
    qcombat_data[attacker_idx].size = hull_idx;
    qcombat_data[attacker_idx].base_ocv = 100;
    qcombat_data[target_idx].player = 1;
    qcombat_data[attacker_idx].beam_weapon_id = weapon_id;
    qcombat_data[attacker_idx].missile_weapon_id = weapon_id;
    qcombat_data[attacker_idx].special_weapon_id = weapon_id;
    qcombat_data[attacker_idx].bomb_weapon_id = weapon_id;
    qcombat_data[target_idx].max_hits = 30000;

    COMBAT::Beam_Attack(attacker_idx, target_idx);
    DOCTEST_CHECK((qcombat_data[target_idx].structural_damage == 7));

    qcombat_data[target_idx].structural_damage = 0;
    COMBAT::Missile_Attack(attacker_idx, target_idx);
    DOCTEST_CHECK((qcombat_data[target_idx].structural_damage == 7));

    qcombat_data[target_idx].structural_damage = 0;
    COMBAT::Strat_Special_Attack(attacker_idx, target_idx);
    DOCTEST_CHECK((qcombat_data[target_idx].structural_damage == 7));

    qcombat_data[target_idx].structural_damage = 0;
    COMBAT::Strat_Bomb_Attack(attacker_idx);
    DOCTEST_CHECK((qcombat_data[target_idx].structural_damage == 7));

    qcombat_data[target_idx].structural_damage = 0;
    TECHDATA::weapons[WEAPON_STELLAR_CONVERTER].max_damage = 200;
    TECHDATA::weapons[WEAPON_STELLAR_CONVERTER].strat_max_damage = 9;
    COMBAT::Stellar_Converter_Attack(target_idx);
    DOCTEST_CHECK((qcombat_data[target_idx].structural_damage == 36));
}

DOCTEST_TEST_CASE("strategic beam damage passes each hit through the Lua rule boundary") {
    TEST_BOMBARDMENT_AMMO::Reset_();
    constexpr int16_t attacker_idx = 1;
    constexpr int16_t target_idx = 0;
    constexpr int16_t weapon_id = 7;
    constexpr int16_t hull_idx = 0;

    TECHDATA::weapons[weapon_id].strat_min_damage = 7;
    TECHDATA::weapons[weapon_id].strat_max_damage = 7;
    TECHDATA::hull_data[hull_idx].strat_beam = 1;
    qcombat_data[attacker_idx].player = kAttacker;
    qcombat_data[attacker_idx].size = hull_idx;
    qcombat_data[attacker_idx].base_ocv = 100;
    qcombat_data[attacker_idx].beam_weapon_id = weapon_id;
    qcombat_data[target_idx].player = 1;
    qcombat_data[target_idx].max_hits = 30000;
    TEST_COMBAT_BEAM_DAMAGE::Set_Adjustment_(5);

    COMBAT::Beam_Attack(attacker_idx, target_idx);

    DOCTEST_CHECK((qcombat_data[target_idx].structural_damage == 12));
    DOCTEST_CHECK(TEST_COMBAT_BEAM_DAMAGE::Calls_() == 1);
    const auto& context = TEST_COMBAT_BEAM_DAMAGE::Context_(0);
    DOCTEST_CHECK(context.attacker_player_id == kAttacker);
    DOCTEST_CHECK(context.target_player_id == 1);
    DOCTEST_CHECK(context.weapon_id == weapon_id);
    DOCTEST_CHECK(context.beam_ordinal == 1);
    DOCTEST_CHECK(context.beam_count == 1);
}

DOCTEST_TEST_CASE("strategic beam damage context follows salvo retargeting") {
    TEST_BOMBARDMENT_AMMO::Reset_();
    constexpr int16_t attacker_idx = 1;
    constexpr int16_t first_target_idx = 0;
    constexpr int16_t second_target_idx = 2;
    constexpr int16_t weapon_id = 7;
    constexpr int16_t hull_idx = 0;

    TECHDATA::weapons[weapon_id].strat_min_damage = 7;
    TECHDATA::weapons[weapon_id].strat_max_damage = 7;
    TECHDATA::hull_data[hull_idx].strat_beam = 2;
    qcombat_data[attacker_idx].player = kAttacker;
    qcombat_data[attacker_idx].size = hull_idx;
    qcombat_data[attacker_idx].base_ocv = 100;
    qcombat_data[attacker_idx].beam_weapon_id = weapon_id;
    qcombat_data[first_target_idx].player = 1;
    qcombat_data[first_target_idx].max_hits = 7;
    qcombat_data[second_target_idx].player = 3;
    qcombat_data[second_target_idx].max_hits = 30000;
    MOX::combat_total_ship_count = 3;

    COMBAT::Beam_Attack(attacker_idx, first_target_idx);

    DOCTEST_CHECK((qcombat_data[first_target_idx].structural_damage == 7));
    DOCTEST_CHECK(qcombat_data[first_target_idx].status == 1);
    DOCTEST_CHECK((qcombat_data[second_target_idx].structural_damage == 7));
    DOCTEST_CHECK(TEST_COMBAT_BEAM_DAMAGE::Calls_() == 2);
    const auto& first = TEST_COMBAT_BEAM_DAMAGE::Context_(0);
    const auto& second = TEST_COMBAT_BEAM_DAMAGE::Context_(1);
    DOCTEST_CHECK(first.target_player_id == 1);
    DOCTEST_CHECK(first.beam_ordinal == 1);
    DOCTEST_CHECK(first.beam_count == 2);
    DOCTEST_CHECK(second.target_player_id == 3);
    DOCTEST_CHECK(second.beam_ordinal == 2);
    DOCTEST_CHECK(second.beam_count == 2);
}

DOCTEST_TEST_CASE("strategic missile attacks use configured scanner reductions") {
    struct s_scanner_case {
        int16_t player;
        int16_t technology;
    };
    const s_scanner_case cases[] = {
        {0, TECH_APP_SENSORS},
        {0, TECH_APP_NEUTRON_SCANNER},
        {0, TECH_APP_TACHYON_SCANNER},
        {MAX_PLAYERS, TECH_APP_NO_TECH},
    };

    for (const s_scanner_case& test_case : cases) {
        TEST_BOMBARDMENT_AMMO::Reset_();
        constexpr int16_t attacker_idx = 1;
        constexpr int16_t target_idx = 0;
        constexpr int16_t weapon_idx = 7;
        constexpr int16_t hull_idx = 0;

        CMBTMIS::missile_defense.sensors = 3;
        CMBTMIS::missile_defense.neutron_scanner = 2;
        CMBTMIS::missile_defense.tachyon_scanner = 1;
        TECHDATA::weapons[weapon_idx].strat_max_damage = 7;
        TECHDATA::hull_data[hull_idx].strat_missile = 1;
        qcombat_data[attacker_idx].player = test_case.player;
        qcombat_data[attacker_idx].size = hull_idx;
        qcombat_data[attacker_idx].missile_weapon_id = weapon_idx;
        qcombat_data[target_idx].missile_defense_bonus = 5;
        qcombat_data[target_idx].max_hits = 30000;
        if (test_case.player < MAX_PLAYERS) {
            players[test_case.player].tech_applications[test_case.technology] =
                TECH_RESEARCH_STATUS_RESEARCHED;
        }

        COMBAT::Missile_Attack(attacker_idx, target_idx);

        DOCTEST_CHECK((qcombat_data[target_idx].structural_damage == 0));
    }
}

DOCTEST_TEST_CASE("empty tactical bombardment uses the context attacker") {
    TEST_BOMBARDMENT_AMMO::Reset_();
    ships[0].owner = 3;
    int16_t unused_ship_indices[] = {0};
    s_combat_bomb_result result = {};
    result.attacker_idx = kAttacker;

    COMBAT::Tactical_Bombardment(unused_ship_indices, 0, &result);

    DOCTEST_CHECK(MOX::combat_attacker_player == kAttacker);
    DOCTEST_CHECK(MOX::fighter_beam_type[kAttacker] == kBestPointDefenseWeapon);
    DOCTEST_CHECK(MOX::fighter_bomb_type[kAttacker] == kBestBombWeapon);
}