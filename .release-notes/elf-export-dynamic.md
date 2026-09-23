## Export PONY_API symbols on ELF platforms when runtime bitcode is merged

On ELF platforms (Linux, FreeBSD, DragonFly, OpenBSD), the linker flag `--export-dynamic` was only passed in debug builds of ponyc. With runtime bitcode now merged into the executable by default, LTO can internalize PONY_API functions that nothing in the program calls directly. A shared library loaded at runtime via FFI that calls PONY_API functions would fail to resolve them.

The ELF linker now passes `--export-dynamic` whenever runtime bitcode is merged, matching the macOS linker path.
