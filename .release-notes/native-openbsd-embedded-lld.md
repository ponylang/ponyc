## Use embedded LLD for native OpenBSD builds

OpenBSD linking now uses the embedded LLD linker (ELF driver) instead of invoking an external compiler driver via `system()`. You no longer need to invoke a C compiler driver solely to link Pony programs on OpenBSD; the base toolchain's `clang` is still required to build ponyc itself, but ponyc no longer shells out to it at link time. This is part of the ongoing work to eliminate external linker dependencies across all platforms.

The embedded LLD path activates automatically for any OpenBSD target without `--linker` set. To use an external linker instead, pass `--linker=<command>` as an escape hatch to the legacy linking path. The `--link-ldcmd` flag is ignored when using embedded LLD; use `--linker` instead to get legacy behavior.
