#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "InfiltratrCommon::Portable" for configuration ""
set_property(TARGET InfiltratrCommon::Portable APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(InfiltratrCommon::Portable PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_NOCONFIG "C"
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/libinfiltratr-portable.a"
  )

list(APPEND _cmake_import_check_targets InfiltratrCommon::Portable )
list(APPEND _cmake_import_check_files_for_InfiltratrCommon::Portable "${_IMPORT_PREFIX}/lib/libinfiltratr-portable.a" )

# Import target "InfiltratrCommon::Common" for configuration ""
set_property(TARGET InfiltratrCommon::Common APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(InfiltratrCommon::Common PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_NOCONFIG "C"
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/libinfiltratr-common.a"
  )

list(APPEND _cmake_import_check_targets InfiltratrCommon::Common )
list(APPEND _cmake_import_check_files_for_InfiltratrCommon::Common "${_IMPORT_PREFIX}/lib/libinfiltratr-common.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
