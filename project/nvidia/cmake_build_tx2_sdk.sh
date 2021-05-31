#!/bin/bash

VER=$1

cd toolchain
source setup_toolchain_environment.sh tx2
cd ..

cd eSPDI_source
rm build -r
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug -DCPU=NVIDIA -DBITS=64 -DVER=${VER} -DUNITY=FALSE ..
make clean
make -j$(nproc)
if [ $? -gt 0 ]; then
    echo Build eSPDI_source Fail.
fi

echo Build eSPDI_source status `$?`

make install
if [ $? -gt 0 ]; then
    echo Install eSPDI_source Fail.
fi

cd ../../console_tester
rm -rf ./out_img/*.*
make VER=$VER CPU=NVIDIA BITS=64 clean
make VER=$VER CPU=NVIDIA BITS=64
if [ $? -gt 0 ]; then
    echo Build console_tester Fail.
fi
