DIR=$(dirname "$(realpath "$0")")

mkdir -p $DIR/build
cd $DIR/build
rm -rf *

DEPS="$DIR/deps"
CMAKES="$DIR/cmake"

cmake ../source \
    -DCMAKE_PREFIX_PATH=$DEPS:$CMAKES \
    -DCMAKE_MODULES_ROOT="$DIR/cmake" \
    -DPYTHON_EXECUTABLE="$DEPS/python/bin/python3" \
    -DBOOST_PYTHON_COMPONENT_NAME=python311 \
    -DABI_VERSION=0 \
    -DJsonCpp_ROOT="$DEPS/jsoncpp" \
    -DOPENVDB_ROOT="$DEPS/openvdb" \
    -DCMAKE_ISPC_COMPILER="$DEPS/ispc/bin/ispc" \
    -DOpenEXR_ROOT=$CMAKES \
    -DOpenImageIO_ROOT=$CMAKES \
    -DPython_ROOT="$DEPS/python" \
    -Dpxr_ROOT=$CMAKES \
    -DBoost_ROOT="$DEPS/boost" \
    -DOpenImageDenoise_ROOT=$CMAKES \
    -DOpenSubDiv_ROOT="$DEPS/opensubdiv" \
    -DEmbree_ROOT=$CMAKES \
    -DRandom123_INCLUDE_DIR="$DEPS/random123/include"
    
cmake --build . -- -j $(nproc)