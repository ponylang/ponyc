cmake_minimum_required(VERSION 3.25 FATAL_ERROR)

project(libponyrt VERSION ${PONYC_PROJECT_VERSION} LANGUAGES C CXX)

set(_c_src
    actor/actor.c
    actor/messageq.c
    asio/asio.c
    asio/emscripten.c
    asio/epoll.c
    asio/event.c
    asio/kqueue.c
    asio/sock_notify.c
    ds/fun.c
    ds/hash.c
    ds/list.c
    ds/stack.c
    gc/actormap.c
    gc/cycle.c
    gc/delta.c
    gc/gc.c
    gc/objectmap.c
    gc/trace.c
    lang/directory.c
    lang/errno.c
    lang/io.c
    lang/paths.c
    lang/process.c
    lang/socket.c
    lang/ssl.c
    lang/stat.c
    lang/stdfd.c
    lang/time.c
    mem/alloc.c
    mem/heap.c
    mem/pagemap.c
    mem/pool.c
    mem/pool_arena.c
    mem/pool_memalign.c
    options/options.c
    platform/ponyassert.c
    platform/threads.c
    sched/cpu.c
    sched/mpmcq.c
    sched/mutemap.c
    sched/scheduler.c
    sched/start.c
    sched/systematic_testing.c
    tracing/tracing.c
)

if(${CMAKE_HOST_SYSTEM_NAME} MATCHES "Darwin")
    set(CMAKE_OSX_DEPLOYMENT_TARGET ${PONY_OSX_PLATFORM})
endif()

add_library(libponyrt STATIC
    ${_c_src}
)


if (NOT MSVC)
    if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86-64")
        target_compile_options(libponyrt PUBLIC -mcx16)
    elseif(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
        target_compile_options(libponyrt PUBLIC -mcx16)
    elseif(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "amd64")
        target_compile_options(libponyrt PUBLIC -mcx16)
    elseif(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "AMD64")
        target_compile_options(libponyrt PUBLIC -mcx16)
    elseif(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86")
        target_compile_options(libponyrt PUBLIC -mcx16)
    endif()
endif()

target_include_directories(libponyrt
    PUBLIC .
    PUBLIC ../common
    PRIVATE ${CMAKE_BINARY_DIR}/pony_generated
)

add_dependencies(libponyrt pony_version)

if (MSVC)
    # VirtualAlloc2, which mem/alloc.c calls, is not exported by kernel32.lib.
    # INTERFACE so every executable linking libponyrt gets it; ponyc links pony
    # programs with its own list, in src/libponyc/platform/vcvars.c.
    target_link_libraries(libponyrt INTERFACE onecore)

    file(GLOB_RECURSE CFILES "${PROJECT_SOURCE_DIR}/*.c")
    set_source_files_properties(${CFILES} PROPERTIES LANGUAGE CXX)

    # copy libponyrt to the ponyc directory for use when linking pony programs
    add_custom_command(TARGET libponyrt POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libponyrt> ${PONY_OUTPUT_DIR}/libponyrt.lib
    )
else()
    target_include_directories(libponyrt
        PRIVATE /usr/local/include
    )

    if(${CMAKE_HOST_SYSTEM_NAME} MATCHES "Darwin")
        set(LIBPONYRT_ARCHIVE libponyrt.a)
    elseif(${CMAKE_HOST_SYSTEM_NAME} MATCHES "BSD|DragonFly")
        set(LIBPONYRT_ARCHIVE libponyrt.a)
    else()
        set(LIBPONYRT_ARCHIVE libponyrt-pic.a)
    endif()

    # copy libponyrt to the ponyc directory for use when linking pony programs
    add_custom_command(TARGET libponyrt POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${libponyrt_BINARY_DIR}/libponyrt.a ${PONY_OUTPUT_DIR}/${LIBPONYRT_ARCHIVE}
    )

endif (MSVC)

if(PONY_RUNTIME_BITCODE)
    if (NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "You can only use runtime-bitcode with Clang.")
    endif()

    set(_c_flags ${CMAKE_C_FLAGS})
    separate_arguments(_c_flags)

    # The runtime bitcode is merged into the user's program and re-targeted
    # at compile time (codegen_stamp_target_attrs overwrites target-cpu and
    # target-features on every function), so the -march used here only
    # affects clang's mid-level optimisation decisions.  Compile with the
    # widest feature set for the CPU family so that those decisions — vector
    # widths, instruction selection during IR transforms — are as good as
    # possible regardless of what the end user targets.
    if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86-64"
        OR ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64"
        OR ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "amd64"
        OR ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "AMD64"
        OR ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86")
        list(FILTER _c_flags EXCLUDE REGEX "^-march=")
        list(FILTER _c_flags EXCLUDE REGEX "^-mtune=")
        list(APPEND _c_flags -march=x86-64-v4 -mcx16)
    elseif(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "aarch64"
        OR ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "arm64"
        OR ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "ARM64")
        list(FILTER _c_flags EXCLUDE REGEX "^-march=")
        list(FILTER _c_flags EXCLUDE REGEX "^-mtune=")
        list(APPEND _c_flags -march=armv8.9-a+crypto+sha3+sm4+dotprod+fp16fml+bf16+i8mm+lse+lse128+rcpc3+pauth+memtag+rng+flagm+predres+ssbs+sb+ls64+hbc+mops+cssc+the+gcs+ite)
    endif()

    if(CMAKE_C_COMPILER_ID STREQUAL "AppleClang")
        list(APPEND _c_flags -fno-stack-check)
    endif()

    set(_ll_src_all ${_c_src})
    foreach(_src_file ${_ll_src_all})
        #message("${libponyrt_SOURCE_DIR}/${_src_file} -> ${libponyrt_BINARY_DIR}/${_src_file}.bc")
        get_filename_component(_src_dir ${_src_file} DIRECTORY)
        add_custom_command(
            COMMAND mkdir -p "${libponyrt_BINARY_DIR}/${_src_dir}" && ${CMAKE_C_COMPILER} --target=$<IF:$<BOOL:${CMAKE_C_COMPILER_TARGET}>,${CMAKE_C_COMPILER_TARGET},${LLVM_TARGET_TRIPLE}> -DBUILD_COMPILER="${CMAKE_C_COMPILER_VERSION}" -D_FILE_OFFSET_BITS=64 -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS -DLLVM_BUILD_MODE=${PONY_LLVM_BUILD_MODE} -DLLVM_VERSION="${LLVM_VERSION}" -DPONY_COMPILER="${CMAKE_C_COMPILER}" -DPONY_ARCH="${CMAKE_SYSTEM_PROCESSOR}" -DPONY_BUILD_CONFIG="release" -DNDEBUG -DPONY_USE_BIGINT ${PONY_RUNTIME_DEFINES} -pthread -fexceptions -Werror -Wconversion -Wno-sign-conversion -Wno-atomic-alignment -Wextra -Wall ${_c_flags} -I${libponyrt_SOURCE_DIR} -I${libponyrt_SOURCE_DIR}/../common -I${CMAKE_BINARY_DIR}/pony_generated -I/usr/local/include -O2 -emit-llvm -MD -MF "${libponyrt_BINARY_DIR}/${_src_file}.bc.d" -o "${libponyrt_BINARY_DIR}/${_src_file}.bc" -c "${libponyrt_SOURCE_DIR}/${_src_file}"
            DEPENDS "${libponyrt_SOURCE_DIR}/${_src_file}"
            DEPFILE "${libponyrt_BINARY_DIR}/${_src_file}.bc.d"
            OUTPUT "${libponyrt_BINARY_DIR}/${_src_file}.bc"
            VERBATIM
        )
    endforeach()

    set(_ll_bc_all ${_ll_src_all})
    list(TRANSFORM _ll_bc_all PREPEND "${libponyrt_BINARY_DIR}/")
    list(TRANSFORM _ll_bc_all APPEND ".bc")

    add_custom_target(libponyrt_bc ALL
        ${libponyrt_SOURCE_DIR}/../../build/libs/bin/llvm-link -o "${libponyrt_BINARY_DIR}/libponyrt.bc" ${_ll_bc_all}
        DEPENDS ${_ll_bc_all}
    )
    add_dependencies(libponyrt_bc pony_version)

    add_custom_command(TARGET libponyrt_bc POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${libponyrt_BINARY_DIR}/libponyrt.bc ${PONY_OUTPUT_DIR}/
    )
endif()
