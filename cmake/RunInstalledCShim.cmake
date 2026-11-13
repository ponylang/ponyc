cmake_minimum_required(VERSION 3.25)

foreach(_arg BUILD_DIR CONFIG VERSIONED_DIR RUNNER FIXTURE WORKDIR)
    if(NOT DEFINED ${_arg} OR "${${_arg}}" STREQUAL "")
        message(FATAL_ERROR "Missing required argument: ${_arg}")
    endif()
endforeach()

if(IS_SYMLINK "${WORKDIR}")
    message(FATAL_ERROR "Scratch path must not be a symlink: ${WORKDIR}")
endif()
# WORKDIR may not exist; REAL_PATH needs an existing path to resolve symlinks.
get_filename_component(_parent "${WORKDIR}" DIRECTORY)
if(NOT IS_DIRECTORY "${_parent}")
    message(FATAL_ERROR "Scratch parent directory is missing: ${_parent}")
endif()
file(REAL_PATH "${_parent}" _parent)
get_filename_component(_name "${WORKDIR}" NAME)
set(_workdir "${_parent}/${_name}")
file(REAL_PATH "${CMAKE_CURRENT_LIST_DIR}/.." _source)
file(REAL_PATH "${BUILD_DIR}" _build)
foreach(_protected "${_source}" "${_build}")
    cmake_path(IS_PREFIX _protected "${_workdir}" NORMALIZE _inside)
    cmake_path(IS_PREFIX _workdir "${_protected}" NORMALIZE _ancestor)
    if(_inside OR _ancestor)
        message(FATAL_ERROR
            "Installed-shim scratch must be outside source and build trees: ${_workdir}")
    endif()
endforeach()

string(RANDOM LENGTH 24 ALPHABET 0123456789abcdef _scratch_id)
set(_workdir "${_workdir}/${_scratch_id}")
if(EXISTS "${_workdir}" OR IS_SYMLINK "${_workdir}")
    message(FATAL_ERROR "Private scratch directory already exists: ${_workdir}")
endif()
set(_prefix "${_workdir}/install")
set(_fixture_root "${_workdir}/source")
set(_tests "${_fixture_root}/tests")
set(_output "${_workdir}/output")
file(MAKE_DIRECTORY "${_prefix}" "${_tests}" "${_output}")
unset(ENV{DESTDIR})
unset(ENV{PONYPATH})

execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${BUILD_DIR}"
        --config "${CONFIG}" --prefix "${_prefix}"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out ERROR_VARIABLE _err
    TIMEOUT 120)
if(NOT _rc STREQUAL "0")
    message(FATAL_ERROR "Installation failed (${_rc}):\n${_out}${_err}")
endif()

set(_compiler "${_prefix}/${VERSIONED_DIR}/bin/ponyc${EXE_SUFFIX}")
if(NOT EXISTS "${_compiler}")
    message(FATAL_ERROR "Installed compiler is missing: ${_compiler}")
endif()
file(REAL_PATH "${_compiler}" _compiler)
file(REAL_PATH "${_prefix}" _prefix)
cmake_path(IS_PREFIX _prefix "${_compiler}" NORMALIZE _installed)
if(NOT _installed)
    message(FATAL_ERROR "Compiler is outside installation prefix: ${_compiler}")
endif()

file(COPY "${FIXTURE}" DESTINATION "${_tests}")
set(_executable "${_output}/c-shim-pony-header${EXE_SUFFIX}")
if(EXISTS "${_executable}")
    message(FATAL_ERROR "Stale fixture executable before compilation: ${_executable}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DRUNNER=${RUNNER}"
        "-DCOMPILER=${_compiler}"
        "-DOUTPUT=${_output}"
        -DTEST_LIB=
        "-DTESTS=${_tests}"
        "-DWORKDIR=${_workdir}"
        "-DSOURCE_DIR=${_fixture_root}"
        -P "${CMAKE_CURRENT_LIST_DIR}/RunFullPrograms.cmake"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out ERROR_VARIABLE _err
    TIMEOUT 150)
set(_log "${_out}${_err}")
message("${_log}")
if(NOT _rc STREQUAL "0")
    message(FATAL_ERROR "Installed C shim failed (${_rc})")
endif()
if(NOT _log MATCHES "Running 1 tests compiled in")
    message(FATAL_ERROR "Installed C shim fixture was not discovered exactly once")
endif()
string(ASCII 27 _escape)
string(REGEX REPLACE "${_escape}\\[[0-9;]*m" "" _log "${_log}")
string(FIND "${_log}" "[       OK ] c-shim-pony-header " _passed)
if(_passed EQUAL -1)
    message(FATAL_ERROR "Installed C shim fixture did not report successful execution")
endif()
string(FIND "${_log}" "[  PASSED  ] 1 test(s)." _summary)
if(_summary EQUAL -1)
    message(FATAL_ERROR "Installed C shim runner did not report one passing test")
endif()
if(NOT EXISTS "${_executable}")
    message(FATAL_ERROR "Installed C shim executable was not produced: ${_executable}")
endif()
file(REMOVE_RECURSE "${_workdir}")
