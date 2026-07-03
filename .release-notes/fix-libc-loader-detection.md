## Fix executables failing to start on Linux systems with both glibc and musl

On Linux systems that have both the GNU (glibc) and musl C libraries installed, `ponyc` could build a program that was linked against one C library but set up to load the other at startup. Compilation succeeded, but the program failed the moment you ran it, printing messages such as `Error loading shared library` or `symbol not found`.

`ponyc` now selects the startup loader that matches the C library your program is linked against. If you pass an explicit `--triple`, `ponyc` honors your choice instead of guessing from what is installed on the build machine.
