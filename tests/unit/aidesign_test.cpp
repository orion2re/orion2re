#include "pch.h"

#include "doctest/doctest.h"
#include "game/aidesign.h"
#include "game/desbox.h"

namespace {
    constexpr int16_t kPlayer = 0;

    s_player players[MAX_PLAYERS];

    const game::s_view* AI_World_() {
        static uint32_t seed;
        static const game::s_view world{
            .players = players,
            .settings = &MOX::settings,
            .stardate = &MOX::stardate,
            .combat_x = &MOX::cur_combat_x,
            .random_seed = &seed,
        };
        return &world;
    }
    bool ship_name_requested;
    int16_t requested_race;
    int16_t requested_design;
    int16_t requested_variant;

    struct s_beam_choices {
        int16_t normal_mods;
        int16_t heavy_type;
        int16_t heavy_mods;
    };

    void Reset_Aidesign_() {
        memset(players, 0, sizeof(players));
        memset(TECHDATA::technology_applications, 0, sizeof(TECHDATA::technology_applications));
        memset(TECHDATA::technology_fields, 0, sizeof(TECHDATA::technology_fields));
        memset(TECHDATA::computers, 0, sizeof(TECHDATA::computers));
        memset(TECHDATA::drives, 0, sizeof(TECHDATA::drives));
        memset(TECHDATA::hull_data, 0, sizeof(TECHDATA::hull_data));
        memset(TECHDATA::specials, 0, sizeof(TECHDATA::specials));
        memset(TECHDATA::weapons, 0, sizeof(TECHDATA::weapons));
        memset(TECHDATA::weapon_modifications, 0, sizeof(TECHDATA::weapon_modifications));
        memset(&aidesign::rules.priorities, 0, sizeof(aidesign::rules.priorities));
        aidesign::rules.black_hole_generator_chance = 3;
        aidesign::rules.stellar_converter_chance = 3;

        ship_name_requested = false;
        requested_race = -1;
        requested_design = -1;
        requested_variant = -1;
        MOX::settings = {};
        MOX::settings.game_difficulty = GAME_DIFFICULTY_AVERAGE;
        players[kPlayer].objectives = PLAYER_OBJECTIVE_HUMAN;
    }

    void Add_Beam_(int16_t weapon_id, int16_t tech_app_id, int16_t damage,
                   uint16_t available_mods, uint16_t natural_mods = 0) {
        TECHDATA::weapons[weapon_id].tech_app_id = tech_app_id;
        TECHDATA::weapons[weapon_id].type = WEAPON_TYPE_BEAM;
        TECHDATA::weapons[weapon_id].max_damage = damage;
        TECHDATA::weapons[weapon_id].available_mods = available_mods;
        TECHDATA::weapons[weapon_id].natural_mods = natural_mods;
        players[kPlayer].tech_applications[tech_app_id] = TECH_RESEARCH_STATUS_RESEARCHED;
    }

    s_beam_choices Determine_Beams_() {
        int16_t types[3] = {};
        int16_t specials[3] = {};
        int16_t arcs[3] = {};

        aidesign::Determine_Beam_Weapon_Modifications(
            AI_World_(),
            legacy_world::Player_Id(kPlayer), SHIP_SIZE_SMALL,
            &types[0], &specials[0], &arcs[0],
            &types[1], &specials[1], &arcs[1],
            &types[2], &specials[2], &arcs[2]);

        return {specials[1], types[2], specials[2]};
    }
}

namespace MOX {
    s_player* player = players;
    s_settings settings = {};
    s_current_design* design = nullptr;
    int16_t design_screen_replacement_slot = 0;
    int16_t cur_combat_x = 0;
    int32_t stardate = 0;
}

namespace TECHDATA {
    s_tech_app_data technology_applications[TECH_APP_COUNT];
    s_tech_field_data technology_fields[TECH_FIELD_COUNT];
    s_tech_computer_data computers[COMPUTER_COUNT];
    s_tech_drive_data drives[DRIVE_COUNT];
    s_tech_hull_data hull_data[HULL_CLASS_COUNT];
    s_tech_shield_data shields[SHIELD_COUNT];
    s_tech_specials_data specials[SPECIAL_COUNT];
    s_tech_weapon_data weapons[WEAPON_COUNT];
    s_tech_weapon_mod_data weapon_modifications[WEAPON_MOD_COUNT];

    int32_t Extra_Structural_Damage(int32_t damage) {
        return damage / 2;
    }
}

namespace DESIGN {
    uint8_t missile_x3_shot = 0;

    void __cdecl Get_Ship_Design_Name(int16_t race_idx, int16_t design_idx,
                                       int16_t variant_idx, char* out_name,
                                       int32_t out_name_size) {
        ship_name_requested = true;
        requested_race = race_idx;
        requested_design = design_idx;
        requested_variant = variant_idx;
        strlcpy(out_name, "Race Doom Star", out_name_size);
    }

    int16_t __cdecl Calculate_Design(int16_t, s_ship_design*, uint8_t* combat_speed) {
        *combat_speed = 0;
        return 0;
    }

    int16_t __cdecl Miniaturization_Level(int16_t, int16_t) {
        return 0;
    }

    int16_t __cdecl Tech_Level(int16_t, int16_t) {
        return 0;
    }

    int8_t __cdecl Miniaturization_Category(int16_t) {
        return 0;
    }

    int32_t __cdecl Space_Given_M_Level(int16_t amount, int16_t, int8_t) {
        return amount;
    }

    int32_t __cdecl Shield_Space_Given_M_Level(int16_t, int16_t, int16_t) {
        return 0;
    }

    int32_t __cdecl Weapon_Space(int16_t, int16_t, int16_t, int16_t, int16_t,
                                  int16_t) {
        return 10;
    }

    bool Should_Autodesign_Use_X3_Rack(int16_t, int16_t, int16_t) {
        return false;
    }

    int16_t Missile_Rack_Quantity(int16_t quantity) {
        return quantity;
    }
}

namespace INITSHIP {
    int16_t __cdecl Best_Computer(int16_t) {
        return 0;
    }

    int16_t __cdecl Best_Warp_Drive(int16_t) {
        return 0;
    }

    int16_t __cdecl Best_Armor(int16_t) {
        return 0;
    }

    int16_t __cdecl Best_Ship_Shield(int16_t) {
        return 0;
    }

    int16_t __cdecl Get_FTL_Speed(int16_t, int16_t) {
        return 0;
    }

    int16_t __cdecl Best_Point_Defense_Weapon_By_Size(int16_t) {
        return 0;
    }
}

namespace struct_ {
    void __cdecl Clear_Structure(void* dest, uint32_t size) {
        memset(dest, 0, size);
    }

    int16_t __cdecl Test_Bit_Field(void* bit_field, uint16_t bit_index) {
        uint8_t* bytes = static_cast<uint8_t*>(bit_field);
        return (bytes[bit_index / 8] & (1 << (bit_index % 8))) != 0;
    }

    void __cdecl Set_Bit_Field(void* bit_field, uint16_t bit_index) {
        uint8_t* bytes = static_cast<uint8_t*>(bit_field);
        bytes[bit_index / 8] |= (1 << (bit_index % 8));
    }
}

namespace KEN {
    void __cdecl Ken_Get_Text_Message(int32_t, char* buffer, int16_t) {
        buffer[0] = '\0';
    }
}

namespace AIPOWER {
    int16_t Get_Best_Ship_Shield_Value(const s_tech_shield_data*, game::t_player_id) {
        return 1;
    }
}

namespace game_random {
    uint32_t __cdecl Random_From_Seed(uint32_t*, uint32_t maximum) { return Random(maximum); }
    uint32_t __cdecl Random(uint32_t max_val) {
        return max_val;
    }
}

DOCTEST_TEST_CASE("heavy autodesign ignores beams without Heavy Mount") {
    Reset_Aidesign_();
    Add_Beam_(WEAPON_LASER_CANNON, TECH_APP_LASER_CANNON, 10,
              WEAPON_MOD_FLAG_HEAVY_MOUNT);
    Add_Beam_(WEAPON_PARTICLE_BEAM, TECH_APP_PARTICLE_BEAM, 100, 0);

    const s_beam_choices choices = Determine_Beams_();

    DOCTEST_CHECK(choices.heavy_type == WEAPON_LASER_CANNON);
    DOCTEST_CHECK((choices.heavy_mods & WEAPON_MOD_FLAG_HEAVY_MOUNT) != 0);
}

DOCTEST_TEST_CASE("heavy autodesign retains one legal beam") {
    Reset_Aidesign_();
    Add_Beam_(WEAPON_LASER_CANNON, TECH_APP_LASER_CANNON, 10,
              WEAPON_MOD_FLAG_HEAVY_MOUNT);

    const s_beam_choices choices = Determine_Beams_();

    DOCTEST_CHECK(choices.heavy_type == WEAPON_LASER_CANNON);
    DOCTEST_CHECK((choices.heavy_mods & WEAPON_MOD_FLAG_HEAVY_MOUNT) != 0);
}

DOCTEST_TEST_CASE("heavy autodesign rejects one legal beam at the score sentinel") {
    Reset_Aidesign_();
    Add_Beam_(WEAPON_LASER_CANNON, TECH_APP_LASER_CANNON, 0,
              WEAPON_MOD_FLAG_HEAVY_MOUNT);

    const s_beam_choices choices = Determine_Beams_();

    DOCTEST_CHECK(choices.heavy_type == 0);
    DOCTEST_CHECK(choices.heavy_mods == 0);
}

DOCTEST_TEST_CASE("heavy autodesign clears output when no beam supports Heavy Mount") {
    Reset_Aidesign_();
    Add_Beam_(WEAPON_LASER_CANNON, TECH_APP_LASER_CANNON, 10, 0);
    Add_Beam_(WEAPON_PARTICLE_BEAM, TECH_APP_PARTICLE_BEAM, 20, 0);

    const s_beam_choices choices = Determine_Beams_();

    DOCTEST_CHECK(choices.heavy_type == 0);
    DOCTEST_CHECK(choices.heavy_mods == 0);
}

DOCTEST_TEST_CASE("Capture autodesign values marine-killing heavy beams") {
    Reset_Aidesign_();
    players[kPlayer].ship_special_theme = AI_SHIP_SPECIAL_THEME_CAPTURE;
    Add_Beam_(WEAPON_LASER_CANNON, TECH_APP_LASER_CANNON, 10,
              WEAPON_MOD_FLAG_HEAVY_MOUNT,
              WEAPON_NATURAL_FLAG_KILLS_MARINES);
    Add_Beam_(WEAPON_PARTICLE_BEAM, TECH_APP_PARTICLE_BEAM, 15,
              WEAPON_MOD_FLAG_HEAVY_MOUNT);

    const s_beam_choices choices = Determine_Beams_();

    DOCTEST_CHECK(choices.heavy_type == WEAPON_LASER_CANNON);
}

DOCTEST_TEST_CASE("beam autodesign compares modification levels as unsigned bytes") {
    struct s_case {
        uint8_t level;
        bool expected_auto_fire;
    };
    const s_case cases[] = {
        {0, true},
        {127, false},
        {128, false},
        {255, false},
    };

    for (const s_case& test_case : cases) {
        Reset_Aidesign_();
        TECHDATA::computers[0].bonus = 100;
        TECHDATA::weapon_modifications[WEAPON_MOD_AUTO_FIRE].level =
            test_case.level;
        Add_Beam_(
            WEAPON_LASER_CANNON, TECH_APP_LASER_CANNON, 10,
            WEAPON_MOD_FLAG_HEAVY_MOUNT | WEAPON_MOD_FLAG_AUTO_FIRE);

        const s_beam_choices choices = Determine_Beams_();

        DOCTEST_CHECK(
            ((choices.normal_mods & WEAPON_MOD_FLAG_AUTO_FIRE) != 0) ==
            test_case.expected_auto_fire);
        DOCTEST_CHECK(
            ((choices.heavy_mods & WEAPON_MOD_FLAG_AUTO_FIRE) != 0) ==
            test_case.expected_auto_fire);
    }
}

DOCTEST_TEST_CASE("AI special themes use source-owned priority lists") {
    Reset_Aidesign_();
    players[kPlayer].ship_special_theme = AI_SHIP_SPECIAL_THEME_BIO_WEAPON;
    aidesign::rules.priorities.special_priority.bio_weapon[0] = {
        SPECIAL_TRANSPORTERS,
        AI_SPECIAL_ITEM_SPECIAL,
    };
    TECHDATA::specials[SPECIAL_TRANSPORTERS].tech_app_id = TECH_APP_TRANSPORTERS;
    players[kPlayer].tech_applications[TECH_APP_TRANSPORTERS] =
        TECH_RESEARCH_STATUS_RESEARCHED;

    s_ship_design design = {};
    int16_t weapon_slot = 0;
    aidesign::Add_Theme_Special_To_Design(
        AI_World_(),
        legacy_world::Player_Id(kPlayer), &design, 100, &weapon_slot);

    DOCTEST_CHECK(struct_::Test_Bit_Field(
        design.special_device_flags, SPECIAL_TRANSPORTERS) != 0);
}

DOCTEST_TEST_CASE("AI armor and shield themes use configured priority lists") {
    Reset_Aidesign_();
    aidesign::rules.priorities.special_priority.armor[0] = {
        SPECIAL_TRANSPORTERS,
        AI_SPECIAL_ITEM_SPECIAL,
    };
    aidesign::rules.priorities.special_priority.shield[0] = {
        SPECIAL_AUGMENTED_ENGINES,
        AI_SPECIAL_ITEM_SPECIAL,
    };
    TECHDATA::specials[SPECIAL_TRANSPORTERS].tech_app_id = TECH_APP_TRANSPORTERS;
    TECHDATA::specials[SPECIAL_AUGMENTED_ENGINES].tech_app_id =
        TECH_APP_AUGMENTED_ENGINES;
    players[kPlayer].tech_applications[TECH_APP_TRANSPORTERS] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    players[kPlayer].tech_applications[TECH_APP_AUGMENTED_ENGINES] =
        TECH_RESEARCH_STATUS_RESEARCHED;

    s_ship_design design = {};
    int16_t weapon_slot = 0;
    players[kPlayer].ship_special_theme = AI_SHIP_SPECIAL_THEME_ARMOR;
    aidesign::Add_Theme_Special_To_Design(
        AI_World_(),
        legacy_world::Player_Id(kPlayer), &design, 100, &weapon_slot);
    DOCTEST_CHECK(struct_::Test_Bit_Field(
        design.special_device_flags, SPECIAL_TRANSPORTERS) != 0);

    design = {};
    design.shield_type = 1;
    players[kPlayer].ship_special_theme = AI_SHIP_SPECIAL_THEME_SHIELD;
    aidesign::Add_Theme_Special_To_Design(
        AI_World_(),
        legacy_world::Player_Id(kPlayer), &design, 100, &weapon_slot);
    DOCTEST_CHECK(struct_::Test_Bit_Field(
        design.special_device_flags, SPECIAL_AUGMENTED_ENGINES) != 0);
}

DOCTEST_TEST_CASE("AI special-device and defense priorities use source-owned lists") {
    Reset_Aidesign_();
    aidesign::rules.priorities.special_device_priority[0] = {
        SPECIAL_TRANSPORTERS,
        AI_SPECIAL_ITEM_SPECIAL,
    };
    TECHDATA::specials[SPECIAL_TRANSPORTERS].tech_app_id = TECH_APP_TRANSPORTERS;
    players[kPlayer].tech_applications[TECH_APP_TRANSPORTERS] =
        TECH_RESEARCH_STATUS_RESEARCHED;

    DOCTEST_CHECK(aidesign::Special_Devices_Available(&aidesign::rules.priorities, &players[kPlayer]) == 1);

    aidesign::rules.priorities.special_device_priority[0] = {
        SPECIAL_TRANSPORTERS,
        AI_SPECIAL_ITEM_WEAPON,
    };
    TECHDATA::weapons[SPECIAL_TRANSPORTERS].tech_app_id = TECH_APP_TRANSPORTERS;
    DOCTEST_CHECK(aidesign::Special_Devices_Available(&aidesign::rules.priorities, &players[kPlayer]) == 1);
}

DOCTEST_TEST_CASE("AI design legality enforces every stealth and cloak pairing") {
    struct s_exclusion_pair {
        int16_t installed;
        int16_t candidate;
    };
    const s_exclusion_pair pairs[] = {
        {SPECIAL_CLOAKING_DEVICE, SPECIAL_PHASING_CLOAK},
        {SPECIAL_CLOAKING_DEVICE, SPECIAL_STEALTH_FIELD},
        {SPECIAL_PHASING_CLOAK, SPECIAL_STEALTH_FIELD},
    };

    for (const s_exclusion_pair& pair : pairs) {
        Reset_Aidesign_();
        s_ship_design design = {};
        design.shield_type = 1;
        struct_::Set_Bit_Field(design.special_device_flags, pair.installed);
        TECHDATA::specials[pair.installed].exclude[0] = pair.candidate;

        DOCTEST_CHECK_FALSE(aidesign::Special_Legal_For_Design(
            TECHDATA::specials,
            &design, pair.candidate));
    }

    Reset_Aidesign_();
    s_ship_design design = {};
    design.shield_type = 1;
    struct_::Set_Bit_Field(
        design.special_device_flags, SPECIAL_CLOAKING_DEVICE);
    TECHDATA::specials[SPECIAL_CLOAKING_DEVICE].exclude[0] = -1;
    DOCTEST_CHECK(aidesign::Special_Legal_For_Design(
        TECHDATA::specials,
        &design, SPECIAL_STEALTH_FIELD));
}

DOCTEST_TEST_CASE("human design legality enforces stealth and cloak exclusions") {
    struct s_exclusion_pair {
        int16_t installed;
        int16_t candidate;
    };
    const s_exclusion_pair pairs[] = {
        {SPECIAL_CLOAKING_DEVICE, SPECIAL_PHASING_CLOAK},
        {SPECIAL_CLOAKING_DEVICE, SPECIAL_STEALTH_FIELD},
        {SPECIAL_PHASING_CLOAK, SPECIAL_STEALTH_FIELD},
    };

    for (const s_exclusion_pair& pair : pairs) {
        Reset_Aidesign_();
        s_current_design design = {};
        design.special_devices[0] = pair.installed;
        MOX::design = &design;
        MOX::design_screen_replacement_slot = 7;
        TECHDATA::specials[pair.candidate].exclude[0] = pair.installed;

        int16_t conflict = 0;
        DOCTEST_CHECK(DESBOX::Special_System_Exclusion(
            pair.candidate, &conflict) == 1);
        DOCTEST_CHECK(conflict == pair.installed);

        MOX::design_screen_replacement_slot = 0;
        conflict = 0;
        DOCTEST_CHECK(DESBOX::Special_System_Exclusion(
            pair.candidate, &conflict) == 0);
    }

    Reset_Aidesign_();
    s_current_design design = {};
    design.special_devices[0] = SPECIAL_CLOAKING_DEVICE;
    MOX::design = &design;
    MOX::design_screen_replacement_slot = 7;
    TECHDATA::specials[SPECIAL_STEALTH_FIELD].exclude[0] = -1;
    int16_t conflict = 0;
    DOCTEST_CHECK(DESBOX::Special_System_Exclusion(
        SPECIAL_STEALTH_FIELD, &conflict) == 0);
    MOX::design = nullptr;
}

DOCTEST_TEST_CASE("Doom Star autodesign uses a race-specific name") {
    Reset_Aidesign_();
    players[kPlayer].race = 4;
    players[kPlayer].tech_applications[TECH_APP_LASER_CANNON] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    TECHDATA::hull_data[SHIP_SIZE_DOOM_STAR].size = 100;

    s_ship_design design{};
    aidesign::Auto_Design_Ship(AI_World_(), legacy_world::Player_Id(kPlayer), &design, SHIP_SIZE_DOOM_STAR);

    DOCTEST_CHECK(design.picture_num == 0x2b);
    DOCTEST_CHECK(ship_name_requested);
    DOCTEST_CHECK(requested_race == 4);
    DOCTEST_CHECK(requested_design == SHIP_SIZE_DOOM_STAR);
    DOCTEST_CHECK(requested_variant == 3);
    DOCTEST_CHECK(strcmp(design.name, "Race Doom Star") == 0);
}

DOCTEST_TEST_CASE("Megafluxers keeps the Auto Design budget wide through Battle Pods") {
    Reset_Aidesign_();
    players[kPlayer].tech_applications[TECH_APP_MEGAFLUXERS] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    players[kPlayer].tech_applications[TECH_APP_BATTLE_PODS] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    Add_Beam_(WEAPON_LASER_CANNON, TECH_APP_LASER_CANNON, 10, 0);
    TECHDATA::hull_data[SHIP_SIZE_SMALL].size = 100;
    TECHDATA::specials[SPECIAL_BATTLE_PODS]
        .space_per_ship_type[SHIP_SIZE_SMALL] = 10;
    memset(aidesign::rules.allocations.profiles, -1,
           sizeof(aidesign::rules.allocations));
    aidesign::rules.allocations.profiles[0][SHIP_SIZE_SMALL].beams = 1;
    DESIGN::megafluxers_tactical_bonus = 32768;
    MOX::cur_combat_x = -1000;

    s_ship_design design = {};
    aidesign::Auto_Design_Ship(AI_World_(), legacy_world::Player_Id(kPlayer), &design, SHIP_SIZE_SMALL);

    DOCTEST_CHECK(struct_::Test_Bit_Field(
        design.special_device_flags, SPECIAL_BATTLE_PODS));
    DOCTEST_CHECK(design.ship_weapon[0].type == WEAPON_LASER_CANNON);
    DOCTEST_CHECK(design.ship_weapon[0].count == 32);

    DESIGN::megafluxers_tactical_bonus = 25;
}

DOCTEST_TEST_CASE("Black Hole Generator chance selects the special-weapon design class") {
    Reset_Aidesign_();
    players[kPlayer].tech_applications[TECH_APP_LASER_CANNON] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    players[kPlayer].tech_applications[TECH_APP_BLACK_HOLE_GENERATOR] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    TECHDATA::hull_data[SHIP_SIZE_TITAN].size = 100;
    TECHDATA::weapons[WEAPON_BLACK_HOLE_GENERATOR].tech_app_id =
        TECH_APP_BLACK_HOLE_GENERATOR;
    memset(aidesign::rules.allocations.profiles, -1,
           sizeof(aidesign::rules.allocations));
    aidesign::rules.allocations.profiles[6][SHIP_SIZE_TITAN].special_weapon = 100;
    aidesign::rules.priorities.special_priority.special_weapon[0] = {
        WEAPON_BLACK_HOLE_GENERATOR,
        AI_SPECIAL_ITEM_WEAPON,
    };
    MOX::cur_combat_x = -1000;

    aidesign::rules.black_hole_generator_chance = 0;
    s_ship_design design = {};
    aidesign::Auto_Design_Ship(AI_World_(), legacy_world::Player_Id(kPlayer), &design, SHIP_SIZE_TITAN);
    DOCTEST_CHECK(design.ship_weapon[0].type == 0);

    aidesign::rules.black_hole_generator_chance = 4;
    aidesign::Auto_Design_Ship(AI_World_(), legacy_world::Player_Id(kPlayer), &design, SHIP_SIZE_TITAN);
    DOCTEST_CHECK(design.ship_weapon[0].type == WEAPON_BLACK_HOLE_GENERATOR);
}

DOCTEST_TEST_CASE("Stellar Converter chance selects the Doom Star special-weapon design class") {
    Reset_Aidesign_();
    players[kPlayer].tech_applications[TECH_APP_LASER_CANNON] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    players[kPlayer].tech_applications[TECH_APP_STELLAR_CONVERTER] =
        TECH_RESEARCH_STATUS_RESEARCHED;
    TECHDATA::hull_data[SHIP_SIZE_DOOM_STAR].size = 100;
    TECHDATA::weapons[WEAPON_STELLAR_CONVERTER].tech_app_id =
        TECH_APP_STELLAR_CONVERTER;
    memset(aidesign::rules.allocations.profiles, -1,
           sizeof(aidesign::rules.allocations));
    aidesign::rules.allocations.profiles[6][SHIP_SIZE_DOOM_STAR].special_weapon = 100;
    aidesign::rules.priorities.special_priority.special_weapon[0] = {
        WEAPON_STELLAR_CONVERTER,
        AI_SPECIAL_ITEM_WEAPON,
    };
    MOX::cur_combat_x = -1000;

    aidesign::rules.stellar_converter_chance = 0;
    s_ship_design design = {};
    aidesign::Auto_Design_Ship(AI_World_(), legacy_world::Player_Id(kPlayer), &design, SHIP_SIZE_DOOM_STAR);
    DOCTEST_CHECK(design.ship_weapon[0].type == 0);

    aidesign::rules.stellar_converter_chance = 4;
    aidesign::Auto_Design_Ship(AI_World_(), legacy_world::Player_Id(kPlayer), &design, SHIP_SIZE_DOOM_STAR);
    DOCTEST_CHECK(design.ship_weapon[0].type == WEAPON_STELLAR_CONVERTER);
}