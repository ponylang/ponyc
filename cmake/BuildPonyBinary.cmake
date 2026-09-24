# cmake -P wrapper: invoke ponyc to compile a Pony binary, reading PONY_DEBUG
# and PONY_THIN_LTO from the environment. PonyBinary.cmake delegates here so
# that environment variables are evaluated at build time, not configure time.
#
# Args (passed with -D): PONYC, ARGSFILE (path to a file with one arg per line).

file(STRINGS ${ARGSFILE} ARGS)

if(DEFINED ENV{PONY_DEBUG} AND "$ENV{PONY_DEBUG}" STREQUAL "1")
    list(PREPEND ARGS --debug)
endif()
if(DEFINED ENV{PONY_THIN_LTO} AND "$ENV{PONY_THIN_LTO}" STREQUAL "1")
    list(APPEND ARGS --thin-lto)
endif()

execute_process(
    COMMAND ${PONYC} ${ARGS}
    RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "ponyc failed (exit ${_rc})")
endif()
