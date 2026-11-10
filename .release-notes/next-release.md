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

