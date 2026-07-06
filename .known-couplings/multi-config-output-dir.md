# Per-config output dir: ctest registrations + ponyc-compiled binaries ↔ the Windows multi-config generator

On the Windows Visual Studio generator the build is **multi-config**: one configure
into `build/build`, and Debug/Release chosen at build time. `CMAKE_BUILD_TYPE` is
empty there, so the plain `CMAKE_RUNTIME_OUTPUT_DIRECTORY`
(`CMakeLists.txt:148` = `${CMAKE_BINARY_DIR}/../${CMAKE_BUILD_TYPE}${PONY_OUTPUT_SUFFIX}`)
is **config-less** — it resolves to `build/` with no debug/release component.

Two kinds of binary must nonetheless land in the same per-config directory:
- **Real MSBuild targets** (ponyc, libponyrt.tests, libponyc.tests, ponyc-shim)
  go to `build/debug` / `build/release` via
  `CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG` / `_RELEASE` (`CMakeLists.txt:144-145`,
  lowercase `debug`/`release`).
- **ponyc-compiled binaries** (the pony-doc/lint/lsp tools, the pony-*-tests, the
  stdlib-* and full-program test artifacts) go wherever ponyc's `-o` points.

So everything that locates a ponyc-compiled binary, sets a ctest working dir, or
tells ponyc where to write, must use the **per-config** path, not the config-less
`CMAKE_RUNTIME_OUTPUT_DIRECTORY`. That path is `PONY_OUTPUT_DIR`
(`CMakeLists.txt:160`):

    ${CMAKE_BINARY_DIR}/../$<LOWER_CASE:$<CONFIG>>${PONY_OUTPUT_SUFFIX}

Consumers that depend on this and must stay in step:
- The ctest registrations in `CMakeLists.txt` (`add_test` COMMAND/WORKDIR, the
  full-program `-DOUTPUT`/`-DTEST_LIB`, the pony-*-tests binary paths).
- `cmake/PonyBinary.cmake` (`add_pony_binary`): the ponyc `-o` and the
  `add_custom_command` OUTPUT for every pony-*-tests binary.
- `tools/pony-lsp/CMakeLists.txt`, `tools/pony-lint/CMakeLists.txt`,
  `tools/pony-doc/CMakeLists.txt`: the tool executables' `-o` and OUTPUT.
- The install of the tool executables (`CMakeLists.txt`, `install(PROGRAMS
  $<TARGET_FILE_DIR:ponyc>/pony-lsp...)`): it reads them from
  `$<TARGET_FILE_DIR:ponyc>` = the same per-config dir, so the tools must be built
  there or `cmake --install` finds nothing.

Two rules for the genex:
- It **must** include `${PONY_OUTPUT_SUFFIX}` and use `$<LOWER_CASE:$<CONFIG>>`
  (lowercase, to match the `_DEBUG`/`_RELEASE` dirs), or a `use=` build (suffixed
  output dir) or a case mismatch splits the binaries from where the tests look.
- `$<TARGET_FILE_DIR:ponyc>` is a valid substitute in an `add_test` COMMAND, but
  **not** in `add_custom_command(OUTPUT ...)` (a target genex there fails at
  generate time), so the custom-command sites use the `$<CONFIG>` form.

Why this is easy to break unseen: on a **single-config** generator (the Unix
Makefiles/Ninja presets) `CMAKE_BUILD_TYPE` is set, so
`CMAKE_RUNTIME_OUTPUT_DIRECTORY` already equals the per-config dir and equals
`PONY_OUTPUT_DIR`. A change that reverts one of these sites to
`CMAKE_RUNTIME_OUTPUT_DIRECTORY` passes every Unix job and only breaks on Windows,
where the binary lands in `build/` while ctest/install look in `build/<config>`.

Run: on a Windows host, `ctest --preset windows-x86-64-debug -L ci-core` and
`ctest --preset windows-x86-64-debug -R pony-<tool>-tests` (after building the
`pony-<tool>-tests` targets), plus `cmake --install build/build --config Debug`
to confirm the tools install.
