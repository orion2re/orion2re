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

add_library(orion2re_test_config_defaults STATIC
    ${ORION2RE_DEFAULT_CONFIG_SOURCES}
)
target_compile_features(orion2re_test_config_defaults PRIVATE cxx_std_20)
target_include_directories(orion2re_test_config_defaults PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(orion2re_test_config_defaults PRIVATE SDL3::Headers)

function(orion2re_add_unit_test TARGET_NAME)
    cmake_parse_arguments(ORION2RE_TEST "LINK_SDL" "" "SOURCES;PRODUCTION_SOURCES;FAKE_SOURCES;LIBRARIES" ${ARGN})

    add_executable(${TARGET_NAME}
        tests/test_main.cpp
        ${ORION2RE_TEST_SOURCES}
        ${ORION2RE_TEST_PRODUCTION_SOURCES}
        ${ORION2RE_TEST_FAKE_SOURCES}
        src/pch.cpp
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

orion2re_add_unit_test(test_fields_helpers
    SOURCES
        tests/unit/fields_helpers_test.cpp
    FAKE_SOURCES
        tests/fakes/fake_exitm.cpp
        tests/fakes/fake_fields_helpers_runtime.cpp
        tests/fakes/fake_video.cpp
)

orion2re_add_unit_test(test_multplay
    SOURCES
        tests/unit/multplay_test.cpp
    PRODUCTION_SOURCES
        src/game/multplay.cpp
)

orion2re_add_unit_test(test_netstart_pick_position
    SOURCES
        tests/unit/netstart_test.cpp
    PRODUCTION_SOURCES
        src/game/netstart.cpp
    FAKE_SOURCES
        tests/fakes/fake_netstart_runtime.cpp
    LINK_SDL
)

if(MSVC)
    target_compile_options(test_netstart_pick_position PRIVATE /Gy)
    target_link_options(test_netstart_pick_position PRIVATE /OPT:REF)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(test_netstart_pick_position PRIVATE
        -ffunction-sections -fdata-sections)
    if(APPLE)
        target_link_options(test_netstart_pick_position PRIVATE -Wl,-dead_strip)
    else()
        target_link_options(test_netstart_pick_position PRIVATE -Wl,--gc-sections)
    endif()
endif()

if(MSVC)
    target_compile_options(test_multplay PRIVATE /Gy)
    target_link_options(test_multplay PRIVATE /OPT:REF)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(test_multplay PRIVATE -ffunction-sections -fdata-sections)
    if(APPLE)
        target_link_options(test_multplay PRIVATE -Wl,-dead_strip)
    else()
        target_link_options(test_multplay PRIVATE -Wl,--gc-sections)
    endif()
endif()

orion2re_add_unit_test(test_mainscr_reload
    SOURCES
        tests/unit/mainscr_test.cpp
    PRODUCTION_SOURCES
        src/game/mainscr.cpp
        src/game/mainscr_main.cpp
    FAKE_SOURCES
        tests/fakes/fake_main_screen_hud_runtime.cpp
    LINK_SDL
)

if(MSVC)
    target_compile_options(test_mainscr_reload PRIVATE /Gy)
    target_link_options(test_mainscr_reload PRIVATE /OPT:REF)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(test_mainscr_reload PRIVATE -ffunction-sections -fdata-sections)
    if(APPLE)
        target_link_options(test_mainscr_reload PRIVATE -Wl,-dead_strip)
    else()
        target_link_options(test_mainscr_reload PRIVATE -Wl,--gc-sections)
    endif()
endif()

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

orion2re_add_unit_test(test_resource_reload
    SOURCES tests/unit/resource_reload_test.cpp
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
target_include_directories(test_resource_reload PRIVATE
    vendors/libsmacker vendors/yyjson/src
)
if(TARGET orion2re_vulkan_scalers)
    target_link_libraries(test_resource_reload PRIVATE orion2re_vulkan_scalers)
endif()

if(MSVC)
    target_compile_options(test_allocate PRIVATE /Gy)
    target_link_options(test_allocate PRIVATE /OPT:REF)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(test_allocate PRIVATE -ffunction-sections -fdata-sections)
    if(APPLE)
        target_link_options(test_allocate PRIVATE -Wl,-dead_strip)
    else()
        target_link_options(test_allocate PRIVATE -Wl,--gc-sections)
    endif()
endif()

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
        src/game/mapgen.cpp
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
    LIBRARIES
        miniz
        orion2re_lua
    LINK_SDL
)
target_compile_definitions(test_lua_mod_runtime PRIVATE
    LUA_MIGRATION_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/lua"
    LUA_SOURCE_PACKAGE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/mods"
)
if(MSVC)
    target_compile_options(test_lua_mod_runtime PRIVATE /Gy)
    target_link_options(test_lua_mod_runtime PRIVATE /OPT:REF)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(test_lua_mod_runtime PRIVATE
        -ffunction-sections -fdata-sections)
    if(APPLE)
        target_link_options(test_lua_mod_runtime PRIVATE -Wl,-dead_strip)
    else()
        target_link_options(test_lua_mod_runtime PRIVATE -Wl,--gc-sections)
    endif()
endif()

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

orion2re_add_unit_test(test_config_bindings
    SOURCES
        tests/unit/config_bindings_test.cpp
        tests/unit/main_screen_config_test.cpp
    PRODUCTION_SOURCES
        src/config/cfg_parser.cpp
        src/config/config_decode.cpp
        src/config/text_encoding.cpp
        src/config/config_runtime.cpp
        src/config/generated/config.cpp
        src/game/arena.cpp
        src/game/aidesign_config_validation.cpp
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
        src/game/ken.cpp
        src/game/mapgen_config.cpp
        src/game/maintain_config.cpp
        src/game/newtech_config.cpp
        src/game/raceopt_config.cpp
        src/game/random.cpp
        src/game/ship_config.cpp
        src/game/spy_config_validation.cpp
        src/game/spy_bonus.cpp
        src/game/techdata.cpp
    FAKE_SOURCES
        tests/fakes/fake_arena_runtime.cpp
        tests/fakes/fake_config_runtime.cpp
    LINK_SDL
)
target_compile_definitions(test_config_bindings PRIVATE
    TEST_CONFIG_RUNTIME_EXTERNAL_KEN
)
if(MSVC)
    target_compile_options(test_config_bindings PRIVATE /Gy)
    target_link_options(test_config_bindings PRIVATE /OPT:REF)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(test_config_bindings PRIVATE -ffunction-sections -fdata-sections)
    if(APPLE)
        target_link_options(test_config_bindings PRIVATE -Wl,-dead_strip)
    else()
        target_link_options(test_config_bindings PRIVATE -Wl,--gc-sections)
    endif()
endif()

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
        src/game/spy.cpp
        src/game/spy_bonus.cpp
    FAKE_SOURCES
        tests/fakes/fake_spy_runtime.cpp
)

if(MSVC)
    target_compile_options(test_spy PRIVATE /Gy)
    target_link_options(test_spy PRIVATE /OPT:REF)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(test_spy PRIVATE -ffunction-sections -fdata-sections)
    if(APPLE)
        target_link_options(test_spy PRIVATE -Wl,-dead_strip)
    else()
        target_link_options(test_spy PRIVATE -Wl,--gc-sections)
    endif()
endif()

orion2re_add_unit_test(test_aidudes
    SOURCES
        tests/unit/aidudes_test.cpp
    PRODUCTION_SOURCES
        src/game/aileader.cpp
        src/game/aidudes.cpp
        src/game/maintain.cpp
    FAKE_SOURCES
        tests/fakes/fake_aidudes_runtime.cpp
)

orion2re_add_unit_test(test_antarans
    SOURCES
        tests/unit/antarans_test.cpp
    PRODUCTION_SOURCES
        src/game/antarans.cpp
        src/game/random.cpp
    FAKE_SOURCES
        tests/fakes/fake_antarans_runtime.cpp
    LINK_SDL
)

orion2re_add_unit_test(test_ai_hypertech
    SOURCES
        tests/unit/aitech_test.cpp
    PRODUCTION_SOURCES
        src/game/aitech.cpp
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

if(MSVC)
    target_compile_options(test_antarans PRIVATE /Gy)
    target_link_options(test_antarans PRIVATE /OPT:REF)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(test_antarans PRIVATE -ffunction-sections -fdata-sections)
    if(APPLE)
        target_link_options(test_antarans PRIVATE -Wl,-dead_strip)
    else()
        target_link_options(test_antarans PRIVATE -Wl,--gc-sections)
    endif()
endif()

orion2re_add_unit_test(test_refit_cost
    SOURCES
        tests/unit/refit_cost_test.cpp
    PRODUCTION_SOURCES
        src/game/aibuild.cpp
        src/game/production_rules.cpp
        src/game/colcalc_config.cpp
        src/game/design.cpp
        src/game/design_config.cpp
        src/game/special.cpp
    FAKE_SOURCES
        tests/fakes/fake_refit_cost_runtime.cpp
        tests/fakes/fake_sdl_string.cpp
)

orion2re_add_unit_test(test_production_rules
    SOURCES
        tests/unit/production_rules_test.cpp
    PRODUCTION_SOURCES
        src/game/production_rules.cpp
)

orion2re_add_unit_test(test_design_weapon_damage
    SOURCES
        tests/unit/design_test.cpp
    PRODUCTION_SOURCES
        src/game/design.cpp
        src/game/design_config.cpp
    FAKE_SOURCES
        tests/fakes/fake_design_weapon_damage_runtime.cpp
        tests/fakes/fake_sdl_string.cpp
)

if(MSVC)
    target_compile_options(test_design_weapon_damage PRIVATE /Gy)
    target_link_options(test_design_weapon_damage PRIVATE /OPT:REF)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(test_design_weapon_damage PRIVATE -ffunction-sections -fdata-sections)
    if(APPLE)
        target_link_options(test_design_weapon_damage PRIVATE -Wl,-dead_strip)
    else()
        target_link_options(test_design_weapon_damage PRIVATE -Wl,--gc-sections)
    endif()
endif()

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

if(MSVC)
    target_compile_options(test_colmove PRIVATE /Gy)
    target_link_options(test_colmove PRIVATE /OPT:REF)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(test_colmove PRIVATE -ffunction-sections -fdata-sections)
    if(APPLE)
        target_link_options(test_colmove PRIVATE -Wl,-dead_strip)
    else()
        target_link_options(test_colmove PRIVATE -Wl,--gc-sections)
    endif()
endif()

orion2re_add_unit_test(test_colony_production_helpers
    SOURCES
        tests/unit/colcalc_base_test.cpp
    PRODUCTION_SOURCES
        src/game/colcalc_base.cpp
        src/game/colcalc_config.cpp
        src/game/plntsum.cpp
    FAKE_SOURCES
        tests/fakes/fake_colcalc_base_runtime.cpp
)

orion2re_add_unit_test(test_colony_production_draw
    SOURCES
        tests/unit/coldraw_test.cpp
    PRODUCTION_SOURCES
        src/game/coldraw.cpp
    FAKE_SOURCES
        tests/fakes/fake_coldraw_runtime.cpp
)

orion2re_add_unit_test(test_colony_scan_info
    SOURCES
        tests/unit/colsum_test.cpp
    PRODUCTION_SOURCES
        src/game/colsum.cpp
    FAKE_SOURCES
        tests/fakes/fake_colony_scan_info_runtime.cpp
    LINK_SDL
)

orion2re_add_unit_test(test_colony_pop_growth
    SOURCES
        tests/unit/colcalc_test.cpp
    PRODUCTION_SOURCES
        src/game/aidata.cpp
        src/game/ai_state.cpp
        src/game/aidata_config.cpp
        src/game/colcalc.cpp
        src/game/colcalc_config.cpp
        src/game/raceopt_config.cpp
        src/game/settler.cpp
    FAKE_SOURCES
        tests/fakes/fake_colony_pop_growth_runtime.cpp
)

orion2re_add_unit_test(test_colony_replicators
    SOURCES
        tests/unit/colcalc_main_test.cpp
    PRODUCTION_SOURCES
        src/game/colcalc.cpp
        src/game/colcalc_base.cpp
        src/game/colcalc_config.cpp
        src/game/colcalc_main.cpp
        src/game/strbuild.cpp
    FAKE_SOURCES
        tests/fakes/fake_colony_replicators_runtime.cpp
)

orion2re_add_unit_test(test_build_queue
    SOURCES
        tests/unit/build_queue_test.cpp
    PRODUCTION_SOURCES
        src/game/build_queue.cpp
    FAKE_SOURCES
        tests/fakes/fake_build_queue_runtime.cpp
)

orion2re_add_unit_test(test_colbldg
    SOURCES
        tests/unit/colbldg_test.cpp
    PRODUCTION_SOURCES
        src/game/colbldg.cpp
        src/game/colbldg_config.cpp
        src/lua/lua_game_ship_identity.cpp
    FAKE_SOURCES
        tests/fakes/fake_colbldg_runtime.cpp
        tests/fakes/fake_sdl_string.cpp
)

if(MSVC)
    target_compile_options(test_colbldg PRIVATE /Gy)
    target_link_options(test_colbldg PRIVATE /OPT:REF)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(test_colbldg PRIVATE -ffunction-sections -fdata-sections)
    if(APPLE)
        target_link_options(test_colbldg PRIVATE -Wl,-dead_strip)
    else()
        target_link_options(test_colbldg PRIVATE -Wl,--gc-sections)
    endif()
endif()

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
    FAKE_SOURCES
        tests/fakes/fake_report_runtime.cpp
)

if(MSVC)
    target_compile_options(test_report PRIVATE /Gy)
    target_link_options(test_report PRIVATE /OPT:REF)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(test_report PRIVATE -ffunction-sections -fdata-sections)
    if(APPLE)
        target_link_options(test_report PRIVATE -Wl,-dead_strip)
    else()
        target_link_options(test_report PRIVATE -Wl,--gc-sections)
    endif()
endif()

orion2re_add_unit_test(test_events
    SOURCES
        tests/unit/events_test.cpp
    PRODUCTION_SOURCES
        src/game/events.cpp
    FAKE_SOURCES
        tests/fakes/fake_events_runtime.cpp
        tests/fakes/fake_sdl_string.cpp
)

orion2re_add_unit_test(test_combfind_rules
    SOURCES
        tests/unit/combfind_rules_test.cpp
    PRODUCTION_SOURCES
        src/game/combfind.cpp
        src/game/combfind_rules.cpp
)

if(MSVC)
    target_compile_options(test_combfind_rules PRIVATE /Gy)
    target_link_options(test_combfind_rules PRIVATE /OPT:REF)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(test_combfind_rules PRIVATE -ffunction-sections -fdata-sections)
    if(APPLE)
        target_link_options(test_combfind_rules PRIVATE -Wl,-dead_strip)
    else()
        target_link_options(test_combfind_rules PRIVATE -Wl,--gc-sections)
    endif()
endif()

orion2re_add_unit_test(test_mainpups_leader_technology
    SOURCES
        tests/unit/mainpups_test.cpp
    PRODUCTION_SOURCES
        src/game/mainpups.cpp
    FAKE_SOURCES
        tests/fakes/fake_mainpups_leader_technology_runtime.cpp
        tests/fakes/fake_sdl_string.cpp
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

if(MSVC)
    target_compile_options(test_events PRIVATE /Gy)
    target_link_options(test_events PRIVATE /OPT:REF)
    target_compile_options(test_mainpups_leader_technology PRIVATE /Gy)
    target_link_options(test_mainpups_leader_technology PRIVATE /OPT:REF)
    target_compile_options(test_nextturn_order PRIVATE /Gy)
    target_link_options(test_nextturn_order PRIVATE /OPT:REF)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(test_events PRIVATE -ffunction-sections -fdata-sections)
    target_compile_options(test_mainpups_leader_technology PRIVATE -ffunction-sections -fdata-sections)
    target_compile_options(test_nextturn_order PRIVATE -ffunction-sections -fdata-sections)
    if(APPLE)
        target_link_options(test_events PRIVATE -Wl,-dead_strip)
        target_link_options(test_mainpups_leader_technology PRIVATE -Wl,-dead_strip)
        target_link_options(test_nextturn_order PRIVATE -Wl,-dead_strip)
    else()
        target_link_options(test_events PRIVATE -Wl,--gc-sections)
        target_link_options(test_mainpups_leader_technology PRIVATE -Wl,--gc-sections)
        target_link_options(test_nextturn_order PRIVATE -Wl,--gc-sections)
    endif()
endif()

orion2re_add_unit_test(test_shipmove
    SOURCES
        tests/unit/shipmove_test.cpp
    PRODUCTION_SOURCES
        src/game/shipmove.cpp
        src/game/ships.cpp
        src/lua/lua_game_ship_identity.cpp
    FAKE_SOURCES
        tests/fakes/fake_shipmove_runtime.cpp
)

orion2re_add_unit_test(test_aimove
    SOURCES
        tests/unit/aimove_test.cpp
    PRODUCTION_SOURCES
        src/game/aimove.cpp
    FAKE_SOURCES
        tests/fakes/fake_aimove_runtime.cpp
)

if(MSVC)
    target_compile_options(test_shipmove PRIVATE /Gy)
    target_link_options(test_shipmove PRIVATE /OPT:REF)
    target_compile_options(test_aimove PRIVATE /Gy)
    target_link_options(test_aimove PRIVATE /OPT:REF)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(test_shipmove PRIVATE -ffunction-sections -fdata-sections)
    target_compile_options(test_aimove PRIVATE -ffunction-sections -fdata-sections)
    if(APPLE)
        target_link_options(test_shipmove PRIVATE -Wl,-dead_strip)
        target_link_options(test_aimove PRIVATE -Wl,-dead_strip)
    else()
        target_link_options(test_shipmove PRIVATE -Wl,--gc-sections)
        target_link_options(test_aimove PRIVATE -Wl,--gc-sections)
    endif()
endif()

orion2re_add_unit_test(test_fleetpop
    SOURCES
        tests/unit/fleetpop_test.cpp
    PRODUCTION_SOURCES
        src/game/flt1.cpp
        src/game/fleetpop.cpp
        src/game/shipstak.cpp
        src/game/sys.cpp
    FAKE_SOURCES
        tests/fakes/fake_fleetpop_runtime.cpp
)

if(MSVC)
    target_compile_options(test_fleetpop PRIVATE /Gy)
    target_link_options(test_fleetpop PRIVATE /OPT:REF)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(test_fleetpop PRIVATE -ffunction-sections -fdata-sections)
    if(APPLE)
        target_link_options(test_fleetpop PRIVATE -Wl,-dead_strip)
    else()
        target_link_options(test_fleetpop PRIVATE -Wl,--gc-sections)
    endif()
endif()

orion2re_add_unit_test(test_fleet_summary
    SOURCES
        tests/unit/flt1_test.cpp
    PRODUCTION_SOURCES
        src/game/flt1.cpp
    FAKE_SOURCES
        tests/fakes/fake_fleet_summary_runtime.cpp
        tests/fakes/fake_sdl_string.cpp
)

if(MSVC)
    target_compile_options(test_fleet_summary PRIVATE /Gy)
    target_link_options(test_fleet_summary PRIVATE /OPT:REF)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(test_fleet_summary PRIVATE -ffunction-sections -fdata-sections)
    if(APPLE)
        target_link_options(test_fleet_summary PRIVATE -Wl,-dead_strip)
    else()
        target_link_options(test_fleet_summary PRIVATE -Wl,--gc-sections)
    endif()
endif()

orion2re_add_unit_test(test_geo
    SOURCES
        tests/unit/geo_test.cpp
    PRODUCTION_SOURCES
        src/game/geo.cpp
    FAKE_SOURCES
        tests/fakes/fake_geo_runtime.cpp
)

orion2re_add_unit_test(test_homegen
    SOURCES
        tests/unit/homegen_test.cpp
    PRODUCTION_SOURCES
        src/game/homegen.cpp
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

if(MSVC)
    target_compile_options(test_geo PRIVATE /Gy)
    target_link_options(test_geo PRIVATE /OPT:REF)
    target_compile_options(test_homegen PRIVATE /Gy)
    target_link_options(test_homegen PRIVATE /OPT:REF)
    target_compile_options(test_mapgen_planet_max PRIVATE /Gy)
    target_link_options(test_mapgen_planet_max PRIVATE /OPT:REF)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(test_geo PRIVATE -ffunction-sections -fdata-sections)
    target_compile_options(test_homegen PRIVATE -ffunction-sections -fdata-sections)
    target_compile_options(test_mapgen_planet_max PRIVATE -ffunction-sections -fdata-sections)
    if(APPLE)
        target_link_options(test_geo PRIVATE -Wl,-dead_strip)
        target_link_options(test_homegen PRIVATE -Wl,-dead_strip)
        target_link_options(test_mapgen_planet_max PRIVATE -Wl,-dead_strip)
    else()
        target_link_options(test_geo PRIVATE -Wl,--gc-sections)
        target_link_options(test_homegen PRIVATE -Wl,--gc-sections)
        target_link_options(test_mapgen_planet_max PRIVATE -Wl,--gc-sections)
    endif()
endif()

orion2re_add_unit_test(test_ground_commando
    SOURCES
        tests/unit/invasion_test.cpp
    PRODUCTION_SOURCES
        src/game/colony.cpp
        src/game/invasion.cpp
        src/game/invasion_config.cpp
        src/game/officer.cpp
        src/lua/lua_game_ship_identity.cpp
    FAKE_SOURCES
        tests/fakes/fake_ground_commando_runtime.cpp
)

orion2re_add_unit_test(test_eric
    SOURCES
        tests/unit/eric_test.cpp
    PRODUCTION_SOURCES
        src/game/eric.cpp
    FAKE_SOURCES
        tests/fakes/fake_eric_runtime.cpp
)

if(MSVC)
    target_compile_options(test_eric PRIVATE /Gy)
    target_link_options(test_eric PRIVATE /OPT:REF)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(test_eric PRIVATE -ffunction-sections -fdata-sections)
    if(APPLE)
        target_link_options(test_eric PRIVATE -Wl,-dead_strip)
    else()
        target_link_options(test_eric PRIVATE -Wl,--gc-sections)
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
        src/game/cmbtmis.cpp
    FAKE_SOURCES
        tests/fakes/fake_exitm.cpp
        tests/fakes/fake_fields_helpers_runtime.cpp
)

orion2re_add_unit_test(test_diplomac
    SOURCES
        tests/unit/diplomac_test.cpp
    PRODUCTION_SOURCES
        src/game/diplomac.cpp
    FAKE_SOURCES
        tests/fakes/fake_diplomac_runtime.cpp
)

if(MSVC)
    target_compile_options(test_diplomac PRIVATE /Gy)
    target_link_options(test_diplomac PRIVATE /OPT:REF)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(test_diplomac PRIVATE -ffunction-sections -fdata-sections)
    if(APPLE)
        target_link_options(test_diplomac PRIVATE -Wl,-dead_strip)
    else()
        target_link_options(test_diplomac PRIVATE -Wl,--gc-sections)
    endif()
endif()

orion2re_add_unit_test(test_npcdiplo
    SOURCES
        tests/unit/npcdiplo_test.cpp
    PRODUCTION_SOURCES
        src/game/npcdiplo.cpp
    FAKE_SOURCES
        tests/fakes/fake_npcdiplo_runtime.cpp
)

if(MSVC)
    target_compile_options(test_npcdiplo PRIVATE /Gy)
    target_link_options(test_npcdiplo PRIVATE /OPT:REF)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(test_npcdiplo PRIVATE
        -ffunction-sections -fdata-sections)
    if(APPLE)
        target_link_options(test_npcdiplo PRIVATE -Wl,-dead_strip)
    else()
        target_link_options(test_npcdiplo PRIVATE -Wl,--gc-sections)
    endif()
endif()

orion2re_add_unit_test(test_combat_stats
    SOURCES
        tests/unit/combat_stats_test.cpp
    PRODUCTION_SOURCES
        src/game/aipower.cpp
        src/game/cmbtai.cpp
        src/game/combat1.cpp
        src/game/cmbtdrw1.cpp
        src/game/cmbtmov1.cpp
    FAKE_SOURCES
        tests/fakes/fake_sdl_string.cpp
)

orion2re_add_unit_test(test_combat_repair
    SOURCES
        tests/unit/combinit_test.cpp
    PRODUCTION_SOURCES
        src/game/combinit.cpp
    FAKE_SOURCES
        tests/fakes/fake_combat_repair_runtime.cpp
    LINK_SDL
)

orion2re_add_unit_test(test_shield_table
    SOURCES
        tests/unit/shield_table_test.cpp
    PRODUCTION_SOURCES
        src/game/aipower.cpp
        src/game/combinit.cpp
        src/game/design.cpp
        src/game/design_config.cpp
        src/game/ship_config.cpp
        src/game/techdata.cpp
    FAKE_SOURCES
        tests/fakes/fake_shield_table_runtime.cpp
        tests/fakes/fake_sdl_string.cpp
)

orion2re_add_unit_test(test_techdata
    SOURCES
        tests/unit/techdata_test.cpp
    PRODUCTION_SOURCES
        src/game/techdata.cpp
)

orion2re_add_unit_test(test_tech_selection
    SOURCES
        tests/unit/tech_selection_test.cpp
    PRODUCTION_SOURCES
        src/game/tech_selection.cpp
)

orion2re_add_unit_test(test_info
    SOURCES
        tests/unit/raceopt_test.cpp
    PRODUCTION_SOURCES
        src/game/info_config.cpp
        src/game/raceopt.cpp
)

orion2re_add_unit_test(test_racesel_digest
    SOURCES
        tests/unit/racesel_test.cpp
    PRODUCTION_SOURCES
        src/game/raceopt.cpp
        src/game/raceopt_config.cpp
        src/game/racesel.cpp
    FAKE_SOURCES
        tests/fakes/fake_racesel_digest_runtime.cpp
        tests/fakes/fake_sdl_string.cpp
)

if(MSVC)
    target_compile_options(test_racesel_digest PRIVATE /Gy /Gw)
    target_link_options(test_racesel_digest PRIVATE /OPT:REF)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(test_racesel_digest PRIVATE -ffunction-sections -fdata-sections)
    if(APPLE)
        target_link_options(test_racesel_digest PRIVATE -Wl,-dead_strip)
    else()
        target_link_options(test_racesel_digest PRIVATE -Wl,--gc-sections)
    endif()
endif()

orion2re_add_unit_test(test_bill_history
    SOURCES
        tests/unit/bill_test.cpp
    PRODUCTION_SOURCES
        src/game/bill.cpp
    FAKE_SOURCES
        tests/fakes/fake_bill_history_runtime.cpp
)

if(MSVC)
    target_compile_options(test_bill_history PRIVATE /Gy /Gw)
    target_link_options(test_bill_history PRIVATE /OPT:REF)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(test_bill_history PRIVATE
        -ffunction-sections -fdata-sections)
    if(APPLE)
        target_link_options(test_bill_history PRIVATE -Wl,-dead_strip)
    else()
        target_link_options(test_bill_history PRIVATE -Wl,--gc-sections)
    endif()
endif()

if(MSVC)
    target_compile_options(test_info PRIVATE /Gy /Gw)
    target_link_options(test_info PRIVATE /OPT:REF)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(test_info PRIVATE -ffunction-sections -fdata-sections)
    if(APPLE)
        target_link_options(test_info PRIVATE -Wl,-dead_strip)
    else()
        target_link_options(test_info PRIVATE -Wl,--gc-sections)
    endif()
endif()

orion2re_add_unit_test(test_bombardment_ammo
    SOURCES
        tests/unit/combat_test.cpp
    PRODUCTION_SOURCES
        src/game/combat.cpp
)

orion2re_add_unit_test(test_aidesign
    SOURCES
        tests/unit/aidesign_test.cpp
    PRODUCTION_SOURCES
        src/game/aidesign.cpp
        src/game/aidesign_config.cpp
        src/game/desbox.cpp
    FAKE_SOURCES
        tests/fakes/fake_sdl_string.cpp
)

if(MSVC)
    target_compile_options(test_aidesign PRIVATE /Gy)
    target_link_options(test_aidesign PRIVATE /OPT:REF)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(test_aidesign PRIVATE
        -ffunction-sections -fdata-sections)
    if(APPLE)
        target_link_options(test_aidesign PRIVATE -Wl,-dead_strip)
    else()
        target_link_options(test_aidesign PRIVATE -Wl,--gc-sections)
    endif()
endif()

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

if(MSVC)
    target_compile_options(test_initship PRIVATE /Gy)
    target_link_options(test_initship PRIVATE /OPT:REF)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(test_initship PRIVATE
        -ffunction-sections -fdata-sections)
    if(APPLE)
        target_link_options(test_initship PRIVATE -Wl,-dead_strip)
    else()
        target_link_options(test_initship PRIVATE -Wl,--gc-sections)
    endif()
endif()

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
        src/game/animate.cpp
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
        src/game/cmbtfire.cpp
        src/game/cmbtfir2.cpp
    FAKE_SOURCES
        tests/fakes/fake_absorber_cleanup_runtime.cpp
        tests/fakes/fake_lua_game_runtime.cpp
)

orion2re_add_unit_test(test_boarding
    SOURCES
        tests/unit/boarding_test.cpp
    PRODUCTION_SOURCES
        src/game/boarding.cpp
        src/game/cmbtai.cpp
        src/game/cmbtfir2.cpp
        src/game/combat1.cpp
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
    FAKE_SOURCES
        tests/fakes/fake_lua_game_runtime.cpp
        tests/fakes/fake_stasis_runtime.cpp
)

orion2re_add_unit_test(test_cmbtspec
    SOURCES
        tests/unit/cmbtspec_test.cpp
    PRODUCTION_SOURCES
        src/game/cmbtspec.cpp
    FAKE_SOURCES
        tests/fakes/fake_cmbtspec_runtime.cpp
)

if(MSVC)
    target_compile_options(test_cmbtspec PRIVATE /Gy)
    target_link_options(test_cmbtspec PRIVATE /OPT:REF)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(test_cmbtspec PRIVATE -ffunction-sections -fdata-sections)
    if(APPLE)
        target_link_options(test_cmbtspec PRIVATE -Wl,-dead_strip)
    else()
        target_link_options(test_cmbtspec PRIVATE -Wl,--gc-sections)
    endif()
endif()

orion2re_add_unit_test(test_stasis_area_effects
    SOURCES
        tests/unit/combat1_test.cpp
        tests/unit/cmbtfir2_test.cpp
    PRODUCTION_SOURCES
        src/game/cmbtfir2.cpp
        src/game/combat1.cpp
        src/game/random.cpp
    FAKE_SOURCES
        tests/fakes/fake_stasis_area_effects_runtime.cpp
)

foreach(TARGET_NAME IN ITEMS test_combat_stats test_combat_repair test_shield_table test_boarding test_stasis_area_effects)
    target_sources(${TARGET_NAME} PRIVATE tests/fakes/fake_combat_grid_runtime.cpp)
endforeach()

if(MSVC)
    target_compile_options(test_aidudes PRIVATE /Gy)
    target_link_options(test_aidudes PRIVATE /OPT:REF)
    target_compile_options(test_ai_hypertech PRIVATE /Gy)
    target_link_options(test_ai_hypertech PRIVATE /OPT:REF)
    target_compile_options(test_refit_cost PRIVATE /Gy)
    target_link_options(test_refit_cost PRIVATE /OPT:REF)
    target_compile_options(test_colony_production_helpers PRIVATE /Gy)
    target_link_options(test_colony_production_helpers PRIVATE /OPT:REF)
    target_compile_options(test_colony_production_draw PRIVATE /Gy)
    target_link_options(test_colony_production_draw PRIVATE /OPT:REF)
    target_compile_options(test_colony_scan_info PRIVATE /Gy)
    target_link_options(test_colony_scan_info PRIVATE /OPT:REF)
    target_compile_options(test_bombardment_ammo PRIVATE /Gy)
    target_link_options(test_bombardment_ammo PRIVATE /OPT:REF)
    target_compile_options(test_town_zero PRIVATE /Gy)
    target_link_options(test_town_zero PRIVATE /OPT:REF)
    target_compile_options(test_animation_frame PRIVATE /Gy)
    target_compile_options(test_ericnet PRIVATE /Gy)
    target_link_options(test_animation_frame PRIVATE /OPT:REF)
    target_link_options(test_ericnet PRIVATE /OPT:REF)
    target_compile_options(test_missile_defense PRIVATE /Gy)
    target_link_options(test_missile_defense PRIVATE /OPT:REF)
    target_compile_options(test_absorber_cleanup PRIVATE /Gy)
    target_link_options(test_absorber_cleanup PRIVATE /OPT:REF)
    target_compile_options(test_boarding PRIVATE /Gy)
    target_link_options(test_boarding PRIVATE /OPT:REF)
    target_compile_options(test_combat_stats PRIVATE /Gy)
    target_link_options(test_combat_stats PRIVATE /OPT:REF)
    target_compile_options(test_ground_commando PRIVATE /Gy)
    target_link_options(test_ground_commando PRIVATE /OPT:REF)
    target_compile_options(test_combat_repair PRIVATE /Gy)
    target_link_options(test_combat_repair PRIVATE /OPT:REF)
    target_compile_options(test_shield_table PRIVATE /Gy)
    target_link_options(test_shield_table PRIVATE /OPT:REF)
    target_compile_options(test_cmbtfire PRIVATE /Gy)
    target_link_options(test_cmbtfire PRIVATE /OPT:REF)
    target_compile_options(test_stasis_area_effects PRIVATE /Gy)
    target_link_options(test_stasis_area_effects PRIVATE /OPT:REF)
    target_compile_options(test_colony_pop_growth PRIVATE /Gy)
    target_link_options(test_colony_pop_growth PRIVATE /OPT:REF)
    target_compile_options(test_colony_replicators PRIVATE /Gy)
    target_link_options(test_colony_replicators PRIVATE /OPT:REF)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(test_aidudes PRIVATE -ffunction-sections -fdata-sections)
    target_compile_options(test_ai_hypertech PRIVATE -ffunction-sections -fdata-sections)
    target_compile_options(test_refit_cost PRIVATE -ffunction-sections -fdata-sections)
    target_compile_options(test_colony_production_helpers PRIVATE -ffunction-sections -fdata-sections)
    target_compile_options(test_colony_production_draw PRIVATE -ffunction-sections -fdata-sections)
    target_compile_options(test_colony_scan_info PRIVATE -ffunction-sections -fdata-sections)
    target_compile_options(test_bombardment_ammo PRIVATE -ffunction-sections -fdata-sections)
    target_compile_options(test_town_zero PRIVATE -ffunction-sections -fdata-sections)
    target_compile_options(test_animation_frame PRIVATE -ffunction-sections -fdata-sections)
    target_compile_options(test_ericnet PRIVATE -ffunction-sections -fdata-sections)
    target_compile_options(test_missile_defense PRIVATE -ffunction-sections -fdata-sections)
    target_compile_options(test_absorber_cleanup PRIVATE -ffunction-sections -fdata-sections)
    target_compile_options(test_boarding PRIVATE -ffunction-sections -fdata-sections)
    target_compile_options(test_combat_stats PRIVATE -ffunction-sections -fdata-sections)
    target_compile_options(test_ground_commando PRIVATE -ffunction-sections -fdata-sections)
    target_compile_options(test_combat_repair PRIVATE -ffunction-sections -fdata-sections)
    target_compile_options(test_shield_table PRIVATE -ffunction-sections -fdata-sections)
    target_compile_options(test_cmbtfire PRIVATE -ffunction-sections -fdata-sections)
    target_compile_options(test_stasis_area_effects PRIVATE -ffunction-sections -fdata-sections)
    target_compile_options(test_colony_pop_growth PRIVATE -ffunction-sections -fdata-sections)
    target_compile_options(test_colony_replicators PRIVATE -ffunction-sections -fdata-sections)
    if(APPLE)
        target_link_options(test_aidudes PRIVATE -Wl,-dead_strip)
        target_link_options(test_ai_hypertech PRIVATE -Wl,-dead_strip)
        target_link_options(test_refit_cost PRIVATE -Wl,-dead_strip)
        target_link_options(test_colony_production_helpers PRIVATE -Wl,-dead_strip)
        target_link_options(test_colony_production_draw PRIVATE -Wl,-dead_strip)
        target_link_options(test_colony_scan_info PRIVATE -Wl,-dead_strip)
        target_link_options(test_bombardment_ammo PRIVATE -Wl,-dead_strip)
        target_link_options(test_town_zero PRIVATE -Wl,-dead_strip)
        target_link_options(test_animation_frame PRIVATE -Wl,-dead_strip)
        target_link_options(test_ericnet PRIVATE -Wl,-dead_strip)
        target_link_options(test_missile_defense PRIVATE -Wl,-dead_strip)
        target_link_options(test_absorber_cleanup PRIVATE -Wl,-dead_strip)
        target_link_options(test_boarding PRIVATE -Wl,-dead_strip)
        target_link_options(test_combat_stats PRIVATE -Wl,-dead_strip)
        target_link_options(test_ground_commando PRIVATE -Wl,-dead_strip)
        target_link_options(test_combat_repair PRIVATE -Wl,-dead_strip)
        target_link_options(test_shield_table PRIVATE -Wl,-dead_strip)
        target_link_options(test_cmbtfire PRIVATE -Wl,-dead_strip)
        target_link_options(test_stasis_area_effects PRIVATE -Wl,-dead_strip)
        target_link_options(test_colony_pop_growth PRIVATE -Wl,-dead_strip)
        target_link_options(test_colony_replicators PRIVATE -Wl,-dead_strip)
    else()
        target_link_options(test_aidudes PRIVATE -Wl,--gc-sections)
        target_link_options(test_ai_hypertech PRIVATE -Wl,--gc-sections)
        target_link_options(test_refit_cost PRIVATE -Wl,--gc-sections)
        target_link_options(test_colony_production_helpers PRIVATE -Wl,--gc-sections)
        target_link_options(test_colony_production_draw PRIVATE -Wl,--gc-sections)
        target_link_options(test_colony_scan_info PRIVATE -Wl,--gc-sections)
        target_link_options(test_bombardment_ammo PRIVATE -Wl,--gc-sections)
        target_link_options(test_town_zero PRIVATE -Wl,--gc-sections)
        target_link_options(test_animation_frame PRIVATE -Wl,--gc-sections)
        target_link_options(test_ericnet PRIVATE -Wl,--gc-sections)
        target_link_options(test_missile_defense PRIVATE -Wl,--gc-sections)
        target_link_options(test_absorber_cleanup PRIVATE -Wl,--gc-sections)
        target_link_options(test_boarding PRIVATE -Wl,--gc-sections)
        target_link_options(test_combat_stats PRIVATE -Wl,--gc-sections)
        target_link_options(test_ground_commando PRIVATE -Wl,--gc-sections)
        target_link_options(test_combat_repair PRIVATE -Wl,--gc-sections)
        target_link_options(test_shield_table PRIVATE -Wl,--gc-sections)
        target_link_options(test_cmbtfire PRIVATE -Wl,--gc-sections)
        target_link_options(test_stasis_area_effects PRIVATE -Wl,--gc-sections)
        target_link_options(test_colony_pop_growth PRIVATE -Wl,--gc-sections)
        target_link_options(test_colony_replicators PRIVATE -Wl,--gc-sections)
    endif()
endif()

orion2re_add_unit_test(test_techinit
    SOURCES
        tests/unit/techinit_test.cpp
    PRODUCTION_SOURCES
        src/game/colcalc.cpp
        src/game/techinit.cpp
        src/game/techdata.cpp
        src/game/config_refresh.cpp
        src/game/arena.cpp
    FAKE_SOURCES
        tests/fakes/fake_arena_runtime.cpp
        tests/fakes/fake_techinit_runtime.cpp
    LINK_SDL
)

if(MSVC)
    target_compile_options(test_techinit PRIVATE /Gy)
    target_link_options(test_techinit PRIVATE /OPT:REF)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(test_techinit PRIVATE -ffunction-sections -fdata-sections)
    if(APPLE)
        target_link_options(test_techinit PRIVATE -Wl,-dead_strip)
    else()
        target_link_options(test_techinit PRIVATE -Wl,--gc-sections)
    endif()
endif()

orion2re_add_unit_test(test_map_scale
    SOURCES
        tests/unit/map_scale_test.cpp
    PRODUCTION_SOURCES
        src/game/harold.cpp
    FAKE_SOURCES
        tests/fakes/fake_config_runtime.cpp
        tests/fakes/fake_map_scale_runtime.cpp
)

if(MSVC)
    target_compile_options(test_map_scale PRIVATE /Gy)
    target_link_options(test_map_scale PRIVATE /OPT:REF)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(test_map_scale PRIVATE -ffunction-sections -fdata-sections)
    if(APPLE)
        target_link_options(test_map_scale PRIVATE -Wl,-dead_strip)
    else()
        target_link_options(test_map_scale PRIVATE -Wl,--gc-sections)
    endif()
endif()

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
        src/game/initgame.cpp
        src/game/initgame_rules.cpp
        src/game/newgame.cpp
        src/game/random.cpp
        src/lua/lua_game_ship_identity.cpp
    FAKE_SOURCES
        tests/fakes/fake_newgame_runtime.cpp
        tests/fakes/fake_sdl_string.cpp
)

if(MSVC)
    target_compile_options(test_newgame PRIVATE /Gy)
    target_link_options(test_newgame PRIVATE /OPT:REF)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(test_newgame PRIVATE -ffunction-sections -fdata-sections)
    if(APPLE)
        target_link_options(test_newgame PRIVATE -Wl,-dead_strip)
    else()
        target_link_options(test_newgame PRIVATE -Wl,--gc-sections)
    endif()
endif()

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
        src/game/mainscr.cpp
        src/game/netturn.cpp
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
target_compile_definitions(test_netturn_host_selection PRIVATE
    TEST_REAL_MAIN_RECEIVE
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(test_netturn_host_selection PRIVATE -ffunction-sections -fdata-sections)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU" AND NOT APPLE)
    target_link_options(test_netturn_host_selection PRIVATE -Wl,--gc-sections)
elseif(APPLE)
    target_link_options(test_netturn_host_selection PRIVATE -Wl,-dead_strip)
endif()

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

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(test_netmox_packet_limits PRIVATE -ffunction-sections -fdata-sections)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU" AND NOT APPLE)
    target_link_options(test_netmox_packet_limits PRIVATE -Wl,--gc-sections)
elseif(APPLE)
    target_link_options(test_netmox_packet_limits PRIVATE -Wl,-dead_strip)
endif()

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

orion2re_add_unit_test(test_smckplay
    SOURCES
        tests/unit/smckplay_test.cpp
    PRODUCTION_SOURCES
        src/game/smckplay.cpp
    FAKE_SOURCES
        tests/fakes/fake_smckplay_runtime.cpp
    LINK_SDL
)

if(MSVC)
    target_compile_options(test_smckplay PRIVATE /Gy)
    target_link_options(test_smckplay PRIVATE /OPT:REF)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(test_smckplay PRIVATE -ffunction-sections -fdata-sections)
    if(APPLE)
        target_link_options(test_smckplay PRIVATE -Wl,-dead_strip)
    else()
        target_link_options(test_smckplay PRIVATE -Wl,--gc-sections)
    endif()
endif()

orion2re_add_unit_test(test_sound
    SOURCES
        tests/unit/sound_test.cpp
    PRODUCTION_SOURCES
        src/game/sound.cpp
    FAKE_SOURCES
        tests/fakes/fake_sound_runtime.cpp
    LINK_SDL
)

if(MSVC)
    target_compile_options(test_sound PRIVATE /Gy)
    target_link_options(test_sound PRIVATE /OPT:REF)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(test_sound PRIVATE -ffunction-sections -fdata-sections)
    if(APPLE)
        target_link_options(test_sound PRIVATE -Wl,-dead_strip)
    else()
        target_link_options(test_sound PRIVATE -Wl,--gc-sections)
    endif()
endif()

orion2re_add_unit_test(test_farload_buffer_vfs
    SOURCES
        tests/unit/farload_test.cpp
    PRODUCTION_SOURCES
        src/game/buffer.cpp
        src/game/compat.cpp
        src/game/farload.cpp
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

orion2re_add_unit_test(test_fields
    SOURCES
        tests/unit/fields_test.cpp
    PRODUCTION_SOURCES
        src/game/exitm.cpp
        src/game/fields.cpp
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