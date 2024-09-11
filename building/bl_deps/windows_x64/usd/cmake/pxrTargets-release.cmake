#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "usd_ms" for configuration "Release"
set_property(TARGET usd_ms APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(usd_ms PROPERTIES
  IMPORTED_IMPLIB_RELEASE "${_IMPORT_PREFIX}/lib/usd_ms.lib"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/usd_ms.dll"
  )

list(APPEND _cmake_import_check_targets usd_ms )
list(APPEND _cmake_import_check_files_for_usd_ms "${_IMPORT_PREFIX}/lib/usd_ms.lib" "${_IMPORT_PREFIX}/lib/usd_ms.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
