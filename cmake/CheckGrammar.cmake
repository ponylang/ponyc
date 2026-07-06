# ctest wrapper: regenerate the ANTLR grammar with the freshly built ponyc and
# compare it against the committed pony.g. Fails if the generated grammar has
# drifted from the checked-in file, meaning pony.g needs regenerating.
#
# Args (passed with -D): PONYC, GRAMMAR (path to pony.g), WORKDIR (where to write
# the regenerated grammar).

execute_process(
    COMMAND "${PONYC}" --antlr
    OUTPUT_FILE "${WORKDIR}/pony.g.new"
    RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "ponyc --antlr failed (exit ${_rc})")
endif()

# --ignore-eol: ponyc's --antlr output is captured through a text-mode stdout on
# Windows, so its line endings are CRLF while the committed pony.g is LF. The
# grammar content is what matters, not the line endings, so compare ignoring them.
# On Unix both are already LF, so this changes nothing there.
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files --ignore-eol "${GRAMMAR}" "${WORKDIR}/pony.g.new"
    RESULT_VARIABLE _diff)
if(NOT _diff EQUAL 0)
    message(FATAL_ERROR
        "generated grammar differs from ${GRAMMAR}; regenerate with 'ponyc --antlr'")
endif()
