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

ExternalProject_Add(ISPC
    URL https://github.com/ispc/ispc/releases/download/v1.20.0/ispc-v1.20.0-windows.zip
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""  # Prints the build directory for debugging
    INSTALL_COMMAND cmake -E copy_directory "<BINARY_DIR>" "${InstallRoot}"
)
set(CHAIN ISPC)

ExternalProject_Add(embree
    URL https://github.com/RenderKit/embree/archive/refs/tags/v4.2.0.tar.gz
    INSTALL_DIR ${InstallRoot}
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
        -DEMBREE_ISPC_EXECUTABLE=<INSTALL_DIR>/bin/ispc.exe
        -DEMBREE_ISPC_SUPPORT=ON
        -DEMBREE_IGNORE_INVALID_RAYS=ON
        -DEMBREE_RAY_MASK=ON
        -DEMBREE_MAX_ISA=AVX2
        -DEMBREE_TUTORIALS=OFF
        -DBUILD_SHARED_LIBS=ON
        -DTBB_ROOT=<INSTALL_DIR>/tbb
        -DTBB_DIR=<INSTALL_DIR>/tbb
    DEPENDS ${CHAIN}
)
set(CHAIN embree)