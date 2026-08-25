#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "LINK::Core" for configuration "Release"
set_property(TARGET LINK::Core APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(LINK::Core PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/liblink-core.a"
  )

list(APPEND _cmake_import_check_targets LINK::Core )
list(APPEND _cmake_import_check_files_for_LINK::Core "${_IMPORT_PREFIX}/lib/liblink-core.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
