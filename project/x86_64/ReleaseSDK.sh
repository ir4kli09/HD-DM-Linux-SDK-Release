#!/bin/sh
VERSION=$1
ONLY_BUILD_X86=$2

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

if [ $ONLY_BUILD_X86 = "FALSE" ]
then
    #save target file to temporary folder
    mkdir .temp_folder
fi

echo "========================  Build X86-64  ========================="
echo "Build X86 64 with OpenCL Lib .."
./project/x86_64/cmake_build_x86_sdk_nodebug.sh $VERSION
if [ $? -gt 0 ]; then
    echo Build break;
    return 1;
fi

if [ $ONLY_BUILD_X86 = "FALSE" ]
then
cp ./console_tester/test_x86 ./.temp_folder
fi

rm -rf ./DMPreview/*.pro

cd DMPreview
rm build -r
mkdir build && cd build
cp ../../project/x86_64/DMPreview.pro ../DMPreview.pro
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
cp DMPreview ../../bin/DMPreview_X86
cd .. && rm build -r
cd ..

if [ $ONLY_BUILD_X86 = "FALSE" ]
then
cp ./bin/DMPreview_X86 ./.temp_folder

echo "========================  Build Nvidia TX2 ======================"
echo "================  Check TX2 toolchain enviroment  ==============="
if [ -f "/usr/local/qt5-tx2/qt-5.13.0/JetsonTX2/qt5-host/bin/qmake" ]; then
    echo "File qmake for Nvidia TX2 exist."
else
    echo "File qmake does not exists, unzip qt5-tx2 to /usr/local/"
    sudo tar -zxvf ./toolchain/tx2/qt5-tx2.tar.gz -C /usr/local/
fi

echo "===================  Build NVIDIA TX2 64 SDK  ==================="
./project/nvidia/cmake_build_tx2_sdk_nodebug.sh $VERSION

echo "=================  Build NVIDIA TX2 DMPreview  =================="
cd DMPreview
rm build -r
mkdir build && cd build
cp ../../project/nvidia/DMPreview_TX2.pro ../DMPreview.pro

if [ -f "/usr/local/qt5-tx2/qt-5.13.0/JetsonTX2/qt5-host/bin/qmake" ]; then
    echo "--- use toolchain's qmake ---"
    /usr/local/qt5-tx2/qt-5.13.0/JetsonTX2/qt5-host/bin/qmake -makefile "LIBS += -L../../eSPDI -L../../eSPDI/opencv/TX2/lib/ -L../../eSPDI/turbojpeg/TX2/lib/ -L../../eSPDI/DepthFilter/TX2/lib/" -o Makefile ../DMPreview.pro 
else
    echo "--- use platform qmake ---"
    qmake -makefile "LIBS += -L../../eSPDI -L../../eSPDI/opencv/TX2/lib/ -L../../eSPDI/turbojpeg/TX2/lib/ -L../../eSPDI/DepthFilter/TX2/lib/" -o Makefile ../DMPreview.pro 
fi

make clean
make
if [ $? -gt 0 ]; then
    echo DMPreview make Fail TX2
    return 1
fi
cp DMPreview ../../bin/DMPreview_TX2
cd .. && rm build -r
cd ..

#copy qt5 profile to DMPreview
cp ./project/x86_64/DMPreview.pro ./DMPreview
cp ./project/nvidia/DMPreview_TX2.pro ./DMPreview
cp ./.temp_folder/DMPreview_X86 ./bin
cp ./.temp_folder/test_x86 ./console_tester/
rm -rf ./.temp_folder
fi

./project/x86_64/Pack_Release.sh $VERSION
