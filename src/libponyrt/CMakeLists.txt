cmake_minimum_required(VERSION 3.25 FATAL_ERROR)

project(libponyrt VERSION ${PONYC_PROJECT_VERSION} LANGUAGES C CXX)

# NOTE: on FreeBSD with use=dtrace these sources are extracted from libponyrt.a
# by basename (see dtrace_freebsd_postbuild.sh), so their basenames must stay
# unique across directories. The postbuild script fails loudly if two collide.
set(_c_src
    actor/actor.c
    actor/messageq.c
    asio/asio.c
    asio/emscripten.c
    asio/epoll.c
    asio/event.c
    asio/kqueue.c
    asio/sock_notify.c
    asio/wfo.c
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

if(PONY_USE_DTRACE)
    set(_dtrace_d "${CMAKE_CURRENT_SOURCE_DIR}/../common/dtrace_probes.d")

    # A use=dtrace build before #5401 wrote the generated header into the
    # source tree at src/common/dtrace_probes.h. We now generate it into the
    # build tree, but dtrace.h includes it with a quoted
    # #include "dtrace_probes.h", which searches dtrace.h's own directory
    # (src/common) before any -I path — so a leftover source-tree copy would
    # still shadow the build-tree one. Remove any stale copy so only the
    # build-tree header can be found.
    file(REMOVE "${CMAKE_CURRENT_SOURCE_DIR}/../common/dtrace_probes.h")

    # The runtime sources include the generated dtrace_probes.h (via dtrace.h),
    # so it must exist before they compile. Generate it into the build tree (not
    # the source tree) and make libponyrt depend on it. The macOS, SystemTap
    # (Linux), and illumos-derived (FreeBSD) dtrace all emit the header the
    # same way with `dtrace -h`; only the object-generation step below differs
    # between them.
    set(_dtrace_header "${libponyrt_BINARY_DIR}/dtrace_probes.h")
    add_custom_command(OUTPUT "${_dtrace_header}"
        COMMAND dtrace -h -s "${_dtrace_d}" -o "${_dtrace_header}"
        DEPENDS "${_dtrace_d}"
    )
    add_custom_target(dtrace_probes_header DEPENDS "${_dtrace_header}")
    add_dependencies(libponyrt dtrace_probes_header)
    target_include_directories(libponyrt PRIVATE "${libponyrt_BINARY_DIR}")

    if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
        # FreeBSD ships the illumos-derived dtrace(1), whose `-G` step works
        # very differently from the SystemTap `dtrace` on Linux and the macOS
        # one:
        #
        #   * It does NOT synthesize the probe object from the .d script alone.
        #     It scans the *compiled* runtime objects for probe call sites,
        #     rewrites those sites in place (turning the undefined __dtrace_*
        #     references into resolved absolute symbols), and emits a separate
        #     object that carries only the DOF plus a constructor that
        #     registers the probes at process start.
        #   * Because the call sites are rewritten in the runtime objects,
        #     those same objects must be the ones archived into libponyrt. The
        #     emitted DOF object must then be linked separately, whole-archive,
        #     so its (otherwise unreferenced) registration constructor survives
        #     the link. genexe.cc already does this via
        #     `--whole-archive -ldtrace_probes`.
        #
        # So after libponyrt.a is built we extract its (pristine) objects, run
        # `dtrace -G` over them, re-archive libponyrt.a from the patched
        # objects, and stash the DOF object in its own libdtrace_probes.a.
        # Doing it in a POST_BUILD step over the archive keeps CMake's tracked
        # objects pristine, so every rebuild redoes the rewrite from clean
        # objects (no "dtrace -G over an already-patched object" hazard) and
        # works under the Makefile generator, where an OBJECT library's outputs
        # aren't usable as custom-command deps. The non-FreeBSD path below
        # keeps the simpler scheme: generate the probe object from the .d alone
        # and fold it straight into libponyrt.
        #
        # POST_BUILD runs whenever libponyrt.a is (re)built, which covers
        # source changes. It also produces libdtrace_probes.a as a side effect,
        # so both archives only regenerate when libponyrt relinks. Two
        # consequences: editing dtrace_probes.d alone (without touching a .c)
        # won't re-run this, and deleting libdtrace_probes.a by hand won't
        # recreate it on its own — do a clean rebuild in either case.
        add_custom_command(TARGET libponyrt POST_BUILD
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/dtrace_freebsd_postbuild.sh"
                    "$<TARGET_FILE:libponyrt>"
                    "${libponyrt_BINARY_DIR}/libdtrace_probes.a"
                    "${_dtrace_d}"
                    "${CMAKE_AR}"
            VERBATIM
        )
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        # macOS: `dtrace -h` (above) is sufficient. The macOS dtrace has no
        # `-G` flag; instead, the LLD MachO linker resolves __dtrace_probe$
        # and __dtrace_isenabled$ symbols natively — no probe object needed.
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        # SystemTap (Linux): `dtrace -G` builds the probe object from the .d
        # alone; fold it straight into libponyrt.
        add_custom_command(OUTPUT dtrace_probes.o
            COMMAND dtrace -G -s "${_dtrace_d}" -o dtrace_probes.o
            DEPENDS "${_dtrace_d}"
        )
        target_sources(libponyrt PRIVATE dtrace_probes.o)
    else()
        message(WARNING "use=dtrace: no probe-generation support for ${CMAKE_SYSTEM_NAME}; probes will be no-ops")
    endif()
endif()

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
)

if (MSVC)
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
    elseif(${CMAKE_HOST_SYSTEM_NAME} MATCHES "Haiku")
        set(LIBPONYRT_ARCHIVE libponyrt.a)
    else()
        set(LIBPONYRT_ARCHIVE libponyrt-pic.a)
    endif()

    # copy libponyrt to the ponyc directory for use when linking pony programs
    add_custom_command(TARGET libponyrt POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${libponyrt_BINARY_DIR}/libponyrt.a ${PONY_OUTPUT_DIR}/${LIBPONYRT_ARCHIVE}
    )

    # On FreeBSD the DOF object lives in its own archive built by the POST_BUILD
    # script above. Copy it next to libponyrt.a so a ponyc run from the build
    # dir can resolve `-ldtrace_probes`. This is registered after that script
    # (both are POST_BUILD on libponyrt, run in order), so the archive exists.
    if(PONY_USE_DTRACE AND CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
        add_custom_command(TARGET libponyrt POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy ${libponyrt_BINARY_DIR}/libdtrace_probes.a ${PONY_OUTPUT_DIR}/libdtrace_probes.a
        )
    endif()
endif (MSVC)

if(PONY_RUNTIME_BITCODE)
    if (NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "You can only use runtime-bitcode with Clang.")
    endif()

    set(_c_flags ${CMAKE_C_FLAGS})
    separate_arguments(_c_flags)

    set(_ll_src_all ${_c_src})
    foreach(_src_file ${_ll_src_all})
        #message("${libponyrt_SOURCE_DIR}/${_src_file} -> ${libponyrt_BINARY_DIR}/${_src_file}.bc")
        get_filename_component(_src_dir ${_src_file} DIRECTORY)
        add_custom_command(
            COMMAND mkdir -p "${libponyrt_BINARY_DIR}/${_src_dir}" && ${CMAKE_C_COMPILER} $<IF:$<BOOL:${CMAKE_C_COMPILER_TARGET}>,--target=${CMAKE_C_COMPILER_TARGET},> -DBUILD_COMPILER="${CMAKE_C_COMPILER_VERSION}" -D_FILE_OFFSET_BITS=64 -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS -DLLVM_BUILD_MODE=${PONY_LLVM_BUILD_MODE} -DLLVM_VERSION="${LLVM_VERSION}" -DPONY_COMPILER="${CMAKE_C_COMPILER}" -DPONY_ARCH="${CMAKE_SYSTEM_PROCESSOR}" -DPONY_BUILD_CONFIG="release" -DPONY_USE_BIGINT -DPONY_VERSION="${PONYC_VERSION}" -DPONY_VERSION_STR="${PONYC_VERSION} [release]\\nCompiled with: LLVM ${LLVM_VERSION} -- ${CMAKE_C_COMPILER_ID}-${CMAKE_C_COMPILER_VERSION}-${CMAKE_C_COMPILER_ARCHITECTURE_ID}" -pthread -fexceptions -Werror -Wconversion -Wno-sign-conversion -Wno-atomic-alignment -Wextra -Wall ${_c_flags} -I. -I../common -emit-llvm -o "${libponyrt_BINARY_DIR}/${_src_file}.bc" -c ${_src_file}
            WORKING_DIRECTORY ${libponyrt_SOURCE_DIR}
            DEPENDS "${libponyrt_SOURCE_DIR}/${_src_file}"
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

    add_custom_command(TARGET libponyrt_bc POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${libponyrt_BINARY_DIR}/libponyrt.bc ${PONY_OUTPUT_DIR}/
    )
endif()
