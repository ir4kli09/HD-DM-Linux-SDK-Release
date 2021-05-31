cmake_minimum_required(VERSION 3.8.2)

set(COMPUTER_LIBRARY_NAME computer)

aux_source_directory(${CMAKE_CURRENT_LIST_DIR} COMPUTER_SOURCE_FILES)

add_library(${COMPUTER_LIBRARY_NAME} STATIC ${COMPUTER_SOURCE_FILES})

set(COMPUTER_LINK_LIBRARIES OpenCL)

# if(ARCH STREQUAL "x86_64" OR ARCH STREQUAL "aarch64")  

#     add_definitions(-DCUDA_SUPPORT)

#     set(CUDA_TOOLKIT_ROOT_DIR "$ENV{EYS3D_ROOT}/toolchain/cuda-10.2")
    
#     execute_process(COMMAND rm "${CUDA_TOOLKIT_ROOT_DIR}/include" -r)
#     execute_process(COMMAND rm "${CUDA_TOOLKIT_ROOT_DIR}/lib64" -r)
#     if(ARCH STREQUAL "x86_64")
#         set(CUDA_ARCH_DIR x86_64-linux)
#     elseif(ARCH STREQUAL "aarch64")
#         set(CUDA_ARCH_DIR aarch64-linux)
#     endif()  
#     execute_process(COMMAND ln -s "targets/${CUDA_ARCH_DIR}/include" include
#                     WORKING_DIRECTORY ${CUDA_TOOLKIT_ROOT_DIR})
#     execute_process(COMMAND ln -s "targets/${CUDA_ARCH_DIR}/lib" lib64
#                     WORKING_DIRECTORY ${CUDA_TOOLKIT_ROOT_DIR})
                  
#     list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
#     find_package(CUDA 10.2 EXACT REQUIRED)

#     set(CUDA_LIBRARIES ${CUDA_cudart_static_LIBRARY} -lrt -lpthread)
#     include_directories(${CUDA_INCLUDE_DIRS})

#     set(CUDA_NVCC_FLAGS ${CUDA_NVCC_FLAGS}; -ccbin ${CMAKE_CXX_COMPILER} --compiler-options -fPIC -std=c++14)

#     if (CPU STREQUAL "NVIDIA")
#         set(CUDA_NVCC_FLAGS ${CUDA_NVCC_FLAGS}; -gencode arch=compute_62,code=sm_62)   
#     endif()

#     if (CMAKE_BUILD_TYPE MATCHES "Debug")
#         set(CUDA_NVCC_FLAGS "${CUDA_NVCC_FLAGS} -g -G --resource-usage -v -lineinfo")
#     else()
#         set(CUDA_NVCC_FLAGS "${CUDA_NVCC_FLAGS} -O3")
#     endif()

#     set(CUDA_SOURCES CComputer_Cuda.cu)
#     cuda_add_library(computer_cuda STATIC ${CUDA_SOURCES})

#     set_target_properties(computer_cuda PROPERTIES COMPILE_FLAGS
#     "-DTHRUST_DEVICE_SYSTEM=THRUST_DEVICE_SYSTEM_CUDA")
#     set_target_properties(computer_cuda PROPERTIES CUDA_SEPERABLE_COMPILATION
#     ON)

#     set_target_properties( ${COMPUTER_LIBRARY_NAME}
#                            PROPERTIES CUDA_SEPARABLE_COMPILATION ON)

#     set(COMPUTER_LINK_LIBRARIES ${COMPUTER_LINK_LIBRARIES} computer_cuda)

# endif()

target_link_libraries(${COMPUTER_LIBRARY_NAME} ${COMPUTER_LINK_LIBRARIES})