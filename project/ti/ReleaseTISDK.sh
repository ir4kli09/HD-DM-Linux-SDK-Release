#!/bin/sh
VERSION=$1
if [ -z "$VERSION" ]
  then
    echo "No argument version"
    VERSION=beta
fi
#svn update

git submodule init	
git submodule update	
git submodule foreach --recursive git pull origin master

if [ -d "/opt/ti-processor-sdk-linux-dra7xx-evm" ]; then
    echo "Directory /opt/ti-processor-sdk-linux-dra7xx-evm exists."
else
    echo "Directory /opt/ti-processor-sdk-linux-dra7xx-evm does not exists. copy!"
    sudo tar -zxvf ./toolchain/ti/ti-processor-sdk-linux-dra7xx-evm.tar.gz -C /opt/
fi

cd toolchain/ti/
. ./environment-setup
cd ../../

rm eSPDI/libeSPDI_*
rm eSPDI/*.h
rm bin/DMPreview_*
rm doc/* -rf

doxygen doxygen/doxygen.cfg

cd doc/latex
make
mv refman.pdf "../API Document HD-DM-LINUX-SDK-$VERSION.pdf"
cd ..
rm latex -rf
cd ..

echo "Build TI 32.."
./project/ti/cmake_build_ti_sdk_nodebug.sh $VERSION

echo "=====================  Build DMPreview  ======================"
rm -rf ./DMPreview/*.pro
cd DMPreview
rm build -r
mkdir build && cd build
cp ../../project/ti/DMPreview_TI.pro ../DMPreview.pro
qmake -makefile -o Makefile ../DMPreview.pro
if [ $? -gt 0 ]; then
    echo DMPreview QMake Fail
    return 1
fi
make clean
make
if [ $? -gt 0 ]; then
    echo DMPreview make Fail
    return 1
fi
cp DMPreview ../../bin/DMPreview_TI
cd .. && rm build -r
cd ..

./project/ti/Pack_TI_Release.sh $VERSION

