file(REMOVE_RECURSE "${OUTPUT}")
file(MAKE_DIRECTORY "${OUTPUT}")
set(_exe "")
if(WIN32)
    set(_exe ".exe")
endif()

function(compile_case name)
    execute_process(
        COMMAND "${COMPILER}" ${ARGN} -b "${name}" --output "${OUTPUT}" "${TEST_SRC}"
        RESULT_VARIABLE _rc
        OUTPUT_VARIABLE _out
        ERROR_VARIABLE _err
        TIMEOUT 120
    )
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "${name} failed to compile: ${_rc}\n${_out}\n${_err}")
    endif()
endfunction()

function(run_case name mutant expected)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env "PONY_MUTANT_ID=${mutant}"
            "${OUTPUT}/${name}${_exe}" --ponymaxthreads=1
        RESULT_VARIABLE _rc
        OUTPUT_VARIABLE _out
        ERROR_VARIABLE _err
        OUTPUT_STRIP_TRAILING_WHITESPACE
        TIMEOUT 30
    )
    string(REPLACE "\r\n" "\n" _out "${_out}")
    if(NOT _rc EQUAL 0 OR NOT _out STREQUAL expected)
        message(FATAL_ERROR
            "${name} mutant ${mutant}: exit ${_rc}, output '${_out}',"
            " expected '${expected}'\n${_err}")
    endif()
endfunction()

compile_case(ordinary)
compile_case(instrumented --mutate --mutate-scope=_donor.pony --mutate-operators=aor)
run_case(ordinary 0 "true\nfalse\n5\n5\n6")
run_case(instrumented 0 "true\nfalse\n5\n5\n6")

file(READ "${OUTPUT}/instrumented.mutants.json" _json)
string(JSON _count LENGTH "${_json}" mutants)
if(NOT _count EQUAL 2)
    message(FATAL_ERROR "expected only the unmarked closure's mutants: ${_json}")
endif()
set(_result_sub 2)
set(_result_mul 8)
foreach(_index RANGE 0 1)
    string(JSON _replacement GET "${_json}" mutants ${_index} replacement)
    if(NOT DEFINED _result_${_replacement})
        message(FATAL_ERROR "unexpected replacement: ${_json}")
    endif()
    string(JSON _id GET "${_json}" mutants ${_index} id)
    run_case(instrumented "${_id}" "true\nfalse\n5\n5\n${_result_${_replacement}}")
    unset(_result_${_replacement})
endforeach()
