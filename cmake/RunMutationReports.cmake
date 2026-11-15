find_program(_python NAMES python3 python REQUIRED)
file(MAKE_DIRECTORY "${OUTPUT}")
set(_bin "${OUTPUT}/report-fixture")
if(WIN32)
    set(_bin "${OUTPUT}/report-fixture.exe")
endif()
set(_flags "")
if(DEFINED ENV{PONY_DEBUG} AND NOT "$ENV{PONY_DEBUG}" STREQUAL "")
    list(APPEND _flags -d)
endif()
if(DEFINED ENV{PONY_THIN_LTO} AND NOT "$ENV{PONY_THIN_LTO}" STREQUAL "")
    list(APPEND _flags --thin-lto)
endif()
execute_process(
    COMMAND "${COMPILER}" ${_flags} -b report-fixture
        --output "${OUTPUT}" "${TEST_SRC}"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _err
    TIMEOUT 120
)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "report fixture compilation: ${_rc}\n${_out}\n${_err}")
endif()
execute_process(
    COMMAND "${_python}" "${TEST_SRC}/check_reports.py"
        --fixture "${_bin}" --runner "${RUNNER}" -v
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _err
    TIMEOUT 180
)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "report tests: ${_rc}\n${_out}\n${_err}")
endif()
message(STATUS "${_out}\n${_err}")
