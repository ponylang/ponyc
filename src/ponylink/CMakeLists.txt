cmake_minimum_required(VERSION 3.25 FATAL_ERROR)

project(ponylink VERSION ${PONYC_PROJECT_VERSION} LANGUAGES CXX)

add_executable(ponylink main.cc)

target_include_directories(ponylink
    PRIVATE ../../build/libs/include
)

target_link_directories(ponylink
    PRIVATE ../../build/libs/lib
)

if(MSVC)
    target_link_libraries(ponylink
        PRIVATE ${PONYC_LLD_LIBS}
        PRIVATE ${PONYC_LLVM_LIBS}
        PRIVATE "$<LINK_LIBRARY:WHOLE_ARCHIVE,libponylto>"
    )
else()
    target_link_libraries(ponylink PRIVATE
        ${PONYC_LLD_LIBS}
        ${PONYC_LLVM_LIBS}
        ${PONYC_EXTRA_LIBS}
        "$<LINK_LIBRARY:WHOLE_ARCHIVE,libponylto>"
    )

    if(${CMAKE_HOST_SYSTEM_NAME} MATCHES "Darwin")
        target_link_libraries(ponylink PRIVATE dl)
    elseif(${CMAKE_HOST_SYSTEM_NAME} MATCHES "BSD")
        target_link_libraries(ponylink PRIVATE execinfo)
    elseif(${CMAKE_HOST_SYSTEM_NAME} MATCHES "DragonFly")
        target_link_libraries(ponylink PRIVATE execinfo atomic)
    else()
        target_link_libraries(ponylink PRIVATE atomic dl)

        execute_process(COMMAND ${CMAKE_CXX_COMPILER} --print-file-name=libstdc++.a
            OUTPUT_VARIABLE LIBSTDCXX_PATH
            OUTPUT_STRIP_TRAILING_WHITESPACE)

        if(LIBSTDCXX_PATH STREQUAL "libstdc++.a")
            execute_process(COMMAND ${CMAKE_CXX_COMPILER} --print-file-name=libc++.a
                OUTPUT_VARIABLE LIBCXX_PATH
                OUTPUT_STRIP_TRAILING_WHITESPACE)

            if(LIBCXX_PATH STREQUAL "libc++.a")
                message(FATAL_ERROR "Cannot find C++ implementation")
            else()
                target_link_options(ponylink PRIVATE "-static-libc++")
            endif()
        else()
            target_link_options(ponylink PRIVATE "-static-libstdc++")
        endif()
    endif()

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        execute_process(COMMAND ${CMAKE_CXX_COMPILER} --print-file-name=libgcc.a
            OUTPUT_VARIABLE LIBGCC_PATH
            OUTPUT_STRIP_TRAILING_WHITESPACE)

        if(NOT LIBGCC_PATH STREQUAL "libgcc.a")
            target_link_options(ponylink PRIVATE "-static-libgcc")
        endif()
    endif()
endif()

if(${CMAKE_HOST_SYSTEM_NAME} MATCHES "Darwin")
    set(CMAKE_OSX_DEPLOYMENT_TARGET ${PONY_OSX_PLATFORM})
endif()
