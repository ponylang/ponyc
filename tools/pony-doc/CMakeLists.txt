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
add_dependencies(tools.pony-doc libponyc-standalone tools_lib_stamp)

file(GLOB_RECURSE DOC_SOURCES CONFIGURE_DEPENDS "*.pony")
add_custom_command(OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${PONY_DOC_EXECUTABLE}
  COMMAND_EXPAND_LISTS
  COMMAND echo "Building pony-doc..."
  COMMAND $<TARGET_FILE:ponyc> ${PONY_CPU_FLAG} --path ${CMAKE_CURRENT_SOURCE_DIR} --path ${CMAKE_CURRENT_SOURCE_DIR}/../lib/ponylang/pony_compiler/ ${PONYC_SELFHOSTED_TOOL_PATH_ARGS} -o ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS
    ${DOC_SOURCES}
    ${TOOLS_LIB_STAMP}
    $<TARGET_FILE:ponyc>
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
