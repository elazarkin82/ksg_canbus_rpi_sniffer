# Sets the minimum CMake version required for this project.
cmake_minimum_required(VERSION 3.22.1)

# Declares the project name.
project("service")

set(CMAKE_CXX_STANDARD 17)

# Calculate absolute paths to project roots correctly
# From: external_server/android/app/src/main/cpp
# 1: main, 2: src, 3: app, 4: android, 5: external_server, 6: root (ksg_canbus_rpi_sniffer)
get_filename_component(CORE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../" REALPATH)
get_filename_component(EXT_SERVER_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../" REALPATH)

# Include directories from the core project and external server
include_directories(
    "${CORE_ROOT}/include"
    "${EXT_SERVER_ROOT}/include"
)

# Core sources required for sniffer_client functionality
# Matches the logic in external_server/CMakeLists.txt
set(CORE_SOURCES
    "${CORE_ROOT}/src/communication/UdpCommunication.cpp"
    "${CORE_ROOT}/src/communication/UdpCanbusCommunication.cpp"
    "${EXT_SERVER_ROOT}/src/sniffer_client.cpp"
)

# Native library that combines core sniffer_client and JNI wrappers
add_library(${CMAKE_PROJECT_NAME} SHARED
    native-lib.cpp
    ${CORE_SOURCES}
)

# Align LOAD segments to 16 KB boundaries for Android 15+ compatibility.
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE "-Wl,-z,max-page-size=16384")

# Link system libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    android
    log
)
