# Compiles test/mutation-deferred-object-literal with --mutate and checks the
# mutants listed for its own files: the inherited get's operators and return
# for _Holder, and the print call in Main. The object literal's class, which
# takes get's body after _Donor's check, has none: get's signature comes from
# _Signature, which marks it \nomutate\. A copy of the program without that
# mark must list the same eight mutants of get for the literal's class too.
#
# Inputs: COMPILER, OUTPUT, TEST_SRC.

file(REMOVE_RECURSE "${OUTPUT}")
file(MAKE_DIRECTORY "${OUTPUT}")

execute_process(
    COMMAND "${COMPILER}" --mutate -b deferred --output "${OUTPUT}" "${TEST_SRC}"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _err
    TIMEOUT 120
)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "failed to compile: ${_rc}\n${_out}\n${_err}")
endif()

file(READ "${OUTPUT}/deferred.mutants.json" _json)
string(JSON _count LENGTH "${_json}" mutants)
math(EXPR _last "${_count} - 1")

file(TO_CMAKE_PATH "${TEST_SRC}" _src)
set(_actual "")
foreach(_index RANGE 0 ${_last})
    string(JSON _file GET "${_json}" mutants ${_index} file)
    file(TO_CMAKE_PATH "${_file}" _file)
    string(FIND "${_file}" "${_src}/" _at)
    if(NOT _at EQUAL 0)
        continue()
    endif()
    get_filename_component(_name "${_file}" NAME)
    foreach(_key line column category original replacement type)
        string(JSON _${_key} GET "${_json}" mutants ${_index} ${_key})
    endforeach()
    list(APPEND _actual
        "${_name}:${_line}:${_column} ${_category} ${_original}->${_replacement} ${_type}")
endforeach()
list(SORT _actual)

set(_expected
    "_donor.pony:5:28 aor add->mul _Holder"
    "_donor.pony:5:28 aor add->sub _Holder"
    "_donor.pony:5:28 cr rhs->n->n+1 _Holder"
    "_donor.pony:5:28 cr rhs->n->n-1 _Holder"
    "_donor.pony:5:28 rvr return->0 _Holder"
    "_donor.pony:5:28 rvr return->1 _Holder"
    "_donor.pony:5:28 uoi result->complement _Holder"
    "_donor.pony:5:28 uoi result->neg _Holder"
    "main.pony:3:13 vcr call->remove Main")
list(SORT _expected)

if(NOT _actual STREQUAL _expected)
    string(REPLACE ";" "\n  " _a "${_actual}")
    string(REPLACE ";" "\n  " _e "${_expected}")
    message(FATAL_ERROR "mutants differ\nexpected:\n  ${_e}\nactual:\n  ${_a}")
endif()

# The same program with get's signature unmarked: the literal's class must get
# the mutants of the body it takes after _Donor's check.
set(_variant "${OUTPUT}/unmarked-src")
file(MAKE_DIRECTORY "${_variant}")
file(GLOB _sources "${TEST_SRC}/*.pony")
foreach(_source ${_sources})
    get_filename_component(_name "${_source}" NAME)
    file(READ "${_source}" _text)
    string(REPLACE "fun \\nomutate\\ get" "fun get" _text "${_text}")
    file(WRITE "${_variant}/${_name}" "${_text}")
endforeach()

execute_process(
    COMMAND "${COMPILER}" --mutate -b unmarked --output "${OUTPUT}" "${_variant}"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _err
    TIMEOUT 120
)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "failed to compile the unmarked copy: ${_rc}\n${_out}\n${_err}")
endif()

file(READ "${OUTPUT}/unmarked.mutants.json" _json)
string(JSON _count LENGTH "${_json}" mutants)
math(EXPR _last "${_count} - 1")
set(_literal 0)
foreach(_index RANGE 0 ${_last})
    string(JSON _file GET "${_json}" mutants ${_index} file)
    string(JSON _type GET "${_json}" mutants ${_index} type)
    get_filename_component(_name "${_file}" NAME)
    if((_name STREQUAL "_donor.pony") AND NOT (_type STREQUAL "_Holder"))
        math(EXPR _literal "${_literal} + 1")
    endif()
endforeach()

if(NOT _literal EQUAL 8)
    message(FATAL_ERROR
        "unmarked copy: expected 8 mutants of get for the literal's class, got ${_literal}")
endif()
