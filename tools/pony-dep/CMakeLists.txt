project(tools.pony-dep)

# Generate version.pony from template
# (.gitignore prevents version.pony from dirtying the git tree)
configure_file(version.pony.in ${CMAKE_CURRENT_SOURCE_DIR}/version.pony @ONLY)

add_pony_binary(tools.pony-dep ALL
    NAME pony-dep
    SOURCE ${CMAKE_CURRENT_SOURCE_DIR}
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}
    WATCH ${CMAKE_CURRENT_SOURCE_DIR})
