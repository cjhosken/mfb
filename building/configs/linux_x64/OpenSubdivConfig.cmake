# OpenSubdivConfig.cmake
# Expanded and merged from multiple files including opensubdiv-config.cmake, 
# opensubdivconfigversion.cmake, opensubdivtargets.cmake, and opensubdivtargets-release.cmake.

# Define the root directory where OpenSubdiv is installed
get_filename_component(PACKAGE_PREFIX_DIR "$ENV{HOME}/.mfb/dependencies/bl_deps/opensubdiv" ABSOLUTE)

# Helper macros
macro(set_and_check _var _file)
  set(${_var} "${_file}")
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist!")
  endif()
endmacro()

macro(check_required_components _NAME)
  foreach(comp ${${_NAME}_FIND_COMPONENTS})
    if(NOT ${_NAME}_${comp}_FOUND)
      if(${_NAME}_FIND_REQUIRED_${comp})
        set(${_NAME}_FOUND FALSE)
      endif()
    endif()
  endforeach()
endmacro()

# Set directories for includes and libraries
set_and_check(OpenSubdiv_INCLUDE_DIR "${PACKAGE_PREFIX_DIR}/include")
set_and_check(OpenSubdiv_LIB_DIR "${PACKAGE_PREFIX_DIR}/lib")

# Include the targets
include("${CMAKE_CURRENT_LIST_DIR}/OpenSubdivTargets.cmake")

# Version management
set(PACKAGE_VERSION "3.6.0")
if(PACKAGE_VERSION VERSION_LESS PACKAGE_FIND_VERSION)
  set(PACKAGE_VERSION_COMPATIBLE FALSE)
else()
  if("3.6.0" MATCHES "^([0-9]+)\\.")
    set(CVF_VERSION_MAJOR "${CMAKE_MATCH_1}")
    if(NOT CVF_VERSION_MAJOR VERSION_EQUAL 0)
      string(REGEX REPLACE "^0+" "" CVF_VERSION_MAJOR "${CVF_VERSION_MAJOR}")
    endif()
  else()
    set(CVF_VERSION_MAJOR "3.6.0")
  endif()

  if(PACKAGE_FIND_VERSION_RANGE)
    math (EXPR CVF_VERSION_MAJOR_NEXT "${CVF_VERSION_MAJOR} + 1")
    if (NOT PACKAGE_FIND_VERSION_MIN_MAJOR STREQUAL CVF_VERSION_MAJOR
        OR ((PACKAGE_FIND_VERSION_RANGE_MAX STREQUAL "INCLUDE" AND NOT PACKAGE_FIND_VERSION_MAX_MAJOR STREQUAL CVF_VERSION_MAJOR)
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

# Size check for 32/64-bit compatibility
if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "" OR "8" STREQUAL "")
  return()
endif()

if(NOT CMAKE_SIZEOF_VOID_P STREQUAL "8")
  math(EXPR installedBits "8 * 8")
  set(PACKAGE_VERSION "${PACKAGE_VERSION} (${installedBits}bit)")
  set(PACKAGE_VERSION_UNSUITABLE TRUE)
endif()

# CMake version checks
if("${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION}" LESS 2.8)
   message(FATAL_ERROR "CMake >= 2.8.0 required")
endif()
if(CMAKE_VERSION VERSION_LESS "2.8.3")
   message(FATAL_ERROR "CMake >= 2.8.3 required")
endif()
cmake_policy(PUSH)
cmake_policy(VERSION 2.8.3...3.24)

# Import OpenSubdiv targets
# Prevent multiple inclusions of targets
set(_cmake_targets_defined "")
set(_cmake_targets_not_defined "")
set(_cmake_expected_targets "")
foreach(_cmake_expected_target IN ITEMS OpenSubdiv::osdCPU_static OpenSubdiv::osdGPU_static OpenSubdiv::osdCPU OpenSubdiv::osdGPU)
  list(APPEND _cmake_expected_targets "${_cmake_expected_target}")
  if(TARGET "${_cmake_expected_target}")
    list(APPEND _cmake_targets_defined "${_cmake_expected_target}")
  else()
    list(APPEND _cmake_targets_not_defined "${_cmake_expected_target}")
  endif()
endforeach()

if(_cmake_targets_defined STREQUAL _cmake_expected_targets)
  unset(_cmake_targets_defined)
  unset(_cmake_targets_not_defined)
  unset(_cmake_expected_targets)
  unset(CMAKE_IMPORT_FILE_VERSION)
  cmake_policy(POP)
  return()
endif()

if(NOT _cmake_targets_defined STREQUAL "")
  string(REPLACE ";" ", " _cmake_targets_defined_text "${_cmake_targets_defined}")
  string(REPLACE ";" ", " _cmake_targets_not_defined_text "${_cmake_targets_not_defined}")
  message(FATAL_ERROR "Some (but not all) targets in this export set were already defined.
Targets Defined: ${_cmake_targets_defined_text}
Targets not yet defined: ${_cmake_targets_not_defined_text}")
endif()

# Import OpenSubdiv release targets
set(OPENSUBIV_ROOT "$ENV{HOME}/.mfb/dependencies/bl_deps/opensubdiv")

# Import targets for "Release" configuration
set_property(TARGET OpenSubdiv::osdCPU APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(OpenSubdiv::osdCPU PROPERTIES
  IMPORTED_LOCATION_RELEASE "${OPENSUBIV_ROOT}/lib/libosdCPU.so.3.6.0"
  IMPORTED_SONAME_RELEASE "libosdCPU.so"
)

set_property(TARGET OpenSubdiv::osdGPU APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(OpenSubdiv::osdGPU PROPERTIES
  IMPORTED_LOCATION_RELEASE "${OPENSUBIV_ROOT}/lib/libosdGPU.so.3.6.0"
  IMPORTED_SONAME_RELEASE "libosdGPU.so"
)

# Check files exist
list(APPEND _cmake_import_check_targets OpenSubdiv::osdCPU OpenSubdiv::osdGPU)
list(APPEND _cmake_import_check_files_for_OpenSubdiv::osdCPU "${OPENSUBIV_ROOT}/lib/libosdCPU.so.3.6.0")
list(APPEND _cmake_import_check_files_for_OpenSubdiv::osdGPU "${OPENSUBIV_ROOT}/lib/libosdGPU.so.3.6.0")

foreach(_cmake_target IN LISTS _cmake_import_check_targets)
  foreach(_cmake_file IN LISTS "_cmake_import_check_files_for_${_cmake_target}")
    if(NOT EXISTS "${_cmake_file}")
      message(FATAL_ERROR "The imported target \"${_cmake_target}\" references the file \"${_cmake_file}\" but this file does not exist.")
    endif()
  endforeach()
endforeach()

# Cleanup temporary variables
unset(_cmake_import_check_targets)
unset(_cmake_import_check_files_for_OpenSubdiv::osdCPU)
unset(_cmake_import_check_files_for_OpenSubdiv::osdGPU)

cmake_policy(POP)