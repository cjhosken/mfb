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

rm -rf $MFB_DIR/dependencies
mkdir $MFB_DIR/dependencies
mkdir $MFB_DIR/dependencies/bin

rm -rf $MFB_DIR/source/building/RHEL9
cp -r $SCRIPT_DIR/RHEL9 $MFB_DIR/source/building/RHEL9

sudo -s source $MFB_DIR/source/building/RHEL9/install_packages.sh

rm -rf $MFB_DIR/build
mkdir $MFB_DIR/build
cd $MFB_DIR/build

cmake $MFB_DIR/source/building/RHEL9 -DInstallRoot="$MFB_DIR/dependencies"
cmake --build $MFB_DIR/build -j $(nproc)

rm -rf $MFB_DIR/build/*
cd $MFB_DIR/dependencies

cp $SCRIPT_DIR/optix.sh $MFB_DIR/dependencies/optix.sh
chmod +x $MFB_DIR/dependencies/optix.sh
bash $MFB_DIR/dependencies/optix.sh --skip-license --exclude-subdir


cp $SCRIPT_DIR/CMakePresets.json $MFB_DIR/source/CMakePresets.json

cp -r $SCRIPT_DIR/configs/linux_x64/* $MFB_DIR/dependencies
mkdir $MFB_DIR/dependencies/bl_deps
cp -r $SCRIPT_DIR/bl_deps/linux_x64/* $MFB_DIR/dependencies/bl_deps

export PATH=/usr/local/cuda/bin:$PATH
export LD_LIBRARY_PATH=/usr/local/cuda/lib64:$MFB_DIR/dependencies/bl_deps/python/lib:$MFB_DIR/dependencies/bl_deps/boost/lib:$MFB_DIR/dependencies/bl_deps/materialx/lib:$MFB_DIR/dependencies/bl_deps/opensubdiv/lib:$MFB_DIR/dependencies/bl_deps/openimageio/lib:$MFB_DIR/dependencies/bl_deps/openvdb/lib:$MFB_DIR/dependencies/bl_deps/openexr/lib:$MFB_DIR/dependencies/bl_deps/imath/lib


unset PYTHONPATH
unset PYTHONHOME

cmake $MFB_DIR/source --preset linux-blender-release
cmake --build $MFB_DIR/build -j $(nproc)

rm -rf $MFB_DIR/installs
mkdir $MFB_DIR/installs
mkdir $MFB_DIR/installs/openmoonray
cmake --install $MFB_DIR/build --prefix $MFB_DIR/installs/openmoonray

source $MFB_DIR/installs/openmoonray/scripts/setup.sh

SOURCE_LINE="export MFB_DIR=$HOME/.mfb; source $MFB_DIR/installs/openmoonray/scripts/setup.sh; export LD_LIBRARY_PATH=/usr/local/cuda/lib64:$MFB_DIR/dependencies/bl_deps/python/lib:$MFB_DIR/dependencies/bl_deps/boost/lib:$MFB_DIR/dependencies/bl_deps/materialx/lib:$MFB_DIR/dependencies/bl_deps/opensubdiv/lib:$MFB_DIR/dependencies/bl_deps/openimageio/lib:$MFB_DIR/dependencies/bl_deps/openvdb/lib:$MFB_DIR/dependencies/bl_deps/openexr/lib:$MFB_DIR/dependencies/bl_deps/imath/lib:$LD_LIBRARY_PATH;"

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

cd $SCRIPT_DIR

kill $!
