# ctest wrapper: check that ponyc (the driver, src/ponyc) on Windows
# exits with the compiler's exit code and prints what the compiler prints,
# given a package path that contains a space, and fails with a message when
# there is no compiler next to it.
#
# Args (passed with -D): PONYC (the driver), PONYC_COMPILER (the compiler
# binary), WORKDIR (a scratch dir the fixture is generated under).

set(_fixture "${WORKDIR}/ponyc-driver-windows")
file(REMOVE_RECURSE "${_fixture}")
file(MAKE_DIRECTORY "${_fixture}/type error")
file(WRITE "${_fixture}/type error/main.pony"
    "actor Main\n  new create(env: Env) =>\n    let x: U8 = \"not a number\"\n")

function(compare _name)
    execute_process(COMMAND "${PONYC_COMPILER}" ${ARGN}
        WORKING_DIRECTORY "${_fixture}"
        RESULT_VARIABLE _compiler_rc
        OUTPUT_VARIABLE _compiler_out ERROR_VARIABLE _compiler_err)
    execute_process(COMMAND "${PONYC}" ${ARGN}
        WORKING_DIRECTORY "${_fixture}"
        RESULT_VARIABLE _driver_rc
        OUTPUT_VARIABLE _driver_out ERROR_VARIABLE _driver_err)
    if(NOT _driver_rc STREQUAL _compiler_rc OR
        NOT "${_driver_out}${_driver_err}" STREQUAL
        "${_compiler_out}${_compiler_err}")
        message(FATAL_ERROR "${_name}: ponyc exited ${_driver_rc}, the compiler "
            "${_compiler_rc}\nponyc:\n${_driver_out}${_driver_err}\n"
            "compiler:\n${_compiler_out}${_compiler_err}")
    endif()
    set(${_name}_rc "${_driver_rc}" PARENT_SCOPE)
    set(${_name}_out "${_driver_out}${_driver_err}" PARENT_SCOPE)
endfunction()

compare(version --version)
compare(typeerror --pass=expr "type error")
if(typeerror_rc EQUAL 0)
    message(FATAL_ERROR "typeerror: ponyc exited 0 for a type error")
endif()
if(NOT "${typeerror_out}" MATCHES "type error")
    message(FATAL_ERROR "typeerror: the output doesn't name the package path:\n${typeerror_out}")
endif()

# No compiler next to the driver: ponyc fails with a message naming the path
# where it looked.
file(MAKE_DIRECTORY "${_fixture}/nocompiler")
file(COPY "${PONYC}" DESTINATION "${_fixture}/nocompiler")
get_filename_component(_driver_name "${PONYC}" NAME)
execute_process(COMMAND "${_fixture}/nocompiler/${_driver_name}" --version
    WORKING_DIRECTORY "${_fixture}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(_rc EQUAL 0 OR NOT "${_out}${_err}" MATCHES
    "ponyc: unable to start the ponyc compiler at [^\n]*nocompiler")
    message(FATAL_ERROR "nocompiler: ponyc exited ${_rc}\n${_out}${_err}")
endif()
