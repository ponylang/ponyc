# Build the vendored LLVM and support libraries (the lib/ CMake project).
#
# Runs the two cmake steps for the lib/ project -- configure it through a preset
# in lib/CMakePresets.json, then build and install it into build/libs. The build
# definition (LLVM targets, compiler, install layout) lives entirely in the
# presets; this file only sequences the two commands, so the CI libs cache -- which
# runs the build as a single command -- has one command to run. Developers run it
# the same way.
#
# Run from the repo root:
#   cmake -P lib/build-libs.cmake
#
# Options (pass with -D before -P):
#   -DPRESET=<name>  configure preset (default: libs; on Windows use
#                    libs-windows-x86-64 or libs-windows-arm64).
#   -DJOBS=<n>       parallel build jobs (default: 4; keep it low -- LLVM is
#                    memory-hungry).
#   -DCMAKE_C_COMPILER=<path> -DCMAKE_CXX_COMPILER=<path>  override the compiler
#                    (DragonFly builds LLVM with gcc, not the preset's clang).
#                    Same flag names the ponyc configure step uses, so one
#                    spelling overrides the compiler at either step.
#   -DPONY_PIC_FLAG=<flag>  position-independent-code flag (default -fpic; some
#                    64-bit ARM targets need -fPIC). Same flag name the ponyc
#                    configure step uses.

if(NOT DEFINED PRESET)
    set(PRESET "libs")
endif()
if(NOT DEFINED JOBS)
    set(JOBS "4")
endif()

set(_src "${CMAKE_CURRENT_LIST_DIR}")
set(_build "${CMAKE_CURRENT_LIST_DIR}/../build/build_libs")

set(_configure cmake --preset "${PRESET}" -S "${_src}")
if(DEFINED CMAKE_C_COMPILER)
    list(APPEND _configure "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}" "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}")
endif()
if(DEFINED PONY_PIC_FLAG)
    list(APPEND _configure "-DPONY_PIC_FLAG=${PONY_PIC_FLAG}")
endif()

execute_process(COMMAND ${_configure} RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "libs configure failed (exit ${_rc})")
endif()

execute_process(
    COMMAND cmake --build "${_build}" --config Release --target install --parallel "${JOBS}"
    RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "libs build failed (exit ${_rc})")
endif()
