# Production sources have one unit-test compilation environment. Test-only
# definitions and fakes stay on the executables and must not affect these objects.
add_library(orion2re_test_compile_settings INTERFACE)
target_compile_features(orion2re_test_compile_settings INTERFACE cxx_std_20)
target_include_directories(orion2re_test_compile_settings INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${ORION2RE_VENDORED_LIBSMACKER_DIR}"
    "${ORION2RE_VENDORED_YYJSON_DIR}/src"
)
target_link_libraries(orion2re_test_compile_settings INTERFACE
    SDL3::Headers uv_a orion2re_lz4 orion2re_lua miniz
)
if(ORION2RE_ENABLE_ASAN)
    target_compile_options(orion2re_test_compile_settings INTERFACE
        ${ORION2RE_SANITIZER_COMPILE_OPTIONS})
endif()
if(MSVC)
    target_compile_options(orion2re_test_compile_settings INTERFACE /W3 /permissive- /Gy /Gw)
    target_compile_definitions(orion2re_test_compile_settings INTERFACE
        WIN32
        $<$<CONFIG:Debug>:_CRT_SECURE_NO_DEPRECATE>
        $<$<CONFIG:Debug>:_CRT_NONSTDC_NO_DEPRECATE>
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    # Keep a consistent section layout for optional dead-code elimination.
    # Dependency closure must also pass the no-dead-stripping link audit.
    target_compile_options(orion2re_test_compile_settings INTERFACE
        -ffunction-sections -fdata-sections)
endif()

add_library(orion2re_test_pch OBJECT "${CMAKE_CURRENT_SOURCE_DIR}/src/pch.cpp")
target_link_libraries(orion2re_test_pch PRIVATE orion2re_test_compile_settings)
target_precompile_headers(orion2re_test_pch PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/src/pch.h>"
)
if(ORION2RE_BUILD_CONFIG_TOOL)
    add_dependencies(orion2re_test_pch config-regenerate)
endif()

function(orion2re_test_shared_sources OUTPUT_VARIABLE)
    set(RESULT)
    foreach(SOURCE IN LISTS ARGN)
        if(SOURCE MATCHES "^\\$<")
            list(APPEND RESULT "${SOURCE}")
            continue()
        endif()
        get_filename_component(ABSOLUTE_SOURCE "${SOURCE}" ABSOLUTE
            BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        file(RELATIVE_PATH RELATIVE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}" "${ABSOLUTE_SOURCE}")
        if(NOT RELATIVE_SOURCE MATCHES "^src/")
            list(APPEND RESULT "${SOURCE}")
            continue()
        endif()
        if(RELATIVE_SOURCE STREQUAL "src/pch.cpp")
            set(OBJECT_TARGET orion2re_test_pch)
        else()
            string(MAKE_C_IDENTIFIER "${RELATIVE_SOURCE}" SOURCE_ID)
            set(OBJECT_TARGET "orion2re_test_object_${SOURCE_ID}")
            if(NOT TARGET ${OBJECT_TARGET})
                add_library(${OBJECT_TARGET} OBJECT "${ABSOLUTE_SOURCE}")
                target_link_libraries(${OBJECT_TARGET} PRIVATE orion2re_test_compile_settings)
                target_precompile_headers(${OBJECT_TARGET} REUSE_FROM orion2re_test_pch)
                if(ORION2RE_BUILD_CONFIG_TOOL)
                    add_dependencies(${OBJECT_TARGET} config-regenerate)
                endif()
            else()
                get_target_property(EXISTING_SOURCE ${OBJECT_TARGET} SOURCES)
                if(NOT EXISTING_SOURCE STREQUAL ABSOLUTE_SOURCE)
                    message(FATAL_ERROR "Shared test object name collision: ${RELATIVE_SOURCE}")
                endif()
            endif()
        endif()
        list(APPEND RESULT "$<TARGET_OBJECTS:${OBJECT_TARGET}>")
    endforeach()
    set(${OUTPUT_VARIABLE} "${RESULT}" PARENT_SCOPE)
endfunction()