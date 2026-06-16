## Fix `use=coverage` builds failing to link with gcc

Building ponyc with `use=coverage` under a gcc toolchain produced a compiler that couldn't link the Pony programs it compiled: every link failed with `undefined symbol: __gcov_init` (and `__gcov_exit`). Because DragonFly BSD only builds ponyc with gcc, coverage builds were unusable there entirely.

Coverage builds under gcc now link correctly on every platform where coverage is supported.
