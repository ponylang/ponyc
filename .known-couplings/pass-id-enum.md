# `pass_id` enum (`src/libponyc/pass/pass.h`)

Adding or reordering values renumbers every subsequent pass id and silently breaks any code matching on specific pass ids. The Pony-side mirror in `tools/lib/ponylang/pony_compiler/pony_compiler/pass.pony` must be kept in sync. Run the same suites as for token changes (see `core-ast-token-definitions.md`): `cmake --build --preset debug --target pony-compiler-tests pony-lint-tests pony-lsp-tests pony-doc-tests && ctest --preset debug -R 'pony-.*-tests'`.
