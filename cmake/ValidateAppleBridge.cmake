# SPDX-License-Identifier: GPL-3.0-or-later
# Validate that an embedding product's native iOS build references every
# portable LINK::Core implementation source. Discover evidence/safety sources
# are intentionally excluded from the iPhone footprint.
#
# Product faces may reference LINK-owned Apple amalgamation entry points instead
# of spelling every implementation source into product-owned wrapper files. The
# validator expands those entry points before checking coverage. Optional native
# providers disabled by the product are excluded from the required source set.

function(link_validate_product_apple_bridge product_root)
    if(NOT IS_DIRECTORY "${product_root}")
        message(FATAL_ERROR "LINK Apple bridge validation root does not exist: ${product_root}")
    endif()

    file(READ "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../CMakeLists.txt" link_cmake_text)
    string(REGEX MATCH
        "add_library\\(link-core STATIC[^\\)]*\\)"
        link_core_block
        "${link_cmake_text}")
    if(NOT link_core_block)
        message(FATAL_ERROR "Unable to locate LINK::Core source list for Apple bridge validation.")
    endif()

    string(REGEX MATCHALL
        "src/[A-Za-z0-9_./-]+\\.c"
        link_core_sources
        "${link_core_block}")
    list(REMOVE_DUPLICATES link_core_sources)
    list(FILTER link_core_sources EXCLUDE REGEX "^src/discover/")

    file(GLOB_RECURSE product_bridge_files
        "${product_root}/src/*.c"
        "${product_root}/app/ios/*.pbxproj")
    # Never allow the nested LINK checkout to satisfy its own bridge coverage.
    # Only product-owned wrapper/bridge source and the product Xcode project
    # count as evidence that the iPhone target actually consumes a LINK source.
    list(FILTER product_bridge_files EXCLUDE REGEX "/src/link/")
    list(FILTER product_bridge_files EXCLUDE REGEX "/src/infiltratr-common/")

    if(NOT product_bridge_files)
        message(FATAL_ERROR
            "Embedding product has no iOS bridge/Xcode sources to validate: ${product_root}")
    endif()

    set(product_bridge_text "")
    foreach(bridge_file IN LISTS product_bridge_files)
        file(READ "${bridge_file}" bridge_text)
        string(APPEND product_bridge_text "\n${bridge_text}")
    endforeach()

    # Expand LINK-owned Apple amalgamation entry points referenced by the
    # product. This proves the product selects shared LINK build glue without
    # forcing copied implementation filenames back into the product repository.
    set(bridge_coverage_text "${product_bridge_text}")
    foreach(apple_bridge IN ITEMS
            LinkPortableCore.c
            LinkPortableObd2.c
            LinkPortableUds.c)
        string(FIND "${product_bridge_text}" "${apple_bridge}" bridge_position)
        if(NOT bridge_position EQUAL -1)
            file(READ
                "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../platform/apple/${apple_bridge}"
                apple_bridge_text)
            string(APPEND bridge_coverage_text "\n${apple_bridge_text}")
        endif()
    endforeach()

    # A non-Mercedes product may explicitly compile the shared Apple engine with
    # the Mercedes me native provider disabled. In that case the shared facade
    # remains available, but the provider implementation translation units must
    # not be required in the branded product binary.
    string(FIND "${product_bridge_text}"
        "LINK_ENABLE_MERCEDES_ME_NATIVE=0" mercedes_disabled_position)
    if(NOT mercedes_disabled_position EQUAL -1)
        list(FILTER link_core_sources EXCLUDE REGEX "^src/core/mercedes_me_.*\\.c$")
    endif()

    set(missing_sources "")
    foreach(link_source IN LISTS link_core_sources)
        string(FIND "${bridge_coverage_text}" "${link_source}" source_position)
        if(source_position EQUAL -1)
            list(APPEND missing_sources "${link_source}")
        endif()
    endforeach()

    if(missing_sources)
        list(JOIN missing_sources ", " missing_text)
        message(FATAL_ERROR
            "The product iOS bridge is missing LINK::Core sources: ${missing_text}. "
            "Update the product bridge/Xcode source wiring before consuming this LINK revision.")
    endif()
endfunction()
