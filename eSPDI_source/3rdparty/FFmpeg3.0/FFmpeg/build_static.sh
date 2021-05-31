#!/bin/sh

BUILD_TYPE=$1
ARCH=$2
PREFIX=$3
if [ -z $PREFIX ]; then
    PREFIX=$PWD
fi

echo "------------------------------------------------------------"
echo "Build FFMPEG Static"
echo "Build Type : $BUILD_TYPE"
echo "Arch : $ARCH"
echo "Prefix : $PREFIX"
echo "------------------------------------------------------------"

CONFIG_FLAGS="--disable-yasm --enable-avresample --enable-pic --prefix=$PREFIX"

if [ $ARCH = "aarch64" ]; then
    CONFIG_FLAGS="$CONFIG_FLAGS --arch=aarch64 --enable-cross-compile --target-os=linux"
elif [ $ARCH = "armhf" ] || [ $ARCH = "arm" ]; then
    CONFIG_FLAGS="$CONFIG_FLAGS --arch=arm --enable-cross-compile --target-os=linux"
elif [ $ARCH != "x86_64" ]; then
    echo "No Support $ARCH."
    exit 1
fi

if [ $BUILD_TYPE = "Release" ]; then
    CONFIG_FLAGS="$CONFIG_FLAGS --disable-debug"
fi

echo "$CONFIG_FLAGS"

./configure $CONFIG_FLAGS
make clean
make -j$(nproc)
make install 
make clean
