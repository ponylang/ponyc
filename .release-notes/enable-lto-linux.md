## Enable LTO by default on Linux and Windows

Release builds on Linux with Clang and on Windows with MSVC now use link-time optimization automatically. On Linux, the system Clang and ponyc's vendored LLVM both use upstream LLVM bitcode, so global LTO works without cross-toolchain mismatches. On Windows, MSVC's whole-program optimization (`/GL` + `/LTCG`) is used.

To disable LTO, pass `-DPONY_USE_LTO=OFF` at configure time.
