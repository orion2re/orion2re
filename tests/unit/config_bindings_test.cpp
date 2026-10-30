#include "pch.h"

#include "game/build_queue.h"

#include <cstring>

#include "config/cfg_parser.h"
#include "config/config_decode.h"
#include "config/config_runtime.h"
#include "doctest/doctest.h"
#include "game/antarans.h"
#include "game/aidata.h"
#include "game/aidesign.h"
#include "game/aimove.h"
#include "game/bomb.h"
#include "game/boarding.h"
#include "game/cmbtai.h"
#include "game/cmbtfire.h"
#include "game/cmbtspec.h"
#include "game/combinit.h"
#include "game/council.h"
#include "game/cmbtmov1.h"
#include "game/cmbtmis.h"
#include "game/colcalc.h"
#include "game/colmove.h"
#include "game/colonize.h"
#include "game/combat.h"
#include "game/combat1.h"
#include "game/combfind.h"
#include "game/design_config.h"
#include "game/dip_scrn.h"
#include "game/diplomac.h"
#include "game/eric.h"
#include "game/ericnet.h"
#include "game/events.h"
#include "game/invasion.h"
#include "game/initgame.h"
#include "game/initship.h"
#include "game/info.h"
#include "game/ken.h"
#include "game/loadsave.h"
#include "game/mapgen.h"
#include "game/mainmenu.h"
#include "game/mainscr.h"
#include "game/mox.h"
#include "game/mox2.h"
#include "game/multplay.h"
#include "game/newtech.h"
#include "game/newpup.h"
#include "game/npcdiplo.h"
#include "game/officer.h"
#include "game/report.h"
#include "game/racecust.h"
#include "game/raceopt.h"
#include "game/racesel.h"
#include "game/score.h"
#include "game/ship_config.h"
#include "game/shipmove.h"
#include "game/shipstak.h"
#include "game/spy.h"
#include "game/settler.h"
#include "game/tech.h"
#include "game/techinit.h"

namespace {
    bool Append_Decoded_(const char* source, config::s_config_write_set* write_set, config::s_cfg_diagnostic* diagnostic) {
        config::s_cfg_document document;
        if (!config::Parse_Cfg("bindings.cfg", source, std::strlen(source), &document, diagnostic)) {
            return false;
        }
        for (const config::s_cfg_statement& statement : document.statements) {
            config::s_config_write_set statement_writes;
            if (!config::Decode_Assignment(&statement, &statement_writes, diagnostic)) {
                return false;
            }
            for (config::s_config_write& write : statement_writes.writes) {
                write_set->writes.push_back(std::move(write));
            }
        }
        return true;
    }

    bool States_Equal_(const config::s_config_state& left,
        const config::s_config_state& right) {
        if (left.values.size() != right.values.size()) {
            return false;
        }
        for (size_t index = 0; index < left.values.size(); ++index) {
            if (left.values[index].type != right.values[index].type ||
                left.values[index].bool_value != right.values[index].bool_value ||
                left.values[index].integer_value != right.values[index].integer_value ||
                left.values[index].string_value != right.values[index].string_value ||
                left.values[index].bitmap_value != right.values[index].bitmap_value ||
                left.values[index].ratio_numerator != right.values[index].ratio_numerator ||
                left.values[index].ratio_denominator != right.values[index].ratio_denominator) {
                return false;
            }
        }
        return true;
    }

    config::s_config_write Invalid_Write_(config::t_variable_id variable_id,
        config::e_value_type type,
        int64_t integer_value) {
        config::s_config_write write;
        write.variable_id = variable_id;
        write.value.type = type;
        write.value.integer_value = integer_value;
        write.provenance.source = "forged.cfg";
        write.provenance.line = 7;
        write.provenance.column = 9;
        return write;
    }

    config::t_variable_id Variable_Id_(const char* path) {
        config::t_variable_id id = config::CONFIG_NO_VARIABLE;
        config::Find_Variable(path, std::strlen(path), &id);
        return id;
    }

    bool Design_Has_Special_(const s_ship_design& design, uint16_t special) {
        return (static_cast<uint8_t>(design.special_device_flags[special / 8]) &
                   (1u << (special % 8))) != 0;
    }

    struct s_npc_theme_result {
        uint8_t objectives;
        uint8_t ship_design_theme;
        int8_t ship_special_theme;
    };

    s_npc_theme_result NPC_Theme_Result_(TRAIT trait, int8_t value, uint32_t random_seed) {
        memset(&MOX::player[0], 0, sizeof(MOX::player[0]));
        MOX::settings.game_difficulty = GAME_DIFFICULTY_AVERAGE;
        MOX::player[0].race = STOCK_RACE_ALKARI;
        MOX::player[0].traits[trait] = value;
        game_random::Set_Random_Seed(random_seed);
        KEN::Init_NPC_Personalities_Objectives_Themes();
        return {
            MOX::player[0].objectives,
            MOX::player[0].ship_design_theme,
            MOX::player[0].ship_special_theme,
        };
    }

    bool NPC_Theme_Results_Equal_(const s_npc_theme_result& left,
        const s_npc_theme_result& right) {
        return left.objectives == right.objectives &&
            left.ship_design_theme == right.ship_design_theme &&
            left.ship_special_theme == right.ship_special_theme;
    }
} // namespace

DOCTEST_TEST_CASE("tactical capacity settings preserve defaults and validate expanded bounds") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COMBINIT::max_ships_per_side == 100);
    DOCTEST_CHECK(COMBINIT::max_flyers_per_side == 300);
    DOCTEST_CHECK(COMBINIT::max_flyers_total == 300);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat max_ships_per_side = 500;"
        "combat max_flyers_per_side = 2500;"
        "combat max_flyers_total = 5000;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COMBINIT::max_ships_per_side == MAX_COMBAT_SHIPS_PER_SIDE);
    DOCTEST_CHECK(COMBINIT::max_flyers_per_side == MAX_COMBAT_FLYERS_PER_SIDE);
    DOCTEST_CHECK(COMBINIT::max_flyers_total == MAX_COMBAT_FLYERS);

    const char* invalid[] = {
        "combat max_ships_per_side = 0;",
        "combat max_ships_per_side = 501;",
        "combat max_flyers_per_side = 0;",
        "combat max_flyers_per_side = 2501;",
        "combat max_flyers_total = 0;",
        "combat max_flyers_total = 5001;",
    };
    for (const char* source : invalid) {
        writes.writes.clear();
        const bool accepted = Append_Decoded_(source, &writes, &diagnostic) &&
            config::Apply_Write_Set(&writes, &state, &diagnostic);
        DOCTEST_CHECK_FALSE(accepted);
    }
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
}

DOCTEST_TEST_CASE("technology tables use stable native defaults and arena-backed names") {
    config::Shutdown_Runtime();
    const uint8_t native_troops_bonus =
        TECHDATA::armor[ARMOR_TITANIUM_ARMOR].troops_bonus;
    const uint16_t native_warp_speed =
        TECHDATA::drives[DRIVE_NUCLEAR_DRIVE].warp_speed;
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));

    TECHDATA::armor[ARMOR_TITANIUM_ARMOR].troops_bonus = 99;
    TECHDATA::drives[DRIVE_NUCLEAR_DRIVE].warp_speed = 999;
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(TECHDATA::armor[ARMOR_TITANIUM_ARMOR].troops_bonus == native_troops_bonus);
    DOCTEST_CHECK(TECHDATA::drives[DRIVE_NUCLEAR_DRIVE].warp_speed == native_warp_speed);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "design tech adamantium_armor name = \"Configured Tech\";"
        "design tech adamantium_armor ai_group = 9;"
        "design armor titanium name = \"Configured Armor\";"
        "design armor titanium troops_bonus = 8;"
        "design building automated_factory maintenance = 7;"
        "design computer electronic bonus = 30;"
        "design drive nuclear warp_speed = 12;"
        "design tech_field advanced_biology cost = 1234;"
        "design fuel standard ship_range = 8;"
        "design hull frigate marines = 3;"
        "design shield class_i shield_class = 2;"
        "design special battle_pods strategic = 44;"
        "design weapon mass_driver min_damage = 6;"
        "design weapon_modification heavy_mount level = 2;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(std::strcmp(
                      TECHDATA::technology_applications[TECH_APP_ADAMANTIUM_ARMOR].name,
                      "Configured Tech") == 0);
    DOCTEST_CHECK(config::Is_Active_String(
        TECHDATA::technology_applications[TECH_APP_ADAMANTIUM_ARMOR].name));
    DOCTEST_CHECK(TECHDATA::technology_applications[TECH_APP_ADAMANTIUM_ARMOR].ai_group == 9);
    DOCTEST_CHECK(std::strcmp(TECHDATA::armor[ARMOR_TITANIUM_ARMOR].name,
                      "Configured Armor") == 0);
    DOCTEST_CHECK(TECHDATA::armor[ARMOR_TITANIUM_ARMOR].troops_bonus == 8);
    DOCTEST_CHECK(TECHDATA::buildings[BUILDING_AUTOMATED_FACTORY].maintenance == 7);
    DOCTEST_CHECK(TECHDATA::computers[COMPUTER_ELECTRONIC_COMPUTER].bonus == 30);
    DOCTEST_CHECK(TECHDATA::drives[DRIVE_NUCLEAR_DRIVE].warp_speed == 12);
    DOCTEST_CHECK(TECHDATA::technology_fields[TECH_FIELD_ADVANCED_BIOLOGY].cost == 1234);
    DOCTEST_CHECK(TECHDATA::fuel[FUEL_STANDARD_FUEL_CELLS].ship_range == 8);
    DOCTEST_CHECK(TECHDATA::hull_data[HULL_FRIGATE].marines == 3);
    DOCTEST_CHECK(TECHDATA::shields[SHIELD_CLASS_I_SHIELD].shield_class == 2);
    DOCTEST_CHECK(TECHDATA::specials[SPECIAL_BATTLE_PODS].strategic == 44);
    DOCTEST_CHECK(TECHDATA::weapons[WEAPON_MASS_DRIVER].min_damage == 6);
    DOCTEST_CHECK(TECHDATA::weapon_modifications[WEAPON_MOD_HEAVY_MOUNT].level == 2);

    config::s_config_write_set clear_name;
    DOCTEST_REQUIRE(Append_Decoded_(
        "design armor titanium name = \"\";", &clear_name, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&clear_name, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(TECHDATA::armor[ARMOR_TITANIUM_ARMOR].name == nullptr);

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(TECHDATA::armor[ARMOR_TITANIUM_ARMOR].troops_bonus == native_troops_bonus);
    DOCTEST_CHECK(TECHDATA::drives[DRIVE_NUCLEAR_DRIVE].warp_speed == native_warp_speed);
    config::Shutdown_Runtime();
}

DOCTEST_TEST_CASE("technology field links use zero termination and valid field indexes") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "design tech_field advanced_biology prev_field_id = starting_tech;"
        "design tech_field advanced_biology next_field_id = sociology;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(TECHDATA::technology_fields[TECH_FIELD_ADVANCED_BIOLOGY].prev_field_id ==
        TECH_FIELD_STARTING_TECH);
    DOCTEST_CHECK(TECHDATA::technology_fields[TECH_FIELD_ADVANCED_BIOLOGY].next_field_id ==
        TECH_FIELD_SOCIOLOGY);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "design tech_field advanced_biology prev_field_id = -1;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "design tech_field advanced_biology next_field_id = 83;",
        &writes,
        &diagnostic));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
}

DOCTEST_TEST_CASE("hull size and hit points preserve unsigned word domains") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "design hull frigate size = 32767;"
        "design hull destroyer size = 32768;"
        "design hull cruiser size = 65535;"
        "design hull frigate armor_hp = 32767;"
        "design hull destroyer armor_hp = 32768;"
        "design hull cruiser armor_hp = 65535;"
        "design hull frigate strat_hits = 32767;"
        "design hull destroyer strat_hits = 32768;"
        "design hull cruiser strat_hits = 65535;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(TECHDATA::hull_data[HULL_FRIGATE].size == 32767);
    DOCTEST_CHECK(TECHDATA::hull_data[HULL_DESTROYER].size == 32768);
    DOCTEST_CHECK(TECHDATA::hull_data[HULL_CRUISER].size == 65535);
    DOCTEST_CHECK(TECHDATA::hull_data[HULL_FRIGATE].armor_hp == 32767);
    DOCTEST_CHECK(TECHDATA::hull_data[HULL_DESTROYER].armor_hp == 32768);
    DOCTEST_CHECK(TECHDATA::hull_data[HULL_CRUISER].armor_hp == 65535);
    DOCTEST_CHECK(TECHDATA::hull_data[HULL_FRIGATE].strat_hits == 32767);
    DOCTEST_CHECK(TECHDATA::hull_data[HULL_DESTROYER].strat_hits == 32768);
    DOCTEST_CHECK(TECHDATA::hull_data[HULL_CRUISER].strat_hits == 65535);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "design hull frigate size = -1;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "design hull frigate armor_hp = 65536;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "design hull frigate strat_hits = 65536;",
        &writes,
        &diagnostic));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
}

DOCTEST_TEST_CASE("ship special exclusions preserve signed 16-bit configuration") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "design special stealth_field exclude frigate = -32768;"
        "design special stealth_field exclude destroyer = -1;"
        "design special stealth_field exclude cruiser = 0;"
        "design special stealth_field exclude battleship = 6;"
        "design special stealth_field exclude titan = 23;"
        "design special stealth_field exclude doomstar = 32767;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    const int16_t expected[6] = {-32768, -1, 0, 6, 23, 32767};
    DOCTEST_CHECK(std::memcmp(
                      TECHDATA::specials[SPECIAL_STEALTH_FIELD].exclude,
                      expected,
                      sizeof(expected)) == 0);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "design special stealth_field exclude frigate = -32769;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "design special stealth_field exclude frigate = 32768;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "design special stealth_field exclude frigate = 65535;",
        &writes,
        &diagnostic));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
}

DOCTEST_TEST_CASE("building data preserves unsigned identity word and byte domains") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "design building automated_factory building_id = 48;"
        "design building automated_factory cost = 65535;"
        "design building automated_factory maintenance = 65535;"
        "design building automated_factory group = 255;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(TECHDATA::buildings[BUILDING_AUTOMATED_FACTORY].building_id == 48);
    DOCTEST_CHECK(TECHDATA::buildings[BUILDING_AUTOMATED_FACTORY].cost == 65535);
    DOCTEST_CHECK(TECHDATA::buildings[BUILDING_AUTOMATED_FACTORY].maintenance == 65535);
    DOCTEST_CHECK(TECHDATA::buildings[BUILDING_AUTOMATED_FACTORY].group == 255);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "design building automated_factory building_id = 49;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "design building automated_factory cost = -1;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "design building automated_factory cost = 65536;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "design building automated_factory maintenance = -1;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "design building automated_factory group = 256;",
        &writes,
        &diagnostic));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
}

DOCTEST_TEST_CASE("weapon cost and tactical damage preserve unsigned words") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "design weapon mass_driver cost = 65535;"
        "design weapon particle_beam min_damage = 65535;"
        "design weapon particle_beam max_damage = 65535;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(TECHDATA::weapons[WEAPON_MASS_DRIVER].cost == 65535);
    DOCTEST_CHECK(TECHDATA::weapons[WEAPON_PARTICLE_BEAM].min_damage == 65535);
    DOCTEST_CHECK(TECHDATA::weapons[WEAPON_PARTICLE_BEAM].max_damage == 65535);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "design weapon mass_driver cost = -1;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "design weapon particle_beam min_damage = 65536;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "design weapon particle_beam max_damage = -1;",
        &writes,
        &diagnostic));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
}

DOCTEST_TEST_CASE("available weapon modification bit zero is numeric only") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "design weapon mass_driver available_mods = 1;"
        "design weapon particle_beam available_mods = 32766;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(TECHDATA::weapons[WEAPON_MASS_DRIVER].available_mods == 1);
    DOCTEST_CHECK(TECHDATA::weapons[WEAPON_PARTICLE_BEAM].available_mods == 32766);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "design weapon mass_driver available_mods = reserved_0;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "design weapon mass_driver available_mods = 32767;",
        &writes,
        &diagnostic));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
}

DOCTEST_TEST_CASE("special space and cost remain signed while strategic is unsigned") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "design special battle_pods space frigate = -32768;"
        "design special battle_pods cost frigate = -1;"
        "design special battle_pods strategic = 65535;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(TECHDATA::specials[SPECIAL_BATTLE_PODS]
                      .space_per_ship_type[SHIP_SIZE_SMALL] == -32768);
    DOCTEST_CHECK(TECHDATA::specials[SPECIAL_BATTLE_PODS]
                      .cost_per_ship_type[SHIP_SIZE_SMALL] == -1);
    DOCTEST_CHECK(TECHDATA::specials[SPECIAL_BATTLE_PODS].strategic == 65535);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "design special battle_pods space frigate = 32768;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "design special battle_pods cost frigate = -32769;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "design special battle_pods strategic = -1;",
        &writes,
        &diagnostic));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
}

DOCTEST_TEST_CASE("weapon modification levels preserve the unsigned byte domain") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "design weapon_modification no_mods level = 0;"
        "design weapon_modification armor_piercing level = 127;"
        "design weapon_modification auto_fire level = 128;"
        "design weapon_modification enveloping level = 255;"
        "design weapon_modification heavy_mount mod_id = 255;"
        "design weapon_modification heavy_mount cost = -32768;"
        "design weapon_modification heavy_mount space = 32767;"
        "design weapon_modification heavy_mount legacy_mount_marker = 255;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(
        TECHDATA::weapon_modifications[WEAPON_MOD_NO_MODS].level == 0);
    DOCTEST_CHECK(
        TECHDATA::weapon_modifications[WEAPON_MOD_ARMOR_PIERCING].level == 127);
    DOCTEST_CHECK(
        TECHDATA::weapon_modifications[WEAPON_MOD_AUTO_FIRE].level == 128);
    DOCTEST_CHECK(
        TECHDATA::weapon_modifications[WEAPON_MOD_ENVELOPING].level == 255);
    DOCTEST_CHECK(
        TECHDATA::weapon_modifications[WEAPON_MOD_HEAVY_MOUNT].mod_id == 255);
    DOCTEST_CHECK(
        TECHDATA::weapon_modifications[WEAPON_MOD_HEAVY_MOUNT].cost == -32768);
    DOCTEST_CHECK(
        TECHDATA::weapon_modifications[WEAPON_MOD_HEAVY_MOUNT].space == 32767);
    DOCTEST_CHECK(
        TECHDATA::weapon_modifications[WEAPON_MOD_HEAVY_MOUNT].col5 == 255);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "design weapon_modification auto_fire level = -1;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "design weapon_modification auto_fire level = 256;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "design weapon_modification heavy_mount mod_id = 256;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "design weapon_modification heavy_mount cost = -32769;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "design weapon_modification heavy_mount space = 32768;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "design weapon_modification heavy_mount legacy_mount_marker = 256;",
        &writes,
        &diagnostic));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
}

DOCTEST_TEST_CASE("drive speeds preserve their domains and fit ship designs") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "design drive none combat_speed_min frigate = 0;"
        "design drive nuclear combat_speed_min destroyer = 127;"
        "design drive nuclear combat_speed_min cruiser = 128;"
        "design drive nuclear combat_speed_min battleship = 255;"
        "design drive nuclear warp_speed = 253;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(TECHDATA::drives[DRIVE_NO_DRIVE]
                      .combat_speed_min[SHIP_SIZE_SMALL] == 0);
    DOCTEST_CHECK(TECHDATA::drives[DRIVE_NUCLEAR_DRIVE]
                      .combat_speed_min[SHIP_SIZE_MEDIUM] == 127);
    DOCTEST_CHECK(TECHDATA::drives[DRIVE_NUCLEAR_DRIVE]
                      .combat_speed_min[SHIP_SIZE_LARGE] == 128);
    DOCTEST_CHECK(TECHDATA::drives[DRIVE_NUCLEAR_DRIVE]
                      .combat_speed_min[SHIP_SIZE_HUGE] == 255);
    DOCTEST_CHECK(TECHDATA::drives[DRIVE_NUCLEAR_DRIVE].warp_speed == 253);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "design drive nuclear combat_speed_min frigate = -1;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "design drive nuclear combat_speed_min frigate = 256;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_REQUIRE(Append_Decoded_(
        "design drive nuclear warp_speed = 256;", &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_CHECK_FALSE(config::Commit_State(&state));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    writes.writes.clear();
    DOCTEST_REQUIRE(Append_Decoded_(
        "design drive nuclear warp_speed = 254;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_CHECK_FALSE(config::Commit_State(&state));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
}

DOCTEST_TEST_CASE("fuel range retains the widened unsigned word domain") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "design fuel none ship_range = 0;"
        "design fuel standard ship_range = 256;"
        "design fuel thorium ship_range = 65535;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(TECHDATA::fuel[FUEL_NO_FUEL_CELLS].ship_range == 0);
    DOCTEST_CHECK(TECHDATA::fuel[FUEL_STANDARD_FUEL_CELLS].ship_range == 256);
    DOCTEST_CHECK(TECHDATA::fuel[FUEL_THORIUM_FUEL_CELLS].ship_range == 65535);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "design fuel standard ship_range = 65536;",
        &writes,
        &diagnostic));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
}

DOCTEST_TEST_CASE("design weapon-type modification masks preserve 1.50 defaults") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(DESBOX::mods_available.beam ==
        (WEAPON_MOD_FLAG_HEAVY_MOUNT |
            WEAPON_MOD_FLAG_POINT_DEFENSE |
            WEAPON_MOD_FLAG_ARMOR_PIERCING |
            WEAPON_MOD_FLAG_CONTINUOUS |
            WEAPON_MOD_FLAG_NO_RANGE_DISSIPATION |
            WEAPON_MOD_FLAG_SHIELD_PIERCING |
            WEAPON_MOD_FLAG_AUTO_FIRE |
            WEAPON_MOD_FLAG_ENVELOPING));
    DOCTEST_CHECK(DESBOX::mods_available.missile ==
        (WEAPON_MOD_FLAG_MIRV |
            WEAPON_MOD_FLAG_ECCM |
            WEAPON_MOD_FLAG_HEAVILY_ARMORED |
            WEAPON_MOD_FLAG_FAST |
            WEAPON_MOD_FLAG_EMISSION_GUIDANCE));
    DOCTEST_CHECK(DESBOX::mods_available.torpedo ==
        (WEAPON_MOD_FLAG_NO_RANGE_DISSIPATION |
            WEAPON_MOD_FLAG_ENVELOPING |
            WEAPON_MOD_FLAG_ECCM |
            WEAPON_MOD_FLAG_EMISSION_GUIDANCE |
            WEAPON_MOD_FLAG_OVERLOADED));

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "design mods_available beam = hv,co;"
        "design mods_available missile = eccm,fst;"
        "design mods_available torpedo = nr,ovr;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(DESBOX::mods_available.beam ==
        (WEAPON_MOD_FLAG_HEAVY_MOUNT |
            WEAPON_MOD_FLAG_CONTINUOUS));
    DOCTEST_CHECK(DESBOX::mods_available.missile ==
        (WEAPON_MOD_FLAG_ECCM | WEAPON_MOD_FLAG_FAST));
    DOCTEST_CHECK(DESBOX::mods_available.torpedo ==
        (WEAPON_MOD_FLAG_NO_RANGE_DISSIPATION |
            WEAPON_MOD_FLAG_OVERLOADED));

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "design mods_available beam = unknown_mod;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "design mods_available missile = 65536;",
        &writes,
        &diagnostic));
}

DOCTEST_TEST_CASE("build queue defaults match the eleven 1.50 hotkey lists") {
    config::s_config_state defaults;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&defaults));
    DOCTEST_REQUIRE(config::Commit_State(&defaults));

    DOCTEST_CHECK(BUILD_QUEUE::presets[0][0] == BUILD_QUEUE::INSTRUCTION_CLEAR_CONTINUOUS);
    DOCTEST_CHECK(BUILD_QUEUE::presets[0][1] == BUILD_QUEUE::INSTRUCTION_MERGE);
    DOCTEST_CHECK(BUILD_QUEUE::presets[0][2] ==
        BUILD_QUEUE::INSTRUCTION_BUILDING_BASE + BUILDING_ALIEN_CONTROL_CENTER);
    DOCTEST_CHECK(BUILD_QUEUE::presets[1][0] == BUILD_QUEUE::INSTRUCTION_PREPEND);
    DOCTEST_CHECK(BUILD_QUEUE::presets[1][1] == BUILD_QUEUE::INSTRUCTION_DESIGN1);
    DOCTEST_CHECK(BUILD_QUEUE::presets[7][1] == BUILD_QUEUE::INSTRUCTION_FREIGHTER_FLEET);
    DOCTEST_CHECK(BUILD_QUEUE::presets[8][1] == BUILD_QUEUE::INSTRUCTION_SPY);
    DOCTEST_CHECK(BUILD_QUEUE::presets[9][0] == BUILD_QUEUE::INSTRUCTION_HOUSING);
    DOCTEST_CHECK(BUILD_QUEUE::presets[10][0] == BUILD_QUEUE::INSTRUCTION_CLEAR);
    for (int preset = 0; preset < BUILD_QUEUE::PRESET_COUNT; ++preset) {
        DOCTEST_CHECK(BUILD_QUEUE::presets[preset][BUILD_QUEUE::INSTRUCTION_CAPACITY - 1] ==
            BUILD_QUEUE::INSTRUCTION_END);
        bool found_end = false;
        for (int instruction = 0; instruction < BUILD_QUEUE::INSTRUCTION_CAPACITY;
             ++instruction) {
            found_end = found_end ||
                BUILD_QUEUE::presets[preset][instruction] == BUILD_QUEUE::INSTRUCTION_END;
        }
        DOCTEST_CHECK(found_end);
    }
}

DOCTEST_TEST_CASE("Tech Review ordering binds named applications and groups") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(INFO::review_weapons[41].application == TECH_APP_TRACTOR_BEAM);
    DOCTEST_CHECK(INFO::review_weapons[41].group == TECH_REVIEW_GROUP_NONE);
    DOCTEST_CHECK(INFO::review_equipment[0].application == TECH_APP_NO_TECH);
    DOCTEST_CHECK(INFO::review_equipment[0].group == TECH_REVIEW_GROUP_SHIELDS);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "interface tech_review_weapons 42 application = plasma_web;"
        "interface tech_review_equipment 1 group = armor;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(INFO::review_weapons[41].application == TECH_APP_PLASMA_WEB);
    DOCTEST_CHECK(INFO::review_equipment[0].group == TECH_REVIEW_GROUP_ARMOR);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "interface tech_review_weapons 42 application = beams;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "interface tech_review_weapons 42 group = tractor_beam;",
        &writes,
        &diagnostic));
}

DOCTEST_TEST_CASE("Tech Review full lists allow swapping technologies between categories") {
    std::string source = R"(
interface tech_review_achievements =
    no_tech new_construction_types
    doom_star_construction none
    planet_construction none
    titan_construction none
    transport none
    freighters none
    outpost_ship none
    colony_ship none
    colony_base none
    no_tech spies
    psionics none
    stealth_suit none
    cyber_security_link none
    telepathic_training none
    neural_scanner none
    spy_network none
    no_tech ground_combat
    battleoids none
    powered_armor none
    personal_shield none
    anti_grav_harness none
    plasma_rifle none
    phasor_rifle none
    fusion_rifle none
    laser_rifle none
    pulse_rifle none
    no_tech ship_equipment
    galactic_currency_exchange none
    megafluxers none
    emissions_guidance_system none
    dauntless_guidance_system none
    advanced_damage_control none
    survival_pods none
    no_tech colony
    star_gate none
    jump_gate none
    android_scientists none
    android_workers none
    android_farmers none
    advanced_city_planning none
    nano_disassemblers none
    microlite_construction none
    biomorphic_fungi none
    universal_antidote none
    microbiotics none
    no_tech miscellaneous
    hyperspace_communications none
    sub_space_communications none
    tachyon_communications none
    evolutionary_mutation none
    heightened_intelligence none
    xeno_psychology none
    imperium none
    galactic_unification none
    federation none
    confederation none
    capitol none
    no_tech none;
interface tech_review_colony =
    no_tech food
    gaia_transformation none
    terraforming none
    food_replicators none
    weather_control_system none
    soil_enrichment none
    subterranean_farms none
    hydroponic_farm none
    no_tech production
    deep_core_mining none
    robotic_factory none
    recyclotron none
    robo_miners none
    automated_factories none
    no_tech pollution
    core_waste_dumps none
    atmospheric_renewer none
    pollution_processor none
    no_tech research
    galactic_cybernet none
    autolab none
    planetary_supercomputer none
    research_laboratory none
    no_tech money
    planetary_stock_exchange none
    spaceport none
    no_tech morale
    virtual_reality_network none
    pleasure_dome none
    holo_simulator none
    no_tech colony_defense
    warp_interdictor none
    artemis_system_net none
    star_fortress none
    battlestation none
    star_base none
    planetary_barrier_shield none
    planetary_flux_shield none
    planetary_radiation_shield none
    ground_batteries none
    fighter_garrison none
    planetary_missile_base none
    armor_barracks none
    marine_barracks none
    no_tech miscellaneous
    dimensional_portal none
    planetary_gravity_generator none
    astro_university none
    alien_management_center none
    space_academy none
    cloning_center none
    biospheres none
    no_tech none;
)";
    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;

    DOCTEST_SUBCASE("one-for-one exchange preserves both list capacities") {
        config::s_config_state state;
        DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
        DOCTEST_REQUIRE_MESSAGE(Append_Decoded_(source.c_str(), &writes, &diagnostic),
            diagnostic.message);
        DOCTEST_CHECK(writes.writes.size() == 2 * (58 + 53));
        DOCTEST_REQUIRE_MESSAGE(config::Apply_Write_Set(&writes, &state, &diagnostic),
            diagnostic.message);
        DOCTEST_REQUIRE(config::Commit_State(&state));

        DOCTEST_CHECK(INFO::review_achievements[26].group == TECH_REVIEW_GROUP_SHIP_EQUIPMENT);
        DOCTEST_CHECK(INFO::review_achievements[27].application == TECH_APP_GALACTIC_CURRENCY_EXCHANGE);
        DOCTEST_CHECK(INFO::review_achievements[27].group == TECH_REVIEW_GROUP_NONE);
        DOCTEST_CHECK(INFO::review_colony[26].group == TECH_REVIEW_GROUP_MORALE);
        DOCTEST_CHECK(INFO::review_colony[27].application == TECH_APP_VIRTUAL_REALITY_NETWORK);
        DOCTEST_CHECK(INFO::review_colony[27].group == TECH_REVIEW_GROUP_NONE);
        for (const s_tech_review_achievement& entry : INFO::review_achievements) {
            DOCTEST_CHECK(entry.application != TECH_APP_VIRTUAL_REALITY_NETWORK);
        }
        for (const s_tech_review_achievement& entry : INFO::review_colony) {
            DOCTEST_CHECK(entry.application != TECH_APP_GALACTIC_CURRENCY_EXCHANGE);
        }
    }

    DOCTEST_SUBCASE("an extra colony entry fits beyond the original capacity") {
        source.insert(source.rfind("    no_tech none;"), "    virtual_reality_network none\n");
        config::s_config_state state;
        DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
        DOCTEST_REQUIRE_MESSAGE(Append_Decoded_(source.c_str(), &writes, &diagnostic),
            diagnostic.message);
        DOCTEST_REQUIRE_MESSAGE(config::Apply_Write_Set(&writes, &state, &diagnostic),
            diagnostic.message);
        DOCTEST_REQUIRE(config::Commit_State(&state));
        DOCTEST_CHECK(INFO::review_colony[52].application == TECH_APP_VIRTUAL_REALITY_NETWORK);
        DOCTEST_CHECK(INFO::review_colony[52].group == TECH_REVIEW_GROUP_NONE);
        DOCTEST_CHECK(INFO::review_colony[53].application == TECH_APP_NO_TECH);
        DOCTEST_CHECK(INFO::review_colony[53].group == TECH_REVIEW_GROUP_NONE);
    }
}

DOCTEST_TEST_CASE("Tech Review categories accept 256 slots and reject overflow") {
    const char* categories[] = {
        "tech_review_achievements",
        "tech_review_colony",
        "tech_review_weapons",
        "tech_review_equipment",
    };
    for (int category = 0; category < 4; ++category) {
        DOCTEST_CAPTURE(categories[category]);
        config::s_config_state state;
        DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
        const std::string path = std::string("interface ") + categories[category];
        std::string source = path + " =\n";
        for (int entry = 0; entry < 255; ++entry) {
            source += entry % 17 == 0 ? "no_tech miscellaneous\n" : "virtual_reality_network none\n";
        }
        source += "no_tech none;";

        config::s_config_write_set writes;
        config::s_cfg_diagnostic diagnostic;
        DOCTEST_REQUIRE_MESSAGE(Append_Decoded_(source.c_str(), &writes, &diagnostic),
            diagnostic.message);
        DOCTEST_CHECK(writes.writes.size() == 512);
        DOCTEST_REQUIRE_MESSAGE(config::Apply_Write_Set(&writes, &state, &diagnostic),
            diagnostic.message);
        DOCTEST_REQUIRE(config::Commit_State(&state));
        const s_tech_review_achievement* review = INFO::review_list[category];
        for (int entry = 0; entry < 255; ++entry) {
            DOCTEST_CHECK(review[entry].application ==
                (entry % 17 == 0 ? TECH_APP_NO_TECH : TECH_APP_VIRTUAL_REALITY_NETWORK));
            DOCTEST_CHECK(review[entry].group ==
                (entry % 17 == 0 ? TECH_REVIEW_GROUP_MISCELLANEOUS : TECH_REVIEW_GROUP_NONE));
        }
        DOCTEST_CHECK(review[255].application == TECH_APP_NO_TECH);
        DOCTEST_CHECK(review[255].group == TECH_REVIEW_GROUP_NONE);

        writes.writes.clear();
        DOCTEST_REQUIRE(Append_Decoded_((path + " 256 = no_tech none;").c_str(),
            &writes,
            &diagnostic));
        DOCTEST_CHECK(writes.writes.size() == 2);
        writes.writes.clear();
        DOCTEST_CHECK_FALSE(Append_Decoded_((path + " 257 = no_tech none;").c_str(),
            &writes,
            &diagnostic));

        writes.writes.clear();
        DOCTEST_REQUIRE(Append_Decoded_(
            (path + " = no_tech miscellaneous virtual_reality_network none no_tech none;").c_str(),
            &writes,
            &diagnostic));
        DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
        DOCTEST_REQUIRE(config::Commit_State(&state));
        DOCTEST_CHECK(review[2].application == TECH_APP_NO_TECH);
        DOCTEST_CHECK(review[2].group == TECH_REVIEW_GROUP_NONE);

        writes.writes.clear();
        source.insert(source.rfind("no_tech none;"), "virtual_reality_network none\n");
        DOCTEST_REQUIRE_FALSE(Append_Decoded_(source.c_str(), &writes, &diagnostic));
        DOCTEST_CHECK(diagnostic.message ==
            std::string("value 513 exceeds selected configuration object 'interface.") +
                categories[category] + "'");
    }
}

DOCTEST_TEST_CASE("colony autobuild settings bind and validate queue defaults") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(BUILD_QUEUE::autobuild_settings.enabled == 0);
    DOCTEST_CHECK(BUILD_QUEUE::autobuild_settings.default_preset == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "colony autobuild enabled = true; colony autobuild default = 10;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(BUILD_QUEUE::autobuild_settings.enabled == 1);
    DOCTEST_CHECK(BUILD_QUEUE::autobuild_settings.default_preset == 10);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "colony autobuild default = 11;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "experiment autobuild enabled = true;", &writes, &diagnostic));
}

DOCTEST_TEST_CASE("AI production rules support labeled sparse overrides and native defaults") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(AIBUILD::autobuild_rules.cutoff.mode ==
        production_rules::CUTOFF_RELATIVE);
    DOCTEST_CHECK(AIBUILD::autobuild_rules.candidates[BUILDING_COUNT].cutoff_stage ==
        production_rules::CUTOFF_STAGE_AFTER);
    DOCTEST_CHECK(AIBUILD::autobuild_rules.candidates[BUILDING_FOOD_REPLICATORS]
                      .eligibility.all.metric_conditions[0]
                      .operator_ == production_rules::COMPARISON_IGNORE);
    DOCTEST_CHECK(AIBUILD::strategic_production.emergency_defense.order[0] ==
        BUILDING_MISSILE_BASE);
    DOCTEST_CHECK(AIBUILD::strategic_production.allocation.candidates[5].weight.defer.peace == 333);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "ai autobuild_rules cutoff mode = disabled;"
        "ai autobuild_rules candidates automated_factory weight = base: 17 formula: additive;"
        "ai autobuild_rules candidates automated_factory weight metric_terms 1 = "
        "metric: population factor: 3/2;"
        "ai autobuild_rules candidates research_lab eligibility flags = "
        "research_enabled: required building_focus: forbidden;"
        "ai strategic_production emergency_defense order 1 = radiation_shield;"
        "ai strategic_production allocation candidates defer weight defer = peace: 444;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(AIBUILD::autobuild_rules.cutoff.mode ==
        production_rules::CUTOFF_DISABLED);
    DOCTEST_CHECK(AIBUILD::autobuild_rules.candidates[BUILDING_AUTOMATED_FACTORY]
                      .weight.base == 17);
    DOCTEST_CHECK(AIBUILD::autobuild_rules.candidates[BUILDING_AUTOMATED_FACTORY]
                      .weight.metric_terms[0]
                      .factor.numerator == 3);
    DOCTEST_CHECK(AIBUILD::autobuild_rules.candidates[BUILDING_AUTOMATED_FACTORY]
                      .weight.metric_terms[0]
                      .factor.denominator == 2);
    DOCTEST_CHECK(AIBUILD::autobuild_rules.candidates[BUILDING_RESEARCH_LAB]
                      .eligibility.all.flags[production_rules::FLAG_RESEARCH_ENABLED] ==
        production_rules::REQUIREMENT_REQUIRED);
    DOCTEST_CHECK(AIBUILD::strategic_production.emergency_defense.order[0] ==
        BUILDING_RADIATION_SHIELD);
    DOCTEST_CHECK(AIBUILD::strategic_production.allocation.candidates[5].weight.defer.peace == 444);

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(AIBUILD::autobuild_rules.cutoff.mode ==
        production_rules::CUTOFF_RELATIVE);
    DOCTEST_CHECK(AIBUILD::strategic_production.emergency_defense.order[0] ==
        BUILDING_MISSILE_BASE);
}

DOCTEST_TEST_CASE("AI production rules are saved and synchronized gameplay settings") {
    const char* paths[] = {
        "ai.autobuild_rules.cutoff.mode",
        "ai.autobuild_rules.candidates.7.weight.base",
        "ai.strategic_production.emergency_defense.order.1",
        "ai.strategic_production.allocation.candidates.5.weight.defer.peace",
    };
    for (const char* path : paths) {
        const config::t_variable_id variable = Variable_Id_(path);
        DOCTEST_REQUIRE(variable != config::CONFIG_NO_VARIABLE);
        const config::s_config_group_descriptor* group =
            config::Get_Config_Group(config::Get_Variable_Group(variable));
        DOCTEST_REQUIRE(group != nullptr);
        DOCTEST_CHECK(std::strcmp(group->id, "gameplay") == 0);
        DOCTEST_CHECK(group->save);
        DOCTEST_CHECK(group->synchronize);
    }
}

DOCTEST_TEST_CASE("combat scroll interval preserves the 1.50 default") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COMBINIT::scroll_interval == 1);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "interface scroll_interval = 4;", &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COMBINIT::scroll_interval == 4);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "interface scroll_interval = 256;", &writes, &diagnostic));
}

DOCTEST_TEST_CASE("interface and colony formula settings preserve 1.50 defaults") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(platform::g_present_scaler == platform::PRESENT_SCALER_PIXELART);
    DOCTEST_CHECK(platform::g_vsync_enabled == 1);
    DOCTEST_CHECK(ERIC::fade_speed.fade_in == 25);
    DOCTEST_CHECK(ERIC::fade_speed.fade_out == 25);
    DOCTEST_CHECK(COLCALC::growth_formula_factor == 2000);
    DOCTEST_CHECK(COLONY::max_buildings_scrapped_per_turn == 1);
    DOCTEST_CHECK(COLCALC::housing_formula_factor == 40);
    DOCTEST_CHECK(COLCALC::cloning_center_growth_bonus == 100);
    DOCTEST_CHECK(MAINSCR::wormhole_link_color == 4);
    DOCTEST_CHECK_FALSE(MAINSCR::main_map_overview_markers);
    DOCTEST_CHECK_FALSE(MAINMENU::show_mod_list);
    DOCTEST_CHECK(INFO::history_graph_smoothing);
    DOCTEST_CHECK(COMBAT1::center_on_next_ship);
    DOCTEST_CHECK(maintain::ship_scrap_refund_percent == 25);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "interface scaler = hq3x;"
        "interface vsync_enabled = false;"
        "interface screen_fade_speed = 100;"
        "interface wormhole_link_color = 20;"
        "interface main_map_overview_markers = true;"
        "interface main_menu_mod_list = true;"
        "interface history_graph_smoothing = false;"
        "interface center_on_next_ship = false;"
        "economy ship_scrap_refund_percent = double;"
        "colony growth_formula_factor = 123456;"
        "colony max_buildings_scrapped_per_turn = 3;"
        "colony housing_formula_factor = 127;"
        "colony cloning_center_growth_bonus = 37;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(platform::g_present_scaler == platform::PRESENT_SCALER_HQ3X);
    DOCTEST_CHECK(platform::g_vsync_enabled == 0);
    DOCTEST_CHECK(ERIC::fade_speed.fade_in == 100);
    DOCTEST_CHECK(ERIC::fade_speed.fade_out == 100);
    DOCTEST_CHECK(MAINSCR::wormhole_link_color == 20);
    DOCTEST_CHECK(MAINSCR::main_map_overview_markers);
    DOCTEST_CHECK(MAINMENU::show_mod_list);
    DOCTEST_CHECK(COLCALC::growth_formula_factor == 123456);
    DOCTEST_CHECK(COLONY::max_buildings_scrapped_per_turn == 3);
    DOCTEST_CHECK(COLCALC::housing_formula_factor == 127);
    DOCTEST_CHECK(COLCALC::cloning_center_growth_bonus == 37);
    DOCTEST_CHECK_FALSE(INFO::history_graph_smoothing);
    DOCTEST_CHECK_FALSE(COMBAT1::center_on_next_ship);
    DOCTEST_CHECK(maintain::ship_scrap_refund_percent == 50);

    writes.writes.clear();
    DOCTEST_REQUIRE(Append_Decoded_(
        "interface screen_fade_speed = 1;", &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(ERIC::fade_speed.fade_in == 1);
    DOCTEST_CHECK(ERIC::fade_speed.fade_out == 1);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "interface screen_fade_speed = 0;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "interface screen_fade_speed = 101;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "interface wormhole_link_color = 256;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "colony growth_formula_factor = 2147483648;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "colony max_buildings_scrapped_per_turn = 0;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "colony housing_formula_factor = 128;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "colony cloning_center_growth_bonus = 128;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "economy ship_scrap_refund_percent = 30;", &writes, &diagnostic));
}

DOCTEST_TEST_CASE("local presentation settings stay out of saves and synchronization") {
    const char* paths[] = {
        "interface.show_exit_message",
        "interface.allow_future_technology_browsing",
        "interface.main_menu_mod_list",
    };
    for (const char* path : paths) {
        const config::t_variable_id variable = Variable_Id_(path);
        DOCTEST_REQUIRE(variable != config::CONFIG_NO_VARIABLE);
        const config::s_config_group_descriptor* group =
            config::Get_Config_Group(config::Get_Variable_Group(variable));
        DOCTEST_REQUIRE(group != nullptr);
        DOCTEST_CHECK(std::strcmp(group->id, "interface") == 0);
        DOCTEST_CHECK_FALSE(group->save);
        DOCTEST_CHECK_FALSE(group->synchronize);
    }
}

DOCTEST_TEST_CASE("exit message defaults off and can be enabled through configuration") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK_FALSE(MOX2::show_exit_message);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_("interface show_exit_message = true;", &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(MOX2::show_exit_message);

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK_FALSE(MOX2::show_exit_message);
}

DOCTEST_TEST_CASE("saved interface settings and synchronized policies use distinct groups") {
    const config::t_variable_id format =
        Variable_Id_("interface.format.0.template");
    DOCTEST_REQUIRE(format != config::CONFIG_NO_VARIABLE);
    const config::s_config_group_descriptor* format_group =
        config::Get_Config_Group(config::Get_Variable_Group(format));
    DOCTEST_REQUIRE(format_group != nullptr);
    DOCTEST_CHECK(std::strcmp(format_group->id, "gameplay") == 0);
    DOCTEST_CHECK(format_group->save);
    DOCTEST_CHECK(format_group->synchronize);

    const config::t_variable_id info_layout =
        Variable_Id_("interface.info_menu_layout");
    DOCTEST_REQUIRE(info_layout != config::CONFIG_NO_VARIABLE);
    const config::s_config_group_descriptor* info_layout_group =
        config::Get_Config_Group(config::Get_Variable_Group(info_layout));
    DOCTEST_REQUIRE(info_layout_group != nullptr);
    DOCTEST_CHECK(std::strcmp(info_layout_group->id, "saved_interface") == 0);
    DOCTEST_CHECK(info_layout_group->save);
    DOCTEST_CHECK_FALSE(info_layout_group->synchronize);

    const config::t_variable_id blocker_warning =
        Variable_Id_("interface.warn_blockers");
    DOCTEST_REQUIRE(blocker_warning != config::CONFIG_NO_VARIABLE);
    const config::s_config_group_descriptor* blocker_group =
        config::Get_Config_Group(config::Get_Variable_Group(blocker_warning));
    DOCTEST_REQUIRE(blocker_group != nullptr);
    DOCTEST_CHECK(std::strcmp(
                      blocker_group->id, "synchronized_interface") == 0);
    DOCTEST_CHECK_FALSE(blocker_group->save);
    DOCTEST_CHECK(blocker_group->synchronize);
}

DOCTEST_TEST_CASE("ship communication ranges preserve and replace classic relay distances") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(SHIPMOVE::ship_communication_range.tachyon == 3);
    DOCTEST_CHECK(SHIPMOVE::ship_communication_range.sub_space == 6);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "movement ship_communication_range tachyon = 4;"
        "movement ship_communication_range sub_space = 7;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(SHIPMOVE::ship_communication_range.tachyon == 4);
    DOCTEST_CHECK(SHIPMOVE::ship_communication_range.sub_space == 7);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "movement ship_communication_range tachyon = 256;",
        &writes,
        &diagnostic));
}

DOCTEST_TEST_CASE("Warp Interdictor range preserves the 1.50 squared-coordinate default") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(SHIPMOVE::warp_interdictor_range == 8100);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "movement warp_interdictor_range = 14400;", &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(SHIPMOVE::warp_interdictor_range == 14400);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "movement warp_interdictor_range = 65536;", &writes, &diagnostic));
}

DOCTEST_TEST_CASE("battle confirmation timeout preserves the 1.50 default") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COMBFIND::confirm_battle_message_timeout == 32767);

    const uint16_t values[] = {0, 180, 32767, 32768, 65535};
    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    for (uint16_t value : values) {
        DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
        writes.writes.clear();
        const std::string source =
            "interface confirm_battle_message_timeout = " +
            std::to_string(value) + ";";
        DOCTEST_REQUIRE(Append_Decoded_(
            source.c_str(), &writes, &diagnostic));
        DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
        DOCTEST_REQUIRE(config::Commit_State(&state));
        DOCTEST_CHECK(COMBFIND::confirm_battle_message_timeout == value);
    }

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "interface confirm_battle_message_timeout = 65536;",
        &writes,
        &diagnostic));
}

DOCTEST_TEST_CASE("AI race variant lists replace earlier layered selections") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    const uint8_t classic[RACECUST::AI_RACE_VARIANT_LIST_SIZE] = {1, 2, 3};
    DOCTEST_CHECK(std::memcmp(RACECUST::ai_race_variants, classic, sizeof(classic)) == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "ai ai_race_variants = 1 2 3 5;", &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    writes.writes.clear();
    DOCTEST_REQUIRE(Append_Decoded_(
        "ai ai_race_variants = 4 5;", &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    const uint8_t extra[RACECUST::AI_RACE_VARIANT_LIST_SIZE] = {4, 5};
    DOCTEST_CHECK(std::memcmp(RACECUST::ai_race_variants, extra, sizeof(extra)) == 0);

    config::t_variable_id first = config::CONFIG_NO_VARIABLE;
    DOCTEST_REQUIRE(config::Find_Variable(
        "ai.ai_race_variants.1", std::strlen("ai.ai_race_variants.1"), &first));
    state.values[first].integer_value = 0;
    DOCTEST_CHECK_FALSE(config::Commit_State(&state));
    state.values[first].integer_value = 4;
    state.values[first + 1].integer_value = 4;
    DOCTEST_CHECK_FALSE(config::Commit_State(&state));
}

DOCTEST_TEST_CASE("AI stealth and race-variant settings use canonical typed state") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK_FALSE(aidesign::allow_stealth_field);
    DOCTEST_CHECK(RACECUST::variants.customization[STOCK_RACE_ALKARI][0][0].count == 0);
    DOCTEST_CHECK(RACECUST::variants.evolution[STOCK_RACE_ALKARI][0][0].count == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "ai stealth_field = true;"
        "ai race_variants customization alkari hard 1 count = 2;"
        "ai race_variants customization alkari hard 1 traits 0 trait = 28;"
        "ai race_variants customization alkari hard 1 traits 0 value = 1;"
        "ai race_variants evolution alkari hard 1 count = 1;"
        "ai race_variants evolution alkari hard 1 traits 0 trait = 7;"
        "ai race_variants evolution alkari hard 1 traits 0 value = 3;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(aidesign::allow_stealth_field);
    DOCTEST_CHECK(RACECUST::variants.customization[STOCK_RACE_ALKARI][0][0]
                      .count == 2);
    DOCTEST_CHECK(RACECUST::variants.customization[STOCK_RACE_ALKARI][0][0]
                      .items[0]
                      .trait_idx ==
        TRAIT_STEALTHY_SHIPS);
    DOCTEST_CHECK(RACECUST::variants.customization[STOCK_RACE_ALKARI][0][0]
                      .items[0]
                      .trait_val == 1);
    DOCTEST_CHECK(RACECUST::variants.evolution[STOCK_RACE_ALKARI][0][0].count == 1);
    DOCTEST_CHECK(RACECUST::variants.evolution[STOCK_RACE_ALKARI][0][0]
                      .items[0]
                      .trait_idx ==
        TRAIT_SHIP_ATTACK);
    DOCTEST_CHECK(RACECUST::variants.evolution[STOCK_RACE_ALKARI][0][0]
                      .items[0]
                      .trait_val == 3);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "ai race_variants customization alkari hard 1 count = 9;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "ai race_variants evolution alkari hard 1 traits 0 trait = 31;",
        &writes,
        &diagnostic));
}

DOCTEST_TEST_CASE("AI special-weapon design chances preserve classic defaults and bounds") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(aidesign::ship_design_bhg_chance == 3);
    DOCTEST_CHECK(aidesign::ship_design_stellar_chance == 3);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "ai ship_design_bhg_chance = 4;"
        "ai ship_design_stellar_chance = 0;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(aidesign::ship_design_bhg_chance == 4);
    DOCTEST_CHECK(aidesign::ship_design_stellar_chance == 0);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "ai ship_design_bhg_chance = 5;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "ai ship_design_stellar_chance = -1;", &writes, &diagnostic));
}

DOCTEST_TEST_CASE("population transfer and future technology browsing preserve 1.50 defaults") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK_FALSE(SETTLER::allow_last_population_transfer);
    DOCTEST_CHECK(TECH::allow_future_technology_browsing);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "colony allow_last_population_transfer = true;"
        "interface allow_future_technology_browsing = false;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(SETTLER::allow_last_population_transfer);
    DOCTEST_CHECK_FALSE(TECH::allow_future_technology_browsing);
}

DOCTEST_TEST_CASE("AI ship design special priorities preserve themed order and bindings") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(aidesign::ship_design.special_priority.bio_weapon[0].id ==
        SPECIAL_PHASE_SHIFTER);
    DOCTEST_CHECK(aidesign::ship_design.special_priority.bio_weapon[3].type ==
        AI_SPECIAL_ITEM_WEAPON);
    DOCTEST_CHECK(aidesign::ship_design.special_priority.capture[0].id ==
        SPECIAL_TRANSPORTERS);
    DOCTEST_CHECK(aidesign::ship_design.special_priority.cloaking[2].id ==
        SPECIAL_STEALTH_FIELD);
    DOCTEST_CHECK(aidesign::ship_design.special_priority.beam_defense[3].id ==
        SPECIAL_INERTIAL_STABILIZER);
    DOCTEST_CHECK(aidesign::ship_design.special_priority.missile_defense[2].id ==
        WEAPON_SPATIAL_COMPRESSOR);
    DOCTEST_CHECK(aidesign::ship_design.special_priority.armor[2].id ==
        SPECIAL_REINFORCED_HULL);
    DOCTEST_CHECK(aidesign::ship_design.special_priority.shield[1].id ==
        SPECIAL_MULTI_PHASED_SHIELDS);
    DOCTEST_CHECK(aidesign::ship_design.special_priority.beam_offense[6].id ==
        SPECIAL_BATTLE_SCANNER);
    DOCTEST_CHECK(aidesign::ship_design.special_priority.missile_offense[1].id ==
        SPECIAL_FAST_MISSILE_RACKS);
    DOCTEST_CHECK(aidesign::ship_design.special_priority.special_weapon[0].id ==
        WEAPON_STELLAR_CONVERTER);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "ai ship_design c1_bio_weapon 1 item = 25;"
        "ai ship_design c1_bio_weapon 1 kind = weapon;"
        "ai ship_design c1_capture 2 item = 8;"
        "ai ship_design c1_cloaking 3 kind = weapon;"
        "ai ship_design c1_beam_defense 4 item = 9;"
        "ai ship_design c1_missile_defense 8 item = 33;"
        "ai ship_design c1_armor 1 item = 35;"
        "ai ship_design c1_shield 2 item = 2;"
        "ai ship_design c2_beam_offense 7 item = 5;"
        "ai ship_design c3_missile_offense 2 item = 12;"
        "ai ship_design c5_special_weapon 3 kind = weapon;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(aidesign::ship_design.special_priority.bio_weapon[0].id ==
        WEAPON_DEATH_SPORE);
    DOCTEST_CHECK(aidesign::ship_design.special_priority.bio_weapon[0].type ==
        AI_SPECIAL_ITEM_WEAPON);
    DOCTEST_CHECK(aidesign::ship_design.special_priority.capture[1].id ==
        SPECIAL_DISPLACEMENT_DEVICE);
    DOCTEST_CHECK(aidesign::ship_design.special_priority.cloaking[2].type ==
        AI_SPECIAL_ITEM_WEAPON);
    DOCTEST_CHECK(aidesign::ship_design.special_priority.beam_defense[3].id ==
        SPECIAL_ECM_JAMMER);
    DOCTEST_CHECK(aidesign::ship_design.special_priority.missile_defense[7].id ==
        WEAPON_ANTI_MISSILE_ROCKET);
    DOCTEST_CHECK(aidesign::ship_design.special_priority.armor[0].id ==
        WEAPON_PLASMA_WEB);
    DOCTEST_CHECK(aidesign::ship_design.special_priority.shield[1].id ==
        SPECIAL_AUGMENTED_ENGINES);
    DOCTEST_CHECK(aidesign::ship_design.special_priority.beam_offense[6].id ==
        SPECIAL_BATTLE_SCANNER);
    DOCTEST_CHECK(aidesign::ship_design.special_priority.missile_offense[1].id ==
        SPECIAL_FAST_MISSILE_RACKS);
    DOCTEST_CHECK(aidesign::ship_design.special_priority.special_weapon[2].type ==
        AI_SPECIAL_ITEM_WEAPON);

    const char* paths[] = {
        "ai.ship_design.c1_bio_weapon.1.item",
        "ai.ship_design.c1_capture.2.kind",
        "ai.ship_design.c1_cloaking.3.kind",
        "ai.ship_design.c1_beam_defense.4.item",
        "ai.ship_design.c1_missile_defense.8.item",
        "ai.ship_design.c1_armor.1.item",
        "ai.ship_design.c1_shield.2.item",
        "ai.ship_design.c2_beam_offense.7.item",
        "ai.ship_design.c3_missile_offense.2.item",
        "ai.ship_design.c5_special_weapon.3.kind",
        "ai.ship_design.c4_defense_human_non_beams.19.item",
        "ai.ship_design.c4_defense_human_beams.19.kind",
        "ai.ship_design.c6_special_device.12.kind",
    };
    for (const char* expected : paths) {
        config::t_variable_id id = config::CONFIG_NO_VARIABLE;
        DOCTEST_REQUIRE(config::Find_Variable(expected, std::strlen(expected), &id));
        std::string resolved;
        DOCTEST_REQUIRE(config::Get_Variable_Name(id, &resolved));
        DOCTEST_CHECK(resolved == expected);
    }

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "ai ship_design c1_capture 1 item = 256;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "ai ship_design c1_capture 1 kind = unknown;",
        &writes,
        &diagnostic));

    writes.writes.clear();
    DOCTEST_REQUIRE(Append_Decoded_(
        "ai ship_design c1_capture 1 item = 45;"
        "ai ship_design c1_capture 1 kind = special;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_CHECK_FALSE(config::Commit_State(&state));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    writes.writes.clear();
    DOCTEST_REQUIRE(Append_Decoded_(
        "ai ship_design c1_armor 1 item = 45;"
        "ai ship_design c1_armor 1 kind = special;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_CHECK_FALSE(config::Commit_State(&state));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
}

DOCTEST_TEST_CASE("AI technology priorities preserve 1.50 defaults and semantic bindings") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(newtech::tech_priority.double_field_1 ==
        TECH_FIELD_ADVANCED_ENGINEERING);
    DOCTEST_CHECK(newtech::tech_priority.double_field_2 == TECH_FIELD_XENO_RELATIONS);
    DOCTEST_CHECK(newtech::tech_priority.double_application == TECH_APP_NEURAL_SCANNER);
    DOCTEST_CHECK(newtech::tech_priority.quintuple_application ==
        TECH_APP_DEUTERIUM_FUEL_CELLS);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "ai tech_priority double_field_1 = chemistry;"
        "ai tech_priority double_field_2 = xeno_relations;"
        "ai tech_priority double_application = battle_scanner;"
        "ai tech_priority quintuple_application = thorium_fuel_cells;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(newtech::tech_priority.double_field_1 == TECH_FIELD_CHEMISTRY);
    DOCTEST_CHECK(newtech::tech_priority.double_field_2 == TECH_FIELD_XENO_RELATIONS);
    DOCTEST_CHECK(newtech::tech_priority.double_application == TECH_APP_BATTLE_SCANNER);
    DOCTEST_CHECK(newtech::tech_priority.quintuple_application ==
        TECH_APP_THORIUM_FUEL_CELLS);

    writes.writes.clear();
    DOCTEST_REQUIRE(Append_Decoded_(
        "ai tech_priority double_field_1 = starting_tech;", &writes, &diagnostic));
    DOCTEST_CHECK_FALSE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    writes.writes.clear();
    DOCTEST_REQUIRE(Append_Decoded_(
        "ai tech_priority double_application = no_tech;", &writes, &diagnostic));
    DOCTEST_CHECK_FALSE(config::Apply_Write_Set(&writes, &state, &diagnostic));
}

DOCTEST_TEST_CASE("initial building priority preserves the 1.50 order and terminator") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(ERICNET::buildlist[0] == BUILDING_STAR_FORTRESS);
    DOCTEST_CHECK(ERICNET::buildlist[41] == BUILDING_SPACE_ACADEMY);
    DOCTEST_CHECK(ERICNET::buildlist[42] == BUILDING_NO_BUILDING);
    DOCTEST_CHECK(ERICNET::buildlist[43] == BUILDING_NO_BUILDING);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "game initial_buildings = automated_factory;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(ERICNET::buildlist[0] == BUILDING_AUTOMATED_FACTORY);
    DOCTEST_CHECK(ERICNET::buildlist[42] == BUILDING_NO_BUILDING);
    DOCTEST_CHECK(ERICNET::buildlist[43] == BUILDING_NO_BUILDING);

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
}

DOCTEST_TEST_CASE("AI surrender delay preserves the 1.50 default") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(npcdiplo::ai_surrender_delay == 150);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "ai surrender_delay = 200;", &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(npcdiplo::ai_surrender_delay == 200);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "ai surrender_delay = 65536;", &writes, &diagnostic));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
}

DOCTEST_TEST_CASE("AI special-device and defense priorities preserve native defaults") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(aidesign::ship_design.special_device_priority[0].id ==
        SPECIAL_QUANTUM_DETONATOR);
    DOCTEST_CHECK(aidesign::ship_design.special_device_priority[1].type ==
        AI_SPECIAL_ITEM_WEAPON);
    DOCTEST_CHECK(aidesign::ship_design.defense_priority.human_non_beams[0].id ==
        SPECIAL_SECURITY_STATIONS);
    DOCTEST_CHECK(aidesign::ship_design.defense_priority.human_beams[18].id ==
        SPECIAL_AUTOMATED_REPAIR_UNIT);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "ai ship_design c6_special_device 1 item = 35;"
        "ai ship_design c6_special_device 1 kind = special;"
        "ai ship_design c4_defense_human_non_beams 1 item = 33;"
        "ai ship_design c4_defense_human_non_beams 1 kind = weapon;"
        "ai ship_design c4_defense_human_beams 19 item = 29;"
        "ai ship_design c4_defense_human_beams 19 kind = weapon;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(aidesign::ship_design.special_device_priority[0].id ==
        SPECIAL_TRANSPORTERS);
    DOCTEST_CHECK(aidesign::ship_design.special_device_priority[0].type ==
        AI_SPECIAL_ITEM_SPECIAL);
    DOCTEST_CHECK(aidesign::ship_design.defense_priority.human_non_beams[0].id ==
        WEAPON_ANTI_MISSILE_ROCKET);
    DOCTEST_CHECK(aidesign::ship_design.defense_priority.human_non_beams[0].type ==
        AI_SPECIAL_ITEM_WEAPON);
    DOCTEST_CHECK(aidesign::ship_design.defense_priority.human_beams[18].id ==
        WEAPON_HEAVY_FIGHTER);
    DOCTEST_CHECK(aidesign::ship_design.defense_priority.human_beams[18].type ==
        AI_SPECIAL_ITEM_WEAPON);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "ai ship_design c6_special_device 13 item = 1;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "ai ship_design c4_defense_human_beams 20 item = 1;",
        &writes,
        &diagnostic));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
}

DOCTEST_TEST_CASE("AI ship design allocations bind every active profile and hull") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(aidesign::ship_design_allocations.profiles
                      [aidesign::SHIP_DESIGN_ALLOCATION_STANDARD_BEAM_MISSILE_X2]
                      [SHIP_SIZE_SMALL]
                          .beams == 85);
    DOCTEST_CHECK(aidesign::ship_design_allocations.profiles
                      [aidesign::SHIP_DESIGN_ALLOCATION_SPECIAL_WEAPON]
                      [SHIP_SIZE_DOOM_STAR]
                          .special_weapon == 60);
    DOCTEST_CHECK(aidesign::ship_design_allocations.profiles
                      [aidesign::SHIP_DESIGN_ALLOCATION_SPECIAL_WEAPON_2]
                      [SHIP_SIZE_DOOM_STAR]
                          .special_weapon == 30);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "ai ship_design_allocations standard_beam_missile_x2 frigate beams = 0;"
        "ai ship_design_allocations hybrid_beam_carrier destroyer fighter = 100;"
        "ai ship_design_allocations special_weapon doomstar bombs = -1;"
        "ai ship_design_allocations special_weapon2 doomstar special_weapon = 55;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(aidesign::ship_design_allocations.profiles
                      [aidesign::SHIP_DESIGN_ALLOCATION_STANDARD_BEAM_MISSILE_X2]
                      [SHIP_SIZE_SMALL]
                          .beams == 0);
    DOCTEST_CHECK(aidesign::ship_design_allocations.profiles
                      [aidesign::SHIP_DESIGN_ALLOCATION_HYBRID_BEAM_CARRIER]
                      [SHIP_SIZE_MEDIUM]
                          .fighters == 100);
    DOCTEST_CHECK(aidesign::ship_design_allocations.profiles
                      [aidesign::SHIP_DESIGN_ALLOCATION_SPECIAL_WEAPON]
                      [SHIP_SIZE_DOOM_STAR]
                          .bombs == -1);
    DOCTEST_CHECK(aidesign::ship_design_allocations.profiles
                      [aidesign::SHIP_DESIGN_ALLOCATION_SPECIAL_WEAPON_2]
                      [SHIP_SIZE_DOOM_STAR]
                          .special_weapon == 55);

    const char* fields[] = {
        "theme_special",
        "beam_special",
        "missile_special",
        "defense_special",
        "special_weapon",
        "special_device",
        "fighter",
        "missile",
        "heavy_beams",
        "beams",
        "point_defense_beams",
        "bombs",
    };
    for (int profile = 0;
         profile < aidesign::SHIP_DESIGN_ALLOCATION_PROFILE_COUNT;
         ++profile) {
        for (int hull = 0; hull < SHIP_SIZE_COUNT; ++hull) {
            for (const char* field : fields) {
                const std::string path = "ai.ship_design_allocations." +
                    std::to_string(profile) + "." + std::to_string(hull) +
                    "." + field;
                DOCTEST_CHECK(Variable_Id_(path.c_str()) !=
                    config::CONFIG_NO_VARIABLE);
            }
        }
    }

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "ai ship_design_allocations standard_carrier titan beams = -2;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "ai ship_design_allocations standard_carrier titan beams = 101;",
        &writes,
        &diagnostic));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
}

DOCTEST_TEST_CASE("alternate diplomacy preserves the classic default") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(diplomac::alternate_diplomacy == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "ai alternate_diplomacy = true;", &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(diplomac::alternate_diplomacy == 1);
}

DOCTEST_TEST_CASE("starting relations preserve the complete 1.50 matrix") {
    const int16_t expected[diplomac::STARTING_RELATIONS_RACE_COUNT][diplomac::STARTING_RELATIONS_RACE_COUNT] = {
        {0, 0, -24, 24, 0, 0, -12, 0, -24, 0, 12, 0, 24, 0},
        {0, 0, -12, 24, 0, 0, 0, -12, 0, 0, 0, 0, 0, 0},
        {-24, -12, 0, 0, 12, 0, 0, 12, 0, 0, 0, 12, 0, 0},
        {24, 24, 0, 0, -24, 0, 0, 0, 24, -12, 0, -12, 0, 0},
        {0, 0, 12, -24, 0, 0, 0, 12, -12, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {-12, 0, 0, 0, 0, 0, 0, 0, -12, 0, -12, 12, 24, 0},
        {0, -12, 12, 0, 12, 0, 0, 0, 12, 0, 0, -12, -12, 0},
        {-24, 0, 0, 24, -12, 0, -12, 12, 0, 0, 0, 0, 12, 0},
        {0, 0, 0, -12, 0, 0, 0, 0, 0, 0, 0, 0, 12, 0},
        {12, 0, 0, 0, 0, 0, -12, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 12, -12, 0, 0, 12, -12, 0, 0, 0, 0, 0, 0},
        {24, 0, 0, 0, 0, 0, 24, -12, 12, 12, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    };

    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(std::memcmp(
                      diplomac::starting_relations,
                      expected,
                      sizeof(expected)) == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "diplomacy starting_relations human trilarian = -37;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(diplomac::starting_relations[STOCK_RACE_HUMAN]
                                               [STOCK_RACE_TRILARIAN] == -37);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "diplomacy starting_relations unknown trilarian = 0;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "diplomacy starting_relations human trilarian = 32768;",
        &writes,
        &diagnostic));
}

DOCTEST_TEST_CASE("AI Colony Base prewarp setting preserves the classic default") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK_FALSE(aidudes::builds_colony_bases_prewarp);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "ai colony_bases_prewarp = true;", &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(aidudes::builds_colony_bases_prewarp);
}

DOCTEST_TEST_CASE("noreport preserves classic diplomacy access by default") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(DIP_SCRN::noreport == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "diplomacy noreport = true;", &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(DIP_SCRN::noreport == 1);
}

DOCTEST_TEST_CASE("ruleset availability settings preserve classic defaults") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COMBAT1::disable_scan == 0);
    DOCTEST_CHECK(COLCALC::disable_housing == 0);
    DOCTEST_CHECK(COLCALC::androids_available == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat disable_scan = true;"
        "colony disable_housing = true;"
        "colony androids_available = true;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COMBAT1::disable_scan == 1);
    DOCTEST_CHECK(COLCALC::disable_housing == 1);
    DOCTEST_CHECK(COLCALC::androids_available == 1);
}

DOCTEST_TEST_CASE("race pick rules bind through named nested choices") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(raceopt::pick_rules.starting == 10);
    DOCTEST_CHECK(raceopt::pick_rules.negative_limit == -10);
    DOCTEST_CHECK(raceopt::pick_rules.mutation_bonus == 4);
    DOCTEST_CHECK(raceopt::pick_rules.primary[raceopt::PRIMARY_PICK_GROWTH]
                                        [raceopt::PRIMARY_CHOICE_PENALTY]
                                            .cost == -4);
    DOCTEST_CHECK(raceopt::pick_rules.primary[raceopt::PRIMARY_PICK_GROWTH]
                                        [raceopt::PRIMARY_CHOICE_MAJOR_BONUS]
                                            .value == 100);
    DOCTEST_CHECK(raceopt::pick_rules.government[raceopt::GOVERNMENT_PICK_DEMOCRACY] == 7);
    DOCTEST_CHECK(raceopt::pick_rules.special[raceopt::SPECIAL_PICK_SUBTERRANEAN] == 6);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "race picks starting = 12;"
        "race picks negative_limit = -12;"
        "race picks mutation_bonus = 6;"
        "race picks primary growth penalty cost = -5;"
        "race picks primary growth major_bonus value = 110;"
        "race picks government democracy = 8;"
        "race picks special subterranean = 7;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(raceopt::pick_rules.starting == 12);
    DOCTEST_CHECK(raceopt::pick_rules.negative_limit == -12);
    DOCTEST_CHECK(raceopt::pick_rules.mutation_bonus == 6);
    DOCTEST_CHECK(raceopt::pick_rules.primary[raceopt::PRIMARY_PICK_GROWTH]
                                        [raceopt::PRIMARY_CHOICE_PENALTY]
                                            .cost == -5);
    DOCTEST_CHECK(raceopt::pick_rules.primary[raceopt::PRIMARY_PICK_GROWTH]
                                        [raceopt::PRIMARY_CHOICE_MAJOR_BONUS]
                                            .value == 110);
    DOCTEST_CHECK(raceopt::pick_rules.government[raceopt::GOVERNMENT_PICK_DEMOCRACY] == 8);
    DOCTEST_CHECK(raceopt::pick_rules.special[raceopt::SPECIAL_PICK_SUBTERRANEAN] == 7);

    const char* paths[] = {
        "race.picks.primary.0.0.cost",
        "race.picks.primary.6.2.value",
        "race.picks.government.3",
        "race.picks.special.20",
    };
    for (const char* expected : paths) {
        config::t_variable_id id = config::CONFIG_NO_VARIABLE;
        DOCTEST_REQUIRE(config::Find_Variable(expected, std::strlen(expected), &id));
        std::string resolved;
        DOCTEST_REQUIRE(config::Get_Variable_Name(id, &resolved));
        DOCTEST_CHECK(resolved == expected);
    }

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "race picks primary economy penalty cost = -5;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "race picks special super_trait = 7;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "race picks primary growth penalty cost = -129;",
        &writes,
        &diagnostic));
}

DOCTEST_TEST_CASE("configured primary trait values drive all NPC theme weights") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    struct s_trait_case {
        TRAIT trait;
        raceopt::e_primary_pick pick;
        raceopt::e_primary_choice choice;
    };
    const s_trait_case cases[] = {
        {TRAIT_POP_GROWTH_BONUS, raceopt::PRIMARY_PICK_GROWTH, raceopt::PRIMARY_CHOICE_BONUS},
        {TRAIT_POP_GROWTH_BONUS, raceopt::PRIMARY_PICK_GROWTH, raceopt::PRIMARY_CHOICE_MAJOR_BONUS},
        {TRAIT_FOOD_BONUS, raceopt::PRIMARY_PICK_FOOD, raceopt::PRIMARY_CHOICE_BONUS},
        {TRAIT_FOOD_BONUS, raceopt::PRIMARY_PICK_FOOD, raceopt::PRIMARY_CHOICE_MAJOR_BONUS},
        {TRAIT_PRODUCTION_BONUS, raceopt::PRIMARY_PICK_PRODUCTION, raceopt::PRIMARY_CHOICE_BONUS},
        {TRAIT_PRODUCTION_BONUS, raceopt::PRIMARY_PICK_PRODUCTION, raceopt::PRIMARY_CHOICE_MAJOR_BONUS},
        {TRAIT_RESEARCH_BONUS, raceopt::PRIMARY_PICK_RESEARCH, raceopt::PRIMARY_CHOICE_BONUS},
        {TRAIT_RESEARCH_BONUS, raceopt::PRIMARY_PICK_RESEARCH, raceopt::PRIMARY_CHOICE_MAJOR_BONUS},
        {TRAIT_SHIP_DEFENSE, raceopt::PRIMARY_PICK_SHIP_DEFENSE, raceopt::PRIMARY_CHOICE_BONUS},
        {TRAIT_SHIP_DEFENSE, raceopt::PRIMARY_PICK_SHIP_DEFENSE, raceopt::PRIMARY_CHOICE_MAJOR_BONUS},
        {TRAIT_SHIP_ATTACK, raceopt::PRIMARY_PICK_SHIP_ATTACK, raceopt::PRIMARY_CHOICE_BONUS},
        {TRAIT_SHIP_ATTACK, raceopt::PRIMARY_PICK_SHIP_ATTACK, raceopt::PRIMARY_CHOICE_MAJOR_BONUS},
        {TRAIT_GROUND_COMBAT_BONUS, raceopt::PRIMARY_PICK_GROUND_COMBAT, raceopt::PRIMARY_CHOICE_BONUS},
        {TRAIT_GROUND_COMBAT_BONUS, raceopt::PRIMARY_PICK_GROUND_COMBAT, raceopt::PRIMARY_CHOICE_MAJOR_BONUS},
        {TRAIT_SPYING_BONUS, raceopt::PRIMARY_PICK_SPYING, raceopt::PRIMARY_CHOICE_BONUS},
        {TRAIT_SPYING_BONUS, raceopt::PRIMARY_PICK_SPYING, raceopt::PRIMARY_CHOICE_MAJOR_BONUS},
    };

    for (const s_trait_case& test_case : cases) {
        int8_t& configured_value =
            raceopt::pick_rules.primary[test_case.pick][test_case.choice].value;
        const int8_t original_value = configured_value;
        const int8_t replacement_value = 127;
        uint32_t differentiating_seed = 0;
        s_npc_theme_result original_result = {};

        for (uint32_t seed = 1; seed <= 10000; ++seed) {
            original_result = NPC_Theme_Result_(
                test_case.trait, original_value, seed);
            const s_npc_theme_result unmatched_result = NPC_Theme_Result_(
                test_case.trait, replacement_value, seed);
            if (!NPC_Theme_Results_Equal_(original_result, unmatched_result)) {
                differentiating_seed = seed;
                break;
            }
        }

        DOCTEST_REQUIRE(differentiating_seed != 0);
        configured_value = replacement_value;
        const s_npc_theme_result configured_result = NPC_Theme_Result_(
            test_case.trait, replacement_value, differentiating_seed);
        configured_value = original_value;
        DOCTEST_CHECK(NPC_Theme_Results_Equal_(
            configured_result, original_result));
    }

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
}

DOCTEST_TEST_CASE("stock race traits preserve classic defaults and bind by race and trait") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(RACESEL::stock_race[STOCK_RACE_ALKARI].defense == 3);
    DOCTEST_CHECK(RACESEL::stock_race[STOCK_RACE_HUMAN].government ==
        GOVERNMENT_DEMOCRACY);
    DOCTEST_CHECK(RACESEL::stock_race[STOCK_RACE_KLACKON].government ==
        GOVERNMENT_UNIFICATION);
    DOCTEST_CHECK(RACESEL::stock_race[STOCK_RACE_SILICOID].growth == 1);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "ai stock_race human government = unification;"
        "ai stock_race alkari growth = 2;"
        "ai stock_race trilarian warlord = true;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(RACESEL::stock_race[STOCK_RACE_HUMAN].government ==
        GOVERNMENT_UNIFICATION);
    DOCTEST_CHECK(RACESEL::stock_race[STOCK_RACE_ALKARI].growth == 2);
    DOCTEST_CHECK(RACESEL::stock_race[STOCK_RACE_TRILARIAN].warlord == 1);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "ai stock_race alkari growth = 4;", &writes, &diagnostic));
}

DOCTEST_TEST_CASE("AI race personalities preserve native stock-race rows") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(KEN::personalities[STOCK_RACE_ALKARI][0] ==
        PLAYER_PERSONALITY_ERRATIC);
    DOCTEST_CHECK(KEN::personalities[STOCK_RACE_HUMAN][0] ==
        PLAYER_PERSONALITY_ERRATIC);
    DOCTEST_CHECK(KEN::personalities[STOCK_RACE_TRILARIAN][9] ==
        PLAYER_PERSONALITY_PACIFIST);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "ai race_personality human roll_1 = xenophobic;"
        "ai race_personality trilarian roll_10 = honorable;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(KEN::personalities[STOCK_RACE_HUMAN][0] ==
        PLAYER_PERSONALITY_XENOPHOBIC);
    DOCTEST_CHECK(KEN::personalities[STOCK_RACE_TRILARIAN][9] ==
        PLAYER_PERSONALITY_HONORABLE);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "ai race_personality human roll_1 = unknown;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "ai race_personality human roll_11 = xenophobic;", &writes, &diagnostic));
}

DOCTEST_TEST_CASE("Antaran fleet policy preserves classic defaults and binds semantic paths") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(antarans::fleet_config.growth_delay[antarans::ANTARAN_CIVILIZATION_PREWARP] == 200);
    DOCTEST_CHECK(antarans::fleet_config.resource_growth.frequency == 25);
    DOCTEST_CHECK(antarans::fleet_config.resource_growth.offensive_multiplier[antarans::ANTARAN_DIFFICULTY_IMPOSSIBLE] == 200);
    DOCTEST_CHECK(
        antarans::fleet_config.attack.minimum_offensive_force_shift == 2);
    DOCTEST_CHECK(
        antarans::fleet_config.attack.lucky_victim_weight_divisor == 3);
    DOCTEST_CHECK(antarans::fleet_config.ship_limit.offensive[antarans::ANTARAN_SHIP_HARBINGER] == 2);
    DOCTEST_CHECK(antarans::fleet_config.ship_cost[antarans::ANTARAN_SHIP_INTRUDER] == 12);
    DOCTEST_CHECK(antarans::fleet_config.retirement[antarans::ANTARAN_RETIREMENT_RAIDER].divisor[antarans::ANTARAN_DIFFICULTY_HARD] == 150);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "ai antaran_fleet growth_delay prewarp = 65535;"
        "ai antaran_fleet resource_growth frequency = 20;"
        "ai antaran_fleet resource_growth offensive_multiplier impossible = 300;"
        "ai antaran_fleet resource_growth defensive_multiplier hard = 175;"
        "ai antaran_fleet attack roll_range = 125;"
        "ai antaran_fleet attack minimum_offensive_force_shift = 3;"
        "ai antaran_fleet attack impossible_human_weight_multiplier = 4;"
        "ai antaran_fleet attack lucky_victim_weight_divisor = 5;"
        "ai antaran_fleet ship_limit offensive harbinger = 6;"
        "ai antaran_fleet ship_limit defensive intruder = 4;"
        "ai antaran_fleet ship_cost intruder = 25;"
        "ai antaran_fleet retirement raider normal_turn_limit = 80;"
        "ai antaran_fleet retirement raider divisor hard = 250;"
        "ai antaran_fleet retirement marauder divisor impossible = 307;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(antarans::fleet_config.growth_delay[antarans::ANTARAN_CIVILIZATION_PREWARP] == 65535);
    DOCTEST_CHECK(antarans::fleet_config.resource_growth.frequency == 20);
    DOCTEST_CHECK(antarans::fleet_config.resource_growth.offensive_multiplier[antarans::ANTARAN_DIFFICULTY_IMPOSSIBLE] == 300);
    DOCTEST_CHECK(antarans::fleet_config.resource_growth.defensive_multiplier[antarans::ANTARAN_DIFFICULTY_HARD] == 175);
    DOCTEST_CHECK(antarans::fleet_config.attack.roll_range == 125);
    DOCTEST_CHECK(
        antarans::fleet_config.attack.minimum_offensive_force_shift == 3);
    DOCTEST_CHECK(antarans::fleet_config.attack.impossible_human_weight_multiplier == 4);
    DOCTEST_CHECK(
        antarans::fleet_config.attack.lucky_victim_weight_divisor == 5);
    DOCTEST_CHECK(antarans::fleet_config.ship_limit.offensive[antarans::ANTARAN_SHIP_HARBINGER] == 6);
    DOCTEST_CHECK(antarans::fleet_config.ship_limit.defensive[antarans::ANTARAN_SHIP_INTRUDER] == 4);
    DOCTEST_CHECK(antarans::fleet_config.ship_cost[antarans::ANTARAN_SHIP_INTRUDER] == 25);
    DOCTEST_CHECK(antarans::fleet_config.retirement[antarans::ANTARAN_RETIREMENT_RAIDER].normal_turn_limit == 80);
    DOCTEST_CHECK(antarans::fleet_config.retirement[antarans::ANTARAN_RETIREMENT_RAIDER].divisor[antarans::ANTARAN_DIFFICULTY_HARD] == 250);
    DOCTEST_CHECK(antarans::fleet_config.retirement[antarans::ANTARAN_RETIREMENT_MARAUDER].divisor[antarans::ANTARAN_DIFFICULTY_IMPOSSIBLE] == 307);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "ai antaran_fleet resource_growth frequency = 0;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "ai antaran_fleet growth_delay prewarp = 65536;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "ai antaran_fleet attack roll_range = 0;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "ai antaran_fleet attack minimum_offensive_force_shift = 32;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "ai antaran_fleet attack impossible_human_weight_multiplier = 128;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "ai antaran_fleet attack lucky_victim_weight_divisor = 0;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "ai antaran_fleet attack lucky_victim_weight_divisor = 101;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "ai antaran_fleet retirement marauder divisor hard = 0;",
        &writes,
        &diagnostic));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
}

DOCTEST_TEST_CASE("leader progression settings preserve 1.50 defaults and bounds") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(OFFICER::progression.experience_thresholds[0] == 60);
    DOCTEST_CHECK(OFFICER::progression.experience_thresholds[4] == 1000);
    DOCTEST_CHECK(OFFICER::progression.availability.base_amount == 10);
    DOCTEST_CHECK(OFFICER::progression.availability.increment_step == 5);
    DOCTEST_CHECK(OFFICER::progression.hire_chance.initial_delay == 5);
    DOCTEST_CHECK(OFFICER::reject_declined_offers == 0);
    DOCTEST_CHECK(
        OFFICER::progression.special_leaders.advanced_government_leader_id == 47);
    DOCTEST_CHECK(
        OFFICER::progression.special_leaders.random_skills_colony_leader_id == 41);
    DOCTEST_CHECK(
        OFFICER::progression.special_leaders.random_skills_ship_officer_id == 52);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "leaders progression experience_thresholds 2 = 75;"
        "leaders progression availability base_amount = 12;"
        "leaders progression availability increment_step = 4;"
        "leaders progression availability charismatic_bonus = 11;"
        "leaders progression hire_chance initial_delay = 7;"
        "leaders progression hire_chance charismatic_bonus = 6;"
        "leaders progression hire_chance repulsive_penalty = 9;"
        "leaders reject_declined_offers = true;"
        "leaders progression special_leaders advanced_government_leader_id = 67;"
        "leaders progression special_leaders random_skills_colony_leader_id = 128;"
        "leaders progression special_leaders random_skills_ship_officer_id = 255;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(OFFICER::progression.experience_thresholds[0] == 75);
    DOCTEST_CHECK(OFFICER::progression.availability.base_amount == 12);
    DOCTEST_CHECK(OFFICER::progression.availability.increment_step == 4);
    DOCTEST_CHECK(OFFICER::progression.availability.charismatic_bonus == 11);
    DOCTEST_CHECK(OFFICER::progression.hire_chance.initial_delay == 7);
    DOCTEST_CHECK(OFFICER::progression.hire_chance.charismatic_bonus == 6);
    DOCTEST_CHECK(OFFICER::progression.hire_chance.repulsive_penalty == 9);
    DOCTEST_CHECK(OFFICER::reject_declined_offers == 1);
    DOCTEST_CHECK(
        OFFICER::progression.special_leaders.advanced_government_leader_id == 67);
    DOCTEST_CHECK(
        OFFICER::progression.special_leaders.random_skills_colony_leader_id == 128);
    DOCTEST_CHECK(
        OFFICER::progression.special_leaders.random_skills_ship_officer_id == 255);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "leaders progression availability increment_step = 0;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "leaders progression special_leaders advanced_government_leader_id = 256;",
        &writes,
        &diagnostic));
}

DOCTEST_TEST_CASE("Council, Antaran crew, and Psionics policies preserve classic defaults") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(council::meeting_schedule.first_meeting_delay == 25);
    DOCTEST_CHECK(council::meeting_schedule.meeting_interval == 25);
    DOCTEST_CHECK(antarans::offensive_crew ==
        antarans::e_offensive_crew::veteran);
    DOCTEST_CHECK(COLCALC::psionics_government_limit ==
        COLCALC::e_psionics_government_limit::imperium);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "gameplay council first_meeting_delay = 127;"
        "gameplay council meeting_interval = 10000;"
        "ai antaran_offensive_crew = elite;"
        "colony psionics_government_limit = federation;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(council::meeting_schedule.first_meeting_delay == 127);
    DOCTEST_CHECK(council::meeting_schedule.meeting_interval == 10000);
    DOCTEST_CHECK(antarans::offensive_crew == antarans::e_offensive_crew::elite);
    DOCTEST_CHECK(COLCALC::psionics_government_limit ==
        COLCALC::e_psionics_government_limit::federation);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "gameplay council first_meeting_delay = 128;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "gameplay council meeting_interval = 0;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "ai antaran_offensive_crew = ultra_elite;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "colony psionics_government_limit = democracy;", &writes, &diagnostic));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
}

DOCTEST_TEST_CASE("Antaran Star Fortress space preserves and replaces the 1.50 default") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COMBINIT::antaran_star_fortress_space == 1500);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "ai antaran_star_fortress_space = 1800;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COMBINIT::antaran_star_fortress_space == 1800);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "ai antaran_star_fortress_space = 65536;",
        &writes,
        &diagnostic));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
}

DOCTEST_TEST_CASE("fighter point-defense setting preserves the classic default") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTFIRE::fighters_use_point_defense == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat fighters_use_point_defense = true;", &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTFIRE::fighters_use_point_defense == 1);
}

DOCTEST_TEST_CASE("fighter beam damage clipping preserves the classic default") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTFIRE::fighter_beam_no_plus_one == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat fighter_beam_no_plus_one = true;", &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTFIRE::fighter_beam_no_plus_one == 1);
}

DOCTEST_TEST_CASE("simplified beam formula preserves the classic default") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTFIRE::simplified_beam_formula == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat simplified_beam_formula = true;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTFIRE::simplified_beam_formula == 1);
}

DOCTEST_TEST_CASE("planet non-enveloping setting preserves the classic default") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTFIRE::planet_non_enveloping == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat planet_non_enveloping = true;", &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTFIRE::planet_non_enveloping == 1);
}

DOCTEST_TEST_CASE("Energy Absorber charge retention preserves the classic default") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COMBAT1::absorber_keep_charge == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat absorber_keep_charge = true;", &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COMBAT1::absorber_keep_charge == 1);
}

DOCTEST_TEST_CASE("Spatial Compressor size scaling preserves the classic default") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTSPEC::compressor_per_size_class_damage == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat compressor_per_size_class_damage = true;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTSPEC::compressor_per_size_class_damage == 1);

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
}

DOCTEST_TEST_CASE("spherical and weapon ranges preserve and replace 1.50 defaults") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(CMBTFIRE::defensive_range_bonus == 0);
    DOCTEST_CHECK(CMBTFIRE::weapon_range.proton_torpedo == 8);
    DOCTEST_CHECK(CMBTFIRE::weapon_range.tractor_beam == 4);
    DOCTEST_CHECK(CMBTFIRE::weapon_range.black_hole_generator == 5);
    DOCTEST_CHECK(CMBTFIRE::spherical_range_to_ship.pulsar == 6);
    DOCTEST_CHECK(CMBTFIRE::spherical_range_to_ship.spatial_compressor == 4);
    DOCTEST_CHECK(CMBTFIRE::spherical_range_to_missile.pulsar == 122);
    DOCTEST_CHECK(
        CMBTFIRE::spherical_range_to_missile.drive_explosion == 135);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat defensive_range_bonus = 2;"
        "combat weapon_range tractor_beam = 7;"
        "combat weapon_range proton_torpedo = 10;"
        "combat spherical_range_to_ship pulsar = 8;"
        "combat spherical_range_to_ship drive_explosion = 9;"
        "combat spherical_range_to_missile spatial_compressor = 120;"
        "combat spherical_range_to_missile drive_explosion = 140;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(CMBTFIRE::defensive_range_bonus == 2);
    DOCTEST_CHECK(CMBTFIRE::weapon_range.tractor_beam == 7);
    DOCTEST_CHECK(CMBTFIRE::weapon_range.proton_torpedo == 10);
    DOCTEST_CHECK(CMBTFIRE::spherical_range_to_ship.pulsar == 8);
    DOCTEST_CHECK(CMBTFIRE::spherical_range_to_ship.drive_explosion == 9);
    DOCTEST_CHECK(
        CMBTFIRE::spherical_range_to_missile.spatial_compressor == 120);
    DOCTEST_CHECK(
        CMBTFIRE::spherical_range_to_missile.drive_explosion == 140);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat weapon_range tractor_beam = 256;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat defensive_range_bonus = 256;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat spherical_range_to_ship pulsar = 256;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat spherical_range_to_missile pulsar = 2147483648;",
        &writes,
        &diagnostic));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
}

DOCTEST_TEST_CASE("absorber and damper ratios preserve classic defaults and bind independently") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTFIRE::absorber_damage_reduction == 25);
    DOCTEST_CHECK(CMBTFIRE::damper_field_ratio == 25);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat absorber_damage_reduction = 40;"
        "combat damper_field_ratio = 60;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTFIRE::absorber_damage_reduction == 40);
    DOCTEST_CHECK(CMBTFIRE::damper_field_ratio == 60);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat absorber_damage_reduction = 101;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat damper_field_ratio = 101;",
        &writes,
        &diagnostic));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
}

DOCTEST_TEST_CASE("ESD bonus preserves the complete unsigned word domain") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(TECHDATA::esd_bonus == 100);

    const uint16_t values[] = {0, 4369, 4370, 65535};
    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    for (uint16_t value : values) {
        DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
        writes.writes.clear();
        const std::string source =
            "combat esd_bonus = " + std::to_string(value) + ";";
        DOCTEST_REQUIRE(Append_Decoded_(
            source.c_str(), &writes, &diagnostic));
        DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
        DOCTEST_REQUIRE(config::Commit_State(&state));
        DOCTEST_CHECK(TECHDATA::esd_bonus == value);
    }

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat esd_bonus = 65536;", &writes, &diagnostic));
}

DOCTEST_TEST_CASE("beam mount damage preserves classic defaults and binds all combinations") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(CMBTFIRE::mount_dmg[CMBTFIRE::HIGH_ENERGY_FOCUS_ABSENT].pd == 50);
    DOCTEST_CHECK(CMBTFIRE::mount_dmg[CMBTFIRE::HIGH_ENERGY_FOCUS_ABSENT].nomod == 100);
    DOCTEST_CHECK(CMBTFIRE::mount_dmg[CMBTFIRE::HIGH_ENERGY_FOCUS_ABSENT].heavy == 150);
    DOCTEST_CHECK(CMBTFIRE::mount_dmg[CMBTFIRE::HIGH_ENERGY_FOCUS_PRESENT].pd == 100);
    DOCTEST_CHECK(CMBTFIRE::mount_dmg[CMBTFIRE::HIGH_ENERGY_FOCUS_PRESENT].nomod == 150);
    DOCTEST_CHECK(CMBTFIRE::mount_dmg[CMBTFIRE::HIGH_ENERGY_FOCUS_PRESENT].heavy == 200);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat mount_dmg nohef pd = 40;"
        "combat mount_dmg nohef nomod = 90;"
        "combat mount_dmg nohef heavy = 140;"
        "combat mount_dmg hef pd = 80;"
        "combat mount_dmg hef nomod = 130;"
        "combat mount_dmg hef heavy = 180;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(CMBTFIRE::mount_dmg[CMBTFIRE::HIGH_ENERGY_FOCUS_ABSENT].pd == 40);
    DOCTEST_CHECK(CMBTFIRE::mount_dmg[CMBTFIRE::HIGH_ENERGY_FOCUS_ABSENT].nomod == 90);
    DOCTEST_CHECK(CMBTFIRE::mount_dmg[CMBTFIRE::HIGH_ENERGY_FOCUS_ABSENT].heavy == 140);
    DOCTEST_CHECK(CMBTFIRE::mount_dmg[CMBTFIRE::HIGH_ENERGY_FOCUS_PRESENT].pd == 80);
    DOCTEST_CHECK(CMBTFIRE::mount_dmg[CMBTFIRE::HIGH_ENERGY_FOCUS_PRESENT].nomod == 130);
    DOCTEST_CHECK(CMBTFIRE::mount_dmg[CMBTFIRE::HIGH_ENERGY_FOCUS_PRESENT].heavy == 180);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat mount_dmg hef heavy = 65536;", &writes, &diagnostic));
}

DOCTEST_TEST_CASE("civilian armor preserves classic defaults and binds by armor name") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    const uint16_t expected[ARMOR_COUNT] = {
        100,
        100,
        100,
        100,
        100,
        100,
        100,
    };
    DOCTEST_CHECK(std::memcmp(CMBTFIRE::civilian_armor, expected, sizeof(expected)) == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat civilian_armor no_armor = 90;"
        "combat civilian_armor titanium = 32767;"
        "combat civilian_armor tritanium = 32768;"
        "combat civilian_armor xentronium = 65535;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTFIRE::civilian_armor[ARMOR_NO_ARMOR] == 90);
    DOCTEST_CHECK(CMBTFIRE::civilian_armor[ARMOR_TITANIUM_ARMOR] == 32767);
    DOCTEST_CHECK(CMBTFIRE::civilian_armor[ARMOR_TRITANIUM_ARMOR] == 32768);
    DOCTEST_CHECK(CMBTFIRE::civilian_armor[ARMOR_XENTRONIUM_ARMOR] == 65535);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat civilian_armor titanium = 0;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat civilian_armor titanium = 65536;", &writes, &diagnostic));
}

DOCTEST_TEST_CASE("Capitol bombardment preserves classic immunity by default") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK_FALSE(BOMB::capitol_can_be_bombed);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat capitol_can_be_bombed = true;", &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(BOMB::capitol_can_be_bombed);

    const config::t_variable_id id =
        Variable_Id_("combat.capitol_can_be_bombed");
    DOCTEST_REQUIRE(id != config::CONFIG_NO_VARIABLE);
    std::string name;
    DOCTEST_REQUIRE(config::Get_Variable_Name(id, &name));
    DOCTEST_CHECK(name == "combat.capitol_can_be_bombed");
}

DOCTEST_TEST_CASE("ground defense armor multiplier preserves and replaces the classic value") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTFIRE::ground_defense_armor_multiplier == 100);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat ground_defense_armor_multiplier = 150;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTFIRE::ground_defense_armor_multiplier == 150);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat ground_defense_armor_multiplier = 0;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat ground_defense_armor_multiplier = 10001;",
        &writes,
        &diagnostic));
}

DOCTEST_TEST_CASE("marine-killing weapon divisor preserves the classic default") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTFIRE::marine_kill_damage == 5);

    config::s_cfg_diagnostic diagnostic;
    const uint8_t divisors[] = {1, 127, 128, 255};
    for (uint8_t divisor : divisors) {
        DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
        config::s_config_write_set writes;
        const std::string source =
            "combat marine_kill_damage = " +
            std::to_string(divisor) + ";";
        DOCTEST_REQUIRE(Append_Decoded_(
            source.c_str(), &writes, &diagnostic));
        DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
        DOCTEST_REQUIRE(config::Commit_State(&state));
        DOCTEST_CHECK(CMBTFIRE::marine_kill_damage == divisor);
    }

    config::s_config_write_set writes;
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat marine_kill_damage = 0;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat marine_kill_damage = -1;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat marine_kill_damage = 256;", &writes, &diagnostic));
}

DOCTEST_TEST_CASE("marine-killing weapon blocker preserves the classic Damper Field") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTFIRE::marine_kill_damage_blocker == SPECIAL_DAMPER_FIELD);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat marine_kill_damage_blocker = transporters;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTFIRE::marine_kill_damage_blocker == SPECIAL_TRANSPORTERS);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat marine_kill_damage_blocker = unknown_special;",
        &writes,
        &diagnostic));
}

DOCTEST_TEST_CASE("shield tables preserve classic defaults and bind by name") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(TECHDATA::shields[SHIELD_CLASS_V_SHIELD].shield_class == 5);
    DOCTEST_CHECK(TECHDATA::shields[SHIELD_CLASS_V_SHIELD]
                      .space_per_ship_type[SHIP_SIZE_LARGE] == 20);
    DOCTEST_CHECK(TECHDATA::shields[SHIELD_CLASS_V_SHIELD]
                      .cost_per_ship_type[SHIP_SIZE_HUGE] == 25);
    DOCTEST_CHECK(TECHDATA::shield_hull_multiplier[HULL_FRIGATE] == 5);
    DOCTEST_CHECK(TECHDATA::shield_hull_multiplier[HULL_DOOM_STAR] == 35);
    DOCTEST_CHECK(TECHDATA::shield_hull_multiplier[HULL_STAR_FORTRESS] == 25);
    DOCTEST_CHECK(TECHDATA::multi_phased_shields_bonus == 50);
    DOCTEST_CHECK(TECHDATA::hard_shields_damage_reduction == 3);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "design shield class_v shield_class = 9;"
        "design shield class_v space cruiser = 33;"
        "design shield class_v cost battleship = 44;"
        "design shield_hull_multiplier doomstar = 41;"
        "design multi_phased_shields_bonus = 75;"
        "combat hard_shields_damage_reduction = 7;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(TECHDATA::shields[SHIELD_CLASS_V_SHIELD].shield_class == 9);
    DOCTEST_CHECK(TECHDATA::shields[SHIELD_CLASS_V_SHIELD]
                      .space_per_ship_type[SHIP_SIZE_LARGE] == 33);
    DOCTEST_CHECK(TECHDATA::shields[SHIELD_CLASS_V_SHIELD]
                      .cost_per_ship_type[SHIP_SIZE_HUGE] == 44);
    DOCTEST_CHECK(TECHDATA::shield_hull_multiplier[HULL_DOOM_STAR] == 41);
    DOCTEST_CHECK(TECHDATA::multi_phased_shields_bonus == 75);
    DOCTEST_CHECK(TECHDATA::hard_shields_damage_reduction == 7);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "design shield class_i shield_class = 65536;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat hard_shields_damage_reduction = 256;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "design multi_phased_shields_bonus = 65536;",
        &writes,
        &diagnostic));
}

DOCTEST_TEST_CASE("Scout Lab research bonuses preserve classic defaults and bind by ship size") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    const uint8_t expected[SHIP_SIZE_COUNT] = {1, 2, 4, 8, 16, 32};
    DOCTEST_CHECK(std::memcmp(COLCALC::scout_lab_research_bonus, expected, sizeof(expected)) == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "research scout_lab_research_bonus destroyer = 7;"
        "research scout_lab_research_bonus doomstar = 41;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(COLCALC::scout_lab_research_bonus[SHIP_SIZE_MEDIUM] == 7);
    DOCTEST_CHECK(COLCALC::scout_lab_research_bonus[SHIP_SIZE_DOOM_STAR] == 41);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "research scout_lab_research_bonus frigate = 256;",
        &writes,
        &diagnostic));
}

DOCTEST_TEST_CASE("research production bonuses preserve 1.50 defaults and bind independently") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(COLCALC::research_colony_bonus.research_lab == 5);
    DOCTEST_CHECK(COLCALC::research_colony_bonus.supercomputer == 10);
    DOCTEST_CHECK(COLCALC::research_colony_bonus.galactic_cybernet == 15);
    DOCTEST_CHECK(COLCALC::research_colony_bonus.autolab == 30);
    DOCTEST_CHECK(COLCALC::research_scientist_bonus.base_scientist == 3);
    DOCTEST_CHECK(COLCALC::research_scientist_bonus.artifacts_planet == 2);
    DOCTEST_CHECK(COLCALC::research_scientist_bonus.orion_planet == 5);
    DOCTEST_CHECK(COLCALC::research_scientist_bonus.research_lab == 1);
    DOCTEST_CHECK(COLCALC::research_scientist_bonus.supercomputer == 2);
    DOCTEST_CHECK(COLCALC::research_scientist_bonus.galactic_cybernet == 3);
    DOCTEST_CHECK(COLCALC::research_scientist_bonus.astro_university == 1);
    DOCTEST_CHECK(COLCALC::research_scientist_bonus.android_scientist == 3);
    DOCTEST_CHECK(COLCALC::research_scientist_bonus.heightened_intelligence == 1);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "research research_colony_bonus research_lab = 41;"
        "research research_colony_bonus supercomputer = 43;"
        "research research_colony_bonus galactic_cybernet = 47;"
        "research research_colony_bonus autolab = 53;"
        "research research_scientist_bonus base_scientist = 7;"
        "research research_scientist_bonus artifacts_planet = 11;"
        "research research_scientist_bonus orion_planet = 13;"
        "research research_scientist_bonus research_lab = 17;"
        "research research_scientist_bonus supercomputer = 19;"
        "research research_scientist_bonus galactic_cybernet = 23;"
        "research research_scientist_bonus astro_university = 29;"
        "research research_scientist_bonus android_scientist = 31;"
        "research research_scientist_bonus heightened_intelligence = 37;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(COLCALC::research_colony_bonus.research_lab == 41);
    DOCTEST_CHECK(COLCALC::research_colony_bonus.supercomputer == 43);
    DOCTEST_CHECK(COLCALC::research_colony_bonus.galactic_cybernet == 47);
    DOCTEST_CHECK(COLCALC::research_colony_bonus.autolab == 53);
    DOCTEST_CHECK(COLCALC::research_scientist_bonus.base_scientist == 7);
    DOCTEST_CHECK(COLCALC::research_scientist_bonus.artifacts_planet == 11);
    DOCTEST_CHECK(COLCALC::research_scientist_bonus.orion_planet == 13);
    DOCTEST_CHECK(COLCALC::research_scientist_bonus.research_lab == 17);
    DOCTEST_CHECK(COLCALC::research_scientist_bonus.supercomputer == 19);
    DOCTEST_CHECK(COLCALC::research_scientist_bonus.galactic_cybernet == 23);
    DOCTEST_CHECK(COLCALC::research_scientist_bonus.astro_university == 29);
    DOCTEST_CHECK(COLCALC::research_scientist_bonus.android_scientist == 31);
    DOCTEST_CHECK(COLCALC::research_scientist_bonus.heightened_intelligence == 37);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "research research_colony_bonus autolab = 256;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "research research_scientist_bonus base_scientist = 256;",
        &writes,
        &diagnostic));
}

DOCTEST_TEST_CASE("hyper-advanced research cost increment binds every cost calculation") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(COLCALC::hyper_advanced_cost_increment == 10000);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "research hyper_advanced_cost_increment = 12000;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(COLCALC::hyper_advanced_cost_increment == 12000);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "research hyper_advanced_cost_increment = 65536;",
        &writes,
        &diagnostic));
}

DOCTEST_TEST_CASE("command-point settings preserve classic defaults and bind independently") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(COLCALC::command_points.starting == 5);
    DOCTEST_CHECK(COLCALC::command_points.star_base == 1);
    DOCTEST_CHECK(COLCALC::command_points.battlestation == 2);
    DOCTEST_CHECK(COLCALC::command_points.star_fortress == 3);
    DOCTEST_CHECK(COLCALC::command_points.tachyon_communications == 1);
    DOCTEST_CHECK(COLCALC::command_points.sub_space_communications == 2);
    DOCTEST_CHECK(COLCALC::command_points.hyperspace_communications == 3);
    DOCTEST_CHECK(COLCALC::command_points.warlord_planet == 2);
    const uint8_t expected[SHIP_SIZE_COUNT] = {1, 2, 3, 4, 5, 6};
    DOCTEST_CHECK(std::memcmp(COLCALC::ship_command_points_usage, expected, sizeof(expected)) == 0);
    DOCTEST_CHECK(INITSHIP::landers_size_class == SHIP_SIZE_SMALL);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "command command_points starting = -1;"
        "command command_points star_base = 4;"
        "command command_points battlestation = 6;"
        "command command_points star_fortress = 8;"
        "command command_points tachyon_communications = -128;"
        "command command_points sub_space_communications = -1;"
        "command command_points hyperspace_communications = -128;"
        "command command_points warlord_planet = 3;"
        "command ship_command_points_usage cruiser = 9;"
        "command landers_size_class = cruiser;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(COLCALC::command_points.starting == -1);
    DOCTEST_CHECK(COLCALC::command_points.star_fortress == 8);
    DOCTEST_CHECK(COLCALC::command_points.tachyon_communications == -128);
    DOCTEST_CHECK(COLCALC::command_points.sub_space_communications == -1);
    DOCTEST_CHECK(COLCALC::command_points.hyperspace_communications == -128);
    DOCTEST_CHECK(COLCALC::ship_command_points_usage[SHIP_SIZE_LARGE] == 9);
    DOCTEST_CHECK(INITSHIP::landers_size_class == SHIP_SIZE_LARGE);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "command command_points starting = -129;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "command command_points starting = 128;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "command ship_command_points_usage frigate = 256;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "command landers_size_class = orbital;", &writes, &diagnostic));
}

DOCTEST_TEST_CASE("research retention settings preserve classic defaults") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COLCALC::keep_research_on_breakthrough == 0);
    DOCTEST_CHECK(COLCALC::keep_research_on_finds == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "research keep_research_on_breakthrough = true;"
        "research keep_research_on_finds = true;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COLCALC::keep_research_on_breakthrough == 1);
    DOCTEST_CHECK(COLCALC::keep_research_on_finds == 1);
}

DOCTEST_TEST_CASE("Post-warp technology defaults and indexed bindings match 1.50") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    const int16_t expected[TECHINIT::POSTWARP_TECH_COUNT] = {
        56,
        20,
        192,
        25,
        82,
        13,
        13,
        22,
        132,
        26,
        179,
        71,
        72,
        51,
        191,
        163,
        46,
        122,
        155,
        61,
        87,
        70,
        73,
        33,
        57,
        104,
        180,
        181,
        66,
    };
    DOCTEST_CHECK(std::memcmp(
                      TECHINIT::postwarp_tech, expected, sizeof(expected)) == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "research postwarp_tech 1 = 0;"
        "research postwarp_tech 29 = 211;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(TECHINIT::postwarp_tech[0] == 0);
    DOCTEST_CHECK(TECHINIT::postwarp_tech[28] == 211);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "research postwarp_tech 1 = -1;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "research postwarp_tech 1 = 212;",
        &writes,
        &diagnostic));
}

DOCTEST_TEST_CASE("Pre-Warp and Average Technology fields bind by starting slot") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    const int16_t expected[6] = {
        TECH_FIELD_ENGINEERING,
        TECH_FIELD_NUCLEAR_FISSION,
        TECH_FIELD_CHEMISTRY,
        TECH_FIELD_PHYSICS,
        TECH_FIELD_ELECTRONICS,
        TECH_FIELD_COLD_FUSION,
    };
    DOCTEST_CHECK(std::memcmp(
                      TECHDATA::starting_tech_field_ids, expected, sizeof(expected)) == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "technology starting_fields prewarp = chemistry;"
        "technology starting_fields average_1 = starting_tech;"
        "technology starting_fields average_2 = electronics;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(TECHDATA::starting_tech_field_ids[0] == TECH_FIELD_CHEMISTRY);
    DOCTEST_CHECK(TECHDATA::starting_tech_field_ids[1] == TECH_FIELD_STARTING_TECH);
    DOCTEST_CHECK(TECHDATA::starting_tech_field_ids[2] == TECH_FIELD_ELECTRONICS);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "technology starting_fields average_5 = 83;", &writes, &diagnostic));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
}

DOCTEST_TEST_CASE("technology branch roots bind independently from game-start fields") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    const int16_t expected[10] = {0, 18, 55, 57, 29, 7, 22, 28, 10, 74};
    DOCTEST_CHECK(std::memcmp(MOX::first_field_in_group, expected, sizeof(expected)) == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "technology starting_techfield biology = advanced_biology;"
        "technology starting_techfield force_fields = distortion_fields;"
        "technology starting_techfield sociology = sociology;"
        "technology starting_techfield xenon = xeno_relations;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(MOX::first_field_in_group[0] == 0);
    DOCTEST_CHECK(TECHDATA::starting_techfield.biology ==
        TECH_FIELD_ADVANCED_BIOLOGY);
    DOCTEST_CHECK(TECHDATA::starting_techfield.force_fields ==
        TECH_FIELD_DISTORTION_FIELDS);
    DOCTEST_CHECK(TECHDATA::starting_techfield.sociology ==
        TECH_FIELD_SOCIOLOGY);
    DOCTEST_CHECK(TECHDATA::starting_techfield.xenon ==
        TECH_FIELD_XENO_RELATIONS);
    DOCTEST_CHECK(TECHDATA::starting_tech_field_ids[0] ==
        TECH_FIELD_ENGINEERING);

    writes.writes.clear();
    DOCTEST_REQUIRE(Append_Decoded_(
        "technology starting_techfield biology = starting_tech;",
        &writes,
        &diagnostic));
    DOCTEST_CHECK_FALSE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "technology starting_techfield biology = 0;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "technology starting_techfield sociology = 83;",
        &writes,
        &diagnostic));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
}

DOCTEST_TEST_CASE("ground Commando settings preserve classic defaults") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(invasion::ground_commando_defender_x5 == 0);
    DOCTEST_CHECK(invasion::ground_commando_attacker_x2 == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "leaders ground_commando_defender_x5 = true;"
        "leaders ground_commando_attacker_x2 = true;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(invasion::ground_commando_defender_x5 == 1);
    DOCTEST_CHECK(invasion::ground_commando_attacker_x2 == 1);
}

DOCTEST_TEST_CASE("leader Warlord setting preserves the classic default") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(OFFICER::leader_warlord_bonus == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "leaders leader_warlord_bonus = true;", &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(OFFICER::leader_warlord_bonus == 1);
}

DOCTEST_TEST_CASE("unlimited leaders setting preserves the classic default") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(OFFICER::leaders_no_limit == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "leaders leaders_no_limit = true;", &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(OFFICER::leaders_no_limit == 1);
}

DOCTEST_TEST_CASE("leader hire-cost settings bind native skill and owner state") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(OFFICER::commando_hire_cost.colony == 1);
    DOCTEST_CHECK(OFFICER::commando_hire_cost.ship == 3);
    DOCTEST_CHECK(OFFICER::megawealth_doubles_hire_cost == 1);

    const char* skill_names[] = {
        "assassin",
        "assassin_improved",
        "commando",
        "commando_improved",
        "diplomat",
        "diplomat_improved",
        "engineer",
        "engineer_improved",
        "environmentalist",
        "environmentalist_improved",
        "famous",
        "famous_improved",
        "farming_leader",
        "farming_leader_improved",
        "fighter_pilot",
        "fighter_pilot_improved",
        "financial_leader",
        "financial_leader_improved",
        "galactic_lore",
        "galactic_lore_improved",
        "helmsman",
        "helmsman_improved",
        "instructor",
        "instructor_improved",
        "labor_leader",
        "labor_leader_improved",
        "medicine",
        "medicine_improved",
        "megawealth",
        "megawealth_improved",
        "navigator",
        "navigator_improved",
        "operations",
        "operations_improved",
        "ordnance",
        "ordnance_improved",
        "researcher",
        "researcher_improved",
        "science_leader",
        "science_leader_improved",
        "security",
        "security_improved",
        "spiritual_leader",
        "spiritual_leader_improved",
        "spy_master",
        "spy_master_improved",
        "tactics",
        "tactics_improved",
        "telepath",
        "telepath_improved",
        "trader",
        "trader_improved",
        "weaponry",
        "weaponry_improved",
    };
    for (const char* skill_name : skill_names) {
        char source[96];
        snprintf(source, sizeof(source), "leaders skill_cost %s = 1;", skill_name);
        config::s_config_write_set semantic_name_writes;
        config::s_cfg_diagnostic semantic_name_diagnostic;
        DOCTEST_CHECK(Append_Decoded_(
            source, &semantic_name_writes, &semantic_name_diagnostic));
    }

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "leaders skill_cost assassin = 7;"
        "leaders skill_cost megawealth_improved = 5;"
        "leaders commando_hire_cost colony = 4;"
        "leaders commando_hire_cost ship = 6;"
        "leaders megawealth_doubles_hire_cost = false;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(MOX::skill_data[0].cost == 7);
    DOCTEST_CHECK(MOX::skill_data[29].cost == 5);
    DOCTEST_CHECK(OFFICER::commando_hire_cost.colony == 4);
    DOCTEST_CHECK(OFFICER::commando_hire_cost.ship == 6);
    DOCTEST_CHECK(OFFICER::megawealth_doubles_hire_cost == 0);

    writes.writes.clear();
    DOCTEST_REQUIRE(Append_Decoded_(
        "leaders megawealth_doubles_hire_cost = true;", &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(OFFICER::megawealth_doubles_hire_cost == 1);

    writes.writes.clear();
    DOCTEST_REQUIRE(Append_Decoded_(
        "leaders commando_hire_cost colony = 128;"
        "leaders commando_hire_cost ship = 255;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(OFFICER::commando_hire_cost.colony == 128);
    DOCTEST_CHECK(OFFICER::commando_hire_cost.ship == 255);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "leaders commando_hire_cost colony = -1;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "leaders commando_hire_cost ship = 256;", &writes, &diagnostic));

    writes.writes.clear();
    DOCTEST_REQUIRE(Append_Decoded_(
        "leaders skill_cost assassin = 128;"
        "leaders skill_cost assassin_improved = 255;"
        "leaders skill_cost commando = 0;"
        "leaders skill_cost commando_improved = 127;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(MOX::skill_data[0].cost == 128);
    DOCTEST_CHECK(MOX::skill_data[1].cost == 255);
    DOCTEST_CHECK(MOX::skill_data[2].cost == 0);
    DOCTEST_CHECK(MOX::skill_data[3].cost == 127);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "leaders skill_cost assassin = 256;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "leaders skill_cost unknown = 2;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "leaders skill_cost assassin2 = 2;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "leaders skill_cost unused = 2;", &writes, &diagnostic));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
}

DOCTEST_TEST_CASE("colony morale rules preserve classic defaults and bind by government") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    const int8_t government_penalty[GOVERNMENT_COUNT] = {-4, -4, -4, 0, 0, 0, 0, 0};
    const int8_t no_capitol_penalty[3] = {-10, -7, -4};
    const uint8_t ignore_mixed_race_penalty[GOVERNMENT_COUNT] = {};
    DOCTEST_CHECK(std::memcmp(COLCALC::morale_lookup_table, government_penalty, sizeof(government_penalty)) == 0);
    DOCTEST_CHECK(std::memcmp(COLCALC::non_capital_morale_lookup,
                      no_capitol_penalty,
                      sizeof(no_capitol_penalty)) == 0);
    DOCTEST_CHECK(std::memcmp(COLCALC::ignore_mixed_race_penalty,
                      ignore_mixed_race_penalty,
                      sizeof(ignore_mixed_race_penalty)) == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "colony morale government_penalty feudal = -5;"
        "colony morale no_capitol_penalty democracy_federation = -6;"
        "colony morale ignore_mixed_race_penalty federation = true;"
        "colony morale ignore_mixed_race_penalty dictatorship = true;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COLCALC::morale_lookup_table[GOVERNMENT_FEUDAL] == -5);
    DOCTEST_CHECK(COLCALC::non_capital_morale_lookup[2] == -6);
    DOCTEST_CHECK(COLCALC::ignore_mixed_race_penalty[GOVERNMENT_FEDERATION] == 1);
    DOCTEST_CHECK(COLCALC::ignore_mixed_race_penalty[GOVERNMENT_DICTATORSHIP] == 1);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "colony morale government_penalty feudal = -129;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "colony morale no_capitol_penalty federation = -5;", &writes, &diagnostic));
}

DOCTEST_TEST_CASE("AI productivity and income bonuses preserve classic defaults") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    const int8_t productivity_bonus[GAME_DIFFICULTY_COUNT] = {-10, 0, 10, 20, 40};
    const int8_t income_bonus[GAME_DIFFICULTY_COUNT] = {0, 0, 1, 2, 3};
    DOCTEST_CHECK(std::memcmp(COLCALC::colony_job_production_difficulty_lookup,
                      productivity_bonus,
                      sizeof(productivity_bonus)) == 0);
    DOCTEST_CHECK(std::memcmp(COLCALC::difficulty_bc_lookup_table,
                      income_bonus,
                      sizeof(income_bonus)) == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "ai productivity_bonus hard = 25;"
        "ai income_bonus impossible = 4;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COLCALC::colony_job_production_difficulty_lookup[GAME_DIFFICULTY_HARD] == 25);
    DOCTEST_CHECK(COLCALC::difficulty_bc_lookup_table[GAME_DIFFICULTY_IMPOSSIBLE] == 4);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "ai productivity_bonus hard = 128;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "ai income_bonus impossible = -129;", &writes, &diagnostic));
}

DOCTEST_TEST_CASE("Housing population assignment setting preserves the classic default") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COLCALC::housing_creates_no_farmers == 0);
    DOCTEST_CHECK(COLCALC::housing_no_popagg == 0);
    DOCTEST_CHECK(COLCALC::new_farmer_food_threshold == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "colony housing_creates_no_farmers = true;"
        "colony housing_no_popagg = true;"
        "colony new_farmer_food_threshold = 127;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COLCALC::housing_creates_no_farmers == 1);
    DOCTEST_CHECK(COLCALC::housing_no_popagg == 1);
    DOCTEST_CHECK(COLCALC::new_farmer_food_threshold == 127);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "colony new_farmer_food_threshold = 128;", &writes, &diagnostic));
}

DOCTEST_TEST_CASE("multibuild preserves the classic default and binds explicitly") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COLCALC::multibuild == 0);
    DOCTEST_CHECK(COLCALC::multibuild_nonbuilding_products_only == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "colony multibuild = true;", &writes, &diagnostic));
    DOCTEST_REQUIRE(Append_Decoded_(
        "colony multibuild_nonbuilding_products_only = true;", &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COLCALC::multibuild == 1);
    DOCTEST_CHECK(COLCALC::multibuild_nonbuilding_products_only == 1);
}

DOCTEST_TEST_CASE("Toxic conversion building preserves the classic default and binds by name") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COLCALC::terraform_toxic_building == BUILDING_NO_BUILDING);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "colony terraform_toxic_building = atmosphere_renewer;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(
        COLCALC::terraform_toxic_building == BUILDING_ATMOSPHERE_RENEWER);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "colony terraform_toxic_building = unknown_building;",
        &writes,
        &diagnostic));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
}

DOCTEST_TEST_CASE("landing ship costs preserve classic defaults and bind independently") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(COLCALC::lander_ship_cost.colony_ship == 500);
    DOCTEST_CHECK(COLCALC::lander_ship_cost.outpost_ship == 100);
    DOCTEST_CHECK(COLCALC::lander_ship_cost.transport_ship == 100);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "colony lander_ship_cost colony_ship = 600;"
        "colony lander_ship_cost outpost_ship = 200;"
        "colony lander_ship_cost transport_ship = 80;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(COLCALC::lander_ship_cost.colony_ship == 600);
    DOCTEST_CHECK(COLCALC::lander_ship_cost.outpost_ship == 200);
    DOCTEST_CHECK(COLCALC::lander_ship_cost.transport_ship == 80);
}

DOCTEST_TEST_CASE("population capacity preserves classic defaults and binds by size and climate") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    const uint8_t size[PLANET_SIZE_COUNT] = {5, 10, 15, 20, 25};
    const uint8_t climate[PLANET_CLIMATE_COUNT] = {
        25,
        25,
        25,
        25,
        25,
        25,
        40,
        60,
        80,
        100,
    };
    DOCTEST_CHECK(std::memcmp(COLCALC::population_capacity.size, size, sizeof(size)) == 0);
    DOCTEST_CHECK(std::memcmp(COLCALC::population_capacity.climate, climate, sizeof(climate)) == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "colony population_capacity size tiny = 0;"
        "colony population_capacity size small = 127;"
        "colony population_capacity size medium = 128;"
        "colony population_capacity size large = 255;"
        "colony population_capacity size huge = 30;"
        "colony population_capacity climate swamp = 70;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(COLCALC::population_capacity.size[PLANET_SIZE_TINY] == 0);
    DOCTEST_CHECK(COLCALC::population_capacity.size[PLANET_SIZE_SMALL] == 127);
    DOCTEST_CHECK(COLCALC::population_capacity.size[PLANET_SIZE_MEDIUM] == 128);
    DOCTEST_CHECK(COLCALC::population_capacity.size[PLANET_SIZE_LARGE] == 255);
    DOCTEST_CHECK(COLCALC::population_capacity.size[PLANET_SIZE_HUGE] == 30);
    DOCTEST_CHECK(COLCALC::population_capacity.climate[PLANET_CLIMATE_SWAMP] == 70);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "colony population_capacity size tiny = 256;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "colony population_capacity climate gaia = 256;", &writes, &diagnostic));
}

DOCTEST_TEST_CASE("population table configuration binds independently and is saved and synchronized") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK_FALSE(COLCALC::population_capacity_table_enabled);
    DOCTEST_CHECK(COLCALC::population_capacity_table
                      [PLANET_CLIMATE_TOXIC][PLANET_SIZE_SMALL] == 3);
    DOCTEST_CHECK(COLCALC::population_capacity_table_tolerant
                      [PLANET_CLIMATE_TOXIC][PLANET_SIZE_SMALL] == 5);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "colony population_capacity_table_enabled = true;"
        "colony population_capacity_table gaia = 0 127 128 255 30;"
        "colony population_capacity_table terran medium = 17;"
        "colony population_capacity_table_tolerant toxic small = 19;"
        "colony population_capacity size huge = 40;"
        "colony population_capacity climate toxic = 35;"
        "colony pop_bonus tolerant = 50;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COLCALC::population_capacity_table_enabled);
    const uint8_t expected[PLANET_SIZE_COUNT] = {0, 127, 128, 255, 30};
    DOCTEST_CHECK(std::memcmp(COLCALC::population_capacity_table
                                  [PLANET_CLIMATE_GAIA],
                      expected,
                      sizeof(expected)) == 0);
    DOCTEST_CHECK(COLCALC::population_capacity_table
                      [PLANET_CLIMATE_TERRAN][PLANET_SIZE_MEDIUM] == 17);
    DOCTEST_CHECK(COLCALC::population_capacity_table_tolerant
                      [PLANET_CLIMATE_TOXIC][PLANET_SIZE_SMALL] == 19);
    DOCTEST_CHECK(COLCALC::population_capacity.size[PLANET_SIZE_HUGE] == 40);
    DOCTEST_CHECK(COLCALC::population_capacity.climate[PLANET_CLIMATE_TOXIC] == 35);
    DOCTEST_CHECK(COLCALC::population_bonus.tolerant == 50);

    for (const char* source : {
             "colony population_capacity_table gaia tiny = -1;",
             "colony population_capacity_table gaia huge = 256;",
             "colony population_capacity_table_tolerant toxic small = -1;",
             "colony population_capacity_table_tolerant toxic small = 256;",
             "colony population_capacity_table unknown small = 5;",
             "colony population_capacity_table_tolerant toxic unknown = 5;",
             "colony population_capacity_table_enabled = 1;"}) {
        writes.writes.clear();
        DOCTEST_CHECK_FALSE(Append_Decoded_(source, &writes, &diagnostic));
    }

    for (const char* path : {
             "colony.population_capacity_table_enabled",
             "colony.population_capacity_table.8.2",
             "colony.population_capacity_table_tolerant.0.1"}) {
        const config::t_variable_id id = Variable_Id_(path);
        DOCTEST_REQUIRE(id != config::CONFIG_NO_VARIABLE);
        std::string resolved;
        DOCTEST_REQUIRE(config::Get_Variable_Name(id, &resolved));
        DOCTEST_CHECK(resolved == path);
        const config::s_config_group_descriptor* group =
            config::Get_Config_Group(config::Get_Variable_Group(id));
        DOCTEST_REQUIRE(group != nullptr);
        DOCTEST_CHECK(group->save);
        DOCTEST_CHECK(group->synchronize);
    }

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK_FALSE(COLCALC::population_capacity_table_enabled);
    DOCTEST_CHECK(COLCALC::population_capacity_table
                      [PLANET_CLIMATE_TERRAN][PLANET_SIZE_MEDIUM] == 12);
    DOCTEST_CHECK(COLCALC::population_capacity_table_tolerant
                      [PLANET_CLIMATE_TOXIC][PLANET_SIZE_SMALL] == 5);
}

DOCTEST_TEST_CASE("Advanced City Planning population bonus preserves the classic default") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COLCALC::population_bonus.advanced_city_planning == 5);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "colony pop_bonus advanced_city_planning = 7;", &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COLCALC::population_bonus.advanced_city_planning == 7);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "colony pop_bonus advanced_city_planning = 256;", &writes, &diagnostic));
}

DOCTEST_TEST_CASE("Tolerant population bonus preserves the classic default") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COLCALC::population_bonus.tolerant == 25);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "colony pop_bonus tolerant = 50;", &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COLCALC::population_bonus.tolerant == 50);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "colony pop_bonus tolerant = -129;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "colony pop_bonus tolerant = 128;", &writes, &diagnostic));
}

DOCTEST_TEST_CASE("starting Outpost Ship setting preserves the classic default") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(ERICNET::starting_outpost_ship == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "colony starting_outpost_ship = true;", &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(ERICNET::starting_outpost_ship == 1);
}

DOCTEST_TEST_CASE("starting resources preserve classic defaults and bind independently") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(INITGAME::start.population == 8);
    DOCTEST_CHECK(INITGAME::start.marines == 3);
    DOCTEST_CHECK(INITGAME::start.money == 50);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "game start population = 10;"
        "game start marines = 4;"
        "game start money = 75;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(INITGAME::start.population == 10);
    DOCTEST_CHECK(INITGAME::start.marines == 4);
    DOCTEST_CHECK(INITGAME::start.money == 75);

    struct s_start_case {
        const char* source;
        uint8_t population;
        uint8_t marines;
        uint32_t money;
    };
    const s_start_case cases[] = {
        {"game start population = 0; game start marines = 0; game start money = 0;",
            0,
            0,
            0},
        {"game start population = 42; game start marines = 255; game start money = 2147483647;",
            42,
            255,
            2147483647},
    };
    for (const s_start_case& test_case : cases) {
        DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
        writes.writes.clear();
        DOCTEST_REQUIRE(Append_Decoded_(
            test_case.source, &writes, &diagnostic));
        DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
        DOCTEST_REQUIRE(config::Commit_State(&state));
        DOCTEST_CHECK(INITGAME::start.population == test_case.population);
        DOCTEST_CHECK(INITGAME::start.marines == test_case.marines);
        DOCTEST_CHECK(INITGAME::start.money == test_case.money);
    }

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "game start population = 43;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "game start population = 255;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "game start population = 256;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "game start marines = 256;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "game start money = 2147483648;", &writes, &diagnostic));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
}

DOCTEST_TEST_CASE("tech-level starting money overrides inherit the global value by default") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    for (const INITGAME::s_tech_start& configured_start : INITGAME::tech_start) {
        DOCTEST_CHECK(configured_start.money == -1);
    }

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "game tech_start prewarp money = 0;"
        "game tech_start average money = 50;"
        "game tech_start advanced money = 100;"
        "game tech_start postwarp money = 2147483647;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(INITGAME::tech_start[STARTING_CIVILIZATION_LEVEL_PRE_WARP].money == 0);
    DOCTEST_CHECK(INITGAME::tech_start[STARTING_CIVILIZATION_LEVEL_AVERAGE].money == 50);
    DOCTEST_CHECK(INITGAME::tech_start[STARTING_CIVILIZATION_LEVEL_ADVANCED].money == 100);
    DOCTEST_CHECK(INITGAME::tech_start[STARTING_CIVILIZATION_LEVEL_POST_WARP].money == 2147483647);

    writes.writes.clear();
    DOCTEST_REQUIRE(Append_Decoded_(
        "game tech_start advanced money = -1;", &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(INITGAME::tech_start[STARTING_CIVILIZATION_LEVEL_ADVANCED].money == -1);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "game tech_start average money = -2;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "game tech_start postwarp money = 2147483648;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "game tech_start unknown money = 50;", &writes, &diagnostic));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
}

DOCTEST_TEST_CASE("freighter cost settings preserve classic defaults and bind independently") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(COLCALC::misc_costs.freighters == 50);
    DOCTEST_CHECK(COLCALC::misc_costs.spy == 100);
    DOCTEST_CHECK(COLCALC::misc_costs.terraforming_step == 250);
    DOCTEST_CHECK(COLCALC::feudal_affects_freighters == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "colony misc_costs freighters = 48;"
        "colony misc_costs spy = 120;"
        "colony misc_costs terraforming_step = 300;"
        "colony feudal_affects_freighters = true;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(COLCALC::misc_costs.freighters == 48);
    DOCTEST_CHECK(COLCALC::misc_costs.spy == 120);
    DOCTEST_CHECK(COLCALC::misc_costs.terraforming_step == 300);
    DOCTEST_CHECK(COLCALC::feudal_affects_freighters == 1);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "colony misc_costs freighters = 32768;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "colony misc_costs spy = 32768;",
        &writes,
        &diagnostic));
}

DOCTEST_TEST_CASE("freighter cash bonus preserves the unsigned byte domain") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COLCALC::freighters_cash_bonus == 0);

    struct s_bonus_case {
        const char* source;
        uint8_t expected;
    };
    const s_bonus_case cases[] = {
        {"colony freighters_cash_bonus = 127;", 127},
        {"colony freighters_cash_bonus = 128;", 128},
        {"colony freighters_cash_bonus = 255;", 255},
    };

    config::s_cfg_diagnostic diagnostic;
    for (const s_bonus_case& test_case : cases) {
        DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
        config::s_config_write_set writes;
        DOCTEST_REQUIRE(Append_Decoded_(
            test_case.source, &writes, &diagnostic));
        DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
        DOCTEST_REQUIRE(config::Commit_State(&state));
        DOCTEST_CHECK(COLCALC::freighters_cash_bonus == test_case.expected);
    }

    config::s_config_write_set writes;
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "colony freighters_cash_bonus = -1;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "colony freighters_cash_bonus = 256;", &writes, &diagnostic));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
}

DOCTEST_TEST_CASE("population growth-rate modifiers preserve 1.50 defaults") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(COLCALC::growth_rate_bonus.general_modifier == 0);
    DOCTEST_CHECK(COLCALC::growth_rate_bonus.microbiotics_tech == 25);
    DOCTEST_CHECK(COLCALC::growth_rate_bonus.antidote_tech == 50);
    DOCTEST_CHECK(COLCALC::growth_rate_bonus.boom_event == 100);
    DOCTEST_CHECK(COLCALC::growth_rate_bonus.plague_event == -200);
    DOCTEST_CHECK(COLCALC::cumulative_growth_tech_bonuses == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "colony growth_rate_bonus general_modifier = 10;"
        "colony growth_rate_bonus microbiotics_tech = 20;"
        "colony growth_rate_bonus antidote_tech = 30;"
        "colony growth_rate_bonus boom_event = 127;"
        "colony growth_rate_bonus plague_event = -300;"
        "colony cumulative_growth_tech_bonuses = true;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(COLCALC::growth_rate_bonus.general_modifier == 10);
    DOCTEST_CHECK(COLCALC::growth_rate_bonus.microbiotics_tech == 20);
    DOCTEST_CHECK(COLCALC::growth_rate_bonus.antidote_tech == 30);
    DOCTEST_CHECK(COLCALC::growth_rate_bonus.boom_event == 127);
    DOCTEST_CHECK(COLCALC::growth_rate_bonus.plague_event == -300);
    DOCTEST_CHECK(COLCALC::cumulative_growth_tech_bonuses == 1);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "colony growth_rate_bonus general_modifier = 256;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "colony growth_rate_bonus boom_event = 128;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "colony growth_rate_bonus plague_event = 1;",
        &writes,
        &diagnostic));
}

DOCTEST_TEST_CASE("technology spy bonuses preserve 1.50 defaults and bind independently") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(spy::spy_tech_bonus.neural_scanner == 10);
    DOCTEST_CHECK(spy::spy_tech_bonus.xeno_psychology == 0);
    DOCTEST_CHECK(spy::spy_tech_bonus.telepathic_training == 5);
    DOCTEST_CHECK(spy::spy_tech_bonus.cyber_security_link == 10);
    DOCTEST_CHECK(spy::spy_tech_bonus.stealth_suit == 10);
    DOCTEST_CHECK(spy::spy_tech_bonus.psionics == 10);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "spy spy_tech_bonus neural_scanner = 12;"
        "spy spy_tech_bonus xeno_psychology = 7;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(spy::spy_tech_bonus.neural_scanner == 12);
    DOCTEST_CHECK(spy::spy_tech_bonus.xeno_psychology == 7);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "spy spy_tech_bonus xeno_psychology = 256;",
        &writes,
        &diagnostic));
}

DOCTEST_TEST_CASE("Telepathic spying bonus preserves the classic default") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(spy::telepathic_bonus == 10);

    config::s_cfg_diagnostic diagnostic;
    struct s_bonus_case {
        const char* source;
        uint8_t expected;
    };
    const s_bonus_case cases[] = {
        {"spy telepathic_bonus = 0;", 0},
        {"spy telepathic_bonus = 10;", 10},
        {"spy telepathic_bonus = 127;", 127},
        {"spy telepathic_bonus = 128;", 128},
        {"spy telepathic_bonus = 255;", 255},
    };
    for (const s_bonus_case& test_case : cases) {
        DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
        config::s_config_write_set writes;
        DOCTEST_REQUIRE(Append_Decoded_(
            test_case.source, &writes, &diagnostic));
        DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
        DOCTEST_REQUIRE(config::Commit_State(&state));
        DOCTEST_CHECK(spy::telepathic_bonus == test_case.expected);
    }

    config::s_config_write_set writes;
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "spy telepathic_bonus = 256;", &writes, &diagnostic));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
}

DOCTEST_TEST_CASE("government spy bonuses preserve their offensive modifiers") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    const int8_t expected[GOVERNMENT_COUNT] = {0, 0, 10, 15, -10, -10, 15, 15};
    DOCTEST_CHECK(std::memcmp(spy::lookup_table, expected, sizeof(expected)) == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "spy government_bonus federation = -20;"
        "spy government_bonus democracy = 12;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(spy::lookup_table[GOVERNMENT_FEDERATION] == -20);
    DOCTEST_CHECK(spy::lookup_table[GOVERNMENT_DEMOCRACY] == 12);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "spy government_bonus federation = 128;",
        &writes,
        &diagnostic));
}

DOCTEST_TEST_CASE("spy theft policy preserves the classic default and named modes") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(spy::steal_mode == spy::STEAL_MODE_EXCEPT_CURRENT_FIELD);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "spy steal_mode = except_current_tech;", &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(spy::steal_mode == spy::STEAL_MODE_EXCEPT_CURRENT_TECH);

    writes.writes.clear();
    DOCTEST_REQUIRE(Append_Decoded_(
        "spy steal_mode = anything;", &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(spy::steal_mode == spy::STEAL_MODE_ANYTHING);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "spy steal_mode = current_only;", &writes, &diagnostic));
}

DOCTEST_TEST_CASE("spy success thresholds preserve 1.50 defaults and bounds") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(spy::success.steal == 80);
    DOCTEST_CHECK(spy::success.steal_and_frame == 90);
    DOCTEST_CHECK(spy::success.sabotage == 70);
    DOCTEST_CHECK(spy::success.sabotage_and_frame == 90);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "spy success steal = 75;"
        "spy success steal_and_frame = 85;"
        "spy success sabotage = 65;"
        "spy success sabotage_and_frame = 95;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(spy::success.steal == 75);
    DOCTEST_CHECK(spy::success.steal_and_frame == 85);
    DOCTEST_CHECK(spy::success.sabotage == 65);
    DOCTEST_CHECK(spy::success.sabotage_and_frame == 95);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "spy success sabotage = 128;", &writes, &diagnostic));
}

DOCTEST_TEST_CASE("spy roll settings preserve 1.50 defaults and enforce count ordering") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(spy::count_bonus.plus_two == 5);
    DOCTEST_CHECK(spy::count_bonus.plus_one == 10);
    DOCTEST_CHECK(spy::random_bonus.attacker == 100);
    DOCTEST_CHECK(spy::random_bonus.defender == 100);
    DOCTEST_CHECK(spy::guaranteed_roll.steal == 100);
    DOCTEST_CHECK(spy::guaranteed_roll.sabotage == 100);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "spy count_bonus plus_two = 6;"
        "spy count_bonus plus_one = 12;"
        "spy roll_random_bonus attacker = 200;"
        "spy roll_random_bonus defender = 300;"
        "spy guaranteed_roll steal = 99;"
        "spy guaranteed_roll sabotage = 98;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(spy::count_bonus.plus_two == 6);
    DOCTEST_CHECK(spy::count_bonus.plus_one == 12);
    DOCTEST_CHECK(spy::random_bonus.attacker == 200);
    DOCTEST_CHECK(spy::random_bonus.defender == 300);
    DOCTEST_CHECK(spy::guaranteed_roll.steal == 99);
    DOCTEST_CHECK(spy::guaranteed_roll.sabotage == 98);

    writes.writes.clear();
    DOCTEST_REQUIRE(Append_Decoded_(
        "spy count_bonus plus_two = 12;", &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_CHECK_FALSE(config::Commit_State(&state));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "spy count_bonus plus_one = 128;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "spy guaranteed_roll sabotage = 128;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "spy roll_random_bonus attacker = 2147483648;",
        &writes,
        &diagnostic));
}

DOCTEST_TEST_CASE("invasion technology and fixed research settings preserve 1.50 defaults") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(invasion::settings.technology_chance.building_player == 125);
    DOCTEST_CHECK(invasion::settings.technology_chance.building_ai == 115);
    DOCTEST_CHECK(invasion::settings.technology_chance.population_player == 65);
    DOCTEST_CHECK(invasion::settings.technology_chance.population_ai == 55);
    DOCTEST_CHECK(COLCALC::fixed_research_cost == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat invasion technology_chance building_player = 500;"
        "combat invasion technology_chance building_ai = 120;"
        "combat invasion technology_chance population_player = 250;"
        "combat invasion technology_chance population_ai = 60;"
        "research fixed_cost = true;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(invasion::settings.technology_chance.building_player == 500);
    DOCTEST_CHECK(invasion::settings.technology_chance.building_ai == 120);
    DOCTEST_CHECK(invasion::settings.technology_chance.population_player == 250);
    DOCTEST_CHECK(invasion::settings.technology_chance.population_ai == 60);
    DOCTEST_CHECK(COLCALC::fixed_research_cost == 1);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat invasion technology_chance building_ai = 256;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat invasion technology_chance population_player = 65536;",
        &writes,
        &diagnostic));

    const uint8_t ai_weights[] = {127, 128, 255};
    for (uint8_t weight : ai_weights) {
        DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
        writes.writes.clear();
        const std::string source =
            "combat invasion technology_chance building_ai = " +
            std::to_string(weight) + ";" +
            "combat invasion technology_chance population_ai = " +
            std::to_string(weight) + ";";
        DOCTEST_REQUIRE(Append_Decoded_(
            source.c_str(), &writes, &diagnostic));
        DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
        DOCTEST_REQUIRE(config::Commit_State(&state));
        DOCTEST_CHECK(invasion::settings.technology_chance.building_ai == weight);
        DOCTEST_CHECK(invasion::settings.technology_chance.population_ai == weight);
    }
}

DOCTEST_TEST_CASE("spy framing, starting ships, initial tech, and crew XP settings bind canonically") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK_FALSE(spy::frame_anyone);
    DOCTEST_CHECK(ERICNET::starting_ships == 1);
    DOCTEST_CHECK(REPORT::initial_tech_prompt == 1);
    DOCTEST_CHECK(ERICNET::crew_gains_turn_xp == 1);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "spy frame_anyone = true;"
        "game starting_ships = false;"
        "interface initial_tech_prompt = false;"
        "combat crew_gains_turn_xp = false;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(spy::frame_anyone);
    DOCTEST_CHECK(ERICNET::starting_ships == 0);
    DOCTEST_CHECK(REPORT::initial_tech_prompt == 0);
    DOCTEST_CHECK(ERICNET::crew_gains_turn_xp == 0);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_("spy frame_anyone = enabled;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_("game starting_ships = 2;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_("interface initial_tech_prompt = 1;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_("combat crew_gains_turn_xp = disabled;", &writes, &diagnostic));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK_FALSE(spy::frame_anyone);
    DOCTEST_CHECK(ERICNET::starting_ships == 1);
    DOCTEST_CHECK(REPORT::initial_tech_prompt == 1);
    DOCTEST_CHECK(ERICNET::crew_gains_turn_xp == 1);
}

DOCTEST_TEST_CASE("Subterranean population bonuses bind by planet size") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    const uint8_t expected[PLANET_SIZE_COUNT] = {2, 4, 6, 8, 10};
    DOCTEST_CHECK(std::memcmp(
                      COLCALC::pop_bonus_subterranean, expected, sizeof(expected)) == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "colony pop_bonus_subterranean tiny = 3;"
        "colony pop_bonus_subterranean medium = 9;"
        "colony pop_bonus_subterranean huge = 12;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(COLCALC::pop_bonus_subterranean[PLANET_SIZE_TINY] == 3);
    DOCTEST_CHECK(COLCALC::pop_bonus_subterranean[PLANET_SIZE_MEDIUM] == 9);
    DOCTEST_CHECK(COLCALC::pop_bonus_subterranean[PLANET_SIZE_HUGE] == 12);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "colony pop_bonus_subterranean small = 256;",
        &writes,
        &diagnostic));
}

DOCTEST_TEST_CASE("Trade Goods ratios preserve 1.50 defaults and bind independently") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COLCALC::trade_goods_ratio.normal == 50);
    DOCTEST_CHECK(COLCALC::trade_goods_ratio.fantastic_traders == 100);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "colony trade_goods_ratio normal = 40;"
        "colony trade_goods_ratio fantastic_traders = 175;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COLCALC::trade_goods_ratio.normal == 40);
    DOCTEST_CHECK(COLCALC::trade_goods_ratio.fantastic_traders == 175);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "colony trade_goods_ratio normal = 65536;",
        &writes,
        &diagnostic));
}

DOCTEST_TEST_CASE("colony BC bonuses preserve 1.50 defaults and bind independently") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COLCALC::bc_bonus.spaceport == 50);
    DOCTEST_CHECK(COLCALC::bc_bonus.planetary_stock_exchange == 100);
    DOCTEST_CHECK(COLCALC::bc_bonus.galactic_currency_exchange == 50);
    DOCTEST_CHECK(COLCALC::planet_bc_bonus.gold_deposits == 5);
    DOCTEST_CHECK(COLCALC::planet_bc_bonus.gem_deposits == 10);

    const uint8_t expected_maintenance[PLANET_CLIMATE_COUNT] = {
        50,
        25,
        0,
        25,
        0,
        0,
        0,
        0,
        0,
        0,
    };
    DOCTEST_CHECK(std::memcmp(COLCALC::climate_maintenance_modifiers,
                      expected_maintenance,
                      sizeof(expected_maintenance)) == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "colony bc_bonus spaceport = 75;"
        "colony bc_bonus planetary_stock_exchange = 125;"
        "colony bc_bonus galactic_currency_exchange = 40;"
        "colony planet_bc_bonus gold_deposits = 7;"
        "colony planet_bc_bonus gem_deposits = 13;"
        "colony planet_maintenance toxic = 0;"
        "colony planet_maintenance radiated = 127;"
        "colony planet_maintenance barren = 128;"
        "colony planet_maintenance desert = 255;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COLCALC::bc_bonus.spaceport == 75);
    DOCTEST_CHECK(COLCALC::bc_bonus.planetary_stock_exchange == 125);
    DOCTEST_CHECK(COLCALC::bc_bonus.galactic_currency_exchange == 40);
    DOCTEST_CHECK(COLCALC::planet_bc_bonus.gold_deposits == 7);
    DOCTEST_CHECK(COLCALC::planet_bc_bonus.gem_deposits == 13);
    DOCTEST_CHECK(COLCALC::climate_maintenance_modifiers[PLANET_CLIMATE_TOXIC] == 0);
    DOCTEST_CHECK(COLCALC::climate_maintenance_modifiers[PLANET_CLIMATE_RADIATED] == 127);
    DOCTEST_CHECK(COLCALC::climate_maintenance_modifiers[PLANET_CLIMATE_BARREN] == 128);
    DOCTEST_CHECK(COLCALC::climate_maintenance_modifiers[PLANET_CLIMATE_DESERT] == 255);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "colony bc_bonus spaceport = 65536;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "colony bc_bonus planetary_stock_exchange = 65536;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "colony bc_bonus galactic_currency_exchange = 65536;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "colony planet_bc_bonus gold_deposits = 256;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "colony planet_maintenance toxic = -1;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "colony planet_maintenance toxic = 256;",
        &writes,
        &diagnostic));
}

DOCTEST_TEST_CASE("food settings preserve 1.50 half-food defaults and bind independently") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    const uint8_t expected_base[PLANET_CLIMATE_COUNT] = {
        0,
        0,
        0,
        2,
        2,
        4,
        4,
        2,
        4,
        6,
    };
    DOCTEST_CHECK(std::memcmp(MOX::food_per_farmer_table, expected_base, sizeof(expected_base)) == 0);
    DOCTEST_CHECK(COLCALC::food_colony_bonus.hydroponic_farms == 2);
    DOCTEST_CHECK(COLCALC::food_colony_bonus.subterranean_farms == 4);
    DOCTEST_CHECK(COLCALC::food_farmer_bonus.soil_enrichment == 2);
    DOCTEST_CHECK(COLCALC::food_farmer_bonus.biomorphic_fungi == 2);
    DOCTEST_CHECK(COLCALC::food_farmer_bonus.weather_controller == 4);
    DOCTEST_CHECK(COLCALC::food_farmer_bonus.astro_university == 2);
    DOCTEST_CHECK(COLCALC::food_farmer_bonus.native == 4);
    DOCTEST_CHECK(COLCALC::food_farmer_bonus.android_farmer == 6);
    DOCTEST_CHECK(COLCALC::fungi_climate == COLCALC::FUNGI_CLIMATE_CLASSIC);
    DOCTEST_CHECK(AIDATA::uncolonized_planet_food_worth.biological == 75);
    DOCTEST_CHECK(AIDATA::uncolonized_planet_food_worth.cybernetic == 38);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "colony food_colony_bonus hydroponic_farms = 3;"
        "colony food_colony_bonus subterranean_farms = 5;"
        "colony food_farmer_base desert = 127;"
        "colony food_farmer_base gaia = 128;"
        "colony food_farmer_base terran = 255;"
        "colony fungi_climate = united;"
        "colony food_farmer_bonus soil_enrichment = 1;"
        "colony food_farmer_bonus biomorphic_fungi = 3;"
        "colony food_farmer_bonus weather_controller = 5;"
        "colony food_farmer_bonus astro_university = 3;"
        "colony food_farmer_bonus native = 5;"
        "colony food_farmer_bonus android_farmer = 7;"
        "ai uncolonized_planet_food_worth biological = 80;"
        "ai uncolonized_planet_food_worth cybernetic = 40;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(COLCALC::food_colony_bonus.hydroponic_farms == 3);
    DOCTEST_CHECK(COLCALC::food_colony_bonus.subterranean_farms == 5);
    DOCTEST_CHECK(MOX::food_per_farmer_table[PLANET_CLIMATE_DESERT] == 127);
    DOCTEST_CHECK(MOX::food_per_farmer_table[PLANET_CLIMATE_GAIA] == 128);
    DOCTEST_CHECK(MOX::food_per_farmer_table[PLANET_CLIMATE_TERRAN] == 255);
    DOCTEST_CHECK(COLCALC::fungi_climate == COLCALC::FUNGI_CLIMATE_UNITED);
    DOCTEST_CHECK(COLCALC::food_farmer_bonus.soil_enrichment == 1);
    DOCTEST_CHECK(COLCALC::food_farmer_bonus.biomorphic_fungi == 3);
    DOCTEST_CHECK(COLCALC::food_farmer_bonus.weather_controller == 5);
    DOCTEST_CHECK(COLCALC::food_farmer_bonus.astro_university == 3);
    DOCTEST_CHECK(COLCALC::food_farmer_bonus.native == 5);
    DOCTEST_CHECK(COLCALC::food_farmer_bonus.android_farmer == 7);
    DOCTEST_CHECK(AIDATA::uncolonized_planet_food_worth.biological == 80);
    DOCTEST_CHECK(AIDATA::uncolonized_planet_food_worth.cybernetic == 40);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "colony food_farmer_base desert = -1;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "colony food_farmer_base desert = 256;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "colony fungi_climate = separate;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "ai uncolonized_planet_food_worth biological = 65536;",
        &writes,
        &diagnostic));
}

DOCTEST_TEST_CASE("Android morale policy preserves the classic default and named modes") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COLCALC::android_morale == COLCALC::ANDROID_MORALE_WORKERS_ONLY);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "colony android_morale = farmers_and_scientists;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COLCALC::android_morale ==
        COLCALC::ANDROID_MORALE_FARMERS_AND_SCIENTISTS);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "colony android_morale = all_androids;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "colony android_morale = 8;", &writes, &diagnostic));
}

DOCTEST_TEST_CASE("colony morale bonuses preserve 1.50 defaults and bind independently") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COLCALC::morale_bonus.holo_simulator == 4);
    DOCTEST_CHECK(COLCALC::morale_bonus.pleasure_dome == 6);
    DOCTEST_CHECK(COLCALC::morale_bonus.psionics == 2);
    DOCTEST_CHECK(COLCALC::morale_bonus.virtual_reality_network == 4);
    const int8_t barracks_bonus[GOVERNMENT_COUNT] = {4, 4, 4, 4, 0, 0, 0, 0};
    DOCTEST_CHECK(std::memcmp(COLCALC::barracks_morale_lookup, barracks_bonus, sizeof(barracks_bonus)) == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "colony morale_bonus holo_simulator = 7;"
        "colony morale_bonus pleasure_dome = 8;"
        "colony morale_bonus psionics = 9;"
        "colony morale_bonus virtual_reality_network = 10;"
        "colony barracks_morale_bonus feudal = 6;"
        "colony barracks_morale_bonus federation = -2;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COLCALC::morale_bonus.holo_simulator == 7);
    DOCTEST_CHECK(COLCALC::morale_bonus.pleasure_dome == 8);
    DOCTEST_CHECK(COLCALC::morale_bonus.psionics == 9);
    DOCTEST_CHECK(COLCALC::morale_bonus.virtual_reality_network == 10);
    DOCTEST_CHECK(COLCALC::barracks_morale_lookup[GOVERNMENT_FEUDAL] == 6);
    DOCTEST_CHECK(COLCALC::barracks_morale_lookup[GOVERNMENT_FEDERATION] == -2);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "colony morale_bonus holo_simulator = 256;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "colony barracks_morale_bonus feudal = 128;", &writes, &diagnostic));
}

DOCTEST_TEST_CASE("production settings preserve classic defaults and bind independently") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    const uint8_t expected_worker_base[MINERAL_RESOURCE_COUNT] = {
        1,
        2,
        3,
        5,
        8,
    };
    const uint8_t expected_robotic_factory[MINERAL_RESOURCE_COUNT] = {
        5,
        8,
        10,
        15,
        20,
    };
    DOCTEST_CHECK(std::memcmp(COLCALC::minerals_per_mine,
                      expected_worker_base,
                      sizeof(expected_worker_base)) == 0);
    DOCTEST_CHECK(COLCALC::production_worker_bonus.automated_factory == 1);
    DOCTEST_CHECK(COLCALC::production_worker_bonus.robo_miner_plant == 2);
    DOCTEST_CHECK(COLCALC::production_worker_bonus.deep_core_mine == 3);
    DOCTEST_CHECK(COLCALC::production_worker_bonus.recyclotron == 1);
    DOCTEST_CHECK(COLCALC::production_worker_bonus.astro_university == 1);
    DOCTEST_CHECK(COLCALC::production_worker_bonus.android_worker == 3);
    DOCTEST_CHECK(
        COLCALC::production_worker_bonus.microlite_construction == 1);
    DOCTEST_CHECK(COLCALC::production_colony_bonus.automated_factory == 5);
    DOCTEST_CHECK(COLCALC::production_colony_bonus.robo_miners == 10);
    DOCTEST_CHECK(COLCALC::production_colony_bonus.deep_core_mines == 15);
    DOCTEST_CHECK(std::memcmp(COLCALC::production_colony_bonus.robotic_factory,
                      expected_robotic_factory,
                      sizeof(expected_robotic_factory)) == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "colony production_worker_base ultra_poor = 0;"
        "colony production_worker_base poor = 127;"
        "colony production_worker_base abundant = 128;"
        "colony production_worker_base rich = 255;"
        "colony production_worker_bonus automated_factory = -4;"
        "colony production_worker_bonus recyclotron = 5;"
        "colony production_worker_bonus android_worker = 7;"
        "colony production_worker_bonus microlite_construction = 6;"
        "colony production_colony_bonus automated_factory = 12;"
        "colony production_colony_bonus robotic_factory rich = 21;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(COLCALC::minerals_per_mine[MINERAL_RESOURCE_ULTRA_POOR] == 0);
    DOCTEST_CHECK(COLCALC::minerals_per_mine[MINERAL_RESOURCE_POOR] == 127);
    DOCTEST_CHECK(COLCALC::minerals_per_mine[MINERAL_RESOURCE_ABUNDANT] == 128);
    DOCTEST_CHECK(COLCALC::minerals_per_mine[MINERAL_RESOURCE_RICH] == 255);
    DOCTEST_CHECK(COLCALC::production_worker_bonus.automated_factory == -4);
    DOCTEST_CHECK(COLCALC::production_worker_bonus.recyclotron == 5);
    DOCTEST_CHECK(COLCALC::production_worker_bonus.android_worker == 7);
    DOCTEST_CHECK(
        COLCALC::production_worker_bonus.microlite_construction == 6);
    DOCTEST_CHECK(COLCALC::production_colony_bonus.automated_factory == 12);
    DOCTEST_CHECK(COLCALC::production_colony_bonus.robotic_factory[MINERAL_RESOURCE_RICH] == 21);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "colony production_worker_bonus automated_factory = 128;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "colony production_worker_bonus automated_factory = -129;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "colony production_worker_base abundant = 256;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "colony production_colony_bonus robotic_factory abundant = 256;",
        &writes,
        &diagnostic));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
}

DOCTEST_TEST_CASE("Food Replicator costs preserve 1.50 defaults and validate their range") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COLCALC::replicator_food_pp == 2);
    DOCTEST_CHECK(COLCALC::replicator_food_bc == 1);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "colony replicator_food_pp = 0;"
        "colony replicator_food_bc = 127;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COLCALC::replicator_food_pp == 0);
    DOCTEST_CHECK(COLCALC::replicator_food_bc == 127);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "colony replicator_food_pp = 128;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "colony replicator_food_bc = 128;", &writes, &diagnostic));
}

DOCTEST_TEST_CASE("special weapon damage display settings bind canonically") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(DESIGN::death_spore_list_damage == 0);
    DOCTEST_CHECK(DESIGN::bio_terminator_list_damage == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "interface death_spore_list_damage = true;"
        "interface bio_terminator_list_damage = true;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(DESIGN::death_spore_list_damage == 1);
    DOCTEST_CHECK(DESIGN::bio_terminator_list_damage == 1);

    const char* paths[] = {
        "interface.death_spore_list_damage",
        "interface.bio_terminator_list_damage",
    };
    for (const char* expected : paths) {
        const config::t_variable_id id = Variable_Id_(expected);
        DOCTEST_REQUIRE(id != config::CONFIG_NO_VARIABLE);
        std::string name;
        DOCTEST_REQUIRE(config::Get_Variable_Name(id, &name));
        DOCTEST_CHECK(name == expected);
    }
}

DOCTEST_TEST_CASE("event delays preserve 1.50 defaults and bind by event name") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(EVENTS::all_events_delay == 50);
    DOCTEST_CHECK(EVENTS::between_events_delay == 5);

    const uint16_t expected[EVENT_RANDOM_COUNT] = {
        0,
        0,
        200,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        100,
        200,
        300,
        150,
        250,
        200,
        0,
        0,
        0,
        0,
    };
    DOCTEST_CHECK(std::memcmp(EVENTS::event_delay, expected, sizeof(expected)) == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "events all_events_delay = 75;"
        "events between_events_delay = 7;"
        "events event_delay comet = 150;"
        "events event_delay space_amoeba = 75;"
        "events event_delay mineral_depletion = 50;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(EVENTS::all_events_delay == 75);
    DOCTEST_CHECK(EVENTS::between_events_delay == 7);
    DOCTEST_CHECK(EVENTS::event_delay[EVENT_COMET] == 150);
    DOCTEST_CHECK(EVENTS::event_delay[EVENT_SPACE_AMOEBA] == 75);
    DOCTEST_CHECK(EVENTS::event_delay[EVENT_MINERAL_DEPLETION] == 50);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "events event_delay comet = 65536;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "events all_events_delay = 256;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "events between_events_delay = 128;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "events event_delay overworking = 50;", &writes, &diagnostic));
}

DOCTEST_TEST_CASE("MENLO, discovery, Dragon, Donation, and Pirate Raid settings bind canonically") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(MAINSCR::menlo_cheat_points == 1000000);
    DOCTEST_CHECK(COMBFIND::system_discovery_bc.space_debris == 50);
    DOCTEST_CHECK(COMBFIND::system_discovery_bc.pirate_cache == 100);
    DOCTEST_CHECK(COMBFIND::dragon_bribe_bc == 500);
    DOCTEST_CHECK(EVENTS::donation_bc.turn_divisor == 20);
    DOCTEST_CHECK(EVENTS::donation_bc.multiplier == 100);
    DOCTEST_CHECK(EVENTS::donation_bc.base == 100);
    DOCTEST_CHECK(EVENTS::pirate_raid.random_percent == 21);
    DOCTEST_CHECK(EVENTS::pirate_raid.fixed_percent == 29);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "research menlo_cheat_points = 2000000;"
        "events system_discovery_bc space_debris = 60;"
        "events system_discovery_bc pirate_cache = 120;"
        "events dragon_bribe_bc = 750;"
        "events donation_bc = 25 80 125;"
        "events pirate_raid = 20 30;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(MAINSCR::menlo_cheat_points == 2000000);
    DOCTEST_CHECK(COMBFIND::system_discovery_bc.space_debris == 60);
    DOCTEST_CHECK(COMBFIND::system_discovery_bc.pirate_cache == 120);
    DOCTEST_CHECK(COMBFIND::dragon_bribe_bc == 750);
    DOCTEST_CHECK(EVENTS::donation_bc.turn_divisor == 25);
    DOCTEST_CHECK(EVENTS::donation_bc.multiplier == 80);
    DOCTEST_CHECK(EVENTS::donation_bc.base == 125);
    DOCTEST_CHECK(EVENTS::pirate_raid.random_percent == 20);
    DOCTEST_CHECK(EVENTS::pirate_raid.fixed_percent == 30);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "events donation_bc turn_divisor = 0;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "events system_discovery_bc space_debris = 128;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "research menlo_cheat_points = 2147483648;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "events dragon_bribe_bc = 2147483648;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "events pirate_raid random_percent = 256;", &writes, &diagnostic));
}

DOCTEST_TEST_CASE("Supernova, Computer Virus, and assimilation settings preserve classic defaults") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(EVENTS::supernova.minimum_research_per_turn == 0);
    DOCTEST_CHECK(EVENTS::computer_virus.random_research == 50);
    DOCTEST_CHECK(EVENTS::computer_virus.fixed_research == 50);
    const int8_t assimilation_points[GOVERNMENT_COUNT] = {30, 60, 30, 60, 60, 120, 12, 16};
    DOCTEST_CHECK(std::memcmp(COLCALC::assimilation_rate_by_government,
                      assimilation_points,
                      sizeof(assimilation_points)) == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "events supernova minimum_research_per_turn = 1000;"
        "events computer_virus random_research = 500;"
        "events computer_virus fixed_research = 200;"
        "colony assimilation_points democracy = 80;"
        "colony assimilation_points galactic_unification = -12;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(EVENTS::supernova.minimum_research_per_turn == 1000);
    DOCTEST_CHECK(EVENTS::computer_virus.random_research == 500);
    DOCTEST_CHECK(EVENTS::computer_virus.fixed_research == 200);
    DOCTEST_CHECK(COLCALC::assimilation_rate_by_government[GOVERNMENT_DEMOCRACY] == 80);
    DOCTEST_CHECK(COLCALC::assimilation_rate_by_government[GOVERNMENT_GALACTIC_UNIFICATION] == -12);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "events supernova minimum_research_per_turn = 65536;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "events computer_virus random_research = 65536;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "events computer_virus fixed_research = 256;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "colony assimilation_points feudal = 128;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "colony assimilation_points feudal = -129;", &writes, &diagnostic));
}

DOCTEST_TEST_CASE("event, shield, info, score, and mind-control settings bind canonically") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(EVENTS::event_roll.all_events == 512);
    DOCTEST_CHECK(EVENTS::event_roll.lucky_player == 1000);
    DOCTEST_CHECK(COMBINIT::shield_recharge_rate.standard == 3);
    DOCTEST_CHECK(COMBINIT::shield_recharge_rate.capacitor == 10);
    DOCTEST_CHECK(INFO::info_menu_layout == INFO::INFO_MENU_LAYOUT_CLASSIC);
    DOCTEST_CHECK(SCORE::victory_bonus.orion == 100);
    DOCTEST_CHECK(SCORE::victory_bonus.antares == 250);
    DOCTEST_CHECK(SCORE::victory_bonus.council == 100);
    DOCTEST_CHECK(ERIC::mind_control_ship_class == SHIP_SIZE_LARGE);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "events roll all_events = 256;"
        "events roll lucky_player = 768;"
        "combat shield_recharge_rate standard = 4;"
        "combat shield_recharge_rate capacitor = 12;"
        "interface info_menu_layout = ice;"
        "score victory_bonus orion = 150;"
        "score victory_bonus antares = 300;"
        "score victory_bonus council = 125;"
        "combat mind_control_ship_class = destroyer;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(EVENTS::event_roll.all_events == 256);
    DOCTEST_CHECK(EVENTS::event_roll.lucky_player == 768);
    DOCTEST_CHECK(COMBINIT::shield_recharge_rate.standard == 4);
    DOCTEST_CHECK(COMBINIT::shield_recharge_rate.capacitor == 12);
    DOCTEST_CHECK(INFO::info_menu_layout == INFO::INFO_MENU_LAYOUT_ICE);
    DOCTEST_CHECK(SCORE::victory_bonus.orion == 150);
    DOCTEST_CHECK(SCORE::victory_bonus.antares == 300);
    DOCTEST_CHECK(SCORE::victory_bonus.council == 125);
    DOCTEST_CHECK(ERIC::mind_control_ship_class == SHIP_SIZE_MEDIUM);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "events roll all_events = 2147483648;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat shield_recharge_rate standard = 2147483648;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "interface info_menu_layout = alternate;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "score victory_bonus council = 32768;", &writes, &diagnostic));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
}

DOCTEST_TEST_CASE("event duration settings preserve 1.50 defaults and bounds") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(EVENTS::duration.anomaly_min == 4);
    DOCTEST_CHECK(EVENTS::duration.beast_min == 4);
    DOCTEST_CHECK(EVENTS::duration.flux_min == 4);
    DOCTEST_CHECK(EVENTS::duration.anomaly_chance == 20);
    DOCTEST_CHECK(EVENTS::duration.beast_chance == 20);
    DOCTEST_CHECK(EVENTS::duration.flux_chance == 20);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "events duration anomaly_min = 5;"
        "events duration beast_min = 6;"
        "events duration flux_min = 7;"
        "events duration anomaly_chance = 21;"
        "events duration beast_chance = 22;"
        "events duration flux_chance = 23;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(EVENTS::duration.anomaly_min == 5);
    DOCTEST_CHECK(EVENTS::duration.beast_min == 6);
    DOCTEST_CHECK(EVENTS::duration.flux_min == 7);
    DOCTEST_CHECK(EVENTS::duration.anomaly_chance == 21);
    DOCTEST_CHECK(EVENTS::duration.beast_chance == 22);
    DOCTEST_CHECK(EVENTS::duration.flux_chance == 23);

    writes.writes.clear();
    DOCTEST_REQUIRE(Append_Decoded_(
        "events duration anomaly_chance = 0;"
        "events duration beast_chance = 0;"
        "events duration flux_chance = 0;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(EVENTS::duration.anomaly_chance == 0);
    DOCTEST_CHECK(EVENTS::duration.beast_chance == 0);
    DOCTEST_CHECK(EVENTS::duration.flux_chance == 0);
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "events duration beast_min = 256;", &writes, &diagnostic));
}

DOCTEST_TEST_CASE("Orion reward technology preserves the classic default and bounds") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(OFFICER::orion_reward_technology.application == TECH_APP_DEATH_RAY);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "leaders orion_reward_technology application = plasma_cannon;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(OFFICER::orion_reward_technology.application == TECH_APP_PLASMA_CANNON);

    writes.writes.clear();
    DOCTEST_REQUIRE(Append_Decoded_(
        "leaders orion_reward_technology application = no_tech;",
        &writes,
        &diagnostic));
    DOCTEST_CHECK_FALSE(config::Apply_Write_Set(&writes, &state, &diagnostic));
}

DOCTEST_TEST_CASE("Loknar and multiplayer End of Turn Wait settings preserve classic defaults") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK_FALSE(OFFICER::loknar_gives_no_tech);
    DOCTEST_CHECK_FALSE(LOADSAVE::keep_end_of_turn_wait_in_multiplayer);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "leaders loknar_gives_no_tech = true;"
        "interface keep_end_of_turn_wait_in_multiplayer = true;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(OFFICER::loknar_gives_no_tech);
    DOCTEST_CHECK(LOADSAVE::keep_end_of_turn_wait_in_multiplayer);
}

DOCTEST_TEST_CASE("forced event selector preserves the 1.50 one-based values") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(EVENTS::force_event_type == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "events force_event_type = space_amoeba;", &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(EVENTS::force_event_type == EVENT_SPACE_AMOEBA + 1);
}

DOCTEST_TEST_CASE("wandering monster lifecycle settings preserve 1.50 defaults") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK_FALSE(COMBFIND::monsters_stay);
    DOCTEST_CHECK(COMBFIND::monsters_stay_chance == 2);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "ai monsters_stay = true;"
        "ai monsters_stay_chance = 255;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COMBFIND::monsters_stay);
    DOCTEST_CHECK(COMBFIND::monsters_stay_chance == 255);

    writes.writes.clear();
    DOCTEST_REQUIRE(Append_Decoded_(
        "ai monsters_stay_chance = 0;", &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COMBFIND::monsters_stay_chance == 0);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "ai monsters_stay_chance = 256;", &writes, &diagnostic));
}

DOCTEST_TEST_CASE("satellite capture setting preserves the classic default") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COMBINIT::satellite_capture_enabled == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat satellite_capture = true;", &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COMBINIT::satellite_capture_enabled == 1);
}

DOCTEST_TEST_CASE("orbital-base construction threshold preserves the classic Cruiser default") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COLCALC::minimum_orbital_base_ship_class == SHIP_SIZE_LARGE);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "colony minimum_orbital_base_ship_class = battleship;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COLCALC::minimum_orbital_base_ship_class == SHIP_SIZE_HUGE);

    writes.writes.clear();
    DOCTEST_REQUIRE(Append_Decoded_(
        "colony minimum_orbital_base_ship_class = none;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COLCALC::minimum_orbital_base_ship_class == SHIP_SIZE_COUNT);
}

DOCTEST_TEST_CASE("Reinforced Hull multipliers preserve classic orbital-defense values") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COMBINIT::reinforced_hull_multiplier.regular_satellites == 3);
    DOCTEST_CHECK(COMBINIT::reinforced_hull_multiplier.antaran_fortress == 3);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    const uint8_t multipliers[] = {0, 127, 128, 255};
    for (uint8_t multiplier : multipliers) {
        DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
        writes.writes.clear();
        const std::string source =
            "combat reinforced_hull_multiplier regular_satellites = " +
            std::to_string(multiplier) + ";"
                                         "combat reinforced_hull_multiplier antaran_fortress = " +
            std::to_string(multiplier) + ";";
        DOCTEST_REQUIRE(Append_Decoded_(
            source.c_str(), &writes, &diagnostic));
        DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
        DOCTEST_REQUIRE(config::Commit_State(&state));
        DOCTEST_CHECK(
            COMBINIT::reinforced_hull_multiplier.regular_satellites ==
            multiplier);
        DOCTEST_CHECK(
            COMBINIT::reinforced_hull_multiplier.antaran_fortress ==
            multiplier);
    }

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat reinforced_hull_multiplier regular_satellites = 256;",
        &writes,
        &diagnostic));
}

DOCTEST_TEST_CASE("Antaran Fortress armor multiplier preserves the unsigned byte domain") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COMBINIT::antaran_fortress_armor_multiplier == 3);

    const uint8_t multipliers[] = {0, 127, 128, 255};
    config::s_cfg_diagnostic diagnostic;
    for (uint8_t multiplier : multipliers) {
        DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
        config::s_config_write_set writes;
        const std::string source =
            "combat antaran_fortress_armor_multiplier = " +
            std::to_string(multiplier) + ";";
        DOCTEST_REQUIRE(Append_Decoded_(
            source.c_str(), &writes, &diagnostic));
        DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
        DOCTEST_REQUIRE(config::Commit_State(&state));
        DOCTEST_CHECK(COMBINIT::antaran_fortress_armor_multiplier ==
            multiplier);
    }

    config::s_config_write_set writes;
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat antaran_fortress_armor_multiplier = -1;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat antaran_fortress_armor_multiplier = 256;",
        &writes,
        &diagnostic));
}

DOCTEST_TEST_CASE("satellite special priorities bind as two-dimensional choices") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(COMBINIT::starbase_special[4][0] == SPECIAL_WIDE_AREA_JAMMER);
    DOCTEST_CHECK(COMBINIT::starbase_special[4][1] == SPECIAL_MULTI_WAVE_ECM_JAMMER);
    DOCTEST_CHECK(COMBINIT::starbase_special[4][2] == SPECIAL_ECM_JAMMER);
    DOCTEST_CHECK(COMBINIT::starbase_special[5][0] == SPECIAL_WARP_DISSIPATOR);
    DOCTEST_CHECK(COMBINIT::starbase_special[13][0] == SPECIAL_TRANSPORTERS);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat starbase_special 6 1 = reflection_field;"
        "combat starbase_special 6 2 = 19;"
        "combat starbase_special 14 1 = warp_dissipator;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(COMBINIT::starbase_special[5][0] == SPECIAL_REFLECTION_FIELD);
    DOCTEST_CHECK(COMBINIT::starbase_special[5][1] == SPECIAL_LIGHTNING_FIELD);
    DOCTEST_CHECK(COMBINIT::starbase_special[13][0] == SPECIAL_WARP_DISSIPATOR);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat starbase_special 15 1 = ecm_jammer;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat starbase_special 6 4 = ecm_jammer;",
        &writes,
        &diagnostic));
}

DOCTEST_TEST_CASE("planetary defense modification masks preserve 1.50 defaults") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    const uint16_t all_beam_mods =
        WEAPON_MOD_FLAG_HEAVY_MOUNT |
        WEAPON_MOD_FLAG_POINT_DEFENSE |
        WEAPON_MOD_FLAG_ARMOR_PIERCING |
        WEAPON_MOD_FLAG_CONTINUOUS |
        WEAPON_MOD_FLAG_NO_RANGE_DISSIPATION |
        WEAPON_MOD_FLAG_SHIELD_PIERCING |
        WEAPON_MOD_FLAG_AUTO_FIRE |
        WEAPON_MOD_FLAG_ENVELOPING;
    DOCTEST_CHECK(COMBINIT::defenses_allowed_mods.heavy == all_beam_mods);
    DOCTEST_CHECK(COMBINIT::defenses_allowed_mods.normal == all_beam_mods);
    DOCTEST_CHECK(COMBINIT::defenses_allowed_mods.pd ==
        (WEAPON_MOD_FLAG_HEAVY_MOUNT |
            WEAPON_MOD_FLAG_POINT_DEFENSE |
            WEAPON_MOD_FLAG_CONTINUOUS |
            WEAPON_MOD_FLAG_NO_RANGE_DISSIPATION |
            WEAPON_MOD_FLAG_AUTO_FIRE |
            WEAPON_MOD_FLAG_ENVELOPING));
    DOCTEST_CHECK(COMBINIT::defenses_allowed_mods.missile_torp ==
        (WEAPON_MOD_FLAG_NO_RANGE_DISSIPATION |
            WEAPON_MOD_FLAG_ENVELOPING |
            WEAPON_MOD_FLAG_ECCM |
            WEAPON_MOD_FLAG_HEAVILY_ARMORED |
            WEAPON_MOD_FLAG_FAST |
            WEAPON_MOD_FLAG_OVERLOADED));

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat defenses_allowed_mods heavy = hv,ap;"
        "combat defenses_allowed_mods normal = co,nr;"
        "combat defenses_allowed_mods pd = pd,af;"
        "combat defenses_allowed_mods missile_torp = eccm,arm,fst;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(COMBINIT::defenses_allowed_mods.heavy ==
        (WEAPON_MOD_FLAG_HEAVY_MOUNT |
            WEAPON_MOD_FLAG_ARMOR_PIERCING));
    DOCTEST_CHECK(COMBINIT::defenses_allowed_mods.normal ==
        (WEAPON_MOD_FLAG_CONTINUOUS |
            WEAPON_MOD_FLAG_NO_RANGE_DISSIPATION));
    DOCTEST_CHECK(COMBINIT::defenses_allowed_mods.pd ==
        (WEAPON_MOD_FLAG_POINT_DEFENSE |
            WEAPON_MOD_FLAG_AUTO_FIRE));
    DOCTEST_CHECK(COMBINIT::defenses_allowed_mods.missile_torp ==
        (WEAPON_MOD_FLAG_ECCM |
            WEAPON_MOD_FLAG_HEAVILY_ARMORED |
            WEAPON_MOD_FLAG_FAST));
}

DOCTEST_TEST_CASE("satellite space allocation preserves and replaces 1.50 defaults") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(COMBINIT::satellite_space_allocation.heavy == 26);
    DOCTEST_CHECK(COMBINIT::satellite_space_allocation.normal == 13);
    DOCTEST_CHECK(COMBINIT::satellite_space_allocation.pd == 12);
    DOCTEST_CHECK(COMBINIT::satellite_space_allocation.missile_torp == 49);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat satellite_space_allocation heavy = 40;"
        "combat satellite_space_allocation normal = 20;"
        "combat satellite_space_allocation pd = 10;"
        "combat satellite_space_allocation missile_torp = 30;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(COMBINIT::satellite_space_allocation.heavy == 40);
    DOCTEST_CHECK(COMBINIT::satellite_space_allocation.normal == 20);
    DOCTEST_CHECK(COMBINIT::satellite_space_allocation.pd == 10);
    DOCTEST_CHECK(COMBINIT::satellite_space_allocation.missile_torp == 30);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat satellite_space_allocation heavy = 65536;",
        &writes,
        &diagnostic));
}

DOCTEST_TEST_CASE("ground defense settings preserve and bind 1.50 defaults") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COMBINIT::ground_defense.missile_base_space == 300);
    DOCTEST_CHECK(COMBINIT::ground_defense.missile_base_ammo == 125);
    DOCTEST_CHECK(COMBINIT::ground_defense.garrison_slots == 2);
    DOCTEST_CHECK(COMBINIT::ground_defense.garrison_interceptors == 5);
    DOCTEST_CHECK(COMBINIT::ground_defense.garrison_bombers == 3);
    DOCTEST_CHECK(COMBINIT::ground_defense.garrison_heavy_fighters == 2);
    DOCTEST_CHECK(COMBINIT::ground_defense.garrison_ammo == 1);
    DOCTEST_CHECK(COMBINIT::ground_defense.garrison_turns_to_replace == 10);
    DOCTEST_CHECK(COMBINIT::ground_defense.battery_hv_space == 338);
    DOCTEST_CHECK(COMBINIT::ground_defense.battery_pd_space == 112);
    DOCTEST_CHECK(COMBINIT::ground_defense.stellar_amount == 1);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat ground_defense missile_base_space = 320;"
        "combat ground_defense missile_base_ammo = 130;"
        "combat ground_defense garrison_slots = 3;"
        "combat ground_defense garrison_interceptors = 6;"
        "combat ground_defense garrison_bombers = 4;"
        "combat ground_defense garrison_heavy_fighters = 3;"
        "combat ground_defense garrison_ammo = 2;"
        "combat ground_defense garrison_turns_to_replace = 8;"
        "combat ground_defense battery_hv_space = 350;"
        "combat ground_defense battery_pd_space = 120;"
        "combat ground_defense stellar_amount = 2;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COMBINIT::ground_defense.missile_base_space == 320);
    DOCTEST_CHECK(COMBINIT::ground_defense.garrison_slots == 3);
    DOCTEST_CHECK(COMBINIT::ground_defense.garrison_heavy_fighters == 3);
    DOCTEST_CHECK(COMBINIT::ground_defense.battery_pd_space == 120);
    DOCTEST_CHECK(COMBINIT::ground_defense.stellar_amount == 2);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat ground_defense garrison_slots = 0;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat ground_defense garrison_slots = 4;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat ground_defense garrison_turns_to_replace = 0;",
        &writes,
        &diagnostic));
}

DOCTEST_TEST_CASE("satellite torpedo setting preserves the classic default") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(INITSHIP::satellite_uses_torpedoes == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat satellite_uses_torpedoes = true;", &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(INITSHIP::satellite_uses_torpedoes == 1);

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
}

DOCTEST_TEST_CASE("Anti-Missile Rocket accuracy preserves 1.50 formula terms") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTSPEC::anti_missile_rocket_accuracy.base == 70);
    DOCTEST_CHECK(CMBTSPEC::anti_missile_rocket_accuracy.range_multiplier == 10);
    DOCTEST_CHECK(CMBTSPEC::anti_missile_rocket_accuracy.range_divisor == 3);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat anti_missile_rocket_accuracy base = 80;"
        "combat anti_missile_rocket_accuracy range_multiplier = 12;"
        "combat anti_missile_rocket_accuracy range_divisor = 4;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTSPEC::anti_missile_rocket_accuracy.base == 80);
    DOCTEST_CHECK(CMBTSPEC::anti_missile_rocket_accuracy.range_multiplier == 12);
    DOCTEST_CHECK(CMBTSPEC::anti_missile_rocket_accuracy.range_divisor == 4);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat anti_missile_rocket_accuracy range_multiplier = 128;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat anti_missile_rocket_accuracy range_divisor = 0;",
        &writes,
        &diagnostic));
}

DOCTEST_TEST_CASE("Crystal fleet retention preserves the classic default") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COMBINIT::crystal_keep == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat crystal_keep = true;", &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COMBINIT::crystal_keep == 1);
}

DOCTEST_TEST_CASE("Amoeba regeneration settings preserve classic defaults") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COMBINIT::amoeba_regeneration == 0);
    DOCTEST_CHECK(COMBINIT::repair_rate.cyber == 10);
    DOCTEST_CHECK(COMBINIT::repair_rate.aru == 20);
    DOCTEST_CHECK(COMBINIT::repair_rate.regen == 20);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat amoeba_regeneration = true;"
        "combat repair_rate cyber = 7;"
        "combat repair_rate aru = 11;"
        "combat repair_rate regen = 13;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COMBINIT::amoeba_regeneration == 1);
    DOCTEST_CHECK(COMBINIT::repair_rate.cyber == 7);
    DOCTEST_CHECK(COMBINIT::repair_rate.aru == 11);
    DOCTEST_CHECK(COMBINIT::repair_rate.regen == 13);

    writes.writes.clear();
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat repair_rate cyber = 0;"
        "combat repair_rate aru = 128;"
        "combat repair_rate regen = 255;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COMBINIT::repair_rate.cyber == 0);
    DOCTEST_CHECK(COMBINIT::repair_rate.aru == 128);
    DOCTEST_CHECK(COMBINIT::repair_rate.regen == 255);
}

DOCTEST_TEST_CASE("first colony population assignment preserves the classic default") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COLONIZE::first_pop_to_production == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "colony first_pop_to_production = true;", &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COLONIZE::first_pop_to_production == 1);

    const config::t_variable_id id =
        Variable_Id_("colony.first_pop_to_production");
    DOCTEST_REQUIRE(id != config::CONFIG_NO_VARIABLE);
    std::string name;
    DOCTEST_REQUIRE(config::Get_Variable_Name(id, &name));
    DOCTEST_CHECK(name == "colony.first_pop_to_production");
}

DOCTEST_TEST_CASE("Plasma Web dissipation preserves the classic default and named modes") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COMBINIT::plasma_web_dissipation ==
        COMBINIT::PLASMA_WEB_DISSIPATION_SUBTRACT_FIVE);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat plasma_web_dissipation = halve;", &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COMBINIT::plasma_web_dissipation ==
        COMBINIT::PLASMA_WEB_DISSIPATION_HALVE);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat plasma_web_dissipation = subtract_ten;", &writes, &diagnostic));
}

DOCTEST_TEST_CASE("Plasma Torpedo dissipation preserves the classic default") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTMIS::plasma_torpedo_dissipation == 5);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat plasma_torpedo_dissipation = 7;", &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTMIS::plasma_torpedo_dissipation == 7);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat plasma_torpedo_dissipation = 256;", &writes, &diagnostic));
}

DOCTEST_TEST_CASE("defensive-fire settings preserve and replace 1.50 defaults") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTFIR2::defensive_fire.initiative_enabled == 0);
    DOCTEST_CHECK(CMBTFIR2::defensive_fire.maximum_range == 8);
    DOCTEST_CHECK(CMBTFIR2::defensive_fire.alternate_enabled == 0);
    const uint8_t expected[8] = {100, 95, 90, 80, 70, 50, 30, 10};
    DOCTEST_CHECK(std::memcmp(CMBTFIR2::defensive_fire.alternate_chance,
                      expected,
                      sizeof(expected)) == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat defensive_fire = true;"
        "combat defensive_fire_max_range = 12;"
        "combat alternate_defensive_fire = true;"
        "combat alternate_defensive_fire_range 6 = 42;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTFIR2::defensive_fire.initiative_enabled == 1);
    DOCTEST_CHECK(CMBTFIR2::defensive_fire.maximum_range == 12);
    DOCTEST_CHECK(CMBTFIR2::defensive_fire.alternate_enabled == 1);
    DOCTEST_CHECK(CMBTFIR2::defensive_fire.alternate_chance[5] == 42);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat defensive_fire_max_range = 128;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat alternate_defensive_fire_range 1 = 128;",
        &writes,
        &diagnostic));
}

DOCTEST_TEST_CASE("combat re-engagement buildings bind by building name") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    const uint8_t expected[] = {
        BUILDING_ARTEMIS_SYSTEM_NET,
        BUILDING_BATTLESTATION,
        BUILDING_MISSILE_BASE,
        BUILDING_GROUND_BATTERIES,
        BUILDING_STAR_BASE,
        BUILDING_STAR_FORTRESS,
        BUILDING_STELLAR_CONVERTER,
        BUILDING_FIGHTER_GARRISON,
        BUILDING_NO_BUILDING,
    };
    DOCTEST_CHECK(std::memcmp(ERICNET::combat_reengage_buildings, expected, sizeof(expected)) == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat reengage_buildings = radiation_shield none;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(ERICNET::combat_reengage_buildings[0] == BUILDING_RADIATION_SHIELD);
    DOCTEST_CHECK(ERICNET::combat_reengage_buildings[1] == BUILDING_NO_BUILDING);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat reengage_buildings = unknown_building none;",
        &writes,
        &diagnostic));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
}

DOCTEST_TEST_CASE("Warlord combat settings preserve classic defaults") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COMBINIT::satellite_warlord_bonus == 0);
    DOCTEST_CHECK(COMBAT::strategic_xp_bonus == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat satellite_warlord_bonus = true;"
        "combat strategic_xp_bonus = true;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COMBINIT::satellite_warlord_bonus == 1);
    DOCTEST_CHECK(COMBAT::strategic_xp_bonus == 1);
}

DOCTEST_TEST_CASE("planetary Damper Field setting preserves the classic default") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COMBINIT::planet_damper_enabled == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat planet_damper = true;", &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COMBINIT::planet_damper_enabled == 1);
}

DOCTEST_TEST_CASE("planet extra specials preserve classic defaults and bind independently") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COMBINIT::planet.rangemaster == 0);
    DOCTEST_CHECK(COMBINIT::planet.achilles == 0);
    DOCTEST_CHECK(COMBINIT::planet.analyzer == 0);
    DOCTEST_CHECK(COMBINIT::planet.hef == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat planet rangemaster = true;"
        "combat planet achilles = true;"
        "combat planet analyzer = true;"
        "combat planet hef = true;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COMBINIT::planet.rangemaster == 1);
    DOCTEST_CHECK(COMBINIT::planet.achilles == 1);
    DOCTEST_CHECK(COMBINIT::planet.analyzer == 1);
    DOCTEST_CHECK(COMBINIT::planet.hef == 1);
}

DOCTEST_TEST_CASE("planetary biological shields and Lithovore Fungi preserve classic defaults") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK_FALSE(COMBINIT::planet.shields_block_bioweapons);
    DOCTEST_CHECK_FALSE(TECHINIT::lithovore_allows_biomorphic_fungi);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat planet shields_block_bioweapons = true;"
        "technology lithovore_allows_biomorphic_fungi = true;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COMBINIT::planet.shields_block_bioweapons);
    DOCTEST_CHECK(TECHINIT::lithovore_allows_biomorphic_fungi);
}

DOCTEST_TEST_CASE("satellite capture state applies once to the matching colony") {
    COMBINIT::s_satellite_capture_state state;
    COMBINIT::Record_Captured_Satellite_State(
        &state, 4, BUILDING_BATTLESTATION);

    DOCTEST_CHECK(COMBINIT::Take_Captured_Satellite_State(&state, 5) == -1);
    DOCTEST_CHECK(COMBINIT::Take_Captured_Satellite_State(&state, 4) ==
        BUILDING_BATTLESTATION);
    DOCTEST_CHECK(COMBINIT::Take_Captured_Satellite_State(&state, 4) == -1);
}

DOCTEST_TEST_CASE("satellite capture requires an attacker-owned prize") {
    COMBINIT::satellite_capture_enabled = 0;
    DOCTEST_CHECK_FALSE(COMBINIT::Satellite_Was_Captured_By_Attacker(1, 1, 1));

    COMBINIT::satellite_capture_enabled = 1;
    DOCTEST_CHECK(COMBINIT::Satellite_Was_Captured_By_Attacker(1, 1, 1));
    DOCTEST_CHECK_FALSE(COMBINIT::Satellite_Was_Captured_By_Attacker(0, 1, 1));
    DOCTEST_CHECK_FALSE(COMBINIT::Satellite_Was_Captured_By_Attacker(1, 0, 1));

    COMBINIT::satellite_capture_enabled = 0;
}

DOCTEST_TEST_CASE("black-hole boarding setting preserves the classic default") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(BOARDING::board_in_black_hole == 1);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat board_in_black_hole = false;", &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(BOARDING::board_in_black_hole == 0);
}

DOCTEST_TEST_CASE("satellite racial defense setting preserves the classic default") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTFIR2::satellite_racial_defense_bonus == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat satellite_racial_defense_bonus = true;", &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTFIR2::satellite_racial_defense_bonus == 1);
}

DOCTEST_TEST_CASE("Antaran Scout Lab and Galactic Lore setting preserves the classic default") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK_FALSE(CMBTFIR2::lab_and_lore_against_antarans);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat lab_and_lore_against_antarans = true;", &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTFIR2::lab_and_lore_against_antarans);
}

DOCTEST_TEST_CASE("teleporter and boarding ranges preserve 1.50 defaults") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(CMBTMOV1::round_teleporter == 0);
    DOCTEST_CHECK(CMBTMOV1::sub_space_teleporter_range == 20);
    DOCTEST_CHECK(BOARDING::board_range.normal == 3);
    DOCTEST_CHECK(BOARDING::board_range.transporters == 12);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat round_teleporter = true;"
        "combat sub_space_teleporter_range = 24;"
        "combat board_range normal = 4;"
        "combat board_range transporters = 15;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(CMBTMOV1::round_teleporter == 1);
    DOCTEST_CHECK(CMBTMOV1::sub_space_teleporter_range == 24);
    DOCTEST_CHECK(BOARDING::board_range.normal == 4);
    DOCTEST_CHECK(BOARDING::board_range.transporters == 15);
}

DOCTEST_TEST_CASE("Damper Field transporter survival preserves the classic divisor") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(BOARDING::boarding.damper_survival_divisor == 3);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat boarding damper_survival_divisor = 0;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(BOARDING::boarding.damper_survival_divisor == 0);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat boarding damper_survival_divisor = 128;",
        &writes,
        &diagnostic));
}

DOCTEST_TEST_CASE("raid item count preserves the 1.50 default and bounds") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(BOARDING::boarding.raid_damage_per_marine == 50);
    DOCTEST_CHECK(BOARDING::boarding.raid_items_per_marine == 2);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat boarding raid_damage_per_marine = 75;"
        "combat boarding raid_items_per_marine = 3;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(BOARDING::boarding.raid_damage_per_marine == 75);
    DOCTEST_CHECK(BOARDING::boarding.raid_items_per_marine == 3);

    const uint8_t item_counts[] = {0, 1, 2, 127};
    for (uint8_t item_count : item_counts) {
        DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
        writes.writes.clear();
        const std::string source =
            "combat boarding raid_items_per_marine = " +
            std::to_string(item_count) + ";";
        DOCTEST_REQUIRE(Append_Decoded_(
            source.c_str(), &writes, &diagnostic));
        DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
        DOCTEST_REQUIRE(config::Commit_State(&state));
        DOCTEST_CHECK(BOARDING::boarding.raid_items_per_marine == item_count);
    }

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat boarding raid_damage_per_marine = 256;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat boarding raid_items_per_marine = 128;",
        &writes,
        &diagnostic));
}

DOCTEST_TEST_CASE("boarding result auto-close preserves the classic default") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(BOARDING::boarding.auto_close_results == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat boarding auto_close_results = true;", &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(BOARDING::boarding.auto_close_results == 1);
}

DOCTEST_TEST_CASE("telepathic multiple capture setting preserves the classic default") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(BOARDING::telepathic_multiple_capture == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat telepathic_multiple_capture = true;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(BOARDING::telepathic_multiple_capture == 1);
}

DOCTEST_TEST_CASE("excess marine capture setting preserves the classic default") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(BOARDING::kill_excess_marines_after_capture == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat kill_excess_marines_after_capture = true;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(BOARDING::kill_excess_marines_after_capture == 1);
}

DOCTEST_TEST_CASE("interactive combat stat settings preserve classic defaults") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COMBAT1::interactive_combat_stats == 0);
    DOCTEST_CHECK(COMBAT1::continuous_bhg == 0);
    DOCTEST_CHECK(COMBAT1::non_ics_tractor_and_bhg_mode == 0);
    DOCTEST_CHECK(COMBAT1::phasing_cloak_decloak_defense == 0);
    DOCTEST_CHECK(COMBAT1::recloak_next_turn == 0);
    DOCTEST_CHECK(COMBAT1::rephase_cloak_next_turn == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat interactive_combat_stats = true;"
        "combat continuous_bhg = true;"
        "combat non_ics_tractor_and_bhg_mode = true;"
        "combat phasing_cloak_decloak_defense = true;"
        "combat recloak_next_turn = true;"
        "combat rephase_cloak_next_turn = true;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COMBAT1::interactive_combat_stats == 1);
    DOCTEST_CHECK(COMBAT1::continuous_bhg == 1);
    DOCTEST_CHECK(COMBAT1::non_ics_tractor_and_bhg_mode == 1);
    DOCTEST_CHECK(COMBAT1::phasing_cloak_decloak_defense == 1);
    DOCTEST_CHECK(COMBAT1::recloak_next_turn == 1);
    DOCTEST_CHECK(COMBAT1::rephase_cloak_next_turn == 1);
}

DOCTEST_TEST_CASE("ship initiative and wait cost preserve 1.50 defaults") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COMBAT1::ship_initiative == -1);
    DOCTEST_CHECK(COMBAT1::wait_movement_cost == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat ship_initiative = 2;"
        "combat wait_movement_cost = 65535;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(COMBAT1::ship_initiative == 2);
    DOCTEST_CHECK(COMBAT1::wait_movement_cost == 65535);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat ship_initiative = -2;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat ship_initiative = 3;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat wait_movement_cost = 65536;", &writes, &diagnostic));

    const int8_t modes[] = {-1, 0, 1, 2};
    for (int8_t mode : modes) {
        DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
        writes.writes.clear();
        const std::string source =
            "combat ship_initiative = " + std::to_string(mode) + ";";
        DOCTEST_REQUIRE(Append_Decoded_(
            source.c_str(), &writes, &diagnostic));
        DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
        DOCTEST_REQUIRE(config::Commit_State(&state));
        DOCTEST_CHECK(COMBAT1::ship_initiative == mode);
    }
}

DOCTEST_TEST_CASE("missile initiative preserves the classic delay by default") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTMIS::missile_initiative == -1);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat missile_initiative = 0;", &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTMIS::missile_initiative == 0);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat missile_initiative = -2;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat missile_initiative = 1;", &writes, &diagnostic));
}

DOCTEST_TEST_CASE("automatic ships without ammunition preserve classic retreat behavior") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTAI::ships_without_ammo_stay == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat ships_without_ammo_stay = true;", &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTAI::ships_without_ammo_stay == 1);
}

DOCTEST_TEST_CASE("human fleet automatic retreat preserves the classic default") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTAI::human_fleet_auto_retreat == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat human_fleet_auto_retreat = true;", &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTAI::human_fleet_auto_retreat == 1);
}

DOCTEST_TEST_CASE("combat bonus settings preserve 1.50 defaults") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTFIR2::combat_bonus.battle_scanner_beam_offense == 50);
    DOCTEST_CHECK(CMBTFIR2::combat_bonus.scout_lab_fleet_beam_offense == 20);
    DOCTEST_CHECK(CMBTFIR2::combat_bonus.starbase_fleet_beam_offense == 0);
    DOCTEST_CHECK(CMBTFIR2::combat_bonus.battlestation_fleet_beam_offense == 10);
    DOCTEST_CHECK(CMBTFIR2::combat_bonus.starfortress_fleet_beam_offense == 20);
    DOCTEST_CHECK(CMBTFIR2::combat_bonus.inertial_stabilizer_beam_defense == 50);
    DOCTEST_CHECK(CMBTFIR2::combat_bonus.inertial_nullifier_beam_defense == 100);
    DOCTEST_CHECK(CMBTFIR2::combat_bonus.stealth_field_beam_defense == 0);
    DOCTEST_CHECK(CMBTFIR2::combat_bonus.cloaking_device_beam_defense == 80);
    DOCTEST_CHECK(CMBTFIR2::combat_bonus.phasing_cloak_beam_defense == 80);
    DOCTEST_CHECK(CMBTFIR2::combat_bonus.phasing_cloak_effective_turns == 10);
    DOCTEST_CHECK(CMBTFIR2::combat_bonus.displacement_device_beam_miss_percent == 30);
    DOCTEST_CHECK(CMBTFIR2::combat_bonus.reflection_field_strength == 15);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat bonus battle_scanner_beam_offense = 37;"
        "combat bonus scout_lab_fleet_beam_offense = 21;"
        "combat bonus starbase_fleet_beam_offense = 17;"
        "combat bonus battlestation_fleet_beam_offense = 11;"
        "combat bonus starfortress_fleet_beam_offense = 22;"
        "combat bonus inertial_stabilizer_beam_defense = 51;"
        "combat bonus inertial_nullifier_beam_defense = 101;"
        "combat bonus stealth_field_beam_defense = 52;"
        "combat bonus cloaking_device_beam_defense = 81;"
        "combat bonus phasing_cloak_beam_defense = 82;"
        "combat bonus phasing_cloak_effective_turns = 12;"
        "combat bonus displacement_device_beam_miss_percent = 14;"
        "combat bonus reflection_field_strength = 16;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTFIR2::combat_bonus.battle_scanner_beam_offense == 37);
    DOCTEST_CHECK(CMBTFIR2::combat_bonus.scout_lab_fleet_beam_offense == 21);
    DOCTEST_CHECK(CMBTFIR2::combat_bonus.starbase_fleet_beam_offense == 17);
    DOCTEST_CHECK(CMBTFIR2::combat_bonus.battlestation_fleet_beam_offense == 11);
    DOCTEST_CHECK(CMBTFIR2::combat_bonus.starfortress_fleet_beam_offense == 22);
    DOCTEST_CHECK(CMBTFIR2::combat_bonus.inertial_stabilizer_beam_defense == 51);
    DOCTEST_CHECK(CMBTFIR2::combat_bonus.inertial_nullifier_beam_defense == 101);
    DOCTEST_CHECK(CMBTFIR2::combat_bonus.stealth_field_beam_defense == 52);
    DOCTEST_CHECK(CMBTFIR2::combat_bonus.cloaking_device_beam_defense == 81);
    DOCTEST_CHECK(CMBTFIR2::combat_bonus.phasing_cloak_beam_defense == 82);
    DOCTEST_CHECK(CMBTFIR2::combat_bonus.phasing_cloak_effective_turns == 12);
    DOCTEST_CHECK(CMBTFIR2::combat_bonus.displacement_device_beam_miss_percent == 14);
    DOCTEST_CHECK(CMBTFIR2::combat_bonus.reflection_field_strength == 16);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat bonus starbase_fleet_beam_offense = 256;",
        &writes,
        &diagnostic));
}

DOCTEST_TEST_CASE("fighter sortie shots preserve 1.50 defaults and bounds") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTMIS::fighter_shots.interceptor == 4);
    DOCTEST_CHECK(CMBTMIS::fighter_shots.bomber == 1);
    DOCTEST_CHECK(CMBTMIS::fighter_shots.heavy_fighter == 2);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat fighter_shots interceptor = 6;"
        "combat fighter_shots bomber = 3;"
        "combat fighter_shots heavy_fighter = 5;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTMIS::fighter_shots.interceptor == 6);
    DOCTEST_CHECK(CMBTMIS::fighter_shots.bomber == 3);
    DOCTEST_CHECK(CMBTMIS::fighter_shots.heavy_fighter == 5);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat fighter_shots bomber = 128;", &writes, &diagnostic));
}

DOCTEST_TEST_CASE("fighter pursuit settings preserve defaults and bind horizon and fallback") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTMIS::fighter_max_pursuit_turns == 3);
    DOCTEST_CHECK(CMBTMIS::fighter_pursuit_fallback == CMBTMIS::FIGHTER_PURSUIT_PURSUE);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat smarter_fighter_retargeting = true;"
        "combat fighter_max_pursuit_turns = 0;"
        "combat fighter_pursuit_fallback = pursue;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTMIS::smarter_fighter_retargeting);
    DOCTEST_CHECK(CMBTMIS::fighter_max_pursuit_turns == 0);
    DOCTEST_CHECK(CMBTMIS::fighter_pursuit_fallback == CMBTMIS::FIGHTER_PURSUIT_PURSUE);

    writes.writes.clear();
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat fighter_max_pursuit_turns = 65535;"
        "combat fighter_pursuit_fallback = return;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTMIS::fighter_max_pursuit_turns == UINT16_MAX);
    DOCTEST_CHECK(CMBTMIS::fighter_pursuit_fallback == CMBTMIS::FIGHTER_PURSUIT_RETURN);

    for (const char* source : {
             "combat fighter_max_pursuit_turns = -1;",
             "combat fighter_max_pursuit_turns = 65536;",
             "combat fighter_pursuit_fallback = unknown;"}) {
        writes.writes.clear();
        DOCTEST_CHECK_FALSE(Append_Decoded_(source, &writes, &diagnostic));
    }

    for (const char* path : {
             "combat.fighter_max_pursuit_turns",
             "combat.fighter_pursuit_fallback"}) {
        const config::t_variable_id variable = Variable_Id_(path);
        DOCTEST_REQUIRE(variable != config::CONFIG_NO_VARIABLE);
        const config::s_config_group_descriptor* group =
            config::Get_Config_Group(config::Get_Variable_Group(variable));
        DOCTEST_REQUIRE(group != nullptr);
        DOCTEST_CHECK(group->save);
        DOCTEST_CHECK(group->synchronize);
    }

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
}

DOCTEST_TEST_CASE("crew quality data preserves native defaults and binds signed bonuses") {
    const s_crew_data defaults[CREW_QUALITY_COUNT] = {
        {0, 0, 0, 0},
        {50, 15, 15, 5},
        {150, 30, 30, 10},
        {500, 50, 50, 15},
        {1000, 75, 75, 20},
    };
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(std::memcmp(MOX::crew_data, defaults, sizeof(defaults)) == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat crew green experience = 0;"
        "combat crew regular experience = 32767;"
        "combat crew veteran experience = 32768;"
        "combat crew elite experience = 65534;"
        "combat crew ultra_elite experience = 65535;"
        "combat crew veteran attack = -5;"
        "combat crew veteran defense = -4;"
        "combat crew veteran ground = -3;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(MOX::crew_data[CREW_QUALITY_GREEN].experience == 0);
    DOCTEST_CHECK(MOX::crew_data[CREW_QUALITY_REGULAR].experience == 32767);
    DOCTEST_CHECK(MOX::crew_data[CREW_QUALITY_VETERAN].experience == 32768);
    DOCTEST_CHECK(MOX::crew_data[CREW_QUALITY_ELITE].experience == 65534);
    DOCTEST_CHECK(MOX::crew_data[CREW_QUALITY_ULTRA_ELITE].experience == 65535);
    DOCTEST_CHECK(MOX::crew_data[CREW_QUALITY_VETERAN].attack == -5);
    DOCTEST_CHECK(MOX::crew_data[CREW_QUALITY_VETERAN].defense == -4);
    DOCTEST_CHECK(MOX::crew_data[CREW_QUALITY_VETERAN].ground == -3);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat crew green experience = 65536;", &writes, &diagnostic));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    writes.writes.clear();
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat crew ultra_elite experience = 500;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_CHECK_FALSE(config::Commit_State(&state));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(std::memcmp(MOX::crew_data, defaults, sizeof(defaults)) == 0);
}

DOCTEST_TEST_CASE("ship-design rebuild policy binds by technology field") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(DESIGN::rebuild_ship_designs_after_field[TECH_FIELD_CHEMISTRY]);
    DOCTEST_CHECK(DESIGN::rebuild_ship_designs_after_field[TECH_FIELD_NUCLEAR_FISSION]);
    DOCTEST_CHECK(DESIGN::rebuild_ship_designs_after_field[TECH_FIELD_PHYSICS]);
    DOCTEST_CHECK_FALSE(DESIGN::rebuild_ship_designs_after_field[TECH_FIELD_COLD_FUSION]);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "design rebuild_ship_designs_after_field physics = false;"
        "design rebuild_ship_designs_after_field cold_fusion = true;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(DESIGN::rebuild_ship_designs_after_field[TECH_FIELD_CHEMISTRY]);
    DOCTEST_CHECK(DESIGN::rebuild_ship_designs_after_field[TECH_FIELD_NUCLEAR_FISSION]);
    DOCTEST_CHECK_FALSE(DESIGN::rebuild_ship_designs_after_field[TECH_FIELD_PHYSICS]);
    DOCTEST_CHECK(DESIGN::rebuild_ship_designs_after_field[TECH_FIELD_COLD_FUSION]);
}

DOCTEST_TEST_CASE("Artemis System Net preserves and binds tactical settings") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTFIR2::artemis_system_net.random_total_damage == 401);
    DOCTEST_CHECK(CMBTFIR2::artemis_system_net.minimum_total_damage == 149);
    DOCTEST_CHECK(CMBTFIR2::artemis_system_net.counter == 20);
    DOCTEST_CHECK(CMBTFIR2::artemis_system_net.single_hit_damage == 20);
    DOCTEST_CHECK(CMBTFIR2::artemis_system_net.inherent_modifications == 0);
    const uint8_t chances[SHIP_SIZE_COUNT] = {20, 30, 40, 50, 80, 100};
    DOCTEST_CHECK(std::memcmp(CMBTFIR2::artemis_system_net.trigger_chance,
                      chances,
                      sizeof(chances)) == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat artemis_system_net random_total_damage = 501;"
        "combat artemis_system_net minimum_total_damage = 199;"
        "combat artemis_system_net counter = 25;"
        "combat artemis_system_net single_hit_damage = 30;"
        "combat artemis_system_net inherent_modifications = extra_structural_damage;"
        "combat artemis_system_net trigger_chance frigate = 25;"
        "combat artemis_system_net trigger_chance doomstar = 95;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTFIR2::artemis_system_net.random_total_damage == 501);
    DOCTEST_CHECK(CMBTFIR2::artemis_system_net.minimum_total_damage == 199);
    DOCTEST_CHECK(CMBTFIR2::artemis_system_net.counter == 25);
    DOCTEST_CHECK(CMBTFIR2::artemis_system_net.single_hit_damage == 30);
    DOCTEST_CHECK(CMBTFIR2::artemis_system_net.inherent_modifications ==
        WEAPON_NATURAL_FLAG_EXTRA_STRUCTURAL_DAMAGE);
    DOCTEST_CHECK(CMBTFIR2::artemis_system_net.trigger_chance[SHIP_SIZE_SMALL] == 25);
    DOCTEST_CHECK(CMBTFIR2::artemis_system_net.trigger_chance[SHIP_SIZE_DOOM_STAR] == 95);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat artemis_system_net counter = 0;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat artemis_system_net counter = 128;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat artemis_system_net single_hit_damage = 256;",
        &writes,
        &diagnostic));

    const uint8_t inherent_values[] = {1, 127, 128, 255};
    for (uint8_t inherent_value : inherent_values) {
        DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
        writes.writes.clear();
        const std::string source =
            "combat artemis_system_net inherent_modifications = " +
            std::to_string(inherent_value) + ";";
        DOCTEST_REQUIRE(Append_Decoded_(
            source.c_str(), &writes, &diagnostic));
        DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
        DOCTEST_REQUIRE(config::Commit_State(&state));
        DOCTEST_CHECK(
            CMBTFIR2::artemis_system_net.inherent_modifications ==
            inherent_value);
    }

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat artemis_system_net inherent_modifications = 256;",
        &writes,
        &diagnostic));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    const config::t_variable_id counter_id =
        Variable_Id_("combat.artemis_system_net.counter");
    DOCTEST_REQUIRE(counter_id != config::CONFIG_NO_VARIABLE);
    state.values[counter_id].integer_value = 0;
    DOCTEST_CHECK_FALSE(config::Commit_State(&state));
}

DOCTEST_TEST_CASE("ship explosion settings preserve 1.50 defaults") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTFIR2::ship_explosion.drive_damage_multiplier == 5);
    DOCTEST_CHECK(CMBTFIR2::ship_explosion.quantum_detonator_damage_multiplier == 3);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat ship_explosion drive_damage_multiplier = 7;"
        "combat ship_explosion quantum_detonator_damage_multiplier = 4;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTFIR2::ship_explosion.drive_damage_multiplier == 7);
    DOCTEST_CHECK(CMBTFIR2::ship_explosion.quantum_detonator_damage_multiplier == 4);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat ship_explosion drive_damage_multiplier = 256;",
        &writes,
        &diagnostic));
}

DOCTEST_TEST_CASE("Quantum Detonator capture chance preserves 1.50 encoding") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTFIR2::quantum_detonator.explode_chance == 6);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat quantum_detonator explode_chance = 11;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTFIR2::quantum_detonator.explode_chance == 11);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat quantum_detonator explode_chance = 0;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat quantum_detonator explode_chance = 12;", &writes, &diagnostic));
}

DOCTEST_TEST_CASE("combat namespace settings resolve bidirectionally and commit defaults") {
    const char* paths[] = {
        "combat.bonus.battle_scanner_beam_offense",
        "combat.bonus.starbase_fleet_beam_offense",
        "combat.bonus.phasing_cloak_effective_turns",
        "combat.bonus.displacement_device_beam_miss_percent",
        "combat.ship_explosion.drive_damage_multiplier",
        "combat.quantum_detonator.explode_chance",
        "combat.satellite_racial_defense_bonus",
        "combat.round_teleporter",
        "combat.board_range.normal",
        "combat.board_range.transporters",
        "combat.sub_space_teleporter_range",
        "combat.recloak_next_turn",
        "combat.rephase_cloak_next_turn",
        "combat.lightning_field_ratio",
        "combat.lightning_field_mode",
    };
    for (const char* expected : paths) {
        config::t_variable_id id = config::CONFIG_NO_VARIABLE;
        DOCTEST_REQUIRE(config::Find_Variable(expected, std::strlen(expected), &id));
        std::string resolved;
        DOCTEST_REQUIRE(config::Get_Variable_Name(id, &resolved));
        DOCTEST_CHECK(resolved == expected);
        DOCTEST_CHECK(config::Get_Variable_Type(id) != nullptr);
    }

    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTMIS::lightning_field_ratio == 50);
    DOCTEST_CHECK(CMBTMIS::lightning_field_mode == CMBTMIS::LIGHTNING_FIELD_MODE_CLASSIC);
}

DOCTEST_TEST_CASE("missile defense settings preserve classic defaults") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(CMBTMIS::missile_defense.inertial_stabilizer_missile_defense == 25);
    DOCTEST_CHECK(CMBTMIS::missile_defense.inertial_nullifier_missile_defense == 50);
    DOCTEST_CHECK(CMBTMIS::missile_defense.ecm_jammer_missile_defense == 70);
    DOCTEST_CHECK(CMBTMIS::missile_defense.multi_wave_jammer_missile_defense == 100);
    DOCTEST_CHECK(CMBTMIS::missile_defense.wide_area_jammer_missile_defense == 130);
    DOCTEST_CHECK(CMBTMIS::missile_defense.wide_area_jammer_fleet_missile_defense == 70);
    DOCTEST_CHECK(CMBTMIS::missile_defense.cloaking_device_missile_defense == 0);
    DOCTEST_CHECK(CMBTMIS::missile_defense.cloaking_device_missile_miss_percent == 50);
    DOCTEST_CHECK(CMBTMIS::missile_defense.displacement_device_missiles_miss_percent == 30);
    DOCTEST_CHECK(CMBTMIS::missile_defense.tachyon_scanner == 20);
    DOCTEST_CHECK(CMBTMIS::missile_defense.neutron_scanner == 40);
    DOCTEST_CHECK(CMBTMIS::missile_defense.sensors == 70);
    DOCTEST_CHECK(CMBTFIR2::combat_bonus.displacement_device_beam_miss_percent == 30);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat missile_defense inertial_stabilizer_missile_defense = 26;"
        "combat missile_defense inertial_nullifier_missile_defense = 51;"
        "combat missile_defense ecm_jammer_missile_defense = 71;"
        "combat missile_defense multi_wave_jammer_missile_defense = 101;"
        "combat missile_defense wide_area_jammer_missile_defense = 131;"
        "combat missile_defense wide_area_jammer_fleet_missile_defense = 72;"
        "combat missile_defense cloaking_device_missile_defense = 60;"
        "combat missile_defense cloaking_device_missile_miss_percent = 45;"
        "combat missile_defense displacement_device_missiles_miss_percent = 35;"
        "combat missile_defense tachyon_scanner = 21;"
        "combat missile_defense neutron_scanner = 41;"
        "combat missile_defense sensors = 71;"
        "combat bonus displacement_device_beam_miss_percent = 17;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(CMBTMIS::missile_defense.inertial_stabilizer_missile_defense == 26);
    DOCTEST_CHECK(CMBTMIS::missile_defense.inertial_nullifier_missile_defense == 51);
    DOCTEST_CHECK(CMBTMIS::missile_defense.ecm_jammer_missile_defense == 71);
    DOCTEST_CHECK(CMBTMIS::missile_defense.multi_wave_jammer_missile_defense == 101);
    DOCTEST_CHECK(CMBTMIS::missile_defense.wide_area_jammer_missile_defense == 131);
    DOCTEST_CHECK(CMBTMIS::missile_defense.wide_area_jammer_fleet_missile_defense == 72);
    DOCTEST_CHECK(CMBTMIS::missile_defense.cloaking_device_missile_defense == 60);
    DOCTEST_CHECK(CMBTMIS::missile_defense.cloaking_device_missile_miss_percent == 45);
    DOCTEST_CHECK(CMBTMIS::missile_defense.displacement_device_missiles_miss_percent == 35);
    DOCTEST_CHECK(CMBTMIS::missile_defense.tachyon_scanner == 21);
    DOCTEST_CHECK(CMBTMIS::missile_defense.neutron_scanner == 41);
    DOCTEST_CHECK(CMBTMIS::missile_defense.sensors == 71);
    DOCTEST_CHECK(CMBTFIR2::combat_bonus.displacement_device_beam_miss_percent == 17);
}

DOCTEST_TEST_CASE("flyer hit points and planetary shields preserve classic defaults") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(CMBTMIS::flyer_hit_points.nuclear_missile == 4);
    DOCTEST_CHECK(CMBTMIS::flyer_hit_points.heavy_fighter == 10);
    DOCTEST_CHECK(TECHDATA::planetary_shields[PLANETARY_SHIELD_NONE].tech_app_id == 0);
    DOCTEST_CHECK(TECHDATA::planetary_shields[PLANETARY_SHIELD_NONE].absorption == 0);
    DOCTEST_CHECK((TECHDATA::planetary_shields[PLANETARY_RADIATION_SHIELD].tech_app_id == 134));
    DOCTEST_CHECK(TECHDATA::planetary_shields[PLANETARY_RADIATION_SHIELD].absorption == 5);
    DOCTEST_CHECK(TECHDATA::planetary_shields[PLANETARY_FLUX_SHIELD].tech_app_id == 130);
    DOCTEST_CHECK(TECHDATA::planetary_shields[PLANETARY_FLUX_SHIELD].absorption == 10);
    DOCTEST_CHECK((TECHDATA::planetary_shields[PLANETARY_BARRIER_SHIELD].tech_app_id == 129));
    DOCTEST_CHECK(TECHDATA::planetary_shields[PLANETARY_BARRIER_SHIELD].absorption == 20);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat flyer_hit_points nuclear_missile = 9;"
        "combat flyer_hit_points heavy_fighter = 20;"
        "combat planetary_shield radiation tech_app_id = 513;"
        "combat planetary_shield radiation absorption = 6;"
        "combat planetary_shield flux tech_app_id = 1027;"
        "combat planetary_shield flux absorption = 11;"
        "combat planetary_shield barrier tech_app_id = 1541;"
        "combat planetary_shield barrier absorption = 24;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(CMBTMIS::flyer_hit_points.nuclear_missile == 9);
    DOCTEST_CHECK(CMBTMIS::flyer_hit_points.heavy_fighter == 20);
    DOCTEST_CHECK(TECHDATA::planetary_shields[PLANETARY_SHIELD_NONE].tech_app_id == 0);
    DOCTEST_CHECK(TECHDATA::planetary_shields[PLANETARY_SHIELD_NONE].absorption == 0);
    DOCTEST_CHECK((TECHDATA::planetary_shields[PLANETARY_RADIATION_SHIELD].tech_app_id == 513));
    DOCTEST_CHECK(TECHDATA::planetary_shields[PLANETARY_RADIATION_SHIELD].absorption == 6);
    DOCTEST_CHECK((TECHDATA::planetary_shields[PLANETARY_FLUX_SHIELD].tech_app_id == 1027));
    DOCTEST_CHECK(TECHDATA::planetary_shields[PLANETARY_FLUX_SHIELD].absorption == 11);
    DOCTEST_CHECK((TECHDATA::planetary_shields[PLANETARY_BARRIER_SHIELD].tech_app_id == 1541));
    DOCTEST_CHECK(TECHDATA::planetary_shields[PLANETARY_BARRIER_SHIELD].absorption == 24);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat flyer_hit_points bomber = 256;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat planetary_shield barrier tech_app_id = 65536;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat planetary_shield barrier absorption = 256;",
        &writes,
        &diagnostic));
}

DOCTEST_TEST_CASE("flyer hit points preserve the unsigned byte domain") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat flyer_hit_points nuclear_missile = 0;"
        "combat flyer_hit_points merculite_missile = 127;"
        "combat flyer_hit_points pulson_missile = 128;"
        "combat flyer_hit_points zeon_missile = 255;"
        "combat flyer_hit_points interceptor = 0;"
        "combat flyer_hit_points assault_shuttle = 127;"
        "combat flyer_hit_points bomber = 128;"
        "combat flyer_hit_points heavy_fighter = 255;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(CMBTMIS::flyer_hit_points.nuclear_missile == 0);
    DOCTEST_CHECK(CMBTMIS::flyer_hit_points.merculite_missile == 127);
    DOCTEST_CHECK(CMBTMIS::flyer_hit_points.pulson_missile == 128);
    DOCTEST_CHECK(CMBTMIS::flyer_hit_points.zeon_missile == 255);
    DOCTEST_CHECK(CMBTMIS::flyer_hit_points.interceptor == 0);
    DOCTEST_CHECK(CMBTMIS::flyer_hit_points.assault_shuttle == 127);
    DOCTEST_CHECK(CMBTMIS::flyer_hit_points.bomber == 128);
    DOCTEST_CHECK(CMBTMIS::flyer_hit_points.heavy_fighter == 255);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat flyer_hit_points interceptor = -1;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat flyer_hit_points interceptor = 256;",
        &writes,
        &diagnostic));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
}

DOCTEST_TEST_CASE("flyer combat modifiers preserve 1.50 defaults and signed values") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTMIS::flyer_combat_modifier.nuclear_missile_defense_bonus == -10);
    DOCTEST_CHECK(CMBTMIS::flyer_combat_modifier.merculite_missile_defense_bonus == 15);
    DOCTEST_CHECK(CMBTMIS::flyer_combat_modifier.pulson_missile_defense_bonus == 40);
    DOCTEST_CHECK(CMBTMIS::flyer_combat_modifier.zeon_missile_defense_bonus == 70);
    DOCTEST_CHECK(CMBTMIS::flyer_combat_modifier.fighters_offense_bonus == 50);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat flyer_combat_modifier nuclear_missile_defense_bonus = -9;"
        "combat flyer_combat_modifier merculite_missile_defense_bonus = 16;"
        "combat flyer_combat_modifier pulson_missile_defense_bonus = 41;"
        "combat flyer_combat_modifier zeon_missile_defense_bonus = 71;"
        "combat flyer_combat_modifier fighters_offense_bonus = 51;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(CMBTMIS::flyer_combat_modifier.nuclear_missile_defense_bonus == -9);
    DOCTEST_CHECK(CMBTMIS::flyer_combat_modifier.merculite_missile_defense_bonus == 16);
    DOCTEST_CHECK(CMBTMIS::flyer_combat_modifier.pulson_missile_defense_bonus == 41);
    DOCTEST_CHECK(CMBTMIS::flyer_combat_modifier.zeon_missile_defense_bonus == 71);
    DOCTEST_CHECK(CMBTMIS::flyer_combat_modifier.fighters_offense_bonus == 51);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat flyer_combat_modifier nuclear_missile_defense_bonus = -129;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat flyer_combat_modifier zeon_missile_defense_bonus = 128;",
        &writes,
        &diagnostic));
}

DOCTEST_TEST_CASE("missile speed settings preserve and replace 1.50 defaults") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(CMBTMIS::speed_base.nuclear_missile == 12);
    DOCTEST_CHECK(CMBTMIS::speed_base.plasma_torpedo == 24);
    DOCTEST_CHECK(CMBTMIS::speed_base.interceptor == 10);
    DOCTEST_CHECK(CMBTMIS::speed_base.assault_shuttle == 6);
    DOCTEST_CHECK(CMBTMIS::speed_fast_bonus == 4);
    DOCTEST_CHECK(COMBAT1::td_combat_speed_bonus == 4);
    DOCTEST_CHECK(CMBTMIS::proton_torpedo_apply_speed_base == 0);
    DOCTEST_CHECK(COMBINIT::satellite_ammo_basis == 20);
    DOCTEST_CHECK(COMBINIT::missile_base_ammo_basis == 5);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "combat speed_base nuclear_missile = 15;"
        "combat speed_base interceptor = 13;"
        "combat speed_fast_bonus = 7;"
        "combat td_combat_speed_bonus = 6;"
        "combat proton_torpedo_apply_speed_base = true;"
        "combat satellite_ammo_basis = 24;"
        "combat missile_base_ammo_basis = 6;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(CMBTMIS::speed_base.nuclear_missile == 15);
    DOCTEST_CHECK(CMBTMIS::speed_base.interceptor == 13);
    DOCTEST_CHECK(CMBTMIS::speed_fast_bonus == 7);
    DOCTEST_CHECK(COMBAT1::td_combat_speed_bonus == 6);
    DOCTEST_CHECK(CMBTMIS::proton_torpedo_apply_speed_base == 1);
    DOCTEST_CHECK(COMBINIT::satellite_ammo_basis == 24);
    DOCTEST_CHECK(COMBINIT::missile_base_ammo_basis == 6);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat speed_base nuclear_missile = 65536;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat speed_fast_bonus = 256;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat td_combat_speed_bonus = 256;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat satellite_ammo_basis = 256;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat missile_base_ammo_basis = 256;", &writes, &diagnostic));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
}

DOCTEST_TEST_CASE("ship design technology bonuses preserve 1.50 defaults") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(DESIGN::megafluxers_tactical_bonus == 25);
    DOCTEST_CHECK(DESIGN::megafluxers_strategic_bonus == 50);
    DOCTEST_CHECK(DESIGN::augmented_engines_speed_bonus == 5);
    DOCTEST_CHECK(DESIGN::Apply_Megafluxers_Tactical_Bonus(120) == 150);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "design megafluxers_tactical_bonus = 40;"
        "design megafluxers_strategic_bonus = 75;"
        "design augmented_engines_speed_bonus = 12;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(DESIGN::megafluxers_tactical_bonus == 40);
    DOCTEST_CHECK(DESIGN::megafluxers_strategic_bonus == 75);
    DOCTEST_CHECK(DESIGN::augmented_engines_speed_bonus == 12);
    DOCTEST_CHECK(DESIGN::Apply_Megafluxers_Tactical_Bonus(120) == 168);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "design augmented_engines_speed_bonus = 128;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "design megafluxers_strategic_bonus = 65536;",
        &writes,
        &diagnostic));
}

DOCTEST_TEST_CASE("Megafluxers tactical scaling uses defined widened arithmetic") {
    struct s_case {
        uint16_t bonus;
        int32_t expected;
    };
    const s_case cases[] = {
        {0, 32767},
        {32767, 10769529},
        {32768, 10769857},
        {65535, 21506620},
    };

    for (const s_case& test_case : cases) {
        DESIGN::megafluxers_tactical_bonus = test_case.bonus;
        DOCTEST_CHECK(
            DESIGN::Apply_Megafluxers_Tactical_Bonus(32767) ==
            test_case.expected);
    }
}

DOCTEST_TEST_CASE("alternative refit formula preserves the classic default") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(DESIGN::alternative_refit == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "design alternative_refit = true;", &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(DESIGN::alternative_refit == 1);
}

DOCTEST_TEST_CASE("DESIGN button component behavior preserves classic defaults") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(DESIGN::design_button_computer == DESIGN::DESIGN_BUTTON_BEHAVIOR_UPDATE);
    DOCTEST_CHECK(DESIGN::design_button_shield == DESIGN::DESIGN_BUTTON_BEHAVIOR_UPDATE);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "design design_button_computer = intact;"
        "design design_button_shield = intact;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(DESIGN::design_button_computer == DESIGN::DESIGN_BUTTON_BEHAVIOR_INTACT);
    DOCTEST_CHECK(DESIGN::design_button_shield == DESIGN::DESIGN_BUTTON_BEHAVIOR_INTACT);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "design design_button_computer = clear;", &writes, &diagnostic));
}

DOCTEST_TEST_CASE("CLEAR button component behavior preserves 1.50 defaults") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(DESIGN::clear_button_computer == DESIGN::CLEAR_BUTTON_COMPUTER_UPDATE);
    DOCTEST_CHECK(DESIGN::clear_button_shield == DESIGN::CLEAR_BUTTON_SHIELD_INTACT);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "design clear_button_computer = intact;"
        "design clear_button_shield = clear;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(DESIGN::clear_button_computer == DESIGN::CLEAR_BUTTON_COMPUTER_INTACT);
    DOCTEST_CHECK(DESIGN::clear_button_shield == DESIGN::CLEAR_BUTTON_SHIELD_CLEAR);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "design clear_button_computer = preserve;",
        &writes,
        &diagnostic));
}

DOCTEST_TEST_CASE("miniaturization curves and categories preserve 1.50 defaults") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    const uint16_t expected_cost[DESIGN::MINIATURIZATION_LEVEL_COUNT] = {
        100,
        75,
        55,
        40,
        30,
        25,
    };
    const uint16_t expected_space[DESIGN::MINIATURIZATION_CATEGORY_COUNT]
                                 [DESIGN::MINIATURIZATION_LEVEL_COUNT] = {
                                     {1000, 800, 650, 500, 350, 250},
                                     {1000, 800, 700, 600, 500, 400},
                                     {1000, 1000, 1000, 1000, 1000, 1000},
                                 };
    DOCTEST_CHECK(std::memcmp(DESIGN::miniaturization.cost, expected_cost, sizeof(expected_cost)) == 0);
    DOCTEST_CHECK(std::memcmp(DESIGN::miniaturization.space, expected_space, sizeof(expected_space)) == 0);

    int category_counts[DESIGN::MINIATURIZATION_CATEGORY_COUNT] = {};
    for (int tech = 0; tech < TECH_APP_COUNT; ++tech) {
        DOCTEST_REQUIRE(DESIGN::miniaturization_categories[tech] <
            DESIGN::MINIATURIZATION_CATEGORY_COUNT);
        category_counts[DESIGN::miniaturization_categories[tech]]++;
    }
    DOCTEST_CHECK(category_counts[DESIGN::MINIATURIZATION_CATEGORY_GROUP_1] == 154);
    DOCTEST_CHECK(category_counts[DESIGN::MINIATURIZATION_CATEGORY_GROUP_2] == 49);
    DOCTEST_CHECK(category_counts[DESIGN::MINIATURIZATION_CATEGORY_GROUP_3] == 9);
    DOCTEST_CHECK(DESIGN::miniaturization_categories[TECH_APP_ANTI_MATTER_TORPEDOES] ==
        DESIGN::MINIATURIZATION_CATEGORY_GROUP_2);
    DOCTEST_CHECK(DESIGN::miniaturization_categories[TECH_APP_REINFORCED_HULL] ==
        DESIGN::MINIATURIZATION_CATEGORY_GROUP_3);
    DOCTEST_CHECK(DESIGN::Miniaturization_Cost_Multiplier(2) == 55);
    DOCTEST_CHECK(DESIGN::Miniaturization_Cost_Multiplier(6) == 25);
    DOCTEST_CHECK(DESIGN::Miniaturization_Space_Multiplier(2,
                      DESIGN::MINIATURIZATION_CATEGORY_GROUP_1) == 650);
    DOCTEST_CHECK(DESIGN::Miniaturization_Space_Multiplier(2,
                      DESIGN::MINIATURIZATION_CATEGORY_GROUP_2) == 700);
    DOCTEST_CHECK(DESIGN::Miniaturization_Space_Multiplier(5,
                      DESIGN::MINIATURIZATION_CATEGORY_GROUP_3) == 1000);
}

DOCTEST_TEST_CASE("miniaturization curves and categories bind by normalized names") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "design miniaturization cost = 100 75 60 40 30 25;"
        "design miniaturization space group1 = 1000 800 675 500 350 250;"
        "design miniaturization space group3 = 1000 1000 900 800 700 600;"
        "design tech_mini anti_matter_torpedo = group1;"
        "design tech_mini reinforced_hull = group2;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(DESIGN::Miniaturization_Cost_Multiplier(2) == 60);
    DOCTEST_CHECK(DESIGN::Miniaturization_Space_Multiplier(2,
                      DESIGN::MINIATURIZATION_CATEGORY_GROUP_1) == 675);
    DOCTEST_CHECK(DESIGN::Miniaturization_Space_Multiplier(5,
                      DESIGN::MINIATURIZATION_CATEGORY_GROUP_3) == 600);
    DOCTEST_CHECK(DESIGN::miniaturization_categories[TECH_APP_ANTI_MATTER_TORPEDOES] ==
        DESIGN::MINIATURIZATION_CATEGORY_GROUP_1);
    DOCTEST_CHECK(DESIGN::miniaturization_categories[TECH_APP_REINFORCED_HULL] ==
        DESIGN::MINIATURIZATION_CATEGORY_GROUP_2);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "design tech_mini reinforced_hull = group4;", &writes, &diagnostic));
}

DOCTEST_TEST_CASE("rifle, troop, and tactical turn settings preserve 1.50 defaults") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(TECHDATA::rifles[RIFLE_LASER_RIFLE].tech_app_id == TECH_APP_LASER_RIFLE);
    DOCTEST_CHECK(TECHDATA::rifles[RIFLE_PLASMA_RIFLE].bonus == 30);
    DOCTEST_CHECK(invasion::troop.type.tanks == 10);
    DOCTEST_CHECK(invasion::troop.type.militia == -10);
    DOCTEST_CHECK(invasion::troop.type.rebels == -20);
    DOCTEST_CHECK(invasion::troop.bonus.low_g == -10);
    DOCTEST_CHECK(invasion::troop.bonus.security_stations == 20);
    DOCTEST_CHECK(COMBAT1::max_combat_turns == 50);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "design rifle plasma technology = fusion_rifle;"
        "design rifle plasma bonus = 31;"
        "combat troop type tanks = 14;"
        "combat troop type militia = -8;"
        "combat troop bonus low_g = -6;"
        "combat troop bonus security_stations = 17;"
        "combat troop bonus battleoids_hits = 3;"
        "combat max_turns = 127;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK((TECHDATA::rifles[RIFLE_PLASMA_RIFLE].tech_app_id == TECH_APP_FUSION_RIFLE));
    DOCTEST_CHECK(TECHDATA::rifles[RIFLE_PLASMA_RIFLE].bonus == 31);
    DOCTEST_CHECK(invasion::troop.type.tanks == 14);
    DOCTEST_CHECK(invasion::troop.type.militia == -8);
    DOCTEST_CHECK(invasion::troop.bonus.low_g == -6);
    DOCTEST_CHECK(invasion::troop.bonus.security_stations == 17);
    DOCTEST_CHECK(invasion::troop.bonus.battleoids_hits == 3);
    DOCTEST_CHECK(COMBAT1::max_combat_turns == 127);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat max_turns = 128;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "design rifle plasma bonus = -1;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "design rifle plasma bonus = 256;", &writes, &diagnostic));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
}

DOCTEST_TEST_CASE("rifle bonuses preserve the unsigned byte domain") {
    config::s_config_state state;
    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    const uint8_t bonuses[] = {0, 127, 128, 255};

    for (uint8_t bonus : bonuses) {
        DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
        writes.writes.clear();
        const std::string source =
            "design rifle plasma bonus = " + std::to_string(bonus) + ";";
        DOCTEST_REQUIRE(Append_Decoded_(
            source.c_str(), &writes, &diagnostic));
        DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
        DOCTEST_REQUIRE(config::Commit_State(&state));
        DOCTEST_CHECK(TECHDATA::rifles[RIFLE_PLASMA_RIFLE].bonus == bonus);
    }
}

DOCTEST_TEST_CASE("x3 missile racks preserve classic defaults and bind autodesign behavior") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    const int16_t classic[DESIGN::MISSILE_RACK_COUNT] = {2, 5, 10, 15, 20};
    for (int16_t i = 0; i < DESIGN::MISSILE_RACK_COUNT; ++i) {
        DOCTEST_CHECK(DESIGN::Missile_Rack_Quantity(i) == classic[i]);
    }
    DOCTEST_CHECK(DESIGN::missile_racks[0].space == 10);
    DOCTEST_CHECK(DESIGN::missile_racks[4].cost == 40);
    DOCTEST_CHECK_FALSE(DESIGN::Should_Autodesign_Use_X3_Rack(5, 3, 3));
    DOCTEST_CHECK_FALSE(DESIGN::missile_default_x2_rack);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "design missile_x3_shot = true;"
        "design missile_default_x2_rack = true;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    const int16_t x3[DESIGN::MISSILE_RACK_COUNT] = {2, 3, 5, 10, 20};
    for (int16_t i = 0; i < DESIGN::MISSILE_RACK_COUNT; ++i) {
        DOCTEST_CHECK(DESIGN::Missile_Rack_Quantity(i) == x3[i]);
    }
    DOCTEST_CHECK(DESIGN::Missile_Rack_Size(3) == 20);
    DOCTEST_CHECK(DESIGN::Missile_Rack_Size(5) == 30);
    DOCTEST_CHECK(DESIGN::Missile_Rack_Size(15) == 0);
    DOCTEST_CHECK(DESIGN::Should_Autodesign_Use_X3_Rack(5, 3, 3));
    DOCTEST_CHECK(DESIGN::Should_Autodesign_Use_X3_Rack(5, 2, 3));
    DOCTEST_CHECK_FALSE(DESIGN::Should_Autodesign_Use_X3_Rack(5, 4, 3));
    DOCTEST_CHECK(DESIGN::missile_default_x2_rack);
}

DOCTEST_TEST_CASE("missile rack space and cost bind independently") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "design missile_racks 1 space = 11;"
        "design missile_racks 1 cost = 12;"
        "design missile_racks 5 space = 41;"
        "design missile_racks 5 cost = 42;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(DESIGN::missile_racks[0].space == 11);
    DOCTEST_CHECK(DESIGN::missile_racks[0].cost == 12);
    DOCTEST_CHECK(DESIGN::missile_racks[4].space == 41);
    DOCTEST_CHECK(DESIGN::missile_racks[4].cost == 42);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "design missile_racks 1 space = 256;", &writes, &diagnostic));
}

DOCTEST_TEST_CASE("weapon arc cost and space bind independently across uint8 domain") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(DESIGN::arc_cost_and_space[DESIGN::WEAPON_ARC_MODIFIER_EXTENDED].cost == 25);
    DOCTEST_CHECK(DESIGN::arc_cost_and_space[DESIGN::WEAPON_ARC_MODIFIER_EXTENDED].space == 25);
    DOCTEST_CHECK(DESIGN::arc_cost_and_space[DESIGN::WEAPON_ARC_MODIFIER_ALL_ROUND].cost == 50);
    DOCTEST_CHECK(DESIGN::arc_cost_and_space[DESIGN::WEAPON_ARC_MODIFIER_ALL_ROUND].space == 50);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "design arc_cost_and_space extended cost = 0;"
        "design arc_cost_and_space extended space = 2;"
        "design arc_cost_and_space all_round cost = 3;"
        "design arc_cost_and_space all_round space = 255;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(DESIGN::arc_cost_and_space[DESIGN::WEAPON_ARC_MODIFIER_EXTENDED].cost == 0);
    DOCTEST_CHECK(DESIGN::arc_cost_and_space[DESIGN::WEAPON_ARC_MODIFIER_EXTENDED].space == 2);
    DOCTEST_CHECK(DESIGN::arc_cost_and_space[DESIGN::WEAPON_ARC_MODIFIER_ALL_ROUND].cost == 3);
    DOCTEST_CHECK(DESIGN::arc_cost_and_space[DESIGN::WEAPON_ARC_MODIFIER_ALL_ROUND].space == 255);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "design arc_cost_and_space extended cost = -1;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "design arc_cost_and_space all_round space = 256;",
        &writes,
        &diagnostic));
}

DOCTEST_TEST_CASE("maximum galaxy star count binds through the canonical map table") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(MAPGEN::maximum_star_count == 99);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "map star_count small = 4;"
        "map star_count medium = 8;"
        "map star_count large = 16;"
        "map star_count huge = 32;"
        "map star_count maximum = 100;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(MAPGEN::star_count_by_galaxy_size[GALAXY_SIZE_SMALL] == 4);
    DOCTEST_CHECK(MAPGEN::star_count_by_galaxy_size[GALAXY_SIZE_MEDIUM] == 8);
    DOCTEST_CHECK(MAPGEN::star_count_by_galaxy_size[GALAXY_SIZE_LARGE] == 16);
    DOCTEST_CHECK(MAPGEN::star_count_by_galaxy_size[GALAXY_SIZE_HUGE] == 32);
    DOCTEST_CHECK(MAPGEN::maximum_star_count == 100);

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    writes.writes.clear();
    DOCTEST_REQUIRE(Append_Decoded_(
        "map star_count maximum = 71;", &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_CHECK_FALSE(config::Commit_State(&state));
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    writes.writes.clear();
    DOCTEST_REQUIRE(Append_Decoded_(
        "map star_count huge = 72;", &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_CHECK_FALSE(config::Commit_State(&state));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "map star_count maximum = 1024;", &writes, &diagnostic));
}

DOCTEST_TEST_CASE("map planet limit supports raised and disabled thresholds") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    DOCTEST_CHECK(MAPGEN::planet_limit == 250);

    const int limits[] = {400, 0, 5120};
    for (int limit : limits) {
        config::s_config_write_set writes;
        config::s_cfg_diagnostic diagnostic;
        const std::string assignment =
            "map planet_limit = " + std::to_string(limit) + ";";
        DOCTEST_REQUIRE(Append_Decoded_(
            assignment.c_str(), &writes, &diagnostic));
        DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
        DOCTEST_REQUIRE(config::Commit_State(&state));
        DOCTEST_CHECK(MAPGEN::planet_limit == limit);
    }

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "map planet_limit = -1;", &writes, &diagnostic));
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "map planet_limit = 5121;", &writes, &diagnostic));
    MAPGEN::planet_limit = 250;
}

DOCTEST_TEST_CASE("descriptive map settings bind native generation policy") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;

    DOCTEST_REQUIRE(Append_Decoded_(
        "map home_system minimum_planets = 5;", &writes, &diagnostic));
    DOCTEST_REQUIRE(Append_Decoded_(
        "map specials generate_orion = false;", &writes, &diagnostic));
    DOCTEST_REQUIRE(Append_Decoded_(
        "map monsters natural_chance impossible = 25;", &writes, &diagnostic));
    DOCTEST_REQUIRE(Append_Decoded_(
        "map satellite_type roll_1 orbit_1 = planet;", &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(MAPGEN::home_system_config.minimum_planets == 5);
    DOCTEST_CHECK(MAPGEN::special_generation_config.generate_orion == 0);
    DOCTEST_CHECK(MAPGEN::monster_generation_config.natural_chance[GAME_DIFFICULTY_IMPOSSIBLE] == 25);
    DOCTEST_CHECK(MOX::orbit_to_satellite_type[0][0] == MAPGEN_SATELLITE_PLANET);
}

DOCTEST_TEST_CASE("special, cloak, and beam settings preserve 1.50 defaults") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    const uint8_t special_chances[SYSTEM_SPECIAL_COUNT] = {
        64,
        5,
        3,
        2,
        3,
        2,
        9,
        4,
        3,
        0,
        5,
        0,
    };
    DOCTEST_CHECK(std::memcmp(MOX::planet_special_weighted_chance,
                      special_chances,
                      sizeof(special_chances)) == 0);
    DOCTEST_CHECK(MOX::planet_special == 64);
    DOCTEST_CHECK(AIMOVE::eel_spawn_frequency == 30);
    DOCTEST_CHECK(AIMOVE::max_eels == 4);
    DOCTEST_CHECK(CMBTSPEC::animation.cloak_steps.normal == 1);
    DOCTEST_CHECK(CMBTSPEC::animation.cloak_steps.fast == 2);
    DOCTEST_CHECK(CMBTFIRE::hit_threshold_fixed_value == 40);
    DOCTEST_CHECK(CMBTFIRE::ion_affects_antarans == 0);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "map planet_special_chance no_special = 0;"
        "map planet_special_chance wormhole = 127;"
        "map planet_special_chance space_debris = 128;"
        "map planet_special_chance pirate_cache = 255;"
        "map planet_special_chance gold_deposits = 0;"
        "map planet_special_chance gem_deposits = 127;"
        "map planet_special_chance natives = 128;"
        "map planet_special_chance splinter_colony = 255;"
        "map planet_special_chance hero = 0;"
        "map planet_special_chance monster = 127;"
        "map planet_special_chance ancient_artifacts = 128;"
        "map planet_special_chance unused_orion = 255;"
        "map star_special_chance = 75;"
        "ai eel_spawn_frequency = 20;"
        "ai max_eels = 8;"
        "interface animation cloak_steps normal = 13;"
        "interface animation cloak_steps fast = 127;"
        "combat hit_threshold_fixed_value = 45;"
        "combat ion_affects_antarans = true;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));
    const uint8_t configured_special_chances[SYSTEM_SPECIAL_COUNT] = {
        0,
        127,
        128,
        255,
        0,
        127,
        128,
        255,
        0,
        127,
        128,
        255,
    };
    DOCTEST_CHECK(std::memcmp(MOX::planet_special_weighted_chance,
                      configured_special_chances,
                      sizeof(configured_special_chances)) == 0);
    DOCTEST_CHECK(MOX::planet_special == 75);
    DOCTEST_CHECK(AIMOVE::eel_spawn_frequency == 20);
    DOCTEST_CHECK(AIMOVE::max_eels == 8);
    DOCTEST_CHECK(CMBTSPEC::animation.cloak_steps.normal == 13);
    DOCTEST_CHECK(CMBTSPEC::animation.cloak_steps.fast == 127);
    DOCTEST_CHECK(CMBTFIRE::hit_threshold_fixed_value == 45);
    DOCTEST_CHECK(CMBTFIRE::ion_affects_antarans == 1);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "map star_special_chance = 101;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "map planet_special_chance wormhole = 256;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "ai eel_spawn_frequency = 0;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "ai eel_spawn_frequency = 101;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "ai max_eels = 0;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "ai max_eels = 101;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "interface animation cloak_steps normal = 0;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "interface animation cloak_steps fast = 128;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat hit_threshold_fixed_value = 101;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "combat ion_affects_antarans = enabled;", &writes, &diagnostic));

    const char* paths[] = {
        "map.planet_special_chance.0",
        "map.planet_special_chance.11",
        "map.star_special_chance",
        "ai.eel_spawn_frequency",
        "ai.max_eels",
        "interface.animation.cloak_steps.normal",
        "combat.hit_threshold_fixed_value",
        "combat.ion_affects_antarans",
    };
    for (const char* expected : paths) {
        config::t_variable_id id = config::CONFIG_NO_VARIABLE;
        DOCTEST_REQUIRE(config::Find_Variable(expected, std::strlen(expected), &id));
        std::string name;
        DOCTEST_REQUIRE(config::Get_Variable_Name(id, &name));
        DOCTEST_CHECK(name == expected);
    }

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
}

DOCTEST_TEST_CASE("map satellite generation tables preserve 1.50 defaults and bindings") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(MOX::class_to_num_satellites[0][STAR_CLASS_B] == 0);
    DOCTEST_CHECK(MOX::class_to_num_satellites[9][STAR_CLASS_DWARF] == 1);
    DOCTEST_CHECK(MAPGEN::satellite_orbit_weight[0][GALAXY_AGE_YOUNG] == 25);
    DOCTEST_CHECK(MAPGEN::satellite_orbit_weight[4][GALAXY_AGE_OLD] == 5);
    DOCTEST_CHECK(MOX::orbit_to_satellite_type[0][0] == MAPGEN_SATELLITE_ASTEROID);
    DOCTEST_CHECK(MOX::orbit_to_satellite_type[1][0] == MAPGEN_SATELLITE_COMPANION_STAR);
    const uint8_t size_thresholds[PLANET_SIZE_COUNT] = {1, 3, 7, 9, 10};
    DOCTEST_CHECK(std::memcmp(MOX::planet_size_table, size_thresholds, sizeof(size_thresholds)) == 0);
    DOCTEST_CHECK(MAPGEN::mineral_class[0][STAR_CLASS_B] ==
        MINERAL_RESOURCE_ABUNDANT);
    DOCTEST_CHECK(MAPGEN::mineral_class[9][STAR_CLASS_DWARF] ==
        MINERAL_RESOURCE_ULTRA_RICH);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "map satellite_count roll_10 brown_dwarf = 5;"
        "map orbit_weight orbit_1 young = 40;"
        "map satellite_type roll_1 orbit_1 = planet;"
        "map planet_size_threshold tiny = 2;"
        "map mineral_class roll_10 white = ultra_rich;"
        "map climate_weight_young_average toxic bombarded = 0;"
        "map climate_weight_old gaia lifeless = 255;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(MOX::class_to_num_satellites[9][STAR_CLASS_DWARF] == 5);
    DOCTEST_CHECK(MAPGEN::satellite_orbit_weight[0][GALAXY_AGE_YOUNG] == 40);
    DOCTEST_CHECK(MOX::orbit_to_satellite_type[0][0] == MAPGEN_SATELLITE_PLANET);
    DOCTEST_CHECK(MOX::planet_size_table[PLANET_SIZE_TINY] == 2);
    DOCTEST_CHECK(MAPGEN::mineral_class[9][STAR_CLASS_F] ==
        MINERAL_RESOURCE_ULTRA_RICH);
    DOCTEST_CHECK(MOX::normal_gal_climate_roll_table[PLANET_CLIMATE_TOXIC][PLANET_GROUP_BOMBARDED] == 0);
    DOCTEST_CHECK(MOX::old_gal_climate_roll_table[PLANET_CLIMATE_GAIA][PLANET_GROUP_LIFELESS] == 255);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "map satellite_count roll_1 blue = 6;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "map orbit_weight orbit_1 young = 256;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "map satellite_type roll_1 orbit_1 = 0;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "map planet_size_threshold huge = 11;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "map mineral_class roll_1 blue = unknown;", &writes, &diagnostic));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));
}

DOCTEST_TEST_CASE("map aggregate validation rejects unsafe table states") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));

    config::t_variable_id variable = config::CONFIG_NO_VARIABLE;
    DOCTEST_REQUIRE(config::Find_Variable(
        "map.planet_size_threshold.4", strlen("map.planet_size_threshold.4"), &variable));
    state.values[variable].integer_value = 9;
    DOCTEST_CHECK_FALSE(config::Commit_State(&state));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    for (int star_class = 0; star_class < STAR_CLASS_COUNT; ++star_class) {
        char path[64];
        snprintf(path, sizeof(path), "map.star_class_weight.%d.0", star_class);
        DOCTEST_REQUIRE(config::Find_Variable(path, strlen(path), &variable));
        state.values[variable].integer_value = 0;
    }
    DOCTEST_CHECK_FALSE(config::Commit_State(&state));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    for (int star_class = 0; star_class < STAR_CLASS_BLACK_HOLE; ++star_class) {
        char path[64];
        snprintf(path, sizeof(path), "map.star_class_weight.%d.0", star_class);
        DOCTEST_REQUIRE(config::Find_Variable(path, strlen(path), &variable));
        state.values[variable].integer_value = 0;
    }
    DOCTEST_CHECK_FALSE(config::Commit_State(&state));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    for (int star_class = 0; star_class < STAR_CLASS_BLACK_HOLE; ++star_class) {
        for (int orbit = 0; orbit < 5; ++orbit) {
            char path[64];
            snprintf(path, sizeof(path), "map.environment_group.%d.%d", star_class, orbit);
            DOCTEST_REQUIRE(config::Find_Variable(path, strlen(path), &variable));
            state.values[variable].integer_value = PLANET_GROUP_LIFELESS;
        }
    }
    DOCTEST_CHECK_FALSE(config::Commit_State(&state));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Find_Variable(
        "map.layout.0.star_spacing", strlen("map.layout.0.star_spacing"), &variable));
    state.values[variable].integer_value = 32767;
    DOCTEST_CHECK_FALSE(config::Commit_State(&state));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Find_Variable(
        "map.layout.4.automatic", strlen("map.layout.4.automatic"), &variable));
    state.values[variable].bool_value = false;
    DOCTEST_CHECK(config::Commit_State(&state));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Find_Variable(
        "map.star_count.4", strlen("map.star_count.4"), &variable));
    state.values[variable].integer_value = 1023;
    DOCTEST_REQUIRE(config::Find_Variable(
        "map.layout.4.automatic", strlen("map.layout.4.automatic"), &variable));
    state.values[variable].bool_value = false;
    DOCTEST_CHECK_FALSE(config::Commit_State(&state));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Find_Variable(
        "map.layout.5.height", strlen("map.layout.5.height"), &variable));
    state.values[variable].integer_value = 800;
    DOCTEST_REQUIRE(config::Find_Variable(
        "map.layout.5.star_spacing",
        strlen("map.layout.5.star_spacing"),
        &variable));
    state.values[variable].integer_value = 30;
    DOCTEST_CHECK_FALSE(config::Commit_State(&state));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    for (int climate = 0; climate < PLANET_CLIMATE_COUNT; ++climate) {
        char path[96];
        snprintf(path, sizeof(path), "map.climate_weight_young_average.%d.%d", climate, PLANET_GROUP_BOMBARDED);
        DOCTEST_REQUIRE(config::Find_Variable(path, strlen(path), &variable));
        state.values[variable].integer_value = 0;
    }
    const int vdc_bombarded_climates[] = {
        PLANET_CLIMATE_TOXIC,
        PLANET_CLIMATE_RADIATED,
        PLANET_CLIMATE_BARREN,
    };
    const int vdc_bombarded_weights[] = {15, 60, 25};
    for (size_t index = 0; index < std::size(vdc_bombarded_climates); ++index) {
        char path[96];
        snprintf(path, sizeof(path), "map.climate_weight_young_average.%d.%d", vdc_bombarded_climates[index], PLANET_GROUP_BOMBARDED);
        DOCTEST_REQUIRE(config::Find_Variable(path, strlen(path), &variable));
        state.values[variable].integer_value = vdc_bombarded_weights[index];
    }
    DOCTEST_CHECK(config::Commit_State(&state));

    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    for (int star_class = 0; star_class < STAR_CLASS_DWARF; ++star_class) {
        for (int roll = 0; roll < 10; ++roll) {
            char path[96];
            snprintf(path, sizeof(path), "map.mineral_class.%d.%d", roll, star_class);
            DOCTEST_REQUIRE(config::Find_Variable(path, strlen(path), &variable));
            state.values[variable].integer_value = MINERAL_RESOURCE_ABUNDANT;
        }
    }
    DOCTEST_CHECK(config::Commit_State(&state));
}

DOCTEST_TEST_CASE("strategic detection settings preserve and replace 1.50 defaults") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(SHIPSTAK::scan_bonus.battle_scanner == 2);
    DOCTEST_CHECK(SHIPSTAK::scan_bonus.scout_lab == 0);
    DOCTEST_CHECK(SHIPSTAK::scan_bonus.star_base == 2);
    DOCTEST_CHECK(SHIPSTAK::scan_bonus.battlestation == 4);
    DOCTEST_CHECK(SHIPSTAK::scan_bonus.star_fortress == 6);
    DOCTEST_CHECK(SHIPSTAK::scan_bonus.no_scanner == 2);
    DOCTEST_CHECK(SHIPSTAK::scan_bonus.space_scanner == 2);
    DOCTEST_CHECK(SHIPSTAK::scan_bonus.tachyon_scanner == 4);
    DOCTEST_CHECK(SHIPSTAK::scan_bonus.neutron_scanner == 6);
    DOCTEST_CHECK(SHIPSTAK::scan_bonus.sensors == 9);
    DOCTEST_CHECK(SHIPSTAK::scan_bonus.planet == 32000);
    DOCTEST_CHECK(SHIPSTAK::scan_bonus.fleet == 32000);
    DOCTEST_CHECK(SHIPSTAK::stealth_bonus.stealthy_ships == 500);
    DOCTEST_CHECK(SHIPSTAK::stealth_bonus.stealth_field == 500);
    DOCTEST_CHECK(SHIPSTAK::stealth_bonus.cloaking_device == 500);
    DOCTEST_CHECK(SHIPSTAK::stealth_bonus.phasing_cloak == 500);
    DOCTEST_CHECK(SHIPSTAK::stealth_size_penalty ==
        SHIPSTAK::e_stealth_size_penalty::race_tech);

    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;
    DOCTEST_REQUIRE(Append_Decoded_(
        "detection scan_bonus battle_scanner = 7;"
        "detection scan_bonus scout_lab = 3;"
        "detection scan_bonus space_scanner = 8;"
        "detection scan_bonus sensors = 12;"
        "detection scan_bonus planet = 30000;"
        "detection scan_bonus fleet = 31000;"
        "detection stealth_bonus stealthy_ships = 450;"
        "detection stealth_bonus stealth_field = 650;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(SHIPSTAK::scan_bonus.battle_scanner == 7);
    DOCTEST_CHECK(SHIPSTAK::scan_bonus.scout_lab == 3);
    DOCTEST_CHECK(SHIPSTAK::scan_bonus.space_scanner == 8);
    DOCTEST_CHECK(SHIPSTAK::scan_bonus.sensors == 12);
    DOCTEST_CHECK(SHIPSTAK::scan_bonus.planet == 30000);
    DOCTEST_CHECK(SHIPSTAK::scan_bonus.fleet == 31000);
    DOCTEST_CHECK(SHIPSTAK::stealth_bonus.stealthy_ships == 450);
    DOCTEST_CHECK(SHIPSTAK::stealth_bonus.stealth_field == 650);

    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "detection scan_bonus battle_scanner = 32768;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "detection scan_bonus space_scanner = 128;",
        &writes,
        &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "detection scan_bonus sensors = 128;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "detection scan_bonus planet = 32768;", &writes, &diagnostic));
    writes.writes.clear();
    DOCTEST_CHECK_FALSE(Append_Decoded_(
        "detection scan_bonus fleet = 32768;", &writes, &diagnostic));
}

DOCTEST_TEST_CASE("strategic stealth size-penalty modes bind by name") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));

    struct s_mode_case {
        const char* name;
        SHIPSTAK::e_stealth_size_penalty expected;
        bool racial_penalty_applies;
        bool device_penalty_applies;
    };
    const s_mode_case cases[] = {
        {"none", SHIPSTAK::e_stealth_size_penalty::none, true, true},
        {"race", SHIPSTAK::e_stealth_size_penalty::race, false, true},
        {"race_tech", SHIPSTAK::e_stealth_size_penalty::race_tech, false, false},
    };

    for (const s_mode_case& mode : cases) {
        const std::string source =
            std::string("detection stealth_size_penalty = ") + mode.name + ";";
        config::s_config_write_set writes;
        config::s_cfg_diagnostic diagnostic;
        DOCTEST_REQUIRE(Append_Decoded_(source.c_str(), &writes, &diagnostic));
        DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
        DOCTEST_REQUIRE(config::Commit_State(&state));
        DOCTEST_CHECK(SHIPSTAK::stealth_size_penalty == mode.expected);
        DOCTEST_CHECK(SHIPSTAK::Traveling_Size_Penalty_Applies(true, false) ==
            mode.racial_penalty_applies);
        DOCTEST_CHECK(SHIPSTAK::Traveling_Size_Penalty_Applies(false, true) ==
            mode.device_penalty_applies);
        DOCTEST_CHECK(SHIPSTAK::Traveling_Size_Penalty_Applies(false, false));
    }
}

DOCTEST_TEST_CASE("shorter build queue assignments replace execution through an earlier end") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    config::s_config_write_set writes;
    config::s_cfg_diagnostic diagnostic;

    DOCTEST_REQUIRE(Append_Decoded_(
        "colony build_queue 1 = prepend design1 design2 design3 end;",
        &writes,
        &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    writes.writes.clear();
    DOCTEST_REQUIRE(Append_Decoded_(
        "colony build_queue 1 = housing end;", &writes, &diagnostic));
    DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
    DOCTEST_REQUIRE(config::Commit_State(&state));

    DOCTEST_CHECK(BUILD_QUEUE::presets[1][0] == BUILD_QUEUE::INSTRUCTION_HOUSING);
    DOCTEST_CHECK(BUILD_QUEUE::presets[1][1] == BUILD_QUEUE::INSTRUCTION_END);
    DOCTEST_CHECK(BUILD_QUEUE::presets[1][2] == BUILD_QUEUE::INSTRUCTION_DESIGN2);
}

DOCTEST_TEST_CASE("shared build queue layouts validate every row terminator") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    config::t_variable_id first = config::CONFIG_NO_VARIABLE;
    DOCTEST_REQUIRE(config::Find_Variable(
        "colony.build_queue.10.0", std::strlen("colony.build_queue.10.0"), &first));
    for (int instruction = 0; instruction < BUILD_QUEUE::INSTRUCTION_CAPACITY;
         ++instruction) {
        state.values[first + instruction].integer_value = BUILD_QUEUE::INSTRUCTION_CLEAR;
    }
    DOCTEST_CHECK_FALSE(config::Commit_State(&state));
}

DOCTEST_TEST_SUITE("generated configuration bindings") {
    DOCTEST_TEST_CASE("predefined ship templates have native defaults before configuration") {
        DOCTEST_CHECK(SHIP_CONFIG::guardian_template.picture_num == 7);
        DOCTEST_CHECK(SHIP_CONFIG::guardian_template.ship_weapon[2].type == 20);
        DOCTEST_CHECK((SHIP_CONFIG::guardian_template.ship_weapon[2].specials == 0x4500));
        DOCTEST_CHECK(SHIP_CONFIG::guardian_template.ship_weapon[2].count == 1);
        DOCTEST_CHECK(SHIP_CONFIG::guardian_template.ship_weapon[2].current_count == 1);
        DOCTEST_CHECK((SHIP_CONFIG::guardian_template.special_device_flags[3] & 0x02) != 0);

        DOCTEST_CHECK(SHIP_CONFIG::amoeba_templates[1].ship_weapon[1].count == 5);
        DOCTEST_CHECK((SHIP_CONFIG::amoeba_templates[0].special_device_flags[4] & 0x80) != 0);

        const s_ship_design& raider = SHIP_CONFIG::antaran_offensive_templates
            [GAME_DIFFICULTY_AVERAGE][SHIP_SIZE_SMALL];
        const s_ship_design& marauder = SHIP_CONFIG::antaran_offensive_templates
            [GAME_DIFFICULTY_AVERAGE][SHIP_SIZE_MEDIUM];
        const s_ship_design& intruder = SHIP_CONFIG::antaran_offensive_templates
            [GAME_DIFFICULTY_AVERAGE][SHIP_SIZE_LARGE];
        const s_ship_design& interdictor = SHIP_CONFIG::antaran_offensive_templates
            [GAME_DIFFICULTY_AVERAGE][SHIP_SIZE_HUGE];
        const s_ship_design& harbinger = SHIP_CONFIG::antaran_offensive_templates
            [GAME_DIFFICULTY_IMPOSSIBLE][SHIP_SIZE_TITAN];
        DOCTEST_CHECK(raider.ship_weapon[1].firing_arc == WEAPON_FIRING_ARC_360);
        DOCTEST_CHECK(marauder.ship_weapon[2].firing_arc == WEAPON_FIRING_ARC_360);
        for (int slot = 2; slot <= 4; ++slot) {
            DOCTEST_CHECK(intruder.ship_weapon[slot].firing_arc ==
                WEAPON_FIRING_ARC_360);
        }
        DOCTEST_CHECK(interdictor.ship_weapon[2].firing_arc ==
            WEAPON_FIRING_ARC_360);
        DOCTEST_CHECK(interdictor.ship_weapon[3].firing_arc ==
            WEAPON_FIRING_ARC_360);
        DOCTEST_CHECK(harbinger.ship_weapon[2].firing_arc ==
            WEAPON_FIRING_ARC_360);
        DOCTEST_CHECK(harbinger.ship_weapon[3].firing_arc ==
            WEAPON_FIRING_ARC_360);
        DOCTEST_CHECK(harbinger.ship_weapon[6].type ==
            WEAPON_BLACK_HOLE_GENERATOR);
        DOCTEST_CHECK(harbinger.ship_weapon[6].count == 2);
        DOCTEST_CHECK(harbinger.ship_weapon[6].current_count == 2);
        DOCTEST_CHECK(harbinger.ship_weapon[6].firing_arc ==
            WEAPON_FIRING_ARC_360);
        DOCTEST_CHECK(harbinger.ship_weapon[7].type == WEAPON_INTERCEPTOR);
        DOCTEST_CHECK(harbinger.ship_weapon[7].count == 12);
        DOCTEST_CHECK(harbinger.ship_weapon[7].current_count == 12);
        DOCTEST_CHECK(harbinger.ship_weapon[7].firing_arc ==
            WEAPON_FIRING_ARC_360);
        DOCTEST_CHECK((SHIP_CONFIG::antaran_offensive_templates[GAME_DIFFICULTY_HARD]
                                                               [SHIP_SIZE_LARGE]
                                                                   .special_device_flags[2] &
                          0x20) != 0);
        DOCTEST_CHECK((SHIP_CONFIG::antaran_offensive_templates[GAME_DIFFICULTY_IMPOSSIBLE]
                                                               [SHIP_SIZE_TITAN]
                                                                   .special_device_flags[4] &
                          0x40) != 0);
        DOCTEST_CHECK(SHIP_CONFIG::antaran_defensive_templates[GAME_DIFFICULTY_AVERAGE]
                                                              [SHIP_SIZE_SMALL]
                                                                  .ship_weapon[1]
                                                                  .type == 24);
        DOCTEST_CHECK(SHIP_CONFIG::antaran_defensive_templates[GAME_DIFFICULTY_AVERAGE]
                                                              [SHIP_SIZE_LARGE]
                                                                  .ship_weapon[4]
                                                                  .count == 3);
        DOCTEST_CHECK(SHIP_CONFIG::antaran_defensive_templates[GAME_DIFFICULTY_AVERAGE]
                                                              [SHIP_SIZE_TITAN]
                                                                  .ship_weapon[7]
                                                                  .count == 6);
        DOCTEST_CHECK(SHIP_CONFIG::antaran_fortress.primary_weapons[0].weapon.type ==
            WEAPON_DEATH_RAY);
        DOCTEST_CHECK(SHIP_CONFIG::antaran_fortress.primary_weapons[0].weapon.mods ==
            WEAPON_MOD_FLAG_HEAVY_MOUNT);
        DOCTEST_CHECK(SHIP_CONFIG::antaran_fortress.primary_weapons[2].weapon.mods ==
            WEAPON_MOD_FLAG_POINT_DEFENSE);
        DOCTEST_CHECK(SHIP_CONFIG::antaran_fortress.specials[7] ==
            SPECIAL_STRUCTURAL_ANALYZER);
    }

    DOCTEST_TEST_CASE("Antaran extra specials follow the authoritative difficulty progression") {
        const int16_t intruder_specials[] = {
            0,
            0,
            SPECIAL_ECM_JAMMER,
            SPECIAL_MULTI_WAVE_ECM_JAMMER,
            SPECIAL_WIDE_AREA_JAMMER,
        };
        const int16_t titan_specials[] = {
            SPECIAL_ECM_JAMMER,
            SPECIAL_MULTI_WAVE_ECM_JAMMER,
            SPECIAL_WIDE_AREA_JAMMER,
            SPECIAL_WIDE_AREA_JAMMER,
            SPECIAL_WIDE_AREA_JAMMER,
        };
        for (int difficulty = GAME_DIFFICULTY_TUTOR;
             difficulty <= GAME_DIFFICULTY_IMPOSSIBLE;
             ++difficulty) {
            const s_ship_design& offensive_intruder =
                SHIP_CONFIG::antaran_offensive_templates[difficulty][SHIP_SIZE_LARGE];
            const s_ship_design& defensive_intruder =
                SHIP_CONFIG::antaran_defensive_templates[difficulty][SHIP_SIZE_LARGE];
            if (intruder_specials[difficulty] == 0) {
                DOCTEST_CHECK_FALSE(Design_Has_Special_(
                    offensive_intruder, SPECIAL_ECM_JAMMER));
                DOCTEST_CHECK_FALSE(Design_Has_Special_(
                    defensive_intruder, SPECIAL_ECM_JAMMER));
            } else {
                DOCTEST_CHECK(Design_Has_Special_(
                    offensive_intruder, intruder_specials[difficulty]));
                DOCTEST_CHECK(Design_Has_Special_(
                    defensive_intruder, intruder_specials[difficulty]));
            }

            const s_ship_design& offensive_titan =
                SHIP_CONFIG::antaran_offensive_templates[difficulty][SHIP_SIZE_TITAN];
            const s_ship_design& defensive_titan =
                SHIP_CONFIG::antaran_defensive_templates[difficulty][SHIP_SIZE_TITAN];
            DOCTEST_CHECK(Design_Has_Special_(
                offensive_titan, titan_specials[difficulty]));
            DOCTEST_CHECK(Design_Has_Special_(
                defensive_titan, titan_specials[difficulty]));
        }
    }

    DOCTEST_TEST_CASE("Interdictor defaults match the authoritative shared design") {
        struct s_expected_weapon {
            int16_t type;
            int8_t arc;
            uint16_t mods;
            int8_t ammo;
            int8_t amount;
        };
        const s_expected_weapon expected[] = {
            {4, 1, 0, 0, 10},
            {4, 1, 2, 0, 4},
            {24, 16, 0, 10, 15},
            {13, 16, 0, 0, 2},
            {4, 1, 4, 0, 10},
            {11, 1, 0, 0, 4},
            {0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0},
        };
        const int8_t expected_specials[GAME_DIFFICULTY_COUNT][5] = {
            {-128, 0x00, 0x00, 0x05, 0x02},
            {-128, 0x02, 0x00, 0x05, 0x02},
            {-128, 0x00, 0x20, 0x05, 0x02},
            {-128, 0x00, 0x00, 0x05, 0x42},
            {-128, 0x00, 0x00, 0x05, 0x42},
        };

        for (int difficulty = GAME_DIFFICULTY_TUTOR;
             difficulty <= GAME_DIFFICULTY_IMPOSSIBLE;
             ++difficulty) {
            const s_ship_design& design =
                SHIP_CONFIG::antaran_offensive_templates[difficulty][SHIP_SIZE_HUGE];
            DOCTEST_CHECK(design.picture_num == 3);
            DOCTEST_CHECK(design.size == SHIP_SIZE_HUGE);
            DOCTEST_CHECK(design.ftl_type == DRIVE_HYPER_DRIVE);
            DOCTEST_CHECK(design.armor_type == ARMOR_XENTRONIUM_ARMOR);
            DOCTEST_CHECK(design.shield_type == SHIELD_NO_SHIELD);
            DOCTEST_CHECK(design.computer_type == COMPUTER_MOLECULARTRONIC_COMPUTER);
            DOCTEST_CHECK(std::memcmp(design.special_device_flags,
                              expected_specials[difficulty],
                              sizeof(design.special_device_flags)) == 0);
            for (int slot = 0; slot < 8; ++slot) {
                const s_ship_weapons& weapon = design.ship_weapon[slot];
                DOCTEST_CHECK((weapon.type == expected[slot].type));
                DOCTEST_CHECK(weapon.firing_arc == expected[slot].arc);
                DOCTEST_CHECK((weapon.specials == expected[slot].mods));
                DOCTEST_CHECK(weapon.ammo == expected[slot].ammo);
                DOCTEST_CHECK(weapon.count == expected[slot].amount);
                DOCTEST_CHECK(weapon.current_count == expected[slot].amount);
            }
        }
    }

    DOCTEST_TEST_CASE("defensive Harbinger defaults match the authoritative shared design") {
        struct s_expected_weapon {
            int16_t type;
            int8_t arc;
            uint16_t mods;
            int8_t ammo;
            int8_t amount;
        };
        const s_expected_weapon expected[] = {
            {4, 1, 0, -1, 10},
            {4, 1, 2, -1, 2},
            {24, 15, 0, 10, 20},
            {13, 15, 0, -1, 3},
            {4, 1, 4, -1, 15},
            {11, 1, 2, -1, 2},
            {37, 15, 0, -1, 1},
            {31, 15, 0, 1, 6},
        };
        const int8_t expected_specials[GAME_DIFFICULTY_COUNT][5] = {
            {-128, 0x02, 0x00, 0x05, 0x02},
            {-128, 0x00, 0x20, 0x05, 0x02},
            {-128, 0x00, 0x00, 0x05, 0x42},
            {-128, 0x00, 0x00, 0x05, 0x42},
            {-128, 0x00, 0x00, 0x05, 0x42},
        };

        for (int difficulty = GAME_DIFFICULTY_TUTOR;
             difficulty <= GAME_DIFFICULTY_IMPOSSIBLE;
             ++difficulty) {
            const s_ship_design& design =
                SHIP_CONFIG::antaran_defensive_templates[difficulty][SHIP_SIZE_TITAN];
            DOCTEST_CHECK(design.picture_num == 4);
            DOCTEST_CHECK(design.size == SHIP_SIZE_TITAN);
            DOCTEST_CHECK(design.ftl_type == DRIVE_HYPER_DRIVE);
            DOCTEST_CHECK(design.armor_type == ARMOR_XENTRONIUM_ARMOR);
            DOCTEST_CHECK(design.shield_type == SHIELD_NO_SHIELD);
            DOCTEST_CHECK(design.computer_type == COMPUTER_MOLECULARTRONIC_COMPUTER);
            DOCTEST_CHECK(std::memcmp(design.special_device_flags,
                              expected_specials[difficulty],
                              sizeof(design.special_device_flags)) == 0);
            for (int slot = 0; slot < 8; ++slot) {
                const s_ship_weapons& weapon = design.ship_weapon[slot];
                DOCTEST_CHECK((weapon.type == expected[slot].type));
                DOCTEST_CHECK(weapon.firing_arc == expected[slot].arc);
                DOCTEST_CHECK((weapon.specials == expected[slot].mods));
                DOCTEST_CHECK(weapon.ammo == expected[slot].ammo);
                DOCTEST_CHECK(weapon.count == expected[slot].amount);
                DOCTEST_CHECK(weapon.current_count == expected[slot].amount);
            }
        }
    }

    DOCTEST_TEST_CASE("Antaran retained slots and complete special bitmaps are configurable") {
        config::s_config_state state;
        DOCTEST_REQUIRE(config::Initialize_Defaults(&state));

        config::s_config_write_set writes;
        config::s_cfg_diagnostic diagnostic;
        DOCTEST_REQUIRE(Append_Decoded_(
            "ships antaran_defensive average frigate weapons 2 amount = 7;"
            "ships antaran_defensive average cruiser weapons 5 amount = 8;"
            "ships antaran_defensive average titan weapons 8 amount = 9;"
            "ships antaran_offensive impossible titan weapons 7 amount = 3;"
            "ships antaran_offensive impossible titan weapons 8 type = heavy_fighter;"
            "ships antaran_offensive impossible titan specials = regeneration;",
            &writes,
            &diagnostic));
        DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
        DOCTEST_REQUIRE(config::Commit_State(&state));

        const s_ship_design& raider = SHIP_CONFIG::antaran_defensive_templates
            [GAME_DIFFICULTY_AVERAGE][SHIP_SIZE_SMALL];
        const s_ship_design& intruder = SHIP_CONFIG::antaran_defensive_templates
            [GAME_DIFFICULTY_AVERAGE][SHIP_SIZE_LARGE];
        const s_ship_design& defensive_harbinger =
            SHIP_CONFIG::antaran_defensive_templates[GAME_DIFFICULTY_AVERAGE]
                                                    [SHIP_SIZE_TITAN];
        s_ship_design& offensive_harbinger =
            SHIP_CONFIG::antaran_offensive_templates[GAME_DIFFICULTY_IMPOSSIBLE]
                                                    [SHIP_SIZE_TITAN];
        DOCTEST_CHECK(raider.ship_weapon[1].count == 7);
        DOCTEST_CHECK(raider.ship_weapon[1].current_count == 7);
        DOCTEST_CHECK(intruder.ship_weapon[4].count == 8);
        DOCTEST_CHECK(intruder.ship_weapon[4].current_count == 8);
        DOCTEST_CHECK(defensive_harbinger.ship_weapon[7].count == 9);
        DOCTEST_CHECK(defensive_harbinger.ship_weapon[7].current_count == 9);
        DOCTEST_CHECK(offensive_harbinger.ship_weapon[6].count == 3);
        DOCTEST_CHECK(offensive_harbinger.ship_weapon[6].current_count == 3);
        DOCTEST_CHECK(offensive_harbinger.ship_weapon[7].type ==
            WEAPON_HEAVY_FIGHTER);
        DOCTEST_CHECK(Design_Has_Special_(
            offensive_harbinger, SPECIAL_REGENERATION));
        DOCTEST_CHECK_FALSE(Design_Has_Special_(
            offensive_harbinger, SPECIAL_DAMPER_FIELD));

        DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
        DOCTEST_REQUIRE(config::Commit_State(&state));
    }

    DOCTEST_TEST_CASE("Antaran appearances stay within the combat picture cache") {
        config::s_config_state state;
        DOCTEST_REQUIRE(config::Initialize_Defaults(&state));

        config::s_config_write_set writes;
        config::s_cfg_diagnostic diagnostic;
        DOCTEST_REQUIRE(Append_Decoded_(
            "ships antaran_offensive average battleship appearance = 0;"
            "ships antaran_offensive impossible titan appearance = 44;"
            "ships antaran_defensive average titan appearance = 44;",
            &writes,
            &diagnostic));
        DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
        DOCTEST_REQUIRE(config::Commit_State(&state));

        DOCTEST_CHECK(SHIP_CONFIG::antaran_offensive_templates
                          [GAME_DIFFICULTY_AVERAGE][SHIP_SIZE_HUGE]
                              .picture_num == 0);
        DOCTEST_CHECK(SHIP_CONFIG::antaran_offensive_templates
                          [GAME_DIFFICULTY_IMPOSSIBLE][SHIP_SIZE_TITAN]
                              .picture_num == 44);
        DOCTEST_CHECK(SHIP_CONFIG::antaran_defensive_templates
                          [GAME_DIFFICULTY_AVERAGE][SHIP_SIZE_TITAN]
                              .picture_num == 44);

        const char* invalid_sources[] = {
            "ships antaran_offensive impossible titan appearance = -1;",
            "ships antaran_offensive impossible titan appearance = 45;",
            "ships antaran_defensive average titan appearance = -1;",
            "ships antaran_defensive average titan appearance = 45;",
            "ships antaran_offensive average battleship appearance = -1;",
            "ships antaran_offensive average battleship appearance = 45;",
        };
        for (const char* source : invalid_sources) {
            writes.writes.clear();
            DOCTEST_CHECK_FALSE(Append_Decoded_(source, &writes, &diagnostic));
        }

        DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
        DOCTEST_REQUIRE(config::Commit_State(&state));
    }

    DOCTEST_TEST_CASE("Antaran Fortress design uses canonical bindings") {
        config::s_config_state state;
        DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
        DOCTEST_REQUIRE(config::Commit_State(&state));

        config::s_config_write_set writes;
        config::s_cfg_diagnostic diagnostic;
        DOCTEST_REQUIRE(Append_Decoded_(
            "ships fortress primary_weapons 1 weapon type = plasma_cannon;"
            "ships fortress primary_weapons 1 weapon arc = forward,forward_extended;"
            "ships fortress primary_weapons 1 weapon mods = hv,pd;"
            "ships fortress primary_weapons 1 space_mods = hv;"
            "ships fortress overflow_weapons 1 type = mass_driver;"
            "ships fortress overflow_weapons 1 arc = 15;"
            "ships fortress overflow_weapons 2 arc = 0;"
            "ships fortress overflow_weapons 2 mods = pd;"
            "ships fortress specials 8 = regeneration;",
            &writes,
            &diagnostic));
        DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
        DOCTEST_REQUIRE(config::Commit_State(&state));

        DOCTEST_CHECK(SHIP_CONFIG::antaran_fortress.primary_weapons[0].weapon.type ==
            WEAPON_PLASMA_CANNON);
        DOCTEST_CHECK(SHIP_CONFIG::antaran_fortress.primary_weapons[0].weapon.arc ==
            (WEAPON_FIRING_ARC_FORWARD |
                WEAPON_FIRING_ARC_FORWARD_EXTENDED));
        DOCTEST_CHECK(SHIP_CONFIG::antaran_fortress.primary_weapons[0].weapon.mods ==
            (WEAPON_MOD_FLAG_HEAVY_MOUNT | WEAPON_MOD_FLAG_POINT_DEFENSE));
        DOCTEST_CHECK(SHIP_CONFIG::antaran_fortress.primary_weapons[0].space_mods ==
            WEAPON_MOD_FLAG_HEAVY_MOUNT);
        DOCTEST_CHECK(SHIP_CONFIG::antaran_fortress.overflow_weapons[0].type ==
            WEAPON_MASS_DRIVER);
        DOCTEST_CHECK(SHIP_CONFIG::antaran_fortress.overflow_weapons[0].arc ==
            WEAPON_FIRING_ARC_ALL_SECTORS);
        DOCTEST_CHECK(SHIP_CONFIG::antaran_fortress.overflow_weapons[1].arc ==
            WEAPON_FIRING_ARC_NONE);
        DOCTEST_CHECK(SHIP_CONFIG::antaran_fortress.overflow_weapons[1].mods ==
            WEAPON_MOD_FLAG_POINT_DEFENSE);
        DOCTEST_CHECK(SHIP_CONFIG::antaran_fortress.specials[7] ==
            SPECIAL_REGENERATION);

        writes.writes.clear();
        DOCTEST_CHECK_FALSE(Append_Decoded_(
            "ships fortress primary_weapons 1 weapon type = unknown_weapon;",
            &writes,
            &diagnostic));
        writes.writes.clear();
        DOCTEST_CHECK_FALSE(Append_Decoded_(
            "ships fortress overflow_weapons 1 space_mods = hv;",
            &writes,
            &diagnostic));
        writes.writes.clear();
        DOCTEST_CHECK_FALSE(Append_Decoded_(
            "ships fortress primary_weapons 1 weapon mods = reserved_0;",
            &writes,
            &diagnostic));
        writes.writes.clear();
        DOCTEST_CHECK_FALSE(Append_Decoded_(
            "ships fortress primary_weapons 1 weapon arc = design_360;",
            &writes,
            &diagnostic));
        writes.writes.clear();
        DOCTEST_CHECK_FALSE(Append_Decoded_(
            "ships fortress primary_weapons 1 weapon arc = 16;",
            &writes,
            &diagnostic));
        writes.writes.clear();
        DOCTEST_CHECK_FALSE(Append_Decoded_(
            "ships fortress primary_weapons 1 space_mods = 32768;",
            &writes,
            &diagnostic));

        DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
        DOCTEST_REQUIRE(config::Commit_State(&state));
        DOCTEST_CHECK(SHIP_CONFIG::antaran_fortress.primary_weapons[0].weapon.type ==
            WEAPON_DEATH_RAY);
        DOCTEST_CHECK(SHIP_CONFIG::antaran_fortress.specials[7] ==
            SPECIAL_STRUCTURAL_ANALYZER);
    }

    DOCTEST_TEST_CASE("monster combat statistics preserve 1.50 defaults and bindings") {
        config::s_config_state state;
        DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
        DOCTEST_REQUIRE(config::Commit_State(&state));

        DOCTEST_CHECK(SHIP_CONFIG::monster.hit_points.system_eel == 300);
        DOCTEST_CHECK(SHIP_CONFIG::monster.hit_points.system_amoeba == 400);
        DOCTEST_CHECK(SHIP_CONFIG::monster.hit_points.system_crystal_hydra_dragon == 500);
        DOCTEST_CHECK(SHIP_CONFIG::monster.hit_points.event_amoeba == 750);
        DOCTEST_CHECK(SHIP_CONFIG::monster.hit_points.event_eel == 1000);
        DOCTEST_CHECK(SHIP_CONFIG::monster.hit_points.event_hydra == 1500);
        DOCTEST_CHECK(SHIP_CONFIG::monster.hit_points.event_crystal_dragon == 2500);
        DOCTEST_CHECK(SHIP_CONFIG::monster.hit_points.system_crystal == -1);
        DOCTEST_CHECK(SHIP_CONFIG::monster.hit_points.system_hydra == -1);
        DOCTEST_CHECK(SHIP_CONFIG::monster.hit_points.system_dragon == -1);
        DOCTEST_CHECK(SHIP_CONFIG::monster.hit_points.event_crystal == -1);
        DOCTEST_CHECK(SHIP_CONFIG::monster.hit_points.event_dragon == -1);
        DOCTEST_CHECK(SHIP_CONFIG::monster.drives.amoeba == DRIVE_FUSION_DRIVE);
        DOCTEST_CHECK(SHIP_CONFIG::monster.drives.guardian == DRIVE_ANTI_MATTER_DRIVE);
        DOCTEST_CHECK(SHIP_CONFIG::monster.drives.crystal == DRIVE_ANTI_MATTER_DRIVE);
        DOCTEST_CHECK(SHIP_CONFIG::monster.drives.dragon == DRIVE_INTERPHASED_DRIVE);
        DOCTEST_CHECK(SHIP_CONFIG::monster.drives.eel == DRIVE_INTERPHASED_DRIVE);
        DOCTEST_CHECK(SHIP_CONFIG::monster.drives.hydra == DRIVE_FUSION_DRIVE);

        config::s_config_write_set writes;
        config::s_cfg_diagnostic diagnostic;
        DOCTEST_REQUIRE(Append_Decoded_(
            "ships monster hit_points system_eel = 301;"
            "ships monster hit_points event_hydra = 1501;"
            "ships monster drives amoeba = ion;"
            "ships monster drives guardian = hyper;"
            "ships monster drives crystal = nuclear;"
            "ships monster drives dragon = hyper;"
            "ships monster drives eel = anti_matter;"
            "ships monster drives hydra = interphased;",
            &writes,
            &diagnostic));
        DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
        DOCTEST_REQUIRE(config::Commit_State(&state));

        DOCTEST_CHECK(SHIP_CONFIG::monster.hit_points.system_eel == 301);
        DOCTEST_CHECK(SHIP_CONFIG::monster.hit_points.event_hydra == 1501);
        DOCTEST_CHECK(SHIP_CONFIG::monster.drives.amoeba == DRIVE_ION_DRIVE);
        DOCTEST_CHECK(SHIP_CONFIG::monster.drives.guardian == DRIVE_HYPER_DRIVE);
        DOCTEST_CHECK(SHIP_CONFIG::monster.drives.crystal == DRIVE_NUCLEAR_DRIVE);
        DOCTEST_CHECK(SHIP_CONFIG::monster.drives.dragon == DRIVE_HYPER_DRIVE);
        DOCTEST_CHECK(SHIP_CONFIG::monster.drives.eel == DRIVE_ANTI_MATTER_DRIVE);
        DOCTEST_CHECK(SHIP_CONFIG::monster.drives.hydra == DRIVE_INTERPHASED_DRIVE);

        DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
        writes.writes.clear();
        DOCTEST_REQUIRE(Append_Decoded_(
            "ships monster hit_points system_eel = 32767;"
            "ships monster hit_points system_amoeba = 32767;"
            "ships monster hit_points system_crystal_hydra_dragon = 32767;"
            "ships monster hit_points event_amoeba = 32767;"
            "ships monster hit_points event_eel = 32767;"
            "ships monster hit_points event_hydra = 32767;"
            "ships monster hit_points event_crystal_dragon = 32767;",
            &writes,
            &diagnostic));
        DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
        DOCTEST_REQUIRE(config::Commit_State(&state));
        DOCTEST_CHECK(SHIP_CONFIG::monster.hit_points.system_eel == 32767);
        DOCTEST_CHECK(SHIP_CONFIG::monster.hit_points.system_amoeba == 32767);
        DOCTEST_CHECK(
            SHIP_CONFIG::monster.hit_points.system_crystal_hydra_dragon == 32767);
        DOCTEST_CHECK(SHIP_CONFIG::monster.hit_points.event_amoeba == 32767);
        DOCTEST_CHECK(SHIP_CONFIG::monster.hit_points.event_eel == 32767);
        DOCTEST_CHECK(SHIP_CONFIG::monster.hit_points.event_hydra == 32767);
        DOCTEST_CHECK(SHIP_CONFIG::monster.hit_points.event_crystal_dragon == 32767);

        const char* invalid_hit_points[] = {
            "ships monster hit_points system_eel = 32768;",
            "ships monster hit_points system_amoeba = 32768;",
            "ships monster hit_points system_crystal_hydra_dragon = 32768;",
            "ships monster hit_points event_amoeba = 32768;",
            "ships monster hit_points event_eel = 32768;",
            "ships monster hit_points event_hydra = 32768;",
            "ships monster hit_points event_crystal_dragon = 32768;",
        };
        for (const char* source : invalid_hit_points) {
            writes.writes.clear();
            DOCTEST_CHECK_FALSE(Append_Decoded_(source, &writes, &diagnostic));
        }
        writes.writes.clear();
        DOCTEST_CHECK_FALSE(Append_Decoded_(
            "ships monster drives amoeba_and_hydra = fusion;",
            &writes,
            &diagnostic));
        writes.writes.clear();
        DOCTEST_CHECK_FALSE(Append_Decoded_(
            "ships monster drives dragon_and_eel = interphased;",
            &writes,
            &diagnostic));
        writes.writes.clear();
        DOCTEST_CHECK_FALSE(Append_Decoded_(
            "ships monster drives guardian = unknown_drive;",
            &writes,
            &diagnostic));

        DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
        DOCTEST_REQUIRE(config::Commit_State(&state));
    }

    DOCTEST_TEST_CASE("individual monster hit points bind independently of shared fallbacks") {
        config::s_config_state state;
        DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
        config::s_config_write_set writes;
        config::s_cfg_diagnostic diagnostic;
        DOCTEST_REQUIRE(Append_Decoded_(
            "ships monster hit_points system_crystal_hydra_dragon = 800;"
            "ships monster hit_points system_crystal = 500;"
            "ships monster hit_points system_hydra = 600;"
            "ships monster hit_points system_dragon = 700;"
            "ships monster hit_points event_crystal = 2500;"
            "ships monster hit_points event_dragon = 3000;"
            "ships monster hit_points event_crystal_dragon = 4000;",
            &writes,
            &diagnostic));
        DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
        DOCTEST_REQUIRE(config::Commit_State(&state));
        DOCTEST_CHECK(SHIP_CONFIG::monster.hit_points.system_crystal_hydra_dragon == 800);
        DOCTEST_CHECK(SHIP_CONFIG::monster.hit_points.system_crystal == 500);
        DOCTEST_CHECK(SHIP_CONFIG::monster.hit_points.system_hydra == 600);
        DOCTEST_CHECK(SHIP_CONFIG::monster.hit_points.system_dragon == 700);
        DOCTEST_CHECK(SHIP_CONFIG::monster.hit_points.event_crystal == 2500);
        DOCTEST_CHECK(SHIP_CONFIG::monster.hit_points.event_dragon == 3000);
        DOCTEST_CHECK(SHIP_CONFIG::monster.hit_points.event_crystal_dragon == 4000);
        DOCTEST_CHECK(SHIP_CONFIG::monster.hit_points.event_hydra == 1500);

        const char* fields[] = {
            "system_crystal",
            "system_hydra",
            "system_dragon",
            "event_crystal",
            "event_dragon",
        };
        for (int value : {0, 32767, -1}) {
            writes.writes.clear();
            for (const char* field : fields) {
                const std::string source = std::string("ships monster hit_points ") +
                    field + " = " + std::to_string(value) + ";";
                DOCTEST_REQUIRE(Append_Decoded_(source.c_str(), &writes, &diagnostic));
            }
            DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
            DOCTEST_REQUIRE(config::Commit_State(&state));
            DOCTEST_CHECK(SHIP_CONFIG::monster.hit_points.system_crystal == value);
            DOCTEST_CHECK(SHIP_CONFIG::monster.hit_points.system_hydra == value);
            DOCTEST_CHECK(SHIP_CONFIG::monster.hit_points.system_dragon == value);
            DOCTEST_CHECK(SHIP_CONFIG::monster.hit_points.event_crystal == value);
            DOCTEST_CHECK(SHIP_CONFIG::monster.hit_points.event_dragon == value);
        }
        for (const char* field : fields) {
            for (int value : {-2, 32768}) {
                writes.writes.clear();
                const std::string source = std::string("ships monster hit_points ") +
                    field + " = " + std::to_string(value) + ";";
                DOCTEST_CHECK_FALSE(Append_Decoded_(source.c_str(), &writes, &diagnostic));
            }
        }

        DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
        DOCTEST_REQUIRE(config::Commit_State(&state));
    }

    DOCTEST_TEST_CASE("Crystal variants use one shared equipment record") {
        config::s_config_state state;
        DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
        DOCTEST_REQUIRE(config::Commit_State(&state));

        DOCTEST_CHECK(SHIP_CONFIG::crystal.shared.shield == SHIELD_NO_SHIELD);
        DOCTEST_CHECK(SHIP_CONFIG::crystal.shared.computer ==
            COMPUTER_MOLECULARTRONIC_COMPUTER);
        DOCTEST_CHECK(SHIP_CONFIG::crystal.shared.special == 0);

        config::s_config_write_set writes;
        config::s_cfg_diagnostic diagnostic;
        DOCTEST_REQUIRE(Append_Decoded_(
            "ships crystal shared shield = class_iii;"
            "ships crystal shared computer = positronic;"
            "ships crystal shared special = battle_scanner;",
            &writes,
            &diagnostic));
        DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
        DOCTEST_REQUIRE(config::Commit_State(&state));

        DOCTEST_CHECK(SHIP_CONFIG::crystal.shared.shield ==
            SHIELD_CLASS_III_SHIELD);
        DOCTEST_CHECK(SHIP_CONFIG::crystal.shared.computer ==
            COMPUTER_POSITRONIC_COMPUTER);
        DOCTEST_CHECK(SHIP_CONFIG::crystal.shared.special ==
            SPECIAL_BATTLE_SCANNER);

        writes.writes.clear();
        DOCTEST_CHECK_FALSE(Append_Decoded_(
            "ships crystal system shield = class_i;",
            &writes,
            &diagnostic));
        writes.writes.clear();
        DOCTEST_CHECK_FALSE(Append_Decoded_(
            "ships crystal event specials = battle_scanner,regeneration;",
            &writes,
            &diagnostic));
        writes.writes.clear();
        DOCTEST_CHECK_FALSE(Append_Decoded_(
            "ships crystal shared special = 40;",
            &writes,
            &diagnostic));

        DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
        DOCTEST_REQUIRE(config::Commit_State(&state));
    }

    DOCTEST_TEST_CASE("Hydra, Dragon, and Guardian templates use canonical bindings") {
        config::s_config_state state;
        DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
        DOCTEST_REQUIRE(config::Commit_State(&state));

        DOCTEST_CHECK(SHIP_CONFIG::hydra_templates[0].picture_num == 24);
        DOCTEST_CHECK(SHIP_CONFIG::hydra_templates[0].ship_weapon[0].type ==
            WEAPON_PLASMA_BREATH);
        DOCTEST_CHECK(SHIP_CONFIG::hydra_templates[0].ship_weapon[0].count == 3);
        DOCTEST_CHECK(SHIP_CONFIG::dragon_templates[0].computer_type ==
            COMPUTER_MOLECULARTRONIC_COMPUTER);
        DOCTEST_CHECK((SHIP_CONFIG::dragon_templates[0].ship_weapon[0].specials ==
            WEAPON_MOD_FLAG_POINT_DEFENSE));
        DOCTEST_CHECK((SHIP_CONFIG::dragon_templates[1].ship_weapon[1].specials ==
            WEAPON_MOD_FLAG_OVERLOADED));
        DOCTEST_CHECK(SHIP_CONFIG::guardian_template.computer_type ==
            COMPUTER_MOLECULARTRONIC_COMPUTER);

        config::s_config_write_set writes;
        config::s_cfg_diagnostic diagnostic;
        DOCTEST_REQUIRE(Append_Decoded_(
            "ships hydra system weapons 1 mods = hv;"
            "ships dragon system shield = class_i;"
            "ships dragon system computer = electronic;"
            "ships dragon system weapons 2 mods = hv;"
            "ships dragon event shield = class_iii;"
            "ships dragon event computer = optronic;"
            "ships dragon event weapons 3 = plasma_breath all_sectors none 0 2;"
            "ships guardian weapons 1 amount = 99;",
            &writes,
            &diagnostic));
        DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
        DOCTEST_REQUIRE(config::Commit_State(&state));

        DOCTEST_CHECK((SHIP_CONFIG::hydra_templates[0].ship_weapon[0].specials ==
            WEAPON_MOD_FLAG_HEAVY_MOUNT));
        DOCTEST_CHECK(SHIP_CONFIG::dragon_templates[0].shield_type ==
            SHIELD_CLASS_I_SHIELD);
        DOCTEST_CHECK(SHIP_CONFIG::dragon_templates[0].computer_type ==
            COMPUTER_ELECTRONIC_COMPUTER);
        DOCTEST_CHECK((SHIP_CONFIG::dragon_templates[0].ship_weapon[1].specials ==
            WEAPON_MOD_FLAG_HEAVY_MOUNT));
        DOCTEST_CHECK(SHIP_CONFIG::dragon_templates[1].shield_type ==
            SHIELD_CLASS_III_SHIELD);
        DOCTEST_CHECK(SHIP_CONFIG::dragon_templates[1].computer_type ==
            COMPUTER_OPTRONIC_COMPUTER);
        DOCTEST_CHECK((SHIP_CONFIG::dragon_templates[1].ship_weapon[2].type ==
            WEAPON_PLASMA_BREATH));
        DOCTEST_CHECK(SHIP_CONFIG::dragon_templates[1].ship_weapon[2].count == 2);
        DOCTEST_CHECK(SHIP_CONFIG::dragon_templates[1].ship_weapon[2].current_count == 2);
        DOCTEST_CHECK(SHIP_CONFIG::guardian_template.ship_weapon[0].count == 99);
        DOCTEST_CHECK(SHIP_CONFIG::guardian_template.ship_weapon[0].current_count == 99);

        writes.writes.clear();
        DOCTEST_CHECK_FALSE(Append_Decoded_(
            "ships guardian weapons 1 amount = 100;", &writes, &diagnostic));

        DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
        DOCTEST_REQUIRE(config::Commit_State(&state));
    }

    DOCTEST_TEST_CASE("predefined ship ammo preserves its signed semantic domain") {
        config::s_config_state state;
        DOCTEST_REQUIRE(config::Initialize_Defaults(&state));

        config::s_config_write_set writes;
        config::s_cfg_diagnostic diagnostic;
        DOCTEST_REQUIRE(Append_Decoded_(
            "ships amoeba system weapons 1 ammo = -1;"
            "ships crystal system weapons 1 ammo = 0;"
            "ships eel system weapons 1 ammo = 127;"
            "ships hydra event weapons 1 amount = 99;",
            &writes,
            &diagnostic));
        DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
        DOCTEST_REQUIRE(config::Commit_State(&state));

        DOCTEST_CHECK(SHIP_CONFIG::amoeba_templates[0].ship_weapon[0].ammo == -1);
        DOCTEST_CHECK(SHIP_CONFIG::crystal.templates[0].ship_weapon[0].ammo == 0);
        DOCTEST_CHECK(SHIP_CONFIG::eel_templates[0].ship_weapon[0].ammo == 127);
        DOCTEST_CHECK(SHIP_CONFIG::hydra_templates[1].ship_weapon[0].count == 99);
        DOCTEST_CHECK(
            SHIP_CONFIG::hydra_templates[1].ship_weapon[0].current_count == 99);

        writes.writes.clear();
        DOCTEST_CHECK_FALSE(Append_Decoded_(
            "ships eel system weapons 1 ammo = -2;", &writes, &diagnostic));
        writes.writes.clear();
        DOCTEST_CHECK_FALSE(Append_Decoded_(
            "ships eel system weapons 1 ammo = 128;", &writes, &diagnostic));
        writes.writes.clear();
        DOCTEST_CHECK_FALSE(Append_Decoded_(
            "ships hydra event weapons 1 amount = -1;", &writes, &diagnostic));
        writes.writes.clear();
        DOCTEST_CHECK_FALSE(Append_Decoded_(
            "ships hydra event weapons 1 amount = 100;", &writes, &diagnostic));

        DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
        DOCTEST_REQUIRE(config::Commit_State(&state));
    }

    DOCTEST_TEST_CASE("captured native defaults restore complete predefined ship templates") {
        const s_ship_design guardian = SHIP_CONFIG::guardian_template;
        const s_ship_design avenger = SHIP_CONFIG::avenger_template;
        s_ship_design amoebas[2];
        s_ship_design crystals[2];
        s_ship_design dragons[2];
        s_ship_design eels[2];
        s_ship_design hydras[2];
        s_ship_design offensive_antarans[GAME_DIFFICULTY_COUNT][5];
        s_ship_design defensive_antarans[GAME_DIFFICULTY_COUNT][5];
        std::memcpy(amoebas, SHIP_CONFIG::amoeba_templates, sizeof(amoebas));
        std::memcpy(crystals, SHIP_CONFIG::crystal.templates, sizeof(crystals));
        std::memcpy(dragons, SHIP_CONFIG::dragon_templates, sizeof(dragons));
        std::memcpy(eels, SHIP_CONFIG::eel_templates, sizeof(eels));
        std::memcpy(hydras, SHIP_CONFIG::hydra_templates, sizeof(hydras));
        std::memcpy(offensive_antarans, SHIP_CONFIG::antaran_offensive_templates, sizeof(offensive_antarans));
        std::memcpy(defensive_antarans, SHIP_CONFIG::antaran_defensive_templates, sizeof(defensive_antarans));

        config::s_config_state defaults;
        DOCTEST_REQUIRE(config::Initialize_Defaults(&defaults));
        DOCTEST_REQUIRE(config::Commit_State(&defaults));

        DOCTEST_CHECK(std::memcmp(&guardian, &SHIP_CONFIG::guardian_template, sizeof(guardian)) == 0);
        DOCTEST_CHECK(std::memcmp(&avenger, &SHIP_CONFIG::avenger_template, sizeof(avenger)) == 0);
        DOCTEST_CHECK(std::memcmp(amoebas, SHIP_CONFIG::amoeba_templates, sizeof(amoebas)) == 0);
        DOCTEST_CHECK(std::memcmp(crystals, SHIP_CONFIG::crystal.templates, sizeof(crystals)) == 0);
        DOCTEST_CHECK(std::memcmp(dragons, SHIP_CONFIG::dragon_templates, sizeof(dragons)) == 0);
        DOCTEST_CHECK(std::memcmp(eels, SHIP_CONFIG::eel_templates, sizeof(eels)) == 0);
        DOCTEST_CHECK(std::memcmp(hydras, SHIP_CONFIG::hydra_templates, sizeof(hydras)) == 0);
        DOCTEST_CHECK(std::memcmp(offensive_antarans,
                          SHIP_CONFIG::antaran_offensive_templates,
                          sizeof(offensive_antarans)) == 0);
        DOCTEST_CHECK(std::memcmp(defensive_antarans,
                          SHIP_CONFIG::antaran_defensive_templates,
                          sizeof(defensive_antarans)) == 0);

        SHIP_CONFIG::guardian_template.ship_weapon[0].count = 99;
        SHIP_CONFIG::guardian_template.special_device_flags[4] = 0;
        SHIP_CONFIG::antaran_offensive_templates[GAME_DIFFICULTY_IMPOSSIBLE]
                                                [SHIP_SIZE_TITAN]
                                                    .ship_weapon[7]
                                                    .current_count = 0;
        SHIP_CONFIG::antaran_defensive_templates[GAME_DIFFICULTY_AVERAGE]
                                                [SHIP_SIZE_TITAN]
                                                    .ship_weapon[7]
                                                    .current_count = 0;

        DOCTEST_REQUIRE(config::Initialize_Defaults(&defaults));
        DOCTEST_REQUIRE(config::Commit_State(&defaults));

        DOCTEST_CHECK(SHIP_CONFIG::guardian_template.ship_weapon[0].count == 2);
        DOCTEST_CHECK(SHIP_CONFIG::guardian_template.ship_weapon[0].current_count == 2);
        DOCTEST_CHECK((SHIP_CONFIG::guardian_template.special_device_flags[3] & 0x02) != 0);
        DOCTEST_CHECK(SHIP_CONFIG::avenger_template.ship_weapon[3].firing_arc ==
            WEAPON_FIRING_ARC_360);
        DOCTEST_CHECK(SHIP_CONFIG::avenger_template.ship_weapon[5].firing_arc ==
            WEAPON_FIRING_ARC_360);
        DOCTEST_CHECK(SHIP_CONFIG::antaran_offensive_templates[GAME_DIFFICULTY_IMPOSSIBLE]
                                                              [SHIP_SIZE_TITAN]
                                                                  .ship_weapon[7]
                                                                  .current_count == 12);
        DOCTEST_CHECK(SHIP_CONFIG::antaran_defensive_templates[GAME_DIFFICULTY_AVERAGE]
                                                              [SHIP_SIZE_TITAN]
                                                                  .ship_weapon[7]
                                                                  .current_count == 6);
    }

    DOCTEST_TEST_CASE("outpost prompt setting preserves classic behavior by default") {
        config::s_config_state state;
        DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
        DOCTEST_REQUIRE(config::Commit_State(&state));
        DOCTEST_CHECK(REPORT::no_outpost_prompt == 0);

        config::s_config_write_set writes;
        config::s_cfg_diagnostic diagnostic;
        DOCTEST_REQUIRE(Append_Decoded_(
            "interface no_outpost_prompt = true;", &writes, &diagnostic));
        DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
        DOCTEST_REQUIRE(config::Commit_State(&state));
        DOCTEST_CHECK(REPORT::no_outpost_prompt == 1);
    }

    DOCTEST_TEST_CASE("native annihilation is disabled by default") {
        config::s_config_state state;
        DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
        DOCTEST_REQUIRE(config::Commit_State(&state));
        DOCTEST_CHECK(COLONY::allow_native_annihilation == 0);

        config::s_config_write_set writes;
        config::s_cfg_diagnostic diagnostic;
        DOCTEST_REQUIRE(Append_Decoded_(
            "gameplay allow_native_annihilation = true;", &writes, &diagnostic));
        DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
        DOCTEST_REQUIRE(config::Commit_State(&state));
        DOCTEST_CHECK(COLONY::allow_native_annihilation == 1);
    }

    DOCTEST_TEST_CASE("rebel troop display preserves classic behavior by default") {
        config::s_config_state state;
        DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
        DOCTEST_REQUIRE(config::Commit_State(&state));
        DOCTEST_CHECK(NEWPUP::rebel_display == 0);

        config::s_config_write_set writes;
        config::s_cfg_diagnostic diagnostic;
        DOCTEST_REQUIRE(Append_Decoded_(
            "interface rebel_display = true;", &writes, &diagnostic));
        DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
        DOCTEST_REQUIRE(config::Commit_State(&state));
        DOCTEST_CHECK(NEWPUP::rebel_display == 1);

        writes.writes.clear();
        DOCTEST_CHECK_FALSE(Append_Decoded_(
            "interface rebel_display = enabled;", &writes, &diagnostic));

        DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
        DOCTEST_REQUIRE(config::Commit_State(&state));
    }

    DOCTEST_TEST_CASE("interface text formats preserve 1.50 defaults and bind strings") {
        config::s_config_state state;
        DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
        DOCTEST_REQUIRE(config::Commit_State(&state));

        DOCTEST_CHECK(std::strcmp(
                          TEXT_FORMAT::format[TEXT_FORMAT::ROW_PROD].template_text,
                          "$TURNS$ turns") == 0);
        DOCTEST_CHECK(TEXT_FORMAT::format[TEXT_FORMAT::ROW_PROD].align ==
            TEXT_FORMAT::ALIGN_LEFT);
        DOCTEST_CHECK(std::strcmp(
                          TEXT_FORMAT::format[TEXT_FORMAT::ROW_LEADER_XP].template_text,
                          "$XP$/$NEXTLVL$ EP") == 0);
        DOCTEST_CHECK(TEXT_FORMAT::format[TEXT_FORMAT::ROW_LEADER_XP].align ==
            TEXT_FORMAT::ALIGN_RIGHT);

        config::s_config_write_set writes;
        config::s_cfg_diagnostic diagnostic;
        DOCTEST_REQUIRE(Append_Decoded_(
            "interface format prod template = \"$TURNS$ cycles\";"
            "interface format prod align = c;"
            "interface format leader_xp x = 300;",
            &writes,
            &diagnostic));
        DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
        DOCTEST_REQUIRE(config::Commit_State(&state));

        DOCTEST_CHECK(std::strcmp(
                          TEXT_FORMAT::format[TEXT_FORMAT::ROW_PROD].template_text,
                          "$TURNS$ cycles") == 0);
        DOCTEST_CHECK(TEXT_FORMAT::format[TEXT_FORMAT::ROW_PROD].align ==
            TEXT_FORMAT::ALIGN_CENTER);
        DOCTEST_CHECK(TEXT_FORMAT::format[TEXT_FORMAT::ROW_LEADER_XP].x == 300);
    }

    DOCTEST_TEST_CASE("online router is an unsaved local interface setting") {
        config::s_config_state state;
        DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
        DOCTEST_REQUIRE(config::Commit_State(&state));
        DOCTEST_CHECK(MULTPLAY::router_endpoint[0] == '\0');

        config::s_config_write_set writes;
        config::s_cfg_diagnostic diagnostic;
        DOCTEST_REQUIRE(Append_Decoded_(
            "interface router = \"router.example.com:47800\";",
            &writes,
            &diagnostic));
        DOCTEST_REQUIRE(config::Apply_Write_Set(&writes, &state, &diagnostic));
        DOCTEST_REQUIRE(config::Commit_State(&state));
        DOCTEST_CHECK(std::strcmp(
                          MULTPLAY::router_endpoint,
                          "router.example.com:47800") == 0);
    }
}