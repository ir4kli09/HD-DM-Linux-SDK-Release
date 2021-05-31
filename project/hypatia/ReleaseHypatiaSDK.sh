#!/bin/sh
VERSION=$1
if [ -z "$VERSION" ]
  then
    echo "No argument version"
    VERSION=beta
fi
#svn update

rm eSPDI/libeSPDI_*
rm eSPDI/*.h
rm bin/DMPreview
rm doc/* -rf

doxygen doxygen/doxygen.cfg

cd doc/latex
make
mv refman.pdf "../API Document HD-DM-LINUX-SDK-$VERSION.pdf"
cd ..
rm latex -rf
cd ..

current=$(shell pwd)
export STAGING_DIR=$(current)/hypatia/toolchain-sunxi-glibc/toolchain/bin:$STAGING_DIR

echo "Build hypatia 32.."
./project/hypatia/cmake_build_hypatia_sdk_nodebug.sh $VERSION
if [ $? -gt 0 ]; then
    echo Build hypatia fail do not pack
    return 1
fi
./project/hypatia/Pack_Hypatia_Release.sh $VERSION

