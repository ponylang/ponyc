# add_pony_binary(<target>
#   NAME   <output-name>      # binary name (passed to ponyc -b)
#   SOURCE <dir>              # directory ponyc compiles
#   [ALL]                    # build under ALL (default: built on demand only)
#   [PATHS <dir> ...]        # extra --path arguments for use resolution
#   [WATCH <dir> ...])       # directories whose *.pony files are build inputs
#
# Compiles a Pony program with the just-built ponyc into the runtime output
# directory, as a custom target that builds under ALL. This is the one place the
# ponyc-invocation pattern lives: the tool binaries (pony-lint/doc/lsp) and the
# test binaries (pony-*-tests) all go through it, so they share one dependency-
# tracking path.
#
# Rebuilds when any watched .pony source changes, when the shared tool libraries
# change (TOOLS_LIB_STAMP), or when ponyc itself is rebuilt.
function(add_pony_binary _target)
    cmake_parse_arguments(_pb "ALL" "NAME;SOURCE" "PATHS;WATCH" ${ARGN})

    # Per-config output directory. PONY_OUTPUT_DIR (defined in the top-level
    # CMakeLists) is a $<CONFIG> generator expression on the Windows multi-config
    # generator (where the plain CMAKE_RUNTIME_OUTPUT_DIRECTORY is config-less) and
    # the plain CMAKE_RUNTIME_OUTPUT_DIRECTORY on a single-config generator.
    # $<TARGET_FILE_DIR:ponyc> can't be used here because a target genex is invalid
    # in add_custom_command(OUTPUT).
    set(_out "${PONY_OUTPUT_DIR}")
    set(_exe "${_out}/${_pb_NAME}${CMAKE_EXECUTABLE_SUFFIX}")

    set(_path_args "")
    foreach(_p IN LISTS _pb_PATHS)
        list(APPEND _path_args --path ${_p})
    endforeach()

    set(_watch_srcs "")
    foreach(_w IN LISTS _pb_WATCH)
        file(GLOB_RECURSE _found CONFIGURE_DEPENDS "${_w}/*.pony")
        list(APPEND _watch_srcs ${_found})
    endforeach()

    add_custom_command(OUTPUT ${_exe}
        COMMAND_EXPAND_LISTS
        COMMAND echo "Building ${_pb_NAME}..."
        COMMAND $<TARGET_FILE:ponyc> ${PONY_CPU_FLAG} ${_path_args}
            ${PONYC_SELFHOSTED_TOOL_PATH_ARGS}
            -b ${_pb_NAME} -o ${_out} ${_pb_SOURCE}
        DEPENDS
            ${_watch_srcs}
            ${TOOLS_LIB_STAMP}
            $<TARGET_FILE:ponyc>
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    if(_pb_ALL)
        add_custom_target(${_target} ALL DEPENDS ${_exe})
    else()
        add_custom_target(${_target} DEPENDS ${_exe})
    endif()
    add_dependencies(${_target} libponyc-standalone tools_lib_stamp)
endfunction()
