## Enable LTO by default on Linux

Release builds on Linux with Clang now use link-time optimization automatically. The system Clang and ponyc's vendored LLVM both use upstream LLVM bitcode, so global LTO works without cross-toolchain mismatches. This applies to the compiler itself and to libponyrt, which is linked into every Pony program.

To disable LTO, pass `-DPONY_USE_LTO=OFF` at configure time.
