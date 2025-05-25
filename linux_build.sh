#!/bin/bash

SCRIPT_DIR="$(dirname "$(realpath "$0")")"
SOURCE_DIR=$SCRIPT_DIR/source
BUILD_DIR=$SCRIPT_DIR/build
BUILD_DEPS_DIR=$SCRIPT_DIR/build-deps
INSTALL_DIR=$SCRIPT_DIR/openmoonray
DEPS_DIR=$SCRIPT_DIR/deps
BLENDER_VERSION=4.4

git clone --recurse-submodules https://github.com/dreamworksanimation/openmoonray.git $SOURCE_DIR

cd $SCRIPT_DIR

git clone https://projects.blender.org/blender/lib-linux_x64.git $DEPS_DIR
cd $DEPS_DIR
git checkout blender-v$BLENDER_VERSION-release

mkdir -p $BUILD_DEPS_DIR
cd $BUILD_DEPS_DIR

cmake .. -DInstallRoot="$DEPS_DIR"
cmake --build . -- -j $(nproc)

mkdir -p $BUILD_DIR
cd $BUILD_DIR

cmake $SOURCE_DIR \
    -DBoost_ROOT="$DEPS_DIR" \
    -DLibuuid_ROOT="$DEPS_DIR" \
    -DCppUnit_ROOT="$DEPS_DIR" \
    -DISPC="$DEPS_DIR/ispc/bin/ispc" \
    -DJsonCpp_ROOT="$DEPS_DIR" \
    -DLibcurl_ROOT="$DEPS_DIR" \
    -DLog4cplus_ROOT="$DEPS_DIR" \
    -DLUA_DIR="$DEPS_DIR" \
    -DOpenEXR_ROOT="$DEPS_DIR/openexr" \
    -DOpenSubDiv_ROOT="$DEPS_DIR/opensubdiv" \
    -DOpenVDB_ROOT="$DEPS_DIR/openvdb" \
    -DOPTIX_ROOT="$SCRIPT_DIR/optix/linux" \
    -DPXR_USD_LOCATION="$DEPS_DIR/usd" \
    -DPXR_INCLUDE_DIRS="$DEPS_DIR/usd/include" \
    -DRandom123_ROOT="$DEPS_DIR/random123" \
    -DZLIB_ROOT="$DEPS_DIR/zlib" \
    -DPYTHON_EXECTAUBLE=python3 \
    -DBOOST_PYTHON_COMPONENT_NAME=python39 \
    -DABI_VERSION=0 \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=$INSTALL_DIR

cmake --build . -- -j $(nproc)

echo "Moonray has been successfully built at $INSTALL_DIR!"