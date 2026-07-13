# ctest wrapper: assert the full-program runner fails the run when a test's
# configuration cannot be read (issue #5749). A test directory that has Pony
# sources but an unreadable configuration file must fail the run, not be dropped
# while the run still exits 0. A directory with no Pony sources is not a test and
# must still be skipped, even when it holds an unreadable configuration file.
#
# The fixture is generated here rather than checked in so a deliberately broken
# configuration file never sits in the source tree where the real full-program
# suite would scan it.
#
# Args (passed with -D): RUNNER (the runner binary), WORKDIR (a scratch dir the
# fixture is generated under).

set(_fixture "${WORKDIR}/full-program-runner-broken-config")
set(_tests "${_fixture}/tests")
file(REMOVE_RECURSE "${_fixture}")
file(MAKE_DIRECTORY "${_tests}/broken")
file(MAKE_DIRECTORY "${_tests}/notatest")
file(MAKE_DIRECTORY "${_fixture}/out")
# A test directory: Pony sources plus an unreadable configuration.
file(WRITE "${_tests}/broken/main.pony"
    "actor Main\n  new create(env: Env) => None\n")
file(WRITE "${_tests}/broken/expected-exit-code.txt" "not-a-number\n")
# Not a test directory: an unreadable configuration but no Pony sources.
file(WRITE "${_tests}/notatest/expected-exit-code.txt" "not-a-number\n")

# The runner rejects the broken configuration during discovery, before it builds
# anything, so no compiler is needed here.
execute_process(
    COMMAND "${RUNNER}" --max_parallel=1 "--output=${_fixture}/out" "${_tests}"
    WORKING_DIRECTORY "${WORKDIR}"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _err)
set(_output "${_out}${_err}")

if(_rc EQUAL 0)
    message(FATAL_ERROR
        "runner exited 0 with an unreadable test configuration; it must fail "
        "the run (issue #5749).\nOutput:\n${_output}")
endif()

# The fixture path contains "broken", so a non-zero exit for the wrong reason
# (e.g. a missing output directory) echoes a path that would match the test
# name. Match a phrase only the broken-configuration report emits, to confirm
# the runner failed for the right reason.
if(NOT "${_output}" MATCHES "could not be configured")
    message(FATAL_ERROR
        "runner failed but did not report the broken configuration; it may have "
        "failed for the wrong reason.\nOutput:\n${_output}")
endif()

# The directory with no Pony sources is not a test and must be skipped, not
# reported as broken, even though it holds an unreadable configuration.
if("${_output}" MATCHES "notatest")
    message(FATAL_ERROR
        "runner reported a directory with no Pony sources as a broken test; a "
        "non-test directory must be skipped.\nOutput:\n${_output}")
endif()
