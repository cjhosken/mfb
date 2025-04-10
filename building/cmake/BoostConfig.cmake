# FindBoost.cmake - Forced Boost configuration with all required components
cmake_minimum_required(VERSION 3.23)

# Disable all automatic Boost finding
set(Boost_NO_BOOST_CMAKE ON)
set(Boost_NO_SYSTEM_PATHS ON)

# Base directory for Boost installation
set(BOOST_ROOT "${CMAKE_CURRENT_LIST_DIR}/boost")
message(STATUS "Manually configuring Boost from: ${BOOST_ROOT}")

# Verify Boost directory exists
if(NOT EXISTS "${BOOST_ROOT}")
    message(FATAL_ERROR "Boost directory not found: ${BOOST_ROOT}")
endif()

# Set include and library paths
set(Boost_INCLUDE_DIRS "${BOOST_ROOT}/include")
set(Boost_LIBRARY_DIRS "${BOOST_ROOT}/lib")

# Verify paths exist
if(NOT EXISTS "${Boost_INCLUDE_DIRS}")
    message(FATAL_ERROR "Boost includes not found: ${Boost_INCLUDE_DIRS}")
endif()

if(NOT EXISTS "${Boost_LIBRARY_DIRS}")
    message(FATAL_ERROR "Boost libraries not found: ${Boost_LIBRARY_DIRS}")
endif()

# Manually set Boost version (adjust to your version)
set(Boost_VERSION "1.82.0")
set(Boost_VERSION_MACRO "108200")

# Manually set ALL required libraries (including the missing ones)
set(Boost_LIBRARIES
    # Original components
    debug "${BOOST_ROOT}/lib/boost_python311-vc143-mt-gd-x64-1_82.lib"
    optimized "${BOOST_ROOT}/lib/boost_python311-vc143-mt-x64-1_82.lib"
    debug "${BOOST_ROOT}/lib/boost_iostreams-vc143-mt-gd-x64-1_82.lib"
    optimized "${BOOST_ROOT}/lib/boost_iostreams-vc143-mt-x64-1_82.lib"
    debug "${BOOST_ROOT}/lib/boost_regex-vc143-mt-gd-x64-1_82.lib"
    optimized "${BOOST_ROOT}/lib/boost_regex-vc143-mt-x64-1_82.lib"
    debug "${BOOST_ROOT}/lib/boost_system-vc143-mt-gd-x64-1_82.lib"
    optimized "${BOOST_ROOT}/lib/boost_system-vc143-mt-x64-1_82.lib"
    debug "${BOOST_ROOT}/lib/boost_date_time-vc143-mt-gd-x64-1_82.lib"
    optimized "${BOOST_ROOT}/lib/boost_date_time-vc143-mt-x64-1_82.lib"
    debug "${BOOST_ROOT}/lib/boost_chrono-vc143-mt-gd-x64-1_82.lib"
    optimized "${BOOST_ROOT}/lib/boost_chrono-vc143-mt-x64-1_82.lib"
    
    # New required components
    debug "${BOOST_ROOT}/lib/boost_filesystem-vc143-mt-gd-x64-1_82.lib"
    optimized "${BOOST_ROOT}/lib/boost_filesystem-vc143-mt-x64-1_82.lib"
    debug "${BOOST_ROOT}/lib/boost_program_options-vc143-mt-gd-x64-1_82.lib"
    optimized "${BOOST_ROOT}/lib/boost_program_options-vc143-mt-x64-1_82.lib"
)

# Mark ALL components as found
set(Boost_python311_FOUND ON)
set(Boost_iostreams_FOUND ON)
set(Boost_regex_FOUND ON)
set(Boost_system_FOUND ON)
set(Boost_date_time_FOUND ON)
set(Boost_chrono_FOUND ON)
set(Boost_filesystem_FOUND ON)
set(Boost_program_options_FOUND ON)

# Force Boost_FOUND
set(Boost_FOUND TRUE)

# Output configuration
message(STATUS "Boost manually configured:")
message(STATUS "  Version: ${Boost_VERSION}")
message(STATUS "  Includes: ${Boost_INCLUDE_DIRS}")
message(STATUS "  Libraries: ${Boost_LIBRARY_DIRS}")
message(STATUS "  Library files: ${Boost_LIBRARIES}")

# Add definitions for static linking
add_definitions(-DBOOST_ALL_NO_LIB -DBOOST_STATIC_LINK)

# Make available to whole project
include_directories(${Boost_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIRS})

# Cache variables
set(Boost_INCLUDE_DIRS ${Boost_INCLUDE_DIRS} CACHE PATH "Boost include directories")
set(Boost_LIBRARY_DIRS ${Boost_LIBRARY_DIRS} CACHE PATH "Boost library directories")
set(Boost_LIBRARIES ${Boost_LIBRARIES} CACHE STRING "Boost library files")