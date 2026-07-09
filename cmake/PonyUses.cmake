# use= build-option parsing and validation.
#
# The `-DPONY_USES` build options documented in BUILD.md
# arrive as a single comma-separated PONY_USES string. This module parses and
# validates them and sets the PONY_USE_* variables the top-level CMakeLists.txt
# consumes. It lives in the shared CMake project so every entry point -- both
# wrappers and any direct cmake/preset invocation, on every platform -- gets
# identical checks. It is include()d (not add_subdirectory'd) so the PONY_USE_*
# it sets land in the includer's scope.
#
# Keep each rejection message's "not supported on <OS>" phrase within the first
# ~78 columns: the BSD reject scripts
# (.ci-scripts/{openbsd,dragonfly}-reject-unsupported-builds.sh) grep for that
# exact substring, and CMake's message() reflows at ~78 columns. Those scripts
# also enumerate the rejected options, so add or drop a platform rejection here
# and the matching script's loop must change too.

set(PONY_USES "" CACHE STRING
    "Comma-separated build options to enable at configure time; see BUILD.md")

# PONY_USES is the single source of truth, so every PONY_USE_* is (re)written
# on each configure. FORCE is required: a plain `set(... CACHE INTERNAL)` does
# NOT overwrite a value already in the cache -- from a previous configure of
# this dir, or a stray -DPONY_USE_* -- so without it, an option cleared from
# PONY_USES would linger. CACHE INTERNAL keeps them visible to the
# subdirectories that read them (e.g. src/libponyrt reads PONY_USE_DTRACE)
# without exposing them in the GUI.
macro(_pony_set_use _name _value)
    set(PONY_USE_${_name} ${_value} CACHE INTERNAL "use= build option" FORCE)
endmacro()

# The canonical set. Keep in sync with the if/elseif chain below, BUILD.md, and
# the PONY_USE_* blocks in CMakeLists.txt.
set(_pony_known_uses
    valgrind
    thread_sanitizer
    address_sanitizer
    undefined_behavior_sanitizer
    coverage
    pooltrack
    dtrace
    scheduler_scaling_pthreads
    systematic_testing
    runtimestats
    runtimestats_messages
    pool_memalign
    pool_retain
    runtime_tracing)

# The subset of the above that compiles on Windows (MSVC), verified by building
# each on Windows. See the MSVC check below. When you add an option here, add a
# matching row to the use_directives_windows matrix in ponyc-weekly-checks.yml,
# or it builds on Windows but is never tested there.
set(_pony_windows_uses
    systematic_testing
    pool_retain
    pooltrack
    runtimestats
    runtimestats_messages
    runtime_tracing)

# Reset every option off before applying PONY_USES.
foreach(_use IN LISTS _pony_known_uses)
    string(TOUPPER "${_use}" _use_upper)
    _pony_set_use(${_use_upper} OFF)
endforeach()

# Split on commas or whitespace (both separators are accepted),
# drop blanks, and de-duplicate. Order doesn't affect which options end up
# enabled, so no sort is needed.
string(REGEX REPLACE "[ \t,]+" ";" _pony_uses_list "${PONY_USES}")
set(_pony_uses "")
foreach(_use IN LISTS _pony_uses_list)
    if(NOT _use STREQUAL "")
        list(APPEND _pony_uses "${_use}")
    endif()
endforeach()
list(REMOVE_DUPLICATES _pony_uses)

foreach(_use IN LISTS _pony_uses)
    message(STATUS "Enabling use option: ${_use}")
    # On Windows (MSVC) only the options in _pony_windows_uses build; the rest
    # need POSIX or Clang/GCC toolchain features cl.exe doesn't provide, verified
    # by building each on Windows: pool_memalign needs posix_memalign; the
    # sanitizers and coverage need -fsanitize=/-fprofile-arcs;
    # scheduler_scaling_pthreads needs pthreads.
    # Reject the unsupported ones here rather than let the build fail partway
    # through with a confusing error. Keep BUILD.md's option list in step.
    if(MSVC AND NOT _use IN_LIST _pony_windows_uses)
        message(FATAL_ERROR "${_use} is not supported when building on Windows. See BUILD.md")
    endif()
    if(_use STREQUAL "valgrind")
        if(CMAKE_HOST_SYSTEM_NAME STREQUAL "OpenBSD")
            message(FATAL_ERROR "Valgrind is not supported on OpenBSD: Valgrind has no OpenBSD port, so its development headers aren't available. See BUILD.md")
        endif()
        _pony_set_use(VALGRIND ON)
    elseif(_use STREQUAL "thread_sanitizer")
        if(CMAKE_HOST_SYSTEM_NAME STREQUAL "OpenBSD")
            message(FATAL_ERROR "ThreadSanitizer is not supported on OpenBSD: the base toolchain ships no ThreadSanitizer runtime. See BUILD.md")
        endif()
        if(CMAKE_HOST_SYSTEM_NAME STREQUAL "DragonFly")
            message(FATAL_ERROR "ThreadSanitizer is not supported on DragonFly: the gcc toolchain ships no ThreadSanitizer runtime. See BUILD.md")
        endif()
        _pony_set_use(THREAD_SANITIZER ON)
    elseif(_use STREQUAL "address_sanitizer")
        if(CMAKE_HOST_SYSTEM_NAME STREQUAL "OpenBSD")
            message(FATAL_ERROR "AddressSanitizer is not supported on OpenBSD: the base toolchain ships no AddressSanitizer runtime. See BUILD.md")
        endif()
        if(CMAKE_HOST_SYSTEM_NAME STREQUAL "DragonFly")
            message(FATAL_ERROR "AddressSanitizer is not supported on DragonFly: the gcc toolchain ships no AddressSanitizer runtime. See BUILD.md")
        endif()
        _pony_set_use(ADDRESS_SANITIZER ON)
    elseif(_use STREQUAL "undefined_behavior_sanitizer")
        if(CMAKE_HOST_SYSTEM_NAME STREQUAL "OpenBSD")
            message(FATAL_ERROR "UndefinedBehaviorSanitizer is not supported on OpenBSD: the base toolchain ships only the minimal UBSan runtime, not the standalone runtime ponyc links against. See BUILD.md")
        endif()
        if(CMAKE_HOST_SYSTEM_NAME STREQUAL "DragonFly")
            message(FATAL_ERROR "UndefinedBehaviorSanitizer is not supported on DragonFly: the gcc toolchain ships no UndefinedBehaviorSanitizer runtime. See BUILD.md")
        endif()
        _pony_set_use(UNDEFINED_BEHAVIOR_SANITIZER ON)
    elseif(_use STREQUAL "coverage")
        if(CMAKE_HOST_SYSTEM_NAME STREQUAL "OpenBSD")
            message(FATAL_ERROR "Coverage instrumentation is not supported on OpenBSD: the base toolchain's profiling runtime is incomplete, so coverage builds fail to link. See BUILD.md")
        endif()
        _pony_set_use(COVERAGE ON)
    elseif(_use STREQUAL "pooltrack")
        _pony_set_use(POOLTRACK ON)
    elseif(_use STREQUAL "dtrace")
        # OS rejections MUST precede the tool check so the BSD reject scripts
        # see "not supported on <OS>" rather than the missing-tool message.
        if(CMAKE_HOST_SYSTEM_NAME STREQUAL "OpenBSD")
            message(FATAL_ERROR "DTrace is not supported on OpenBSD. See BUILD.md")
        endif()
        if(CMAKE_HOST_SYSTEM_NAME STREQUAL "DragonFly")
            message(FATAL_ERROR "DTrace is not supported on DragonFly. See BUILD.md")
        endif()
        find_program(PONY_DTRACE_EXECUTABLE dtrace)
        if(NOT PONY_DTRACE_EXECUTABLE)
            message(FATAL_ERROR "No dtrace compatible user application static probe generation tool found")
        endif()
        _pony_set_use(DTRACE ON)
    elseif(_use STREQUAL "scheduler_scaling_pthreads")
        _pony_set_use(SCHEDULER_SCALING_PTHREADS ON)
    elseif(_use STREQUAL "systematic_testing")
        _pony_set_use(SYSTEMATIC_TESTING ON)
    elseif(_use STREQUAL "runtimestats")
        _pony_set_use(RUNTIMESTATS ON)
    elseif(_use STREQUAL "runtimestats_messages")
        _pony_set_use(RUNTIMESTATS_MESSAGES ON)
    elseif(_use STREQUAL "pool_memalign")
        _pony_set_use(POOL_MEMALIGN ON)
    elseif(_use STREQUAL "pool_retain")
        _pony_set_use(POOL_RETAIN ON)
    elseif(_use STREQUAL "runtime_tracing")
        _pony_set_use(RUNTIME_TRACING ON)
    else()
        message(FATAL_ERROR "Unknown use option specified: ${_use}")
    endif()
endforeach()
