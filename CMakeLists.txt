cmake_minimum_required (VERSION 3.23.1)
project(mfb_third_party)

include(ExternalProject)
include(ProcessorCount)
ProcessorCount(NPROC)
if(NPROC EQUAL 0)
    set(NPROC 2)
endif()

set(InstallRoot ./deps/ CACHE FILEPATH "Install root for dependencies")

ExternalProject_Add(JsonCpp
    GIT_REPOSITORY https://github.com/open-source-parsers/jsoncpp.git
    GIT_TAG 5defb4ed1a4293b8e2bf641e16b156fb9de498cc # 1.9.5
    INSTALL_DIR ${InstallRoot}
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
        -DBUILD_SHARED_LIBS=1
        -DPYTHON_EXECUTABLE=/usr/bin/python3
        -DJSONCPP_LIB_BUILD_SHARED:BOOL=ON
        -DJSONCPP_WITH_PKGCONFIG_SUPPORT=OFF
)
set(CHAIN JsonCpp)

ExternalProject_Add(Random123
    GIT_REPOSITORY https://github.com/DEShawResearch/random123
    GIT_TAG 726a093cd9a73f3ec3c8d7a70ff10ed8efec8d13 # v1.14.0 (internally we use 1.08.3)
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND make install-include prefix=${InstallRoot}
    DEPENDS ${CHAIN}
)
set(CHAIN Random123)

ExternalProject_Add(ISPC
    URL https://github.com/ispc/ispc/releases/download/v1.21.0/ispc-v1.21.0-linux.tar.gz
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND cp bin/ispc ${InstallRoot}/bin
    DEPENDS ${CHAIN}
)
set(CHAIN ISPC)

ExternalProject_Add(Boost
    URL https://archives.boost.io/release/1.82.0/source/boost_1_82_0.tar.gz
    INSTALL_DIR ${InstallRoot}
    CONFIGURE_COMMAND env CXXFLAGS=-I/usr/include/python3.14 ./bootstrap.sh --prefix=<INSTALL_DIR> --with-python=/usr/bin/python3 --with-libraries=chrono,date_time,filesystem,program_options,system,python,regex,thread
    BUILD_COMMAND sh -c "./b2 -j${NPROC} link=shared runtime-link=shared --keep-going || true"
    BUILD_IN_SOURCE 1
    INSTALL_COMMAND sh -c "cp -f stage/lib/*.so* <INSTALL_DIR>/lib/ && mkdir -p <INSTALL_DIR>/lib/cmake && cp -rf stage/lib/cmake/* <INSTALL_DIR>/lib/cmake/ && cp -rf boost <INSTALL_DIR>/ && sed -i 's|/../../../|/../../|g' <INSTALL_DIR>/lib/cmake/boost_*/*.cmake"
    DEPENDS ${CHAIN}
)
set(CHAIN Boost)
