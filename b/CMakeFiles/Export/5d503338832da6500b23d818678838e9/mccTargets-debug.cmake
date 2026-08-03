#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "mcc::libmcc" for configuration "Debug"
set_property(TARGET mcc::libmcc APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(mcc::libmcc PROPERTIES
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib64/libmcc.so"
  IMPORTED_SONAME_DEBUG "libmcc.so"
  )

list(APPEND _cmake_import_check_targets mcc::libmcc )
list(APPEND _cmake_import_check_files_for_mcc::libmcc "${_IMPORT_PREFIX}/lib64/libmcc.so" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
