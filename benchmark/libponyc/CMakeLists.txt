cmake_minimum_required(VERSION 3.11 FATAL_ERROR)

project(libponyc.benchmarks VERSION ${PONYC_PROJECT_VERSION} LANGUAGES CXX)

add_executable(libponyc.benchmarks
    gbenchmark_main.cc
    common/common.cc
)

target_include_directories(libponyc.benchmarks
    PRIVATE ../../src/common
    PRIVATE ../../src/libponyc
    PRIVATE ../../build/libs/include
)

target_link_directories(libponyc.benchmarks
    PRIVATE ../../build/libs/lib
)

target_link_libraries(libponyc.benchmarks
    PRIVATE libponyc
    PRIVATE libponyrt
    PRIVATE blake2
    PRIVATE benchmark::benchmark
)

if (MSVC)
    target_link_libraries(libponyc.benchmarks
        PRIVATE dbghelp
        PRIVATE ucrt
        PRIVATE Ws2_32
        PRIVATE Shlwapi
    )
elseif(${CMAKE_HOST_SYSTEM_NAME} MATCHES "Darwin")
    target_link_libraries(libponyc.benchmarks PRIVATE dl)
elseif(${CMAKE_HOST_SYSTEM_NAME} MATCHES "BSD")
    target_link_libraries(libponyc.benchmarks PRIVATE execinfo)
elseif(${CMAKE_HOST_SYSTEM_NAME} MATCHES "DragonFly")
    target_link_libraries(libponyc.benchmarks PRIVATE execinfo atomic)
else()
    target_link_libraries(libponyc.benchmarks PRIVATE atomic dl)
    target_link_options(libponyc.benchmarks PRIVATE "-static-libstdc++")
endif ()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(libponyc.benchmarks PRIVATE "-static-libgcc")
endif()

if(NOT PONY_CROSS_LIBPONYRT)
    target_link_libraries(libponyc.benchmarks PRIVATE ${PONYC_LLVM_LIBS})
endif()
