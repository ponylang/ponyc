## Fix crash when compiling with --runtimebc

Programs compiled with `--runtimebc` could crash at runtime with a segfault. The runtime bitcode was being built with debug-mode settings instead of release-mode, and with a mismatched target triple that produced a linker warning on every `--runtimebc` compile.

Both are fixed.
