cmake_minimum_required(VERSION 3.25 FATAL_ERROR)

project(ponyc VERSION ${PONYC_PROJECT_VERSION} LANGUAGES CXX)

# ponyc, the program users run. It starts the compiler (the ponyc-compiler
# target) next to itself.
add_executable(ponyc main.cc)

target_include_directories(ponyc
    PRIVATE ../common
)

target_compile_definitions(ponyc PRIVATE
    "PONY_COMPILER_NAME=\"$<TARGET_FILE_NAME:ponyc-compiler>\""
)

if(MSVC)
    target_compile_definitions(ponyc PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    if(NOT ${CMAKE_HOST_SYSTEM_NAME} MATCHES "Darwin|BSD|DragonFly")
        execute_process(COMMAND ${CMAKE_CXX_COMPILER} --print-file-name=libstdc++.a
            OUTPUT_VARIABLE LIBSTDCXX_PATH
            OUTPUT_STRIP_TRAILING_WHITESPACE)

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

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        execute_process(COMMAND ${CMAKE_CXX_COMPILER} --print-file-name=libgcc.a
            OUTPUT_VARIABLE LIBGCC_PATH
            OUTPUT_STRIP_TRAILING_WHITESPACE)

        if(NOT LIBGCC_PATH STREQUAL "libgcc.a")
            target_link_options(ponyc PRIVATE "-static-libgcc")
        endif()
    endif()
endif()
