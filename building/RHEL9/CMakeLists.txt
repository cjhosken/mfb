# Copyright 2023-2024 DreamWorks Animation LLC
# SPDX-License-Identifier: Apache-2.0

# These ExternalProject targets can be used to download, build and
# install many of the Moonray dependencies.
# The targets are chained using dependencies so that they run
# serially.

cmake_minimum_required (VERSION 3.23.1)
project(openmoonray_third_party)

include(ExternalProject)

set(InstallRoot ../../deps CACHE FILEPATH "Install root for dependencies")

ExternalProject_Add(JsonCpp
    GIT_REPOSITORY https://github.com/open-source-parsers/jsoncpp.git
    GIT_TAG 5defb4ed1a4293b8e2bf641e16b156fb9de498cc # 1.9.5
    INSTALL_DIR ${InstallRoot}
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>/jsoncpp
        -DBUILD_SHARED_LIBS=1
        -DPYTHON_EXECUTABLE=<INSTALL_DIR>/python/bin/python3.11
        -DJSONCPP_LIB_BUILD_SHARED:BOOL=ON
        -DJSONCPP_WITH_PKGCONFIG_SUPPORT=OFF
)
set(CHAIN JsonCpp)

ExternalProject_Add(Random123
    GIT_REPOSITORY https://github.com/DEShawResearch/random123
    GIT_TAG 726a093cd9a73f3ec3c8d7a70ff10ed8efec8d13 # v1.14.0
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND make install-include prefix=${InstallRoot}/random123
    DEPENDS ${CHAIN}
)
set(CHAIN Random123)

ExternalProject_Add(ISPC
    URL https://github.com/ispc/ispc/releases/download/v1.20.0/ispc-v1.20.0-linux.tar.gz
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ""
    BUILD_COMMAND mkdir ${InstallRoot}/ispc ${InstallRoot}/ispc/bin
    INSTALL_COMMAND cp bin/ispc ${InstallRoot}/ispc/bin
    DEPENDS ${CHAIN}
)
set(CHAIN ISPC)

