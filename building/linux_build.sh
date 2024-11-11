#!/bin/bash

sudo -v
while true; do sudo -n true; sleep 60; done 2>/dev/null &

SCRIPT_DIR="$(dirname "$(realpath "$0")")"

DEFAULT_MFB_DIR="$HOME/.mfb"

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

echo -e "boost\nimath\nmaterialx\nopencolorio\nopenexr\nopenimagedenoise\nopenimageio\nopensubdiv\nopenvdb\npython\nusd" > .git/info/sparse-checkout

git fetch origin

git checkout 483736b00b6a767342e30f5bd95eebcc3c6a4219

mkdir -p $MFB_DIR/dependencies/bin
sudo -s source $MFB_DIR/source/building/linux_x64/install_packages.sh

mkdir -p $MFB_DIR/source/build
cd $MFB_DIR/source/build

cmake $MFB_DIR/source/building/linux_x64 -DInstallRoot="$MFB_DIR/dependencies"
cmake --build . -j $(nproc)

rm -rf *

cp $SCRIPT_DIR/CMakePresets.json $MFB_DIR/source/CMakePresets.json
cp -r $SCRIPT_DIR/configs/linux_x64/* $MFB_DIR/dependencies

chmod +x $MFB_DIR/dependencies/optix.sh
bash $MFB_DIR/dependencies/optix.sh --skip-license --exclude-subdir

export PATH=/usr/local/cuda/bin:$PATH
LD_LIBRARY_PATH=/usr/local/cuda/lib64:$BTOA_DIR/dependencies/boost/lib:$BTOA_DIR/dependencies/materialx/lib:$BTOA_DIR/dependencies/imath/lib:$BTOA_DIR/dependencies/openvdb/lib:$BTOA_DIR/dependencies/opensubdiv/lib:$BTOA_DIR/dependencies/openimageio/lib:$LD_LIBRARY_PATH

unset PYTHONPATH
unset PYTHONHOME

cmake .. --preset linux-blender-release -Wno-dev
cmake --build . -j $(nproc)

cmake --install . --prefix $MFB_DIR/openmoonray

source $MFB_DIR/openmoonray/scripts/setup.sh

SOURCE_LINE="export MFB_DIR=$HOME/.mfb; source $MFB_DIR/openmoonray/scripts/setup.sh; export LD_LIBRARY_PATH=/usr/local/cuda/lib64:$MFB_DIR/dependencies/bl_deps/python/lib:$MFB_DIR/dependencies/bl_deps/boost/lib:$MFB_DIR/dependencies/bl_deps/materialx/lib:$MFB_DIR/dependencies/bl_deps/opensubdiv/lib:$MFB_DIR/dependencies/bl_deps/openimageio/lib:$MFB_DIR/dependencies/bl_deps/openvdb/lib:$MFB_DIR/dependencies/bl_deps/openexr/lib:$MFB_DIR/dependencies/bl_deps/imath/lib:$LD_LIBRARY_PATH;"

# The .bashrc file path
BASHRC_PATH="$HOME/.bashrc"

# Check if the line already exists in .bashrc
if grep -Fxq "$SOURCE_LINE" "$BASHRC_PATH"; then
    echo "The line already exists in .bashrc."
else
    # Ensure the file ends with a newline before adding the new line
    echo >> "$BASHRC_PATH"

    # Add the source line to .bashrc
    echo "$SOURCE_LINE" >> "$BASHRC_PATH"
    echo "The line has been added to .bashrc."
fi

echo "MoonRay has been built at $MFB_DIR"