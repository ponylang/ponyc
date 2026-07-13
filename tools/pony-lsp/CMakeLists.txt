project(tools.pony-lsp VERSION ${PONYC_PROJECT_VERSION})

set(PONY_LSP_EXECUTABLE "pony-lsp${CMAKE_EXECUTABLE_SUFFIX}")

# Generate version.pony from template
# (.gitignore prevents version.pony from dirtying the git tree)
configure_file(version.pony.in ${CMAKE_CURRENT_SOURCE_DIR}/version.pony @ONLY)

add_custom_target(tools.pony-lsp ALL DEPENDS ${PONY_OUTPUT_DIR}/${PONY_LSP_EXECUTABLE})
add_dependencies(tools.pony-lsp libponyc-standalone tools_lib_stamp)

# TODO STA: --path entries are temporary until a ponyc bug is fixed.
file(GLOB_RECURSE LSP_SOURCES CONFIGURE_DEPENDS "*.pony")
add_custom_command(OUTPUT ${PONY_OUTPUT_DIR}/${PONY_LSP_EXECUTABLE}
  COMMAND_EXPAND_LISTS
  COMMAND echo "Building pony-lsp..."
  COMMAND $<TARGET_FILE:ponyc> ${PONY_CPU_FLAG} --path ${CMAKE_CURRENT_SOURCE_DIR} --path ${CMAKE_CURRENT_SOURCE_DIR}/../lib/ponylang/pony_compiler/ ${PONYC_SELFHOSTED_TOOL_PATH_ARGS} -o ${PONY_OUTPUT_DIR} ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS
    ${LSP_SOURCES}
    ${TOOLS_LIB_STAMP}
    $<TARGET_FILE:ponyc>
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
