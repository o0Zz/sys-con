cmake_minimum_required(VERSION 3.14)
project(SysConModule VERSION 1.0.0)

# Only the platform-independent part of the sysmodule is host-buildable today.
# usb_module.cpp, controller_handler.cpp, psc_module.cpp and everything in src/platform/
# include <switch.h> and are compiled by the device build only, so they have no test
# coverage. See doc/ARCHITECTURE.md.
set(SRC_FILES
    config_handler.cpp
    logger.cpp
)

# Listed only so they show up in IDE project trees.
set(HEADER_FILES
    config_handler.h
    logger.h
    ../platform/IFileManager.h
    ../platform/libnx/StdFileManager.h
)

add_library(SysConModule ${SRC_FILES} ${HEADER_FILES})

syscon_set_standard(SysConModule)
target_link_libraries(SysConModule PRIVATE syscon::warnings)

target_link_libraries(SysConModule PUBLIC SysConControllerLib)
target_link_libraries(SysConModule PRIVATE SysConIni)

target_include_directories(SysConModule PUBLIC
    ${PROJECT_SOURCE_DIR}
    # IFileManager.h is the app<->platform seam and is reached from config_handler.h via
    # logger.h, so it has to be public. StdFileManager.h is the host implementation the
    # tests inject; the libnx-only headers next to it are never compiled here.
    ${PROJECT_SOURCE_DIR}/../platform
    ${PROJECT_SOURCE_DIR}/../platform/libnx
)
