cmake_minimum_required(VERSION 3.10)
project(RpiCanbusSniffer)

set(CMAKE_CXX_STANDARD 17)

include_directories(include)

# Main Library Sources
file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.c")

# Main Executable
add_executable(RpiCanbusSniffer ${SOURCES})

# Test Executable
add_executable(tcp_communication_tester tests/tcp_communication_tester.cpp ${SOURCES})
