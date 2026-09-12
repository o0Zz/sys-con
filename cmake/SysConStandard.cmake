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
# gnu++23 by lib/Atmosphere-libs/config/common.mk, which is a submodule and not ours to
# change. Since gnu++23 is a superset, code written to the C++20 floor compiles in both,
# and the harmful case -- the test target sitting *below* the libraries it links, so the
# tests exercised the production code under weaker language rules -- is gone.
#
# Net effect: five standards (11 / 17 / 17 / 20 / gnu++23) reduced to two, C++20 on the
# host and gnu++23 on the device.

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
