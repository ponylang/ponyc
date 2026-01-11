project(tools.pony-lsp)

set(PONY_LSP_EXECUTABLE "pony-lsp${CMAKE_EXECUTABLE_SUFFIX}")

# Use the same output directory that C/C++ executables use
if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../${CMAKE_BUILD_TYPE})
endif()

add_custom_target(tools.pony-lsp ALL DEPENDS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${PONY_LSP_EXECUTABLE})

# TODO STA: this needs to get all the *.pony files not just main.pony
# TODO STA: --path entries are temporary until a ponyc bug is fixed.
add_custom_command(OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${PONY_LSP_EXECUTABLE}
  COMMAND echo "Building pony-lsp..."
  COMMAND $<TARGET_FILE:ponyc> --path ${CMAKE_CURRENT_SOURCE_DIR} --path ${CMAKE_CURRENT_SOURCE_DIR}/../lib/ponylang/peg/ --path ${CMAKE_CURRENT_SOURCE_DIR}/../lib/mfelsche/pony-ast/ --path ${CMAKE_CURRENT_SOURCE_DIR}/../lib/mfelsche/pony-binarysearch/ --path ${CMAKE_CURRENT_SOURCE_DIR}/../lib/mfelsche/pony-immutable-json/ -o ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS
    main.pony
    $<TARGET_FILE:ponyc>
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
