cmake_minimum_required(VERSION 3.10)
project(Ultimate64Viewer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find SDL2 on macOS with Homebrew
if(APPLE)
    # Get Homebrew prefix
    execute_process(
        COMMAND brew --prefix sdl2
        OUTPUT_VARIABLE SDL2_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    
    if(SDL2_PREFIX)
        message(STATUS "Found SDL2 via Homebrew at: ${SDL2_PREFIX}")
        
        set(SDL2_INCLUDE_DIRS "${SDL2_PREFIX}/include/SDL2")
        set(SDL2_LIBRARY_DIRS "${SDL2_PREFIX}/lib")
        
        # Find the actual library files
        find_library(SDL2_LIBRARY
            NAMES SDL2
            PATHS ${SDL2_LIBRARY_DIRS}
            NO_DEFAULT_PATH
        )
        
        find_library(SDL2MAIN_LIBRARY
            NAMES SDL2main
            PATHS ${SDL2_LIBRARY_DIRS}
            NO_DEFAULT_PATH
        )
        
        if(SDL2_LIBRARY)
            set(SDL2_LIBRARIES ${SDL2_LIBRARY})
            if(SDL2MAIN_LIBRARY)
                list(APPEND SDL2_LIBRARIES ${SDL2MAIN_LIBRARY})
            endif()
            set(SDL2_FOUND TRUE)
        endif()
    endif()
    
    # Fallback to pkg-config if Homebrew method failed
    if(NOT SDL2_FOUND)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(SDL2 sdl2)
        endif()
    endif()
else()
    # On other platforms, use pkg-config
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED sdl2)
endif()

# Debug output
message(STATUS "SDL2_FOUND: ${SDL2_FOUND}")
message(STATUS "SDL2_INCLUDE_DIRS: ${SDL2_INCLUDE_DIRS}")
message(STATUS "SDL2_LIBRARIES: ${SDL2_LIBRARIES}")
message(STATUS "SDL2_LIBRARY_DIRS: ${SDL2_LIBRARY_DIRS}")

if(NOT SDL2_FOUND)
    message(FATAL_ERROR "SDL2 not found! Please install SDL2 via: brew install sdl2")
endif()

# Include directories
include_directories(${SDL2_INCLUDE_DIRS})
include_directories(src)

# Add library directories
if(SDL2_LIBRARY_DIRS)
    link_directories(${SDL2_LIBRARY_DIRS})
endif()

# Common source files for the AV framework
set(AV_FRAMEWORK_SOURCES
    src/av_frame_serialization.cpp
    src/file_av_recorder.cpp
    src/file_av_producer.cpp
)

# SDL AV Player sources
set(SDL_PLAYER_SOURCES
    src/sdl_av_player.cpp
    ${AV_FRAMEWORK_SOURCES}
)

# Network sources (existing Ultimate64 components)
set(NETWORK_SOURCES
    src/network_av_producer.cpp
    src/video_stream_receiver.cpp
    src/audio_stream_receiver.cpp
    src/stream_receiver.cpp
    src/av_synchronizer.cpp
)

# Main Ultimate64Viewer application
add_executable(Ultimate64Viewer
    src/main.cpp
    ${SDL_PLAYER_SOURCES}
    ${NETWORK_SOURCES}
)

# Simple file player application
add_executable(simple_file_player
    src/simple_file_player.cpp
    ${SDL_PLAYER_SOURCES}
)

# Test executables (no SDL2 needed)
add_executable(test_serialization 
    src/test_serialization.cpp
    src/av_frame_serialization.cpp
)

add_executable(test_file_roundtrip 
    src/test_file_roundtrip.cpp
    ${AV_FRAMEWORK_SOURCES}
)

# Link SDL2 to applications that need it
target_link_libraries(Ultimate64Viewer ${SDL2_LIBRARIES})
target_link_libraries(simple_file_player ${SDL2_LIBRARIES})

# Handle filesystem library for older compilers
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9.0")
    target_link_libraries(test_file_roundtrip stdc++fs)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9.0")
    target_link_libraries(test_file_roundtrip c++fs)
endif()

# On macOS, add required frameworks
if(APPLE)
    target_link_libraries(Ultimate64Viewer 
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
        "-framework CoreAudio"
        "-framework AudioToolbox"
        "-framework ForceFeedback"
        "-framework Metal"
        "-framework CoreHaptics"
        "-framework GameController"
    )
    target_link_libraries(simple_file_player 
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
        "-framework CoreAudio"
        "-framework AudioToolbox"
        "-framework ForceFeedback"
        "-framework Metal"
        "-framework CoreHaptics"
        "-framework GameController"
    )
endif()

# Compiler-specific options
if(WIN32)
    target_link_libraries(Ultimate64Viewer ws2_32)
    target_link_libraries(simple_file_player ws2_32)
endif()

# Add compiler warnings for all targets
if(MSVC)
    target_compile_options(Ultimate64Viewer PRIVATE /W4)
    target_compile_options(simple_file_player PRIVATE /W4)
    target_compile_options(test_serialization PRIVATE /W4)
    target_compile_options(test_file_roundtrip PRIVATE /W4)
else()
    target_compile_options(Ultimate64Viewer PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(simple_file_player PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(test_serialization PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(test_file_roundtrip PRIVATE -Wall -Wextra -pedantic)
endif()

# For debug builds
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# Install targets
install(TARGETS Ultimate64Viewer simple_file_player DESTINATION bin)

# Add custom target to run tests
add_custom_target(run_tests
    COMMAND test_serialization
    COMMAND test_file_roundtrip
    DEPENDS test_serialization test_file_roundtrip
    COMMENT "Running all tests"
)

# Individual test targets
add_custom_target(test_serial
    COMMAND test_serialization
    DEPENDS test_serialization
    COMMENT "Running serialization tests"
)

add_custom_target(test_files
    COMMAND test_file_roundtrip
    DEPENDS test_file_roundtrip
    COMMENT "Running file round-trip tests"
)
