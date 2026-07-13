cmake_minimum_required(VERSION 3.25 FATAL_ERROR)

file(STRINGS "VERSION" PONYC_PROJECT_VERSION)
project(Pony VERSION ${PONYC_PROJECT_VERSION} LANGUAGES C CXX)

# Grab the PonyC version number from the "VERSION" source file. When building in
# a git checkout, append the short commit sha; from a release tarball (no .git,
# or git unavailable) use the bare version. The
# git-success guard matters: without it a tarball build produces a trailing dash
# ("0.66.0-"), which would put the install in lib/pony/0.66.0- instead of
# lib/pony/0.66.0.
if(NOT DEFINED PONYC_VERSION)
    set(PONYC_VERSION "${PONYC_PROJECT_VERSION}")
    if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
        execute_process(
            COMMAND git rev-parse --short HEAD
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE GIT_REVISION
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE GIT_REVISION_RESULT
        )
        if(GIT_REVISION_RESULT EQUAL 0 AND NOT GIT_REVISION STREQUAL "")
            set(PONYC_VERSION "${PONYC_PROJECT_VERSION}-${GIT_REVISION}")
        endif()
    endif()
endif()

# Uncomment this to show build commands
# set(CMAKE_VERBOSE_MAKEFILE ON)

# We require C++17 (because LLVM does)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# use= build-option validation. Parses and validates the PONY_USES string and
# sets the PONY_USE_* variables the blocks below consume.
#
# LOAD-BEARING PLACEMENT: keep this include between project() above and the
# find_package(LLVM ...) calls below.
#  - Below project(): the checks read CMAKE_HOST_SYSTEM_NAME (BSD rejections)
#    and MSVC (the Windows allowlist), which project() populates.
#  - Above find_package(LLVM): the BSD reject scripts run `cmake ...
#    -DPONY_USES=<unsupported>` on a no-libs checkout and grep for "not supported
#    on <OS>"; below find_package it would fail with "LLVM not found" instead.
# It also runs before the PONY_USE_* consumption blocks below.
# See cmake/PonyUses.cmake.
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/PonyUses.cmake)

# We require LLVM, Google Test and Google Benchmark
if(NOT PONY_CROSS_LIBPONYRT)
    find_package(LLVM REQUIRED CONFIG PATHS "build/libs/lib/cmake/llvm" "build/libs/lib64/cmake/llvm" NO_DEFAULT_PATH)
    find_package(LLD REQUIRED CONFIG PATHS "build/libs/lib/cmake/lld" "build/libs/lib64/cmake/lld" NO_DEFAULT_PATH)
    find_package(Clang REQUIRED CONFIG PATHS "build/libs/lib/cmake/clang" "build/libs/lib64/cmake/clang" NO_DEFAULT_PATH)
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

# Per-config output directory for ponyc-compiled binaries and the ctest wrappers.
# On the Windows Visual Studio multi-config generator CMAKE_RUNTIME_OUTPUT_DIRECTORY
# above is config-less (CMAKE_BUILD_TYPE is empty there), so the per-config dir must
# be derived from $<CONFIG> as a generator expression. On a single-config generator
# (the non-Windows presets) CMAKE_RUNTIME_OUTPUT_DIRECTORY already carries the
# config, so use it verbatim -- this keeps the non-Windows build byte-for-byte
# unchanged (no genex on that path). $<LOWER_CASE:$<CONFIG>> matches the lowercase
# debug/release dirs the _DEBUG/_RELEASE vars above use; ${PONY_OUTPUT_SUFFIX} keeps
# use= builds aligned. Everything that locates a ponyc-compiled binary, sets a ctest
# working dir, or tells ponyc where to write must use PONY_OUTPUT_DIR rather than
# CMAKE_RUNTIME_OUTPUT_DIRECTORY: on the single-config generators the two are equal,
# so a site that reverts to the latter passes every Unix job and breaks only Windows.
if(CMAKE_CONFIGURATION_TYPES)
    set(PONY_OUTPUT_DIR "${CMAKE_BINARY_DIR}/../$<LOWER_CASE:$<CONFIG>>${PONY_OUTPUT_SUFFIX}")
else()
    set(PONY_OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
endif()

# Libs are now always built in release mode.
set(PONY_LLVM_BUILD_MODE "1")

if(NOT DEFINED PONY_ARCH)
    set(PONY_ARCH "native")
endif()

# Driver link-fragment capture (used by sanitizer and coverage builds).
#
# Some build options instrument libponyrt's C code so that every executable
# ponyc links references a runtime library the compiler driver would normally
# pull in: the sanitizer runtime for -fsanitize=, or libgcov (__gcov_init etc.)
# for gcc's -fprofile-arcs. ponyc links natively via embedded LLD, which does
# NOT go through the driver, so it cannot rely on those flags to pull the
# runtime in. Instead we ask the driver, at configure time, for the exact link
# fragment it emits for the flag and embed it as a generated header the embedded
# linker splices in (genexe.cc).
#
# Asking the driver (rather than locating the runtime libraries ourselves)
# avoids re-implementing its per-compiler library selection and wrapping rules,
# and avoids depending on a compiler's runtime-directory layout: the driver
# emits whatever paths and flags are correct for itself. A fragment's library
# paths may be absolute on the build machine, which is fine: such a ponyc is a
# developer/CI artifact that runs where it was built. The splice is gated on
# !is_cross_compiling (see genexe.cc).
#
# The capture is the same algorithm for every flag (probe the driver with and
# without the flag via -###, then take the ordered set difference); only the
# flag, the sanity-check pattern, and the output names vary. It lives in one
# function so a fix to the (subtle) supersequence diff covers every caller.

# Extract the linker subcommand's arguments from a compiler "-###" stderr dump.
# The link subcommand is the line naming the probe object (clang emits a direct
# ld line, gcc a collect2 line; both name the object). Tokenize it respecting
# shell quoting via separate_arguments — clang double-quotes every token, gcc
# leaves most bare, and separate_arguments handles both. Drop volatile tokens:
# gcc's LTO plugin injects "-plugin-opt=-fresolution=<tmp>.res" with a fresh
# temp path on every invocation, which otherwise differs between the baseline
# and flagged probe runs and desyncs the ordered diff below.
function(_pony_driver_link_tokens raw marker outvar)
    string(REGEX MATCH "[^\n]*${marker}[^\n]*" _line "${raw}")
    separate_arguments(_toks UNIX_COMMAND "${_line}")
    set(_out "")
    foreach(_t IN LISTS _toks)
        if(NOT _t MATCHES "fresolution=" AND NOT _t MATCHES "\\.res$")
            list(APPEND _out "${_t}")
        endif()
    endforeach()
    set(${outvar} "${_out}" PARENT_SCOPE)
endfunction()

# Capture the link fragment the driver inserts for `flag` and write it as a
# generated C header. Args:
#   flag         - the compiler flag to probe (e.g. -fsanitize=address,undefined
#                  or -fprofile-arcs).
#   sanity_regex - a regex the captured fragment must match, else FATAL_ERROR
#                  (guards against the extraction silently producing nothing).
#   header_name  - output header filename, written under pony_generated/.
#   sym_prefix   - C symbol prefix: emits ${sym_prefix}_LINK_ARGS[] and
#                  ${sym_prefix}_LINK_ARGS_COUNT.
#   what         - short noun for diagnostics (e.g. "sanitizer", "coverage").
function(_pony_capture_driver_link_fragment flag sanity_regex header_name sym_prefix what)
    # Scratch files are named from `what` (a lowercase, regex/path-safe noun),
    # not `sym_prefix` (a C identifier), so the names stay clean and distinct
    # per caller (pony_sanitizer_probe.* vs pony_coverage_probe.*).
    set(_probe_c "${CMAKE_BINARY_DIR}/pony_${what}_probe.c")
    set(_probe_o "${CMAKE_BINARY_DIR}/pony_${what}_probe.o")
    set(_probe_bin "${CMAKE_BINARY_DIR}/pony_${what}_probe.bin")
    # The marker is matched as a regex in _pony_driver_link_tokens; escape the
    # '.' before the extension so it matches literally (as the old hardcoded
    # pattern did). `what` itself contains no regex metacharacters.
    get_filename_component(_marker "${_probe_o}" NAME)
    string(REPLACE "." "\\." _marker "${_marker}")
    file(WRITE "${_probe_c}" "int main(void){return 0;}\n")

    # -### prints the link command without running it, but only for an input
    # that exists, so compile a real (uninstrumented) object first. No -march or
    # other arch flag is passed to any probe: the runtime the driver selects
    # depends on the target (the driver's default target == this native host,
    # which is what we link for), not on -march, so an arch flag would only add
    # an unrelated way for the probe to fail.
    execute_process(
        COMMAND ${CMAKE_C_COMPILER} -c ${_probe_c} -o ${_probe_o}
        RESULT_VARIABLE _probe_rc
        OUTPUT_QUIET ERROR_QUIET)
    if(NOT _probe_rc EQUAL 0)
        message(FATAL_ERROR "${what} link capture: failed to compile probe object")
    endif()

    # Capture the driver's link line with and without the flag. -### writes the
    # subcommands to stderr. Check the exit codes: if the driver can't emit a
    # link line at all (e.g. an unrecognized flag value), fail here with the real
    # cause rather than letting it surface downstream as a confusing "not a
    # supersequence" error. (A mutually-exclusive combo like -fsanitize=
    # thread,address is accepted by -### but rejected later when libponyrt
    # compiles with those flags, so it fails loudly there - not caught here.)
    execute_process(
        COMMAND ${CMAKE_C_COMPILER} "-###" ${_probe_o} -o ${_probe_bin}
        RESULT_VARIABLE _baseline_rc
        ERROR_VARIABLE _baseline_raw OUTPUT_QUIET)
    execute_process(
        COMMAND ${CMAKE_C_COMPILER} ${flag} "-###" ${_probe_o} -o ${_probe_bin}
        RESULT_VARIABLE _flagged_rc
        ERROR_VARIABLE _flagged_raw OUTPUT_QUIET)
    if(NOT _baseline_rc EQUAL 0 OR NOT _flagged_rc EQUAL 0)
        message(FATAL_ERROR
            "${what} link capture: ${CMAKE_C_COMPILER} failed to emit a link "
            "line for ${flag} (-### exited non-zero)")
    endif()

    _pony_driver_link_tokens("${_baseline_raw}" "${_marker}" _base_toks)
    _pony_driver_link_tokens("${_flagged_raw}" "${_marker}" _flag_toks)

    # Ordered diff. The flagged link line is a supersequence of the baseline (the
    # flag only inserts arguments, never removes or reorders existing ones, once
    # the volatile LTO token is filtered out). A two-pointer scan over the
    # flagged tokens, advancing the baseline pointer on each match, collects
    # exactly the inserted arguments in driver order (e.g. for sanitizers clang's
    # whole-archive/clang_rt/--dynamic-list block, or gcc's libasan_preinit.o +
    # -lasan/-lubsan; for coverage gcc's -lgcov). Nothing is hardcoded. If the
    # baseline is not fully consumed in order the supersequence assumption is
    # violated (a new volatile token, or a driver that reorders existing args);
    # we stop with a clear error rather than emit a fragment we can't trust.
    list(LENGTH _base_toks _base_len)
    set(_j 0)
    set(_fragment "")
    foreach(_t IN LISTS _flag_toks)
        set(_matched FALSE)
        if(_j LESS _base_len)
            list(GET _base_toks ${_j} _bt)
            if(_t STREQUAL _bt)
                math(EXPR _j "${_j} + 1")
                set(_matched TRUE)
            endif()
        endif()
        if(NOT _matched)
            list(APPEND _fragment "${_t}")
        endif()
    endforeach()

    if(NOT _j EQUAL _base_len)
        message(FATAL_ERROR
            "${what} link capture: the ${flag} link line is not a "
            "supersequence of the baseline after filtering volatile tokens "
            "(a new non-deterministic token, or a driver that reorders existing "
            "arguments). See discussion ponylang/ponyc#5399.")
    endif()

    # Sanity: the captured fragment must reference the expected runtime, or the
    # extraction silently produced nothing useful (e.g. for sanitizers clang's
    # libclang_rt.* / gcc's -lasan; for coverage gcc's -lgcov).
    string(REGEX MATCH "${sanity_regex}" _has_rt "${_fragment}")
    if(_has_rt STREQUAL "")
        message(FATAL_ERROR
            "${what} link capture: no expected runtime (matching '${sanity_regex}') "
            "found in the captured fragment for ${flag}")
    endif()

    # Emit each token as a C string literal, escaping backslash then quote so a
    # toolchain path containing either can't break out of the literal. (Linux
    # toolchain paths don't normally contain these, but a malformed header is a
    # confusing failure mode worth closing off.)
    set(_array "")
    foreach(_a IN LISTS _fragment)
        string(REPLACE "\\" "\\\\" _a_esc "${_a}")
        string(REPLACE "\"" "\\\"" _a_esc "${_a_esc}")
        string(APPEND _array "  \"${_a_esc}\",\n")
    endforeach()

    set(_gen_dir "${CMAKE_BINARY_DIR}/pony_generated")
    file(MAKE_DIRECTORY "${_gen_dir}")
    file(WRITE "${_gen_dir}/${header_name}"
"// Generated by CMake (top-level CMakeLists.txt). Do not edit. The ${what}\n"
"// runtime link fragment captured from the compiler driver for ${flag}.\n"
"#pragma once\n"
"#include <stddef.h>\n"
"static const char* const ${sym_prefix}_LINK_ARGS[] = {\n${_array}};\n"
"static const size_t ${sym_prefix}_LINK_ARGS_COUNT =\n"
"  sizeof(${sym_prefix}_LINK_ARGS) / sizeof(${sym_prefix}_LINK_ARGS[0]);\n")
    message("-- Captured ${what} link fragment for ${flag}")

    # The probe scratch files have served their purpose at configure time.
    file(REMOVE "${_probe_c}" "${_probe_o}" "${_probe_bin}")
endfunction()

# Sanitizer build: ponyc links references the sanitizer runtime (__asan_* etc.).
# The driver fragment differs per compiler — clang emits whole-archived clang_rt
# static archives + --dynamic-list, gcc a libasan_preinit.o + shared
# -lasan/-lubsan, Apple clang the clang_rt dylib by absolute path + -rpath (and
# folds UBSan into ASan). Native Linux, FreeBSD, and macOS sanitizer builds
# consume this (see genexe.cc); DragonFly/OpenBSD sanitizer builds are rejected
# at configure time. The matching include dir is wired in below via
# PONY_GENERATED_INCLUDE_NEEDED, and the splice lives in genexe.cc under
# #if defined(PONY_SANITIZER).
if(PONY_SANITIZERS_ENABLED)
    _pony_capture_driver_link_fragment(
        "-fsanitize=${PONY_SANITIZERS_VALUE}"
        "asan|ubsan|tsan"
        "sanitizer_link_args.h"
        "PONY_SANITIZER"
        "sanitizer")
    set(PONY_GENERATED_INCLUDE_NEEDED TRUE)
endif()

# Coverage build with gcc: -fprofile-arcs instruments libponyrt's C objects with
# gcov constructors that reference __gcov_init/__gcov_exit from gcc's libgcov, so
# every Pony program ponyc links needs libgcov (issue #5434). Capture the
# driver's coverage fragment (just -lgcov) and splice it in genexe.cc under
# #if defined(PONY_COVERAGE). Only real gcc needs this: -lgcov is gcc's coverage
# runtime specifically. clang's -fprofile-instr-generate (the PONY_USE_COVERAGE
# clang branch above) links fine without a splice, and a non-GNU compiler that
# takes the -fprofile-arcs instrumentation branch instead — e.g. Apple clang on
# macOS, which CMake reports as "AppleClang" — emits its own profile runtime
# (clang_rt.profile), NOT libgcov, so -lgcov would be wrong for it. Gating on
# CMAKE_C_COMPILER_ID == GNU restricts the capture to gcc, where libgcov is the
# correct runtime (the instrumented code needing it is libponyrt's C). Since
# PONY_COVERAGE is defined only here, no other compiler or platform pulls in the
# spliced fragment.
if(PONY_USE_COVERAGE AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
    _pony_capture_driver_link_fragment(
        "-fprofile-arcs"
        "gcov"
        "coverage_link_args.h"
        "PONY_COVERAGE"
        "coverage")
    # -DPONY_COVERAGE gates the genexe.cc include + splice; it is a compile-time
    # preprocessor define with no link-time meaning, so it goes on compile
    # options only (unlike the sanitizer block's -fsanitize=, which the linker
    # genuinely needs).
    add_compile_options(-DPONY_COVERAGE)
    set(PONY_GENERATED_INCLUDE_NEEDED TRUE)
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

    # Imported targets exported by ClangConfig.cmake (found above). Each target
    # carries its transitive clang/LLVM dependencies, so this list only needs
    # the components gencshim uses directly: the frontend driving an in-process
    # compile (CompilerInstance/CompilerInvocation) and object emission
    # (EmitObjAction), plus the layers they sit on.
    set(PONYC_CLANG_LIBS
        clangFrontend
        clangCodeGen
        clangDriver
        clangParse
        clangSema
        clangAnalysis
        clangAST
        clangLex
        clangBasic
        clangSerialization
        clangEdit
        clangSupport
    )

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
    # /MP lets cl.exe compile the many source files within a single project (e.g.
    # libponyc) concurrently; without it MSBuild's project-level --parallel only
    # parallelizes across projects, leaving big single-target compiles serial.
    add_compile_options(
        /wd4142
        /wd4996
        $<$<CONFIG:Debug>:/MD>
        /WX
        /EHa
        /MP
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

# Install layout. Everything ponyc needs lands in a versioned directory so
# several ponyc versions can coexist under one prefix. Runtime and compiler libs
# go where package.c looks for them relative to the ponyc binary (package.c
# ~752-772): ../lib/<arch> on Unix, flat ../lib on Windows. The Unix arch subdir
# is what carries the configure-time arch into `cmake --install` -- the fix for
# #3898 -- since one PONY_ARCH cache variable drives both the compile define
# package.c reads and this destination, so the two can't disagree. Stable entry
# points in <prefix>/bin (and <prefix>/lib) point at the active version: symlinks
# on Unix, a shim on Windows (added after the shim is verified). Nothing tests
# this -- a mismatch surfaces as an installed ponyc that can't find libponyrt.a.
set(PONY_VERSIONED_DIR "lib/pony/${PONYC_VERSION}")
if(WIN32)
    set(PONY_INSTALL_LIBDIR "${PONY_VERSIONED_DIR}/lib")
else()
    set(PONY_INSTALL_LIBDIR "${PONY_VERSIONED_DIR}/lib/${PONY_ARCH}")
endif()
option(PONY_INSTALL_SYMLINKS
    "Create <prefix>/bin and <prefix>/lib links to the versioned install" ON)

if (PONY_CROSS_LIBPONYRT)
    add_subdirectory(src/libponyrt)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_subdirectory(src/crt)
    endif()
    # Cross-built runtime for another arch, installed into the same versioned
    # tree so a host ponyc can link programs for that arch. Must match the arch
    # subdir the main install and package.c use. Not locally verifiable without a
    # cross toolchain -- exercised by the cross-compile CI (see #5588 cross work).
    install(TARGETS libponyrt DESTINATION ${PONY_INSTALL_LIBDIR})
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

    # The Windows stable-entry-point shim (Unix uses symlinks instead).
    if(WIN32)
        add_subdirectory(src/ponyc-shim)
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

    # Helper for compiling Pony binaries with the just-built ponyc (the tools and
    # the pony-*-tests). See cmake/PonyBinary.cmake.
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/PonyBinary.cmake)

    add_subdirectory(tools/pony-lsp)
    add_subdirectory(tools/pony-lint)
    add_subdirectory(tools/pony-doc)

    # Test binaries compiled with the just-built ponyc. Built on demand, NOT under
    # ALL: each runs ponyc to compile builtin and the stdlib packages, so building
    # them is expensive, and a plain build -- including the sanitizer, coverage, and
    # dtrace smokes that build everything but never run these -- must not pay for
    # them. Building four self-hosted tools under a sanitizer on a memory-constrained
    # CI VM OOMs. The tool-tests aggregate target below builds all four; CI and
    # developers build it before `ctest -L tools`.
    add_pony_binary(pony-compiler-tests
        NAME pony-compiler-tests
        SOURCE ${CMAKE_SOURCE_DIR}/tools/lib/ponylang/pony_compiler/tests
        PATHS ${CMAKE_SOURCE_DIR}/tools/lib/ponylang/pony_compiler
        WATCH ${CMAKE_SOURCE_DIR}/tools/lib/ponylang/pony_compiler/tests)

    add_pony_binary(pony-doc-tests
        NAME pony-doc-tests
        SOURCE ${CMAKE_SOURCE_DIR}/tools/pony-doc/test
        PATHS ${CMAKE_SOURCE_DIR}/tools/lib/ponylang/pony_compiler
        WATCH ${CMAKE_SOURCE_DIR}/tools/pony-doc)

    add_pony_binary(pony-lint-tests
        NAME pony-lint-tests
        SOURCE ${CMAKE_SOURCE_DIR}/tools/pony-lint/test
        PATHS ${CMAKE_SOURCE_DIR}/tools/lib/ponylang/pony_compiler
        WATCH ${CMAKE_SOURCE_DIR}/tools/pony-lint)

    # pony-lsp-tests compiles tools/ (tools/_test.pony pulls in pony-lsp/test via
    # `use`), not tools/pony-lsp/test.
    add_pony_binary(pony-lsp-tests
        NAME pony-lsp-tests
        SOURCE ${CMAKE_SOURCE_DIR}/tools
        PATHS ${CMAKE_SOURCE_DIR}/tools/lib/ponylang/pony_compiler
        WATCH ${CMAKE_SOURCE_DIR}/tools/pony-lsp)

    # On-demand aggregate: build every self-hosted tool test in one target. Run
    # `cmake --build <dir> --target tool-tests` before `ctest -L tools`.
    add_custom_target(tool-tests)
    add_dependencies(tool-tests
        pony-compiler-tests pony-doc-tests pony-lint-tests pony-lsp-tests)

    # ctest registration. Every test binary builds under ALL, so ctest only runs
    # them. Labels group the suites: `ci-core` is the core CI suite (the old
    # test-ci-core), `tools` the tool test suites. Run e.g.
    # `ctest --test-dir <builddir> -L ci-core`.
    enable_testing()

    add_test(NAME check-version COMMAND $<TARGET_FILE:ponyc> --version)
    add_test(NAME libponyrt.tests
        COMMAND ${CMAKE_COMMAND} -DBIN=$<TARGET_FILE:libponyrt.tests>
            -DARGS=--gtest_shuffle -DGTEST=ON
            -DWORKDIR=${PONY_OUTPUT_DIR}
            -P ${CMAKE_SOURCE_DIR}/cmake/RunTest.cmake)
    add_test(NAME libponyc.tests
        COMMAND ${CMAKE_COMMAND} -DBIN=$<TARGET_FILE:libponyc.tests>
            -DARGS=--gtest_shuffle -DGTEST=ON
            -DWORKDIR=${PONY_OUTPUT_DIR}
            -P ${CMAKE_SOURCE_DIR}/cmake/RunTest.cmake)
    set_tests_properties(check-version libponyrt.tests libponyc.tests
        PROPERTIES LABELS ci-core)

    add_test(NAME pony-doc-tests
        COMMAND ${PONY_OUTPUT_DIR}/pony-doc-tests${CMAKE_EXECUTABLE_SUFFIX} --sequential
        WORKING_DIRECTORY ${PONY_OUTPUT_DIR})
    add_test(NAME pony-lsp-tests
        COMMAND ${PONY_OUTPUT_DIR}/pony-lsp-tests${CMAKE_EXECUTABLE_SUFFIX} --sequential
        WORKING_DIRECTORY ${PONY_OUTPUT_DIR})
    add_test(NAME pony-lint-tests
        COMMAND ${PONY_OUTPUT_DIR}/pony-lint-tests${CMAKE_EXECUTABLE_SUFFIX} --sequential
        WORKING_DIRECTORY ${PONY_OUTPUT_DIR})
    set_tests_properties(pony-lint-tests PROPERTIES
        ENVIRONMENT "PONYPATH=${CMAKE_SOURCE_DIR}/packages")
    add_test(NAME pony-compiler-tests
        COMMAND ${PONY_OUTPUT_DIR}/pony-compiler-tests${CMAKE_EXECUTABLE_SUFFIX} --sequential
        WORKING_DIRECTORY ${PONY_OUTPUT_DIR})
    # PONYPATH separates its entries with ':' on Unix but ';' on Windows. The ';'
    # is backslash-escaped so CMake keeps it inside the ENVIRONMENT value instead of
    # splitting it as a list separator. (pony-lint-tests above passes a single path,
    # so it needs no separator and no platform branch.)
    if(WIN32)
        set_tests_properties(pony-compiler-tests PROPERTIES
            ENVIRONMENT "PONYPATH=${CMAKE_SOURCE_DIR}/tools/lib/ponylang/pony_compiler\;${CMAKE_SOURCE_DIR}/packages")
    else()
        set_tests_properties(pony-compiler-tests PROPERTIES
            ENVIRONMENT "PONYPATH=${CMAKE_SOURCE_DIR}/tools/lib/ponylang/pony_compiler:${CMAKE_SOURCE_DIR}/packages")
    endif()
    set_tests_properties(pony-doc-tests pony-lsp-tests pony-lint-tests pony-compiler-tests
        PROPERTIES LABELS tools)

    # Compile-then-run ci-core tests. Each compiles Pony sources with the freshly
    # built ponyc and then runs (or diffs) the result, so they depend on ponyc
    # (and, for the full-program tests, the tests.runner target and the test_lib
    # fixtures) already being built under ALL. They go through cmake -P wrappers,
    # which also read the run-time test knobs from the environment -- the debugger
    # to run under (PONY_TEST_DEBUGGER), the full-program timeout
    # (PONY_FULL_PROGRAM_TIMEOUT), the stdlib excludes (PONY_STDLIB_TEST_EXCLUDES).
    # Those are CI/environment concerns, set at run time with defaults in the
    # wrappers, not baked in at configure.

    set(_full_program_runner
        ${CMAKE_BINARY_DIR}/test/full-program-runner/full-program-runner${CMAKE_EXECUTABLE_SUFFIX})
    # The per-config full-program-tests output dirs can't be created here on the
    # multi-config generator (the path is config-dependent); RunFullPrograms.cmake
    # creates its --output dir at test time instead.

    add_test(NAME full-programs-debug
        COMMAND ${CMAKE_COMMAND}
            -DRUNNER=${_full_program_runner}
            -DCOMPILER=$<TARGET_FILE:ponyc>
            -DOUTPUT=${PONY_OUTPUT_DIR}/full-program-tests/debug
            -DTEST_LIB=${PONY_OUTPUT_DIR}/test_lib
            -DTESTS=${CMAKE_SOURCE_DIR}/test/full-program-tests
            -DWORKDIR=${PONY_OUTPUT_DIR} -DDEBUG=ON
            -P ${CMAKE_SOURCE_DIR}/cmake/RunFullPrograms.cmake)
    add_test(NAME full-programs-release
        COMMAND ${CMAKE_COMMAND}
            -DRUNNER=${_full_program_runner}
            -DCOMPILER=$<TARGET_FILE:ponyc>
            -DOUTPUT=${PONY_OUTPUT_DIR}/full-program-tests/release
            -DTEST_LIB=${PONY_OUTPUT_DIR}/test_lib
            -DTESTS=${CMAKE_SOURCE_DIR}/test/full-program-tests
            -DWORKDIR=${PONY_OUTPUT_DIR} -DDEBUG=OFF
            -P ${CMAKE_SOURCE_DIR}/cmake/RunFullPrograms.cmake)

    add_test(NAME full-program-runner-rejects-broken-config
        COMMAND ${CMAKE_COMMAND}
            -DRUNNER=${_full_program_runner}
            -DWORKDIR=${PONY_OUTPUT_DIR}
            -P ${CMAKE_SOURCE_DIR}/cmake/RunBrokenConfig.cmake)

    add_test(NAME stdlib-debug
        COMMAND ${CMAKE_COMMAND}
            -DPONYC=$<TARGET_FILE:ponyc>
            -DSTDLIB_SRC=${CMAKE_SOURCE_DIR}/packages/stdlib
            -DWORKDIR=${PONY_OUTPUT_DIR}
            -DBUILD_NAME=stdlib-debug -DDEBUG=ON
            -P ${CMAKE_SOURCE_DIR}/cmake/RunStdlibTest.cmake)
    add_test(NAME stdlib-release
        COMMAND ${CMAKE_COMMAND}
            -DPONYC=$<TARGET_FILE:ponyc>
            -DSTDLIB_SRC=${CMAKE_SOURCE_DIR}/packages/stdlib
            -DWORKDIR=${PONY_OUTPUT_DIR}
            -DBUILD_NAME=stdlib-release -DDEBUG=OFF
            -P ${CMAKE_SOURCE_DIR}/cmake/RunStdlibTest.cmake)

    add_test(NAME examples
        COMMAND ${CMAKE_COMMAND}
            -DPONYC=$<TARGET_FILE:ponyc>
            -DEXAMPLES=${CMAKE_SOURCE_DIR}/examples
            -DWORKDIR=${PONY_OUTPUT_DIR}
            -P ${CMAKE_SOURCE_DIR}/cmake/RunExamples.cmake)

    add_test(NAME validate-grammar
        COMMAND ${CMAKE_COMMAND}
            -DPONYC=$<TARGET_FILE:ponyc>
            -DGRAMMAR=${CMAKE_SOURCE_DIR}/pony.g
            -DWORKDIR=${PONY_OUTPUT_DIR}
            -P ${CMAKE_SOURCE_DIR}/cmake/CheckGrammar.cmake)

    set_tests_properties(full-programs-debug full-programs-release
        full-program-runner-rejects-broken-config
        stdlib-debug stdlib-release examples validate-grammar
        PROPERTIES LABELS ci-core)

    # Binaries. ponyc is the only real executable target; the tools are custom
    # targets producing a file next to ponyc, so they install by path as
    # PROGRAMS. Test and benchmark binaries are not installed.
    install(TARGETS ponyc DESTINATION ${PONY_VERSIONED_DIR}/bin)
    install(PROGRAMS
        $<TARGET_FILE_DIR:ponyc>/pony-lsp${CMAKE_EXECUTABLE_SUFFIX}
        $<TARGET_FILE_DIR:ponyc>/pony-lint${CMAKE_EXECUTABLE_SUFFIX}
        $<TARGET_FILE_DIR:ponyc>/pony-doc${CMAKE_EXECUTABLE_SUFFIX}
        DESTINATION ${PONY_VERSIONED_DIR}/bin)

    # Runtime + compiler static libs, into the lib dir ponyc searches at run time
    # (PONY_INSTALL_LIBDIR: ../lib/<arch> on Unix, flat on Windows -- see above).
    # libponyrt and libponyc are real targets. libponyc-standalone.a (an ar-built
    # archive wrapped in a custom target), libponyrt-pic.a (Linux), the crt
    # objects (Linux), and libdtrace_probes.a (FreeBSD + use=dtrace) are
    # custom-command outputs, installed by path and OPTIONAL so a platform that
    # doesn't produce one doesn't fail the install.
    install(TARGETS libponyrt libponyc DESTINATION ${PONY_INSTALL_LIBDIR})
    install(FILES
        $<TARGET_FILE_DIR:ponyc>/libponyc-standalone.a
        $<TARGET_FILE_DIR:ponyc>/libponyrt-pic.a
        $<TARGET_FILE_DIR:ponyc>/libdtrace_probes.a
        $<TARGET_FILE_DIR:ponyc>/crtbeginS.o
        $<TARGET_FILE_DIR:ponyc>/crtendS.o
        $<TARGET_FILE_DIR:ponyc>/crtbeginT.o
        $<TARGET_FILE_DIR:ponyc>/crtend.o
        DESTINATION ${PONY_INSTALL_LIBDIR} OPTIONAL)

    # The stdlib packages.
    install(DIRECTORY packages/ DESTINATION ${PONY_VERSIONED_DIR}/packages)

    # Clang's resource directory (builtin headers such as stddef.h), built by the
    # libs step alongside LLVM/LLD. ponyc's in-process clang needs it at a known
    # offset from the binary (lib/clang/<ver>/include, like the bundled packages/
    # dir). LLVM installs it under lib/ or lib64/ depending on the distro; install
    # whichever exists. OPTIONAL so an LLD-only libs build (e.g. an older cached
    # build/libs) doesn't fail.
    install(DIRECTORY build/libs/lib/clang
        DESTINATION ${PONY_VERSIONED_DIR}/lib OPTIONAL)
    install(DIRECTORY build/libs/lib64/clang
        DESTINATION ${PONY_VERSIONED_DIR}/lib OPTIONAL)

    # ponyc's own runtime headers, so a C shim can #include <pony.h> and
    # #include <ponyassert.h>. gencshim puts <versioned>/include/pony (plus
    # <versioned>/include for the transitive <pony/detail/atomics.h>) on the shim
    # include path (add_pony_include_args). Layout must match what gencshim
    # expects: the headers a shim names directly at include/pony/, atomics at
    # include/pony/detail/. Kept under pony/ so a shared system include never gets
    # a flat collision-prone name. The gtest suite only exercises the build-tree
    # offsets, so a packaging change here surfaces only against an installed tree.
    install(FILES src/libponyrt/pony.h
        DESTINATION ${PONY_VERSIONED_DIR}/include/pony)
    install(FILES src/common/pony/detail/atomics.h
        DESTINATION ${PONY_VERSIONED_DIR}/include/pony/detail)
    # pony.h and pony_assert (ponyassert.h) are supported for shims;
    # platform.h/threads.h/paths.h are ponyassert.h's internal include closure,
    # not a committed interface. vcvars.h is omitted on Unix; on Windows
    # platform.h includes it, so it's in the closure there -- gate it on MSVC
    # rather than dropping it outright.
    install(FILES
        src/common/ponyassert.h
        src/common/platform.h
        src/common/threads.h
        src/common/paths.h
        DESTINATION ${PONY_VERSIONED_DIR}/include/pony)
    if(MSVC)
        install(FILES src/common/vcvars.h
            DESTINATION ${PONY_VERSIONED_DIR}/include/pony)
    endif()

    # Stable entry points in <prefix>/bin (and <prefix>/lib) pointing at the
    # active version. On Unix these are symlinks; ponyc resolves its real path
    # (/proc/self/exe, realpath), so running the link lands on the versioned dir.
    # Windows can't (GetModuleFileName returns the link's own path), so each entry
    # point is a copy of the shim (src/ponyc-shim) launched under the tool's name,
    # plus a pony-active-version pointer file the shim reads to find the versioned
    # binary. Skipped when PONY_INSTALL_SYMLINKS is OFF, which is what packaging
    # sets (it stages only the versioned tree). The Unix branch runs last (its
    # EXISTS checks need the versioned files already installed). DESTDIR staging
    # isn't supported, but packaging turns symlinks off, so it never applies.
    if(PONY_INSTALL_SYMLINKS AND NOT WIN32)
        install(CODE "
            set(_prefix \"\${CMAKE_INSTALL_PREFIX}\")
            # Remove stale symlinks an older (pre-versioned) install left under
            # <prefix> pointing into <prefix>/lib/pony/, so an in-place upgrade
            # doesn't leave dangling links behind. The old Makefile install ran
            # this cleanup (#5588).
            foreach(_legacy
                    include/pony.h include/ponyassert.h include/platform.h
                    include/threads.h include/paths.h
                    include/pony/detail/atomics.h lib/libponyrt.a
                    lib/libponyrt-pic.a lib/libdtrace_probes.a)
                set(_ll \"\${_prefix}/\${_legacy}\")
                if(IS_SYMLINK \"\${_ll}\")
                    file(READ_SYMLINK \"\${_ll}\" _lt)
                    string(FIND \"\${_lt}\" \"\${_prefix}/lib/pony/\" _pos)
                    if(_pos EQUAL 0)
                        file(REMOVE \"\${_ll}\")
                    endif()
                endif()
            endforeach()
            file(MAKE_DIRECTORY \"\${_prefix}/bin\" \"\${_prefix}/lib\")
            foreach(_tool ponyc pony-lsp pony-lint pony-doc)
                set(_t \"\${_prefix}/${PONY_VERSIONED_DIR}/bin/\${_tool}\")
                if(EXISTS \"\${_t}\")
                    file(CREATE_LINK \"\${_t}\" \"\${_prefix}/bin/\${_tool}\" SYMBOLIC)
                endif()
            endforeach()
            foreach(_lib libponyc.a libponyc-standalone.a)
                set(_l \"\${_prefix}/${PONY_INSTALL_LIBDIR}/\${_lib}\")
                if(EXISTS \"\${_l}\")
                    file(CREATE_LINK \"\${_l}\" \"\${_prefix}/lib/\${_lib}\" SYMBOLIC)
                endif()
            endforeach()
        ")
    elseif(PONY_INSTALL_SYMLINKS AND WIN32)
        foreach(_tool ponyc pony-lsp pony-lint pony-doc)
            install(PROGRAMS $<TARGET_FILE:ponyc-shim>
                DESTINATION bin RENAME ${_tool}.exe)
        endforeach()
        install(CODE
            "file(WRITE \"\${CMAKE_INSTALL_PREFIX}/bin/pony-active-version\" \"${PONYC_VERSION}\")")
    endif()
endif()
