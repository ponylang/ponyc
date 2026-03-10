## Add OpenBSD 7.8 as a tier 3 CI target

OpenBSD is now a tier 3 (best-effort) platform for ponyc. A weekly CI job builds and tests the compiler, standard library, and tools (pony-doc, pony-lint, pony-lsp) on OpenBSD 7.8.

The tools previously couldn't find the standard library on OpenBSD because they hardcoded their binary name when looking up the executable directory. This works on Linux, macOS, and FreeBSD, which have platform APIs that ignore argv0, but fails on OpenBSD where argv0 is the only mechanism for resolving the executable path. All three tools now pass the real `argv[0]` from the runtime.

`libponyc-standalone` is now built on OpenBSD, so programs can link against the compiler as a library on this platform.
