# Windows DbgHelp use is shared by `ponyassert.c` and `tracing.c` with no lock

On Windows, two places in the runtime drive DbgHelp (`SymInitialize` / `SymFromAddr` / `StackWalk64` / `SymCleanup`) to symbolize a backtrace:

- `ponyint_assert_fail` in `src/libponyrt/platform/ponyassert.c`, on whatever thread hit a failed assertion.
- The flight recorder dump in `src/libponyrt/tracing/tracing.c` (`windows_format_frame`, called from `handle_message` on the tracing thread), only in `runtime_tracing` builds.

DbgHelp is single-threaded: all of its calls must be serialized process-wide. Neither site takes a lock, and there is no shared mutex between them. In the common paths this is safe by construction: a failed assertion calls `SymCleanup` and then `abort()`, and only afterwards does the SIGABRT handler run the tracing dump, so the two never overlap; and during a flight recorder dump the other scheduler threads are `SuspendThread`-frozen, leaving the tracing thread as the only live DbgHelp user. The unguarded case is narrow — a hardware fault on one thread concurrent with an assertion failure on another, before the suspend takes effect. The failure mode there is a deadlock, not corruption: the tracing thread's `SymInitialize`/`SymFromAddr` blocks on DbgHelp's process-wide lock held by a thread frozen mid-DbgHelp-call, and the dump is lost only when the crashing thread's 15-second timeout fires. Accepted as best-effort crash reporting.

The coupling: if either site changes how it uses DbgHelp (adds a persistent `SymInitialize` session, symbolizes from a different thread, or stops suspending the other threads during a dump), the informal serialization that keeps these two from clobbering each other can break, with no compile-time or test-time signal. If you add a third DbgHelp user, or make either of these concurrent, introduce a shared lock. Both sites carry a comment pointing here.

This coupling is Windows-only; on POSIX both sites use `backtrace`/`backtrace_symbols`, which do not share this constraint.
