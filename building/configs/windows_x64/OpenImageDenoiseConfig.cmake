#----------------------------------------------------------------
# Combined OpenImageDenoise Config File
#----------------------------------------------------------------

# Ensure CMake version requirement
if("${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION}" LESS 2.8)
   message(FATAL_ERROR "CMake >= 2.8.0 required")
endif()
if(CMAKE_VERSION VERSION_LESS "2.8.3")
   message(FATAL_ERROR "CMake >= 2.8.3 required")
endif()
if(CMAKE_VERSION VERSION_LESS "3.0.0")
   message(FATAL_ERROR "This file relies on CMake 3.0.0 or greater.")
endif()

# Set the import file version
set(CMAKE_IMPORT_FILE_VERSION 1)

# Define the root directory for OpenImageDenoise
set(OIDN_ROOT "$ENV{HOME}/.mfb/dependencies/bl_deps/openimagedenoise")

# Protect against multiple inclusions
set(_cmake_targets_defined "")
set(_cmake_targets_not_defined "")
set(_cmake_expected_targets "")
foreach(_cmake_expected_target IN ITEMS OpenImageDenoise_common OpenImageDenoise_core OpenImageDenoise)
  list(APPEND _cmake_expected_targets "${_cmake_expected_target}")
  if(TARGET "${_cmake_expected_target}")
    list(APPEND _cmake_targets_defined "${_cmake_expected_target}")
  else()
    list(APPEND _cmake_targets_not_defined "${_cmake_expected_target}")
  endif()
endforeach()
unset(_cmake_expected_target)
if(_cmake_targets_defined STREQUAL _cmake_expected_targets)
  unset(_cmake_targets_defined)
  unset(_cmake_targets_not_defined)
  unset(_cmake_expected_targets)
  unset(CMAKE_IMPORT_FILE_VERSION)
  return()
endif()
if(NOT _cmake_targets_defined STREQUAL "")
  string(REPLACE ";" ", " _cmake_targets_defined_text "${_cmake_targets_defined}")
  string(REPLACE ";" ", " _cmake_targets_not_defined_text "${_cmake_targets_not_defined}")
  message(FATAL_ERROR "Some (but not all) targets in this export set were already defined.\nTargets Defined: ${_cmake_targets_defined_text}\nTargets not yet defined: ${_cmake_targets_not_defined_text}\n")
endif()
unset(_cmake_targets_defined)
unset(_cmake_targets_not_defined)
unset(_cmake_expected_targets)

# Create imported targets
add_library(OpenImageDenoise_common INTERFACE IMPORTED)
add_library(OpenImageDenoise_core SHARED IMPORTED)
set_target_properties(OpenImageDenoise_core PROPERTIES INTERFACE_LINK_LIBRARIES "OpenImageDenoise_common")
add_library(OpenImageDenoise SHARED IMPORTED)
set_target_properties(OpenImageDenoise PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${OIDN_ROOT}/include")

# Import target properties for Release configuration
set_property(TARGET OpenImageDenoise_core APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(OpenImageDenoise_core PROPERTIES
  IMPORTED_LOCATION_RELEASE "${OIDN_ROOT}/lib/libOpenImageDenoise_core.so.2.3.0"
  IMPORTED_SONAME_RELEASE "libOpenImageDenoise_core.so"
  )
list(APPEND _cmake_import_check_targets OpenImageDenoise_core )
list(APPEND _cmake_import_check_files_for_OpenImageDenoise_core "${OIDN_ROOT}/lib/libOpenImageDenoise_core.so.2.3.0" )

set_property(TARGET OpenImageDenoise APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(OpenImageDenoise PROPERTIES
  IMPORTED_LINK_DEPENDENT_LIBRARIES_RELEASE "OpenImageDenoise_core"
  IMPORTED_LOCATION_RELEASE "${OIDN_ROOT}/lib/libOpenImageDenoise.so"
  IMPORTED_SONAME_RELEASE "libOpenImageDenoise.so"
  )
list(APPEND _cmake_import_check_targets OpenImageDenoise )
list(APPEND _cmake_import_check_files_for_OpenImageDenoise "${OIDN_ROOT}/lib/libOpenImageDenoise.so" )

# Load additional configuration files
file(GLOB _cmake_config_files "${CMAKE_CURRENT_LIST_DIR}/OpenImageDenoiseConfig-*.cmake")
foreach(_cmake_config_file IN LISTS _cmake_config_files)
  include("${_cmake_config_file}")
endforeach()
unset(_cmake_config_file)
unset(_cmake_config_files)

# Verify that all imported files exist
foreach(_cmake_target IN LISTS _cmake_import_check_targets)
  foreach(_cmake_file IN LISTS "_cmake_import_check_files_for_${_cmake_target}")
    if(NOT EXISTS "${_cmake_file}")
      message(FATAL_ERROR "The imported target \"${_cmake_target}\" references the file \"${_cmake_file}\" but this file does not exist. Possible reasons include:\n* The file was deleted, renamed, or moved to another location.\n* An install or uninstall procedure did not complete successfully.\n* The installation package was faulty and contained \"${CMAKE_CURRENT_LIST_FILE}\" but not all the files it references.\n")
    endif()
  endforeach()
  unset(_cmake_file)
  unset("_cmake_import_check_files_for_${_cmake_target}")
endforeach()
unset(_cmake_target)
unset(_cmake_import_check_targets)

# Check version compatibility
set(PACKAGE_VERSION "2.3.0")

if(PACKAGE_VERSION VERSION_LESS PACKAGE_FIND_VERSION)
  set(PACKAGE_VERSION_COMPATIBLE FALSE)
else()
  if("2.3.0" MATCHES "^([0-9]+)\\.")
    set(CVF_VERSION_MAJOR "${CMAKE_MATCH_1}")
    if(NOT CVF_VERSION_MAJOR VERSION_EQUAL 0)
      string(REGEX REPLACE "^0+" "" CVF_VERSION_MAJOR "${CVF_VERSION_MAJOR}")
    endif()
  else()
    set(CVF_VERSION_MAJOR "2.3.0")
  endif()

  if(PACKAGE_FIND_VERSION_RANGE)
    math (EXPR CVF_VERSION_MAJOR_NEXT "${CVF_VERSION_MAJOR} + 1")
    if (NOT PACKAGE_FIND_VERSION_MIN_MAJOR STREQUAL CVF_VERSION_MAJOR
        OR ((PACKAGE_FIND_VERSION_RANGE_MAX STREQUAL "INCLUDE" AND NOT PACKAGE_FIND_VERSION_MAX STREQUAL CVF_VERSION_MAJOR)
          OR (PACKAGE_FIND_VERSION_RANGE_MAX STREQUAL "EXCLUDE" AND NOT PACKAGE_FIND_VERSION_MAX VERSION_LESS_EQUAL CVF_VERSION_MAJOR_NEXT)))
      set(PACKAGE_VERSION_COMPATIBLE FALSE)
    elseif(PACKAGE_FIND_VERSION_MIN_MAJOR STREQUAL CVF_VERSION_MAJOR
        AND ((PACKAGE_FIND_VERSION_RANGE_MAX STREQUAL "INCLUDE" AND PACKAGE_VERSION VERSION_LESS_EQUAL PACKAGE_FIND_VERSION_MAX)
        OR (PACKAGE_FIND_VERSION_RANGE_MAX STREQUAL "EXCLUDE" AND PACKAGE_VERSION VERSION_LESS PACKAGE_FIND_VERSION_MAX)))
      set(PACKAGE_VERSION_COMPATIBLE TRUE)
    else()
      set(PACKAGE_VERSION_COMPATIBLE FALSE)
    endif()
  else()
    if(PACKAGE_FIND_VERSION_MAJOR STREQUAL CVF_VERSION_MAJOR)
      set(PACKAGE_VERSION_COMPATIBLE TRUE)
    else()
      set(PACKAGE_VERSION_COMPATIBLE FALSE)
    endif()

    if(PACKAGE_FIND_VERSION STREQUAL PACKAGE_VERSION)
      set(PACKAGE_VERSION_EXACT TRUE)
    endif()
  endif()
endif()

if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "" OR "8" STREQUAL "")
  return()
endif()

if(NOT CMAKE_SIZEOF_VOID_P STREQUAL "8")
  math(EXPR installedBits "8 * 8")
  set(PACKAGE_VERSION "${PACKAGE_VERSION} (${installedBits}bit)")
  set(PACKAGE_VERSION_UNSUITABLE TRUE)
endif()

# Cleanup temporary variables
set(_IMPORT_PREFIX)
set(CMAKE_IMPORT_FILE_VERSION)