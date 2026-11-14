#include "pch.h"

#include <cstring>
#include <string>
#include <vector>

#include "doctest/doctest.h"
#include "game/config_refresh.h"
#include "game/techinit.h"

namespace TEST_TECHINIT {
    void Reset_();
    void Set_Tech_Name_Block_(const std::string& value);
    char* Copy_Config_String_(const char* value);
    int Reload_Count_();
    int Resource_Request_Count_();
    int Grant_Count_();
    int16_t Granted_App_(int index);
}

namespace {
    constexpr size_t TECH_NAME_COUNT =
        (size_t)TECH_FIELD_COUNT + (size_t)TECH_APP_COUNT + (size_t)BUILDING_COUNT +
        (size_t)SPECIAL_COUNT + (size_t)ARMOR_COUNT + (size_t)SHIELD_COUNT +
        (size_t)WEAPON_COUNT + (size_t)COMPUTER_COUNT + (size_t)DRIVE_COUNT +
        (size_t)UNIT_COUNT + (size_t)FUEL_COUNT + (size_t)WEAPON_MOD_COUNT * 2 +
        (size_t)WEAPON_COUNT + (size_t)HULL_CLASS_COUNT * 2;

    std::string Tech_Name_Block_(const char* prefix) {
        std::string block;
        for (size_t index = 0; index < TECH_NAME_COUNT; ++index) {
            block += prefix + std::to_string(index);
            block.push_back('\0');
        }
        return block;
    }

    char* Name_At_(std::string* block, size_t target_index) {
        size_t offset = 0;
        for (size_t index = 0; index < target_index; ++index) {
            offset = block->find('\0', offset) + 1;
        }
        return block->data() + offset;
    }
}

DOCTEST_TEST_CASE("configuration refresh waits for the first technology name load") {
    TEST_TECHINIT::Reset_();
    std::string block = Tech_Name_Block_("ready-");
    TEST_TECHINIT::Set_Tech_Name_Block_(block);

    CONFIG_REFRESH::Refresh_After_Config_Group_Reload(
        config::CONFIG_GROUP_TECHNOLOGY);
    DOCTEST_CHECK(TEST_TECHINIT::Reload_Count_() == 0);

    TECHINIT::Load_Tech_Names();
    CONFIG_REFRESH::Refresh_After_Config_Group_Reload(
        config::CONFIG_GROUP_INTERFACE);
    DOCTEST_CHECK(TEST_TECHINIT::Reload_Count_() == 1);
    CONFIG_REFRESH::Refresh_After_Config_Group_Reload(
        config::CONFIG_GROUP_TECHNOLOGY);
    DOCTEST_CHECK(TEST_TECHINIT::Reload_Count_() == 2);
    CONFIG_REFRESH::Refresh_After_Config_Reload(nullptr);
    DOCTEST_CHECK(TEST_TECHINIT::Reload_Count_() == 3);
    DOCTEST_CHECK(TEST_TECHINIT::Resource_Request_Count_() == 1);
    bool groups[config::CONFIG_GROUP_COUNT] = {};
    groups[config::CONFIG_GROUP_TECHNOLOGY] = true;
    CONFIG_REFRESH::Refresh_After_Config_Reload(groups);
    DOCTEST_CHECK(TEST_TECHINIT::Reload_Count_() == 4);
    // A reloaded group schedules a resource reload (the group can feed
    // resources), so this is the second request: one from the nullptr
    // reload above, one from this group reload.
    DOCTEST_CHECK(TEST_TECHINIT::Resource_Request_Count_() == 2);
}

DOCTEST_TEST_CASE("technology names preserve active configuration strings across LBX reloads") {
    TEST_TECHINIT::Reset_();
    std::string first = Tech_Name_Block_("first-");
    TEST_TECHINIT::Set_Tech_Name_Block_(first);
    TECHINIT::Load_Tech_Names();

    DOCTEST_CHECK(std::strcmp(TECHDATA::technology_fields[0].name,
                              Name_At_(&first, 0)) == 0);
    DOCTEST_CHECK(std::strcmp(TECHDATA::technology_applications[0].name,
                              Name_At_(&first, TECH_FIELD_COUNT)) == 0);
    DOCTEST_CHECK(std::strcmp(TECHDATA::hull_data[HULL_CLASS_COUNT - 1].size_name,
                              Name_At_(&first, TECH_NAME_COUNT - 1)) == 0);

    char* configured = TEST_TECHINIT::Copy_Config_String_("Configured application");
    TECHDATA::technology_applications[0].name = configured;
    TECHDATA::armor[0].name = nullptr;
    char* previous_fallback = TECHDATA::technology_fields[0].name;

    std::string second = Tech_Name_Block_("second-");
    TEST_TECHINIT::Set_Tech_Name_Block_(second);
    TECHINIT::Load_Tech_Names();

    DOCTEST_CHECK(TECHDATA::technology_applications[0].name == configured);
    DOCTEST_CHECK(TECHDATA::technology_fields[0].name != previous_fallback);
    DOCTEST_CHECK(std::strcmp(TECHDATA::technology_fields[0].name,
                              Name_At_(&second, 0)) == 0);
    const size_t armor_start = (size_t)TECH_FIELD_COUNT + (size_t)TECH_APP_COUNT +
                               (size_t)BUILDING_COUNT + (size_t)SPECIAL_COUNT;
    DOCTEST_CHECK(std::strcmp(TECHDATA::armor[0].name,
                              Name_At_(&second, armor_start)) == 0);
    DOCTEST_CHECK(std::strcmp(TECHDATA::hull_data[HULL_CLASS_COUNT - 1].size_name,
                              Name_At_(&second, TECH_NAME_COUNT - 1)) == 0);
}

DOCTEST_TEST_CASE("technology initialization rebuilds derived fields on every call") {
    TEST_TECHINIT::Reset_();
    TEST_TECHINIT::Set_Tech_Name_Block_(Tech_Name_Block_("init-"));

    TECHINIT::Init_Tech();
    std::vector<int16_t> first_fields;
    first_fields.reserve(TECH_FIELD_COUNT * 4);
    for (int field = 0; field < TECH_FIELD_COUNT; ++field) {
        for (int slot = 0; slot < 4; ++slot) {
            first_fields.push_back(TECHDATA::technology_fields[field].tech[slot]);
        }
    }

    TECHINIT::Init_Tech();
    size_t offset = 0;
    for (int field = 0; field < TECH_FIELD_COUNT; ++field) {
        for (int slot = 0; slot < 4; ++slot) {
            DOCTEST_CHECK(TECHDATA::technology_fields[field].tech[slot] ==
                          first_fields[offset++]);
        }
    }
    DOCTEST_CHECK(TECHDATA::technology_applications[
        TECHDATA::buildings[1].tech_app].type == TECH_APPLICATION_TYPE_BUILDING);
}

DOCTEST_TEST_CASE("technology graph validation enforces native source invariants") {
    TEST_TECHINIT::Reset_();
    TEST_TECHINIT::Set_Tech_Name_Block_(Tech_Name_Block_("graph-"));
    TECHINIT::Init_Tech();
    s_tech_app_data applications[TECH_APP_COUNT];
    s_tech_field_data fields[TECH_FIELD_COUNT];
    int16_t starting_fields[6];
    std::memcpy(applications,
        TECHDATA::technology_applications,
        sizeof(applications));
    std::memcpy(fields, TECHDATA::technology_fields, sizeof(fields));
    std::memcpy(starting_fields,
        TECHDATA::starting_tech_field_ids,
        sizeof(starting_fields));
    std::string diagnostic;

    SUBCASE("valid source rebuilds derived membership") {
        applications[TECH_APP_ADVANCED_DAMAGE_CONTROL].tech_field_id =
            TECH_FIELD_PLASMA_PHYSICS;
        DOCTEST_REQUIRE(TECHINIT::Validate_And_Rebuild_Technology(
            applications, fields, starting_fields, &diagnostic));
        bool found = false;
        for (const int16_t application_id :
            fields[TECH_FIELD_PLASMA_PHYSICS].tech) {
            found = found || application_id == TECH_APP_ADVANCED_DAMAGE_CONTROL;
        }
        DOCTEST_CHECK(found);
    }
    SUBCASE("application identity is canonical") {
        applications[1].tech_app_id = 2;
        DOCTEST_CHECK_FALSE(TECHINIT::Validate_And_Rebuild_Technology(
            applications, fields, starting_fields, &diagnostic));
    }
    SUBCASE("sentinel links preserve native termination") {
        fields[TECH_FIELD_STARTING_TECH].next_field_id =
            TECH_FIELD_ADVANCED_BIOLOGY;
        DOCTEST_CHECK_FALSE(TECHINIT::Validate_And_Rebuild_Technology(
            applications, fields, starting_fields, &diagnostic));
    }
    SUBCASE("field identity is canonical") {
        fields[1].tech_field_id = 2;
        DOCTEST_CHECK_FALSE(TECHINIT::Validate_And_Rebuild_Technology(
            applications, fields, starting_fields, &diagnostic));
    }
    SUBCASE("application membership is bounded") {
        for (int16_t application_id = 1; application_id <= 5;
             ++application_id) {
            applications[application_id].tech_field_id =
                TECH_FIELD_PLASMA_PHYSICS;
        }
        DOCTEST_CHECK_FALSE(TECHINIT::Validate_And_Rebuild_Technology(
            applications, fields, starting_fields, &diagnostic));
    }
    SUBCASE("links are reciprocal") {
        fields[TECH_FIELD_MULTI_DIMENSIONAL_PHYSICS].prev_field_id =
            TECH_FIELD_STARTING_TECH;
        DOCTEST_CHECK_FALSE(TECHINIT::Validate_And_Rebuild_Technology(
            applications, fields, starting_fields, &diagnostic));
    }
    SUBCASE("cycles are rejected") {
        fields[TECH_FIELD_PLASMA_PHYSICS].next_field_id =
            TECH_FIELD_PLASMA_PHYSICS;
        fields[TECH_FIELD_PLASMA_PHYSICS].prev_field_id =
            TECH_FIELD_PLASMA_PHYSICS;
        DOCTEST_CHECK_FALSE(TECHINIT::Validate_And_Rebuild_Technology(
            applications, fields, starting_fields, &diagnostic));
    }
    SUBCASE("every branch field is covered") {
        fields[TECH_FIELD_ASTRO_BIOLOGY].next_field_id =
            TECH_FIELD_STARTING_TECH;
        DOCTEST_CHECK_FALSE(TECHINIT::Validate_And_Rebuild_Technology(
            applications, fields, starting_fields, &diagnostic));
    }
    SUBCASE("costs remain within native limits") {
        fields[TECH_FIELD_ADVANCED_BIOLOGY].cost = UINT32_MAX;
        DOCTEST_CHECK_FALSE(TECHINIT::Validate_And_Rebuild_Technology(
            applications, fields, starting_fields, &diagnostic));
    }
    SUBCASE("starting fields are required native fields") {
        starting_fields[0] = TECH_FIELD_COUNT;
        DOCTEST_CHECK_FALSE(TECHINIT::Validate_And_Rebuild_Technology(
            applications, fields, starting_fields, &diagnostic));
    }
}

DOCTEST_TEST_CASE("cybernetic races cannot receive advanced damage control") {
    TEST_TECHINIT::Reset_();
    MOX::player[0].traits[TRAIT_CYBERNETIC] = 1;

    DOCTEST_CHECK(
        TECHINIT::Tech_Is_Legal_For_Player(
            0, TECH_APP_ADVANCED_DAMAGE_CONTROL) == 0);
    DOCTEST_CHECK(
        TECHINIT::Tech_Is_Legal_For_Player(
            0, TECH_APP_ADAMANTIUM_ARMOR) == 1);
}

DOCTEST_TEST_CASE("non-cybernetic races may receive advanced damage control") {
    TEST_TECHINIT::Reset_();

    DOCTEST_CHECK(
        TECHINIT::Tech_Is_Legal_For_Player(
            0, TECH_APP_ADVANCED_DAMAGE_CONTROL) == 1);
}

DOCTEST_TEST_CASE("Lithovore Biomorphic Fungi setting changes only that technology") {
    TEST_TECHINIT::Reset_();
    MOX::player[0].traits[TRAIT_EATS_MINERALS] = 1;

    DOCTEST_CHECK(TECHINIT::Tech_Is_Legal_For_Player(
        0, TECH_APP_BIOMORPHIC_FUNGI) == 0);
    DOCTEST_CHECK(TECHINIT::Tech_Is_Legal_For_Player(
        0, TECH_APP_HYDROPONIC_FARM) == 0);

    TECHINIT::lithovore_allows_biomorphic_fungi = 1;
    DOCTEST_CHECK(TECHINIT::Tech_Is_Legal_For_Player(
        0, TECH_APP_BIOMORPHIC_FUNGI) == 1);
    DOCTEST_CHECK(TECHINIT::Tech_Is_Legal_For_Player(
        0, TECH_APP_HYDROPONIC_FARM) == 0);
}

DOCTEST_TEST_CASE("strategic combat uses the corrected technology tree") {
    TEST_TECHINIT::Reset_();
    MOX::settings.strategic_combat_flag = 1;

    DOCTEST_CHECK(
        TECHINIT::Tech_Is_Legal_For_Player(
            0, TECH_APP_ANTI_MISSILE_ROCKETS) == 0);
    DOCTEST_CHECK(
        TECHINIT::Tech_Is_Legal_For_Player(
            0, TECH_APP_ENERGY_ABSORBER) == 0);
    DOCTEST_CHECK(
        TECHINIT::Tech_Is_Legal_For_Player(
            0, TECH_APP_SECURITY_STATIONS) == 0);
    DOCTEST_CHECK(
        TECHINIT::Tech_Is_Legal_For_Player(
            0, TECH_APP_WARP_DISSIPATER) == 0);
    DOCTEST_CHECK(
        TECHINIT::Tech_Is_Legal_For_Player(
            0, TECH_APP_WARP_INTERDICTOR) == 1);
}

DOCTEST_TEST_CASE("Uncreative races retain applications in modded gray-box fields") {
    TEST_TECHINIT::Reset_();
    TEST_TECHINIT::Set_Tech_Name_Block_(Tech_Name_Block_("graybox-"));
    TECHINIT::Init_Tech();

    s_tech_field_data& field =
        TECHDATA::technology_fields[TECH_FIELD_CHEMISTRY];
    const uint32_t original_cost = field.cost;
    field.cost = original_cost + 1;
    MOX::player[0].traits[TRAIT_UNCREATIVE] = 1;

    TECHINIT::Init_Player_Tech(0);

    DOCTEST_CHECK(
        MOX::player[0].tech_applications[TECH_APP_NUCLEAR_MISSILE] ==
        TECH_RESEARCH_STATUS_AVAILABLE);
    field.cost = original_cost;
}

DOCTEST_TEST_CASE("technology initialization tolerates empty configured fields") {
    TEST_TECHINIT::Reset_();
    TEST_TECHINIT::Set_Tech_Name_Block_(Tech_Name_Block_("empty-field-"));
    TECHINIT::Init_Tech();

    s_tech_field_data& field =
        TECHDATA::technology_fields[TECH_FIELD_ADVANCED_BIOLOGY];
    int16_t original_tech_ids[4];
    std::memcpy(original_tech_ids, field.tech, sizeof(original_tech_ids));
    std::memset(field.tech, 0, sizeof(field.tech));
    MOX::player[0].traits[TRAIT_UNCREATIVE] = 1;

    TECHINIT::Init_Player_Tech(0);

    DOCTEST_CHECK(
        MOX::player[0].tech_applications[TECH_APP_NO_TECH] ==
        TECH_RESEARCH_STATUS_UNAVAILABLE);
    std::memcpy(field.tech, original_tech_ids, sizeof(original_tech_ids));
}

DOCTEST_TEST_CASE("Uncreative initialization falls back when a field has no legal applications") {
    TEST_TECHINIT::Reset_();
    TEST_TECHINIT::Set_Tech_Name_Block_(Tech_Name_Block_("illegal-field-"));
    TECHINIT::Init_Tech();

    s_tech_field_data& field =
        TECHDATA::technology_fields[TECH_FIELD_ADVANCED_BIOLOGY];
    int16_t original_tech_ids[4];
    std::memcpy(original_tech_ids, field.tech, sizeof(original_tech_ids));
    field.tech[0] = TECH_APP_ANDROID_FARMERS;
    field.tech[1] = 0;
    field.tech[2] = 0;
    field.tech[3] = 0;
    MOX::player[0].traits[TRAIT_UNCREATIVE] = 1;
    MOX::player[0].traits[TRAIT_EATS_MINERALS] = 1;

    TECHINIT::Init_Player_Tech(0);

    DOCTEST_CHECK(
        MOX::player[0].tech_applications[TECH_APP_ANDROID_FARMERS] ==
        TECH_RESEARCH_STATUS_AVAILABLE);
    std::memcpy(field.tech, original_tech_ids, sizeof(original_tech_ids));
}

DOCTEST_TEST_CASE("Post-warp uses the Average base civilization tier") {
    DOCTEST_CHECK(STARTING_CIVILIZATION_LEVEL_ADVANCED == 2);
    DOCTEST_CHECK(
        Starting_Civilization_Level_For_Classic_Rules_(
            STARTING_CIVILIZATION_LEVEL_POST_WARP) ==
        STARTING_CIVILIZATION_LEVEL_AVERAGE);
    DOCTEST_CHECK(
        Starting_Civilization_Level_For_Classic_Rules_(
            STARTING_CIVILIZATION_LEVEL_ADVANCED) ==
        STARTING_CIVILIZATION_LEVEL_ADVANCED);
    DOCTEST_CHECK(
        Starting_Civilization_Level_For_Classic_Rules_(4) ==
        STARTING_CIVILIZATION_LEVEL_AVERAGE);
}

DOCTEST_TEST_CASE("Pre-Warp and Average Technology process every configured starting field") {
    TEST_TECHINIT::Reset_();
    TEST_TECHINIT::Set_Tech_Name_Block_(Tech_Name_Block_("starting-fields-"));
    TECHINIT::Init_Tech();

    int16_t original_fields[6];
    std::memcpy(
        original_fields,
        TECHDATA::starting_tech_field_ids,
        sizeof(original_fields));

    MOX::settings.starting_civilization_level =
        STARTING_CIVILIZATION_LEVEL_PRE_WARP;
    TECHDATA::starting_tech_field_ids[0] = TECH_FIELD_STARTING_TECH;
    TECHINIT::Init_Player_Tech(0);
    DOCTEST_CHECK(MOX::player[0].tech_fields[TECH_FIELD_STARTING_TECH] == 2);
    DOCTEST_CHECK(MOX::player[0].tech_fields[TECH_FIELD_ENGINEERING] == 2);

    TEST_TECHINIT::Reset_();
    MOX::settings.starting_civilization_level =
        STARTING_CIVILIZATION_LEVEL_AVERAGE;
    TECHDATA::starting_tech_field_ids[0] = TECH_FIELD_CHEMISTRY;
    TECHDATA::starting_tech_field_ids[1] = TECH_FIELD_STARTING_TECH;
    TECHDATA::starting_tech_field_ids[5] = TECH_FIELD_STARTING_TECH;
    TECHINIT::Init_Player_Tech(0);
    DOCTEST_CHECK(MOX::player[0].tech_fields[TECH_FIELD_CHEMISTRY] == 3);
    DOCTEST_CHECK(MOX::player[0].tech_fields[TECH_FIELD_STARTING_TECH] == 2);
    DOCTEST_CHECK(MOX::player[0].tech_fields[TECH_FIELD_NUCLEAR_FISSION] == 2);

    std::memcpy(
        TECHDATA::starting_tech_field_ids,
        original_fields,
        sizeof(original_fields));
}

DOCTEST_TEST_CASE("Post-warp tactical games grant the common technology segment") {
    TEST_TECHINIT::Reset_();

    TECHINIT::Grant_Postwarp_Technologies();

    DOCTEST_CHECK(
        MOX::player[0].tech_applications[TECH_APP_REINFORCED_HULL] !=
        TECH_RESEARCH_STATUS_RESEARCHED);
    DOCTEST_CHECK(
        MOX::player[0].tech_applications[TECH_APP_AUTOMATED_FACTORIES] ==
        TECH_RESEARCH_STATUS_RESEARCHED);
    DOCTEST_CHECK(
        MOX::player[0].tech_applications[TECH_APP_FIGHTER_BAYS] ==
        TECH_RESEARCH_STATUS_RESEARCHED);
}

DOCTEST_TEST_CASE("Post-warp grants application zero and preserves configured table order") {
    TEST_TECHINIT::Reset_();
    int16_t original[TECHINIT::POSTWARP_TECH_COUNT];
    std::memcpy(original, TECHINIT::postwarp_tech, sizeof(original));
    TECHINIT::postwarp_tech[TECHINIT::POSTWARP_STRATEGIC_EXTRA_TECH_COUNT] = 0;
    TECHINIT::postwarp_tech[TECHINIT::POSTWARP_STRATEGIC_EXTRA_TECH_COUNT + 1] = 211;
    TECHINIT::postwarp_tech[TECHINIT::POSTWARP_STRATEGIC_EXTRA_TECH_COUNT + 2] =
        TECH_APP_AUTOMATED_FACTORIES;

    TECHINIT::Grant_Postwarp_Technologies();

    DOCTEST_CHECK(TEST_TECHINIT::Grant_Count_() ==
                  TECHINIT::POSTWARP_TECH_COUNT -
                      TECHINIT::POSTWARP_STRATEGIC_EXTRA_TECH_COUNT);
    DOCTEST_CHECK(TEST_TECHINIT::Granted_App_(0) == 0);
    DOCTEST_CHECK(TEST_TECHINIT::Granted_App_(1) == 211);
    DOCTEST_CHECK(TEST_TECHINIT::Granted_App_(2) == TECH_APP_AUTOMATED_FACTORIES);
    DOCTEST_CHECK(MOX::player[0].tech_applications[0] ==
                  TECH_RESEARCH_STATUS_RESEARCHED);
    DOCTEST_CHECK(MOX::player[0].tech_applications[211] ==
                  TECH_RESEARCH_STATUS_RESEARCHED);
    DOCTEST_CHECK(MOX::player[0].got_new_app == 1);

    std::memcpy(TECHINIT::postwarp_tech, original, sizeof(original));
}

DOCTEST_TEST_CASE("Post-warp strategic games also grant the leading exceptions") {
    TEST_TECHINIT::Reset_();
    MOX::settings.strategic_combat_flag = 1;

    TECHINIT::Grant_Postwarp_Technologies();

    DOCTEST_CHECK(
        MOX::player[0].tech_applications[TECH_APP_REINFORCED_HULL] ==
        TECH_RESEARCH_STATUS_RESEARCHED);
    DOCTEST_CHECK(
        MOX::player[0].tech_applications[TECH_APP_ANTI_MISSILE_ROCKETS] ==
        TECH_RESEARCH_STATUS_RESEARCHED);
    DOCTEST_CHECK(
        MOX::player[0].tech_applications[TECH_APP_FIGHTER_BAYS] ==
        TECH_RESEARCH_STATUS_RESEARCHED);
}
