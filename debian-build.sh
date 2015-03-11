#!/bin/bash
#A script to build working executables for debian

#WARNING:  This will not work on debian wheezy.
#          The network code was partially rewritten to use SFML2 which is not available on that distribution

DEV_PACKAGES=( zlib1g-dev libpng-dev libgl1-mesa-dev libsdl1.2-dev libsfml-dev libgtkmm-2.4-dev libopenal-dev libwxgtk2.8-dev libgtkglextmm-x11-1.2-dev )

BUILD_PACKAGES=( build-essential cmake git ${DEV_PACKAGES[*]} zip )

BUILD_DIRECTORY="build"

sudo apt-get update
sudo apt-get install ${BUILD_PACKAGES[*]}

if [ -d "$BUILD_DIRECTORY" ]; then
  rm -r "$BUILD_DIRECTORY"
fi
mkdir "$BUILD_DIRECTORY"
cd build
# cmake -DCMAKE_BUILD_TYPE=Debug ..
#scan-build cmake -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang ..
cmake ..
make
#scan-build make
