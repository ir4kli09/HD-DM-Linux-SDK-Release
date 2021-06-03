#!/bin/sh

BUILD_QT=$1

rm ./eSPDI/libeSPDI*
rm ./console_tester/test*
rm ./bin/DMPreview*

echo "===================="
echo "Build libeSPDI_X86_64 testing"
echo "===================="

sh ./project/x86_64/cmake_build_x86_sdk.sh

if [ -n "${BUILD_QT}" ]
then
cd DMPreview
mv DMPreview.pro bak_DMPreview.pro
rm build -r
mkdir build && cd build
cp ../../project/x86_64/DMPreview.pro ../DMPreview.pro
qmake -makefile -o Makefile ../DMPreview.pro
make cleancd 
make
cp DMPreview ../../bin/DMPreview_X86
cd .. && rm build -r
mv bak_DMPreview.pro DMPreview.pro
cd ..
fi

echo "===================="
echo "Build libeSPDI_NVIDIA_64 testing"
echo "===================="

current=$(shell pwd)
export STAGING_DIR=$(current)/tx2/toolchain/bin:$STAGING_DIR

./project/nvidia/cmake_build_tx2_sdk_nodebug.sh

if [ -n "${BUILD_QT}" ]
then
cd DMPreview
mv DMPreview.pro bak_DMPreview.pro
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

make clean
make -j8

cp DMPreview ../../bin/DMPreview_TX2
cd .. && rm build -r
mv bak_DMPreview.pro DMPreview.pro
cd ..
fi

echo "===================="
echo "Build libeSPDI_hypatia_32 testing"
echo "===================="

current=$(shell pwd)
export STAGING_DIR=$(current)/hypatia/toolchain-sunxi-glibc/toolchain/bin:$STAGING_DIR
./project/hypatia/cmake_build_hypatia_sdk_nodebug.sh $VERSION

echo "===================="
echo "Build libeSPDI_TI_32 testing"
echo "===================="

if [ -d "/opt/ti-processor-sdk-linux-dra7xx-evm" ]; then
    echo "Directory /opt/ti-processor-sdk-linux-dra7xx-evm exists."
else
    echo "Directory /opt/ti-processor-sdk-linux-dra7xx-evm does not exists. copy!"
    sudo tar -zxvf ./toolchain/ti/ti-processor-sdk-linux-dra7xx-evm.tar.gz -C /opt/
fi

cd toolchain/ti/
. ./environment-setup
cd ../../

./project/ti/cmake_build_ti_sdk_nodebug.sh

if [ -n "${BUILD_QT}" ]
then
cd DMPreview
mv DMPreview.pro bak_DMPreview.pro
rm build -r
mkdir build && cd build
cp ../../project/ti/DMPreview_TI.pro ../DMPreview.pro
qmake -makefile -o Makefile ../DMPreview.pro 
make clean
make -j8
cp DMPreview ../../bin/DMPreview_TI
cd .. && rm build -r
mv bak_DMPreview.pro DMPreview.pro
cd ..
fi

# check result
./test/build_test/check_result.sh

rm ./eSPDI/libeSPDI*
rm ./console_tester/test*
rm ./bin/DMPreview*