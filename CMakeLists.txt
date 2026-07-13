cmake_minimum_required(VERSION 3.14)
project(SysCon)

# Needed at the top level so `ctest --test-dir build` can discover tests declared
# by tests/CMakeLists.txt further down the tree - without this, no CTestTestfile.cmake
# is generated at the build root and ctest reports "No tests were found!!!".
enable_testing()

#Import the library
add_subdirectory(source/Sysmodule)

#Import the library
add_subdirectory(source/ControllerLib)

#HIDDataInterpreter
add_subdirectory(lib/HIDDataInterpreter/src)

#Import the tests
add_subdirectory(tests)
