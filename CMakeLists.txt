# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.20)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" LINK_VERSION LIMIT_COUNT 1)
project(LINK VERSION ${LINK_VERSION} LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include(CTest)
include(CMakeParseArguments)

if(NOT TARGET InfiltratrCommon::Portable)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/infiltratr-common/CMakeLists.txt")
        message(FATAL_ERROR "Infiltratr Common is required. Initialise src/infiltratr-common or provide InfiltratrCommon::Portable from the parent project.")
    endif()
    add_subdirectory(src/infiltratr-common)
endif()

add_library(link-core STATIC
    src/core/workspace.c
    src/core/isotp.c
    src/core/parameter.c
    src/core/scheduler.c
    src/core/telemetry.c
    src/core/transport.c
    src/elm327/elm327.c
    src/elm327/can.c
    src/elm327/probe.c
    src/elm327/session.c
    src/discover/safety.c
    src/discover/evidence.c
)
add_library(LINK::Core ALIAS link-core)

target_include_directories(link-core PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>)
target_link_libraries(link-core PUBLIC InfiltratrCommon::Portable)
if(MSVC)
    target_compile_options(link-core PRIVATE /W4)
else()
    target_compile_options(link-core PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Add the shared native Windows Discover shell. Product repositories supply
# identity only; the J2534 workflow, menu model, About behaviour and layout stay
# in LINK so MBLINK and JAGLINK cannot silently diverge again.
function(link_add_windows_discover target)
    if(NOT WIN32)
        message(FATAL_ERROR "link_add_windows_discover is supported on Windows only.")
    endif()

    if(ARGC EQUAL 4 AND NOT ARGV1 STREQUAL "PRODUCT_NAME")
        # Legacy positional form retained for source compatibility.
        set(LINK_FACE_PRODUCT_NAME "${ARGV1}")
        set(LINK_FACE_PRODUCT_SLUG "${ARGV2}")
        set(LINK_FACE_WINDOW_CLASS "${ARGV3}")
    else()
        set(one_value_args
            PRODUCT_NAME
            PRODUCT_SLUG
            WINDOW_CLASS
            PRODUCT_VERSION
            PRODUCT_COPYRIGHT
            PRODUCT_SUBTITLE)
        cmake_parse_arguments(LINK_FACE "" "${one_value_args}" "" ${ARGN})
        if(LINK_FACE_UNPARSED_ARGUMENTS)
            message(FATAL_ERROR "Unknown link_add_windows_discover arguments: ${LINK_FACE_UNPARSED_ARGUMENTS}")
        endif()
    endif()

    foreach(required PRODUCT_NAME PRODUCT_SLUG WINDOW_CLASS)
        if(NOT LINK_FACE_${required})
            message(FATAL_ERROR "link_add_windows_discover requires ${required}.")
        endif()
    endforeach()

    if(NOT LINK_FACE_PRODUCT_VERSION)
        set(LINK_FACE_PRODUCT_VERSION "unknown")
    endif()
    if(NOT LINK_FACE_PRODUCT_COPYRIGHT)
        set(LINK_FACE_PRODUCT_COPYRIGHT "Copyright (C) 2026 The First Infiltrator")
    endif()
    if(NOT LINK_FACE_PRODUCT_SUBTITLE)
        set(LINK_FACE_PRODUCT_SUBTITLE "Vehicle Diagnostics")
    endif()

    add_executable(${target} WIN32
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/platform/windows/link-discover-ui.c")
    target_link_libraries(${target} PRIVATE LINK::Core comdlg32 advapi32 shell32)
    target_compile_definitions(${target} PRIVATE
        _CRT_SECURE_NO_WARNINGS
        LINK_PRODUCT_NAME="${LINK_FACE_PRODUCT_NAME}"
        LINK_PRODUCT_SLUG="${LINK_FACE_PRODUCT_SLUG}"
        LINK_PRODUCT_WINDOW_CLASS="${LINK_FACE_WINDOW_CLASS}"
        LINK_PRODUCT_VERSION="${LINK_FACE_PRODUCT_VERSION}"
        LINK_PRODUCT_COPYRIGHT="${LINK_FACE_PRODUCT_COPYRIGHT}"
        LINK_PRODUCT_SUBTITLE="${LINK_FACE_PRODUCT_SUBTITLE}")
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /utf-8)
    endif()
    set_target_properties(${target} PROPERTIES OUTPUT_NAME "${LINK_FACE_PRODUCT_SLUG}-discover")
endfunction()

option(LINK_BUILD_REFERENCE_PRODUCT_FACES "Build MBLINK and JAGLINK reference Windows Discover faces when LINK is top-level" OFF)
if(WIN32 AND PROJECT_IS_TOP_LEVEL)
    link_add_windows_discover(link-discover
        PRODUCT_NAME "LINK"
        PRODUCT_SLUG "link"
        WINDOW_CLASS "LINKDiscoverWindow"
        PRODUCT_VERSION "${PROJECT_VERSION}"
        PRODUCT_COPYRIGHT "Copyright (C) 2026 The First Infiltrator"
        PRODUCT_SUBTITLE "Shared Vehicle Diagnostics")
    if(LINK_BUILD_REFERENCE_PRODUCT_FACES)
        link_add_windows_discover(mblink-discover
            PRODUCT_NAME "MBLINK"
            PRODUCT_SLUG "mblink"
            WINDOW_CLASS "MBLINKDiscoverWindow"
            PRODUCT_VERSION "${PROJECT_VERSION}"
            PRODUCT_COPYRIGHT "Copyright (C) 2026 Shannon Smith"
            PRODUCT_SUBTITLE "Mercedes-Benz Diagnostics")
        link_add_windows_discover(jaglink-discover
            PRODUCT_NAME "JAGLINK"
            PRODUCT_SLUG "jaglink"
            WINDOW_CLASS "JAGLINKDiscoverWindow"
            PRODUCT_VERSION "${PROJECT_VERSION}"
            PRODUCT_COPYRIGHT "Copyright (C) 2026 Xavier Wheaton and Shannon Smith"
            PRODUCT_SUBTITLE "Jaguar X400 Diagnostics")
    endif()
endif()

if(BUILD_TESTING AND PROJECT_IS_TOP_LEVEL)
    add_executable(link-test-workspace tests/test_workspace.c)
    target_link_libraries(link-test-workspace PRIVATE LINK::Core)
    add_test(NAME link-workspace COMMAND link-test-workspace)
    add_executable(link-test-isotp tests/test_isotp.c)
    target_link_libraries(link-test-isotp PRIVATE LINK::Core)
    add_test(NAME link-isotp COMMAND link-test-isotp)
    add_executable(link-test-runtime tests/test_runtime.c)
    target_link_libraries(link-test-runtime PRIVATE LINK::Core)
    add_test(NAME link-runtime COMMAND link-test-runtime)
    add_executable(link-test-elm327 tests/test_elm327.c)
    target_link_libraries(link-test-elm327 PRIVATE LINK::Core)
    add_test(NAME link-elm327 COMMAND link-test-elm327)
    add_executable(link-test-discover-safety tests/test_discover_safety.c)
    target_link_libraries(link-test-discover-safety PRIVATE LINK::Core)
    add_test(NAME link-discover-safety COMMAND link-test-discover-safety)
    add_executable(link-test-evidence tests/test_evidence.c)
    target_link_libraries(link-test-evidence PRIVATE LINK::Core)
    add_test(NAME link-evidence COMMAND link-test-evidence)
endif()
