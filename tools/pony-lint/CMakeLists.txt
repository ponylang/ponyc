project(tools.pony-lint)

set(PONY_LINT_EXECUTABLE "pony-lint${CMAKE_EXECUTABLE_SUFFIX}")

# Generate version.pony from template
# (.gitignore prevents version.pony from dirtying the git tree)
configure_file(version.pony.in ${CMAKE_CURRENT_SOURCE_DIR}/version.pony @ONLY)

add_custom_target(tools.pony-lint ALL DEPENDS ${PONY_OUTPUT_DIR}/${PONY_LINT_EXECUTABLE})
add_dependencies(tools.pony-lint libponyc-standalone tools_lib_stamp)

file(GLOB_RECURSE LINT_SOURCES CONFIGURE_DEPENDS "*.pony")
add_custom_command(OUTPUT ${PONY_OUTPUT_DIR}/${PONY_LINT_EXECUTABLE}
  COMMAND_EXPAND_LISTS
  COMMAND echo "Building pony-lint..."
  COMMAND $<TARGET_FILE:ponyc> ${PONY_CPU_FLAG} --path ${CMAKE_CURRENT_SOURCE_DIR} --path ${CMAKE_CURRENT_SOURCE_DIR}/../lib/ponylang/pony_compiler/ ${PONYC_SELFHOSTED_TOOL_PATH_ARGS} -o ${PONY_OUTPUT_DIR} ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS
    ${LINT_SOURCES}
    ${TOOLS_LIB_STAMP}
    $<TARGET_FILE:ponyc>
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
