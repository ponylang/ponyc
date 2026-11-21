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
        COMMAND "${COMPILER}" ${flags} -D midbehavior_gc -o "${dir}" "${TEST_SRC}"
        COMMAND_ERROR_IS_FATAL ANY)
    execute_process(
        COMMAND "${dir}/midbehavior-gc${CMAKE_EXECUTABLE_SUFFIX}"
            --ponymaxthreads=1 --ponynoscale
        COMMAND_ERROR_IS_FATAL ANY)
    execute_process(
        COMMAND "${dir}/midbehavior-gc${CMAKE_EXECUTABLE_SUFFIX}"
            check-finalizers --ponymaxthreads=1 --ponynoscale
        COMMAND_ERROR_IS_FATAL ANY)
    execute_process(
        COMMAND "${dir}/midbehavior-gc${CMAKE_EXECUTABLE_SUFFIX}"
            transfer
        COMMAND_ERROR_IS_FATAL ANY)
endforeach()
