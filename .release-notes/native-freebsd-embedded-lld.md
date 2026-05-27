## Use embedded LLD for native FreeBSD builds

FreeBSD linking now uses the embedded LLD linker (ELF driver) instead of invoking an external compiler driver via `system()`. You no longer need a C compiler installed solely to link Pony programs on FreeBSD. This is part of the ongoing work to eliminate external linker dependencies across all platforms.

The embedded LLD path activates automatically for any FreeBSD target without `--linker` set. To use an external linker instead, pass `--linker=<command>` as an escape hatch to the legacy linking path. The `--link-ldcmd` flag is ignored when using embedded LLD; use `--linker` instead to get legacy behavior.
