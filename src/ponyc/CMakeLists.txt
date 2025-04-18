cmake_minimum_required(VERSION 3.21 FATAL_ERROR)

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
        execute_process(COMMAND ${CMAKE_CXX_COMPILER} --print-file-name=libstdc++.a
            OUTPUT_VARIABLE LIBSTDCXX_PATH
            OUTPUT_STRIP_TRAILING_WHITESPACE)

        target_link_libraries(ponyc PRIVATE atomic dl)

        if(LIBSTDCXX_PATH STREQUAL "libstdc++.a")
            execute_process(COMMAND ${CMAKE_CXX_COMPILER} --print-file-name=libc++.a
                OUTPUT_VARIABLE LIBCXX_PATH
                OUTPUT_STRIP_TRAILING_WHITESPACE)

            if(LIBCXX_PATH STREQUAL "libc++.a")
                message(FATAL_ERROR "Cannot find C++ implementation, looked for libstdc++ and libc++")
            else()
                target_link_options(ponyc PRIVATE "-static-libc++")
            endif()
        else()
            target_link_options(ponyc PRIVATE "-static-libstdc++")
        endif()
    endif()

    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        execute_process(COMMAND ${CMAKE_CXX_COMPILER} --print-file-name=libgcc.a
            OUTPUT_VARIABLE LIBGCC_PATH
            OUTPUT_STRIP_TRAILING_WHITESPACE)

        if(NOT LIBGCC_PATH STREQUAL "libgcc.a")
            target_link_options(ponyc PRIVATE "-static-libgcc")
        endif()
    endif()
endif (MSVC)

if(NOT PONY_CROSS_LIBPONYRT)
    target_link_libraries(ponyc PRIVATE ${PONYC_LLVM_LIBS})
endif()
