# ponyc

<!-- contributor-only -->
## Contributing with an AI assistant

This is a Pony project. The ponylang org maintains a set of LLM coding skills. Get set up with them before contributing:

- **Not set up yet?** Install them once:

  ```bash
  git clone https://github.com/ponylang/llm-skills.git
  cd llm-skills
  python install.py
  ```

- **Already set up?** Make sure you're on the latest. If you installed with the script above, `git pull` in the directory where you cloned `llm-skills` and the symlinked skills update automatically — if you set them up another way, refresh them however that setup expects.

See the [llm-skills README](https://github.com/ponylang/llm-skills) for details and other harnesses.

When you start working on this project, load the `pony-skills` skill — it tells your assistant which Pony skill to use for each task.

Read [CONTRIBUTING.md](CONTRIBUTING.md).
<!-- /contributor-only -->

## Building

The build uses CMake presets. See [BUILD.md](BUILD.md) for platform-specific instructions and build options.

Build ponyc in debug mode:

```bash
cmake --build --preset debug
```

The output goes in `build/debug`. Use `--preset release` for a release build. The vendored LLVM libraries must be built first with `cmake -P lib/build-libs.cmake` — this only needs to run once (or when the LLVM submodule changes).

## Testing

Tests are registered with ctest and grouped by label. Two labels matter:

- **`ci-core`** — built by a normal `cmake --build --preset debug`. The C/C++ compiler tests (`libponyc.tests`), the runtime tests (`libponyrt.tests`), the stdlib suite, full-program integration tests, example compilation, and grammar validation.
- **`tools`** — **not** built by a normal `cmake --build --preset debug`. The self-hosted tool test suites: pony-compiler, pony-lsp, pony-lint, pony-doc, and pony-dep.

### Core tests (ci-core)

Run the full core suite:

```bash
ctest --preset debug -L ci-core
```

Run individual tests by name:

- `ctest --preset debug -R libponyc.tests` — compiler C/C++ unit tests (GTest)
- `ctest --preset debug -R libponyrt.tests` — runtime C/C++ unit tests (GTest)
- `ctest --preset debug -R stdlib-debug` — stdlib test suite, compiled and run in debug mode
- `ctest --preset debug -R stdlib-release` — stdlib test suite, release mode
- `ctest --preset debug -R full-programs-debug` — compile-and-run integration tests, debug mode
- `ctest --preset debug -R full-programs-release` — compile-and-run integration tests, release mode
- `ctest --preset debug -R ^example/` — compiles all examples (each is a separate test named `example/<path>`)
- `ctest --preset debug -R validate-grammar` — checks `pony.g` against the compiler

#### Per-package stdlib tests

A single package's tests can be compiled and run without rebuilding the full stdlib suite:

```bash
cd build/debug && ./ponyc -d -b stdlib-debug --checktree -Dopenssl_3.0.x --pic --strip ../../packages/collections
./stdlib-debug --sequential
```

The SSL flag must match the installed SSL library: `-Dopenssl_3.0.x` for OpenSSL 3.x, `-Dopenssl_1.1.x` for OpenSSL 1.1.x, `-Dlibressl` for LibreSSL. CMake detects this automatically for `ctest` runs; the manual command needs it explicitly.

For a release build, drop `-d` and `--strip`.

### Tool tests

Tool test binaries must be built explicitly before running. Build one target, then run through ctest:

- `cmake --build --preset debug --target pony-compiler-tests && ctest --preset debug -R pony-compiler-tests`
- `cmake --build --preset debug --target pony-lsp-tests && ctest --preset debug -R pony-lsp-tests`
- `cmake --build --preset debug --target pony-lint-tests && ctest --preset debug -R pony-lint-tests`
- `cmake --build --preset debug --target pony-doc-tests && ctest --preset debug -R pony-doc-tests`
- `cmake --build --preset debug --target pony-dep-tests && ctest --preset debug -R pony-dep-tests`

Build all tool test binaries at once with `cmake --build --preset debug --target tool-tests`, then run them with `ctest --preset debug -L tools`.

The first build of a tool test binary compiles from Pony source (~60s); the binary is not recompiled when nothing under its source tree changed. On Windows, use the `windows-x86-64-debug` preset the same way.

### Linting tool source

Run the `pony-lint` binary (built by a normal `cmake --build --preset debug`) against a tool's directory:

```bash
cd build/debug && PONYPATH=../../tools/lib/ponylang/pony_compiler ./pony-lint ../../tools/pony-lint/
```

The same works for `../../tools/pony-lsp/`, `../../tools/pony-doc/`, and `../../tools/pony-dep/`.

## Adding threads or locks

Never add a mutex or a new thread unless the idea came from the human operator or a committer expressly approved it. Pony's runtime is built on a deliberate concurrency model, and adding either is an architectural decision, not an implementation detail. If a change looks like it needs one, stop and raise it rather than writing it.

## Declaring runtime FFI functions on Windows

On the Windows MSVC build, libponyrt's `.c` files compile as C++ (`src/libponyrt/CMakeLists.txt` sets `LANGUAGE CXX` on them). A `PONY_API` function whose definition is not inside a `PONY_EXTERN_C_BEGIN`/`PONY_EXTERN_C_END` region gets a C++-mangled symbol, which the stdlib FFI — referencing the plain C name — can't resolve at link (`lld-link: undefined symbol`). Linux, macOS, and BSD compile `.c` as C, so the failure is Windows-only and won't show up in a local build there. Every runtime `.c` already wraps its definitions in that region (see `lang/stat.c`, `lang/socket.c`); put any new `PONY_API` function inside it. To check a symbol's linkage, run `dumpbin /SYMBOLS <lib> | findstr <name>` from a vcvars64 shell: a C-linkage function shows the plain name, a mangled one shows the C++ form.

## Dispatching workflows on a branch

`gh workflow run <workflow> --ref <branch> -f ref=<branch>` — both flags are needed. `--ref` selects which branch the workflow YAML is read from; `-f ref=` sets the `inputs.ref` that the checkout step uses. Without `-f ref=`, the job checks out main regardless of `--ref`.
