project(tools.pony-lsp)

# Generate version.pony from template
# (.gitignore prevents version.pony from dirtying the git tree)
configure_file(version.pony.in ${CMAKE_CURRENT_SOURCE_DIR}/version.pony @ONLY)

# TODO STA: the PATHS entries are temporary until a ponyc bug is fixed.
add_pony_binary(tools.pony-lsp ALL
    NAME pony-lsp
    SOURCE ${CMAKE_CURRENT_SOURCE_DIR}
    PATHS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../lib/ponylang/pony_compiler
    WATCH ${CMAKE_CURRENT_SOURCE_DIR})
