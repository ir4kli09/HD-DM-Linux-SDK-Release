cmake_minimum_required(VERSION 3.8.2)

project(eYs3D_watchdog)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_subdirectory(rpclib-2.2.1 EXCLUDE_FROM_ALL)

aux_source_directory("${CMAKE_CURRENT_LIST_DIR}/src" SOURCE_FILES)

include_directories(${CMAKE_CURRENT_LIST_DIR}/rpclib-2.2.1/include)

add_library(${CMAKE_PROJECT_NAME} STATIC ${SOURCE_FILES} $<TARGET_OBJECTS:rpc>)

target_link_libraries(${CMAKE_PROJECT_NAME} -lpthread)

set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES PUBLIC_HEADER 
                      "src/WatchDog_Client.h;src/WatchDog_Server.h;src/common_def.h")

set(CMAKE_INSTALL_INCLUDEDIR "${CMAKE_CURRENT_LIST_DIR}/include")

install(TARGETS ${CMAKE_PROJECT_NAME}
        ARCHIVE
        DESTINATION "${CMAKE_CURRENT_LIST_DIR}/lib"
        PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
