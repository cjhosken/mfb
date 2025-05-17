
# - Configuration file for the pxr project
# Defines the following variables:
# PXR_MAJOR_VERSION - Major version number.
# PXR_MINOR_VERSION - Minor version number.
# PXR_PATCH_VERSION - Patch version number.
# PXR_VERSION       - Complete pxr version string.
# PXR_INCLUDE_DIRS  - Root include directory for the installed project.
# PXR_LIBRARIES     - List of all libraries, by target name.
# PXR_foo_LIBRARY   - Absolute path to individual libraries.
# The preprocessor definition PXR_STATIC will be defined if appropriate

get_filename_component(PXR_CMAKE_DIR "${CMAKE_CURRENT_LIST_FILE}/../deps/usd" PATH)

set(PXR_MAJOR_VERSION 23)
set(PXR_MINOR_VERSION 11)
set(PXR_PATCH_VERSION 0)
set(PXR_VERSION 23.11.0)

include(CMakeFindDependencyMacro)

# If Python support was enabled for this USD build, find the import
# targets by invoking the appropriate FindPython module. Use the same
# LIBRARY and INCLUDE_DIR settings from the original build if they
# were set. This can be overridden by specifying different values when
# running cmake.

find_package(Python3 COMPONENTS Development)
find_package(MaterialX)
find_package(Imath)

find_library(PXR_LIB usd_ms PATHS "${PXR_CMAKEE_DIR}/lib" NO_DEFAULT_PATH)
set(PXR_LIBRARIES "arch;tf;gf;js;trace;work;plug;vt;ts;ar;kind;sdf;ndr;sdr;pcp;usd;usdGeom;usdVol;usdMedia;usdShade;usdLux;usdProc;usdRender;usdHydra;usdRi;usdSkel;usdUI;usdUtils;usdPhysics;usdMtlx;garch;hf;hio;cameraUtil;pxOsd;geomUtil;glf;hgi;hgiGL;hgiInterop;hd;hdar;hdGp;hdsi;hdMtlx;hioOpenVDB;hdSt;hdx;usdImaging;usdImagingGL;usdProcImaging;usdRiPxrImaging;usdSkelImaging;usdVolImaging;usdAppUtils")
set(PXR_INCLUDE_DIRS "${PXR_CMAKE_DIR}/include")
string(REPLACE " " ";" PXR_LIBRARIES "${libs}")
foreach(lib ${libs})
    add_library(${lib} ALIAS ${PXR_LIB})
endforeach()
if(NOT @BUILD_SHARED_LIBS@)
    if(WIN32)
        list(APPEND PXR_LIBRARIES Shlwapi.lib)
        list(APPEND PXR_LIBRARIES Dbghelp.lib)
    endif()
    add_definitions(-DPXR_STATIC)
endif()

include_directories(${PXR_INCLUDE_DIRS})