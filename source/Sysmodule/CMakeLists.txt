cmake_minimum_required(VERSION 3.14)
project(SysConModule VERSION 1.0.0)

set (CMAKE_CXX_STANDARD 20)

file(GLOB SRC_FILES 
    ${PROJECT_SOURCE_DIR}/source/config_handler.cpp 
    ${PROJECT_SOURCE_DIR}/source/logger.cpp
    ${PROJECT_SOURCE_DIR}/../Ini/ini.c)

file(GLOB HEADERS_FILES ${PROJECT_SOURCE_DIR}/source/*.h)
file(GLOB HEADERS_FILES ${PROJECT_SOURCE_DIR}/../Ini/*.h)

add_library(SysConModule ${SRC_FILES} ${HEADERS_FILES})

target_link_libraries(SysConModule PRIVATE SysConControllerLib)

target_include_directories(SysConModule PUBLIC ${PROJECT_SOURCE_DIR}/source ${PROJECT_SOURCE_DIR}/../Ini)