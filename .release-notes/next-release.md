## Fix building on 32-bit

The runtime failed to compile on 32-bit ARM with GCC 8.3 due to two issues: implicit narrowing conversions from `int` to `uint16_t` in actor batch-size arithmetic, and a call to `gettid()` whose glibc wrapper is only available in glibc 2.30 and later.

The narrowing conversions are caused by C integer promotion: compound assignments like `batch_size -= 25` promote both operands to `int`, and the implicit conversion back to `uint16_t` triggers `-Werror=conversion`. The `gettid()` call is replaced with `syscall(SYS_gettid)`, which works on all glibc versions.

## Add i386 (32-bit x86) support to the embedded LLD linker

The embedded LLD linker can now link Pony programs targeting 32-bit x86 Linux (i386/i686). Previously, compiling for an i386 target failed at link time with "unsupported architecture for embedded LLD." The embedded linker already supported 32-bit ARM; this extends the same support to 32-bit x86, covering both musl and glibc systems.

## Fix PosixDate on 32-bit

On systems with 32-bit `time_t` (such as 32-bit ARM Linux), every `PosixDate` produced wrong field values regardless of the input. `PosixDate(0, 0)` returned year 1905 instead of 1970, and `time()` returned `-2037645953` instead of `0`.

The upper bound for dates is now clamped to the `time_t` range on each platform. On 32-bit `time_t` systems, this means the maximum representable date is 2038-01-19 03:14:07 UTC (the Y2038 limit) rather than year 2147483647.

