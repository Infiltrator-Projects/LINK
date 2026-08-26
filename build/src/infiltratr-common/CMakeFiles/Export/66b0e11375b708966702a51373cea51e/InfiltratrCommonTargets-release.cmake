#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "InfiltratrCommon::Portable" for configuration "Release"
set_property(TARGET InfiltratrCommon::Portable APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(InfiltratrCommon::Portable PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libinfiltratr-portable.a"
  )

list(APPEND _cmake_import_check_targets InfiltratrCommon::Portable )
list(APPEND _cmake_import_check_files_for_InfiltratrCommon::Portable "${_IMPORT_PREFIX}/lib/libinfiltratr-portable.a" )

# Import target "InfiltratrCommon::Common" for configuration "Release"
set_property(TARGET InfiltratrCommon::Common APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(InfiltratrCommon::Common PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libinfiltratr-common.a"
  )

list(APPEND _cmake_import_check_targets InfiltratrCommon::Common )
list(APPEND _cmake_import_check_files_for_InfiltratrCommon::Common "${_IMPORT_PREFIX}/lib/libinfiltratr-common.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
