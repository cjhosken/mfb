DIR=$(dirname "$(realpath "$0")")

mkdir -p $HOME/.mfb
mkdir -p $HOME/.mfb/source

cd $HOME/.mfb/source

git clone --recurse-submodules https://github.com/dreamworksanimation/openmoonray.git

git clone https://projects.blender.org/blender/lib-linux_x64.git --branch blender-v4.1-release $HOME/.mfb/source/deps

mkdir $HOME/.mfb/source/build
cd $HOME/.mfb/source/build

rm -rf *

cp -r $DIR/building/isph/* $HOME/.mfb/source/deps/embree/include/embree4

cmake $DIR/building/RHEL9 -DInstallRoot="$HOME/.mfb/source/deps"
cmake --build . -- -j $(nproc)

rm -rf $HOME/.mfb/source/build/*