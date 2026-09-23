cmake_minimum_required(VERSION 3.14)
project(SysConTests)

include(FetchContent)
FetchContent_Declare(
    googletest_1_15_0
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.0
)

set(BUILD_GMOCK ON CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest_1_15_0)

enable_testing()

set(TEST_FILES
    core/test_deadzone.cpp
    core/test_input_binding.cpp
    core/test_normalize.cpp
    core/test_read_next_buffer.cpp
    core/test_rumble_template.cpp
    core/test_status.cpp
    drivers/test_dualshock3.cpp
    drivers/test_network_controller.cpp
    drivers/test_report_bounds.cpp
    drivers/test_sinput.cpp
    drivers/test_steam2026.cpp
    drivers/test_switch.cpp
    drivers/test_wii.cpp
    drivers/test_xboxone.cpp
    app/test_config_store.cpp
    app/test_network_profile.cpp
    app/test_ini_reader.cpp
)

set(MOCK_FILES
    mocks/Device.h
    mocks/Logger.h
    mocks/MemoryFileManager.h
    mocks/USBEndpoint.h
    mocks/USBInterface.h
)

add_executable(SysConTests ${TEST_FILES} ${MOCK_FILES})

target_link_libraries(SysConTests PRIVATE GTest::gmock_main)
target_link_libraries(SysConTests PRIVATE ControllerLib)
target_link_libraries(SysConTests PRIVATE SysConModule)
target_link_libraries(SysConTests PRIVATE SysConIni)
target_link_libraries(SysConTests PRIVATE syscon::warnings)


target_compile_definitions(SysConTests PRIVATE
    CONFIG_FULLPATH_PROJECT="${CMAKE_CURRENT_SOURCE_DIR}/../src/app/config.ini"
)

target_include_directories(SysConTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

include(GoogleTest)
gtest_discover_tests(SysConTests) # discovers tests by asking the compiled test executable to enumerate its tests
