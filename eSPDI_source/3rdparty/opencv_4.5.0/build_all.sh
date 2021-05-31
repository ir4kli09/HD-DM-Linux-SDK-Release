#!/bin/sh

rm out -rf

BUILD_TYPE=$1

if [ -z $BUILD_TYPE ] || ( [ $BUILD_TYPE != "Debug" ] && [ $BUILD_TYPE != "Release" ]); then
    echo "Please type build type( Debug / Release ) on first argument. ex: ./build_all.sh Debug"
    exit 2
fi

./build_static.sh $BUILD_TYPE x86_64
./build_static.sh $BUILD_TYPE aarch64
./build_static.sh $BUILD_TYPE armhf
./build_static.sh $BUILD_TYPE arm