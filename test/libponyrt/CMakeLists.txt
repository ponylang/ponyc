cmake_minimum_required(VERSION 3.11 FATAL_ERROR)

project(libponyrt.tests VERSION ${PONYC_PROJECT_VERSION} LANGUAGES C CXX)

add_executable(libponyrt.tests
    util.cc
    ds/fun.cc
    ds/hash.cc
    ds/list.cc
    lang/error.cc
    mem/heap.cc
    mem/pagemap.cc
    mem/pool.cc
)

target_include_directories(libponyrt.tests
    PRIVATE ../../src/common
    PRIVATE ../../src/libponyrt
    PRIVATE ../../build/libs/include
)

target_link_libraries(libponyrt.tests
    PRIVATE libponyrt
    PRIVATE GTest::gtest
)

if (MSVC)
    target_link_libraries(libponyrt.tests PRIVATE dbghelp)
elseif(${CMAKE_HOST_SYSTEM_NAME} MATCHES "Darwin")
    target_link_libraries(libponyrt.tests PRIVATE dl)
elseif(${CMAKE_HOST_SYSTEM_NAME} MATCHES "BSD|DragonFly")
    target_link_libraries(libponyrt.tests PRIVATE execinfo)
else()
    target_link_libraries(libponyrt.tests PRIVATE atomic dl)
    target_link_options(libponyrt.tests PRIVATE "-static-libstdc++")
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(libponyrt.tests PRIVATE "-static-libgcc")
endif()
