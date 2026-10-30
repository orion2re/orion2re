#include "pch.h"

#include <cstring>

#include "doctest/doctest.h"
#include "game/ship_config.h"
#include "game/initship.h"
#include "game/shipstak.h"

namespace {
    constexpr int16_t kPlayer = 0;
    constexpr int16_t kDrive = DRIVE_NUCLEAR_DRIVE;
    constexpr int16_t kStrategicDrive = DRIVE_ANTI_MATTER_DRIVE;
    constexpr int16_t kMissileWeapon = 15;
    constexpr int16_t kStrategicMissileWeapon = 16;
    constexpr int16_t kTorpedoWeapon = 18;
    constexpr int16_t kTacticalBeamWeapon = 3;
    constexpr int16_t kStrategicBeamWeapon = 4;
    constexpr int16_t kTacticalBombWeapon = 21;
    constexpr int16_t kStrategicBombWeapon = 22;

    s_player players[MAX_PLAYERS];
    s_ship_data ships[1];
    s_combat_data combat_data[1];
    char guardian_name[] = "Guardian";
    char amoeba_name[] = "Amoeba";
    char crystal_name[] = "Crystal";
    char dragon_name[] = "Dragon";
    char eel_name[] = "Eel";
    char hydra_name[] = "Hydra";
    char doom_star_name[] = "Doom Star";

    void Reset_Initship_() {
        memset(players, 0, sizeof(players));
        memset(ships, 0, sizeof(ships));
        memset(combat_data, 0, sizeof(combat_data));
        memset(MOX::crew_data, 0, sizeof(MOX::crew_data));
        memset(TECHDATA::computers, 0, sizeof(TECHDATA::computers));
        memset(TECHDATA::drives, 0, sizeof(TECHDATA::drives));
        memset(TECHDATA::hull_data, 0, sizeof(TECHDATA::hull_data));
        memset(TECHDATA::specials, 0, sizeof(TECHDATA::specials));
        memset(TECHDATA::weapons, 0, sizeof(TECHDATA::weapons));
        memset(TECHDATA::weapon_modifications, 0, sizeof(TECHDATA::weapon_modifications));
        MOX::race_names[14] = guardian_name;
        MOX::race_names[15] = amoeba_name;
        MOX::race_names[16] = crystal_name;
        MOX::race_names[17] = dragon_name;
        MOX::race_names[18] = eel_name;
        MOX::race_names[19] = hydra_name;

        TECHDATA::drives[kDrive].tech_app_id = TECH_APP_NUCLEAR_DRIVE;
        TECHDATA::drives[kDrive].warp_speed = 2;
        players[kPlayer].tech_applications[TECH_APP_NUCLEAR_DRIVE] =
            TECH_RESEARCH_STATUS_RESEARCHED;

        TECHDATA::weapons[kMissileWeapon].tech_app_id =
            TECH_APP_NUCLEAR_MISSILE;
        TECHDATA::weapons[kMissileWeapon].type = WEAPON_TYPE_MISSILE;
        TECHDATA::weapons[kMissileWeapon].max_damage = 10;

        TECHDATA::weapons[kTorpedoWeapon].tech_app_id =
            TECH_APP_ANTI_MATTER_TORPEDOES;
        TECHDATA::weapons[kTorpedoWeapon].type = WEAPON_TYPE_TORPEDO;
        TECHDATA::weapons[kTorpedoWeapon].max_damage = 20;

        players[kPlayer].tech_applications[TECH_APP_NUCLEAR_MISSILE] =
            TECH_RESEARCH_STATUS_RESEARCHED;
        players[kPlayer].tech_applications[TECH_APP_ANTI_MATTER_TORPEDOES] =
            TECH_RESEARCH_STATUS_RESEARCHED;

        INITSHIP::satellite_uses_torpedoes = 0;
        INITSHIP::td_ftl_bonus = 2;
        INITSHIP::transport_ship_extended_range = 0;
        INITSHIP::landers_size_class = SHIP_SIZE_SMALL;
        INITSHIP::lander_ship_uses_stealth = {};
        SHIP_CONFIG::monster.drives = {
            DRIVE_FUSION_DRIVE,
            DRIVE_ANTI_MATTER_DRIVE,
            DRIVE_ANTI_MATTER_DRIVE,
            DRIVE_INTERPHASED_DRIVE,
            DRIVE_INTERPHASED_DRIVE,
            DRIVE_FUSION_DRIVE,
        };
        DESIGN::megafluxers_strategic_bonus = 50;
        SHIPSTAK::stealth_bonus = {500, 500, 500, 500};
        MOX::settings = {};
    }
} // namespace

namespace MOX {
    s_player* player = players;
    s_ship_data* ship = ships;
    s_combat_data* combat_data = ::combat_data;
    char* race_names[20] = {};
    int16_t cur_combat_x;
    int32_t stardate;
    s_crew_data crew_data[CREW_QUALITY_COUNT];
    s_settings settings = {};
    const char ship_type_names[SHIP_TYPE_COUNT][6][16] = {};
} // namespace MOX

namespace global {
    const char s_Intruder_005576a0[] = "Intruder";
}

namespace TECHDATA {
    s_tech_armor_data armor[ARMOR_COUNT];
    s_tech_computer_data computers[COMPUTER_COUNT];
    s_tech_drive_data drives[DRIVE_COUNT];
    s_tech_hull_data hull_data[HULL_CLASS_COUNT];
    s_tech_shield_data shields[SHIELD_COUNT];
    s_tech_specials_data specials[SPECIAL_COUNT];
    s_tech_weapon_data weapons[WEAPON_COUNT];
    s_tech_weapon_mod_data weapon_modifications[WEAPON_MOD_COUNT];
} // namespace TECHDATA

namespace CMBTFIR2 {
    s_combat_bonus combat_bonus = {
        50,
        20,
        0,
        10,
        20,
        50,
        100,
        0,
        80,
        80,
        10,
        30,
        15,
    };

    void __cdecl Init_Internal_Space(s_combat_data*) {
    }

    int16_t __cdecl Helmsman_Bonus(int16_t) {
        return 0;
    }

    int16_t __cdecl Weaponry_Bonus(int16_t) {
        return 0;
    }
} // namespace CMBTFIR2

namespace COMBINIT {
    int16_t __cdecl Calc_Combat_Shield_Max(int16_t ship_idx) {
        return MOX::combat_data[ship_idx].shield_type > SHIELD_NO_SHIELD ? 37 : 0;
    }
} // namespace COMBINIT

namespace DESIGN {
    int16_t __cdecl Miniaturization_Level(int16_t, int16_t) {
        return 0;
    }

    void __cdecl Get_Ship_Design_Name(int16_t, int16_t, int16_t, char*, int32_t) {
    }

    void __cdecl Cost_Reduction_For_Govt_Type(int16_t, int32_t*) {
    }
} // namespace DESIGN

namespace aidesign {
    void __cdecl Auto_Design_Ship(int16_t, s_ship_design* design, int16_t hull_idx) {
        memset(design, 0, sizeof(*design));
        design->size = static_cast<int8_t>(hull_idx);
        design->cost = static_cast<int16_t>(100 + hull_idx);
    }
} // namespace aidesign

namespace COLCALC {
    int32_t __cdecl Ship_Type_Cost_For_Player(int32_t, int16_t) {
        return 100;
    }
} // namespace COLCALC

DOCTEST_TEST_CASE("tactical initialization fills all six persistent designs") {
    Reset_Initship_();
    players[kPlayer].objectives = PLAYER_OBJECTIVE_HUMAN;
    MOX::settings.starting_civilization_level = 2;

    INITSHIP::Init_Ship_Designs(kPlayer);

    for (int16_t design_idx = 0; design_idx < SHIP_SIZE_COUNT; ++design_idx) {
        DOCTEST_CHECK(players[kPlayer].ship_designs[design_idx].size == design_idx);
        DOCTEST_CHECK((players[kPlayer].ship_designs[design_idx].cost ==
            100 + design_idx));
    }
}

DOCTEST_TEST_CASE("monster drive selection uses independent settings") {
    Reset_Initship_();
    SHIP_CONFIG::monster.drives = {
        DRIVE_ION_DRIVE,
        DRIVE_HYPER_DRIVE,
        DRIVE_NUCLEAR_DRIVE,
        DRIVE_ANTI_MATTER_DRIVE,
        DRIVE_FUSION_DRIVE,
        DRIVE_INTERPHASED_DRIVE,
    };

    DOCTEST_CHECK(INITSHIP::Best_Warp_Drive(NONPLAYER_SHIP_TYPE_AMOEBA) ==
        DRIVE_ION_DRIVE);
    DOCTEST_CHECK(INITSHIP::Best_Warp_Drive(NONPLAYER_SHIP_TYPE_HYDRA) ==
        DRIVE_INTERPHASED_DRIVE);
    DOCTEST_CHECK(INITSHIP::Best_Warp_Drive(NONPLAYER_SHIP_TYPE_GUARDIAN) ==
        DRIVE_HYPER_DRIVE);
    DOCTEST_CHECK(INITSHIP::Best_Warp_Drive(NONPLAYER_SHIP_TYPE_CRYSTAL) ==
        DRIVE_NUCLEAR_DRIVE);
    DOCTEST_CHECK(INITSHIP::Best_Warp_Drive(NONPLAYER_SHIP_TYPE_DRAGON) ==
        DRIVE_ANTI_MATTER_DRIVE);
    DOCTEST_CHECK(INITSHIP::Best_Warp_Drive(NONPLAYER_SHIP_TYPE_EEL) ==
        DRIVE_FUSION_DRIVE);
}

DOCTEST_TEST_CASE("Crystal loader applies one shared equipment selection") {
    Reset_Initship_();
    const SHIP_CONFIG::s_crystal_shared_equipment original =
        SHIP_CONFIG::crystal.shared;
    SHIP_CONFIG::crystal.shared = {
        SHIELD_CLASS_III_SHIELD,
        COMPUTER_POSITRONIC_COMPUTER,
        SPECIAL_BATTLE_SCANNER,
    };

    s_ship_design system = {};
    s_ship_design event = {};
    INITSHIP::Load_Crystal_Ship_Design(&system, 0);
    INITSHIP::Load_Crystal_Ship_Design(&event, 1);

    const int8_t expected_specials[5] = {0x20, 0, 0, 0, 0};
    DOCTEST_CHECK(system.shield_type == SHIELD_CLASS_III_SHIELD);
    DOCTEST_CHECK(event.shield_type == SHIELD_CLASS_III_SHIELD);
    DOCTEST_CHECK(system.computer_type == COMPUTER_POSITRONIC_COMPUTER);
    DOCTEST_CHECK(event.computer_type == COMPUTER_POSITRONIC_COMPUTER);
    DOCTEST_CHECK(std::memcmp(system.special_device_flags,
                      expected_specials,
                      sizeof(expected_specials)) == 0);
    DOCTEST_CHECK(std::memcmp(event.special_device_flags,
                      expected_specials,
                      sizeof(expected_specials)) == 0);
    DOCTEST_CHECK(system.picture_num != event.picture_num);
    DOCTEST_CHECK(system.size != event.size);
    DOCTEST_CHECK(system.ftl_type != event.ftl_type);

    SHIP_CONFIG::crystal.shared.special = 0;
    INITSHIP::Load_Crystal_Ship_Design(&system, 0);
    const int8_t no_specials[5] = {};
    DOCTEST_CHECK(std::memcmp(system.special_device_flags,
                      no_specials,
                      sizeof(no_specials)) == 0);

    SHIP_CONFIG::crystal.shared = original;
}

DOCTEST_TEST_CASE("monster loaders apply independent configured drives") {
    Reset_Initship_();
    SHIP_CONFIG::monster.drives = {
        DRIVE_ION_DRIVE,
        DRIVE_HYPER_DRIVE,
        DRIVE_NUCLEAR_DRIVE,
        DRIVE_ANTI_MATTER_DRIVE,
        DRIVE_FUSION_DRIVE,
        DRIVE_INTERPHASED_DRIVE,
    };
    TECHDATA::drives[DRIVE_ION_DRIVE]
        .combat_speed_min[SHIP_SIZE_LARGE] = 11;
    TECHDATA::drives[DRIVE_ION_DRIVE]
        .combat_speed_min[SHIP_SIZE_HUGE] = 12;
    TECHDATA::drives[DRIVE_HYPER_DRIVE]
        .combat_speed_min[SHIP_SIZE_TITAN] = 13;
    TECHDATA::drives[DRIVE_NUCLEAR_DRIVE]
        .combat_speed_min[SHIP_SIZE_LARGE] = 14;
    TECHDATA::drives[DRIVE_NUCLEAR_DRIVE]
        .combat_speed_min[SHIP_SIZE_TITAN] = 15;
    TECHDATA::drives[DRIVE_ANTI_MATTER_DRIVE]
        .combat_speed_min[SHIP_SIZE_LARGE] = 16;
    TECHDATA::drives[DRIVE_ANTI_MATTER_DRIVE]
        .combat_speed_min[SHIP_SIZE_TITAN] = 17;
    TECHDATA::drives[DRIVE_FUSION_DRIVE]
        .combat_speed_min[SHIP_SIZE_LARGE] = 18;
    TECHDATA::drives[DRIVE_FUSION_DRIVE]
        .combat_speed_min[SHIP_SIZE_HUGE] = 19;
    TECHDATA::drives[DRIVE_INTERPHASED_DRIVE]
        .combat_speed_min[SHIP_SIZE_LARGE] = 20;
    TECHDATA::drives[DRIVE_INTERPHASED_DRIVE]
        .combat_speed_min[SHIP_SIZE_TITAN] = 21;
    TECHDATA::drives[DRIVE_INTERPHASED_DRIVE]
        .combat_speed_min[SHIP_SIZE_HUGE] = 22;
    TECHDATA::drives[DRIVE_FUSION_DRIVE]
        .combat_speed_min[SHIP_SIZE_TITAN] = 23;

    s_ship_design guardian = {};
    s_ship_design amoeba_system = {};
    s_ship_design amoeba_event = {};
    s_ship_design crystal_system = {};
    s_ship_design crystal_event = {};
    s_ship_design dragon_system = {};
    s_ship_design dragon_event = {};
    s_ship_design eel_system = {};
    s_ship_design eel_event = {};
    s_ship_design hydra_system = {};
    s_ship_design hydra_event = {};
    INITSHIP::Load_Guardian_Ship_Design(&guardian);
    INITSHIP::Load_Amoeba_Ship_Design(&amoeba_system, 0);
    INITSHIP::Load_Amoeba_Ship_Design(&amoeba_event, 1);
    INITSHIP::Load_Crystal_Ship_Design(&crystal_system, 0);
    INITSHIP::Load_Crystal_Ship_Design(&crystal_event, 1);
    INITSHIP::Load_Dragon_Ship_Design(&dragon_system, 0);
    INITSHIP::Load_Dragon_Ship_Design(&dragon_event, 1);
    INITSHIP::Load_Eel_Ship_Design(&eel_system, 0);
    INITSHIP::Load_Eel_Ship_Design(&eel_event, 1);
    INITSHIP::Load_Hydra_Ship_Design(&hydra_system, 0);
    INITSHIP::Load_Hydra_Ship_Design(&hydra_event, 1);

    DOCTEST_CHECK(guardian.ftl_type == DRIVE_NO_DRIVE);
    DOCTEST_CHECK(guardian.combat_speed == 13);
    DOCTEST_CHECK(amoeba_system.ftl_type == DRIVE_NO_DRIVE);
    DOCTEST_CHECK(amoeba_system.combat_speed == 11);
    DOCTEST_CHECK(amoeba_event.ftl_type == DRIVE_FUSION_DRIVE);
    DOCTEST_CHECK(amoeba_event.combat_speed == 19);
    DOCTEST_CHECK(crystal_system.ftl_type == DRIVE_NO_DRIVE);
    DOCTEST_CHECK(crystal_system.combat_speed == 14);
    DOCTEST_CHECK(crystal_event.ftl_type == DRIVE_ANTI_MATTER_DRIVE);
    DOCTEST_CHECK(crystal_event.combat_speed == 17);
    DOCTEST_CHECK(dragon_system.ftl_type == DRIVE_NO_DRIVE);
    DOCTEST_CHECK(dragon_system.combat_speed == 20);
    DOCTEST_CHECK(dragon_event.ftl_type == DRIVE_INTERPHASED_DRIVE);
    DOCTEST_CHECK(dragon_event.combat_speed == 25);
    DOCTEST_CHECK(eel_system.ftl_type == DRIVE_NO_DRIVE);
    DOCTEST_CHECK(eel_system.combat_speed == 26);
    DOCTEST_CHECK(eel_event.ftl_type == DRIVE_INTERPHASED_DRIVE);
    DOCTEST_CHECK(eel_event.combat_speed == 30);
    DOCTEST_CHECK(hydra_system.ftl_type == DRIVE_NO_DRIVE);
    DOCTEST_CHECK(hydra_system.combat_speed == 20);
    DOCTEST_CHECK(hydra_event.ftl_type == DRIVE_FUSION_DRIVE);
    DOCTEST_CHECK(hydra_event.combat_speed == 23);
}

DOCTEST_TEST_CASE("monster templates own FTL and no-drive designs use fallback speed") {
    Reset_Initship_();
    const int8_t guardian_drive = SHIP_CONFIG::guardian_template.ftl_type;
    const int8_t eel_system_drive = SHIP_CONFIG::eel_templates[0].ftl_type;
    const int8_t eel_event_drive = SHIP_CONFIG::eel_templates[1].ftl_type;

    SHIP_CONFIG::guardian_template.ftl_type = DRIVE_ION_DRIVE;
    SHIP_CONFIG::eel_templates[0].ftl_type = DRIVE_NUCLEAR_DRIVE;
    SHIP_CONFIG::eel_templates[1].ftl_type = DRIVE_NO_DRIVE;
    SHIP_CONFIG::monster.drives.guardian = DRIVE_HYPER_DRIVE;
    SHIP_CONFIG::monster.drives.eel = DRIVE_FUSION_DRIVE;
    TECHDATA::drives[DRIVE_ION_DRIVE]
        .combat_speed_min[SHIP_SIZE_TITAN] = 30;
    TECHDATA::drives[DRIVE_NUCLEAR_DRIVE]
        .combat_speed_min[SHIP_SIZE_LARGE] = 31;
    TECHDATA::drives[DRIVE_FUSION_DRIVE]
        .combat_speed_min[SHIP_SIZE_HUGE] = 32;

    s_ship_design guardian = {};
    s_ship_design eel_system = {};
    s_ship_design eel_event = {};
    INITSHIP::Load_Guardian_Ship_Design(&guardian);
    INITSHIP::Load_Eel_Ship_Design(&eel_system, 0);
    INITSHIP::Load_Eel_Ship_Design(&eel_event, 1);

    DOCTEST_CHECK(guardian.ftl_type == DRIVE_ION_DRIVE);
    DOCTEST_CHECK(guardian.speed == 1);
    DOCTEST_CHECK(guardian.combat_speed == 30);
    DOCTEST_CHECK(eel_system.ftl_type == DRIVE_NUCLEAR_DRIVE);
    DOCTEST_CHECK(eel_system.speed == 1);
    DOCTEST_CHECK(eel_system.combat_speed == 39);
    DOCTEST_CHECK(eel_event.ftl_type == DRIVE_NO_DRIVE);
    DOCTEST_CHECK(eel_event.speed == 0);
    DOCTEST_CHECK(eel_event.combat_speed == 40);

    SHIP_CONFIG::guardian_template.ftl_type = guardian_drive;
    SHIP_CONFIG::eel_templates[0].ftl_type = eel_system_drive;
    SHIP_CONFIG::eel_templates[1].ftl_type = eel_event_drive;
}

DOCTEST_TEST_CASE("Avenger loader preserves corrected design 360 arcs") {
    Reset_Initship_();
    s_ship_design avenger = {};

    INITSHIP::Load_Loknar_Ship_Design(&avenger);

    DOCTEST_CHECK(avenger.ship_weapon[3].firing_arc ==
        WEAPON_FIRING_ARC_360);
    DOCTEST_CHECK(avenger.ship_weapon[5].firing_arc ==
        WEAPON_FIRING_ARC_360);
    DOCTEST_CHECK(avenger.ftl_type == DRIVE_INTERPHASED_DRIVE);
}

DOCTEST_TEST_CASE("monster structural hits use configured encounter values") {
    Reset_Initship_();
    const SHIP_CONFIG::s_monster_hit_points original =
        SHIP_CONFIG::monster.hit_points;
    SHIP_CONFIG::monster.hit_points = {
        301,
        401,
        501,
        751,
        1001,
        1501,
        2501,
        -1,
        -1,
        -1,
        -1,
        -1,
    };

    ships[0].d.previous_owner = NONPLAYER_SHIP_TYPE_AMOEBA;
    ships[0].d.ftl_type = DRIVE_NO_DRIVE;
    DOCTEST_CHECK(INITSHIP::Get_Ship_Structure(0) == 401);
    ships[0].d.ftl_type = DRIVE_FUSION_DRIVE;
    DOCTEST_CHECK(INITSHIP::Get_Ship_Structure(0) == 751);

    ships[0].d.previous_owner = NONPLAYER_SHIP_TYPE_EEL;
    ships[0].d.ftl_type = DRIVE_NO_DRIVE;
    DOCTEST_CHECK(INITSHIP::Get_Ship_Structure(0) == 301);
    ships[0].d.ftl_type = DRIVE_INTERPHASED_DRIVE;
    DOCTEST_CHECK(INITSHIP::Get_Ship_Structure(0) == 1001);

    ships[0].d.previous_owner = NONPLAYER_SHIP_TYPE_HYDRA;
    ships[0].d.ftl_type = DRIVE_NO_DRIVE;
    DOCTEST_CHECK(INITSHIP::Get_Ship_Structure(0) == 501);
    ships[0].d.ftl_type = DRIVE_FUSION_DRIVE;
    DOCTEST_CHECK(INITSHIP::Get_Ship_Structure(0) == 1501);

    const uint8_t builders[] = {
        NONPLAYER_SHIP_TYPE_CRYSTAL,
        NONPLAYER_SHIP_TYPE_HYDRA,
        NONPLAYER_SHIP_TYPE_DRAGON,
    };
    for (uint8_t builder : builders) {
        ships[0].d.previous_owner = builder;
        ships[0].d.ftl_type = DRIVE_NO_DRIVE;
        DOCTEST_CHECK(INITSHIP::Get_Ship_Structure(0) == 501);
        ships[0].d.ftl_type = DRIVE_FUSION_DRIVE;
        DOCTEST_CHECK(INITSHIP::Get_Ship_Structure(0) ==
            (builder == NONPLAYER_SHIP_TYPE_HYDRA ? 1501 : 2501));
    }

    SHIP_CONFIG::monster.hit_points.system_crystal = 502;
    SHIP_CONFIG::monster.hit_points.system_hydra = 503;
    SHIP_CONFIG::monster.hit_points.system_dragon = 504;
    SHIP_CONFIG::monster.hit_points.event_crystal = 2502;
    SHIP_CONFIG::monster.hit_points.event_dragon = 2503;
    const int16_t system_hits[] = {502, 503, 504};
    const int16_t event_hits[] = {2502, 1501, 2503};
    for (int i = 0; i < 3; ++i) {
        ships[0].d.previous_owner = builders[i];
        ships[0].d.ftl_type = DRIVE_NO_DRIVE;
        DOCTEST_CHECK(INITSHIP::Get_Ship_Structure(0) == system_hits[i]);
        ships[0].d.ftl_type = DRIVE_FUSION_DRIVE;
        DOCTEST_CHECK(INITSHIP::Get_Ship_Structure(0) == event_hits[i]);
    }

    ships[0].d.previous_owner = NONPLAYER_SHIP_TYPE_DRAGON;
    SHIP_CONFIG::monster.hit_points.event_dragon = 0;
    DOCTEST_CHECK(INITSHIP::Get_Ship_Structure(0) == 0);
    SHIP_CONFIG::monster.hit_points.event_dragon = 32767;
    DOCTEST_CHECK(INITSHIP::Get_Ship_Structure(0) == 32767);
    SHIP_CONFIG::monster.hit_points.event_dragon = -1;
    DOCTEST_CHECK(INITSHIP::Get_Ship_Structure(0) == 2501);
    ships[0].d.ftl_type = DRIVE_NO_DRIVE;
    SHIP_CONFIG::monster.hit_points.system_dragon = 0;
    DOCTEST_CHECK(INITSHIP::Get_Ship_Structure(0) == 0);
    SHIP_CONFIG::monster.hit_points.system_dragon = 32767;
    DOCTEST_CHECK(INITSHIP::Get_Ship_Structure(0) == 32767);
    SHIP_CONFIG::monster.hit_points.system_dragon = -1;
    DOCTEST_CHECK(INITSHIP::Get_Ship_Structure(0) == 501);

    SHIP_CONFIG::monster.hit_points = original;
}

DOCTEST_TEST_CASE("defensive Antaran loaders preserve configured weapons and specials") {
    Reset_Initship_();
    MOX::settings.game_difficulty = GAME_DIFFICULTY_AVERAGE;

    INITSHIP::Load_Small_Antaran_Combat_Ship(0);
    DOCTEST_CHECK((combat_data[0].weapons[1].weapon_id == 24));
    DOCTEST_CHECK(combat_data[0].weapons[1].count == 1);
    DOCTEST_CHECK(combat_data[0].weapons[0].ammo == -1);

    INITSHIP::Load_Large_Antaran_Combat_Ship(0);
    DOCTEST_CHECK(combat_data[0].weapons[4].weapon_id == 31);
    DOCTEST_CHECK(combat_data[0].weapons[4].count == 3);

    INITSHIP::Load_Titan_Antaran_Combat_Ship(0);
    DOCTEST_CHECK((combat_data[0].weapons[7].weapon_id == 31));
    DOCTEST_CHECK(combat_data[0].weapons[7].count == 6);

    s_ship_design& titan = SHIP_CONFIG::antaran_defensive_templates
        [GAME_DIFFICULTY_AVERAGE][SHIP_SIZE_TITAN];
    const int8_t original_special = titan.special_device_flags[1];
    const int8_t original_shield = titan.shield_type;
    titan.special_device_flags[1] = 0x04;
    titan.shield_type = SHIELD_CLASS_I_SHIELD;

    INITSHIP::Load_Titan_Antaran_Combat_Ship(0);
    DOCTEST_CHECK(combat_data[0].special_device_flags[1] == 0x04);
    DOCTEST_CHECK((combat_data[0].shield_arc_max == 37));
    for (int i = 0; i < 4; ++i) {
        DOCTEST_CHECK((combat_data[0].shield_arc_current[i] == 37));
    }

    titan.special_device_flags[1] = original_special;
    titan.shield_type = original_shield;
}

DOCTEST_TEST_CASE("Antaran loaders preserve configured picture indexes") {
    Reset_Initship_();
    s_ship_design& offensive_interdictor =
        SHIP_CONFIG::antaran_offensive_templates[GAME_DIFFICULTY_AVERAGE]
                                                [SHIP_SIZE_HUGE];
    s_ship_design& offensive_harbinger =
        SHIP_CONFIG::antaran_offensive_templates[GAME_DIFFICULTY_IMPOSSIBLE]
                                                [SHIP_SIZE_TITAN];
    s_ship_design& defensive_harbinger =
        SHIP_CONFIG::antaran_defensive_templates[GAME_DIFFICULTY_AVERAGE]
                                                [SHIP_SIZE_TITAN];
    const int8_t original_interdictor_picture = offensive_interdictor.picture_num;
    const int8_t original_offensive_harbinger_picture =
        offensive_harbinger.picture_num;
    const int8_t original_defensive_harbinger_picture =
        defensive_harbinger.picture_num;
    offensive_interdictor.picture_num = 0;
    offensive_harbinger.picture_num = 44;
    defensive_harbinger.picture_num = 44;

    MOX::settings.game_difficulty = GAME_DIFFICULTY_AVERAGE;
    s_ship_design design = {};
    INITSHIP::Load_Huge_Antaran_Design(&design);
    DOCTEST_CHECK(design.picture_num == 0);
    INITSHIP::Load_Titan_Antaran_Combat_Ship(0);
    DOCTEST_CHECK(combat_data[0].picture_num == 44);

    MOX::settings.game_difficulty = GAME_DIFFICULTY_IMPOSSIBLE;
    INITSHIP::Load_Titan_Antaran_Design(&design);
    DOCTEST_CHECK(design.picture_num == 44);

    offensive_interdictor.picture_num = original_interdictor_picture;
    offensive_harbinger.picture_num = original_offensive_harbinger_picture;
    defensive_harbinger.picture_num = original_defensive_harbinger_picture;
}

DOCTEST_TEST_CASE("default Interdictor loader preserves six weapons and difficulty special") {
    Reset_Initship_();
    for (int difficulty = GAME_DIFFICULTY_TUTOR;
         difficulty <= GAME_DIFFICULTY_IMPOSSIBLE;
         ++difficulty) {
        MOX::settings.game_difficulty = difficulty;
        s_ship_design design = {};
        INITSHIP::Load_Huge_Antaran_Design(&design);

        DOCTEST_CHECK(design.ship_weapon[0].type == WEAPON_PARTICLE_BEAM);
        DOCTEST_CHECK(design.ship_weapon[0].count == 10);
        DOCTEST_CHECK(design.ship_weapon[5].type == WEAPON_DEATH_RAY);
        DOCTEST_CHECK(design.ship_weapon[5].count == 4);
        DOCTEST_CHECK(design.ship_weapon[6].type == 0);
        DOCTEST_CHECK(design.ship_weapon[6].count == 0);
        DOCTEST_CHECK(design.ship_weapon[7].type == 0);
        DOCTEST_CHECK(design.ship_weapon[7].count == 0);
        DOCTEST_CHECK(struct_::Test_Bit_Field(
            design.special_device_flags, SPECIAL_DAMPER_FIELD));
        if (difficulty == GAME_DIFFICULTY_TUTOR) {
            DOCTEST_CHECK_FALSE(struct_::Test_Bit_Field(
                design.special_device_flags, SPECIAL_ECM_JAMMER));
        } else if (difficulty == GAME_DIFFICULTY_EASY) {
            DOCTEST_CHECK(struct_::Test_Bit_Field(
                design.special_device_flags, SPECIAL_ECM_JAMMER));
        } else if (difficulty == GAME_DIFFICULTY_AVERAGE) {
            DOCTEST_CHECK(struct_::Test_Bit_Field(
                design.special_device_flags, SPECIAL_MULTI_WAVE_ECM_JAMMER));
        } else {
            DOCTEST_CHECK(struct_::Test_Bit_Field(
                design.special_device_flags, SPECIAL_WIDE_AREA_JAMMER));
        }
    }
}

DOCTEST_TEST_CASE("default defensive Harbinger loader preserves all tactical slots") {
    Reset_Initship_();
    for (int difficulty = GAME_DIFFICULTY_TUTOR;
         difficulty <= GAME_DIFFICULTY_IMPOSSIBLE;
         ++difficulty) {
        MOX::settings.game_difficulty = difficulty;
        INITSHIP::Load_Titan_Antaran_Combat_Ship(0);

        DOCTEST_CHECK(combat_data[0].weapons[0].weapon_id == WEAPON_PARTICLE_BEAM);
        DOCTEST_CHECK(combat_data[0].weapons[0].ammo == -1);
        DOCTEST_CHECK(combat_data[0].weapons[2].weapon_id == WEAPON_NEUTRONIUM_BOMB);
        DOCTEST_CHECK(combat_data[0].weapons[2].ammo == 10);
        DOCTEST_CHECK(combat_data[0].weapons[6].weapon_id ==
            WEAPON_BLACK_HOLE_GENERATOR);
        DOCTEST_CHECK(combat_data[0].weapons[6].count == 1);
        DOCTEST_CHECK((combat_data[0].weapons[7].weapon_id == WEAPON_INTERCEPTOR));
        DOCTEST_CHECK(combat_data[0].weapons[7].count == 6);
        DOCTEST_CHECK((combat_data[0].weapons[7].ammo == 1));
        if (difficulty == GAME_DIFFICULTY_TUTOR) {
            DOCTEST_CHECK(struct_::Test_Bit_Field(
                combat_data[0].special_device_flags, SPECIAL_ECM_JAMMER));
        } else if (difficulty == GAME_DIFFICULTY_EASY) {
            DOCTEST_CHECK(struct_::Test_Bit_Field(
                combat_data[0].special_device_flags,
                SPECIAL_MULTI_WAVE_ECM_JAMMER));
        } else {
            DOCTEST_CHECK(struct_::Test_Bit_Field(
                combat_data[0].special_device_flags, SPECIAL_WIDE_AREA_JAMMER));
        }
    }
}

DOCTEST_TEST_CASE("offensive Antaran Harbinger preserves authoritative final slots") {
    Reset_Initship_();
    MOX::settings.game_difficulty = GAME_DIFFICULTY_IMPOSSIBLE;

    s_ship_design design = {};
    INITSHIP::Load_Titan_Antaran_Design(&design);

    DOCTEST_CHECK(design.ship_weapon[6].type == WEAPON_BLACK_HOLE_GENERATOR);
    DOCTEST_CHECK(design.ship_weapon[6].count == 2);
    DOCTEST_CHECK(design.ship_weapon[6].current_count == 2);
    DOCTEST_CHECK(design.ship_weapon[6].firing_arc == WEAPON_FIRING_ARC_360);
    DOCTEST_CHECK(design.ship_weapon[7].type == WEAPON_INTERCEPTOR);
    DOCTEST_CHECK(design.ship_weapon[7].count == 12);
    DOCTEST_CHECK(design.ship_weapon[7].current_count == 12);
    DOCTEST_CHECK(design.ship_weapon[7].firing_arc == WEAPON_FIRING_ARC_360);
    DOCTEST_CHECK(struct_::Test_Bit_Field(
        design.special_device_flags, SPECIAL_WIDE_AREA_JAMMER));
}

DOCTEST_TEST_CASE("Trans-Dimensional FTL speed uses the configured bonus") {
    Reset_Initship_();
    players[kPlayer].traits[TRAIT_TRANS_DIMENSIONAL] = 1;

    DOCTEST_CHECK(INITSHIP::Calc_Player_FTL_Speed(kPlayer) == 4);
    DOCTEST_CHECK(INITSHIP::Get_FTL_Speed(kPlayer, kDrive) == 4);

    INITSHIP::td_ftl_bonus = 5;

    DOCTEST_CHECK(INITSHIP::Calc_Player_FTL_Speed(kPlayer) == 7);
    DOCTEST_CHECK(INITSHIP::Get_FTL_Speed(kPlayer, kDrive) == 7);

    players[kPlayer].traits[TRAIT_TRANS_DIMENSIONAL] = 0;
    DOCTEST_CHECK(INITSHIP::Get_FTL_Speed(kPlayer, kDrive) == 2);
}

DOCTEST_TEST_CASE("planetary defenses select torpedoes only when enabled") {
    Reset_Initship_();

    DOCTEST_CHECK(INITSHIP::Best_Missile_Weapon_NT(kPlayer) ==
        kMissileWeapon);

    INITSHIP::satellite_uses_torpedoes = 1;

    DOCTEST_CHECK(INITSHIP::Best_Missile_Weapon_NT(kPlayer) ==
        kTorpedoWeapon);
}

DOCTEST_TEST_CASE("strategic designs rank weapons by strategic damage") {
    Reset_Initship_();

    TECHDATA::weapons[kTacticalBeamWeapon].tech_app_id =
        TECH_APP_LASER_CANNON;
    TECHDATA::weapons[kTacticalBeamWeapon].type = WEAPON_TYPE_BEAM;
    TECHDATA::weapons[kTacticalBeamWeapon].max_damage = 30;
    TECHDATA::weapons[kTacticalBeamWeapon].strat_max_damage = 4;
    TECHDATA::weapons[kStrategicBeamWeapon].tech_app_id =
        TECH_APP_PARTICLE_BEAM;
    TECHDATA::weapons[kStrategicBeamWeapon].type = WEAPON_TYPE_BEAM;
    TECHDATA::weapons[kStrategicBeamWeapon].max_damage = 10;
    TECHDATA::weapons[kStrategicBeamWeapon].strat_max_damage = 25;

    TECHDATA::weapons[kMissileWeapon].max_damage = 30;
    TECHDATA::weapons[kMissileWeapon].strat_max_damage = 5;
    TECHDATA::weapons[kStrategicMissileWeapon].tech_app_id =
        TECH_APP_PULSON_MISSILE;
    TECHDATA::weapons[kStrategicMissileWeapon].type = WEAPON_TYPE_MISSILE;
    TECHDATA::weapons[kStrategicMissileWeapon].max_damage = 10;
    TECHDATA::weapons[kStrategicMissileWeapon].strat_max_damage = 25;
    TECHDATA::weapons[kTorpedoWeapon].max_damage = 10;
    TECHDATA::weapons[kTorpedoWeapon].strat_max_damage = 30;

    TECHDATA::weapons[kTacticalBombWeapon].tech_app_id =
        TECH_APP_NUCLEAR_BOMB;
    TECHDATA::weapons[kTacticalBombWeapon].type = WEAPON_TYPE_BOMB;
    TECHDATA::weapons[kTacticalBombWeapon].max_damage = 30;
    TECHDATA::weapons[kTacticalBombWeapon].strat_max_damage = 3;
    TECHDATA::weapons[kStrategicBombWeapon].tech_app_id =
        TECH_APP_FUSION_BOMB;
    TECHDATA::weapons[kStrategicBombWeapon].type = WEAPON_TYPE_BOMB;
    TECHDATA::weapons[kStrategicBombWeapon].max_damage = 10;
    TECHDATA::weapons[kStrategicBombWeapon].strat_max_damage = 24;

    players[kPlayer].tech_applications[TECH_APP_LASER_CANNON] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    players[kPlayer].tech_applications[TECH_APP_PARTICLE_BEAM] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    players[kPlayer].tech_applications[TECH_APP_PULSON_MISSILE] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    players[kPlayer].tech_applications[TECH_APP_NUCLEAR_BOMB] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    players[kPlayer].tech_applications[TECH_APP_FUSION_BOMB] =
        TECH_RESEARCH_STATUS_RESEARCHED;

    DOCTEST_CHECK(INITSHIP::Best_Beam_Weapon(kPlayer) ==
        kTacticalBeamWeapon);
    DOCTEST_CHECK(INITSHIP::Best_Missile_Weapon(kPlayer) ==
        kMissileWeapon);
    DOCTEST_CHECK(INITSHIP::Best_Bomb_Weapon(kPlayer) ==
        kTacticalBombWeapon);

    MOX::settings.strategic_combat_flag = 1;

    DOCTEST_CHECK(INITSHIP::Best_Beam_Weapon(kPlayer) ==
        kStrategicBeamWeapon);
    DOCTEST_CHECK(INITSHIP::Best_Missile_Weapon(kPlayer) ==
        kTorpedoWeapon);
    DOCTEST_CHECK(INITSHIP::Best_Bomb_Weapon(kPlayer) ==
        kStrategicBombWeapon);
    DOCTEST_CHECK(INITSHIP::Best_Missile_Weapon_NT(kPlayer) ==
        kStrategicMissileWeapon);

    INITSHIP::satellite_uses_torpedoes = 1;
    DOCTEST_CHECK(INITSHIP::Best_Missile_Weapon_NT(kPlayer) ==
        kTorpedoWeapon);
}

DOCTEST_TEST_CASE("strategic special selectors rank the complete unsigned word domain") {
    struct s_selector_case {
        int16_t(__cdecl* selector)(int16_t);
        int8_t category;
    };
    const s_selector_case cases[] = {
        {INITSHIP::Best_Beam_To_Hit_Special, SPECIAL_CATEGORY_BEAM_ACCURACY},
        {INITSHIP::Best_Beam_Damage_Special, SPECIAL_CATEGORY_BEAM_DAMAGE},
        {INITSHIP::Best_Beam_Defense_Special, SPECIAL_CATEGORY_BEAM_DEFENSE},
        {INITSHIP::Best_Missile_Defense_Special, SPECIAL_CATEGORY_MISSILE_DEFENSE},
        {INITSHIP::Best_Missile_Damage_Special, SPECIAL_CATEGORY_MISSILE_DAMAGE},
    };

    for (const s_selector_case& test_case : cases) {
        Reset_Initship_();
        s_tech_specials_data& first =
            TECHDATA::specials[SPECIAL_ACHILLES_TARGETING_UNIT];
        s_tech_specials_data& second =
            TECHDATA::specials[SPECIAL_BATTLE_SCANNER];
        first.tech_app_id = TECH_APP_ACHILLES_TARGETING_UNIT;
        second.tech_app_id = TECH_APP_BATTLE_SCANNER;
        first.category = test_case.category;
        second.category = test_case.category;
        players[kPlayer].tech_applications[first.tech_app_id] =
            TECH_RESEARCH_STATUS_RESEARCHED;
        players[kPlayer].tech_applications[second.tech_app_id] =
            TECH_RESEARCH_STATUS_RESEARCHED;

        first.strategic = 0x7FFF;
        second.strategic = 0x8000;
        DOCTEST_CHECK(test_case.selector(kPlayer) == SPECIAL_BATTLE_SCANNER);

        first.strategic = 0xFFFF;
        DOCTEST_CHECK(test_case.selector(kPlayer) ==
            SPECIAL_ACHILLES_TARGETING_UNIT);

        second.strategic = 0xFFFF;
        DOCTEST_CHECK(test_case.selector(kPlayer) ==
            SPECIAL_ACHILLES_TARGETING_UNIT);
    }
}

DOCTEST_TEST_CASE("Doom Star loader follows the requested combat mode") {
    Reset_Initship_();
    TECHDATA::hull_data[HULL_DOOM_STAR].name = doom_star_name;
    TECHDATA::hull_data[HULL_DOOM_STAR].cost = 200;
    TECHDATA::hull_data[HULL_DOOM_STAR].strat_beam = 7;
    TECHDATA::weapons[kStrategicBeamWeapon].tech_app_id =
        TECH_APP_PARTICLE_BEAM;
    TECHDATA::weapons[kStrategicBeamWeapon].type = WEAPON_TYPE_BEAM;
    TECHDATA::weapons[kStrategicBeamWeapon].strat_max_damage = 25;
    players[kPlayer].tech_applications[TECH_APP_PARTICLE_BEAM] =
        TECH_RESEARCH_STATUS_RESEARCHED;

    s_ship_design tactical = {};
    INITSHIP::Load_Doom_Star_Ship_Design(kPlayer, &tactical, false);
    DOCTEST_CHECK(tactical.size == HULL_DOOM_STAR);
    DOCTEST_CHECK(tactical.cost == 100 + HULL_DOOM_STAR);
    DOCTEST_CHECK(tactical.picture_num == 0);

    s_ship_design strategic = {};
    strategic.size = SHIP_SIZE_SMALL;
    INITSHIP::Load_Doom_Star_Ship_Design(kPlayer, &strategic, true);
    DOCTEST_CHECK(strategic.ship_type == SHIP_TYPE_COMBAT);
    DOCTEST_CHECK(strategic.size == HULL_DOOM_STAR);
    DOCTEST_CHECK(strategic.cost == 300);
    DOCTEST_CHECK(strategic.picture_num == 0x2B);
    DOCTEST_CHECK(strategic.ship_weapon[0].type == kStrategicBeamWeapon);
    DOCTEST_CHECK(strategic.ship_weapon[0].count == 7);
}

DOCTEST_TEST_CASE("heavy defense beams require Heavy Mount capability") {
    Reset_Initship_();

    TECHDATA::weapons[kTacticalBeamWeapon].tech_app_id =
        TECH_APP_LASER_CANNON;
    TECHDATA::weapons[kTacticalBeamWeapon].type = WEAPON_TYPE_BEAM;
    TECHDATA::weapons[kTacticalBeamWeapon].max_damage = 30;

    TECHDATA::weapons[kStrategicBeamWeapon].tech_app_id =
        TECH_APP_PARTICLE_BEAM;
    TECHDATA::weapons[kStrategicBeamWeapon].type = WEAPON_TYPE_BEAM;
    TECHDATA::weapons[kStrategicBeamWeapon].max_damage = 20;
    TECHDATA::weapons[kStrategicBeamWeapon].available_mods =
        WEAPON_MOD_FLAG_HEAVY_MOUNT;

    players[kPlayer].tech_applications[TECH_APP_LASER_CANNON] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    players[kPlayer].tech_applications[TECH_APP_PARTICLE_BEAM] =
        TECH_RESEARCH_STATUS_RESEARCHED;

    DOCTEST_CHECK(INITSHIP::Best_Beam_Weapon(kPlayer) ==
        kTacticalBeamWeapon);
    DOCTEST_CHECK(INITSHIP::Best_Beam_Weapon(
                      kPlayer, WEAPON_MOD_FLAG_HEAVY_MOUNT) ==
        kStrategicBeamWeapon);
}

DOCTEST_TEST_CASE("Megafluxers strategic beam bonus is configurable and rounds down") {
    Reset_Initship_();
    TECHDATA::hull_data[SHIP_SIZE_SMALL].strat_beam = 3;

    DOCTEST_CHECK(
        INITSHIP::Strat_Beam_Weapon_Count(kPlayer, SHIP_SIZE_SMALL) == 3);

    players[kPlayer].tech_applications[TECH_APP_MEGAFLUXERS] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    DOCTEST_CHECK(
        INITSHIP::Strat_Beam_Weapon_Count(kPlayer, SHIP_SIZE_SMALL) == 4);

    DESIGN::megafluxers_strategic_bonus = 25;
    DOCTEST_CHECK(
        INITSHIP::Strat_Beam_Weapon_Count(kPlayer, SHIP_SIZE_SMALL) == 3);

    DESIGN::megafluxers_strategic_bonus = 100;
    DOCTEST_CHECK(
        INITSHIP::Strat_Beam_Weapon_Count(kPlayer, SHIP_SIZE_SMALL) == 6);
    DOCTEST_CHECK(
        INITSHIP::Strat_Beam_Weapon_Count(MAX_PLAYERS, SHIP_SIZE_SMALL) == 3);
}

DOCTEST_TEST_CASE("fighter point defense skips shield-only beams") {
    Reset_Initship_();

    TECHDATA::weapon_modifications[WEAPON_MOD_POINT_DEFENSE].level = 0;

    TECHDATA::weapons[kMissileWeapon].type = WEAPON_TYPE_BEAM;
    TECHDATA::weapons[kMissileWeapon].available_mods =
        WEAPON_MOD_FLAG_POINT_DEFENSE;
    TECHDATA::weapons[kMissileWeapon].max_damage = 10;

    TECHDATA::weapons[kTorpedoWeapon].type = WEAPON_TYPE_BEAM;
    TECHDATA::weapons[kTorpedoWeapon].available_mods =
        WEAPON_MOD_FLAG_POINT_DEFENSE;
    TECHDATA::weapons[kTorpedoWeapon].natural_mods =
        WEAPON_NATURAL_FLAG_ANTI_SHIELD;
    TECHDATA::weapons[kTorpedoWeapon].max_damage = 20;

    DOCTEST_CHECK(
        INITSHIP::Best_Point_Defense_Weapon(kPlayer) == kMissileWeapon);
}

DOCTEST_TEST_CASE("strategic structure preserves the maximum supported hull hit count") {
    Reset_Initship_();
    MOX::settings.strategic_combat_flag = 1;
    ships[0].d.size = SHIP_SIZE_SMALL;
    TECHDATA::hull_data[SHIP_SIZE_SMALL].strat_hits = 32767;

    DOCTEST_CHECK(INITSHIP::Get_Ship_Structure(0) == 32767);
}

DOCTEST_TEST_CASE("strategic ship bonuses use hull defense and suppress lander defense") {
    Reset_Initship_();
    s_ship_data& ship = ships[0];
    ship.owner = kPlayer;
    ship.d.ship_type = SHIP_TYPE_COMBAT;
    ship.d.size = SHIP_SIZE_SMALL;
    ship.d.combat_speed = 4;
    ship.d.computer_type = COMPUTER_ELECTRONIC_COMPUTER;
    ship.crew_quality = CREW_QUALITY_GREEN;
    ship.officer_index = -1;

    TECHDATA::drives[kStrategicDrive].tech_app_id =
        TECH_APP_NUCLEAR_DRIVE;
    TECHDATA::computers[COMPUTER_ELECTRONIC_COMPUTER].bonus = 10;
    TECHDATA::hull_data[SHIP_SIZE_SMALL].strat_def_bonus = 50;
    players[kPlayer].traits[TRAIT_TRANS_DIMENSIONAL] = 1;

    int16_t defense;
    int16_t missile_defense;
    int16_t attack;

    MOX::settings.strategic_combat_flag = 1;
    INITSHIP::Get_Ship_Combat_Bonuses(
        0, &defense, &missile_defense, &attack);
    DOCTEST_CHECK(defense == 100);
    DOCTEST_CHECK(missile_defense == 0);
    DOCTEST_CHECK(attack == 10);

    MOX::settings.strategic_combat_flag = 0;
    INITSHIP::Get_Ship_Combat_Bonuses(
        0, &defense, &missile_defense, &attack);
    DOCTEST_CHECK(defense == 40);

    MOX::settings.strategic_combat_flag = 1;
    ship.d.ship_type = SHIP_TYPE_COLONY;
    INITSHIP::Get_Ship_Combat_Bonuses(
        0, &defense, &missile_defense, &attack);
    DOCTEST_CHECK(defense == 0);
    DOCTEST_CHECK(missile_defense == 0);
    DOCTEST_CHECK(attack == 10);
}

DOCTEST_TEST_CASE("racial ship defense contributes fully to missile defense") {
    Reset_Initship_();
    s_ship_data& ship = ships[0];
    ship.owner = kPlayer;
    ship.d.ship_type = SHIP_TYPE_COMBAT;
    ship.d.size = SHIP_SIZE_SMALL;
    ship.crew_quality = CREW_QUALITY_GREEN;
    ship.officer_index = -1;
    players[kPlayer].traits[TRAIT_SHIP_DEFENSE] = 20;

    int16_t defense;
    int16_t missile_defense;
    int16_t attack;
    INITSHIP::Get_Ship_Combat_Bonuses(
        0, &defense, &missile_defense, &attack);

    DOCTEST_CHECK(defense == 20);
    DOCTEST_CHECK(missile_defense == 20);
}

DOCTEST_TEST_CASE("Battle Scanner design offense uses the configured bonus") {
    Reset_Initship_();
    s_ship_design design = {};
    design.computer_type = COMPUTER_ELECTRONIC_COMPUTER;
    TECHDATA::computers[design.computer_type].bonus = 10;
    struct_::Set_Bit_Field(
        design.special_device_flags, SPECIAL_BATTLE_SCANNER);

    int16_t attack;
    int16_t defense;
    int16_t missile_defense;
    INITSHIP::Get_Design_Combat_Bonuses(
        design, &attack, &defense, &missile_defense);
    DOCTEST_CHECK(attack == 60);

    CMBTFIR2::combat_bonus.battle_scanner_beam_offense = 37;
    INITSHIP::Get_Design_Combat_Bonuses(
        design, &attack, &defense, &missile_defense);
    DOCTEST_CHECK(attack == 47);

    CMBTFIR2::combat_bonus.battle_scanner_beam_offense = 50;
}

DOCTEST_TEST_CASE("transport extended range follows its setting") {
    Reset_Initship_();

    s_ship_design design = {};
    INITSHIP::Load_Transport_Ship_Design(kPlayer, &design);
    DOCTEST_CHECK_FALSE(struct_::Test_Bit_Field(
        design.special_device_flags, SPECIAL_EXTENDED_FUEL_TANKS));

    INITSHIP::transport_ship_extended_range = 1;
    INITSHIP::Load_Transport_Ship_Design(kPlayer, &design);
    DOCTEST_CHECK(struct_::Test_Bit_Field(
        design.special_device_flags, SPECIAL_EXTENDED_FUEL_TANKS));
}

DOCTEST_TEST_CASE("configured landing-ship size applies to every lander design") {
    Reset_Initship_();
    INITSHIP::landers_size_class = SHIP_SIZE_MEDIUM;

    s_ship_design colony = {};
    s_ship_design outpost = {};
    s_ship_design transport = {};
    INITSHIP::Load_Colony_Ship_Design(kPlayer, &colony);
    INITSHIP::Load_Outpost_Ship_Design(kPlayer, &outpost);
    INITSHIP::Load_Transport_Ship_Design(kPlayer, &transport);

    DOCTEST_CHECK(colony.size == SHIP_SIZE_MEDIUM);
    DOCTEST_CHECK(outpost.size == SHIP_SIZE_MEDIUM);
    DOCTEST_CHECK(transport.size == SHIP_SIZE_MEDIUM);
}

DOCTEST_TEST_CASE("landing ships preserve unsigned drive minimum-speed bytes") {
    Reset_Initship_();
    TECHDATA::drives[kDrive].combat_speed_min[SHIP_SIZE_SMALL] = 255;

    s_ship_design colony = {};
    INITSHIP::Load_Colony_Ship_Design(kPlayer, &colony);

    DOCTEST_CHECK((uint8_t)colony.combat_speed == 255);
}

DOCTEST_TEST_CASE("lander stealth remains disabled by default") {
    Reset_Initship_();
    players[kPlayer].tech_applications[TECH_APP_PHASING_CLOAK] =
        TECH_RESEARCH_STATUS_RESEARCHED;

    s_ship_design colony = {};
    s_ship_design outpost = {};
    s_ship_design transport = {};
    INITSHIP::Load_Colony_Ship_Design(kPlayer, &colony);
    INITSHIP::Load_Outpost_Ship_Design(kPlayer, &outpost);
    INITSHIP::Load_Transport_Ship_Design(kPlayer, &transport);

    DOCTEST_CHECK(struct_::Test_Bit_Field(
        colony.special_device_flags, SPECIAL_EXTENDED_FUEL_TANKS));
    DOCTEST_CHECK(struct_::Test_Bit_Field(
        outpost.special_device_flags, SPECIAL_EXTENDED_FUEL_TANKS));
    DOCTEST_CHECK_FALSE(struct_::Test_Bit_Field(
        colony.special_device_flags, SPECIAL_PHASING_CLOAK));
    DOCTEST_CHECK_FALSE(struct_::Test_Bit_Field(
        outpost.special_device_flags, SPECIAL_PHASING_CLOAK));
    DOCTEST_CHECK_FALSE(struct_::Test_Bit_Field(
        transport.special_device_flags, SPECIAL_PHASING_CLOAK));
}

DOCTEST_TEST_CASE("enabled landers install the best researched stealth device") {
    Reset_Initship_();
    players[kPlayer].tech_applications[TECH_APP_PHASING_CLOAK] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    INITSHIP::lander_ship_uses_stealth = {1, 1, 1};

    s_ship_design colony = {};
    s_ship_design outpost = {};
    s_ship_design transport = {};
    INITSHIP::Load_Colony_Ship_Design(kPlayer, &colony);
    INITSHIP::Load_Outpost_Ship_Design(kPlayer, &outpost);
    INITSHIP::Load_Transport_Ship_Design(kPlayer, &transport);

    DOCTEST_CHECK(struct_::Test_Bit_Field(
        colony.special_device_flags, SPECIAL_PHASING_CLOAK));
    DOCTEST_CHECK(struct_::Test_Bit_Field(
        outpost.special_device_flags, SPECIAL_PHASING_CLOAK));
    DOCTEST_CHECK(struct_::Test_Bit_Field(
        transport.special_device_flags, SPECIAL_PHASING_CLOAK));
}

DOCTEST_TEST_CASE("lander stealth selection works for nonzero players") {
    Reset_Initship_();
    constexpr int16_t player_idx = 1;
    players[player_idx].tech_applications[TECH_APP_CLOAKING_DEVICE] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    players[player_idx].tech_applications[TECH_APP_PHASING_CLOAK] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    INITSHIP::lander_ship_uses_stealth.colony_ship = 1;
    SHIPSTAK::stealth_bonus.cloaking_device = 600;

    s_ship_design design = {};
    INITSHIP::Load_Colony_Ship_Design(player_idx, &design);

    DOCTEST_CHECK(struct_::Test_Bit_Field(
        design.special_device_flags, SPECIAL_CLOAKING_DEVICE));
    DOCTEST_CHECK_FALSE(struct_::Test_Bit_Field(
        design.special_device_flags, SPECIAL_PHASING_CLOAK));

    SHIPSTAK::stealth_bonus.phasing_cloak = 600;
    INITSHIP::Load_Colony_Ship_Design(player_idx, &design);

    DOCTEST_CHECK_FALSE(struct_::Test_Bit_Field(
        design.special_device_flags, SPECIAL_CLOAKING_DEVICE));
    DOCTEST_CHECK(struct_::Test_Bit_Field(
        design.special_device_flags, SPECIAL_PHASING_CLOAK));
}