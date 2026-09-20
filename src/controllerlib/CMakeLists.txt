cmake_minimum_required(VERSION 3.14)
project(ControllerLib VERSION 1.0.0 LANGUAGES CXX)

# ControllerLib is a standalone library. It knows nothing about the applications that embed
# it, or the platforms they run on, and it has to configure and build on its own:
#
#     cmake -S src/controllerlib -B build-controllerlib && cmake --build build-controllerlib
#
# so nothing in this file may reference a macro, option, variable or target defined by a
# parent project. Everything the library needs -- its language standard, its warning set,
# its one dependency -- is declared here. A host embeds it with add_subdirectory() and
# links `ControllerLib`; it gets the include path and the language floor transitively.

# True when this file is the top of the build, i.e. someone configured the library on its
# own rather than through a host project.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(CONTROLLERLIB_STANDALONE TRUE)
else()
    set(CONTROLLERLIB_STANDALONE FALSE)
endif()

set(CONTROLLERLIB_CXX_STANDARD 20)

# C++20 is a hard floor, not a preference: PinId defaults its operator== (C++20 defaulted
# comparisons) and EnumArray calls std::array::fill from a constexpr context, which only
# became constexpr in C++20. It is PUBLIC, so a host configured below it fails at configure
# time with a clear message instead of at compile time with template noise.

option(CONTROLLERLIB_WERROR "Treat compiler warnings as errors (intended for CI)" OFF)

# ---------------------------------------------------------------------------------------
# Dependency: HID report descriptor parsing, used by GenericHIDController.
#
# If the host project already defines the target it is left alone -- this is the only
# concession to being embedded, and it is a passive one. Otherwise the library pulls the
# dependency in itself, which is what makes the standalone configure above work.
# ---------------------------------------------------------------------------------------
# Where to look when nobody has said. The candidates are generic layout conventions -- a
# vendored copy underneath this library, a sibling checkout, or a dependency directory in an
# enclosing tree -- so none of them names a particular host project. Innermost first, so a
# copy this library owns always beats whatever an embedding tree happens to have lying
# around; that also means this list still works unchanged if the library is ever extracted
# to its own repository and vendors the dependency itself.
set(_controllerlib_hdi_candidates "")
foreach(_controllerlib_base
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
        "${CMAKE_CURRENT_SOURCE_DIR}/../..")
    list(APPEND _controllerlib_hdi_candidates "${_controllerlib_base}/HIDDataInterpreter")
    foreach(_controllerlib_dir "external" "third_party" "lib" "deps")
        list(APPEND _controllerlib_hdi_candidates
             "${_controllerlib_base}/${_controllerlib_dir}/HIDDataInterpreter")
    endforeach()
endforeach()

# Probe for src/CMakeLists.txt, not for the directory: an uninitialised submodule leaves an
# empty directory behind, which would otherwise win the search and then fail confusingly.
set(_controllerlib_hdi_default "${CMAKE_CURRENT_SOURCE_DIR}/../HIDDataInterpreter")
foreach(_controllerlib_candidate ${_controllerlib_hdi_candidates})
    if(EXISTS "${_controllerlib_candidate}/src/CMakeLists.txt")
        set(_controllerlib_hdi_default "${_controllerlib_candidate}")
        break()
    endif()
endforeach()

# A cache default is only consulted when the entry does not already exist, so both a -D on
# the command line and a value set by an embedding host keep winning over the search above.
# That is why there is no FORCE anywhere in this file.
set(CONTROLLERLIB_HIDDATAINTERPRETER_DIR "${_controllerlib_hdi_default}"
    CACHE PATH "Location of the HIDDataInterpreter checkout")

if(NOT TARGET HIDDataInterpreterLib)
    if(NOT EXISTS "${CONTROLLERLIB_HIDDATAINTERPRETER_DIR}/src/CMakeLists.txt")
        string(REPLACE ";" "\n    " _controllerlib_tried "${_controllerlib_hdi_candidates}")
        message(FATAL_ERROR
            "HIDDataInterpreter not found at ${CONTROLLERLIB_HIDDATAINTERPRETER_DIR}.\n"
            "Searched:\n    ${_controllerlib_tried}\n"
            "It ships as a git submodule: run `git submodule update --init --recursive`, "
            "or pass -DCONTROLLERLIB_HIDDATAINTERPRETER_DIR=<path> to point at a checkout.")
    endif()

    add_subdirectory(${CONTROLLERLIB_HIDDATAINTERPRETER_DIR}/src
                     ${CMAKE_CURRENT_BINARY_DIR}/HIDDataInterpreter)

    # The dependency pins itself to C++11 with a directory-scoped CMAKE_CXX_STANDARD. Raise
    # it here rather than editing a checkout we do not own, so everything in one build
    # speaks the same language version.
    target_compile_features(HIDDataInterpreterLib PUBLIC cxx_std_${CONTROLLERLIB_CXX_STANDARD})
    set_target_properties(HIDDataInterpreterLib PROPERTIES
        CXX_STANDARD ${CONTROLLERLIB_CXX_STANDARD}
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
endif()

unset(_controllerlib_hdi_candidates)
unset(_controllerlib_hdi_default)

# ---------------------------------------------------------------------------------------
# The library
# ---------------------------------------------------------------------------------------

# Sources are listed explicitly rather than globbed: a glob without CONFIGURE_DEPENDS does
# not re-run CMake when a file is added, so a new driver silently fails to build until
# someone reconfigures by hand.
set(SRC_FILES
    Status.cpp
    drivers/BaseController.cpp
    drivers/Dualshock3Controller.cpp
    drivers/GenericHIDController.cpp
    drivers/NetworkController.cpp
    drivers/SInputController.cpp
    drivers/SteamController2026.cpp
    drivers/SwitchController.cpp
    drivers/WiiController.cpp
    drivers/Xbox360Controller.cpp
    drivers/Xbox360WirelessController.cpp
    drivers/XboxController.cpp
    drivers/XboxOneController.cpp
)

# Listed only so they show up in IDE project trees.
set(HEADER_FILES
    AnalogAxis.h
    EnumArray.h
    GamepadButton.h
    PinId.h
    Status.h
    ControllerConfig.h
    ControllerTypes.h
    Controllers.h
    IController.h
    ILogger.h
    IUSBDevice.h
    IUSBEndpoint.h
    IUSBInterface.h
    drivers/BaseController.h
    drivers/Dualshock3Controller.h
    drivers/GenericHIDController.h
    drivers/NetworkController.h
    drivers/SInputController.h
    drivers/SteamController2026.h
    drivers/SwitchController.h
    drivers/WiiController.h
    drivers/Xbox360Controller.h
    drivers/Xbox360WirelessController.h
    drivers/XboxController.h
    drivers/XboxOneController.h
)

add_library(ControllerLib ${SRC_FILES} ${HEADER_FILES})
add_library(ControllerLib::ControllerLib ALIAS ControllerLib)

target_compile_features(ControllerLib PUBLIC cxx_std_${CONTROLLERLIB_CXX_STANDARD})
set_target_properties(ControllerLib PROPERTIES
    CXX_STANDARD ${CONTROLLERLIB_CXX_STANDARD}
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# PUBLIC, not PRIVATE: GenericHIDController.h is part of this library's own public surface
# and includes HIDJoystick.h, so the include path has to reach consumers.
target_link_libraries(ControllerLib PUBLIC HIDDataInterpreterLib)

target_include_directories(ControllerLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Warnings are PRIVATE and owned by this library: a host that embeds it keeps its own
# warning policy rather than inheriting this one, and this one applies whether the library
# was configured standalone or as a subdirectory.
if(MSVC)
    target_compile_options(ControllerLib PRIVATE
        /W4
        /permissive-        # Reject non-conforming constructs stricter toolchains reject.
        /wd4100             # Unreferenced parameter: the code marks these with (void)x.
        /wd4201             # Anonymous struct in a union (RGBAColor) - deliberate, and
                            # standard under the gnu++23 the device build uses.
    )
    if(CONTROLLERLIB_WERROR)
        target_compile_options(ControllerLib PRIVATE /WX)
    endif()
else()
    target_compile_options(ControllerLib PRIVATE
        -Wall
        -Wextra
        # Deliberately no -Wpedantic: the code uses GNU extensions on purpose (anonymous
        # structs in unions, packed structs).
        -Wshadow            # Catches the shadowed `result` locals in OpenInterfaces().
        -Wundef
        -Wformat=2          # Catches printf/arg mismatches at the ILogger call sites.
        -Wno-unused-parameter
    )
    if(CONTROLLERLIB_WERROR)
        target_compile_options(ControllerLib PRIVATE -Werror)
    endif()
endif()

if(CONTROLLERLIB_STANDALONE)
    message(STATUS "ControllerLib ${PROJECT_VERSION} (standalone, C++${CONTROLLERLIB_CXX_STANDARD})")
endif()
