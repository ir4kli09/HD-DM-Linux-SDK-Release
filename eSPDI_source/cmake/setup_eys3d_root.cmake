if (NOT DEFINED ENV{EYS3D_ROOT})
	set(ENV{EYS3D_ROOT} "${CMAKE_CURRENT_LIST_DIR}/../..")
	message(STATUS "EYS3D_ROOT: $ENV{EYS3D_ROOT}")
endif()

set(ESPDI_SOURCE_PATH "$ENV{EYS3D_ROOT}/eSPDI_source")

if (NOT DEFINED ARCH)
	include("${CMAKE_CURRENT_LIST_DIR}/TargetArch.cmake")
	target_architecture(ARCH)
	message(STATUS "ARCH : ${ARCH}")
endif()