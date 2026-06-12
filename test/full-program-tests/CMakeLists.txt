project(libponyc.tests.run VERSION ${PONYC_PROJECT_VERSION} LANGUAGES C)

# External-lib fixtures keep their C source in <test>/c-src/; it is built
# here into a <test>-additional library that the test links with
# `use "lib:<test>-additional"`. Only c-src/ is globbed, deliberately: a C
# file in a test's top directory is a shim that ponyc itself discovers and
# compiles (see genc), and building it here too would shadow the
# `use "lib:"` path the external-lib tests exist to cover.
# Assumes that there is only 1 C source file per test
file(GLOB test_c_sources RELATIVE ${CMAKE_CURRENT_LIST_DIR} */c-src/*.c */c-src/*.cc)

foreach(test_c_source ${test_c_sources})
  string(FIND ${test_c_source} "/" slash_pos)
  string(SUBSTRING ${test_c_source} 0 ${slash_pos} test_lib_name)
  set(test_lib_name "${test_lib_name}-additional")

  if(MSVC)
    add_library(${test_lib_name} STATIC ${test_c_source})
    target_link_libraries(${test_lib_name}
      PRIVATE libponyrt
      PRIVATE ucrt
      PRIVATE Ws2_32
      PRIVATE Shlwapi
    )
  else()
    add_library(${test_lib_name} SHARED ${test_c_source})
    target_link_libraries(${test_lib_name} PRIVATE libponyrt)
  endif()

  add_custom_command(TARGET ${test_lib_name} POST_BUILD
    COMMAND $<$<CONFIG:Debug>:${CMAKE_COMMAND}> ARGS -E make_directory ${CMAKE_BINARY_DIR}/../debug${PONY_OUTPUT_SUFFIX}/test_lib
    COMMAND $<$<CONFIG:Release>:${CMAKE_COMMAND}> ARGS -E make_directory ${CMAKE_BINARY_DIR}/../release${PONY_OUTPUT_SUFFIX}/test_lib
    COMMAND $<$<CONFIG:RelWithDebInfo>:${CMAKE_COMMAND}> ARGS -E make_directory ${CMAKE_BINARY_DIR}/../relwithdebinfo${PONY_OUTPUT_SUFFIX}/test_lib
    COMMAND $<$<CONFIG:MinSizeRel>:${CMAKE_COMMAND}> ARGS -E make_directory ${CMAKE_BINARY_DIR}/../minsizerel${PONY_OUTPUT_SUFFIX}/test_lib

    COMMAND $<$<CONFIG:Debug>:${CMAKE_COMMAND}> ARGS -E copy $<TARGET_FILE:${test_lib_name}> ${CMAKE_BINARY_DIR}/../debug${PONY_OUTPUT_SUFFIX}/test_lib
    COMMAND $<$<CONFIG:Release>:${CMAKE_COMMAND}> ARGS -E copy $<TARGET_FILE:${test_lib_name}> ${CMAKE_BINARY_DIR}/../release${PONY_OUTPUT_SUFFIX}/test_lib
    COMMAND $<$<CONFIG:RelWithDebInfo>:${CMAKE_COMMAND}> ARGS -E copy $<TARGET_FILE:${test_lib_name}> ${CMAKE_BINARY_DIR}/../relwithdebinfo${PONY_OUTPUT_SUFFIX}/test_lib
    COMMAND $<$<CONFIG:MinSizeRel>:${CMAKE_COMMAND}> ARGS -E copy $<TARGET_FILE:${test_lib_name}> ${CMAKE_BINARY_DIR}/../minsizerel${PONY_OUTPUT_SUFFIX}/test_lib
  )
endforeach()
