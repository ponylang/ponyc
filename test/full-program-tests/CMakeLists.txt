project(libponyc.tests.run VERSION ${PONYC_PROJECT_VERSION} LANGUAGES C)

# External-lib fixtures keep their C source in <test>/c-src/; it is built
# here into a <test>-additional library that the test links with
# `use "lib:<test>-additional"`. Only c-src/ is globbed, deliberately: a C
# file in a test's top directory is a shim that ponyc itself discovers and
# compiles (see gencshim), and building it here too would shadow the
# `use "lib:"` path the external-lib tests exist to cover.
# Assumes that there is only 1 C source file per test
file(GLOB test_c_sources RELATIVE ${CMAKE_CURRENT_LIST_DIR} CONFIGURE_DEPENDS */c-src/*.c */c-src/*.cc)

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
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PONY_OUTPUT_DIR}/test_lib
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${test_lib_name}> ${PONY_OUTPUT_DIR}/test_lib
  )
endforeach()
