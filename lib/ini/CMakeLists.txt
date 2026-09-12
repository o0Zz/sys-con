cmake_minimum_required(VERSION 3.14)
project(SysConIni)

# Vendored third-party code (inih, https://github.com/benhoyt/inih), deliberately left as
# upstream wrote it. It gets its own target so that our warning flags -- and later
# clang-tidy -- apply to first-party code only, and so that reformatting never touches it.
#
# The one local change from upstream is INI_MAX_LINE in ini.h (0x480 instead of 200).

add_library(SysConIni ini.c)

target_include_directories(SysConIni PUBLIC ${PROJECT_SOURCE_DIR})
