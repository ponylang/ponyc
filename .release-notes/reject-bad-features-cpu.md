## Report an error for malformed `--features` and invalid `--cpu` values

Some bad `--features` and `--cpu` values crashed ponyc partway through compiling instead of producing an error. A `--features` string with an empty entry, such as `--features=+sse4.2,`, or with an entry that doesn't start with `+` or `-`, such as `--features=cx16` or `"--features=+sse4.2, +avx"`, aborted with the same LLVM message printed hundreds of times. On x86-64, a `--cpu` name that wasn't a processor for the target, such as `--cpu=bogus` or the 32-bit-only `--cpu=athlon-xp`, stopped with an LLVM fatal error. On RISC-V, a CPU for the other bit width, such as `--cpu=generic-rv32` with a 64-bit triple, also stopped with an LLVM fatal error.

ponyc now checks both options before it compiles anything and prints an error that names the bad value. For `--features`, ponyc checks that each entry is non-empty and starts with `+` or `-`; it doesn't check which features are being turned on or off.

```
Error:
invalid --features value "cx16": feature "cx16" must start with '+' or '-'
```

```
Error:
invalid --cpu value "bogus": not a recognized processor for target x86_64-unknown-linux-gnu
```

An empty `--features=` and an empty `--cpu=` are still accepted. A feature name that LLVM doesn't know, such as `+notafeature`, is still accepted with a warning, as before.

Some `--cpu` values that used to be accepted are now errors:

- On targets other than x86, such as ARM, AArch64, and RISC-V, an unrecognized `--cpu` name used to print an LLVM warning and build for the target's default processor. It is now rejected.
- On x86, `--cpu=generic` is now rejected. It already failed for any program with C shims. To build for the baseline x86-64 processor, use `--cpu=x86-64`.
- On RISC-V, `--cpu=generic` and other names that only set tuning, such as `rocket`, are now rejected. Use `--cpu=generic-rv64` or `--cpu=generic-rv32` instead.
- On every target, `--cpu=help` is now rejected instead of printing LLVM's CPU list.
