# `pass_opt_t` struct layout (`src/libponyc/pass/pass.h`)

The Pony tools (`pony-compiler`, `pony-lint`, `pony-lsp`, `pony-doc`) reach libponyc's options through an FFI struct `_PassOpt` in `tools/lib/ponylang/pony_compiler/pony_compiler/pass.pony`, which must have the same memory layout as the C `pass_opt_t`. Adding, removing, or reordering a field in `pass_opt_t` — even a bool that currently lands in existing padding — must be mirrored in `_PassOpt`, or a later field addition shifts every offset below it and the tools read garbage. `_PassOpt` keeps `strtab` last and `llvm_args` unconditional for exactly this reason (see the comments there).

This is the struct-layout sibling of the `pass_id` enum coupling in `pass-id-enum.md`; both live in `pass.h` and both mirror into `pass.pony`.

Run: `cmake --build --preset debug --target pony-compiler-tests pony-lint-tests pony-lsp-tests pony-doc-tests && ctest --preset debug -R 'pony-.*-tests'`.
