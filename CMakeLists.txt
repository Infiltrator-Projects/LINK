# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.20)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" LINK_VERSION LIMIT_COUNT 1)
project(LINK VERSION ${LINK_VERSION} LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include(CTest)

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
