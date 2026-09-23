# Assert libponyrt.bc was produced. A clang build with PONY_RUNTIME_BITCODE=ON
# must produce this file; if CMake misconfiguration silently prevents it, the
# graceful fallback in codegen_merge_runtime_bitcode masks the problem.
if(NOT EXISTS "${DIR}/libponyrt.bc")
    message(FATAL_ERROR "libponyrt.bc is not in ${DIR}")
endif()
