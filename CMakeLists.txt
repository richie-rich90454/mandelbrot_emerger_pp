cmake_minimum_required(VERSION 3.25)
project(mandelbrot_emerger LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()
include(FetchContent)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3 GIT_REPOSITORY https://github.com/libsdl-org/SDL.git GIT_TAG release-3.2.16)
FetchContent_MakeAvailable(SDL3)
add_executable(mandelbrot_emerger
    src/main.cpp
    src/application.cpp
    src/viewport.cpp
    src/simulation.cpp
    src/color_scheme.cpp
    src/png_writer.cpp
)
target_include_directories(mandelbrot_emerger PRIVATE src)
target_link_libraries(mandelbrot_emerger PRIVATE SDL3::SDL3)
if(NOT MSVC)
    target_link_options(mandelbrot_emerger PRIVATE -static -static-libgcc -static-libstdc++)
endif()
