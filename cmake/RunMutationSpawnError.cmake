# ctest wrapper: verify pony-mutate exits 2 when workers cannot spawn the
# mutant binary.
#
# Compiles a Pony program with --mutate so a valid .mutants.json exists,
# then deletes the binary and runs pony-mutate --no-baseline against the
# missing binary. Every mutant hits a spawn error, so the tool must exit 2
# and report the failures.
#
# Args (passed with -D): COMPILER, RUNNER, OUTPUT, TEST_SRC.

file(REMOVE_RECURSE "${OUTPUT}")
file(MAKE_DIRECTORY "${OUTPUT}")

set(_bin "${OUTPUT}/spawn-error-test")
if(WIN32)
    set(_bin "${_bin}.exe")
endif()

# Compile with --mutate, scoped to main.pony
execute_process(
    COMMAND "${COMPILER}" --mutate --mutate-scope=main.pony
        -b spawn-error-test --output "${OUTPUT}" "${TEST_SRC}"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _err
)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "compilation failed (exit ${_rc}):\n${_out}\n${_err}")
endif()

set(_json "${OUTPUT}/spawn-error-test.mutants.json")
if(NOT EXISTS "${_json}")
    message(FATAL_ERROR "no .mutants.json file found at ${_json}")
endif()

# Delete the binary so spawning it fails.
file(REMOVE "${_bin}")

# Run pony-mutate with --no-baseline (the binary is gone).
execute_process(
    COMMAND "${RUNNER}" --no-baseline --workers=1 "${_bin}"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr
    TIMEOUT 120
)

# Expect exit 2 — spawn errors are operational errors.
if(NOT _rc EQUAL 2)
    message(FATAL_ERROR
        "pony-mutate exited ${_rc}, expected 2 (spawn errors should be exit 2)"
        "\nstdout:\n${_stdout}\nstderr:\n${_stderr}")
endif()

# Verify that "SPAWN ERROR" appears in progress output (stderr).
string(FIND "${_stderr}" "SPAWN ERROR" _spawn_err_pos)
if(_spawn_err_pos EQUAL -1)
    message(FATAL_ERROR
        "'SPAWN ERROR' not found in stderr"
        "\nstdout:\n${_stdout}\nstderr:\n${_stderr}")
endif()

# Verify that the terminal report includes "Spawn errors:" line.
string(FIND "${_stdout}" "Spawn errors:" _report_pos)
if(_report_pos EQUAL -1)
    message(FATAL_ERROR
        "'Spawn errors:' not found in terminal report"
        "\nstdout:\n${_stdout}\nstderr:\n${_stderr}")
endif()

# Verify mutation score is 0.0% when all mutants are spawn errors.
string(FIND "${_stdout}" "Mutation score: 0.0%" _score_pos)
if(_score_pos EQUAL -1)
    message(FATAL_ERROR
        "'Mutation score: 0.0%' not found in terminal report"
        "\nstdout:\n${_stdout}\nstderr:\n${_stderr}")
endif()

# Verify Stryker JSON maps spawn errors to "RuntimeError".
execute_process(
    COMMAND "${RUNNER}" --no-baseline --workers=1 --format=json "${_bin}"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _json_stdout
    ERROR_VARIABLE _json_stderr
    TIMEOUT 120
)

if(NOT _rc EQUAL 2)
    message(FATAL_ERROR
        "json run exited ${_rc}, expected 2"
        "\nstdout:\n${_json_stdout}\nstderr:\n${_json_stderr}")
endif()

string(FIND "${_json_stdout}" "RuntimeError" _re_pos)
if(_re_pos EQUAL -1)
    message(FATAL_ERROR
        "'RuntimeError' not found in Stryker JSON output"
        "\nstdout:\n${_json_stdout}\nstderr:\n${_json_stderr}")
endif()

message(STATUS "PASS: pony-mutate reports spawn errors correctly")
