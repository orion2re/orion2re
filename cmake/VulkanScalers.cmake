option(ORION2RE_ENABLE_VULKAN "Build Vulkan GPU presentation when the SDK is available" ON)
if(ORION2RE_ENABLE_VULKAN AND ORION2RE_BUILD_GAME AND NOT CMAKE_CROSSCOMPILING)
    find_package(Vulkan QUIET COMPONENTS glslangValidator)
    if(Vulkan_FOUND AND Vulkan_GLSLANG_VALIDATOR_EXECUTABLE)
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
        set(ORION2RE_VULKAN_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/vulkan")
        set(ORION2RE_VULKAN_SHADER "${ORION2RE_VULKAN_GENERATED_DIR}/scaler.comp")
        set(ORION2RE_VULKAN_SPV "${ORION2RE_VULKAN_GENERATED_DIR}/scaler.spv")
        set(ORION2RE_VULKAN_HEADER "${ORION2RE_VULKAN_GENERATED_DIR}/vulkan_scaler_spv.h")
        add_custom_command(OUTPUT "${ORION2RE_VULKAN_SHADER}"
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_scaler_shader.py"
                "${CMAKE_CURRENT_SOURCE_DIR}/src/game/scalers" "${ORION2RE_VULKAN_SHADER}"
            DEPENDS tools/generate_scaler_shader.py src/game/scalers/hqx.cpp src/game/scalers/mmpx.cpp
            VERBATIM)
        add_custom_command(OUTPUT "${ORION2RE_VULKAN_SPV}"
            COMMAND "${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE}" -V --target-env vulkan1.0
                "${ORION2RE_VULKAN_SHADER}" -o "${ORION2RE_VULKAN_SPV}"
            DEPENDS "${ORION2RE_VULKAN_SHADER}" VERBATIM)
        add_custom_command(OUTPUT "${ORION2RE_VULKAN_HEADER}"
            COMMAND "${CMAKE_COMMAND}" "-DINPUT_FILE=${ORION2RE_VULKAN_SPV}"
                "-DOUTPUT_FILE=${ORION2RE_VULKAN_HEADER}" "-DSYMBOL_NAME=k_vulkan_scaler"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_binary.cmake"
            DEPENDS "${ORION2RE_VULKAN_SPV}" VERBATIM)
        set(ORION2RE_PALETTE_SPV "${ORION2RE_VULKAN_GENERATED_DIR}/palette.spv")
        set(ORION2RE_PALETTE_HEADER "${ORION2RE_VULKAN_GENERATED_DIR}/vulkan_palette_spv.h")
        add_custom_command(OUTPUT "${ORION2RE_PALETTE_SPV}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${ORION2RE_VULKAN_GENERATED_DIR}"
            COMMAND "${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE}" -V --target-env vulkan1.0
                "${CMAKE_CURRENT_SOURCE_DIR}/src/game/scalers/palette.comp" -o "${ORION2RE_PALETTE_SPV}"
            DEPENDS src/game/scalers/palette.comp VERBATIM)
        add_custom_command(OUTPUT "${ORION2RE_PALETTE_HEADER}"
            COMMAND "${CMAKE_COMMAND}" "-DINPUT_FILE=${ORION2RE_PALETTE_SPV}"
                "-DOUTPUT_FILE=${ORION2RE_PALETTE_HEADER}" "-DSYMBOL_NAME=k_vulkan_palette"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_binary.cmake"
            DEPENDS "${ORION2RE_PALETTE_SPV}" VERBATIM)
        add_library(orion2re_vulkan_scalers STATIC
            src/game/scalers/vulkan_scaler.cpp "${ORION2RE_VULKAN_HEADER}" "${ORION2RE_PALETTE_HEADER}")
        target_compile_features(orion2re_vulkan_scalers PRIVATE cxx_std_20)
        target_include_directories(orion2re_vulkan_scalers PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src"
            PRIVATE "${ORION2RE_VULKAN_GENERATED_DIR}")
        target_link_libraries(orion2re_vulkan_scalers PUBLIC SDL3::SDL3-static)
        if(ORION2RE_ENABLE_ASAN)
            if(MSVC)
                target_compile_options(orion2re_vulkan_scalers PRIVATE /fsanitize=address)
                target_link_options(orion2re_vulkan_scalers INTERFACE /INFERASANLIBS /fsanitize=address)
            elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
                target_compile_options(orion2re_vulkan_scalers PRIVATE -fsanitize=address -fno-omit-frame-pointer)
                target_link_options(orion2re_vulkan_scalers INTERFACE -fsanitize=address)
            endif()
        endif()
        add_executable(vulkan-scaler-check EXCLUDE_FROM_ALL tools/vulkan_scaler_check.cpp)
        target_compile_features(vulkan-scaler-check PRIVATE cxx_std_20)
        target_link_libraries(vulkan-scaler-check PRIVATE orion2re_vulkan_scalers orion2re_scalers)
        add_executable(vulkan-scaler-profile EXCLUDE_FROM_ALL tools/vulkan_scaler_profile.cpp
            "${ORION2RE_VULKAN_HEADER}" "${ORION2RE_PALETTE_HEADER}")
        target_compile_features(vulkan-scaler-profile PRIVATE cxx_std_20)
        target_include_directories(vulkan-scaler-profile PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${ORION2RE_VULKAN_GENERATED_DIR}")
        target_link_libraries(vulkan-scaler-profile PRIVATE SDL3::SDL3-static Vulkan::Vulkan orion2re_scalers)
        message(STATUS "Vulkan GPU presentation enabled")
    else()
        message(STATUS "Vulkan SDK/glslangValidator unavailable; using SDL presentation")
    endif()
endif()
