cmake_minimum_required(VERSION 3.8.2)

# set the project name
project(EtronDI_Linux)

# enviroment setting
include(cmake/install_dependency_package.cmake)
include(cmake/write_version_header.cmake)

# set library version
write_version_header_file(${CMAKE_CURRENT_LIST_DIR}/include/eSPDI_version.h VER)

#include(cmake/update_git_submodules.cmake)
include(cmake/set_compiler_flag.cmake)
include(cmake/set_include_directories.cmake)

if(UNITY STREQUAL "TRUE")
	message("Build unity log callback for debug.")
	add_definitions("-DUNITY_DEBUG_LOG")
	set(SOURCE_FILES ${SOURCE_FILES} DebugCPP.cpp)
endif()

add_subdirectory(3rdparty)
add_subdirectory(computer)
add_subdirectory(DepthFilter)
add_subdirectory(post_process)

include(ExternalProject)
ExternalProject_Add(watchdog_lib
    				SOURCE_DIR ${WATCHDOG_DIR}
					CMAKE_ARGS -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
)

set(CMAKE_INSTALL_RPATH $ORIGIN)
set(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_RPATH}:$ORIGIN/OpenCL/${ARCH}/lib)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

set(SOURCE_FILES 
	src/EtronDI.cpp
	src/InterruptHandle.cpp
	src/eSPDI.cpp
	src/eSPDI_PostProcess.cpp
	src/videodevice.cpp
	src/DataEncrypt.cpp
	src/En-Decrypt.cpp
	src/Mp4FileUtility.cpp
	${FISHEYE_DIR}/fisheye360_api_cc.cpp
	src/uac.cpp
	src/DepthMerge.cpp
	src/CVideoDeviceGroup.cpp
	src/CVideoDevicePool.cpp
	src/utEtronVideoDeviceVertifyManager.cpp
)

# set shared library name
set(SHARED_LIB_NAME "eSPDI_${CPU}_${BITS}")

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# add the shared libraray
add_library(${SHARED_LIB_NAME} SHARED ${SOURCE_FILES})

add_dependencies(computer watchdog_lib ${3RDPARTY_DEPENDENCIES})
add_dependencies(${SHARED_LIB_NAME} computer)

find_package(Eigen3 3.3.9 PATHS ${EIGEN_DIR} NO_DEFAULT_PATH)
find_package(OpenCV 4.5.0 PATHS ${OPENCV_INSTALL_PATH} NO_DEFAULT_PATH REQUIRED)

target_link_libraries(${SHARED_LIB_NAME} PRIVATE
										 asound 
										 computer 
										 depthFilter 
										 eYs3D_watchdog 
										 turbojpeg
										 post_process
										 -Wl,--whole-archive						 
										 ${OpenCV_LIBS}
										 -Wl,--no-whole-archive
										 )

set_target_properties(${SHARED_LIB_NAME}
    PROPERTIES
    VERSION     ${VER}
	PUBLIC_HEADER "include/eSPDI.h;include/eSPDI_def.h;include/eSPDI_version.h"
    )

install(
TARGETS ${SHARED_LIB_NAME}
LIBRARY DESTINATION "$ENV{EYS3D_ROOT}/eSPDI/"
PUBLIC_HEADER DESTINATION "$ENV{EYS3D_ROOT}/eSPDI/"
)
