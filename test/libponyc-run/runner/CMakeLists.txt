project(libponyc.tests.runner)

add_custom_target(runner ALL
  COMMAND $<TARGET_FILE:ponyc> --output ${CMAKE_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS ponyc
  SOURCES
    main.pony
    _build_process_notify.pony
    _cli_options.pony
    _coordinator.pony
    _find_executable.pony
    _test_definitions.pony
    _test_process_notify.pony
    _tester_timer_notify.pony
    _tester.pony
)
