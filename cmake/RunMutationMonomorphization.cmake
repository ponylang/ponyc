file(REMOVE_RECURSE "${OUTPUT}")
file(MAKE_DIRECTORY "${OUTPUT}")
set(_bin "${OUTPUT}/mutation-monomorphization")
if(WIN32)
    set(_bin "${_bin}.exe")
endif()

execute_process(
    COMMAND "${COMPILER}" --mutate --mutate-operators=aor,uoi
        --mutate-scope=_sum.pony --mutate-scope=_default.pony
        -b mutation-monomorphization
        --output "${OUTPUT}" "${TEST_SRC}"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _err
    TIMEOUT 120
)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "compilation failed: ${_rc}\n${_out}\n${_err}")
endif()
file(READ "${OUTPUT}/mutation-monomorphization.mutants.json" _json)
string(JSON _count LENGTH "${_json}" mutants)
math(EXPR _last "${_count} - 1")
set(_default_ids "")
foreach(_index RANGE 0 ${_last})
    foreach(_field id category replacement shared coverage_counter)
        string(JSON _${_field} GET "${_json}" mutants ${_index} ${_field})
    endforeach()
    if(_shared AND _category STREQUAL "uoi")
        set(_${_replacement}_id "${_id}")
        set(_${_replacement}_counter "${_coverage_counter}")
    elseif(NOT _shared AND _category STREQUAL "aor"
            AND _replacement STREQUAL "sub")
        list(APPEND _default_ids "${_id}")
    endif()
endforeach()
if(NOT DEFINED _neg_id OR NOT DEFINED _complement_id)
    message(FATAL_ERROR "missing shared generic replacements: ${_json}")
endif()
if(_neg_counter EQUAL _complement_counter)
    message(FATAL_ERROR "generic replacements share coverage")
endif()
list(LENGTH _default_ids _defaults_count)
if(NOT _defaults_count EQUAL 2)
    message(FATAL_ERROR "expected two independent default copies: ${_json}")
endif()

function(run_case mode mutant expected)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env "PONY_MUTANT_ID=${mutant}"
            "${_bin}" --ponynoscale --ponymaxthreads=1 "${mode}"
        RESULT_VARIABLE _rc
        OUTPUT_VARIABLE _out
        ERROR_VARIABLE _err
        OUTPUT_STRIP_TRAILING_WHITESPACE
        TIMEOUT 30
    )
    string(REPLACE "\r\n" "\n" _out "${_out}")
    if(NOT _rc EQUAL 0 OR NOT _out STREQUAL expected)
        message(FATAL_ERROR
            "${mode} mutant ${mutant}: exit ${_rc}, output '${_out}',"
            " expected '${expected}'\n${_err}")
    endif()
endfunction()

function(check_coverage counter reached)
    file(READ "${_bin}.coverage" _coverage HEX)
    math(EXPR _offset "(4 + ${counter} * 8) * 2")
    string(SUBSTRING "${_coverage}" ${_offset} 16 _value)
    if(reached AND _value STREQUAL "0000000000000000")
        message(FATAL_ERROR "counter ${counter} was not reached")
    elseif(NOT reached AND NOT _value STREQUAL "0000000000000000")
        message(FATAL_ERROR "counter ${counter} was unexpectedly reached")
    endif()
endfunction()

run_case(float 0 "5")
check_coverage(${_neg_counter} TRUE)
check_coverage(${_complement_counter} FALSE)
run_case(integer 0 "5")
check_coverage(${_neg_counter} TRUE)
check_coverage(${_complement_counter} TRUE)
run_case(float ${_neg_id} "-5")
run_case(integer ${_neg_id} "-5")
run_case(integer ${_complement_id} "-6")
run_case(defaults 0 "5\n5")
list(GET _default_ids 0 _first_id)
list(GET _default_ids 1 _second_id)
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "PONY_MUTANT_ID=${_first_id}"
        "${_bin}" --ponynoscale --ponymaxthreads=1 defaults
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    OUTPUT_STRIP_TRAILING_WHITESPACE
    TIMEOUT 30
)
string(REPLACE "\r\n" "\n" _out "${_out}")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "first default mutant failed: ${_rc}")
endif()
if(_out STREQUAL "1\n5")
    run_case(defaults ${_second_id} "5\n1")
elseif(_out STREQUAL "5\n1")
    run_case(defaults ${_second_id} "1\n5")
else()
    message(FATAL_ERROR "default mutant changed wrong copies: '${_out}'")
endif()
message(STATUS
    "PASS: shared selection, applicable coverage, independent defaults")
