cmake_minimum_required(VERSION 3.25)

function(check_behavior_sweeps trace actor behavior)
    file(STRINGS "${trace}" events REGEX
        "\"name\":\"(actor_gc_heap_sweep|actor_behavior_run)\".*\"cat\":\"ACTOR: ${actor}\"")
    set(active_actor "")
    set(expected ${ARGN})
    foreach(event IN LISTS events)
        if(event MATCHES "\"ph\":\"([be])\".*\"id\":\"([^\"]+)\".*\"behavior_name\":\"${actor}[.]${behavior}\"")
            if(CMAKE_MATCH_1 STREQUAL "b")
                if(active_actor OR "${expected}" STREQUAL "")
                    message(FATAL_ERROR "Unexpected start of ${actor}.${behavior}")
                endif()
                set(active_actor "${CMAKE_MATCH_2}")
                set(swept FALSE)
            else()
                if(NOT active_actor STREQUAL CMAKE_MATCH_2)
                    message(FATAL_ERROR "Unmatched end of ${actor}.${behavior}")
                endif()
                list(POP_FRONT expected want_sweep)
                if(NOT swept STREQUAL want_sweep)
                    message(FATAL_ERROR "${actor}.${behavior}: expected heap sweep=${want_sweep}, observed=${swept}")
                endif()
                set(active_actor "")
            endif()
        elseif(active_actor AND event MATCHES
            "\"name\":\"actor_gc_heap_sweep\".*\"ph\":\"e\".*\"id\":\"${active_actor}\"")
            set(swept TRUE)
        endif()
    endforeach()
    if(active_actor OR NOT "${expected}" STREQUAL "")
        message(FATAL_ERROR "Missing complete ${actor}.${behavior} behavior")
    endif()
endfunction()

foreach(mode debug release)
    set(flags "")
    if(mode STREQUAL "debug")
        list(APPEND flags -d)
    endif()
    if(DEFINED ENV{PONY_THIN_LTO} AND "$ENV{PONY_THIN_LTO}" STREQUAL "1")
        list(APPEND flags --thin-lto)
    endif()
    set(dir "${OUTPUT}/${mode}")
    file(MAKE_DIRECTORY "${dir}")
    execute_process(
        COMMAND "${COMPILER}" ${flags} -o "${dir}" "${TEST_SRC}"
        COMMAND_ERROR_IS_FATAL ANY)
    set(trace "${dir}/ponytrace.json")
    file(REMOVE "${trace}")
    execute_process(
        COMMAND "${dir}/midbehavior-gc${CMAKE_EXECUTABLE_SUFFIX}"
            --ponymaxthreads=1 --ponynoscale
            --ponytracingcategories=actor_behavior,actor_gc
        WORKING_DIRECTORY "${dir}"
        COMMAND_ERROR_IS_FATAL ANY)
    check_behavior_sweeps("${trace}" Main _churn TRUE FALSE)
    file(REMOVE "${trace}")
    execute_process(
        COMMAND "${dir}/midbehavior-gc${CMAKE_EXECUTABLE_SUFFIX}"
            check-finalizers --ponymaxthreads=1 --ponynoscale
            --ponytracingcategories=actor_behavior
        WORKING_DIRECTORY "${dir}"
        COMMAND_ERROR_IS_FATAL ANY)
    file(STRINGS "${trace}" churn_ends REGEX
        [["name":"actor_behavior_run".*"ph":"e".*"behavior_name":"Main[.]_churn"]])
    list(LENGTH churn_ends end_count)
    if(NOT end_count EQUAL 2)
        message(FATAL_ERROR "Expected both Main._churn behavior ends")
    endif()
    foreach(event IN LISTS churn_ends)
        if(NOT event MATCHES [=["unscheduled":false[,}]]=])
            message(FATAL_ERROR "Incorrect unscheduled state in ${event}")
        endif()
    endforeach()
    execute_process(
        COMMAND "${dir}/midbehavior-gc${CMAKE_EXECUTABLE_SUFFIX}"
            transfer --ponymaxthreads=2 --ponynoscale
            --ponytracingcategories=actor_behavior,actor_gc
        WORKING_DIRECTORY "${dir}"
        COMMAND_ERROR_IS_FATAL ANY)
    check_behavior_sweeps("${trace}" _Owner _self_send TRUE)
    check_behavior_sweeps("${trace}" _Owner _while_sent TRUE)
    check_behavior_sweeps("${trace}" _Writer _start TRUE)
    check_behavior_sweeps("${trace}" _Reader read TRUE)
    file(REMOVE "${trace}")
    execute_process(
        COMMAND "${dir}/midbehavior-gc${CMAKE_EXECUTABLE_SUFFIX}"
            alias-order --ponymaxthreads=2 --ponynoscale
            --ponytracingcategories=actor_behavior,actor_gc
        WORKING_DIRECTORY "${dir}"
        COMMAND_ERROR_IS_FATAL ANY)
    check_behavior_sweeps("${trace}" _AliasSender _churn TRUE)
    check_behavior_sweeps("${trace}" _AliasReceiver tag_first TRUE)
    check_behavior_sweeps("${trace}" _AliasReceiver val_first TRUE)
    check_behavior_sweeps("${trace}" _AliasReceiver check TRUE)
    file(REMOVE "${trace}")
    execute_process(
        COMMAND "${dir}/midbehavior-gc${CMAKE_EXECUTABLE_SUFFIX}"
            recursion --ponymaxthreads=1 --ponynoscale
            --ponytracingcategories=actor_behavior,actor_gc
        WORKING_DIRECTORY "${dir}"
        COMMAND_ERROR_IS_FATAL ANY)
    check_behavior_sweeps("${trace}" _Recurser _run TRUE)
    file(REMOVE "${trace}")
    execute_process(
        COMMAND "${dir}/midbehavior-gc${CMAKE_EXECUTABLE_SUFFIX}"
            unwind --ponymaxthreads=1 --ponynoscale
            --ponytracingcategories=actor_behavior,actor_gc
        WORKING_DIRECTORY "${dir}"
        COMMAND_ERROR_IS_FATAL ANY)
    check_behavior_sweeps("${trace}" _Unwinder _run TRUE)
    file(REMOVE "${trace}")
    execute_process(
        COMMAND "${dir}/midbehavior-gc${CMAKE_EXECUTABLE_SUFFIX}"
            result --ponymaxthreads=1 --ponynoscale
            --ponytracingcategories=actor_behavior,actor_gc
        WORKING_DIRECTORY "${dir}"
        COMMAND_ERROR_IS_FATAL ANY)
    check_behavior_sweeps("${trace}" _Returner _run TRUE)
endforeach()
