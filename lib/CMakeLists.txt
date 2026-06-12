cmake_minimum_required(VERSION 3.21 FATAL_ERROR)

file(STRINGS "../VERSION" PONYC_PROJECT_VERSION)
project(ponyclibs VERSION ${PONYC_PROJECT_VERSION} LANGUAGES C CXX)

include(ExternalProject)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

if(NOT DEFINED PONYC_LIBS_BUILD_TYPE)
    set(PONYC_LIBS_BUILD_TYPE Release)
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

# Libraries

set(PONYC_GBENCHMARK_URL https://github.com/google/benchmark/archive/v1.9.5.tar.gz)
ExternalProject_Add(gbenchmark
    URL ${PONYC_GBENCHMARK_URL}
    CMAKE_ARGS -DCMAKE_BUILD_TYPE=${PONYC_LIBS_BUILD_TYPE} -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX} -DBENCHMARK_ENABLE_GTEST_TESTS=OFF -DBENCHMARK_ENABLE_WERROR=OFF -DCMAKE_CXX_FLAGS=${PONY_PIC_FLAG} --no-warn-unused-cli
)

set(PONYC_GOOGLETEST_URL https://github.com/google/googletest/releases/download/v1.17.0/googletest-1.17.0.tar.gz)

ExternalProject_Add(googletest
    URL ${PONYC_GOOGLETEST_URL}
    CMAKE_ARGS -DCMAKE_BUILD_TYPE=${PONYC_LIBS_BUILD_TYPE} -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX} -DCMAKE_CXX_FLAGS=${PONY_PIC_FLAG} -Dgtest_force_shared_crt=ON --no-warn-unused-cli
)

find_package(Git)

set(LLVM_DESIRED_HASH "fc4aad7b5db3fff421df9a9637605b9ca5667881")
set(PATCHES_DESIRED_HASH "13b39b2de75d035854176945a7877997ad3e37ec8f90eb28ca743713f06f78a3")

if(GIT_FOUND)
    if(EXISTS "${PROJECT_SOURCE_DIR}/../.git")
        # Update submodules as needed
        option(GIT_SUBMODULE "Check submodules during build" ON)
        if(GIT_SUBMODULE)
            message(STATUS "Updating submodules...")
            execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive --depth 1
                            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                            RESULT_VARIABLE git_submod_result)
            #message("git_submod_result ${git_submod_result}")
            if(NOT git_submod_result EQUAL "0")
                message(FATAL_ERROR "git submodule update --init --recursive --depth 1 failed with ${git_submod_result}, please checkout submodules")
            endif()

            # we check to make sure the submodule hash matches
            # the reason the submodule hash is in this file is to be able to use this file as a key for caching the libs in CI
            execute_process(COMMAND ${GIT_EXECUTABLE} submodule status
                            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                            OUTPUT_VARIABLE git_submod_output)
            #message("git_submod_output ${git_submod_output}")
            string(FIND "${git_submod_output}" "${LLVM_DESIRED_HASH}" LLVM_SUBMOD_POS)
            if(LLVM_SUBMOD_POS EQUAL "-1")
                message(FATAL_ERROR "Expecting the lib/llvm/src submodule to be at hash '${LLVM_DESIRED_HASH}'; found '${git_submod_output}'; update the LLVM_DESIRED_HASH variable in lib/CMakeLists.txt if you've updated the submodule.")
            endif()
        endif()
    endif()

    # Apply patches
    message("Applying patches...")
    file(GLOB PONY_LLVM_PATCHES "${PROJECT_SOURCE_DIR}/llvm/patches/*.diff")

    # check to see if the patch hashes match
    message("Checking patches ${PONY_LLVM_PATCHES}")
    set(PATCHES_ACTUAL_HASH "needed_if_no_patches")
    foreach (PATCH ${PONY_LLVM_PATCHES})
        file(STRINGS ${PATCH} patch_file NEWLINE_CONSUME)
        string(REPLACE "\n" " " patch_file "${patch_file}")
        string(SHA256 patch_file_hash "${patch_file}")
        string(CONCAT PATCHES_ACTUAL_HASH ${PATCHES_ACTUAL_HASH} ${patch_file_hash})
    endforeach()
    string(SHA256 PATCHES_ACTUAL_HASH ${PATCHES_ACTUAL_HASH})
    if(NOT PATCHES_ACTUAL_HASH MATCHES "${PATCHES_DESIRED_HASH}")
        message(FATAL_ERROR "Patch hash actual '${PATCHES_ACTUAL_HASH}' does not match desired '${PATCHES_DESIRED_HASH}'")
    endif()

    foreach (PATCH ${PONY_LLVM_PATCHES})
        message("  Checking ${PATCH}")
        execute_process(COMMAND ${GIT_EXECUTABLE} apply --check -p 1 --ignore-whitespace --whitespace=nowarn ${PATCH}
                        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/llvm/src"
                        ERROR_VARIABLE _err_out
                        RESULT_VARIABLE git_apply_check_result)
        if(git_apply_check_result EQUAL "0")
            message("    Applying ${PATCH}")
            execute_process(COMMAND ${GIT_EXECUTABLE} apply -p 1 --ignore-whitespace --whitespace=nowarn ${PATCH}
                            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/llvm/src"
                            RESULT_VARIABLE git_apply_result)
            if(NOT git_apply_result EQUAL "0")
                message(FATAL_ERROR "Unable to apply ${PATCH}")
            endif()
        else()
            # The forward check failed. That means either the patch is
            # already applied, or `git apply` could not evaluate it at all.
            # The latter happens when lib/llvm/src has a dangling submodule
            # gitlink: a `.git` file pointing at a `.git/modules/...` target
            # that isn't present (e.g. the tree was copied or rsynced without
            # the parent's `.git/modules`). In that case `git apply` aborts
            # with "fatal: not a git repository" and a non-zero exit. Note
            # this loop runs unconditionally inside `if(GIT_FOUND)`, even when
            # the `EXISTS ../.git` submodule-update guard above is false, so
            # the dangling-gitlink tree reaches this point unchecked.
            #
            # Distinguish the two with a reverse check: if the patch
            # reverse-applies cleanly it is genuinely in the tree and we skip
            # it; otherwise fail loudly rather than silently build an
            # unpatched source tree.
            execute_process(COMMAND ${GIT_EXECUTABLE} apply --reverse --check -p 1 --ignore-whitespace --whitespace=nowarn ${PATCH}
                            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/llvm/src"
                            ERROR_VARIABLE _reverse_err_out
                            RESULT_VARIABLE git_apply_reverse_check_result)
            if(git_apply_reverse_check_result EQUAL "0")
                message("    Already applied ${PATCH}")
            else()
                message(FATAL_ERROR "Unable to apply or verify ${PATCH}.\n"
                        "The patch does not apply and is not already applied; the source tree may be missing its git metadata (e.g. a dangling submodule gitlink).\n"
                        "Forward check error:\n${_err_out}\n"
                        "Reverse check error:\n${_reverse_err_out}")
            endif()
        endif()
    endforeach()
else()
    message(FATAL_ERROR "Git not found!")
endif()

message("Building targets: ${LLVM_TARGETS_TO_BUILD}")

set(LLVM_ENABLE_BINDINGS OFF CACHE BOOL "ponyc specific override of LLVM cache entry")
set(LLVM_ENABLE_OCAMLDOC OFF CACHE BOOL "ponyc specific override of LLVM cache entry")
# Historically disabled on macOS to fix an LLVM build failure there. On
# other platforms keep LLVM's default (ON) so PIE-default distributions
# can link the resulting static libs into pony-lsp and pony-lint.
if(${CMAKE_HOST_SYSTEM_NAME} MATCHES "Darwin")
    set(LLVM_ENABLE_PIC OFF CACHE BOOL "ponyc specific override of LLVM cache entry")
endif()
# We statically link LLVM and don't use its plugin infrastructure. Disable
# explicitly so it doesn't track LLVM_ENABLE_PIC: on OpenBSD that pulls in
# the Bye example, whose include of llvm/CodeGen/GenVT.inc resolves to the
# base-system header in /usr/include/llvm before tablegen has generated the
# in-tree one, producing a macro-signature mismatch.
set(LLVM_ENABLE_PLUGINS OFF CACHE BOOL "ponyc specific override of LLVM cache entry")
set(LLVM_ENABLE_TERMINFO OFF CACHE BOOL "ponyc specific override of LLVM cache entry")
set(LLVM_ENABLE_WARNINGS OFF CACHE BOOL "ponyc specific override of LLVM cache entry")
set(LLVM_ENABLE_Z3_SOLVER OFF CACHE BOOL "ponyc specific override of LLVM cache entry")
# The LLVM command-line tools (llc, opt, llvm-link, the llvm-* utilities, and the
# standalone lld/clang driver binaries) are built by default. ponyc links LLVM
# and LLD as static *libraries* and never invokes these binaries, so for a normal
# build they are pure dead weight -- the bulk of build/libs/bin (1.6 GB today)
# and a large share of link time. Pass llvm_tools=false (PONY_LLVM_TOOLS=OFF) to
# omit them; CI uses this to keep the cached libs small. LLVM_BUILD_TOOLS gates
# only the tool *executables* (AddLLVM.cmake: llvm_add_tool marks them
# EXCLUDE_FROM_ALL and skips their install rules), so the LLVM and LLD libraries
# ponyc links are unaffected. The one consumer of a tool binary is the opt-in
# runtime-bitcode build, which drives llvm-link, so it needs libs built with the
# tools; with PONY_LLVM_TOOLS=OFF that binary is absent and the bitcode build
# fails to link (rare enough that we document it rather than guard it).
if(NOT DEFINED PONY_LLVM_TOOLS)
    set(PONY_LLVM_TOOLS ON)
endif()
if(NOT PONY_LLVM_TOOLS)
    set(LLVM_BUILD_TOOLS OFF CACHE BOOL "ponyc specific override of LLVM cache entry")
endif()
set(LLVM_INCLUDE_BENCHMARKS OFF CACHE BOOL "ponyc specific override of LLVM cache entry")
set(LLVM_INCLUDE_TESTS OFF CACHE BOOL "ponyc specific override of LLVM cache entry")
set(LLVM_TOOL_REMARKS_SHLIB_BUILD OFF CACHE BOOL "ponyc specific override of LLVM cache entry")
if(NOT MSVC)
  set(LLVM_ENABLE_ZLIB FORCE_ON CACHE STRING "ponyc specific override of LLVM cache entry")
else()
  set(LLVM_ENABLE_ZLIB OFF CACHE BOOL "ponyc specific override of LLVM cache entry")
endif()
set(LLVM_ENABLE_ZSTD OFF CACHE BOOL "ponyc specific override of LLVM cache entry")
set(LLVM_ENABLE_LIBXML2 OFF CACHE BOOL "ponyc specific override of LLVM cache entry")

if(${CMAKE_HOST_SYSTEM_NAME} MATCHES "Darwin")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mmacosx-version-min=10.14 -DUSE_SCHEDULER_SCALING_PTHREADS")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -mmacosx-version-min=10.14")
endif()

set(LLVM_ENABLE_PROJECTS "lld;clang" CACHE STRING "ponyc: build LLD and clang as LLVM sub-projects")

add_subdirectory(llvm/src/llvm)
