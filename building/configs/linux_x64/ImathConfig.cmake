# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the OpenEXR Project.

####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was ImathConfig.cmake.in                            ####

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)

macro(set_and_check _var _file)
  set(${_var} "${_file}")
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist !")
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

####################################################################################

# Version compatibility logic from IMathconfigversion.cmake

set(PACKAGE_VERSION "3.1.9")

if(PACKAGE_VERSION VERSION_LESS PACKAGE_FIND_VERSION)
  set(PACKAGE_VERSION_COMPATIBLE FALSE)
else()
  if("3.1.9" MATCHES "^([0-9]+)\\.")
    set(CVF_VERSION_MAJOR "${CMAKE_MATCH_1}")
    if(NOT CVF_VERSION_MAJOR VERSION_EQUAL 0)
      string(REGEX REPLACE "^0+" "" CVF_VERSION_MAJOR "${CVF_VERSION_MAJOR}")
    endif()
  else()
    set(CVF_VERSION_MAJOR "3.1.9")
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

# Ensure 32/64-bit compatibility
if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "" OR "8" STREQUAL "")
  return()
endif()

if(NOT CMAKE_SIZEOF_VOID_P STREQUAL "8")
  math(EXPR installedBits "8 * 8")
  set(PACKAGE_VERSION "${PACKAGE_VERSION} (${installedBits}bit)")
  set(PACKAGE_VERSION_UNSUITABLE TRUE)
endif()

####################################################################################

# Include target definitions from ImathTargets.cmake and ImathTargets-release.cmake

set(IMATH_ROOT "$ENV{HOME}/.mfb/dependencies/bl_deps/imath")

if("${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION}" LESS 2.8)
   message(FATAL_ERROR "CMake >= 2.8.0 required")
endif()
if(CMAKE_VERSION VERSION_LESS "2.8.3")
   message(FATAL_ERROR "CMake >= 2.8.3 required")
endif()
cmake_policy(PUSH)
cmake_policy(VERSION 2.8.3...3.24)

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Define Imath targets
set(_cmake_targets_defined "")
set(_cmake_targets_not_defined "")
set(_cmake_expected_targets "Imath::ImathConfig;Imath::Imath")
foreach(_cmake_expected_target IN ITEMS ${_cmake_expected_targets})
  if(TARGET "${_cmake_expected_target}")
    list(APPEND _cmake_targets_defined "${_cmake_expected_target}")
  else()
    list(APPEND _cmake_targets_not_defined "${_cmake_expected_target}")
  endif()
endforeach()

if(_cmake_targets_defined STREQUAL _cmake_expected_targets)
  cmake_policy(POP)
  return()
endif()

if(NOT _cmake_targets_defined STREQUAL "")
  string(REPLACE ";" ", " _cmake_targets_defined_text "${_cmake_targets_defined}")
  string(REPLACE ";" ", " _cmake_targets_not_defined_text "${_cmake_targets_not_defined}")
  message(FATAL_ERROR "Some (but not all) targets in this export set were already defined.\nTargets Defined: ${_cmake_targets_defined_text}\nTargets not yet defined: ${_cmake_targets_not_defined_text}\n")
endif()

# Define Imath::ImathConfig target
add_library(Imath::ImathConfig INTERFACE IMPORTED)
set_target_properties(Imath::ImathConfig PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${IMATH_ROOT}/include;${IMATH_ROOT}/include/Imath"
)

# Define Imath::Imath target for Release configuration
add_library(Imath::Imath SHARED IMPORTED)
set_target_properties(Imath::Imath PROPERTIES
  INTERFACE_COMPILE_FEATURES "cxx_std_11"
  INTERFACE_INCLUDE_DIRECTORIES "${IMATH_ROOT}/include"
  INTERFACE_LINK_LIBRARIES "Imath::ImathConfig;m"
  IMPORTED_LOCATION_RELEASE "${IMATH_ROOT}/lib/libImath.so"
  IMPORTED_SONAME_RELEASE "libImath.so.30"
)

list(APPEND _cmake_import_check_targets Imath::Imath )
list(APPEND _cmake_import_check_files_for_Imath::Imath "${IMATH_ROOT}/lib/libImath.so" )

# Load additional target configuration
file(GLOB _cmake_config_files "${CMAKE_CURRENT_LIST_DIR}/ImathTargets-*.cmake")
foreach(_cmake_config_file IN LISTS _cmake_config_files)
  include("${_cmake_config_file}")
endforeach()

# Verify imported files exist
foreach(_cmake_target IN LISTS _cmake_import_check_targets)
  foreach(_cmake_file IN LISTS "_cmake_import_check_files_for_${_cmake_target}")
    if(NOT EXISTS "${_cmake_file}")
      message(FATAL_ERROR "The imported target \"${_cmake_target}\" references the file
      \"${_cmake_file}\"
      but this file does not exist. Possible reasons include:
      * The file was deleted, renamed, or moved to another location.
      * An install or uninstall procedure did not complete successfully.
      * The installation package was faulty and contained
      \"${CMAKE_CURRENT_LIST_FILE}\"
      but not all the files it references.")
    endif()
  endforeach()
endforeach()

# Cleanup temporary variables.
unset(_IMPORT_PREFIX)
unset(_cmake_import_check_targets)
unset(_cmake_import_check_files_for_Imath::Imath)
cmake_policy(POP)
