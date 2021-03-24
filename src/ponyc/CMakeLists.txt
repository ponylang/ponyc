cmake_minimum_required(VERSION 3.11 FATAL_ERROR)

project(ponyc VERSION ${PONYC_PROJECT_VERSION} LANGUAGES C CXX)

add_executable(ponyc main.c)

target_include_directories(ponyc
    PRIVATE ../../src/common
    PRIVATE ../../src/libponyc
    PRIVATE ../../build/libs/include
)

target_link_directories(ponyc
    PRIVATE ../../build/libs/lib
)

if (MSVC)
    target_link_libraries(ponyc
        PRIVATE libponyc
        PRIVATE libponyrt
        PRIVATE blake2
        PRIVATE dbghelp
        PRIVATE ucrt
        PRIVATE Ws2_32
        PRIVATE Shlwapi
    )

    set_source_files_properties(main.c PROPERTIES LANGUAGE CXX)
else()
    target_link_libraries(ponyc PRIVATE
        libponyc
        libponyrt
        blake2
    )

    if(${CMAKE_HOST_SYSTEM_NAME} MATCHES "Darwin")
        target_link_libraries(ponyc PRIVATE dl)
    elseif(${CMAKE_HOST_SYSTEM_NAME} MATCHES "BSD")
        target_link_libraries(ponyc PRIVATE execinfo)
    elseif(${CMAKE_HOST_SYSTEM_NAME} MATCHES "DragonFly")
        target_link_libraries(ponyc PRIVATE execinfo atomic)
    else()
        target_link_libraries(ponyc PRIVATE atomic dl)
        target_link_options(ponyc PRIVATE "-static-libstdc++")
    endif()

    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_options(ponyc PRIVATE "-static-libgcc")
    endif()
endif (MSVC)

if(NOT PONY_CROSS_LIBPONYRT)
    target_link_libraries(ponyc PRIVATE ${PONYC_LLVM_LIBS})
endif()