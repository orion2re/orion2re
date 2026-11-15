if(MSVC)
    string(REPLACE "/Ob0" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    string(REPLACE "/Od" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
endif()

add_library(orion2re_scalers STATIC ${ORION2RE_SCALER_SOURCES})
target_compile_features(orion2re_scalers PRIVATE cxx_std_20)
target_include_directories(orion2re_scalers PRIVATE
    "${PROJECT_SOURCE_DIR}/src"
    ${ORION2RE_LIBSMACKER_INCLUDE_DIRS}
)
target_link_libraries(orion2re_scalers PRIVATE SDL3::Headers)

if(ORION2RE_LITTLE_ENDIAN_COMPILE_FLAG)
    target_compile_options(orion2re_scalers PRIVATE
        "${ORION2RE_LITTLE_ENDIAN_COMPILE_FLAG}"
    )
endif()

if(MSVC)
    target_compile_options(orion2re_scalers PRIVATE /W3 /permissive- /Ox)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(orion2re_scalers PRIVATE -O3)
endif()

if(ORION2RE_ENABLE_ASAN)
    if(MSVC)
        target_compile_options(orion2re_scalers PRIVATE /fsanitize=address)
        target_link_options(orion2re_scalers INTERFACE
            /INFERASANLIBS /fsanitize=address
        )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(orion2re_scalers PRIVATE
            -fsanitize=address -fno-omit-frame-pointer
        )
        target_link_options(orion2re_scalers INTERFACE -fsanitize=address)
    endif()
endif()