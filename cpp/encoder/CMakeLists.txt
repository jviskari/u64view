cmake_minimum_required(VERSION 3.10)
project(Ultimate64Encoder)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find FFmpeg
find_package(PkgConfig REQUIRED)

# Find FFmpeg components
pkg_check_modules(LIBAV REQUIRED IMPORTED_TARGET
    libavcodec
    libavformat
    libavutil
    libswscale
    libswresample
)

if(NOT LIBAV_FOUND)
    message(FATAL_ERROR "FFmpeg libraries not found! Install with: brew install ffmpeg")
endif()

message(STATUS "Found FFmpeg:")
message(STATUS "  libavcodec: ${LIBAV_libavcodec_VERSION}")
message(STATUS "  libavformat: ${LIBAV_libavformat_VERSION}")
message(STATUS "  libavutil: ${LIBAV_libavutil_VERSION}")
message(STATUS "  libswscale: ${LIBAV_libswscale_VERSION}")
message(STATUS "  libswresample: ${LIBAV_libswresample_VERSION}")

# Include parent directory for shared headers
include_directories(../src)

# Source files for the encoder
set(ENCODER_SOURCES
    src/encoder_tool.cpp
    src/raw_encoder.cpp
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

# Link FFmpeg libraries
target_link_libraries(RawEncoder PkgConfig::LIBAV)

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
message(STATUS "Building Ultimate 64 Raw Encoder with FFmpeg support")