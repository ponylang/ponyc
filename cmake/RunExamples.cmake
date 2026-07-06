# ctest wrapper: compile every example under examples/ with the freshly built
# ponyc. Each example directory holding
# .pony files is compiled with `-d -s --checktree -o <dir> <dir>`. Directories
# whose path contains "ffi-" are skipped: those examples link against C the
# stock toolchain may not have, so they are excluded from the build check.
#
# Args (passed with -D): PONYC, EXAMPLES (the examples directory), WORKDIR (the
# output directory to run ponyc from).

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

# Every .pony file's directory is a package to compile. This globs
# examples/*/* (files at least one level below examples/), so a .pony sitting
# directly in examples/ is not a package and is skipped below.
file(GLOB_RECURSE _pony_files "${EXAMPLES}/*.pony")
set(_dirs "")
foreach(_f ${_pony_files})
    get_filename_component(_d "${_f}" DIRECTORY)
    list(APPEND _dirs "${_d}")
endforeach()
list(REMOVE_DUPLICATES _dirs)
list(SORT _dirs)

foreach(_d ${_dirs})
    if(_d STREQUAL "${EXAMPLES}")
        continue()
    endif()
    if(_d MATCHES "ffi-")
        continue()
    endif()
    execute_process(
        COMMAND "${PONYC}" -d -s --checktree -o "${_d}" "${_d}"
        WORKING_DIRECTORY "${WORKDIR}"
        RESULT_VARIABLE _rc)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "compiling example ${_d} failed (exit ${_rc})")
    endif()
endforeach()
