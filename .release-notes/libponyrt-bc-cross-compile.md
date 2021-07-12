## Allow cross-compiling Pony runtime bitcode target

This change allows one to cross-compile the `libponyrt.bc` target in CMake, so that you can compile the runtime for a platform different from the one where you are compiling it.

Setting the following CMake variables will enable cross-compilation of the `libponyrt.bc` target:
- `CMAKE_C_COMPILER_TARGET` - the target triple (e.g. `arm64-apple-macosx`)
- `CMAKE_SYSTEM_NAME` - the operating system name (e.g. `Darwin`)
- `CMAKE_SYSTEM_PROCESSOR` - the processor type (e.g. `arm64`)
