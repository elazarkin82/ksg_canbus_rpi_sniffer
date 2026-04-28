cmake_minimum_required(VERSION 3.10)
project(RpiCanbusSniffer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 99)

# --- Debug Flags ---
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "DEBUG flag: ${DEBUG}")
message(STATUS "DEBUG_MSG flag: ${DEBUG_MSG}")

if(DEBUG)
    add_definitions(-DDEBUG)
    message(STATUS "Adding -DDEBUG to compiler definitions")
endif()

if(DEBUG_MSG)
    add_definitions(-DDEBUG_MSG)
    message(STATUS "Adding -DDEBUG_MSG to compiler definitions")
endif()

include_directories(include)
include_directories(emulators/car_system/include)
include_directories(emulators/car_computer/include)
include_directories(external_server/include)

# Find Threads library (required for std::thread on Linux)
find_package(Threads REQUIRED)

# Collect all sources EXCEPT main.cpp
file(GLOB_RECURSE LIB_SOURCES "src/*.cpp" "src/*.c")
list(REMOVE_ITEM LIB_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

# Main Executable (includes main.cpp)
add_executable(sniffer_service src/main.cpp ${LIB_SOURCES})
target_link_libraries(sniffer_service Threads::Threads)

# Test Executables
add_executable(tcp_communication_tester tests/tcp_communication_tester.cpp ${LIB_SOURCES})
target_link_libraries(tcp_communication_tester Threads::Threads)

add_executable(tcp_canbus_tester tests/tcp_canbus_tester.cpp ${LIB_SOURCES})
target_link_libraries(tcp_canbus_tester Threads::Threads)

add_executable(udp_communication_tester tests/udp_communication_tester.cpp ${LIB_SOURCES})
target_link_libraries(udp_communication_tester Threads::Threads)

add_executable(udp_canbus_tester tests/udp_canbus_tester.cpp ${LIB_SOURCES})
target_link_libraries(udp_canbus_tester Threads::Threads)

add_executable(params_tester tests/params_tester.cpp)
target_link_libraries(params_tester Threads::Threads)

if(RPI_BUILD)
    add_executable(led_controller_tester tests/ledControlerUtilsTester.cpp ${LIB_SOURCES})
    target_link_libraries(led_controller_tester Threads::Threads)
    message(STATUS "RPI_BUILD enabled - Building led_controller_tester")
endif()

# --- Emulators ---

# Car System Emulator (Standalone Executable)
file(GLOB CAR_SYSTEM_SOURCES "emulators/car_system/src/*.c")
add_executable(car_system_emulator ${CAR_SYSTEM_SOURCES})
target_link_libraries(car_system_emulator Threads::Threads m)

# Car Computer Emulator (Standalone Executable)
# Need to link with LIB_SOURCES because it uses ObdCanbusCommunication
add_executable(car_computer_emulator emulators/car_computer/src/main.cpp emulators/car_computer/src/CarComputer.cpp ${LIB_SOURCES})
target_link_libraries(car_computer_emulator Threads::Threads)

# --- Sniffer Test Library ---
add_library(sniffer_test_lib SHARED
    tests/sniffer_test_lib.cpp
    ${LIB_SOURCES}
)
target_link_libraries(sniffer_test_lib Threads::Threads)

# --- Main Service Library (for Python Tests) ---
add_library(main_service_lib SHARED
    tests/wrapper/main_service_wrapper.cpp
    src/core/MainService.cpp
    ${LIB_SOURCES}
)
target_link_libraries(main_service_lib Threads::Threads)

# --- Sniffer Client Library (SDK) ---
add_library(sniffer_client SHARED
    external_server/src/sniffer_client.cpp
    src/communication/UdpCanbusCommunication.cpp
    src/communication/UdpCommunication.cpp
)
target_link_libraries(sniffer_client Threads::Threads)