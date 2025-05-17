git clone --recurse-submodules https://github.com/dreamworksanimation/openmoonray.git ./source

git clone https://projects.blender.org/blender/lib-linux_x64.git --branch blender-v4.1-release ./deps

mkdir ./build
cd ./build

cmake ../building/RHEL9 -DInstallRoot="../deps"
cmake --build . -- -j $(nproc)