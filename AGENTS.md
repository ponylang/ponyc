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

## Testing the self-hosted tools

The build uses CMake presets (see BUILD.md). The self-hosted tools' test binaries — for the compiler, pony-lsp, pony-lint, and pony-doc — are **built on demand**: a normal `cmake --build` does not build them. Build the test target, then run it through ctest:

- `cmake --build --preset debug --target pony-compiler-tests && ctest --preset debug -R pony-compiler-tests`
- `cmake --build --preset debug --target pony-lsp-tests && ctest --preset debug -R pony-lsp-tests`
- `cmake --build --preset debug --target pony-lint-tests && ctest --preset debug -R pony-lint-tests`
- `cmake --build --preset debug --target pony-doc-tests && ctest --preset debug -R pony-doc-tests`

The first build of a test binary compiles from Pony source (~60s); later runs skip the rebuild when nothing under its source tree changed. On Windows, use the `windows-x86-64-debug` preset the same way.

To lint a tool's own source, run the `pony-lint` binary (built by a normal `cmake --build --preset debug`) against the tool's directory:

```
cd build/debug && PONYPATH=../../tools/lib/ponylang/pony_compiler ./pony-lint ../../tools/pony-lint/
```

The same works for `../../tools/pony-lsp/` and `../../tools/pony-doc/`.

## Release notes

Load the `/pony-release-notes` skill for the full procedure. The one thing to know without it: add a `.release-notes/<slug>.md` file for each user-facing PR — do **not** edit `next-release.md` directly, because CI aggregates the individual files.
