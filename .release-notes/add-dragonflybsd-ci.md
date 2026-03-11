## Add DragonFly BSD 6.4.2 as a tier 3 CI target

DragonFly BSD is now a tier 3 (best-effort) platform for ponyc. A weekly CI job builds and tests the compiler, standard library, and tools (pony-doc, pony-lint, pony-lsp) on DragonFly BSD 6.4.2.

`libponyc-standalone` is now built on DragonFly BSD, so programs can link against the compiler as a library on this platform. The pony_compiler library and the ffi-libponyc-standalone test previously excluded DragonFly BSD entirely.
