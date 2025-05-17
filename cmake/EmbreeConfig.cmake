

# Manually set OpenEXR paths (adjust as needed)
set(Embree_ROOT="${CMAKE_CURRENT_LIST_DIR}/../deps/openexr")
set(Embree_INCLUDE_DIRS "${Embree_ROOT}/include")
set(Embree_LIB_DIR "${Embree_ROOT}/lib")

# Find OpenEXR libraries (adjust names if needed)
find_library(Embree_LIBRARY embree4 PATHS ${Embree_LIB_DIR} NO_DEFAULT_PATH)

# Include OpenEXR headers
include_directories(${Embree_INCLUDE_DIRS})

# Link OpenEXR to your target
link_libraries(
    ${Embree_LIBRARY}
)


set(Embree_VERSION 4.2.0)