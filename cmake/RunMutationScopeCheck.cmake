# ctest wrapper: verify --mutate-scope rejects unmatched file patterns.
#
# Compiles a Pony program with --mutate --mutate-scope=nonexistent.pony and
# expects the compiler to fail. Then compiles with a valid scope to confirm
# the positive case still works. Finally, checks that mixing a valid and an
# invalid scope still fails.
#
# Args (passed with -D): COMPILER, OUTPUT, TEST_SRC.

file(REMOVE_RECURSE "${OUTPUT}")
file(MAKE_DIRECTORY "${OUTPUT}")

set(_bad_bin "${OUTPUT}/scope-bad")
if(WIN32)
    set(_bad_bin "${_bad_bin}.exe")
endif()

# 1. A scope that matches no source file should fail.
execute_process(
    COMMAND "${COMPILER}" --mutate --mutate-scope=nonexistent.pony
        -b scope-bad --output "${OUTPUT}" "${TEST_SRC}"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _err
)
if(_rc EQUAL 0)
    message(FATAL_ERROR
        "compilation succeeded with unmatched --mutate-scope; expected failure")
endif()

string(FIND "${_err}" "didn't match any source file" _pos)
if(_pos EQUAL -1)
    message(FATAL_ERROR
        "error output missing expected message:\n${_err}")
endif()

if(EXISTS "${_bad_bin}")
    message(FATAL_ERROR "binary was written despite scope mismatch")
endif()

message(STATUS "PASS: unmatched scope correctly rejected")

# 2. A valid scope should still compile.
execute_process(
    COMMAND "${COMPILER}" --mutate --mutate-scope=main.pony
        -b scope-good --output "${OUTPUT}" "${TEST_SRC}"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _err
)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR
        "compilation failed with valid --mutate-scope (exit ${_rc}):\n${_out}\n${_err}")
endif()

message(STATUS "PASS: valid scope compiles successfully")

# 3. A valid scope mixed with an invalid scope should still fail.
execute_process(
    COMMAND "${COMPILER}" --mutate
        --mutate-scope=main.pony --mutate-scope=nonexistent.pony
        -b scope-mixed --output "${OUTPUT}" "${TEST_SRC}"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _err
)
if(_rc EQUAL 0)
    message(FATAL_ERROR
        "compilation succeeded with mixed valid/invalid --mutate-scope; expected failure")
endif()

string(FIND "${_err}" "didn't match any source file" _pos)
if(_pos EQUAL -1)
    message(FATAL_ERROR
        "error output missing expected message for mixed scopes:\n${_err}")
endif()

message(STATUS "PASS: mixed valid/invalid scopes correctly rejected")
