cmake_minimum_required(VERSION 3.14)
project(SysConControllerLib VERSION 1.0.0)

# Sources are listed explicitly rather than globbed: a glob without CONFIGURE_DEPENDS does
# not re-run CMake when a file is added, so a new driver silently fails to build until
# someone reconfigures by hand.
set(SRC_FILES
    drivers/BaseController.cpp
    drivers/Dualshock3Controller.cpp
    drivers/GenericHIDController.cpp
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
    ControllerConfig.h
    ControllerResult.h
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
    drivers/SteamController2026.h
    drivers/SwitchController.h
    drivers/WiiController.h
    drivers/Xbox360Controller.h
    drivers/Xbox360WirelessController.h
    drivers/XboxController.h
    drivers/XboxOneController.h
)

add_library(SysConControllerLib ${SRC_FILES} ${HEADER_FILES})

syscon_set_standard(SysConControllerLib)
target_link_libraries(SysConControllerLib PRIVATE syscon::warnings)

# PUBLIC, not PRIVATE: GenericHIDController.h is part of this library's own public surface
# and includes HIDJoystick.h. With PRIVATE linkage the include path did not propagate, and
# the device Makefile had to re-add it by hand -- two build systems owning one path.
target_link_libraries(SysConControllerLib PUBLIC HIDDataInterpreterLib)

target_include_directories(SysConControllerLib PUBLIC ${PROJECT_SOURCE_DIR}/)
