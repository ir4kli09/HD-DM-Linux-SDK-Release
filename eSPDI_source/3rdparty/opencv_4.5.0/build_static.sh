#!/bin/sh

# BUILD_TYPE=$1
BUILD_TYPE=$1
ARCH=$2
FFMPEG_PATH=$3
OPENCV_ROOT=$PWD
PREFIX=$4
if [ -z $PREFIX ]; then
    PREFIX=$OPENCV_ROOT
fi

echo "------------------------------------------------------------"
echo "Build OpenCV Static"
echo "Build Type : $BUILD_TYPE"
echo "Arch : $ARCH"
echo "OpenCV Root : $OPENCV_ROOT"
echo "FFMPEG Path : $PREFIX"
echo "Prefix : $FFMPEG_PATH"
echo "------------------------------------------------------------"

CMAKE_FLAGS="-DBUILD_SHARED_LIBS=OFF \
-DCMAKE_BUILD_TYPE=$BUILD_TYPE  \
-DOPENCV_FORCE_3RDPARTY_BUILD=ON \
-DCMAKE_INSTALL_PREFIX=$PREFIX  \
-DWITH_FFMPEG=OFF \
-DOPENCV_FFMPEG_SKIP_BUILD_CHECK=ON \
-DWITH_GSTREAMER=OFF \
-DBUILD_TESTS=OFF \
-DBUILD_PERF_TESTS=OFF \
-DBUILD_EXAMPLES=OFF \
-DBUILD_opencv_apps=OFF "

if [ $ARCH = "aarch64" ]; then
    CMAKE_FLAGS="$CMAKE_FLAGS -DCMAKE_TOOLCHAIN_FILE=$OPENCV_ROOT/platforms/linux/aarch64-gnu.toolchain.cmake"
elif [ $ARCH = "armhf" ]; then
    CMAKE_FLAGS="$CMAKE_FLAGS -DCMAKE_TOOLCHAIN_FILE=$OPENCV_ROOT/platforms/linux/arm-gnueabi.toolchain.cmake"
elif [ $ARCH = "arm" ]; then
    CMAKE_FLAGS="$CMAKE_FLAGS -DSOFTFP=ON -DCMAKE_TOOLCHAIN_FILE=$OPENCV_ROOT/platforms/linux/arm-gnueabi.toolchain.cmake"
elif [ $ARCH != "x86_64" ]; then
    echo "No Support $ARCH."
    exit 1
fi

echo "$CMAKE_FLAGS"

rm _build_$ARCH -rf
mkdir _build_$ARCH
cd _build_$ARCH

# export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$FFMPEG_PATH/lib
# export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:$FFMPEG_PATH/lib/pkgconfig
# export PKG_CONFIG_LIBDIR=$PKG_CONFIG_LIBDIR:$FFMPEG_PATH/lib

cmake $CMAKE_FLAGS ..

make install -j$(nproc)

cd .. 
rm _build_$ARCH -rf