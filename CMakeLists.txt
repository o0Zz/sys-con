cmake_minimum_required(VERSION 3.14)
project(SysCon)

# ---------------------------------------------------------------------------------------
# Warning configuration, exposed as an INTERFACE target.
#
# Previously the host (CMake) build set no warning flags at all, while the shipped device
# build set only -Wall -- and the Atmosphere flavour, which CI never builds, was the single
# configuration compiled with -Wextra -Werror. This lines them up.
#
# Usage:  target_link_libraries(<target> PRIVATE syscon::warnings)
# ---------------------------------------------------------------------------------------

add_library(syscon_warnings INTERFACE)
add_library(syscon::warnings ALIAS syscon_warnings)

option(SYSCON_WERROR "Treat compiler warnings as errors (intended for CI)" OFF)

if(MSVC)
    target_compile_options(syscon_warnings INTERFACE
        /W4
        /permissive-        # Reject non-conforming constructs the device toolchain rejects.
        /wd4100             # Unreferenced parameter: the codebase marks these with (void)x.
        /wd4201             # Anonymous struct in a union (RGBAColor) - deliberate, and the
                            # device build compiles as gnu++23 where it is standard.
        /wd4996             # "Consider fopen_s instead": StdFileManager uses std::fopen on
                            # purpose. <fstream> would pull libstdc++'s locale machinery into
                            # the sysmodule, which is half its binary size.
    )
    if(SYSCON_WERROR)
        target_compile_options(syscon_warnings INTERFACE /WX)
    endif()
else()
    target_compile_options(syscon_warnings INTERFACE
        -Wall
        -Wextra
        # Deliberately no -Wpedantic: the device build compiles as gnu++23 and the code
        # uses GNU extensions on purpose (anonymous structs in unions, packed structs).
        -Wshadow
        -Wundef             # Catches `#if ATMOSPHERE` where ATMOSPHERE is never defined.
        -Wformat=2          # Catches the printf/arg mismatches in the ILogger call sites.
        -Wno-unused-parameter
    )
    if(SYSCON_WERROR)
        target_compile_options(syscon_warnings INTERFACE -Werror)
    endif()
endif()

# ---------------------------------------------------------------------------------------
# The C++ standard, in one place.
#
# There used to be five different values in play: gnu++23 for the sysmodule, c++17 for
# AppletCompanion, 20 for the two CMake libraries, 17 for the tests, and 11 for the
# HIDDataInterpreter submodule. The tests therefore compiled the production library under a
# different language version than production did. config_handler.cpp already uses
# std::string::starts_with (C++20) and only linked because that call happened to live in a
# C++20 translation unit.
#
# C++20 is the floor for everything built here.
#
# It is not literally one value everywhere, and cannot be: the device build is pinned to
# gnu++23 by external/Atmosphere-libs/config/common.mk, which is a submodule and not ours to
# change. Since gnu++23 is a superset, code written to the C++20 floor compiles in both,
# and the harmful case -- the test target sitting *below* the libraries it links, so the
# tests exercised the production code under weaker language rules -- is gone.
#
# Net effect: five standards (11 / 17 / 17 / 20 / gnu++23) reduced to two, C++20 on the
# host and gnu++23 on the device.
# ---------------------------------------------------------------------------------------

set(SYSCON_CXX_STANDARD 20)

# Applies the standard as a target requirement, so it propagates to anything that links
# this target rather than depending on directory-scoped variables.
function(syscon_set_standard target)
    target_compile_features(${target} PUBLIC cxx_std_${SYSCON_CXX_STANDARD})
    set_target_properties(${target} PROPERTIES
        CXX_STANDARD ${SYSCON_CXX_STANDARD}
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
endfunction()

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
