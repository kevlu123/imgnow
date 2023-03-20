cmake_minimum_required(VERSION 3.0)

project(imgnow)

# Disable clip examples and tests
set(CLIP_EXAMPLES OFF CACHE BOOL "Compile clip examples")
set(CLIP_TESTS OFF CACHE BOOL "Compile clip tests")

set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(imgnow)
add_subdirectory(clip)

option(SDL_SHARED "" OFF)
option(SDL_STATIC "" ON)
add_subdirectory(SDL)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build SDL2_net as a static library" FORCE)
add_subdirectory(SDL_net)

configure_file(clip/LICENSE.txt "${CMAKE_BINARY_DIR}/bin/LICENSE.txt" COPYONLY)
