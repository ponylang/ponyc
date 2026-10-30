# ctest wrapper: compile two small Pony programs and run them under different
# tracing configurations, verifying the trace output after each run.
#
# Args (passed with -D): PONYC, TESTS (source dir), WORKDIR (build output dir),
# SOURCE_DIR (repo root).

if(CMAKE_HOST_WIN32)
    set(_exe ".exe")
else()
    set(_exe "")
endif()

set(_ponyc_args --checktree)
if(NOT CMAKE_HOST_WIN32)
    list(APPEND _ponyc_args --pic)
endif()

set(_pass_count 0)
set(_fail_count 0)
set(_scenarios "")

# --- helpers ----------------------------------------------------------------

macro(compile_test _name _src _bin)
    message(STATUS "Compiling ${_name} ...")
    execute_process(
        COMMAND "${PONYC}" ${_ponyc_args} -b "${_name}" "${_src}"
        WORKING_DIRECTORY "${WORKDIR}"
        RESULT_VARIABLE _rc)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "compiling ${_name} failed (exit ${_rc})")
    endif()
    set(${_bin} "${WORKDIR}/${_name}${_exe}")
endmacro()

macro(run_scenario _label _binary _cli_args)
    list(APPEND _scenarios "${_label}")
    set(_run_dir "${WORKDIR}/scenario-${_label}")
    file(MAKE_DIRECTORY "${_run_dir}")
    # Remove any leftover trace file.
    file(REMOVE "${_run_dir}/ponytrace.json")

    message(STATUS "Running scenario: ${_label}")
    execute_process(
        COMMAND ${_binary} ${_cli_args}
        WORKING_DIRECTORY "${_run_dir}"
        RESULT_VARIABLE _rc
        TIMEOUT 30)
    if(NOT _rc EQUAL 0)
        message(WARNING "  ${_label}: program exited with ${_rc}")
        math(EXPR _fail_count "${_fail_count} + 1")
    endif()
endmacro()

# Check that a file exists.
macro(expect_file _label _path)
    if(NOT EXISTS "${_path}")
        message(WARNING "  FAIL ${_label}: expected file does not exist: ${_path}")
        math(EXPR _fail_count "${_fail_count} + 1")
    else()
        message(STATUS "  PASS ${_label}: file exists")
        math(EXPR _pass_count "${_pass_count} + 1")
    endif()
endmacro()

# Check that a file does NOT exist.
macro(expect_no_file _label _path)
    if(EXISTS "${_path}")
        message(WARNING "  FAIL ${_label}: file should not exist: ${_path}")
        math(EXPR _fail_count "${_fail_count} + 1")
    else()
        message(STATUS "  PASS ${_label}: file correctly absent")
        math(EXPR _pass_count "${_pass_count} + 1")
    endif()
endmacro()

# Check that a file contains a string.
macro(expect_contains _label _path _pattern)
    file(READ "${_path}" _contents)
    string(FIND "${_contents}" "${_pattern}" _pos)
    if(_pos EQUAL -1)
        message(WARNING "  FAIL ${_label}: '${_pattern}' not found in ${_path}")
        math(EXPR _fail_count "${_fail_count} + 1")
    else()
        message(STATUS "  PASS ${_label}")
        math(EXPR _pass_count "${_pass_count} + 1")
    endif()
endmacro()

# Check that a file does NOT contain a string.
macro(expect_not_contains _label _path _pattern)
    file(READ "${_path}" _contents)
    string(FIND "${_contents}" "${_pattern}" _pos)
    if(NOT _pos EQUAL -1)
        message(WARNING "  FAIL ${_label}: '${_pattern}' unexpectedly found in ${_path}")
        math(EXPR _fail_count "${_fail_count} + 1")
    else()
        message(STATUS "  PASS ${_label}")
        math(EXPR _pass_count "${_pass_count} + 1")
    endif()
endmacro()

# --- setup ------------------------------------------------------------------

file(MAKE_DIRECTORY "${WORKDIR}")

# --- compile ----------------------------------------------------------------

compile_test(tracing-cli "${TESTS}/cli-flags" _cli_bin)
compile_test(tracing-api "${TESTS}/runtime-api" _api_bin)

# --- scenario 1: no tracing flags at all -> no trace file -------------------

run_scenario(no-flags "${_cli_bin}" "")
set(_trace "${WORKDIR}/scenario-no-flags/ponytrace.json")
expect_no_file("no-flags: no trace file" "${_trace}")

# --- scenario 2: actors=all, categories=actor -> actor events present -------

run_scenario(actors-all
    "${_cli_bin}"
    "--ponytracingactors=all;--ponytracingcategories=actor")
set(_trace "${WORKDIR}/scenario-actors-all/ponytrace.json")
expect_file("actors-all: trace file exists" "${_trace}")
expect_contains("actors-all: has Worker actor events"
    "${_trace}" "ACTOR: Worker")

# --- scenario 3: actors=none, categories=actor -> no actor events -----------

run_scenario(actors-none
    "${_cli_bin}"
    "--ponytracingactors=none;--ponytracingcategories=actor")
set(_trace "${WORKDIR}/scenario-actors-none/ponytrace.json")
expect_file("actors-none: trace file exists" "${_trace}")
expect_not_contains("actors-none: no Worker actor events"
    "${_trace}" "ACTOR: Worker")

# --- scenario 4: actors=cd_only, categories=actor -> CD events, no Worker --

run_scenario(actors-cd-only
    "${_cli_bin}"
    "--ponytracingactors=cd_only;--ponytracingcategories=actor")
set(_trace "${WORKDIR}/scenario-actors-cd-only/ponytrace.json")
expect_file("actors-cd-only: trace file exists" "${_trace}")
expect_contains("actors-cd-only: has cycle detector events"
    "${_trace}" "ACTOR: cycle detector")
expect_not_contains("actors-cd-only: no Worker actor events"
    "${_trace}" "ACTOR: Worker")

# --- scenario 5: actors=all, categories=scheduler -> scheduler but not actor-

run_scenario(categories-scheduler
    "${_cli_bin}"
    "--ponytracingactors=all;--ponytracingcategories=scheduler")
set(_trace "${WORKDIR}/scenario-categories-scheduler/ponytrace.json")
expect_file("categories-scheduler: trace file exists" "${_trace}")
expect_contains("categories-scheduler: has scheduler events"
    "${_trace}" "thread_start")
expect_not_contains("categories-scheduler: no actor events"
    "${_trace}" "ACTOR: Worker")

# --- scenario 6: runtime API enables tracing -> actor events present --------

run_scenario(runtime-api "${_api_bin}" "")
set(_trace "${WORKDIR}/scenario-runtime-api/ponytrace.json")
expect_file("runtime-api: trace file exists" "${_trace}")
expect_contains("runtime-api: has Worker actor events"
    "${_trace}" "ACTOR: Worker")

# --- summary ----------------------------------------------------------------

message(STATUS "")
message(STATUS "Tracing tests: ${_pass_count} passed, ${_fail_count} failed")
if(_fail_count GREATER 0)
    message(FATAL_ERROR "Tracing tests failed")
endif()
