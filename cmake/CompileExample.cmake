# ctest wrapper: compile a single example package with the freshly built ponyc.
#
# Args (passed with -D): PONYC, EXAMPLE_DIR (the package directory), WORKDIR
# (the output directory to run ponyc from), PONY_SSL_FLAG (the SSL -D flag for
# the net package, e.g. -Dopenssl_3.0.x).

if(CMAKE_HOST_WIN32)
    set(_sep ";")
else()
    set(_sep ":")
endif()
if(DEFINED ENV{PONYPATH} AND NOT "$ENV{PONYPATH}" STREQUAL "")
    set(ENV{PONYPATH} "${WORKDIR}${_sep}$ENV{PONYPATH}")
else()
    set(ENV{PONYPATH} "${WORKDIR}")
endif()

execute_process(
    COMMAND "${PONYC}" -d -s --checktree -o "${EXAMPLE_DIR}" "${PONY_SSL_FLAG}" "${EXAMPLE_DIR}"
    WORKING_DIRECTORY "${WORKDIR}"
    RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "compiling example ${EXAMPLE_DIR} failed (exit ${_rc})")
endif()
