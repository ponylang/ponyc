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

# Run pony-mutate with all mutants
execute_process(
    COMMAND "${RUNNER}" "${_bin}" --workers=2
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr
    TIMEOUT 120
)

# Expect exit 1 — some mutants survive because equivalent substitutions
# exist for the test program's comparison
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

# Verify coverage filtering: _never_called() is never called, so its mutants
# should be skipped.
string(FIND "${_stderr}" "Skipping" _skip_pos)
if(_skip_pos EQUAL -1)
    message(FATAL_ERROR
        "expected 'Skipping ... no coverage' in stderr"
        "\nstdout:\n${_stdout}\nstderr:\n${_stderr}")
endif()

string(FIND "${_stdout}" "No coverage:" _nocov_pos)
if(_nocov_pos EQUAL -1)
    message(FATAL_ERROR
        "expected 'No coverage:' in terminal report"
        "\nstdout:\n${_stdout}\nstderr:\n${_stderr}")
endif()

# Verify operator filtering: rerun with --operators=aor.
# All aor mutants should be killed.
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

# Verify the aor run actually tested mutants (not a vacuous pass)
string(FIND "${_stdout}" "Killed:" _killed_pos)
if(_killed_pos EQUAL -1)
    message(FATAL_ERROR
        "'Killed:' line not found in aor report — test may be vacuous"
        "\nstdout:\n${_stdout}\nstderr:\n${_stderr}")
endif()

string(REGEX MATCH "Killed: +([0-9]+)" _killed_match "${_stdout}")
if(NOT CMAKE_MATCH_1 GREATER 0)
    message(FATAL_ERROR
        "aor run killed 0 mutants — test is vacuous"
        "\nstdout:\n${_stdout}\nstderr:\n${_stderr}")
endif()

message(STATUS "PASS: pony-mutate runner works end-to-end")
