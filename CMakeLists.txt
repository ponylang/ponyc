cmake_minimum_required(VERSION 3.21 FATAL_ERROR)

file(STRINGS "VERSION" PONYC_PROJECT_VERSION)
project(Pony VERSION ${PONYC_PROJECT_VERSION} LANGUAGES C CXX)

# Grab the PonyC version number from the "VERSION" source file.
if(NOT DEFINED PONYC_VERSION)
    execute_process(
        COMMAND git rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_REVISION
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    set(PONYC_VERSION "${PONYC_PROJECT_VERSION}-${GIT_REVISION}")
endif()

# Uncomment this to show build commands
# set(CMAKE_VERBOSE_MAKEFILE ON)

# We require C++17 (because LLVM does)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# We require LLVM, Google Test and Google Benchmark
if(NOT PONY_CROSS_LIBPONYRT)
    find_package(LLVM REQUIRED CONFIG PATHS "build/libs/lib/cmake/llvm" "build/libs/lib64/cmake/llvm" NO_DEFAULT_PATH)
    find_package(LLD REQUIRED CONFIG PATHS "build/libs/lib/cmake/lld" "build/libs/lib64/cmake/lld" NO_DEFAULT_PATH)
    find_package(GTest REQUIRED CONFIG PATHS "build/libs/lib/cmake/GTest" "build/libs/lib64/cmake/GTest" NO_DEFAULT_PATH)
    find_package(benchmark REQUIRED CONFIG PATHS "build/libs/lib/cmake/benchmark" "build/libs/lib64/cmake/benchmark" NO_DEFAULT_PATH)
endif()

# Uses
if(PONY_USE_VALGRIND)
    set(PONY_OUTPUT_SUFFIX "${PONY_OUTPUT_SUFFIX}-valgrind")
    add_compile_options(-DUSE_VALGRIND)
endif()

if(PONY_USE_THREAD_SANITIZER)
    set(PONY_OUTPUT_SUFFIX "${PONY_OUTPUT_SUFFIX}-thread_sanitizer")
    list(APPEND PONY_SANITIZERS_ENABLED "thread")
endif()

if(PONY_USE_ADDRESS_SANITIZER)
    set(PONY_OUTPUT_SUFFIX "${PONY_OUTPUT_SUFFIX}-address_sanitizer")
    list(APPEND PONY_SANITIZERS_ENABLED "address")
    add_compile_options(-DUSE_ADDRESS_SANITIZER)
    add_link_options(-DUSE_ADDRESS_SANITIZER)
endif()

if(PONY_USE_UNDEFINED_BEHAVIOR_SANITIZER)
    set(PONY_OUTPUT_SUFFIX "${PONY_OUTPUT_SUFFIX}-undefined_behavior_sanitizer")
    list(APPEND PONY_SANITIZERS_ENABLED "undefined")
endif()

if(PONY_SANITIZERS_ENABLED)
    list(JOIN PONY_SANITIZERS_ENABLED "," PONY_SANITIZERS_VALUE)
    add_compile_options(-fsanitize=${PONY_SANITIZERS_VALUE} -DPONY_SANITIZER=\"${PONY_SANITIZERS_VALUE}\")
    add_link_options(-fsanitize=${PONY_SANITIZERS_VALUE} -DPONY_SANITIZER=\"${PONY_SANITIZERS_VALUE}\")
endif()

if(PONY_USE_COVERAGE)
    set(PONY_OUTPUT_SUFFIX "${PONY_OUTPUT_SUFFIX}-coverage")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        add_compile_options(-O0 -fprofile-instr-generate -fcoverage-mapping)
        add_link_options(-fprofile-instr-generate -fcoverage-mapping)
    else()
        add_compile_options(-O0 -fprofile-arcs -ftest-coverage)
        add_link_options(-fprofile-arcs)
    endif()
endif()

if(PONY_USE_POOLTRACK)
    set(PONY_OUTPUT_SUFFIX "${PONY_OUTPUT_SUFFIX}-pooltrack")
    add_compile_options(-DUSE_POOLTRACK)
endif()

if(PONY_USE_DTRACE)
    set(PONY_OUTPUT_SUFFIX "${PONY_OUTPUT_SUFFIX}-dtrace")
    add_compile_options(-DUSE_DYNAMIC_TRACE)
endif()

if(PONY_USE_RUNTIMESTATS)
    set(PONY_OUTPUT_SUFFIX "${PONY_OUTPUT_SUFFIX}-runtimestats")
    add_compile_options(-DUSE_RUNTIMESTATS)
endif()

if(PONY_USE_SCHEDULER_SCALING_PTHREADS)
    set(PONY_OUTPUT_SUFFIX "${PONY_OUTPUT_SUFFIX}-scheduler_scaling_pthreads")
    add_compile_options(-DUSE_SCHEDULER_SCALING_PTHREADS)
endif()

if(PONY_USE_SYSTEMATIC_TESTING)
    set(PONY_OUTPUT_SUFFIX "${PONY_OUTPUT_SUFFIX}-systematic_testing")
    add_compile_options(-DUSE_SYSTEMATIC_TESTING)
endif()

if(PONY_USE_RUNTIMESTATS_MESSAGES)
    set(PONY_OUTPUT_SUFFIX "${PONY_OUTPUT_SUFFIX}-runtimestats_messages")
    add_compile_options(-DUSE_RUNTIMESTATS -DUSE_RUNTIMESTATS_MESSAGES)
endif()

if(PONY_USE_POOL_MEMALIGN)
    set(PONY_OUTPUT_SUFFIX "${PONY_OUTPUT_SUFFIX}-pool_memalign")
    add_compile_options(-DUSE_POOL_MEMALIGN)
endif()

if(PONY_USE_POOL_RETAIN)
    set(PONY_OUTPUT_SUFFIX "${PONY_OUTPUT_SUFFIX}-pool_retain")
    add_compile_options(-DUSE_POOL_RETAIN)
endif()

if(PONY_USE_RUNTIME_TRACING)
    set(PONY_OUTPUT_SUFFIX "${PONY_OUTPUT_SUFFIX}-runtime_tracing")
    add_compile_options(-DUSE_RUNTIME_TRACING)
endif()

# LibPonyC tests assume that our outputs are two directories above the root directory.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/../debug${PONY_OUTPUT_SUFFIX}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/../release${PONY_OUTPUT_SUFFIX}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/../minsizerel${PONY_OUTPUT_SUFFIX}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/../relwithdebinfo${PONY_OUTPUT_SUFFIX}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/../${CMAKE_BUILD_TYPE}${PONY_OUTPUT_SUFFIX}")

# Libs are now always built in release mode.
set(PONY_LLVM_BUILD_MODE "1")

if(NOT DEFINED PONY_ARCH)
    set(PONY_ARCH "native")
endif()

# Sanitizer link fragment capture.
#
# When ponyc is built with sanitizers, libponyrt's C code is instrumented, so
# every executable ponyc links references the sanitizer runtime (__asan_* etc.)
# and must link it. ponyc links natively via embedded LLD, which does not go
# through the compiler driver, so it cannot rely on -fsanitize= to pull the
# runtime in. Instead we ask the driver, at configure time, for the exact link
# fragment it emits for -fsanitize=${PONY_SANITIZERS_VALUE} and embed it as a
# generated header that the embedded linker splices in (genexe.cc).
#
# Asking the driver (rather than locating the runtime libraries ourselves)
# avoids re-implementing its per-compiler library selection, ubsan-folding, and
# wrapping rules, and avoids depending on a compiler's runtime-directory layout
# (clang's libclang_rt.asan-<arch>.a vs per-target lib/<triple>/libclang_rt.asan.a
# vs gcc's libasan_preinit.o + shared libasan): the driver emits whatever paths
# and flags are correct for itself. This works for both clang (whole-archived
# clang_rt static archives + --dynamic-list) and gcc (preinit object + shared
# -lasan/-lubsan). On macOS, Apple clang emits the clang_rt dylib by absolute
# path plus -rpath entries (and folds UBSan into ASan when both are enabled).
# The fragment's library paths are absolute on the build machine, which is fine:
# a sanitizer ponyc is a developer/CI artifact that runs where it was built.
# Native Linux, FreeBSD, and macOS sanitizer builds consume this (see
# genexe.cc); the splice is gated on !is_cross_compiling.

# Extract the linker subcommand's arguments from a compiler "-###" stderr dump.
# The link subcommand is the line naming the probe object (clang emits a direct
# ld line, gcc a collect2 line; both name the object). Tokenize it respecting
# shell quoting via separate_arguments — clang double-quotes every token, gcc
# leaves most bare, and separate_arguments handles both. Drop volatile tokens:
# gcc's LTO plugin injects "-plugin-opt=-fresolution=<tmp>.res" with a fresh
# temp path on every invocation, which otherwise differs between the baseline
# and sanitized probe runs and desyncs the ordered diff below.
function(_pony_sanitizer_link_tokens raw outvar)
    string(REGEX MATCH "[^\n]*pony_sanitizer_probe\\.o[^\n]*" _line "${raw}")
    separate_arguments(_toks UNIX_COMMAND "${_line}")
    set(_out "")
    foreach(_t IN LISTS _toks)
        if(NOT _t MATCHES "fresolution=" AND NOT _t MATCHES "\\.res$")
            list(APPEND _out "${_t}")
        endif()
    endforeach()
    set(${outvar} "${_out}" PARENT_SCOPE)
endfunction()

if(PONY_SANITIZERS_ENABLED)
    set(_san_probe_c "${CMAKE_BINARY_DIR}/pony_sanitizer_probe.c")
    set(_san_probe_o "${CMAKE_BINARY_DIR}/pony_sanitizer_probe.o")
    set(_san_probe_bin "${CMAKE_BINARY_DIR}/pony_sanitizer_probe.bin")
    file(WRITE "${_san_probe_c}" "int main(void){return 0;}\n")

    # -### prints the link command without running it, but only for an input
    # that exists, so compile a real (uninstrumented) object first. No -march or
    # other arch flag is passed to any probe: the sanitizer runtime the driver
    # selects depends on the target (the driver's default target == this native
    # host, which is what we link for), not on -march, so an arch flag would only
    # add an unrelated way for the probe to fail.
    execute_process(
        COMMAND ${CMAKE_C_COMPILER} -c ${_san_probe_c} -o ${_san_probe_o}
        RESULT_VARIABLE _san_probe_rc
        OUTPUT_QUIET ERROR_QUIET)
    if(NOT _san_probe_rc EQUAL 0)
        message(FATAL_ERROR "sanitizer link capture: failed to compile probe object")
    endif()

    # Capture the driver's link line with and without -fsanitize=. -### writes
    # the subcommands to stderr. Check the exit codes: if the driver can't emit
    # a link line at all (e.g. an unrecognized -fsanitize= value), fail here with
    # the real cause rather than letting it surface downstream as a confusing
    # "not a supersequence" error. (A mutually-exclusive combo like
    # thread,address is accepted by -### but rejected later when libponyrt
    # compiles with those flags, so it fails loudly there - not caught here.)
    execute_process(
        COMMAND ${CMAKE_C_COMPILER} "-###" ${_san_probe_o} -o ${_san_probe_bin}
        RESULT_VARIABLE _san_baseline_rc
        ERROR_VARIABLE _san_baseline_raw OUTPUT_QUIET)
    execute_process(
        COMMAND ${CMAKE_C_COMPILER} -fsanitize=${PONY_SANITIZERS_VALUE} "-###" ${_san_probe_o} -o ${_san_probe_bin}
        RESULT_VARIABLE _san_sanitized_rc
        ERROR_VARIABLE _san_sanitized_raw OUTPUT_QUIET)
    if(NOT _san_baseline_rc EQUAL 0 OR NOT _san_sanitized_rc EQUAL 0)
        message(FATAL_ERROR
            "sanitizer link capture: ${CMAKE_C_COMPILER} failed to emit a link "
            "line for -fsanitize=${PONY_SANITIZERS_VALUE} (-### exited non-zero)")
    endif()

    _pony_sanitizer_link_tokens("${_san_baseline_raw}" _san_base_toks)
    _pony_sanitizer_link_tokens("${_san_sanitized_raw}" _san_san_toks)

    # Ordered diff. The sanitized link line is a supersequence of the baseline
    # (-fsanitize only inserts arguments, never removes or reorders existing
    # ones, once the volatile LTO token is filtered out). A two-pointer scan
    # over the sanitized tokens, advancing the baseline pointer on each match,
    # collects exactly the inserted arguments in driver order: clang's
    # whole-archive/clang_rt/--dynamic-list block plus extra syslibs, or gcc's
    # libasan_preinit.o + -lasan/-lubsan. Nothing is hardcoded. If the baseline
    # is not fully consumed in order the supersequence assumption is violated (a
    # new volatile token, or a driver that reorders existing args); we stop with
    # a clear error rather than emit a fragment we can't trust.
    list(LENGTH _san_base_toks _san_base_len)
    set(_san_j 0)
    set(_san_fragment "")
    foreach(_t IN LISTS _san_san_toks)
        set(_san_matched FALSE)
        if(_san_j LESS _san_base_len)
            list(GET _san_base_toks ${_san_j} _san_bt)
            if(_t STREQUAL _san_bt)
                math(EXPR _san_j "${_san_j} + 1")
                set(_san_matched TRUE)
            endif()
        endif()
        if(NOT _san_matched)
            list(APPEND _san_fragment "${_t}")
        endif()
    endforeach()

    if(NOT _san_j EQUAL _san_base_len)
        message(FATAL_ERROR
            "sanitizer link capture: the -fsanitize link line is not a "
            "supersequence of the baseline after filtering volatile tokens "
            "(a new non-deterministic token, or a driver that reorders existing "
            "arguments). See discussion ponylang/ponyc#5399.")
    endif()

    # Sanity: the captured fragment must reference a sanitizer runtime, or the
    # extraction silently produced nothing useful. clang names it libclang_rt.*,
    # gcc names it libasan/-lasan/libubsan/libtsan; matching the sanitizer
    # substrings covers both.
    string(REGEX MATCH "asan|ubsan|tsan" _san_has_rt "${_san_fragment}")
    if(_san_has_rt STREQUAL "")
        message(FATAL_ERROR
            "sanitizer link capture: no sanitizer runtime (asan/ubsan/tsan) "
            "found in the captured fragment for -fsanitize=${PONY_SANITIZERS_VALUE}")
    endif()

    # Emit each token as a C string literal, escaping backslash then quote so a
    # toolchain path containing either can't break out of the literal. (Linux
    # toolchain paths don't normally contain these, but a malformed header is a
    # confusing failure mode worth closing off.)
    set(_san_array "")
    foreach(_a IN LISTS _san_fragment)
        string(REPLACE "\\" "\\\\" _a_esc "${_a}")
        string(REPLACE "\"" "\\\"" _a_esc "${_a_esc}")
        string(APPEND _san_array "  \"${_a_esc}\",\n")
    endforeach()

    set(_san_gen_dir "${CMAKE_BINARY_DIR}/pony_generated")
    file(MAKE_DIRECTORY "${_san_gen_dir}")
    file(WRITE "${_san_gen_dir}/sanitizer_link_args.h"
"// Generated by CMake (PONY_SANITIZERS_ENABLED block in the top-level\n"
"// CMakeLists.txt). Do not edit. The sanitizer runtime link fragment captured\n"
"// from the compiler driver for -fsanitize=${PONY_SANITIZERS_VALUE}.\n"
"#pragma once\n"
"#include <stddef.h>\n"
"static const char* const PONY_SANITIZER_LINK_ARGS[] = {\n${_san_array}};\n"
"static const size_t PONY_SANITIZER_LINK_ARGS_COUNT =\n"
"  sizeof(PONY_SANITIZER_LINK_ARGS) / sizeof(PONY_SANITIZER_LINK_ARGS[0]);\n")
    message("-- Captured sanitizer link fragment for "
        "-fsanitize=${PONY_SANITIZERS_VALUE}")

    # The probe scratch files have served their purpose at configure time.
    file(REMOVE "${_san_probe_c}" "${_san_probe_o}" "${_san_probe_bin}")
endif()

if(DEFINED PONY_CPU AND NOT PONY_CPU STREQUAL "")
    set(PONY_CPU_FLAG --cpu ${PONY_CPU})
else()
    set(PONY_CPU_FLAG "")
endif()

# System information
message("-- CMAKE_SYSTEM_INFO_FILE: ${CMAKE_SYSTEM_INFO_FILE}")
message("-- CMAKE_SYSTEM_NAME:      ${CMAKE_SYSTEM_NAME}")
message("-- CMAKE_SYSTEM_PROCESSOR: ${CMAKE_SYSTEM_PROCESSOR}")
message("-- CMAKE_SYSTEM:           ${CMAKE_SYSTEM}")

set(_compiler_arch ${CMAKE_C_COMPILER_ARCHITECTURE_ID})
if("${_compiler_arch}" STREQUAL "")
    set(_compiler_arch ${CMAKE_SYSTEM_PROCESSOR})
endif()

message("Compiler architecture is ${_compiler_arch}")

if(NOT MSVC)
    if((NOT DEFINED PONY_PIC_FLAG) OR (PONY_PIC_FLAG STREQUAL ""))
        set(PONY_PIC_FLAG "-fpic")
    endif()
endif()

set(PONY_OSX_PLATFORM 13.0.0)

# LLVM component setup
if(NOT PONY_CROSS_LIBPONYRT)
    set(LLVM_COMPONENTS
        core
        demangle
        objcarcopts
        orcjit
    )

    list (FIND LLVM_TARGETS_TO_BUILD "AArch64" _index)
    if (${_index} GREATER -1)
        list(APPEND
            LLVM_COMPONENTS
            aarch64asmparser
            aarch64codegen
            aarch64desc
            aarch64info
        )
    endif()

    list (FIND LLVM_TARGETS_TO_BUILD "ARM" _index)
    if (${_index} GREATER -1)
        list(APPEND
            LLVM_COMPONENTS
            armasmparser
            armcodegen
            armdesc
            arminfo
        )
    endif()

    list (FIND LLVM_TARGETS_TO_BUILD "WebAssembly" _index)
    if (${_index} GREATER -1)
        list(APPEND
            LLVM_COMPONENTS
            webassemblyasmparser
            webassemblycodegen
            webassemblydesc
            webassemblyinfo
        )
    endif()

    list (FIND LLVM_TARGETS_TO_BUILD "X86" _index)
    if (${_index} GREATER -1)
        list(APPEND
            LLVM_COMPONENTS
            x86asmparser
            x86codegen
            x86desc
            x86info
        )
    endif()

    list (FIND LLVM_TARGETS_TO_BUILD "RISCV" _index)
    if (${_index} GREATER -1)
        list(APPEND
            LLVM_COMPONENTS
            riscvasmparser
            riscvcodegen
            riscvdesc
            riscvinfo
        )
    endif()

    llvm_map_components_to_libnames(PONYC_LLVM_LIBS ${LLVM_COMPONENTS})
    # message("PONYC_LLVM_LIBS: ${PONYC_LLVM_LIBS}")

    set(PONYC_LLD_LIBS lldELF lldMachO lldCOFF lldWasm lldMinGW lldCommon)
    set(PONYC_SELFHOSTED_TOOL_PATH_ARGS "")

    if(NOT MSVC)
        find_package(ZLIB REQUIRED)
        set(PONYC_EXTRA_LIBS ZLIB::ZLIB)

        # Self-hosted Pony tools use `use "lib:z"` via pony_compiler. Pass the
        # resolved zlib libdir so embedded LLD can find non-system installs.
        foreach(PONYC_ZLIB_LIBRARY IN LISTS ZLIB_LIBRARIES)
            if(IS_ABSOLUTE "${PONYC_ZLIB_LIBRARY}")
                get_filename_component(PONYC_ZLIB_LIBRARY_DIR "${PONYC_ZLIB_LIBRARY}" DIRECTORY)
                list(APPEND PONYC_SELFHOSTED_TOOL_PATH_ARGS "--path" "${PONYC_ZLIB_LIBRARY_DIR}")
                break()
            endif()
        endforeach()
    endif()
endif()

# Required definitions.  We use these generators so that the defines are correct for both *nix (where the config applies at configuration time) and Windows (where the config applies at build time).
add_compile_definitions(
    BUILD_COMPILER="${CMAKE_C_COMPILER_VERSION}"
    _FILE_OFFSET_BITS=64
    __STDC_CONSTANT_MACROS
    __STDC_FORMAT_MACROS
    __STDC_LIMIT_MACROS
    LLVM_BUILD_MODE=${PONY_LLVM_BUILD_MODE}
    LLVM_VERSION="${LLVM_VERSION}"
    PONY_COMPILER="${CMAKE_C_COMPILER}"
    PONY_ARCH="${PONY_ARCH}"
    PONY_DEFAULT_PIC=true
    $<$<CONFIG:Debug>:PONY_BUILD_CONFIG="debug">
    $<$<CONFIG:Release>:PONY_BUILD_CONFIG="release">
    $<$<CONFIG:RelWithDebInfo>:PONY_BUILD_CONFIG="release">
    $<$<CONFIG:MinSizeRel>:PONY_BUILD_CONFIG="release">
    PONY_USE_BIGINT
    PONY_VERSION="${PONYC_VERSION}"
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
    $<$<CONFIG:RelWithDebInfo>:NDEBUG>
    $<$<CONFIG:MinSizeRel>:NDEBUG>
    $<$<CONFIG:Debug>:PONY_VERSION_STR="${PONYC_VERSION} [debug]\\nCompiled with: LLVM ${LLVM_VERSION} -- ${CMAKE_C_COMPILER_ID}-${CMAKE_C_COMPILER_VERSION}-${_compiler_arch}">
    $<$<CONFIG:Release>:PONY_VERSION_STR="${PONYC_VERSION} [release]\\nCompiled with: LLVM ${LLVM_VERSION} -- ${CMAKE_C_COMPILER_ID}-${CMAKE_C_COMPILER_VERSION}-${_compiler_arch}">
    $<$<CONFIG:RelWithDebInfo>:PONY_VERSION_STR="${PONYC_VERSION} [relwithdebinfo]\\nCompiled with: LLVM ${LLVM_VERSION} -- ${CMAKE_C_COMPILER_ID}-${CMAKE_C_COMPILER_VERSION}-${_compiler_arch}">
    $<$<CONFIG:MinSizeRel>:PONY_VERSION_STR="${PONYC_VERSION} [minsizerel]\\nCompiled with: LLVM ${LLVM_VERSION} -- ${CMAKE_C_COMPILER_ID}-${CMAKE_C_COMPILER_VERSION}-${_compiler_arch}">
    PONY_OSX_PLATFORM=${PONY_OSX_PLATFORM}
)

include(CheckIPOSupported)
if(PONY_USE_LTO)
    check_ipo_supported(RESULT _ipo_supported OUTPUT error)
endif()

if(_ipo_supported)
    message("Configuring with IPO/LTO")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    add_compile_definitions(PONY_USE_LTO)
endif()

if(MSVC)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)

    # Disable some warnings for Windows, and compile Debug builds with the regular CRT.
    add_compile_options(
        /wd4142
        /wd4996
        $<$<CONFIG:Debug>:/MD>
        /WX
        /EHa
    )
    add_compile_definitions(
        _MBCS
        _CRT_SECURE_NO_WARNINGS
        $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>
    )
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu11 -Werror -Wconversion -Wno-sign-conversion -Wextra -Wall -Wno-unknown-warning-option ${PONY_PIC_FLAG} -fexceptions")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11 -fno-rtti ${PONY_PIC_FLAG} -fexceptions")
    add_link_options(-rdynamic)
endif()

if(${CMAKE_HOST_SYSTEM_NAME} MATCHES "Darwin")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUSE_SCHEDULER_SCALING_PTHREADS")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif()

if(${CMAKE_HOST_SYSTEM_NAME} MATCHES "DragonFly")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L/usr/local/cxx_atomics")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -isystem /usr/local/cxx_atomics")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -isystem /usr/local/cxx_atomics")
endif()

set(CMAKE_STATIC_LIBRARY_PREFIX "")

if (PONY_CROSS_LIBPONYRT)
    add_subdirectory(src/libponyrt)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_subdirectory(src/crt)
    endif()
    install(TARGETS libponyrt)
else()
    add_subdirectory(src/libponyc)
    add_subdirectory(src/libponyrt)
    add_subdirectory(src/ponyc)

    add_subdirectory(test/libponyc)
    add_subdirectory(test/libponyrt)
    add_subdirectory(test/full-program-runner)
    add_subdirectory(test/full-program-tests)

    add_subdirectory(benchmark/libponyc)
    add_subdirectory(benchmark/libponyrt)

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_subdirectory(src/crt)
    endif()

    # Stamp file so tools rebuild when shared library sources change.
    # Adding new shared libraries under tools/lib/ only requires updating
    # the glob here rather than every tool's CMakeLists.txt.
    file(GLOB_RECURSE TOOLS_LIB_SOURCES CONFIGURE_DEPENDS "tools/lib/*.pony")
    set(TOOLS_LIB_STAMP ${CMAKE_BINARY_DIR}/tools_lib.stamp)
    add_custom_command(OUTPUT ${TOOLS_LIB_STAMP}
      COMMAND ${CMAKE_COMMAND} -E touch ${TOOLS_LIB_STAMP}
      DEPENDS ${TOOLS_LIB_SOURCES}
      COMMENT "Shared tool library sources changed"
    )
    add_custom_target(tools_lib_stamp DEPENDS ${TOOLS_LIB_STAMP})

    add_subdirectory(tools/pony-lsp)
    add_subdirectory(tools/pony-lint)
    add_subdirectory(tools/pony-doc)

    # The ponyc compiler binary. Test and benchmark binaries are deliberately
    # NOT installed — they are build artifacts, not part of a user install.
    install(TARGETS
        ponyc
        DESTINATION bin)

    # Where the runtime libraries are installed. The compiler locates its runtime
    # relative to the ponyc binary via add_exec_dir() in
    # src/libponyc/pkg/package.c, and that lookup differs by platform:
    #   * non-Windows: "../lib/<arch>", where <arch> is the compiled-in PONY_ARCH
    #     (or --link-arch when the user overrides it at compile time). The install
    #     destination must match PONY_ARCH so the default lookup resolves.
    #   * Windows: a flat "..\lib" with no arch component.
    # The destination is gated on platform to stay in sync with both branches.
    if(WIN32)
        set(PONY_LIB_DESTINATION lib)
    else()
        set(PONY_LIB_DESTINATION lib/${PONY_ARCH})
    endif()

    # Static libraries that are real CMake targets; CMake resolves their archive
    # paths regardless of where they are emitted.
    install(TARGETS
        libponyc
        libponyrt
        DESTINATION ${PONY_LIB_DESTINATION})

    # Static libs and crt objects produced by custom build steps into the output
    # directory. OPTIONAL because which ones exist is platform/config dependent:
    # libponyrt-pic.a (Linux), libponyc-standalone.a, the crt objects (Linux),
    # and libdtrace_probes.a (FreeBSD + DTrace).
    install(FILES
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libponyc-standalone.a
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libponyrt-pic.a
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libdtrace_probes.a
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/crtbeginS.o
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/crtendS.o
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/crtbeginT.o
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/crtend.o
        DESTINATION ${PONY_LIB_DESTINATION}
        OPTIONAL)

    # Public headers.
    install(FILES src/libponyrt/pony.h DESTINATION include)
    install(FILES src/common/pony/detail/atomics.h DESTINATION include/pony/detail)

    install(DIRECTORY packages/ DESTINATION packages)
    install(DIRECTORY examples/ DESTINATION examples PATTERN .gitignore EXCLUDE)
    # OPTIONAL mirrors the old Makefile's "if [ -f ... ]" guards: a build that
    # excludes a tool binary should skip it, not fail the whole install.
    install(PROGRAMS
        $<TARGET_FILE_DIR:ponyc>/pony-lsp${CMAKE_EXECUTABLE_SUFFIX}
        DESTINATION bin
        OPTIONAL)
    install(PROGRAMS
        $<TARGET_FILE_DIR:ponyc>/pony-lint${CMAKE_EXECUTABLE_SUFFIX}
        DESTINATION bin
        OPTIONAL)
    install(PROGRAMS
        $<TARGET_FILE_DIR:ponyc>/pony-doc${CMAKE_EXECUTABLE_SUFFIX}
        DESTINATION bin
        OPTIONAL)
endif()
