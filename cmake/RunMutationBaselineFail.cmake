# ctest wrapper: verify pony-mutate prints diagnostic output when the baseline
# fails.
#
# Compiles a Pony program that always exits 1, runs pony-mutate against it,
# and checks that stderr contains the command line, environment, and the
# test binary's stdout and stderr.
#
# Args (passed with -D): COMPILER, RUNNER, OUTPUT, TEST_SRC.

file(REMOVE_RECURSE "${OUTPUT}")
file(MAKE_DIRECTORY "${OUTPUT}")

set(_bin "${OUTPUT}/baseline-fail-test")
if(WIN32)
    set(_bin "${_bin}.exe")
endif()

# Compile with --mutate
execute_process(
    COMMAND "${COMPILER}" --mutate
        -b baseline-fail-test --output "${OUTPUT}" "${TEST_SRC}"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _err
)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "compilation failed (exit ${_rc}):\n${_out}\n${_err}")
endif()

# Run pony-mutate — baseline should fail
execute_process(
    COMMAND "${RUNNER}" "${_bin}"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr
    TIMEOUT 60
)

# Expect non-zero exit
if(_rc EQUAL 0)
    message(FATAL_ERROR
        "pony-mutate exited 0, expected non-zero (baseline should fail)"
        "\nstdout:\n${_stdout}\nstderr:\n${_stderr}")
endif()

# Verify the error message appears
string(FIND "${_stderr}" "baseline run failed" _err_pos)
if(_err_pos EQUAL -1)
    message(FATAL_ERROR
        "'baseline run failed' not found in stderr"
        "\nstdout:\n${_stdout}\nstderr:\n${_stderr}")
endif()

# Verify the command line is printed
string(FIND "${_stderr}" "Command:" _cmd_pos)
if(_cmd_pos EQUAL -1)
    message(FATAL_ERROR
        "'Command:' not found in stderr"
        "\nstdout:\n${_stdout}\nstderr:\n${_stderr}")
endif()

# Verify the environment is printed
string(FIND "${_stderr}" "Environment:" _env_pos)
if(_env_pos EQUAL -1)
    message(FATAL_ERROR
        "'Environment:' not found in stderr"
        "\nstdout:\n${_stdout}\nstderr:\n${_stderr}")
endif()

# Verify the stdout section label and captured content
string(FIND "${_stderr}" "stdout:" _stdout_label_pos)
if(_stdout_label_pos EQUAL -1)
    message(FATAL_ERROR
        "'stdout:' label not found in stderr"
        "\nstdout:\n${_stdout}\nstderr:\n${_stderr}")
endif()

string(FIND "${_stderr}" "baseline-marker" _marker_pos)
if(_marker_pos EQUAL -1)
    message(FATAL_ERROR
        "'baseline-marker' (test binary's stdout) not found in stderr"
        "\nstdout:\n${_stdout}\nstderr:\n${_stderr}")
endif()

# Verify the stderr section label and captured content
string(FIND "${_stderr}" "stderr:" _stderr_label_pos)
if(_stderr_label_pos EQUAL -1)
    message(FATAL_ERROR
        "'stderr:' label not found in stderr"
        "\nstdout:\n${_stdout}\nstderr:\n${_stderr}")
endif()

string(FIND "${_stderr}" "stderr-marker" _stderr_marker_pos)
if(_stderr_marker_pos EQUAL -1)
    message(FATAL_ERROR
        "'stderr-marker' (test binary's stderr) not found in stderr"
        "\nstdout:\n${_stdout}\nstderr:\n${_stderr}")
endif()

message(STATUS "PASS: pony-mutate prints diagnostics on baseline failure")
