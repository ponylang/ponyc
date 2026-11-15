project(tools.pony-mutate)

add_pony_binary(tools.pony-mutate ALL
    NAME pony-mutate
    SOURCE ${CMAKE_CURRENT_SOURCE_DIR}
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}
    WATCH ${CMAKE_CURRENT_SOURCE_DIR})
