# ctest wrapper: verify pony-mutate prints diagnostic output when the
# baseline run fails.
#
# Compiles a Pony program that always exits 1, runs pony-mutate against it,
# and checks that the error message includes the command line, environment,
# and captured stdout/stderr from the child process.
#
# Args (passed with -D): COMPILER, RUNNER, OUTPUT, TEST_SRC.

file(REMOVE_RECURSE "${OUTPUT}")
file(MAKE_DIRECTORY "${OUTPUT}")

set(_bin "${OUTPUT}/mutation-baseline-fail-test")
if(WIN32)
    set(_bin "${_bin}.exe")
endif()

# Compile with --mutate so the binary has a .mutants.json info file.
execute_process(
    COMMAND "${COMPILER}" --mutate
        -b mutation-baseline-fail-test --output "${OUTPUT}" "${TEST_SRC}"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _err
)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "compilation failed (exit ${_rc}):\n${_out}\n${_err}")
endif()

# Run pony-mutate — baseline must fail.
execute_process(
    COMMAND "${RUNNER}" "${_bin}"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr
    TIMEOUT 120
)

if(_rc EQUAL 0)
    message(FATAL_ERROR
        "pony-mutate exited 0, expected non-zero (baseline should fail)"
        "\nstdout:\n${_stdout}\nstderr:\n${_stderr}")
endif()

# Verify the error message appeared.
string(FIND "${_stderr}" "baseline run failed" _bl_pos)
if(_bl_pos EQUAL -1)
    message(FATAL_ERROR
        "'baseline run failed' not found in stderr"
        "\nstdout:\n${_stdout}\nstderr:\n${_stderr}")
endif()

# Verify command line was printed.
string(FIND "${_stderr}" "Command:" _cmd_pos)
if(_cmd_pos EQUAL -1)
    message(FATAL_ERROR
        "'Command:' not found in stderr"
        "\nstdout:\n${_stdout}\nstderr:\n${_stderr}")
endif()

# Verify environment was printed.
string(FIND "${_stderr}" "Environment:" _env_pos)
if(_env_pos EQUAL -1)
    message(FATAL_ERROR
        "'Environment:' not found in stderr"
        "\nstdout:\n${_stdout}\nstderr:\n${_stderr}")
endif()

# Verify captured child stdout appeared.
string(FIND "${_stderr}" "baseline-fail-marker" _marker_pos)
if(_marker_pos EQUAL -1)
    message(FATAL_ERROR
        "child stdout marker not found in pony-mutate stderr"
        "\nstdout:\n${_stdout}\nstderr:\n${_stderr}")
endif()

# Verify captured child stderr appeared.
string(FIND "${_stderr}" "baseline-fail-stderr-marker" _err_marker_pos)
if(_err_marker_pos EQUAL -1)
    message(FATAL_ERROR
        "child stderr marker not found in pony-mutate stderr"
        "\nstdout:\n${_stdout}\nstderr:\n${_stderr}")
endif()

message(STATUS "PASS: pony-mutate prints diagnostic on baseline failure")
