# Build-time script: recompute the version string on every build so that the
# git hash and VERSION file stay current without a manual reconfigure.
#
# Expected -D arguments (set by the custom target in the top-level CMakeLists):
#   SOURCE_DIR         – path to the source tree (CMAKE_SOURCE_DIR)
#   OUTPUT_FILE        – path to write the generated header to
#   TEMPLATE_FILE      – path to pony_version.h.in
#   LLVM_VERSION       – LLVM version string
#   CMAKE_C_COMPILER_ID      – compiler family (GNU, Clang, …)
#   CMAKE_C_COMPILER_VERSION – compiler version
#   COMPILER_ARCH      – target architecture
#   OVERRIDE_VERSION   – optional; when set, skip VERSION/git and use this

if(DEFINED OVERRIDE_VERSION AND NOT OVERRIDE_VERSION STREQUAL "")
    set(PONYC_VERSION "${OVERRIDE_VERSION}")
else()
    file(STRINGS "${SOURCE_DIR}/VERSION" PONYC_VERSION)

    if(EXISTS "${SOURCE_DIR}/.git")
        execute_process(
            COMMAND git rev-parse --short HEAD
            WORKING_DIRECTORY "${SOURCE_DIR}"
            OUTPUT_VARIABLE _hash
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE _rc
        )
        if(_rc EQUAL 0 AND NOT _hash STREQUAL "")
            set(PONYC_VERSION "${PONYC_VERSION}-${_hash}")
        endif()
    endif()
endif()

configure_file("${TEMPLATE_FILE}" "${OUTPUT_FILE}" @ONLY)
