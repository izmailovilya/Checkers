cmake_minimum_required(VERSION 3.16)
project(Checkers)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find SDL2 manually
set(SDL2_INCLUDE_DIRS "/opt/homebrew/include/SDL2")
set(SDL2_LIBRARIES "-L/opt/homebrew/lib -lSDL2")

# Find SDL2_image
set(SDL2_IMAGE_INCLUDE_DIR "/opt/homebrew/include/SDL2")
set(SDL2_IMAGE_LIBRARY "-L/opt/homebrew/lib -lSDL2_image")

# Find nlohmann/json
find_package(nlohmann_json 3.12.0 REQUIRED)

# Source files
file(GLOB_RECURSE SOURCES
    "main.cpp"
    "Game/*.h"
    "Models/*.h"
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE 
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_IMAGE_INCLUDE_DIR}
)

# Link libraries
target_link_libraries(${PROJECT_NAME} 
    ${SDL2_LIBRARIES}
    ${SDL2_IMAGE_LIBRARY}
    nlohmann_json::nlohmann_json
)