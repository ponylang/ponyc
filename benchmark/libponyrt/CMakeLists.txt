cmake_minimum_required(VERSION 3.11 FATAL_ERROR)

project(libponyrt.benchmarks VERSION ${PONYC_PROJECT_VERSION} LANGUAGES CXX)

add_executable(libponyrt.benchmarks
    gbenchmark_main.cc
    common/common.cc
    ds/hash.cc
    mem/heap.cc
    mem/pool.cc
)

target_include_directories(libponyrt.benchmarks
    PRIVATE ../../src/common
    PRIVATE ../../src/libponyrt
    PRIVATE ../../build/libs/include
)

target_link_libraries(libponyrt.benchmarks
    PRIVATE libponyrt
    PRIVATE benchmark::benchmark
)

if (MSVC)
    target_link_libraries(libponyrt.benchmarks PRIVATE dbghelp)
elseif(${CMAKE_HOST_SYSTEM_NAME} MATCHES "Darwin")
    target_link_libraries(libponyrt.benchmarks PRIVATE dl)
    target_link_options(libponyrt.benchmarks PRIVATE "-static-libstdc++")
elseif(${CMAKE_HOST_SYSTEM_NAME} MATCHES "BSD")
    target_link_libraries(libponyrt.benchmarks PRIVATE execinfo)
    target_link_options(libponyrt.benchmarks PRIVATE "-static-libstdc++")
else()
    target_link_libraries(libponyrt.benchmarks PRIVATE atomic dl)
    target_link_options(libponyrt.benchmarks PRIVATE "-static-libgcc")
endif (MSVC)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(libponyrt.benchmarks PRIVATE "-static-libgcc")
endif()
