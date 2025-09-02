cmake_minimum_required(VERSION 3.16)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Raw Player Tool
find_package(SDL2 REQUIRED)

add_executable(raw_player
    raw_player.cpp
    raw_player_main.cpp
    ../src/raw_capture.cpp
)

target_link_libraries(raw_player
    ${SDL2_LIBRARIES}
)

target_include_directories(raw_player PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ../src
)

# Copy to build directory
set_target_properties(raw_player PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tools
)

# Install
install(TARGETS raw_player
    RUNTIME DESTINATION bin
)