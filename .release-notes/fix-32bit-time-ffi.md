## Fix time functions truncating to 32 bits on 32-bit musl Linux

On 32-bit musl Linux (such as Alpine on i386 or ARM), `Time.seconds()`, `Time.now()`, and file timestamp operations (`FilePath.set_time`, `Directory.set_time_at`, `File.set_time`) silently truncated time values to 32 bits. File timestamps past January 2038 wrapped to negative values, and after 2038, `Time.seconds()` and `Time.now()` would return wrong results.

musl defines `time_t` as 64-bit on all architectures, but the plain C symbol names (`time`, `clock_gettime`, `utimes`, `futimes`, `futimesat`) are 32-bit compatibility wrappers on 32-bit targets. C code compiled with musl's headers gets the 64-bit versions automatically; Pony's FFI declarations bypassed the headers and bound the 32-bit wrappers directly.

Time values now use the full 64-bit range on 32-bit musl. On glibc 32-bit systems, `time_t` is still 32 bits at the C level, so the Y2038 limit remains there — that is a glibc constraint, not a Pony one.
