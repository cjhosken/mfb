
cmake_minimum_required (VERSION 3.23.1)
include(ExternalProject)

set(InstallRoot deps CACHE FILEPATH "Install root for dependencies")

ExternalProject_Add(Random123
    GIT_REPOSITORY https://github.com/DEShawResearch/random123
    GIT_TAG 726a093cd9a73f3ec3c8d7a70ff10ed8efec8d13 # v1.14.0
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND make install-include prefix=${InstallRoot}/random123
)
set(CHAIN Random123)

ExternalProject_Add(ISPC
    URL https://github.com/ispc/ispc/releases/download/v1.20.0/ispc-v1.20.0-linux.tar.gz
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND mkdir ${InstallRoot}/ispc ${InstallRoot}/ispc/bin
    BUILD_COMMAND ""
    INSTALL_COMMAND cp bin/ispc ${InstallRoot}/ispc/bin/ispc
    DEPENDS ${CHAIN}
)
set(CHAIN ISPC)