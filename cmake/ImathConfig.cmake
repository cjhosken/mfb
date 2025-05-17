# Manually set Imath paths (adjust as needed)
set(Imath_ROOT="${CMAKE_CURRENT_LIST_DIR}/../deps/imath")
set(Imath_INCLUDE_DIR "${Imath_ROOT}/include")
set(Imath_LIB_DIR "${Imath_ROOT}/lib")

# Find OpenEXR libraries (adjust names if needed)
find_library(Imath_LIB Imath PATHS ${Imath_LIB_DIR} NO_DEFAULT_PATH)


# Include OpenEXR headers
include_directories(${Imath_INCLUDE_DIR})
include_directories(${Imath_INCLUDE_DIR}/Imath)  # Sometimes needed

# Link OpenEXR to your target
link_libraries(${Imath_LIB})