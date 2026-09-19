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
add_executable(mandelbrot_emerger
    src/main.cpp
    src/application.cpp
    src/viewport.cpp
    src/simulation.cpp
    src/color_scheme.cpp
    src/png_writer.cpp
)
target_include_directories(mandelbrot_emerger PRIVATE src)
if(EMSCRIPTEN)
    target_compile_options(mandelbrot_emerger PRIVATE -pthread -sUSE_SDL=3 -Wno-dollar-in-identifier-extension)
    target_link_options(mandelbrot_emerger PRIVATE
        -pthread
        -sUSE_SDL=3
        -sENVIRONMENT=web,worker
        -sPTHREAD_POOL_SIZE=navigator.hardwareConcurrency
        -sPTHREAD_POOL_SIZE_STRICT=0
        -sALLOW_MEMORY_GROWTH=1
        -sINITIAL_MEMORY=96MB
        -O3
        --shell-file "${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
    )
    # the build tree is separate (build/web); artifacts land in build-web, which is exactly what a static host serves
    set_target_properties(mandelbrot_emerger PROPERTIES
        SUFFIX ".html" OUTPUT_NAME "index"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build-web"
    )
    add_custom_command(TARGET mandelbrot_emerger POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/web/coi-serviceworker.js" "$<TARGET_FILE_DIR:mandelbrot_emerger>"
    )
else()
    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3 GIT_REPOSITORY https://github.com/libsdl-org/SDL.git GIT_TAG release-3.2.16)
    FetchContent_MakeAvailable(SDL3)
    target_link_libraries(mandelbrot_emerger PRIVATE SDL3::SDL3)
    if(NOT MSVC)
        target_link_options(mandelbrot_emerger PRIVATE -static -static-libgcc -static-libstdc++)
    endif()
endif()
