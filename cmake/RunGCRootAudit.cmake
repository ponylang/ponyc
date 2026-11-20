cmake_minimum_required(VERSION 3.25)

file(STRINGS "${TEST_SRC}/expected.txt" expected)
list(SORT expected)

function(check_application_stderr)
    string(REPLACE "\n" ";" lines "${diagnostics}")
    list(FILTER lines INCLUDE REGEX "^application")
    if(NOT lines STREQUAL "application print;application write")
        message(FATAL_ERROR
            "${mode} (${schedulers} schedulers) incomplete or unexpected "
            "application output on stderr:\n${diagnostics}")
    endif()
endfunction()

function(check_audit)
    string(REPLACE "\n" ";" lines "${diagnostics}")
    set(observed)
    foreach(line IN LISTS lines)
        if(line MATCHES "^gc-root-audit (Worker|Sink|Main)_")
            string(REGEX REPLACE
                "^gc-root-audit (Worker|Sink|Main)_tag_([a-z_]+)_[A-Za-z0-9]+ "
                "\\1.\\2 " record "${line}")
            list(APPEND observed "${record}")
        endif()
    endforeach()
    list(SORT observed)
    if(NOT observed STREQUAL expected)
        list(JOIN observed "\n" records)
        list(JOIN expected "\n" wanted)
        message(FATAL_ERROR
            "${mode} (${schedulers} schedulers) expected:\n${wanted}\n"
            "Observed:\n${records}")
    endif()
endfunction()

if(WIN32)
    set(suffix ".exe")
endif()

foreach(mode default debug optimized)
    set(dir "${OUTPUT}/${mode}")
    file(MAKE_DIRECTORY "${dir}")
    set(flags)
    if(NOT mode STREQUAL "default")
        list(APPEND flags --gc-root-audit)
    endif()
    if(mode STREQUAL "debug")
        list(APPEND flags -d)
    endif()
    execute_process(
        COMMAND "${COMPILER}" ${flags} -b audit --output "${dir}" "${TEST_SRC}"
        RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 120)
    if(NOT status STREQUAL "0")
        message(FATAL_ERROR "${mode} compiler exited ${status}\n${out}\n${err}")
    endif()

    foreach(schedulers single default)
        set(runtime_flags)
        if(schedulers STREQUAL "single")
            list(APPEND runtime_flags --ponymaxthreads=1 --ponynoscale)
        endif()
        execute_process(
            COMMAND "${dir}/audit${suffix}" ${runtime_flags}
            RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE diagnostics
            TIMEOUT 30)
        if(NOT status STREQUAL "0" OR NOT out MATCHES "resource finalized"
            OR NOT out MATCHES "constructed")
            message(FATAL_ERROR
                "${mode} (${schedulers} schedulers) exited ${status}\n${out}\n${diagnostics}")
        endif()
        string(REPLACE "\r\n" "\n" diagnostics "${diagnostics}")
        check_application_stderr()
        if(mode STREQUAL "default")
            if(diagnostics MATCHES "gc-root-audit")
                message(FATAL_ERROR "Default build emitted root diagnostics\n${diagnostics}")
            endif()
        else()
            check_audit()
        endif()
    endforeach()
endforeach()
