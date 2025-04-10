#!/bin/bash

sudo -v
while true; do sudo -n true; sleep 60; done 2>/dev/null &

SCRIPT_DIR="$(dirname "$(realpath "$0")")"

DEFAULT_MFB_DIR="$SCRIPT_DIR/../mfb"

# Initialize variables with default values
MFB_DIR="$DEFAULT_MFB_DIR"

# Parse command-line arguments using getopts
while [[ "$#" -gt 0 ]]; do
    case $1 in
        --mfb-dir)
            MFB_DIR="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [--mfb-dir /path/to/mfb] [--blender-dir /path/to/blender]"
            exit 1
            ;;
    esac
done

echo "MFB_DIR: $MFB_DIR"

mkdir -p "$MFB_DIR"
cd "$MFB_DIR"

git clone --recurse-submodules https://github.com/dreamworksanimation/openmoonray.git $MFB_DIR/source

cp -r $SCRIPT_DIR/linux_x64 $MFB_DIR/source/building/linux_x64

git init $MFB_DIR/dependencies
cd $MFB_DIR/dependencies

git remote add -f origin https://projects.blender.org/blender/lib-linux_x64.git
git config core.sparseCheckout true

echo -e "imath\nmaterialx\nopencolorio\nopenexr\nopenimagedenoise\nopenimageio\nopensubdiv\nopenvdb\npython\nusd" > .git/info/sparse-checkout

git fetch origin

git checkout blender-v4.2-release

unset CMAKE_TOOLCHAIN_FILE



# Building dependencies

mkdir -p $MFB_DIR/dependencies/build
cd $MFB_DIR/dependencies/build

git clone https://github.com/open-source-parsers/jsoncpp.git
cd jsoncpp
mkdir ./build
cd ./build
cmake .. -DCMAKE_INSTALL_PREFIX=$MFB_DIR/dependencies
make -j$(nproc)
make install


# Building MoonrRay

mkdir -p $MFB_DIR/source/build
cd $MFB_DIR/source/build

cmake .. \
    -DCMAKE_C_COMPILER=/usr/bin/gcc \
    -DCMAKE_CXX_COMPILER=/usr/bin/g++ \
    -DCMAKE_PREFIX_PATH="$MFB_DIR/dependencies/*:$MFB_DIR/dependencies/" \
    -DPYTHON_EXECUTABLE="$MFB_DIR/dependencies/python/bin/python3" \
    -DBOOST_PYTHON_COMPONENT_NAME=python311 \
    -DABI_VERSION=0 \
    -Wno-dev \

cmake --build . -j $(nproc)
cmake --install . --prefix $MFB_DIR/openmoonray