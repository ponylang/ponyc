cmake_minimum_required(VERSION 3.25 FATAL_ERROR)

project(libponyrt.tests VERSION ${PONYC_PROJECT_VERSION} LANGUAGES C CXX)

add_executable(libponyrt.tests
    util.cc
    ds/fun.cc
    ds/hash.cc
    ds/list.cc
    lang/socket.cc
    mem/heap.cc
    mem/pagemap.cc
    mem/pool.cc
    sched/cpu.cc
    sched/scheduler.cc
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
    # The Windows asio backend (sock_notify.c) calls Winsock directly
    # (closesocket, ProcessSocketNotifications), so the runtime now pulls in
    # ws2_32 wherever libponyrt is linked into an executable.
    target_link_libraries(libponyrt.tests PRIVATE Ws2_32)
elseif(${CMAKE_HOST_SYSTEM_NAME} MATCHES "Darwin")
    target_link_libraries(libponyrt.tests PRIVATE dl)
elseif(${CMAKE_HOST_SYSTEM_NAME} MATCHES "BSD")
    target_link_libraries(libponyrt.tests PRIVATE execinfo)
elseif(${CMAKE_HOST_SYSTEM_NAME} MATCHES "DragonFly")
    target_link_libraries(libponyrt.tests PRIVATE execinfo atomic)
else()
    target_link_libraries(libponyrt.tests PRIVATE atomic dl)
    target_link_options(libponyrt.tests PRIVATE "-static-libstdc++")
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(libponyrt.tests PRIVATE "-static-libgcc")
endif()
