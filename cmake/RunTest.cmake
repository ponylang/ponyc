# ctest wrapper: run a test binary, optionally under a debugger for crash
# backtraces. The debugger command is an environment/CI concern -- this is only the
# generic "run under it if one is set" plumbing; the command itself is never baked
# in here.
#
# Args (passed with -D): BIN (the test binary), WORKDIR (working directory);
# ARGS (optional ;-separated extra args); GTEST (ON to add
# --gtest_throw_on_failure when a debugger is set, so a failed assertion aborts
# into the debugger).
#
# Run-time knob from the environment (default: none, run the binary directly):
#   PONY_TEST_DEBUGGER  a debugger command, e.g. "lldb --batch ... --"

set(_run "${BIN}")
if(ARGS)
    list(APPEND _run ${ARGS})
endif()

set(_debugger FALSE)
if(DEFINED ENV{PONY_TEST_DEBUGGER} AND NOT "$ENV{PONY_TEST_DEBUGGER}" STREQUAL "")
    if(GTEST)
        list(APPEND _run --gtest_throw_on_failure)
    endif()
    separate_arguments(_dbg UNIX_COMMAND "$ENV{PONY_TEST_DEBUGGER}")
    set(_run ${_dbg} ${_run})
    set(_debugger TRUE)
endif()

# A debugger is instrumentation for a backtrace; it must not decide pass/fail.
# The pinned Windows lldb (.ci-scripts/windows-install-deps.ps1) can crash on
# its own exit with 0xc0000374 after the test binary exited cleanly, so its exit
# code is unreliable. Under a debugger, capture the output and read the binary's
# own status from the "Process N exited with status = N" line; if it is absent
# the binary never ran, so keep the debugger's exit code. Without a debugger,
# stream the output live and use the binary's real exit code.
# See .known-couplings/debugger-exit-status-parsing.md.
if(_debugger)
    execute_process(COMMAND ${_run} WORKING_DIRECTORY "${WORKDIR}"
        OUTPUT_VARIABLE _out ERROR_VARIABLE _errout
        ECHO_OUTPUT_VARIABLE ECHO_ERROR_VARIABLE RESULT_VARIABLE _rc)
    if("${_out}${_errout}"
       MATCHES "Process [0-9]+ exited with status = ([0-9]+)")
        set(_rc "${CMAKE_MATCH_1}")
    endif()
else()
    execute_process(COMMAND ${_run} WORKING_DIRECTORY "${WORKDIR}"
        RESULT_VARIABLE _rc)
endif()
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "test failed: ${BIN} (exit ${_rc})")
endif()
