#include "pch.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

#include "doctest/doctest.h"
#include "generate_cpp.h"
#include "generate_docs.h"
#include "json5_reader.h"
#include "schema_model.h"

namespace {
    std::string Fixture_Path_(const char* name) {
        return (std::filesystem::path(CONFIG_SCHEMA_TEST_DATA_DIR) / name).string();
    }

    bool Load_(const char* name, config::tool::s_schema* schema, std::string* diagnostic) {
        const std::string path = Fixture_Path_(name);
        return config::tool::Load_And_Validate_Schema(path.c_str(), schema, diagnostic);
    }

    const config::tool::s_json_value* Member_(
        const config::tool::s_json_value* object, const char* name) {
        if (object == nullptr || object->kind != config::tool::JSON_OBJECT) {
            return nullptr;
        }
        for (size_t index = 0; index < object->member_names.size(); ++index) {
            if (object->member_names[index] == name) {
                return &object->values[index];
            }
        }
        return nullptr;
    }

    int64_t Integer_(const config::tool::s_json_value* value) {
        if (value->kind == config::tool::JSON_OBJECT) {
            value = Member_(value, "value");
        }
        if (value->kind == config::tool::JSON_SIGNED) {
            return value->signed_value;
        }
        return static_cast<int64_t>(value->unsigned_value);
    }

    std::string Read_File_(const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary);
        DOCTEST_REQUIRE(file.is_open());
        return std::string((std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>());
    }

    size_t Count_(const std::string& text, const std::string& needle) {
        size_t count = 0;
        size_t offset = 0;
        while ((offset = text.find(needle, offset)) != std::string::npos) {
            ++count;
            offset += needle.size();
        }
        return count;
    }
} // namespace

DOCTEST_TEST_CASE("aggregate schema normalizes traversal leaves defaults and native targets") {
    config::tool::s_schema schema;
    std::string diagnostic;

    DOCTEST_REQUIRE(Load_("valid_aggregates.json5", &schema, &diagnostic));
    DOCTEST_REQUIRE(schema.parameters.size() == 12);
    DOCTEST_CHECK(schema.parameters[0].path == "ships.guardian.appearance");
    DOCTEST_CHECK(schema.parameters[0].group == "interface");
    DOCTEST_CHECK(schema.parameters[1].path == "ships.guardian.weapon.1.type");
    DOCTEST_CHECK(schema.parameters[1].group == "gameplay");
    DOCTEST_CHECK(schema.parameters[1].scalar_kind == config::tool::SCALAR_ENUM);
    DOCTEST_CHECK(schema.parameters[1].default_integer == 11);
    DOCTEST_REQUIRE(schema.parameters[1].values.size() == 3);
    DOCTEST_CHECK(schema.parameters[1].values[0].description.empty());
    DOCTEST_CHECK(schema.parameters[1].values[1].description ==
        "A kinetic cannon that fires dense projectiles.");
    DOCTEST_CHECK(schema.parameters[2].scalar_kind == config::tool::SCALAR_MASK);
    DOCTEST_CHECK(schema.parameters[2].default_integer == 15);
    DOCTEST_REQUIRE(schema.parameters[2].flags.size() == 4);
    DOCTEST_CHECK(schema.parameters[2].flags[0].description ==
        "The weapon can fire toward the front of the ship.");
    DOCTEST_CHECK(schema.parameters[4].native_targets.size() == 2);
    DOCTEST_CHECK(schema.parameters[4].native_targets[0] ==
        "fixture.weapons[0].amount");
    DOCTEST_CHECK(schema.parameters[4].native_targets[1] ==
        "fixture.weapons[0].current_amount");
    DOCTEST_CHECK(schema.parameters[9].scalar_kind == config::tool::SCALAR_BITMAP);
    DOCTEST_CHECK(schema.parameters[9].bitmap_bit_count == 40);
    DOCTEST_REQUIRE(schema.parameters[9].default_bitmap.size() == 5);
    DOCTEST_CHECK(schema.parameters[9].default_bitmap[0] == 0x08);
    DOCTEST_CHECK(schema.parameters[9].default_bitmap[4] == 0x80);
    DOCTEST_CHECK(schema.parameters[9].native_targets[0] ==
        "fixture.special_device_flags");
    DOCTEST_CHECK(schema.parameters[10].default_string == "Grüße");
    DOCTEST_CHECK(schema.parameters[11].default_ratio_numerator == 1);
    DOCTEST_CHECK(schema.parameters[11].default_ratio_denominator == 3);

    DOCTEST_REQUIRE(schema.nodes.size() == 17);
    DOCTEST_CHECK(schema.nodes[0].kind == config::tool::NODE_NAMESPACE);
    DOCTEST_CHECK(schema.nodes[0].first_parameter == 0);
    DOCTEST_CHECK(schema.nodes[0].last_parameter == 11);
    DOCTEST_CHECK(schema.nodes[3].kind == config::tool::NODE_ARRAY);
    DOCTEST_CHECK(schema.nodes[3].first_parameter == 1);
    DOCTEST_CHECK(schema.nodes[3].last_parameter == 8);
    DOCTEST_REQUIRE(schema.nodes[3].index_aliases.size() == 2);
    DOCTEST_CHECK(schema.nodes[3].index_aliases[0].name == "primary");
    DOCTEST_CHECK(schema.nodes[3].index_aliases[0].value == 1);
    DOCTEST_CHECK(schema.nodes[3].index_aliases[1].name == "secondary");
    DOCTEST_CHECK(schema.nodes[3].index_aliases[1].value == 2);

    DOCTEST_REQUIRE(schema.type_declarations.size() == 6);
    DOCTEST_CHECK(schema.type_declarations[1].metadata.title == "Weapon");
    DOCTEST_REQUIRE(schema.record_declarations.size() == 1);
    DOCTEST_CHECK(schema.record_declarations[0].fields[0].metadata.title == "Weapon");
    DOCTEST_REQUIRE(schema.parameter_declarations.size() == 1);
    DOCTEST_CHECK(schema.parameter_declarations[0].name_space == "ships");
    DOCTEST_CHECK(schema.parameter_declarations[0].type.node_kind == config::tool::NODE_RECORD);
    DOCTEST_REQUIRE(schema.parameter_declarations[0].type.fields.size() == 5);
    DOCTEST_CHECK(schema.parameter_declarations[0].type.fields[1].type->node_kind ==
        config::tool::NODE_ARRAY);
    DOCTEST_CHECK(schema.parameter_declarations[0].type.fields[1].type->array_count == 2);
    DOCTEST_CHECK(schema.parameter_declarations[0].type.fields[1].type->array_index_type ==
        "weapon_slot_index");
    DOCTEST_CHECK(schema.record_declarations[0].used_by.size() == 1);
    DOCTEST_CHECK(schema.type_declarations[1].used_by.size() == 1);
}

DOCTEST_TEST_CASE("map and vector schema use aggregate variables and document collection defaults") {
    config::tool::s_schema schema;
    std::string diagnostic;

    DOCTEST_REQUIRE(Load_("valid_map_vector.json5", &schema, &diagnostic));
    DOCTEST_REQUIRE(schema.parameters.size() == 3);
    DOCTEST_CHECK(schema.parameters[0].path == "settings.keyed.0");
    DOCTEST_CHECK(schema.parameters[1].path == "settings.keyed.1");
    DOCTEST_CHECK(schema.parameters[0].aggregate);
    DOCTEST_CHECK_FALSE(schema.parameters[0].vector);
    DOCTEST_REQUIRE(schema.parameters[0].aggregate_leaves.size() == 2);
    DOCTEST_CHECK(schema.parameters[2].path == "settings.dynamic");
    DOCTEST_CHECK(schema.parameters[2].aggregate);
    DOCTEST_CHECK(schema.parameters[2].vector);
    DOCTEST_CHECK(schema.parameters[2].default_element_count == 1);
    DOCTEST_REQUIRE(schema.parameters[2].aggregate_leaves.size() == 4);
    DOCTEST_CHECK(schema.parameters[2].aggregate_leaves[0].default_integer == 3);
    DOCTEST_CHECK(schema.parameters[2].aggregate_leaves[1].default_integer == 4);
    DOCTEST_CHECK(schema.parameters[2].aggregate_leaves[2].default_integer == 0);
    DOCTEST_CHECK(schema.parameters[2].aggregate_leaves[3].default_integer == 0);

    const std::filesystem::path root = std::filesystem::temp_directory_path() /
        "orion2re-config-map-vector-test";
    const std::filesystem::path generated = root / "generated";
    const std::filesystem::path documentation = root / "documentation";
    std::filesystem::remove_all(root);
    DOCTEST_REQUIRE(config::tool::Generate_Cpp(
        &schema, generated.string().c_str(), &diagnostic));
    DOCTEST_REQUIRE(config::tool::Generate_Documentation(
        &schema, documentation.string().c_str(), &diagnostic));

    const std::string source = Read_File_(generated / "config.cpp");
    DOCTEST_CHECK(source.find("const size_t variable_count = 3;") !=
        std::string::npos);
    DOCTEST_CHECK(source.find("CONFIG_NODE_MAP") != std::string::npos);
    DOCTEST_CHECK(source.find("CONFIG_NODE_VECTOR") != std::string::npos);
    DOCTEST_CHECK(source.find("{ \"settings.dynamic\"") != std::string::npos);
    DOCTEST_CHECK(source.find(".resize(element_count)") != std::string::npos);

    const std::string markdown = Read_File_(documentation / "gameplay.md");
    DOCTEST_CHECK(markdown.find("2 keyed entries") != std::string::npos);
    DOCTEST_CHECK(markdown.find("zero-based dynamically sized entries") !=
        std::string::npos);
    DOCTEST_CHECK(markdown.find(
                      "settings keyed =\n"
                      "    1 2\n"
                      "    1 2;") != std::string::npos);
    DOCTEST_CHECK(markdown.find(
                      "settings dynamic =\n"
                      "    3 4;") != std::string::npos);
    std::filesystem::remove_all(root);
}

DOCTEST_TEST_SUITE("configuration schema") {
    DOCTEST_TEST_CASE("JSON reader replaces reused output instead of appending to it") {
        config::tool::s_json_value reused;
        config::tool::s_json_value expected;
        std::string diagnostic;
        const std::string primitives = Fixture_Path_("valid_primitives.json5");
        const std::string aggregates = Fixture_Path_("valid_aggregates.json5");

        DOCTEST_REQUIRE(config::tool::Read_Json5_File(
            primitives.c_str(), &reused, &diagnostic));
        DOCTEST_REQUIRE(config::tool::Read_Json5_File(
            aggregates.c_str(), &reused, &diagnostic));
        DOCTEST_REQUIRE(config::tool::Read_Json5_File(
            aggregates.c_str(), &expected, &diagnostic));

        DOCTEST_CHECK(reused.kind == expected.kind);
        DOCTEST_CHECK(reused.member_names == expected.member_names);
        DOCTEST_CHECK(reused.values.size() == expected.values.size());
    }

    DOCTEST_TEST_CASE("main schema game-domain types are documented and match native values") {
        config::tool::s_schema schema;
        std::string diagnostic;
        DOCTEST_REQUIRE(config::tool::Load_And_Validate_Schema(
            CONFIG_MAIN_SCHEMA_PATH, &schema, &diagnostic));

        config::tool::s_json_value root;
        DOCTEST_REQUIRE(config::tool::Read_Json5_File(
            CONFIG_MAIN_SCHEMA_PATH, &root, &diagnostic));
        const config::tool::s_json_value* types = Member_(&root, "types");
        DOCTEST_REQUIRE(types != nullptr);

        DOCTEST_REQUIRE(schema.documentation_categories.size() == 12);
        for (const config::tool::s_parameter_declaration& parameter :
            schema.parameter_declarations) {
            DOCTEST_CHECK_FALSE(parameter.documentation_category.empty());
        }

        for (size_t type_index = 0; type_index < types->values.size(); ++type_index) {
            const config::tool::s_json_value& type = types->values[type_index];
            for (const char* field : {"title", "description", "remarks"}) {
                const config::tool::s_json_value* documentation = Member_(&type, field);
                DOCTEST_REQUIRE_MESSAGE(
                    documentation != nullptr,
                    types->member_names[type_index],
                    " is missing ",
                    field);
                DOCTEST_CHECK(documentation->kind == config::tool::JSON_STRING);
                DOCTEST_CHECK_FALSE(documentation->string_value.empty());
            }
        }

        struct s_expected_symbol {
            const char* type;
            const char* collection;
            const char* symbol;
            int64_t value;
        };
        const s_expected_symbol symbols[] = {
            {"game_difficulty", "values", "tutor", GAME_DIFFICULTY_TUTOR},
            {"game_difficulty", "values", "impossible", GAME_DIFFICULTY_IMPOSSIBLE},
            {"galaxy_age", "values", "old", GALAXY_AGE_OLD},
            {"galaxy_size", "values", "maximum", GALAXY_SIZE_MAXIMUM},
            {"government", "values", "feudal", GOVERNMENT_FEUDAL},
            {"government", "values", "galactic_unification", GOVERNMENT_GALACTIC_UNIFICATION},
            {"stock_race_index", "values", "alkari", STOCK_RACE_ALKARI},
            {"stock_race_index", "values", "trilarian", STOCK_RACE_TRILARIAN},
            {"crew_quality", "values", "ultra_elite", CREW_QUALITY_ULTRA_ELITE},
            {"leader_type", "values", "colony", LEADER_TYPE_COLONY},
            {"strategic_special_category", "values", "beam_missile_damage", SPECIAL_CATEGORY_BEAM_MISSILE_DAMAGE},
            {"technology_application_type", "values", "reserved_6", TECH_APPLICATION_TYPE_RESERVED_6},
            {"technology_application_type", "values", "special", TECH_APPLICATION_TYPE_SPECIAL},
            {"planet_environment_group", "values", "lifeless", PLANET_GROUP_LIFELESS},
            {"map_generation_satellite", "values", "companion_star", MAPGEN_SATELLITE_COMPANION_STAR},
            {"map_star_class", "values", "brown_dwarf", STAR_CLASS_DWARF},
            {"map_star_class_or_black_hole", "values", "black_hole", STAR_CLASS_BLACK_HOLE},
            {"planet_gravity", "values", "heavy", PLANET_GRAVITY_HEAVY_G},
            {"map_system_special", "values", "ancient_artifacts", SYSTEM_SPECIAL_ANCIENT_ARTIFACTS},
            {"random_roll_10", "values", "roll_10", 9},
            {"system_orbit", "values", "orbit_5", 4},
            {"star_visual_size", "values", "large", 2},
            {"ai_special_choice_item_kind", "values", "weapon", AI_SPECIAL_ITEM_WEAPON},
            {"ai_ship_design_theme", "values", "human", AI_SHIP_DESIGN_THEME_HUMAN},
            {"ai_ship_special_theme", "values", "human", AI_SHIP_SPECIAL_THEME_HUMAN},
            {"ai_ship_design_allocation_profile", "values", "special_weapon", aidesign::SHIP_DESIGN_ALLOCATION_SPECIAL_WEAPON},
            {"weapon_modification_id", "values", "no_mods", WEAPON_MOD_NO_MODS},
            {"weapon_modification_id", "values", "overloaded", WEAPON_MOD_OVERLOADED},
            {"available_weapon_modification_mask", "flags", "ovr", WEAPON_MOD_FLAG_OVERLOADED},
            {"installed_weapon_modification_mask", "flags", "ovr", WEAPON_MOD_FLAG_OVERLOADED},
            {"natural_weapon_property_mask", "flags", "sh", WEAPON_NATURAL_FLAG_ANTI_SHIELD},
            {"natural_weapon_property_mask", "flags", "ai2", WEAPON_NATURAL_FLAG_AI_SPATIAL_COMPRESSOR},
            {"weapon_firing_arc", "values", "all_sectors", WEAPON_FIRING_ARC_ALL_SECTORS},
            {"weapon_firing_arc", "values", "design_360", WEAPON_FIRING_ARC_360},
        };

        for (const s_expected_symbol& expected : symbols) {
            const config::tool::s_json_value* type = Member_(types, expected.type);
            const config::tool::s_json_value* collection = Member_(type, expected.collection);
            const config::tool::s_json_value* symbol = Member_(collection, expected.symbol);
            DOCTEST_REQUIRE_MESSAGE(symbol != nullptr, expected.type, ".", expected.symbol);
            DOCTEST_CHECK(Integer_(symbol) == expected.value);
        }

        const s_expected_symbol random_events[] = {
            {"random_event", "values", "derelict", EVENT_DERELICT},
            {"random_event", "values", "climatic_change", EVENT_CLIMATIC_CHANGE},
            {"random_event", "values", "comet", EVENT_COMET},
            {"random_event", "values", "computer_virus", EVENT_COMPUTER_VIRUS},
            {"random_event", "values", "diplomatic_blunder", EVENT_DIPLOMATIC_BLUNDER},
            {"random_event", "values", "diplomatic_marriage", EVENT_DIPLOMATIC_MARRIAGE},
            {"random_event", "values", "donation", EVENT_DONATION},
            {"random_event", "values", "earthquake", EVENT_EARTHQUAKE},
            {"random_event", "values", "freak_accident", EVENT_FREAK_ACCIDENT},
            {"random_event", "values", "hyperspace_flux", EVENT_HYPERSPACE_FLUX},
            {"random_event", "values", "industrial_accident", EVENT_INDUSTRIAL_ACCIDENT},
            {"random_event", "values", "mineral_depletion", EVENT_MINERAL_DEPLETION},
            {"random_event", "values", "mineral_deposit", EVENT_MINERAL_DEPOSIT},
            {"random_event", "values", "mutiny", EVENT_MUTINY},
            {"random_event", "values", "pirate_activity", EVENT_PIRATE_ACTIVITY},
            {"random_event", "values", "pirate_raid", EVENT_PIRATE_RAID},
            {"random_event", "values", "plague", EVENT_PLAGUE},
            {"random_event", "values", "population_boom", EVENT_POPULATION_BOOM},
            {"random_event", "values", "serendipity", EVENT_SERENDIPITY},
            {"random_event", "values", "space_amoeba", EVENT_SPACE_AMOEBA},
            {"random_event", "values", "space_crystal", EVENT_SPACE_CRYSTAL},
            {"random_event", "values", "space_dragon", EVENT_SPACE_DRAGON},
            {"random_event", "values", "space_eel", EVENT_SPACE_EEL},
            {"random_event", "values", "space_hydra", EVENT_SPACE_HYDRA},
            {"random_event", "values", "supernova", EVENT_SUPERNOVA},
            {"random_event", "values", "time_space_anomaly", EVENT_TIME_SPACE_ANOMALY},
            {"random_event", "values", "warp_beast", EVENT_WARP_BEAST},
            {"random_event", "values", "warp_funnel", EVENT_WARP_FUNNEL},
            {"random_event", "values", "unstable_wormhole", EVENT_UNSTABLE_WORMHOLE},
        };
        const config::tool::s_json_value* random_event_values =
            Member_(Member_(types, "random_event"), "values");
        DOCTEST_REQUIRE(random_event_values != nullptr);
        DOCTEST_REQUIRE(random_event_values->member_names.size() == EVENT_RANDOM_COUNT);
        for (size_t index = 0; index < EVENT_RANDOM_COUNT; ++index) {
            DOCTEST_CHECK(random_event_values->member_names[index] == random_events[index].symbol);
            DOCTEST_CHECK(Integer_(&random_event_values->values[index]) == random_events[index].value);
        }

        const char* described_symbols[][2] = {
            {"technology_application_type", "special"},
            {"planet_environment_group", "hostile"},
            {"ai_special_choice_item_kind", "special"},
            {"ai_ship_design_theme", "special"},
            {"weapon_firing_arc", "all_sectors"},
            {"weapon_firing_arc", "design_360"},
        };
        for (const auto& expected : described_symbols) {
            const config::tool::s_json_value* type = Member_(types, expected[0]);
            const config::tool::s_json_value* symbol = Member_(Member_(type, "values"), expected[1]);
            const config::tool::s_json_value* description = Member_(symbol, "description");
            DOCTEST_REQUIRE_MESSAGE(description != nullptr, expected[0], ".", expected[1]);
            DOCTEST_CHECK_FALSE(description->string_value.empty());
        }

        struct s_expected_extent {
            const char* type;
            const char* collection;
            size_t count;
        };
        const s_expected_extent extents[] = {
            {"game_difficulty", "values", GAME_DIFFICULTY_COUNT},
            {"galaxy_age", "values", GALAXY_AGE_COUNT},
            {"galaxy_size", "values", GALAXY_SIZE_COUNT - 1},
            {"government", "values", GOVERNMENT_COUNT},
            {"stock_race_index", "values", STOCK_RACE_COUNT},
            {"crew_quality", "values", CREW_QUALITY_COUNT},
            {"leader_type", "values", LEADER_TYPE_COUNT},
            {"strategic_special_category", "values", SPECIAL_CATEGORY_COUNT},
            {"technology_application_type", "values", TECH_APPLICATION_TYPE_COUNT},
            {"planet_environment_group", "values", PLANET_GROUP_COUNT},
            {"map_generation_satellite", "values", MAPGEN_SATELLITE_COUNT - 1},
            {"map_star_class", "values", STAR_CLASS_COUNT - 1},
            {"map_star_class_or_black_hole", "values", STAR_CLASS_COUNT},
            {"planet_gravity", "values", PLANET_GRAVITY_COUNT},
            {"map_system_special", "values", SYSTEM_SPECIAL_COUNT - 1},
            {"random_roll_10", "values", 10},
            {"system_orbit", "values", 5},
            {"star_visual_size", "values", 3},
            {"weapon_modification_id", "values", WEAPON_MOD_COUNT},
            {"available_weapon_modification_mask", "flags", WEAPON_MOD_COUNT - 1},
            {"installed_weapon_modification_mask", "flags", WEAPON_MOD_COUNT - 1},
        };
        for (const s_expected_extent& expected : extents) {
            const config::tool::s_json_value* type = Member_(types, expected.type);
            const config::tool::s_json_value* collection = Member_(type, expected.collection);
            DOCTEST_REQUIRE_MESSAGE(collection != nullptr, expected.type);
            DOCTEST_CHECK(collection->member_names.size() == expected.count);
        }

        const config::tool::s_json_value* records = Member_(&root, "records");
        const config::tool::s_json_value* missile_defense =
            Member_(records, "missile_defense");
        const config::tool::s_json_value* missile_fields =
            Member_(missile_defense, "fields");
        DOCTEST_REQUIRE(missile_fields != nullptr);
        const config::tool::s_json_value* missile_description = nullptr;
        for (const config::tool::s_json_value& field : missile_fields->values) {
            const config::tool::s_json_value* id = Member_(&field, "id");
            if (id != nullptr && id->string_value == "displacement_device_missiles_miss_percent") {
                missile_description = Member_(&field, "description");
                break;
            }
        }
        DOCTEST_REQUIRE(missile_description != nullptr);
        DOCTEST_CHECK(missile_description->string_value ==
            "Chance for each missile to miss a ship with a working Displacement Device.");

        const config::tool::s_json_value* parameters = Member_(&root, "parameters");
        for (const char* id : {"autobuild_rules", "strategic_production"}) {
            const auto declaration = std::find_if(
                schema.parameter_declarations.begin(),
                schema.parameter_declarations.end(),
                [id](const config::tool::s_parameter_declaration& parameter) {
                    return parameter.id == id;
                });
            DOCTEST_REQUIRE(declaration != schema.parameter_declarations.end());
            DOCTEST_CHECK_FALSE(declaration->include_default_configuration);
        }
        size_t autobuild_candidates = 0;
        size_t strategic_candidates = 0;
        for (const config::tool::s_parameter& parameter : schema.parameters) {
            if (parameter.path.starts_with(
                    "ai.autobuild_rules.candidates.")) {
                ++autobuild_candidates;
                DOCTEST_CHECK(parameter.aggregate);
                DOCTEST_CHECK_FALSE(parameter.vector);
                DOCTEST_CHECK(parameter.element_leaf_count == 1541);
            }
            if (parameter.path.starts_with(
                    "ai.strategic_production.allocation.candidates.")) {
                ++strategic_candidates;
                DOCTEST_CHECK(parameter.aggregate);
                DOCTEST_CHECK_FALSE(parameter.vector);
                DOCTEST_CHECK(parameter.element_leaf_count == 1541);
            }
        }
        DOCTEST_CHECK(autobuild_candidates == 51);
        DOCTEST_CHECK(strategic_candidates == 6);
        DOCTEST_CHECK(schema.parameters.size() < 19000);
        const config::tool::s_json_value* bonus = Member_(parameters, "bonus");
        const config::tool::s_json_value* bonus_fields =
            Member_(Member_(bonus, "type"), "fields");
        DOCTEST_REQUIRE(bonus_fields != nullptr);
        const config::tool::s_json_value* beam_description = nullptr;
        for (const config::tool::s_json_value& field : bonus_fields->values) {
            const config::tool::s_json_value* id = Member_(&field, "id");
            if (id != nullptr && id->string_value == "displacement_device_beam_miss_percent") {
                beam_description = Member_(&field, "description");
                break;
            }
        }
        DOCTEST_REQUIRE(beam_description != nullptr);
        DOCTEST_CHECK(beam_description->string_value ==
            "Chance for an incoming beam or Energy Absorber attack to miss a ship with a working Displacement Device.");
    }

    DOCTEST_TEST_CASE("primitive schema preserves declaration order and normalizes CP850 defaults") {
        config::tool::s_schema schema;
        std::string diagnostic;

        DOCTEST_REQUIRE(Load_("valid_primitives.json5", &schema, &diagnostic));
        DOCTEST_REQUIRE(schema.parameters.size() == 3);
        DOCTEST_CHECK(schema.parameters[0].path == "enabled");
        DOCTEST_CHECK(schema.parameters[1].path == "signed_value");
        DOCTEST_CHECK(schema.parameters[2].path == "captain_name");
        DOCTEST_CHECK(schema.parameters[1].constraints.min == -10);
        DOCTEST_CHECK(schema.parameters[1].constraints.max == 10);
        DOCTEST_CHECK(schema.parameters[2].default_string == "Grüße");
        DOCTEST_REQUIRE(schema.nodes.size() == 3);
        DOCTEST_CHECK(schema.nodes[0].parameter == 0);
        DOCTEST_CHECK(schema.nodes[1].parameter == 1);
        DOCTEST_CHECK(schema.nodes[2].parameter == 2);
        const std::string encoded = config::tool::Encode_Game_String(schema.parameters[2].default_string);
        DOCTEST_REQUIRE(encoded.size() == 5);
        DOCTEST_CHECK(static_cast<uint8_t>(encoded[2]) == 0x81);
        DOCTEST_CHECK(static_cast<uint8_t>(encoded[3]) == 0xE1);
        DOCTEST_CHECK(config::tool::Decode_Game_String(encoded) ==
            schema.parameters[2].default_string);
    }

    DOCTEST_TEST_CASE("interleaved namespace members receive contiguous depth-first runtime IDs") {
        config::tool::s_schema schema;
        std::string diagnostic;

        DOCTEST_REQUIRE(Load_("valid_interleaved_namespaces.json5", &schema, &diagnostic));
        DOCTEST_REQUIRE(schema.parameters.size() == 3);
        DOCTEST_CHECK(schema.parameters[0].path == "combat.first");
        DOCTEST_CHECK(schema.parameters[1].path == "combat.second");
        DOCTEST_CHECK(schema.parameters[2].path == "standalone");
        DOCTEST_REQUIRE(schema.nodes.size() == 4);
        DOCTEST_CHECK(schema.nodes[0].kind == config::tool::NODE_NAMESPACE);
        DOCTEST_CHECK(schema.nodes[0].first_parameter == 0);
        DOCTEST_CHECK(schema.nodes[0].last_parameter == 1);
        DOCTEST_CHECK(schema.nodes[1].parameter == 0);
        DOCTEST_CHECK(schema.nodes[2].parent == UINT32_MAX);
        DOCTEST_CHECK(schema.nodes[2].first_parameter == 2);
        DOCTEST_CHECK(schema.nodes[2].last_parameter == 2);
        DOCTEST_CHECK(schema.nodes[3].parent == 0);
        DOCTEST_CHECK(schema.nodes[3].parameter == 1);
        DOCTEST_CHECK(config::tool::Validate_Runtime_Layout(&schema, &diagnostic));

        const std::filesystem::path output = std::filesystem::temp_directory_path() /
            "orion2re-config-interleaved-namespace-test";
        std::filesystem::remove_all(output);
        DOCTEST_REQUIRE(config::tool::Generate_Cpp(
            &schema, output.string().c_str(), &diagnostic));
        const std::string generated = Read_File_(output / "config.cpp");
        DOCTEST_CHECK(generated.find(
                          "{ \"combat\", CONFIG_NODE_NAMESPACE, CONFIG_NO_NODE, 0, 0, 1") !=
            std::string::npos);
        DOCTEST_CHECK(generated.find(
                          "{ \"standalone\", CONFIG_NODE_SCALAR, CONFIG_NO_NODE, 0, 2, 2, 2") !=
            std::string::npos);
        std::filesystem::remove_all(output);
    }

    DOCTEST_TEST_CASE("C++ generation rejects invalid runtime variable ranges") {
        config::tool::s_schema schema;
        std::string diagnostic;
        DOCTEST_REQUIRE(Load_("valid_interleaved_namespaces.json5", &schema, &diagnostic));
        schema.nodes[0].last_parameter = 2;

        DOCTEST_CHECK_FALSE(config::tool::Validate_Runtime_Layout(&schema, &diagnostic));
        DOCTEST_CHECK(diagnostic.find("aggregate range does not match its descendants") !=
            std::string::npos);
        const std::filesystem::path output = std::filesystem::temp_directory_path() /
            "orion2re-config-invalid-layout-test";
        std::filesystem::remove_all(output);
        DOCTEST_CHECK_FALSE(config::tool::Generate_Cpp(
            &schema, output.string().c_str(), &diagnostic));
        DOCTEST_CHECK_FALSE(std::filesystem::exists(output / "config.cpp"));
        std::filesystem::remove_all(output);
    }

    DOCTEST_TEST_CASE("schema diagnostics reject malformed and unsupported declarations") {
        struct s_case {
            const char* fixture;
            const char* expected;
        };
        const s_case cases[] = {
            {"duplicate_key.json5", "duplicate object key 'title'"},
            {"unknown_property.json5", "unknown property 'format'"},
            {"invalid_default.json5", "integer violates type or field constraints"},
            {"invalid_unicode.json5", "not representable in CP850"},
            {"invalid_aggregate_default.json5", "record default is missing field 'right'"},
            {"invalid_recursive_record.json5", "recursive aggregate value type"},
            {"invalid_mask.json5", "flags must be single positive bits"},
            {"invalid_bitmap.json5", "expected a bit index within the bitmap"},
            {"invalid_namespace.json5", "namespace conflicts with a top-level parameter"},
            {"invalid_unused_type.json5", "composite contains unknown bits"},
            {"invalid_public_type_documentation.json5", ".title: expected a non-empty string"},
            {"invalid_ratio_binding.json5", "ratio leaves require numerator and denominator targets"},
            {"invalid_array_index_type.json5", "index_type: expected an enum type"},
            {"invalid_array_index_range.json5",
                "index_type.outside: value is outside the array's external index range"},
            {"invalid_string_storage.json5",
                "string_storage: declare string storage on a writable parameter or record field"},
            {"invalid_native_default_with_default.json5",
                "default_source cannot be combined with default"},
            {"invalid_nested_native_default.json5",
                "default_source: accepted only on a top-level parameter"},
            {"invalid_nullable_string.json5",
                "nullable: requires an arena-backed string leaf"},
            {"invalid_field_group.json5", ".group: unknown group 'missing'"},
        };

        for (const s_case& test_case : cases) {
            config::tool::s_schema schema;
            std::string diagnostic;
            DOCTEST_CHECK_FALSE(Load_(test_case.fixture, &schema, &diagnostic));
            DOCTEST_CHECK_MESSAGE(diagnostic.find(test_case.expected) != std::string::npos,
                test_case.fixture,
                ": ",
                diagnostic);
        }
    }

    DOCTEST_TEST_CASE("C++ generation is byte-for-byte deterministic") {
        config::tool::s_schema schema;
        std::string diagnostic;
        DOCTEST_REQUIRE(Load_("valid_primitives.json5", &schema, &diagnostic));

        const std::filesystem::path root = std::filesystem::temp_directory_path() /
            "orion2re-config-schema-test";
        const std::filesystem::path first = root / "first";
        const std::filesystem::path second = root / "second";
        std::filesystem::remove_all(root);

        DOCTEST_REQUIRE(config::tool::Generate_Cpp(&schema, first.string().c_str(), &diagnostic));
        DOCTEST_REQUIRE(config::tool::Generate_Cpp(&schema, second.string().c_str(), &diagnostic));

        const std::string first_content = Read_File_(first / "config.cpp");
        const std::string second_content = Read_File_(second / "config.cpp");
        DOCTEST_CHECK(first_content == second_content);
        DOCTEST_CHECK(first_content.find("const s_variable_descriptor variables[]") ==
            std::string::npos);
        DOCTEST_CHECK(first_content.find("bool Get_Default_Value(") != std::string::npos);
        DOCTEST_CHECK(first_content.find("bool Resolve_Variable(") != std::string::npos);
        DOCTEST_CHECK(first_content.find("bool Resize_Vector(") != std::string::npos);
        DOCTEST_CHECK(first_content.find("generated_variable_resolvers[]") !=
            std::string::npos);
        DOCTEST_CHECK(first_content.find("static const uint32_t generated_") !=
            std::string::npos);
        DOCTEST_CHECK(first_content.find("&generated_type_0") != std::string::npos);
        DOCTEST_CHECK(first_content.find("decltype") == std::string::npos);
        DOCTEST_CHECK(first_content.find(
                          "static_assert(CONFIG_MAX_VARIABLE_TARGETS >= 1);") != std::string::npos);
        DOCTEST_CHECK(first_content.find("CONFIG_PARAMETER_") == std::string::npos);
        DOCTEST_CHECK(first_content.find(
                          "{ \"interface\", false, false }") != std::string::npos);
        DOCTEST_CHECK(Count_(first_content, "CONFIG_GROUP_INTERFACE") >= 1);
        DOCTEST_CHECK(first_content.find(
                          "const s_config_snapshot_descriptor synchronized_config_snapshot") !=
            std::string::npos);
        DOCTEST_CHECK(first_content.find(
                          "const s_config_snapshot_descriptor saved_config_snapshot") !=
            std::string::npos);

        const std::string group_header = Read_File_(first / "config_groups.h");
        DOCTEST_CHECK(group_header.find("CONFIG_GROUP_INTERFACE") != std::string::npos);
        DOCTEST_CHECK(group_header.find("CONFIG_GROUP_COUNT") != std::string::npos);

        const std::string enum_header = Read_File_(first / "config_enums.h");
        DOCTEST_CHECK(enum_header.find("Find_Generated_Enum") !=
            std::string::npos);
        const std::string enum_source = Read_File_(first / "config_enums.cpp");
        DOCTEST_CHECK(enum_source.find("generated_enums[]") !=
            std::string::npos);
        DOCTEST_CHECK(enum_source.find("{ nullptr, nullptr, 0 }") !=
            std::string::npos);
        DOCTEST_CHECK(enum_source.find("generated_enum_count = 0") !=
            std::string::npos);

        std::filesystem::remove_all(root);
    }

    DOCTEST_TEST_CASE("C++ generation emits array aliases only for path resolution") {
        config::tool::s_schema schema;
        std::string diagnostic;
        DOCTEST_REQUIRE(Load_("valid_aggregates.json5", &schema, &diagnostic));

        const std::filesystem::path root = std::filesystem::temp_directory_path() /
            "orion2re-config-alias-test";
        std::filesystem::remove_all(root);
        DOCTEST_REQUIRE(config::tool::Generate_Cpp(
            &schema, root.string().c_str(), &diagnostic));

        const std::string generated = Read_File_(root / "config.cpp");
        DOCTEST_CHECK(generated.find(
                          "{ 3, \"primary\", 1 }") != std::string::npos);
        DOCTEST_CHECK(generated.find(
                          "{ 3, \"secondary\", 2 }") != std::string::npos);
        DOCTEST_CHECK(generated.find("guardian.weapon.primary") == std::string::npos);
        DOCTEST_CHECK(generated.find("guardian.weapon.secondary") == std::string::npos);
        DOCTEST_CHECK(generated.find(
                          "static_assert(sizeof(fixture.special_device_flags) == 5);") !=
            std::string::npos);
        DOCTEST_CHECK(generated.find(
                          "{ (void*)&fixture.special_device_flags, "
                          "sizeof(fixture.special_device_flags), "
                          "s_variable_target::CONFIG_TARGET_VALUE }") != std::string::npos);
        DOCTEST_CHECK(Count_(generated,
                          "static const s_symbol_descriptor generated_values_") == 2);
        DOCTEST_CHECK(Count_(generated,
                          "static const s_symbol_descriptor generated_flags_") == 1);
        DOCTEST_CHECK(Count_(generated,
                          "static const s_bitmap_symbol_descriptor generated_bitmap_values_") == 1);
        DOCTEST_CHECK(Count_(generated,
                          "static const s_bitmap_symbol_descriptor generated_bitmap_flags_") == 1);
        DOCTEST_CHECK(Count_(generated, "generated_values_0") == 2);
        DOCTEST_CHECK(Count_(generated, "generated_type_1") >= 3);
        DOCTEST_CHECK(generated.find(
                          "Number of weapons installed in this slot.") == std::string::npos);
        DOCTEST_CHECK(generated.find("Aggregate schema fixture.") == std::string::npos);
        DOCTEST_CHECK(generated.find("s_config_group_descriptor") != std::string::npos);
        DOCTEST_CHECK(Count_(generated, "CONFIG_GROUP_GAMEPLAY") >= 1);
        DOCTEST_CHECK(Count_(generated, "CONFIG_GROUP_INTERFACE") >= 1);
        DOCTEST_CHECK(generated.find("variable_1_values") == std::string::npos);
        DOCTEST_CHECK(generated.find("variable_6_values") == std::string::npos);

        std::filesystem::remove_all(root);
    }

    DOCTEST_TEST_CASE("arena string storage is explicit in generated bindings") {
        config::tool::s_schema schema;
        std::string diagnostic;
        DOCTEST_REQUIRE(Load_("valid_arena_string.json5", &schema, &diagnostic));
        DOCTEST_REQUIRE(schema.parameters.size() == 1);
        DOCTEST_CHECK(schema.parameters[0].string_storage ==
            config::tool::STRING_STORAGE_ARENA);
        DOCTEST_CHECK(schema.parameters[0].nullable);

        const std::filesystem::path output = std::filesystem::temp_directory_path() /
            "orion2re-config-arena-string-test";
        std::filesystem::remove_all(output);
        DOCTEST_REQUIRE(config::tool::Generate_Cpp(
            &schema, output.string().c_str(), &diagnostic));
        const std::string generated = Read_File_(output / "config.cpp");
        DOCTEST_CHECK(generated.find(
                          "static_assert(std::is_same_v<std::remove_reference_t<"
                          "decltype((fixture.label))>, char*>);") !=
            std::string::npos);
        DOCTEST_CHECK(generated.find(
                          "s_variable_target::CONFIG_TARGET_NULLABLE_ARENA_STRING_POINTER") !=
            std::string::npos);
        std::filesystem::remove_all(output);
    }

    DOCTEST_TEST_CASE("native defaults read bound values instead of generated literals") {
        config::tool::s_schema schema;
        std::string diagnostic;
        DOCTEST_REQUIRE(Load_("valid_native_default.json5", &schema, &diagnostic));
        DOCTEST_REQUIRE(schema.parameters.size() == 3);
        for (const config::tool::s_parameter& parameter : schema.parameters) {
            DOCTEST_CHECK(parameter.default_from_native);
        }

        const std::filesystem::path output = std::filesystem::temp_directory_path() /
            "orion2re-config-native-default-test";
        std::filesystem::remove_all(output);
        DOCTEST_REQUIRE(config::tool::Generate_Cpp(
            &schema, output.string().c_str(), &diagnostic));
        const std::string generated = Read_File_(output / "config.cpp");
        DOCTEST_CHECK(Count_(generated,
                          "if (!Get_Current_Value(variable_id, &current)) return false;") == 1);

        const std::filesystem::path documentation = output / "documentation";
        DOCTEST_CHECK_FALSE(config::tool::Generate_Documentation(
            &schema, documentation.string().c_str(), &diagnostic));
        DOCTEST_CHECK(diagnostic.find("native default value was not loaded") !=
            std::string::npos);
        schema.parameters[0].default_integer = 7;
        schema.parameters[1].default_integer = -2;
        schema.parameters[2].default_integer = 9;
        for (config::tool::s_parameter& parameter : schema.parameters) {
            parameter.default_from_native = false;
        }
        DOCTEST_REQUIRE(config::tool::Generate_Documentation(
            &schema, documentation.string().c_str(), &diagnostic));
        const std::string markdown = Read_File_(documentation / "gameplay.md");
        DOCTEST_CHECK(markdown.find("These values come from the native game data.") ==
            std::string::npos);
        DOCTEST_CHECK(markdown.find("scalar = 7;") != std::string::npos);
        DOCTEST_CHECK(markdown.find("aggregate = -2 9;") != std::string::npos);
        std::filesystem::remove_all(output);
    }

    DOCTEST_TEST_CASE("Markdown documentation is deterministic linked and author-facing") {
        config::tool::s_schema schema;
        std::string diagnostic;
        DOCTEST_REQUIRE(Load_("valid_aggregates.json5", &schema, &diagnostic));

        const std::filesystem::path root = std::filesystem::temp_directory_path() /
            "orion2re-config-docs-test";
        const std::filesystem::path first = root / "first";
        const std::filesystem::path second = root / "second";
        std::filesystem::remove_all(root);
        DOCTEST_REQUIRE(config::tool::Generate_Documentation(
            &schema, first.string().c_str(), &diagnostic));
        DOCTEST_REQUIRE(config::tool::Generate_Documentation(
            &schema, second.string().c_str(), &diagnostic));

        for (const char* file : {"index.md", "gameplay.md", "allowed-values.md", "record-layouts.md"}) {
            DOCTEST_CHECK(Read_File_(first / file) == Read_File_(second / file));
        }
        const std::string generated = Read_File_(first / "gameplay.md");
        const std::string allowed_values = Read_File_(first / "allowed-values.md");
        const std::string record_layouts = Read_File_(first / "record-layouts.md");
        DOCTEST_CHECK(generated.starts_with(
            "# Configuration: Gameplay\n\nDeterministic gameplay settings.\n"));
        DOCTEST_CHECK(generated.find("Generated file") == std::string::npos);
        DOCTEST_CHECK(generated.find("id=\"parameter-guardian\"") != std::string::npos);
        DOCTEST_CHECK(allowed_values.find("id=\"type-weapon-id\"") != std::string::npos);
        DOCTEST_CHECK(record_layouts.find("id=\"record-weapon-slot\"") != std::string::npos);
        DOCTEST_CHECK(allowed_values.find("[Guardian](gameplay.md#parameter-guardian)") !=
            std::string::npos);
        DOCTEST_CHECK(allowed_values.find("#### Used by") != std::string::npos);
        DOCTEST_CHECK(allowed_values.find(
                          "A kinetic cannon that fires dense projectiles.") !=
            std::string::npos);
        DOCTEST_CHECK(allowed_values.find(
                          "The weapon can fire toward the front of the ship.") !=
            std::string::npos);
        DOCTEST_CHECK(generated.find("Configuration path") != std::string::npos);
        DOCTEST_CHECK(generated.find("`ships guardian`") != std::string::npos);
        DOCTEST_CHECK(generated.find("Persistence:** varies by field") !=
            std::string::npos);
        DOCTEST_CHECK(generated.find("Multiplayer:** varies by field") !=
            std::string::npos);
        DOCTEST_CHECK(generated.find("#### Fields") != std::string::npos);
        const size_t appearance = generated.find(
            "**Appearance**: Visual style used for the ship.");
        DOCTEST_REQUIRE(appearance != std::string::npos);
        const size_t appearance_end = generated.find('\n', appearance);
        const std::string appearance_field = generated.substr(
            appearance, appearance_end - appearance);
        DOCTEST_CHECK(appearance_field.find("Default: `2`.") !=
            std::string::npos);
        DOCTEST_CHECK(appearance_field.find(
                          "Local and not saved. May differ between players.") !=
            std::string::npos);
        const size_t weapon = generated.find(
            "**Weapons**: The two configurable weapon slots.");
        DOCTEST_REQUIRE(weapon != std::string::npos);
        const size_t weapon_end = generated.find('\n', weapon);
        const std::string weapon_field = generated.substr(
            weapon, weapon_end - weapon);
        DOCTEST_CHECK(weapon_field.find(
                          "Saved in saved games. Synchronized and must match.") !=
            std::string::npos);
        DOCTEST_CHECK(generated.find("#### Built-in default configuration") !=
            std::string::npos);
        DOCTEST_CHECK(generated.find("ships guardian appearance = 2;") != std::string::npos);
        DOCTEST_CHECK(generated.find(
                          "ships guardian weapon =\n"
                          "    plasma_cannon all 0 2\n"
                          "    mass_driver forward,aft 0 10;") !=
            std::string::npos);
        DOCTEST_CHECK(generated.find(
                          "ships guardian weapon primary type = plasma_cannon;") == std::string::npos);
        DOCTEST_CHECK(generated.find("ships guardian weapon 1 type") == std::string::npos);
        DOCTEST_CHECK(generated.find(
                          "ships guardian specials = self_repairing;") != std::string::npos);
        DOCTEST_CHECK(generated.find(
                          "ships guardian label = \"Grüße\";") != std::string::npos);
        DOCTEST_CHECK(generated.find("ships guardian reduction = 1/3;") != std::string::npos);
        DOCTEST_CHECK(Count_(generated, "ships guardian weapon =") == 1);
        DOCTEST_CHECK(generated.find("Storage") == std::string::npos);
        DOCTEST_CHECK(generated.find("uint8") == std::string::npos);
        DOCTEST_CHECK(generated.find("uint16") == std::string::npos);
        DOCTEST_CHECK(generated.find("int8") == std::string::npos);
        DOCTEST_CHECK(generated.find("int16") == std::string::npos);
        DOCTEST_CHECK(generated.find("Implementation details") == std::string::npos);
        DOCTEST_CHECK(generated.find("fixture.weapons") == std::string::npos);
        DOCTEST_CHECK(generated.find("guardian.weapon.1.type") == std::string::npos);
        DOCTEST_CHECK(generated.find("guardian.weapon.2.type") == std::string::npos);
        DOCTEST_CHECK(generated.find("guardian label = \"Sentinel\";") !=
            std::string::npos);

        std::filesystem::remove_all(root);
    }

    DOCTEST_TEST_CASE("Markdown built-in defaults stop at explicit and omit implicit list terminators") {
        config::tool::s_schema schema;
        std::string diagnostic;
        DOCTEST_REQUIRE(config::tool::Load_And_Validate_Schema(
            CONFIG_MAIN_SCHEMA_PATH, &schema, &diagnostic));
        for (config::tool::s_parameter& parameter : schema.parameters) {
            parameter.default_from_native = false;
        }

        const std::filesystem::path root = std::filesystem::temp_directory_path() /
            "orion2re-config-docs-implicit-list-test";
        const std::filesystem::path output = root / "configuration";
        std::filesystem::remove_all(root);
        DOCTEST_REQUIRE(config::tool::Generate_Documentation(
            &schema, output.string().c_str(), &diagnostic));

        const std::string generated = Read_File_(output / "ai-and-diplomacy.md");
        const size_t autobuild = generated.find("id=\"parameter-autobuild-rules\"");
        const size_t strategic = generated.find("id=\"parameter-strategic-production\"");
        const size_t race_variants = generated.find("id=\"parameter-race-variants\"");
        DOCTEST_REQUIRE(autobuild != std::string::npos);
        DOCTEST_REQUIRE(strategic != std::string::npos);
        DOCTEST_REQUIRE(race_variants != std::string::npos);
        DOCTEST_CHECK(generated.substr(autobuild, strategic - autobuild).find("#### Built-in default configuration") == std::string::npos);
        DOCTEST_CHECK(generated.substr(strategic, race_variants - strategic).find("#### Built-in default configuration") == std::string::npos);
        DOCTEST_CHECK(generated.substr(autobuild, strategic - autobuild).find("ai autobuild_rules cutoff mode = relative;") !=
            std::string::npos);
        DOCTEST_CHECK(generated.substr(strategic, race_variants - strategic).find("ai strategic_production emergency_defense order 1 = missile_base;") !=
            std::string::npos);
        DOCTEST_CHECK(generated.find(
                          "ai ai_race_variants =\n"
                          "    1\n"
                          "    2\n"
                          "    3;") != std::string::npos);
        DOCTEST_CHECK(generated.find(
                          "ai ai_race_variants =\n"
                          "    1\n"
                          "    2\n"
                          "    3\n"
                          "    0") == std::string::npos);

        const std::string colonies = Read_File_(output / "colonies-and-economy.md");
        for (size_t queue = 0; queue <= 10; ++queue) {
            const size_t start = colonies.find(
                "colony build_queue " + std::to_string(queue) + " =\n");
            DOCTEST_REQUIRE(start != std::string::npos);
            const size_t end = colonies.find(';', start);
            DOCTEST_REQUIRE(end != std::string::npos);
            const std::string defaults = colonies.substr(start, end - start + 1);
            DOCTEST_CHECK(Count_(defaults, "    end") == 1);
            DOCTEST_CHECK(defaults.ends_with("    end;"));
        }
        DOCTEST_CHECK(colonies.find(
                          "colony build_queue 1 =\n"
                          "    prepend\n"
                          "    design1\n"
                          "    end;") != std::string::npos);

        std::filesystem::remove_all(root);
    }

    DOCTEST_TEST_CASE("Markdown documentation escapes metadata and handles empty public sections") {
        config::tool::s_schema schema;
        std::string diagnostic;
        DOCTEST_REQUIRE(Load_("valid_primitives.json5", &schema, &diagnostic));
        schema.groups[0].metadata.description = "Local <script>alert(1)</script> & settings.";
        schema.parameter_declarations[0].metadata.title = "Enabled <control>";
        schema.parameter_declarations[0].metadata.examples = {
            "enabled = \"<unsafe>&\";",
        };

        const std::filesystem::path root = std::filesystem::temp_directory_path() /
            "orion2re-config-docs-escape-test";
        const std::filesystem::path output = root / "configuration";
        std::filesystem::remove_all(root);
        DOCTEST_REQUIRE(config::tool::Generate_Documentation(
            &schema, output.string().c_str(), &diagnostic));

        const std::string generated = Read_File_(output / "interface.md");
        const std::string allowed_values = Read_File_(output / "allowed-values.md");
        const std::string record_layouts = Read_File_(output / "record-layouts.md");
        DOCTEST_CHECK(generated.find("Local \\<script\\>alert(1)\\</script\\> & settings.") !=
            std::string::npos);
        DOCTEST_CHECK(generated.find("Enabled \\<control\\>") != std::string::npos);
        DOCTEST_CHECK(generated.find("enabled = \"<unsafe>&\";") !=
            std::string::npos);
        DOCTEST_CHECK(generated.find("Local <script>alert(1)</script>") == std::string::npos);
        DOCTEST_CHECK(allowed_values.find("There are no public allowed-value declarations.") !=
            std::string::npos);
        DOCTEST_CHECK(record_layouts.find("There are no public record-layout declarations.") !=
            std::string::npos);

        std::filesystem::remove_all(root);
    }
}
