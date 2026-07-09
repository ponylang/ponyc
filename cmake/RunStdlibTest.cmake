# ctest wrapper: compile packages/stdlib with the freshly built ponyc, then run
# the resulting test binary.
#
# Args (passed with -D): PONYC, STDLIB_SRC (packages/stdlib), WORKDIR (the output
# directory the binary is built into and run from), BUILD_NAME (stdlib-debug or
# stdlib-release), DEBUG (ON for the debug build).
#
# Run-time knobs read from the environment (set by the caller/CI, defaulted here):
#   PONY_STDLIB_TEST_EXCLUDES  extra args appended, e.g. --exclude=net/Broadcast
#                              (default: none)
#   PONY_TEST_DEBUGGER         a debugger command to run the test binary under for
#                              crash backtraces, e.g. "lldb --batch ... --"
#                              (default: none; run the binary directly)

if(CMAKE_HOST_WIN32)
    set(_sep ";")
    set(_exe ".exe")
else()
    set(_sep ":")
    set(_exe "")
endif()
if(DEFINED ENV{PONYPATH} AND NOT "$ENV{PONYPATH}" STREQUAL "")
    set(ENV{PONYPATH} "${WORKDIR}${_sep}$ENV{PONYPATH}")
else()
    set(ENV{PONYPATH} "${WORKDIR}")
endif()

set(_args -b "${BUILD_NAME}" --checktree)
if(DEBUG)
    list(PREPEND _args -d)
endif()
# Match the pre-migration behavior: the Unix Makefile compiled stdlib with --pic
# (and --strip for the debug build); the Windows make.ps1 passed neither.
if(NOT CMAKE_HOST_WIN32)
    list(APPEND _args --pic)
    if(DEBUG)
        list(APPEND _args --strip)
    endif()
endif()

execute_process(
    COMMAND "${PONYC}" ${_args} "${STDLIB_SRC}"
    WORKING_DIRECTORY "${WORKDIR}"
    RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "compiling stdlib (${BUILD_NAME}) failed (exit ${_rc})")
endif()

# ponyc names the binary ${BUILD_NAME}.exe on Windows. Run directly, the Windows
# loader appends .exe; but a debugger (PONY_TEST_DEBUGGER, below) gets the path
# as a literal argument and won't, so name the binary exactly.
set(_run "${WORKDIR}/${BUILD_NAME}${_exe}" --sequential)
if(DEFINED ENV{PONY_STDLIB_TEST_EXCLUDES} AND NOT "$ENV{PONY_STDLIB_TEST_EXCLUDES}" STREQUAL "")
    separate_arguments(_extra UNIX_COMMAND "$ENV{PONY_STDLIB_TEST_EXCLUDES}")
    list(APPEND _run ${_extra})
endif()

# Run under a debugger (for crash backtraces) if the caller/CI set one.
set(_debugger FALSE)
if(DEFINED ENV{PONY_TEST_DEBUGGER} AND NOT "$ENV{PONY_TEST_DEBUGGER}" STREQUAL "")
    separate_arguments(_dbg UNIX_COMMAND "$ENV{PONY_TEST_DEBUGGER}")
    set(_run ${_dbg} ${_run})
    set(_debugger TRUE)
endif()

# A debugger is instrumentation for a backtrace; it must not decide pass/fail.
# The pinned Windows lldb (.ci-scripts/windows-install-deps.ps1) can crash on
# its own exit with 0xc0000374 after the test binary exited cleanly, so its exit
# code is unreliable. Under a debugger, capture the output and read the binary's
# own status from the "Process N exited with status = N" line, as make.ps1 did;
# if it is absent the binary never ran, so keep the debugger's exit code.
# Without a debugger, stream output live and use the binary's real exit code.
if(_debugger)
    execute_process(
        COMMAND ${_run}
        WORKING_DIRECTORY "${WORKDIR}"
        OUTPUT_VARIABLE _out ERROR_VARIABLE _errout
        ECHO_OUTPUT_VARIABLE ECHO_ERROR_VARIABLE
        RESULT_VARIABLE _rc)
    if("${_out}${_errout}"
       MATCHES "Process [0-9]+ exited with status = ([0-9]+)")
        set(_rc "${CMAKE_MATCH_1}")
    endif()
else()
    execute_process(
        COMMAND ${_run}
        WORKING_DIRECTORY "${WORKDIR}"
        RESULT_VARIABLE _rc)
endif()
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "stdlib tests (${BUILD_NAME}) failed (exit ${_rc})")
endif()
