# Set USD (PXR) paths
set(PXR_ROOT "$ENV{HOME}/.mfb/source/deps/usd")
set(PXR_INCLUDE_DIR "${PXR_ROOT}/include")
set(PXR_LIB_DIR "${PXR_ROOT}/lib")

# Python and Boost paths
set(Python_ROOT "$ENV{HOME}/.mfb/source/deps/python")
set(Python_INCLUDE_DIRS "${Python_ROOT}/include")

# Find dependencies with EXACT version matching
find_package(OpenImageIO CONFIG REQUIRED)
find_package(MaterialX CONFIG REQUIRED)
find_package(OpenVDB CONFIG REQUIRED)
find_package(OpenSubdiv CONFIG REQUIRED)

if(pxr_FOUND)
    message(STATUS "Found USD via config file: ${pxr_DIR}")
else()
    # Fall back to manual setup
    message(STATUS "USD not found via config, trying manual setup...")

    # Verify USD installation structure
    if(NOT EXISTS "${PXR_INCLUDE_DIR}/pxr/pxr.h")
        message(FATAL_ERROR "USD headers not found in ${PXR_INCLUDE_DIR}")
    endif()

    # Include directories
    include_directories(
        ${Python_INCLUDE_DIRS}
        ${Python_INCLUDE_DIRS}/python3.11
        ${PXR_INCLUDE_DIR}
        ${PXR_INCLUDE_DIR}/pxr
    )

    find_library(PXR_LIBRARY
        NAMES usd_ms
        PATHS ${PXR_LIB_DIR}
        NO_DEFAULT_PATH
    )

    # Find all USD libraries with proper versioned names
    set(USD_LIBRARIES
        arch tf gf js trace work plug vt ts ar kind sdf ndr sdr pcp usd
        usdGeom usdVol usdMedia usdShade usdLux usdProc usdRender usdHydra
        usdRi usdSkel usdUI usdUtils usdPhysics usdMtlx garch hf hio cameraUtil
        pxOsd geomUtil glf hgi hgiGL hgiInterop hd hdar hdGp hdsi hdMtlx
        hioOpenVDB hdSt hdx usdImaging usdImagingGL usdProcImaging
        usdRiPxrImaging usdSkelImaging usdVolImaging usdAppUtils
    )

    foreach(lib ${USD_LIBRARIES})        
        add_library(${lib} UNKNOWN IMPORTED)
        set_target_properties(${lib} PROPERTIES
            IMPORTED_LOCATION "${PXR_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIR}"
            INTERFACE_LINK_LIBRARIES
                "OpenImageIO::OpenImageIO;OpenImageIO::OpenImageIO_Util;MaterialX::MaterialXCore;OpenVDB::openvdb;OpenSubdiv::osdCPU;"
        )
        list(APPEND PXR_ALL_LIBRARIES ${lib})
    endforeach()
endif()

# Set RPATH for runtime library resolution
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH "${PXR_LIB_DIR}")
list(APPEND CMAKE_INSTALL_RPATH
    "${OPENIMAGEIO_ROOT}/lib"
    "${MATERIALX_ROOT}/lib"
    "${OPENVDB_ROOT}/lib"
    "${OPENSUBDIV_ROOT}/lib"
    "${OPENEXR_ROOT}/lib"
    "${IMATH_ROOT}/lib"
)

# Version information
set(PXR_MAJOR_VERSION 23)
set(PXR_MINOR_VERSION 11)
set(PXR_PATCH_VERSION 0)
set(PXR_VERSION 23.11.0)