# Assert the ponyc-compiled tools sit in the same directory as the ponyc binary.
# install() reads them from ponyc's directory (the install(PROGRAMS ...) rules in
# the top-level CMakeLists), so a build that writes them anywhere else installs a
# tree with no tools. DIR is ponyc's directory; SUFFIX is the platform executable
# suffix.
foreach(_tool pony-lsp pony-lint pony-doc)
    if(NOT EXISTS "${DIR}/${_tool}${SUFFIX}")
        message(FATAL_ERROR
            "${_tool}${SUFFIX} is not in ${DIR}, where ponyc is and install() reads it")
    endif()
endforeach()
