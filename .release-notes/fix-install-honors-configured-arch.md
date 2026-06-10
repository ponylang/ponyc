## Make `make install` honor the architecture chosen at `make configure`

Previously, the Unix Makefile's `install` target built its destination from
make's `arch` variable, which defaults to `native` on every invocation. The
build, however, was configured against the `PONY_ARCH` cached by CMake at
`make configure` time. Running `make configure arch=<non-default>`, `make
build`, then `make install` (without re-passing `arch=`) installed the runtime
libraries into `lib/native/` while the compiler — which looks for its runtime
at `lib/<arch>` using the architecture it was compiled with — searched
elsewhere, producing an installation that could not find its own libraries.

The `install` target now delegates to `cmake --install`, and the install rules
place the runtime into `lib/${PONY_ARCH}` (a flat `lib/` on Windows, matching
the compiler's platform-specific lookup). `PONY_ARCH`, set once at configure
time, is the single source of truth for both the compiled-in lookup path and
the install destination, so the install location can no longer drift from what
the build configured.
