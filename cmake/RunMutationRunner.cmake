# ctest wrapper: verify pony-mutate runner operates end-to-end.
#
# Compiles a Pony program with --mutate --mutate-scope=main.pony (narrowing
# to the test file's mutations only), then runs pony-mutate against the
# compiled binary.  Checks exit code and basic output structure.
#
# Args (passed with -D): COMPILER, RUNNER, OUTPUT, TEST_SRC.

file(MAKE_DIRECTORY "${OUTPUT}")

set(_bin "${OUTPUT}/mutation-runner-test")
if(WIN32)
    set(_bin "${_bin}.exe")
endif()

# Compile with --mutate, scoped to main.pony
execute_process(
    COMMAND "${COMPILER}" --mutate --mutate-scope=main.pony
        -b mutation-runner-test --output "${OUTPUT}" "${TEST_SRC}"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _err
)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "compilation failed (exit ${_rc}):\n${_out}\n${_err}")
endif()

set(_json "${OUTPUT}/mutation-runner-test.mutants.json")
if(NOT EXISTS "${_json}")
    message(FATAL_ERROR "no .mutants.json file found at ${_json}")
endif()

# Run pony-mutate with all mutants (scoped at compile time to ~7)
execute_process(
    COMMAND "${RUNNER}" "${_bin}" --workers=2
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr
    TIMEOUT 120
)

# Expect exit 1 (some mutants survive: eq->le and eq->ge are equivalent
# for 3+2==5 since 5<=5 and 5>=5 are both true)
if(_rc EQUAL 0)
    message(FATAL_ERROR
        "pony-mutate exited 0, expected 1 (some mutants should survive)"
        "\nstdout:\n${_stdout}\nstderr:\n${_stderr}")
endif()

# Verify terminal report appeared
string(FIND "${_stdout}" "Mutation Testing Report" _report_pos)
if(_report_pos EQUAL -1)
    message(FATAL_ERROR
        "terminal report not found in output"
        "\nstdout:\n${_stdout}\nstderr:\n${_stderr}")
endif()

string(FIND "${_stdout}" "Survived:" _survived_pos)
if(_survived_pos EQUAL -1)
    message(FATAL_ERROR
        "'Survived:' line not found in report"
        "\nstdout:\n${_stdout}\nstderr:\n${_stderr}")
endif()

# Verify operator filtering: rerun with --operators=aor
# All aor mutants should be killed (add->sub and add->mul both produce
# wrong results for 3+2==5)
execute_process(
    COMMAND "${RUNNER}" "${_bin}" --workers=2 --operators=aor
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr
    TIMEOUT 120
)

if(NOT _rc EQUAL 0)
    message(FATAL_ERROR
        "aor-only run exited ${_rc}, expected 0 (all aor mutants killed)"
        "\nstdout:\n${_stdout}\nstderr:\n${_stderr}")
endif()

message(STATUS "PASS: pony-mutate runner works end-to-end")
