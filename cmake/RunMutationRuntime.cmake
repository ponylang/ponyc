# ctest wrapper: verify PONY_MUTANT_ID runtime behavior.
#
# Compiles a Pony program with --mutate, runs it without PONY_MUTANT_ID
# (expecting exit 0), finds the add->sub mutant from the test source in the
# JSON, and runs again with that ID set (expecting exit 1).
#
# Args (passed with -D): COMPILER, OUTPUT, TEST_SRC.

file(REMOVE_RECURSE "${OUTPUT}")
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
# Pin to _compute's line so another RVR record can't match first.
execute_process(
    COMMAND grep -n "fun _compute" "${TEST_SRC}/main.pony"
    OUTPUT_VARIABLE _compute_src
    RESULT_VARIABLE _grep_rc
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT _grep_rc EQUAL 0 OR NOT _compute_src)
    message(FATAL_ERROR "could not find 'fun _compute' in test source")
endif()
string(REGEX REPLACE "^([0-9]+):.*" "\\1" _compute_lineno "${_compute_src}")
# The return site is on the line after the fun declaration.
math(EXPR _rvr_lineno "${_compute_lineno} + 1")

execute_process(
    COMMAND grep -m1 "main.pony.*\"line\": ${_rvr_lineno}.*rvr.*return.*\"replacement\": \"0\"" "${_json}"
    OUTPUT_VARIABLE _rvr_line
    RESULT_VARIABLE _grep_rc
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT _grep_rc EQUAL 0 OR NOT _rvr_line)
    message(FATAL_ERROR "could not find rvr return->0 mutant on line ${_rvr_lineno} in JSON")
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
    OUTPUT_VARIABLE _cr_arith_out
    TIMEOUT 30
)
unset(ENV{PONY_MUTANT_ID})
if(NOT _rc EQUAL 1)
    message(FATAL_ERROR "CR arithmetic mutated run exited ${_rc}, expected 1")
endif()
string(FIND "${_cr_arith_out}" "cr_arith=14" _pos)
if(_pos EQUAL -1)
    message(FATAL_ERROR
        "CR arithmetic mutant did not compute expected value 14 (n+1: 3+11)\nstdout: ${_cr_arith_out}")
endif()

message(STATUS "PASS: CR arithmetic mutant ${_cr_arith_id} computes 14")

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
    OUTPUT_VARIABLE _cr_div_out
    TIMEOUT 30
)
unset(ENV{PONY_MUTANT_ID})
if(NOT _rc EQUAL 1)
    message(FATAL_ERROR "CR divrem mutated run exited ${_rc}, expected 1")
endif()
string(FIND "${_cr_div_out}" "cr_rem=3\n" _pos)
if(_pos EQUAL -1)
    message(FATAL_ERROR
        "CR divrem mutant did not compute expected value 3 (n+1: 3%4)\nstdout: ${_cr_div_out}")
endif()

message(STATUS "PASS: CR divrem mutant ${_cr_div_id} computes 3")

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
    OUTPUT_VARIABLE _cr_shift_out
    TIMEOUT 30
)
unset(ENV{PONY_MUTANT_ID})
if(NOT _rc EQUAL 1)
    message(FATAL_ERROR "CR shift mutated run exited ${_rc}, expected 1")
endif()
string(FIND "${_cr_shift_out}" "cr_shift=48" _pos)
if(_pos EQUAL -1)
    message(FATAL_ERROR
        "CR shift mutant did not compute expected value 48 (n+1: 3<<4)\nstdout: ${_cr_shift_out}")
endif()

message(STATUS "PASS: CR shift mutant ${_cr_shift_id} computes 48")

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
    OUTPUT_VARIABLE _cr_bit_out
    TIMEOUT 30
)
unset(ENV{PONY_MUTANT_ID})
if(NOT _rc EQUAL 1)
    message(FATAL_ERROR "CR bitwise mutated run exited ${_rc}, expected 1")
endif()
string(FIND "${_cr_bit_out}" "cr_bit=0\n" _pos)
if(_pos EQUAL -1)
    message(FATAL_ERROR
        "CR bitwise mutant did not compute expected value 0 (n+1: 3 and 8)\nstdout: ${_cr_bit_out}")
endif()

message(STATUS "PASS: CR bitwise mutant ${_cr_bit_id} computes 0")

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
    OUTPUT_VARIABLE _cr_neg_out
    TIMEOUT 30
)
unset(ENV{PONY_MUTANT_ID})
if(NOT _rc EQUAL 1)
    message(FATAL_ERROR "CR neg mutated run exited ${_rc}, expected 1")
endif()
string(FIND "${_cr_neg_out}" "cr_neg=-8" _pos)
if(_pos EQUAL -1)
    message(FATAL_ERROR
        "CR neg mutant did not compute expected value -8 (n+1: -8)\nstdout: ${_cr_neg_out}")
endif()

message(STATUS "PASS: CR neg mutant ${_cr_neg_id} computes -8")

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
    OUTPUT_VARIABLE _cr_not_out
    TIMEOUT 30
)
unset(ENV{PONY_MUTANT_ID})
if(NOT _rc EQUAL 1)
    message(FATAL_ERROR "CR not mutated run exited ${_rc}, expected 1")
endif()
string(FIND "${_cr_not_out}" "cr_not=3\n" _pos)
if(_pos EQUAL -1)
    message(FATAL_ERROR
        "CR not mutant did not compute expected value 3 (n+1: 3 and ~16)\nstdout: ${_cr_not_out}")
endif()

message(STATUS "PASS: CR not mutant ${_cr_not_id} computes 3")

# --- CR U8 boundary: find a cr mutant for "y_u8 + 255" ---
# n+1 wraps 255 to 0 via LLVMConstInt truncation, so 1+0=1 instead of 1+255=0.
execute_process(
    COMMAND grep -n "let cr_u8: U8" "${TEST_SRC}/main.pony"
    OUTPUT_VARIABLE _cr_u8_src
    RESULT_VARIABLE _grep_rc
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT _grep_rc EQUAL 0 OR NOT _cr_u8_src)
    message(FATAL_ERROR "could not find 'let cr_u8: U8' in test source")
endif()
string(REGEX REPLACE "^([0-9]+):.*" "\\1" _cr_u8_lineno "${_cr_u8_src}")

execute_process(
    COMMAND grep -m1 "main.pony.*\"line\": ${_cr_u8_lineno}.*cr.*n->n[+]1" "${_json}"
    OUTPUT_VARIABLE _cr_u8_line
    RESULT_VARIABLE _grep_rc
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT _grep_rc EQUAL 0 OR NOT _cr_u8_line)
    message(FATAL_ERROR
        "could not find cr mutant on line ${_cr_u8_lineno} in JSON")
endif()

string(REGEX REPLACE ".*\"id\": *([0-9]+).*" "\\1" _cr_u8_id "${_cr_u8_line}")

set(ENV{PONY_MUTANT_ID} "${_cr_u8_id}")
execute_process(
    COMMAND "${_bin}" --ponynoscale --ponymaxthreads=1
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _cr_u8_out
    TIMEOUT 30
)
unset(ENV{PONY_MUTANT_ID})
if(NOT _rc EQUAL 1)
    message(FATAL_ERROR "CR U8 boundary mutated run exited ${_rc}, expected 1")
endif()
string(FIND "${_cr_u8_out}" "cr_u8=1" _pos)
if(_pos EQUAL -1)
    message(FATAL_ERROR
        "CR U8 boundary mutant did not compute expected value 1 (n+1 wraps 255->0: 1+0=1)\nstdout: ${_cr_u8_out}")
endif()

message(STATUS "PASS: CR U8 boundary mutant ${_cr_u8_id} (n+1 wraps 255->0) computes 1")

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
    OUTPUT_VARIABLE _cr_wide_out
    TIMEOUT 30
)
unset(ENV{PONY_MUTANT_ID})
if(NOT _rc EQUAL 1)
    message(FATAL_ERROR "CR wide literal mutated run exited ${_rc}, expected 1")
endif()
# n+1: 0x1_0000_0000_0000_000B = 2^64+11, result = 3 + 2^64+11 = 18446744073709551630
# If replacement were 64-bit truncated, result would be 3+11=14.
string(FIND "${_cr_wide_out}" "cr_wide=18446744073709551630" _pos)
if(_pos EQUAL -1)
    message(FATAL_ERROR
        "CR wide literal mutant did not compute expected value 18446744073709551630\nstdout: ${_cr_wide_out}")
endif()

message(STATUS "PASS: CR wide literal mutant ${_cr_wide_id} computes 18446744073709551630")

# --- Helper macro: find a BOR mutant by source grep and JSON pattern, run it,
# check exit code and expected stdout string (with trailing \n). ---
macro(bor_test_row _label _src_grep _json_cat _json_orig _json_repl _expect_exit _expect_str)
    execute_process(
        COMMAND grep -n "${_src_grep}" "${TEST_SRC}/main.pony"
        OUTPUT_VARIABLE _row_src
        RESULT_VARIABLE _grep_rc
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NOT _grep_rc EQUAL 0 OR NOT _row_src)
        message(FATAL_ERROR "${_label}: could not find '${_src_grep}' in test source")
    endif()
    string(REGEX REPLACE "^([0-9]+):.*" "\\1" _row_lineno "${_row_src}")

    execute_process(
        COMMAND grep -m1 "main.pony.*\"line\": ${_row_lineno}.*${_json_cat}.*${_json_orig}.*${_json_repl}" "${_json}"
        OUTPUT_VARIABLE _row_json
        RESULT_VARIABLE _grep_rc
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NOT _grep_rc EQUAL 0 OR NOT _row_json)
        message(FATAL_ERROR "${_label}: could not find ${_json_cat} ${_json_orig}->${_json_repl} mutant on line ${_row_lineno} in JSON")
    endif()

    string(REGEX REPLACE ".*\"id\": *([0-9]+).*" "\\1" _row_id "${_row_json}")
    if(NOT _row_id MATCHES "^[0-9]+$")
        message(FATAL_ERROR "${_label}: could not extract mutant ID from: ${_row_json}")
    endif()

    set(ENV{PONY_MUTANT_ID} "${_row_id}")
    execute_process(
        COMMAND "${_bin}" --ponynoscale --ponymaxthreads=1
        RESULT_VARIABLE _rc
        OUTPUT_VARIABLE _row_out
        TIMEOUT 30
    )
    unset(ENV{PONY_MUTANT_ID})
    if(NOT _rc EQUAL ${_expect_exit})
        message(FATAL_ERROR "${_label}: mutated run exited ${_rc}, expected ${_expect_exit}")
    endif()
    string(FIND "${_row_out}" "${_expect_str}\n" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "${_label}: expected '${_expect_str}' not found in stdout:\n${_row_out}")
    endif()

    message(STATUS "PASS: ${_label} mutant ${_row_id}")
endmacro()

# Row 1: atrue and h1.f(false) — and->or: true,false -> true,false
bor_test_row("bor1-and->or" "let r1 = atrue and" "bor" "and" "or" 1 "bor1=true,false")
# Row 2: afalse and h2.f(true) — and->or: false,false -> true,true
bor_test_row("bor2-and->or" "let r2 = afalse and" "bor" "and" "or" 1 "bor2=true,true")
# Row 3: atrue or h3.f(false) — or->and: true,false -> false,true
bor_test_row("bor3-or->and" "let r3 = atrue or" "bor" "or" "and" 1 "bor3=false,true")
# Row 4: afalse or h4.f(true) — or->and: true,true -> false,false
bor_test_row("bor4-or->and" "let r4 = afalse or" "bor" "or" "and" 1 "bor4=false,false")
# Row 5: (p > q) and true — and->or: false -> true (no ran)
bor_test_row("bor5-and->or" "let r5 = " "bor" "and" "or" 1 "bor5=true")
# Row 6: afalse xor h6.f(true) — xor->and: true,true -> false,false
bor_test_row("bor6-xor->and" "let r6 = afalse xor" "bor" "xor" "and" 1 "bor6=false,false")
# Row 7: atrue xor h7.f(true) — xor->and: false,true -> true,true
bor_test_row("bor7-xor->and" "let r7 = atrue xor h7" "bor" "xor" "and" 1 "bor7=true,true")
# Row 8: atrue xor h8.f(true) — xor->or: false,true -> true,false
bor_test_row("bor8-xor->or" "let r8 = atrue xor h8" "bor" "xor" "\"or\"" 1 "bor8=true,false")
# Row 9: try afalse xor h9.g(true)? else false end — xor->and: false,true -> false,false
bor_test_row("bor9-xor->and" "let r9 = try" "bor" "xor" "and" 0 "bor9=false,false")

# --- VCR side-effect bypass: acc2.add(acc2.bump()) ---
# When VCR fires, both the argument (bump) and the call (add) should be
# skipped. Baseline: bump increments to 1, add adds 1 → total=2.
# VCR active: neither runs → total=0, printed as vcr_se=0.
execute_process(
    COMMAND grep -n "acc2\\.add(acc2\\.bump())" "${TEST_SRC}/main.pony"
    OUTPUT_VARIABLE _vcr_se_src
    RESULT_VARIABLE _grep_rc
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT _grep_rc EQUAL 0 OR NOT _vcr_se_src)
    message(FATAL_ERROR "could not find acc2.add(acc2.bump()) in test source")
endif()
string(REGEX REPLACE "^([0-9]+):.*" "\\1" _vcr_se_lineno "${_vcr_se_src}")

execute_process(
    COMMAND grep -m1 "main.pony.*\"line\": ${_vcr_se_lineno}.*vcr.*call.*remove" "${_json}"
    OUTPUT_VARIABLE _vcr_se_line
    RESULT_VARIABLE _grep_rc
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT _grep_rc EQUAL 0 OR NOT _vcr_se_line)
    message(FATAL_ERROR
        "could not find vcr call->remove mutant on line ${_vcr_se_lineno} in JSON")
endif()

string(REGEX REPLACE ".*\"id\": *([0-9]+).*" "\\1" _vcr_se_id "${_vcr_se_line}")
if(NOT _vcr_se_id MATCHES "^[0-9]+$")
    message(FATAL_ERROR "could not extract VCR side-effect mutant ID from: ${_vcr_se_line}")
endif()

message(STATUS "Using VCR side-effect mutant ID: ${_vcr_se_id}")

set(ENV{PONY_MUTANT_ID} "${_vcr_se_id}")
execute_process(
    COMMAND "${_bin}" --ponynoscale --ponymaxthreads=1
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _vcr_se_out
    TIMEOUT 30
)
unset(ENV{PONY_MUTANT_ID})

# VCR mutant should exit 1 (total is 0, not 2).
if(NOT _rc EQUAL 1)
    message(FATAL_ERROR
        "VCR side-effect mutated run exited ${_rc}, expected 1\nstdout: ${_vcr_se_out}")
endif()

# The argument expression (bump) must not have run: total should be 0.
string(FIND "${_vcr_se_out}" "vcr_se=0" _pos)
if(_pos EQUAL -1)
    message(FATAL_ERROR
        "VCR side-effect bypass failed: expected vcr_se=0 (arg not evaluated)\nstdout: ${_vcr_se_out}")
endif()

message(STATUS "PASS: VCR side-effect mutant ${_vcr_se_id} skips arg evaluation (vcr_se=0)")
