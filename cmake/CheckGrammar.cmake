# ctest wrapper: regenerate the ANTLR grammar with the freshly built ponyc and
# compare it against the committed pony.g. Mirrors the Makefile's
# test-validate-grammar. Fails if the generated grammar has drifted from the
# checked-in file, meaning pony.g needs regenerating.
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

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${GRAMMAR}" "${WORKDIR}/pony.g.new"
    RESULT_VARIABLE _diff)
if(NOT _diff EQUAL 0)
    message(FATAL_ERROR
        "generated grammar differs from ${GRAMMAR}; regenerate with 'ponyc --antlr'")
endif()
