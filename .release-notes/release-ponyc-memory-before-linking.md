## Release ponyc's memory before linking

On Linux, macOS, and the BSDs, ponyc's memory is now freed before the final link starts, so peak memory use during a build is lower. On the optimized stdlib test build on macOS arm64, combined peak memory dropped from 17.8 GB to about 11 GB. On Linux x86-64, it dropped from about 9.0 GB to 6.4 GB. Memory use on Windows is unchanged.

To do this, `ponyc` is now a small program that runs the compiler, a new binary named `ponyc-compiler`, and then runs the linker, `ponylink`, after the compiler has exited. If you package ponyc, `ponyc`, `ponyc-compiler`, and `ponylink` must be installed together. Installing with `cmake --install` or ponyup already does this.

To run the compiler under a debugger, valgrind, or a profiler, run `ponyc-compiler` instead of `ponyc`. It is in the same directory as `ponyc`: `<prefix>/lib/pony/<version>/bin/`. Running `ponyc` under one of those tools shows only the small program that starts the compiler.

Some output now comes in a different order for the final link, which is the only link in a single-package build. `--stats` output prints before the linker's output, a plugin's final hook runs before the link, and the file written by `--pass-timings-json` doesn't include the link's status or time.

If `ponyc` is killed with SIGKILL, the compiler is killed with it on Linux and FreeBSD. On macOS, OpenBSD, and DragonFly, the compiler runs to the end, then removes the link's temporary files and exits without linking.
