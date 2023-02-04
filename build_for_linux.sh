#!/bin/bash

git submodule update --init --recursive

# build_type=Debug
build_type=Release
# build_type=RelWithDebInfo
is_build_test=OFF
is_install=ON
install_prefix=install
echo build type $build_type
echo build test switch $is_build_test
echo install switch $is_install
echo install prefix $install_prefix

rm -rf ./build-linux
mkdir -p build-linux
cd build-linux

cmake .. \
-DCMAKE_BUILD_TYPE=$build_type \
-DIS_BUILD_TEST=$is_build_test \
-DIS_BUILD_INSTALL=$is_install \
-DUSER_INSTALL_PREFIX=$install_prefix 

make -j6

if [ $is_install == ON ]
then
    make install
fi