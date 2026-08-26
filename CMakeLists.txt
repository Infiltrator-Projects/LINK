# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.20)

if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" LINK_PROJECT_VERSION)
string(STRIP "${LINK_PROJECT_VERSION}" LINK_PROJECT_VERSION)
if(NOT LINK_PROJECT_VERSION MATCHES "^[0-9]+\.[0-9]+\.[0-9]+$")
    message(FATAL_ERROR "VERSION must contain a semantic major.minor.patch version.")
endif()
project(LINK VERSION "${LINK_PROJECT_VERSION}" LANGUAGES C)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)
include(CMakeParseArguments)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(LINK_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(LINK_BUILD_REFERENCE_PRODUCT_FACES
    "Build MBLINK and JAGLINK reference Windows Discover faces when LINK is top-level"
    OFF)
option(LINK_ENABLE_INSTALL
    "Install LINK headers, library and CMake package when LINK is top-level"
    ${PROJECT_IS_TOP_LEVEL})

set(LINK_COMMON_VERSION "1.13.0")
set(LINK_COMMON_COMMIT "1b2e258bd576e86968261463375d144a8874bd01")
set(LINK_COMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/infiltratr-common")

if(NOT TARGET InfiltratrCommon::Portable)
    if(NOT EXISTS "${LINK_COMMON_DIR}/CMakeLists.txt")
        message(FATAL_ERROR
            "Infiltratr Common ${LINK_COMMON_VERSION} is required. Initialise src/infiltratr-common or provide InfiltratrCommon::Portable from the parent project.")
    endif()
    file(READ "${LINK_COMMON_DIR}/VERSION" LINK_COMMON_FOUND_VERSION)
    string(STRIP "${LINK_COMMON_FOUND_VERSION}" LINK_COMMON_FOUND_VERSION)
    if(NOT LINK_COMMON_FOUND_VERSION STREQUAL LINK_COMMON_VERSION)
        message(FATAL_ERROR
            "Infiltratr Common ${LINK_COMMON_VERSION} is required; found ${LINK_COMMON_FOUND_VERSION}.")
    endif()
    find_program(LINK_GIT_EXECUTABLE git)
    if(LINK_GIT_EXECUTABLE)
        execute_process(
            COMMAND "${LINK_GIT_EXECUTABLE}" -C "${LINK_COMMON_DIR}" rev-parse HEAD
            OUTPUT_VARIABLE LINK_COMMON_FOUND_COMMIT
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
            RESULT_VARIABLE LINK_COMMON_COMMIT_RESULT)
        if(LINK_COMMON_COMMIT_RESULT EQUAL 0 AND
           NOT LINK_COMMON_FOUND_COMMIT STREQUAL LINK_COMMON_COMMIT)
            message(FATAL_ERROR
                "Infiltratr Common must be pinned to ${LINK_COMMON_COMMIT}; found ${LINK_COMMON_FOUND_COMMIT}.")
        endif()
    endif()
    add_subdirectory(src/infiltratr-common)
endif()

if(LINK_ENABLE_SANITIZERS AND
   NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    message(FATAL_ERROR "LINK_ENABLE_SANITIZERS requires GCC or Clang.")
endif()

set(LINK_WARNING_FLAGS
    -Wall -Wextra -Wpedantic -Wshadow -Wformat=2
    -Wstrict-prototypes -Wmissing-prototypes -Werror)
set(LINK_SANITIZER_FLAGS
    -fsanitize=address,undefined -fno-omit-frame-pointer)

add_library(link-core STATIC
    src/core/workspace.c
    src/core/i18n.c
    src/core/i18n_platform.c
    src/core/language_pack.c
    src/core/fuel_economy.c
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
    src/obd2/dtc_knowledge.c
    src/uds/uds.c
    src/uds/uds_services.c
    src/discover/safety.c
    src/discover/evidence.c
    src/discover/ecu_probe.c)
add_library(LINK::Core ALIAS link-core)
set_target_properties(link-core PROPERTIES EXPORT_NAME Core)
target_include_directories(link-core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(link-core PUBLIC InfiltratrCommon::Portable)

if(MSVC)
    target_compile_options(link-core PRIVATE /W4 /WX /utf-8)
else()
    target_compile_options(link-core PRIVATE ${LINK_WARNING_FLAGS})
endif()
if(LINK_ENABLE_SANITIZERS)
    target_compile_options(link-core PRIVATE ${LINK_SANITIZER_FLAGS})
    target_link_options(link-core INTERFACE -fsanitize=address,undefined)
endif()

function(link_enable_linux_gtk_shell target)
    if(NOT UNIX OR APPLE)
        message(FATAL_ERROR "link_enable_linux_gtk_shell is supported on Linux only.")
    endif()
    if(NOT TARGET ${target})
        message(FATAL_ERROR "link_enable_linux_gtk_shell target does not exist: ${target}")
    endif()
    set(link_gtk_shell_source "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/platform/linux/link-gtk-shell.c")
    set(link_gtk_i18n_header "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/platform/linux/link-gtk-i18n.h")
    target_sources(${target} PRIVATE
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/platform/linux/link-linux-serial.c"
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/platform/linux/link-gtk-i18n.c"
        "${link_gtk_shell_source}")
    set_source_files_properties("${link_gtk_shell_source}" PROPERTIES
        COMPILE_OPTIONS "-include;${link_gtk_i18n_header}")
    target_include_directories(${target} PRIVATE
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/platform/linux")
    find_path(LINK_BLUEZ_INCLUDE_DIR bluetooth/bluetooth.h REQUIRED)
    find_library(LINK_BLUEZ_LIBRARY bluetooth REQUIRED)
    target_include_directories(${target} PRIVATE "${LINK_BLUEZ_INCLUDE_DIR}")
    target_link_libraries(${target} PRIVATE LINK::Core "${LINK_BLUEZ_LIBRARY}")
endfunction()

function(link_add_windows_discover target)
    if(NOT WIN32)
        message(FATAL_ERROR "link_add_windows_discover is supported on Windows only.")
    endif()

    if(ARGC EQUAL 4 AND NOT ARGV1 STREQUAL "PRODUCT_NAME")
        set(LINK_FACE_PRODUCT_NAME "${ARGV1}")
        set(LINK_FACE_PRODUCT_SLUG "${ARGV2}")
        set(LINK_FACE_WINDOW_CLASS "${ARGV3}")
    else()
        set(one_value_args
            PRODUCT_NAME PRODUCT_SLUG WINDOW_CLASS PRODUCT_VERSION
            PRODUCT_COPYRIGHT PRODUCT_SUBTITLE PRODUCT_WEBSITE PRODUCT_ICON_PNG)
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

    set(link_windows_discover_source "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/platform/windows/link-discover-ui.c")
    set(link_windows_i18n_header "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/platform/windows/link-discover-i18n.h")
    add_executable(${target} WIN32
        "${link_windows_discover_source}"
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/platform/windows/link-discover-i18n.c")
    target_link_libraries(${target} PRIVATE LINK::Core comdlg32 advapi32 shell32 comctl32)
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
        set_property(TARGET ${target} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        target_compile_options(${target} PRIVATE /W4 /WX /utf-8)
        set_source_files_properties("${link_windows_discover_source}" PROPERTIES
            COMPILE_OPTIONS "/FI${link_windows_i18n_header}")
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        set_source_files_properties("${link_windows_discover_source}" PROPERTIES
            COMPILE_OPTIONS "-include;${link_windows_i18n_header}")
    endif()
    if(LINK_FACE_PRODUCT_ICON_PNG)
        if(NOT EXISTS "${LINK_FACE_PRODUCT_ICON_PNG}")
            message(FATAL_ERROR "PRODUCT_ICON_PNG does not exist: ${LINK_FACE_PRODUCT_ICON_PNG}")
        endif()
        find_program(LINK_POWERSHELL_EXECUTABLE NAMES powershell.exe powershell REQUIRED)
        set(generated_icon "${CMAKE_CURRENT_BINARY_DIR}/${target}-product.ico")
        set(generated_rc "${CMAKE_CURRENT_BINARY_DIR}/${target}-product.rc")
        add_custom_command(
            OUTPUT "${generated_icon}"
            COMMAND "${LINK_POWERSHELL_EXECUTABLE}" -NoProfile -ExecutionPolicy Bypass
                -File "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/platform/windows/png-to-ico.ps1"
                -InputPng "${LINK_FACE_PRODUCT_ICON_PNG}" -OutputIco "${generated_icon}"
            DEPENDS "${LINK_FACE_PRODUCT_ICON_PNG}" "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/platform/windows/png-to-ico.ps1"
            VERBATIM)
        set(version_major 0)
        set(version_minor 0)
        set(version_patch 0)
        if(LINK_FACE_PRODUCT_VERSION MATCHES "^([0-9]+)\.([0-9]+)\.([0-9]+)$")
            set(version_major "${CMAKE_MATCH_1}")
            set(version_minor "${CMAKE_MATCH_2}")
            set(version_patch "${CMAKE_MATCH_3}")
        endif()
        file(WRITE "${generated_rc}"
            "1 ICON \"${generated_icon}\"\n"
            "1 VERSIONINFO\nFILEVERSION ${version_major},${version_minor},${version_patch},0\n"
            "PRODUCTVERSION ${version_major},${version_minor},${version_patch},0\n"
            "FILEFLAGSMASK 0x3fL\nFILEFLAGS 0x0L\nFILEOS 0x40004L\nFILETYPE 0x1L\nFILESUBTYPE 0x0L\n"
            "BEGIN\n  BLOCK \"StringFileInfo\"\n  BEGIN\n    BLOCK \"040904b0\"\n    BEGIN\n"
            "      VALUE \"CompanyName\", \"The First Infiltrator\"\n"
            "      VALUE \"FileDescription\", \"${LINK_FACE_PRODUCT_NAME} Discover\"\n"
            "      VALUE \"FileVersion\", \"${LINK_FACE_PRODUCT_VERSION}\"\n"
            "      VALUE \"InternalName\", \"${LINK_FACE_PRODUCT_SLUG}-discover\"\n"
            "      VALUE \"LegalCopyright\", \"${LINK_FACE_PRODUCT_COPYRIGHT}\"\n"
            "      VALUE \"OriginalFilename\", \"${LINK_FACE_PRODUCT_NAME}-${LINK_FACE_PRODUCT_VERSION}-windows-discover.exe\"\n"
            "      VALUE \"ProductName\", \"${LINK_FACE_PRODUCT_NAME}\"\n"
            "      VALUE \"ProductVersion\", \"${LINK_FACE_PRODUCT_VERSION}\"\n"
            "    END\n  END\n  BLOCK \"VarFileInfo\"\n  BEGIN\n    VALUE \"Translation\", 0x0409, 1200\n  END\nEND\n")
        set_source_files_properties("${generated_rc}" PROPERTIES OBJECT_DEPENDS "${generated_icon}")
        target_sources(${target} PRIVATE "${generated_rc}")
    endif()
    set_target_properties(${target} PROPERTIES OUTPUT_NAME "${LINK_FACE_PRODUCT_SLUG}-discover")
endfunction()

if(WIN32 AND PROJECT_IS_TOP_LEVEL)
    link_add_windows_discover(link-discover
        PRODUCT_NAME "LINK" PRODUCT_SLUG "link" WINDOW_CLASS "LINKDiscoverWindow"
        PRODUCT_VERSION "${PROJECT_VERSION}" PRODUCT_COPYRIGHT "Copyright (C) 2026 The First Infiltrator"
        PRODUCT_SUBTITLE "Shared Vehicle Diagnostics"
        PRODUCT_WEBSITE "https://github.com/The-First-Infiltrator/LINK")
    if(LINK_BUILD_REFERENCE_PRODUCT_FACES)
        link_add_windows_discover(mblink-discover PRODUCT_NAME "MBLINK" PRODUCT_SLUG "mblink"
            WINDOW_CLASS "MBLINKDiscoverWindow" PRODUCT_VERSION "${PROJECT_VERSION}"
            PRODUCT_COPYRIGHT "Copyright (C) 2026 Shannon Smith" PRODUCT_SUBTITLE "Mercedes-Benz Diagnostics"
            PRODUCT_WEBSITE "https://github.com/The-First-Infiltrator/MBLINK")
        link_add_windows_discover(jaglink-discover PRODUCT_NAME "JAGLINK" PRODUCT_SLUG "jaglink"
            WINDOW_CLASS "JAGLINKDiscoverWindow" PRODUCT_VERSION "${PROJECT_VERSION}"
            PRODUCT_COPYRIGHT "Copyright (C) 2026 Xavier Wheaton and Shannon Smith" PRODUCT_SUBTITLE "Jaguar X400 Diagnostics"
            PRODUCT_WEBSITE "https://github.com/The-First-Infiltrator/Jaglink")
    endif()
endif()

function(link_add_test target source test_name)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE LINK::Core)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /utf-8)
    else()
        target_compile_options(${target} PRIVATE ${LINK_WARNING_FLAGS})
    endif()
    if(LINK_ENABLE_SANITIZERS)
        target_compile_options(${target} PRIVATE ${LINK_SANITIZER_FLAGS})
        target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endif()
    add_test(NAME ${test_name} COMMAND ${target})
endfunction()

if(BUILD_TESTING AND PROJECT_IS_TOP_LEVEL)
    link_add_test(link-test-workspace tests/test_workspace.c link-workspace)
    link_add_test(link-test-i18n tests/test_i18n.c link-i18n)
    link_add_test(link-test-fuel-economy tests/test_fuel_economy.c link-fuel-economy)
    link_add_test(link-test-diagnostic-flow tests/test_diagnostic_flow.c link-diagnostic-flow)
    link_add_test(link-test-isotp tests/test_isotp.c link-isotp)
    link_add_test(link-test-runtime tests/test_runtime.c link-runtime)
    link_add_test(link-test-elm327 tests/test_elm327.c link-elm327)
    link_add_test(link-test-obd2 tests/test_obd2.c link-obd2)
    link_add_test(link-test-dtc-knowledge tests/test_dtc_knowledge.c link-dtc-knowledge)
    link_add_test(link-test-uds tests/test_uds.c link-uds)
    link_add_test(link-test-discover-safety tests/test_discover_safety.c link-discover-safety)
    link_add_test(link-test-evidence tests/test_evidence.c link-evidence)
    link_add_test(link-test-ecu-probe tests/test_ecu_probe.c link-ecu-probe)
endif()

if(LINK_ENABLE_INSTALL)
    install(TARGETS link-core EXPORT LINKTargets ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
    install(DIRECTORY include/link DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
    install(EXPORT LINKTargets FILE LINKTargets.cmake NAMESPACE LINK:: DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/LINK")
    configure_package_config_file(cmake/LINKConfig.cmake.in "${CMAKE_CURRENT_BINARY_DIR}/LINKConfig.cmake"
        INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/LINK")
    write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/LINKConfigVersion.cmake"
        VERSION "${PROJECT_VERSION}" COMPATIBILITY SameMajorVersion)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/LINKConfig.cmake" "${CMAKE_CURRENT_BINARY_DIR}/LINKConfigVersion.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/LINK")
endif()

if(NOT PROJECT_IS_TOP_LEVEL AND EXISTS "${CMAKE_SOURCE_DIR}/app/ios")
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ValidateAppleBridge.cmake")
    link_validate_product_apple_bridge("${CMAKE_SOURCE_DIR}")
endif()
