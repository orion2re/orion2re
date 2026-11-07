file(READ "${CMAKE_CURRENT_LIST_DIR}/../VERSION" ORION2RE_VERSION)
string(STRIP "${ORION2RE_VERSION}" ORION2RE_VERSION)
if(NOT ORION2RE_VERSION MATCHES "^(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)$")
    message(FATAL_ERROR "VERSION must contain a major.minor.patch version without leading zeroes.")
endif()

if(DEFINED RELEASE_TAG AND NOT RELEASE_TAG STREQUAL "v${ORION2RE_VERSION}")
    message(FATAL_ERROR "Release tag '${RELEASE_TAG}' does not match VERSION; expected 'v${ORION2RE_VERSION}'.")
endif()