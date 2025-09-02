cmake_minimum_required(VERSION 3.10)
project(Ultimate64Encoder)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include parent directory for shared headers
include_directories(../src)

# Source files for the encoder
set(ENCODER_SOURCES
    src/encoder_tool.cpp
    src/raw_encoder.cpp
    ../src/stream_types.h
    ../src/config.h
    ../src/raw_capture.cpp
)

# Headers
set(ENCODER_HEADERS
    src/raw_encoder.h
    ../src/stream_types.h
    ../src/config.h
    ../src/raw_capture.h
)

# Add executable
add_executable(RawEncoder ${ENCODER_SOURCES})

# Add compiler warnings
if(MSVC)
    target_compile_options(RawEncoder PRIVATE /W4)
else()
    target_compile_options(RawEncoder PRIVATE -Wall -Wextra -pedantic)
endif()

# For debug builds
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# Platform-specific linking
if(WIN32)
    target_link_libraries(RawEncoder ws2_32)
endif()

# Install target
install(TARGETS RawEncoder DESTINATION bin)

# Print build info
message(STATUS "Building Ultimate 64 Raw Encoder")
message(STATUS "Sources: ${ENCODER_SOURCES}")