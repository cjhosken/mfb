

# Manually set OpenEXR paths (adjust as needed)
set(OpenEXR_ROOT="${CMAKE_CURRENT_LIST_DIR}/../deps/openexr")
set(OpenEXR_INCLUDE_DIR "${OpenEXR_ROOT}/include")
set(OpenEXR_LIB_DIR "${OpenEXR_ROOT}/lib")

find_package(Imath CONFIG REQUIRED HINT "${CMAKE_CURRENT_LIST_DIR}")

# Find OpenEXR libraries (adjust names if needed)
find_library(OpenEXR_LIB OpenEXR PATHS ${OpenEXR_LIB_DIR} NO_DEFAULT_PATH)
find_library(OpenEXR_CORE_LIB OpenEXRCore PATHS ${OpenEXR_LIB_DIR} NO_DEFAULT_PATH)
find_library(OpenEXR_UTIL_LIB OpenEXRUtil PATHS ${OpenEXR_LIB_DIR} NO_DEFAULT_PATH)
find_library(OpenEXR_ILMTHREAD_LIB IlmThread PATHS ${OpenEXR_LIB_DIR} NO_DEFAULT_PATH)
find_library(OpenEXR_IEX_LIB Iex PATHS ${OpenEXR_LIB_DIR} NO_DEFAULT_PATH)

# Include OpenEXR headers
include_directories(${OpenEXR_INCLUDE_DIR})

# Link OpenEXR to your target
link_libraries(
    ${OpenEXR_LIB}
    ${OpenEXR_CORE_LIB}
    ${OpenEXR_UTIL_LIB}
    ${OpenEXR_ILMTHREAD_LIB}
    ${OpenEXR_IEX_LIB}
)