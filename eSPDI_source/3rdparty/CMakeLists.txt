cmake_minimum_required(VERSION 3.8.2)

include(ExternalProject)

# set(3RDPARTY_BUILD_TYPE ${CMAKE_BUILD_TYPE})
set(3RDPARTY_BUILD_TYPE "Release")

option(WITH_BUILD_TURBOJPEG "Build TurboJpeg" ON)
option(WITH_BUILD_OPENCL "Build OpenCL" ON)
option(WITH_BUILD_ALSA "Build Alsa" ON)
option(WITH_BUILD_FFMPEG "Build FFMPEG" ON)
option(WITH_BUILD_OPENCV "Build OpenCV" ON)

set(3RDPARTY_DEPENDENCIES "")
if (NOT WITH_BUILD_3RDPARTY)
    if (EXISTS ${TURBOJPEG_INSTALL_PATH})
        set(WITH_BUILD_TURBOJPEG OFF)
    endif()

    if (EXISTS ${OPENCL_INSTALL_PATH})
        set(WITH_BUILD_OPENCL OFF)
    endif()

    if (EXISTS ${ALSA_INSTALL_PATH})
        set(WITH_BUILD_ALSA OFF)
    endif()

    if (EXISTS ${FFMPEG_INSTALL_PATH})
        set(WITH_BUILD_FFMPEG OFF)
    endif()

    if (EXISTS ${OPENCV_INSTALL_PATH})
        set(WITH_BUILD_OPENCV OFF)
    endif()
endif()

# build turbojpeg
if (WITH_BUILD_TURBOJPEG)
    ExternalProject_Add(libturbojpeg
                        SOURCE_DIR ${TURBOJPEG_SOURCE_DIR}
                        CMAKE_ARGS -DCMAKE_BUILD_TYPE=${3RDPARTY_BUILD_TYPE} -DCMAKE_INSTALL_PREFIX=${TURBOJPEG_INSTALL_PATH})
    set(3RDPARTY_DEPENDENCIES "${3RDPARTY_DEPENDENCIES} libturbojpeg")
endif()

# build OpenCL shared library

if (WITH_BUILD_OPENCL)   
    ExternalProject_Add(libopencl
                        SOURCE_DIR ${OPENCL_SOURCE_DIR}
                        CMAKE_ARGS -DCMAKE_BUILD_TYPE=${3RDPARTY_BUILD_TYPE} -DCMAKE_INSTALL_PREFIX=${OPENCL_INSTALL_PATH}
                        INSTALL_COMMAND make install && ${CMAKE_COMMAND} -E copy_directory "${OPENCL_SOURCE_DIR}/inc/CL" "${OPENCL_INSTALL_PATH}/include")
    set(3RDPARTY_DEPENDENCIES "${3RDPARTY_DEPENDENCIES} libopencl")
endif()

# build alsa-lib
if (WITH_BUILD_ALSA)
    execute_process(COMMAND ./build_static.sh ${ARCH} ${ALSA_INSTALL_PATH}
                    WORKING_DIRECTORY ${ALSA_SOURCE_DIR})
endif()

# build ffmpeg && OpenCV
if (WITH_BUILD_FFMPEG)
    execute_process(COMMAND ./build_static.sh ${3RDPARTY_BUILD_TYPE} ${ARCH} ${FFMPEG_INSTALL_PATH}
                    WORKING_DIRECTORY ${FFMPEG_SOURCE_DIR})
endif()

if (WITH_BUILD_OPENCV)
execute_process(COMMAND ./build_static.sh ${3RDPARTY_BUILD_TYPE} ${ARCH} ${FFMPEG_INSTALL_PATH} ${OPENCV_INSTALL_PATH}
				WORKING_DIRECTORY ${OPENCV_SOURCE_DIR})
endif()