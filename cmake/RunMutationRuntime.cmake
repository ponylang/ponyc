# ctest wrapper: verify PONY_MUTANT_ID runtime behavior.
#
# Compiles a Pony program with --mutate, runs it without PONY_MUTANT_ID
# (expecting exit 0), finds the add->sub mutant from the test source in the
# JSON, and runs again with that ID set (expecting exit 1).
#
# Args (passed with -D): COMPILER, OUTPUT, TEST_SRC.

file(MAKE_DIRECTORY "${OUTPUT}")

set(_bin "${OUTPUT}/mutation-runtime-test")

# Compile with --mutate
execute_process(
    COMMAND "${COMPILER}" --mutate -b mutation-runtime-test
        --output "${OUTPUT}" "${TEST_SRC}"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _err
)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "compilation failed (exit ${_rc}):\n${_out}\n${_err}")
endif()

set(_json "${OUTPUT}/mutation-runtime-test.mutants.json")
if(NOT EXISTS "${_json}")
    message(FATAL_ERROR "no .mutants.json file found at ${_json}")
endif()

# Run without PONY_MUTANT_ID — should exit 0
unset(ENV{PONY_MUTANT_ID})
execute_process(
    COMMAND "${_bin}" --ponynoscale --ponymaxthreads=1
    RESULT_VARIABLE _rc
    TIMEOUT 30
)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "default run exited ${_rc}, expected 0")
endif()

# Verify .coverage file was written
set(_cov "${_bin}.coverage")
if(NOT EXISTS "${_cov}")
    message(FATAL_ERROR "no .coverage file found at ${_cov}")
endif()
file(SIZE "${_cov}" _cov_size)
# At minimum: 4 bytes (count) + 8 bytes (one counter)
if(_cov_size LESS 12)
    message(FATAL_ERROR ".coverage file too small (${_cov_size} bytes)")
endif()
message(STATUS "coverage file exists (${_cov_size} bytes)")

# Find the add->sub mutant from the test file using grep.
execute_process(
    COMMAND grep -m1 "main.pony.*add.*sub" "${_json}"
    OUTPUT_VARIABLE _match_line
    RESULT_VARIABLE _grep_rc
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT _grep_rc EQUAL 0 OR NOT _match_line)
    message(FATAL_ERROR "could not find add->sub mutant from test source in JSON")
endif()

# Extract the mutant ID from the matched line.
string(REGEX MATCH "\"id\": *([0-9]+)" _id_match "${_match_line}")
string(REGEX REPLACE ".*\"id\": *([0-9]+).*" "\\1" _mutant_id "${_match_line}")

if(NOT _mutant_id MATCHES "^[0-9]+$")
    message(FATAL_ERROR "could not extract numeric mutant ID from: ${_match_line}")
endif()

message(STATUS "Using mutant ID: ${_mutant_id} (add->sub from main.pony)")

# Run with PONY_MUTANT_ID set — should exit 1 (3-2=1, not 5)
set(ENV{PONY_MUTANT_ID} "${_mutant_id}")
execute_process(
    COMMAND "${_bin}" --ponynoscale --ponymaxthreads=1
    RESULT_VARIABLE _rc
    TIMEOUT 30
)
unset(ENV{PONY_MUTANT_ID})
if(NOT _rc EQUAL 1)
    message(FATAL_ERROR "mutated run exited ${_rc}, expected 1")
endif()

message(STATUS "PASS: AOR mutant ${_mutant_id} (add->sub) exits 1")

# --- RVR: find the return->0 mutant from _compute in main.pony ---
execute_process(
    COMMAND grep -m1 "main.pony.*rvr.*return.*0" "${_json}"
    OUTPUT_VARIABLE _rvr_line
    RESULT_VARIABLE _grep_rc
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT _grep_rc EQUAL 0 OR NOT _rvr_line)
    message(FATAL_ERROR "could not find rvr return->0 mutant from test source in JSON")
endif()

string(REGEX REPLACE ".*\"id\": *([0-9]+).*" "\\1" _rvr_id "${_rvr_line}")
if(NOT _rvr_id MATCHES "^[0-9]+$")
    message(FATAL_ERROR "could not extract RVR mutant ID from: ${_rvr_line}")
endif()

message(STATUS "Using RVR mutant ID: ${_rvr_id}")

set(ENV{PONY_MUTANT_ID} "${_rvr_id}")
execute_process(
    COMMAND "${_bin}" --ponynoscale --ponymaxthreads=1
    RESULT_VARIABLE _rc
    TIMEOUT 30
)
unset(ENV{PONY_MUTANT_ID})
if(NOT _rc EQUAL 1)
    message(FATAL_ERROR "RVR mutated run exited ${_rc}, expected 1")
endif()

message(STATUS "PASS: RVR mutant ${_rvr_id} (return->0) exits 1")

# --- VCR: find the call->remove mutant for acc.add in main.pony ---
# Find the line number of "acc.add" in the test source.
execute_process(
    COMMAND grep -n "acc\\.add" "${TEST_SRC}/main.pony"
    OUTPUT_VARIABLE _acc_add_line
    RESULT_VARIABLE _grep_rc
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT _grep_rc EQUAL 0 OR NOT _acc_add_line)
    message(FATAL_ERROR "could not find acc.add in test source")
endif()
string(REGEX REPLACE "^([0-9]+):.*" "\\1" _vcr_lineno "${_acc_add_line}")

# Match the VCR mutant on that line.
execute_process(
    COMMAND grep -m1 "main.pony.*\"line\": ${_vcr_lineno}.*vcr.*call.*remove" "${_json}"
    OUTPUT_VARIABLE _vcr_line
    RESULT_VARIABLE _grep_rc
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT _grep_rc EQUAL 0 OR NOT _vcr_line)
    message(FATAL_ERROR "could not find vcr call->remove mutant on line ${_vcr_lineno} in JSON")
endif()

string(REGEX REPLACE ".*\"id\": *([0-9]+).*" "\\1" _vcr_id "${_vcr_line}")
if(NOT _vcr_id MATCHES "^[0-9]+$")
    message(FATAL_ERROR "could not extract VCR mutant ID from: ${_vcr_line}")
endif()

message(STATUS "Using VCR mutant ID: ${_vcr_id}")

set(ENV{PONY_MUTANT_ID} "${_vcr_id}")
execute_process(
    COMMAND "${_bin}" --ponynoscale --ponymaxthreads=1
    RESULT_VARIABLE _rc
    TIMEOUT 30
)
unset(ENV{PONY_MUTANT_ID})
if(NOT _rc EQUAL 1)
    message(FATAL_ERROR "VCR mutated run exited ${_rc}, expected 1")
endif()

message(STATUS "PASS: VCR mutant ${_vcr_id} (call->remove) exits 1")
