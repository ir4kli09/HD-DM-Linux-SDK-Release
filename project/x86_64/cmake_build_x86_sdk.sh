#!/bin/sh
VER=$1

cd eSPDI_source

rm build -r
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug -DCPU=X86 -DBITS=64 -DVER=${VER} -DUNITY=FALSE ..
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

echo "build x86 console test"
cd ../../console_tester
rm -rf ./out_img/*.*
make VER=$VER CPU=X86 BITS=64 clean
make VER=$VER CPU=X86 BITS=64
if [ $? -gt 0 ]; then
    echo Build console_tester Fail.
fi
