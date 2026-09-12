# One place for warning configuration, exposed as an INTERFACE target.
#
# Previously the host (CMake) build set no warning flags at all, while the shipped device
# build set only -Wall -- and the Atmosphere flavour, which CI never builds, was the single
# configuration compiled with -Wextra -Werror. This lines them up.
#
# Usage:  target_link_libraries(<target> PRIVATE syscon::warnings)

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
        -Wshadow            # Catches the shadowed `result` locals in OpenInterfaces().
        -Wundef             # Catches `#if ATMOSPHERE` where ATMOSPHERE is never defined.
        -Wformat=2          # Catches the printf/arg mismatches in the ILogger call sites.
        -Wno-unused-parameter
    )
    if(SYSCON_WERROR)
        target_compile_options(syscon_warnings INTERFACE -Werror)
    endif()
endif()
