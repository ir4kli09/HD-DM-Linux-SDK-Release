cmake_minimum_required(VERSION 3.8.2)

set(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_RPATH}:$ORIGIN/../../../eSPDI/)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

add_executable(${CMAKE_PROJECT_NAME} 
                 main.cpp
                 testModule.cpp
                 sqlite3/sqlite3.c
                 ModeConfig.cpp
                 FPSCalculator.cpp
               )

target_link_libraries(${CMAKE_PROJECT_NAME} 
                      gmock_main gmock gtest_main gtest pthread
                      dl
                      eSPDI_X86_64)

install(TARGETS ${CMAKE_PROJECT_NAME}
        RUNTIME DESTINATION ${PROJECT_SOURCE_DIR}/bin/)