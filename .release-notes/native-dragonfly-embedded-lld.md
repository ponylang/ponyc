## Use embedded LLD for native DragonFly builds

DragonFly linking now uses the embedded LLD linker (ELF driver) instead of invoking an external C compiler driver via `system()`. Pkg gcc (e.g. `gcc13`) is still required at link time to supply libgcc, libgcc_s, libatomic, and the GCC CRT objects — the same install BUILD.md already prescribes for building ponyc on DragonFly. This is part of the ongoing work to eliminate external linker dependencies across all platforms.
