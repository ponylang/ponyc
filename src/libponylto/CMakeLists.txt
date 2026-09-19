cmake_minimum_required(VERSION 3.25 FATAL_ERROR)

project(libponylto VERSION ${PONYC_PROJECT_VERSION} LANGUAGES CXX)

add_library(libponylto STATIC
    heap_to_stack.cc
    plugin.cc
)

target_include_directories(libponylto
    PRIVATE ../../build/libs/include
)

if(MSVC)
    target_compile_options(libponylto PRIVATE /wd4624)
endif()

if(${CMAKE_HOST_SYSTEM_NAME} MATCHES "Darwin")
    set(CMAKE_OSX_DEPLOYMENT_TARGET ${PONY_OSX_PLATFORM})
endif()
