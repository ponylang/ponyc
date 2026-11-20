cmake_minimum_required(VERSION 3.25)

file(STRINGS "${TEST_SRC}/gc_root_audit.pony" source)

function(check_site method expression expected)
    list(FIND source "${expression}" index)
    if(index EQUAL -1)
        message(FATAL_ERROR "Fixture expression missing: ${expression}")
    endif()
    math(EXPR line "${index} + 1")
    string(REGEX MATCHALL
        "(^|\n)gc-root-audit [^ ]+_tag_${method}_[^ ]+ ${line} [^\n]*"
        observations "${diagnostics}")
    list(LENGTH observations count)
    if(NOT count EQUAL 1 OR NOT observations MATCHES " ${expected}$")
        message(FATAL_ERROR
            "${mode} ${threads} threads: expected '${expected}' at "
            "${method} line ${line}, got '${observations}'\n${diagnostics}")
    endif()
endfunction()

function(check_audit)
    set(params "receiver=0 actor=0 val=2 tag=2")
    check_site(audit_roots "    let x: Leaf val = Leaf(n)"
        "frame-ready ${params}")
    check_site(audit_roots "    let y: Leaf val = x"
        "frame-ready ${params} val=4")
    check_site(audit_roots
        "    if (peer is this) and (a is b) and (x is y) and (x.n == n) then"
        "frame-ready ${params} val=4 val=4")
    check_site(audit_roots "    audit_tuple((a, a))" "refused unsupported-prefix")
    check_site(audit_tuple "  be audit_tuple(pair: (Leaf val, Leaf val)) => None"
        "refused unsupported-root")
    check_site(audit_transfer "    sink.take(consume item)"
        "frame-ready receiver=0 iso=1 actor=2")
    check_site(audit_transfer "    @puts(\"sent\".cstring())"
        "refused unsupported-prefix")
    check_site(take "    item.n = 17" "refused receiver-layout")
    check_site(audit_finalisers "  be audit_finalisers() => None"
        "refused finalisers receiver=0")
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

    foreach(threads 1 2)
        execute_process(
            COMMAND "${dir}/audit${suffix}"
                --ponymaxthreads=${threads} --ponynoscale
            RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE diagnostics
            TIMEOUT 30)
        if(NOT status STREQUAL "0" OR NOT out MATCHES "resource finalized")
            message(FATAL_ERROR
                "${mode} ${threads} threads exited ${status}\n${out}\n${diagnostics}")
        endif()
        string(REPLACE "\r\n" "\n" diagnostics "${diagnostics}")
        if(mode STREQUAL "default")
            if(diagnostics MATCHES "gc-root-audit")
                message(FATAL_ERROR "Default build emitted root diagnostics\n${diagnostics}")
            endif()
        else()
            check_audit()
        endif()
    endforeach()
endforeach()
