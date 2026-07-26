# ctest wrapper: run the full-program test runner. The runner's per-program timeout
# and debugger are run-time knobs read from the environment (defaulted here); the
# rest of the invocation is fixed and passed in via -D.
#
# Args (passed with -D): RUNNER, COMPILER, OUTPUT, TEST_LIB, TESTS (the test dir),
# WORKDIR, DEBUG (ON adds --debug for the debug-runtime build).
#
# Run-time knobs from the environment:
#   PONY_FULL_PROGRAM_TIMEOUT  per-program timeout in seconds
#                              (default: 60; 120 on Windows)
#   PONY_TEST_DEBUGGER         debugger command the runner runs each program under
#                              (default: none)

# The old make.ps1 gave Windows full-program tests a 120s per-program timeout,
# twice Unix's 60s, for the slower Windows runners; keep that split.
if(CMAKE_HOST_WIN32)
    set(_timeout 120)
else()
    set(_timeout 60)
endif()
if(DEFINED ENV{PONY_FULL_PROGRAM_TIMEOUT} AND NOT "$ENV{PONY_FULL_PROGRAM_TIMEOUT}" STREQUAL "")
    set(_timeout "$ENV{PONY_FULL_PROGRAM_TIMEOUT}")
endif()

# Create the per-config output directory. On the Windows multi-config generator
# the caller can't create it at configure time (the path is config-dependent),
# so the runner's --output dir may not exist yet; make it here. Harmless on Unix,
# where it already exists.
file(MAKE_DIRECTORY "${OUTPUT}")

set(_run "${RUNNER}"
    "--debugger=$ENV{PONY_TEST_DEBUGGER}"
    "--timeout_s=${_timeout}"
    --max_parallel=1
    "--compiler=${COMPILER}"
    "--output=${OUTPUT}"
    "--test_lib=${TEST_LIB}")
if(DEBUG)
    list(APPEND _run --debug)
endif()
list(APPEND _run "${TESTS}")

execute_process(COMMAND ${_run} WORKING_DIRECTORY "${WORKDIR}" RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "full-program tests failed (exit ${_rc})")
endif()
