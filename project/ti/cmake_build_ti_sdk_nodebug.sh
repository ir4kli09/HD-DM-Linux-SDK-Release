#!/bin/bash

VER=$1

cd toolchain
source setup_toolchain_environment.sh ti
cd ..

cd eSPDI_source

rm build -r
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCPU=TI -DBITS=32 -DVER=${VER} ..
make clean
make -j$(nproc)
if [ $? -gt 0 ]; then
    echo Build eSPDI_source Fail.
fi

echo Build eSPDI_source status `$?`

make install -j$(nproc)
if [ $? -gt 0 ]; then
    echo Install eSPDI_source Fail.
fi

cd ../../console_tester
rm -rf ./out_img/*.*
make VER=$VER CPU=TI BITS=32 clean
make VER=$VER CPU=TI BITS=32
if [ $? -gt 0 ]; then
    echo Build console_tester Fail.
fi
