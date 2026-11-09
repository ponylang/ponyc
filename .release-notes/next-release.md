## Fix building on 32-bit

The runtime failed to compile on 32-bit ARM with GCC 8.3 due to two issues: implicit narrowing conversions from `int` to `uint16_t` in actor batch-size arithmetic, and a call to `gettid()` whose glibc wrapper is only available in glibc 2.30 and later.

The narrowing conversions are caused by C integer promotion: compound assignments like `batch_size -= 25` promote both operands to `int`, and the implicit conversion back to `uint16_t` triggers `-Werror=conversion`. The `gettid()` call is replaced with `syscall(SYS_gettid)`, which works on all glibc versions.

## Add i386 (32-bit x86) support to the embedded LLD linker

The embedded LLD linker can now link Pony programs targeting 32-bit x86 Linux (i386/i686). Previously, compiling for an i386 target failed at link time with "unsupported architecture for embedded LLD." The embedded linker already supported 32-bit ARM; this extends the same support to 32-bit x86, covering both musl and glibc systems.

