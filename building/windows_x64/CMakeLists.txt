# Copyright 2023-2024 DreamWorks Animation LLC
# SPDX-License-Identifier: Apache-2.0

# These ExternalProject targets can be used to download, build and
# install many of the Moonray dependencies.
# The targets are chained using dependencies so that they run
# serially.

cmake_minimum_required(VERSION 3.23.1)
project(openmoonray_third_party)

include(ExternalProject)

set(THIS_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(InstallRoot ${CMAKE_CURRENT_SOURCE_DIR}/dependencies CACHE PATH "Install root for dependencies")

# Ensure the InstallRoot directory exists
file(MAKE_DIRECTORY ${InstallRoot})

# ExternalProject_Add for JsonCpp
ExternalProject_Add(JsonCpp
    URL https://github.com/open-source-parsers/jsoncpp/archive/refs/tags/1.9.5.tar.gz
    INSTALL_DIR ${InstallRoot}
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        -DBUILD_SHARED_LIBS=ON
        -DPYTHON_EXECUTABLE=${Python_EXECUTABLE}  # Set this to your Python executable
)
set(CHAIN JsonCpp)

# ExternalProject_Add for Random123
ExternalProject_Add(Random123
    URL https://github.com/DEShawResearch/random123/archive/refs/tags/v1.14.0.tar.gz
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND cmd /c "make install-include prefix=${InstallRoot}"
    DEPENDS ${CHAIN}
)
set(CHAIN Random123)

# ExternalProject_Add for ISPC
ExternalProject_Add(ISPC
    URL https://github.com/ispc/ispc/releases/download/v1.20.0/ispc-v1.20.0-win64.zip
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND cmd /c "xcopy /Y bin\\ispc ${InstallRoot}\\bin"
    DEPENDS ${CHAIN}
)
set(CHAIN ISPC)

# ExternalProject_Add for Embree
ExternalProject_Add(embree
    URL https://github.com/RenderKit/embree/archive/refs/tags/v4.2.0.tar.gz
    INSTALL_DIR ${InstallRoot}
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        -DEMBREE_ISPC_EXECUTABLE=${InstallRoot}/bin/ispc
        -DEMBREE_ISPC_SUPPORT=ON
        -DEMBREE_IGNORE_INVALID_RAYS=ON
        -DEMBREE_RAY_MASK=ON
        -DEMBREE_MAX_ISA=AVX512
        -DEMBREE_TUTORIALS=OFF
        -DBUILD_SHARED_LIBS=ON
    DEPENDS ${CHAIN}
)
set(CHAIN embree)
