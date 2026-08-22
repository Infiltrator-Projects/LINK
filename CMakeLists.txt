# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.20)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" LINK_VERSION LIMIT_COUNT 1)
project(LINK VERSION ${LINK_VERSION} LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include(CTest)
include(CMakeParseArguments)

# When LINK is embedded by MBLINK/JAGLINK, reuse the parent's already-loaded
# Infiltratr Common target. Standalone LINK builds use the pinned local
# submodule instead. This keeps one Common implementation in the final graph.
if(NOT TARGET InfiltratrCommon::Portable)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/infiltratr-common/CMakeLists.txt")
        message(FATAL_ERROR "Infiltratr Common is required. Initialise src/infiltratr-common or provide InfiltratrCommon::Portable from the parent project.")
    endif()
    add_subdirectory(src/infiltratr-common)
endif()

add_library(link-core STATIC
    src/core/workspace.c
    src/discover/safety.c
    src/discover/evidence.c
)
add_library(LINK::Core ALIAS link-core)

target_include_directories(link-core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(link-core
    PUBLIC
        InfiltratrCommon::Portable
)

if(MSVC)
    target_compile_options(link-core PRIVATE /W4)
else()
    target_compile_options(link-core PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Build one shared Windows Discover/OpenPort implementation with a product face.
# The product repositories supply identity only; they never own a second scanner
# source file. This function is the sole supported constructor for product
# Windows Discover targets.
function(link_add_windows_discover target)
    if(NOT WIN32)
        message(FATAL_ERROR "link_add_windows_discover is supported on Windows only.")
    endif()

    set(one_value_args PRODUCT_NAME PRODUCT_SLUG WINDOW_CLASS)
    cmake_parse_arguments(LINK_FACE "" "${one_value_args}" "" ${ARGN})

    if(LINK_FACE_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "Unknown link_add_windows_discover arguments: ${LINK_FACE_UNPARSED_ARGUMENTS}")
    endif()

    foreach(required PRODUCT_NAME PRODUCT_SLUG WINDOW_CLASS)
        if(NOT LINK_FACE_${required})
            message(FATAL_ERROR
                "link_add_windows_discover requires ${required}.")
        endif()
    endforeach()

    add_executable(${target} WIN32
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/platform/windows/link-discover.c")
    target_link_libraries(${target} PRIVATE LINK::Core comdlg32 advapi32)
    target_compile_definitions(${target} PRIVATE
        _CRT_SECURE_NO_WARNINGS
        LINK_PRODUCT_NAME="${LINK_FACE_PRODUCT_NAME}"
        LINK_PRODUCT_SLUG="${LINK_FACE_PRODUCT_SLUG}"
        LINK_PRODUCT_WINDOW_CLASS="${LINK_FACE_WINDOW_CLASS}")
    set_target_properties(${target} PROPERTIES
        OUTPUT_NAME "${LINK_FACE_PRODUCT_SLUG}-discover")
endfunction()

option(LINK_BUILD_REFERENCE_PRODUCT_FACES
    "Build MBLINK and JAGLINK reference Windows Discover faces when LINK is top-level"
    OFF)

if(WIN32 AND PROJECT_IS_TOP_LEVEL)
    link_add_windows_discover(link-discover
        PRODUCT_NAME "LINK"
        PRODUCT_SLUG "link"
        WINDOW_CLASS "LINKDiscoverWindow")

    if(LINK_BUILD_REFERENCE_PRODUCT_FACES)
        link_add_windows_discover(mblink-discover
            PRODUCT_NAME "MBLINK"
            PRODUCT_SLUG "mblink"
            WINDOW_CLASS "MBLINKDiscoverWindow")
        link_add_windows_discover(jaglink-discover
            PRODUCT_NAME "JAGLINK"
            PRODUCT_SLUG "jaglink"
            WINDOW_CLASS "JAGLINKDiscoverWindow")
    endif()
endif()

if(BUILD_TESTING AND PROJECT_IS_TOP_LEVEL)
    add_executable(link-test-workspace tests/test_workspace.c)
    target_link_libraries(link-test-workspace PRIVATE LINK::Core)
    add_test(NAME link-workspace COMMAND link-test-workspace)

    add_executable(link-test-discover-safety tests/test_discover_safety.c)
    target_link_libraries(link-test-discover-safety PRIVATE LINK::Core)
    add_test(NAME link-discover-safety COMMAND link-test-discover-safety)

    add_executable(link-test-evidence tests/test_evidence.c)
    target_link_libraries(link-test-evidence PRIVATE LINK::Core)
    add_test(NAME link-evidence COMMAND link-test-evidence)
endif()
