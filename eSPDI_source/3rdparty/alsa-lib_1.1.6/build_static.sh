#!/bin/sh

ARCH=$1
PREFIX=$2
if [ -z $PREFIX ]; then
    PREFIX=$PWD/out
fi

echo "------------------------------------------------------------"
echo "Build Alsa Static"
echo "Arch : $ARCH"
echo "------------------------------------------------------------"

CONFIG_FLAGS="--prefix=$PREFIX --enable-static=yes --enable-shared=no --with-debug=no"
if [ $ARCH = "aarch64" ]; then
    CONFIG_FLAGS="$CONFIG_FLAGS --build=aarch64 --target=linux --host=x86_64"
elif [ $ARCH = "armhf" ] || [ $ARCH = "arm" ]; then
    CONFIG_FLAGS="$CONFIG_FLAGS --build=arm --target=linux --host=x86_64"
elif [ $ARCH != "x86_64" ]; then
    echo "No Support $ARCH."
    exit 1
fi

make clean
echo "ARGS :$CONFIG_FLAGS"
./gitcompile $CONFIG_FLAGS
make install