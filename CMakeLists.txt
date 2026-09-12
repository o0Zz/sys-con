cmake_minimum_required(VERSION 3.14)
project(SysCon)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(SysConStandard)
include(SysConWarnings)

# Needed at the top level so `ctest --test-dir build` can discover tests declared
# by tests/CMakeLists.txt further down the tree - without this, no CTestTestfile.cmake
# is generated at the build root and ctest reports "No tests were found!!!".
enable_testing()

#HIDDataInterpreter (submodule - declared first so the libraries below can link it)
add_subdirectory(lib/HIDDataInterpreter/src)

# The submodule pins itself to C++11. Raise it here rather than editing a submodule, so
# everything in the build speaks the same language version.
syscon_set_standard(HIDDataInterpreterLib)

#Import the library
add_subdirectory(src/core)

#Vendored INI parser (inih) - separate target so our warning flags skip it
add_subdirectory(lib/ini)

#Import the library
add_subdirectory(src/app)

#Import the tests
add_subdirectory(tests)
