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
rm bin/DMPreview_*
rm doc/* -rf

doxygen doxygen/doxygen.cfg

cd doc/latex
make
mv refman.pdf "../API Document HD-DM-LINUX-SDK-$VERSION.pdf"
cd ..
rm latex -rf
cd ..

current=$(shell pwd)
export STAGING_DIR=$(current)/tx2/toolchain/bin:$STAGING_DIR

echo "====================  Check toolchain enviroment  ===================="
if [ -f "/usr/local/qt5-tx2/qt-5.13.0/JetsonTX2/qt5-host/bin/qmake" ]; then
    echo "File qmake for Nvidia TX2 exist."
else
    echo "File qmake does not exists, unzip qt5-tx2 to /usr/local/"
    sudo tar -zxvf ./toolchain/tx2/qt5-tx2.tar.gz -C /usr/local/
fi

echo "=====================  Build NVIDIA TX2 64 SDK  ======================"

./project/nvidia/cmake_build_tx2_sdk_nodebug.sh $VERSION

echo "=====================  Build DMPreview  ======================"

rm -rf ./DMPreview/*.pro
cd DMPreview
rm build -r
mkdir build && cd build
cp ../../project/nvidia/DMPreview_TX2.pro ../DMPreview.pro

if [ -f "/usr/local/qt5-tx2/qt-5.13.0/JetsonTX2/qt5-host/bin/qmake" ]; then
    echo "--- use toolchain's qmake ---"
    /usr/local/qt5-tx2/qt-5.13.0/JetsonTX2/qt5-host/bin/qmake -makefile -o Makefile ../DMPreview.pro 
else
    echo "--- use platform qmake ---"
    qmake -makefile -o Makefile ../DMPreview.pro 
fi
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

cp DMPreview ../../bin/DMPreview_TX2
cd .. && rm build -r
cd ..

./project/nvidia/Pack_TX2_Release.sh $VERSION

