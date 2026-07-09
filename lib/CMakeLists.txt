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

# ExternalProject_Add does not forward this project's compiler to the gbenchmark
# and googletest sub-builds; only the generator carries over. So on a
# single-config generator (Make/Ninja) the sub-builds pick their compiler from
# the CC/CXX environment and ignore a CMAKE_C_COMPILER/CMAKE_CXX_COMPILER set on
# this project (e.g. via a preset's cacheVariables). When those disagree -- a
# preset pinning clang while the environment's default cc is gcc -- gtest builds
# with gcc while LLVM (an add_subdirectory, which does inherit the compiler)
# builds with clang. On aarch64 that mismatch broke the debug link: gcc's -fpic
# gtest overflowed the small-model GOT in libponyc.tests where clang's did not.
# Forward the compiler explicitly so the sub-builds match. MSVC selects its
# compiler through the generator/toolset, which ExternalProject does carry over,
# so leave it alone there.
set(PONYC_LIBS_COMPILER_ARGS "")
if(NOT MSVC)
    set(PONYC_LIBS_COMPILER_ARGS
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER})
endif()

# Libraries

set(PONYC_GBENCHMARK_URL https://github.com/google/benchmark/archive/v1.9.5.tar.gz)
ExternalProject_Add(gbenchmark
    URL ${PONYC_GBENCHMARK_URL}
    CMAKE_ARGS -DCMAKE_BUILD_TYPE=${PONYC_LIBS_BUILD_TYPE} -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX} ${PONYC_LIBS_COMPILER_ARGS} -DBENCHMARK_ENABLE_GTEST_TESTS=OFF -DBENCHMARK_ENABLE_WERROR=OFF -DCMAKE_CXX_FLAGS=${PONY_PIC_FLAG} --no-warn-unused-cli
)

set(PONYC_GOOGLETEST_URL https://github.com/google/googletest/releases/download/v1.17.0/googletest-1.17.0.tar.gz)

ExternalProject_Add(googletest
    URL ${PONYC_GOOGLETEST_URL}
    CMAKE_ARGS -DCMAKE_BUILD_TYPE=${PONYC_LIBS_BUILD_TYPE} -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX} ${PONYC_LIBS_COMPILER_ARGS} -DCMAKE_CXX_FLAGS=${PONY_PIC_FLAG} -Dgtest_force_shared_crt=ON --no-warn-unused-cli
)

find_package(Git)

set(LLVM_DESIRED_HASH "fc4aad7b5db3fff421df9a9637605b9ca5667881")
set(PATCHES_DESIRED_HASH "49d865c5ab194d7b59781e8e99c68e1b8bc8b27e0cdaa0d1b498c5474e7ed060")

if(GIT_FOUND)
    if(EXISTS "${PROJECT_SOURCE_DIR}/../.git")
        # Update submodules as needed
        option(GIT_SUBMODULE "Check submodules during build" ON)
        if(GIT_SUBMODULE)
            message(STATUS "Updating submodules...")

            # The submodule clone (github.com/llvm/llvm-project, one of the
            # largest repos on GitHub) is anonymous by default. GitHub throttles
            # unauthenticated git operations, so on a cold CI cache -- when many
            # platforms clone at once -- the clone can fail with HTTP 429. When a
            # GITHUB_TOKEN is present in the environment, rewrite github.com URLs
            # to carry it so the clone authenticates and is billed to the token's
            # higher rate-limit bucket instead of the shared anonymous-per-IP
            # one. The rewrite goes through git's config-via-environment
            # (GIT_CONFIG_*): this file never writes the token to a command line,
            # to .gitmodules, or to on-disk git config. (git's own https helper
            # still carries the rewritten URL in its subprocess argv, so the
            # token is briefly visible to `ps` on this runner -- inherent to
            # embedding a credential in a URL.)
            #
            # A build with no GITHUB_TOKEN -- the usual developer machine -- adds
            # no rewrite and clones just as it would without this change. If a
            # developer does export GITHUB_TOKEN, this rewrite takes precedence
            # over a github.com URL rewrite of their own (e.g. an SSH `insteadOf`)
            # and forces an authenticated HTTPS clone. CI configures no such
            # rewrite.
            set(_llvm_clone_auth FALSE)
            if(NOT "$ENV{GITHUB_TOKEN}" STREQUAL "")
                set(ENV{GIT_CONFIG_COUNT} "1")
                set(ENV{GIT_CONFIG_KEY_0} "url.https://x-access-token:$ENV{GITHUB_TOKEN}@github.com/.insteadOf")
                set(ENV{GIT_CONFIG_VALUE_0} "https://github.com/")
                set(_llvm_clone_auth TRUE)
            endif()

            # Retry the clone: a 429 is transient. `git submodule update --init`
            # is safe to re-run -- a failed fetch leaves the submodule
            # un-updated, and the next attempt refetches -- so try up to three
            # times with backoff instead of failing the whole build on the first
            # blip. This retry wraps every build, authenticated or not. No jitter
            # -- not worth the complexity for two retries.
            set(_llvm_clone_delays 5 20)
            foreach(_attempt RANGE 1 3)
                execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive --depth 1
                                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                                RESULT_VARIABLE git_submod_result)
                if(git_submod_result EQUAL "0")
                    break()
                endif()
                if(_attempt LESS 3)
                    math(EXPR _delay_idx "${_attempt} - 1")
                    list(GET _llvm_clone_delays ${_delay_idx} _delay)
                    message(STATUS "git submodule update failed (${git_submod_result}); retrying in ${_delay}s...")
                    execute_process(COMMAND ${CMAKE_COMMAND} -E sleep ${_delay})
                endif()
            endforeach()

            # Clear the auth env before the git calls below (submodule status,
            # and the patch apply further down). The insteadOf rewrite only
            # touches github.com URLs, so those local git operations are
            # unaffected either way; this is tidiness.
            if(_llvm_clone_auth)
                unset(ENV{GIT_CONFIG_COUNT})
                unset(ENV{GIT_CONFIG_KEY_0})
                unset(ENV{GIT_CONFIG_VALUE_0})
            endif()

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

# ponyc consumes clang as a set of static libraries (gencshim's in-process shim
# compiles) plus the resource headers, so skip the clang driver binary and
# the other tool executables (verified: ~0.8G less in the install). The
# resource headers (clang-resource-headers) are part of the library build
# and unaffected. Note the analyzer archives and the libclang dylibs are
# library components that build regardless; CLANG_ENABLE_STATIC_ANALYZER
# only stops the (not-built) driver from wiring the analyzer in.
set(CLANG_BUILD_TOOLS OFF CACHE BOOL "ponyc specific override of clang cache entry")
set(CLANG_ENABLE_STATIC_ANALYZER OFF CACHE BOOL "ponyc specific override of clang cache entry")

# clang-fuzzer is clang's libFuzzer harness; ponyc never uses it. Its
# handle-llvm object library (clangHandleLLVM) reaches into LLVM internals and
# has a fragile build dependency on a generated header (TargetLibraryInfo.inc)
# that races under parallel builds -- it broke the OpenBSD libs build, whose
# -j4 timing lost the race while every other platform happened to win it.
# add_clang_subdirectory gates each tool on a CLANG_TOOL_<NAME>_BUILD option
# (AddLLVM.cmake's add_llvm_subdirectory), so turning this off skips the whole
# clang-fuzzer subtree on every platform.
set(CLANG_TOOL_CLANG_FUZZER_BUILD OFF CACHE BOOL "ponyc: skip the unused clang-fuzzer subtree")

add_subdirectory(llvm/src/llvm)
