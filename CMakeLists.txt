cmake_minimum_required(VERSION 3.14)
project(SysCon)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(SysConStandard)
include(SysConWarnings)

# Needed at the top level so `ctest --test-dir build` can discover tests declared
# by tests/CMakeLists.txt further down the tree - without this, no CTestTestfile.cmake
# is generated at the build root and ctest reports "No tests were found!!!".
enable_testing()

# ControllerLib is a standalone library that happens to live in this repo -- it has no
# dependency on sys-con, and this project must not give it one. It owns its own language
# standard, warning set and dependencies, so all this project does is say where its checkout
# of the library's one dependency lives, add it, and link the `ControllerLib` target.
# See src/controllerlib/README.md.
#
# Deliberately no FORCE: a -DCONTROLLERLIB_HIDDATAINTERPRETER_DIR=... on the command line is
# written to the cache before this runs, so an explicit override still wins.
set(CONTROLLERLIB_HIDDATAINTERPRETER_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/external/HIDDataInterpreter"
    CACHE PATH "Location of the HIDDataInterpreter checkout")

add_subdirectory(src/controllerlib)

#Vendored INI parser (inih) - separate target so our warning flags skip it
add_subdirectory(external/ini)

#Import the library
add_subdirectory(src/app)

#Import the tests
add_subdirectory(tests)
