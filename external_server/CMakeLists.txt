cmake_minimum_required(VERSION 3.10)
project(ExternalServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 99)

# Include directories
include_directories(include)
include_directories(../include) # For CanbusProtocol.h

# Find Threads
find_package(Threads REQUIRED)

# Source files
set(SOURCES
    src/sniffer_client.cpp
    ../src/communication/UdpCanbusCommunication.cpp
    ../src/communication/UdpCommunication.cpp
)

# Shared Library
add_library(sniffer_client SHARED ${SOURCES})
target_link_libraries(sniffer_client Threads::Threads)

# Python Example (Optional, just to ensure it's there)
# configure_file(python/main.py python/main.py COPYONLY)
