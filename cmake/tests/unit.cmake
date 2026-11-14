include("${CMAKE_CURRENT_LIST_DIR}/shared_objects.cmake")

set(ORION2RE_DEFAULT_CONFIG_SOURCES
    src/game/aibuild_config.cpp
    src/game/aidesign_config.cpp
    src/game/aimove_config.cpp
    src/game/antarans_config.cpp
    src/game/bomb_config.cpp
    src/game/design_defaults_config.cpp
    src/game/ericnet_config.cpp
    src/game/events_config.cpp
    src/game/info_config.cpp
    src/game/invasion_config.cpp
    src/game/boarding_config.cpp
    src/game/build_queue_config.cpp
    src/game/cmbtfir2_config.cpp
    src/game/cmbtfire_config.cpp
    src/game/cmbtmis_config.cpp
    src/game/cmbtmov1_config.cpp
    src/game/cmbtspec_config.cpp
    src/game/colmove_config.cpp
    src/game/colsum_config.cpp
    src/game/combat_config.cpp
    src/game/combat1_config.cpp
    src/game/combfind_config.cpp
    src/game/combinit_config.cpp
    src/game/colonize_config.cpp
    src/game/colony_config.cpp
    src/game/council_config.cpp
    src/game/eric_config.cpp
    src/game/flt1_config.cpp
    src/game/initgame_config.cpp
    src/game/initship_config.cpp
    src/game/loadsave_config.cpp
    src/game/mainmenu_config.cpp
    src/game/maintain_config.cpp
    src/game/mainscr_config.cpp
    src/game/mox2_config.cpp
    src/game/multplay_config.cpp
    src/game/newtech_config.cpp
    src/game/officer_config.cpp
    src/game/racecust_config.cpp
    src/game/racesel_config.cpp
    src/game/report_config.cpp
    src/game/savegame_config.cpp
    src/game/score_config.cpp
    src/game/settler_config.cpp
    src/game/shipmove_config.cpp
    src/game/shipstak_config.cpp
    src/game/spy_config.cpp
    src/game/tech_config.cpp
    src/game/techinit_config.cpp
)

orion2re_test_shared_sources(ORION2RE_DEFAULT_CONFIG_OBJECTS ${ORION2RE_DEFAULT_CONFIG_SOURCES})
add_library(orion2re_test_config_defaults STATIC
    ${ORION2RE_DEFAULT_CONFIG_OBJECTS}
)
target_compile_features(orion2re_test_config_defaults PRIVATE cxx_std_20)
target_include_directories(orion2re_test_config_defaults PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(orion2re_test_config_defaults PRIVATE SDL3::Headers)
orion2re_enable_sanitizers(orion2re_test_config_defaults)

function(orion2re_add_unit_test TARGET_NAME)
    cmake_parse_arguments(ORION2RE_TEST "LINK_SDL" "" "SOURCES;PRODUCTION_SOURCES;FAKE_SOURCES;LIBRARIES" ${ARGN})

    if("src/game/compat.cpp" IN_LIST ORION2RE_TEST_PRODUCTION_SOURCES)
        list(APPEND ORION2RE_TEST_PRODUCTION_SOURCES src/game/user_paths.cpp src/game/legacy_sort.cpp)
    endif()

    orion2re_test_shared_sources(ORION2RE_TEST_OBJECTS
        ${ORION2RE_TEST_SOURCES}
        ${ORION2RE_TEST_PRODUCTION_SOURCES}
        ${ORION2RE_TEST_FAKE_SOURCES}
        src/pch.cpp
    )
    add_executable(${TARGET_NAME}
        tests/test_main.cpp
        ${ORION2RE_TEST_OBJECTS}
    )

    target_compile_features(${TARGET_NAME} PRIVATE cxx_std_20)
    target_include_directories(${TARGET_NAME} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${ORION2RE_VENDORED_DOCTEST_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
    )

    target_precompile_headers(${TARGET_NAME} PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/src/pch.h>"
    )

    target_link_libraries(${TARGET_NAME} PRIVATE SDL3::Headers)

    if(ORION2RE_TEST_LINK_SDL)
        target_link_libraries(${TARGET_NAME} PRIVATE SDL3::SDL3-static)
    endif()

    target_link_libraries(${TARGET_NAME} PRIVATE
        orion2re_test_config_defaults
        ${ORION2RE_TEST_LIBRARIES}
    )
    orion2re_enable_sanitizers(${TARGET_NAME})

    if(MSVC)
        target_compile_options(${TARGET_NAME} PRIVATE /W3 /permissive-)
        target_compile_definitions(${TARGET_NAME} PRIVATE
            WIN32
            DOCTEST_CONFIG_NO_WINDOWS_SEH
            DOCTEST_CONFIG_NO_POSIX_SIGNALS
            $<$<CONFIG:Debug>:_CRT_SECURE_NO_DEPRECATE>
            $<$<CONFIG:Debug>:_CRT_NONSTDC_NO_DEPRECATE>
        )
    endif()

    add_test(NAME ${TARGET_NAME} COMMAND ${TARGET_NAME})
endfunction()

# Reuse the real runtime for integration tests and game-data assertions.
function(orion2re_add_game_test TARGET_NAME)
    orion2re_add_unit_test(${TARGET_NAME}
        SOURCES ${ARGN}
        PRODUCTION_SOURCES
            $<TARGET_OBJECTS:orion2re_game_objects>
            vendors/libsmacker/smacker.c
            vendors/libsmacker/smk_bitstream.c
            vendors/libsmacker/smk_hufftree.c
            vendors/yyjson/src/yyjson.c
        LIBRARIES
            uv_a orion2re_lz4 orion2re_lua orion2re_scalers miniz orion2re_imgui
        LINK_SDL
    )
    target_include_directories(${TARGET_NAME} PRIVATE
        vendors/libsmacker vendors/yyjson/src
    )
endfunction()

orion2re_add_unit_test(test_user_paths
    SOURCES
        tests/unit/user_paths_test.cpp
    PRODUCTION_SOURCES
        src/game/compat.cpp
        src/game/dos.cpp
        src/startup_directory.cpp
    LINK_SDL
)

orion2re_add_unit_test(test_fields_helpers
    SOURCES
        tests/unit/fields_helpers_test.cpp
    FAKE_SOURCES
        tests/fakes/fake_exitm.cpp
        tests/fakes/fake_fields_helpers_runtime.cpp
        tests/fakes/fake_video.cpp
)

# Link this unit without dead stripping so every production dependency must be supplied.
orion2re_add_unit_test(test_multplay
    SOURCES
        tests/unit/multplay_test.cpp
    PRODUCTION_SOURCES
        src/game/multplay_fields.cpp
)

# Keep these focused units linkable without dead stripping on every platform.
orion2re_add_unit_test(test_netstart_pick_position
    SOURCES
        tests/unit/netstart_test.cpp
    PRODUCTION_SOURCES
        src/game/netstart_position.cpp
    FAKE_SOURCES
        tests/fakes/fake_netstart_runtime.cpp
    LINK_SDL
)

orion2re_add_unit_test(test_mainscr_reload
    SOURCES
        tests/unit/mainscr_test.cpp
    PRODUCTION_SOURCES
        src/game/mainscr_hotkeys.cpp
        src/game/mainscr_hud.cpp
        src/game/mainscr_view.cpp
    FAKE_SOURCES
        tests/fakes/fake_main_screen_hud_runtime.cpp
    LINK_SDL
)

orion2re_add_unit_test(test_arena
    SOURCES
        tests/unit/arena_test.cpp
    PRODUCTION_SOURCES
        src/game/arena.cpp
    FAKE_SOURCES
        tests/fakes/fake_arena_runtime.cpp
)

orion2re_add_unit_test(test_allocate
    SOURCES
        tests/unit/allocate_test.cpp
    PRODUCTION_SOURCES
        src/game/allocate.cpp
)

orion2re_add_unit_test(test_score
    SOURCES tests/unit/score_test.cpp
    PRODUCTION_SOURCES
        src/game/score_calculation.cpp
        src/game/allocate.cpp
    LINK_SDL
)

orion2re_add_game_test(test_resource_reload
    tests/unit/resource_reload_test.cpp
)


orion2re_add_unit_test(test_ai_state
    SOURCES
        tests/unit/ai_state_test.cpp
    PRODUCTION_SOURCES
        src/game/ai_state.cpp
    FAKE_SOURCES
        tests/fakes/fake_ai_state_runtime.cpp
)

orion2re_add_unit_test(test_config_arena_runtime
    SOURCES
        tests/unit/config_runtime_test.cpp
    PRODUCTION_SOURCES
        src/config/config_lifecycle.cpp
        src/config/config_decode.cpp
        src/config/text_encoding.cpp
        src/config/config_runtime.cpp
        src/game/arena.cpp
    FAKE_SOURCES
        tests/fakes/fake_arena_runtime.cpp
        tests/fakes/fake_config_arena_generated.cpp
    LINK_SDL
)

if(ORION2RE_BUILD_CONFIG_TOOL)
    orion2re_add_unit_test(test_config_schema
        SOURCES
            tests/unit/config_schema_test.cpp
        PRODUCTION_SOURCES
            tools/config/file_util.cpp
            tools/config/generate_cpp.cpp
            tools/config/generate_docs.cpp
            tools/config/json5_reader.cpp
            tools/config/schema_model.cpp
            vendors/yyjson/src/yyjson.c
    )
    target_include_directories(test_config_schema PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/config"
        "${ORION2RE_VENDORED_YYJSON_DIR}/src"
    )
    target_compile_definitions(test_config_schema PRIVATE
        CONFIG_SCHEMA_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data/config/schema"
        CONFIG_MAIN_SCHEMA_PATH="${CMAKE_CURRENT_SOURCE_DIR}/specs/config/main.json5"
    )
endif()

orion2re_add_unit_test(test_cfg_parser
    SOURCES
        tests/unit/cfg_parser_test.cpp
    PRODUCTION_SOURCES
        src/config/cfg_parser.cpp
)

orion2re_add_unit_test(test_config_writer
    SOURCES
        tests/unit/config_writer_test.cpp
    PRODUCTION_SOURCES
        src/config/cfg_parser.cpp
        src/config/config_writer.cpp
)

orion2re_add_unit_test(test_mod_catalogue
    SOURCES
        tests/unit/mod_catalogue_test.cpp
    PRODUCTION_SOURCES
        src/config/cfg_parser.cpp
        src/config/mod_catalogue.cpp
        src/config/mod_manifest.cpp
        src/game/compat.cpp
        src/game/vfs.cpp
        src/game/vfs_file.cpp
        src/game/vfs_lbx.cpp
        src/game/vfs_path.cpp
        src/game/vfs_zip.cpp
        vendors/yyjson/src/yyjson.c
    LIBRARIES
        miniz
    LINK_SDL
)
target_include_directories(test_mod_catalogue PRIVATE
    "${ORION2RE_VENDORED_YYJSON_DIR}/src"
)

orion2re_add_unit_test(test_config_enums
    SOURCES
        tests/unit/config_enums_test.cpp
    PRODUCTION_SOURCES
        src/config/generated/config_enums.cpp
)

orion2re_add_unit_test(test_lua_runtime
    SOURCES
        tests/unit/lua_runtime_test.cpp
    PRODUCTION_SOURCES
        src/lua/lua_runtime.cpp
    LIBRARIES
        orion2re_lua
)

orion2re_add_unit_test(test_lua_mod_runtime
    SOURCES
        tests/unit/lua_galaxy_migration_test.cpp
        tests/unit/lua_game_runtime_test.cpp
        tests/unit/lua_mod_runtime_test.cpp
    PRODUCTION_SOURCES
        src/game/sha256.cpp
        src/game/random.cpp
        src/game/mapgen_names.cpp
        src/config/generated/config_enums.cpp
        src/lua/lua_game_dispatch.cpp
        src/lua/lua_game_leader_nebula.cpp
        src/lua/lua_game_planet.cpp
        src/lua/lua_game_player.cpp
        src/lua/lua_game_ship.cpp
        src/lua/lua_game_ship_identity.cpp
        src/lua/lua_game_state.cpp
        src/lua/lua_game_storage_reference.cpp
        src/lua/lua_game_technology.cpp
        src/lua/lua_fingerprint_compare.cpp
        src/lua/lua_mod_runtime.cpp
        src/lua/lua_game_traits.cpp
        src/lua/lua_network.cpp
        src/lua/lua_registry.cpp
        src/lua/lua_runtime.cpp
        src/lua/lua_save.cpp
        src/lua/lua_storage.cpp
        src/game/serialization.cpp
        src/game/raceopt_config.cpp
        src/game/techdata.cpp
        src/game/techinit.cpp
        src/game/compat.cpp
        src/game/vfs.cpp
        src/game/vfs_file.cpp
        src/game/vfs_lbx.cpp
        src/game/vfs_path.cpp
        src/game/vfs_zip.cpp
    FAKE_SOURCES
        tests/fakes/fake_lua_game_state.cpp
        tests/fakes/fake_lua_network_config.cpp
        tests/fakes/fake_string_table_loader.cpp
    LIBRARIES
        miniz
        orion2re_lua
    LINK_SDL
)
target_compile_definitions(test_lua_mod_runtime PRIVATE
    LUA_MIGRATION_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/lua"
    LUA_SOURCE_PACKAGE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/mods"
)

orion2re_add_unit_test(test_sha256
    SOURCES
        tests/unit/sha256_test.cpp
    PRODUCTION_SOURCES
        src/game/sha256.cpp
)

orion2re_add_unit_test(test_config_decode
    SOURCES
        tests/unit/config_decode_test.cpp
    PRODUCTION_SOURCES
        src/config/cfg_parser.cpp
        src/config/config_decode.cpp
        src/config/text_encoding.cpp
        src/config/config_runtime.cpp
        src/config/generated/config.cpp
        src/game/arena.cpp
        src/game/aidata_config.cpp
        src/game/colbldg_config.cpp
        src/game/colcalc_config.cpp
        src/game/colmove_config.cpp
        src/game/combat1_config.cpp
        src/game/design_config.cpp
        src/game/ericnet_config.cpp
        src/game/ericnet_config_validation.cpp
        src/game/events_config.cpp
        src/game/invasion_config.cpp
        src/game/info_config.cpp
        src/game/initship_config_validation.cpp
        src/game/mapgen_config.cpp
        src/game/maintain_config.cpp
        src/game/newtech_config.cpp
        src/game/raceopt_config.cpp
        src/game/ship_config.cpp
        src/game/spy_config_validation.cpp
        src/game/spy_bonus.cpp
        src/game/techdata.cpp
    FAKE_SOURCES
        tests/fakes/fake_arena_runtime.cpp
        tests/fakes/fake_config_runtime.cpp
    LINK_SDL
)

orion2re_add_game_test(test_config_bindings
    tests/unit/config_bindings_test.cpp
    tests/unit/main_screen_config_test.cpp
)

orion2re_add_unit_test(test_config_lifecycle
    SOURCES
        tests/unit/config_lifecycle_test.cpp
    PRODUCTION_SOURCES
        src/config/cfg_parser.cpp
        src/config/config_decode.cpp
        src/config/text_encoding.cpp
        src/config/config_lifecycle.cpp
        src/config/config_runtime.cpp
        src/config/generated/config.cpp
        src/game/arena.cpp
        src/game/aidata_config.cpp
        src/game/colbldg_config.cpp
        src/game/colcalc_config.cpp
        src/game/colmove_config.cpp
        src/game/design_config.cpp
        src/game/ericnet_config.cpp
        src/game/ericnet_config_validation.cpp
        src/game/events_config.cpp
        src/game/invasion_config.cpp
        src/game/info_config.cpp
        src/game/initship_config_validation.cpp
        src/game/mapgen_config.cpp
        src/game/raceopt_config.cpp
        src/game/ship_config.cpp
        src/game/spy_config_validation.cpp
        src/game/spy_bonus.cpp
        src/game/techdata.cpp
    FAKE_SOURCES
        tests/fakes/fake_arena_runtime.cpp
        tests/fakes/fake_config_runtime.cpp
    LINK_SDL
)

orion2re_add_unit_test(test_startup_directory
    SOURCES
        tests/unit/startup_directory_test.cpp
    PRODUCTION_SOURCES
        src/startup_directory.cpp
        src/game/compat.cpp
        src/game/vfs.cpp
        src/game/vfs_file.cpp
        src/game/vfs_lbx.cpp
        src/game/vfs_path.cpp
        src/game/vfs_zip.cpp
    LIBRARIES
        miniz
    LINK_SDL
)

orion2re_add_unit_test(test_startup_options
    SOURCES
        tests/unit/startup_options_test.cpp
    PRODUCTION_SOURCES
        src/command_line.cpp
        src/startup_options.cpp
)

orion2re_add_unit_test(test_config_loader
    SOURCES
        tests/unit/config_loader_test.cpp
    PRODUCTION_SOURCES
        src/config/config_identity.cpp
        src/game/sha256.cpp
        src/config/cfg_parser.cpp
        src/config/config_decode.cpp
        src/config/text_encoding.cpp
        src/config/config_lifecycle.cpp
        src/config/config_loader.cpp
        src/config/config_snapshot.cpp
        src/config/config_writer.cpp
        src/config/mod_manifest.cpp
        src/config/mod_manager.cpp
        src/game/netturn.cpp
        src/lua/lua_fingerprint.cpp
        src/lua/lua_fingerprint_compare.cpp
        src/lua/lua_mod_runtime.cpp
        src/lua/lua_network.cpp
        src/config/generated/config_enums.cpp
        src/lua/lua_game_ship_identity.cpp
        src/lua/lua_game_traits.cpp
        src/lua/lua_registry.cpp
        src/lua/lua_runtime.cpp
        src/lua/lua_save.cpp
        src/lua/lua_storage.cpp
        src/config/config_runtime.cpp
        src/config/generated/config.cpp
        src/game/arena.cpp
        src/game/aidata_config.cpp
        src/game/colbldg_config.cpp
        src/game/colcalc_config.cpp
        src/game/colmove_config.cpp
        src/game/design_config.cpp
        src/game/ericnet_config.cpp
        src/game/ericnet_config_validation.cpp
        src/game/events_config.cpp
        src/game/invasion_config.cpp
        src/game/info_config.cpp
        src/game/initship_config_validation.cpp
        src/game/mapgen_config.cpp
        src/game/raceopt_config.cpp
        src/game/ship_config.cpp
        src/game/spy_config_validation.cpp
        src/game/spy_bonus.cpp
        src/game/techdata.cpp
        src/game/compat.cpp
        src/game/serialization.cpp
        src/game/vfs.cpp
        src/game/vfs_file.cpp
        src/game/vfs_lbx.cpp
        src/game/vfs_path.cpp
        src/game/vfs_zip.cpp
        vendors/yyjson/src/yyjson.c
    FAKE_SOURCES
        tests/fakes/fake_config_netturn_runtime.cpp
        tests/fakes/fake_config_refresh.cpp
        tests/fakes/fake_config_runtime.cpp
        tests/fakes/fake_lua_game_storage_reference.cpp
        tests/fakes/fake_lua_game_state_api.cpp
    LIBRARIES
        miniz
        orion2re_lua
    LINK_SDL
)
target_include_directories(test_config_loader PRIVATE
    "${ORION2RE_VENDORED_YYJSON_DIR}/src"
)
target_compile_definitions(test_config_loader PRIVATE
    LUA_SOURCE_PACKAGE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/mods"
)

orion2re_add_unit_test(test_settings_value
    SOURCES
        tests/unit/settings_value_test.cpp
    PRODUCTION_SOURCES
        src/config/settings_value.cpp
        src/config/config_runtime.cpp
        src/config/generated/config.cpp
        src/config/generated/config_enums.cpp
        src/config/config_identity.cpp
        src/game/sha256.cpp
        src/game/arena.cpp
        src/game/aidata_config.cpp
        src/game/colbldg_config.cpp
        src/game/colcalc_config.cpp
        src/game/colmove_config.cpp
        src/game/design_config.cpp
        src/game/ericnet_config.cpp
        src/game/ericnet_config_validation.cpp
        src/game/events_config.cpp
        src/game/invasion_config.cpp
        src/game/info_config.cpp
        src/game/initship_config_validation.cpp
        src/game/mapgen_config.cpp
        src/game/raceopt_config.cpp
        src/game/ship_config.cpp
        src/game/spy_config_validation.cpp
        src/game/spy_bonus.cpp
        src/game/techdata.cpp
    FAKE_SOURCES
        tests/fakes/fake_config_runtime.cpp
        tests/fakes/fake_exitm.cpp
        tests/fakes/fake_platform_exit.cpp
    LINK_SDL
)

orion2re_add_unit_test(test_settings_layout
    SOURCES
        tests/unit/settings_layout_test.cpp
    PRODUCTION_SOURCES
        src/game/settings_layout.cpp
)

orion2re_add_unit_test(test_settings_dropdown
    SOURCES
        tests/unit/settings_dropdown_test.cpp
    PRODUCTION_SOURCES
        src/game/settings_layout.cpp
)

orion2re_add_unit_test(test_config_snapshot
    SOURCES
        tests/unit/config_snapshot_test.cpp
    PRODUCTION_SOURCES
        src/config/config_identity.cpp
        src/config/config_snapshot.cpp
        src/game/sha256.cpp
        src/config/config_runtime.cpp
        src/config/generated/config.cpp
        src/game/serialization.cpp
        src/game/arena.cpp
        src/game/aidata_config.cpp
        src/game/colbldg_config.cpp
        src/game/colcalc_config.cpp
        src/game/colmove_config.cpp
        src/game/design_config.cpp
        src/game/ericnet_config.cpp
        src/game/ericnet_config_validation.cpp
        src/game/events_config.cpp
        src/game/invasion_config.cpp
        src/game/info_config.cpp
        src/game/initship_config_validation.cpp
        src/game/mapgen_config.cpp
        src/game/raceopt_config.cpp
        src/game/ship_config.cpp
        src/game/spy_config_validation.cpp
        src/game/spy_bonus.cpp
        src/game/techdata.cpp
    FAKE_SOURCES
        tests/fakes/fake_arena_runtime.cpp
        tests/fakes/fake_config_runtime.cpp
        tests/fakes/fake_config_snapshot_loader.cpp
    LINK_SDL
)

orion2re_add_unit_test(test_pop_helpers
    SOURCES
        tests/unit/pop_test.cpp
)

orion2re_add_unit_test(test_racecust
    SOURCES
        tests/unit/racecust_test.cpp
    PRODUCTION_SOURCES
        src/game/racecust.cpp
    FAKE_SOURCES
        tests/fakes/fake_racecust_runtime.cpp
)

orion2re_add_unit_test(test_text_format
    SOURCES
        tests/unit/text_format_test.cpp
    PRODUCTION_SOURCES
        src/game/strbuild.cpp
        src/game/text_format.cpp
        src/game/text_format_config.cpp
    FAKE_SOURCES
        tests/fakes/fake_text_format_runtime.cpp
)

orion2re_add_unit_test(test_fmtpara
    SOURCES
        tests/unit/fmtpara_test.cpp
    PRODUCTION_SOURCES
        src/game/fmtpara.cpp
    FAKE_SOURCES
        tests/fakes/fake_fmtpara_runtime.cpp
    LINK_SDL
)

orion2re_add_unit_test(test_spy
    SOURCES
        tests/unit/spy_test.cpp
    PRODUCTION_SOURCES
        src/game/spy_actions.cpp
        src/game/spy_bonus.cpp
    FAKE_SOURCES
        tests/fakes/fake_spy_runtime.cpp
)

orion2re_add_unit_test(test_aidudes
    SOURCES
        tests/unit/aidudes_test.cpp
    PRODUCTION_SOURCES
        src/game/ai_population.cpp
        src/game/fleet_maintenance.cpp
        src/game/aileader_reassignment.cpp
        src/game/aidudes_ship_upgrades.cpp
        src/game/legacy_sort.cpp
    FAKE_SOURCES
        tests/fakes/fake_aidudes_runtime.cpp
)

orion2re_add_unit_test(test_orbital_defense
    SOURCES
        tests/unit/orbital_defense_test.cpp
    PRODUCTION_SOURCES
        src/game/ai_empire_production.cpp
        src/game/colcalc_config.cpp
        src/game/legacy_sort.cpp
        src/game/ericnet.cpp
        src/game/production_rules.cpp
        src/game/special.cpp
        src/game/angle.cpp
        src/game/system_summary.cpp
    FAKE_SOURCES
        tests/fakes/fake_ericnet_runtime.cpp
        tests/fakes/fake_orbital_defense_runtime.cpp
)

if(MSVC)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    if(APPLE)
    else()
    endif()
endif()

orion2re_add_game_test(test_antarans
    tests/unit/antarans_test.cpp
)

orion2re_add_unit_test(test_ai_hypertech
    SOURCES
        tests/unit/aitech_test.cpp
    PRODUCTION_SOURCES
        src/game/tech_selection.cpp
    FAKE_SOURCES
        tests/fakes/fake_ai_hypertech_runtime.cpp
)

orion2re_add_unit_test(test_newtech_choose
    SOURCES
        tests/unit/newtech_test.cpp
    PRODUCTION_SOURCES
        src/game/newtech.cpp
    FAKE_SOURCES
        tests/fakes/fake_newtech_choose_runtime.cpp
)

orion2re_add_game_test(test_refit_cost
    tests/unit/refit_cost_test.cpp
    tests/fakes/fake_refit_cost_runtime.cpp
)

orion2re_add_unit_test(test_production_rules
    SOURCES
        tests/unit/production_rules_test.cpp
    PRODUCTION_SOURCES
        src/game/production_rules.cpp
)

orion2re_add_game_test(test_design_weapon_damage
    tests/unit/design_test.cpp
)

orion2re_add_game_test(test_list_navigation
    tests/unit/list_navigation_test.cpp
)

orion2re_add_unit_test(test_settler
    SOURCES
        tests/unit/settler_test.cpp
    PRODUCTION_SOURCES
        src/game/settler.cpp
    FAKE_SOURCES
        tests/fakes/fake_settler_runtime.cpp
)

orion2re_add_unit_test(test_colmove
    SOURCES
        tests/unit/colmove_test.cpp
    PRODUCTION_SOURCES
        src/game/colmove.cpp
        src/game/colmove_config.cpp
    FAKE_SOURCES
        tests/fakes/fake_colmove_runtime.cpp
    LINK_SDL
)

orion2re_add_unit_test(test_colony_production_helpers
    SOURCES
        tests/unit/colcalc_base_test.cpp
    PRODUCTION_SOURCES
        src/game/colcalc_base.cpp
        src/game/research_completion.cpp
        src/game/production_rules.cpp
        src/game/planet_environment.cpp
        src/game/plntsum_environment.cpp
        src/game/colcalc_config.cpp
    FAKE_SOURCES
        tests/fakes/fake_colcalc_base_runtime.cpp
)

orion2re_add_unit_test(test_plntsum_filters
    SOURCES
        tests/unit/plntsum_test.cpp
    PRODUCTION_SOURCES
        src/game/colony_lifecycle.cpp
        src/game/planet_environment.cpp
        src/game/plntsum_environment.cpp
        src/game/colcalc_config.cpp
        src/game/legacy_sort.cpp
        src/game/planet_summary.cpp
        src/game/harold_queries.cpp
        src/game/struct.cpp
    FAKE_SOURCES
        tests/fakes/fake_plntsum_runtime.cpp
        tests/fakes/fake_sdl_string.cpp
)

orion2re_add_unit_test(test_colony_production_draw
    SOURCES
        tests/unit/coldraw_test.cpp
    PRODUCTION_SOURCES
        src/game/colony_production_display.cpp
    FAKE_SOURCES
        tests/fakes/fake_coldraw_runtime.cpp
)

orion2re_add_unit_test(test_colony_scan_info
    SOURCES
        tests/unit/colsum_test.cpp
    PRODUCTION_SOURCES
        src/game/colsum_details.cpp
    FAKE_SOURCES
        tests/fakes/fake_colony_scan_info_runtime.cpp
    LINK_SDL
)

orion2re_add_unit_test(test_colony_pop_growth
    SOURCES
        tests/unit/colcalc_test.cpp
        tests/unit/colony_services_test.cpp
    PRODUCTION_SOURCES
        src/game/ai_planet_values.cpp
        src/game/aidata_config.cpp
        src/game/colcalc.cpp
        src/game/research_completion.cpp
        src/game/production_rules.cpp
        src/game/ground_unit_limits.cpp
        src/game/planet_environment.cpp
        src/game/plntsum_environment.cpp
        src/game/colcalc_config.cpp
        src/game/raceopt_config.cpp
        src/game/settler.cpp
        src/game/legacy_sort.cpp
        src/game/strbuild.cpp
    FAKE_SOURCES
        tests/fakes/fake_colony_pop_growth_runtime.cpp
)

orion2re_add_game_test(test_colony_replicators
    tests/unit/colcalc_main_test.cpp
    tests/fakes/fake_colony_replicators_runtime.cpp
)

orion2re_add_game_test(test_colony_queue
    tests/unit/colony_queue_test.cpp
)

orion2re_add_unit_test(test_build_queue
    SOURCES
        tests/unit/build_queue_test.cpp
    PRODUCTION_SOURCES
        src/game/build_queue.cpp
    FAKE_SOURCES
        tests/fakes/fake_build_queue_runtime.cpp
)

orion2re_add_game_test(test_colbldg
    tests/unit/colbldg_test.cpp
    tests/fakes/fake_colbldg_runtime.cpp
)

orion2re_add_unit_test(test_colonize
    SOURCES
        tests/unit/colonize_test.cpp
    PRODUCTION_SOURCES
        src/game/colonize.cpp
    FAKE_SOURCES
        tests/fakes/fake_colonize_runtime.cpp
)

orion2re_add_unit_test(test_report
    SOURCES
        tests/unit/report_test.cpp
    PRODUCTION_SOURCES
        src/game/report.cpp
        src/game/random.cpp
    FAKE_SOURCES
        tests/fakes/fake_report_runtime.cpp
)

orion2re_add_unit_test(test_events
    SOURCES
        tests/unit/events_test.cpp
    PRODUCTION_SOURCES
        src/game/events.cpp
        src/game/struct.cpp
    FAKE_SOURCES
        tests/fakes/fake_events_runtime.cpp
        tests/fakes/fake_string_message_loader.cpp
        tests/fakes/fake_sdl_string.cpp
)

orion2re_add_unit_test(test_combfind_rules
    SOURCES
        tests/unit/combfind_rules_test.cpp
    PRODUCTION_SOURCES
        src/game/combfind_rules.cpp
)

orion2re_add_game_test(test_mainpups_leader_technology
    tests/unit/mainpups_test.cpp
)

orion2re_add_unit_test(test_nextturn_order
    SOURCES
        tests/unit/nextturn_test.cpp
    PRODUCTION_SOURCES
        src/game/nextturn.cpp
        src/game/random.cpp
    FAKE_SOURCES
        tests/fakes/fake_nextturn_runtime.cpp
)

orion2re_add_game_test(test_nonplayer_spawn
    tests/unit/nonplayer_spawn_test.cpp
)

orion2re_add_unit_test(test_shipmove
    SOURCES
        tests/unit/shipmove_test.cpp
    PRODUCTION_SOURCES
        src/game/shipmove.cpp
        src/lua/lua_game_ship_identity.cpp
    FAKE_SOURCES
        tests/fakes/fake_shipmove_runtime.cpp
)

orion2re_add_unit_test(test_aimove
    SOURCES
        tests/unit/aimove_test.cpp
    PRODUCTION_SOURCES
        src/game/aimove_npc.cpp
        src/game/legacy_sort.cpp
        src/game/special.cpp
        src/game/angle.cpp
    FAKE_SOURCES
        tests/fakes/fake_aimove_runtime.cpp
)

orion2re_add_game_test(test_fleetpop
    tests/unit/fleetpop_test.cpp
    tests/fakes/fake_fleetpop_runtime.cpp
)

orion2re_add_unit_test(test_fleet_summary
    SOURCES
        tests/unit/flt1_test.cpp
    PRODUCTION_SOURCES
        src/game/fleet_summary.cpp
    FAKE_SOURCES
        tests/fakes/fake_fleet_summary_runtime.cpp
        tests/fakes/fake_sdl_string.cpp
)

orion2re_add_game_test(test_geo
    tests/unit/geo_test.cpp
    tests/fakes/fake_geo_runtime.cpp
)

orion2re_add_unit_test(test_homegen
    SOURCES
        tests/unit/homegen_test.cpp
    PRODUCTION_SOURCES
        src/game/homegen_planets.cpp
        src/game/legacy_sort.cpp
    FAKE_SOURCES
        tests/fakes/fake_homegen_runtime.cpp
)

orion2re_add_unit_test(test_mapgen_planet_max
    SOURCES
        tests/unit/mapgen_test.cpp
    PRODUCTION_SOURCES
        src/game/mapgen.cpp
    FAKE_SOURCES
        tests/fakes/fake_exitm.cpp
        tests/fakes/fake_fields_helpers_runtime.cpp
        tests/fakes/fake_mapgen_planet_max_runtime.cpp
)

orion2re_add_game_test(test_colgcbt
    tests/unit/colgcbt_test.cpp
)

orion2re_add_unit_test(test_ground_commando
    SOURCES
        tests/unit/invasion_test.cpp
    PRODUCTION_SOURCES
        src/game/colony_policy.cpp
        src/game/colony_military_display.cpp
        src/game/invasion.cpp
        src/game/ground_unit_limits.cpp
        src/game/invasion_config.cpp
        src/game/officer_rules.cpp
        src/lua/lua_game_ship_identity.cpp
    FAKE_SOURCES
        tests/fakes/fake_ground_commando_runtime.cpp
)

orion2re_add_unit_test(test_eric
    SOURCES
        tests/unit/eric_test.cpp
    PRODUCTION_SOURCES
        src/game/eric.cpp
        src/game/animation_frames.cpp
        src/game/random.cpp
    FAKE_SOURCES
        tests/fakes/fake_eric_runtime.cpp
    LINK_SDL
)

if(MSVC)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    if(APPLE)
    else()
    endif()
endif()

orion2re_add_unit_test(test_ericnet
    SOURCES
        tests/unit/ericnet_test.cpp
    PRODUCTION_SOURCES
        src/game/colcalc_config.cpp
        src/game/ericnet.cpp
        src/game/ericnet_config.cpp
    FAKE_SOURCES
        tests/fakes/fake_ericnet_runtime.cpp
)

orion2re_add_unit_test(test_missile_defense
    SOURCES
        tests/unit/cmbtmis_test.cpp
    PRODUCTION_SOURCES
        src/game/missile_rules.cpp
        src/game/missile_tracking.cpp
    FAKE_SOURCES
        tests/fakes/fake_exitm.cpp
        tests/fakes/fake_fields_helpers_runtime.cpp
)

orion2re_add_game_test(test_diplomac
    tests/unit/diplomac_test.cpp
    tests/fakes/fake_diplomac_runtime.cpp
)

orion2re_add_unit_test(test_npcdiplo
    SOURCES
        tests/unit/npcdiplo_test.cpp
    PRODUCTION_SOURCES
        src/game/npcdiplo_rules.cpp
        src/game/legacy_sort.cpp
    FAKE_SOURCES
        tests/fakes/fake_npcdiplo_runtime.cpp
)

orion2re_add_unit_test(test_combat_stats
    SOURCES
        tests/unit/combat_stats_test.cpp
    PRODUCTION_SOURCES
        src/game/aipower_ground.cpp
        src/game/cmbtai_targets.cpp
        src/game/cmbtai_damage.cpp
        src/game/combat_range.cpp
        src/game/combat1_fields.cpp
        src/game/combat1_turn_order.cpp
        src/game/combat_ship_queries.cpp
        src/game/combat_network.cpp
        src/game/cmbtdrw1_hud.cpp
        src/game/cmbtdrw1_geometry.cpp
        src/game/cmbtmov1_simulation.cpp
        src/game/cmbtmov1_network.cpp
        src/game/cmbtmov1_cost.cpp
    FAKE_SOURCES
        tests/fakes/fake_sdl_string.cpp
)

orion2re_add_unit_test(test_combat_repair
    SOURCES
        tests/unit/combinit_test.cpp
    PRODUCTION_SOURCES
        src/game/combat_lifecycle.cpp
        src/game/combat_layout.cpp
    FAKE_SOURCES
        tests/fakes/fake_combat_repair_runtime.cpp
    LINK_SDL
)

orion2re_add_game_test(test_shield_table
    tests/unit/shield_table_test.cpp
    tests/fakes/fake_shield_table_runtime.cpp
)

orion2re_add_unit_test(test_techdata
    SOURCES
        tests/unit/techdata_test.cpp
    PRODUCTION_SOURCES
        src/game/techdata.cpp
)

orion2re_add_game_test(test_tech_selection
    tests/unit/tech_selection_test.cpp
)

orion2re_add_game_test(test_info
    tests/unit/raceopt_test.cpp
)

orion2re_add_unit_test(test_racesel_digest
    SOURCES
        tests/unit/racesel_test.cpp
    PRODUCTION_SOURCES
        src/game/race_display.cpp
        src/game/raceopt_display.cpp
        src/game/raceopt_config.cpp
    FAKE_SOURCES
        tests/fakes/fake_racesel_digest_runtime.cpp
        tests/fakes/fake_sdl_string.cpp
)

orion2re_add_game_test(test_bill_history
    tests/unit/bill_test.cpp
)

orion2re_add_unit_test(test_bombardment_ammo
    SOURCES
        tests/unit/combat_test.cpp
    PRODUCTION_SOURCES
        src/game/combat_attacks.cpp
        src/game/legacy_sort.cpp
)

orion2re_add_unit_test(test_aidesign
    SOURCES
        tests/unit/aidesign_test.cpp
    PRODUCTION_SOURCES
        src/game/aidesign.cpp
        src/game/aidesign_config.cpp
        src/game/design_specials.cpp
        src/game/aidesign_specials.cpp
    FAKE_SOURCES
        tests/fakes/fake_sdl_string.cpp
)

orion2re_add_unit_test(test_initship
    SOURCES
        tests/unit/initship_test.cpp
    PRODUCTION_SOURCES
        src/game/initship.cpp
        src/game/ship_config.cpp
    FAKE_SOURCES
        tests/fakes/fake_memory.cpp
    LINK_SDL
)

orion2re_add_unit_test(test_town_zero
    SOURCES
        tests/unit/bomb_test.cpp
    PRODUCTION_SOURCES
        src/game/bomb.cpp
    FAKE_SOURCES
        tests/fakes/fake_town_zero_runtime.cpp
)

orion2re_add_unit_test(test_animation_frame
    SOURCES
        tests/unit/animate_test.cpp
    PRODUCTION_SOURCES
        src/game/animation_frames.cpp
)

orion2re_add_unit_test(test_animation_speed
    SOURCES
        tests/unit/timer_test.cpp
    PRODUCTION_SOURCES
        src/game/key.cpp
        src/game/timer.cpp
)

orion2re_add_unit_test(test_absorber_cleanup
    SOURCES
        tests/unit/absorber_cleanup_test.cpp
    PRODUCTION_SOURCES
        src/game/combat_ship_health.cpp
    FAKE_SOURCES
        tests/fakes/fake_absorber_cleanup_runtime.cpp
)

orion2re_add_unit_test(test_boarding
    SOURCES
        tests/unit/boarding_test.cpp
    PRODUCTION_SOURCES
        src/game/boarding.cpp
        src/game/cmbtai_boarding.cpp
        src/game/combat1_boarding.cpp
        src/game/combat_range.cpp
        src/game/cmbtmov1_cost.cpp
        src/game/angle.cpp
        src/game/cmbtfir2.cpp
        src/game/cmbtfir2_boarding.cpp
        src/game/cmbtfir2_targeting.cpp
        src/game/combat_ship_state.cpp
        src/game/combat_ship_queries.cpp
        src/game/combat_network.cpp
        src/game/strbuild.cpp
        src/game/struct.cpp
    FAKE_SOURCES
        tests/fakes/fake_boarding_runtime.cpp
    LINK_SDL
)

orion2re_add_unit_test(test_cmbtfire
    SOURCES
        tests/unit/cmbtfire_test.cpp
    PRODUCTION_SOURCES
        src/game/cmbtfire.cpp
        src/game/struct.cpp
    FAKE_SOURCES
        tests/fakes/fake_lua_game_runtime.cpp
        tests/fakes/fake_stasis_runtime.cpp
)

orion2re_add_unit_test(test_cmbtspec
    SOURCES
        tests/unit/cmbtspec_test.cpp
    PRODUCTION_SOURCES
        src/game/cmbtspec.cpp
        src/game/animation_frames.cpp
        src/game/special.cpp
        src/game/angle.cpp
    FAKE_SOURCES
        tests/fakes/fake_cmbtspec_runtime.cpp
)

if(MSVC)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    if(APPLE)
    else()
    endif()
endif()

orion2re_add_unit_test(test_stasis_area_effects
    SOURCES
        tests/unit/combat1_test.cpp
        tests/unit/cmbtfir2_test.cpp
    PRODUCTION_SOURCES
        src/game/cmbtfir2.cpp
        src/game/cmbtfir2_missile_defense.cpp
        src/game/combat1_missiles.cpp
        src/game/combat_ship_state.cpp
        src/game/combat_ship_queries.cpp
        src/game/random.cpp
    FAKE_SOURCES
        tests/fakes/fake_stasis_area_effects_runtime.cpp
)

foreach(TARGET_NAME IN ITEMS test_combat_stats test_combat_repair test_boarding test_stasis_area_effects)
    target_sources(${TARGET_NAME} PRIVATE tests/fakes/fake_combat_grid_runtime.cpp)
endforeach()

orion2re_add_unit_test(test_techinit
    SOURCES
        tests/unit/techinit_test.cpp
    PRODUCTION_SOURCES
        src/game/techinit.cpp
        src/game/techdata.cpp
        src/game/config_refresh.cpp
        src/game/arena.cpp
    FAKE_SOURCES
        tests/fakes/fake_arena_runtime.cpp
        tests/fakes/fake_techinit_runtime.cpp
        tests/fakes/fake_string_table_loader.cpp
    LINK_SDL
)

orion2re_add_game_test(test_map_scale
    tests/unit/map_scale_test.cpp
)

orion2re_add_unit_test(test_erichelp
    SOURCES
        tests/unit/erichelp_test.cpp
    PRODUCTION_SOURCES
        src/game/erichelp.cpp
)

orion2re_add_unit_test(test_newgame
    SOURCES
        tests/unit/initgame_test.cpp
    PRODUCTION_SOURCES
        src/game/initgame_rules.cpp
        src/game/newgame_options.cpp
        src/game/random.cpp
    FAKE_SOURCES
        tests/fakes/fake_newgame_runtime.cpp
        tests/fakes/fake_sdl_string.cpp
)

orion2re_add_unit_test(test_msg_queue
    SOURCES
        tests/unit/msg_queue_test.cpp
    PRODUCTION_SOURCES
        src/game/msg_queue.cpp
)

orion2re_add_unit_test(test_savegame
    SOURCES
        tests/unit/savegame_test.cpp
    PRODUCTION_SOURCES
        src/config/config_snapshot.cpp
        src/config/config_runtime.cpp
        src/config/generated/config.cpp
        src/game/savegame.cpp
        src/game/save_sections.cpp
        src/lua/lua_game_ship_identity.cpp
        src/game/serialization.cpp
        src/game/arena.cpp
        src/game/aidata_config.cpp
        src/game/colbldg_config.cpp
        src/game/colcalc_config.cpp
        src/game/colmove_config.cpp
        src/game/design_config.cpp
        src/game/ericnet_config.cpp
        src/game/ericnet_config_validation.cpp
        src/game/events_config.cpp
        src/game/invasion_config.cpp
        src/game/info_config.cpp
        src/game/initship_config_validation.cpp
        src/game/mapgen_config.cpp
        src/game/raceopt_config.cpp
        src/game/ship_config.cpp
        src/game/spy_config_validation.cpp
        src/game/spy_bonus.cpp
        src/game/techdata.cpp
    FAKE_SOURCES
        tests/fakes/fake_config_runtime.cpp
        tests/fakes/fake_config_snapshot_loader.cpp
        tests/fakes/fake_lua_save.cpp
        tests/fakes/fake_savegame_runtime.cpp
    LINK_SDL
)

target_compile_definitions(test_savegame PRIVATE
    SAVEGAME_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data"
    TEST_CONFIG_RUNTIME_EXTERNAL_SETTINGS
    TEST_CONFIG_RUNTIME_EXTERNAL_SHIPS
)

orion2re_add_unit_test(test_netturn_host_selection
    SOURCES
        tests/unit/netturn_test.cpp
    PRODUCTION_SOURCES
        src/game/mainscr_network.cpp
        src/game/netturn.cpp
        src/game/netmox.cpp
        src/game/netproto.cpp
        src/game/random.cpp
        src/game/serialization.cpp
        src/game/sha256.cpp
    FAKE_SOURCES
        tests/fakes/fake_allocate.cpp
        tests/fakes/fake_fields_helpers_runtime.cpp
        tests/fakes/fake_lua_network.cpp
        tests/fakes/fake_netturn_runtime.cpp
        tests/fakes/fake_video.cpp
    LINK_SDL
)
target_compile_definitions(test_netturn_host_selection PRIVATE
    TEST_REAL_MAIN_RECEIVE
)

orion2re_add_unit_test(test_netmox_packet_limits
    SOURCES
        tests/unit/netmox_test.cpp
    PRODUCTION_SOURCES
        src/game/netmox.cpp
        src/game/netproto.cpp
        src/game/random.cpp
        src/game/serialization.cpp
        src/game/sha256.cpp
    FAKE_SOURCES
        tests/fakes/fake_allocate.cpp
        tests/fakes/fake_exitm.cpp
        tests/fakes/fake_fields_helpers_runtime.cpp
        tests/fakes/fake_lua_network.cpp
        tests/fakes/fake_netturn_runtime.cpp
        tests/fakes/fake_video.cpp
    LINK_SDL
)

orion2re_add_unit_test(test_video_scan_bounds
    SOURCES
        tests/unit/setvesa_test.cpp
    PRODUCTION_SOURCES
        src/game/setvesa.cpp
    FAKE_SOURCES
        tests/fakes/fake_video.cpp
)

orion2re_add_unit_test(test_hqx
    SOURCES
        tests/unit/hqx_test.cpp
    LIBRARIES
        orion2re_scalers
)

orion2re_add_unit_test(test_vfs
    SOURCES
        tests/unit/vfs_test.cpp
    PRODUCTION_SOURCES
        src/game/compat.cpp
        src/game/vfs.cpp
        src/game/vfs_file.cpp
        src/game/vfs_lbx.cpp
        src/game/vfs_path.cpp
        src/game/vfs_zip.cpp
    LIBRARIES
        miniz
    LINK_SDL
)

orion2re_add_unit_test(test_smack
    SOURCES
        tests/unit/smack_test.cpp
    PRODUCTION_SOURCES
        src/game/smack.cpp
)

orion2re_add_unit_test(test_smckplay
    SOURCES
        tests/unit/smckplay_test.cpp
    PRODUCTION_SOURCES
        src/game/smckplay.cpp
        src/game/setvesa.cpp
    FAKE_SOURCES
        tests/fakes/fake_smckplay_runtime.cpp
        tests/fakes/fake_video.cpp
    LINK_SDL
)

orion2re_add_game_test(test_sound
    tests/unit/sound_test.cpp
)

orion2re_add_unit_test(test_farload_buffer_vfs
    SOURCES
        tests/unit/farload_test.cpp
    PRODUCTION_SOURCES
        src/game/buffer.cpp
        src/game/compat.cpp
        src/game/farload.cpp
        src/game/string_overrides.cpp
        src/config/text_encoding.cpp
        vendors/yyjson/src/yyjson.c
        src/game/vfs.cpp
        src/game/vfs_file.cpp
        src/game/vfs_lbx.cpp
        src/game/vfs_path.cpp
        src/game/vfs_zip.cpp
    FAKE_SOURCES
        tests/fakes/fake_allocate.cpp
        tests/fakes/fake_exitm.cpp
        tests/fakes/fake_fields_helpers_runtime.cpp
        tests/fakes/fake_memory.cpp
        tests/fakes/fake_sound.cpp
        tests/fakes/fake_test_state.cpp
        tests/fakes/fake_video.cpp
    LIBRARIES
        miniz
    LINK_SDL
)

target_include_directories(test_farload_buffer_vfs PRIVATE "${ORION2RE_VENDORED_YYJSON_DIR}/src")
target_compile_definitions(test_farload_buffer_vfs PRIVATE
    STRING_OVERRIDE_BASE_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/base"
)

orion2re_add_unit_test(test_button_art
    SOURCES
        tests/unit/button_art_test.cpp
    PRODUCTION_SOURCES
        src/game/buffer.cpp
        src/game/button_art.cpp
        src/game/compat.cpp
        src/game/farload.cpp
        src/game/string_overrides.cpp
        src/config/text_encoding.cpp
        vendors/yyjson/src/yyjson.c
        src/game/vfs.cpp
        src/game/vfs_file.cpp
        src/game/vfs_lbx.cpp
        src/game/vfs_path.cpp
        src/game/vfs_zip.cpp
    FAKE_SOURCES
        tests/fakes/fake_allocate.cpp
        tests/fakes/fake_exitm.cpp
        tests/fakes/fake_fields_helpers_runtime.cpp
        tests/fakes/fake_memory.cpp
        tests/fakes/fake_sound.cpp
        tests/fakes/fake_test_state.cpp
        tests/fakes/fake_video.cpp
    LIBRARIES
        miniz
    LINK_SDL
)

target_include_directories(test_button_art PRIVATE "${ORION2RE_VENDORED_YYJSON_DIR}/src")
target_compile_definitions(test_button_art PRIVATE
    BUTTON_ART_BASE_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/base"
)

orion2re_add_unit_test(test_sprite_art
    SOURCES
        tests/unit/sprite_art_test.cpp
    PRODUCTION_SOURCES
        src/game/buffer.cpp
        src/game/button_art.cpp
        src/game/compat.cpp
        src/game/farload.cpp
        src/game/sprite_art.cpp
        src/game/string_overrides.cpp
        src/config/text_encoding.cpp
        vendors/yyjson/src/yyjson.c
        src/game/vfs.cpp
        src/game/vfs_file.cpp
        src/game/vfs_lbx.cpp
        src/game/vfs_path.cpp
        src/game/vfs_zip.cpp
    FAKE_SOURCES
        tests/fakes/fake_allocate.cpp
        tests/fakes/fake_animate.cpp
        tests/fakes/fake_exitm.cpp
        tests/fakes/fake_memory.cpp
        tests/fakes/fake_platform_exit.cpp
        tests/fakes/fake_sound.cpp
        tests/fakes/fake_test_state.cpp
        tests/fakes/fake_video.cpp
    LIBRARIES
        miniz
    LINK_SDL
)

target_include_directories(test_sprite_art PRIVATE "${ORION2RE_VENDORED_YYJSON_DIR}/src")
target_compile_definitions(test_sprite_art PRIVATE
    SPRITE_ART_BASE_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/base"
)

foreach(TARGET_NAME IN ITEMS test_newgame
    test_lua_mod_runtime)
    target_sources(${TARGET_NAME} PRIVATE tests/fakes/fake_string_overrides.cpp)
endforeach()

orion2re_add_unit_test(test_fields
    SOURCES
        tests/unit/fields_test.cpp
    PRODUCTION_SOURCES
        src/game/exitm.cpp
        src/game/fields.cpp
        src/game/mouse_wheel.cpp
    FAKE_SOURCES
        tests/fakes/fake_animate.cpp
        tests/fakes/fake_fields_runtime.cpp
        tests/fakes/fake_input.cpp
        tests/fakes/fake_memory.cpp
        tests/fakes/fake_mmio.cpp
        tests/fakes/fake_sound.cpp
        tests/fakes/fake_textbox.cpp
        tests/fakes/fake_video.cpp
    LINK_SDL
)

orion2re_add_unit_test(test_mouse_wheel
    SOURCES
        tests/unit/mouse_wheel_test.cpp
    PRODUCTION_SOURCES
        src/game/mouse_wheel.cpp
)

orion2re_add_unit_test(test_router_protocol
    SOURCES
        tests/unit/router_protocol_test.cpp
    PRODUCTION_SOURCES
        src/network/router_protocol.cpp
    LIBRARIES
        orion2re_lz4
)

orion2re_add_unit_test(test_router_core
    SOURCES
        tests/unit/router_core_test.cpp
    PRODUCTION_SOURCES
        src/network/router_core.cpp
        src/network/router_protocol.cpp
    LIBRARIES
        orion2re_lz4
)

orion2re_add_unit_test(test_router_service
    SOURCES
        tests/unit/router_service_test.cpp
    PRODUCTION_SOURCES
        src/network/router_core.cpp
        src/network/router_protocol.cpp
        src/network/router_service.cpp
    LIBRARIES
        uv_a
        orion2re_lz4
    LINK_SDL
)

orion2re_add_unit_test(test_router_session_browser
    SOURCES
        tests/unit/router_session_browser_test.cpp
    PRODUCTION_SOURCES
        src/network/router_client.cpp
        src/network/router_core.cpp
        src/network/router_protocol.cpp
        src/network/router_session_browser.cpp
        src/network/router_service.cpp
    LIBRARIES
        uv_a
        orion2re_lz4
    LINK_SDL
)

orion2re_add_unit_test(test_netadapter
    SOURCES
        tests/unit/netadapter_test.cpp
    PRODUCTION_SOURCES
        src/game/netadapter.cpp
        src/game/netcode_bootstrap.cpp
        src/network/router_client.cpp
        src/network/router_core.cpp
        src/network/router_protocol.cpp
        src/network/router_session_browser.cpp
        src/network/router_service.cpp
    LIBRARIES
        uv_a
        orion2re_lz4
    LINK_SDL
)

orion2re_add_unit_test(test_router_client
    SOURCES
        tests/unit/router_client_test.cpp
    PRODUCTION_SOURCES
        src/network/router_client.cpp
        src/network/router_core.cpp
        src/network/router_protocol.cpp
        src/network/router_service.cpp
    LIBRARIES
        uv_a
        orion2re_lz4
    LINK_SDL
)