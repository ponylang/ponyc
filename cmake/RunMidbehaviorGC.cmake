cmake_minimum_required(VERSION 3.25)

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
    execute_process(
        COMMAND "${dir}/midbehavior-gc${CMAKE_EXECUTABLE_SUFFIX}"
            --ponymaxthreads=1 --ponynoscale
        COMMAND_ERROR_IS_FATAL ANY)
    set(trace "${dir}/ponytrace.json")
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
            transfer
        COMMAND_ERROR_IS_FATAL ANY)
endforeach()
