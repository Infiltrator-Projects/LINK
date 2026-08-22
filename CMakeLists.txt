# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.20)

# The Windows Discover executables are distributed as standalone files. Select
# the static MSVC runtime through the documented CMake abstraction so a clean
# Windows installation does not need a matching Visual C++ redistributable.
if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

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
    src/core/diagnostic_flow.c
    src/core/isotp.c
    src/core/parameter.c
    src/core/scheduler.c
    src/core/telemetry.c
    src/core/transport.c
    src/elm327/elm327.c
    src/elm327/can.c
    src/elm327/probe.c
    src/elm327/session.c
    src/obd2/obd2.c
    src/uds/uds.c
    src/discover/safety.c
    src/discover/evidence.c
)
add_library(LINK::Core ALIAS link-core)

target_include_directories(link-core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(link-core PUBLIC InfiltratrCommon::Portable)
if(MSVC)
    target_compile_options(link-core PRIVATE /W4)
else()
    target_compile_options(link-core PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Add the shared native Windows Discover shell. Product repositories supply
# identity only; the J2534 workflow, menu model, About behaviour, launch model
# and layout stay in LINK so MBLINK and JAGLINK cannot silently diverge.
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
            PRODUCT_SUBTITLE
            PRODUCT_WEBSITE
            PRODUCT_ICON_PNG)
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
    if(NOT LINK_FACE_PRODUCT_WEBSITE)
        set(LINK_FACE_PRODUCT_WEBSITE "https://github.com/The-First-Infiltrator/LINK")
    endif()

    add_executable(${target} WIN32
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/platform/windows/link-discover-ui.c")
    target_link_libraries(${target} PRIVATE
        LINK::Core
        comdlg32
        advapi32
        shell32
        comctl32)
    target_compile_definitions(${target} PRIVATE
        _CRT_SECURE_NO_WARNINGS
        LINK_PRODUCT_NAME="${LINK_FACE_PRODUCT_NAME}"
        LINK_PRODUCT_SLUG="${LINK_FACE_PRODUCT_SLUG}"
        LINK_PRODUCT_WINDOW_CLASS="${LINK_FACE_WINDOW_CLASS}"
        LINK_PRODUCT_VERSION="${LINK_FACE_PRODUCT_VERSION}"
        LINK_PRODUCT_COPYRIGHT="${LINK_FACE_PRODUCT_COPYRIGHT}"
        LINK_PRODUCT_SUBTITLE="${LINK_FACE_PRODUCT_SUBTITLE}"
        LINK_PRODUCT_WEBSITE="${LINK_FACE_PRODUCT_WEBSITE}")

    if(MSVC)
        set_property(TARGET ${target} PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        target_compile_options(${target} PRIVATE /W4 /utf-8)
    endif()

    # The product's iPhone AppIcon PNG is the canonical identity image. Windows
    # needs an ICO resource, so LINK wraps those exact PNG bytes in an ICO
    # container at build time rather than maintaining a second artwork source.
    if(LINK_FACE_PRODUCT_ICON_PNG)
        if(NOT EXISTS "${LINK_FACE_PRODUCT_ICON_PNG}")
            message(FATAL_ERROR "PRODUCT_ICON_PNG does not exist: ${LINK_FACE_PRODUCT_ICON_PNG}")
        endif()

        find_program(LINK_POWERSHELL_EXECUTABLE
            NAMES powershell.exe powershell REQUIRED)
        set(generated_icon "${CMAKE_CURRENT_BINARY_DIR}/${target}-product.ico")
        set(generated_rc "${CMAKE_CURRENT_BINARY_DIR}/${target}-product.rc")

        add_custom_command(
            OUTPUT "${generated_icon}"
            COMMAND "${LINK_POWERSHELL_EXECUTABLE}" -NoProfile -ExecutionPolicy Bypass
                -File "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/platform/windows/png-to-ico.ps1"
                -InputPng "${LINK_FACE_PRODUCT_ICON_PNG}"
                -OutputIco "${generated_icon}"
            DEPENDS
                "${LINK_FACE_PRODUCT_ICON_PNG}"
                "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/platform/windows/png-to-ico.ps1"
            VERBATIM)

        set(version_major 0)
        set(version_minor 0)
        set(version_patch 0)
        if(LINK_FACE_PRODUCT_VERSION MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
            set(version_major "${CMAKE_MATCH_1}")
            set(version_minor "${CMAKE_MATCH_2}")
            set(version_patch "${CMAKE_MATCH_3}")
        endif()

        file(WRITE "${generated_rc}"
            "1 ICON \"${generated_icon}\"\n"
            "1 VERSIONINFO\n"
            "FILEVERSION ${version_major},${version_minor},${version_patch},0\n"
            "PRODUCTVERSION ${version_major},${version_minor},${version_patch},0\n"
            "FILEFLAGSMASK 0x3fL\n"
            "FILEFLAGS 0x0L\n"
            "FILEOS 0x40004L\n"
            "FILETYPE 0x1L\n"
            "FILESUBTYPE 0x0L\n"
            "BEGIN\n"
            "  BLOCK \"StringFileInfo\"\n"
            "  BEGIN\n"
            "    BLOCK \"040904b0\"\n"
            "    BEGIN\n"
            "      VALUE \"CompanyName\", \"The First Infiltrator\"\n"
            "      VALUE \"FileDescription\", \"${LINK_FACE_PRODUCT_NAME} Discover\"\n"
            "      VALUE \"FileVersion\", \"${LINK_FACE_PRODUCT_VERSION}\"\n"
            "      VALUE \"InternalName\", \"${LINK_FACE_PRODUCT_SLUG}-discover\"\n"
            "      VALUE \"LegalCopyright\", \"${LINK_FACE_PRODUCT_COPYRIGHT}\"\n"
            "      VALUE \"OriginalFilename\", \"${LINK_FACE_PRODUCT_NAME}-${LINK_FACE_PRODUCT_VERSION}-windows-discover.exe\"\n"
            "      VALUE \"ProductName\", \"${LINK_FACE_PRODUCT_NAME}\"\n"
            "      VALUE \"ProductVersion\", \"${LINK_FACE_PRODUCT_VERSION}\"\n"
            "    END\n"
            "  END\n"
            "  BLOCK \"VarFileInfo\"\n"
            "  BEGIN\n"
            "    VALUE \"Translation\", 0x0409, 1200\n"
            "  END\n"
            "END\n")
        set_source_files_properties("${generated_rc}" PROPERTIES
            OBJECT_DEPENDS "${generated_icon}")
        target_sources(${target} PRIVATE "${generated_rc}")
    endif()

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
        WINDOW_CLASS "LINKDiscoverWindow"
        PRODUCT_VERSION "${PROJECT_VERSION}"
        PRODUCT_COPYRIGHT "Copyright (C) 2026 The First Infiltrator"
        PRODUCT_SUBTITLE "Shared Vehicle Diagnostics"
        PRODUCT_WEBSITE "https://github.com/The-First-Infiltrator/LINK")
    if(LINK_BUILD_REFERENCE_PRODUCT_FACES)
        link_add_windows_discover(mblink-discover
            PRODUCT_NAME "MBLINK"
            PRODUCT_SLUG "mblink"
            WINDOW_CLASS "MBLINKDiscoverWindow"
            PRODUCT_VERSION "${PROJECT_VERSION}"
            PRODUCT_COPYRIGHT "Copyright (C) 2026 Shannon Smith"
            PRODUCT_SUBTITLE "Mercedes-Benz Diagnostics"
            PRODUCT_WEBSITE "https://github.com/The-First-Infiltrator/MBLINK")
        link_add_windows_discover(jaglink-discover
            PRODUCT_NAME "JAGLINK"
            PRODUCT_SLUG "jaglink"
            WINDOW_CLASS "JAGLINKDiscoverWindow"
            PRODUCT_VERSION "${PROJECT_VERSION}"
            PRODUCT_COPYRIGHT "Copyright (C) 2026 Xavier Wheaton and Shannon Smith"
            PRODUCT_SUBTITLE "Jaguar X400 Diagnostics"
            PRODUCT_WEBSITE "https://github.com/The-First-Infiltrator/Jaglink")
    endif()
endif()

if(BUILD_TESTING AND PROJECT_IS_TOP_LEVEL)
    add_executable(link-test-workspace tests/test_workspace.c)
    target_link_libraries(link-test-workspace PRIVATE LINK::Core)
    add_test(NAME link-workspace COMMAND link-test-workspace)

    add_executable(link-test-diagnostic-flow tests/test_diagnostic_flow.c)
    target_link_libraries(link-test-diagnostic-flow PRIVATE LINK::Core)
    add_test(NAME link-diagnostic-flow COMMAND link-test-diagnostic-flow)

    add_executable(link-test-isotp tests/test_isotp.c)
    target_link_libraries(link-test-isotp PRIVATE LINK::Core)
    add_test(NAME link-isotp COMMAND link-test-isotp)

    add_executable(link-test-runtime tests/test_runtime.c)
    target_link_libraries(link-test-runtime PRIVATE LINK::Core)
    add_test(NAME link-runtime COMMAND link-test-runtime)

    add_executable(link-test-elm327 tests/test_elm327.c)
    target_link_libraries(link-test-elm327 PRIVATE LINK::Core)
    add_test(NAME link-elm327 COMMAND link-test-elm327)

    add_executable(link-test-obd2 tests/test_obd2.c)
    target_link_libraries(link-test-obd2 PRIVATE LINK::Core)
    add_test(NAME link-obd2 COMMAND link-test-obd2)

    add_executable(link-test-uds tests/test_uds.c)
    target_link_libraries(link-test-uds PRIVATE LINK::Core)
    add_test(NAME link-uds COMMAND link-test-uds)

    add_executable(link-test-discover-safety tests/test_discover_safety.c)
    target_link_libraries(link-test-discover-safety PRIVATE LINK::Core)
    add_test(NAME link-discover-safety COMMAND link-test-discover-safety)

    add_executable(link-test-evidence tests/test_evidence.c)
    target_link_libraries(link-test-evidence PRIVATE LINK::Core)
    add_test(NAME link-evidence COMMAND link-test-evidence)
endif()
