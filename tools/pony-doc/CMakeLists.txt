project(tools.pony-doc)

set(PONY_DOC_EXECUTABLE "pony-doc${CMAKE_EXECUTABLE_SUFFIX}")

# Use the same output directory that C/C++ executables use
if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../${CMAKE_BUILD_TYPE})
endif()

# Generate version.pony from template
# (.gitignore prevents version.pony from dirtying the git tree)
configure_file(version.pony.in ${CMAKE_CURRENT_SOURCE_DIR}/version.pony @ONLY)

add_custom_target(tools.pony-doc ALL DEPENDS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${PONY_DOC_EXECUTABLE})

file(GLOB_RECURSE DOC_SOURCES CONFIGURE_DEPENDS "*.pony")
add_custom_command(OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${PONY_DOC_EXECUTABLE}
  COMMAND echo "Building pony-doc..."
  COMMAND $<TARGET_FILE:ponyc> --path ${CMAKE_CURRENT_SOURCE_DIR} --path ${CMAKE_CURRENT_SOURCE_DIR}/../lib/ponylang/pony_compiler/ -o ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS
    ${DOC_SOURCES}
    $<TARGET_FILE:ponyc>
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

# Test target: compiles the test/ subdirectory with --path to access the
# parent pony-doc package and the pony_compiler dependency.
set(PONY_DOC_TEST_EXECUTABLE "pony-doc-tests${CMAKE_EXECUTABLE_SUFFIX}")

add_custom_target(tools.pony-doc-tests DEPENDS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${PONY_DOC_TEST_EXECUTABLE})

add_custom_command(OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${PONY_DOC_TEST_EXECUTABLE}
  COMMAND echo "Building pony-doc-tests..."
  COMMAND $<TARGET_FILE:ponyc> --path ${CMAKE_CURRENT_SOURCE_DIR} --path ${CMAKE_CURRENT_SOURCE_DIR}/../lib/ponylang/pony_compiler/ -b ${PONY_DOC_TEST_EXECUTABLE} -o ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} ${CMAKE_CURRENT_SOURCE_DIR}/test
  DEPENDS
    ${DOC_SOURCES}
    $<TARGET_FILE:ponyc>
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
