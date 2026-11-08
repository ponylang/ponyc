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

# --- AMR: verify wrapping add records an amr mutant ---
execute_process(
    COMMAND grep -m1 "main.pony.*amr.*add.*add_unsafe" "${_json}"
    OUTPUT_VARIABLE _amr_line
    RESULT_VARIABLE _grep_rc
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT _grep_rc EQUAL 0 OR NOT _amr_line)
    message(FATAL_ERROR "could not find amr add->add_unsafe mutant in JSON")
endif()

string(REGEX REPLACE ".*\"id\": *([0-9]+).*" "\\1" _amr_id "${_amr_line}")
if(NOT _amr_id MATCHES "^[0-9]+$")
    message(FATAL_ERROR "could not extract AMR mutant ID from: ${_amr_line}")
endif()

message(STATUS "PASS: AMR mutant ${_amr_id} (add->add_unsafe) recorded in JSON")

# --- CR: find the n->n+1 mutant for the literal 5 in "result == 5" ---
# Find the line number of "result == 5" in the test source.
execute_process(
    COMMAND grep -n "result == 5 then" "${TEST_SRC}/main.pony"
    OUTPUT_VARIABLE _cr_src_line
    RESULT_VARIABLE _grep_rc
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT _grep_rc EQUAL 0 OR NOT _cr_src_line)
    message(FATAL_ERROR "could not find 'result == 5' in test source")
endif()
string(REGEX REPLACE "^([0-9]+):.*" "\\1" _cr_lineno "${_cr_src_line}")

# Match the CR mutant (n->n+1) on that line.
execute_process(
    COMMAND grep -m1 "main.pony.*\"line\": ${_cr_lineno}.*cr.*n->n.1" "${_json}"
    OUTPUT_VARIABLE _cr_line
    RESULT_VARIABLE _grep_rc
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT _grep_rc EQUAL 0 OR NOT _cr_line)
    message(FATAL_ERROR
        "could not find cr n->n+1 mutant on line ${_cr_lineno} in JSON")
endif()

string(REGEX REPLACE ".*\"id\": *([0-9]+).*" "\\1" _cr_id "${_cr_line}")
if(NOT _cr_id MATCHES "^[0-9]+$")
    message(FATAL_ERROR "could not extract CR mutant ID from: ${_cr_line}")
endif()

message(STATUS "Using CR mutant ID: ${_cr_id}")

set(ENV{PONY_MUTANT_ID} "${_cr_id}")
execute_process(
    COMMAND "${_bin}" --ponynoscale --ponymaxthreads=1
    RESULT_VARIABLE _rc
    TIMEOUT 30
)
unset(ENV{PONY_MUTANT_ID})
if(NOT _rc EQUAL 1)
    message(FATAL_ERROR "CR mutated run exited ${_rc}, expected 1")
endif()

message(STATUS "PASS: CR mutant ${_cr_id} (n->n+1) exits 1")

# --- SOR: find the shl->shr mutant for "a << b" ---
execute_process(
    COMMAND grep -n "a << b" "${TEST_SRC}/main.pony"
    OUTPUT_VARIABLE _sor_src_line
    RESULT_VARIABLE _grep_rc
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT _grep_rc EQUAL 0 OR NOT _sor_src_line)
    message(FATAL_ERROR "could not find 'a << b' in test source")
endif()
string(REGEX REPLACE "^([0-9]+):.*" "\\1" _sor_lineno "${_sor_src_line}")

execute_process(
    COMMAND grep -m1 "main.pony.*\"line\": ${_sor_lineno}.*sor.*shl.*shr" "${_json}"
    OUTPUT_VARIABLE _sor_line
    RESULT_VARIABLE _grep_rc
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT _grep_rc EQUAL 0 OR NOT _sor_line)
    message(FATAL_ERROR
        "could not find sor shl->shr mutant on line ${_sor_lineno} in JSON")
endif()

string(REGEX REPLACE ".*\"id\": *([0-9]+).*" "\\1" _sor_id "${_sor_line}")
if(NOT _sor_id MATCHES "^[0-9]+$")
    message(FATAL_ERROR "could not extract SOR mutant ID from: ${_sor_line}")
endif()

message(STATUS "Using SOR mutant ID: ${_sor_id}")

set(ENV{PONY_MUTANT_ID} "${_sor_id}")
execute_process(
    COMMAND "${_bin}" --ponynoscale --ponymaxthreads=1
    RESULT_VARIABLE _rc
    TIMEOUT 30
)
unset(ENV{PONY_MUTANT_ID})
if(NOT _rc EQUAL 1)
    message(FATAL_ERROR "SOR mutated run exited ${_rc}, expected 1")
endif()

message(STATUS "PASS: SOR mutant ${_sor_id} (shl->shr) exits 1")

# --- BOR: find the and->or mutant for "0xFF and 0x0F" ---
execute_process(
    COMMAND grep -n "0xFF) and" "${TEST_SRC}/main.pony"
    OUTPUT_VARIABLE _bor_src_line
    RESULT_VARIABLE _grep_rc
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT _grep_rc EQUAL 0 OR NOT _bor_src_line)
    message(FATAL_ERROR "could not find '0xFF) and' in test source")
endif()
string(REGEX REPLACE "^([0-9]+):.*" "\\1" _bor_lineno "${_bor_src_line}")

execute_process(
    COMMAND grep -m1 "main.pony.*\"line\": ${_bor_lineno}.*bor.*and.*or" "${_json}"
    OUTPUT_VARIABLE _bor_line
    RESULT_VARIABLE _grep_rc
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT _grep_rc EQUAL 0 OR NOT _bor_line)
    message(FATAL_ERROR
        "could not find bor and->or mutant on line ${_bor_lineno} in JSON")
endif()

string(REGEX REPLACE ".*\"id\": *([0-9]+).*" "\\1" _bor_id "${_bor_line}")
if(NOT _bor_id MATCHES "^[0-9]+$")
    message(FATAL_ERROR "could not extract BOR mutant ID from: ${_bor_line}")
endif()

message(STATUS "Using BOR mutant ID: ${_bor_id}")

set(ENV{PONY_MUTANT_ID} "${_bor_id}")
execute_process(
    COMMAND "${_bin}" --ponynoscale --ponymaxthreads=1
    RESULT_VARIABLE _rc
    TIMEOUT 30
)
unset(ENV{PONY_MUTANT_ID})
if(NOT _rc EQUAL 1)
    message(FATAL_ERROR "BOR mutated run exited ${_rc}, expected 1")
endif()

message(STATUS "PASS: BOR mutant ${_bor_id} (and->or) exits 1")

# --- ROR: find a comparison mutant for "a > b" ---
execute_process(
    COMMAND grep -n "a > b" "${TEST_SRC}/main.pony"
    OUTPUT_VARIABLE _ror_src_line
    RESULT_VARIABLE _grep_rc
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT _grep_rc EQUAL 0 OR NOT _ror_src_line)
    message(FATAL_ERROR "could not find 'a > b' in test source")
endif()
string(REGEX REPLACE "^([0-9]+):.*" "\\1" _ror_lineno "${_ror_src_line}")

execute_process(
    COMMAND grep -m1 "main.pony.*\"line\": ${_ror_lineno}.*ror.*gt.*\"lt\"" "${_json}"
    OUTPUT_VARIABLE _ror_line
    RESULT_VARIABLE _grep_rc
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT _grep_rc EQUAL 0 OR NOT _ror_line)
    message(FATAL_ERROR
        "could not find ror mutant on line ${_ror_lineno} in JSON")
endif()

string(REGEX REPLACE ".*\"id\": *([0-9]+).*" "\\1" _ror_id "${_ror_line}")
if(NOT _ror_id MATCHES "^[0-9]+$")
    message(FATAL_ERROR "could not extract ROR mutant ID from: ${_ror_line}")
endif()

message(STATUS "Using ROR mutant ID: ${_ror_id}")

set(ENV{PONY_MUTANT_ID} "${_ror_id}")
execute_process(
    COMMAND "${_bin}" --ponynoscale --ponymaxthreads=1
    RESULT_VARIABLE _rc
    TIMEOUT 30
)
unset(ENV{PONY_MUTANT_ID})
if(NOT _rc EQUAL 1)
    message(FATAL_ERROR "ROR mutated run exited ${_rc}, expected 1")
endif()

message(STATUS "PASS: ROR mutant ${_ror_id} exits 1")

# --- CR arithmetic: find a cr mutant for "a + 10" ---
execute_process(
    COMMAND grep -n "let cr_arith" "${TEST_SRC}/main.pony"
    OUTPUT_VARIABLE _cr_arith_src
    RESULT_VARIABLE _grep_rc
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT _grep_rc EQUAL 0 OR NOT _cr_arith_src)
    message(FATAL_ERROR "could not find 'let cr_arith' in test source")
endif()
string(REGEX REPLACE "^([0-9]+):.*" "\\1" _cr_arith_lineno "${_cr_arith_src}")

execute_process(
    COMMAND grep -m1 "main.pony.*\"line\": ${_cr_arith_lineno}.*cr.*n->n.1" "${_json}"
    OUTPUT_VARIABLE _cr_arith_line
    RESULT_VARIABLE _grep_rc
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT _grep_rc EQUAL 0 OR NOT _cr_arith_line)
    message(FATAL_ERROR
        "could not find cr mutant on line ${_cr_arith_lineno} in JSON")
endif()

string(REGEX REPLACE ".*\"id\": *([0-9]+).*" "\\1" _cr_arith_id "${_cr_arith_line}")

set(ENV{PONY_MUTANT_ID} "${_cr_arith_id}")
execute_process(
    COMMAND "${_bin}" --ponynoscale --ponymaxthreads=1
    RESULT_VARIABLE _rc
    TIMEOUT 30
)
unset(ENV{PONY_MUTANT_ID})
if(NOT _rc EQUAL 1)
    message(FATAL_ERROR "CR arithmetic mutated run exited ${_rc}, expected 1")
endif()

message(STATUS "PASS: CR arithmetic mutant ${_cr_arith_id} exits 1")

# --- CR divrem: find a cr mutant for "a % 3" ---
execute_process(
    COMMAND grep -n "let cr_rem" "${TEST_SRC}/main.pony"
    OUTPUT_VARIABLE _cr_div_src
    RESULT_VARIABLE _grep_rc
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT _grep_rc EQUAL 0 OR NOT _cr_div_src)
    message(FATAL_ERROR "could not find 'let cr_rem' in test source")
endif()
string(REGEX REPLACE "^([0-9]+):.*" "\\1" _cr_div_lineno "${_cr_div_src}")

execute_process(
    COMMAND grep -m1 "main.pony.*\"line\": ${_cr_div_lineno}.*cr.*n->n.1" "${_json}"
    OUTPUT_VARIABLE _cr_div_line
    RESULT_VARIABLE _grep_rc
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT _grep_rc EQUAL 0 OR NOT _cr_div_line)
    message(FATAL_ERROR
        "could not find cr mutant on line ${_cr_div_lineno} in JSON")
endif()

string(REGEX REPLACE ".*\"id\": *([0-9]+).*" "\\1" _cr_div_id "${_cr_div_line}")

set(ENV{PONY_MUTANT_ID} "${_cr_div_id}")
execute_process(
    COMMAND "${_bin}" --ponynoscale --ponymaxthreads=1
    RESULT_VARIABLE _rc
    TIMEOUT 30
)
unset(ENV{PONY_MUTANT_ID})
if(NOT _rc EQUAL 1)
    message(FATAL_ERROR "CR divrem mutated run exited ${_rc}, expected 1")
endif()

message(STATUS "PASS: CR divrem mutant ${_cr_div_id} exits 1")

# --- CR shift: find a cr mutant for "a << 3" ---
execute_process(
    COMMAND grep -n "let cr_shift" "${TEST_SRC}/main.pony"
    OUTPUT_VARIABLE _cr_shift_src
    RESULT_VARIABLE _grep_rc
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT _grep_rc EQUAL 0 OR NOT _cr_shift_src)
    message(FATAL_ERROR "could not find 'let cr_shift' in test source")
endif()
string(REGEX REPLACE "^([0-9]+):.*" "\\1" _cr_shift_lineno "${_cr_shift_src}")

execute_process(
    COMMAND grep -m1 "main.pony.*\"line\": ${_cr_shift_lineno}.*cr.*n->n.1" "${_json}"
    OUTPUT_VARIABLE _cr_shift_line
    RESULT_VARIABLE _grep_rc
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT _grep_rc EQUAL 0 OR NOT _cr_shift_line)
    message(FATAL_ERROR
        "could not find cr mutant on line ${_cr_shift_lineno} in JSON")
endif()

string(REGEX REPLACE ".*\"id\": *([0-9]+).*" "\\1" _cr_shift_id "${_cr_shift_line}")

set(ENV{PONY_MUTANT_ID} "${_cr_shift_id}")
execute_process(
    COMMAND "${_bin}" --ponynoscale --ponymaxthreads=1
    RESULT_VARIABLE _rc
    TIMEOUT 30
)
unset(ENV{PONY_MUTANT_ID})
if(NOT _rc EQUAL 1)
    message(FATAL_ERROR "CR shift mutated run exited ${_rc}, expected 1")
endif()

message(STATUS "PASS: CR shift mutant ${_cr_shift_id} exits 1")

# --- CR bitwise: find a cr mutant for "a and 0x07" ---
execute_process(
    COMMAND grep -n "let cr_bit" "${TEST_SRC}/main.pony"
    OUTPUT_VARIABLE _cr_bit_src
    RESULT_VARIABLE _grep_rc
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT _grep_rc EQUAL 0 OR NOT _cr_bit_src)
    message(FATAL_ERROR "could not find 'let cr_bit' in test source")
endif()
string(REGEX REPLACE "^([0-9]+):.*" "\\1" _cr_bit_lineno "${_cr_bit_src}")

execute_process(
    COMMAND grep -m1 "main.pony.*\"line\": ${_cr_bit_lineno}.*cr.*n->n.1" "${_json}"
    OUTPUT_VARIABLE _cr_bit_line
    RESULT_VARIABLE _grep_rc
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT _grep_rc EQUAL 0 OR NOT _cr_bit_line)
    message(FATAL_ERROR
        "could not find cr mutant on line ${_cr_bit_lineno} in JSON")
endif()

string(REGEX REPLACE ".*\"id\": *([0-9]+).*" "\\1" _cr_bit_id "${_cr_bit_line}")

set(ENV{PONY_MUTANT_ID} "${_cr_bit_id}")
execute_process(
    COMMAND "${_bin}" --ponynoscale --ponymaxthreads=1
    RESULT_VARIABLE _rc
    TIMEOUT 30
)
unset(ENV{PONY_MUTANT_ID})
if(NOT _rc EQUAL 1)
    message(FATAL_ERROR "CR bitwise mutated run exited ${_rc}, expected 1")
endif()

message(STATUS "PASS: CR bitwise mutant ${_cr_bit_id} exits 1")

# --- CR neg: find a cr mutant for "-7" ---
execute_process(
    COMMAND grep -n "let cr_neg" "${TEST_SRC}/main.pony"
    OUTPUT_VARIABLE _cr_neg_src
    RESULT_VARIABLE _grep_rc
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT _grep_rc EQUAL 0 OR NOT _cr_neg_src)
    message(FATAL_ERROR "could not find 'let cr_neg' in test source")
endif()
string(REGEX REPLACE "^([0-9]+):.*" "\\1" _cr_neg_lineno "${_cr_neg_src}")

execute_process(
    COMMAND grep -m1 "main.pony.*\"line\": ${_cr_neg_lineno}.*cr.*n->n.1" "${_json}"
    OUTPUT_VARIABLE _cr_neg_line
    RESULT_VARIABLE _grep_rc
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT _grep_rc EQUAL 0 OR NOT _cr_neg_line)
    message(FATAL_ERROR
        "could not find cr mutant on line ${_cr_neg_lineno} in JSON")
endif()

string(REGEX REPLACE ".*\"id\": *([0-9]+).*" "\\1" _cr_neg_id "${_cr_neg_line}")

set(ENV{PONY_MUTANT_ID} "${_cr_neg_id}")
execute_process(
    COMMAND "${_bin}" --ponynoscale --ponymaxthreads=1
    RESULT_VARIABLE _rc
    TIMEOUT 30
)
unset(ENV{PONY_MUTANT_ID})
if(NOT _rc EQUAL 1)
    message(FATAL_ERROR "CR neg mutated run exited ${_rc}, expected 1")
endif()

message(STATUS "PASS: CR neg mutant ${_cr_neg_id} exits 1")

# --- CR not: find a cr mutant for "not U64(0x0F)" ---
execute_process(
    COMMAND grep -n "let cr_not" "${TEST_SRC}/main.pony"
    OUTPUT_VARIABLE _cr_not_src
    RESULT_VARIABLE _grep_rc
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT _grep_rc EQUAL 0 OR NOT _cr_not_src)
    message(FATAL_ERROR "could not find 'let cr_not' in test source")
endif()
string(REGEX REPLACE "^([0-9]+):.*" "\\1" _cr_not_lineno "${_cr_not_src}")

execute_process(
    COMMAND grep -m1 "main.pony.*\"line\": ${_cr_not_lineno}.*cr.*n->n.1" "${_json}"
    OUTPUT_VARIABLE _cr_not_line
    RESULT_VARIABLE _grep_rc
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT _grep_rc EQUAL 0 OR NOT _cr_not_line)
    message(FATAL_ERROR
        "could not find cr mutant on line ${_cr_not_lineno} in JSON")
endif()

string(REGEX REPLACE ".*\"id\": *([0-9]+).*" "\\1" _cr_not_id "${_cr_not_line}")

set(ENV{PONY_MUTANT_ID} "${_cr_not_id}")
execute_process(
    COMMAND "${_bin}" --ponynoscale --ponymaxthreads=1
    RESULT_VARIABLE _rc
    TIMEOUT 30
)
unset(ENV{PONY_MUTANT_ID})
if(NOT _rc EQUAL 1)
    message(FATAL_ERROR "CR not mutated run exited ${_rc}, expected 1")
endif()

message(STATUS "PASS: CR not mutant ${_cr_not_id} exits 1")

# --- CR wide literal: find a cr mutant for "small + 0x1_0000..." ---
execute_process(
    COMMAND grep -n "let cr_wide" "${TEST_SRC}/main.pony"
    OUTPUT_VARIABLE _cr_wide_src
    RESULT_VARIABLE _grep_rc
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT _grep_rc EQUAL 0 OR NOT _cr_wide_src)
    message(FATAL_ERROR "could not find 'let cr_wide' in test source")
endif()
string(REGEX REPLACE "^([0-9]+):.*" "\\1" _cr_wide_lineno "${_cr_wide_src}")

execute_process(
    COMMAND grep -m1 "main.pony.*\"line\": ${_cr_wide_lineno}.*cr.*n->n.1" "${_json}"
    OUTPUT_VARIABLE _cr_wide_line
    RESULT_VARIABLE _grep_rc
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT _grep_rc EQUAL 0 OR NOT _cr_wide_line)
    message(FATAL_ERROR
        "could not find cr mutant on line ${_cr_wide_lineno} in JSON")
endif()

string(REGEX REPLACE ".*\"id\": *([0-9]+).*" "\\1" _cr_wide_id "${_cr_wide_line}")

set(ENV{PONY_MUTANT_ID} "${_cr_wide_id}")
execute_process(
    COMMAND "${_bin}" --ponynoscale --ponymaxthreads=1
    RESULT_VARIABLE _rc
    TIMEOUT 30
)
unset(ENV{PONY_MUTANT_ID})
if(NOT _rc EQUAL 1)
    message(FATAL_ERROR "CR wide literal mutated run exited ${_rc}, expected 1")
endif()

message(STATUS "PASS: CR wide literal mutant ${_cr_wide_id} exits 1")
