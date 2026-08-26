## Build ponyc with LTO when using a release preset

The `release`, `x86-64-release`, `armv8-release` and `armv8-a-release` CMake presets now set `PONY_USE_LTO`, which applies when you build ponyc with Clang. Building with GCC or MSVC skips LTO and says so at the configure step, because those toolchains' LTO objects are not in a form the linker embedded in ponyc can read. The Windows presets don't set it at all. On a 64-core x86-64 Linux machine, compiling `packages/stdlib` with the resulting compiler drops from 201 to 188 seconds, and the front end through reachability analysis from 59 to 48 seconds. Configure with `-DPONY_USE_LTO=OFF` to build without it.

`PONY_USE_LTO` covers the compiler and not the runtime. `libponyrt.a` and `libponyrt-pic.a` hold native objects whether or not it is set, so linking a Pony program costs the same either way.

`libponyc.a` and `libponyc-standalone.a` do hold bitcode when LTO is on. Embedding either archive then requires an LTO-capable linker.
