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

# Add library directories
if(SDL2_LIBRARY_DIRS)
    link_directories(${SDL2_LIBRARY_DIRS})
endif()

# Add executable
add_executable(Ultimate64Viewer src/main.cpp)

# Link libraries
target_link_libraries(Ultimate64Viewer ${SDL2_LIBRARIES})

# On macOS, we might need additional frameworks
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
endif()

# Compiler-specific options
if(WIN32)
    target_link_libraries(Ultimate64Viewer ws2_32)
endif()

# Set compiler flags
if(SDL2_CFLAGS_OTHER)
    target_compile_options(Ultimate64Viewer PRIVATE ${SDL2_CFLAGS_OTHER})
endif()

# For debug builds
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# Install target
install(TARGETS Ultimate64Viewer DESTINATION bin)
