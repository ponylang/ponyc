project(libponyc.tests.run VERSION ${PONYC_PROJECT_VERSION} LANGUAGES C)

# Assumes that there is only 1 C source file per test
file(GLOB_RECURSE test_c_sources RELATIVE ${CMAKE_CURRENT_LIST_DIR} *.c)
foreach(test_c_source ${test_c_sources})
  string(FIND ${test_c_source} "/" slash_pos)
  string(SUBSTRING ${test_c_source} 0 ${slash_pos} test_lib_name)

  if(MSVC)
    add_library(${test_lib_name} STATIC ${test_c_source})
    target_link_libraries(${test_lib_name}
      PRIVATE $<TARGET_FILE:libponyrt>
      PRIVATE ucrt
      PRIVATE Ws2_32
      PRIVATE Shlwapi
    )
  else()
    add_library(${test_lib_name} SHARED ${test_c_source})
    target_link_libraries(${test_lib_name} PRIVATE $<TARGET_FILE:libponyrt>)
  endif()

  add_custom_command(TARGET ${test_lib_name} POST_BUILD
    COMMAND $<$<CONFIG:Debug>:${CMAKE_COMMAND}> ARGS -E make_directory ${CMAKE_BINARY_DIR}/../debug/test_lib
    COMMAND $<$<CONFIG:Release>:${CMAKE_COMMAND}> ARGS -E make_directory ${CMAKE_BINARY_DIR}/../release/test_lib
    COMMAND $<$<CONFIG:RelWithDebInfo>:${CMAKE_COMMAND}> ARGS -E make_directory ${CMAKE_BINARY_DIR}/../relwithdebinfo/test_lib
    COMMAND $<$<CONFIG:MinSizeRel>:${CMAKE_COMMAND}> ARGS -E make_directory ${CMAKE_BINARY_DIR}/../minsizerel/test_lib

    COMMAND $<$<CONFIG:Debug>:${CMAKE_COMMAND}> ARGS -E copy $<TARGET_FILE:${test_lib_name}> ${CMAKE_BINARY_DIR}/../debug/test_lib
    COMMAND $<$<CONFIG:Release>:${CMAKE_COMMAND}> ARGS -E copy $<TARGET_FILE:${test_lib_name}> ${CMAKE_BINARY_DIR}/../release/test_lib
    COMMAND $<$<CONFIG:RelWithDebInfo>:${CMAKE_COMMAND}> ARGS -E copy $<TARGET_FILE:${test_lib_name}> ${CMAKE_BINARY_DIR}/../relwithdebinfo/test_lib
    COMMAND $<$<CONFIG:MinSizeRel>:${CMAKE_COMMAND}> ARGS -E copy $<TARGET_FILE:${test_lib_name}> ${CMAKE_BINARY_DIR}/../minsizerel/test_lib
  )
endforeach()
