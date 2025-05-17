DIR=$(dirname "$(realpath "$0")")

mkdir -p $HOME/.mfb/source/build
cd $HOME/.mfb/source/build

DEPS="$HOME/.mfb/source/deps"
CMAKES="$DIR/cmake"

cmake ../openmoonray \
    -DCMAKE_PREFIX_PATH=$DEPS:$CMAKES \
    -DCMAKE_MODULE_PATH="$DIR/cmake" \
    -DPYTHON_EXECUTABLE="$DEPS/python/bin/python311" \
    -DBOOST_PYTHON_COMPONENT_NAME=python311 \
    -DABI_VERSION=0 \
    -DJsonCpp_ROOT="$DEPS/jsoncpp" \
    -DOPENVDB_ROOT="$DEPS/openvdb" \
    -DCMAKE_ISPC_COMPILER="$DEPS/ispc/bin/ispc" \
    -DOpenEXR_ROOT=$CMAKES \
    -DOpenImageIO_ROOT=$CMAKES \
    -DPython_ROOT="$DEPS/python" \
    -Dpxr_ROOT=$CMAKES \
    -DMaterialX_DIR=$CMAKES \
    -DOpenVDB_DIR=$CMAKES \
    -DOpenImageDenoise_ROOT="$DEPS/openimagedenoise" \
    -DOpenSubDiv_ROOT="$DEPS/opensubdiv" \
    -DOpenSubdiv_DIR="$CMAKES" \
    -DEmbree_DIR=$CMAKES \
    -DRandom123_INCLUDE_DIR="$DEPS/random123/include" \
    -Dsycl_DIR=$CMAKES \
    -DBOOST_INCLUDEDIR="$DEPS/boost/include" \
    -DBOOST_LIBRARYDIR="$DEPS/boost/lib" \
    -DMOONRAY_USE_OPTIX=NO
    
cmake --build . -- -j $(nproc)