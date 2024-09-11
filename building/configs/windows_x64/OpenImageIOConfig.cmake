# OpenImageIOConfig.cmake

####### Combined OpenImageIO Config CMake File ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)

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

####################################################################################

include(CMakeFindDependencyMacro)

# Add here all the find_dependency() as needed
if (2.5.7 VERSION_GREATER_EQUAL 3.0)
    find_dependency(Imath HINTS Imath_DIR-NOTFOUND)
elseif (2.5.7 VERSION_GREATER_EQUAL 2.4 AND 1)
    find_dependency(OpenEXR)
    find_dependency(ZLIB 1.2.11)  # Because OpenEXR doesn't do it
    find_dependency(Threads)  # Because OpenEXR doesn't do it
endif()

# Set OpenImageIO directories
set_and_check(OpenImageIO_INCLUDE_DIR "/home/hoske/.mfb/dependencies/bl_deps/openimageio/include")
set_and_check(OpenImageIO_INCLUDES "/home/hoske/.mfb/dependencies/bl_deps/openimageio/include")
set_and_check(OpenImageIO_LIB_DIR "/home/hoske/.mfb/dependencies/bl_deps/openimageio/lib")
set(OpenImageIO_PLUGIN_SEARCH_PATH "")

if(NOT 1)
    list(APPEND OpenImageIO_INCLUDES ${IMATH_INCLUDES} ${OPENEXR_INCLUDES})
endif()

# Check required components for OpenImageIO
check_required_components("OpenImageIO")

# Set if OpenImageIO has OpenColorIO support
set(OpenImageIO_HAS_OpenColorIO 0)

####################################################################################

# Version compatibility logic from oiioconfigversion.cmake

set(PACKAGE_VERSION "2.5.11")

if(PACKAGE_VERSION VERSION_LESS PACKAGE_FIND_VERSION)
  set(PACKAGE_VERSION_COMPATIBLE FALSE)
else()

  if("2.5.11" MATCHES "^([0-9]+)\\.")
    set(CVF_VERSION_MAJOR "${CMAKE_MATCH_1}")
    if(NOT CVF_VERSION_MAJOR VERSION_EQUAL 0)
      string(REGEX REPLACE "^0+" "" CVF_VERSION_MAJOR "${CVF_VERSION_MAJOR}")
    endif()
  else()
    set(CVF_VERSION_MAJOR "2.5.11")
  endif()

  if(PACKAGE_FIND_VERSION_RANGE)
    # both endpoints of the range must have the expected major version
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

# Bitness check
if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "" OR "8" STREQUAL "")
  return()
endif()

# Check that the installed version has the same 32/64bit-ness as the one searching
if(NOT CMAKE_SIZEOF_VOID_P STREQUAL "8")
  math(EXPR installedBits "8 * 8")
  set(PACKAGE_VERSION "${PACKAGE_VERSION} (${installedBits}bit)")
  set(PACKAGE_VERSION_UNSUITABLE TRUE)
endif()

####################################################################################

# Target definitions from oiiotargets.cmake

if("${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION}" LESS 2.8)
   message(FATAL_ERROR "CMake >= 2.8.0 required")
endif()
if(CMAKE_VERSION VERSION_LESS "2.8.3")
   message(FATAL_ERROR "CMake >= 2.8.3 required")
endif()
cmake_policy(PUSH)
cmake_policy(VERSION 2.8.3...3.24)

# Define OpenImageIO targets
foreach(_cmake_expected_target IN ITEMS OpenImageIO::OpenImageIO_Util OpenImageIO::OpenImageIO OpenImageIO::iconvert OpenImageIO::idiff OpenImageIO::igrep OpenImageIO::iinfo OpenImageIO::maketx OpenImageIO::oiiotool OpenImageIO::iv)
  if(NOT TARGET "${_cmake_expected_target}")
    add_library("${_cmake_expected_target}" SHARED IMPORTED)
  endif()
endforeach()

# Define target properties
set_target_properties(OpenImageIO::OpenImageIO_Util PROPERTIES
  INTERFACE_COMPILE_FEATURES "cxx_std_14"
  INTERFACE_INCLUDE_DIRECTORIES "$ENV{HOME}/.mfb/dependencies/bl_deps/openimageio/include"
  INTERFACE_LINK_LIBRARIES "\$<TARGET_NAME_IF_EXISTS:OpenEXR::Imath>;\$<TARGET_NAME_IF_EXISTS:OpenEXR::OpenEXR>;\$<TARGET_NAME_IF_EXISTS:OpenEXR::IlmThread>;\$<TARGET_NAME_IF_EXISTS:OpenEXR::Iex>;Threads::Threads"
)

set_target_properties(OpenImageIO::OpenImageIO PROPERTIES
  INTERFACE_COMPILE_FEATURES "cxx_std_14"
  INTERFACE_INCLUDE_DIRECTORIES "$ENV{HOME}/.mfb/dependencies/bl_deps/openimageio/include"
  INTERFACE_LINK_LIBRARIES "OpenImageIO::OpenImageIO_Util;\$<TARGET_NAME_IF_EXISTS:OpenEXR::Imath>;\$<TARGET_NAME_IF_EXISTS:OpenEXR::OpenEXR>;\$<TARGET_NAME_IF_EXISTS:OpenEXR::IlmThread>;\$<TARGET_NAME_IF_EXISTS:OpenEXR::Iex>;Threads::Threads"
)

####################################################################################

# Import paths for release builds from oiiotargets-release.cmake

set(OIIO_ROOT "$ENV{HOME}/.mfb/dependencies/bl_deps/openimageio")

# Release build targets
set_target_properties(OpenImageIO::OpenImageIO_Util PROPERTIES
  IMPORTED_LOCATION_RELEASE "${OIIO_ROOT}/lib/libOpenImageIO_Util.so"
  IMPORTED_SONAME_RELEASE "libOpenImageIO_Util.so"
)

set_target_properties(OpenImageIO::OpenImageIO PROPERTIES
  IMPORTED_LOCATION_RELEASE "${OIIO_ROOT}/lib/libOpenImageIO.so"
  IMPORTED_SONAME_RELEASE "libOpenImageIO.so"
)

set_target_properties(OpenImageIO::idiff PROPERTIES
  IMPORTED_LOCATION_RELEASE "${OIIO_ROOT}/bin/idiff"
)

set_target_properties(OpenImageIO::maketx PROPERTIES
  IMPORTED_LOCATION_RELEASE "${OIIO_ROOT}/bin/maketx"
)

set_target_properties(OpenImageIO::oiiotool PROPERTIES
  IMPORTED_LOCATION_RELEASE "${OIIO_ROOT}/bin/oiiotool"
)

####################################################################################

cmake_policy(POP)