#include "pch.h"

#include "doctest/doctest.h"
#include "game/cmbtfir2.h"

namespace TEST_ABSORBER_CLEANUP {
    void Reset_();
    void Set_Special_(int16_t ship_idx, int16_t special_id);
    int Warning_Count_();
    const char* Last_Warning_();
    int Combat_Refresh_Count_();
}

DOCTEST_TEST_CASE("absorber overload uses the overload explosion") {
    TEST_ABSORBER_CLEANUP::Reset_();
    MOX::combat_data[1].reflected_damage_pool = INT16_MAX;
    TEST_ABSORBER_CLEANUP::Set_Special_(1, SPECIAL_QUANTUM_DETONATOR);

    DOCTEST_CHECK(
        CMBTFIR2::Damage_From_Ship_Exploding(1) == 30 * 5 * 77);
}

DOCTEST_TEST_CASE("visible absorber overload displays its warning") {
    TEST_ABSORBER_CLEANUP::Reset_();
    MOX::combat_data[1].reflected_damage_pool = INT16_MAX;
    MOX::apply_damage_to_planet = 1;

    CMBTFIR2::Damage_From_Ship_Exploding(1);

    DOCTEST_CHECK(TEST_ABSORBER_CLEANUP::Warning_Count_() == 1);
    DOCTEST_CHECK(
        std::string(TEST_ABSORBER_CLEANUP::Last_Warning_()) ==
        "Absorber Overloaded");
    DOCTEST_CHECK(TEST_ABSORBER_CLEANUP::Combat_Refresh_Count_() == 1);
}

DOCTEST_TEST_CASE("quantum detonator keeps its normal explosion multiplier") {
    TEST_ABSORBER_CLEANUP::Reset_();
    TEST_ABSORBER_CLEANUP::Set_Special_(1, SPECIAL_QUANTUM_DETONATOR);

    DOCTEST_CHECK(
        CMBTFIR2::Damage_From_Ship_Exploding(1) == 30 * 5 * 3);
}

DOCTEST_TEST_CASE("configured explosion multipliers affect their respective damage stages") {
    TEST_ABSORBER_CLEANUP::Reset_();
    CMBTFIR2::ship_explosion = {7, 4};

    DOCTEST_CHECK(CMBTFIR2::Damage_From_Ship_Exploding(1) == 30 * 7);

    TEST_ABSORBER_CLEANUP::Set_Special_(1, SPECIAL_QUANTUM_DETONATOR);
    DOCTEST_CHECK(CMBTFIR2::Damage_From_Ship_Exploding(1) == 30 * 7 * 4);
}

DOCTEST_TEST_CASE("computerless ships have no damageable computer subsystem") {
    TEST_ABSORBER_CLEANUP::Reset_();
    s_combat_data* ship = &MOX::combat_data[1];
    TECHDATA::hull_data[HULL_CRUISER].computer_hp = 5;

    ship->computer_type = COMPUTER_NO_COMPUTER;
    CMBTFIR2::Init_Internal_Space(ship);

    DOCTEST_CHECK(ship->computer_max_hits == 0);
    DOCTEST_CHECK(ship->computer_current_hits == 0);

    ship->computer_type = COMPUTER_ELECTRONIC_COMPUTER;
    CMBTFIR2::Init_Internal_Space(ship);

    DOCTEST_CHECK(ship->computer_max_hits == 5);
    DOCTEST_CHECK(ship->computer_current_hits == 5);
}

DOCTEST_TEST_CASE("bomb and anti-missile rocket subsystem hits ignore rack capacity") {
    TEST_ABSORBER_CLEANUP::Reset_();
    s_combat_data* ship = &MOX::combat_data[1];

    ship->weapons[0].weapon_id = WEAPON_NUCLEAR_BOMB;
    ship->weapons[0].count = 2;
    ship->weapons[0].ammo = 10;
    TECHDATA::weapons[WEAPON_NUCLEAR_BOMB].type = WEAPON_TYPE_BOMB;

    ship->weapons[1].weapon_id = WEAPON_ANTI_MISSILE_ROCKET;
    ship->weapons[1].count = 1;
    ship->weapons[1].ammo = 20 * ship->weapons[1].count;

    ship->weapons[2].weapon_id = WEAPON_ANTI_MISSILE_ROCKET;
    ship->weapons[2].count = 2;
    ship->weapons[2].ammo = 20 * ship->weapons[2].count;

    ship->weapons[3].weapon_id = WEAPON_NUCLEAR_MISSILE;
    ship->weapons[3].count = 2;
    ship->weapons[3].ammo = 20;

    CMBTFIR2::Init_Internal_Space(ship);

    DOCTEST_CHECK(ship->weapon_unit_hits[0] == 2);
    DOCTEST_CHECK(ship->weapon_total_hits[0] == 4);
    DOCTEST_CHECK(ship->weapon_unit_hits[1] == 2);
    DOCTEST_CHECK(ship->weapon_total_hits[1] == 2);
    DOCTEST_CHECK(ship->weapon_unit_hits[2] == 2);
    DOCTEST_CHECK(ship->weapon_total_hits[2] == 4);
    DOCTEST_CHECK(ship->weapon_unit_hits[3] == 6);
    DOCTEST_CHECK(ship->weapon_total_hits[3] == 12);
}

DOCTEST_TEST_CASE("configured subsystem hit points retain the unsigned word domain") {
    const uint16_t hit_values[] = {32767, 32768, UINT16_MAX};

    for (uint16_t hit_value : hit_values) {
        TEST_ABSORBER_CLEANUP::Reset_();
        s_combat_data* ship = &MOX::combat_data[1];
        ship->ftl_type = DRIVE_NUCLEAR_DRIVE;
        ship->computer_type = COMPUTER_ELECTRONIC_COMPUTER;
        ship->shield_type = SHIELD_CLASS_I_SHIELD;
        TECHDATA::hull_data[HULL_CRUISER].drive_hp = hit_value;
        TECHDATA::hull_data[HULL_CRUISER].computer_hp = hit_value;
        TECHDATA::hull_data[HULL_CRUISER].shield_hp = hit_value;

        CMBTFIR2::Init_Internal_Space(ship);

        DOCTEST_CHECK(ship->drive_max_hits == hit_value);
        DOCTEST_CHECK(ship->drive_current_hits == hit_value);
        DOCTEST_CHECK(ship->computer_max_hits == hit_value);
        DOCTEST_CHECK(ship->computer_current_hits == hit_value);
        DOCTEST_CHECK(ship->shield_max_hits == hit_value);
        DOCTEST_CHECK(ship->shield_current_hits == hit_value);
    }
}

DOCTEST_TEST_CASE("configured systems repair percentage scales internal repair") {
    TEST_ABSORBER_CLEANUP::Reset_();
    s_combat_data* ship = &MOX::combat_data[1];
    ship->ship_idx = 0;
    ship->structure_damage = 100;
    ship->drive_max_hits = 200;
    ship->drive_current_hits = 0;
    CMBTFIR2::systems_repair_percent = 25;

    CMBTFIR2::Repair_Combat_Ship(1, 20);

    DOCTEST_CHECK(ship->drive_current_hits == 5);
}

DOCTEST_TEST_CASE("internal repair pool follows signed machine-width boundaries") {
    TEST_ABSORBER_CLEANUP::Reset_();
    s_combat_data* ship = &MOX::combat_data[1];
    ship->ship_idx = 0;
    ship->drive_max_hits = 200;
    ship->drive_current_hits = 0;
    ship->computer_max_hits = 19900;
    ship->computer_current_hits = 19900;
    ship->weapon_total_hits[0] = 20000;
    ship->weapon_hits[0] = 20000;
    CMBTFIR2::systems_repair_percent = UINT16_MAX;

    CMBTFIR2::Repair_Combat_Ship(1, 255);

    DOCTEST_CHECK(ship->drive_current_hits == 1);
}

DOCTEST_TEST_CASE("satellite internals use station hull rows") {
    TEST_ABSORBER_CLEANUP::Reset_();
    s_combat_data* ship = &MOX::combat_data[1];
    ship->ship_idx = -1;
    ship->ftl_type = DRIVE_NO_DRIVE;
    ship->size_class = SHIP_SIZE_HUGE;
    ship->computer_type = COMPUTER_ELECTRONIC_COMPUTER;
    ship->shield_type = SHIELD_CLASS_I_SHIELD;
    COMBINIT::satellite_size_class_plus_one = 1;

    TECHDATA::hull_data[HULL_STAR_BASE].computer_hp = 11;
    TECHDATA::hull_data[HULL_STAR_BASE].shield_hp = 13;

    CMBTFIR2::Init_Internal_Space(ship);

    DOCTEST_CHECK(ship->computer_max_hits == 11);
    DOCTEST_CHECK(ship->shield_max_hits == 13);
}

DOCTEST_TEST_CASE("station repair uses each station armor row for armor bonuses") {
    TEST_ABSORBER_CLEANUP::Reset_();
    s_combat_data* ship = &MOX::combat_data[1];
    ship->ship_idx = -1;
    ship->ftl_type = DRIVE_NO_DRIVE;
    ship->size_class = SHIP_SIZE_HUGE;
    ship->armor_type = ARMOR_TRITANIUM_ARMOR;
    ship->structure_damage = 300;

    TECHDATA::hull_data[HULL_STAR_BASE].armor_hp = 100;
    TECHDATA::hull_data[HULL_BATTLESTATION].armor_hp = 200;
    TECHDATA::armor[ARMOR_TRITANIUM_ARMOR].ships_bonus = 50;

    CMBTFIR2::Repair_Combat_Ship(1, 100);

    DOCTEST_CHECK(ship->armor_remaining == 100);
}

DOCTEST_TEST_CASE("combat satellite identity distinguishes Antaran combat records") {
    TEST_ABSORBER_CLEANUP::Reset_();

    s_combat_data& planet = MOX::combat_data[0];
    planet.owner = MAX_PLAYERS;
    planet.ship_idx = -1;
    planet.ftl_type = DRIVE_NO_DRIVE;
    DOCTEST_CHECK_FALSE(COMBINIT::Is_Combat_Satellite(&planet));

    s_combat_data& ship = MOX::combat_data[1];
    ship.owner = MAX_PLAYERS;
    ship.previous_owner = MAX_PLAYERS;
    ship.ship_idx = -1;
    ship.officer_idx = -1;
    ship.ftl_type = DRIVE_HYPER_DRIVE;
    DOCTEST_CHECK_FALSE(COMBINIT::Is_Combat_Satellite(&ship));

    ship.ftl_type = DRIVE_NO_DRIVE;
    ship.size_class = SHIP_SIZE_TITAN;
    DOCTEST_CHECK(COMBINIT::Is_Generated_Antaran_Combat_Ship(&ship));
    DOCTEST_CHECK_FALSE(COMBINIT::Is_Combat_Satellite(&ship));

    ship.previous_owner = 0;
    DOCTEST_CHECK(COMBINIT::Is_Combat_Satellite(&ship));
    DOCTEST_CHECK(COMBINIT::Combat_Satellite_Hull_Class(&ship) ==
                  HULL_STAR_FORTRESS);

    COMBINIT::satellite_size_class_plus_one = 1;
    ship.size_class = SHIP_SIZE_DOOM_STAR;
    DOCTEST_CHECK(COMBINIT::Combat_Satellite_Hull_Class(&ship) ==
                  HULL_STAR_FORTRESS);
}