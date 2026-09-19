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
set(MANDELBROT_SOURCES
    src/main.cpp
    src/application.cpp
    src/viewport.cpp
    src/simulation.cpp
    src/color_scheme.cpp
    src/png_writer.cpp
)
# the build tree is separate (build/web); artifacts land in build-web, which is exactly what a static host serves
set(MANDELBROT_WEB_DIST "${CMAKE_CURRENT_SOURCE_DIR}/build-web")
if(EMSCRIPTEN)
    # two modules: the threaded one for cross-origin-isolated pages and a single-threaded one for hosts that
    # cannot provide COOP/COEP at all, so the dist runs anywhere without touching server configuration
    function(mandelbrot_web_variant target output threads)
        add_executable(${target} ${MANDELBROT_SOURCES})
        target_include_directories(${target} PRIVATE src)
        target_compile_options(${target} PRIVATE -sUSE_SDL=3 -Wno-dollar-in-identifier-extension)
        set(linkOptions
            -sUSE_SDL=3
            -sALLOW_MEMORY_GROWTH=1
            -sINITIAL_MEMORY=96MB
            -O3
        )
        if(threads)
            target_compile_options(${target} PRIVATE -pthread)
            list(APPEND linkOptions
                -pthread
                -sENVIRONMENT=web,worker
                -sPTHREAD_POOL_SIZE=navigator.hardwareConcurrency
                -sPTHREAD_POOL_SIZE_STRICT=0
            )
        else()
            list(APPEND linkOptions -sENVIRONMENT=web)
        endif()
        target_link_options(${target} PRIVATE ${linkOptions})
        set_target_properties(${target} PROPERTIES
            SUFFIX ".js" OUTPUT_NAME "${output}"
            RUNTIME_OUTPUT_DIRECTORY "${MANDELBROT_WEB_DIST}"
        )
    endfunction()
    mandelbrot_web_variant(mandelbrot_emerger threaded TRUE)
    mandelbrot_web_variant(mandelbrot_emerger_st fallback FALSE)
    # the hand-written dist files - the page loader and the service worker - are not linker outputs, so they must
    # not ride on the link step: a POST_BUILD copy is skipped whenever ninja finds the target up to date, which
    # would leave a wiped or manually deleted build-web without them. This target runs on every build of either
    # module, restores the directory if needed, copies unconditionally and then verifies each copy byte for byte
    add_custom_target(mandelbrot_web_dist
        COMMAND ${CMAKE_COMMAND} -E make_directory "${MANDELBROT_WEB_DIST}"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/web/index.html" "${MANDELBROT_WEB_DIST}/index.html"
        COMMAND ${CMAKE_COMMAND} -E compare_files "${CMAKE_CURRENT_SOURCE_DIR}/web/index.html" "${MANDELBROT_WEB_DIST}/index.html"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/web/coi-serviceworker.js" "${MANDELBROT_WEB_DIST}/coi-serviceworker.js"
        COMMAND ${CMAKE_COMMAND} -E compare_files "${CMAKE_CURRENT_SOURCE_DIR}/web/coi-serviceworker.js" "${MANDELBROT_WEB_DIST}/coi-serviceworker.js"
        DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/web/index.html"
            "${CMAKE_CURRENT_SOURCE_DIR}/web/coi-serviceworker.js"
        COMMENT "Staging the web page loader and service worker into build-web"
        VERBATIM
    )
    add_dependencies(mandelbrot_emerger mandelbrot_web_dist)
    add_dependencies(mandelbrot_emerger_st mandelbrot_web_dist)
else()
    add_executable(mandelbrot_emerger ${MANDELBROT_SOURCES})
    target_include_directories(mandelbrot_emerger PRIVATE src)
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
