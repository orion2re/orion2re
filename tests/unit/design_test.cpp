#include "pch.h"

#include <cstring>

#include "doctest/doctest.h"

namespace {
    struct s_damage_text_fixture {
        s_current_design design = {};
        s_current_design* saved_design = MOX::design;
        decltype(MOX::weapon_mod_field_status) saved_mods;
        decltype(MOX::h_string_ix) saved_indexes;
        decltype(TECHDATA::weapons) saved_weapons;
        std::vector<char> saved_strings;
        decltype(DESIGN::death_spore_list_damage) saved_spore = DESIGN::death_spore_list_damage;
        decltype(DESIGN::bio_terminator_list_damage) saved_bio = DESIGN::bio_terminator_list_damage;

        s_damage_text_fixture() {
            memcpy(saved_mods, MOX::weapon_mod_field_status, sizeof(saved_mods));
            memcpy(saved_indexes, MOX::h_string_ix, sizeof(saved_indexes));
            memcpy(saved_weapons, TECHDATA::weapons, sizeof(saved_weapons));
            MOX::h_strings.swap(saved_strings);
            const char messages[] = "\0special";
            MOX::h_strings.assign(messages, messages + sizeof(messages));
            memset(MOX::h_string_ix, 0, sizeof(MOX::h_string_ix));
            MOX::h_string_ix[HESTR_065_SPECIAL] = 1;
            MOX::design = &design;
            memset(MOX::weapon_mod_field_status, 0, sizeof(MOX::weapon_mod_field_status));
        }

        ~s_damage_text_fixture() {
            MOX::design = saved_design;
            memcpy(MOX::weapon_mod_field_status, saved_mods, sizeof(saved_mods));
            memcpy(MOX::h_string_ix, saved_indexes, sizeof(saved_indexes));
            memcpy(TECHDATA::weapons, saved_weapons, sizeof(saved_weapons));
            MOX::h_strings.swap(saved_strings);
            DESIGN::death_spore_list_damage = saved_spore;
            DESIGN::bio_terminator_list_damage = saved_bio;
        }
    };

    void Check_Weapon_Damage_String_(int16_t weapon_id, const char* expected) {
        char buffer[32];
        DESIGN::Design_Weapon_Damage_String(weapon_id, buffer, sizeof(buffer), 0, 0);
        DOCTEST_CHECK(std::strcmp(buffer, expected) == 0);
    }
}

DOCTEST_TEST_CASE("Death Spore and Bio Terminator damage displays are independently configurable") {
    s_damage_text_fixture fixture;
    TECHDATA::weapons[WEAPON_DEATH_SPORE].min_damage = 4;
    TECHDATA::weapons[WEAPON_DEATH_SPORE].max_damage = 6;
    TECHDATA::weapons[WEAPON_BIO_TERMINATOR].min_damage = 8;
    TECHDATA::weapons[WEAPON_BIO_TERMINATOR].max_damage = 8;

    DESIGN::death_spore_list_damage = 0;
    DESIGN::bio_terminator_list_damage = 0;
    Check_Weapon_Damage_String_(WEAPON_DEATH_SPORE, "special");
    Check_Weapon_Damage_String_(WEAPON_BIO_TERMINATOR, "special");
    Check_Weapon_Damage_String_(WEAPON_STASIS_FIELD, "special");

    DESIGN::death_spore_list_damage = 1;
    Check_Weapon_Damage_String_(WEAPON_DEATH_SPORE, "4-6");
    Check_Weapon_Damage_String_(WEAPON_BIO_TERMINATOR, "special");

    DESIGN::bio_terminator_list_damage = 1;
    Check_Weapon_Damage_String_(WEAPON_BIO_TERMINATOR, "8");
    Check_Weapon_Damage_String_(WEAPON_STASIS_FIELD, "special");
}