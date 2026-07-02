# ctest wrapper: compile packages/stdlib with the freshly built ponyc, then run
# the resulting test binary. Mirrors the Makefile's test-stdlib-{debug,release}.
#
# Args (passed with -D): PONYC, STDLIB_SRC (packages/stdlib), WORKDIR (the output
# directory the binary is built into and run from), BUILD_NAME (stdlib-debug or
# stdlib-release), DEBUG (ON for the debug build), EXCLUDES (optional
# space-separated extra args, e.g. --exclude=net/Broadcast).

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

set(_args -b "${BUILD_NAME}" --pic --checktree)
if(DEBUG)
    list(PREPEND _args -d)
    list(APPEND _args --strip)
endif()

execute_process(
    COMMAND "${PONYC}" ${_args} "${STDLIB_SRC}"
    WORKING_DIRECTORY "${WORKDIR}"
    RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "compiling stdlib (${BUILD_NAME}) failed (exit ${_rc})")
endif()

set(_run "${WORKDIR}/${BUILD_NAME}" --sequential)
if(EXCLUDES)
    separate_arguments(_extra UNIX_COMMAND "${EXCLUDES}")
    list(APPEND _run ${_extra})
endif()

execute_process(
    COMMAND ${_run}
    WORKING_DIRECTORY "${WORKDIR}"
    RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "stdlib tests (${BUILD_NAME}) failed (exit ${_rc})")
endif()
