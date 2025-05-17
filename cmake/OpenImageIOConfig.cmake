
# Manually set OpenEXR paths (adjust as needed)
set(OpenImageIO_ROOT="${CMAKE_CURRENT_LIST_DIR}/../deps/openimageio")
set(OpenImageIO_INCLUDE_DIR "${OpenImageIO_ROOT}/include")
set(OpenImageIO_LIB_DIR "${OpenImageIO_ROOT}/lib")

# Find OpenEXR libraries (adjust names if needed)
find_library(OpenImageIO_LIB OpenImageIO PATHS ${OpenImageIO_LIB_DIR} NO_DEFAULT_PATH)
find_library(OpenImageIO_UTIL_LIB OpenImageIO_Util PATHS ${OpenImageIO_LIB_DIR} NO_DEFAULT_PATH)

# Include OpenEXR headers
include_directories(${OpenImageIO_INCLUDE_DIR})

# Link OpenEXR to your target
link_libraries(
    ${OpenImageIO_LIB}
    ${OpenImageIO_UTIL_LIB}
)