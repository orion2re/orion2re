set(ORION2RE_RPC_BACKEND_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendors/libwebsockets" CACHE PATH
    "Checkout of the pinned libwebsockets backend used only by RPC-enabled game builds")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/specs/rpc/backend.json" ORION2RE_RPC_BACKEND_PROFILE)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/specs/rpc/backend.json")
string(JSON ORION2RE_RPC_BACKEND_REVISION GET "${ORION2RE_RPC_BACKEND_PROFILE}" revision)
if(NOT EXISTS "${ORION2RE_RPC_BACKEND_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "RPC requires libwebsockets ${ORION2RE_RPC_BACKEND_REVISION}. Set ORION2RE_RPC_BACKEND_DIR to that checkout. Disabled and router-only builds do not need it.")
endif()
find_package(Git REQUIRED)
execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${ORION2RE_RPC_BACKEND_DIR}" rev-parse HEAD
    OUTPUT_VARIABLE ORION2RE_RPC_ACTUAL_REVISION OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE ORION2RE_RPC_GIT_RESULT)
if(NOT ORION2RE_RPC_GIT_RESULT EQUAL 0 OR NOT ORION2RE_RPC_ACTUAL_REVISION STREQUAL ORION2RE_RPC_BACKEND_REVISION)
    message(FATAL_ERROR "RPC backend revision does not match specs/rpc/backend.json")
endif()
execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${ORION2RE_RPC_BACKEND_DIR}" diff --quiet HEAD
    RESULT_VARIABLE ORION2RE_RPC_DIRTY_RESULT)
if(NOT ORION2RE_RPC_DIRTY_RESULT EQUAL 0)
    message(FATAL_ERROR "RPC backend checkout must have no tracked modifications")
endif()
string(JSON ORION2RE_RPC_OPTION_COUNT LENGTH "${ORION2RE_RPC_BACKEND_PROFILE}" cmake)
math(EXPR ORION2RE_RPC_OPTION_LAST "${ORION2RE_RPC_OPTION_COUNT} - 1")
foreach(ORION2RE_RPC_INDEX RANGE 0 ${ORION2RE_RPC_OPTION_LAST})
    string(JSON ORION2RE_RPC_OPTION MEMBER "${ORION2RE_RPC_BACKEND_PROFILE}" cmake ${ORION2RE_RPC_INDEX})
    string(JSON ORION2RE_RPC_VALUE GET "${ORION2RE_RPC_BACKEND_PROFILE}" cmake "${ORION2RE_RPC_OPTION}")
    set(${ORION2RE_RPC_OPTION} "${ORION2RE_RPC_VALUE}" CACHE BOOL "Pinned RPC backend option" FORCE)
endforeach()
set(LWS_LIBUV_INCLUDE_DIRS "${ORION2RE_VENDORED_LIBUV_DIR}/include" CACHE PATH "" FORCE)
set(LWS_LIBUV_LIBRARIES uv_a CACHE STRING "Use the project's pinned libuv target" FORCE)
add_subdirectory("${ORION2RE_RPC_BACKEND_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/rpc-backend" EXCLUDE_FROM_ALL)
if(MSVC)
    # libwebsockets' ssize_t macro makes libuv repeat the Windows SSIZE_T typedef.
    target_compile_options(websockets PRIVATE /wd4142)
endif()
configure_file("${ORION2RE_RPC_BACKEND_DIR}/LICENSE"
    "${CMAKE_CURRENT_BINARY_DIR}/data/licenses/libwebsockets.txt" COPYONLY)
find_package(Threads REQUIRED)

add_library(orion2re_rpc STATIC
    src/rpc/rpc_json.cpp
    src/rpc/rpc_protocol.cpp
    src/rpc/generated/rpc_contract_data.cpp
    src/rpc/rpc_bridge.cpp
    src/rpc/rpc_output.cpp
    src/rpc/rpc_service.cpp
    src/rpc/rpc_game.cpp
    "${ORION2RE_VENDORED_YYJSON_DIR}/src/yyjson.c"
)
target_compile_features(orion2re_rpc PUBLIC cxx_std_20)
if(WIN32)
    target_compile_definitions(orion2re_rpc PRIVATE NOMINMAX)
endif()
target_include_directories(orion2re_rpc PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_include_directories(orion2re_rpc PRIVATE
    "${ORION2RE_VENDORED_YYJSON_DIR}/src"
    "${ORION2RE_RPC_BACKEND_DIR}/include"
    "${CMAKE_CURRENT_BINARY_DIR}/rpc-backend"
    "${ORION2RE_VENDORED_LIBUV_DIR}/include")
target_link_libraries(orion2re_rpc PRIVATE websockets uv_a Threads::Threads)
orion2re_enable_sanitizers(orion2re_rpc)
orion2re_enable_sanitizers(websockets)
set(ORION2RE_RPC_BUILT ON)

if(NOT CMAKE_CROSSCOMPILING)
    find_package(Python3 3.10 QUIET COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/rpc-contract.stamp"
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_rpc_schema.py"
            COMMAND "${CMAKE_COMMAND}" -E touch "${CMAKE_CURRENT_BINARY_DIR}/rpc-contract.stamp"
            DEPENDS specs/rpc/methods.json specs/rpc/contracts.json specs/rpc/field_policies.json
                tools/generate_rpc_schema.py tools/audit_rpc_contract.py
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            VERBATIM)
        add_custom_target(rpc-regenerate DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/rpc-contract.stamp")
        add_dependencies(orion2re_rpc rpc-regenerate)
    endif()
endif()

add_executable(rpc-test-server EXCLUDE_FROM_ALL tools/rpc_test_server.cpp)
target_include_directories(rpc-test-server PRIVATE "${ORION2RE_VENDORED_YYJSON_DIR}/src")
target_link_libraries(rpc-test-server PRIVATE orion2re_rpc)
orion2re_enable_sanitizers(rpc-test-server)
