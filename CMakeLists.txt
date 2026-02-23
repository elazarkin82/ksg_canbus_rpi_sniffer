cmake_minimum_required(VERSION 3.10)
project(RpiCanbusSniffer)

set(CMAKE_CXX_STANDARD 17)

include_directories(include)

# Find Threads library (required for std::thread on Linux)
find_package(Threads REQUIRED)

# Collect all sources EXCEPT main.cpp
file(GLOB_RECURSE LIB_SOURCES "src/*.cpp" "src/*.c")
list(REMOVE_ITEM LIB_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

# Main Executable (includes main.cpp)
add_executable(RpiCanbusSniffer src/main.cpp ${LIB_SOURCES})
target_link_libraries(RpiCanbusSniffer Threads::Threads)

# Test Executable (does NOT include main.cpp)
add_executable(tcp_communication_tester tests/tcp_communication_tester.cpp ${LIB_SOURCES})
target_link_libraries(tcp_communication_tester Threads::Threads)
