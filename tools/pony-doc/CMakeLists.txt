project(tools.pony-doc)

set(PONY_DOC_EXECUTABLE "pony-doc${CMAKE_EXECUTABLE_SUFFIX}")

# Build into the same per-config output directory ponyc and the C/C++ executables
# use. PONY_OUTPUT_DIR (defined in the top-level CMakeLists) is a $<CONFIG>
# generator expression on the Windows multi-config generator (where
# CMAKE_RUNTIME_OUTPUT_DIRECTORY is config-less) and the plain
# CMAKE_RUNTIME_OUTPUT_DIRECTORY on a single-config generator.

# Generate version.pony from template
# (.gitignore prevents version.pony from dirtying the git tree)
configure_file(version.pony.in ${CMAKE_CURRENT_SOURCE_DIR}/version.pony @ONLY)

add_custom_target(tools.pony-doc ALL DEPENDS ${PONY_OUTPUT_DIR}/${PONY_DOC_EXECUTABLE})
add_dependencies(tools.pony-doc libponyc-standalone tools_lib_stamp)

file(GLOB_RECURSE DOC_SOURCES CONFIGURE_DEPENDS "*.pony")
add_custom_command(OUTPUT ${PONY_OUTPUT_DIR}/${PONY_DOC_EXECUTABLE}
  COMMAND_EXPAND_LISTS
  COMMAND echo "Building pony-doc..."
  COMMAND $<TARGET_FILE:ponyc> ${PONY_CPU_FLAG} --path ${CMAKE_CURRENT_SOURCE_DIR} --path ${CMAKE_CURRENT_SOURCE_DIR}/../lib/ponylang/pony_compiler/ ${PONYC_SELFHOSTED_TOOL_PATH_ARGS} -o ${PONY_OUTPUT_DIR} ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS
    ${DOC_SOURCES}
    ${TOOLS_LIB_STAMP}
    $<TARGET_FILE:ponyc>
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
