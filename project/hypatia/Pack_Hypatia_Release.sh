#!/bin/bash

VERSION=$1
RELEASE_TITLE="HD-DM-Linux-SDK-Hypatia"

STANDARD_NAME="${RELEASE_TITLE}-${VERSION}"

rm -r out
rm -r sdk_out/${STANDARD_NAME}

mkdir -p out/Audio
mkdir -p out/image
mkdir -p out/IMULog
mkdir -p out/ReadRegisterLog
mkdir -p out/RectifyLog
mkdir -p sdk_out/${STANDARD_NAME}
cd sdk_out/${STANDARD_NAME}
cp ../../image . -r
cp ../../eSPDI . -r
cp ../../util . -r
cp ../../doc . -r
cp ../../cfg . -r
cp ../../out . -r
cp ../../console_tester . -r
rm console_tester/Makefile
mv console_tester/Makefile.out console_tester/Makefile
cp ../../toolchain/hypatia/ . -r
cp ../../Buid_Environment/setup_env.sh ./DMPreview/ -r
cp ../../Buid_Environment/check_opencl.txt ./DMPreview/ -r
cp ../../project/hypatia/readme.txt . -r

cd ..

#tar.gz folder
rm -rf ${STANDARD_NAME}.tar.gz
tar zcvf ${STANDARD_NAME}.tar.gz ${STANDARD_NAME}
