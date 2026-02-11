project(tools.pony-lsp)

set(PONY_LSP_EXECUTABLE "pony-lsp${CMAKE_EXECUTABLE_SUFFIX}")

# Use the same output directory that C/C++ executables use
if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../${CMAKE_BUILD_TYPE})
endif()

# Generate version.pony in build dir, then copy to source dir during compilation
# (.gitignore prevents it from dirtying the git tree)
set(VERSION_PONY_FILE "${CMAKE_CURRENT_BINARY_DIR}/version.pony")
add_custom_command(
  OUTPUT ${VERSION_PONY_FILE}
  COMMAND ${CMAKE_COMMAND} -E echo "primitive PonyLspVersion" > ${VERSION_PONY_FILE}
  COMMAND ${CMAKE_COMMAND} -E echo "  fun version_string(): String => \"${PONYC_VERSION}\"" >> ${VERSION_PONY_FILE}
  DEPENDS ${CMAKE_SOURCE_DIR}/VERSION
  COMMENT "Generating version.pony with version ${PONYC_VERSION}"
  VERBATIM
)

add_custom_target(tools.pony-lsp ALL DEPENDS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${PONY_LSP_EXECUTABLE})

# TODO STA: --path entries are temporary until a ponyc bug is fixed.
file(GLOB_RECURSE LSP_SOURCES CONFIGURE_DEPENDS "*.pony")
add_custom_command(OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${PONY_LSP_EXECUTABLE}
  COMMAND echo "Building pony-lsp..."
  COMMAND ${CMAKE_COMMAND} -E copy ${VERSION_PONY_FILE} ${CMAKE_CURRENT_SOURCE_DIR}/version.pony
  COMMAND $<TARGET_FILE:ponyc> --path ${CMAKE_CURRENT_SOURCE_DIR} --path ${CMAKE_CURRENT_SOURCE_DIR}/../lib/ponylang/peg/ --path ${CMAKE_CURRENT_SOURCE_DIR}/../lib/mfelsche/pony-ast/ --path ${CMAKE_CURRENT_SOURCE_DIR}/../lib/mfelsche/pony-binarysearch/ --path ${CMAKE_CURRENT_SOURCE_DIR}/../lib/mfelsche/pony-immutable-json/ -o ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS
    ${VERSION_PONY_FILE}
    ${LSP_SOURCES}
    $<TARGET_FILE:ponyc>
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
