# Codegen hand-mirrors libponyrt's PONY_API signatures

The compiler does not read `pony.h`. Every runtime function a generated program
calls is hand-declared in `src/libponyc/codegen/codegen.c` (the LLVM prototype,
e.g. `LLVMAddFunction(c->module, "pony_start", ...)`) and called from the
`gen*.cc` files (e.g. `gencall_runtime(c, "pony_start", args, n, "")` in
`genexe.cc`). These prototypes and call sites must match the actual C signature
of the function in libponyrt.

Change a runtime PONY_API signature — add/remove a parameter, reorder, or change
a type — and update only the runtime, and nothing local fails: libponyrt's own
unit tests pass and codegen still compiles. The break surfaces only when a
generated program runs, because the linker resolves C symbols by name without
checking arity or types, so the generated `main` passes arguments the runtime
reads from the wrong registers. The failure can be silent rather than a crash:
e.g. dropping `pony_start`'s leading `library` param without re-shifting the
codegen call would feed the language-features pointer in where `exit_code` is
expected, so networking initialisation would quietly never happen.

Keep the prototype in `codegen.c`, the call in the relevant `gen*.cc`, and the
runtime definition in lockstep, in the same change.

Run: `ctest --preset debug -R 'libponyc\.tests|libponyrt\.tests|full-programs'`
(full-program tests link generated code against libponyrt)
and `ctest --preset debug -R stdlib-release` (exercises the runtime API breadth,
including the network path that depends on `pony_start`'s language-features
argument).
