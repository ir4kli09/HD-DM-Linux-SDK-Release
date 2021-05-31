cmake_minimum_required(VERSION 3.8.2)

include_directories(include)
set(DEPTH_FILTER_LIBRARY_NAME depthFilter)

set(DEPTH_FILTER_SOURCE_FILES 
    src/DepthmapFilter.cpp
)

add_library(${DEPTH_FILTER_LIBRARY_NAME} STATIC ${DEPTH_FILTER_SOURCE_FILES})
target_link_libraries(${DEPTH_FILTER_LIBRARY_NAME} OpenCL)