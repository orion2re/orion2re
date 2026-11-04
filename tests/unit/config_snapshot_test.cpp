#include "pch.h"

#include <cstring>
#include <vector>

#include "config/config_snapshot.h"
#include "config/config_identity.h"
#include "doctest/doctest.h"
#include "game/info.h"
#include "game/serialization.h"
#include "game/sha256.h"

namespace TEST_CONFIG_SNAPSHOT {
    extern int commit_count;
    extern config::s_config_state committed_state;
    extern std::vector<config::s_mod_requirement> required_mods;
} // namespace TEST_CONFIG_SNAPSHOT

namespace {
    config::t_variable_id Variable_(const char* path);
    config::s_variable_accessor Accessor_(const char* path);
    bool Snapshot_Contains_(
        const config::s_config_snapshot_descriptor* descriptor, const char* path);
    std::vector<uint8_t> Encode_(
        const config::s_config_snapshot_descriptor* descriptor,
        const config::s_config_state* state,
        const std::vector<config::s_mod_requirement>* required_mods = nullptr);
} // namespace

DOCTEST_TEST_CASE("combat capacity limits round trip through saved and multiplayer snapshots") {
    const char* paths[] = {
        "combat.max_ships_per_side",
        "combat.max_flyers_per_side",
        "combat.max_flyers_total",
    };
    const int64_t limits[][3] = {
        {1, 1, 1},
        {100, 300, 300},
        {500, 2500, 5000},
        {127, 128, 301},
    };
    const config::s_config_snapshot_descriptor* descriptors[] = {
        &config::saved_config_snapshot,
        &config::synchronized_config_snapshot,
    };
    for (const config::s_config_snapshot_descriptor* descriptor : descriptors) {
        for (const int64_t* values : limits) {
            config::s_config_state source;
            DOCTEST_REQUIRE(config::Initialize_Defaults(&source));
            for (int index = 0; index < 3; ++index) {
                const config::t_variable_id variable = Variable_(paths[index]);
                DOCTEST_REQUIRE(variable != config::CONFIG_NO_VARIABLE);
                DOCTEST_CHECK(config::Get_Variable_Group(variable) == config::CONFIG_GROUP_GAMEPLAY);
                DOCTEST_CHECK(Snapshot_Contains_(descriptor, paths[index]));
                source.values[variable].integer_value = values[index];
            }
            const std::vector<uint8_t> encoded = Encode_(descriptor, &source);
            config::s_config_state decoded;
            SERIAL::Reader_ reader(encoded.data(), (uint32_t)encoded.size(), "snapshot read failed");
            DOCTEST_REQUIRE(config::Read_Config_Snapshot(&reader, descriptor, &decoded));
            DOCTEST_CHECK(reader.Length() == encoded.size());
            for (int index = 0; index < 3; ++index) {
                DOCTEST_CHECK(decoded.values[Variable_(paths[index])].integer_value == values[index]);
            }
        }
    }
}

DOCTEST_TEST_CASE("starting conditions and Telepathic bonus round trip through gameplay snapshots") {
    const config::t_variable_id population = Variable_("game.start.population");
    const config::t_variable_id marines = Variable_("game.start.marines");
    const config::t_variable_id money = Variable_("game.start.money");
    const config::t_variable_id telepathic = Variable_("spy.telepathic_bonus");
    DOCTEST_REQUIRE(population != config::CONFIG_NO_VARIABLE);
    DOCTEST_REQUIRE(marines != config::CONFIG_NO_VARIABLE);
    DOCTEST_REQUIRE(money != config::CONFIG_NO_VARIABLE);
    DOCTEST_REQUIRE(telepathic != config::CONFIG_NO_VARIABLE);

    struct s_snapshot_case {
        int64_t population;
        int64_t marines;
        int64_t money;
        int64_t telepathic;
    };
    const s_snapshot_case cases[] = {
        {0, 0, 0, 0},
        {42, 255, 2147483647, 10},
        {8, 3, 50, 127},
        {8, 3, 50, 128},
        {8, 3, 50, 255},
    };
    const config::s_config_snapshot_descriptor* descriptors[] = {
        &config::saved_config_snapshot,
        &config::synchronized_config_snapshot,
    };

    for (const config::s_config_snapshot_descriptor* descriptor : descriptors) {
        for (const s_snapshot_case& test_case : cases) {
            config::s_config_state source;
            DOCTEST_REQUIRE(config::Initialize_Defaults(&source));
            source.values[population].integer_value = test_case.population;
            source.values[marines].integer_value = test_case.marines;
            source.values[money].integer_value = test_case.money;
            source.values[telepathic].integer_value = test_case.telepathic;
            const std::vector<uint8_t> encoded = Encode_(descriptor, &source);

            config::s_config_state decoded;
            DOCTEST_REQUIRE(config::Initialize_Defaults(&decoded));
            SERIAL::Reader_ reader(encoded.data(), (uint32_t)encoded.size(), "snapshot read failed");
            DOCTEST_REQUIRE(config::Read_Config_Snapshot(
                &reader, descriptor, &decoded));
            DOCTEST_CHECK(decoded.values[population].integer_value ==
                test_case.population);
            DOCTEST_CHECK(decoded.values[marines].integer_value ==
                test_case.marines);
            DOCTEST_CHECK(decoded.values[money].integer_value == test_case.money);
            DOCTEST_CHECK(decoded.values[telepathic].integer_value ==
                test_case.telepathic);
        }
    }
}

DOCTEST_TEST_CASE("AI production maps use one dirty bit per candidate") {
    const config::s_variable_accessor first = Accessor_(
        "ai.autobuild_rules.candidates.7.weight.base");
    const config::s_variable_accessor second = Accessor_(
        "ai.autobuild_rules.candidates.7.weight.defer.peace");
    const config::s_variable_accessor other = Accessor_(
        "ai.autobuild_rules.candidates.8.weight.base");
    DOCTEST_REQUIRE(first.variable_id != config::CONFIG_NO_VARIABLE);
    DOCTEST_CHECK(first.variable_id == second.variable_id);
    DOCTEST_CHECK(first.variable_id != other.variable_id);

    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Tracked_Defaults(&state));
    config::s_typed_value value;
    DOCTEST_REQUIRE(config::Read_State_Leaf_Value(
        &state, first.variable_id, first.leaf_index, &value));
    ++value.integer_value;
    DOCTEST_REQUIRE(config::Set_State_Leaf_Value(
        &state, first.variable_id, first.leaf_index, value));
    DOCTEST_CHECK(config::State_Value_Is_Non_Default(
        &state, first.variable_id));
    DOCTEST_CHECK(config::State_Leaf_Is_Non_Default(
        &state, first.variable_id, first.leaf_index));
    DOCTEST_CHECK_FALSE(config::State_Leaf_Is_Non_Default(
        &state, second.variable_id, second.leaf_index));
    DOCTEST_CHECK_FALSE(config::State_Value_Is_Non_Default(
        &state, other.variable_id));
    config::s_typed_value other_value;
    DOCTEST_REQUIRE(config::Read_State_Leaf_Value(
        &state, other.variable_id, other.leaf_index, &other_value));
    ++other_value.integer_value;
    DOCTEST_REQUIRE(config::Set_State_Leaf_Value(
        &state, other.variable_id, other.leaf_index, other_value));
    const config::s_non_default_aggregate* other_aggregate =
        config::Get_State_Non_Default_Aggregate(&state, other.variable_id);
    DOCTEST_REQUIRE(other_aggregate != nullptr);

    config::s_typed_value default_value;
    DOCTEST_REQUIRE(config::generated::Get_Default_Leaf_Value(
        first.variable_id, first.leaf_index, &default_value));
    DOCTEST_REQUIRE(config::Set_State_Leaf_Value(
        &state, first.variable_id, first.leaf_index, default_value));
    DOCTEST_CHECK_FALSE(config::State_Value_Is_Non_Default(
        &state, first.variable_id));
    DOCTEST_CHECK(config::State_Leaf_Is_Non_Default(
        &state, other.variable_id, other.leaf_index));
    other_aggregate = config::Get_State_Non_Default_Aggregate(
        &state, other.variable_id);
    DOCTEST_REQUIRE(other_aggregate != nullptr);
    DOCTEST_CHECK(other_aggregate->variable_id == other.variable_id);
    DOCTEST_REQUIRE(state.non_default_aggregates.size() == 1);
    DOCTEST_REQUIRE(state.non_default_variable_ids.size() == 1);
    DOCTEST_CHECK(state.non_default_variable_ids[0] == other.variable_id);
    DOCTEST_CHECK(state.non_default_aggregate_positions[other.variable_id] == 0);
    DOCTEST_CHECK(state.non_default_variable_positions[other.variable_id] == 0);
    DOCTEST_CHECK(state.non_default_values.size() ==
        (config::variable_count + 63) / 64);
}

DOCTEST_TEST_CASE("default missile rack selection is a local interface setting") {
    const config::t_variable_id rack =
        Variable_("design.missile_default_x2_rack");
    DOCTEST_REQUIRE(rack != config::CONFIG_NO_VARIABLE);
    DOCTEST_CHECK(config::Get_Variable_Group(rack) ==
        config::CONFIG_GROUP_INTERFACE);
    DOCTEST_CHECK_FALSE(Snapshot_Contains_(
        &config::saved_config_snapshot, "design.missile_default_x2_rack"));
    DOCTEST_CHECK_FALSE(Snapshot_Contains_(
        &config::synchronized_config_snapshot,
        "design.missile_default_x2_rack"));
}

DOCTEST_TEST_CASE("DESIGN button behavior is synchronized but not saved") {
    const char* paths[] = {
        "design.design_button_computer",
        "design.design_button_shield",
    };

    for (const char* path : paths) {
        const config::t_variable_id variable = Variable_(path);
        DOCTEST_REQUIRE(variable != config::CONFIG_NO_VARIABLE);
        DOCTEST_CHECK(config::Get_Variable_Group(variable) ==
            config::CONFIG_GROUP_SYNCHRONIZED_INTERFACE);
        DOCTEST_CHECK(Snapshot_Contains_(
            &config::synchronized_config_snapshot, path));
        DOCTEST_CHECK_FALSE(Snapshot_Contains_(
            &config::saved_config_snapshot, path));
    }
}

DOCTEST_TEST_CASE("presentation settings stay out of gameplay snapshots") {
    const char* paths[] = {
        "interface.allow_future_technology_browsing",
        "interface.show_exit_message",
        "interface.rpc_enabled",
        "interface.rpc_port",
        "interface.rpc_key",
    };
    const config::s_config_snapshot_descriptor* descriptors[] = {
        &config::saved_config_snapshot,
        &config::synchronized_config_snapshot,
    };

    for (const char* path : paths) {
        const config::t_variable_id variable = Variable_(path);
        DOCTEST_REQUIRE(variable != config::CONFIG_NO_VARIABLE);
        DOCTEST_CHECK(config::Get_Variable_Group(variable) ==
            config::CONFIG_GROUP_INTERFACE);
        for (const config::s_config_snapshot_descriptor* descriptor : descriptors) {
            DOCTEST_CHECK_FALSE(Snapshot_Contains_(descriptor, path));
        }
    }
}

DOCTEST_TEST_CASE("local RPC settings default to disabled with no key") {
    config::s_typed_value value;
    DOCTEST_REQUIRE(config::generated::Get_Default_Value(Variable_("interface.rpc_enabled"), &value));
    DOCTEST_CHECK_FALSE(value.bool_value);
    DOCTEST_REQUIRE(config::generated::Get_Default_Value(Variable_("interface.rpc_port"), &value));
    DOCTEST_CHECK(value.integer_value == 17362);
    DOCTEST_REQUIRE(config::generated::Get_Default_Value(Variable_("interface.rpc_key"), &value));
    DOCTEST_CHECK(value.string_value.empty());
}

DOCTEST_TEST_CASE("interface formats persist and synchronize") {
    const char* fields[] = {"template", "style", "align", "x", "y"};
    for (int row = 0; row < 5; ++row) {
        for (const char* field : fields) {
            const std::string path = "interface.format." +
                std::to_string(row) + "." + field;
            const config::t_variable_id variable = Variable_(path.c_str());
            DOCTEST_REQUIRE(variable != config::CONFIG_NO_VARIABLE);
            const config::s_config_group_descriptor* group =
                config::Get_Config_Group(config::Get_Variable_Group(variable));
            DOCTEST_REQUIRE(group != nullptr);
            DOCTEST_CHECK(std::strcmp(group->id, "gameplay") == 0);
            DOCTEST_CHECK(Snapshot_Contains_(
                &config::saved_config_snapshot, path.c_str()));
            DOCTEST_CHECK(Snapshot_Contains_(
                &config::synchronized_config_snapshot, path.c_str()));
        }
    }
}

DOCTEST_TEST_CASE("saved interface settings persist without synchronization") {
    const char* path = "interface.info_menu_layout";
    const config::t_variable_id variable = Variable_(path);
    DOCTEST_REQUIRE(variable != config::CONFIG_NO_VARIABLE);
    const config::s_config_group_descriptor* group =
        config::Get_Config_Group(config::Get_Variable_Group(variable));
    DOCTEST_REQUIRE(group != nullptr);
    DOCTEST_CHECK(std::strcmp(group->id, "saved_interface") == 0);
    DOCTEST_CHECK(Snapshot_Contains_(&config::saved_config_snapshot, path));
    DOCTEST_CHECK_FALSE(Snapshot_Contains_(
        &config::synchronized_config_snapshot, path));
}

DOCTEST_TEST_CASE("blocker warning is synchronized but not saved") {
    const char* path = "interface.warn_blockers";
    const config::t_variable_id variable = Variable_(path);
    DOCTEST_REQUIRE(variable != config::CONFIG_NO_VARIABLE);
    DOCTEST_CHECK(config::Get_Variable_Group(variable) ==
        config::CONFIG_GROUP_SYNCHRONIZED_INTERFACE);
    DOCTEST_CHECK(Snapshot_Contains_(
        &config::synchronized_config_snapshot, path));
    DOCTEST_CHECK_FALSE(Snapshot_Contains_(
        &config::saved_config_snapshot, path));
}

DOCTEST_TEST_CASE("host build-list presentation and outpost policy synchronize without saving") {
    struct s_integer_case {
        const char* path;
        int64_t value;
    };
    const s_integer_case integer_cases[] = {
        {"interface.build_list_colors.3.0.base", 17},
        {"interface.build_list_colors.3.1.dark", 29},
        {"interface.build_list_format.0.order", -12},
        {"interface.build_list_format.0.color", 3},
    };
    const char* outpost_path = "interface.no_outpost_prompt";

    config::s_config_state source;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&source));
    for (const s_integer_case& test_case : integer_cases) {
        const config::s_variable_accessor accessor = Accessor_(test_case.path);
        DOCTEST_REQUIRE(accessor.variable_id != config::CONFIG_NO_VARIABLE);
        DOCTEST_CHECK(config::Get_Variable_Group(accessor.variable_id) ==
            config::CONFIG_GROUP_SYNCHRONIZED_INTERFACE);
        DOCTEST_CHECK(Snapshot_Contains_(
            &config::synchronized_config_snapshot, test_case.path));
        DOCTEST_CHECK_FALSE(Snapshot_Contains_(
            &config::saved_config_snapshot, test_case.path));
        config::s_typed_value value;
        DOCTEST_REQUIRE(config::Read_State_Leaf_Value(&source,
            accessor.variable_id,
            accessor.leaf_index,
            &value));
        value.integer_value = test_case.value;
        DOCTEST_REQUIRE(config::Set_State_Leaf_Value(&source,
            accessor.variable_id,
            accessor.leaf_index,
            value));
    }

    const config::t_variable_id outpost = Variable_(outpost_path);
    DOCTEST_REQUIRE(outpost != config::CONFIG_NO_VARIABLE);
    DOCTEST_CHECK(config::Get_Variable_Group(outpost) ==
        config::CONFIG_GROUP_SYNCHRONIZED_INTERFACE);
    DOCTEST_CHECK(Snapshot_Contains_(
        &config::synchronized_config_snapshot, outpost_path));
    DOCTEST_CHECK_FALSE(Snapshot_Contains_(
        &config::saved_config_snapshot, outpost_path));
    source.values[outpost].bool_value = true;

    const std::vector<uint8_t> encoded = Encode_(
        &config::synchronized_config_snapshot, &source);
    config::s_config_state decoded;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&decoded));
    SERIAL::Reader_ reader(
        encoded.data(), (uint32_t)encoded.size(), "snapshot read failed");
    DOCTEST_REQUIRE(config::Read_Config_Snapshot(
        &reader, &config::synchronized_config_snapshot, &decoded));

    for (const s_integer_case& test_case : integer_cases) {
        const config::s_variable_accessor accessor = Accessor_(test_case.path);
        config::s_typed_value value;
        DOCTEST_REQUIRE(config::Read_State_Leaf_Value(&decoded,
            accessor.variable_id,
            accessor.leaf_index,
            &value));
        DOCTEST_CHECK(value.integer_value == test_case.value);
    }
    DOCTEST_CHECK(decoded.values[outpost].bool_value);
}

DOCTEST_TEST_CASE("artifact presenter is synchronized but not saved") {
    const char* path = "research.finds_presented_by_scientist";
    const config::t_variable_id variable = Variable_(path);
    DOCTEST_REQUIRE(variable != config::CONFIG_NO_VARIABLE);
    DOCTEST_CHECK(config::Get_Variable_Group(variable) ==
        config::CONFIG_GROUP_SYNCHRONIZED_INTERFACE);
    DOCTEST_CHECK(Snapshot_Contains_(
        &config::synchronized_config_snapshot, path));
    DOCTEST_CHECK_FALSE(Snapshot_Contains_(
        &config::saved_config_snapshot, path));

    config::s_config_state source;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&source));
    source.values[variable].bool_value = true;
    const std::vector<uint8_t> encoded = Encode_(
        &config::synchronized_config_snapshot, &source);

    config::s_config_state decoded;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&decoded));
    SERIAL::Reader_ reader(
        encoded.data(), (uint32_t)encoded.size(), "snapshot read failed");
    DOCTEST_REQUIRE(config::Read_Config_Snapshot(
        &reader, &config::synchronized_config_snapshot, &decoded));
    DOCTEST_CHECK(decoded.values[variable].bool_value);
}

DOCTEST_TEST_CASE("boarding result auto-close is synchronized but not saved") {
    const char* path = "combat.boarding.auto_close_results";
    const config::t_variable_id variable = Variable_(path);
    DOCTEST_REQUIRE(variable != config::CONFIG_NO_VARIABLE);
    DOCTEST_CHECK(config::Get_Variable_Group(variable) ==
        config::CONFIG_GROUP_SYNCHRONIZED_INTERFACE);
    DOCTEST_CHECK(Snapshot_Contains_(
        &config::synchronized_config_snapshot, path));
    DOCTEST_CHECK_FALSE(Snapshot_Contains_(
        &config::saved_config_snapshot, path));
}

DOCTEST_TEST_CASE("battle confirmation timeout is synchronized but not saved") {
    const char* path = "interface.confirm_battle_message_timeout";
    const config::t_variable_id variable = Variable_(path);
    DOCTEST_REQUIRE(variable != config::CONFIG_NO_VARIABLE);
    DOCTEST_CHECK(config::Get_Variable_Group(variable) ==
        config::CONFIG_GROUP_SYNCHRONIZED_INTERFACE);
    DOCTEST_CHECK(Snapshot_Contains_(
        &config::synchronized_config_snapshot, path));
    DOCTEST_CHECK_FALSE(Snapshot_Contains_(
        &config::saved_config_snapshot, path));

    config::s_config_state source;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&source));
    source.values[variable].integer_value = 180;
    const std::vector<uint8_t> encoded = Encode_(
        &config::synchronized_config_snapshot, &source);

    config::s_config_state decoded;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&decoded));
    SERIAL::Reader_ reader(
        encoded.data(), (uint32_t)encoded.size(), "snapshot read failed");
    DOCTEST_REQUIRE(config::Read_Config_Snapshot(
        &reader, &config::synchronized_config_snapshot, &decoded));
    DOCTEST_CHECK(decoded.values[variable].integer_value == 180);
}

DOCTEST_TEST_CASE("corrected AI race variants round trip through gameplay snapshots") {
    const char* paths[] = {
        "ai.race_variants.customization.0.0.5.count",
        "ai.race_variants.evolution.1.1.1.count",
        "ai.race_variants.evolution.3.0.4.traits.0.value",
        "ai.race_variants.evolution.3.0.5.traits.0.value",
        "ai.race_variants.evolution.3.1.3.traits.0.value",
    };
    const int64_t expected[] = {3, 1, 3, 3, 3};
    const config::s_config_snapshot_descriptor* descriptors[] = {
        &config::saved_config_snapshot,
        &config::synchronized_config_snapshot,
    };

    for (const config::s_config_snapshot_descriptor* descriptor : descriptors) {
        config::s_config_state source;
        DOCTEST_REQUIRE(config::Initialize_Defaults(&source));
        for (int index = 0; index < 5; ++index) {
            const config::t_variable_id variable = Variable_(paths[index]);
            DOCTEST_REQUIRE(variable != config::CONFIG_NO_VARIABLE);
            source.values[variable].integer_value = expected[index];
        }

        const std::vector<uint8_t> encoded = Encode_(descriptor, &source);
        config::s_config_state decoded;
        DOCTEST_REQUIRE(config::Initialize_Defaults(&decoded));
        SERIAL::Reader_ reader(encoded.data(), (uint32_t)encoded.size(), "snapshot read failed");
        DOCTEST_REQUIRE(config::Read_Config_Snapshot(
            &reader, descriptor, &decoded));
        for (int index = 0; index < 5; ++index) {
            const config::t_variable_id variable = Variable_(paths[index]);
            DOCTEST_CHECK(decoded.values[variable].integer_value == expected[index]);
        }
    }
}

DOCTEST_TEST_CASE("AI design allocations and fortress topology round trip through gameplay snapshots") {
    const char* paths[] = {
        "ai.ship_design_allocations.0.0.beams",
        "ai.ship_design_allocations.6.5.bombs",
        "ships.fortress.primary_weapons.1.space_mods",
        "ships.fortress.overflow_weapons.1.mods",
    };
    const int64_t expected[] = {0, -1, WEAPON_MOD_FLAG_AUTO_FIRE, WEAPON_MOD_FLAG_OVERLOADED};
    const config::s_config_snapshot_descriptor* descriptors[] = {
        &config::saved_config_snapshot,
        &config::synchronized_config_snapshot,
    };

    for (const config::s_config_snapshot_descriptor* descriptor : descriptors) {
        config::s_config_state source;
        DOCTEST_REQUIRE(config::Initialize_Defaults(&source));
        for (int index = 0; index < 4; ++index) {
            const config::t_variable_id variable = Variable_(paths[index]);
            DOCTEST_REQUIRE(variable != config::CONFIG_NO_VARIABLE);
            source.values[variable].integer_value = expected[index];
        }

        const std::vector<uint8_t> encoded = Encode_(descriptor, &source);
        config::s_config_state decoded;
        DOCTEST_REQUIRE(config::Initialize_Defaults(&decoded));
        SERIAL::Reader_ reader(encoded.data(), (uint32_t)encoded.size(), "snapshot read failed");
        DOCTEST_REQUIRE(config::Read_Config_Snapshot(
            &reader, descriptor, &decoded));
        for (int index = 0; index < 4; ++index) {
            const config::t_variable_id variable = Variable_(paths[index]);
            DOCTEST_CHECK(decoded.values[variable].integer_value == expected[index]);
        }
    }
}

DOCTEST_TEST_CASE("AI production rules round trip through gameplay snapshots") {
    const char* paths[] = {
        "ai.autobuild_rules.cutoff.multiplier.3",
        "ai.autobuild_rules.candidates.7.weight.base",
        "ai.strategic_production.expansion.colony_base_minimum_industry",
        "ai.strategic_production.allocation.candidates.5.weight.defer.peace",
    };
    const int64_t expected[] = {5, 21, 18, 444};
    const config::s_config_snapshot_descriptor* descriptors[] = {
        &config::saved_config_snapshot,
        &config::synchronized_config_snapshot,
    };

    for (const config::s_config_snapshot_descriptor* descriptor : descriptors) {
        config::s_config_state source;
        DOCTEST_REQUIRE(config::Initialize_Defaults(&source));
        for (int index = 0; index < 4; ++index) {
            const config::s_variable_accessor accessor = Accessor_(paths[index]);
            DOCTEST_REQUIRE(accessor.variable_id != config::CONFIG_NO_VARIABLE);
            DOCTEST_CHECK(Snapshot_Contains_(descriptor, paths[index]));
            config::s_typed_value value;
            DOCTEST_REQUIRE(config::Read_State_Leaf_Value(&source,
                accessor.variable_id,
                accessor.leaf_index,
                &value));
            value.integer_value = expected[index];
            DOCTEST_REQUIRE(config::Set_State_Leaf_Value(&source,
                accessor.variable_id,
                accessor.leaf_index,
                value));
        }

        const std::vector<uint8_t> encoded = Encode_(descriptor, &source);
        config::s_config_state decoded;
        DOCTEST_REQUIRE(config::Initialize_Defaults(&decoded));
        SERIAL::Reader_ reader(encoded.data(), (uint32_t)encoded.size(), "snapshot read failed");
        DOCTEST_REQUIRE(config::Read_Config_Snapshot(
            &reader, descriptor, &decoded));
        for (int index = 0; index < 4; ++index) {
            const config::s_variable_accessor accessor = Accessor_(paths[index]);
            config::s_typed_value value;
            DOCTEST_REQUIRE(config::Read_State_Leaf_Value(&decoded,
                accessor.variable_id,
                accessor.leaf_index,
                &value));
            DOCTEST_CHECK(value.integer_value == expected[index]);
        }
    }
}

DOCTEST_TEST_CASE("synchronized configuration identity is canonical and complete") {
    config::s_config_state first;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&first));
    config::s_config_state second = first;
    const config::t_variable_id synchronized = Variable_("race.picks.starting");
    const config::t_variable_id local = Variable_("interface.scroll_interval");
    DOCTEST_REQUIRE(synchronized != config::CONFIG_NO_VARIABLE);
    DOCTEST_REQUIRE(local != config::CONFIG_NO_VARIABLE);

    std::vector<uint8_t> first_identity;
    std::vector<uint8_t> repeated_identity;
    std::string diagnostic;
    DOCTEST_REQUIRE(config::Build_Canonical_Config_Identity(
        &config::synchronized_config_snapshot, &first, &first_identity, &diagnostic));
    DOCTEST_REQUIRE(config::Build_Canonical_Config_Identity(
        &config::synchronized_config_snapshot, &first, &repeated_identity, &diagnostic));
    DOCTEST_CHECK(first_identity == repeated_identity);
    DOCTEST_CHECK(first_identity.size() > 12);
    DOCTEST_CHECK(first_identity[0] == 'O');
    DOCTEST_CHECK(first_identity[1] == '2');
    DOCTEST_CHECK(first_identity[2] == 'C');
    DOCTEST_CHECK(first_identity[3] == 'I');
    DOCTEST_CHECK(first_identity[4] == 0);
    DOCTEST_CHECK(first_identity[5] == 0);
    DOCTEST_CHECK(first_identity[6] == 0);
    DOCTEST_CHECK(first_identity[7] == 1);

    second.values[local].integer_value = 9;
    std::vector<uint8_t> local_identity;
    DOCTEST_REQUIRE(config::Build_Canonical_Config_Identity(
        &config::synchronized_config_snapshot, &second, &local_identity, &diagnostic));
    DOCTEST_CHECK(local_identity == first_identity);

    second.values[synchronized].integer_value = 12;
    std::vector<uint8_t> changed_identity;
    DOCTEST_REQUIRE(config::Build_Canonical_Config_Identity(
        &config::synchronized_config_snapshot, &second, &changed_identity, &diagnostic));
    DOCTEST_CHECK(changed_identity != first_identity);
    DOCTEST_CHECK(orion2re::crypto::sha256_hex(std::string_view(
                      reinterpret_cast<const char*>(changed_identity.data()),
                      changed_identity.size())) !=
        orion2re::crypto::sha256_hex(std::string_view(
            reinterpret_cast<const char*>(first_identity.data()),
            first_identity.size())));
}

DOCTEST_TEST_CASE("configuration identity rejects malformed descriptors") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    std::vector<uint8_t> identity;
    std::string diagnostic;

    const uint32_t invalid_variables[] = {
        static_cast<uint32_t>(config::variable_count),
    };
    const config::s_config_snapshot_descriptor invalid = {
        invalid_variables,
        1,
        UINT32_MAX,
    };
    DOCTEST_CHECK_FALSE(config::Build_Canonical_Config_Identity(
        &invalid, &state, &identity, &diagnostic));
    DOCTEST_CHECK(diagnostic ==
        "invalid synchronized configuration identity descriptor");

    const uint32_t duplicate_variables[] = {
        config::synchronized_config_snapshot.variables[0],
        config::synchronized_config_snapshot.variables[0],
    };
    const config::s_config_snapshot_descriptor duplicate = {
        duplicate_variables,
        2,
        config::synchronized_config_snapshot.group_mask,
    };
    DOCTEST_CHECK_FALSE(config::Build_Canonical_Config_Identity(
        &duplicate, &state, &identity, &diagnostic));
}

DOCTEST_TEST_CASE("sparse identities canonicalize defaults and aggregate edits") {
    config::s_config_state defaults;
    DOCTEST_REQUIRE(config::Initialize_Tracked_Defaults(&defaults));
    auto changed = defaults;
    auto repeated = defaults;
    std::vector<uint8_t> base_identity, changed_identity, repeated_identity;
    std::string diagnostic;
    const auto* descriptor = &config::synchronized_config_snapshot;
    DOCTEST_REQUIRE(config::Build_Sparse_Config_Identity(descriptor, &defaults, &base_identity, &diagnostic));
    DOCTEST_CHECK(base_identity.size() < 100);
    DOCTEST_CHECK(base_identity[7] == 2);
    const auto leaf = Accessor_("ai.autobuild_rules.candidates.7.weight.base");
    config::s_typed_value value;
    DOCTEST_REQUIRE(config::Read_State_Leaf_Value(&defaults, leaf.variable_id, leaf.leaf_index, &value));
    ++value.integer_value;
    DOCTEST_REQUIRE(config::Set_State_Leaf_Value(&changed, leaf.variable_id, leaf.leaf_index, value));
    const auto id = Variable_("race.picks.starting");
    repeated.values[id] = defaults.values.Read(id);
    DOCTEST_REQUIRE(config::Set_State_Leaf_Value(&repeated, leaf.variable_id, leaf.leaf_index, value));
    DOCTEST_REQUIRE(config::Build_Sparse_Config_Identity(descriptor, &changed, &changed_identity, &diagnostic));
    DOCTEST_REQUIRE(config::Build_Sparse_Config_Identity(descriptor, &repeated, &repeated_identity, &diagnostic));
    DOCTEST_CHECK(changed_identity == repeated_identity);
    DOCTEST_CHECK(changed_identity != base_identity);
    DOCTEST_CHECK(changed_identity.size() < 150);
    DOCTEST_REQUIRE(config::Read_State_Leaf_Value(&defaults, leaf.variable_id, leaf.leaf_index, &value));
    DOCTEST_REQUIRE(config::Set_State_Leaf_Value(&changed, leaf.variable_id, leaf.leaf_index, value));
    DOCTEST_REQUIRE(config::Build_Sparse_Config_Identity(descriptor, &changed, &changed_identity, &diagnostic));
    DOCTEST_CHECK(changed_identity == base_identity);
    changed.values[id].integer_value = 100000;
    DOCTEST_CHECK_FALSE(config::Build_Sparse_Config_Identity(descriptor, &changed, &changed_identity, &diagnostic));
}

namespace {
    config::t_variable_id Variable_(const char* path) {
        config::t_variable_id variable_id = config::CONFIG_NO_VARIABLE;
        config::Find_Variable(path, std::strlen(path), &variable_id);
        return variable_id;
    }

    config::s_variable_accessor Accessor_(const char* path) {
        config::s_variable_accessor accessor;
        config::Find_Variable_Accessor(path, std::strlen(path), &accessor);
        return accessor;
    }

    bool Snapshot_Contains_(const config::s_config_snapshot_descriptor* descriptor,
        const char* path) {
        const config::t_variable_id variable = Variable_(path);
        if (variable == config::CONFIG_NO_VARIABLE) return false;
        for (uint32_t index = 0; index < descriptor->variable_count; ++index) {
            if (descriptor->variables[index] == variable) return true;
        }
        return false;
    }

    std::vector<uint8_t> Encode_(const config::s_config_snapshot_descriptor* descriptor,
        const config::s_config_state* state,
        const std::vector<config::s_mod_requirement>* required_mods) {
        const uint32_t size = config::Config_Snapshot_Size(
            descriptor, state, required_mods);
        std::vector<uint8_t> buffer(size);
        SERIAL::Writer_ writer(buffer.data(), size, "snapshot write failed");
        config::Write_Config_Snapshot(
            &writer, descriptor, state, required_mods);
        buffer.resize(writer.Length());
        return buffer;
    }
} // namespace

DOCTEST_TEST_CASE("snapshot rejects duplicate aggregate leaf paths") {
    const config::s_variable_accessor accessor = Accessor_(
        "ai.autobuild_rules.candidates.7.weight.base");
    DOCTEST_REQUIRE(accessor.variable_id != config::CONFIG_NO_VARIABLE);
    config::s_config_state source;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&source));
    config::s_typed_value value;
    DOCTEST_REQUIRE(config::Read_State_Leaf_Value(
        &source, accessor.variable_id, accessor.leaf_index, &value));
    ++value.integer_value;
    DOCTEST_REQUIRE(config::Set_State_Leaf_Value(
        &source, accessor.variable_id, accessor.leaf_index, value));

    std::vector<uint8_t> encoded = Encode_(
        &config::synchronized_config_snapshot, &source);
    DOCTEST_REQUIRE(encoded.size() > 16);
    const size_t entry_size = encoded.size() - 16;
    encoded.insert(encoded.end(), encoded.begin() + 16, encoded.end());
    const uint32_t payload_size = static_cast<uint32_t>(entry_size * 2);
    encoded[8] = static_cast<uint8_t>(payload_size);
    encoded[9] = static_cast<uint8_t>(payload_size >> 8);
    encoded[10] = static_cast<uint8_t>(payload_size >> 16);
    encoded[11] = static_cast<uint8_t>(payload_size >> 24);
    encoded[12] = 2;
    encoded[13] = 0;
    encoded[14] = 0;
    encoded[15] = 0;

    config::s_config_state decoded;
    SERIAL::Reader_ reader(encoded.data(), static_cast<uint32_t>(encoded.size()), "snapshot read failed");
    DOCTEST_CHECK_FALSE(config::Read_Config_Snapshot(
        &reader, &config::synchronized_config_snapshot, &decoded));
}

DOCTEST_TEST_CASE("snapshot metadata is canonical and group selected") {
    const config::s_config_snapshot_descriptor* descriptor =
        &config::synchronized_config_snapshot;
    DOCTEST_REQUIRE(descriptor->variable_count != 0);
    DOCTEST_CHECK((descriptor->group_mask &
                      (UINT32_C(1) << config::CONFIG_GROUP_INTERFACE)) == 0);
    for (uint32_t index = 0; index < descriptor->variable_count; ++index) {
        const config::t_variable_id variable = descriptor->variables[index];
        DOCTEST_CHECK(variable < config::variable_count);
        DOCTEST_CHECK(config::Get_Variable_Group(variable) !=
            config::CONFIG_GROUP_INTERFACE);
        if (index != 0) {
            std::string previous_path;
            std::string path;
            DOCTEST_REQUIRE(config::Get_Variable_Name(
                descriptor->variables[index - 1], &previous_path));
            DOCTEST_REQUIRE(config::Get_Variable_Name(variable, &path));
            DOCTEST_CHECK(previous_path < path);
        }
    }
}

DOCTEST_TEST_CASE("classic galaxy star counts are saved and synchronized") {
    const config::s_config_snapshot_descriptor* descriptors[] = {
        &config::saved_config_snapshot,
        &config::synchronized_config_snapshot,
    };
    for (const config::s_config_snapshot_descriptor* descriptor : descriptors) {
        for (int size = GALAXY_SIZE_SMALL; size <= GALAXY_SIZE_HUGE; ++size) {
            const std::string path = "map.star_count." + std::to_string(size);
            DOCTEST_CHECK(Snapshot_Contains_(descriptor, path.c_str()));
        }
    }
}

DOCTEST_TEST_CASE("proximity detection belongs to saved and synchronized snapshots") {
    const char* paths[] = {
        "detection.scan_bonus.planet",
        "detection.scan_bonus.fleet",
    };
    const config::s_config_snapshot_descriptor* descriptors[] = {
        &config::saved_config_snapshot,
        &config::synchronized_config_snapshot,
    };

    for (const config::s_config_snapshot_descriptor* descriptor : descriptors) {
        for (const char* path : paths) {
            const config::t_variable_id variable = Variable_(path);
            DOCTEST_REQUIRE(variable != config::CONFIG_NO_VARIABLE);
            DOCTEST_CHECK(config::Get_Variable_Group(variable) ==
                config::CONFIG_GROUP_GAMEPLAY);

            bool found = false;
            for (uint32_t index = 0; index < descriptor->variable_count; ++index) {
                found = found || descriptor->variables[index] == variable;
            }
            DOCTEST_CHECK(found);
        }
    }
}

DOCTEST_TEST_CASE("map generation tables belong to saved and synchronized snapshots") {
    const char* paths[] = {
        "map.climate_weight_young_average.0.0",
        "map.mineral_class.0.0",
    };
    const config::s_config_snapshot_descriptor* descriptors[] = {
        &config::saved_config_snapshot,
        &config::synchronized_config_snapshot,
    };

    for (const config::s_config_snapshot_descriptor* descriptor : descriptors) {
        for (const char* path : paths) {
            DOCTEST_CHECK(Snapshot_Contains_(descriptor, path));
        }
    }
}

DOCTEST_TEST_CASE("Crystal equipment round trips through gameplay snapshots") {
    const config::t_variable_id shield =
        Variable_("ships.crystal.shared.shield");
    const config::t_variable_id computer =
        Variable_("ships.crystal.shared.computer");
    const config::t_variable_id special =
        Variable_("ships.crystal.shared.special");
    const config::t_variable_id death_spore =
        Variable_("interface.death_spore_list_damage");
    DOCTEST_REQUIRE(shield != config::CONFIG_NO_VARIABLE);
    DOCTEST_REQUIRE(computer != config::CONFIG_NO_VARIABLE);
    DOCTEST_REQUIRE(special != config::CONFIG_NO_VARIABLE);
    DOCTEST_REQUIRE(death_spore != config::CONFIG_NO_VARIABLE);

    const config::s_config_snapshot_descriptor* descriptors[] = {
        &config::saved_config_snapshot,
        &config::synchronized_config_snapshot,
    };
    for (const config::s_config_snapshot_descriptor* descriptor : descriptors) {
        bool found_shield = false;
        bool found_computer = false;
        bool found_special = false;
        bool found_death_spore = false;
        for (uint32_t index = 0; index < descriptor->variable_count; ++index) {
            const config::t_variable_id variable = descriptor->variables[index];
            found_shield = found_shield || variable == shield;
            found_computer = found_computer || variable == computer;
            found_special = found_special || variable == special;
            found_death_spore = found_death_spore || variable == death_spore;
        }
        DOCTEST_CHECK(found_shield);
        DOCTEST_CHECK(found_computer);
        DOCTEST_CHECK(found_special);
        DOCTEST_CHECK_FALSE(found_death_spore);

        config::s_config_state source;
        DOCTEST_REQUIRE(config::Initialize_Defaults(&source));
        source.values[shield].integer_value = SHIELD_CLASS_III_SHIELD;
        source.values[computer].integer_value =
            COMPUTER_POSITRONIC_COMPUTER;
        source.values[special].integer_value = SPECIAL_BATTLE_SCANNER;
        const std::vector<uint8_t> encoded = Encode_(descriptor, &source);

        config::s_config_state decoded;
        DOCTEST_REQUIRE(config::Initialize_Defaults(&decoded));
        SERIAL::Reader_ reader(encoded.data(), (uint32_t)encoded.size(), "snapshot read failed");
        DOCTEST_REQUIRE(config::Read_Config_Snapshot(
            &reader, descriptor, &decoded));
        DOCTEST_CHECK(decoded.values[shield].integer_value ==
            SHIELD_CLASS_III_SHIELD);
        DOCTEST_CHECK(decoded.values[computer].integer_value ==
            COMPUTER_POSITRONIC_COMPUTER);
        DOCTEST_CHECK(decoded.values[special].integer_value ==
            SPECIAL_BATTLE_SCANNER);
    }
}

DOCTEST_TEST_CASE("raid item counts round trip through saved and synchronized snapshots") {
    const config::t_variable_id variable =
        Variable_("combat.boarding.raid_items_per_marine");
    DOCTEST_REQUIRE(variable != config::CONFIG_NO_VARIABLE);
    const config::s_config_snapshot_descriptor* descriptors[] = {
        &config::saved_config_snapshot,
        &config::synchronized_config_snapshot,
    };
    const int64_t item_counts[] = {0, 1, 2, 127};

    for (const config::s_config_snapshot_descriptor* descriptor : descriptors) {
        for (int64_t item_count : item_counts) {
            config::s_config_state source;
            DOCTEST_REQUIRE(config::Initialize_Defaults(&source));
            source.values[variable].integer_value = item_count;
            const std::vector<uint8_t> encoded = Encode_(descriptor, &source);

            config::s_config_state decoded;
            DOCTEST_REQUIRE(config::Initialize_Defaults(&decoded));
            SERIAL::Reader_ reader(encoded.data(), (uint32_t)encoded.size(), "snapshot read failed");
            DOCTEST_REQUIRE(config::Read_Config_Snapshot(
                &reader, descriptor, &decoded));
            DOCTEST_CHECK(reader.Length() == encoded.size());
            DOCTEST_CHECK(decoded.values[variable].integer_value == item_count);
        }
    }
}

DOCTEST_TEST_CASE("ESD and Megafluxers word values are saved and synchronized") {
    const config::t_variable_id esd = Variable_("combat.esd_bonus");
    const config::t_variable_id megafluxers =
        Variable_("design.megafluxers_tactical_bonus");
    DOCTEST_REQUIRE(esd != config::CONFIG_NO_VARIABLE);
    DOCTEST_REQUIRE(megafluxers != config::CONFIG_NO_VARIABLE);
    const config::s_config_snapshot_descriptor* descriptors[] = {
        &config::saved_config_snapshot,
        &config::synchronized_config_snapshot,
    };

    for (const config::s_config_snapshot_descriptor* descriptor : descriptors) {
        config::s_config_state source;
        DOCTEST_REQUIRE(config::Initialize_Defaults(&source));
        source.values[esd].integer_value = 65535;
        source.values[megafluxers].integer_value = 65535;
        const std::vector<uint8_t> encoded = Encode_(descriptor, &source);

        config::s_config_state decoded;
        DOCTEST_REQUIRE(config::Initialize_Defaults(&decoded));
        SERIAL::Reader_ reader(encoded.data(), (uint32_t)encoded.size(), "snapshot read failed");
        DOCTEST_REQUIRE(config::Read_Config_Snapshot(
            &reader, descriptor, &decoded));
        DOCTEST_CHECK(decoded.values[esd].integer_value == 65535);
        DOCTEST_CHECK(decoded.values[megafluxers].integer_value == 65535);
    }
}

DOCTEST_TEST_CASE("technology table word domains are saved and synchronized") {
    const char* paths[] = {
        "design.building.7.cost",
        "design.building.7.maintenance",
        "design.weapon.1.cost",
        "design.weapon.4.min_damage",
        "design.special.4.strategic",
    };
    const config::s_config_snapshot_descriptor* descriptors[] = {
        &config::saved_config_snapshot,
        &config::synchronized_config_snapshot,
    };

    for (const config::s_config_snapshot_descriptor* descriptor : descriptors) {
        config::s_config_state source;
        DOCTEST_REQUIRE(config::Initialize_Defaults(&source));
        for (const char* path : paths) {
            DOCTEST_CAPTURE(path);
            const config::t_variable_id variable = Variable_(path);
            DOCTEST_REQUIRE(variable != config::CONFIG_NO_VARIABLE);
            source.values[variable].integer_value = 65535;
        }

        const std::vector<uint8_t> encoded = Encode_(descriptor, &source);
        config::s_config_state decoded;
        DOCTEST_REQUIRE(config::Initialize_Defaults(&decoded));
        SERIAL::Reader_ reader(encoded.data(), (uint32_t)encoded.size(), "snapshot read failed");
        DOCTEST_REQUIRE(config::Read_Config_Snapshot(
            &reader, descriptor, &decoded));
        for (const char* path : paths) {
            DOCTEST_CHECK(decoded.values[Variable_(path)].integer_value == 65535);
        }
    }
}

DOCTEST_TEST_CASE("global technology graph source state is saved and synchronized") {
    const char* paths[] = {
        "design.tech.137.tech_field_id",
        "design.tech_field.59.prev_field_id",
        "design.tech_field.59.next_field_id",
        "technology.starting_fields.0",
    };
    const config::s_config_snapshot_descriptor* descriptors[] = {
        &config::saved_config_snapshot,
        &config::synchronized_config_snapshot,
    };

    for (const config::s_config_snapshot_descriptor* descriptor : descriptors) {
        config::s_config_state source;
        DOCTEST_REQUIRE(config::Initialize_Defaults(&source));
        for (const char* path : paths) {
            const config::t_variable_id variable = Variable_(path);
            DOCTEST_REQUIRE(variable != config::CONFIG_NO_VARIABLE);
            DOCTEST_CHECK(Snapshot_Contains_(descriptor, path));
        }
        source.values[Variable_(paths[0])].integer_value =
            TECH_FIELD_ADVANCED_BIOLOGY;
        source.values[Variable_(paths[1])].integer_value =
            TECH_FIELD_ADVANCED_BIOLOGY;
        source.values[Variable_(paths[2])].integer_value =
            TECH_FIELD_ADVANCED_CHEMISTRY;
        source.values[Variable_(paths[3])].integer_value =
            TECH_FIELD_ADVANCED_ENGINEERING;
        const std::vector<uint8_t> encoded = Encode_(descriptor, &source);

        config::s_config_state decoded;
        DOCTEST_REQUIRE(config::Initialize_Defaults(&decoded));
        SERIAL::Reader_ reader(encoded.data(), (uint32_t)encoded.size(), "snapshot read failed");
        DOCTEST_REQUIRE(config::Read_Config_Snapshot(
            &reader, descriptor, &decoded));
        for (const char* path : paths) {
            DOCTEST_CHECK(decoded.values[Variable_(path)].integer_value ==
                source.values[Variable_(path)].integer_value);
        }
    }
}

DOCTEST_TEST_CASE("leader skill costs belong to saved and synchronized snapshots") {
    config::s_config_state source;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&source));
    const config::t_variable_id assassin =
        Variable_("leaders.skill_cost.0");
    DOCTEST_REQUIRE(assassin != config::CONFIG_NO_VARIABLE);
    source.values[assassin].integer_value = 255;

    const config::s_config_snapshot_descriptor* descriptors[] = {
        &config::saved_config_snapshot,
        &config::synchronized_config_snapshot,
    };

    for (const config::s_config_snapshot_descriptor* descriptor : descriptors) {
        for (int skill = 0; skill < 54; ++skill) {
            const std::string path =
                "leaders.skill_cost." + std::to_string(skill);
            DOCTEST_CHECK(Snapshot_Contains_(descriptor, path.c_str()));
        }

        const std::vector<uint8_t> encoded = Encode_(descriptor, &source);
        config::s_config_state decoded;
        DOCTEST_REQUIRE(config::Initialize_Defaults(&decoded));
        SERIAL::Reader_ reader(encoded.data(), (uint32_t)encoded.size(), "snapshot read failed");
        DOCTEST_REQUIRE(config::Read_Config_Snapshot(
            &reader, descriptor, &decoded));
        DOCTEST_CHECK(decoded.values[assassin].integer_value == 255);
    }
}

DOCTEST_TEST_CASE("Post-warp technologies and Android morale are saved and synchronized") {
    config::s_config_state source;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&source));
    const config::t_variable_id postwarp =
        Variable_("research.postwarp_tech.1");
    const config::t_variable_id android =
        Variable_("colony.android_morale");
    DOCTEST_REQUIRE(postwarp != config::CONFIG_NO_VARIABLE);
    DOCTEST_REQUIRE(android != config::CONFIG_NO_VARIABLE);
    source.values[postwarp].integer_value = 211;
    source.values[android].integer_value = COLCALC::ANDROID_MORALE_ALL_DROIDS;

    const config::s_config_snapshot_descriptor* descriptors[] = {
        &config::saved_config_snapshot,
        &config::synchronized_config_snapshot,
    };
    for (const config::s_config_snapshot_descriptor* descriptor : descriptors) {
        const std::vector<uint8_t> encoded = Encode_(descriptor, &source);
        config::s_config_state decoded;
        DOCTEST_REQUIRE(config::Initialize_Defaults(&decoded));
        SERIAL::Reader_ reader(encoded.data(), (uint32_t)encoded.size(), "snapshot read failed");
        DOCTEST_REQUIRE(config::Read_Config_Snapshot(
            &reader, descriptor, &decoded));
        DOCTEST_CHECK(decoded.values[postwarp].integer_value == 211);
        DOCTEST_CHECK(decoded.values[android].integer_value ==
            COLCALC::ANDROID_MORALE_ALL_DROIDS);
    }
}

DOCTEST_TEST_CASE("ship initiative modes are saved and synchronized") {
    const config::t_variable_id initiative =
        Variable_("combat.ship_initiative");
    DOCTEST_REQUIRE(initiative != config::CONFIG_NO_VARIABLE);
    const config::s_config_snapshot_descriptor* descriptors[] = {
        &config::saved_config_snapshot,
        &config::synchronized_config_snapshot,
    };
    const int8_t modes[] = {-1, 0, 1, 2};

    for (const config::s_config_snapshot_descriptor* descriptor : descriptors) {
        for (int8_t mode : modes) {
            config::s_config_state source;
            DOCTEST_REQUIRE(config::Initialize_Defaults(&source));
            source.values[initiative].integer_value = mode;

            const std::vector<uint8_t> encoded = Encode_(descriptor, &source);
            config::s_config_state decoded;
            DOCTEST_REQUIRE(config::Initialize_Defaults(&decoded));
            SERIAL::Reader_ reader(
                encoded.data(), (uint32_t)encoded.size(), "snapshot read failed");
            DOCTEST_REQUIRE(config::Read_Config_Snapshot(
                &reader, descriptor, &decoded));
            DOCTEST_CHECK(decoded.values[initiative].integer_value == mode);
        }
    }
}

DOCTEST_TEST_CASE("technology branch roots belong to saved and synchronized snapshots") {
    const char* paths[] = {
        "technology.starting_techfield.biology",
        "technology.starting_techfield.power",
        "technology.starting_techfield.physics",
        "technology.starting_techfield.construction",
        "technology.starting_techfield.force_fields",
        "technology.starting_techfield.chemistry",
        "technology.starting_techfield.computers",
        "technology.starting_techfield.sociology",
        "technology.starting_techfield.xenon",
    };
    const config::s_config_snapshot_descriptor* descriptors[] = {
        &config::saved_config_snapshot,
        &config::synchronized_config_snapshot,
    };

    for (const config::s_config_snapshot_descriptor* descriptor : descriptors) {
        for (const char* path : paths) {
            DOCTEST_CHECK(Snapshot_Contains_(descriptor, path));
        }
    }
}

DOCTEST_TEST_CASE("synchronized snapshot round trips race rules and stock races") {
    config::s_config_state source;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&source));
    const config::t_variable_id picks = Variable_("race.picks.starting");
    const config::t_variable_id race = Variable_("ai.stock_race.0.growth");
    const config::t_variable_id interface = Variable_("interface.scroll_interval");
    DOCTEST_REQUIRE(picks != config::CONFIG_NO_VARIABLE);
    DOCTEST_REQUIRE(race != config::CONFIG_NO_VARIABLE);
    DOCTEST_REQUIRE(interface != config::CONFIG_NO_VARIABLE);
    source.values[picks].integer_value = 12;
    source.values[race].integer_value = 2;

    const std::vector<uint8_t> encoded = Encode_(
        &config::synchronized_config_snapshot, &source);
    DOCTEST_CHECK(encoded.size() < config::CONFIG_MAX_SNAPSHOT_SIZE);
    DOCTEST_REQUIRE(encoded.size() >= 16);
    const uint32_t entry_count = static_cast<uint32_t>(encoded[12]) |
        static_cast<uint32_t>(encoded[13]) << 8 |
        static_cast<uint32_t>(encoded[14]) << 16 |
        static_cast<uint32_t>(encoded[15]) << 24;
    DOCTEST_CHECK(entry_count == 2);

    config::s_config_state decoded;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&decoded));
    decoded.values[interface].integer_value = 7;
    SERIAL::Reader_ reader(encoded.data(), (uint32_t)encoded.size(), "snapshot read failed");
    DOCTEST_REQUIRE(config::Read_Config_Snapshot(
        &reader, &config::synchronized_config_snapshot, &decoded));
    DOCTEST_CHECK(reader.Length() == encoded.size());
    DOCTEST_CHECK(decoded.values[picks].integer_value == 12);
    DOCTEST_CHECK(decoded.values[race].integer_value == 2);
    DOCTEST_CHECK(decoded.values[interface].integer_value != 7);
    DOCTEST_CHECK(decoded.values[interface].integer_value ==
        source.values[interface].integer_value);
}

DOCTEST_TEST_CASE("snapshot hard fails unknown or reordered host paths") {
    config::s_config_state source;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&source));
    source.values[Variable_("race.picks.starting")].integer_value = 12;
    std::vector<uint8_t> encoded = Encode_(
        &config::synchronized_config_snapshot, &source);
    DOCTEST_REQUIRE(encoded.size() > 24);

    const uint16_t path_size =
        (uint16_t)(encoded[16] | (uint16_t)encoded[17] << 8);
    DOCTEST_REQUIRE(path_size != 0);
    encoded[24] = encoded[24] == 'z' ? 'y' : 'z';

    config::s_config_state decoded;
    DOCTEST_REQUIRE(config::Read_Current_State(&decoded));
    const config::s_config_state original = decoded;
    SERIAL::Reader_ reader(encoded.data(), (uint32_t)encoded.size(), "snapshot read failed");
    DOCTEST_CHECK_FALSE(config::Read_Config_Snapshot(
        &reader, &config::synchronized_config_snapshot, &decoded));
    DOCTEST_CHECK(decoded.values[Variable_("race.picks.starting")].integer_value ==
        original.values[Variable_("race.picks.starting")].integer_value);
}

DOCTEST_TEST_CASE("snapshot rejects missing host entries without applying state") {
    config::s_config_state source;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&source));
    source.values[Variable_("race.picks.starting")].integer_value = 12;
    source.values[Variable_("ai.stock_race.0.growth")].integer_value = 2;
    std::vector<uint8_t> encoded = Encode_(
        &config::synchronized_config_snapshot, &source);
    DOCTEST_REQUIRE(encoded.size() > 16);
    encoded[12]--;

    TEST_CONFIG_SNAPSHOT::commit_count = 0;
    SERIAL::Reader_ reader(encoded.data(), (uint32_t)encoded.size(), "snapshot read failed");
    DOCTEST_CHECK_FALSE(config::Apply_Config_Snapshot(
        &reader, &config::synchronized_config_snapshot));
    DOCTEST_CHECK(TEST_CONFIG_SNAPSHOT::commit_count == 0);
}

DOCTEST_TEST_CASE("default snapshot contains no variable entries") {
    config::s_config_state defaults;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&defaults));
    const std::vector<uint8_t> encoded = Encode_(
        &config::synchronized_config_snapshot, &defaults);
    DOCTEST_REQUIRE(encoded.size() == 16);
    DOCTEST_CHECK(encoded[12] == 0);
    DOCTEST_CHECK(encoded[13] == 0);
    DOCTEST_CHECK(encoded[14] == 0);
    DOCTEST_CHECK(encoded[15] == 0);
}

DOCTEST_TEST_CASE("tracked and untracked states produce identical snapshots") {
    const config::t_variable_id picks =
        Variable_("race.picks.starting");
    const config::t_variable_id growth =
        Variable_("ai.stock_race.0.growth");
    DOCTEST_REQUIRE(picks != config::CONFIG_NO_VARIABLE);
    DOCTEST_REQUIRE(growth != config::CONFIG_NO_VARIABLE);

    config::s_config_state untracked;
    config::s_config_state tracked;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&untracked));
    DOCTEST_REQUIRE(config::Initialize_Tracked_Defaults(&tracked));
    untracked.values[picks].integer_value = 12;
    untracked.values[growth].integer_value = 2;
    config::s_typed_value picks_value = tracked.values[picks];
    config::s_typed_value growth_value = tracked.values[growth];
    picks_value.integer_value = 12;
    growth_value.integer_value = 2;
    DOCTEST_REQUIRE(config::Set_State_Value(
        &tracked, picks, picks_value));
    DOCTEST_REQUIRE(config::Set_State_Value(
        &tracked, growth, growth_value));

    const std::vector<uint8_t> expected = Encode_(
        &config::synchronized_config_snapshot, &untracked);
    const std::vector<uint8_t> actual = Encode_(
        &config::synchronized_config_snapshot, &tracked);
    DOCTEST_CHECK(actual == expected);

    config::s_prepared_config_snapshot prepared;
    DOCTEST_REQUIRE(config::Prepare_Config_Snapshot(
        &config::synchronized_config_snapshot,
        &tracked,
        nullptr,
        &prepared));
    std::vector<uint8_t> prepared_bytes(prepared.bytes.size());
    SERIAL::Writer_ writer(prepared_bytes.data(),
        static_cast<uint32_t>(prepared_bytes.size()),
        "prepared snapshot write failed");
    config::Write_Prepared_Config_Snapshot(&writer, &prepared);
    DOCTEST_CHECK(prepared_bytes == expected);

    config::s_config_state decoded;
    SERIAL::Reader_ reader(actual.data(),
        static_cast<uint32_t>(actual.size()),
        "snapshot read failed");
    DOCTEST_REQUIRE(config::Read_Config_Snapshot(
        &reader, &config::synchronized_config_snapshot, &decoded));
    DOCTEST_CHECK(config::State_Tracks_Non_Default_Values(&decoded));
    DOCTEST_CHECK(config::State_Value_Is_Non_Default(&decoded, picks));
    DOCTEST_CHECK(config::State_Value_Is_Non_Default(&decoded, growth));
}

DOCTEST_TEST_CASE("snapshot persists and reapplies an ordered required mod list") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    state.values[Variable_("race.picks.starting")].integer_value = 12;
    const std::vector<config::s_mod_requirement> required_mods = {
        {"balance", "1"},
        {"shared-assets", "3.4"},
    };
    const std::vector<uint8_t> encoded = Encode_(
        &config::synchronized_config_snapshot, &state, &required_mods);
    DOCTEST_REQUIRE(encoded.size() > 20);
    DOCTEST_CHECK(encoded[4] == 1);
    DOCTEST_CHECK(encoded[5] == 0);
    DOCTEST_CHECK(encoded[6] == 24);
    DOCTEST_CHECK(encoded[7] == 0);

    config::s_config_snapshot decoded;
    SERIAL::Reader_ reader(encoded.data(), (uint32_t)encoded.size(), "snapshot read failed");
    DOCTEST_REQUIRE(config::Read_Config_Snapshot(
        &reader, &config::synchronized_config_snapshot, &decoded));
    DOCTEST_CHECK(decoded.has_required_mods);
    DOCTEST_CHECK(decoded.required_mods == required_mods);
    DOCTEST_CHECK(decoded.state.values[Variable_("race.picks.starting")].integer_value == 12);

    TEST_CONFIG_SNAPSHOT::commit_count = 0;
    TEST_CONFIG_SNAPSHOT::required_mods.clear();
    SERIAL::Reader_ apply_reader(encoded.data(), (uint32_t)encoded.size(), "snapshot read failed");
    DOCTEST_REQUIRE(config::Apply_Config_Snapshot(
        &apply_reader, &config::synchronized_config_snapshot));
    DOCTEST_CHECK(TEST_CONFIG_SNAPSHOT::commit_count == 1);
    DOCTEST_CHECK(TEST_CONFIG_SNAPSHOT::required_mods == required_mods);
}

DOCTEST_TEST_CASE("values-only version one snapshot retains legacy application") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&state));
    const std::vector<uint8_t> encoded = Encode_(
        &config::synchronized_config_snapshot, &state);
    DOCTEST_REQUIRE(encoded.size() == 16);

    TEST_CONFIG_SNAPSHOT::commit_count = 0;
    TEST_CONFIG_SNAPSHOT::required_mods = {{"unchanged", "1.0.0"}};
    SERIAL::Reader_ reader(encoded.data(), (uint32_t)encoded.size(), "snapshot read failed");
    DOCTEST_REQUIRE(config::Apply_Config_Snapshot(
        &reader, &config::synchronized_config_snapshot));
    DOCTEST_CHECK(TEST_CONFIG_SNAPSHOT::commit_count == 1);
    DOCTEST_CHECK(TEST_CONFIG_SNAPSHOT::required_mods ==
        std::vector<config::s_mod_requirement>{{"unchanged", "1.0.0"}});
}

DOCTEST_TEST_CASE("IDs-only version one snapshot retains legacy mod application") {
    const char mod_id[] = "legacy-mod";
    const uint32_t payload_size = sizeof(uint16_t) + sizeof(mod_id) - 1;
    std::vector<uint8_t> encoded(20 + payload_size);
    SERIAL::Writer_ writer(encoded.data(), (uint32_t)encoded.size(), "snapshot write failed");
    writer.Write_Uint32(0x4746434f);
    writer.Write_Uint16(1);
    writer.Write_Uint16(20);
    writer.Write_Uint32(payload_size);
    writer.Write_Uint32(0);
    writer.Write_Uint32(1);
    writer.Write_Uint16(sizeof(mod_id) - 1);
    writer.Write_Bytes(mod_id, sizeof(mod_id) - 1);

    config::s_config_snapshot decoded;
    SERIAL::Reader_ reader(encoded.data(), (uint32_t)encoded.size(), "snapshot read failed");
    DOCTEST_REQUIRE(config::Read_Config_Snapshot(
        &reader, &config::synchronized_config_snapshot, &decoded));
    DOCTEST_REQUIRE(decoded.required_mods.size() == 1);
    DOCTEST_CHECK(decoded.required_mods[0].id == mod_id);
    DOCTEST_CHECK(decoded.required_mods[0].version.empty());
}

DOCTEST_TEST_CASE("saved and synchronized snapshots share the same codec") {
    config::s_config_state state;
    DOCTEST_REQUIRE(config::Read_Current_State(&state));
    const uint32_t synchronized_size = config::Config_Snapshot_Size(
        &config::synchronized_config_snapshot, &state);
    const uint32_t saved_size = config::Config_Snapshot_Size(
        &config::saved_config_snapshot, &state);
    DOCTEST_CHECK(synchronized_size != 0);
    DOCTEST_CHECK(saved_size != 0);
}