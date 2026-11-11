## Fix building on 32-bit

The runtime failed to compile on 32-bit ARM with GCC 8.3 due to two issues: implicit narrowing conversions from `int` to `uint16_t` in actor batch-size arithmetic, and a call to `gettid()` whose glibc wrapper is only available in glibc 2.30 and later.

The narrowing conversions are caused by C integer promotion: compound assignments like `batch_size -= 25` promote both operands to `int`, and the implicit conversion back to `uint16_t` triggers `-Werror=conversion`. The `gettid()` call is replaced with `syscall(SYS_gettid)`, which works on all glibc versions.

## Add i386 (32-bit x86) support to the embedded LLD linker

The embedded LLD linker can now link Pony programs targeting 32-bit x86 Linux (i386/i686). Previously, compiling for an i386 target failed at link time with "unsupported architecture for embedded LLD." The embedded linker already supported 32-bit ARM; this extends the same support to 32-bit x86, covering both musl and glibc systems.

## Fix PosixDate on 32-bit

On systems with 32-bit `time_t` (such as 32-bit ARM Linux), every `PosixDate` produced wrong field values regardless of the input. `PosixDate(0, 0)` returned year 1905 instead of 1970, and `time()` returned `-2037645953` instead of `0`.

The upper bound for dates is now clamped to the `time_t` range on each platform. On 32-bit `time_t` systems, this means the maximum representable date is 2038-01-19 03:14:07 UTC (the Y2038 limit) rather than year 2147483647.

## Fix short-circuit `and`/`or` dropping side effects when left operand is a compile-time constant

When the left operand of a short-circuit `and` or `or` folded to a compile-time constant, ponyc dropped the code that computed it. Any side effects in that left operand — assignments, field writes — never ran, and the program silently produced wrong results.

```pony
var x: U32 = 1
let r = ((x = x + 1; true) and (x > 2))
// x was 1 (the assignment never ran); now x is 2
```

A related shape — a nested short-circuit that folds to a constant, used as the left operand of an outer `and` or `or` — crashed the compiler instead of producing a wrong answer.

## Fix F64.min() and F64.max() on ARM32

`F64.min()` and `F64.max()` returned incorrect results on ARM32 — both returned the maximum of their two arguments regardless of which was called. They now produce correct results, preserving IEEE 754-2019 minimum/maximum semantics for NaN and signed zero.

## Fix incorrect constant folding of I128 division and shift operations

Constant I128 division and remainder gave wrong results when the divisor was a 128-bit value whose low 64 bits were all ones — for example, `0xFFFF_FFFF_FFFF_FFFF` (which is 2^64−1, not −1). Such divisors were truncated to −1, producing the signed-overflow result instead of the actual quotient.

```pony
// Before: d was I128.min_value(), r was 0
// After: d and r are both -9223372036854775808
let d: I128 = I128.min_value() / 0xFFFF_FFFF_FFFF_FFFF
let r: I128 = I128.min_value() % 0xFFFF_FFFF_FFFF_FFFF
```

Constant 128-bit shift amounts above 2^64 were also accepted when they should have been rejected as exceeding the type width.

```pony
// Before: compiled without error, produced undefined behavior
// After: "shift amount greater than type width" compile error
let a: U128 = 1 << 0x1_0000_0000_0000_0005
```

## Fix `ponyc -r=ir` writing builtin's IR over the program's IR

When emitting LLVM IR with `-r=ir`, ponyc wrote both the program's module and builtin's module to the same `.main.ll` file. Builtin was written second, so the file contained only builtin's definitions — the program's IR was lost. Each module is now written to a distinct file: `<name>.main.ll` for the program and `<name>.builtin.ll` for builtin.

## Fix user package named `main` colliding with program module at link

A program with a sub-package in a directory named `main` (or `Main`, `MAIN`, or any other casing) failed to link. The compiler assigned duplicate `.bc` filenames to the user package and the program's own module, producing a linker error. The compiler now detects the collision and disambiguates the user package's filename.

## Fix time functions truncating to 32 bits on 32-bit musl Linux

On 32-bit musl Linux (such as Alpine on i386 or ARM), `Time.seconds()`, `Time.now()`, and file timestamp operations (`FilePath.set_time`, `Directory.set_time_at`, `File.set_time`) silently truncated time values to 32 bits. File timestamps past January 2038 wrapped to negative values, and after 2038, `Time.seconds()` and `Time.now()` would return wrong results.

musl defines `time_t` as 64-bit on all architectures, but the plain C symbol names (`time`, `clock_gettime`, `utimes`, `futimes`, `futimesat`) are 32-bit compatibility wrappers on 32-bit targets. C code compiled with musl's headers gets the 64-bit versions automatically; Pony's FFI declarations bypassed the headers and bound the 32-bit wrappers directly.

Time values now use the full 64-bit range on 32-bit musl. On glibc 32-bit systems, `time_t` is still 32 bits at the C level, so the Y2038 limit remains there — that is a glibc constraint, not a Pony one.

## Fix floating-point arithmetic giving wrong results on 32-bit x86 without SSE2

On 32-bit x86 targets whose CPU lacked SSE2, floating-point arithmetic could produce incorrect results that violated IEEE 754. ponyc now unconditionally enables SSE2 for all 32-bit x86 targets.

## Fix link failure on x86-64 when passing `--features=-cx16`

The Pony runtime uses 128-bit atomic operations that need the x86-64 `cx16` CPU feature. ponyc turns that feature on for x86-64 targets, but passing `--features=-cx16` turned it off and ponyc didn't turn it back on. The compiled program then referenced `__atomic_compare_exchange_16`, and on platforms where nothing provides that function, such as macOS, linking failed with an undefined symbol. ponyc now keeps `cx16` on for every x86-64 target, even when `--features` turns it off.

## Fix misaligned loads and stores in Array[U8] multi-byte accessors

`Array[U8]`'s multi-byte methods (`read_u16` through `read_u128`, `update_u16` through `update_u128`, and `push_u16` through `push_u128`) generated loads and stores assuming the wider type's alignment at an arbitrary byte offset. On i386 with SSE2, the `u128` operations crashed with a segfault at unaligned offsets; `buffered.Reader` and `buffered.Writer` u128 operations crashed the same way. On other targets the operations could silently produce wrong results, even when they didn't crash.

The twelve methods now work correctly at any byte offset.

## Fix integer overflow in Array[U8] multi-byte bounds checks

`Array[U8]`'s multi-byte read and update methods (`read_u16` through `read_u128`, `update_u16` through `update_u128`) accepted offsets that should have been rejected. An offset near `USize.max_value()` bypassed the bounds check and read or wrote arbitrary memory outside the array instead of raising an error.

## Add NixOS linking support

ponyc's embedded linker can now link Pony programs on NixOS when run inside `nix-shell` or `nix develop`. Previously, linking failed because NixOS places libc, GCC runtime libraries, and system headers under `/nix/store` instead of the standard `/usr/lib` and `/usr/include` paths that ponyc searches.

When the `NIX_CC` environment variable points to a Nix compiler wrapper with a `nix-support/` directory, ponyc reads the dynamic linker path from `nix-support/dynamic-linker`, extracts library directories from `NIX_LDFLAGS`, reads the glibc prefix from `nix-support/orig-libc` (for CRT startup objects), reads compiler runtime library paths from `nix-support/cc-ldflags` (for libgcc/libgcc_s), and extracts system include directories from `NIX_CFLAGS_COMPILE`. These paths are used for CRT object discovery, GCC runtime discovery, the ELF dynamic linker (PT_INTERP), `-L`/`-rpath` link flags, and C shim include paths. On macOS with nix-darwin, only library search paths are added.

If ponyc detects it is running on NixOS without the Nix environment variables set, it prints a diagnostic suggesting `nix-shell` or `nix develop`.

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

## Fix integer overflow in Array.copy_from and Array.copy_to bounds arithmetic

Passing a large index to `Array.copy_from` or `Array.copy_to` could write past the end of the destination array's allocation. `copy_from` could also read past the end of the source array.

## Fix crashes when disabling required x86-64 CPU features

On x86-64, passing `--features=-sse`, `-sse2`, `-64bit`, or `-64bit-mode` could crash the compiler or fail during linking. ponyc now keeps these required features enabled, even when `--features` requests disabling them. This follows the existing behavior for `cx16` on x86-64 and `sse2` on 32-bit x86.

