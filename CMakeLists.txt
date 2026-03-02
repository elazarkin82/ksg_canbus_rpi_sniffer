cmake_minimum_required(VERSION 3.10)
project(RpiCanbusSniffer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 99)

include_directories(include)
include_directories(emulators/car_system/include)
include_directories(emulators/car_computer/include)

# Find Threads library (required for std::thread on Linux)
find_package(Threads REQUIRED)

# Collect all sources EXCEPT main.cpp
file(GLOB_RECURSE LIB_SOURCES "src/*.cpp" "src/*.c")
list(REMOVE_ITEM LIB_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

# Main Executable (includes main.cpp)
add_executable(RpiCanbusSniffer src/main.cpp ${LIB_SOURCES})
target_link_libraries(RpiCanbusSniffer Threads::Threads)

# Test Executables
add_executable(tcp_communication_tester tests/tcp_communication_tester.cpp ${LIB_SOURCES})
target_link_libraries(tcp_communication_tester Threads::Threads)

add_executable(tcp_canbus_tester tests/tcp_canbus_tester.cpp ${LIB_SOURCES})
target_link_libraries(tcp_canbus_tester Threads::Threads)

add_executable(udp_communication_tester tests/udp_communication_tester.cpp ${LIB_SOURCES})
target_link_libraries(udp_communication_tester Threads::Threads)

add_executable(udp_canbus_tester tests/udp_canbus_tester.cpp ${LIB_SOURCES})
target_link_libraries(udp_canbus_tester Threads::Threads)

# --- Emulators ---

# Car System Emulator (Standalone Executable)
file(GLOB CAR_SYSTEM_SOURCES "emulators/car_system/src/*.c")
add_executable(car_system_emulator ${CAR_SYSTEM_SOURCES})
target_link_libraries(car_system_emulator Threads::Threads m)

# Car Computer Emulator (Standalone Executable)
# Need to link with LIB_SOURCES because it uses ObdCanbusCommunication
add_executable(car_computer_emulator emulators/car_computer/src/main.cpp emulators/car_computer/src/CarComputer.cpp ${LIB_SOURCES})
target_link_libraries(car_computer_emulator Threads::Threads)
