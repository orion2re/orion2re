cmake_minimum_required(VERSION 3.20)

project(orion2re LANGUAGES C CXX)

include(CTest)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(ORION2RE_TARGET_ARCH x86)
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(ORION2RE_TARGET_ARCH x64)
    else()
        message(FATAL_ERROR "Linux builds are only supported for x86 and x64 targets.")
    endif()
elseif(APPLE)
    set(ORION2RE_LITTLE_ENDIAN_COMPILE_FLAG -mlittle-endian)
elseif(WIN32)
    set(ORION2RE_WINDOWS_ARCH "" CACHE STRING "Target Windows architecture (x86 or x64)")
    set_property(CACHE ORION2RE_WINDOWS_ARCH PROPERTY STRINGS x86 x64)

    string(TOLOWER "${ORION2RE_WINDOWS_ARCH}" ORION2RE_WINDOWS_ARCH_NORMALIZED)

    if(NOT ORION2RE_WINDOWS_ARCH_NORMALIZED)
        set(ORION2RE_WINDOWS_ARCH_HINT "")

        if(CMAKE_VS_PLATFORM_NAME)
            set(ORION2RE_WINDOWS_ARCH_HINT "${CMAKE_VS_PLATFORM_NAME}")
        elseif(CMAKE_GENERATOR_PLATFORM)
            set(ORION2RE_WINDOWS_ARCH_HINT "${CMAKE_GENERATOR_PLATFORM}")
        elseif(DEFINED ENV{VSCMD_ARG_TGT_ARCH})
            set(ORION2RE_WINDOWS_ARCH_HINT "$ENV{VSCMD_ARG_TGT_ARCH}")
        endif()

        string(TOLOWER "${ORION2RE_WINDOWS_ARCH_HINT}" ORION2RE_WINDOWS_ARCH_HINT)

        if(ORION2RE_WINDOWS_ARCH_HINT STREQUAL "win32" OR ORION2RE_WINDOWS_ARCH_HINT STREQUAL "x86")
            set(ORION2RE_WINDOWS_ARCH_NORMALIZED x86)
        elseif(ORION2RE_WINDOWS_ARCH_HINT STREQUAL "x64" OR ORION2RE_WINDOWS_ARCH_HINT STREQUAL "amd64")
            set(ORION2RE_WINDOWS_ARCH_NORMALIZED x64)
        endif()

        if(ORION2RE_WINDOWS_ARCH_NORMALIZED)
            set(ORION2RE_WINDOWS_ARCH "${ORION2RE_WINDOWS_ARCH_NORMALIZED}" CACHE STRING "Target Windows architecture (x86 or x64)" FORCE)
            set_property(CACHE ORION2RE_WINDOWS_ARCH PROPERTY STRINGS x86 x64)
        endif()
    endif()

    if(NOT ORION2RE_WINDOWS_ARCH_NORMALIZED)
        message(FATAL_ERROR
            "Select a Windows target architecture with -DORION2RE_WINDOWS_ARCH=x86 or x64 "
            "and configure the generator or toolchain for the same target."
        )
    elseif(ORION2RE_WINDOWS_ARCH_NORMALIZED STREQUAL "x86")
        set(ORION2RE_TARGET_ARCH x86)
        set(ORION2RE_WINDOWS_POINTER_SIZE 4)
    elseif(ORION2RE_WINDOWS_ARCH_NORMALIZED STREQUAL "x64")
        set(ORION2RE_TARGET_ARCH x64)
        set(ORION2RE_WINDOWS_POINTER_SIZE 8)
    else()
        message(FATAL_ERROR "Unsupported ORION2RE_WINDOWS_ARCH value '${ORION2RE_WINDOWS_ARCH}'. Use x86 or x64.")
    endif()

    if(CMAKE_SIZEOF_VOID_P AND NOT CMAKE_SIZEOF_VOID_P EQUAL ORION2RE_WINDOWS_POINTER_SIZE)
        if(MSVC)
            message(FATAL_ERROR
                "ORION2RE_WINDOWS_ARCH=${ORION2RE_WINDOWS_ARCH_NORMALIZED} does not match the configured MSVC generator platform. "
                "Use -A Win32 for x86 or -A x64 for x64."
            )
        else()
            message(FATAL_ERROR
                "ORION2RE_WINDOWS_ARCH=${ORION2RE_WINDOWS_ARCH_NORMALIZED} does not match the configured Windows toolchain target."
            )
        endif()
    endif()
endif()

set(ORION2RE_VENDORED_SDL3_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendors/SDL")
set(ORION2RE_VENDORED_SDL3_COMMIT "829a65d769d935c4852f8159e964312c0957260a")
set(ORION2RE_VENDORED_DOCTEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendors/doctest")
set(ORION2RE_VENDORED_DOCTEST_COMMIT "2d0a9359a60c51affe2a9bebb1be1dca47868151")
set(ORION2RE_VENDORED_LIBUV_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendors/libuv")
set(ORION2RE_VENDORED_LIBUV_COMMIT "840404ce8ba7cc0204be52389a6cfff9f2c90fb6")
set(ORION2RE_VENDORED_LZ4_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendors/lz4")
set(ORION2RE_VENDORED_LZ4_COMMIT "ebb370ca83af193212df4dcbadcc5d87bc0de2f0")
set(ORION2RE_VENDORED_MINIZ_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendors/miniz")
set(ORION2RE_VENDORED_MINIZ_COMMIT "77d0dce8627735138c51770d1799a1ef48f2117d")
set(ORION2RE_VENDORED_LIBSMACKER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendors/libsmacker")
set(ORION2RE_VENDORED_YYJSON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendors/yyjson")
set(ORION2RE_VENDORED_YYJSON_COMMIT "6447536015f3d600f3d65323b10976103b337ca7")
set(ORION2RE_VENDORED_LUA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendors/lua")
set(ORION2RE_VENDORED_LUA_COMMIT "7579fc9d7ed90240487251dfb69168f8e64e9294")
set(ORION2RE_VENDORED_IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendors/imgui")
set(ORION2RE_VENDORED_IMGUI_COMMIT "f5befd2d29e66809cd1110a152e375a7f1981f06")

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "" FORCE)
endif()

set(ORION2RE_GAME_DIR "" CACHE PATH "Optional game directory used for runtime output and debugger working directory")
set(ORION2RE_BASE_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/data/base")
set(ORION2RE_PACKAGED_BASE_ARCHIVE "${CMAKE_CURRENT_BINARY_DIR}/data/base.zip")
set(ORION2RE_DEFAULT_CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/data/orion2re.cfg")
set(ORION2RE_SUPPORTED_MODS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/data/mods")
set(ORION2RE_PACKAGED_MODS_DIR "${CMAKE_CURRENT_BINARY_DIR}/data/mods")

set(ORION2RE_LOCAL_CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/cmake/local-user.cmake")
if(EXISTS "${ORION2RE_LOCAL_CONFIG}")
    include("${ORION2RE_LOCAL_CONFIG}")
endif()

option(ORION2RE_ENABLE_ASAN "Enable AddressSanitizer (and UBSan with GCC/Clang) for supported debug builds" OFF)
option(ORION2RE_RELEASE_DEBUG_INFO "Emit debug info for non-Debug MSVC builds" OFF)
option(ORION2RE_BUILD_GAME "Build the SDL game executable" ON)
option(ORION2RE_USE_SYSTEM_SDL3
       "Link against a system-installed SDL3 located with find_package instead of building the vendored submodule"
       OFF)
option(ORION2RE_BUILD_TESTS "Build tests" OFF)
set(ORION2RE_BUILD_CONFIG_TOOL_DEFAULT OFF)
if(NOT CMAKE_CROSSCOMPILING AND
   EXISTS "${ORION2RE_VENDORED_YYJSON_DIR}/src/yyjson.h" AND
   EXISTS "${ORION2RE_VENDORED_YYJSON_DIR}/src/yyjson.c")
    set(ORION2RE_BUILD_CONFIG_TOOL_DEFAULT ON)
endif()

option(ORION2RE_BUILD_CONFIG_TOOL
       "Build configuration tools and regenerate bindings when their inputs change"
       ${ORION2RE_BUILD_CONFIG_TOOL_DEFAULT})
option(ORION2RE_BUILD_DOCUMENTATION
       "Build the MkDocs documentation portal"
       OFF)
option(ORION2RE_ENFORCE_ORIGINAL_STRUCT_SIZES "Require structures to retain their original game sizes" OFF)
option(ORION2RE_EXCLUDE_RUNTIME_FROM_ALL "Exclude runtime executables from the default all target" ${ORION2RE_BUILD_TESTS})

if(ORION2RE_ENABLE_ASAN)
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        message(FATAL_ERROR "ORION2RE_ENABLE_ASAN requires a Debug build.")
    endif()

    if(MSVC)
        set(ORION2RE_SANITIZER_COMPILE_OPTIONS /fsanitize=address)
        set(ORION2RE_SANITIZER_LINK_OPTIONS /INFERASANLIBS /fsanitize=address)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        set(ORION2RE_SANITIZER_COMPILE_OPTIONS
            -fsanitize=address,undefined -fno-omit-frame-pointer
            -fno-sanitize-recover=undefined)
        set(ORION2RE_SANITIZER_LINK_OPTIONS -fsanitize=address,undefined)
    else()
        message(FATAL_ERROR "ORION2RE_ENABLE_ASAN is unsupported for the current compiler.")
    endif()
endif()

function(orion2re_enable_sanitizers TARGET_NAME)
    if(ORION2RE_ENABLE_ASAN)
        target_compile_options(${TARGET_NAME} PRIVATE ${ORION2RE_SANITIZER_COMPILE_OPTIONS})
        get_target_property(TARGET_TYPE ${TARGET_NAME} TYPE)
        if(TARGET_TYPE STREQUAL "STATIC_LIBRARY")
            target_link_options(${TARGET_NAME} INTERFACE ${ORION2RE_SANITIZER_LINK_OPTIONS})
        else()
            target_link_options(${TARGET_NAME} PRIVATE ${ORION2RE_SANITIZER_LINK_OPTIONS})
        endif()
    endif()
endfunction()

if(ORION2RE_BUILD_GAME OR ORION2RE_BUILD_CONFIG_TOOL)
    if(NOT EXISTS "${ORION2RE_VENDORED_IMGUI_DIR}/imgui.h")
        message(FATAL_ERROR
            "The debug overlay requires the Dear ImGui submodule at vendors/imgui. "
            "Run 'git submodule update --init vendors/imgui' and ensure it is pinned to ${ORION2RE_VENDORED_IMGUI_COMMIT}."
        )
    endif()

    add_library(orion2re_imgui STATIC
        "${ORION2RE_VENDORED_IMGUI_DIR}/imgui.cpp"
        "${ORION2RE_VENDORED_IMGUI_DIR}/imgui_draw.cpp"
        "${ORION2RE_VENDORED_IMGUI_DIR}/imgui_tables.cpp"
        "${ORION2RE_VENDORED_IMGUI_DIR}/imgui_widgets.cpp"
    )
    target_compile_features(orion2re_imgui PUBLIC cxx_std_20)
    target_include_directories(orion2re_imgui PUBLIC
        "${ORION2RE_VENDORED_IMGUI_DIR}"
    )

    set(ORION2RE_DEBUG_OVERLAY_GENERATED_DIR
        "${CMAKE_CURRENT_BINARY_DIR}/generated/debug_overlay")
    set(ORION2RE_PROGGY_TINY_HEADER
        "${ORION2RE_DEBUG_OVERLAY_GENERATED_DIR}/proggy_tiny_font.h")
    add_custom_command(
        OUTPUT "${ORION2RE_PROGGY_TINY_HEADER}"
        COMMAND ${CMAKE_COMMAND}
            "-DINPUT_FILE=${ORION2RE_VENDORED_IMGUI_DIR}/misc/fonts/ProggyTiny.ttf"
            "-DOUTPUT_FILE=${ORION2RE_PROGGY_TINY_HEADER}"
            "-DSYMBOL_NAME=k_proggy_tiny_font"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_binary.cmake"
        DEPENDS
            "${ORION2RE_VENDORED_IMGUI_DIR}/misc/fonts/ProggyTiny.ttf"
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_binary.cmake"
        VERBATIM
    )
endif()

if(ORION2RE_BUILD_DOCUMENTATION AND CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR
        "ORION2RE_BUILD_DOCUMENTATION requires a native build because configuration documentation reads game-linked native defaults."
    )
endif()

if(ORION2RE_BUILD_DOCUMENTATION AND NOT ORION2RE_BUILD_CONFIG_TOOL)
    message(FATAL_ERROR
        "ORION2RE_BUILD_DOCUMENTATION requires ORION2RE_BUILD_CONFIG_TOOL=ON."
    )
endif()

if(ORION2RE_BUILD_CONFIG_TOOL AND CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR
        "ORION2RE_BUILD_CONFIG_TOOL requires a native build because documentation generation executes the game-linked tool."
    )
endif()

if(ORION2RE_BUILD_CONFIG_TOOL AND
   (NOT EXISTS "${ORION2RE_VENDORED_YYJSON_DIR}/src/yyjson.h" OR
    NOT EXISTS "${ORION2RE_VENDORED_YYJSON_DIR}/src/yyjson.c"))
    message(FATAL_ERROR
        "Configuration tooling requires the yyjson submodule at vendors/yyjson. "
        "Run 'git submodule update --init vendors/yyjson' and ensure it is pinned to ${ORION2RE_VENDORED_YYJSON_COMMIT}."
    )
endif()

if((ORION2RE_BUILD_GAME OR ORION2RE_BUILD_TESTS OR ORION2RE_BUILD_CONFIG_TOOL) AND
   (NOT EXISTS "${ORION2RE_VENDORED_YYJSON_DIR}/src/yyjson.h" OR
    NOT EXISTS "${ORION2RE_VENDORED_YYJSON_DIR}/src/yyjson.c"))
    message(FATAL_ERROR
        "Runtime mod loading requires the yyjson submodule at vendors/yyjson. "
        "Run 'git submodule update --init vendors/yyjson' and ensure it is pinned to ${ORION2RE_VENDORED_YYJSON_COMMIT}."
    )
endif()

if(ORION2RE_ENFORCE_ORIGINAL_STRUCT_SIZES)
    add_compile_definitions(ORION2RE_ENFORCE_ORIGINAL_STRUCT_SIZES=1)
endif()

if(ORION2RE_BUILD_GAME OR ORION2RE_BUILD_TESTS OR ORION2RE_BUILD_CONFIG_TOOL)
    if(NOT EXISTS "${ORION2RE_VENDORED_LUA_DIR}/lua.h")
        message(FATAL_ERROR
            "Lua integration requires the Lua submodule at vendors/lua. "
            "Run 'git submodule update --init vendors/lua' and ensure it is pinned to ${ORION2RE_VENDORED_LUA_COMMIT}."
        )
    endif()

    set(ORION2RE_LUA_SOURCE_DIR "${ORION2RE_VENDORED_LUA_DIR}")
    add_library(orion2re_lua STATIC
        "${ORION2RE_LUA_SOURCE_DIR}/lapi.c"
        "${ORION2RE_LUA_SOURCE_DIR}/lauxlib.c"
        "${ORION2RE_LUA_SOURCE_DIR}/lbaselib.c"
        "${ORION2RE_LUA_SOURCE_DIR}/lcode.c"
        "${ORION2RE_LUA_SOURCE_DIR}/lctype.c"
        "${ORION2RE_LUA_SOURCE_DIR}/ldebug.c"
        "${ORION2RE_LUA_SOURCE_DIR}/ldo.c"
        "${ORION2RE_LUA_SOURCE_DIR}/ldump.c"
        "${ORION2RE_LUA_SOURCE_DIR}/lfunc.c"
        "${ORION2RE_LUA_SOURCE_DIR}/lgc.c"
        "${ORION2RE_LUA_SOURCE_DIR}/llex.c"
        "${ORION2RE_LUA_SOURCE_DIR}/lmem.c"
        "${ORION2RE_LUA_SOURCE_DIR}/lobject.c"
        "${ORION2RE_LUA_SOURCE_DIR}/lopcodes.c"
        "${ORION2RE_LUA_SOURCE_DIR}/lparser.c"
        "${ORION2RE_LUA_SOURCE_DIR}/lstate.c"
        "${ORION2RE_LUA_SOURCE_DIR}/lstring.c"
        "${ORION2RE_LUA_SOURCE_DIR}/lstrlib.c"
        "${ORION2RE_LUA_SOURCE_DIR}/ltable.c"
        "${ORION2RE_LUA_SOURCE_DIR}/ltablib.c"
        "${ORION2RE_LUA_SOURCE_DIR}/ltm.c"
        "${ORION2RE_LUA_SOURCE_DIR}/lundump.c"
        "${ORION2RE_LUA_SOURCE_DIR}/lvm.c"
        "${ORION2RE_LUA_SOURCE_DIR}/lzio.c"
    )
    target_include_directories(orion2re_lua PUBLIC "${ORION2RE_LUA_SOURCE_DIR}")
    target_compile_definitions(orion2re_lua PRIVATE LUA_NOCVTS2N=1)
    set_target_properties(orion2re_lua PROPERTIES
        C_STANDARD 99
        C_STANDARD_REQUIRED YES
        POSITION_INDEPENDENT_CODE YES
    )
endif()

set(ORION2RE_TESTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cmake/tests")

set(ORION2RE_SCALER_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/game/scalers/dirty_regions.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/game/scalers/hqx.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/game/scalers/mmpx.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/game/scalers/scaler.cpp"
)

if(ORION2RE_LITTLE_ENDIAN_COMPILE_FLAG)
    include(CheckCCompilerFlag)
    include(CheckCXXCompilerFlag)

    check_c_compiler_flag("${ORION2RE_LITTLE_ENDIAN_COMPILE_FLAG}" ORION2RE_C_SUPPORTS_LITTLE_ENDIAN_FLAG)
    check_cxx_compiler_flag("${ORION2RE_LITTLE_ENDIAN_COMPILE_FLAG}" ORION2RE_CXX_SUPPORTS_LITTLE_ENDIAN_FLAG)

    if(NOT ORION2RE_C_SUPPORTS_LITTLE_ENDIAN_FLAG OR NOT ORION2RE_CXX_SUPPORTS_LITTLE_ENDIAN_FLAG)
        message(FATAL_ERROR "The configured compiler must support ${ORION2RE_LITTLE_ENDIAN_COMPILE_FLAG} for macOS builds.")
    endif()
endif()

if(ORION2RE_BUILD_TESTS AND NOT ORION2RE_BUILD_GAME)
    message(FATAL_ERROR "ORION2RE_BUILD_TESTS currently requires ORION2RE_BUILD_GAME=ON because the test targets depend on SDL.")
endif()

if((ORION2RE_BUILD_GAME OR ORION2RE_BUILD_TESTS OR ORION2RE_BUILD_CONFIG_TOOL) AND
   NOT ORION2RE_USE_SYSTEM_SDL3 AND
   NOT EXISTS "${ORION2RE_VENDORED_SDL3_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "This project requires the SDL3 submodule at vendors/SDL. "
        "Run 'git submodule update --init --recursive vendors/SDL' and ensure it is pinned to ${ORION2RE_VENDORED_SDL3_COMMIT}, "
        "or configure with -DORION2RE_USE_SYSTEM_SDL3=ON to use a system-installed SDL3."
    )
endif()

if(ORION2RE_BUILD_TESTS AND NOT EXISTS "${ORION2RE_VENDORED_DOCTEST_DIR}/doctest/doctest.h")
    message(FATAL_ERROR
        "Tests require the doctest submodule at vendors/doctest. "
        "Run 'git submodule update --init --recursive vendors/doctest' and ensure it is pinned to ${ORION2RE_VENDORED_DOCTEST_COMMIT}."
    )
endif()

if(NOT EXISTS "${ORION2RE_VENDORED_LIBUV_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "The router service requires the libuv submodule at vendors/libuv. "
        "Run 'git submodule update --init --recursive vendors/libuv' and ensure it is pinned to ${ORION2RE_VENDORED_LIBUV_COMMIT}."
    )
endif()

if(NOT EXISTS "${ORION2RE_VENDORED_LZ4_DIR}/lib/lz4.h" OR NOT EXISTS "${ORION2RE_VENDORED_LZ4_DIR}/lib/lz4.c")
    message(FATAL_ERROR
        "The router protocol requires the lz4 submodule at vendors/lz4. "
        "Run 'git submodule update --init --recursive vendors/lz4' and ensure it is pinned to ${ORION2RE_VENDORED_LZ4_COMMIT}."
    )
endif()

if((ORION2RE_BUILD_GAME OR ORION2RE_BUILD_TESTS OR ORION2RE_BUILD_CONFIG_TOOL) AND
   (NOT EXISTS "${ORION2RE_VENDORED_MINIZ_DIR}/CMakeLists.txt" OR
    NOT EXISTS "${ORION2RE_VENDORED_MINIZ_DIR}/miniz.h" OR
    NOT EXISTS "${ORION2RE_VENDORED_MINIZ_DIR}/miniz.c"))
    message(FATAL_ERROR
        "VFS zip support requires the miniz submodule at vendors/miniz. "
        "Run 'git submodule update --init --recursive vendors/miniz' and ensure it is pinned to ${ORION2RE_VENDORED_MINIZ_COMMIT}."
    )
endif()

if(NOT EXISTS "${ORION2RE_VENDORED_LIBSMACKER_DIR}/smacker.c")
    message(FATAL_ERROR
        "This project requires libsmacker sources at vendors/libsmacker."
    )
endif()

if(ORION2RE_BUILD_GAME OR ORION2RE_BUILD_TESTS OR ORION2RE_BUILD_CONFIG_TOOL)
    # ORION2RE_SDL3_LIBRARY names the SDL3 link target for every SDL-dependent
    # target in the project. SDL3::Headers is provided by both the vendored build
    # and the installed SDL3 CMake package.
    if(ORION2RE_USE_SYSTEM_SDL3)
        find_package(SDL3 3.2 REQUIRED CONFIG)
        # SDL3::SDL3 resolves to whichever of the shared or static library the
        # system package provides.
        set(ORION2RE_SDL3_LIBRARY SDL3::SDL3)
        message(STATUS "Using system SDL3 ${SDL3_VERSION} from ${SDL3_DIR}")
    else()
        set(SDL_SHARED OFF CACHE BOOL "" FORCE)
        set(SDL_STATIC ON CACHE BOOL "" FORCE)
        set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
        set(SDL_TESTS OFF CACHE BOOL "" FORCE)

        add_subdirectory("${ORION2RE_VENDORED_SDL3_DIR}" EXCLUDE_FROM_ALL)
        set(ORION2RE_SDL3_LIBRARY SDL3::SDL3-static)

        if(UNIX AND NOT APPLE AND SDL_UNIX_CONSOLE_BUILD AND ORION2RE_BUILD_GAME)
            message(WARNING
                "SDL_UNIX_CONSOLE_BUILD is enabled, so the orion2re game target is being built without desktop video backends and is not intended for gameplay. "
                "Use -DORION2RE_BUILD_GAME=OFF to build only orion2re-router."
            )
        endif()
    endif()

    add_subdirectory("${ORION2RE_VENDORED_MINIZ_DIR}" EXCLUDE_FROM_ALL)
    add_subdirectory(src/game/scalers)
endif()

set(LIBUV_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(LIBUV_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LIBUV_BUILD_BENCH OFF CACHE BOOL "" FORCE)

add_subdirectory("${ORION2RE_VENDORED_LIBUV_DIR}" EXCLUDE_FROM_ALL)

add_library(orion2re_lz4 STATIC
    "${ORION2RE_VENDORED_LZ4_DIR}/lib/lz4.c"
)

set(ORION2RE_CONFIG_TOOL_CORE_SOURCES
    tools/config/file_util.cpp
    tools/config/file_util.h
    tools/config/generate_cpp.cpp
    tools/config/generate_cpp.h
    tools/config/generate_docs.cpp
    tools/config/generate_docs.h
    tools/config/json5_reader.cpp
    tools/config/json5_reader.h
    tools/config/schema_model.cpp
    tools/config/schema_model.h
)
set(ORION2RE_CONFIG_TOOL_SOURCES
    tools/config/config_tool.cpp
    ${ORION2RE_CONFIG_TOOL_CORE_SOURCES}
)

set(ORION2RE_CONFIG_SCHEMA "${CMAKE_CURRENT_SOURCE_DIR}/specs/config/main.json5")
set(ORION2RE_CONFIG_GENERATED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/config/generated")
set(ORION2RE_CONFIG_GENERATED_FILES
    "${ORION2RE_CONFIG_GENERATED_DIR}/config.cpp"
    "${ORION2RE_CONFIG_GENERATED_DIR}/config_enums.cpp"
    "${ORION2RE_CONFIG_GENERATED_DIR}/config_enums.h"
    "${ORION2RE_CONFIG_GENERATED_DIR}/config_groups.h"
)

if(ORION2RE_BUILD_CONFIG_TOOL)
    add_executable(config-generator EXCLUDE_FROM_ALL
        ${ORION2RE_CONFIG_TOOL_SOURCES}
        "${ORION2RE_VENDORED_YYJSON_DIR}/src/yyjson.c"
    )
    target_compile_features(config-generator PRIVATE cxx_std_20)
    target_include_directories(config-generator PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/config"
        "${ORION2RE_VENDORED_YYJSON_DIR}/src"
    )

    add_custom_target(config-schema-validate
        COMMAND $<TARGET_FILE:config-generator> validate "${ORION2RE_CONFIG_SCHEMA}"
        DEPENDS config-generator "${ORION2RE_CONFIG_SCHEMA}"
        VERBATIM
    )

    add_custom_target(config-regenerate
        ALL
        COMMAND $<TARGET_FILE:config-generator> generate
            "${ORION2RE_CONFIG_SCHEMA}"
            "${ORION2RE_CONFIG_GENERATED_DIR}"
        DEPENDS
            config-generator
            "${ORION2RE_CONFIG_SCHEMA}"
            ${ORION2RE_CONFIG_TOOL_SOURCES}
        VERBATIM
    )

    set_source_files_properties(${ORION2RE_CONFIG_GENERATED_FILES} PROPERTIES
        GENERATED TRUE
    )

    add_custom_target(config-generated-check
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${CMAKE_CURRENT_BINARY_DIR}/generated-check"
        COMMAND $<TARGET_FILE:config-generator> check
            "${ORION2RE_CONFIG_SCHEMA}"
            "${ORION2RE_CONFIG_GENERATED_DIR}"
            "${CMAKE_CURRENT_BINARY_DIR}/generated-check"
        DEPENDS config-generator "${ORION2RE_CONFIG_SCHEMA}" ${ORION2RE_CONFIG_TOOL_SOURCES}
        VERBATIM
    )
elseif((ORION2RE_BUILD_GAME OR ORION2RE_BUILD_TESTS) AND
       (NOT EXISTS "${ORION2RE_CONFIG_GENERATED_DIR}/config.cpp" OR
        NOT EXISTS "${ORION2RE_CONFIG_GENERATED_DIR}/config_enums.cpp" OR
        NOT EXISTS "${ORION2RE_CONFIG_GENERATED_DIR}/config_enums.h" OR
        NOT EXISTS "${ORION2RE_CONFIG_GENERATED_DIR}/config_groups.h"))
    message(FATAL_ERROR
        "Building the game or tests requires generated configuration bindings. "
        "Enable ORION2RE_BUILD_CONFIG_TOOL and use a native build to generate them."
    )
endif()
target_include_directories(orion2re_lz4 PUBLIC
    "${ORION2RE_VENDORED_LZ4_DIR}/lib"
)

if(ORION2RE_BUILD_GAME OR ORION2RE_BUILD_CONFIG_TOOL)
    file(GLOB_RECURSE ORION2RE_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    )

    file(GLOB_RECURSE ORION2RE_HEADERS CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
    )

    list(REMOVE_ITEM ORION2RE_SOURCES ${ORION2RE_CONFIG_GENERATED_FILES})
    list(REMOVE_ITEM ORION2RE_SOURCES ${ORION2RE_SCALER_SOURCES})
    list(APPEND ORION2RE_SOURCES ${ORION2RE_CONFIG_GENERATED_FILES})
    list(REMOVE_ITEM ORION2RE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

    add_library(orion2re_game_objects OBJECT
        ${ORION2RE_SOURCES}
        ${ORION2RE_HEADERS}
    )
    target_compile_features(orion2re_game_objects PRIVATE cxx_std_20)
    target_include_directories(orion2re_game_objects PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${ORION2RE_VENDORED_LIBSMACKER_DIR}"
        "${ORION2RE_VENDORED_YYJSON_DIR}/src"
    )
    target_precompile_headers(orion2re_game_objects PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/src/pch.h>"
    )
    target_link_libraries(orion2re_game_objects PRIVATE
        SDL3::Headers
        uv_a
        orion2re_lz4
        orion2re_lua
        miniz
    )
    target_compile_definitions(orion2re_game_objects PRIVATE ORION2RE_ENABLE_DEBUG_OVERLAY=1)
    target_include_directories(orion2re_game_objects PRIVATE
        "${ORION2RE_DEBUG_OVERLAY_GENERATED_DIR}"
    )
    target_sources(orion2re_game_objects PRIVATE
        "${ORION2RE_PROGGY_TINY_HEADER}"
    )
    target_link_libraries(orion2re_game_objects PRIVATE orion2re_imgui)
    if(ORION2RE_BUILD_CONFIG_TOOL)
        add_dependencies(orion2re_game_objects config-regenerate)
    endif()
endif()

file(GLOB_RECURSE ORION2RE_BASE_DATA_FILES CONFIGURE_DEPENDS
    "${ORION2RE_BASE_DATA_DIR}/*"
)
set(ORION2RE_PACKAGE_BASE_DATA_COMMANDS
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/data"
    COMMAND ${CMAKE_COMMAND} -E tar cf "${ORION2RE_PACKAGED_BASE_ARCHIVE}"
        --format=zip -- .
)
if(ORION2RE_GAME_DIR)
    list(APPEND ORION2RE_PACKAGE_BASE_DATA_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E make_directory "${ORION2RE_GAME_DIR}/data"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ORION2RE_PACKAGED_BASE_ARCHIVE}"
            "${ORION2RE_GAME_DIR}/data/base.zip"
    )
endif()
add_custom_target(package-base-data
    ${ORION2RE_PACKAGE_BASE_DATA_COMMANDS}
    WORKING_DIRECTORY "${ORION2RE_BASE_DATA_DIR}"
    DEPENDS ${ORION2RE_BASE_DATA_FILES}
    VERBATIM
)

set(ORION2RE_PACKAGE_DEFAULT_CONFIG_COMMANDS
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/data"
    COMMAND ${CMAKE_COMMAND}
        "-DINPUT_FILE=${ORION2RE_DEFAULT_CONFIG}"
        "-DOUTPUT_FILE=${CMAKE_CURRENT_BINARY_DIR}/data/orion2re.cfg"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_if_missing.cmake"
)
if(ORION2RE_GAME_DIR)
    list(APPEND ORION2RE_PACKAGE_DEFAULT_CONFIG_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E make_directory "${ORION2RE_GAME_DIR}/data"
        COMMAND ${CMAKE_COMMAND}
            "-DINPUT_FILE=${ORION2RE_DEFAULT_CONFIG}"
            "-DOUTPUT_FILE=${ORION2RE_GAME_DIR}/data/orion2re.cfg"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_if_missing.cmake"
    )
endif()
add_custom_target(package-default-config
    ${ORION2RE_PACKAGE_DEFAULT_CONFIG_COMMANDS}
    DEPENDS "${ORION2RE_DEFAULT_CONFIG}"
    VERBATIM
)

set(ORION2RE_SUPPORTED_MOD_ARCHIVES)
file(GLOB ORION2RE_SUPPORTED_MOD_IDS CONFIGURE_DEPENDS
    RELATIVE "${ORION2RE_SUPPORTED_MODS_DIR}"
    "${ORION2RE_SUPPORTED_MODS_DIR}/*"
)
foreach(ORION2RE_SUPPORTED_MOD_ID IN LISTS ORION2RE_SUPPORTED_MOD_IDS)
    set(ORION2RE_SUPPORTED_MOD_SOURCE
        "${ORION2RE_SUPPORTED_MODS_DIR}/${ORION2RE_SUPPORTED_MOD_ID}")
    if(IS_DIRECTORY "${ORION2RE_SUPPORTED_MOD_SOURCE}")
        set(ORION2RE_SUPPORTED_MOD_ARCHIVE
            "${ORION2RE_PACKAGED_MODS_DIR}/${ORION2RE_SUPPORTED_MOD_ID}.zip")
        file(GLOB_RECURSE ORION2RE_SUPPORTED_MOD_FILES CONFIGURE_DEPENDS
            "${ORION2RE_SUPPORTED_MOD_SOURCE}/*"
        )
        add_custom_command(
            OUTPUT "${ORION2RE_SUPPORTED_MOD_ARCHIVE}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${ORION2RE_PACKAGED_MODS_DIR}"
            COMMAND ${CMAKE_COMMAND} -E tar cf "${ORION2RE_SUPPORTED_MOD_ARCHIVE}"
                --format=zip -- .
            WORKING_DIRECTORY "${ORION2RE_SUPPORTED_MOD_SOURCE}"
            DEPENDS ${ORION2RE_SUPPORTED_MOD_FILES}
            VERBATIM
        )
        list(APPEND ORION2RE_SUPPORTED_MOD_ARCHIVES
            "${ORION2RE_SUPPORTED_MOD_ARCHIVE}")
    endif()
endforeach()
add_custom_target(package-supported-mods
    DEPENDS ${ORION2RE_SUPPORTED_MOD_ARCHIVES}
)

if(ORION2RE_GAME_DIR)
    add_custom_target(deploy-supported-mods
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${ORION2RE_GAME_DIR}/data/mods"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${ORION2RE_SUPPORTED_MOD_ARCHIVES}
            "${ORION2RE_GAME_DIR}/data/mods"
        DEPENDS package-supported-mods
        VERBATIM
    )
endif()

if(ORION2RE_GAME_DIR)
    add_custom_target(package-runtime-data
        DEPENDS package-base-data package-default-config deploy-supported-mods
    )
else()
    add_custom_target(package-runtime-data
        DEPENDS package-base-data package-default-config package-supported-mods
    )
endif()

set(ORION2RE_ROUTER_SOURCES
    src/network/router_cli.cpp
    src/network/router_core.cpp
    src/network/router_protocol.cpp
    src/network/router_service.cpp
)

if(ORION2RE_BUILD_GAME)
    add_executable(orion2re
        src/main.cpp
        $<TARGET_OBJECTS:orion2re_game_objects>
        "${ORION2RE_VENDORED_LIBSMACKER_DIR}/smacker.c"
        "${ORION2RE_VENDORED_LIBSMACKER_DIR}/smk_bitstream.c"
        "${ORION2RE_VENDORED_LIBSMACKER_DIR}/smk_hufftree.c"
        "${ORION2RE_VENDORED_YYJSON_DIR}/src/yyjson.c"
    )

    if(ORION2RE_EXCLUDE_RUNTIME_FROM_ALL)
        set_target_properties(orion2re PROPERTIES EXCLUDE_FROM_ALL YES)
    endif()

    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" FILES
        ${ORION2RE_SOURCES}
        ${ORION2RE_HEADERS}
    )

    target_compile_features(orion2re PRIVATE cxx_std_20)
    target_include_directories(orion2re PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${ORION2RE_VENDORED_LIBSMACKER_DIR}"
        "${ORION2RE_VENDORED_YYJSON_DIR}/src"
    )

    target_precompile_headers(orion2re PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/src/pch.h>"
    )

    target_link_libraries(orion2re PRIVATE
        ${ORION2RE_SDL3_LIBRARY}
        uv_a
        orion2re_lz4
        orion2re_lua
        orion2re_scalers
        miniz
    )
    target_link_libraries(orion2re PRIVATE orion2re_imgui)
    add_executable(config-load-benchmark EXCLUDE_FROM_ALL
        tools/config/load_benchmark.cpp
        $<TARGET_OBJECTS:orion2re_game_objects>
        "${ORION2RE_VENDORED_LIBSMACKER_DIR}/smacker.c"
        "${ORION2RE_VENDORED_LIBSMACKER_DIR}/smk_bitstream.c"
        "${ORION2RE_VENDORED_LIBSMACKER_DIR}/smk_hufftree.c"
        "${ORION2RE_VENDORED_YYJSON_DIR}/src/yyjson.c"
    )
    target_compile_features(config-load-benchmark PRIVATE cxx_std_20)
    target_include_directories(config-load-benchmark PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${ORION2RE_VENDORED_LIBSMACKER_DIR}"
        "${ORION2RE_VENDORED_YYJSON_DIR}/src")
    target_link_libraries(config-load-benchmark PRIVATE ${ORION2RE_SDL3_LIBRARY} uv_a
        orion2re_lz4 orion2re_lua orion2re_scalers miniz orion2re_imgui)
    if(MSVC)
        target_compile_definitions(config-load-benchmark PRIVATE WIN32 _CRT_SECURE_NO_DEPRECATE _CRT_NONSTDC_NO_DEPRECATE)
    endif()
    add_dependencies(orion2re package-runtime-data)
    if(ORION2RE_BUILD_CONFIG_TOOL)
        add_dependencies(orion2re config-regenerate)
    endif()

    if(ORION2RE_GAME_DIR)
        add_custom_command(TARGET orion2re POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:orion2re>
                "${ORION2RE_GAME_DIR}/$<TARGET_FILE_NAME:orion2re>"
        )

        if(MSVC)
            add_custom_command(TARGET orion2re POST_BUILD
                COMMAND ${CMAKE_COMMAND}
                    "-DINPUT_FILE=$<TARGET_PDB_FILE:orion2re>"
                    "-DOUTPUT_FILE=${ORION2RE_GAME_DIR}/$<TARGET_PDB_FILE_NAME:orion2re>"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_if_exists.cmake"
            )
        endif()
    endif()

    if(APPLE AND CMAKE_BUILD_TYPE STREQUAL "Release")
        add_custom_target(orion2re-app ALL DEPENDS orion2re)
        set(ORION2RE_APP_DIRS "${CMAKE_CURRENT_BINARY_DIR}")
        if(ORION2RE_GAME_DIR)
            list(APPEND ORION2RE_APP_DIRS "${ORION2RE_GAME_DIR}")
        endif()
        foreach(ORION2RE_APP_DIR IN LISTS ORION2RE_APP_DIRS)
            set(ORION2RE_APP_CONTENTS "${ORION2RE_APP_DIR}/Orion2 Redone.app/Contents")
            add_custom_command(TARGET orion2re-app POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory "${ORION2RE_APP_CONTENTS}/MacOS"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/orion2re.Info.plist"
                    "${ORION2RE_APP_CONTENTS}/Info.plist"
                COMMAND ${CMAKE_COMMAND} -E create_symlink
                    "../../../$<TARGET_FILE_NAME:orion2re>"
                    "${ORION2RE_APP_CONTENTS}/MacOS/orion2re"
                VERBATIM
            )
        endforeach()
    endif()
endif()

if(ORION2RE_BUILD_CONFIG_TOOL)
    add_executable(config-tool EXCLUDE_FROM_ALL
        ${ORION2RE_CONFIG_TOOL_SOURCES}
        $<TARGET_OBJECTS:orion2re_game_objects>
        "${ORION2RE_VENDORED_LIBSMACKER_DIR}/smacker.c"
        "${ORION2RE_VENDORED_LIBSMACKER_DIR}/smk_bitstream.c"
        "${ORION2RE_VENDORED_LIBSMACKER_DIR}/smk_hufftree.c"
        "${ORION2RE_VENDORED_YYJSON_DIR}/src/yyjson.c"
    )
    target_compile_features(config-tool PRIVATE cxx_std_20)
    target_compile_definitions(config-tool PRIVATE
        ORION2RE_CONFIG_TOOL_LINK_GAME=1
    )
    target_include_directories(config-tool PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/config"
        "${ORION2RE_VENDORED_LIBSMACKER_DIR}"
        "${ORION2RE_VENDORED_YYJSON_DIR}/src"
    )
    target_link_libraries(config-tool PRIVATE
        ${ORION2RE_SDL3_LIBRARY}
        uv_a
        orion2re_lz4
        orion2re_lua
        orion2re_scalers
        miniz
    )
    target_link_libraries(config-tool PRIVATE orion2re_imgui)

    set(ORION2RE_DOCUMENTATION_BUILD_DIR
        "${CMAKE_CURRENT_BINARY_DIR}/docs")
    set(ORION2RE_CONFIG_DOCUMENTATION_DIR
        "${ORION2RE_DOCUMENTATION_BUILD_DIR}/generated/configuration")
    set(ORION2RE_CONFIG_DOCUMENTATION_STAMP
        "${ORION2RE_DOCUMENTATION_BUILD_DIR}/config-docs.stamp")
    add_custom_command(
        OUTPUT "${ORION2RE_CONFIG_DOCUMENTATION_STAMP}"
        COMMAND ${CMAKE_COMMAND} -E rm -rf
            "${ORION2RE_CONFIG_DOCUMENTATION_DIR}"
        COMMAND $<TARGET_FILE:config-tool> docs
            "${ORION2RE_CONFIG_SCHEMA}"
            "${ORION2RE_CONFIG_DOCUMENTATION_DIR}"
        COMMAND ${CMAKE_COMMAND} -E touch
            "${ORION2RE_CONFIG_DOCUMENTATION_STAMP}"
        DEPENDS config-tool "${ORION2RE_CONFIG_SCHEMA}"
            ${ORION2RE_CONFIG_TOOL_SOURCES}
        VERBATIM
    )
    add_custom_target(config-docs
        DEPENDS "${ORION2RE_CONFIG_DOCUMENTATION_STAMP}"
    )
endif()

if(ORION2RE_BUILD_DOCUMENTATION)
    find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" -c "import ensurepip, venv"
        RESULT_VARIABLE ORION2RE_PYTHON_MODULE_CHECK
        ERROR_VARIABLE ORION2RE_PYTHON_MODULE_ERROR
    )
    if(NOT ORION2RE_PYTHON_MODULE_CHECK EQUAL 0)
        message(FATAL_ERROR
            "Documentation requires Python modules 'venv' and 'ensurepip'. "
            "Install the platform's Python virtual-environment package and reconfigure. "
            "Python reported: ${ORION2RE_PYTHON_MODULE_ERROR}"
        )
    endif()

    set(ORION2RE_DOCUMENTATION_SOURCE_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/docs")
    set(ORION2RE_DOCUMENTATION_LOCK
        "${ORION2RE_DOCUMENTATION_SOURCE_DIR}/requirements.lock")
    set(ORION2RE_DOCUMENTATION_VENV
        "${CMAKE_CURRENT_SOURCE_DIR}/.venv")
    set(ORION2RE_DOCUMENTATION_SITE_DIR
        "${ORION2RE_DOCUMENTATION_BUILD_DIR}/site")
    set(ORION2RE_DOCUMENTATION_BOOTSTRAP_STAMP
        "${ORION2RE_DOCUMENTATION_VENV}/.orion2re-documentation.stamp")
    if(WIN32)
        set(ORION2RE_DOCUMENTATION_PYTHON
            "${ORION2RE_DOCUMENTATION_VENV}/Scripts/python.exe")
    else()
        set(ORION2RE_DOCUMENTATION_PYTHON
            "${ORION2RE_DOCUMENTATION_VENV}/bin/python")
    endif()

    add_custom_command(
        OUTPUT "${ORION2RE_DOCUMENTATION_BOOTSTRAP_STAMP}"
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/docs_environment.py"
            "${ORION2RE_DOCUMENTATION_VENV}"
            "${ORION2RE_DOCUMENTATION_LOCK}"
        COMMAND ${CMAKE_COMMAND} -E touch
            "${ORION2RE_DOCUMENTATION_BOOTSTRAP_STAMP}"
        DEPENDS "${ORION2RE_DOCUMENTATION_LOCK}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/docs_environment.py"
        COMMENT "Creating the pinned documentation environment"
        VERBATIM
    )
    add_custom_target(docs-bootstrap
        DEPENDS "${ORION2RE_DOCUMENTATION_BOOTSTRAP_STAMP}"
    )

    add_custom_target(docs-build
        COMMAND ${CMAKE_COMMAND} -E env
            "ORION2RE_GENERATED_CONFIG_DOCS=${ORION2RE_CONFIG_DOCUMENTATION_DIR}"
            "ORION2RE_DOCUMENTATION_SITE_DIR=${ORION2RE_DOCUMENTATION_SITE_DIR}"
            "DISABLE_MKDOCS_2_WARNING=true"
            "${ORION2RE_DOCUMENTATION_PYTHON}" -m mkdocs build
            --strict
            --config-file "${ORION2RE_DOCUMENTATION_SOURCE_DIR}/mkdocs.yml"
            --site-dir "${ORION2RE_DOCUMENTATION_SITE_DIR}"
        WORKING_DIRECTORY "${ORION2RE_DOCUMENTATION_SOURCE_DIR}"
        DEPENDS docs-bootstrap config-docs
        COMMENT "Building the documentation portal"
        VERBATIM
    )
    add_custom_target(docs-check DEPENDS docs-build)

    set(ORION2RE_PDF_VENV "${ORION2RE_DOCUMENTATION_BUILD_DIR}/pdf-venv")
    if(WIN32)
        set(ORION2RE_PDF_PYTHON "${ORION2RE_PDF_VENV}/Scripts/python.exe")
    else()
        set(ORION2RE_PDF_PYTHON "${ORION2RE_PDF_VENV}/bin/python")
    endif()
    set(ORION2RE_PDF_LOCK "${ORION2RE_DOCUMENTATION_SOURCE_DIR}/requirements-pdf.lock")
    set(ORION2RE_PDF_BOOTSTRAP_STAMP "${ORION2RE_PDF_VENV}/.bootstrap.stamp")
    add_custom_command(
        OUTPUT "${ORION2RE_PDF_BOOTSTRAP_STAMP}"
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/docs_environment.py"
            --pdf "${ORION2RE_PDF_VENV}" "${ORION2RE_PDF_LOCK}"
        COMMAND ${CMAKE_COMMAND} -E touch "${ORION2RE_PDF_BOOTSTRAP_STAMP}"
        DEPENDS "${ORION2RE_PDF_LOCK}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/docs_environment.py"
        COMMENT "Creating the pinned PDF environment (Python 3.11+ required)"
        VERBATIM
    )
    add_custom_target(docs-pdf-bootstrap DEPENDS "${ORION2RE_PDF_BOOTSTRAP_STAMP}")
    add_custom_target(docs-pdf
        COMMAND ${CMAKE_COMMAND} -E env
            "ORION2RE_GENERATED_CONFIG_DOCS=${ORION2RE_CONFIG_DOCUMENTATION_DIR}"
            "DISABLE_MKDOCS_2_WARNING=true"
            "${ORION2RE_PDF_PYTHON}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/build_documentation_pdf.py"
            --config "${ORION2RE_DOCUMENTATION_SOURCE_DIR}/mkdocs-pdf.yml"
            --site-dir "${ORION2RE_DOCUMENTATION_BUILD_DIR}/pdf-site"
            --output "${ORION2RE_DOCUMENTATION_BUILD_DIR}/orion2re-documentation.pdf"
        WORKING_DIRECTORY "${ORION2RE_DOCUMENTATION_SOURCE_DIR}"
        DEPENDS docs-pdf-bootstrap config-docs
        COMMENT "Building and validating the complete documentation PDF"
        VERBATIM
    )

    add_custom_target(docs-serve
        COMMAND ${CMAKE_COMMAND} -E env
            "ORION2RE_GENERATED_CONFIG_DOCS=${ORION2RE_CONFIG_DOCUMENTATION_DIR}"
            "ORION2RE_DOCUMENTATION_SITE_DIR=${ORION2RE_DOCUMENTATION_SITE_DIR}"
            "ORION2RE_CONFIG_DOCS_BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}"
            "DISABLE_MKDOCS_2_WARNING=true"
            "${ORION2RE_DOCUMENTATION_PYTHON}" -m mkdocs serve
            --strict
            --dev-addr 127.0.0.1:8000
            --config-file "${ORION2RE_DOCUMENTATION_SOURCE_DIR}/mkdocs.yml"
        WORKING_DIRECTORY "${ORION2RE_DOCUMENTATION_SOURCE_DIR}"
        DEPENDS docs-bootstrap config-docs
        USES_TERMINAL
        VERBATIM
    )

    add_custom_target(docs-clean
        COMMAND ${CMAKE_COMMAND} -E rm -rf
            "${ORION2RE_DOCUMENTATION_BUILD_DIR}/generated"
            "${ORION2RE_DOCUMENTATION_SITE_DIR}"
            "${ORION2RE_DOCUMENTATION_BUILD_DIR}/pdf-site"
            "${ORION2RE_DOCUMENTATION_BUILD_DIR}/orion2re-documentation.pdf"
            "${ORION2RE_CONFIG_DOCUMENTATION_STAMP}"
        COMMENT "Removing generated documentation while retaining the virtual environment"
        VERBATIM
    )
endif()

add_executable(orion2re-router
    tools/router_main.cpp
    ${ORION2RE_ROUTER_SOURCES}
)

if(ORION2RE_EXCLUDE_RUNTIME_FROM_ALL)
    set_target_properties(orion2re-router PROPERTIES EXCLUDE_FROM_ALL YES)
endif()

target_compile_features(orion2re-router PRIVATE cxx_std_20)
target_include_directories(orion2re-router PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(orion2re-router PRIVATE
    uv_a
    orion2re_lz4
)

if(ORION2RE_LITTLE_ENDIAN_COMPILE_FLAG)
    if(TARGET orion2re_game_objects)
        target_compile_options(orion2re_game_objects PRIVATE
            "${ORION2RE_LITTLE_ENDIAN_COMPILE_FLAG}")
    endif()
    if(TARGET orion2re_lua)
        target_compile_options(orion2re_lua PRIVATE
            "${ORION2RE_LITTLE_ENDIAN_COMPILE_FLAG}")
    endif()
    if(TARGET orion2re)
        target_compile_options(orion2re PRIVATE "${ORION2RE_LITTLE_ENDIAN_COMPILE_FLAG}")
    endif()
    target_compile_options(orion2re-router PRIVATE "${ORION2RE_LITTLE_ENDIAN_COMPILE_FLAG}")
endif()
if(TARGET orion2re_game_objects)
    target_compile_definitions(orion2re_game_objects PRIVATE
        $<$<CONFIG:Debug>:ORION2RE_DEBUG_BUILD=1>
    )
endif()

if(WIN32 AND TARGET orion2re)
    set_target_properties(orion2re PROPERTIES WIN32_EXECUTABLE YES)
endif()

if(TARGET orion2re)
    target_compile_definitions(orion2re PRIVATE
        $<$<CONFIG:Debug>:ORION2RE_DEBUG_BUILD=1>
    )
endif()

if(ORION2RE_GAME_DIR AND TARGET orion2re)
    set_target_properties(orion2re PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${ORION2RE_GAME_DIR}"
    )
endif()

if(MSVC)
    if(TARGET orion2re)
        target_compile_options(orion2re PRIVATE /W3 /permissive-)
    endif()
    target_compile_options(orion2re-router PRIVATE /W3 /permissive-)
    if(TARGET config-tool)
        target_compile_options(config-tool PRIVATE /W3 /permissive-)
    endif()
    if(TARGET config-generator)
        target_compile_options(config-generator PRIVATE /W3 /permissive-)
    endif()
    if(TARGET orion2re_game_objects)
        target_compile_options(orion2re_game_objects PRIVATE /W3 /permissive-)
        target_compile_definitions(orion2re_game_objects PRIVATE
            WIN32
            _CRT_SECURE_NO_DEPRECATE
            _CRT_NONSTDC_NO_DEPRECATE
        )
    endif()
    if(TARGET orion2re)
        target_link_options(orion2re PRIVATE /MANIFESTUAC:NO)
    endif()

    if(TARGET orion2re)
        target_compile_definitions(orion2re PRIVATE
            WIN32
            _CRT_SECURE_NO_DEPRECATE
            _CRT_NONSTDC_NO_DEPRECATE
        )
    endif()

    target_compile_definitions(orion2re-router PRIVATE
        WIN32
        _CRT_SECURE_NO_DEPRECATE
        _CRT_NONSTDC_NO_DEPRECATE
    )

    if(TARGET orion2re)
        target_compile_options(orion2re PRIVATE
            $<$<CONFIG:Debug>:/RTC1>
            $<$<CONFIG:Debug>:/GS>
            $<$<CONFIG:Release>:/Gy>
            $<$<CONFIG:Release>:/Oi>
            $<$<CONFIG:Release>:/GS>
            $<$<AND:$<CONFIG:Release>,$<BOOL:${ORION2RE_RELEASE_DEBUG_INFO}>>:/Zi>
            $<$<AND:$<CONFIG:Release>,$<BOOL:${ORION2RE_RELEASE_DEBUG_INFO}>>:/Zo>
        )

        target_link_options(orion2re PRIVATE
            /DYNAMICBASE
            /NXCOMPAT
            $<$<CONFIG:Release>:/OPT:REF>
            $<$<CONFIG:Release>:/OPT:ICF>
            $<$<AND:$<CONFIG:Release>,$<BOOL:${ORION2RE_RELEASE_DEBUG_INFO}>>:/DEBUG>
        )
    endif()
    if(TARGET config-tool)
        target_link_options(config-tool PRIVATE
            /DYNAMICBASE
            /NXCOMPAT
            $<$<CONFIG:Release>:/OPT:REF>
            $<$<CONFIG:Release>:/OPT:ICF>
        )
    endif()
endif()

if(ORION2RE_ENABLE_ASAN)
    if(TARGET orion2re)
        target_compile_definitions(orion2re PRIVATE ORION2RE_ASAN_BUILD=1)
    endif()
    if(TARGET orion2re_game_objects)
        target_compile_definitions(orion2re_game_objects PRIVATE ORION2RE_ASAN_BUILD=1)
    endif()

    foreach(ORION2RE_SANITIZER_TARGET IN ITEMS
            orion2re_lua orion2re orion2re-router orion2re_game_objects
            config-tool config-load-benchmark)
        if(TARGET ${ORION2RE_SANITIZER_TARGET})
            orion2re_enable_sanitizers(${ORION2RE_SANITIZER_TARGET})
        endif()
    endforeach()
endif()

if(ORION2RE_BUILD_TESTS)
    include("${ORION2RE_TESTS_DIR}/tests.cmake")
    add_executable(scaler-benchmark EXCLUDE_FROM_ALL
        tools/scaler_benchmark.cpp
        src/pch.cpp
    )
    target_compile_features(scaler-benchmark PRIVATE cxx_std_20)
    target_include_directories(scaler-benchmark PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_precompile_headers(scaler-benchmark PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/src/pch.h>"
    )
    target_link_libraries(scaler-benchmark PRIVATE SDL3::Headers orion2re_scalers)
    if(ORION2RE_BUILD_CONFIG_TOOL)
        foreach(ORION2RE_CONFIG_TEST_TARGET IN ITEMS
                test_config_enums
                test_lua_mod_runtime
                test_config_decode
                test_config_bindings
                test_config_lifecycle
                test_config_loader
                test_config_snapshot
                test_savegame)
            if(TARGET ${ORION2RE_CONFIG_TEST_TARGET})
                add_dependencies(${ORION2RE_CONFIG_TEST_TARGET} config-regenerate)
            endif()
        endforeach()
    endif()
endif()