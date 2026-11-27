# ctest wrapper: run the full-program test runner. The runner's per-program timeout
# and debugger are run-time knobs read from the environment (defaulted here); the
# rest of the invocation is fixed and passed in via -D.
#
# Args (passed with -D): RUNNER, COMPILER, OUTPUT, TEST_LIB, TESTS (the test dir),
# WORKDIR, SOURCE_DIR (the repo root), DB_NAME (the directory under .ponytest/
# for property test regressions). Optional: MAX_PARALLEL (default 1), CHECKTREE
# (ON passes --checktree), PARALLEL_FROM_ENV (ON lets PONY_FULL_PROGRAM_PARALLEL
# override MAX_PARALLEL).
#
# Run-time knobs from the environment:
#   PONY_DEBUG                 set to 1 to pass --debug to the runner
#   PONY_THIN_LTO              set to 1 to pass --thin-lto to the runner
#   PONY_CHECKTREE             set to 1 to pass --checktree to the runner
#   PONY_FULL_PROGRAM_TIMEOUT  per-program timeout in seconds (default: 120)
#   PONY_FULL_PROGRAM_PARALLEL number of programs built and run at once, for
#                              the entries that pass PARALLEL_FROM_ENV
#                              (default: MAX_PARALLEL)
#   PONY_TEST_DEBUGGER         debugger command the runner runs each program under
#                              (default: none)

set(_timeout 120)
if(DEFINED ENV{PONY_FULL_PROGRAM_TIMEOUT} AND NOT "$ENV{PONY_FULL_PROGRAM_TIMEOUT}" STREQUAL "")
    set(_timeout "$ENV{PONY_FULL_PROGRAM_TIMEOUT}")
endif()

# Create the per-config output directory. On the Windows multi-config generator
# the caller can't create it at configure time (the path is config-dependent),
# so the runner's --output dir may not exist yet; make it here. Harmless on Unix,
# where it already exists.
file(MAKE_DIRECTORY "${OUTPUT}")

# Property test regression persistence: store regression files in the source
# tree so they can be committed, not in the build directory where make clean
# removes them.
set(ENV{PONYTEST_DB_DIR} "${SOURCE_DIR}/.ponytest/${DB_NAME}")

if(NOT DEFINED MAX_PARALLEL)
    set(MAX_PARALLEL 1)
endif()
if(PARALLEL_FROM_ENV AND DEFINED ENV{PONY_FULL_PROGRAM_PARALLEL}
    AND NOT "$ENV{PONY_FULL_PROGRAM_PARALLEL}" STREQUAL "")
    set(MAX_PARALLEL "$ENV{PONY_FULL_PROGRAM_PARALLEL}")
endif()

set(_run "${RUNNER}"
    "--debugger=$ENV{PONY_TEST_DEBUGGER}"
    "--timeout_s=${_timeout}"
    "--max_parallel=${MAX_PARALLEL}"
    "--compiler=${COMPILER}"
    "--output=${OUTPUT}"
    "--test_lib=${TEST_LIB}")
if(DEFINED ENV{PONY_DEBUG} AND "$ENV{PONY_DEBUG}" STREQUAL "1")
    list(APPEND _run --debug)
endif()
if(DEFINED ENV{PONY_THIN_LTO} AND "$ENV{PONY_THIN_LTO}" STREQUAL "1")
    list(APPEND _run --thin-lto)
endif()
if(CHECKTREE OR (DEFINED ENV{PONY_CHECKTREE} AND "$ENV{PONY_CHECKTREE}" STREQUAL "1"))
    list(APPEND _run --checktree)
endif()
list(APPEND _run "${TESTS}")

execute_process(COMMAND ${_run} WORKING_DIRECTORY "${WORKDIR}" RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "full-program tests in ${TESTS} failed (exit ${_rc})")
endif()
